#include <iostream>
#include <chrono>
#include <vector>

#include <xtensor/xarray.hpp>
#include <xtensor/xtensor.hpp>
#include <xtensor/xio.hpp>
#include <xtensor/xview.hpp>
#include "xtensor/xnoalias.hpp"

#include <samurai/box.hpp>
#include <samurai/level_cell_array.hpp>
#include <samurai/mr_config.hpp>

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


int main()
{
    constexpr size_t dim = 3;
    constexpr size_t level = 8 ;
    constexpr size_t nrun = 10;
    const size_t end = std::pow(2, level);
    using Config = samurai::MRConfig<dim>;
    samurai::Box<int, dim> box({0, 0, 0}, {(int) end, (int) end, (int) end});


    samurai::LevelCellArray<Config> lca = {box};

    auto array_1 = xt::xtensor<double, 1>::from_shape({lca.nb_cells()});
    array_1.fill(1.);
    auto array_2 = xt::xtensor<double, 1>::from_shape({lca.nb_cells()});

    std::cout << "Samurai:\n";
    for(size_t n=0; n<nrun; ++n)
    {
        tic();
        lca.for_each_block([&](auto & load_input, auto & load_output){
            auto view = load_input(array_1);

            xt::noalias(load_output(array_2)) =
                  2.*xt::view(view, xt::range(1, view.shape()[0]-1), xt::range(1, view.shape()[1]-1), xt::range(1, view.shape()[2]-1))
                -    xt::view(view, xt::range(2,   view.shape()[0]), xt::range(1, view.shape()[1]-1), xt::range(1, view.shape()[2]-1))
                -    xt::view(view, xt::range(0, view.shape()[0]-2), xt::range(1, view.shape()[1]-1), xt::range(1, view.shape()[2]-1))
                -    xt::view(view, xt::range(1, view.shape()[0]-1),   xt::range(2, view.shape()[1]), xt::range(1, view.shape()[2]-1))
                -    xt::view(view, xt::range(1, view.shape()[0]-1), xt::range(0, view.shape()[1]-2), xt::range(1, view.shape()[2]-1))
                -    xt::view(view, xt::range(1, view.shape()[0]-1), xt::range(1, view.shape()[1]-1), xt::range(0, view.shape()[2]-2))
                -    xt::view(view, xt::range(1, view.shape()[0]-1), xt::range(1, view.shape()[1]-1), xt::range(2,   view.shape()[2]));

        });
        auto duration = toc();

        std::cout << "\tRun #" << n << " in " << duration << "s (" << xt::sum(array_2) << ")\n";
    }


    auto view = xt::reshape_view(array_1, {end, end, end});
    auto array_3 = xt::xtensor<double, 1>::from_shape({lca.nb_cells()});
    auto array_3_ = xt::reshape_view(array_3, {end, end, end});

    std::cout << "xtensor:\n";
    for(size_t n=0; n<nrun; ++n)
    {
        tic();

        xt::noalias(xt::view(array_3_, xt::range(1, array_3_.shape()[0]-1), xt::range(1, array_3_.shape()[1]-1), xt::range(1, array_3_.shape()[1]-1))) =
                  2.*xt::view(view, xt::range(1, view.shape()[0]-1), xt::range(1, view.shape()[1]-1), xt::range(1, view.shape()[2]-1))
                -    xt::view(view, xt::range(2,   view.shape()[0]), xt::range(1, view.shape()[1]-1), xt::range(1, view.shape()[2]-1))
                -    xt::view(view, xt::range(0, view.shape()[0]-2), xt::range(1, view.shape()[1]-1), xt::range(1, view.shape()[2]-1))
                -    xt::view(view, xt::range(1, view.shape()[0]-1),   xt::range(2, view.shape()[1]), xt::range(1, view.shape()[2]-1))
                -    xt::view(view, xt::range(1, view.shape()[0]-1), xt::range(0, view.shape()[1]-2), xt::range(1, view.shape()[2]-1))
                -    xt::view(view, xt::range(1, view.shape()[0]-1), xt::range(1, view.shape()[1]-1), xt::range(0, view.shape()[2]-2))
                -    xt::view(view, xt::range(1, view.shape()[0]-1), xt::range(1, view.shape()[1]-1), xt::range(2,   view.shape()[2]));

        auto duration = toc();
        std::cout << "\tRun #" << n << " in " << duration << "s (" << xt::sum(array_3) << ")\n";
    }

    std::vector<double> vector_1(lca.nb_cells(), 1.);
    std::vector<double> vector_2(lca.nb_cells(), 0  );

    std::cout << "std::vector:\n";
    for(size_t n=0; n<nrun; ++n)
    {
        tic();
        for(size_t k=1; k<end-1; ++k)
        {
            for(size_t j=1; j<end-1; ++j)
            {
                for(size_t i=1; i<end-1; ++i)
                {
                    vector_2[i + end*(j + k*end)] = 2*vector_1[i + end*(j + k*end)]
                                    -   vector_1[i+1 + end*(j + k*end)]
                                    -   vector_1[i-1 + end*(j + k*end)]
                                    -   vector_1[i + end*(j + 1 + k*end)]
                                    -   vector_1[i + end*(j - 1 + k*end)]
                                    -   vector_1[i + end*(j + (k + 1)*end)]
                                    -   vector_1[i + end*(j + (k - 1)*end)];
                }
            }
        }
        auto duration = toc();
        std::cout << "\tRun #" << n << " in " << duration << "s (" << std::accumulate(vector_2.begin(), vector_2.end(), 0.) << ")\n";
    }
}
