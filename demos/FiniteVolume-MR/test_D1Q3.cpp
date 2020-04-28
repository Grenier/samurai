#include <math.h>
#include <vector>
#include <fstream>

#include <cxxopts.hpp>
#include <spdlog/spdlog.h>

#include <xtensor/xio.hpp>

#include <mure/mure.hpp>
#include "coarsening.hpp"
#include "refinement.hpp"
#include "criteria.hpp"

#include <chrono>


/// Timer used in tic & toc
auto tic_timer = std::chrono::high_resolution_clock::now();

/// Launching the timer
void tic()
{
    tic_timer = std::chrono::high_resolution_clock::now();
}


/// Stopping the timer and returning the duration in seconds
double toc()
{
    const auto toc_timer = std::chrono::high_resolution_clock::now();
    const std::chrono::duration<double> time_span = toc_timer - tic_timer;
    return time_span.count();
}

std::array<double, 2> exact_solution(double x, double t)   {

    double g = 1.0;
    double x0 = 0.0;

    double hL = 2.0;
    double hR = 1.0;
    double uL = 0.0;
    double uR = 0.0;

    double cL = std::sqrt(g*hL);
    double cR = std::sqrt(g*hR);
    double cStar = 1.20575324689; // To be computed
    double hStar = cStar*cStar / g;

    double xFanL = x0 - cL * t;
    double xFanR = x0 + (2*cL - 3*cStar) * t;
    double xShock = x0 + (2*cStar*cStar*(cL - cStar)) / (cStar*cStar - cR*cR) * t;

    double h = (x <= xFanL) ? hL : ((x <= xFanR) ? 4./(9.*g)*pow(cL-(x-x0)/(2.*t), 2.0) : ((x < xShock) ? hStar : hR));
    double u = (x <= xFanL) ? uL : ((x <= xFanR) ? 2./3.*(cL+(x-x0)/t) : ((x < xShock) ? 2.*(cL - cStar) : uR));

    return {h, u};

}

template<class Config>
auto init_f(mure::Mesh<Config> &mesh, double t)
{
    constexpr std::size_t nvel = 3;
    mure::BC<1> bc{ {{ {mure::BCType::neumann, 0.0},
                       {mure::BCType::neumann, 0.0},
                       {mure::BCType::neumann, 0.0},
                    }} };

    mure::Field<Config, double, nvel> f("f", mesh, bc);
    f.array().fill(0);

    mesh.for_each_cell([&](auto &cell) {
        auto center = cell.center();
        auto x = center[0];
        
        
        double g = 1.0;
        auto u = exact_solution(x, 0.0);

        double h = u[0];
        double q = h * u[1]; // Linear momentum
        double k = q*q/h + 0.5*g*h*h;

        double lambda = 2.0;

        f[cell][0] = h - k/(lambda*lambda);
        f[cell][1] = 0.5 * ( q + k/lambda)/lambda;
        f[cell][2] = 0.5 * (-q + k/lambda)/lambda;
    });

    return f;
}

