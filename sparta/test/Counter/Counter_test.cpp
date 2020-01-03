
#include <inttypes.h>
#include <iostream>

#include "sparta/sparta.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/statistics/Counter.hpp"
#include "sparta/statistics/ReadOnlyCounter.hpp"
#include "sparta/statistics/CycleCounter.hpp"
#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/simulation/ResourceFactory.hpp"
#include "sparta/events/Event.hpp"
#include "sparta/events/StartupEvent.hpp"
#include "sparta/events/EventSet.hpp"

#include <boost/timer/timer.hpp>
/*!
 * \file main.cpp
 * \brief Test for Register
 *
 * Register is built on DataView and RegisterSet is built on ArchData.
 * The DataView test performs extensive testing so some test-cases related
 * to register sizes and layouts may be omitted from this test.
 */

TEST_INIT;

using sparta::Counter;
using sparta::CounterBase;
using sparta::ReadOnlyCounter;
using sparta::CycleCounter;
using sparta::StatisticSet;
using sparta::RootTreeNode;
using sparta::ResourceTreeNode;


//! Dummy device
class DummyDevice : public sparta::Resource
{
public:

    static constexpr const char* name = "DummyDevice";

    class ParameterSet : public sparta::ParameterSet
    {
    public:
        ParameterSet(sparta::TreeNode* tn) : sparta::ParameterSet(tn) {}
    };

    DummyDevice(sparta::TreeNode* node,
                const DummyDevice::ParameterSet*) :
        sparta::Resource(node),
        es(node)
    {
        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(DummyDevice, dummyCallback));
    }

    // Infinite loop
    void dummyCallback() {
        dummy_callback.schedule();
    }

    sparta::EventSet es;
    sparta::Event<> dummy_callback{&es, "dummy_callback",
            CREATE_SPARTA_HANDLER(DummyDevice, dummyCallback), 1};
};

