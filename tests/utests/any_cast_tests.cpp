#include <boost/test/unit_test.hpp>

#include <type_traits>

#include <iostream>

#include <boost/fusion/algorithm.hpp>
#include <boost/fusion/container.hpp>
#include <boost/fusion/sequence.hpp>

namespace any_cast_tests {

template <uint16_t TypeNumber> struct object
{
    static constexpr uint16_t type_id = TypeNumber;
};

struct empty_type
{
};

struct a_type : public object<1>
{
};

struct b_type : public object<5>
{
};

struct c_type : public object<10>
{
};

template <uint16_t Id> struct get_object_type
{
    typedef empty_type type;
};

template <> struct get_object_type<a_type::type_id>
{
    typedef a_type type;
};

template <> struct get_object_type<b_type::type_id>
{
    typedef b_type type;
};

template <> struct get_object_type<c_type::type_id>
{
    typedef c_type type;
};

template <typename T, typename U> struct decay_equiv : std::is_same<typename std::decay<T>::type, U>::type
{
};

static struct find_object_type
{
    boost::fusion::vector<get_object_type<0>,
                          get_object_type<1>,
                          get_object_type<2>,
                          get_object_type<3>,
                          get_object_type<4>,
                          get_object_type<5>,
                          get_object_type<6>,
                          get_object_type<7>,
                          get_object_type<8>,
                          get_object_type<9>>
        space_0_9;
    boost::fusion::vector<get_object_type<10>,
                          get_object_type<11>,
                          get_object_type<12>,
                          get_object_type<13>,
                          get_object_type<14>,
                          get_object_type<15>,
                          get_object_type<16>,
                          get_object_type<17>,
                          get_object_type<18>,
                          get_object_type<19>>
        space_10_19;

    template <typename Visitor, typename Container, int N, int End> struct find_object_type_impl
    {
        bool operator()(Visitor& v, Container& t, const int id) const
        {
            if (id == N)
            {
                auto& tp = boost::fusion::at_c<N>(t);
                using object_type = typename std::decay<decltype(tp)>::type;
                if ((decay_equiv<typename object_type::type, empty_type>::value))
                    return false;

                typename object_type::type* objp = nullptr;
                return v(objp);
            }
            return find_object_type_impl<Visitor, Container, N + 1, End>()(v, t, id);
        }
    };

    template <typename Visitor, typename Container, int N> struct find_object_type_impl<Visitor, Container, N, N>
    {
        bool operator()(Visitor&, Container&, const int) const
        {
            return false;
        }
    };

    template <typename Visitor> bool apply(Visitor& v, int id)
    {
        if (id < 9)
        {
            auto& space = space_0_9;
            return find_object_type_impl<Visitor, std::decay<decltype(space)>::type, 0,
                                         boost::fusion::result_of::size<std::decay<decltype(space)>::type>::type::
                                             value>()(v, space, id);
        }
        else if (id < 19)
        {
            auto& space = space_10_19;
            return find_object_type_impl<Visitor, std::decay<decltype(space)>::type, 0,
                                         boost::fusion::result_of::size<std::decay<decltype(space)>::type>::type::
                                             value>()(v, space, id - 10);
        }
        return false;
    }
} object_types;

struct object_visitor
{
    bool operator()(const empty_type*) const
    {
        return false;
    }

    template <class T> bool operator()(const T*) const
    {
        auto object_name = boost::core::demangle(typeid(T).name());
        std::cerr << object_name << T::type_id << std::endl;
        return true;
    }
};

BOOST_AUTO_TEST_SUITE(any_cast_tests)

BOOST_AUTO_TEST_CASE(get_object_type_by_id)
{
    BOOST_REQUIRE((decay_equiv<get_object_type<1>::type, a_type>::value));
    BOOST_REQUIRE((decay_equiv<get_object_type<5>::type, b_type>::value));
    BOOST_REQUIRE((decay_equiv<get_object_type<10>::type, c_type>::value));

    object_visitor v;

    BOOST_CHECK(!object_types.apply(v, 0));

    uint16_t id = 1;
    BOOST_CHECK(object_types.apply(v, id));

    id = 4;
    BOOST_CHECK(!object_types.apply(v, id));

    id = 5;
    BOOST_CHECK(object_types.apply(v, id));

    id = 10;
    BOOST_CHECK(object_types.apply(v, id));

    id = 1111;
    BOOST_CHECK(!object_types.apply(v, id));
}

BOOST_AUTO_TEST_SUITE_END()
}
