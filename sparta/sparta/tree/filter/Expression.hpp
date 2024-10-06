// <Expression.hpp> -*- C++ -*-

/*!
 * \file Expression.hpp
 * \brief Expression for representing a filtering function for TreeNodes based
 *        on their attributes
 */

#pragma once

#include "sparta/statistics/StatisticDef.hpp"
#include "sparta/statistics/CounterBase.hpp"
#include "sparta/statistics/InstrumentationNode.hpp"

#include <string>


namespace sparta {
    namespace tree {
        namespace filter {

class Expression;

std::ostream& operator<<(std::ostream& out, const Expression& e);

class Expression
{
public:

    /*!
     * \brief Type of comparison to perform on visibility attribute
     */
    enum VisibilityComparison {
        VISCOMP_EQ = 0,
        VISCOMP_GT = 1,
        VISCOMP_LT = 2,
        VISCOMP_GE = 3,
        VISCOMP_LE = 4,
        VISCOMP_NE = 5,
        NUM_VISCOMPS
    };

    /*!
     * \brief Type of comparison to perform on type attribute
     */
    enum TypeComparison {
        TYPECOMP_EQ = 0,
        TYPECOMP_NE = 1,
        NUM_TYPECOMPS
    };

    /*!
     * \brief Type of comparison to perform on tag attribute
     */
    enum TagComparison {
        TAGCOMP_EQ  = 0,
        TAGCOMP_NE  = 1,
        TAGCOMP_REM = 2, //!< Regex match with tag
        NUM_TAGCOMPS
    };


    /*!
     * \brief Type of comparison to perform on tag attribute
     */
    enum NameComparison {
        NAMECOMP_EQ  = 0,
        NAMECOMP_NE  = 1,
        NAMECOMP_REM = 2, //!< Regex match with name
        NUM_NAMECOMPS
    };


    /*!
     * \brief Operation types of a node in the expression
     */
    enum Operation {
        OP_INVALID = 0,
        OP_FALSE,
        OP_TRUE,
        OP_XOR,
        OP_OR,
        OP_AND,
        OP_NOT,
        OP_EVAL_VIS,
        OP_EVAL_TYPE,
        OP_EVAL_TAG,
        OP_EVAL_NAME,
        NUM_OPS
    };

    Expression() = default;

    /*!
     * \brief Copy constructor
     */
    //Expression(const Expression&) = default;
    Expression(const Expression& rhp) {
        op_ = rhp.op_;
        operands_ = rhp.operands_;
        instrument_type_ = rhp.instrument_type_;
        visibility_ = rhp.visibility_;
        tag_ = rhp.tag_;
        name_ = rhp.name_;
        vis_comparison_ = rhp.vis_comparison_;
        type_comparison_ = rhp.type_comparison_;
        tag_comparison_ = rhp.tag_comparison_;
        name_comparison_ = rhp.name_comparison_;
    }

    /*!
     * \brief Move constructor
     */
    Expression(Expression&& rhp) {
        op_ = rhp.op_;
        operands_ = rhp.operands_;
        instrument_type_ = rhp.instrument_type_;
        visibility_ = rhp.visibility_;
        tag_ = rhp.tag_;
        name_ = rhp.name_;
        vis_comparison_ = rhp.vis_comparison_;
        type_comparison_ = rhp.type_comparison_;
        tag_comparison_ = rhp.tag_comparison_;
        name_comparison_ = rhp.name_comparison_;

        // Effectively expire rhp
        rhp.op_ = OP_INVALID;
    }

    /*!
     * \brief construct with a visibility
     */
    Expression(InstrumentationNode::Visibility vis,
               VisibilityComparison vcomp=VISCOMP_EQ) {
        visibility_ = vis;
        vis_comparison_ = vcomp;
        op_ = OP_EVAL_VIS;
    }

    /*!
     * \brief construct with a visibility
     */
    Expression(uint64_t vis,
               VisibilityComparison vcomp) {
        visibility_ = vis;
        vis_comparison_ = vcomp;
        op_ = OP_EVAL_VIS;
    }

