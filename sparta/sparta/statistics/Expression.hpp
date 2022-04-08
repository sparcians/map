// <Expression.hpp> -*- C++ -*-

#pragma once

#include <iostream>
#include <sstream>
#include <memory>
#include <string>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/statistics/ExpressionNode.hpp"
#include "sparta/statistics/ExpressionNodeTypes.hpp"

namespace sparta {

    class StatisticInstance;
    class Clock;

    /*!
     * \brief Namespace containing methods for computing and generating
     * statistical information using instrumentation extracted from sparta
     * structures such as Counters
     */
    namespace statistics {

        /*!
         * \brief Type for storing each stat added
         */
        typedef std::pair<std::string, StatisticInstance> stat_pair_t;

        /*!
         * \brief Namespace containing methods for parsing, building, and
         * evaluating statistical expressions in sparta
         */
        namespace expression {

/*!
 * \brief Expression container/builder. Contains a single ExpressionNode
 * representing the root of an expression tree. This is the object on which the
 * parser operates to build an expression incrementally through basic operators.
 * \note Any TreeNodes referenced by an expression should outlast it. It is not
 * safe to evaluate or even print the expression if any its references are
 * destructed because they will leave dangling pointers.
 *
 * Expressions have an inherit computation window built in (like StatisticDef).
 * This means that when the expression is created, any counter or stat-def
 * referenced is treated as if it was 0 at that point. When the expression is
 * later evaluated, the delta for those counters or stat-defs is used.
 * This can be avoided simply by evaluating with evaluateAbsolute (generally not
 * recommended)
 *
 * If expressions created during different simulation states are joined
 * together, then they may see different windows. This is intentional so that
 * comparisons can be made between different simulation time windows in a single
 * expression (most users will not need to do this). This issue can be avoided
 * by manually calling start()
 *
 */
class Expression
{
    /*!
     * \brief Content of this expression (e.g. a operation, variable, constant,
     * etc.).
     * \note if nullptr, this Expression can do nothing and cannot be evaluated
     */
    std::unique_ptr<ExpressionNode> content_;

public:

    /*!
     * \brief Constructs an expression containing no content
     */
    Expression() = default;

    /*!
     * \brief Copy Constructor
     */
    Expression(const Expression& rhp)
    {
        //std::cout << "Copying      " << this << " <- " << &rhp << std::endl;

        if(rhp.content_ != nullptr){
            content_.reset(rhp.content_->clone());
        }
    }

    /*!
     * \brief Move Constructor
     */
    Expression(Expression&& rhp) :
        content_(std::move(rhp.content_))
    {
        //std::cout << "Moving      " << this << " <- " << &rhp << std::endl;
    }

    /*!
     * \brief Construct with string expression
     * \param expr String containing an arithmetic expression
     * \param context TreeNode from which variables in the expression
     * will be searched for. Must not be nullptr
     */
    Expression(const std::string& expression,
               TreeNode* context);

    /*!
     * \brief Construct with string expression
     * \param expr String containing an arithmetic expression
     * \param context TreeNode from which variables in the expression
     * \param report_si Previously defined StatisticInstances in the report
     */
    Expression(const std::string& expression,
               TreeNode* context,
               const std::vector<stat_pair_t>&report_si);

    /*!
     * \brief Construct with string expression
     * \param expr String containing an arithmetic expression
     * \param context Optional TreeNode from which variables in the expression
     * will be searched for. Must not be nullptr
     * \param already_used TreeNodes already in an expression containing this
     * expression
     */
    Expression(const std::string& expression,
               TreeNode* context,
               std::vector<const TreeNode*>& already_used);

    /*!
     * \brief Blind content constructor
     * \param item Expression item to contain. Takes ownership
     */
    Expression(ExpressionNode* item) :
        Expression()
    {
        sparta_assert(item != nullptr);
        content_.reset(item);
    }

    /*!
     * \brief Constant construction
     */
    Expression(double d);

    /*!
     * \brief Operation construction
     */
    Expression(operation_t type,
               ExpressionNode * op1,
               ExpressionNode * op2=nullptr,
               ExpressionNode * op3=nullptr);

