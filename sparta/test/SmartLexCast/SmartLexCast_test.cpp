
#include <iostream>

#include "sparta/sparta.hpp"
#include "sparta/utils/SmartLexicalCast.hpp"
#include "sparta/utils/SpartaTester.hpp"

/*!
 * \file smartLexicalCast_test.cpp
 * \brief Test for sparta smart lexical casting
 */

TEST_INIT

//! \brief Macro for comparing smart lexical cast result with another value
#define SLC_U_EQUAL(x,y) EXPECT_EQUAL(sparta::utils::smartLexicalCast<uint64_t>((x), end_pos), (y));

#define SLC_S_EQUAL(x,y) EXPECT_EQUAL(sparta::utils::smartLexicalCast<int64_t>((x), end_pos), (y));

int main() {

    size_t end_pos;

    // Test some values utilizing prefixes and suffixes
    SLC_U_EQUAL("10",          10);
    SLC_U_EQUAL("100,000,000", 100000000); // Comma separators
    SLC_U_EQUAL("100_000_000", 100000000); // Underscore separator
    SLC_U_EQUAL("100 000 000", 100000000); // Space separator
    SLC_U_EQUAL("1,0,0,0,0,",  10000);     // Separators can be completely fouled up
    SLC_U_EQUAL("1 k",         1000);      // Separators can come between prefixes
    SLC_U_EQUAL("1,k",         1000);      // Separators can come between prefixes (because of non-strict implementation)
    SLC_U_EQUAL("1ki",         1024);
    SLC_U_EQUAL("1kI",         1024);
    SLC_U_EQUAL("100k",        100000);
    SLC_U_EQUAL("1M500",       1000500);
    SLC_U_EQUAL("0x6",         0x6);
    SLC_U_EQUAL("0xc",         0xc);
    SLC_U_EQUAL("070",         070);
    SLC_U_EQUAL("0k070",       70); // Prefix ignored on secondary value, treated as decimal 070 => 70
    SLC_U_EQUAL("0b110",       6);
    SLC_U_EQUAL("1b",          1000000000);
    SLC_U_EQUAL("1b2k",        1000002000);
    SLC_U_EQUAL("1b2k50",      1000002050);
    SLC_U_EQUAL("0x10b",       0x10b);        // 'b' is interpreted as a hex digit
    SLC_U_EQUAL("0x10b5k",     0x10b5*1000);  // 'b' is interpreted as a hex digit
    SLC_U_EQUAL("0x10g",       16000000000);  // 'g' must be used for 'giga' in a hex string
    SLC_U_EQUAL("0x10g5b",     21000000000);  // 'b' in second value is a suffix
    SLC_U_EQUAL("6p5t4b3M2k1", 6005004003002001);
    SLC_U_EQUAL("6p\n5t 4b 3, M2    k\t1", 6005004003002001);  // Ridiculous spacing supported
    SLC_U_EQUAL("18446744073709551615", 18446744073709551615_u64); // max for uint64_t
    SLC_U_EQUAL("9223372036854775807",  9223372036854775807);  // max for int64_t
    SLC_U_EQUAL("0b1111111111111111111111111111111111111111111111111111111111111111", 18446744073709551615_u64);

    // Signed value reading
    SLC_S_EQUAL("-1", -1);
    SLC_S_EQUAL("-   6p\n5t 4b 3, M2    k\t1", -6005004003002001); // Ridiculous spacing supported

    // Fun with decimal points
    SLC_U_EQUAL(".5M",         500000);
    SLC_U_EQUAL("0.5M",        500000);
    SLC_U_EQUAL("0.654321M",   654321);
    SLC_U_EQUAL(" . 5 M",      500000);
    SLC_U_EQUAL(".5M 2k",      502000);
    SLC_U_EQUAL(".5M.3k",      500300);
    SLC_U_EQUAL("0 . 5M",      500000);
    SLC_U_EQUAL("42.5M",       42500000);
    SLC_U_EQUAL("0.444k",      444);

    // Should this actually be legal?
    // Parsed as two appended numbers hex/octal and then decimal ".Nk"
    SLC_U_EQUAL("00.1k",    100);
    SLC_U_EQUAL("0x0.1k",   100);

    // Invalid strings
    EXPECT_THROW(SLC_U_EQUAL("1k,i",     1)); // Garbage character ','
    EXPECT_THROW(SLC_U_EQUAL("10.5",     1)); // Garbage character '.'
    EXPECT_THROW(SLC_U_EQUAL("10b0xaa",  1)); // No prefixes on second values
    EXPECT_THROW(SLC_U_EQUAL("100f",     1)); // Garbage suffix | not a decimal character
    EXPECT_THROW(SLC_U_EQUAL("0xdeafq",  1)); // Garbage suffix, not a decimal character
    EXPECT_THROW(SLC_U_EQUAL("12komg",   1)); // Garbage suffix, not a decimal character
    EXPECT_THROW(SLC_U_EQUAL("0xg",      1)); // No number
    EXPECT_THROW(SLC_U_EQUAL(".123456k", 1)); // Not a whole number
    EXPECT_THROW(SLC_U_EQUAL(".4444",    1)); // Not a whole number
    EXPECT_THROW(SLC_U_EQUAL("0x.1k",    1)); // What is a hex prefix doing here
    EXPECT_THROW(SLC_U_EQUAL("18446744073709551616", 0)); // Too big for a uint64_t


    // Done

    REPORT_ERROR;

    return ERROR_CODE;
}
