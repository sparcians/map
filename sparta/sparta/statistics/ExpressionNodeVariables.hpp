// <ExpressionNodeVariables> -*- C++ -*-

/*!
 * \file ExpressionNodeVariables.hpp
 * \brief This exists mainly to remove circular dependencies on StatVariable and
 * SimVariable: Expression -> StatVarible -> StatisticInstance -> Expression
 */

#pragma once

#include <memory>

#include "sparta/statistics/ExpressionNode.hpp"
#include "sparta/statistics/StatisticInstance.hpp"

namespace sparta {

class StatInstCalculator;

    namespace statistics {
        namespace expression {

struct StatVariable : public ExpressionNode
{
    /*!
     * \brief Contained statistic
     */
    sparta::StatisticInstance stat_;

    /*!
     * \brief Not default constructable
     */
    StatVariable() = delete;

    /*!
     * \brief Construct from a given Counter or Statistic
     * \param n TreeNode to use as base for this variable
     * \param used Vector of TreeNodes already used within the expression
     * containing this variable. New nodes are tested against this list in
     * order to ensure there are no cycles in expressions
     */
    StatVariable(const TreeNode* n, std::vector<const TreeNode*>& used) :
        stat_(n, used)
    {
        //std::cout << "StatVariable at " << this << " with stat " << &stat_ << std::endl;
    }

    /*!
     * \brief Construct from a given StatInstCalculator (wrapper
     * class around a SpartaHandler).
     * \param calculator User-defined callback which generates the stat value.
     * \note calculator->getNode() must return a non-null TreeNode.
     * \param used Vector of TreeNodes already used within the expression
     * containing this variable. New nodes are tested against this list in
     * order to ensure there are no cycles in expressions.
     */
    StatVariable(std::shared_ptr<StatInstCalculator> & calculator,
                 std::vector<const TreeNode*>& used) :
        stat_(calculator, used)
    { }

    StatVariable(const StatVariable& rhp) :
        ExpressionNode(),
        stat_(rhp.stat_)
    { }

    virtual ~StatVariable() {
        //std::cout << "~StatVariable at " << this << " with stat " << &stat_ << std::endl;
    }

    virtual StatVariable* clone_() const override {
        return new StatVariable(*this);
    }

    virtual double evaluate_() const override {
        return stat_.getValue();
    }

    virtual bool supportsCompression() const override {
        return stat_.supportsCompression();
    }

    /*!
     * \brief Returns the sparta StatisticInstance contained in this
     * ExpressionNode
     * Node
     * \return StatisticInstance owned by this Object. This pointer is valid for
     * as long as this node exists.
     */
    StatisticInstance* getStatisticInstance() {
        return &stat_;
    }

    virtual void start() override {
        stat_.start();
    }
    virtual void end() override {
        stat_.end();
    }

    virtual void dump(std::ostream& o,
                      bool show_range=true,
                      bool resolve_subexprs=true) const override {
        o << stat_.getExpressionString(show_range, resolve_subexprs);
    }

    virtual void getClocks(std::vector<const Clock*>& clocks) const override {
        stat_.getClocks(clocks);
    }

private:

    virtual uint32_t getStats_(std::vector<const StatisticInstance*>& results) const override {
        results.push_back(&stat_);
        return 1;
    }
}; // class StatVariable

/*!
 * \brief Expression node for a simulation variable.
 * Anything that cannot actually vary within a simulation can be handled as a
 * Constant. If it can vary, however, it should be handled through this.
 * \note SimVariables are treated as invariant for the lifetime of an expression
 */
struct SimVariable : public ExpressionNode
{
    /*!
     * \brief Typedef of getter function used to retrieve a variable
     */
    typedef double (*getter_t)();

    /*!
     * \brief Name of this variable
     */
    std::string which_;

    /*!
     * \brief Function for retrieving the value of this variable
     */
    getter_t getter_;

    SimVariable() = delete;

