// <Array_test> -*- C++ -*-

/**
 * \file Array_test
 * \brief A test that creates a producer and consumer, and
 * then runs some test cases on both a normal type array and
 * an aged array.
 */

#define ARGOS_TESTING = 1;
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/sparta.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/report/Report.hpp"

#include "sparta/utils/SpartaTester.hpp"
#include "sparta/resources/Array.hpp"
#include "sparta/resources/FrontArray.hpp"
#include "sparta/simulation/ClockManager.hpp"

#include "sparta/collection/PipelineCollector.hpp"

#include <string>

TEST_INIT;

#define PIPEOUT_GEN

typedef sparta::Array<uint32_t, sparta::ArrayType::NORMAL> MyArray;
typedef sparta::Array<uint32_t, sparta::ArrayType::AGED> AgedArray;
typedef sparta::FrontArray<uint32_t, sparta::ArrayType::NORMAL> RajArray;

//Test non-integral aged array data types
namespace sparta {
    struct SchedulerAccess {
        SchedulerAccess(const int val) : val(val) {}
        const int val;
    };
};

void testStatsOutput()
{
    sparta::Scheduler sched;
    sparta::RootTreeNode rtn;
    sparta::ClockManager cm(&sched);
    sparta::Clock::Handle root_clk;
    root_clk = cm.makeRoot(&rtn, "root_clk");
    rtn.setClock(root_clk.get());
    cm.normalize();
    sparta::Report r1("report 1", &rtn);

    sparta::StatisticSet stats(&rtn);
    MyArray b("buf_const_test", 10, root_clk.get(), &stats);
    const std::string report_def =
        R"(name: "String-based report Autopopulation Test"
style:
    decimal_places: 3
content:
    top:
        subreport:
            name: All stats
            style:
                collapsible_children: no
            content:
                autopopulate:
                    attributes: "!=vis:hidden && !=vis:summary"
                    max_report_depth: 1
        subreport:
            name: Hidden stats
            style:
                collapsible_children: no
            content:
                autopopulate:
                    attributes: "==vis:hidden"
                    max_report_depth: 1
        )";

    r1.setContext(rtn.getSearchScope());
    r1.addDefinitionString(report_def);

    rtn.enterConfiguring();
    rtn.enterFinalized();

    std::cout << r1 << std::endl;

    rtn.enterTeardown();
}

