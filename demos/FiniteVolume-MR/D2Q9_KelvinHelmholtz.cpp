/*
    The choice of the momenti is that
    of Geier as testesd in pyLBM and working properly
*/

#include <math.h>
#include <vector>

#include <cxxopts.hpp>
#include <spdlog/spdlog.h>
#include <math.h>

#include <samurai/samurai.hpp>
#include "coarsening.hpp"
#include "refinement.hpp"
#include "criteria.hpp"
#include "prediction_map_2d.hpp"


double mach   = 0.1;
double lambda = sqrt(3.0) / mach;
double rho_0  = 1.0;
double U_0    = 0.05;//0.5;
double zeta   = 0.0366;
double mu     = 1.0E-6;
double k      = 80.0;
double delta  = 0.05;


template<class Config>
auto init_f(samurai::Mesh<Config> &mesh, double t)
{
    constexpr std::size_t nvel = 9;
    samurai::BC<2> bc{ {{ {samurai::BCType::neumann, 0}, // W
                       {samurai::BCType::neumann, 0}, // E
                       {samurai::BCType::neumann, 0}, // S
                       {samurai::BCType::neumann, 0}  // N
                    }} };

    samurai::Field<Config, double, nvel> f("f", mesh, bc);
    f.array().fill(0);

    mesh.for_each_cell([&](auto &cell) {
        auto center = cell.center();
        auto x = center[0];
        auto y = center[1];

        double rho = rho_0;
        // double qx = 0.0;
        // double qy = U_0 * delta * sin(2. * M_PI * (x + .25));

        // if (y <= 0.5)
        //     qx = U_0 * tanh(k * (y - .25));
        // else
        //     qx = U_0 * tanh(k * (.75 - y));

        double qx = 1.0;
        double qy = 0.5 * tanh(3*k * (x - .5));

        // qx = 0.5 * (x + 2.0*y);
        // double qy = tanh(k * (y - .25));

        // We give standard names
        double c02 = lambda * lambda / 3.0; // sound velocity squared

        double m0 = rho;
        double m1 = qx;
        double m2 = qy;
        double m3 = (qx*qx+qy*qy)/rho + 2.*rho*c02;
        double m4 = qx*(c02+(qy/rho)*(qy/rho));
        double m5 = qy*(c02+(qx/rho)*(qx/rho));
        double m6 = rho*(c02+(qx/rho)*(qx/rho))*(c02+(qy/rho)*(qy/rho));
        double m7 = (qx*qx-qy*qy)/rho;
        double m8 = qx*qy/rho;



        // We come back to the distributions

        double r1 = 1.0 / lambda;
        double r2 = 1.0 / (lambda*lambda);
        double r3 = 1.0 / (lambda*lambda*lambda);
        double r4 = 1.0 / (lambda*lambda*lambda*lambda);

        f[cell][0] = m0                      -     r2*m3                        +     r4*m6                         ;
        f[cell][1] =     .5*r1*m1            + .25*r2*m3 - .5*r3*m4             -  .5*r4*m6 + .25*r2*m7             ;
        f[cell][2] =                .5*r1*m2 + .25*r2*m3            -  .5*r3*m5 -  .5*r4*m6 - .25*r2*m7             ;
        f[cell][3] =    -.5*r1*m1            + .25*r2*m3 + .5*r3*m4             -  .5*r4*m6 + .25*r2*m7             ;
        f[cell][4] =              - .5*r1*m2 + .25*r2*m3            +  .5*r3*m5 -  .5*r4*m6 - .25*r2*m7             ;
        f[cell][5] =                                      .25*r3*m4 + .25*r3*m5 + .25*r4*m6             + .25*r2*m8 ;
        f[cell][6] =                                     -.25*r3*m4 + .25*r3*m5 + .25*r4*m6             - .25*r2*m8 ;
        f[cell][7] =                                     -.25*r3*m4 - .25*r3*m5 + .25*r4*m6             + .25*r2*m8 ;
        f[cell][8] =                                      .25*r3*m4 - .25*r3*m5 + .25*r4*m6             - .25*r2*m8 ;

    });

    return f;
}



