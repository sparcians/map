
#include <iostream>
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"

TEST_INIT

void throw_an_exception()
{
    sparta::SpartaException e("My reasons are purely my own");
    e << "filename and line: ";
    e << 10;
    throw e;
}

int main()
{
    sparta::SpartaException e("My reasons are purely my own");
    e << ": filename and line: ";
    e << 10;
    e << std::hex << " 0x" << 10;

    std::cout << e.what() << std::endl;

    EXPECT_THROW(throw_an_exception());

    EXPECT_THROW(sparta_assert(!"This shoud fail"));

    REPORT_ERROR;
    return ERROR_CODE;
}
