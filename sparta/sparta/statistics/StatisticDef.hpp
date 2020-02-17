// <StatisticDef> -*- C++ -*-


/*!
 * \file StatisticDef.hpp
 * \brief Contains a statistic definition (some useful information which can be
 * computed)
 */

#ifndef __STATISTIC_DEF_H__
#define __STATISTIC_DEF_H__

#include <iostream>
#include <sstream>
#include <cstdarg>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/statistics/InstrumentationNode.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/StringManager.hpp"
#include "sparta/statistics/Expression.hpp"

namespace sparta
{
namespace trigger {
class ContextCounterTrigger;
}  // namespace trigger

    /*!
     * \brief Contains a statistic definition (some useful information which can
     * be computed)
     */
    class StatisticDef : public InstrumentationNode
    {
    public:

        //! \name Types
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Intermediate type for minimizing the number of distinct
         * constructors that must be created for this class because any
         * constructor can take an Expression object or a string representing an
         * expression
         */
        struct ExpressionArg {

            ExpressionArg(const std::string& str) :
                expr_str(str)
            {;}

            ExpressionArg(const char str[]) :
                expr_str(str)
            {;}

            ExpressionArg(const sparta::statistics::expression::Expression& expr) :
                expr_obj(new sparta::statistics::expression::Expression(expr)),
                expr_str("")
            {;}

            ExpressionArg(const ExpressionArg& rhp) :
                expr_obj(rhp.expr_obj ? new sparta::statistics::expression::Expression(*rhp.expr_obj) : nullptr),
                expr_str(rhp.expr_str)
            {;}

            std::unique_ptr<sparta::statistics::expression::Expression> expr_obj;
            std::string expr_str;
        };

        /*!
         * \brief How should the value of this statistic be interpreted
         * Certain outputters (e.g. report formatters) may use this information
         * to enhance the presented data.
         * \note This gives no hint as to what the statistic itself represents
         */
        enum ValueSemantic {

            /*!
             * \brief Invalid semantic. No StatisticDef should have this value
             * semantic
             */
            VS_INVALID    = 0,

            /*!
             * \brief An absolute number having no units (typical default)
             */
            VS_ABSOLUTE   = 1,

            /*!
             * \brief A percentage. This value should be in the range [0,100].
             * Some report formatters may add a '%' when displaying values
             * having this semantic or generate more content (e.g.
             * percentage bars)
             */
            VS_PERCENTAGE = 2,

            /*!
             * \brief A fractional number. This value should be in the range
             * [0,1]. Some report formatters could show this with additional
             * content (e.g. percentage bars)
             */
            VS_FRACTIONAL = 3

            ///*!
            // * \brief A number (typically an absolute number) having specific
            // * units (i.e. not a percentage/fraction). See CounterBase::getUnits
            // * and StatisticDef::getUnits.
            // */
            //VS_UNITS      = 4
        };

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Construction
        //! @{
        ////////////////////////////////////////////////////////////////////////

        //! \brief Not copy-constructable
        StatisticDef(const StatisticDef&) = delete;

        //! \brief Not move-constructable (too complicated)
        StatisticDef(const StatisticDef&&) = delete;

        /*!
         * \brief Move constructor
         * \pre See InstrumentationNode move constructor
         */
        StatisticDef(StatisticDef&& rhp) :
            InstrumentationNode(std::move(rhp)),
            prebuilt_expr_(std::move(rhp.prebuilt_expr_)),
            expr_str_(rhp.expr_str_),
            context_(rhp.context_),
            semantic_(rhp.semantic_)

        {
            // Note: this must happen in leaf type
            TreeNode* parent = rhp.getParent();
            if(parent != nullptr){
                parent->addChild(this);
            }
        }

        //! \brief Not assign-constructable
        StatisticDef& operator=(const StatisticDef&) = delete;

