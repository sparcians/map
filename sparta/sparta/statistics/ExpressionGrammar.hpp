// <ExpressionGrammar.hpp> -*- C++ -*-

#define BOOST_SPIRIT_USE_PHOENIX_V3 0

#pragma once

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>

#include "sparta/statistics/Expression.hpp"
#include "sparta/simulation/TreeNode.hpp"

#include <string>

// Alias deep boost namespaces
namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;


namespace sparta {
    namespace statistics {
        namespace expression {

            /*!
             * \brief Statistical Expression Grammar
             */
            namespace grammar {


class ExpressionGrammar :
    public qi::grammar<std::string::const_iterator,
                       Expression(),
                       ascii::space_type>
{
    /*!
     * \brief Symbol table for constant
     */
    struct constants_
        : qi::symbols<typename std::iterator_traits<std::string::const_iterator>::value_type,
                      Expression>
    {
        constants_();

    }; // struct constants_

    /*!
     * \brief Symbol table for built-in (simulator) variables taking no
     * arguments.
     */
    struct builtin_vars_
        : qi::symbols<typename std::iterator_traits<std::string::const_iterator>::value_type,
                      std::function<Expression(void)>>
    {
        /*!
         * \brief Constructor
         * \param n TreeNode context for evaluating builtin variables
         * \param used TreeNodes which cannot be variables because they have
         *             already been used by an expression containing this one
         */
        builtin_vars_(sparta::TreeNode* n,
                      std::vector<const TreeNode*>& used);

    }; // struct builtin_vars_

    /*!
     * \brief Dynamic variable (TreeNode|simulation) name parser
     */
    struct variable_ : qi::grammar<std::string::const_iterator,
                                   Expression(),
                                   ascii::space_type>
    {
        /*!
         * \brief Constructor
         * \param n TreeNode context for evaluating dynamic variables
         * \param used TreeNodes which cannot be variables because they have
         *             already been used by an expression containing this one
         * \param report_si Exising report statistic instances
         */
        variable_(sparta::TreeNode* n,
                  std::vector<const TreeNode*>& used,
                  const std::vector<stat_pair_t> & report_si);

        qi::rule<std::string::const_iterator,
                 Expression(),
                 ascii::space_type> start;
        qi::rule<std::string::const_iterator,
                 std::string(),
                 ascii::space_type> str;
    }; // struct variable_

    /*!
     * \brief Symbol table for unary  functions: e.g.: double(*)(double).
     * Can also be used for builtin expression modifiers which take an argument
     */
    struct ufunc_
        : qi::symbols<typename std::iterator_traits<std::string::const_iterator>::value_type,
                      std::function<Expression(Expression&)>>
    {
        /*!
         * \brief Function pointer type when returning a double
         */
        typedef double(*fptr_dd_t)(double);

        /*!
         * \brief Function pointer type when returning a bool
         */
        typedef bool(*fptr_bd_t)(long double);

        /*!
         * \brief create table of function pointers
         */
        ufunc_(std::vector<const TreeNode*>& already_used);

    }; // ufunc_

    /*!
     * \brief Symbol table for binary functions: e.g. double(*)(double,double)
     * Can also be used for builtin expression modifiers which take an argument
     */
    struct bfunc_
        : qi::symbols<typename std::iterator_traits<std::string::const_iterator>::value_type,
                      std::function<Expression(Expression&, Expression&)>>
    {
        /*!
         * \brief Function pointer type
         */
        typedef double(*fptr_ddd_t)(double, double);

        /*!
         * \brief Function pointer type where arguments and return value are
         * const references
         */
        typedef const double&(*fptr_drdrdrt)(const double&, const double&);

        /*!
         * \brief create table of function pointers
         */
        bfunc_(const std::vector<const TreeNode*>& already_used);

    }; // bfunc_

     /*!
     * \brief Symbol table for ternary functions: i.g. double(*)(double,double,double)
     * Can also be used for builtin expression modifiers which take an argument
     */
    struct tfunc_
        : qi::symbols<typename std::iterator_traits<std::string::const_iterator>::value_type,
                      std::function<Expression(Expression&, Expression&, Expression&)>>
    {
        /*!
         * \brief Function pointer type
         */
        typedef double(*fptr_dddd_t)(double, double, double);

        /*!
         * \brief Function pointer type where arguments and return value are
         * const references
         */
        typedef const double&(*fptr_drdrdrdrt)(const double&, const double&, const double&);

        /*!
         * \brief create table of function pointers
         */
        tfunc_(const std::vector<const TreeNode*>& already_used);

    }; // tfunc_


    // Sub-Parsers

    struct constants_ constants;
    struct builtin_vars_ builtin_vars;
    struct ufunc_ ufunc;
    struct bfunc_ bfunc;
    struct tfunc_ tfunc;
    struct variable_ var;


    // Configuration

    /*!
     * \brief Root TreeNode for looking up node names encountered during parsing
     */
    sparta::TreeNode* const root_;

    /*!
     * \brief Disallowed nodes because they would create a cycle
     */
    std::vector<const TreeNode*> already_used_;


    // Rules

    typedef qi::rule<std::string::const_iterator,
                     Expression(),
                     ascii::space_type> ExpressionRule;

    ExpressionRule expression,
                   term,
                   factor,
                   primary;

public:

    /*!
     * \brief Expression Grammar constructor
     * \param root Root TreeNode context for looking up children node names
     * encountered during parsing
     * \param already_used Nodes which have been used by an expression
     * containing this. These Nodes are off-limits for parsing here and should
     * throw an Exception if encountered
     * \param report_si StatisticInstance objects already created from
     *                   previous expressions that now live in the report
     */
    ExpressionGrammar(sparta::TreeNode* root,
                      std::vector<const TreeNode*>& already_used,
                      const std::vector<statistics::stat_pair_t> &report_si);


}; // struct grammar

            } // namespace grammar
        } // namespace expression
    } // namespace statistics
} // namespace sparta