template<class Field, class interval_t, class FieldTag>
xt::xtensor<double, 1> prediction(const Field& f, std::size_t level_g, std::size_t level, const interval_t &i, const std::size_t item, 
                                  const FieldTag & tag, std::map<std::tuple<std::size_t, std::size_t, std::size_t, interval_t>, 
                                  xt::xtensor<double, 1>> & mem_map)
{

    // We check if the element is already in the map
    auto it = mem_map.find({item, level_g, level, i});
    if (it != mem_map.end())   {
        //std::cout<<std::endl<<"Found by memoization";
        return it->second;
    }
    else {

        auto mesh = f.mesh();
        xt::xtensor<double, 1> out = xt::empty<double>({i.size()/i.step});//xt::eval(f(item, level_g, i));
        auto mask = mesh.exists(level_g + level, i);

        // std::cout << level_g + level << " " << i << " " << mask << "\n"; 
        if (xt::all(mask))
        {         
            return xt::eval(f(item, level_g + level, i));
        }

        auto step = i.step;
        auto ig = i / 2;
        ig.step = step >> 1;
        xt::xtensor<double, 1> d = xt::empty<double>({i.size()/i.step});

        for (int ii=i.start, iii=0; ii<i.end; ii+=i.step, ++iii)
        {
            d[iii] = (ii & 1)? -1.: 1.;
        }

    
        auto val = xt::eval(prediction(f, level_g, level-1, ig, item, tag, mem_map) - 1./8 * d * (prediction(f, level_g, level-1, ig+1, item, tag, mem_map) 
                                                                                       - prediction(f, level_g, level-1, ig-1, item, tag, mem_map)));
        

        xt::masked_view(out, !mask) = xt::masked_view(val, !mask);
        for(int i_mask=0, i_int=i.start; i_int<i.end; ++i_mask, i_int+=i.step)
        {
            if (mask[i_mask])
            {
                out[i_mask] = f(item, level_g + level, {i_int, i_int + 1})[0];
            }
        }

        // The value should be added to the memoization map before returning
        return mem_map[{item, level_g, level, i}] = out;

        //return out;
    }

}


template<class Field, class interval_t>
xt::xtensor<double, 2> prediction_all(const Field& f, std::size_t level_g, std::size_t level, const interval_t &i, 
                                  std::map<std::tuple<std::size_t, std::size_t, interval_t>, 
                                  xt::xtensor<double, 2>> & mem_map)
{

    using namespace xt::placeholders;
    // We check if the element is already in the map
    auto it = mem_map.find({level_g, level, i});
    if (it != mem_map.end())
    {
        return it->second;
    }
    else
    {
        auto mesh = f.mesh();
        std::vector<std::size_t> shape = {i.size(), 3};
        xt::xtensor<double, 2> out = xt::empty<double>(shape);
        auto mask = mesh.exists(level_g + level, i);

        xt::xtensor<double, 2> mask_all = xt::empty<double>(shape);
        xt::view(mask_all, xt::all(), 0) = mask;
        xt::view(mask_all, xt::all(), 1) = mask;
        xt::view(mask_all, xt::all(), 2) = mask;

        if (xt::all(mask))
        {         
            return xt::eval(f(level_g + level, i));
        }

        auto ig = i >> 1;
        ig.step = 1;

        xt::xtensor<double, 2> val = xt::empty<double>(shape);
        auto current = xt::eval(prediction_all(f, level_g, level-1, ig, mem_map));
        auto left = xt::eval(prediction_all(f, level_g, level-1, ig-1, mem_map));
        auto right = xt::eval(prediction_all(f, level_g, level-1, ig+1, mem_map));

        std::size_t start_even = (i.start&1)? 1: 0;
        std::size_t start_odd = (i.start&1)? 0: 1;
        std::size_t end_even = (i.end&1)? ig.size(): ig.size()-1;
        std::size_t end_odd = (i.end&1)? ig.size()-1: ig.size();
        xt::view(val, xt::range(start_even, _, 2)) = xt::view(current - 1./8 * (right - left), xt::range(start_even, _));
        xt::view(val, xt::range(start_odd, _, 2)) = xt::view(current + 1./8 * (right - left), xt::range(_, end_odd));

        xt::masked_view(out, !mask_all) = xt::masked_view(val, !mask_all);
        for(int i_mask=0, i_int=i.start; i_int<i.end; ++i_mask, ++i_int)
        {
            if (mask[i_mask])
            {
                xt::view(out, i_mask) = xt::view(f(level_g + level, {i_int, i_int + 1}), 0);
            }
        }

        // The value should be added to the memoization map before returning
        return out;// mem_map[{level_g, level, i, ig}] = out;
    }
}

