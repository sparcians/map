// <ExpressionNodeTypes> -*- C++ -*-

#pragma once

#include <vector>
#include <memory>

#include "sparta/statistics/ExpressionNode.hpp"

namespace sparta {

    class StatisticInstance;

    namespace statistics {
        namespace expression {

/*!
 * \brief Operation Node (e.g. +,-,*,/)
 */
struct Operation : public ExpressionNode
{
    /*!
     * \brief Type of operation to perform
     */
    operation_t type_;

    /*!
     * \brief All operands (subexpressions) in this node
     */
    std::vector<std::unique_ptr<ExpressionNode>> operands_;

    /*!
     * \brief Default constructor with null operations and no type
     */
    Operation()
        : type_(OP_NULL)
    { }

    /*!
     * \brief Copy constructor. Deep copies the operands of the operation
     */
    Operation(const Operation& rhp) :
        ExpressionNode(),
        type_(rhp.type_)
    {
        for(auto& item : rhp.operands_){
            ExpressionNode* clone = item->clone();
            operands_.emplace_back(clone);
        }
    }

    /*!
     * \brief Contruct with operation type and operands
     * \param type Type of operation
     * \param op1 Operand 1 (required). Must not be nullptr
     * \param op2 Optional operand 2
     * \param op3 Optional operand 3
     */
    Operation(operation_t type,
              ExpressionNode * op1,
              ExpressionNode * op2=nullptr,
              ExpressionNode * op3=nullptr) :
        type_(type)
    {
        sparta_assert(op1 != nullptr);
        operands_.emplace_back(op1);

        if(op2 != nullptr){
            operands_.emplace_back(op2);
        }
        if(op3 != nullptr){
            operands_.emplace_back(op3);
        }
    }

    virtual ~Operation()
    { }

    virtual Operation* clone_() const override {
        return new Operation(*this);
    }

    /*!
     * \brief Manually add an operand
     */
    void addOperand(ExpressionNode* op){
        operands_.emplace_back(op);
    }

    virtual double evaluate_() const override {
        switch(type_){
        case OP_NULL:
            throw SpartaException("Cannot compute value of a NULL op");
        case OP_ADD:
            return operands_.at(0)->evaluate() + operands_.at(1)->evaluate();
        case OP_SUB:
            return operands_.at(0)->evaluate() - operands_.at(1)->evaluate();
        case OP_MUL:
            return operands_.at(0)->evaluate() * operands_.at(1)->evaluate();
        case OP_DIV:
            return operands_.at(0)->evaluate() / operands_.at(1)->evaluate();
        case OP_NEGATE:
            return -(operands_.at(0)->evaluate());
        case OP_PROMOTE:
            return +(operands_.at(0)->evaluate());
        case OP_FORWARD:
            return operands_.at(0)->evaluate();
        default:
            throw SpartaException("Unknown operation type: ") << type_;
        }
    }

    //! Every SI needs to make an estimation (ahead of simulation)
    //! whether it's a good candidate for compression or not. There
    //! are some obvious good choices such as integral counters and
    //! constants. As for generic StatisticDef expressions, we make
    //! an estimation that we *are* a good candidate for compression
    //! if all of our operands say that they support compression, and
    //! this SI expression does NOT have a divide anywhere in it.
    //!
    //!                                               Compress it?
    //! -------------------------------------        ---------------
    //!   "counterA + counterB"                                yes
    //!
    //!   "counterA * counterB"                                yes
    //!
    //!   "statdefA - statdefB"                         depends on
    //!                                                 what those
    //!                                              statdef's are
    //!                                                going to do
    //!
    //!   "counterA / counterB"                                 NO
    //!
    //! The rational for not trying to compress an SI with a divide
    //! in its expression is that double-precision values tend to
    //! compress less than integral values do. The zlib compression
    //! library supports a small variety of compression algorithms
    //! however, and we should try them all. If RLE is used, doubles
    //! may not be worth the performance hit. But this is under design /
    //! up in the air at the moment, and is implementation detail to
    //! the outside world either way.
    virtual bool supportsCompression() const override {
        switch (type_) {
            case OP_NULL:
                return false;
            case OP_ADD:
            case OP_SUB:
            case OP_MUL: {
                bool supports_compression = true;
                for (const auto & op : operands_) {
                    supports_compression &= op->supportsCompression();
                }
                return supports_compression;
            }
            case OP_DIV:
                return false;
            case OP_NEGATE:
            case OP_PROMOTE:
            case OP_FORWARD:
                return operands_.at(0)->supportsCompression();
            default:
                throw SpartaException("Unknown operation type: ") << type_;
        }

        sparta_assert(!"Unreachable");
        return false;
    }