    /*!
     * \brief Stat/Counter/Parameter construction
     * \param[in] n Node to use (StatisticDef, Parameter, or Counter variant)
     * \param[in] used Vector of nodes already used higher up in an enclosing
     *            expression (for preventing cycles). Use a dummy object if
     *            there is no chance of cyclic expressions
     */
    Expression(const TreeNode* n,
               std::vector<const TreeNode*>& used);

    /*!
     * \brief Unary function construction
     * \tparam RetT Return type of function pointer. Must be convertable to
     * double
     * \tparam ArgT Argument type of function pointer.
     */
    template<typename RetT, typename ArgT>
    Expression(const std::string& name,
               RetT(*fxn)(ArgT),
               const Expression& a);

    /*!
     * \brief Unary function construction
     * \tparam RetT Return type of functor. Must be convertable to
     * double
     * \tparam ArgT Argument type of functor.
     */
    template<typename RetT, typename ArgT>
    Expression(const std::string& name,
               std::function<RetT (ArgT)>&,
               const Expression& a);

    /*!
     * \brief Binary function construction
     * \tparam RetT Return type of function pointer. Must be convertable to
     * double
     * \tparam ArgT Argument type of function pointer.
     */
    template<typename RetT, typename ArgT>
    Expression(const std::string& name,
               RetT(*fxn)(ArgT, ArgT),
               const Expression& a,
               const Expression& b);

    /*!
     * \brief Binary function construction
     * \tparam RetT Return type of functor. Must be convertable to
     * double
     * \tparam ArgT Argument type of functor.
     */
    template<typename RetT>
    Expression(const std::string& name,
               const RetT& functor,
               const Expression& a,
               const Expression& b);

    /*!
     * \brief Ternary function construction
     * \tparam RetT Return type of function pointer. Must be convertable to
     * double
     * \tparam ArgT Argument type of function pointer.
     */
    template<typename RetT, typename ArgT>
    Expression(const std::string& name,
               RetT(*fxn)(ArgT, ArgT, ArgT),
               const Expression& a,
               const Expression& b,
               const Expression& c);

    /*!
     * \brief Virtual destructor
     */
    virtual ~Expression();

    /*!
     * \brief Assignment operator. Clones the content of rhp and discards
     * current expression content
     * \param rhp Expression to copy content from
     */
    Expression& operator=(const Expression& rhp)
    {
        //std::cout << "Assignment   " << this << " <- " << &rhp << std::endl;

        if(rhp.content_ != nullptr){
            content_.reset(rhp.content_->clone());
        }

        return *this;
    }

    /*!
     * \brief Assignment operator with move. Moves content of rhp and discards
     * current expression content
     * \param rhp Expression to move content from
     */
    Expression& operator=(Expression&& rhp)
    {
        //std::cout << "Move   " << this << " <- " << &rhp << std::endl;

        content_ = std::move(rhp.content_);
        return *this;
    }

    /*!
     * \brief Makes a clone of the content of this expression.
     * \throw SpartaException if this node has null content
     */
    ExpressionNode* cloneContent() const {
        if(content_ == nullptr){
            throw SpartaException("Cannot clone content of an expression with null content");
        }
        return content_->clone();
    }

    /*!
     * \brief Gets the statistics present in this expression
     * \return Number of stats added to results
     * \param results Vector of pointers to StatisticInstances. All statistics
     * within this class will be appended to the results vector.
     * These pointers are valid until this expression is modified or deleted
     */
    uint32_t getStats(std::vector<const StatisticInstance*>& results) const {
        return getStats_(results);
    }

    /*!
     * \brief Does this expression have content.
     * \note This does not necessarily imply that the expression will be
     * successfully evaluated
     */
    bool hasContent() const {
        return content_ != nullptr;
    }

    /*!
     * \brief Construct a unary function having the given name and function
     * \tparam RetT Return type of function pointer. Must be convertable to
     * double
     * \tparam ArgT Argument type of function pointer.
     * \param name Name of the function (e.g. fabs)
     * \param fxn Function pointer to invoke to compute the value. Must not be
     * nullptr
     * \param a Operand of the unary function
     */
    template<typename RetT, typename ArgT>
    Expression ufunc(const std::string& name,
                     RetT(*fxn)(ArgT),
                     const Expression& a) const;

