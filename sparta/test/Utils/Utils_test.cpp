#include <memory>
#include <unordered_map>
#include <map>
#include "sparta/utils/Utils.hpp"
#include "sparta/utils/MathUtils.hpp"
#include "sparta/utils/Bits.hpp"
#include "sparta/utils/PointerUtils.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/utils/StringUtils.hpp"
#include "sparta/utils/LifeTracker.hpp"
#include "sparta/utils/SpartaAssert.hpp"

TEST_INIT;

class A {
 public:
    A() {}
    virtual ~A() {}
    virtual int value() const { return 1; }
};

class B : public A {
 public:
    B() {}
    virtual ~B() {}
    int value() const override { return 2; }
};

class C : public A {
 public:
    C() {}
    virtual ~C() {}
    int value() const override { return 3; }
};

#if !defined(__linux__)
// mac always has to use libc++
#define SHARED_PTR_NAME "std::__1::shared_ptr"
#else
// note that libc++ uses std::__1, so this will be the wrong name if you link against it
#define SHARED_PTR_NAME "std::shared_ptr"
#endif

class MyVolatileTrackedClass
{
public:
    using MyVolatileTrackedClassTracker = sparta::utils::LifeTracker<MyVolatileTrackedClass>;
    const MyVolatileTrackedClassTracker & getLifeTracker() const {
        return life_tracker_;
    }
    const uint32_t value = 10;

private:
    MyVolatileTrackedClassTracker life_tracker_{this};
};

void testLifeTracker()
{
    std::weak_ptr<MyVolatileTrackedClass::MyVolatileTrackedClassTracker> wptr;
    {
        MyVolatileTrackedClass my_object;
        wptr = my_object.getLifeTracker();
        EXPECT_FALSE(wptr.expired());
        if(false == wptr.expired()) {
            std::cout << "The class is still valid: " <<
                wptr.lock()->tracked_object->value << std::endl;
        }
    }
    EXPECT_TRUE(wptr.expired());
    if(wptr.expired()) {
        std::cout << "The class has expired" << std::endl;
    }
}

