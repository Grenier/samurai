#pragma once

#include <fmt/format.h>

#include "../algorithm.hpp"
#include "../box.hpp"
#include "../interval.hpp"
#include "../mesh.hpp"

namespace samurai
{
    namespace amr
    {
        enum class AMR_Id
        {
            cells = 0,
            cells_and_ghosts = 1,
            proj_cells = 2,
            pred_cells = 3,
            all_cells = 4,
            count = 5,
            reference = all_cells
            // reference = cells_and_ghosts
        };

        template <std::size_t dim_>
        struct Config
        {
            static constexpr std::size_t dim = dim_;
            static constexpr std::size_t max_refinement_level = 20;
            static constexpr int ghost_width = 3;
            static constexpr int prediction_width = 1;

            using interval_t = Interval<int>;
            using mesh_id_t = AMR_Id;
        };

        /////////////////////////
        // AMR mesh definition //
        /////////////////////////

        template <class Config>
        class Mesh: public Mesh_base<Mesh<Config>, Config>
        {
        public:
            using base_type = Mesh_base<Mesh<Config>, Config>;
            using config = typename base_type::config;
            static constexpr std::size_t dim = config::dim;

            using mesh_id_t = typename base_type::mesh_id_t;
            using cl_type = typename base_type::cl_type;
            using lcl_type = typename base_type::lcl_type;

            using ca_type = typename base_type::ca_type;

            Mesh(const cl_type &cl, std::size_t min_level, std::size_t max_level);
            Mesh(const Box<double, dim>& b, std::size_t start_level, std::size_t min_level, std::size_t max_level);

            void update_sub_mesh_impl();
        };

        /////////////////////////////
        // AMR mesh implementation //
        /////////////////////////////

        template <class Config>
        inline Mesh<Config>::Mesh(const cl_type &cl, std::size_t min_level, std::size_t max_level)
        : base_type(cl, min_level, max_level)
        {}

        template <class Config>
        inline Mesh<Config>::Mesh(const Box<double, dim>& b, std::size_t start_level, std::size_t min_level, std::size_t max_level)
        : base_type(b, start_level, min_level, max_level)
        {}

        template <class Config>
        inline void Mesh<Config>::update_sub_mesh_impl()
        {
            cl_type cl;
            for_each_interval(this->m_cells[mesh_id_t::cells], [&](std::size_t level, const auto& interval, const auto& index_yz)
            {
                lcl_type& lcl = cl[level];
                // add ghosts for the scheme in space using stencil star
                // in x direction
                lcl[index_yz].add_interval({interval.start - config::ghost_width,
                                            interval.end + config::ghost_width});
                // in y direction
                static_nested_loop<dim - 1, -config::ghost_width, config::ghost_width + 1>([&](auto stencil)
                {
                    auto index = xt::eval(index_yz + stencil);
                    lcl[index].add_interval(interval);
                });
                // add ghosts for the prediction
                static_nested_loop<dim - 1, -config::prediction_width, config::prediction_width + 1>([&](auto stencil)
                {
                    auto index = xt::eval(index_yz + stencil);
                    lcl[index].add_interval({interval.start - config::prediction_width,
                                            interval.end + config::prediction_width});
                });

            });
            this->m_cells[mesh_id_t::cells_and_ghosts] = {cl, false};


            auto max_level = this->m_cells[mesh_id_t::cells].max_level();
            auto min_level = this->m_cells[mesh_id_t::cells].min_level();
            // Construct union cells
            ca_type union_cells;
            union_cells[max_level] = {max_level};

            for (std::size_t level = max_level; level >= ((min_level == 0) ? 1 : min_level); --level)
            {
                lcl_type lcl{level - 1};
                auto expr = union_(this->m_cells[mesh_id_t::cells][level],
                                union_cells[level])
                        .on(level - 1);

                expr([&](const auto& interval, const auto& index_yz)
                {
                    lcl[index_yz].add_interval(interval);
                });

                union_cells[level - 1] = {lcl};
            }

            // construction of projection cells
            this->m_cells[mesh_id_t::proj_cells][min_level] = {min_level};
            for (std::size_t level = min_level + 1; level <= max_level; ++level)
            {
                auto expr = difference(union_(intersection(this->m_cells[mesh_id_t::cells_and_ghosts][level - 1],
                                                union_cells[level - 1]),
                                    this->m_cells[mesh_id_t::proj_cells][level - 1]),
                                    this->m_cells[mesh_id_t::cells][level - 1])
                            .on(level);

                lcl_type lcl{level};
                expr([&](const auto& interval, const auto& index_yz)
                {
                    lcl[index_yz].add_interval({interval.start, interval.end});
                });

                this->m_cells[mesh_id_t::proj_cells][level] = {lcl};
            }

            // construction of prediction cells
            for (std::size_t level = min_level; level <= max_level; ++level)
            {
                auto expr = intersection(difference(this->m_cells[mesh_id_t::cells_and_ghosts][level],
                                                    union_(union_cells[level], this->m_cells[mesh_id_t::cells][level])),
                                        this->m_domain)
                            .on(level);

                lcl_type lcl{level};
                expr([&](const auto& interval, const auto& index_yz)
                {
                    lcl[index_yz].add_interval(interval);
                });

                this->m_cells[mesh_id_t::pred_cells][level] = {lcl};
            }

            for (std::size_t level = min_level; level <= max_level; ++level)
            {
                auto expr = intersection(this->m_cells[mesh_id_t::pred_cells][level],
                                        this->m_cells[mesh_id_t::pred_cells][level])
                            .on(level-1);

                lcl_type& lcl = cl[level-1];

                expr([&](const auto& interval, const auto& index_yz)
                {
                    // add ghosts for the prediction
                    static_nested_loop<dim - 1, -config::prediction_width, config::prediction_width + 1>([&](auto stencil)
                    {
                        auto index = xt::eval(index_yz + stencil);
                        lcl[index].add_interval({interval.start - config::prediction_width,
                                                interval.end + config::prediction_width});
                    });
                });
            }
            this->m_cells[mesh_id_t::cells_and_ghosts] = {cl, false};

            for (std::size_t level = min_level; level <= max_level; ++level)
            {
                lcl_type lcl{level};
                auto expr = union_(this->m_cells[mesh_id_t::cells_and_ghosts][level],
                                        this->m_cells[mesh_id_t::proj_cells][level]);
                expr([&](const auto& interval, const auto& index_yz)
                {
                    lcl[index_yz].add_interval(interval);
                });

                this->m_cells[mesh_id_t::all_cells][level] = {lcl};
            }
        }
    }
}

template <>
struct fmt::formatter<samurai::amr::AMR_Id>: formatter<string_view>
{
    template <typename FormatContext>
    auto format(samurai::amr::AMR_Id c, FormatContext& ctx)
    {
        string_view name = "unknown";
        switch (c) {
        case samurai::amr::AMR_Id::cells:            name = "cells"; break;
        case samurai::amr::AMR_Id::cells_and_ghosts: name = "cells and ghosts"; break;
        case samurai::amr::AMR_Id::proj_cells:       name = "proj cells"; break;
        case samurai::amr::AMR_Id::pred_cells:       name = "pred cells"; break;
        case samurai::amr::AMR_Id::all_cells:        name = "all cells"; break;
        }
        return formatter<string_view>::format(name, ctx);
    }
};