template<class Field, class FieldTag>
void one_time_step(Field &f, const FieldTag & tag, double s)
{
    constexpr std::size_t nvel = Field::size;
    double lambda = 2.;//, s = 1.0;
    auto mesh = f.mesh();
    auto max_level = mesh.max_level();

    mure::mr_projection(f);
    mure::mr_prediction(f);


    // MEMOIZATION
    // All is ready to do a little bit  of mem...
    using interval_t = typename Field::Config::interval_t;
    std::map<std::tuple<std::size_t, std::size_t, std::size_t, interval_t>, xt::xtensor<double, 1>> memoization_map;
    memoization_map.clear(); // Just to be sure...

    Field new_f{"new_f", mesh};
    new_f.array().fill(0.);

    for (std::size_t level = 0; level <= max_level; ++level)
    {
        auto exp = mure::intersection(mesh[mure::MeshType::cells][level],
                                      mesh[mure::MeshType::cells][level]);
        exp([&](auto, auto &interval, auto) {
            auto i = interval[0];


            // STREAM

            std::size_t j = max_level - level;

            double coeff = 1. / (1 << j);

            // This is the STANDARD FLUX EVALUATION

            auto f0 = f(0, level, i);
            
            auto fp = f(1, level, i) + coeff * (prediction(f, level, j, i*(1<<j)-1, 1, tag, memoization_map)
                                             -  prediction(f, level, j, (i+1)*(1<<j)-1, 1, tag, memoization_map));

            auto fm = f(2, level, i) - coeff * (prediction(f, level, j, i*(1<<j), 2, tag, memoization_map)
                                             -  prediction(f, level, j, (i+1)*(1<<j), 2, tag, memoization_map));
            
            
            // COLLISION    

            auto h = xt::eval(f0 + fp + fm);
            auto q = xt::eval(lambda * (fp - fm));
            auto k = xt::eval(lambda*lambda * (fp + fm));

            double g = 1.0;
            auto k_coll = (1 - s) * k + s * q*q/h + 0.5*g*h*h;


            new_f(0, level, i) = h - k_coll/(lambda*lambda);
            new_f(1, level, i) = 0.5 * ( q + k_coll/lambda)/lambda;
            new_f(2, level, i) = 0.5 * (-q + k_coll/lambda)/lambda;


        });
    }

    std::swap(f.array(), new_f.array());
}

