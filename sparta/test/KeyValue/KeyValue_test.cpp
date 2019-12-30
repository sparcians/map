
#include <inttypes.h>
#include <iostream>

#include "sparta/sparta.hpp"
#include "sparta/utils/KeyValue.hpp"
#include "sparta/utils/SpartaTester.hpp"

TEST_INIT;

using sparta::KeyValue;

int main()
{
    KeyValue val1("unsigned integer", 10u);
    KeyValue val2("a float", 10.1);
    KeyValue val3("string", (std::string)"hello");
    KeyValue val4("signed ", -10);
    KeyValue val5("unsigned long", uint64_t(10));

    uint32_t v1_inst = val1;
    std::cout << v1_inst << std::endl;
    EXPECT_TRUE(v1_inst == 10);

    std::string v3_inst = val3;
    std::cout << v3_inst << std::endl;
    EXPECT_TRUE(v3_inst == "hello");

    std::cout << val1.getValue<uint32_t>() << std::endl;

    uint32_t count = 0;
    for(uint32_t i = -0; i < 1000000000; ++i) {
        v1_inst = val1.getValue<uint32_t>();
        ++count;
    }

    std::cout << "Got it " << count << " times" << std::endl;

    std::cout << uint32_t(val1) << std::endl;
    std::cout << val3.getValue<std::string>() << std::endl;

    uint64_t v5 = val5;
    uint32_t v6 = 0;
    EXPECT_THROW(v6 = val5);

    (void) v5;
    (void) v6;


    REPORT_ERROR;

    return ERROR_CODE;
}