    /*!
     * \brief Construct with a type
     */
    Expression(InstrumentationNode::Type type,
               TypeComparison tcomp=TYPECOMP_EQ) {
        instrument_type_ = type;
        type_comparison_ = tcomp;
        op_ = OP_EVAL_TYPE;
    }

    /*!
     * \brief Construct with a tag/pattern
     */
    Expression(const std::string& tag,
               TagComparison tcomp=TAGCOMP_EQ) {
        tag_ = tag;
        tag_comparison_ = tcomp;
        op_ = OP_EVAL_TAG;
    }

    /*!
     * \brief Construct with a name/pattern
     */
    Expression(const std::string& name,
               NameComparison ncomp=NAMECOMP_EQ) {
        name_ = name;
        name_comparison_ = ncomp;
        op_ = OP_EVAL_NAME;
    }


    /*!
     * \brief Boolean const constructor. Creates node with operation of OP_TRUE
     * or OP_FALSE depending on \a f
     * \param f boolean value that determines which operation to intialize this
     * expression node with.
     */
    Expression(bool f) {
        if(f){
            op_ = OP_TRUE;
        }else{
            op_ = OP_FALSE;
        }
    }

    /*!
     * \brief Assignment operator
     */
    Expression& operator=(const Expression& rhp) {
        op_ = rhp.op_;
        operands_ = rhp.operands_;
        instrument_type_ = rhp.instrument_type_;
        visibility_ = rhp.visibility_;
        tag_ = rhp.tag_;
        name_ = rhp.name_;
        vis_comparison_ = rhp.vis_comparison_;
        type_comparison_ = rhp.type_comparison_;
        tag_comparison_ = rhp.tag_comparison_;
        name_comparison_ = rhp.name_comparison_;

        return *this;
    }

    /*!
     * \brief Tese this expression for validity on a particular node
     * \param n Node whose attributes will be tested against this expression.
     * Must not be nullptr
     * \param trace Inf true, print a trace of all tests and intermediate
     * results to cout when evaluating this expression.
     * \return true if the node given meet the expression's conditions
     */
    bool valid(const TreeNode* n, bool trace=false) const;

    /*!
     * \brief Compare the visibility of the given node against the visibility
     * type stored in visibility_
     * \return True if node \a n has a InstrumentationNode visibility which matches
     * visibility_. Returns false if n is not an instrumentation node
     * \param n Node whose visibility type will be checked
     */
    bool evaluateVisibility_(const TreeNode* n, bool trace=false) const;

    /*!
     * \brief Compare the type of the given node against the type stored in
     * instrument_type_
     * \return True if node \a n has a InstrumentationNode type which matches
     * instrument_type_. Returns false if n is not a instrumentationnode
     * \param n Node whose type will be checked
     */
    bool evaluateType_(const TreeNode* n, bool trace=false) const;

    /*!
     * \brief Compare the tags of the given node against the tag stored in
     * tag_
     * \return True if node \a n has a tag matching the current tag_. Comparison
     * is based on tag_comparison_ value
     * \param n Node whose tags will be checked
     */
    bool evaluateTag_(const TreeNode* n, bool trace=false) const;

    /*!
     * \brief Compare the name of the given node against name_
     * \return True if node \a n's name matches name_. Comparison
     * is based on name_comparison_ value
     * \param n Node whose name will be checked
     */
    bool evaluateName_(const TreeNode* n, bool trace=false) const;

    /*!
     * \brief Convert this expression to a string (as a debug-level description)
     * \note This output cannot be reparsed as an expression
     */
    std::string stringize() const {
        std::stringstream ss;
        dump(ss);
        return ss.str();
    }

    /*!
     * \brief Dump this expression to a string (as a debug-level description)
     * \note This output cannot be reparsed as an expression
     */
    void dump(std::ostream& out) const;

    //! \name Expression Manipulations
    //! @{
    ////////////////////////////////////////////////////////////////////////

    /*!
     * \brief Return this expression with a node logically and-ing rhp
     */
    Expression& operator&&(const Expression& rhp) {
        Expression tmp(*this); // Copy construct from this node
        op_ = OP_AND; // Change operation
        operands_.clear();
        operands_.emplace_back(tmp); // Copy (again) from tmp;
        operands_.emplace_back(rhp); // Copy construct from rhp

        return *this;
    }