int main()
{
    auto u_map = std::unordered_map<std::string, int>{{"Key1", 1}, {"Key2", 2}, {"Key3", 3}};
    auto flipped_map = sparta::flipMap(u_map);
    EXPECT_TRUE(flipped_map[1] == "Key1");
    EXPECT_TRUE(flipped_map[2] == "Key2");
    EXPECT_TRUE(flipped_map[3] == "Key3");
    
    auto u_map2 = std::map<int, std::string>{{10, "Key10"}, {11, "Key11"}, {12, "Key12"}};
    auto flipper_map2 = sparta::flipMap(u_map2);
    EXPECT_TRUE(flipper_map2["Key10"] == 10);
    EXPECT_TRUE(flipper_map2["Key11"] == 11);
    EXPECT_TRUE(flipper_map2["Key12"] == 12);
    
    testLifeTracker();

    // testing checked casting of pointers and shared pointers
    auto b = std::make_shared<B>();
    EXPECT_TRUE(sparta::utils::checked_dynamic_pointer_cast<A>(b) != nullptr);
    auto a = std::make_shared<A>();

    EXPECT_THROW_MSG_CONTAINS(sparta::utils::checked_dynamic_pointer_cast<B>(a),
                  "destination != nullptr:  dynamic_pointer_cast failed, this shared_ptr is of type " SHARED_PTR_NAME "<A>, not of type " SHARED_PTR_NAME "<B>");

    std::shared_ptr<A> c = std::make_shared<C>();
    EXPECT_TRUE(sparta::utils::checked_dynamic_pointer_cast<C>(c) != nullptr);

    auto uncheckedC = sparta::utils::checked_dynamic_pointer_cast<C, A, true>(c);
    EXPECT_TRUE(uncheckedC != nullptr);

    auto *bb = new B();
    EXPECT_TRUE(sparta::utils::checked_dynamic_cast<A*>(bb) != nullptr);
    delete bb;
    auto *aa = new A();
    EXPECT_THROW_MSG_CONTAINS(sparta::utils::checked_dynamic_cast<B*>(aa),
      "destination != nullptr:  dynamic_cast failed, this pointer is of type A*, not of type B*");
    delete aa;
    A *cc = new C();
    EXPECT_TRUE(sparta::utils::checked_dynamic_cast<C*>(cc) != nullptr);

    auto uncheckedCC = sparta::utils::checked_dynamic_cast<C*, A*, true>(cc);
    EXPECT_TRUE(uncheckedCC != nullptr);
    delete cc;

    EXPECT_THROW_MSG_CONTAINS(sparta_throw("unconditionally aborting!"),
                          "abort: unconditionally aborting!");

    EXPECT_TRUE(sparta::utils::count_1_bits(uint32_t(0)) == 0);
    EXPECT_TRUE(sparta::utils::count_1_bits(uint32_t(-1)) == sizeof(uint32_t) * 8);
    EXPECT_TRUE(sparta::utils::count_1_bits(uint32_t(0x33333333)) == sizeof(uint32_t) * 4);
    EXPECT_TRUE(sparta::utils::count_1_bits(uint32_t(0x55555555)) == sizeof(uint32_t) * 4);
    EXPECT_TRUE(sparta::utils::count_1_bits(uint32_t(0x99999999)) == sizeof(uint32_t) * 4);
    EXPECT_TRUE(sparta::utils::count_1_bits(uint32_t(0xAAAAAAAA)) == sizeof(uint32_t) * 4);
    EXPECT_TRUE(sparta::utils::count_1_bits(uint32_t(0xCCCCCCCC)) == sizeof(uint32_t) * 4);

    for(uint32_t i = 0; i < sizeof(uint32_t) * 8; ++i) {
    EXPECT_TRUE(sparta::utils::count_1_bits(uint32_t(0x1 << i)) == 1);
    }

    EXPECT_TRUE(sparta::utils::count_1_bits(uint64_t(0)) == 0);
    EXPECT_TRUE(sparta::utils::count_1_bits(uint64_t(-1)) == sizeof(uint64_t) * 8);
    EXPECT_TRUE(sparta::utils::count_1_bits(uint64_t(0x3333333333333333)) == sizeof(uint64_t) * 4);
    EXPECT_TRUE(sparta::utils::count_1_bits(uint64_t(0x5555555555555555)) == sizeof(uint64_t) * 4);
    EXPECT_TRUE(sparta::utils::count_1_bits(uint64_t(0x9999999999999999)) == sizeof(uint64_t) * 4);
    EXPECT_TRUE(sparta::utils::count_1_bits(uint64_t(0xAAAAAAAAAAAAAAAA)) == sizeof(uint64_t) * 4);
    EXPECT_TRUE(sparta::utils::count_1_bits(uint64_t(0xCCCCCCCCCCCCCCCC)) == sizeof(uint64_t) * 4);

    for(uint64_t i = 0; i < sizeof(uint64_t) * 8; ++i) {
    EXPECT_TRUE(sparta::utils::count_1_bits(uint64_t(0x1ull << i)) == 1);
    }

    EXPECT_TRUE(sparta::utils::next_power_of_2(0) == 1);
    EXPECT_TRUE(sparta::utils::next_power_of_2(1) == 1);
    EXPECT_TRUE(sparta::utils::next_power_of_2(2) == 2);
    EXPECT_TRUE(sparta::utils::next_power_of_2(3) == 4);
    EXPECT_TRUE(sparta::utils::next_power_of_2(4) == 4);
    EXPECT_TRUE(sparta::utils::next_power_of_2(5) == 8);
    EXPECT_TRUE(sparta::utils::next_power_of_2(6) == 8);
    EXPECT_TRUE(sparta::utils::next_power_of_2(7) == 8);
    EXPECT_TRUE(sparta::utils::next_power_of_2(8) == 8);
    EXPECT_TRUE(sparta::utils::next_power_of_2(31) == 32);
    EXPECT_TRUE(sparta::utils::next_power_of_2(32) == 32);
    EXPECT_TRUE(sparta::utils::next_power_of_2(63) == 64);
    EXPECT_TRUE(sparta::utils::next_power_of_2(64) == 64);
    EXPECT_TRUE(sparta::utils::next_power_of_2(511) == 512);
    EXPECT_TRUE(sparta::utils::next_power_of_2(1024) == 1024);
    EXPECT_TRUE(sparta::utils::next_power_of_2(1025) == 2048);
    EXPECT_TRUE(sparta::utils::next_power_of_2(2049) == 4096);

    //Test out lower/uppercase utilities with const char* constructor
    sparta::utils::lowercase_string lower_s1("HeLlO worLD");
    sparta::utils::lowercase_string lower_s2("helLO WoRlD");
    sparta::utils::lowercase_string lower_answer("hello world");

    EXPECT_TRUE(lower_s1 == lower_s2);
    EXPECT_TRUE(lower_s1 == lower_answer);

    sparta::utils::UPPERCASE_STRING upper_s1("HeLlO worLD");
    sparta::utils::UPPERCASE_STRING upper_s2("hEllO WoRlD");
    sparta::utils::UPPERCASE_STRING upper_answer("HELLO WORLD");

    EXPECT_TRUE(upper_s1 == upper_s2);
    EXPECT_TRUE(upper_s1 == upper_answer);

    //Test the lower/uppercase utilities with std::string constructor
    std::string std_string_name = "The Quick Brown Fox";
    sparta::utils::lowercase_string lower_s3(std_string_name);
    sparta::utils::UPPERCASE_STRING upper_s3(std_string_name);

    std::string lower_expected = std_string_name;
    std::transform(lower_expected.begin(),
                   lower_expected.end(),
                   lower_expected.begin(),
                   ::tolower);

    std::string upper_expected = std_string_name;
    std::transform(upper_expected.begin(),
                   upper_expected.end(),
                   upper_expected.begin(),
                   ::toupper);

    EXPECT_TRUE(lower_s3 == lower_expected);
    EXPECT_TRUE(upper_s3 == upper_expected);

    //Test the assignment operator (rhs is a TransformedString)
    sparta::utils::lowercase_string lower_s4;
    sparta::utils::UPPERCASE_STRING upper_s4;

    lower_s4 = lower_s3;
    EXPECT_TRUE(lower_s4 == lower_expected);

    upper_s4 = upper_s3;
    EXPECT_TRUE(upper_s4 == upper_expected);

    //Test the assignment operator (rhs is a std::string)
    sparta::utils::lowercase_string lower_s5;
    sparta::utils::UPPERCASE_STRING upper_s5;

    std_string_name = "Jumps Over The Lazy Dog";
    lower_s5 = std_string_name;
    upper_s5 = std_string_name;

    lower_expected = std_string_name;
    upper_expected = std_string_name;

    std::transform(lower_expected.begin(),
                   lower_expected.end(),
                   lower_expected.begin(),
                   ::tolower);

    std::transform(upper_expected.begin(),
                   upper_expected.end(),
                   upper_expected.begin(),
                   ::toupper);

    EXPECT_TRUE(lower_s5 == lower_expected);
    EXPECT_TRUE(upper_s5 == upper_expected);

    // utils::lowercase_string == std:string
    sparta::utils::lowercase_string lower_s6("FOO");
    lower_expected = "foo";
    EXPECT_TRUE(lower_s6 == lower_expected);

    // utils::lowercase_string != std:string
    sparta::utils::lowercase_string lower_s7("FOO");
    std::string lower_unexpected = "bar";
    EXPECT_TRUE(lower_s7 != lower_unexpected);

    // std::string == utils::lowercase_string
    EXPECT_TRUE(lower_expected == lower_s6);

    // std::string != utils::lowercase_string
    EXPECT_TRUE(lower_unexpected != lower_s7);

    // utils::UPPERCASE_STRING == std:string
    sparta::utils::UPPERCASE_STRING upper_s6("foo");
    upper_expected = "FOO";
    EXPECT_TRUE(upper_s6 == upper_expected);

    // utils::UPPERCASE_STRING != std:string
    sparta::utils::UPPERCASE_STRING upper_s7("foo");
    std::string upper_unexpected = "BAR";
    EXPECT_TRUE(upper_s7 != upper_unexpected);

    // std::string == utils::UPPERCASE_STRING
    EXPECT_TRUE(upper_unexpected != upper_s6);

    // std::string != utils::UPPERCASE_STRING
    EXPECT_TRUE(upper_unexpected != upper_s7);

    // utils::lowercase_string != utils::lowercase_string
    sparta::utils::lowercase_string lower_s8("hello"), lower_s9("world");
    EXPECT_TRUE(lower_s8 != lower_s9);

    // utils::UPPERCASE_STRING != utils::UPPERCASE_STRING
    sparta::utils::UPPERCASE_STRING upper_s8("HELLO"), upper_s9("WORLD");
    EXPECT_TRUE(upper_s8 != upper_s9);

    // Test cast-to-std-string operator
    sparta::utils::lowercase_string lower_s10("AbCdEfG");
    const std::string expected_from_get_string = lower_s10.getString();
    const std::string cast_from_utils_object = (std::string)lower_s10;
    EXPECT_TRUE(expected_from_get_string == cast_from_utils_object);

    // Test operator< operator
    std::set<std::string> expected_ordered_strings = {
        "biz",
        "foo",
        "bar",
        "baz"
    };

    std::set<sparta::utils::lowercase_string> actual_ordered_utils_objs;
    for (const auto & str : expected_ordered_strings) {
        actual_ordered_utils_objs.insert(str);
    }

    EXPECT_EQUAL(expected_ordered_strings.size(),
                 actual_ordered_utils_objs.size());

    auto expected_iter = expected_ordered_strings.begin();
    auto actual_iter = actual_ordered_utils_objs.begin();
    while (expected_iter != expected_ordered_strings.end()) {
        const std::string & expected_str = *expected_iter;
        const std::string & actual_str = *actual_iter;
        EXPECT_EQUAL(expected_str, actual_str);

        ++expected_iter;
        ++actual_iter;
    }

    //Test multi-line tokenizer using a specific line separator
    //and a specific token / delimiter applied to each line
    std::stringstream ss;
    ss << "x:foo?y:bar:z:buz";

    std::vector<std::vector<std::string>> str_vectors;
    sparta::utils::split_lines_around_tokens(ss, str_vectors, ":", '?');

    EXPECT_EQUAL(str_vectors.size(), 2);
    EXPECT_EQUAL(str_vectors[0].size(), 2);
    EXPECT_EQUAL(str_vectors[1].size(), 4);

    EXPECT_EQUAL(str_vectors[0][0], "x");
    EXPECT_EQUAL(str_vectors[0][1], "foo");

    EXPECT_EQUAL(str_vectors[1][0], "y");
    EXPECT_EQUAL(str_vectors[1][1], "bar");
    EXPECT_EQUAL(str_vectors[1][2], "z");
    EXPECT_EQUAL(str_vectors[1][3], "buz");

    REPORT_ERROR;
    return ERROR_CODE;
}
