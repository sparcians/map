// <Grammar> -*- C++ -*-

/*!
 * \file Grammar.hpp
 * \brief Grammar for specifying a filtering function for TreeNodes based on
 * their attributes
 */

#pragma once

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/operator.hpp>
#include <boost/phoenix/fusion.hpp>
#include <boost/phoenix/stl.hpp>
#include <boost/phoenix/object.hpp>

#include "sparta/statistics/StatisticDef.hpp"
#include "sparta/statistics/CounterBase.hpp"
#include "sparta/statistics/InstrumentationNode.hpp"

#include "sparta/tree/filter/Expression.hpp"

#include <string>

// Alias deep boost namespaces
namespace fusion = boost::fusion;
namespace phoenix = boost::phoenix;
namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;


namespace sparta {
    namespace tree {
        namespace filter {
            namespace grammar {


/*!
 * \brief Functor class for generating expressions in VisVariable semantic actions
 */
class lazy_gen_var_
{
public:

    /*!
     * \brief Constructor
     */
    lazy_gen_var_()
    { }

    template <typename A1, typename A2>
    struct result { typedef Expression type; };

    /*!
     * \brief Lazy call. This method handles multiple signatures of
     * expression constructor depending on whether it is called from VisVariable
     * or TypeVariable
     * \verbatim
     * Expression(uint64_t, Expression::VisibilityComparison)
     * \endverbatim
     * and
     * \verbatim
     * Expression(InstrumentationNode::Type, Expression::TypeComparison)
     * \endverbatim
     */
    template <typename A1, typename A2>
    Expression operator()(A1 a1, A2 a2) const
    {
        return Expression(a1, a2);
    }
}; // class lazy_gen_var_

/*!
 * \brief Grammar for defining a filtering function for a TreeNode based on its
 * attributes.
 *
 * \verbatim
   expression ::= lxor
   lxor       ::= lor ("^^" lor)?
   lor        ::= land ("||" land)?
   land       ::= not ("&&" not)?
   not        ::= ("!" | "not") ? attr_comp
   attr_comp  ::= vis_expr | type_expr | tag_expr | primary
   primary    ::= "(" expression ")" | constant
   constant   ::= true | false
   vis_expr   ::=
               ( "=="
               | ">="
               | "<="
               | "<"
               | ">"
               | "!="
               )?
               " "*
               vis_val
   vis_val    ::= "vis" " "* ":" " "*
                 ("summary" | "normal" | "detail" | "support" | "hidden" | vis_num)
   vis_num    ::= [0-9]+
   type_expr  ::=
               ( "=="
               | "!="
               )?
               " "*
               type_val
   type_val    ::= "type" " "* ":" " "*
                 ("statistic" | "statisticdef" | "stat" | "statdef" | "counter" | "parameter" | "param" | "histogram")
   tag_expr   ::=
               ( "=="
               | "!="
               | "regex"
               )?
               " "*
               tag_val
   tag_val    ::= "tag" pm_expr
   name_expr  ::=
               ( "=="
               | "!="
               | "regex"
               )?
               " "*
               name_val
   name_val   ::= "name" pm_expr
   pm_expr    ::= " "* ":" " "*
                  ("\"" dq_pattern "\"")
                | ("'' sq_pattern "'')
                | pattern
   pattern ::= <regex with no spaces>
   dq_patern ::= <regex with spaces and double quotes>
   sq_patern ::= <regex with spaces and single quotes>
   \endverbatim
 * For a diagram of this grammar, see: <a href="../examples/tree_filter_grammar.xhtml"><b>tree_filter_grammar.xhtml</b></a>
 */
class Grammar :
    public qi::grammar<std::string::const_iterator,
                       Expression(),
                       boost::spirit::ascii::space_type>
{
    /*!
     * \brief Symbol table for constants
     */
    struct constants_
        : qi::symbols<typename std::iterator_traits<std::string::const_iterator>::value_type,
                      Expression>
    {
        constants_();

    }; // struct constants_

    /*!
     * \brief Visibility Variable. Parses a visibility attribute comparison
     * and builds an expression
     */
    struct VisVariable : qi::grammar<std::string::const_iterator,
                                     Expression(),
                                     boost::spirit::ascii::space_type>
    {
        /*!
         * \brief Constructor
         */
        VisVariable();

        qi::rule<std::string::const_iterator,
                 std::string(),
                 boost::spirit::ascii::space_type> num;
        qi::rule<std::string::const_iterator,
                 Expression(),
                 boost::spirit::ascii::space_type> vis_expr;
        qi::rule<std::string::const_iterator,
                 uint64_t(),
                 boost::spirit::ascii::space_type> vis_val;
    };

    /*!
     * \brief Type Variable. Parses a type attribute comparison
     * and builds an expression
     */
    struct TypeVariable : qi::grammar<std::string::const_iterator,
                                      Expression(),
                                      boost::spirit::ascii::space_type>
    {
        /*!
         * \brief Constructor
         */
        TypeVariable();

        qi::rule<std::string::const_iterator,
                 Expression(),
                 boost::spirit::ascii::space_type> type_expr;
        qi::rule<std::string::const_iterator,
                 InstrumentationNode::Type(),
                 boost::spirit::ascii::space_type> type_val;
    };

    /*!
     * \brief Regex-based Variable supporting only ==, != and regex matches
     * (e.g. names, tags. Parses an attribute comparison of the attribute
     * selected during construction and builds an expression.
     */
    template <typename OperationT, OperationT eq, OperationT ne, OperationT re>
    struct RegexVariable : qi::grammar<std::string::const_iterator,
                                      Expression(),
                                      boost::spirit::ascii::space_type>
    {
        std::string attribute_;
        /*!
         * \brief Constructor
         * \param[in] attribute Attribute to match on
         */
        RegexVariable(const std::string & attribute) :
            RegexVariable::base_type(tag_expr),
            attribute_(attribute)
        {
            using qi::ascii::char_;
            using qi::_val;
            using qi::_1;
            using qi::_2;
            using qi::no_case;

            // Variable factory
            lazy_gen_var_ lgv;
            phoenix::function<lazy_gen_var_> lazy_gen_var(lgv);

            tag_expr =
                    (no_case["=="] >> *qi::string(" ") >> tag_val    [_val = lazy_gen_var(_1, eq)])
                |   (no_case["!="] >> *qi::string(" ") >> tag_val    [_val = lazy_gen_var(_1, ne)])
                |   (no_case["regex"] >> *qi::string(" ") >> tag_val [_val = lazy_gen_var(_1, re)])
                |   (tag_val                                         [_val = lazy_gen_var(_1, eq)])
                ;

            tag_val =
                // Tags with space-less regex expressions
                  (no_case[attribute_] >> *(char_(" \t")) >> ":" >> *(char_(" \t")) >>
                    tag_pattern_simple [_val = _1]
                  )
                // Tags with spaces (requires quotes)
                | (no_case[attribute_] >> *(char_(" \t")) >> ":" >> *(char_(" \t")) >>
                    (
                      // Double-Quote
                        (char_("\"") >>
                         (tag_pattern_indq [_val = _1]) >> // All regex items are ok
                         char_("\"")
                        )
                      // Single-Quote
                      | (char_("'") >>
                         (tag_pattern_insq [_val = _1]) >> // All regex items are ok
                         char_("'")
                        )
                    )
                  )
                ;

            tag_pattern_simple %= +(char_("\\.0-9A-Za-z_\\[\\]\\+\\-\\(\\)$\\^\\?\\*")); // No spaces or quotes
            tag_pattern_indq %= +(char_("\\.0-9A-Za-z_\\[\\]\\+\\-\\(\\)$\\^\\?\\*\\\\ '")); // Spaces and single quotes ok
            tag_pattern_insq %= +(char_("\\.0-9A-Za-z_\\[\\]\\+\\-\\(\\)$\\^\\?\\*\\\\ \\\"")); // Spaces and double quotes ok

            tag_expr.name("expr");
            tag_val.name("val");
            tag_pattern_simple.name("pattern simple");
            tag_pattern_indq.name("pattern in double quotes");
            tag_pattern_insq.name("pattern in single quotes");

            //qi::debug(tag_expr);
            //qi::debug(tag_val);
            //qi::debug(tag_pattern_simple);
            //qi::debug(tag_pattern_indq);
            //qi::debug(tag_pattern_insq);
        }

        qi::rule<std::string::const_iterator,
                 Expression(),
                 boost::spirit::ascii::space_type> tag_expr;
        qi::rule<std::string::const_iterator,
                 std::string(),
                 boost::spirit::ascii::space_type> tag_val;
        qi::rule<std::string::const_iterator,
                 std::string()> tag_pattern_simple; //!< Unquoted tag expr
        qi::rule<std::string::const_iterator,
                 std::string()> tag_pattern_indq; //!< Double quoted tag expr
        qi::rule<std::string::const_iterator,
                 std::string()> tag_pattern_insq; //!< Single-quoted tag expr

    };


    // Sub-Parsers

    struct constants_ constants;
    struct VisVariable vis_var;
    struct TypeVariable type_var;
    struct RegexVariable <Expression::TagComparison,
                          Expression::TAGCOMP_EQ,
                          Expression::TAGCOMP_NE,
                          Expression::TAGCOMP_REM> tag_var{{"tag"}};
    struct RegexVariable <Expression::NameComparison,
                          Expression::NAMECOMP_EQ,
                          Expression::NAMECOMP_NE,
                          Expression::NAMECOMP_REM> name_var{{"name"}};


    // Rules

    typedef qi::rule<std::string::const_iterator,
                     Expression(),
                     boost::spirit::ascii::space_type> ExpressionRule;

    ExpressionRule logic_xor,
                   logic_or,
                   logic_and,
                   inversion,
                   vis_comp,
                   primary;

public:

    /*!
     * \brief Expression Grammar constructor
     */
    Grammar();


}; // class Grammar

            } // namespace grammar
        } // namespace filter
    } // namespace tree
} // namespace sparta