// template<class Field, class FieldR>
template<class Config, class FieldR>
std::array<double, 4> compute_error(mure::Field<Config, double, 3> &f, FieldR & fR, double t)
{

    auto mesh = f.mesh();

    auto meshR = fR.mesh();
    auto max_level = meshR.max_level();

    mure::mr_projection(f);
    mure::mr_prediction(f);  // C'est supercrucial de le faire.

    f.update_bc(); // Important especially when we enforce Neumann...for the Riemann problem
    fR.update_bc();    

    // Getting ready for memoization
    // using interval_t = typename Field::Config::interval_t;
    using interval_t = typename Config::interval_t;
    std::map<std::tuple<std::size_t, std::size_t, interval_t>, xt::xtensor<double, 2>> error_memoization_map;
    error_memoization_map.clear();

    double error_h = 0.0; // First momentum 
    double error_q = 0.0; // Second momentum
    double diff_h = 0.0;
    double diff_q = 0.0;


    double dx = 1.0 / (1 << max_level);

    for (std::size_t level = 0; level <= max_level; ++level)
    {
        auto exp = mure::intersection(meshR[mure::MeshType::cells][max_level],
                                      mesh[mure::MeshType::cells][level])
                  .on(max_level);

        exp([&](auto, auto &interval, auto) {
            auto i = interval[0];
            auto j = max_level - level;

            auto sol  = prediction_all(f, level, j, i, error_memoization_map);
            auto solR = xt::view(fR(max_level, i), xt::all(), xt::range(0, 3));


            xt::xtensor<double, 1> x = dx*xt::linspace<int>(i.start, i.end - 1, i.size()) + 0.5*dx;

            xt::xtensor<double, 1> hexact = xt::zeros<double>(x.shape());
            xt::xtensor<double, 1> qexact = xt::zeros<double>(x.shape());

            for (std::size_t idx = 0; idx < x.shape()[0]; ++idx)    {
                auto ex_sol = exact_solution(x[idx], t);

                hexact[idx] = ex_sol[0];
                qexact[idx] = ex_sol[0]*ex_sol[1];

            }

            error_h += xt::sum(xt::abs(xt::flatten(xt::view(fR(max_level, i), xt::all(), xt::range(0, 1)) 
                                                 + xt::view(fR(max_level, i), xt::all(), xt::range(1, 2))
                                                 + xt::view(fR(max_level, i), xt::all(), xt::range(2, 3))) 
                                     - hexact))[0];

            double lambda = 2.0;
            error_q += xt::sum(xt::abs(lambda*xt::flatten(xt::view(fR(max_level, i), xt::all(), xt::range(1, 2))
                                                        - xt::view(fR(max_level, i), xt::all(), xt::range(2, 3))) 
                                     - qexact))[0];


            diff_h += xt::sum(xt::abs(xt::flatten(xt::view(sol, xt::all(), xt::range(0, 1)) 
                                                + xt::view(sol, xt::all(), xt::range(1, 2))
                                                + xt::view(sol, xt::all(), xt::range(2, 3))) 
                                                - xt::flatten(xt::view(fR(max_level, i), xt::all(), xt::range(0, 1)) 
                                                            + xt::view(fR(max_level, i), xt::all(), xt::range(1, 2))
                                                            + xt::view(fR(max_level, i), xt::all(), xt::range(2, 3))))) [0];
            
            diff_q += xt::sum(xt::abs(lambda * xt::flatten(xt::view(sol, xt::all(), xt::range(1, 2))
                                                         - xt::view(sol, xt::all(), xt::range(2, 3))) 
                                                - lambda * xt::flatten(xt::view(fR(max_level, i), xt::all(), xt::range(1, 2))
                                                                     - xt::view(fR(max_level, i), xt::all(), xt::range(2, 3))))) [0];
        });
    }

    return {dx * error_h, dx * diff_h, 
            dx * error_q, dx * diff_q};   
}

