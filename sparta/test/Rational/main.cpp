#include <inttypes.h>
#include <iostream>

#include "sparta/sparta.hpp"
#include "sparta/utils/Rational.hpp"
#include "sparta/utils/SpartaTester.hpp"

TEST_INIT;

using sparta::utils::Rational;
using namespace std;

int main()
{
    Rational<uint32_t> r(12);
    Rational<uint32_t> s(2,3);

    EXPECT_TRUE((r * s) == 8);
    EXPECT_TRUE((r / s) == 18);
    EXPECT_TRUE((r + s) == Rational<uint32_t>(38,3));
    EXPECT_TRUE((r - s) == Rational<uint32_t>(34,3));
    EXPECT_TRUE((s - r) == Rational<uint32_t>(34,3));

    r *= s;
    EXPECT_TRUE(r == 8);
    r /= s;
    EXPECT_TRUE(r == 12);
    r += s;
    EXPECT_TRUE(r == Rational<uint32_t>(38,3));
    r -= s;
    EXPECT_TRUE(r == 12);

    EXPECT_TRUE((r * 2) == 24);
    EXPECT_TRUE((r / 2) == 6);
    EXPECT_TRUE((r + 2) == 14);
    EXPECT_TRUE((r - 2) == 10);

    EXPECT_TRUE(uint32_t(r) == 12);

    REPORT_ERROR;

    return ERROR_CODE;
}