int main()
{
    sparta::Scheduler sched;
    sparta::Clock clk("clock", &sched);
    //Create a dummy treenode.
    sparta::RootTreeNode root_node("root");
    root_node.setClock(&clk);

    sparta::TreeNode root("root", "root tree node");
    root_node.addChild(&root);

    sparta::StatisticSet sset(&root);
    sset.setClock(&clk);
    AgedArray aged_array("aged_array", 10, &clk, &sset);
    aged_array.enableCollection(&root);

    AgedArray aged_collected_array("aged_collected_array", 10, &clk);
    aged_collected_array.enableCollection(&root);
    RajArray front_array("front_array", 8, &clk, &sset);

    root_node.enterConfiguring();
    root_node.enterFinalized();

#ifdef PIPEOUT_GEN
    sparta::collection::PipelineCollector pc("test_collection_", 1000, &clk, &root);
#endif

    sched.finalize();

#ifdef PIPEOUT_GEN
    pc.startCollection(&root_node);
#endif

    std::cout << sset << std::endl;

    EXPECT_EQUAL(aged_array.numFree(), 10);

    // Test iteration on an empty Array
    AgedArray::const_iterator bit = aged_array.abegin();
    AgedArray::const_iterator eit = aged_array.aend();
    uint32_t cnt = 0;
    while(bit != eit) {
        ++cnt;
    }
    EXPECT_EQUAL(cnt, 0);

    bit = aged_array.begin();
    eit = aged_array.end();
    cnt = 0;
    while(bit != eit) {
        EXPECT_FALSE(bit.isValid());
        ++bit;
        ++cnt;
    }
    EXPECT_EQUAL(cnt, 10);

    for(uint32_t i = 0; i < 10; ++i)
    {
        aged_array.write(i, i);
        aged_collected_array.write(i,i);
    }

    aged_array.erase(5);

    // Make index 5 the youngest index.
    aged_array.write(5, 5);

    std::vector<uint32_t> expected_vector {0, 1, 2, 3, 4, 6, 7, 8, 9, 5};
    uint32_t idx = 0;

    // Iterates in order of age, oldest to youngest
    for(auto it = aged_array.abegin(); it != aged_array.aend(); ++it) {
        EXPECT_EQUAL(*it, expected_vector[idx++]);
    }


    AgedArray aged_array_test("aged_array_test", 5, &clk);
    aged_array_test.write(4, 12);
    aged_array_test.write(1, 21);
    aged_array_test.write(3, 90);
    aged_array_test.write(0, 92);
    aged_array_test.write(2, 3);
    uint32_t idxx = 0;
    std::vector<uint32_t> result_vec {12, 21, 90, 92, 3};
    for(auto it = aged_array_test.abegin(); it != aged_array_test.aend(); ++it){
        EXPECT_EQUAL(*it, result_vec[idxx++]);
    }
    uint32_t test_index = 4;
    EXPECT_TRUE(aged_array_test.getNextOldestIndex(test_index));
    EXPECT_EQUAL(test_index, 1);
    EXPECT_TRUE(aged_array_test.getNextOldestIndex(test_index));
    EXPECT_EQUAL(test_index, 3);
    EXPECT_TRUE(aged_array_test.getNextOldestIndex(test_index));
    EXPECT_EQUAL(test_index, 0);
    EXPECT_TRUE(aged_array_test.getNextOldestIndex(test_index));
    EXPECT_EQUAL(test_index, 2);
    EXPECT_FALSE(aged_array_test.getNextOldestIndex(test_index));

#ifdef PIPEOUT_GEN
    sched.run(1);
#endif

    aged_collected_array.erase(0); //+9 records.
    aged_collected_array.erase(1); //+8 records.
    aged_collected_array.write(0, 0);

#ifdef PIPEOUT_GEN
    sched.run(1);
#endif

    EXPECT_EQUAL(aged_array.abegin().getIndex(), aged_array.getOldestIndex().getIndex());
    bit = aged_array.abegin();
    eit = aged_array.aend();
    cnt = 0;
    while(bit != eit) {
        // In the aged array, each iterator SHOULD point to a valid
        // entry
        EXPECT_TRUE(bit.isValid());
        std::cout << "AA: " << *bit << std::endl;
        ++bit;
        ++cnt;
    }
    EXPECT_EQUAL(cnt, 10);

    EXPECT_EQUAL(aged_array.numFree(), 0);
    EXPECT_EQUAL(aged_array.numValid(), 10);

    EXPECT_EQUAL(aged_array.getYoungestIndex().getIndex(), 5);
    EXPECT_EQUAL(aged_array.getOldestIndex().getIndex(), 0);

    aged_array.erase(4);
    aged_array.erase(2);
    aged_array.erase(1);

    bit = aged_array.abegin();
    eit = aged_array.aend();
    cnt = 0;
    while(bit != eit) {
        // In the aged array, each iterator SHOULD point to a valid
        // entry
        EXPECT_TRUE(bit.isValid());
        std::cout << "AA: " << *bit << std::endl;
        ++bit;
        ++cnt;
    }
    EXPECT_EQUAL(cnt, 7);

#ifdef PIPEOUT_GEN
    sched.run(1);
#endif

    aged_array.write(4, 4);
    aged_array.write(2, 2);
    aged_array.write(1, 1);

#ifdef PIPEOUT_GEN
    sched.run(1);
#endif

   EXPECT_THROW(aged_array.write(0,0));

    AgedArray::iterator it = aged_array.getCircularIterator();
    while(it != aged_array.getCircularIterator(aged_array.capacity() - 1))
    {
        aged_array.erase(it);
        ++it;
    }
    //this dereference should work if the -> operator was set up properly.
    uint32_t dat = it.operator->();
    EXPECT_EQUAL(dat, 9);
    aged_array.erase(it);

#ifdef PIPEOUT_GEN
    sched.run(1);
#endif

    //set up the pipeline collection.

    // Run some tests on a non timed standard sparta array.
    MyArray ns_array("untimed_array", 10, &clk);

    ns_array.write(0, 0);
    ns_array.write(1, 1);
    ns_array.write(2, 2);

#ifdef PIPEOUT_GEN
    sched.run(1);
#endif

    //ns_array.getOldestIndex(0); THROWS a static assert message since
    //ns_array is not aged.
    EXPECT_EQUAL(ns_array.read(0), 0);
    EXPECT_EQUAL(ns_array.read(1), 1);
    ns_array.erase(0);
    EXPECT_THROW(ns_array.read(0));
    ns_array.erase(2);
    EXPECT_THROW(ns_array.read(2));
    EXPECT_EQUAL(ns_array.read(1), 1);
    EXPECT_EQUAL(ns_array.numValid(), 1);
    EXPECT_EQUAL(ns_array.capacity(), 10);

#ifdef PIPEOUT_GEN
    sched.run(1);
#endif


    ns_array.write(5, 5);
    ns_array.write(3, 3);
    ns_array.write(0, 0);

#ifdef PIPEOUT_GEN
    sched.run(1);
#endif

    MyArray::iterator iter = ns_array.begin();
    uint32_t i = 0;
    while(iter != ns_array.end())
    {
        std::cout << "idx: " << i << " valid: " << iter.isValid() << std::endl;
        EXPECT_EQUAL(i, iter.getIndex());
        if(iter.isValid())
        {
            std::cout << "value at: " << i << ": " << *iter << std::endl;
        }
        ++iter;
        ++i;
    }

#ifdef PIPEOUT_GEN
    sched.run(1);
#endif
    iter = ns_array.begin();
    std::advance(iter, 3);

    EXPECT_EQUAL(3, *iter);
    ++iter;
    ns_array.write(iter, 4);
    EXPECT_EQUAL(ns_array.read(4), 4);

    MyArray::iterator old = ns_array.getCircularIterator(5);
    MyArray::iterator young = ns_array.getCircularIterator(4);

    EXPECT_TRUE(old.isOlder(young));
    EXPECT_TRUE(old.isOlder(young.getIndex()));

    EXPECT_TRUE(young.isYounger(old));
    EXPECT_TRUE(young.isYounger(old.getIndex()));

    MyArray::iterator invalid_it = ns_array.getUnitializedIterator();
    EXPECT_THROW(++invalid_it);
    //test some FrontArray usage.
    //Make sure counters work


    for(uint32_t i = 0; i < 8; ++ i)
    {
        front_array.writeFront(i);
    }
    for(uint32_t i = 0; i < 8; ++i)
    {
        EXPECT_EQUAL(front_array.read(i), i);
    }

    front_array.erase(4);
    front_array.writeFront(50);
    EXPECT_EQUAL(front_array.read(4), 50);

    EXPECT_EQUAL(front_array.readValid(0), 0);

    ////////////////////////////////////////////////////////////
    // Test clearing
    ns_array.clear();
    EXPECT_TRUE(ns_array.size() == 0);
    ns_array.write(0, 0);
    ns_array.write(1, 1);
    ns_array.write(2, 2);
    EXPECT_TRUE(ns_array.size() == 3);
    ns_array.clear();
    EXPECT_TRUE(ns_array.size() == 0);

    aged_array.clear();
    EXPECT_TRUE(aged_array.size() == 0);
    aged_array.write(0, 0);
    aged_array.write(1, 1);
    aged_array.write(2, 2);
    EXPECT_TRUE(aged_array.size() == 3);
    auto oldest_it = aged_array.getOldestIndex();
    EXPECT_TRUE(oldest_it.isValid());
    aged_array.clear();
    EXPECT_TRUE(aged_array.size() == 0);
    EXPECT_FALSE(oldest_it.isValid());

    //Test non-integer data types
    sparta::Array<std::shared_ptr<sparta::SchedulerAccess>,
                sparta::ArrayType::AGED> sched_access("access", 3, &clk);
    sched_access.write(1, std::shared_ptr<sparta::SchedulerAccess>(new sparta::SchedulerAccess(9)));
    sched_access.write(0, std::shared_ptr<sparta::SchedulerAccess>(new sparta::SchedulerAccess(5)));
    sched_access.write(2, std::shared_ptr<sparta::SchedulerAccess>(new sparta::SchedulerAccess(7)));
    const std::vector<int> sched_access_expected_vals = {9, 5, 7};
    auto sched_access_expected_iter = sched_access_expected_vals.begin();
    auto sched_access_iter = sched_access.abegin();
    while (sched_access_iter != sched_access.aend()) {
        EXPECT_EQUAL((*sched_access_iter)->val, *sched_access_expected_iter);
        ++sched_access_iter;
        ++sched_access_expected_iter;
    }

    //try normal iteration
    //this below is never going to work since array iterates wrap around
    //to the front. Sorry...
//     iter = ns_array.begin();
//     while(iter != ns_array.end())
//     {
//         std::cout << "valid: " << iter.isValid() << std::endl;
//     }

#ifdef PIPEOUT_GEN
    sched.run(10);
    pc.destroy();
#endif

    //it's now safe to tear down our dummy tree
    root_node.enterTeardown();

    testStatsOutput();

    ENSURE_ALL_REACHED(0);
    REPORT_ERROR;
    return ERROR_CODE;
}