    /*!
     * \brief Construct a unary function having the given name and functor
     * \tparam RetT Return type of functor. Must be convertable to
     * double
     * \tparam ArgT Argument type of functor.
     * \param name Name of the function (e.g. fabs)
     * \param fxn Functor to invoke to compute the value. Must not be
     * null
     * \param a Operand of the unary function
     */
    template<typename RetT, typename ArgT>
    Expression ufunc(const std::string& name,
                     std::function<RetT (ArgT)>&,
                     const Expression& a) const;

    /*!
     * \brief Construct a binary function having the given name and function
     * \tparam RetT Return type of function pointer. Must be convertable to
     * double
     * \tparam ArgT Argument type of function pointer.
     * \param name Name of the function (e.g. fabs)
     * \param fxn Function pointer to invoke to compute the value. Must not be
     * nullptr
     * \param a Operand 1 of the binary function
     * \param b Operand 2 of the binary function
     */
    template<typename RetT, typename ArgT>
    Expression bfunc(const std::string& name,
                     RetT(*fxn)(ArgT, ArgT),
                     const Expression& a,
                     const Expression& b) const;

    /*!
     * \brief Construct a binary function having the given name and functor
     * \tparam RetT Return type of functor. Must be convertable to
     * double
     * \tparam ArgT Argument type of functor.
     * \param name Name of the function (e.g. fabs)
     * \param fxn Functor to invoke to compute the value. Must not be
     * null
     * \param a Operand 1 of the binary function
     * \param b Operand 2 of the binary function
     */
    template<typename RetT>
    Expression bfunc(const std::string& name,
                     const RetT& functor,
                     const Expression& a,
                     const Expression& b) const;

    /*!
     * \brief Construct a ternary function having the given name and function
     * \tparam RetT Return type of function pointer. Must be convertable to
     * double
     * \tparam ArgT Argument type of function pointer.
     * \param name Name of the function (e.g. fabs)
     * \param fxn Function pointer to invoke to compute the value. Must not be
     * nullptr
     * \param a Operand 1 of the binary function
     * \param b Operand 2 of the binary function
     * \param c Operand 3 of the binary function
     */
    template<typename RetT, typename ArgT>
    Expression tfunc(const std::string& name,
                     RetT(*fxn)(ArgT, ArgT, ArgT),
                     const Expression& a,
                     const Expression& b,
                     const Expression& c) const;

    /*!
     * \brief Construct a constant node
     * \note that this is a non-const operation
     * \return *this
     */
    Expression& operator=(double d);

    /*!
     * \brief Return this expression with no effect
     */
    Expression operator+() const;

    /*!
     * \brief Return this expression with a negation inserted
     */
    Expression operator-() const;

    /*!
     * \brief Return a new expression of: *this + rhp
     */
    Expression operator+(const Expression& rhp) const;

    /*!
     * \brief Return a new expression of: *this - rhp
     */
    Expression operator-(const Expression& rhp) const;

    /*!
     * \brief Return a new expression of: *this * rhp
     */
    Expression operator*(const Expression& rhp) const;

    /*!
     * \brief Return a new expression of: *this / rhp
     */
    Expression operator/(const Expression& rhp) const;

    /*!
     * \brief Return this expression with a node adding rhp
     */
    Expression& operator+=(const Expression& rhp);

    /*!
     * \brief Return this expression with a node subtracting rhp
     */
    Expression& operator-=(const Expression& rhp);

    /*!
     * \brief Return this expression with a node multiplying by rhp
     */
    Expression& operator*=(const Expression& rhp);

    /*!
     * \brief Return this expression with a node dividing by rhp
     */
    Expression& operator/=(const Expression& rhp);

    /*!
     * \brief Compute value of this operate in simulation for the current
     * computation window
     */
    double evaluate() const {
        if(content_ == nullptr){
            throw SpartaException("Cannot evaluate expression because it has no content. Test with "
                                "hasContent before blindly evaluating foreign expressions");
        }

        return content_->evaluate();
    };