template<class coord_index_t>
auto compute_prediction(std::size_t min_level, std::size_t max_level)
{
    coord_index_t i = 0, j = 0;
    std::vector<std::vector<prediction_map<coord_index_t>>> data(max_level-min_level+1);

    for(std::size_t k=0; k<max_level-min_level+1; ++k)
    {
        int size = (1<<k);
        data[k].resize(8); // The null velocity is not used

        // Velocities parallel to the axis
        for (int l = 0; l < size; ++l)
        {
            data[k][0] += prediction(k, i*size - 1, j*size + l) - prediction(k, (i+1)*size - 1, j*size + l);
            data[k][1] += prediction(k, i*size + l, j*size - 1) - prediction(k, i*size + l, (j+1)*size - 1);
            data[k][2] += prediction(k, (i+1)*size, j*size + l) - prediction(k, i*size, j*size + l);
            data[k][3] += prediction(k, i*size + l, (j+1)*size) - prediction(k, i*size + l, j*size);
        }
        // Diagonal velocities -  x stripes
        for (int l = 0; l < size; ++l)
        {
            data[k][4] += prediction(k, i*size - l - 1, j*size - 1) - prediction(k, i*size + l, (j+1)*size - 1);
            data[k][5] += prediction(k, i*size + l + 1, j*size - 1) - prediction(k, i*size + l, (j+1)*size - 1);
            data[k][6] += prediction(k, i*size + l + 1, (j+1)*size) - prediction(k, i*size + l, j*size);
            data[k][7] += prediction(k, i*size + l - 1, (j+1)*size) - prediction(k, i*size + l, j*size);

        }
        // Diagonal velocities -  y stripes
        for (int l = 1; l < size; ++l) // We start from 1 in order not to count the angular cells twice
        {
            data[k][4] += prediction(k, i*size - 1, j*size + l - 1) - prediction(k, (i+1)*size - 1, j*size + l - 1);
            data[k][5] += prediction(k, (i+1)*size - 1, j*size + l - 1) - prediction(k, i*size, j*size + l - 1);
            data[k][6] += prediction(k, (i+1)*size, (j+1)*size - l) - prediction(k, i*size, (j+1)*size - l);
            data[k][7] += prediction(k, i*size - 1, (j+1)*size - l) - prediction(k, (i+1)*size - 1, (j+1)*size - l);
        }

    }
    return data;
}

template<class Field, class interval_t, class index_t>
auto prediction(const Field& f, std::size_t level_g, std::size_t level, const interval_t &k, const index_t h, const std::size_t item)
{
    if (level == 0)
    {
        return xt::eval(f(item, level_g, k, h));
    }

    auto step = k.step;
    auto kg = k / 2;
    auto hg = h / 2;
    kg.step = step >> 1;
    xt::xtensor<double, 1> d_x = xt::empty<double>({k.size()/k.step});
    xt::xtensor<double, 1> d_xy = xt::empty<double>({k.size()/k.step});
    double d_y = (h & 1)? -1.: 1.;

    for (int ii=k.start, iii=0; ii<k.end; ii+=k.step, ++iii)
    {
        d_x[iii] = (ii & 1)? -1.: 1.;
        d_xy[iii] = ((ii+h) & 1)? -1.: 1.;
    }

    return xt::eval(prediction(f, level_g, level-1, kg, hg, item) - 1./8 * d_x * (prediction(f, level_g, level-1, kg+1, hg, item)
                                                                               - prediction(f, level_g, level-1, kg-1, hg, item))
                                                                  - 1./8 * d_y * (prediction(f, level_g, level-1, kg, hg+1, item)
                                                                               - prediction(f, level_g, level-1, kg, hg-1, item))
                                                                  - 1./64 * d_xy * (prediction(f, level_g, level-1, kg+1, hg+1, item)
                                                                                 - prediction(f, level_g, level-1, kg+1, hg-1, item)
                                                                                 - prediction(f, level_g, level-1, kg-1, hg+1, item)
                                                                                 + prediction(f, level_g, level-1, kg-1, hg+1, item)));
}

