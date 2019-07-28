#pragma once

#include <memory>
#include <type_traits>

#include <xtensor/xexpression.hpp>
#include <xtl/xtype_traits.hpp>

#include "../level_cell_array.hpp"
#include "../utils.hpp"

namespace mure
{
    /**********************
     * node_op definition *
     **********************/

    template<class D>
    class node_op {
      public:
        using derived_type = D;
        static constexpr std::size_t dim = derived_type::dim;

        derived_type &derived_cast() & noexcept;
        const derived_type &derived_cast() const &noexcept;
        derived_type derived_cast() && noexcept;

        auto index(int i) const noexcept;
        auto size(std::size_t dim) const noexcept;
        auto start(std::size_t dim, std::size_t index) const noexcept;
        auto end(std::size_t dim, std::size_t index) const noexcept;
        auto offset(std::size_t dim, std::size_t index) const noexcept;
        auto interval(std::size_t dim, std::size_t index) const noexcept;
        auto offsets_size(std::size_t dim) const noexcept;
        auto data() const noexcept;

        template<class Mesh>
        void data(Mesh &mesh) noexcept;

        template<class T>
        auto create_interval(T start, T end) const noexcept;
        auto create_index_yz() const noexcept;

        std::size_t level() const noexcept;

        template<class Func>
        void for_each_interval_in_x(Func &&f) const;

      protected:
        node_op(){};
        ~node_op() = default;

        node_op(const node_op &) = default;
        node_op &operator=(const node_op &) = default;

        node_op(node_op &&) = default;
        node_op &operator=(node_op &&) = default;

        template<class Func, class index_t, std::size_t N>
        void for_each_interval_in_x_impl(
            Func &&f, index_t &index, std::size_t start_index,
            std::size_t end_index,
            std::integral_constant<std::size_t, N>) const;

        template<class Func, class index_t>
        void for_each_interval_in_x_impl(
            Func &&f, index_t &index, std::size_t start_index,
            std::size_t end_index,
            std::integral_constant<std::size_t, 0>) const;
    };

    /**************************
     * node_op implementation *
     **************************/

    template<class D>
        inline auto node_op<D>::derived_cast() & noexcept -> derived_type &
    {
        return *static_cast<derived_type *>(this);
    }

    template<class D>
        inline auto node_op<D>::derived_cast() const &
        noexcept -> const derived_type &
    {
        return *static_cast<const derived_type *>(this);
    }

    template<class D>
        inline auto node_op<D>::derived_cast() && noexcept -> derived_type
    {
        return *static_cast<derived_type *>(this);
    }

    template<class D>
    inline auto node_op<D>::index(int i) const noexcept
    {
        return this->derived_cast().m_data.index(i);
    }

    template<class D>
    inline auto node_op<D>::size(std::size_t dim) const noexcept
    {
        return this->derived_cast().m_data.size(dim);
    }

    template<class D>
    inline auto node_op<D>::start(std::size_t dim, std::size_t index) const
        noexcept
    {
        return this->derived_cast().m_data.start(dim, index);
    }

    template<class D>
    inline auto node_op<D>::end(std::size_t dim, std::size_t index) const
        noexcept
    {
        return this->derived_cast().m_data.end(dim, index);
    }

    template<class D>
    inline auto node_op<D>::offset(std::size_t dim, std::size_t index) const
        noexcept
    {
        return this->derived_cast().m_data.offset(dim, index);
    }

    template<class D>
    inline auto node_op<D>::interval(std::size_t dim, std::size_t index) const
        noexcept
    {
        return this->derived_cast().m_data.interval(dim, index);
    }

    template<class D>
    inline auto node_op<D>::offsets_size(std::size_t dim) const noexcept
    {
        return this->derived_cast().m_data.offsets_size(dim);
    }

    template<class D>
    inline auto node_op<D>::data() const noexcept
    {
        return this->derived_cast().m_data.data();
    }

