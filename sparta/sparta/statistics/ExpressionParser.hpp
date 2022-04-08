// <ExpressionParser> -*- C++ -*-

#pragma once

#include <iostream>
#include <sstream>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/statistics/StatisticInstance.hpp"

#include "sparta/statistics/Expression.hpp"
#include "sparta/statistics/ExpressionNodeVariables.hpp"
#include "sparta/statistics/ExpressionGrammar.hpp"

#include <memory>
#include <stack>

namespace sparta {
    namespace statistics {
        namespace expression {

/*!
 * \brief Class for parsing statistical expressions based on a specific TreeNode
 * context
 */
class ExpressionParser
{
    /*!
     * \brief Temporary expression for use when returning rvalue references from
     * the parse method
     * \note Should not be accessed outside of parse
     */
    Expression parse_temp_;

    const grammar::ExpressionGrammar grammar_;

public:

    /*!
     * \brief Construct a parser in the context of a specific TreeNode
     * \param n TreeNode context for parsing expressions using this parser
     * \param already_used Nodes which have been used by an expression
     *                     containing this. These Nodes are off-limits
     *                     for parsing here and should throw an
     *                     Exception if encountered
     * \param report_si StatisticInstance objects already created from
     *                   previous expressions that now live in the report
     */
    ExpressionParser(TreeNode* n,
                     std::vector<const TreeNode*>& already_used,
                     const std::vector<statistics::stat_pair_t> &report_si) :
        grammar_(n, already_used, report_si)
    { }

    Expression&& parse(const std::string& input) {
        using boost::spirit::ascii::space;

        std::string::const_iterator iter = input.begin();
        std::string::const_iterator end  = input.end();
        bool r = qi::phrase_parse(iter, end, grammar_, space, parse_temp_);

        if (!r || iter != end){
            throw SpartaException("Parsing expression \"") << input << "\" failed to parse remainder"
                << " \"" << std::string(iter, end) << "\". Note that this does not indicate which "
                "part of the expression was wrong, but how much the recursive parser could "
                "properly interpret";
        }

        return std::move(parse_temp_);
    }
};

        } // namespace expression
    } // namespace statistics
} // namespace sparta
