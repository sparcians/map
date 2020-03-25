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

struct dummy_struct
{
    uint16_t int16_field;
    uint32_t int32_field;
    std::string s_field;

    dummy_struct() = default;
    dummy_struct(const uint16_t int16_field, const uint32_t int32_field, const std::string &s_field) : 
        int16_field{int16_field},
        int32_field{int32_field},
        s_field{s_field} {}
};
std::ostream &operator<<(std::ostream &os, const dummy_struct &obj)
{
    os << obj.int16_field << " " << obj.int32_field << obj.s_field << "\n";
    return os;
}

typedef sparta::Array<uint32_t, sparta::ArrayType::NORMAL> MyArray;
typedef sparta::Array<uint32_t, sparta::ArrayType::AGED> AgedArray;
typedef sparta::FrontArray<uint32_t, sparta::ArrayType::NORMAL> FrontArray;
typedef sparta::Array<dummy_struct*, sparta::ArrayType::NORMAL> DummyArray;
typedef sparta::Array<dummy_struct, sparta::ArrayType::NORMAL> DummyMoveArray;
typedef sparta::FrontArray<dummy_struct, sparta::ArrayType::NORMAL> FrontMoveArray;

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
    FrontArray front_array("front_array", 8, &clk, &sset);
    
    DummyArray dummy_array("dummy_array", 3, &clk, &sset);

    // Make perfect forwarding arrays
    DummyMoveArray dummy_array_pf("dummy_array_pf", 4, &clk, &sset);
    FrontMoveArray front_array_pf("front_array_pf", 4, &clk, &sset);
    DummyMoveArray dummy_array_pfc("dummy_array_pfc", 4, &clk, &sset);
    FrontMoveArray front_array_pfc("front_array_pfc", 4, &clk, &sset);

    root_node.enterConfiguring();
    root_node.enterFinalized();

#ifdef PIPEOUT_GEN
    sparta::collection::PipelineCollector pc("test_collection_", 1000, &clk, &root);
#endif

    sched.finalize();

#ifdef PIPEOUT_GEN
    pc.startCollection(&root_node);
