// <ContextCounter> -*- C++ -*-


#pragma once

#include <boost/lexical_cast.hpp>
#include <cstddef>
#include <cstdint>
#include <string>
#include <cinttypes>
#include <iterator>
#include <ostream>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>

#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/statistics/StatisticDef.hpp"
#include "sparta/statistics/ReadOnlyCounter.hpp"
#include "sparta/report/Report.hpp"
#include "sparta/utils/StringUtils.hpp"
#include "sparta/trigger/ContextCounterTrigger.hpp"
#include "sparta/statistics/InstrumentationNode.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/kernel/SpartaHandler.hpp"
#include "sparta/statistics/StatisticInstance.hpp"

namespace sparta {
class CounterBase;
class ReadOnlyCounter;
class StatisticSet;
}  // namespace sparta

// Non-template internal counter information for JSON serialization
struct ContextCounterInfo {
    std::string name_;
    std::string desc_;
    sparta::InstrumentationNode::visibility_t vis_ = 0;
    double val_ = 0;
    const void * ctx_addr_ = nullptr;
};

bool __groupedPrinting(
    std::set<const void*> & dont_print_these,
    void * grouped_json_, void * doc_,
    const std::vector<ContextCounterInfo> & ctx_info,
    const std::string & aggregate_desc,
    const sparta::InstrumentationNode::visibility_t aggregate_vis);

bool __groupedPrintingReduced(
    std::set<const void*> & dont_print_these,
    void * grouped_json_, void * doc_,
    const std::vector<ContextCounterInfo> & ctx_info);

bool __groupedPrintingDetail(
    std::set<const void*> & dont_print_these,
    void * grouped_json_, void * doc_,
    const std::vector<ContextCounterInfo> & ctx_info);