template<class Field, class pred>
void one_time_step(Field &f, const pred& pred_coeff)
{
    constexpr std::size_t nvel = Field::size;
    using coord_index_t = typename Field::coord_index_t;

    auto mesh = f.mesh();
    auto max_level = mesh.max_level();

    double space_step = 1.0 / (1 << max_level);

    samurai::mr_projection(f);
    samurai::mr_prediction(f);

    Field new_f{"new_f", mesh};
    new_f.array().fill(0.);

    for (std::size_t level = 0; level <= max_level; ++level)
    {
        auto exp = samurai::intersection(mesh[samurai::MeshType::cells][level],
                                      mesh[samurai::MeshType::cells][level]);
        exp([&](auto& index, auto &interval, auto) {
            auto k = interval[0]; // Logical index in x
            auto h = index[0];    // Logical index in y

            std::size_t j = max_level - level;
            double coeff = 1. / (1 << (2*j)); // The factor 2 comes from the 2D

            auto f0 = xt::eval(f(0, level, k, h));
            auto f1 = xt::eval(f(1, level, k, h));
            auto f2 = xt::eval(f(2, level, k, h));
            auto f3 = xt::eval(f(3, level, k, h));
            auto f4 = xt::eval(f(4, level, k, h));
            auto f5 = xt::eval(f(5, level, k, h));
            auto f6 = xt::eval(f(6, level, k, h));
            auto f7 = xt::eval(f(7, level, k, h));
            auto f8 = xt::eval(f(8, level, k, h));
            // Fluxes
            // Velocity 1
            for(auto &c: pred_coeff[j][1 - 1].coeff)
            {
                coord_index_t stencil_x, stencil_y;
                std::tie(stencil_x, stencil_y) = c.first;
                f1 += coeff*c.second*f(1, level, k + stencil_x, h + stencil_y);
            }
            // Velocity 2
            for(auto &c: pred_coeff[j][2 - 1].coeff)
            {
                coord_index_t stencil_x, stencil_y;
                std::tie(stencil_x, stencil_y) = c.first;
                f2 += coeff*c.second*f(2, level, k + stencil_x, h + stencil_y);
            }
            // Velocity 3
            for(auto &c: pred_coeff[j][3 - 1].coeff)
            {
                coord_index_t stencil_x, stencil_y;
                std::tie(stencil_x, stencil_y) = c.first;
                f3 += coeff*c.second*f(3, level, k + stencil_x, h + stencil_y);
            }
            // Velocity 4
            for(auto &c: pred_coeff[j][4 - 1].coeff)
            {
                coord_index_t stencil_x, stencil_y;
                std::tie(stencil_x, stencil_y) = c.first;
                f4 += coeff*c.second*f(4, level, k + stencil_x, h + stencil_y);
            }
            // Velocity 5
            for(auto &c: pred_coeff[j][5 - 1].coeff)
            {
                coord_index_t stencil_x, stencil_y;
                std::tie(stencil_x, stencil_y) = c.first;
                f5 += coeff*c.second*f(5, level, k + stencil_x, h + stencil_y);
            }
            // Velocity 6
            for(auto &c: pred_coeff[j][6 - 1].coeff)
            {
                coord_index_t stencil_x, stencil_y;
                std::tie(stencil_x, stencil_y) = c.first;
                f6 += coeff*c.second*f(6, level, k + stencil_x, h + stencil_y);
            }
            // Velocity 7
            for(auto &c: pred_coeff[j][7 - 1].coeff)
            {
                coord_index_t stencil_x, stencil_y;
                std::tie(stencil_x, stencil_y) = c.first;
                f7 += coeff*c.second*f(7, level, k + stencil_x, h + stencil_y);
            }
            // Velocity 8
            for(auto &c: pred_coeff[j][8 - 1].coeff)
            {
                coord_index_t stencil_x, stencil_y;
                std::tie(stencil_x, stencil_y) = c.first;
                f8 += coeff*c.second*f(8, level, k + stencil_x, h + stencil_y);
            }


            // // We compute the advected momenti
            double l1 = lambda;
            double l2 = l1 * lambda;
            double l3 = l2 * lambda;
            double l4 = l3 * lambda;

            auto m0 = xt::eval(f0 + f1 + f2 + f3 + f4 + f5 + f6 + f7 + f8);
            auto m1 = xt::eval(l1*(f1 - f3 + f5 - f6 - f7 + f8));
            auto m2 = xt::eval(l1*(f2 - f4 + f5 + f6 - f7 - f8));
            auto m3 = xt::eval(l2*(f1 + f2 + f3 + f4 + 2*f5 + 2*f6 + 2*f7 + 2*f8));
            auto m4 = xt::eval(l3*(f5 - f6 - f7 + f8));
            auto m5 = xt::eval(l3*(f5 + f6 - f7 - f8));
            auto m6 = xt::eval(l4*(f5 + f6 + f7 + f8));
            auto m7 = xt::eval(l2*(f1 - f2 + f3 - f4));
            auto m8 = xt::eval(l2*(f5 - f6 + f7 - f8));



            // Collision

            double dummy = 3.0/(lambda*rho_0*space_step);
            double sigma_1 = dummy*zeta;
            double sigma_2 = dummy*mu;
            double s_1 = 1/(.5+sigma_1);
            double s_2 = 1/(.5+sigma_2);

            double c02 = lambda * lambda / 3.0; // sound velocity squared


            m3 = (1. - s_1) * m3 + s_1 * ((m1*m1+m2*m2)/m0 + 2.*m0*c02);
            m4 = (1. - s_1) * m4 + s_1 * (m1*(c02+(m2/m0)*(m2/m0)));
            m5 = (1. - s_1) * m5 + s_1 * (m2*(c02+(m1/m0)*(m1/m0)));
            m6 = (1. - s_1) * m6 + s_1 * (m0*(c02+(m1/m0)*(m1/m0))*(c02+(m2/m0)*(m2/m0)));
            m7 = (1. - s_2) * m7 + s_2 * ((m1*m1-m2*m2)/m0);
            m8 = (1. - s_2) * m8 + s_2 * (m1*m2/m0);

            // We come back to the distributions

            double r1 = 1.0 / lambda;
            double r2 = 1.0 / (lambda*lambda);
            double r3 = 1.0 / (lambda*lambda*lambda);
            double r4 = 1.0 / (lambda*lambda*lambda*lambda);

            new_f(0, level, k, h) = m0                      -     r2*m3                        +     r4*m6                         ;
            new_f(1, level, k, h) =     .5*r1*m1            + .25*r2*m3 - .5*r3*m4             -  .5*r4*m6 + .25*r2*m7             ;
            new_f(2, level, k, h) =                .5*r1*m2 + .25*r2*m3            -  .5*r3*m5 -  .5*r4*m6 - .25*r2*m7             ;
            new_f(3, level, k, h) =    -.5*r1*m1            + .25*r2*m3 + .5*r3*m4             -  .5*r4*m6 + .25*r2*m7             ;
            new_f(4, level, k, h) =              - .5*r1*m2 + .25*r2*m3            +  .5*r3*m5 -  .5*r4*m6 - .25*r2*m7             ;
            new_f(5, level, k, h) =                                      .25*r3*m4 + .25*r3*m5 + .25*r4*m6             + .25*r2*m8 ;
            new_f(6, level, k, h) =                                     -.25*r3*m4 + .25*r3*m5 + .25*r4*m6             - .25*r2*m8 ;
            new_f(7, level, k, h) =                                     -.25*r3*m4 - .25*r3*m5 + .25*r4*m6             + .25*r2*m8 ;
            new_f(8, level, k, h) =                                      .25*r3*m4 - .25*r3*m5 + .25*r4*m6             - .25*r2*m8 ;

        });
    }

    std::swap(f.array(), new_f.array());
}