    SimVariable(const SimVariable& rhp) :
        ExpressionNode(),
        which_(rhp.which_),
        getter_(rhp.getter_)
    { }

    /*!
     * \brief Construct with a getter function
     * \param which Name of the variable
     * \param getter Pointer to function for getting the variable as a double
     */
    SimVariable(const std::string& which, const getter_t getter) :
        which_(which),
        getter_(getter)
    { }

    virtual SimVariable* clone_() const override {
        return new SimVariable(*this);
    }

    virtual double evaluate_() const override {
        // Look up the value
        return getter_();
    }

    //! The SimVariable is a wrapper around a function
    //! pointer which returns a double. It might as well
    //! be generating random floating-point numbers. Let's
    //! not try to compress it.
    virtual bool supportsCompression() const override {
        return false;
    }

    virtual void start() override {
        // No action on start
    }

    virtual void end() override {
        // No action on end
    }

    virtual void dump(std::ostream& o,
                      bool show_range=true,
                      bool resolve_subexprs=true) const override {
        (void) show_range;
        (void) resolve_subexprs;
        o << "{simvar " << which_ << "}";
    }

    virtual void getClocks(std::vector<const Clock*>& clocks) const override {
        (void) clocks;
        // No clocks in a SimVariable
    }

private:

    virtual uint32_t getStats_(std::vector<const StatisticInstance*>& results) const override {
        (void) results;
        // No stats used
        return 0;
    }
}; // class SimVariable

/*!
 * \brief Expression node for a reference to a double.
 * Anything that cannot actually vary can be handled as a constant through
 * Expression. If it can vary, however, it should be handled through this or
 * SimVariable (for functions)
 * \note ReferenceVariables are treated as invariant for the lifetime of an
 * expression
 */
struct ReferenceVariable : public ExpressionNode
{
     /*!
     * \brief Name of this variable
     */
    std::string which_;

    /*!
     * \brief Reference to actual variable
     */
    const double& ref_;

    ReferenceVariable() = delete;

    ReferenceVariable(const ReferenceVariable& rhp) :
        ExpressionNode(),
        which_(rhp.which_),
        ref_(rhp.ref_)
    { }

    /*!
     * \brief Construct with a value reference
     * \param which Name of the variable
     * \param ref Reference to value
     */
    template <typename T>
    ReferenceVariable(const std::string& which,
                      const T& ref) :
        which_(which),
        ref_(ref)
    {
        // C++ was allowing implicit casting from uint64_t& to double& which
        // compiles but produces incorrect results. This assertion exists to
        // catch misuses.
        static_assert(std::is_same<T,double>::value,
                      "ReferenceVariable must be constructed with a double&. Someone called this "
                      "method with a non-double argument");
    }

    /*!
     * \brief Copy-assignment Disallowed
     */
    ReferenceVariable operator=(const ReferenceVariable&) = delete;

    virtual ReferenceVariable* clone_() const override {
        return new ReferenceVariable(*this);
    }

    virtual double evaluate_() const override {
        // Read the value by refrence
        return ref_;
    }

    //! We currently are not attempting compression for
    //! ReferenceVariable's. These are not used with nearly
    //! as much frequency as counters, constants, and parameters.
    virtual bool supportsCompression() const override {
        return false;
    }

    virtual void start() override {
        // No action on start
    }

    virtual void end() override {
        // No action on end
    }

    virtual void dump(std::ostream& o,
                      bool show_range=true,
                      bool resolve_subexprs=true) const override {
        (void) show_range;
        (void) resolve_subexprs;
        o << "{" << which_ << ": " << ref_ << "}";
    }

    virtual void getClocks(std::vector<const Clock*>& clocks) const override {
        (void) clocks;
        // No clocks in a ReferenceVariable
    }

private:

    virtual uint32_t getStats_(std::vector<const StatisticInstance*>& results) const override {
        (void) results;
        // No stats used
        return 0;
    }
}; // class ReferenceVariable


        } // namespace expression
    } // namespace statistics
} // namespace sparta