namespace sparta
{
    /*!
      \class ContextCounter
      \brief A container type that allows a modeler to build, store, and charge counts to a specific context

      The sparta::ContextCounter allows the modeler to extend the basic
      sparta::CounterBase type with contexts (sub-counts) and an
      aggregate.  This class is useful for counting/charging counts
      towards elements related to threading or processes running on a
      model.  The sparta::ContextCounter also provides an adjustable
      aggregate count for all the registered contexts in the counter.

      To be useful, the sparta::ContextCounter should have two or more
      registered contexts.  Having only one registered context is
      meaningless, and the modeler should consider using a
      sparta::CounterBase type instead.  However, the
      sparta::ContextCounter will support a single context.

      sparta::ContextCounter objects and their contexts can be created
      abstractly using sparta::ContextCounter's given name and a count
      representing the number of contexts.  The aggregate counter is
      added to the unit's sparta::StatisticSet and each internal context
      is a subchild of that aggregate counter.

      The sparta::ContextCounter is by definition a sparta::StatisticDef, is
      like any other sparta::InstrumentationNode, expecting the parent to
      be a sparta::StatisticSet.  Contexts are counter types expected to
      be derived from sparta::CounterBase.

      Since sparta::ContextCounter is a sparta::StatisticDef, the
      sparta::StatisticDef::ExpressionArg that is used by default is a
      sum of the internal contexts.  This is formed like so:
      \f$\sum{context_nameN}\f$

      \code
      # Context counter representation in the SPARTA tree.
      top.unit.stats.example_context_counter           # Aggregate value
      top.unit.stats.example_context_counter.context0  # Context value, index 0
      top.unit.stats.example_context_counter.context1  # Context value, index 1

      \endcode

      \note Context accessing is zero-based.
      \note The CounterT \a must \a be a base type of sparta::CounterBase
      \note The CounterT \a cannot \a be sparta::ReadOnlyCounter type -- doesn't make sense

      <h2>Construction Examples</h2>

      <h3>Basic Example</h3>

      To construct a ContextCounter, the first four parameters of the constructor should always be:

      * The sparta::StatisticSet the counter belongs to
      * The Name of the context counter -- this is also the name of the aggregate
      * The Description of the counter
      * The Number of contexts

      The next arguments are those construction arguments passed to
      the CounterT \a after the sparta::StatisticSet, name, and description.

      \code

      // Construction allowing the ContextCounter to create standard sparta::Counter types
      const uint32_t num_contexts = 2;
      sparta::ContextCounter<sparta::Counter> cxt_counter(getStatisticSet(),
                                                      "example_context_counter",
                                                      "Create a sparta::ContextCounter with two contexts, each being a sparta::Counter",
                                                      num_contexts,
                                                      // sparta::Counter arguments AFTER name, description
                                                      "context_name",
                                                      sparta::Counter::COUNT_NORMAL,
                                                      sparta::InstrumentationNode::VIS_NORMAL);

      sparta::ContextCounter<sparta::CycleCounter> cxt_cycle_counter(getStatisticSet(),
                                                                 "example_context_histogram_counter",
                                                                 "Create a sparta::ContextCounter with two contexts, each being a CycleCounter",
                                                                 num_contexts,
                                                                 // sparta::CycleCounter arguments AFTER name and description
                                                                 "context_name",
                                                                 sparta::Counter::COUNT_NORMAL, &clk,
                                                                 sparta::InstrumentationNode::VIS_NORMAL);


      \endcode

      <h3>Expression Example</h3>

      If the default summing aggregation is not preferred, the modeler
      can override this using the second constructor that provides a
      sparta::StatisticDef::ExpressionArg.

      Example:

      \code
      // Construction allowing the ContextCounter to create standard sparta::Counter types
      const uint32_t num_contexts = 3;
      sparta::ContextCounter<sparta::Counter> cxt_counter(getStatisticSet(),
                                                      "example_context_counter",
                                                      "Create a sparta::ContextCounter with tthree contexts, each being a sparta::Counter",
                                                      num_contexts,
                                                      sparta::StatisticDef::ExpressionArg("(thread0+thread1+thread2)/3"),
                                                      // sparta::Counter arguments AFTER name, description
                                                      "thread",
                                                      sparta::Counter::COUNT_NORMAL,
                                                      sparta::InstrumentationNode::VIS_NORMAL);

      \endcode

      <h2>Accessing and/or Incrementing</h2>

      Accessing and incrementing a context within the counter is
      explicit, by requesting the context directly and operating on
      that type.  For example, if the context is a sparta::Counter type,
      standard incrementing operation is available.  The design
      decision was made to \b not allow \c operator[] overloading in
      C++ to avoid confusion between context accessing and array of
      counters.

      \code

      // Make sure we have two contexts
      sparta_assert(cxt_counter.numContexts() == 2);

      const uint32_t context_0 = 0;
      const uint32_t context_1 = 1;

      ++cxt_counter.context(context_0);
      ++cxt_counter.context(context_1);

      if(SPARTA_EXPECT_FALSE(info_logger_.enabled()) {
          info_logger_ << "The current aggregate: " << cxt_counter;
      }

      // ++cxt_counter.context(context_1 + 1); // Will throw an exception

      \endcode

      To explicitly 'print' the sparta::ContextCounter as a value, it
      must be wrapped and represented as sparta::StatisticInstance.

      Example:

      \code

      sparta::StatisticInstance cxt_counter_si(&cxt_counter);
      std::cout << cxt_counter_si.getValue() << std::endl;

      \endcode

      There is another example of the ContextCounter in the
      core_example::Dispatch block.


    */
    template<class CounterT>
    class ContextCounter : public sparta::StatisticDef
    {

        StatisticDef::ExpressionArg generateExprArg_(uint32_t num_contexts, const std::string & context_name) const
        {
            std::stringstream expr;
            uint32_t i = 0;
            expr << context_name << i;
            ++i;
            while(i < num_contexts) {
                expr << "+" << context_name << i;
                ++i;
            }
            return expr.str();
        }

        virtual bool groupedPrinting(const std::vector<const StatisticInstance*> & sub_stats,
                                     std::set<const void*> & dont_print_these,
                                     void * grouped_json_,
                                     void * doc_) const override
        {
            extractCtxInfo_(sub_stats);
            return __groupedPrinting(dont_print_these, grouped_json_, doc_,
                                     ctx_info_, getDesc(), getVisibility());
        }

        virtual bool groupedPrintingReduced(const std::vector<const StatisticInstance*> & sub_stats,
                                            std::set<const void*> & dont_print_these,
                                            void * grouped_json_,
                                            void * doc_) const override
        {
            extractCtxInfo_(sub_stats);
            return __groupedPrintingReduced(dont_print_these, grouped_json_, doc_, ctx_info_);
        }