    /*!
     * \brief Notify every item in this expression to start a new computation
     * window
     * \note Has no effect if this expression has no content
     * \see sparta::StatisticInstance
     * \see hasContent
     */
    void start() {
        if(content_){
            content_->start();
        }
    }

    /*!
     * \brief Notify every item in this expression to end the current
     * computation window
     * \note Has no effect if this expression has no content
     * \see sparta::StatisticInstance
     * \see hasContent
     */
    void end() {
        if(content_){
            content_->end();
        }
    }

    /*!
     * \brief Write the content of this entire expression to an ostream
     * \note Does not evaluate the expression
     * \param o Ostream to write to
     * \param show_range Should the range be shown in any subexpression
     * nodes.
     * \param resolve_subexprs Should any referenced statistic defs be
     * expanded to their full expressions so that this becomes an expression
     * containing only counters.
     */
    void dump(std::ostream& o,
              bool show_range=true,
              bool resolve_subexprs=true) const {
        if(!content_){
            o << "???";
        }else{
            content_->dump(o, show_range, resolve_subexprs);
        }
    }

    bool supportsCompression() const {
        if (content_ == nullptr) {
            return false;
        }
        return content_->supportsCompression();
    }

    /*!
     * \brief Return a string representing this expression including any
     * TreeNode dependencies.
     * \warning It is not safe to call this if the expression depends on any
     * destructed TreeNodes.
     * \param show_range See dump
     * \param resolve_subexprs See dump
     * \return String representation of this expression
     */
    std::string stringize(bool show_range=true,
                          bool resolve_subexprs=true) const {
        std::stringstream ss;
        dump(ss, show_range, resolve_subexprs);
        return ss.str();
    }

    /*!
     * \brief Gets the clock associated with the content of this expression.
     * This is done by finding all TreeNodes on which the Expression depends
     * \return The clock if at least one TreeNode is found in this expression
     * and all found TreeNodes have the same clock. Returns nullptr if this
     * Expression contains no TreeNodes or none of those nodes have associated
     * Clocks.
     */
    const Clock* getClock() {
        std::vector<const Clock*> clocks;
        getClocks(clocks);
        if(clocks.size() == 0){
            return nullptr;
        }
        const Clock* result = *(clocks.begin());
        for(auto itr = clocks.begin()+1; itr != clocks.end(); ++itr){
            if(*itr != result){
                throw SpartaException("Multiple TreeNodes found with different clocks when "
                                    "attempting to determine the clock associated with the "
                                    "expression: ") << stringize();
            }
        }
        return result;
    }

    /*!
     * \brief Gets all clocks associated with this Expression.
     * \param clocks Vector of clocks to which all clocks contained in this
     * Expression will be added. Does not clear this vector.
     */
    void getClocks(std::vector<const Clock*>& clocks) const{
        if(content_){
            content_->getClocks(clocks);
        }
    }

private:

    /*!
     * \brief Implements getStats
     */
    uint32_t getStats_(std::vector<const StatisticInstance*>& results) const {
        return content_ ? content_->getStats(results) : 0;
    }