    template<class D>
    template<class Mesh>
    inline void node_op<D>::data(Mesh &mesh) noexcept
    {
        return this->derived_cast().m_data.data(mesh);
    }

    template<class D>
    inline std::size_t node_op<D>::level() const noexcept
    {
        return this->derived_cast().m_data.level();
    }

    template<class D>
    template<class T>
    inline auto node_op<D>::create_interval(T start, T end) const noexcept
    {
        return this->derived_cast().m_data.create_interval(start, end);
    }

    template<class D>
    inline auto node_op<D>::create_index_yz() const noexcept
    {
        return this->derived_cast().create_index_yz();
    }

    template<class D>
    template<class Func>
    void node_op<D>::for_each_interval_in_x(Func &&f) const
    {
        auto index_yz = this->derived_cast().create_index_yz();
        for_each_interval_in_x_impl(
            std::forward<Func>(f), index_yz, 0,
            this->derived_cast().size(dim - 1),
            std::integral_constant<std::size_t, dim - 1>{});
    }

    template<class D>
    template<class Func, class index_t, std::size_t N>
    void node_op<D>::for_each_interval_in_x_impl(
        Func &&f, index_t &index, std::size_t start_index,
        std::size_t end_index, std::integral_constant<std::size_t, N>) const
    {
        for (std::size_t i = start_index; i < end_index; ++i)
        {
            auto interval = this->derived_cast().interval(N, i);
            auto start = this->derived_cast().start(N, i);
            for (auto c = interval.start, cc = 0; c < interval.end; ++c, ++cc)
            {
                index[N - 1] = start + cc;
                auto off_ind = static_cast<std::size_t>(interval.index + c);
                for_each_interval_in_x_impl(
                    std::forward<Func>(f), index,
                    this->derived_cast().offset(N, off_ind),
                    this->derived_cast().offset(N, off_ind + 1),
                    std::integral_constant<std::size_t, N - 1>{});
            }
        }
    }

    template<class D>
    template<class Func, class index_t>
    void node_op<D>::for_each_interval_in_x_impl(
        Func &&f, index_t &index, std::size_t start_index,
        std::size_t end_index, std::integral_constant<std::size_t, 0>) const
    {
        for (std::size_t i = start_index; i < end_index; ++i)
        {
            auto interval = this->derived_cast().create_interval(
                this->derived_cast().start(0, i),
                this->derived_cast().end(0, i));
            f(index, interval);
        }
    }

    template<class E>
    using is_node_op = xt::is_crtp_base_of<node_op, E>;

    /************************
     * mesh_node definition *
     ************************/

    template<class Mesh>
    struct mesh_node : public node_op<mesh_node<Mesh>>
    {
        using mesh_type = Mesh;
        static constexpr std::size_t dim = mesh_type::dim;
        using interval_t = typename mesh_type::interval_t;
        using coord_index_t = typename mesh_type::coord_index_t;

        mesh_node(const Mesh &v);

        mesh_node() : m_data{nullptr}
        {}

        mesh_node(const mesh_node &) = default;
        mesh_node &operator=(const mesh_node &) = default;

        mesh_node(mesh_node &&) = default;
        mesh_node &operator=(mesh_node &&) = default;

        auto index(int i) const noexcept;
        auto size(std::size_t dim) const noexcept;
        auto start(std::size_t dim, std::size_t index) const noexcept;
        auto end(std::size_t dim, std::size_t index) const noexcept;
        auto offset(std::size_t dim, std::size_t off_ind) const noexcept;
        auto offsets_size(std::size_t dim) const noexcept;
        auto interval(std::size_t dim, std::size_t index) const noexcept;
        const Mesh &data() const noexcept;
        void data(Mesh &mesh) noexcept;
        std::size_t level() const noexcept;

        auto create_interval(coord_index_t start, coord_index_t end) const
            noexcept;
        auto create_index_yz() const noexcept;

