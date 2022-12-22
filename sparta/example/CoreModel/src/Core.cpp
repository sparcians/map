
#include <string>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/statistics/Counter.hpp"
#include "sparta/functional/Register.hpp"

#include "Core.hpp"

namespace core_example
{
    // Register Tables for Core
    const char* regfoo_aliases[] = { "the_foo_reg", "reg0", 0 };

    sparta::Register::Definition core_reg_defs[] = {
        { 0, "regfoo", 1, "reg", 0, "regfoo's description", 4, {
                { "field1", "this is field 1. It is 2 bits", 0, 1 },
                { "field2", "this is field 2. It is 4 bits", 0, 3 },
                { "field3", "this is field 3. It is 3 bits", 1, 3 } },
          {}, regfoo_aliases, sparta::Register::INVALID_ID, 0, NULL, 0, 0 },
        sparta::Register::DEFINITION_END
    };

    Core::Core(sparta::TreeNode * node, // TreeNode which owns this. Publish child nodes to this
               const CoreParameterSet * p) : // Core::ParameterSet, not generic SPARTA set
        sparta::Unit(node),
        regs_(sparta::RegisterSet::create(node, core_reg_defs)), // Create register set
        counter_incr_event_(&unit_event_set_, "counter_incr_event",
                            CREATE_SPARTA_HANDLER(Core, incrementCounter_))
    {
        // Now parameters and ports are fixed and sparta device tree is
        // now finalizing, so the parameters and ports can be used to
        // initialize this unit once and permanently.  The TreeNode
        // arg node is the TreeNode which constructed this resource.
        // In this constructor (only), we have the opportunity to add
        // more TreeNodes as children of this node, such as
        // RegisterSet, CounterSet, Register, Counter,
        // Register::Field, etc.  No new ResourceTreeNodes may be
        // added, however.  This unit's clock can be derived from
        // node->getClock().  Child resources of this node who do not
        // have their own nodes could examine the parameters argument
        // here and attach counters to this node's CounterSet.

        ////////////////////////////////////////////////////////////////////////////////
        // Interpret Parameters (ignore any which are not read)
        p->foo.ignore(); // Explicitly ignore (or framework will think we forgot to use it)
        counter_incr_amount_ = p->ctr_incr_amount;
        counter_incr_period_ = p->ctr_incr_period;

        // Print out contents (vector<string>)
        for(auto c : p->contents){
            (void) c;
            //std::cout << c << std::endl;
        }

        ////////////////////////////////////////////////////////////////////////////////
        // Create Counters
        fooctr_ = &unit_stat_set_.createCounter<sparta::Counter>("counter_foo", "Example Counter", sparta::Counter::COUNT_NORMAL);
        assert(node->getClock());

        warn_logger_ << " Completed construction of Core";
    }

    void Core::incrementCounter_() {
        (*fooctr_) += counter_incr_amount_;
        counter_incr_event_.schedule(counter_incr_period_);
    }

}