#endif
    
    // Test perfect forwarding arrays move
    {
        auto dummy_1 = dummy_struct(1, 2, "ABC");
        auto dummy_2 = dummy_struct(3, 4, "DEF");
        auto dummy_3 = dummy_struct(5, 6, "GHI");
        auto dummy_4 = dummy_struct(7, 8, "JKL");
        dummy_array_pf.write(0, std::move(dummy_1));
        dummy_array_pf.write(1, std::move(dummy_2));
        dummy_array_pf.write(2, std::move(dummy_3));
        dummy_array_pf.write(3, std::move(dummy_4));
        EXPECT_TRUE(dummy_1.s_field.size() == 0);
        EXPECT_TRUE(dummy_2.s_field.size() == 0);
        EXPECT_TRUE(dummy_3.s_field.size() == 0);
        EXPECT_TRUE(dummy_4.s_field.size() == 0);
        EXPECT_TRUE(dummy_array_pf.read(0).s_field == "ABC");
        EXPECT_TRUE(dummy_array_pf.read(1).s_field == "DEF");
        EXPECT_TRUE(dummy_array_pf.read(2).s_field == "GHI");
        EXPECT_TRUE(dummy_array_pf.read(3).s_field == "JKL");
        dummy_array_pf.clear();
        EXPECT_TRUE(dummy_array_pf.size() == 0);
        auto dummy_5 = dummy_struct(10, 20, "abc");
        auto dummy_6 = dummy_struct(30, 40, "def");
        auto dummy_7 = dummy_struct(50, 60, "ghi");
        auto dummy_8 = dummy_struct(70, 80, "jkl");
        auto itr = dummy_array_pf.begin();
        dummy_array_pf.write(itr++, std::move(dummy_5));
        dummy_array_pf.write(itr++, std::move(dummy_6));
        dummy_array_pf.write(itr++, std::move(dummy_7));
        dummy_array_pf.write(itr++, std::move(dummy_8));
        EXPECT_TRUE(dummy_5.s_field.size() == 0);
        EXPECT_TRUE(dummy_6.s_field.size() == 0);
        EXPECT_TRUE(dummy_7.s_field.size() == 0);
        EXPECT_TRUE(dummy_8.s_field.size() == 0);
        EXPECT_TRUE(dummy_array_pf.read(0).s_field == "abc");
        EXPECT_TRUE(dummy_array_pf.read(1).s_field == "def");
        EXPECT_TRUE(dummy_array_pf.read(2).s_field == "ghi");
        EXPECT_TRUE(dummy_array_pf.read(3).s_field == "jkl");
    }

    // Test perfect forwarding front arrays move
    {
        auto dummy_1 = dummy_struct(1, 2, "ABC");
        auto dummy_2 = dummy_struct(3, 4, "DEF");
        auto dummy_3 = dummy_struct(5, 6, "GHI");
        auto dummy_4 = dummy_struct(7, 8, "JKL");
        front_array_pf.write(0, std::move(dummy_1));
        front_array_pf.write(1, std::move(dummy_2));
        front_array_pf.write(2, std::move(dummy_3));
        front_array_pf.write(3, std::move(dummy_4));
        EXPECT_TRUE(dummy_1.s_field.size() == 0);
        EXPECT_TRUE(dummy_2.s_field.size() == 0);
        EXPECT_TRUE(dummy_3.s_field.size() == 0);
        EXPECT_TRUE(dummy_4.s_field.size() == 0);
        EXPECT_TRUE(front_array_pf.read(0).s_field == "ABC");
        EXPECT_TRUE(front_array_pf.read(1).s_field == "DEF");
        EXPECT_TRUE(front_array_pf.read(2).s_field == "GHI");
        EXPECT_TRUE(front_array_pf.read(3).s_field == "JKL");
        front_array_pf.erase(2);
        EXPECT_TRUE(front_array_pf.size() == 3);
        auto dummy_5 = dummy_struct(10, 20, "abc");
        front_array_pf.writeFront(std::move(dummy_5));
        EXPECT_TRUE(front_array_pf.read(2).s_field == "abc");
        front_array_pf.erase(3);
        auto dummy_6 = dummy_struct(30, 40, "def");
        front_array_pf.writeBack(std::move(dummy_6));
        EXPECT_TRUE(front_array_pf.read(3).s_field == "def");
        EXPECT_TRUE(dummy_5.s_field.size() == 0);
        EXPECT_TRUE(dummy_6.s_field.size() == 0);
    }

    // Test perfect forwarding arrays copy
    {
        auto dummy_1 = dummy_struct(1, 2, "ABC");
        auto dummy_2 = dummy_struct(3, 4, "DEF");
        auto dummy_3 = dummy_struct(5, 6, "GHI");
        auto dummy_4 = dummy_struct(7, 8, "JKL");
        dummy_array_pfc.write(0, dummy_1);
        dummy_array_pfc.write(1, dummy_2);
        dummy_array_pfc.write(2, dummy_3);
        dummy_array_pfc.write(3, dummy_4);
        EXPECT_TRUE(dummy_1.int16_field == 1);
        EXPECT_TRUE(dummy_1.int32_field == 2);
        EXPECT_TRUE(dummy_1.s_field == "ABC");
        EXPECT_TRUE(dummy_2.int16_field == 3);
        EXPECT_TRUE(dummy_2.int32_field == 4);
        EXPECT_TRUE(dummy_2.s_field == "DEF");
        EXPECT_TRUE(dummy_3.int16_field == 5);
        EXPECT_TRUE(dummy_3.int32_field == 6);
        EXPECT_TRUE(dummy_3.s_field == "GHI");
        EXPECT_TRUE(dummy_4.int16_field == 7);
        EXPECT_TRUE(dummy_4.int32_field == 8);
        EXPECT_TRUE(dummy_4.s_field == "JKL");
        EXPECT_TRUE(dummy_array_pfc.read(0).int16_field == 1);
        EXPECT_TRUE(dummy_array_pfc.read(0).int32_field == 2);
        EXPECT_TRUE(dummy_array_pfc.read(0).s_field == "ABC");
        EXPECT_TRUE(dummy_array_pfc.read(1).int16_field == 3);
        EXPECT_TRUE(dummy_array_pfc.read(1).int32_field == 4);
        EXPECT_TRUE(dummy_array_pfc.read(1).s_field == "DEF");
        EXPECT_TRUE(dummy_array_pfc.read(2).int16_field == 5);
        EXPECT_TRUE(dummy_array_pfc.read(2).int32_field == 6);
        EXPECT_TRUE(dummy_array_pfc.read(2).s_field == "GHI");
        EXPECT_TRUE(dummy_array_pfc.read(3).int16_field == 7);
        EXPECT_TRUE(dummy_array_pfc.read(3).int32_field == 8);
        EXPECT_TRUE(dummy_array_pfc.read(3).s_field == "JKL");
        dummy_array_pfc.clear();
        EXPECT_TRUE(dummy_array_pfc.size() == 0);
        auto dummy_5 = dummy_struct(10, 20, "abc");
        auto dummy_6 = dummy_struct(30, 40, "def");
        auto dummy_7 = dummy_struct(50, 60, "ghi");
        auto dummy_8 = dummy_struct(70, 80, "jkl");
        auto itr = dummy_array_pfc.begin();
        dummy_array_pfc.write(itr++, dummy_5);
        dummy_array_pfc.write(itr++, dummy_6);
        dummy_array_pfc.write(itr++, dummy_7);
        dummy_array_pfc.write(itr++, dummy_8);
        EXPECT_TRUE(dummy_5.int16_field == 10);
        EXPECT_TRUE(dummy_5.int32_field == 20);
        EXPECT_TRUE(dummy_5.s_field == "abc");
        EXPECT_TRUE(dummy_6.int16_field == 30);
        EXPECT_TRUE(dummy_6.int32_field == 40);
        EXPECT_TRUE(dummy_6.s_field == "def");
        EXPECT_TRUE(dummy_7.int16_field == 50);
        EXPECT_TRUE(dummy_7.int32_field == 60);
        EXPECT_TRUE(dummy_7.s_field == "ghi");
        EXPECT_TRUE(dummy_8.int16_field == 70);
        EXPECT_TRUE(dummy_8.int32_field == 80);
        EXPECT_TRUE(dummy_8.s_field == "jkl");
        EXPECT_TRUE(dummy_array_pfc.read(0).int16_field == 10);
        EXPECT_TRUE(dummy_array_pfc.read(0).int32_field == 20);
        EXPECT_TRUE(dummy_array_pfc.read(0).s_field == "abc");
        EXPECT_TRUE(dummy_array_pfc.read(1).int16_field == 30);
        EXPECT_TRUE(dummy_array_pfc.read(1).int32_field == 40);
        EXPECT_TRUE(dummy_array_pfc.read(1).s_field == "def");
        EXPECT_TRUE(dummy_array_pfc.read(2).int16_field == 50);
        EXPECT_TRUE(dummy_array_pfc.read(2).int32_field == 60);
        EXPECT_TRUE(dummy_array_pfc.read(2).s_field == "ghi");
        EXPECT_TRUE(dummy_array_pfc.read(3).int16_field == 70);
        EXPECT_TRUE(dummy_array_pfc.read(3).int32_field == 80);
        EXPECT_TRUE(dummy_array_pfc.read(3).s_field == "jkl");
    }

    // Test perfect forwarding front arrays copy
    {
        auto dummy_1 = dummy_struct(1, 2, "ABC");
        auto dummy_2 = dummy_struct(3, 4, "DEF");
        auto dummy_3 = dummy_struct(5, 6, "GHI");
        auto dummy_4 = dummy_struct(7, 8, "JKL");
        front_array_pfc.write(0, dummy_1);
        front_array_pfc.write(1, dummy_2);
        front_array_pfc.write(2, dummy_3);
        front_array_pfc.write(3, dummy_4);
        EXPECT_TRUE(dummy_1.int16_field == 1);
        EXPECT_TRUE(dummy_1.int32_field == 2);
        EXPECT_TRUE(dummy_1.s_field == "ABC");
        EXPECT_TRUE(dummy_2.int16_field == 3);
        EXPECT_TRUE(dummy_2.int32_field == 4);
        EXPECT_TRUE(dummy_2.s_field == "DEF");
        EXPECT_TRUE(dummy_3.int16_field == 5);
        EXPECT_TRUE(dummy_3.int32_field == 6);
        EXPECT_TRUE(dummy_3.s_field == "GHI");
        EXPECT_TRUE(dummy_4.int16_field == 7);
        EXPECT_TRUE(dummy_4.int32_field == 8);
        EXPECT_TRUE(dummy_4.s_field == "JKL");
        EXPECT_TRUE(front_array_pfc.read(0).int16_field == 1);
        EXPECT_TRUE(front_array_pfc.read(0).int32_field == 2);
        EXPECT_TRUE(front_array_pfc.read(0).s_field == "ABC");
        EXPECT_TRUE(front_array_pfc.read(1).int16_field == 3);
        EXPECT_TRUE(front_array_pfc.read(1).int32_field == 4);
        EXPECT_TRUE(front_array_pfc.read(1).s_field == "DEF");
        EXPECT_TRUE(front_array_pfc.read(2).int16_field == 5);
        EXPECT_TRUE(front_array_pfc.read(2).int32_field == 6);
        EXPECT_TRUE(front_array_pfc.read(2).s_field == "GHI");
        EXPECT_TRUE(front_array_pfc.read(3).int16_field == 7);
        EXPECT_TRUE(front_array_pfc.read(3).int32_field == 8);
        EXPECT_TRUE(front_array_pfc.read(3).s_field == "JKL");
        front_array_pfc.erase(2);
        EXPECT_TRUE(front_array_pfc.size() == 3);
        auto dummy_5 = dummy_struct(10, 20, "abc");
        front_array_pfc.writeFront(dummy_5);
        EXPECT_TRUE(dummy_5.s_field == "abc");
        EXPECT_TRUE(front_array_pfc.read(2).s_field == "abc");
        front_array_pfc.erase(3);
        auto dummy_6 = dummy_struct(30, 40, "def");
        front_array_pfc.writeBack(dummy_6);
        EXPECT_TRUE(dummy_6.s_field == "def");
        EXPECT_TRUE(front_array_pfc.read(3).s_field == "def");
    }

    dummy_array.write(0, new dummy_struct{16, 314, "dummy struct 1"});
    EXPECT_TRUE(dummy_array.size() == 1);
    dummy_array.write(1, new dummy_struct{32, 123, "dummy struct 2"});
    EXPECT_TRUE(dummy_array.size() == 2);
    dummy_array.write(2, new dummy_struct{64, 109934, "dummy struct 3"});
    EXPECT_TRUE(dummy_array.size() == 3);
    
    // Test pointer to member operator
    EXPECT_TRUE(dummy_array.read(0)->int16_field == 16);
    EXPECT_TRUE(dummy_array.read(1)->int16_field == 32);
    EXPECT_TRUE(dummy_array.read(2)->int16_field == 64);
    EXPECT_TRUE(dummy_array.read(0)->int32_field == 314);
    EXPECT_TRUE(dummy_array.read(1)->int32_field == 123);
    EXPECT_TRUE(dummy_array.read(2)->int32_field == 109934);
    EXPECT_TRUE(dummy_array.read(0)->s_field == "dummy struct 1");
    EXPECT_TRUE(dummy_array.read(1)->s_field == "dummy struct 2");
    EXPECT_TRUE(dummy_array.read(2)->s_field == "dummy struct 3");
    
    // Test dereference operator
    EXPECT_TRUE((*(dummy_array.read(0))).int16_field == 16);
    EXPECT_TRUE((*(dummy_array.read(1))).int16_field == 32);
    EXPECT_TRUE((*(dummy_array.read(2))).int16_field == 64);
    EXPECT_TRUE((*(dummy_array.read(0))).int32_field == 314);
    EXPECT_TRUE((*(dummy_array.read(1))).int32_field == 123);
    EXPECT_TRUE((*(dummy_array.read(2))).int32_field == 109934);
    EXPECT_TRUE((*(dummy_array.read(0))).s_field == "dummy struct 1");
    EXPECT_TRUE((*(dummy_array.read(1))).s_field == "dummy struct 2");
    EXPECT_TRUE((*(dummy_array.read(2))).s_field == "dummy struct 3");
    
    delete dummy_array.read(0);
    delete dummy_array.read(1);
    delete dummy_array.read(2);

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
    uint32_t* dat = it.operator->();
    EXPECT_EQUAL(*dat, 9);
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