    virtual void start() override {
        for(auto& op : operands_){
            op->start();
        }
    }
    virtual void end() override {
        for(auto& op : operands_){
            op->end();
        }
    }

    virtual void dump(std::ostream& o,
                      bool show_range=true,
                      bool resolve_subexprs=true) const override {

        if(operands_.size() == 2){
            o << "(";
            operands_.at(0)->dump(o, show_range, resolve_subexprs);
            o << "" << (char)(type_ & 0xff);
            operands_.at(1)->dump(o, show_range, resolve_subexprs);
            o << ')';
        }else{
            if((uint32_t)type_ > 255){
                o << "op" << type_ << "(" ;
            }else{
                o << "op" << (char)(type_ & 0xff) << "(" ;
            }

            uint32_t idx = 0;
            for(auto& op : operands_){
                if(idx != 0){
                    o << ", ";
                }
                op->dump(o, show_range, resolve_subexprs);
                ++idx;
            }
            o << ')';
        }
    }

    virtual void getClocks(std::vector<const Clock*>& clocks) const override {
        for(auto& op : operands_){
            op->getClocks(clocks);
        }
    }

private:

    virtual uint32_t getStats_(std::vector<const StatisticInstance*>& results) const override {
        uint32_t added = 0;
        for(auto& op : operands_){
            added += op->getStats(results);
        }
        return added;
    }
}; // class Operation

struct Constant : public ExpressionNode
{
    double value_;

    Constant(const Constant& rhp) :
        ExpressionNode(),
        value_(rhp.value_)
    { }

    Constant(double val) :
        value_(val)
    {
        //std::cout << "Constant at " << this << std::endl;
    }

    virtual Constant* clone_() const override {
        return new Constant(*this);
    }

    virtual double evaluate_() const override {
        return value_;
    }

    //! Constants are always good candidates for compression
    virtual bool supportsCompression() const override {
        return true;
    }

    virtual void start() override {
    }

    virtual void end() override {
    }

    virtual void dump(std::ostream& o,
                      bool show_range=true,
                      bool resolve_subexprs=true) const override {
        (void) show_range;
        (void) resolve_subexprs;
        o << value_;
    }

    virtual void getClocks(std::vector<const Clock*>& clocks) const override {
        (void) clocks;
        // No clocks in a constant
    }

private:

    virtual uint32_t getStats_(std::vector<const StatisticInstance*>& results) const override {
        (void) results;
        return 0;
    }
};

/*!
 * \brief Represents a Unary function node
 * \tparam RetT Return type of function pointer. Must be convertable to double
 */
template <typename RetT=double, typename ArgT=double, typename fxn_t = RetT (* const)(ArgT)>
struct UnaryFunction : public ExpressionNode
{
    /*!
     * \brief Name of this unary function
     */
    const std::string name_;

    /*!
     * \brief Unary function to invoke
     */
    const fxn_t fxn_;

    /*!
     * \brief Operand of the unary function fxn_
     */
    std::unique_ptr<ExpressionNode> operand_;


    /*!
     * \brief No default construction
     */
    UnaryFunction() = delete;

    UnaryFunction(const UnaryFunction& rhp) :
        ExpressionNode(),
        name_(rhp.name_),
        fxn_(rhp.fxn_)
    {
        sparta_assert(rhp.operand_ != nullptr); // Should not be possible
        operand_.reset(rhp.operand_->clone());
    }

    /*!
     * brief Construct a new unary function
     * \param Name of function
     * \param unary double function pointer
     * \param op Operand. Must not be nullptr. Takes ownership
     */
    UnaryFunction(const std::string& name,
                  fxn_t fxn,
                  ExpressionNode* op) :
        name_(name),
        fxn_(fxn),
        operand_(op)
    {
        sparta_assert(operand_ != nullptr,
                          "operand of unary function \"" << name << "\" cannot be nullptr");
    }

    /*!
     * \brief Non-assignable
     */
    UnaryFunction& operator=(const UnaryFunction&) = delete;

    virtual UnaryFunction* clone_() const override {
        return new UnaryFunction(*this);
    }

    virtual double evaluate_() const override {
        return (double)fxn_(operand_->evaluate());
    }

    //! We currently are not attempting compression for UnaryFunction,
    //! BinaryFunction, and TernaryFunction SI's. These are not used
    //! with nearly as much frequency as counters, constants, and
    //! parameters.
    virtual bool supportsCompression() const override {
        return false;
    }

