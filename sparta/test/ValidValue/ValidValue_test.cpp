

#include "sparta/utils/ValidValue.hpp"
#include "sparta/utils/SpartaTester.hpp"

#include <limits>
#include <iostream>
#include <string>

TEST_INIT;

class Foo
{
public:
    Foo() {}
    explicit Foo(const uint32_t) {}
    Foo(const uint32_t, const std::string) {}
};

int main()
{
    sparta::utils::ValidValue<uint32_t> i;
    EXPECT_FALSE(i.isValid());
    EXPECT_THROW(i.getValue());
    bool dummy = false; // to avoid a clang warning
    EXPECT_THROW(dummy = (i == uint32_t(0)));
    (void) dummy;

    // This should print out "<invalid>"
    EXPECT_NOTHROW(std::cout << i << std::endl);

    i = 10;
    EXPECT_TRUE(i.isValid());
    std::cout << i << std::endl;

    i = std::numeric_limits<decltype(i)::value_type>::max();

    // The following is a compile-time error (expected)
    //EXPECT_FALSE(i == std::numeric_limits<uint64_t>::max());
    EXPECT_TRUE(i == std::numeric_limits<decltype(i)::value_type>::max());
    EXPECT_FALSE(i != std::numeric_limits<decltype(i)::value_type>::max());

    const uint64_t val = i;
    EXPECT_TRUE(val == std::numeric_limits<decltype(i)::value_type>::max());

    i.clearValid();
    EXPECT_THROW(dummy = (i == std::numeric_limits<decltype(i)::value_type>::max()));
    // This should print out "<invalid>"
    EXPECT_NOTHROW(std::cout << i << std::endl);

    i = 20;

    sparta::utils::ValidValue<uint32_t> another_vv(30);
    EXPECT_TRUE(another_vv.isValid());
    EXPECT_FALSE(another_vv == i);

    another_vv = i;

    sparta::utils::ValidValue<uint32_t> moveable_vv(10);
    EXPECT_TRUE(moveable_vv.isValid());
    sparta::utils::ValidValue<uint32_t> movedto_vv = std::move(moveable_vv);
    EXPECT_FALSE(moveable_vv.isValid());
    EXPECT_TRUE(movedto_vv.isValid());

    sparta::utils::ValidValue<Foo> foo_type;
    foo_type = Foo(5);
    EXPECT_TRUE(foo_type.isValid());
    sparta::utils::ValidValue<Foo> moveable_foo(std::move(foo_type));
    EXPECT_FALSE(foo_type.isValid());

    sparta::utils::ValidValue<Foo> foo_type2(10, "hello");
    EXPECT_TRUE(foo_type2.isValid());


    REPORT_ERROR;
    return ERROR_CODE;
}