      private:
        std::shared_ptr<Mesh> m_data;

        friend class node_op<mesh_node<Mesh>>;
    };

    /****************************
     * mesh_node implementation *
     ****************************/

    template<class Mesh>
    inline mesh_node<Mesh>::mesh_node(const Mesh &v)
        : m_data{std::make_shared<Mesh>(v)}
    {}

    template<class Mesh>
    inline auto mesh_node<Mesh>::index(int i) const noexcept
    {
        return i;
    }

    template<class Mesh>
    inline auto mesh_node<Mesh>::size(std::size_t dim) const noexcept
    {
        return (*m_data)[dim].size();
    }

    template<class Mesh>
    inline auto mesh_node<Mesh>::start(std::size_t dim, std::size_t index) const
        noexcept
    {
        if (m_data->empty())
        {
            return std::numeric_limits<coord_index_t>::max();
        }
        return (*m_data)[dim][index].start;
    }

    template<class Mesh>
    inline auto mesh_node<Mesh>::end(std::size_t dim, std::size_t index) const
        noexcept
    {
        if (m_data->empty())
        {
            return std::numeric_limits<coord_index_t>::max();
        }
        return (*m_data)[dim][index].end;
    }

    template<class Mesh>
    inline auto mesh_node<Mesh>::offset(std::size_t dim,
                                        std::size_t off_ind) const noexcept
    {
        return m_data->offsets(dim)[off_ind];
    }

    template<class Mesh>
    inline auto mesh_node<Mesh>::offsets_size(std::size_t dim) const noexcept
    {
        return m_data->offsets(dim).size();
    }

    template<class Mesh>
    inline auto mesh_node<Mesh>::interval(std::size_t dim,
                                          std::size_t index) const noexcept
    {
        return (*m_data)[dim][index];
    }

    template<class Mesh>
    inline const Mesh &mesh_node<Mesh>::data() const noexcept
    {
        return *(m_data.get());
    }

    template<class Mesh>
    inline void mesh_node<Mesh>::data(Mesh &mesh) noexcept
    {
        m_data = std::make_shared<Mesh>(mesh);
    }

    template<class Mesh>
    inline std::size_t mesh_node<Mesh>::level() const noexcept
    {
        return m_data->get_level();
    }

    template<class Mesh>
    inline auto mesh_node<Mesh>::create_interval(coord_index_t start,
                                                 coord_index_t end) const
        noexcept
    {
        return interval_t{start, end};
    }

    template<class Mesh>
    inline auto mesh_node<Mesh>::create_index_yz() const noexcept
    {
        return xt::xtensor_fixed<coord_index_t, xt::xshape<dim - 1>>{};
    }

    /***************************
     * translate_op definition *
     ***************************/

    template<int x, int y, int z, class T>
    struct translate_op : public node_op<translate_op<x, y, z, T>>
    {
        using mesh_type = typename T::mesh_type;
        static constexpr std::size_t dim = mesh_type::dim;
        using interval_t = typename mesh_type::interval_t;
        using coord_index_t = typename mesh_type::coord_index_t;

        translate_op(T &&v);
        translate_op(const T &v);

        auto start(std::size_t dim, std::size_t index) const noexcept;
        auto end(std::size_t dim, std::size_t index) const noexcept;

        auto create_interval(coord_index_t start, coord_index_t end) const
            noexcept;
        auto create_index_yz() const noexcept;

      private:
        T m_data;

        friend class node_op<translate_op<x, y, z, T>>;
    };

    /*******************************
     * translate_op implementation *
     *******************************/

    template<int x, int y, int z, class T>
    inline translate_op<x, y, z, T>::translate_op(T &&v)
        : m_data{std::forward<T>(v)}
    {}

    template<int x, int y, int z, class T>
    inline translate_op<x, y, z, T>::translate_op(const T &v) : m_data{v}
    {}