    virtual void start() override {
        operand_->start();
    }

    virtual void end() override {
        operand_->end();
    }

    virtual void dump(std::ostream& o,
                      bool show_range=true,
                      bool resolve_subexprs=true) const override {
        o << name_ << "(";
        operand_->dump(o, show_range, resolve_subexprs);
        o << ")";
    }

    virtual void getClocks(std::vector<const Clock*>& clocks) const override {
        operand_->getClocks(clocks);
    }

private:

    virtual uint32_t getStats_(std::vector<const StatisticInstance*>& results) const override {
        return operand_->getStats(results);
    }

}; // class UnaryFunction

/*!
 * \brief Represents a Binary function node
 * \tparam RetT Return type of function pointer. Must be convertable to double
 * \tparam ArgT Argument type of function pointer. Must be convertable from
 * a lvalue double
 */
template <typename RetT=double, typename ArgT=double>
struct BinaryFunction : public ExpressionNode
{
    /*!
     * \brief Function evaluation handler type
     */
    using fxn_t = typename std::conditional<
        std::is_class<RetT>::value,
            RetT,
                RetT(* const)(ArgT, ArgT)>::type;

    /*!
     * \brief Name of this binary function
     */
    const std::string name_;

    /*!
     * \brief Binary function to invoke
     */
    const fxn_t fxn_;

    /*!
     * \brief Operand 1 of the unary function fxn_
     */
    std::unique_ptr<ExpressionNode> operand_1_;

    /*!
     * \brief Operand 2 of the unary function fxn_
     */
    std::unique_ptr<ExpressionNode> operand_2_;


    /*!
     * \brief No default construction
     */
    BinaryFunction() = delete;

    BinaryFunction(const BinaryFunction& rhp) :
        ExpressionNode(),
        name_(rhp.name_),
        fxn_(rhp.fxn_)
    {
        sparta_assert(rhp.operand_1_ != nullptr); // Should not be possible
        sparta_assert(rhp.operand_2_ != nullptr); // Should not be possible
        operand_1_.reset(rhp.operand_1_->clone());
        operand_2_.reset(rhp.operand_2_->clone());
    }

    /*!
     * brief Construct a new binary function
     * \param Name of function
     * \param unary double function pointer
     * \param op1 Operand 1. Must not be nullptr. Takes ownership
     * \param op2 Operand 1. Must not be nullptr. Takes ownership
     */
    BinaryFunction(const std::string& name,
                  fxn_t fxn,
                  ExpressionNode* op1,
                  ExpressionNode* op2) :
        name_(name),
        fxn_(fxn),
        operand_1_(op1),
        operand_2_(op2)
    {
        sparta_assert(operand_1_ != nullptr,
                          "operand 1 of binary function \"" << name << "\" cannot be nullptr");
        sparta_assert(operand_2_ != nullptr,
                          "operand 2 of binary function \"" << name << "\" cannot be nullptr");
    }

    /*!
     * \brief Non-assignable
     */
    BinaryFunction& operator=(const BinaryFunction&) = delete;

    virtual BinaryFunction* clone_() const override {
        return new BinaryFunction(*this);
    }

    virtual double evaluate_() const override {
        auto x = operand_1_->evaluate();
        auto y = operand_2_->evaluate();
        return (double)fxn_(x, y);
    }

    //! We currently are not attempting compression for UnaryFunction,
    //! BinaryFunction, and TernaryFunction SI's. These are not used
    //! with nearly as much frequency as counters, constants, and
    //! parameters.
    virtual bool supportsCompression() const override {
        return false;
    }

    virtual void start() override {
        operand_1_->start();
        operand_2_->start();
    }

    virtual void end() override {
        operand_1_->end();
        operand_2_->end();
    }

    virtual void dump(std::ostream& o,
                      bool show_range=true,
                      bool resolve_subexprs=true) const override {
        o << name_ << "(";
        operand_1_->dump(o, show_range, resolve_subexprs);
        o << ", ";
        operand_2_->dump(o, show_range, resolve_subexprs);
        o << ")";
    }

    virtual void getClocks(std::vector<const Clock*>& clocks) const override {
        operand_1_->getClocks(clocks);
        operand_2_->getClocks(clocks);
    }

private:

    virtual uint32_t getStats_(std::vector<const StatisticInstance*>& results) const override {
        return operand_1_->getStats(results) + operand_2_->getStats(results);
    }

}; // class BinaryFunction

/*!
 * \brief Represents a Ternary function node
 * \tparam RetT Return type of function pointer. Must be convertable to double
 * \tparam ArgT Argument type of function pointer. Must be convertable from
 * a lvalue double
 */
template <typename RetT=double, typename ArgT=double>
struct TernaryFunction : public ExpressionNode
{
    /*!
     * \brief Function evaluation handler type
     */
    typedef RetT(* const fxn_t)(ArgT, ArgT, ArgT);

    /*!
     * \brief Name of this ternary function
     */
    const std::string name_;

    /*!
     * \brief Ternary function to invoke
     */
    const fxn_t fxn_;

    /*!
     * \brief Operand 1 of the unary function fxn_
     */
    std::unique_ptr<ExpressionNode> operand_1_;

    /*!
     * \brief Operand 2 of the unary function fxn_
     */
    std::unique_ptr<ExpressionNode> operand_2_;

        /*!
     * \brief Operand 2 of the unary function fxn_
     */
    std::unique_ptr<ExpressionNode> operand_3_;


    /*!
     * \brief No default construction
     */
    TernaryFunction() = delete;

    TernaryFunction(const TernaryFunction& rhp) :
        ExpressionNode(),
        name_(rhp.name_),
        fxn_(rhp.fxn_)
    {
        sparta_assert(rhp.operand_1_ != nullptr); // Should not be possible
        sparta_assert(rhp.operand_2_ != nullptr); // Should not be possible
        sparta_assert(rhp.operand_3_ != nullptr); // Should not be possible
        operand_1_.reset(rhp.operand_1_->clone());
        operand_2_.reset(rhp.operand_2_->clone());
        operand_3_.reset(rhp.operand_3_->clone());
    }

    /*!
     * brief Construct a new ternary function
     * \param Name of function
     * \param unary double function pointer
     * \param op1 Operand 1. Must not be nullptr. Takes ownership
     * \param op2 Operand 1. Must not be nullptr. Takes ownership
     */
    TernaryFunction(const std::string& name,
                  fxn_t fxn,
                  ExpressionNode* op1,
                  ExpressionNode* op2,
                  ExpressionNode* op3) :
        name_(name),
        fxn_(fxn),
        operand_1_(op1),
        operand_2_(op2),
        operand_3_(op3)
    {
        sparta_assert(operand_1_ != nullptr,
                          "operand 1 of ternary function \"" << name << "\" cannot be nullptr");
        sparta_assert(operand_2_ != nullptr,
                          "operand 2 of ternary function \"" << name << "\" cannot be nullptr");
        sparta_assert(operand_3_ != nullptr,
                          "operand 3 of ternary function \"" << name << "\" cannot be nullptr");
    }

    /*!
     * \brief Non-assignable
     */
    TernaryFunction& operator=(const TernaryFunction&) = delete;

    virtual TernaryFunction* clone_() const override {
        return new TernaryFunction(*this);
    }

    virtual double evaluate_() const override {
        return (double)fxn_(operand_1_->evaluate(), operand_2_->evaluate(), operand_3_->evaluate());
    }

    //! We currently are not attempting compression for UnaryFunction,
    //! BinaryFunction, and TernaryFunction SI's. These are not used
    //! with nearly as much frequency as counters, constants, and
    //! parameters.
    virtual bool supportsCompression() const override {
        return false;
    }

    virtual void start() override {
        operand_1_->start();
        operand_2_->start();
        operand_3_->start();
    }

    virtual void end() override {
        operand_1_->end();
        operand_2_->end();
        operand_3_->end();
    }

    virtual void dump(std::ostream& o,
                      bool show_range=true,
                      bool resolve_subexprs=true) const override {
        o << name_ << "(";
        operand_1_->dump(o, show_range, resolve_subexprs);
        o << ", ";
        operand_2_->dump(o, show_range, resolve_subexprs);
        o << ", ";
        operand_3_->dump(o, show_range, resolve_subexprs);
        o << ")";
    }

    virtual void getClocks(std::vector<const Clock*>& clocks) const override {
        operand_1_->getClocks(clocks);
        operand_2_->getClocks(clocks);
        operand_3_->getClocks(clocks);
    }

private:

    virtual uint32_t getStats_(std::vector<const StatisticInstance*>& results) const override {
        return operand_1_->getStats(results) + operand_2_->getStats(results) + operand_3_->getStats(results);
    }

}; // class TernaryFunction

        } // namespace expression
    } // namespace statistics
} // namespace sparta