        virtual bool groupedPrintingDetail(const std::vector<const StatisticInstance*> & sub_stats,
                                           std::set<const void*> & dont_print_these,
                                           void * grouped_json_,
                                           void * doc_) const override
        {
            extractCtxInfo_(sub_stats);
            return __groupedPrintingDetail(dont_print_these, grouped_json_, doc_, ctx_info_);
        }

        void extractCtxInfo_(const std::vector<const StatisticInstance*> & sub_stats) const
        {
            if (!ctx_info_.empty()) {
                sparta_assert(sub_stats.size() == ctx_info_.size());
                for (size_t idx = 0; idx < sub_stats.size(); ++idx) {
                    const auto & stat_si = sub_stats[idx];
                    auto & ctx_info = ctx_info_[idx];
                    ctx_info.val_ = stat_si->getValue();
                }
                return;
            }

            sparta_assert(sub_stats.size() == internal_counters_.size());

            ctx_info_.reserve(internal_counters_.size());
            for (size_t idx = 0; idx < sub_stats.size(); ++idx) {
                const auto & counter = internal_counters_[idx];
                const auto & stat_si = sub_stats[idx];

                ContextCounterInfo info;
                info.name_ = counter.getName();
                info.desc_ = counter.getDesc();
                info.vis_ = counter.getVisibility();
                info.val_ = stat_si->getValue();
                info.ctx_addr_ = &counter;
                ctx_info_.push_back(info);

                //There is code that needs to know our internal_counter_
                //variables' this pointers. That code goes through the
                //StatisticDef::getSubStatistics() method to get those
                //pointers without asking the ContextCounter directly.
                //This assertion is to prevent that outside logic from
                //breaking unexpectedly.
                sparta_assert(stat_si->getCounter() == &counter);
            }
        }

        mutable std::vector<ContextCounterInfo> ctx_info_;

    public:
        using counter_type = CounterT;

        //! \name Construction & Initialization
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief ContextCounter constructor
         * \param parent parent node. Must not be nullptr
         * \param name Name of this counter. Must be a valid TreeNode name
         * \param desc Description of this node. Required to be a valid
         *             TreeNode description.
         * \param num_contexts The number of contexts to create
         * \param ...args Arguments to pass to the sparta::CounterBase type, \a after
         *                the standard TreeNode *, name, and description
         */
        template <class... CounterTArgs>
        ContextCounter(StatisticSet * stat_set,
                       const std::string & name,
                       const std::string & desc,
                       const uint32_t num_contexts,
                       const std::string & context_name,
                       CounterTArgs && ...args) :
            ContextCounter(stat_set,
                           name,
                           desc,
                           num_contexts,
                           generateExprArg_(num_contexts, context_name),
                           context_name,
                           std::forward<CounterTArgs>(args)...)
        { }

        /*!
         * \brief ContextCounter constructor
         * \param parent parent node. Must not be nullptr
         * \param name Name of this counter. Must be a valid TreeNode name
         * \param desc Description of this node. Required to be a valid
         *             TreeNode description.
         * \param num_contexts The number of contexts to create
         * \param expression A string argument that represents how the aggregate should be presented
         * \param ...args Arguments to pass to the sparta::CounterBase type, \a after
         *                the standard TreeNode *, name, and description
         */
        template <class... CounterTArgs>
        ContextCounter(StatisticSet * stat_set,
                       const std::string & name,
                       const std::string & desc,
                       const uint32_t num_contexts,
                       const StatisticDef::ExpressionArg & expression,
                       const std::string & context_name,
                       CounterTArgs && ...args) :
            sparta::StatisticDef(stat_set, name, desc + " aggregate", this, expression)
        {
            static_assert(std::is_same<sparta::ReadOnlyCounter, CounterT>::value == false,
                          "The Counter type (CounterT) of ContextCounter cannot be a ReadOnlyCounter -- doesn't make sense!");
            static_assert(std::is_base_of<sparta::CounterBase, CounterT>::value,
                          "The Counter type (CounterT) of ContextCounter must be a sparta::CounterBase type");

            // You MUST reserve memory for the Counters in the vector
            // otherwise, the vector will MOVE the counters internally
            // on each growth and reorder the counters in the tree --
            // putting them out-of-order
            internal_counters_.reserve(num_contexts);
            for(uint32_t i = 0; i < num_contexts; ++i) {
                internal_counters_.emplace_back(this,
                                                context_name + utils::uint32_to_str(i),
                                                "A context of counter " + name, args...);
            }

            auto make_sub_stat_name = [&context_name](const std::string & name,
                                                      const size_t sub_stat_index) {
                std::ostringstream oss;
                oss << name << "_" << context_name << sub_stat_index;
                return oss.str();
            };

            size_t sub_stat_index = 0;
            for (const auto & counter : internal_counters_) {
                addSubStatistic_(&counter, make_sub_stat_name(name, sub_stat_index));
                ++sub_stat_index;
            }

            addMetadata_("context_name", context_name);
            addMetadata_("num_contexts", std::to_string(num_contexts));

            begin_counter_ = &internal_counters_.front();
            end_counter_ = &internal_counters_.back();
            ++end_counter_;
        }