template<class Field>
void save_solution(Field &f, double eps, std::size_t ite, std::string ext="")
{
    using Config = typename Field::Config;
    auto mesh = f.mesh();
    std::size_t min_level = mesh.min_level();
    std::size_t max_level = mesh.max_level();

    std::stringstream str;
    str << "LBM_D2Q9_KelvinHelmholtz_" << ext << "_lmin_" << min_level << "_lmax-" << max_level << "_eps-"
        << eps << "_ite-" << ite;

    auto h5file = samurai::Hdf5(str.str().data());
    h5file.add_mesh(mesh);
    samurai::Field<Config> level_{"level", mesh};
    samurai::Field<Config> rho{"rho", mesh};
    samurai::Field<Config> qx{"qx", mesh};
    samurai::Field<Config> qy{"qy", mesh};
    samurai::Field<Config> vel_mod{"vel_modulus", mesh};

    mesh.for_each_cell([&](auto &cell) {
        level_[cell] = static_cast<double>(cell.level);
        rho[cell] = f[cell][0] + f[cell][1] + f[cell][2] + f[cell][3]
                  + f[cell][4] + f[cell][5] + f[cell][6] + f[cell][7] + f[cell][8];

        qx[cell] = lambda * (f[cell][1] - f[cell][3] + f[cell][5] - f[cell][6] - f[cell][7] + f[cell][8]);
        qy[cell] = lambda * (f[cell][2] - f[cell][4] + f[cell][5] + f[cell][6] - f[cell][7] - f[cell][8]);

        vel_mod[cell] = xt::sqrt((qx[cell] / rho[cell]) * (qx[cell] / rho[cell])
                                + (qy[cell] / rho[cell]) * (qy[cell] / rho[cell]));

    });
    h5file.add_field(rho);
    h5file.add_field(qx);
    h5file.add_field(qy);
    h5file.add_field(vel_mod);

    h5file.add_field(f);
    h5file.add_field(level_);
}