        /*!
         * \brief Constructor with string expression
         * \note Do not use this constructor from a subclass
         * \note Does not test validity of expression here because dependencies
         * may not yet exist.
         *
         * Example:
         * \code
         * StatisticDef s1(parent, "foo0", "foo"m, 0, "The Foo", &stat_set,
         *                 "ctr_a/ctr_b",
         *                 StatisticSet::VS_ABSOLUTE,
         *                 StatisticSet::VIS_NORMAL);
         * StatisticDef s2(parent, "foo0", "foo"m, 0, "The Foo", &stat_set,
         *                 Expression(5) / Expression(2),
         *                 StatisticSet::VS_ABSOLUTE,
         *                 StatisticSet::VIS_NORMAL);
         * \endcode
         */
        StatisticDef(TreeNode* parent,
                     const std::string& name,
                     const std::string& group,
                     group_idx_type group_idx,
                     const std::string& desc,
                     TreeNode* context,
                     ExpressionArg expression,
                     ValueSemantic semantic,
                     visibility_t visibility) :
            InstrumentationNode(name,
                                group,
                                group_idx,
                                desc,
                                InstrumentationNode::TYPE_STATISTICDEF,
                                visibility),
            prebuilt_expr_(std::move(expression.expr_obj)),
            expr_str_(std::move(expression.expr_str)),
            context_(context),
            semantic_(semantic)
        {
            setExpectedParent_(parent);

            sparta_assert(semantic_ != VS_INVALID,
                              "Cannot construct a StatisticDef with VS_INVALID value semantic");

            if(prebuilt_expr_ != nullptr){
                expr_str_ = prebuilt_expr_->stringize();
            }else{
                if(context_ == nullptr){
                    throw SpartaException("When constructing StatisticDef")
                        << getLocation() << " context must not be nullptr. It must be a TreeNode which "
                        "will be used to look up any Node names found in the expression";
                }

                if(expr_str_ == ""){
                    throw SpartaException("When constructing StatisticDef ")
                        << getLocation() << " without a prebuilt expression, the expression string "
                        "must not be \"\". It must be a non-empty string containing an arithmetic "
                        "expression referring to nodes relative to the context \""
                        << context_->getLocation() << "\"";
                }
            }

            ensureParentIsStatisticSet_(parent);

            // Other initialization here
            // ...

            if(parent){
                parent->addChild(this);
            }
        }

        /*!
         * \brief Constructor
         */
        StatisticDef(const std::string& name,
                     const std::string& group,
                     group_idx_type group_idx,
                     const std::string& desc,
                     TreeNode* context,
                     ExpressionArg expression,
                     ValueSemantic semantic,
                     visibility_t visibility) :
            StatisticDef(nullptr,
                         name,
                         group,
                         group_idx,
                         desc,
                         context,
                         expression,
                         semantic,
                         visibility)
        {
            // Delegated constructor
        }

        /*!
         * \brief Constructor
         * \note Do not use this constructor from a subclass
         *
         * group defaults to GROUP_NAME_NONE. group_idx defaults to
         * GROUP_IDX_NONE.
         */
        StatisticDef(TreeNode* parent,
                     const std::string& name,
                     const std::string& desc,
                     TreeNode* context,
                     ExpressionArg expression,
                     ValueSemantic semantic,
                     visibility_t visibility) :
            StatisticDef(parent,
                         name,
                         GROUP_NAME_NONE,
                         GROUP_IDX_NONE,
                         desc,
                         context,
                         expression,
                         semantic,
                         visibility)
        {
            // Delegated constructor
        }

        /*!
         * \brief TreeNode constructor with no parent node or group information
         * \see other sparta::TreeNode constructors
         *
         * group defaults to GROUP_NAME_NONE. group_idx defaults to
         * GROUP_IDX_NONE.
         */
        StatisticDef(const std::string& name,
                     const std::string& desc,
                     TreeNode* context,
                     ExpressionArg expression,
                     ValueSemantic semantic,
                     visibility_t visibility) :
            StatisticDef(nullptr,
                         name,
                         GROUP_NAME_NONE,
                         GROUP_IDX_NONE,
                         desc,
                         context,
                         expression,
                         semantic,
                         visibility)
        {
            // Delegated Constructor
        }

        /*!
         * \brief Constructor
         * \note Do not use this constructor from a subclass
         * \note Does not test validity of expression here because dependencies
         * may not yet exist.
         */
        StatisticDef(TreeNode* parent,
                     const std::string& name,
                     const std::string& group,
                     group_idx_type group_idx,
                     const std::string& desc,
                     TreeNode* context,
                     ExpressionArg expression) :
            StatisticDef(parent,
                         name,
                         group,
                         group_idx,
                         desc,
                         context,
                         expression,
                         VS_ABSOLUTE,
                         DEFAULT_VISIBILITY)
        {
            // Delegated constructor
        }

        /*!
         * \brief Constructor
         */
        StatisticDef(const std::string& name,
                     const std::string& group,
                     group_idx_type group_idx,
                     const std::string& desc,
                     TreeNode* context,
                     ExpressionArg expression) :
            StatisticDef(nullptr,
                         name,
                         group,
                         group_idx,
                         desc,
                         context,
                         expression)
        {
            // Delegated constructor
        }

