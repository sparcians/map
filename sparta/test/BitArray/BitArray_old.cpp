/*
 */
#include "sparta/utils/SpartaTester.hpp"
#include "BitVector.h"
using namespace ttfw2;

TEST_INIT;

static void testOperatorEqual()
{
    const BitVector a(uint32_t(0xdeadbeef));
    const BitVector b(uint32_t(0xdeadbeef));
    const BitVector c(uint32_t(0xabcdabcd));

    EXPECT_TRUE(a == b);
    // EXPECT_TRUE(a.getValue<uint32_t>() == b.getValue<uint32_t>());

    EXPECT_TRUE(a != c);
    // EXPECT_TRUE(a.getValue<uint32_t>() != c.getValue<uint32_t>());
}

static void testCopyConstructor()
{
    const BitVector a(uint32_t(0xdeadbeef));
    const BitVector b(a);

    EXPECT_TRUE(a == b);
}

static void testAssignmentConstructor()
{
    const uint32_t a_value = 0xdeadbeef;
    const uint32_t b_value = 0xabcdabcd;
    BitVector a(a_value);
    BitVector b(b_value);

    EXPECT_TRUE(a != b);
    a = b;
    EXPECT_TRUE(a == b);

    // EXPECT_TRUE(b.getValue<uint32_t>() == b_value);
}

static void testOperatorLeftShift()
{
    const BitVector a(0xf0f0f0f0);
    for (size_t i = 0; i < 8 * sizeof(uint32_t); i++) {
        const BitVector b(uint32_t(0xf0f0f0f0) << i);

        std::cout << b << " " << (a << i) << std::endl;

        EXPECT_TRUE((a << i) == b);
    }
}

static void testOperatorLeftShiftAssign()
{
    BitVector a(0xf0f0f0f0);
    for (size_t i = 0; i < 8 * sizeof(uint32_t); i++) {
        const BitVector b(uint32_t(0xf0f0f0f0) << i);

        EXPECT_TRUE(a == b);
        a <<= 1;
    }
}

static void testOperatorRightShift()
{
    BitVector a(0xf0f0f0f0);
    for (size_t i = 0; i < 8 * sizeof(uint32_t); i++) {
        const BitVector b(uint32_t(0xf0f0f0f0) >> i);

        EXPECT_TRUE((a >> i) == b);
    }
}

static void testOperatorRightShiftAssign()
{
    BitVector a(0xf0f0f0f0);
    for (size_t i = 0; i < 8 * sizeof(uint32_t); i++) {
        const BitVector b(uint32_t(0xf0f0f0f0) >> i);

        EXPECT_TRUE(a == b);
        a >>= 1;
    }
}

static void testOperatorAnd()
{
    const uint32_t a = 0x12345678;
    const uint32_t b = 0x87654321;

    const BitVector aa(a);
    const BitVector bb(b);
    const BitVector cc(a & b);

    EXPECT_TRUE((aa & bb) == cc);
}

static void testOperatorAndAssign()
{
    const uint32_t a = 0x12345678;
    const uint32_t b = 0x87654321;

    BitVector aa(a);
    const BitVector bb(b);
    const BitVector cc(a & b);

    aa &= bb;
    EXPECT_TRUE(aa == cc);
}

static void testOperatorOr()
{
    uint32_t a = 0x12345678;
    uint32_t b = 0x87654321;

    const BitVector aa(a);
    const BitVector bb(b);
    const BitVector cc(a | b);

    EXPECT_TRUE((aa | bb) == cc);
}

static void testOperatorOrAssign()
{
    uint32_t a = 0x12345678;
    uint32_t b = 0x87654321;

    BitVector aa(a);
    const BitVector bb(b);
    const BitVector cc(a | b);

    aa |= bb;
    EXPECT_TRUE(aa == cc);
}

static void testOperatorNegate()
{
    const BitVector a(uint32_t(0xdeadbeef));
    const BitVector b(uint32_t(~0xdeadbeef));

    EXPECT_TRUE(~a == b);
}

int main()
{
    /* This is important for performance */
    // EXPECT_TRUE(std::is_move_constructible<BitVector>::value);
    // EXPECT_TRUE(std::is_move_assignable<BitVector>::value);

    for(uint32_t i = 0; i < 1; ++i)
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