int main()
{
    {
        sparta::Scheduler sched;
        sparta::Clock clk("clock", &sched);
        RootTreeNode root;
        root.setClock(&clk); // Set clock within configuration phase
        sparta::ResourceFactory<DummyDevice, DummyDevice::ParameterSet> rfact;
        sparta::ResourceTreeNode dummy(&root, "dummy", "dummy node", &rfact);
        StatisticSet cset(&dummy);

        // Print current counter set by the ostream insertion operator
        std::cout << cset << std::endl;

        // Print current counter set by iteration
        for(CounterBase* c : cset.getCounters()){
            std::cout << c << std::endl;
        }
        std::cout << std::endl;


        // Build the set
        Counter ctr_a(&cset, "A", "group", 0, "The A counter", Counter::COUNT_NORMAL);
        Counter ctr_b(&cset, "B", "group", 1, "The B counter", Counter::COUNT_INTEGRAL);
        Counter ctr_c(&cset, "C", "The C counter", Counter::COUNT_LATEST);
        Counter ctr_d(&cset, "D", "The D counter", Counter::COUNT_NORMAL);

        // Ensure construction of simple arrays
        Counter ctrarr[2] = {{&cset, "X", "test", 0, "The A counter", Counter::COUNT_NORMAL},
                             {&cset, "Y", "test", 1, "The B counter", Counter::COUNT_INTEGRAL}};
        (void) ctrarr;

        // Ensure counters can be move constructed from an initializer list
        //std::vector<Counter> initialized_ctr_vec = {
        //        std::move(Counter(&cset, "D_1", "groupd", 1001, "d counter", Counter::COUNT_NORMAL)),
        //        std::move(Counter(&cset, "D_2", "groupd", 1002, "d counter", Counter::COUNT_NORMAL)),
        //        std::move(Counter(&cset, "D_3", "groupd", 1003, "d counter", Counter::COUNT_NORMAL))
        //};

        // Ensure counters can be added to vectors (no reallocation going on)
        std::vector<Counter> small_ctr_vec;
        std::stringstream name;
        small_ctr_vec.emplace_back(&cset, "A_1", "groupa", 1001, "A counter", Counter::COUNT_NORMAL);

        // Ensure counters can be added to vectors (with reallocation and moving)
        std::vector<Counter> ctr_vec_reserved;
        ctr_vec_reserved.reserve(4);
        for(auto i : {1,2,3,4}){
            std::stringstream name;
            name << "B_" << i;
            ctr_vec_reserved.emplace_back(&cset, name.str(), "groupb", 1000+i, "B counter", Counter::COUNT_NORMAL);
        }

        //EXPECT_THROW((ctr_vec_reserved[0] = {&cset, "B_1", "groupb", 1001, "B counter", Counter::COUNT_NORMAL}));
        //ctr_vec_reserved[0] = std::move(Counter(&cset, "B_5", "groupb", 1005, "B counter", Counter::COUNT_NORMAL));

        // Ensure counters can be added to vectors (with reallocation and moving)
        std::vector<Counter> ctr_vec;
        for(auto i : {1,2,3,4,5,6,7,8,9}){
            std::stringstream name;
            name << "C_" << i;
            ctr_vec.emplace_back(&cset, name.str(), "groupc", 1000+i, "C counter", Counter::COUNT_NORMAL);
            ctr_vec.back() += i;
            std::cout << "The tree after " << name.str() << " at " << i << std::endl
                      << cset.renderSubtree(-1, true) << std::endl;
        }

        std::vector<Counter> moved_ctr_vec(std::move(ctr_vec));
        EXPECT_EQUAL(moved_ctr_vec.size(), 9);
        EXPECT_EQUAL(ctr_vec.size(), 0);

        // Attempt to access moved counters
        EXPECT_EQUAL(moved_ctr_vec.at(2).getName(), "C_3");
        EXPECT_EQUAL(moved_ctr_vec.at(2).get(), 3);
        ++moved_ctr_vec.at(2);
        EXPECT_EQUAL(moved_ctr_vec.at(2).get(), 4);
        EXPECT_EQUAL(cset.getChildAs<Counter>("C_3")->get(), 4);
        EXPECT_EQUAL(cset.getCounter("C_3")->get(), 4);
        EXPECT_EQUAL(moved_ctr_vec.at(8).get(), 9);

        uint64_t e_val = 0;
        ReadOnlyCounter ctr_e(&cset, "E", "The E counter (read only)", Counter::COUNT_NORMAL, &e_val);
        CycleCounter  cyc1(&cset, "F", "The F counter (cycle counter)", Counter::COUNT_NORMAL, &clk);
        CycleCounter  cyc2(&cset, "G", "The G counter (cycle counter)", Counter::COUNT_NORMAL, &clk);
        CycleCounter  cyc3(&cset, "H", "The H counter (cycle counter)", Counter::COUNT_NORMAL, &clk);
        CycleCounter  cyc4(&cset, "I", "The I counter (integral cycle counter)", Counter::COUNT_INTEGRAL, &clk);
        CycleCounter  cyc5(&cset, "J", "The J counter (cycle counter)", Counter::COUNT_NORMAL, &clk);

        EXPECT_THROW(cset.addChild(&ctr_a)); // Counter already added

        // Print a counter before tree finalization
        EXPECT_NOTHROW(std::cout << cset.getCounterAs<Counter>("A") << std::endl;);
        EXPECT_NOTHROW(std::cout << cset.getCounterAs<Counter>("B") << std::endl;);


        // Procedural addition of aliases to counter (NOT ALLOWED)
        EXPECT_TRUE(cset.getCounterAs<Counter>("A")->getParent());
        EXPECT_THROW(cset.getCounterAs<Counter>("A")->addAlias("alias_name_that_shouldnt_exist")); // Already has a parent node; Cannot add aliases


        // Jump through the phases for now. Other tests adequately test the tree-building phases.
        root.enterConfiguring();
        std::cout << "\nCONFIGURING" << std::endl;

        root.enterFinalized();
        EXPECT_TRUE(root.isFinalized());
        sched.finalize();
        std::cout << "\nFINALIZED" << std::endl;


        // Child Counter lookup:
        // by name
        Counter *a=0, *b=0, *c=0, *d=0;
        ReadOnlyCounter *e=0;
        CycleCounter *f=0, *g=0, *h=0, *i=0;
        EXPECT_NOTHROW(a = cset.getCounterAs<Counter>("A"));
        EXPECT_NOTEQUAL(a, nullptr);
        EXPECT_NOTHROW(b = cset.getCounterAs<Counter>("B"));
        EXPECT_NOTEQUAL(b, nullptr);
        EXPECT_NOTHROW(c = cset.getCounterAs<Counter>("C"));
        EXPECT_NOTEQUAL(c, nullptr);
        EXPECT_NOTHROW(d = cset.getCounterAs<Counter>("D"));
        EXPECT_NOTEQUAL(d, nullptr);
        EXPECT_THROW(cset.getCounterAs<ReadOnlyCounter>("D")); // D is not a RO counter
        EXPECT_NOTHROW(e = cset.getCounterAs<ReadOnlyCounter>("E"));
        EXPECT_NOTEQUAL(e, nullptr);
        EXPECT_NOTHROW(f = cset.getCounterAs<CycleCounter>("F"));
        EXPECT_NOTEQUAL(f, nullptr);
        EXPECT_NOTHROW(g = cset.getCounterAs<CycleCounter>("G"));
        EXPECT_NOTEQUAL(g, nullptr);
        EXPECT_NOTHROW(h = cset.getCounterAs<CycleCounter>("H"));
        EXPECT_NOTEQUAL(h, nullptr);
        EXPECT_NOTHROW(i = cset.getCounterAs<CycleCounter>("I"));
        EXPECT_NOTEQUAL(i, nullptr);
        EXPECT_THROW(cset.getCounterAs<Counter>("E")); // E is not a sparta::counter counter
        EXPECT_THROW(cset.getCounterAs<Counter>("there_is_no_counter_by_this_name_here_or_anywhere")); // No counter by this name

        // Advance simulation time for the CycleCounters
        cyc1.startCounting();
        cyc3.startCounting();
        cyc4.startCountingWithMultiplier(4);
        cyc5.startCounting();
        sched.run(1);
        cyc5.stopCounting();
        sched.run(9);
        cyc1.stopCounting();

        cyc2.startCounting();
        cyc4.stopCounting();
        sched.run(15);
        cyc2.stopCounting();

        // Counter printing by pointer
        std::cout << "Counters: "
                  << a << " "
                  << b << " "
                  << c << " "
                  << d << " "
                  << e << " "
                  << f << " "
                  << g << " "
                  << h << " "
                  << i << " "
                  << std::endl;

        // Counter printing by value/reference
        if(a){
            std::cout << *a << std::endl;
        }
        if(b){
            std::cout << *b << std::endl;
        }
        if(e){
            std::cout << *e << std::endl;
        }
        if(f){
            std::cout << *f << std::endl;
        }
        if(g){
            std::cout << *g << std::endl;
        }
        if(h){
            std::cout << *h << std::endl;
        }
        if(i){
            std::cout << *i << std::endl;
        }


        // Printing
        //
        //! \todo register printing by group
        // TODO
        // by group + index
        // TODO
        // by name expression
        // TODO


        // Counter Reads
        EXPECT_EQUAL((uint64_t)*a, 0);
        EXPECT_EQUAL((uint64_t)*b, 0);
        EXPECT_EQUAL((uint64_t)*c, 0);
        EXPECT_EQUAL((uint64_t)*d, 0);
        EXPECT_EQUAL((uint64_t)*e, 0);

        EXPECT_EQUAL(a->get(), 0);
        EXPECT_EQUAL(b->get(), 0);
        EXPECT_EQUAL(c->get(), 0);
        EXPECT_EQUAL(d->get(), 0);
        EXPECT_EQUAL(e->get(), 0);

        EXPECT_EQUAL(cyc1.get(), 10);
        EXPECT_EQUAL(cyc2.get(), 15);
        EXPECT_EQUAL(cyc3.get(), 25);
        EXPECT_EQUAL(f->get(), 10);
        EXPECT_EQUAL(g->get(), 15);
        EXPECT_EQUAL(h->get(), 25);
        EXPECT_EQUAL(i->get(), 40);
        EXPECT_EQUAL(*f, 10u);
        EXPECT_EQUAL(*g, 15u);
        EXPECT_EQUAL(*h, 25u);

        // Counter comparison
        // (No lt/le/gt/ge today)
        EXPECT_TRUE(*a == 0);
        EXPECT_TRUE(*b == 0);
        EXPECT_TRUE(*c == 0);
        EXPECT_TRUE(*d == 0);
        EXPECT_TRUE(*e == 0);

        // Counter cast
        Counter::counter_type tmp=0;
        tmp = *a;
        EXPECT_EQUAL(tmp, 0);
        tmp = *b;
        EXPECT_EQUAL(tmp, 0);
        tmp = *c;
        EXPECT_EQUAL(tmp, 0);
        tmp = *d;
        EXPECT_EQUAL(tmp, 0);
        tmp = *e;
        EXPECT_EQUAL(tmp, 0);


        // Counter Writes
        EXPECT_THROW(a->set(100)); // Cannot set on COUNT_NORMAL
        EXPECT_THROW(b->set(100)); // Cannot set on COUNT_INTEGRAL
        EXPECT_NOTHROW(c->set(100));

        EXPECT_THROW(*a = 100); // Cannot set on COUNT_NORMAL
        EXPECT_THROW(*b = 100); // Cannot set on COUNT_INTEGRAL
        EXPECT_NOTHROW(*c = 100);

        EXPECT_THROW(*a = 100); // Cannot set on COUNT_NORMAL
        EXPECT_THROW(*b = 100); // Cannot set on COUNT_INTEGRAL
        EXPECT_NOTHROW(*c = 100);

        // Counter increments
        EXPECT_NOTHROW(a->increment(100));
        EXPECT_NOTHROW(b->increment(100));
        EXPECT_NOTHROW(c->increment(100));

        EXPECT_NOTHROW(*a += 100);
        EXPECT_NOTHROW(*b += 100);
        EXPECT_NOTHROW(*c += 100);

        EXPECT_NOTHROW(++*a);
        EXPECT_NOTHROW(++*b);
        EXPECT_NOTHROW(++*c);
        ++*d;
        // NO postincrement: EXPECT_EQUAL((*d)++, 1u);
        EXPECT_EQUAL(++*d, 2_u64);
        EXPECT_EQUAL(*e, 0_u64);
        ++e_val;


        // Counter Reads (validate)
        EXPECT_EQUAL((uint64_t)*a, 201);
        EXPECT_EQUAL((uint64_t)*b, 201);
        EXPECT_EQUAL((uint64_t)*c, 301);
        EXPECT_EQUAL((uint64_t)*d, 2);
        EXPECT_EQUAL((uint64_t)*e, 1);

        // Test rollover
        EXPECT_EQUAL((uint64_t)*a, 201);
        *a += (~(Counter::counter_type)0) - *a;
        EXPECT_EQUAL((uint64_t)*a, ~(Counter::counter_type)0);
        EXPECT_NOTHROW(*a += 11); // 0xffff...ffff -> 0 + 10 -> 10  Old code assumed 0 to not be included
        EXPECT_EQUAL((uint64_t)*a, 10); // look for a sane result nonetheless
        *a += (~(Counter::counter_type)0) - *a - 30;
        EXPECT_EQUAL((uint64_t)*a, (~(Counter::counter_type)0) - 30);
        EXPECT_NOTHROW(*a += 50); // overflow!
        EXPECT_EQUAL((uint64_t)*a, 19); // look for a sane result nonetheless

        boost::timer::cpu_timer t;
        t.start();
        //performance test
        const uint64_t outer = 500;
        const uint64_t inner = 10000000;
        for(uint32_t i =0; i < outer; i++)
        {
            for(uint32_t t = 0; t < inner; ++t)
            {
                ++*b;
            }
        }
        t.stop();
        EXPECT_EQUAL((uint64_t)*b, 201 + (outer * inner));

        // Render Tree

        std::cout << "The tree from the top with builtins: " << std::endl << root.renderSubtree(-1, true) << std::endl;
        std::cout << "The tree from the top without builtins: " << std::endl << root.renderSubtree() << std::endl;
        std::cout << "The tree from statisticset: " << std::endl << cset.renderSubtree(-1, true);
        std::cout << "The tree from a: " << std::endl << a->renderSubtree(-1, true);
        std::cout << "The tree from b: " << std::endl << b->renderSubtree(-1, true);
        std::cout << "The performance (sec) is: " << t.elapsed().user/1000000000.0 << std::endl;

        root.enterTeardown();
    }

    // Show remaining nodes
    std::cout << sparta::TreeNode::formatAllNodes() << std::endl;

    // Done

    REPORT_ERROR;

    return ERROR_CODE;
}