    template<int x, int y, int z, class T>
    inline auto translate_op<x, y, z, T>::start(std::size_t dim,
                                                std::size_t index) const
        noexcept
    {
        if (dim == 0)
            return m_data.start(dim, index) + x;
        if (dim == 1)
            return m_data.start(dim, index) + y;
        if (dim == 2)
            return m_data.start(dim, index) + z;
    }

    template<int x, int y, int z, class T>
    inline auto translate_op<x, y, z, T>::end(std::size_t dim,
                                              std::size_t index) const noexcept
    {
        if (dim == 0)
            return m_data.end(dim, index) + x;
        if (dim == 1)
            return m_data.end(dim, index) + y;
        if (dim == 2)
            return m_data.end(dim, index) + z;
    }

    template<int x, int y, int z, class T>
    inline auto
    translate_op<x, y, z, T>::create_interval(coord_index_t start,
                                              coord_index_t end) const noexcept
    {
        return interval_t{start, end};
    }

    template<int x, int y, int z, class T>
    inline auto translate_op<x, y, z, T>::create_index_yz() const noexcept
    {
        return xt::xtensor_fixed<coord_index_t, xt::xshape<dim - 1>>{};
    }

    /*****************************
     * contraction_op definition *
     *****************************/

    template<class T>
    struct contraction_op : public node_op<contraction_op<T>>
    {
        using mesh_type = typename T::mesh_type;
        static constexpr std::size_t dim = mesh_type::dim;
        using interval_t = typename mesh_type::interval_t;
        using coord_index_t = typename mesh_type::coord_index_t;

        contraction_op(T &&v);
        contraction_op(const T &v);

        auto start(std::size_t dim, std::size_t index) const noexcept;
        auto end(std::size_t dim, std::size_t index) const noexcept;

      private:
        T m_data;

        friend class node_op<contraction_op<T>>;
    };

    /*********************************
     * contraction_op implementation *
     *********************************/

    template<class T>
    inline contraction_op<T>::contraction_op(T &&v) : m_data{std::forward<T>(v)}
    {}

    template<class T>
    inline contraction_op<T>::contraction_op(const T &v) : m_data{v}
    {}

    template<class T>
    inline auto contraction_op<T>::start(std::size_t dim,
                                         std::size_t index) const noexcept
    {
        return m_data.start(dim, index) + 1;
    }

    template<class T>
    inline auto contraction_op<T>::end(std::size_t dim, std::size_t index) const
        noexcept
    {
        return m_data.end(dim, index) - 1;
    }

    /****************************
     * projection_op definition *
     ****************************/

    template<class T>
    struct projection_op : public node_op<projection_op<T>>
    {
        using mesh_type = typename T::mesh_type;
        static constexpr std::size_t dim = mesh_type::dim;
        using interval_t = typename mesh_type::interval_t;
        using coord_index_t = typename mesh_type::coord_index_t;

        projection_op(std::size_t ref_level, T &&v);
        projection_op(std::size_t ref_level, const T &v);

        auto index(int i) const noexcept;
        auto size(std::size_t dim) const noexcept;
        auto start(std::size_t dim, std::size_t index) const noexcept;
        auto end(std::size_t dim, std::size_t index) const noexcept;
        auto offset(std::size_t dim, std::size_t off_ind) const noexcept;
        auto offsets_size(std::size_t dim) const noexcept;
        auto interval(std::size_t dim, std::size_t index) const noexcept;
        const mesh_type &data() const noexcept;
        std::size_t level() const noexcept;

      private:
        T m_data;
        int m_shift;
        std::size_t m_ref_level;
        mesh_type m_mesh;
        mesh_node<mesh_type> m_node;
        void make_projection();

