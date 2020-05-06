

#pragma once

#include <string>

#include "sparta/simulation/Clock.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/functional/RegisterSet.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/statistics/Counter.hpp"
#include "sparta/events/Event.hpp"


namespace core_example
{
    class Core : public sparta::Unit
    {
    public:

        //! \brief Parameters for Core model
        class CoreParameterSet : public sparta::ParameterSet
        {
        public:
            CoreParameterSet(sparta::TreeNode* n) :
                sparta::ParameterSet(n)
            {
                foo.addDependentValidationCallback([](std::string& s, const sparta::TreeNode*){return s.size() < 10;},
                                                   "Length must be < 10");
                ctr_incr_period.addDependentValidationCallback([](uint64_t& v, const sparta::TreeNode*){return v > 0;},
                                                   "Counter increment period must be > 0");
                ctr_incr_amount.addDependentValidationCallback([](uint32_t& v, const sparta::TreeNode*){return v > 0;},
                                                   "Counter incrementor must be > 0");
            }

            PARAMETER(std::string, foo, "default", "test parameter")
            PARAMETER(uint64_t, ctr_incr_period, 1000, "Period of each 'counter_foo' counter update. Must be > 0")
            PARAMETER(uint32_t, ctr_incr_amount, 1, "Value to increment the counter 'counter_foo' each counter update period. Must be > 0")
            PARAMETER(std::vector<std::string>, contents, {}, "Content???")
        };

        //! \brief Name of this resource. Required by sparta::UnitFactory
        static constexpr char name[] = "core_example_core";

        /*!
         * \brief Core constructor
         * \note This signature is dictated by the sparta::UnitFactory created
         * to contain this node. Generally,
         * \param node TreeNode that is creating this core (always a
         * sparta::UnitTreeNode)
         * \param params Fully configured and validated params, which were
         * instantiate by the sparta::UnitFactory which is instnatiating this
         * resource. Note that the type of params is the ParamsT argument of
         * sparta::UnitFactory and defaults to this class' nested ParameterSet
         * type, NOT sparta::ParameterSet.
         * \param ports Fully configured ports for this component.
         */
        Core(sparta::TreeNode * node,
             const CoreParameterSet * params); // Temporary type

        ~Core() {}


    private:

        /*!
         * \brief Increments the example foo counter in this core
         */
        void incrementCounter_();

        std::vector<Resource *> resources_;
        std::unique_ptr<sparta::RegisterSet> regs_; //!< RegisterSet node
        sparta::Counter* fooctr_; //!< Foo counter example (points to counter owned by ctrs_)
        sparta::Event<> counter_incr_event_; //!< Callback for incrementing counter
        uint64_t counter_incr_period_; //!< Period of counter increments (from Parameters)
        uint64_t counter_incr_amount_; //!< Amount of each counter increment (from Parameters)
    };
}