    /*!
     * \brief Return this expression with a node logically and-ing rhp
     */
    Expression& operator||(const Expression& rhp) {
        Expression tmp(*this); // Copy construct from this node
        op_ = OP_OR; // Change operation
        operands_.clear();
        operands_.emplace_back(tmp); // Copy (again) from tmp;
        operands_.emplace_back(rhp); // Copy construct from rhp

        return *this;
    }

    /*!
     * \brief Return this expression with a node logically and-ing rhp
     */
    Expression& operator!=(const Expression& rhp) {
        Expression tmp(*this); // Copy construct from this node
        op_ = OP_XOR; // Change operation
        operands_.clear();
        operands_.emplace_back(tmp); // Copy (again) from tmp;
        operands_.emplace_back(rhp); // Copy construct from rhp

        return *this;
    }

    /*!
     * \brief Return this expression with a node logically and-ing rhp
     */
    Expression& operator&=(const Expression& rhp) {
        Expression tmp(*this); // Copy construct from this node
        op_ = OP_AND; // Change operation
        operands_.clear();
        operands_.emplace_back(tmp); // Copy (again) from tmp;
        operands_.emplace_back(rhp); // Copy construct from rhp

        return *this;
    }

    /*!
     * \brief Return this expression with a node logically or-ing rhp
     */
    Expression& operator|=(const Expression& rhp) {
        Expression tmp(*this); // Copy construct from this node
        op_ = OP_OR; // Change operation
        operands_.clear();
        operands_.emplace_back(tmp); // Copy (again) from tmp;
        operands_.emplace_back(rhp); // Copy construct from rhp

        return *this;
    }

    /*!
     * \brief Return this expression with a node logically xor-ing rhp
     */
    Expression& operator^=(const Expression& rhp) {
        Expression tmp(*this); // Copy construct from this node
        op_ = OP_XOR; // Change operation
        operands_.clear();
        operands_.emplace_back(tmp); // Copy (again) from tmp;
        operands_.emplace_back(rhp); // Copy construct from rhp

        return *this;
    }

    /*!
     * \brief Return this expression with a node inverting itself
     */
    Expression& operator!() {
        Expression tmp(*this); // Copy construct from this node
        op_ = OP_NOT; // Change operation
        operands_.clear();
        operands_.emplace_back(tmp); // Copy (again) from tmp;

        return *this;
    }

    ////////////////////////////////////////////////////////////////////////
    //! @}

private:

    /*!
     * \brief Operation to perform
     */
    Operation op_ = OP_INVALID;

    /*!
     * \brief All operands for this node
     */
    std::vector<Expression> operands_;

    /*!
     * \brief Visibility expected (if op is OP_EVAL_TYPE)
     */
    InstrumentationNode::Type instrument_type_ = InstrumentationNode::NUM_TYPES;

    /*!
     * \brief Visibility expected (if op is OP_EVAL_VIS)
     */
    uint64_t visibility_ = InstrumentationNode::VIS_NORMAL;

    /*!
     * \brief Tag to compare with (if op is OP_EVAL_TAG)
     */
    std::string tag_ = "";

    /*!
     * \brief Name to compare with (if op is OP_EVAL_NAME)
     */
    std::string name_ = "";

    /*!
     * \brief Type of visibility comparison to perfom
     */
    VisibilityComparison vis_comparison_ = NUM_VISCOMPS;

    /*!
     * \brief Type of type-comparison to perform
     */
    TypeComparison type_comparison_ = NUM_TYPECOMPS;

    /*!
     * \brief Type of tag comparison to perform
     */
    TagComparison tag_comparison_ = NUM_TAGCOMPS;

    /*!
     * \brief Type of name comparison to perform
     */
    NameComparison name_comparison_ = NUM_NAMECOMPS;

}; // class Expression

/*!
 * \brief Render a representation of an expression
 */
inline std::ostream& operator<<(std::ostream& out, const Expression& e){
    e.dump(out);
    return out;
}

        } // namespace filter
    } // namespace tree
} // namespace sparta