        /*!
         * \brief Constructor
         * \note Do not use this constructor from a subclass
         *
         * group defaults to GROUP_NAME_NONE. group_idx defaults to
         * GROUP_IDX_NONE.
         */
        StatisticDef(TreeNode* parent,
                     const std::string& name,
                     const std::string& desc,
                     TreeNode* context,
                     ExpressionArg expression) :
            StatisticDef(parent,
                         name,
                         GROUP_NAME_NONE,
                         GROUP_IDX_NONE,
                         desc,
                         context,
                         expression)
        {
            // Delegated constructor
        }

        /*!
         * \brief TreeNode constructor with no parent node or group information
         * \see other sparta::TreeNode constructors
         *
         * group defaults to GROUP_NAME_NONE. group_idx defaults to
         * GROUP_IDX_NONE.
         */
        StatisticDef(const std::string& name,
                     const std::string& desc,
                     TreeNode* context,
                     ExpressionArg expression) :
            StatisticDef(nullptr,
                         name,
                         GROUP_NAME_NONE,
                         GROUP_IDX_NONE,
                         desc,
                         context,
                         expression)
        {
            // Delegated Constructor
        }

        /*!
         * \brief Constructor
         */
        StatisticDef(TreeNode* parent,
                     const std::string& name,
                     const std::string& group,
                     group_idx_type group_idx,
                     const std::string& desc,
                     TreeNode* context,
                     ExpressionArg expression,
                     ValueSemantic semantic) :
            StatisticDef(parent,
                         name,
                         group,
                         group_idx,
                         desc,
                         context,
                         expression,
                         semantic,
                         DEFAULT_VISIBILITY)
        {
            // Delegated constructor
        }

        /*!
         * \brief Constructor
         */
        StatisticDef(const std::string& name,
                     const std::string& group,
                     group_idx_type group_idx,
                     const std::string& desc,
                     TreeNode* context,
                     ExpressionArg expression,
                     ValueSemantic semantic) :
            StatisticDef(nullptr,
                         name,
                         group,
                         group_idx,
                         desc,
                         context,
                         expression,
                         semantic)
        {
            // Delegated constructor
        }

        /*!
         * \brief Constructor
         * \note Do not use this constructor from a subclass
         *
         * group defaults to GROUP_NAME_NONE. group_idx defaults to
         * GROUP_IDX_NONE.
         */
        StatisticDef(TreeNode* parent,
                     const std::string& name,
                     const std::string& desc,
                     TreeNode* context,
                     ExpressionArg expression,
                     ValueSemantic semantic) :
            StatisticDef(parent,
                         name,
                         GROUP_NAME_NONE,
                         GROUP_IDX_NONE,
                         desc,
                         context,
                         expression,
                         semantic)
        {
            // Delegated constructor
        }

        /*!
         * \brief TreeNode constructor with no parent node or group information
         * \see other sparta::TreeNode constructors
         *
         * group defaults to GROUP_NAME_NONE. group_idx defaults to
         * GROUP_IDX_NONE.
         */
        StatisticDef(const std::string& name,
                     const std::string& desc,
                     TreeNode* context,
                     ExpressionArg expression,
                     ValueSemantic semantic) :
            StatisticDef(nullptr,
                         name,
                         GROUP_NAME_NONE,
                         GROUP_IDX_NONE,
                         desc,
                         context,
                         expression,
                         semantic)
        {
            // Delegated Constructor
        }

        /*!
         * \brief Virtual destructor
         */
        virtual ~StatisticDef()
        { }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Attributes & Generation
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief This helper class serves as a placeholder for substatistic
         * creation
         */
        class PendingSubStatCreationInfo
        {
        public:
            PendingSubStatCreationInfo(const TreeNode * stat_node,
                                       const std::string & stat_name) :
                stat_node_(stat_node),
                stat_name_(stat_name)
            {}
            const TreeNode * getNode() const {
                return stat_node_;
            }
            const std::string & getName() const {
                return stat_name_;
            }
        private:
            const TreeNode * stat_node_ = nullptr;
            std::string stat_name_;
        };

        inline const std::vector<PendingSubStatCreationInfo> & getSubStatistics() const {
            return sub_statistics_;
        }

        /*!
         * \brief Get the TreeNode location where this
         * StatisticDef lives. Returns an empty string
         * if the TreeNode* given to the constructor
         * was null.
         */
        std::string getContextLocation() const {
            return context_ ? context_->getLocation() : "";
        }

    private:

        /*!
         * \brief Tell this SI that it needs to automatically deregister itself
         * with the ContextCounterTrigger singleton registry of custom aggregation
         * routines
         */
        void deregisterAggregationFcnUponDestruction_() const {
            //It is possible that our auto-deregister object has been set already, which
            //happens when one ContextCounter has more than one registered aggregation
            //callback. So we need to check for null or we will inadvertently deregister
            //our aggregation callback too soon (calling .reset on a non-null unique_ptr
            //will trigger the AutoContextCounterDeregistration object's destructor of
            //course).
            if (auto_cc_deregister_ == nullptr) {
                auto_cc_deregister_.reset(new AutoContextCounterDeregistration(this));
            }
        }