        template<class LevelCellList, class index_t>
        void add_nodes(LevelCellList &lcl, const index_t &index_yz,
                       const interval_t &interval, Dim<1>) const;
        template<class LevelCellList, class index_t>
        void add_nodes(LevelCellList &lcl, const index_t &index_yz,
                       const interval_t &interval, Dim<2>) const;
        template<class LevelCellList, class index_t>
        void add_nodes(LevelCellList &lcl, const index_t &index_yz,
                       const interval_t &interval, Dim<3>) const;

        friend class node_op<projection_op<T>>;
    };

    /********************************
     * projection_op implementation *
     ********************************/

    template<class T>
    inline projection_op<T>::projection_op(std::size_t ref_level, T &&v)
        : m_ref_level{ref_level}, m_data{std::forward<T>(v)}
    {
        m_shift = m_data.level() - ref_level;
        make_projection();
    }

    template<class T>
    inline projection_op<T>::projection_op(std::size_t ref_level, const T &v)
        : m_ref_level{ref_level}, m_data{v}
    {
        m_shift = m_data.level() - ref_level;
        make_projection();
    }

    template<class T>
    template<class LevelCellList, class index_t>
    void projection_op<T>::add_nodes(LevelCellList &lcl,
                                     const index_t &index_yz,
                                     const interval_t &interval, Dim<1>) const
    {
        lcl[{}].add_interval(
            {interval.start << -m_shift, interval.end << -m_shift});
    }

    template<class T>
    template<class LevelCellList, class index_t>
    void projection_op<T>::add_nodes(LevelCellList &lcl,
                                     const index_t &index_yz,
                                     const interval_t &interval, Dim<2>) const
    {
        for (int j = 0; j < 2 * -m_shift; ++j)
        {
            lcl[xt::eval((index_yz << -m_shift) + j)].add_interval(
                {interval.start << -m_shift, interval.end << -m_shift});
        }
    }

    template<class T>
    template<class LevelCellList, class index_t>
    void projection_op<T>::add_nodes(LevelCellList &lcl,
                                     const index_t &index_yz,
                                     const interval_t &interval, Dim<3>) const
    {
        for (int k = 0; k < 2 * -m_shift; ++k)
        {
            for (int j = 0; j < 2 * -m_shift; ++j)
            {
                xt::xtensor_fixed<coord_index_t, xt::xshape<dim - 1>> ind{j, k};
                lcl[xt::eval((index_yz << -m_shift) + ind)].add_interval(
                    {interval.start << -m_shift, interval.end << -m_shift});
            }
        }
    }

    template<class T>
    inline void projection_op<T>::make_projection()
    {
        m_mesh = m_data.data();
        if (m_shift > 0)
        {
            LevelCellList<dim, interval_t> lcl{m_ref_level};
            m_data.for_each_interval_in_x(
                [&](auto const &index_yz, auto const &interval) {
                    auto new_start = interval.start >> m_shift;
                    auto new_end = interval.end >> m_shift;
                    if (new_start == new_end)
                    {
                        new_end++;
                    }
                    lcl[index_yz >> m_shift].add_interval({new_start, new_end});
                });
            m_mesh = {lcl};
            m_node = {m_mesh};
        }
        else if (m_shift < 0)
        {
            LevelCellList<dim, interval_t> lcl{m_ref_level};
            m_data.for_each_interval_in_x(
                [&](auto const &index_yz, auto const &interval) {
                    add_nodes(lcl, index_yz, interval, Dim<dim>{});
                });
            m_mesh = {lcl};
            m_node = {m_mesh};
        }
        // std::cout << m_shift << " " << m_mesh << m_data.data() << "\n";
    }

    template<class T>
    inline auto projection_op<T>::index(int i) const noexcept
    {
        if (m_shift == 0)
            return m_data.index(i);
        else
            return m_node.index(i);
    }

    template<class T>
    inline auto projection_op<T>::size(std::size_t dim) const noexcept
    {
        if (m_shift == 0)
            return m_data.size(dim);
        else
            return m_node.size(dim);
    }

