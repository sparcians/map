#include "sparta/utils/SpartaTester.hpp"
#include "sparta/sparta.hpp"
#include "sparta/simulation/RootTreeNode.hpp"
#include "sparta/statistics/Counter.hpp"
#include "sparta/log/Tap.hpp"
#include "sparta/trigger/Trigger.hpp"
#include "sparta/trigger/SingleTrigger.hpp"
#include "sparta/trigger/Triggerable.hpp"
#include "sparta/trigger/TriggerManager.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include <list>
TEST_INIT;

typedef std::list<uint32_t> AssertList;

namespace sparta
{
    class TestTriggerable : public trigger::Triggerable
    {
    public:
        TestTriggerable(Clock* clk,
                        AssertList*on_vec, AssertList* off_vec) :
            trigger::Triggerable(),
            clk_(clk),
            on_vector_(on_vec),
            off_vector_(off_vec)
        {
        }
        //Check to make sure all of the go triggers in the on_vector fire,
        //and all of the stop triggers in the off_vector fire.
        ~TestTriggerable()
        {
            EXPECT_EQUAL(on_vector_->size(), 0);
            EXPECT_EQUAL(off_vector_->size(), 0);
        }
        virtual void go()
        {
            EXPECT_REACHED();
            auto it = on_vector_->begin();
            EXPECT_EQUAL(clk_->currentCycle(), *it);
            on_vector_->erase(it);
        }

        virtual void repeat()
        {
            go();
        }

        virtual void stop()
        {
            EXPECT_REACHED();
            auto it = off_vector_->begin();
            EXPECT_EQUAL(clk_->currentCycle(), *it);
            off_vector_->erase(it);
        }

    private:
        Clock* clk_;
        AssertList* on_vector_;
        AssertList* off_vector_;

    };

    class CounterTriggerable
    {
    public:

        bool hit = false;

        void onFire() {

            // Note: No current assumptions about the order of setting the trigger inactive
            // (i.e. before or after this callback)

            EXPECT_REACHED();
            hit = true;
        }
    };
}
using namespace sparta;

