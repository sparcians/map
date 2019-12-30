/*
 */
#include "sparta/utils/SpartaTester.hpp"
#include <boost/dynamic_bitset.hpp>

TEST_INIT;

static void testOperatorEqual()
{
    const boost::dynamic_bitset<> a(sizeof(uint32_t) * 8, 0xdeadbeef);
    const boost::dynamic_bitset<> b(sizeof(uint32_t) * 8, 0xdeadbeef);
    const boost::dynamic_bitset<> c(sizeof(uint32_t) * 8, 0xabcdabcd);

    EXPECT_TRUE(a == b);
    EXPECT_TRUE(a != c);
}

static void testCopyConstructor()
{
    const boost::dynamic_bitset<> a(sizeof(uint32_t) * 8, 0xdeadbeef);
    const boost::dynamic_bitset<> b(a);

    EXPECT_TRUE(a == b);
}

static void testAssignmentConstructor()
{
    const uint32_t a_value = 0xdeadbeef;
    const uint32_t b_value = 0xabcdabcd;
    boost::dynamic_bitset<> a(sizeof(uint32_t) * 8, a_value);
    boost::dynamic_bitset<> b(sizeof(uint32_t) * 8, b_value);

    EXPECT_TRUE(a != b);
    a = b;
    EXPECT_TRUE(a == b);
}

static void testOperatorLeftShift()
{
    const boost::dynamic_bitset<> a(sizeof(uint32_t) * 8, 0xf0f0f0f0);
    for (size_t i = 0; i < sizeof(uint32_t) * 8; i++) {
        const boost::dynamic_bitset<> b(sizeof(uint32_t) * 8, uint32_t(0xf0f0f0f0) << i);

        EXPECT_TRUE((a << i) == b);
    }
}

static void testOperatorLeftShiftAssign()
{
    boost::dynamic_bitset<> a(sizeof(uint32_t) * 8, 0xf0f0f0f0);
    for (size_t i = 0; i < sizeof(uint32_t) * 8; i++) {
        const boost::dynamic_bitset<> b(sizeof(uint32_t) * 8, uint32_t(0xf0f0f0f0) << i);

        EXPECT_TRUE(a == b);
        a <<= 1;
    }
}

static void testOperatorRightShift()
{
    boost::dynamic_bitset<> a(sizeof(uint32_t) * 8, 0xf0f0f0f0);
    for (size_t i = 0; i < sizeof(uint32_t) * 8; i++) {
        const boost::dynamic_bitset<> b(sizeof(uint32_t) * 8, uint32_t(0xf0f0f0f0) >> i);

        EXPECT_TRUE((a >> i) == b);
    }
}

static void testOperatorRightShiftAssign()
{
    boost::dynamic_bitset<> a(sizeof(uint32_t) * 8, 0xf0f0f0f0);
    for (size_t i = 0; i < sizeof(uint32_t)*8; i++) {
        const boost::dynamic_bitset<> b(sizeof(uint32_t) * 8, uint32_t(0xf0f0f0f0) >> i);
        // std::cout << "i == " << i << " " << b << " " << a << std::endl;

        EXPECT_TRUE(a == b);
        a >>= 1;
    }
}

static void testOperatorAnd()
{
    const uint32_t a = 0x12345678;
    const uint32_t b = 0x87654321;

    const boost::dynamic_bitset<> aa(sizeof(uint32_t) * 8, a);
    const boost::dynamic_bitset<> bb(sizeof(uint32_t) * 8, b);
    const boost::dynamic_bitset<> cc(sizeof(uint32_t) * 8, a & b);

    EXPECT_TRUE((aa & bb) == cc);
}

static void testOperatorAndAssign()
{
    const uint32_t a = 0x12345678;
    const uint32_t b = 0x87654321;

    boost::dynamic_bitset<> aa(sizeof(uint32_t) * 8, a);
    const boost::dynamic_bitset<> bb(sizeof(uint32_t) * 8, b);
    const boost::dynamic_bitset<> cc(sizeof(uint32_t) * 8, a & b);

    aa &= bb;
    EXPECT_TRUE(aa == cc);
}

static void testOperatorOr()
{
    uint32_t a = 0x12345678;
    uint32_t b = 0x87654321;

    const boost::dynamic_bitset<> aa(sizeof(uint32_t) * 8, a);
    const boost::dynamic_bitset<> bb(sizeof(uint32_t) * 8, b);
    const boost::dynamic_bitset<> cc(sizeof(uint32_t) * 8, a | b);

    EXPECT_TRUE((aa | bb) == cc);
}

static void testOperatorOrAssign()
{
    uint32_t a = 0x12345678;
    uint32_t b = 0x87654321;

    boost::dynamic_bitset<> aa(sizeof(uint32_t) * 8, a);
    const boost::dynamic_bitset<> bb(sizeof(uint32_t) * 8, b);
    const boost::dynamic_bitset<> cc(sizeof(uint32_t) * 8, a | b);

    aa |= bb;
    EXPECT_TRUE(aa == cc);
}

static void testOperatorNegate()
{
    const boost::dynamic_bitset<> a(sizeof(uint32_t) * 8, uint32_t(0xdeadbeef));
    const boost::dynamic_bitset<> b(sizeof(uint32_t) * 8, uint32_t(~0xdeadbeef));

    EXPECT_TRUE(~a == b);
}

int main()
{
    /* This is important for performance */
    for(uint32_t i = 0; i < 1000000; ++i)
    {

        testOperatorEqual();

        testOperatorLeftShift();
        testOperatorLeftShiftAssign();

        testOperatorRightShift();
        testOperatorRightShiftAssign();

        testOperatorAnd();
        testOperatorAndAssign();

        testOperatorOr();
        testOperatorOrAssign();

        testOperatorNegate();
    }

    REPORT_ERROR;
    return ERROR_CODE;
}