int main(int argc, char *argv[])
{
    cxxopts::Options options("lbm_d2q5_kelvin_helhotlz",
                             "...");

    options.add_options()
                       ("min_level", "minimum level", cxxopts::value<std::size_t>()->default_value("2"))
                       ("max_level", "maximum level", cxxopts::value<std::size_t>()->default_value("8"))
                       ("epsilon", "maximum level", cxxopts::value<double>()->default_value("0.01"))
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
            constexpr size_t dim = 2;
            using Config = samurai::MRConfig<dim, 2>;

            spdlog::set_level(log_level[result["log"].as<std::string>()]);
            std::size_t min_level = result["min_level"].as<std::size_t>();
            std::size_t max_level = result["max_level"].as<std::size_t>();
            double eps = result["epsilon"].as<double>();

            samurai::Box<double, dim> box({0, 0}, {1, 1});
            samurai::Mesh<Config> mesh{box, min_level, max_level};

            using coord_index_t = typename Config::coord_index_t;
            auto pred_coeff = compute_prediction<coord_index_t>(min_level, max_level);


            // Initialization
            auto f = init_f(mesh, 0);

            double T = 20.0;
            double dx = 1.0 / (1 << max_level);
            double dt = dx / lambda;

            std::size_t N = static_cast<std::size_t>(T / dt);

            for (std::size_t nb_ite = 0; nb_ite < 2; ++nb_ite)
            {
                std::cout <<"Iteration" << nb_ite<<" Time = "<<nb_ite * dt << "\n";


                // const std::size_t how_often_save = 100;
                // if (nb_ite % how_often_save)
                //     save_solution(f, eps, nb_ite / how_often_save);
                save_solution(f, eps, nb_ite);

                for (std::size_t i=0; i<max_level-min_level; ++i)
                {
                    if (coarsening(f, eps, i))
                        break;
                }
                std::cout << "coarsening\n";
                save_solution(f, eps, nb_ite, "coarsening");

                for (std::size_t i=0; i<max_level-min_level; ++i)
                {
                    if (refinement(f, eps, i))
                        break;
                }
                std::cout << "refinement\n";

                save_solution(f, eps, nb_ite, "refinement");

                f.update_bc(); // Very important



                if (nb_ite == 0)    {

                    std::stringstream str;
                    str << "debug_KH";

                    auto h5file = samurai::Hdf5(str.str().data());
                    h5file.add_mesh(mesh);
                    // We save with the levels
                    h5file.add_field_by_level(mesh, f);

                }

                one_time_step(f, pred_coeff);

                // save_solution(f, eps, nb_ite);
            }
        }
    }
    catch (const cxxopts::OptionException &e)
    {
        std::cout << options.help() << "\n";
    }
    return 0;
}
