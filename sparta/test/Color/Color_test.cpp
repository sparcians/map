
#include <iostream>

#include "sparta/sparta.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/Colors.hpp"
#include "sparta/utils/SpartaTester.hpp"

/*!
 * \file Color_test.cpp
 * \brief Test for sparta color lists. This ensures that colors can be iterated
 * without crashing. There is no expectation of output
 */

TEST_INIT

int main() {
    // Try the Color scheme class
    sparta::color::ColorScheme color_scheme;

    // Print some colors
    const char* const * color = color_scheme.nextBasicColor();
    for(uint32_t i=0; i<40; ++i){
        // Print out the next color
        std::cout << *color << "\\/";
        color = color_scheme.nextBasicColor(color);
    }
    color = color_scheme.nextBasicColor();
    std::cout << std::endl;
    for(uint32_t i=0; i<40; ++i){
        // Print out the next color
        std::cout << *color << "/\\";
        color = color_scheme.nextBasicColor(color);
    }


    char name_str[2] = "a";
    char& name = name_str[0];
    std::vector<std::unique_ptr<sparta::TreeNode>> nodes;

    // Build up the deep tree
    sparta::TreeNode* prev_node = nullptr;
    for(uint32_t i=0; i<26; ++i){
        nodes.emplace_back(new sparta::TreeNode(prev_node, name_str, "A node"));
        prev_node = nodes.back().get();
        name += 1;
    }

    // Print out the tree (in color)
    std::cout << "\nThe tree from the top (with all builtin groups): " << std::endl
        << nodes.front()->renderSubtree(-1, true) << std::endl;




    std::string red(color_scheme.color(Color::BrightRed));
    EXPECT_EQUAL(red, std::string(SPARTA_UNMANAGED_COLOR_BRIGHT_RED));
    std::string green(color_scheme.color(Color::Green));
    EXPECT_EQUAL(green, std::string(SPARTA_UNMANAGED_COLOR_GREEN));
    EXPECT_EQUAL(std::string(*color_scheme.nextBasicColor()), std::string(SPARTA_UNMANAGED_COLOR_RED));
    const char* const * firstcol = color_scheme.nextBasicColor();
    EXPECT_EQUAL(std::string(*color_scheme.nextBasicColor(firstcol)), std::string(SPARTA_UNMANAGED_COLOR_GREEN));

    std::cout << color_scheme.color(Color::Green) << " Yay this is green! " << color_scheme.color(Color::Normal) << std::endl;
    color_scheme.enabled(false);
    std::string nored(color_scheme.color(Color::Red));
    EXPECT_EQUAL(nored, "");
    EXPECT_EQUAL(std::string(*color_scheme.nextBasicColor(firstcol)), std::string(""));



    // Done

    REPORT_ERROR;

    return ERROR_CODE;
}