int main(int argc, char *argv[])
{
    cxxopts::Options options("lbm_d1q2_burgers",
                             "Multi resolution for a D1Q2 LBM scheme for Burgers equation");

    options.add_options()
                       ("min_level", "minimum level", cxxopts::value<std::size_t>()->default_value("2"))
                       ("max_level", "maximum level", cxxopts::value<std::size_t>()->default_value("10"))
                       ("epsilon", "maximum level", cxxopts::value<double>()->default_value("0.01"))
                       ("s", "relaxation parameter", cxxopts::value<double>()->default_value("1.0"))
                       ("log", "log level", cxxopts::value<std::string>()->default_value("warning"))
                       ("h, help", "Help");

    try
    {
        auto result = options.parse(argc, argv);

        if (result.count("help"))
            std::cout << options.help() << "\n";
        else
        {
            std::map<std::string, spdlog::level::level_enum> log_level{{"debug", spdlog::level::debug},
                                                               {"warning", spdlog::level::warn}};
            constexpr size_t dim = 1;
            using Config = mure::MRConfig<dim, 2>;

            spdlog::set_level(log_level[result["log"].as<std::string>()]);
            std::size_t min_level = 2;//result["min_level"].as<std::size_t>();
            std::size_t max_level = 9;//result["max_level"].as<std::size_t>();


            // We set some parameters according
            // to the problem.
            double sol_reg = 0.0;
            double T = 0.2;
            std::string case_name("s_d");;

            mure::Box<double, dim> box({-3}, {3});

            std::vector<double> s_vect {0.75, 1.0, 1.25, 1.5, 1.75};

            for (auto s : s_vect)   {
                std::cout<<std::endl<<"Relaxation parameter s = "<<s;

                std::string prefix (case_name + "_s_"+std::to_string(s)+"_");

                std::cout<<std::endl<<"Testing time behavior"<<std::endl;
                {
                    double eps = 1.0e-4; // This remains fixed

                    mure::Mesh<Config> mesh{box, min_level, max_level};
                    mure::Mesh<Config> meshR{box, max_level, max_level}; // This is the reference scheme

                    // Initialization
                    auto f  = init_f(mesh , 0.0);
                    auto fR = init_f(meshR, 0.0);             

                    double dx = 1.0 / (1 << max_level);
                    double dt = dx; // Since lb = 1

                    std::size_t N = static_cast<std::size_t>(T / dt);

                    double t = 0.0;

                    std::ofstream out_time_frames;
                    
                    std::ofstream out_error_h_exact_ref; // On the height
                    std::ofstream out_diff_h_ref_adap;

                    std::ofstream out_error_q_exact_ref; // On the momentum
                    std::ofstream out_diff_q_ref_adap;

                    std::ofstream out_compression;

                    out_time_frames.open     ("./d1q3/time/"+prefix+"time.dat");

                    out_error_h_exact_ref.open ("./d1q3/time/"+prefix+"error_h.dat");
                    out_diff_h_ref_adap.open   ("./d1q3/time/"+prefix+"diff_h.dat");

                    out_error_q_exact_ref.open ("./d1q3/time/"+prefix+"error_q.dat");
                    out_diff_q_ref_adap.open   ("./d1q3/time/"+prefix+"diff_q.dat");

                    out_compression.open     ("./d1q3/time/"+prefix+"comp.dat");


                    for (std::size_t nb_ite = 0; nb_ite < N; ++nb_ite)
                    {
                        for (std::size_t i=0; i<max_level-min_level; ++i)
                        {
                            if (coarsening(f, eps, i))
                                break;
                        }

                        for (std::size_t i=0; i<max_level-min_level; ++i)
                        {
                            if (refinement(f, eps, sol_reg, i))
                                break;
                        }

                        mure::Field<Config, int, 1> tag_leaf{"tag_leaf", mesh};
                        tag_leaf.array().fill(0);
                        mesh.for_each_cell([&](auto &cell) {
                            tag_leaf[cell] = static_cast<int>(1);
                        });
        
                        mure::Field<Config, int, 1> tag_leafR{"tag_leafR", meshR};
                        tag_leafR.array().fill(0);
                        meshR.for_each_cell([&](auto &cell) {
                            tag_leafR[cell] = static_cast<int>(1);
                        });

                        auto error = compute_error(f, fR, t);

                        out_time_frames    <<t       <<std::endl;

                        out_error_h_exact_ref<<error[0]<<std::endl;
                        out_diff_h_ref_adap  <<error[1]<<std::endl;

                        out_error_q_exact_ref<<error[2]<<std::endl;
                        out_diff_q_ref_adap  <<error[3]<<std::endl;

                        out_compression    <<static_cast<double>(mesh.nb_cells(mure::MeshType::cells)) 
                                           / static_cast<double>(meshR.nb_cells(mure::MeshType::cells))<<std::endl;

                        std::cout<<std::endl<<"Time = "<<t<<" Diff_h = "<<error[1]<<std::endl<<"Diff q = "<<error[3];

                
                        one_time_step(f, tag_leaf, s);
                        one_time_step(fR, tag_leafR, s);

                        t += dt;
             
                    }

                    std::cout<<std::endl;
            
                    out_time_frames.close();

                    out_error_h_exact_ref.close();
                    out_diff_h_ref_adap.close();

                    out_error_q_exact_ref.close();
                    out_diff_q_ref_adap.close();

                    out_compression.close();
                }
                
                std::cout<<std::endl<<"Testing eps behavior"<<std::endl;
                {
                    double eps = 1.0e-1;//0.1;
                    std::size_t N_test = 50;//50;
                    double factor = 0.60;
                    std::ofstream out_eps;
                    
                    std::ofstream out_diff_h_ref_adap;
                    std::ofstream out_diff_q_ref_adap;

                    std::ofstream out_compression;

                    out_eps.open             ("./d1q3/eps/"+prefix+"eps.dat");

                    out_diff_h_ref_adap.open   ("./d1q3/eps/"+prefix+"diff_h.dat");
                    out_diff_q_ref_adap.open   ("./d1q3/eps/"+prefix+"diff_q.dat");

                    out_compression.open     ("./d1q3/eps/"+prefix+"comp.dat");

                    for (std::size_t n_test = 0; n_test < N_test; ++ n_test)    {
                        std::cout<<std::endl<<"Test "<<n_test<<" eps = "<<eps;

                        mure::Mesh<Config> mesh{box, min_level, max_level};
                        mure::Mesh<Config> meshR{box, max_level, max_level}; // This is the reference scheme

                        // Initialization
                        auto f  = init_f(mesh , 0.0);
                        auto fR = init_f(meshR, 0.0);             

                        double dx = 1.0 / (1 << max_level);
                        double dt = dx/2.0; // Since lb = 2

                        std::size_t N = static_cast<std::size_t>(T / dt);

                        double t = 0.0;

                        for (std::size_t nb_ite = 0; nb_ite < N; ++nb_ite)
                        {
                            for (std::size_t i=0; i<max_level-min_level; ++i)
                            {
                                if (coarsening(f, eps, i))
                                    break;
                            }

                            for (std::size_t i=0; i<max_level-min_level; ++i)
                            {
                                if (refinement(f, eps, sol_reg, i))
                                    break;
                            }

                            mure::Field<Config, int, 1> tag_leaf{"tag_leaf", mesh};
                            tag_leaf.array().fill(0);
                            mesh.for_each_cell([&](auto &cell) {
                                tag_leaf[cell] = static_cast<int>(1);
                            });

                            mure::Field<Config, int, 1> tag_leafR{"tag_leafR", meshR};
                            tag_leafR.array().fill(0);
                            meshR.for_each_cell([&](auto &cell) {
                                tag_leafR[cell] = static_cast<int>(1);
                            });

                            { // This is ultra important if we do not want to compute the error
                            // at each time step.
                                mure::mr_projection(f);
                                mure::mr_prediction(f); 

                                f.update_bc(); //
                                fR.update_bc();    
                            }
                        
                            // if (nb_ite == N - 1){
                            //     auto error = compute_error(f, fR, t);

                            //     std::cout<<std::endl<<"Eps = "<<eps<<" Diff_h = "<<error[1]<<std::endl<<"Diff q = "<<error[3];
                            // }
                
                            one_time_step(f, tag_leaf, s);
                            one_time_step(fR, tag_leafR, s);

                            t += dt;
             
                        }


                        auto error = compute_error(f, fR, 0.0);

                        std::cout<<"Diff  h= "<<error[1]<<std::endl<<"Diff q = "<<error[3]<<std::endl;
                            
                            
                        
                        out_eps<<eps<<std::endl;

                        out_diff_h_ref_adap<<error[1]<<std::endl;
                        out_diff_q_ref_adap<<error[3]<<std::endl;

                        out_compression<<static_cast<double>(mesh.nb_cells(mure::MeshType::cells)) 
                                           / static_cast<double>(meshR.nb_cells(mure::MeshType::cells))<<std::endl;

                        eps *= factor;
                    }
            
                    out_eps.close();  

                    out_diff_h_ref_adap.close();
                    out_diff_q_ref_adap.close();
                    
                    out_compression.close();

                }
            }
        }
    }
    
    catch (const cxxopts::OptionException &e)
    {
        std::cout << options.help() << "\n";
    }



    return 0;
}
