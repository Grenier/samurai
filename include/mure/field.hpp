#pragma once

#include <array>
#include <memory>

#include <spdlog/spdlog.h>

#include <xtensor/xfixed.hpp>
#include <xtensor/xview.hpp>

#include "cell.hpp"
#include "field_expression.hpp"
#include "mr/mesh.hpp"
#include "mr/mesh_type.hpp"

namespace mure
{
    template<class MRConfig, class value_t>
    class Field : public field_expression<Field<MRConfig, value_t>> {
      public:
        static constexpr auto dim = MRConfig::dim;
        static constexpr auto max_refinement_level =
            MRConfig::max_refinement_level;

        using value_type = value_t;
        using data_type = xt::xtensor<value_type, 1>;
        using view_type =
            decltype(xt::view(std::declval<data_type &>(), xt::range(0, 1, 1)));

        using coord_index_t = typename MRConfig::coord_index_t;
        using index_t = typename MRConfig::index_t;
        using interval_t = typename MRConfig::interval_t;

        inline Field(std::string name, Mesh<MRConfig> &mesh)
            : name_(name), m_mesh(&mesh),
              m_data(std::array<std::size_t, 1>{mesh.nb_total_cells()})
        {
            m_data.fill(0);
        }

        template<class E>
        inline Field &operator=(const field_expression<E> &e)
        {
            // mesh->for_each_cell(
            //     [&](auto &cell) { (*this)[cell] = e.derived_cast()(cell); });

            for (std::size_t level = 0; level <= max_refinement_level; ++level)
            {
                auto subset = intersection((*m_mesh)[MeshType::cells][level],
                                           (*m_mesh)[MeshType::cells][level]);

                subset.apply_op(level, apply_expr(*this, e));
            }
            return *this;
        }

        inline value_type const operator()(const Cell<coord_index_t, dim> &cell) const
        {
            return m_data[cell.index];
        }

        inline value_type &operator()(const Cell<coord_index_t, dim> &cell)
        {
            return m_data[cell.index];
        }

        inline value_type const operator[](const Cell<coord_index_t, dim> &cell) const
        {
            return m_data[cell.index];
        }

        inline value_type &operator[](const Cell<coord_index_t, dim> &cell)
        {
            return m_data[cell.index];
        }

        template<class... T>
        inline auto operator()(interval_t interval, T... index) const
        {
            return xt::view(m_data, xt::range(interval.start, interval.end));
        }

        template<class... T>
        inline auto operator()(interval_t interval, T... index)
        {
            return xt::view(m_data, xt::range(interval.start, interval.end));
        }

        template<class... T>
        inline auto operator()(const std::size_t level, const interval_t &interval,
                        const T... index)
        {
            auto interval_tmp = m_mesh->get_interval(level, interval, index...);
            if ((interval_tmp.end - interval_tmp.step <
                 interval.end - interval.step) or
                (interval_tmp.start > interval.start))
            {
                spdlog::critical("WRITE FIELD ERROR on level {} for "
                                 "interval_tmp {} and interval {}",
                                 level, interval_tmp, interval);
            }
            return xt::view(m_data,
                            xt::range(interval_tmp.index + interval.start,
                                      interval_tmp.index + interval.end,
                                      interval.step));
        }

        template<class... T>
        inline auto operator()(const std::size_t level, const interval_t &interval,
                        const T... index) const
        {
            auto interval_tmp = m_mesh->get_interval(level, interval, index...);
            if ((interval_tmp.end - interval_tmp.step <
                 interval.end - interval.step) or
                (interval_tmp.start > interval.start))
            {
                spdlog::critical("READ FIELD ERROR on level {} for "
                                 "interval_tmp {} and interval {}",
                                 level, interval_tmp, interval);
            }
            return xt::view(m_data,
                            xt::range(interval_tmp.index + interval.start,
                                      interval_tmp.index + interval.end,
                                      interval.step));
        }

        inline auto data(MeshType mesh_type) const
        {
            std::array<std::size_t, 1> shape = {m_mesh->nb_cells(mesh_type)};
            xt::xtensor<double, 1> output(shape);
            std::size_t index = 0;
            m_mesh->for_each_cell(
                [&](auto cell) { output[index++] = m_data[cell.index]; },
                mesh_type);
            return output;
        }

        inline auto data_on_level(std::size_t level, MeshType mesh_type) const
        {
            std::array<std::size_t, 1> shape = {
                m_mesh->nb_cells(level, mesh_type)};
            xt::xtensor<double, 1> output(shape);
            std::size_t index = 0;
            m_mesh->for_each_cell(
                level, [&](auto cell) { output[index++] = m_data[cell.index]; },
                mesh_type);
            return output;
        }

        inline auto const array() const
        {
            return m_data;
        }

        inline auto &array()
        {
            return m_data;
        }

        inline std::size_t nb_cells(MeshType mesh_type) const
        {
            return m_mesh->nb_cells(mesh_type);
        }

        inline std::size_t nb_cells(std::size_t level, MeshType mesh_type) const
        {
            return m_mesh->nb_cells(level, mesh_type);
        }

        inline auto const &name() const
        {
            return name_;
        }

        inline auto mesh()
        {
            return *m_mesh;
        }

        inline auto mesh_ptr()
        {
            return m_mesh;
        }

        inline void to_stream(std::ostream &os) const
        {
            os << "Field " << name_ << "\n";
            m_mesh->for_each_cell(
                [&](auto &cell) {
                    os << cell.level << "[" << cell.center()
                       << "]:" << m_data[cell.index] << "\n";
                },
                // MeshType::all_cells);
                MeshType::cells);
        }

      private:
        std::string name_;
        Mesh<MRConfig> *m_mesh;
        data_type m_data;
    };

    template<class MRConfig, class T>
    inline std::ostream &operator<<(std::ostream &out, const Field<MRConfig, T> &field)
    {
        field.to_stream(out);
        return out;
    }
}