        /*!
         * \brief Return the internal counter at the given context
         * \param idx The context idx
         */
        const counter_type & context(const uint32_t idx) const {
            sparta_assert(idx < internal_counters_.size());
            return internal_counters_[idx];
        }

        /*!
         * \brief Return the internal counter at the given context
         * \param idx The context idx
         */
        counter_type & context(const uint32_t idx) {
            sparta_assert(idx < internal_counters_.size());
            return internal_counters_[idx];
        }

        /*!
         * \brief Return a pointer to the first internal counter
         */
        const counter_type * begin() const {
            sparta_assert(begin_counter_);
            return begin_counter_;
        }

        /*!
         * \brief Return a pointer to one past the last internal counter
         */
        const counter_type * end() const {
            sparta_assert(end_counter_);
            return end_counter_;
        }

        /*!
         * \brief Return the number of contexts in this ContextCounter
         */
        uint32_t numContexts() const {
            return std::distance(begin(), end());
        }

    private:
        // Internal counters
        std::vector<CounterT> internal_counters_;
        counter_type * begin_counter_ = nullptr;
        counter_type * end_counter_ = nullptr;
    };

}

/*!
 * \brief Register a context counter aggregate function that is a member function
 * of a user-supplied sparta::ContextCounter<T> subclass.
 *
 * This macro should be called like this:
 *
 *     REGISTER_CONTEXT_COUNTER_AGGREGATE_FCN(
 *         ClassT (i.e. ThreadedContextCounter<sparta::Counter>, etc.)
 *         Class method name that calculates the aggregate value,
 *         Member variable that holds the calculated aggregate value);
 *
 * For example:
 *
 *     template <typename CounterT>
 *     class MyContextCounter : public sparta::ContextCounter<CounterT>
 *     {
 *     public:
 *        MyContextCounter() {
 *          REGISTER_CONTEXT_COUNTER_AGGREGATE_FCN(
 *            MyContextCounter<CounterT>, myCalcMethod_, my_calc_value_);
 *       }
 *     private:
 *       void myCalcMethod_() {
 *         //do some calculations...
 *         my_calc_value_ = 3.14;
 *       }
 *       double my_calc_value_ = 0;
 *     };
 *
 * Some notes:
 *   - The method that is to be called for the calculation (myCalcMethod_)
 *     must have the signature 'void method()', with no inputs and no
 *     outputs
 *   - The member variable used to store the calculated aggregate value
 *     must be of type double
 */
#define REGISTER_CONTEXT_COUNTER_AGGREGATE_FCN(ClassT, method, aggregated_value) \
    static_assert(std::is_base_of<sparta::TreeNode, ClassT>::value, \
                  "Invalid attempt to register a context counter aggregate function " \
                  "that is not a method of a sparta::TreeNode subclass."); \
    { \
    auto handler = CREATE_SPARTA_HANDLER(ClassT, method); \
    sparta::trigger::ContextCounterTrigger:: \
        registerContextCounterAggregateFcn(handler, this, #method, aggregated_value); \
    }

