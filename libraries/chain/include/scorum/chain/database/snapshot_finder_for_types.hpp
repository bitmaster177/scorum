#pragma once

#include <scorum/chain/database/snapshot_types.hpp>

#include <boost/preprocessor/iteration/local.hpp>
#include <boost/preprocessor/repetition/enum.hpp>
#include <boost/preprocessor/arithmetic/add.hpp>
#include <boost/preprocessor/arithmetic/mul.hpp>
#include <boost/preprocessor/arithmetic/sub.hpp>

#include <boost/fusion/algorithm.hpp>
#include <boost/fusion/container.hpp>
#include <boost/fusion/sequence.hpp>

#define BLOCK_SIZE 10
#define BLOCK_COUNT 2

namespace scorum {
struct find_object_type
{
// clang-format off
#define GET_OBJECT_TYPE(_1, i, z) get_object_type<BOOST_PP_ADD(BOOST_PP_MUL(BLOCK_SIZE, z), i)>

#define OBJECT_TYPES_VECTOR(_1, k, _2) \
    boost::fusion::vector<BOOST_PP_ENUM(BLOCK_SIZE, GET_OBJECT_TYPE, k)> BOOST_PP_CAT(space_, k)

#define BOOST_PP_LOCAL_MACRO(n) OBJECT_TYPES_VECTOR(_1, n, _2);
#define BOOST_PP_LOCAL_LIMITS (0, BLOCK_COUNT - 1)
#include BOOST_PP_LOCAL_ITERATE()
    // clang-format on

    template <typename Visitor, typename Container, int N, int End> struct find_object_type_impl
    {
        bool operator()(const Visitor& v, Container& t, const int id) const
        {
            if (id == N)
            {
                auto tp = boost::fusion::at_c<N>(t);
                if (!tp.initialized)
                    return false;

                return v(tp);
            }
            return find_object_type_impl<Visitor, Container, N + 1, End>()(v, t, id);
        }
    };

    template <typename Visitor, typename Container, int N> struct find_object_type_impl<Visitor, Container, N, N>
    {
        bool operator()(const Visitor&, Container&, const int) const
        {
            return false;
        }
    };

    template <typename Visitor> bool apply(const Visitor& v, int id)
    {
// clang-format off
#define FIND_OBJECT_TYPE_IN_SECTION(_1, k, _2)                                                                             \
        if (id < BOOST_PP_SUB(BOOST_PP_MUL(BOOST_PP_ADD(k, 1), BLOCK_SIZE), 1))                                            \
        {                                                                                                                  \
            auto& space = BOOST_PP_CAT(space_, k);                                                                         \
            return find_object_type_impl<Visitor, std::decay<decltype(space)>::type, 0,                                    \
                                         boost::fusion::result_of::size<std::decay<decltype(space)>::type>::type::         \
                                             value>()(v, BOOST_PP_CAT(space_, k), id - BOOST_PP_MUL(k, BLOCK_SIZE));       \
        }                                                                                                                  \
        else
#define BOOST_PP_LOCAL_MACRO(n) FIND_OBJECT_TYPE_IN_SECTION(_1, n, _2);
#define BOOST_PP_LOCAL_LIMITS (0, BLOCK_COUNT - 1)
#include BOOST_PP_LOCAL_ITERATE()
        // clang-format on
        {
        }

        return false;
    }
};
}
