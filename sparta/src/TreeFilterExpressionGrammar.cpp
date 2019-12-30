// <TreeFilterExpressionGrammar> -*- C++ -*-

#include "sparta/tree/filter/Grammar.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SmartLexicalCast.hpp"

namespace sparta {
    namespace tree {
        namespace filter {
            namespace grammar {


/*!
 * \brief Functor class for generating integers from visibility value strings
 */
class lazy_lex_cast_
{
public:

    /*!
     * \brief Constructor
     */
    lazy_lex_cast_()
    { }

    template <typename A1>
    struct result { typedef uint64_t type; };

    template <typename A1>
    uint64_t operator()(A1 a1) const
    {
        size_t end_pos;
        return utils::smartLexicalCast<uint64_t>(a1, end_pos);
    }

}; // class lazy_lex_cast_


Grammar::constants_::constants_()
{
    // Constants
    this->add
        ("true",              Expression(true))
        ("false",             Expression(false))
        ;
}

Grammar::VisVariable::VisVariable() :
    VisVariable::base_type(vis_expr)
{
    using qi::ascii::char_;
    using qi::_val;
    using qi::_1;
    using qi::_2;
    using qi::no_case;

    // Variable factory
    lazy_gen_var_ lgv;
    phoenix::function<lazy_gen_var_> lazy_gen_var(lgv);

    // lex caster
    lazy_lex_cast_ llc;
    phoenix::function<lazy_lex_cast_> lazy_lex_cast(llc);

    vis_expr =
        (no_case["=="] >> *qi::string(" ") >> vis_val [_val = lazy_gen_var(_1, Expression::VISCOMP_EQ)])
      | (no_case[">="] >> *qi::string(" ") >> vis_val [_val = lazy_gen_var(_1, Expression::VISCOMP_GE)])
      | (no_case["<="] >> *qi::string(" ") >> vis_val [_val = lazy_gen_var(_1, Expression::VISCOMP_LE)])
      | (no_case["<"]  >> *qi::string(" ") >> vis_val [_val = lazy_gen_var(_1, Expression::VISCOMP_LT)])
      | (no_case[">"]  >> *qi::string(" ") >> vis_val [_val = lazy_gen_var(_1, Expression::VISCOMP_GT)])
      | (no_case["!="] >> *qi::string(" ") >> vis_val [_val = lazy_gen_var(_1, Expression::VISCOMP_NE)])
      | (vis_val                                      [_val = lazy_gen_var(_1, Expression::VISCOMP_EQ)])
        ;

    vis_val =
        no_case["vis"] >> *(char_(" \t")) >> ":" >> *(char_(" \t")) >>
            (   (no_case["summary"] [_val = (uint32_t)InstrumentationNode::VIS_SUMMARY])
             |  (no_case["normal"]  [_val = (uint32_t)InstrumentationNode::VIS_NORMAL])
             |  (no_case["detail"]  [_val = (uint32_t)InstrumentationNode::VIS_DETAIL])
             |  (no_case["support"] [_val = (uint32_t)InstrumentationNode::VIS_SUPPORT])
             |  (no_case["hidden"]  [_val = (uint32_t)InstrumentationNode::VIS_HIDDEN])
             |  (num                [_val = lazy_lex_cast(_1)])
             )
        ;

    // Determine all valid characters in smartLexicalCast strings
    std::set<char> chars;
    for(const auto& rp: utils::prefixes){
        for(const auto& prfx : rp.options){
            for(char c : std::string(prfx)){
                chars.insert(c);
            }
        }
    }
    for(const auto& mod: utils::suffixes){
        for(const auto& sfx : mod.options){
            for(char c : std::string(sfx)){
                chars.insert(c);
            }
        }
    }
    for(char c : std::string(utils::DECIMAL_DIGITS)){
        chars.insert(c);
    }

    std::stringstream valid_chars;
    for(char c : chars){
        valid_chars << c;
    }

    // Visibility value chars
    num %= +(char_(valid_chars.str()));

    vis_expr.name("vis expr");
    vis_val.name("vis val");
    num.name("num");

    //qi::debug(vis_expr);
    //qi::debug(vis_val);
    //qi::debug(num);
}

Grammar::TypeVariable::TypeVariable() :
    TypeVariable::base_type(type_expr)
{
    using qi::ascii::char_;
    using qi::_val;
    using qi::_1;
    using qi::_2;
    using qi::no_case;

    // Variable factory
    lazy_gen_var_ lgv;
    phoenix::function<lazy_gen_var_> lazy_gen_var(lgv);

    type_expr =
            (no_case["=="] >> *qi::string(" ") >> type_val [_val = lazy_gen_var(_1, Expression::TYPECOMP_EQ)])
        |   (no_case["!="] >> *qi::string(" ") >> type_val [_val = lazy_gen_var(_1, Expression::TYPECOMP_NE)])
        |   (type_val                                      [_val = lazy_gen_var(_1, Expression::TYPECOMP_EQ)])
        ;

    type_val =
        no_case["type"] >> *(char_(" \t")) >> ":" >> *(char_(" \t")) >>
            (   (no_case["statistic"]    [_val = InstrumentationNode::TYPE_STATISTICDEF])
             |  (no_case["statisticdef"] [_val = InstrumentationNode::TYPE_STATISTICDEF])
             |  (no_case["stat"]         [_val = InstrumentationNode::TYPE_STATISTICDEF])
             |  (no_case["statdef"]      [_val = InstrumentationNode::TYPE_STATISTICDEF])
             |  (no_case["counter"]      [_val = InstrumentationNode::TYPE_COUNTER])
             |  (no_case["parameter"]    [_val = InstrumentationNode::TYPE_PARAMETER])
             |  (no_case["param"]        [_val = InstrumentationNode::TYPE_PARAMETER])
             |  (no_case["histogram"]    [_val = InstrumentationNode::TYPE_HISTOGRAM])
             )
        ;

    type_expr.name("type expr");
    type_val.name("type val");

    //qi::debug(type_expr);
    //qi::debug(type_val);
}



Grammar::Grammar() :
    Grammar::base_type(logic_xor)
{
    namespace qi = qi;
    using qi::no_case;
    using qi::_val;
    using phoenix::ref;

    logic_xor =
        logic_or                   [_val =  qi::_1]
        >> *(  ("^^" >> logic_or   [_val ^= qi::_1])
            )
        ;
    auto& toplevel = logic_xor; // Move up if anything has lower precedence than this

    logic_or =
        logic_and                  [_val =  qi::_1]
        >> *(  ("||" >> logic_and  [_val |= qi::_1])
            )
        ;

    logic_and =
        inversion                  [_val =  qi::_1]
        >> *(  ("&&" >> inversion  [_val &= qi::_1])
            )
        ;

    inversion =
          ('!' >> vis_comp       [_val = !qi::_1])
        | ("not" >> vis_comp     [_val = !qi::_1])
        | vis_comp               [_val =  qi::_1]
        ;

    vis_comp =
            no_case[vis_var]       [_val =  qi::_1]
        |   no_case[type_var]      [_val =  qi::_1]
        |   no_case[tag_var]       [_val =  qi::_1]
        |   no_case[name_var]      [_val =  qi::_1]
        |   primary                [_val =  qi::_1]
        ;

    primary =
            '(' >> toplevel        [_val =  qi::_1] >> ')'
        |   no_case[constants]     [_val =  qi::_1]
        ;

    logic_xor.name("lxor");
    logic_or.name("lor");
    logic_and.name("land");
    inversion.name("not");
    vis_comp.name("vis comp");
    primary.name("primary");

    qi::on_error<qi::fail>
    (
        toplevel
      , std::cout
            << phoenix::val("Error! Expecting ")
            << qi::_4                               // what failed?
            << phoenix::val(" here: \"")
            << phoenix::construct<std::string>(qi::_3, qi::_2)   // iterators to error-pos, end
            << phoenix::val("\"")
            << std::endl
    );

    // Debugging switches
    //qi::debug(logic_xor);
    //qi::debug(logic_or);
    //qi::debug(logic_and);
    //qi::debug(inversion);
    //qi::debug(vis_comp);
    //qi::debug(primary);
}

            } // namespace grammar
        } // namespace filter
    } // namespace tree
} // namespace sparta