    /*!
     * \brief Parse an expression to generate the internal representation
     * \note To be used ONLY by the constructors of this class
     * \post sets content_
     * \throw SpartaException if expression cannot be evaluated and resolved
     */
    void parse_(const std::string& expression,
                TreeNode* context,
                std::vector<const TreeNode*>& already_used,
                const std::vector<stat_pair_t>&report_si);
};

inline Expression::Expression(double d) :
    Expression()
{
    content_.reset(new Constant(d));
}

inline Expression::Expression(operation_t type,
                              ExpressionNode * op1,
                              ExpressionNode * op2,
                              ExpressionNode * op3) :
    Expression()
{
    content_.reset(new Operation(type, op1, op2, op3));
}

template <typename RetT, typename ArgT>
inline Expression::Expression(const std::string& name,
                              RetT(*fxn)(ArgT),
                              const Expression& a) {
    sparta_assert(fxn != nullptr,
                      "function pointer of unary function \"" << fxn << "\" must not be nullptr");
    content_.reset(new UnaryFunction<RetT, ArgT>(name, fxn, a.cloneContent()));
}

template <typename RetT, typename ArgT>
inline Expression::Expression(const std::string& name,
                              std::function<RetT (ArgT)>& fxn,
                              const Expression& a) {
    sparta_assert(fxn,
        "function pointer of unary function \"" << "\" must not be nullptr");
    content_.reset(new UnaryFunction<RetT, ArgT, std::function<RetT (ArgT)>>(name, fxn, a.cloneContent()));
}

template <typename RetT, typename ArgT>
inline Expression::Expression(const std::string& name,
                              RetT(*fxn)(ArgT, ArgT),
                              const Expression& a,
                              const Expression& b) {
    sparta_assert(fxn != nullptr,
                      "function pointer of binary function \"" << fxn << "\" must not be nullptr");
    content_.reset(new BinaryFunction<RetT, ArgT>(name, fxn, a.cloneContent(), b.cloneContent()));
}

template <typename RetT>
inline Expression::Expression(const std::string& name,
                              const RetT& functor,
                              const Expression& a,
                              const Expression& b) {
    content_.reset(new BinaryFunction<RetT>(name, functor, a.cloneContent(), b.cloneContent()));
}

template <typename RetT, typename ArgT>
inline Expression::Expression(const std::string& name,
                              RetT(*fxn)(ArgT, ArgT, ArgT),
                              const Expression& a,
                              const Expression& b,
                              const Expression& c) {
    sparta_assert(fxn != nullptr,
                      "function pointer of ternary function \"" << fxn << "\" must not be nullptr");
    content_.reset(new TernaryFunction<RetT, ArgT>(name, fxn, a.cloneContent(), b.cloneContent(), c.cloneContent()));
}

template <typename RetT, typename ArgT>
inline Expression Expression::ufunc(const std::string& name,
                                    RetT(*fxn)(ArgT),
                                    const Expression& a) const {
    sparta_assert(fxn != nullptr,
                      "function pointer of unary function \"" << fxn << "\" must not be nullptr");
    //std::cout << "unary func \"" << name << "\"" << std::endl;
    return Expression(new UnaryFunction<RetT, ArgT>(name, fxn, a.cloneContent()));
}

template <typename RetT, typename ArgT>
inline Expression Expression::ufunc(const std::string& name,
                                    std::function<RetT (ArgT)>& fxn,
                                    const Expression& a) const {
    sparta_assert(fxn,
        "function pointer of unary function \"" << "\" must not be nullptr");
    return Expression(new UnaryFunction<RetT, ArgT, std::function<RetT (ArgT)>>(name, fxn, a.cloneContent()));
}

template <typename RetT, typename ArgT>
inline Expression Expression::bfunc(const std::string& name,
                                    RetT(*fxn)(ArgT, ArgT),
                                    const Expression& a,
                                    const Expression& b) const {
    sparta_assert(fxn != nullptr,
                      "function pointer of binary function \"" << fxn << "\" must not be nullptr");
    return Expression(new BinaryFunction<RetT, ArgT>(name, fxn, a.cloneContent(), b.cloneContent()));
}

template <typename RetT>
inline Expression Expression::bfunc(const std::string& name,
                                    const RetT& functor,
                                    const Expression& a,
                                    const Expression& b) const {
    return Expression(new BinaryFunction<RetT>(name, functor, a.cloneContent(), b.cloneContent()));
}

template <typename RetT, typename ArgT>
inline Expression Expression::tfunc(const std::string& name,
                                    RetT(*fxn)(ArgT, ArgT, ArgT),
                                    const Expression& a,
                                    const Expression& b,
                                    const Expression& c) const {
    sparta_assert(fxn != nullptr,
                      "function pointer of ternary function \"" << fxn << "\" must not be nullptr");
    return Expression(new TernaryFunction<RetT, ArgT>(name, fxn, a.cloneContent(), b.cloneContent(), c.cloneContent()));
}

inline Expression& Expression::operator=(double d)
{
    sparta_assert(content_ == nullptr, "Cannot call operator= on an expression which already has an item. Item would ""be discarded");
    content_.reset(new Constant(d));
    return *this;
}

inline Expression Expression::operator+() const
{
    sparta_assert(content_ != nullptr, "Cannot call operator+() on an expression which has no item. A lhp is ""required in order to promote");

    return Expression(new Operation(OP_PROMOTE,
                                    cloneContent()));
}

inline Expression Expression::operator-() const
{
    sparta_assert(content_ != nullptr, "Cannot call operator-() on an expression which has no item. A lhp is ""required in order to negate");

    return Expression(new Operation(OP_NEGATE,
                                    cloneContent()));
}

inline Expression Expression::operator+(const Expression& rhp) const
{
    sparta_assert(content_ != nullptr, "Cannot call operator+(rhp) on an expression which has no item. A lhp is ""required in order to add");

    return Expression(new Operation(OP_ADD,            // Operation
                                    cloneContent(),    // Takes ownership of content
                                    rhp.cloneContent() // Clones content
                                   ));
}

inline Expression Expression::operator-(const Expression& rhp) const
{
    sparta_assert(content_ != nullptr, "Cannot call operator-(rhp) on an expression which has no item. A lhp is ""required in order to subtract");

    return Expression(new Operation(OP_SUB,            // Operation
                                    cloneContent(),    // Takes ownership of content
                                    rhp.cloneContent() // Clones content
                                   ));
}

inline Expression Expression::operator*(const Expression& rhp) const
{
    sparta_assert(content_ != nullptr, "Cannot call operator*(rhp) on an expression which has no item. A lhp is ""required in order to multiply");

    return Expression(new Operation(OP_MUL,            // Operation
                                    cloneContent(),    // Takes ownership of content
                                    rhp.cloneContent() // Clones content
                                   ));
}

inline Expression Expression::operator/(const Expression& rhp) const
{
    sparta_assert(content_ != nullptr, "Cannot call operator/(rhp) on an expression which has no item. A lhp is ""required in order to divide");

    return Expression(new Operation(OP_DIV,            // Operation
                                    cloneContent(),    // Takes ownership of content
                                    rhp.cloneContent() // Clones content
                                   ));
}

inline Expression& Expression::operator+=(const Expression& rhp)
{
    sparta_assert(content_ != nullptr, "Cannot call operator+=(rhp) on an expression which has no item. A lhp is ""required in order to add");

    content_.reset(new Operation(OP_ADD,            // Operation
                                 cloneContent(),    // Takes ownership of content
                                 rhp.cloneContent() // Clones content
                                ));
    return *this;
}

inline Expression& Expression::operator-=(const Expression& rhp)
{
    sparta_assert(content_ != nullptr, "Cannot call operator-=(rhp) on an expression which has no item. A lhp is ""required in order to subtract");

    content_.reset(new Operation(OP_SUB,            // Operation
                                 cloneContent(),    // Takes ownership of content
                                 rhp.cloneContent() // Clones content
                                ));
    return *this;
}


inline Expression& Expression::operator*=(const Expression& rhp)
{
    sparta_assert(content_ != nullptr, "Cannot call operator*=(rhp) on an expression which has no item. A lhp is ""required in order to divide");

    content_.reset(new Operation(OP_MUL,            // Operation
                                 cloneContent(),    // Takes ownership of content
                                 rhp.cloneContent() // Clones content
                                ));
    return *this;
}

inline Expression& Expression::operator/=(const Expression& rhp)
{
    sparta_assert(content_ != nullptr, "Cannot call operator/=(rhp) on an expression which has no item. A lhp is ""required in order to divide");

    content_.reset(new Operation(OP_DIV,            // Operation
                                 cloneContent(),    // Takes ownership of content
                                 rhp.cloneContent() // Clones content
                                ));
    return *this;
}

/*!
 * \brief Ostream printing function for Expressions
 */
inline std::ostream& operator<<(std::ostream& o, const sparta::statistics::expression::Expression& ex){
    ex.dump(o);
    return o;
}

        } // namespace expression
    } // namespace statistics
} // namespace sparta
