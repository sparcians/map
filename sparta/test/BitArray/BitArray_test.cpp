/*
 */
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/utils/BitArray.hpp"
using namespace sparta::utils;

TEST_INIT;

template <typename T>
static void testValueConstructor(const uint64_t value)
{
    const BitArray a_larger(value, sizeof(value));
    EXPECT_EQUAL(a_larger.getSize(), sizeof(value));
    EXPECT_EQUAL(a_larger.getValue<T>(), T(value));

    const BitArray a_same(value, sizeof(T));
    EXPECT_EQUAL(a_same.getSize(), sizeof(T));
    EXPECT_EQUAL(a_same.getValue<T>(), T(value));

    const BitArray a_smaller(value, sizeof(uint8_t));
    EXPECT_EQUAL(a_smaller.getSize(), sizeof(uint8_t));
    EXPECT_EQUAL(a_smaller.getValue<uint8_t>(), uint8_t(value));
}

static void testDataConstructor(const void *buf,
                                const size_t data_size,
                                const size_t array_size)
{
    const BitArray a(buf, data_size, array_size);
    EXPECT_TRUE(!std::memcmp(buf, a.getValue(), std::min(data_size, array_size)));
}

static void testConstructors()
{
    const uint64_t value = 0xdeadbeefdeadbeef;

    testValueConstructor<uint8_t>(value);
    testValueConstructor<uint16_t>(value);
    testValueConstructor<uint32_t>(value);
    testValueConstructor<uint64_t>(value);

    const uint64_t data[] = {value, value};
    for (size_t i = 1; i <= 16; i += i) {
        testDataConstructor(data, sizeof(data), sizeof(data) / i);
    }
}

static void testOperatorEqual()
{
    const BitArray a(uint32_t(0xdeadbeef));
    const BitArray b(uint32_t(0xdeadbeef));
    const BitArray c(uint32_t(0xabcdabcd));

    EXPECT_TRUE(a == b);
    EXPECT_TRUE(a.getValue<uint32_t>() == b.getValue<uint32_t>());

    EXPECT_TRUE(a != c);
    EXPECT_TRUE(a.getValue<uint32_t>() != c.getValue<uint32_t>());
}

static void testCopyConstructor()
{
    const BitArray a(uint32_t(0xdeadbeef));
    const BitArray b(a);

    EXPECT_TRUE(a == b);
}

static void testAssignmentConstructor()
{
    const uint32_t a_value = 0xdeadbeef;
    const uint32_t b_value = 0xabcdabcd;
    BitArray a(a_value);
    BitArray b(b_value);

    EXPECT_TRUE(a != b);
    a = b;
    EXPECT_TRUE(a == b);

    EXPECT_TRUE(b.getValue<uint32_t>() == b_value);
}

static void testOperatorLeftShift()
{
    const BitArray a(0xf0f0f0f0, sizeof(uint32_t));
    for (size_t i = 0; i < 8 * sizeof(uint32_t); i++) {
        const BitArray b(uint32_t(0xf0f0f0f0) << i);

        EXPECT_TRUE((a << i) == b);
    }
}

static void testOperatorLeftShiftAssign()
{
    BitArray a(0xf0f0f0f0, sizeof(uint32_t));
    for (size_t i = 0; i < 8 * sizeof(uint32_t); i++) {
        const BitArray b(uint32_t(0xf0f0f0f0) << i);

        EXPECT_TRUE(a == b);
        a <<= 1;
    }
}

static void testOperatorRightShift()
{
    BitArray a(0xf0f0f0f0, sizeof(uint32_t));
    for (size_t i = 0; i < 8 * sizeof(uint32_t); i++) {
        const BitArray b(uint32_t(0xf0f0f0f0) >> i);

        EXPECT_TRUE((a >> i) == b);
    }
}

static void testOperatorRightShiftAssign()
{
    BitArray a(0xf0f0f0f0, sizeof(uint32_t));
    for (size_t i = 0; i < 8 * sizeof(uint32_t); i++) {
        const BitArray b(uint32_t(0xf0f0f0f0) >> i);

        EXPECT_TRUE(a == b);
        a >>= 1;
    }
}

static void testOperatorAnd()
{
    const uint32_t a = 0x12345678;
    const uint32_t b = 0x87654321;

    const BitArray aa(a);
    const BitArray bb(b);
    const BitArray cc(a & b);

    EXPECT_TRUE((aa & bb) == cc);
}

static void testOperatorAndAssign()
{
    const uint32_t a = 0x12345678;
    const uint32_t b = 0x87654321;

    BitArray aa(a);
    const BitArray bb(b);
    const BitArray cc(a & b);

    aa &= bb;
    EXPECT_TRUE(aa == cc);
}

static void testOperatorOr()
{
    uint32_t a = 0x12345678;
    uint32_t b = 0x87654321;

    const BitArray aa(a);
    const BitArray bb(b);
    const BitArray cc(a | b);

    EXPECT_TRUE((aa | bb) == cc);
}

static void testOperatorOrAssign()
{
    uint32_t a = 0x12345678;
    uint32_t b = 0x87654321;

    BitArray aa(a);
    const BitArray bb(b);
    const BitArray cc(a | b);

    aa |= bb;
    EXPECT_TRUE(aa == cc);
}

static void testOperatorNegate()
{
    const BitArray a(uint32_t(0xdeadbeef));
    const BitArray b(uint32_t(~0xdeadbeef));

    EXPECT_TRUE(~a == b);
}

static void testFill()
{
    BitArray a(uint32_t(0));
    a.fill(uint8_t(0xab));
    EXPECT_TRUE(a.getValue<uint32_t>() == uint32_t(0xabababab));

    BitArray b(uint32_t(0));
    b.fill(uint16_t(0xabcd));
    EXPECT_TRUE(b.getValue<uint32_t>() == uint32_t(0xabcdabcd));

    BitArray c(uint32_t(0));
    c.fill(uint32_t(0xabcdabcd));
    EXPECT_TRUE(c.getValue<uint32_t>() == uint32_t(0xabcdabcd));
}

int main()
{
    /* This is important for performance */
    EXPECT_TRUE(std::is_move_constructible<BitArray>::value);
    EXPECT_TRUE(std::is_move_assignable<BitArray>::value);

    testConstructors();

    testOperatorEqual();

    testCopyConstructor();

    testAssignmentConstructor();

    testOperatorLeftShift();
    testOperatorLeftShiftAssign();

    testOperatorRightShift();
    testOperatorRightShiftAssign();

    testOperatorAnd();
    testOperatorAndAssign();

    testOperatorOr();
    testOperatorOrAssign();

    testOperatorNegate();

    REPORT_ERROR;
    return ERROR_CODE;
}
