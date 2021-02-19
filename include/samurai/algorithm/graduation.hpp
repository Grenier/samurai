// Copyright 2021 SAMURAI TEAM. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#pragma once

#include <xtensor/xtensor.hpp>
#include <xtensor/xmasked_view.hpp>

#include "../subset/subset_op.hpp"
#include "../mr/cell_flag.hpp"

namespace samurai
{
    ///////////////////////
    // graduate operator //
    ///////////////////////

    template<class TInterval>
    class graduate_op : public field_operator_base<TInterval>
    {
    public:

        INIT_OPERATOR(graduate_op)

        template <class T, class Stencil>
        inline void operator()(Dim<1>, T& tag, const Stencil& s) const
        {
            auto mask = tag(level, i  - s[0]) & static_cast<int>(CellFlag::refine);
            auto i_c = i >> 1;
            xt::masked_view(tag(level - 1, i_c), mask) |= static_cast<int>(CellFlag::refine);

            mask = tag(level, i  - s[0]) & static_cast<int>(CellFlag::keep);
            xt::masked_view(tag(level - 1, i_c), mask) |= static_cast<int>(CellFlag::keep);
        }

        template <class T, class Stencil>
        inline void operator()(Dim<2>, T& tag, const Stencil& s) const
        {
            auto i_f = i.even_elements();
            auto j_f = j;

            if (i_f.is_valid())
            {
                auto mask = tag(level, i_f  - s[0], j_f - s[1]) & static_cast<int>(CellFlag::refine);
                auto i_c = i_f >> 1;
                auto j_c = j_f >> 1;
                xt::masked_view(tag(level - 1, i_c, j_c), mask) |= static_cast<int>(CellFlag::refine);

                mask = tag(level, i_f  - s[0], j_f - s[1]) & static_cast<int>(CellFlag::keep);
                xt::masked_view(tag(level - 1, i_c, j_c), mask) |= static_cast<int>(CellFlag::keep);
            }

            i_f = i.odd_elements();
            if (i_f.is_valid())
            {
                auto mask = tag(level, i_f  - s[0], j_f - s[1]) & static_cast<int>(CellFlag::refine);
                auto i_c = i_f >> 1;
                auto j_c = j_f >> 1;
                xt::masked_view(tag(level - 1, i_c, j_c), mask) |= static_cast<int>(CellFlag::refine);

                mask = tag(level, i_f  - s[0], j_f - s[1]) & static_cast<int>(CellFlag::keep);
                xt::masked_view(tag(level - 1, i_c, j_c), mask) |= static_cast<int>(CellFlag::keep);
            }
        }

        template <class T, class Stencil>
        inline void operator()(Dim<3>, T& tag, const Stencil& s) const
        {
            auto i_f = i.even_elements();
            auto j_f = j;
            auto k_f = k;

            if (i_f.is_valid())
            {
                auto mask = tag(level, i_f  - s[0], j_f - s[1], k_f - s[2]) & static_cast<int>(CellFlag::refine);
                auto i_c = i_f >> 1;
                auto j_c = j_f >> 1;
                auto k_c = k_f >> 1;
                xt::masked_view(tag(level - 1, i_c, j_c, k_c), mask) |= static_cast<int>(CellFlag::refine);

                mask = tag(level, i_f  - s[0], j_f - s[1], k_f - s[2]) & static_cast<int>(CellFlag::keep);
                xt::masked_view(tag(level - 1, i_c, j_c, k_c), mask) |= static_cast<int>(CellFlag::keep);
            }

            i_f = i.odd_elements();
            if (i_f.is_valid())
            {
                auto mask = tag(level, i_f  - s[0], j_f - s[1], k_f - s[2]) & static_cast<int>(CellFlag::refine);
                auto i_c = i_f >> 1;
                auto j_c = j_f >> 1;
                auto k_c = k_f >> 1;
                xt::masked_view(tag(level - 1, i_c, j_c, k_c), mask) |= static_cast<int>(CellFlag::refine);

                mask = tag(level, i_f  - s[0], j_f - s[1], k_f - s[2]) & static_cast<int>(CellFlag::keep);
                xt::masked_view(tag(level - 1, i_c, j_c, k_c), mask) |= static_cast<int>(CellFlag::keep);
            }
        }
    };

    template<class T, class Stencil>
    inline auto graduate(T& tag, const Stencil& s)
    {
        return make_field_operator_function<graduate_op>(tag, s);
    }

    template<class Tag>
    void graduation(Tag& tag)
    {
        auto mesh = tag.mesh();
        constexpr std::size_t dim = Tag::dim;
        using mesh_t = typename Tag::mesh_t;
        using mesh_id_t = typename Tag::mesh_t::mesh_id_t;

        std::size_t min_level = mesh.min_level();
        std::size_t max_level = mesh.max_level();

        constexpr int ghost_width = 1; //mesh_t::config::ghost_width;

        for(std::size_t level = max_level; level > min_level; --level)
        {
            /**
             *
             *        |-----|-----|                                  |-----|-----|
             *                                    --------------->
             *                                                             K
             *        |===========|-----------|                      |===========|-----------|
             */

            auto ghost_subset = intersection(mesh[mesh_id_t::cells][level],
                                             mesh[mesh_id_t::reference][level-1])
                              .on(level - 1);

            ghost_subset.apply_op(tag_to_keep<0>(tag));

            /**
             *                 R                                 K     R     K
             *        |-----|-----|=====|   --------------->  |-----|-----|=====|
             *
             */

            auto subset_2 = intersection(mesh[mesh_id_t::cells][level],
                                         mesh[mesh_id_t::cells][level]);

            subset_2.apply_op(tag_to_keep<ghost_width>(tag, CellFlag::refine));

            /**
             *      K     C                          K     K
             *   |-----|-----|   -------------->  |-----|-----|
             *
             *   |-----------|
             *
             */

            auto keep_subset = intersection(mesh[mesh_id_t::cells][level],
                                            mesh[mesh_id_t::cells][level])
                            .on(level - 1);
            keep_subset.apply_op(keep_children_together(tag));

            /**
             * Case 1
             * ======
             *                   R     K                                             R     K
             *                |-----|-----|   -------------->                     |-----|-----|
             *       C or K                                                 R
             *   |-----------|                                        |-----------|
             *
             * Case 2
             * ======
             *                   K     K                                             K     K
             *                |-----|-----|   -------------->                     |-----|-----|
             *         C                                                    K
             *   |-----------|                                        |-----------|
             *
             */


            // xt::xtensor_fixed<int, xt::xshape<4, dim>> stencil{{3, 3}, {-3, -3}, {-3, 3}, {3, -3}};
            xt::xtensor_fixed<int, xt::xshape<4, dim>> stencil{{1, 1}, {-1, -1}, {-1, 1}, {1, -1}};
            // xt::xtensor_fixed<int, xt::xshape<4, dim>> stencil{{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

            for(std::size_t i = 0; i < stencil.shape()[0]; ++i)
            {
                auto s = xt::view(stencil, i);
                auto subset = intersection(translate(mesh[mesh_id_t::cells][level], s),
                                                     mesh[mesh_id_t::cells][level - 1])
                            .on(level);
                subset.apply_op(graduate(tag, s));
            }
        }
    }
}