int main()
{
    sparta::Scheduler sched;
    sparta::Clock clk("clock", &sched);

    /*///////////////////////////Test a trigger that uses
      relative timing and defualt period options. With a period of
      10, start 50, and end 100. stop() 100 to be the last part of this
      trigger
    */
    trigger::Trigger trigger("reaccuring_default_options", &clk);
    AssertList trigger_ons {53, 60, 70, 80, 90, 100};
    AssertList trigger_offs {103};
    TestTriggerable testTriggeredObj(&clk, &trigger_ons,
                                     &trigger_offs);

    trigger.addTriggeredObject(&testTriggeredObj);
    trigger.setTriggerStartAbsolute(&clk, 53);//, &clk, 50);
    trigger.setTriggerStopAbsolute(&clk, 103);
    trigger.setRecurring(&clk, 10);
    trigger.setPeriodAlignmentOptions(true);
    std::cout << trigger;

    /* ----------------------------------------------
       Test a trigger that is similar to the last, but uses relative
       scheduling to stop
    */
    trigger::Trigger stop_rel("start_only", &clk);
    AssertList stop_rel_ons {53, 63, 73, 83, 93};
    AssertList stop_rel_offs {103};
    TestTriggerable testTriggeredObjRel(&clk, &stop_rel_ons,
                                            &stop_rel_offs);
    stop_rel.addTriggeredObject(&testTriggeredObjRel);
    stop_rel.setTriggerStartAbsolute(&clk, 53);
    stop_rel.setTriggerStopRelativeToStart(&clk, 50);
    stop_rel.setRecurring(&clk, 10);
    stop_rel.setPeriodAlignmentOptions(false);


    /* ----------------------------------------------
       Test a trigger that is similar to the last, but uses relative
       scheduling to stop
    */
    trigger::Trigger start_only("start_only", &clk);
    AssertList start_only_ons {75};
    AssertList start_only_offs {};
    TestTriggerable testTriggeredObjStart(&clk, &start_only_ons,
                                            &start_only_offs);
    start_only.addTriggeredObject(&testTriggeredObjStart);
    start_only.setTriggerStartAbsolute(&clk, 75);

    try{
        sched.finalize();
        sched.run(109, true);
    }catch(const sparta::DAG::CycleException& ce){
        ce.writeCycleAsDOT(std::cerr);
    };

    {
        RootTreeNode root;
        log::Tap t(&sched, "", std::cout);
        ClockManager cm(&sched);
        Clock::Handle c_root = cm.makeRoot();
        Clock::Handle c_12   = cm.makeClock("C21", c_root, 2, 1);
        root.setClock(c_12.get());
        StatisticSet ss(&root);
        Counter& ctr = ss.createCounter<Counter>("foo", "Foo counter", CounterBase::COUNT_NORMAL);

        root.enterConfiguring();
        root.enterFinalized();

        ctr += 2;
        EXPECT_EQUAL(ctr, 2ull);

        CounterTriggerable counter_triggerable;
        auto handler = SpartaHandler::from_member<CounterTriggerable, &CounterTriggerable::onFire>
                                        (&counter_triggerable, "CounterTriggerable::onFire()");

        trigger::CounterTrigger ctrig("foo trigger", handler, &ctr, 100);
        EXPECT_EQUAL(ctrig.hasFired(), false);
        EXPECT_EQUAL(ctrig.isActive(), true); // Expected to be active at construction

        trigger::CounterTrigger ctrig2("bar trigger", handler, &ctr, 110);
        EXPECT_EQUAL(ctrig2.hasFired(), false);
        EXPECT_EQUAL(ctrig2.isActive(), true); // Expected to be active at construction

        sparta::trigger::TriggerManager & trig_mgr =
            sparta::trigger::TriggerManager::getTriggerManager();

        trigger::CounterTrigger ctrig3(ctrig);
        EXPECT_EQUAL(ctrig3.isActive(), true);
        EXPECT_EQUAL(trig_mgr.hasTrigger(&ctrig3), true);

        ctrig3 = ctrig;
        EXPECT_EQUAL(ctrig3.isActive(), true);
        EXPECT_EQUAL(trig_mgr.hasTrigger(&ctrig3), true);

        uint32_t i;
        auto SCHEDULER_START_TICK = sched.getCurrentTick();
        for(i = 0; i < 200; ++i){
            std::cout << " i = " << i << ", tick = " << sched.getCurrentTick()
                      << std::endl;
            ctr += 3;
            sched.run(1, true);
            if(counter_triggerable.hit == true){
                break;
            }
            sparta_assert(sched.getCurrentTick() == SCHEDULER_START_TICK + i + 1,
                  "Scheduler did not run for 1 tick. Cur tick is "
                  << sched.getCurrentTick() << " but should be "
                  << SCHEDULER_START_TICK + i + 1 << " (i=" << i << ")");
            EXPECT_EQUAL(ctrig.hasFired(), false);
            EXPECT_EQUAL(ctrig.isActive(), true);
        }
        EXPECT_EQUAL(i, 32);
        EXPECT_EQUAL(ctr, 101ull);
        EXPECT_EQUAL(ctrig.hasFired(), true);
        EXPECT_EQUAL(ctrig.isActive(), false);
        EXPECT_EQUAL(sched.getCurrentTick(), SCHEDULER_START_TICK + 33);

        ctrig.deactivate();
        EXPECT_EQUAL(ctrig.isActive(), false);
        EXPECT_EQUAL(trig_mgr.hasTrigger(&ctrig), false);

        ctrig3 = ctrig;
        EXPECT_EQUAL(ctrig3.isActive(), false);
        EXPECT_EQUAL(trig_mgr.hasTrigger(&ctrig3), false);

        root.enterTeardown();
    }

    ENSURE_ALL_REACHED(3);
    REPORT_ERROR;
    return ERROR_CODE;
}
