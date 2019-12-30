/*!
 * \file MetaTypeList.cpp
 * \brief Test for Compile-Time MetaTypeList functionalities.
 */

#include <iostream>
#include "sparta/utils/MetaTypeList.hpp"
#include "sparta/utils/SpartaTester.hpp"
TEST_INIT;

int main() {

    using type = MetaTypeList::create_t<>;
    EXPECT_EQUAL(true, MetaTypeList::is_empty<type>::value);
    EXPECT_EQUAL(true, MetaTypeList::is_meta_typelist<type>::value);

    using type_2 = MetaTypeList::push_back<type, int>;
    EXPECT_EQUAL(false, MetaTypeList::is_empty<type_2>::value);
    EXPECT_EQUAL(true, MetaTypeList::is_meta_typelist<type_2>::value);

    using type_3 = MetaTypeList::push_back<type_2, double>;
    EXPECT_EQUAL(false, MetaTypeList::is_empty<type_3>::value);
    EXPECT_EQUAL(true, MetaTypeList::is_meta_typelist<type_3>::value);

    using res_type = MetaTypeList::nth_element<type_3, 0>;
    {
        bool val = std::is_same<int, res_type>::value;
        EXPECT_EQUAL(true, val);
    }

    using res_type_2 = MetaTypeList::nth_element<type_3, 1>;
    {
        bool val = std::is_same<double, res_type_2>::value;
        EXPECT_EQUAL(true, val);
    }

    using type_4 = MetaTypeList::push_front<type_3, std::string>;
    EXPECT_EQUAL(false, MetaTypeList::is_empty<type_4>::value);
    EXPECT_EQUAL(true, MetaTypeList::is_meta_typelist<type_4>::value);

    using res_type_3 = MetaTypeList::nth_element<type_4, 0>;
    {
        bool val = std::is_same<std::string, res_type_3>::value;
        EXPECT_EQUAL(true, val);
    }

    using res_type_4 = MetaTypeList::front<type_4>;
    {
        bool val = std::is_same<std::string, res_type_4>::value;
        EXPECT_EQUAL(true, val);
    }

    using type_5 = MetaTypeList::pop_front<type_4>;
    EXPECT_EQUAL(false, MetaTypeList::is_empty<type_5>::value);
    EXPECT_EQUAL(true, MetaTypeList::is_meta_typelist<type_5>::value);

    using res_type_5 = MetaTypeList::front<type_5>;
    {
        bool val = std::is_same<int, res_type_5>::value;
        EXPECT_EQUAL(true, val);
    }

    using type_6 = MetaTypeList::pop_front<type_5>;
    using res_type_6 = MetaTypeList::front<type_6>;
    {
        bool val = std::is_same<double, res_type_6>::value;
        EXPECT_EQUAL(true, val);
    }

    EXPECT_EQUAL(false, MetaTypeList::is_empty<type_6>::value);
    EXPECT_EQUAL(true, MetaTypeList::is_meta_typelist<type_6>::value);

    using type_7 = MetaTypeList::pop_front<type_6>;
    EXPECT_EQUAL(true, MetaTypeList::is_empty<type_7>::value);
    EXPECT_EQUAL(true, MetaTypeList::is_meta_typelist<type_7>::value);

    return 0;
}
