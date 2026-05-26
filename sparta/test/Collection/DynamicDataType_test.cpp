#include "sparta/collection/DynamicDataType.hpp"
#include "sparta/utils/SpartaTester.hpp"

TEST_INIT

using sparta::collection::detail::DynamicDataType;

using P1 = DynamicDataType::ParserImpl1;
using P2 = DynamicDataType::ParserImpl2;
using P3 = DynamicDataType::ParserImpl3;
using P4 = DynamicDataType::ParserImpl4;
using P5 = DynamicDataType::ParserImpl5;
using P6 = DynamicDataType::ParserImpl6;
using P7 = DynamicDataType::ParserImpl7;

using MT = DynamicDataType::MinimalType;

void expectFieldType(const DynamicDataType::ParserBase& parser,
                     const size_t index,
                     const MT expected)
{
    EXPECT_EQUAL(static_cast<int>(parser.getFieldTypes().at(index)),
                 static_cast<int>(expected));
}

void expectOnlyImpl1(const std::string& s)
{
    EXPECT_TRUE(P1::isValidFormat(s));
    EXPECT_FALSE(P2::isValidFormat(s));
    EXPECT_FALSE(P3::isValidFormat(s));
    EXPECT_FALSE(P4::isValidFormat(s));
    EXPECT_FALSE(P5::isValidFormat(s));
    EXPECT_FALSE(P6::isValidFormat(s));
    EXPECT_FALSE(P7::isValidFormat(s));
}

void expectOnlyImpl2(const std::string& s)
{
    EXPECT_FALSE(P1::isValidFormat(s));
    EXPECT_TRUE(P2::isValidFormat(s));
    EXPECT_FALSE(P3::isValidFormat(s));
    EXPECT_FALSE(P4::isValidFormat(s));
    EXPECT_FALSE(P5::isValidFormat(s));
    EXPECT_FALSE(P6::isValidFormat(s));
    EXPECT_FALSE(P7::isValidFormat(s));
}

void expectOnlyImpl3(const std::string& s)
{
    EXPECT_FALSE(P1::isValidFormat(s));
    EXPECT_FALSE(P2::isValidFormat(s));
    EXPECT_TRUE(P3::isValidFormat(s));
    EXPECT_FALSE(P4::isValidFormat(s));
    EXPECT_FALSE(P5::isValidFormat(s));
    EXPECT_FALSE(P6::isValidFormat(s));
    EXPECT_FALSE(P7::isValidFormat(s));
}

void expectOnlyImpl4(const std::string& s)
{
    EXPECT_FALSE(P1::isValidFormat(s));
    EXPECT_FALSE(P2::isValidFormat(s));
    EXPECT_FALSE(P3::isValidFormat(s));
    EXPECT_TRUE(P4::isValidFormat(s));
    EXPECT_FALSE(P5::isValidFormat(s));
    EXPECT_FALSE(P6::isValidFormat(s));
    EXPECT_FALSE(P7::isValidFormat(s));
}

void expectOnlyImpl5(const std::string& s)
{
    EXPECT_FALSE(P1::isValidFormat(s));
    EXPECT_FALSE(P2::isValidFormat(s));
    EXPECT_FALSE(P3::isValidFormat(s));
    EXPECT_FALSE(P4::isValidFormat(s));
    EXPECT_TRUE(P5::isValidFormat(s));
    EXPECT_FALSE(P6::isValidFormat(s));
    EXPECT_FALSE(P7::isValidFormat(s));
}

void expectOnlyImpl6(const std::string& s)
{
    EXPECT_FALSE(P1::isValidFormat(s));
    EXPECT_FALSE(P2::isValidFormat(s));
    EXPECT_FALSE(P3::isValidFormat(s));
    EXPECT_FALSE(P4::isValidFormat(s));
    EXPECT_FALSE(P5::isValidFormat(s));
    EXPECT_TRUE(P6::isValidFormat(s));
    EXPECT_FALSE(P7::isValidFormat(s));
}

void expectOnlyImpl7(const std::string& s)
{
    EXPECT_FALSE(P1::isValidFormat(s));
    EXPECT_FALSE(P2::isValidFormat(s));
    EXPECT_FALSE(P3::isValidFormat(s));
    EXPECT_FALSE(P4::isValidFormat(s));
    EXPECT_FALSE(P5::isValidFormat(s));
    EXPECT_FALSE(P6::isValidFormat(s));
    EXPECT_TRUE(P7::isValidFormat(s));
}