        //! sparta::trigger::ContextCounterTrigger needs to be a fried to call the
        //! private 'deregister' method above.
        friend class trigger::ContextCounterTrigger;

    protected:

        /*!
         * \brief Allow subclasses to forward along substatistic information
         * to this stat definition. Substatistics will not be created until
         * the report adds this definition's statistic instance to its list
         * of stats
         */
        inline void addSubStatistic_(const TreeNode * stat_node,
                                     const std::string & stat_name) const {
            sub_statistics_.emplace_back(stat_node, stat_name);
        }

    public:

        /*!
         * \brief Returns a reference to the expression string which this node
         * was constructed with or a rendering of the expression object which
         * this node was constructed which (depending on which was given at
         * construction)
         */
        std::string getExpression() const {
            return expr_str_;
        }

        /*!
         * \brief Returns a unique Expression for this StatisticInstance given
         * a set of substitutions that the expression may use in parsing. If
         * this class was constructed with an expression object instead of a
         * string, a copy of that object is returned.
         * \param used TreeNodes already in an expression containing this e
         * expression.
         */
        sparta::statistics::expression::Expression
        realizeExpression(std::vector<const TreeNode*>& used) const {
            if(prebuilt_expr_){
                return *prebuilt_expr_; // Returns a copy
            }
            // This is deferred until this point because the expression can
            // contain variables populated using the 'used' vector
            return sparta::statistics::expression::Expression(expr_str_, context_, used);
        }

        /*!
         * \brief Retuns the value-semantic associated with this node at
         * construction
         */
        ValueSemantic getValueSemantic() const {
            return semantic_;
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Printing Methods
        //! @{
        ////////////////////////////////////////////////////////////////////////

        // Override from TreeNode
        virtual std::string stringize(bool pretty=false) const override {
            (void) pretty;
            std::stringstream ss;
            ss << '<' << getLocation() << " expr:" << getExpression();
            ss << " vis:" << getVisibility();
            stringizeTags(ss);
            ss << '>';
            return ss.str();
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

private:

        /*!
         * \brief Ensures that the parent node is a StatisticSet
         * \throw SpartaException if node is not a StatisticSet.
         * \noet Uses dynamic_cast
         */
        void ensureParentIsStatisticSet_(TreeNode*);

        /*!
         * \brief Ensure that this statistic can be evaluated after finalization
         * \note Override of TreeNode::validateNode_
         */
        virtual void validateNode_() const override {
            if(prebuilt_expr_){
                // Guaranteed OK because expression was built before this node was constructed
            }else{
                try{
                    sparta::statistics::expression::Expression(expr_str_, context_);
                }catch(SpartaException &ex){
                    throw SpartaException("Failed to validate StatisticDef: \"") << getLocation()
                        << "\": " << ex.what();
                }
            }
        }

        /*!
         * \brief Pre-built expression specified at construction.
         * If this is not nullptr, use this as the expression for this node
         * instead of expr_str_.
         */
        std::unique_ptr<sparta::statistics::expression::Expression> prebuilt_expr_;

        /*!
         * \brief Expression string contained by this def. This exists so that
         * StatisticDef nodes can be constructed with a string and realized
         * once the rest of the device tree has been constructed later.
         * \note Stores the prebuilt_expr_.stringize() if prebuilt_expr_ is not
         * nullptr
         */
        std::string expr_str_;

        /*!
         * \brief Contenxt for lookup of TreeNodes found by name in expr_str_
         */
        TreeNode* const context_;

        /*!
         * \brief Value semantic
         */
        const ValueSemantic semantic_;

        /*!
         * \brief All pending substatistic information (TreeNode* and statistic
         * name)
         */
        mutable std::vector<PendingSubStatCreationInfo> sub_statistics_;

        /*!
         * \brief Class which handles automatic deregistration of ContextCounter
         * aggregate functions when those objects (the StatisticDef subclasses)
         * go out of scope
         */
        class AutoContextCounterDeregistration {
        public:
            explicit AutoContextCounterDeregistration(const StatisticDef * sd);
            ~AutoContextCounterDeregistration();
        private:
            const StatisticDef *const sd_;
        };

        mutable std::unique_ptr<AutoContextCounterDeregistration> auto_cc_deregister_;
    };

} // namespace sparta

// __STATISTIC_DEF_H__
#endif
