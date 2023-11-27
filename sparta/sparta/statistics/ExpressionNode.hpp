// <ExpressionNode> -*- C++ -*-

#pragma once

#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaException.hpp"

namespace sparta {

    class StatisticInstance;

    namespace statistics {
        namespace expression {

/*!
 * \brief Types of operations supported
 */
enum operation_t {
    OP_NULL     = 0,   //!< No operation
    OP_ADD      = '+', //!< Addition
    OP_SUB      = '-', //!< Subtraction
    OP_MUL      = '*', //!< Multiplication
    OP_DIV      = '/', //!< Division
    OP_NEGATE   = 'n', //!< Negation: -x
    OP_PROMOTE  = 'p', //!< Promotion: +x
    OP_FORWARD  = 'f'  //!< Forwarding: (x)

};

/*!
 * \brief Abstract interface class for an item in an expression. Subclasses can
 * contain other ExpressionNodes.
 *
 * Typically, these are created by the Expression class and there is no need for
 * a client to directly access this interface or any subclasses
 */
class ExpressionNode
{
public:

    /*!
     * \brief No copy-constructable
     */
    ExpressionNode(const ExpressionNode&) = delete;

    ExpressionNode() {
        //std::cout << "EI Con  " << this << std::endl;
    }

    virtual ~ExpressionNode() {
        //std::cout << "EI Des ~" << this << std::endl;
    }

    ExpressionNode* clone() const {
        ExpressionNode* item = clone_();
        //std::cout << "cloned " << item << " <- " << this << std::endl;
        return item;
    }

    /*!
     * \brief Gets the statistics present in this expression
     * \return Number of stats added to results
     * \param results Vector of pointers to StatisticInstances. All statistics
     * within this class will be appended to the results vector.
     * These pointers are valid until this item or its children are modified or
     * deleted
     */
    uint32_t getStats(std::vector<const StatisticInstance*>& results) const {
        return getStats_(results);
    }

    /*!
     * \brief Compute value of this item in simulation. Must be implemented by
     * subclass
     */
    double evaluate(){
        double val = evaluate_();

        // trace
        //dump(std::cout);
        //std::cout << " => " << val << std::endl;

        return val;
    }

    virtual void start() = 0;
    virtual void end() = 0;

    virtual bool supportsCompression() const = 0;

    /*!
     * \brief Dump the content of this expression item
     * \note Must not evaluate the expression
     * \param o Ostream to write to
     * \param show_range Should the range be shown in any subexpression
     * nodes.
     * \param resolve_subexprs Should any referenced statistic defs be
     * expanded to their full expressions so that this becomes an expression
     * containing only counters.
     */
    virtual void dump(std::ostream& o,
                      bool show_range=true,
                      bool resolve_subexprs=true) const = 0;

    /*!
     * \brief Populates a vector with the clocks found in this subexpression
     * node
     * \param clocks Vector of clocks to which any found clocks will be
     * appended. This vector is not cleared.
     */
    virtual void getClocks(std::vector<const Clock*>& clocks) const = 0;

private:

    /*!
     * \brief Compute value of this item in simulation
     */
    virtual double evaluate_() const = 0;

    /*!
     * \brief Implements getStats
     */
    virtual uint32_t getStats_(std::vector<const StatisticInstance*>& results) const = 0;

    /*!
     * \brief Deep copy of the content of this item
     *
     * To be overridden by subclasses
     */
    virtual ExpressionNode* clone_() const = 0;

};

        } // namespace expression
    } // namespace statistics
} // namespace sparta
