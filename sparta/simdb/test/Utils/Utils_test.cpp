/*!
 * \file Utils_test.cpp
 *
 * \brief Tests for various utility classes in SimDB.
 */

#include "simdb/test/SimDBTester.hpp"

//Core database headers
#include "simdb/utils/StringUtils.hpp"

//Standard headers
#include <string>

#define PRINT_ENTER_TEST                                                              \
  std::cout << std::endl;                                                             \
  std::cout << "*************************************************************"        \
            << "*** Beginning '" << __FUNCTION__ << "'"                               \
            << "*************************************************************"        \
            << std::endl;

void testTransformedString()
{
    PRINT_ENTER_TEST

    simdb::utils::lowercase_string lower("HeLlOWoRlD");

    //Quick check for the size() and empty() methods
    const std::string lower_cppstring(lower.getString());
    EXPECT_EQUAL(lower.size(), lower_cppstring.size());
    EXPECT_FALSE(lower.empty());

    //Test equality with const char*
    const char * lower_expected1_cstring = "helloworld";
    EXPECT_EQUAL(lower, lower_expected1_cstring);

    //Append const char*
    lower += "_HELLOAGAIN";

    //Test equality with std::string
    const std::string lower_expected2_cppstring = "helloworld_helloagain";
    EXPECT_EQUAL(lower, lower_expected2_cppstring);

    //Append std::string
    lower += std::string("_GoodBye");

    //Test equality with std::string
    EXPECT_EQUAL(lower, "helloworld_helloagain_goodbye");

    //Append char
    lower += '!';

    //Test equality with std::string
    EXPECT_EQUAL(lower, "helloworld_helloagain_goodbye!");

    //Invoke copy constructor from lowercase to uppercase
    simdb::utils::UPPERCASE_STRING upper(lower);
    EXPECT_EQUAL(upper, "HELLOWORLD_HELLOAGAIN_GOODBYE!");

    //Final check for clear() and empty() methods
    upper.clear();
    EXPECT_TRUE(upper.empty());
}

int main()
{
    testTransformedString();
}