int main()
{
    // ParserImpl1 - space-separated name(val)
    expectOnlyImpl1("uid(42) type(LOAD) mnemonic(add)");
    expectOnlyImpl1("field(val)");
    expectOnlyImpl1("Draw-P_Val_1(abc)");
    expectOnlyImpl1("uid (42)  type (LOAD)");

    EXPECT_FALSE(P1::isValidFormat("field(val), field2(val2)"));
    EXPECT_FALSE(P1::isValidFormat(""));

    // ParserImpl2 - comma-separated name(val)
    expectOnlyImpl2("field1(val1), field2(val2)");
    expectOnlyImpl2("field1 (val1) , field2 (val2)");
    expectOnlyImpl2("a(1), b(2)");

    EXPECT_FALSE(P2::isValidFormat("field(val)"));
    EXPECT_FALSE(P2::isValidFormat("uid(42) type(LOAD)"));

    // ParserImpl3 - space-separated name:val
    expectOnlyImpl3("a:1 b:2");
    expectOnlyImpl3("foo:bar baz:qux");

    EXPECT_FALSE(P3::isValidFormat("a:b"));
    EXPECT_FALSE(P3::isValidFormat("a:1, b:2"));

    // ParserImpl4 - comma-separated name:val
    expectOnlyImpl4("a:1, b:2");
    expectOnlyImpl4("a:b");
    expectOnlyImpl4("Draw-P_Val_1:abc, field2:xyz");

    EXPECT_FALSE(P4::isValidFormat("a:1 b:2"));

    // ParserImpl5 - space-separated bare values
    expectOnlyImpl5("1 2 3");
    expectOnlyImpl5("hello");
    expectOnlyImpl5("1   2   3");

    EXPECT_FALSE(P5::isValidFormat("1, 2"));
    EXPECT_FALSE(P5::isValidFormat("a:1"));

    // ParserImpl6 - comma-separated bare values
    expectOnlyImpl6("1, 2, 3");
    expectOnlyImpl6("foo, bar");

    EXPECT_FALSE(P6::isValidFormat("1 2 3"));
    EXPECT_FALSE(P6::isValidFormat("hello"));

    // ParserImpl7 - bracket list
    expectOnlyImpl7("[1, 2, 3]");
    expectOnlyImpl7("[]");
    expectOnlyImpl7("[foo]");

    EXPECT_FALSE(P7::isValidFormat("1, 2, 3"));
    EXPECT_FALSE(P7::isValidFormat("[a, b"));

    // ParserImpl1 - space-separated name(val)
    P1 p1(nullptr, nullptr, nullptr, 0);
    p1.parseAndDump("foo(4) bar(5)");
    p1.parseAndDump("foo(4) bar(8)");
    EXPECT_THROW(p1.parseAndDump("foo(4) biz(333)"));
    EXPECT_THROW(p1.parseAndDump("foo(4) bar(8) biz(3)"));

    // ParserImpl2 - comma-separated name(val)
    P2 p2(nullptr, nullptr, nullptr, 0);
    p2.parseAndDump("foo(4), bar(5)");
    p2.parseAndDump("foo(4), bar(8)");
    EXPECT_THROW(p2.parseAndDump("foo(4), biz(333)"));
    EXPECT_THROW(p2.parseAndDump("foo(4), bar(8), biz(3)"));

    // ParserImpl3 - space-separated name:val
    P3 p3(nullptr, nullptr, nullptr, 0);
    p3.parseAndDump("foo:4 bar:5");
    p3.parseAndDump("foo:4 bar:8");
    EXPECT_THROW(p3.parseAndDump("foo:4 biz:333"));
    EXPECT_THROW(p3.parseAndDump("foo:4 bar:8 biz:3"));

    // ParserImpl4 - comma-separated name:val
    P4 p4(nullptr, nullptr, nullptr, 0);
    p4.parseAndDump("foo:4, bar:5");
    p4.parseAndDump("foo:4, bar:8");
    EXPECT_THROW(p4.parseAndDump("foo:4, biz:333"));
    EXPECT_THROW(p4.parseAndDump("foo:4, bar:8, biz:3"));

    // ParserImpl5 - space-separated bare values
    P5 p5(nullptr, nullptr, nullptr, 0);
    p5.parseAndDump("4 5");
    p5.parseAndDump("8 9");
    EXPECT_THROW(p5.parseAndDump("4 5 6 7 8"));

    // ParserImpl6 - comma-separated bare values
    P6 p6(nullptr, nullptr, nullptr, 0);
    p6.parseAndDump("4, 5");
    p6.parseAndDump("8, 9");
    EXPECT_THROW(p6.parseAndDump("4, 5, 6, 7, 8"));

    // ParserImpl7 - bracket list
    P7 p7(nullptr, nullptr, nullptr, 0);
    p7.parseAndDump("[1,2,3]");
    p7.parseAndDump("[4,5,6]");
    EXPECT_THROW(p7.parseAndDump("[1,2,3,4,5]"));

    P7 p7_empty(nullptr, nullptr, nullptr, 0);
    EXPECT_THROW(p7_empty.parseAndDump("[]"));

    // MinimalType inference (ParserImpl1)
    P1 p1_types(nullptr, nullptr, nullptr, 0);
    p1_types.parseAndDump("foo(4) bar(5)");
    expectFieldType(p1_types, 0, MT::uint8_t);
    expectFieldType(p1_types, 1, MT::uint8_t);

    p1_types.parseAndDump("foo(-35) bar(300)");
    expectFieldType(p1_types, 0, MT::int8_t);
    expectFieldType(p1_types, 1, MT::uint16_t);

    p1_types.parseAndDump("foo(-35) bar(65535)");
    expectFieldType(p1_types, 0, MT::int8_t);
    expectFieldType(p1_types, 1, MT::uint16_t);

    p1_types.parseAndDump("foo(-500) bar(-3)");
    expectFieldType(p1_types, 0, MT::int16_t);
    expectFieldType(p1_types, 1, MT::int32_t);

    // 2147483648 = INT32_MAX + 1
    p1_types.parseAndDump("foo(-500) bar(2147483648)");
    expectFieldType(p1_types, 0, MT::int16_t);
    expectFieldType(p1_types, 1, MT::int64_t);

    // "bar" is int64_t right now, which cannot go to uint64_t
    // 9223372036854775808 = INT64_MAX + 1
    EXPECT_THROW(p1_types.parseAndDump("foo(-500) bar(9223372036854775808)"));

    // Hex-prefix lock: foo field is decimal-locked above; hex literals must throw.
    EXPECT_THROW(p1_types.parseAndDump("foo(0x10) bar(-5)"));

    P1 p1_hex_types(nullptr, nullptr, nullptr, 0);
    p1_hex_types.parseAndDump("foo(0x4) bar(5)");
    expectFieldType(p1_hex_types, 0, MT::uint8_t);
    expectFieldType(p1_hex_types, 1, MT::uint8_t);
    p1_hex_types.parseAndDump("foo(0xFFFFFFFF) bar(300)");
    expectFieldType(p1_hex_types, 0, MT::uint32_t);
    expectFieldType(p1_hex_types, 1, MT::uint16_t);

    P1 p1_overflow(nullptr, nullptr, nullptr, 0);
    p1_overflow.parseAndDump("foo(4) bar(5)");
    p1_overflow.parseAndDump("foo(-35) bar(300)");
    p1_overflow.parseAndDump("foo(-500) bar(-3)");
    EXPECT_THROW(p1_overflow.parseAndDump("foo(18446744073709551615) bar(-5)"));

    P1 p1_bool(nullptr, nullptr, nullptr, 0);
    p1_bool.parseAndDump("foo(true) bar(4)");
    expectFieldType(p1_bool, 0, MT::bool_t);
    expectFieldType(p1_bool, 1, MT::uint8_t);
    EXPECT_THROW(p1_bool.parseAndDump("foo(4.5) bar(5)"));

    P1 p1_float_string(nullptr, nullptr, nullptr, 0);
    p1_float_string.parseAndDump("foo(4.5) bar(hello)");
    expectFieldType(p1_float_string, 0, MT::float_t);
    expectFieldType(p1_float_string, 1, MT::string_t);
    p1_float_string.parseAndDump("foo(6.8) bar(true)");
    expectFieldType(p1_float_string, 0, MT::float_t);
    expectFieldType(p1_float_string, 1, MT::string_t);

    P1 p1_hex_lock(nullptr, nullptr, nullptr, 0);
    p1_hex_lock.parseAndDump("foo(4) bar(5)");
    EXPECT_THROW(p1_hex_lock.parseAndDump("foo(0x10) bar(5)"));

    P1 p1_decimal_lock(nullptr, nullptr, nullptr, 0);
    p1_decimal_lock.parseAndDump("foo(0x4) bar(5)");
    EXPECT_THROW(p1_decimal_lock.parseAndDump("foo(42) bar(5)"));

    REPORT_ERROR;
    return ERROR_CODE;
}