    template<class T>
    inline auto projection_op<T>::start(std::size_t dim,
                                        std::size_t index) const noexcept
    {
        if (m_shift == 0)
            return m_data.start(dim, index);
        else
            return m_node.start(dim, index);
    }

    template<class T>
    inline auto projection_op<T>::end(std::size_t dim, std::size_t index) const
        noexcept
    {
        if (m_shift == 0)
            return m_data.end(dim, index);
        else
            return m_node.end(dim, index);
    }

    template<class T>
    inline auto projection_op<T>::offset(std::size_t dim,
                                         std::size_t off_ind) const noexcept
    {
        if (m_shift == 0)
            return m_data.offset(dim, off_ind);
        else
            return m_node.offset(dim, off_ind);
    }

    template<class T>
    inline auto projection_op<T>::offsets_size(std::size_t dim) const noexcept
    {
        if (m_shift == 0)
            return m_data.offsets_size(dim);
        else
            return m_node.offsets_size(dim);
    }

    template<class T>
    inline auto projection_op<T>::interval(std::size_t dim,
                                           std::size_t index) const noexcept
    {
        if (m_shift == 0)
            return m_data.interval(dim, index);
        else
            return m_node.interval(dim, index);
    }

    template<class T>
    inline auto projection_op<T>::data() const noexcept -> const mesh_type &
    {
        if (m_shift == 0)
            return m_data.data();
        else
            return m_node.data();
    }

    template<class T>
    inline std::size_t projection_op<T>::level() const noexcept
    {
        if (m_shift == 0)
            return m_data.level();
        else
            return m_node.level();
    }

    namespace detail
    {
        template<class T>
        struct get_arg_node_impl
        {
            template<class R>
            decltype(auto) operator()(R &&r)
            {
                return std::forward<R>(r);
            }
        };

        template<std::size_t Dim, class TInterval>
        struct get_arg_node_impl<LevelCellArray<Dim, TInterval>>
        {
            using mesh_t = LevelCellArray<Dim, TInterval>;

            decltype(auto) operator()(LevelCellArray<Dim, TInterval> &r)
            {
                return mesh_node<mesh_t>(r);
            }
        };
    }

    template<class T>
    decltype(auto) get_arg_node(T &&t)
    {
        detail::get_arg_node_impl<std::decay_t<T>> inv;
        return inv(std::forward<T>(t));
    }

    template<int x, int y, int z, class T>
    inline auto translate(T &&t)
    {
        auto arg = get_arg_node(std::forward<T>(t));
        using arg_t = decltype(arg);
        return translate_op<x, y, z, arg_t>{std::forward<arg_t>(arg)};
    }

    template<int x, class T>
    inline auto translate_in_x(T &&t)
    {
        auto arg = get_arg_node(std::forward<T>(t));
        using arg_t = decltype(arg);
        return translate_op<x, 0, 0, arg_t>{std::forward<arg_t>(arg)};
    }

    template<int y, class T>
    inline auto translate_in_y(T &&t)
    {
        auto arg = get_arg_node(std::forward<T>(t));
        using arg_t = decltype(arg);
        return translate_op<0, y, 0, arg_t>{std::forward<arg_t>(arg)};
    }

    template<int z, class T>
    inline auto translate_in_z(T &&t)
    {
        auto arg = get_arg_node(std::forward<T>(t));
        using arg_t = decltype(arg);
        return translate_op<0, 0, z, arg_t>{std::forward<arg_t>(arg)};
    }

    template<class T>
    inline auto contraction(T &&t)
    {
        auto arg = get_arg_node(std::forward<T>(t));
        using arg_t = decltype(arg);
        return contraction_op<arg_t>{std::forward<arg_t>(arg)};
    }

    template<class T>
    inline auto projection(std::size_t ref_level, T &&t)
    {
        auto arg = get_arg_node(std::forward<T>(t));
        using arg_t = decltype(arg);
        return projection_op<arg_t>{ref_level, std::forward<arg_t>(arg)};
    }
}