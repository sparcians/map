// <Parser> -*- C++ -*-

/*!
 * \file Parser.hpp
 * \brief Defines a parser that uses the tree filter grammar
 */

#pragma once

#include <iostream>
#include <sstream>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/statistics/StatisticInstance.hpp"

#include "sparta/tree/filter/Grammar.hpp"

#include <memory>
#include <stack>

namespace sparta {
    namespace tree {
        namespace filter {

/*!
 * \brief Class for parsing statistical expressions based on a specific TreeNode
 * context
 */
class Parser
{
    /*!
     * \brief Temporary expression for use when returning rvalue references from
     * the parse method
     * \note Should not be accessed outside of parse
     */
    Expression parse_temp_;

    const filter::grammar::Grammar grammar_;

public:

    /*!
     * \brief Construct a parser in the context of a specific TreeNode
     * \param n TreeNode context for parsing expressions using this parser
     * \param already_used Nodes which have been used by an expression
     * containing this. These Nodes are off-limits for parsing here and should
     * throw an Exception if encountered
     */
    Parser()
    { }

    Expression&& parse(const std::string& input) {
        using boost::spirit::ascii::space;

        std::string::const_iterator iter = input.begin();
        std::string::const_iterator end  = input.end();
        bool r = qi::phrase_parse(iter, end, grammar_, space, parse_temp_);

        if (!r || iter != end){
            throw SpartaException("Parsing tree filter expression \"") << input << "\" failed to "
                "parse remainder \"" << std::string(iter, end) << "\". Note that this does not "
                "indicate which part of the expression was wrong, but how much the recursive "
                "parser could properly interpret";
        }

        return std::move(parse_temp_);
    }
}; // class Parser

        } // namespace filter
    } // namespace tree
} // namespace sparta
