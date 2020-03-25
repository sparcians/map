// -*- C++ -*-


#include <iostream>
#include <inttypes.h>
#include <boost/timer/timer.hpp>
#include <vector>

#include "sparta/resources/CircularBuffer.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/report/Report.hpp"

TEST_INIT;

//#define PIPEOUT_GEN
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


void testPushBack()
{
    sparta::Scheduler sched;
    sparta::ClockManager  cm(&sched);
    sparta::RootTreeNode  rtn;
    sparta::Clock::Handle root_clk;
    sparta::StatisticSet  buf10_stats(&rtn);
    root_clk = cm.makeRoot(&rtn, "root_clk");
    cm.normalize();
    sparta::CircularBuffer<uint32_t> cir_buffer("test_circ_buffer", 10,
                                              root_clk.get(), &buf10_stats);

    sparta::CircularBuffer<dummy_struct> buf_dummy("test_circ_buffer_pf", 4,
                                              root_clk.get(), &buf10_stats);
    sparta::CircularBuffer<dummy_struct> buf_dummy_cp("test_circ_buffer_pfc", 4,
                                              root_clk.get(), &buf10_stats);

    // Test perfect forwarding Circular Buffer move
    {
        auto dummy_1 = dummy_struct(1, 2, "ABC");
        auto dummy_2 = dummy_struct(3, 4, "DEF");
        auto dummy_3 = dummy_struct(5, 6, "GHI");
        auto dummy_4 = dummy_struct(7, 8, "JKL");
        buf_dummy.push_back(std::move(dummy_1));
        EXPECT_TRUE(dummy_1.s_field.size() == 0);
        EXPECT_TRUE(buf_dummy[0].s_field == "ABC");
        auto itr = buf_dummy.begin();
        buf_dummy.insert(itr, std::move(dummy_2));
        EXPECT_TRUE(dummy_2.s_field.size() == 0);
        EXPECT_TRUE(buf_dummy[0].s_field == "DEF");
        itr = buf_dummy.begin();
        buf_dummy.insert(itr, std::move(dummy_3));
        EXPECT_TRUE(dummy_3.s_field.size() == 0);
        EXPECT_TRUE(buf_dummy[0].s_field == "GHI");
        itr = buf_dummy.begin();
        buf_dummy.insert(itr, std::move(dummy_4));
        EXPECT_TRUE(dummy_4.s_field.size() == 0);
        EXPECT_TRUE(buf_dummy[0].s_field == "JKL");
    }

    // Test perfect forwarding Circular Buffer copy
    {
        auto dummy_1 = dummy_struct(1, 2, "ABC");
        auto dummy_2 = dummy_struct(3, 4, "DEF");
        auto dummy_3 = dummy_struct(5, 6, "GHI");
        auto dummy_4 = dummy_struct(7, 8, "JKL");
        buf_dummy_cp.push_back(dummy_1);
        EXPECT_TRUE(dummy_1.int16_field == 1);
        EXPECT_TRUE(dummy_1.int32_field == 2);
        EXPECT_TRUE(dummy_1.s_field == "ABC");
        EXPECT_TRUE(buf_dummy_cp[0].int16_field == 1);
        EXPECT_TRUE(buf_dummy_cp[0].int32_field == 2);
        EXPECT_TRUE(buf_dummy_cp[0].s_field == "ABC");
        auto itr = buf_dummy_cp.begin();
        buf_dummy_cp.insert(itr, dummy_2);
        EXPECT_TRUE(dummy_2.int16_field == 3);
        EXPECT_TRUE(dummy_2.int32_field == 4);
        EXPECT_TRUE(dummy_2.s_field == "DEF");
        EXPECT_TRUE(buf_dummy_cp[0].int16_field == 3);
        EXPECT_TRUE(buf_dummy_cp[0].int32_field == 4);
        EXPECT_TRUE(buf_dummy_cp[0].s_field == "DEF");
        itr = buf_dummy_cp.begin();
        buf_dummy_cp.insert(itr, dummy_3);
        EXPECT_TRUE(dummy_3.int16_field == 5);
        EXPECT_TRUE(dummy_3.int32_field == 6);
        EXPECT_TRUE(dummy_3.s_field == "GHI");
        EXPECT_TRUE(buf_dummy_cp[0].int16_field == 5);
        EXPECT_TRUE(buf_dummy_cp[0].int32_field == 6);
        EXPECT_TRUE(buf_dummy_cp[0].s_field == "GHI");
        itr = buf_dummy_cp.begin();
        buf_dummy_cp.insert(itr, dummy_4);
        EXPECT_TRUE(dummy_4.int16_field == 7);
        EXPECT_TRUE(dummy_4.int32_field == 8);
        EXPECT_TRUE(dummy_4.s_field == "JKL");
        EXPECT_TRUE(buf_dummy_cp[0].int16_field == 7);
        EXPECT_TRUE(buf_dummy_cp[0].int32_field == 8);
        EXPECT_TRUE(buf_dummy_cp[0].s_field == "JKL");
    }

    for(uint32_t i = 0; i < 5; ++i) {
        cir_buffer.push_back(i);
    }
    EXPECT_EQUAL(cir_buffer.size(), 5);

    for(uint32_t i = 0; i < 10; ++i) {
        cir_buffer.push_back(i);
    }
    EXPECT_EQUAL(cir_buffer.size(), 10);

    for(uint32_t i = 0; i < 10; ++i) {
        cir_buffer.push_back(i);
    }
    EXPECT_EQUAL(cir_buffer.size(), 10);

    rtn.enterTeardown();
}

void testForwardIterators()
{
    sparta::RootTreeNode  rtn;
    sparta::Scheduler sched;
    sparta::ClockManager  cm(&sched);
    sparta::Clock::Handle root_clk;
    sparta::StatisticSet  buf10_stats(&rtn);
    root_clk = cm.makeRoot(&rtn, "root_clk");
    cm.normalize();

    // Buffer setup
    const uint32_t BUF_SIZE = 10;
    sparta::CircularBuffer<uint32_t> cir_buffer("test_circ_buffer", BUF_SIZE,
                                              root_clk.get(), &buf10_stats);

    EXPECT_EQUAL(cir_buffer.capacity(), BUF_SIZE);

    for(uint32_t i = 0; i < BUF_SIZE/2; ++i) {
        cir_buffer.push_back(i);
    }
    EXPECT_EQUAL(cir_buffer.size(), BUF_SIZE/2);
    uint32_t num_valid = 0;
    for(sparta::CircularBuffer<uint32_t>::iterator it = cir_buffer.begin();
        it != cir_buffer.end();
        ++it)
    {
        EXPECT_TRUE(it.isValid());
        ++num_valid;
    }
    EXPECT_EQUAL(num_valid, BUF_SIZE/2);

    // Test clear
    cir_buffer.clear();
    EXPECT_EQUAL(cir_buffer.size(), 0);

    for(uint32_t i = 0; i < BUF_SIZE; ++i) {
        cir_buffer.push_back(i);
    }
    EXPECT_EQUAL(cir_buffer.size(), BUF_SIZE);

    // Test that we have BUF_SIZE valid iterators
    num_valid = 0;
    for(sparta::CircularBuffer<uint32_t>::iterator it = cir_buffer.begin();
        it != cir_buffer.end();
        ++it)
    {
        EXPECT_TRUE(it.isValid());
        ++num_valid;
    }
    EXPECT_EQUAL(num_valid, BUF_SIZE);

    // Test bad increments
    sparta::CircularBuffer<uint32_t>::iterator eit = cir_buffer.end();
    EXPECT_THROW(++eit);

    sparta::CircularBuffer<uint32_t>::iterator bad_it;
    EXPECT_FALSE(bad_it.isValid());
    EXPECT_THROW(++bad_it);

    // Grab the begin iterator from the valid buffer
    sparta::CircularBuffer<uint32_t>::iterator valid_bit = cir_buffer.begin();
    EXPECT_TRUE(valid_bit.isValid());
    EXPECT_EQUAL(*valid_bit, 0);

    sparta::CircularBuffer<uint32_t>::iterator next_valid_it = valid_bit;
    next_valid_it++;
    EXPECT_TRUE(next_valid_it.isValid());
    EXPECT_EQUAL(*next_valid_it, 1);

    // Add something to the Buffer -- the iterator should now be bad
    // -- the push back would have clobbered the old beginning
    cir_buffer.push_back(300);
    EXPECT_FALSE(valid_bit.isValid());
    EXPECT_THROW(*valid_bit);

    // The next iterator should still be valid
    EXPECT_TRUE(next_valid_it.isValid());
    EXPECT_EQUAL(*next_valid_it, 1);

    for(uint32_t i = 1; i < BUF_SIZE; ++i) {
        EXPECT_EQUAL(*next_valid_it, i);
        ++next_valid_it;
    }
    EXPECT_EQUAL(*next_valid_it, 300);
    ++next_valid_it;
    EXPECT_TRUE(next_valid_it == cir_buffer.end());

    // Try to change the values in the Circular buffer using the iterators
    valid_bit = cir_buffer.begin();
    for(uint32_t i = 0; i < BUF_SIZE; ++i) {
        *valid_bit = (i + 20);
        ++valid_bit;
    }
    uint32_t i = 0;
    for(sparta::CircularBuffer<uint32_t>::iterator it = cir_buffer.begin();
        it != cir_buffer.end(); ++it)
    {
        EXPECT_EQUAL(*it, (i + 20));
        ++i;
    }

    // Finally, attempt decrementing
    valid_bit = cir_buffer.begin();
    EXPECT_THROW(--valid_bit);
    EXPECT_THROW(valid_bit--);

    sparta::CircularBuffer<uint32_t>::iterator valid_eit = cir_buffer.end();
    EXPECT_NOTHROW(--valid_eit);
    EXPECT_EQUAL(*valid_eit, 29);
    EXPECT_NOTHROW(valid_eit--);
    EXPECT_EQUAL(*valid_eit, 28);


    // Really, really test clear
    EXPECT_TRUE(valid_eit.isValid());
    EXPECT_TRUE(valid_bit.isValid());
    cir_buffer.clear();
    EXPECT_FALSE(valid_eit.isValid());
    EXPECT_FALSE(valid_bit.isValid());
    EXPECT_EQUAL(cir_buffer.size(), 0);

    num_valid = 0;
    for(sparta::CircularBuffer<uint32_t>::iterator it = cir_buffer.begin();
        it != cir_buffer.end();
        ++it)
    {
        EXPECT_TRUE(it.isValid());
        ++num_valid;
    }
    EXPECT_EQUAL(num_valid, 0);

    EXPECT_TRUE(cir_buffer.begin() == cir_buffer.end());

    // Get a constant iterator and ensure we can do "non const" stuff with it
    cir_buffer.push_back(1);
    sparta::CircularBuffer<uint32_t>::const_iterator cit = cir_buffer.begin();
    std::cout << "The value: " << *cit << std::endl;
    ++cit;
    EXPECT_TRUE(cit == cir_buffer.end());
    EXPECT_THROW(*cit);

    // Finally, test a large push_back as this isn't supposed to fail
    // ever, nor is the size of the buffer supposed to change (it's
    // circular after all)
    for(uint32_t i = 0; i < BUF_SIZE * 10; ++i) {
        cir_buffer.push_back(i);
    }
    EXPECT_EQUAL(cir_buffer.size(), BUF_SIZE);

    num_valid = 0;
    for(sparta::CircularBuffer<uint32_t>::iterator it = cir_buffer.begin();
        it != cir_buffer.end();
        ++it)
    {
        EXPECT_TRUE(it.isValid());
        ++num_valid;
    }
    EXPECT_EQUAL(num_valid, BUF_SIZE);

    // Test lt/gt
    sparta::CircularBuffer<uint32_t>::const_iterator old   = cir_buffer.begin();
    sparta::CircularBuffer<uint32_t>::const_iterator young = cir_buffer.end();

    EXPECT_TRUE(old > young);
    --young;
    EXPECT_TRUE(old > young);
    --young;
    EXPECT_TRUE(old > young);
    --young;
    EXPECT_TRUE(old > young);

    // Test index
    cir_buffer.clear();
    for(uint32_t i = 0; i < BUF_SIZE; ++i) {
        cir_buffer.push_back(i);
    }

    for(uint32_t i = 0; i < cir_buffer.size(); ++i)
    {
        EXPECT_EQUAL(cir_buffer[i], i);
    }
    EXPECT_THROW(cir_buffer[BUF_SIZE]);


    // test c++11 range for loop
    i = 0;
    for(auto & dat : cir_buffer) {
        EXPECT_EQUAL(dat, i);
        ++i;
    }

#if SPARTA_GCC_VERSION > 40800
    // Test const iterators
    const auto & const_cir_buff = cir_buffer;
    i = 0;
    for(sparta::CircularBuffer<uint32_t>::const_iterator it = const_cir_buff.begin();
        it != const_cir_buff.end(); ++it)
    {
        EXPECT_EQUAL(*it, i);
        ++i;
    }
#endif

    rtn.enterTeardown();
}

void testReverseIterators()
{
    sparta::RootTreeNode  rtn;
    sparta::Scheduler sched;
    sparta::ClockManager  cm(&sched);
    sparta::Clock::Handle root_clk;
    sparta::StatisticSet  buf10_stats(&rtn);
    root_clk = cm.makeRoot(&rtn, "root_clk");
    cm.normalize();

    // Buffer setup
    const uint32_t BUF_SIZE = 10;
    sparta::CircularBuffer<uint32_t> cir_buffer("test_circ_buffer", BUF_SIZE,
                                              root_clk.get(), &buf10_stats);

    for(uint32_t i = 0; i < BUF_SIZE; ++i) {
        cir_buffer.push_back(i);
    }
    EXPECT_EQUAL(cir_buffer.size(), BUF_SIZE);

    uint32_t i = 9;
    for(sparta::CircularBuffer<uint32_t>::reverse_iterator it = cir_buffer.rbegin();
        it != cir_buffer.rend(); ++it)
    {
        EXPECT_EQUAL(*it, i);
        --i;
    }

    sparta::CircularBuffer<uint32_t>::reverse_iterator rit = cir_buffer.rbegin();
    EXPECT_THROW(rit--);
    EXPECT_THROW(--rit);

    rit = cir_buffer.rend();
    EXPECT_THROW(rit++);
    EXPECT_THROW(++rit);

    --rit;
    sparta::CircularBuffer<uint32_t>::iterator bit = cir_buffer.begin();
    EXPECT_EQUAL(*rit, *bit);

    rtn.enterTeardown();
}

void testEraseInsert()
{
    sparta::RootTreeNode  rtn;
    sparta::Scheduler sched;
    sparta::ClockManager  cm(&sched);
    sparta::Clock::Handle root_clk;
    sparta::StatisticSet  buf10_stats(&rtn);
    root_clk = cm.makeRoot(&rtn, "root_clk");
    cm.normalize();

    // Buffer setup
    const uint32_t BUF_SIZE = 10;
    sparta::CircularBuffer<uint32_t> cir_buffer("test_circ_buffer", BUF_SIZE,
                                              root_clk.get(), &buf10_stats);

    for(uint32_t i = 0; i < BUF_SIZE; ++i) {
        cir_buffer.push_back(i);
    }
    EXPECT_EQUAL(cir_buffer.size(), BUF_SIZE);

    // Test erase
    sparta::CircularBuffer<uint32_t>::iterator bit = cir_buffer.begin();
    cir_buffer.erase(bit);
    EXPECT_FALSE(bit.isValid());

    sparta::CircularBuffer<uint32_t>::reverse_iterator rbit = cir_buffer.rbegin();
    cir_buffer.erase(rbit);
    EXPECT_FALSE(rbit.isValid());
    EXPECT_THROW(--rbit);
    EXPECT_THROW(rbit--);

    sparta::CircularBuffer<uint32_t>::const_reverse_iterator rcbit = cir_buffer.rbegin();
    cir_buffer.erase(rcbit);
    EXPECT_FALSE(rcbit.isValid());
    EXPECT_THROW(--rcbit);
    EXPECT_THROW(rcbit--);

    cir_buffer.clear();
    EXPECT_EQUAL(cir_buffer.size(), 0);
    rcbit = cir_buffer.rbegin();
    rbit  = cir_buffer.rbegin();
    EXPECT_FALSE(rcbit.isValid());
    EXPECT_FALSE(rbit.isValid());

    // Test insert
    auto nit = cir_buffer.insert(cir_buffer.begin(), 1);
    EXPECT_EQUAL(cir_buffer.size(), 1);
    EXPECT_EQUAL(*(cir_buffer.begin()), 1);
    EXPECT_EQUAL(*nit, 1);

    nit = cir_buffer.insert(cir_buffer.begin(), 2);
    EXPECT_EQUAL(cir_buffer.size(), 2);
    EXPECT_EQUAL(*nit, 2);
    EXPECT_EQUAL(*(cir_buffer.rbegin()), 1);

    rtn.enterTeardown();
}

// This code works with GCC v4.9 or higher
#if SPARTA_GCC_VERSION > 40800
void testCollection()
{
    sparta::RootTreeNode  rtn;
    sparta::Scheduler sched;
    sparta::ClockManager  cm(&sched);
    sparta::Clock::Handle root_clk;
    sparta::StatisticSet  buf10_stats(&rtn);
    root_clk = cm.makeRoot(&rtn, "root_clk");
    rtn.setClock(root_clk.get());
    cm.normalize();

    // Buffer setup
    const uint32_t BUF_SIZE = 10;
    sparta::CircularBuffer<uint32_t> cir_buffer("test_circ_buffer", BUF_SIZE,
                                              root_clk.get(), &buf10_stats);
    cir_buffer.enableCollection(&rtn);

    rtn.enterConfiguring();
    rtn.enterFinalized();

    sparta::collection::PipelineCollector pc("testCircBuffer", 1000000,
                                           root_clk.get(), &rtn);

    sparta::Scheduler::getScheduler()->finalize();

    for(uint32_t i = 0; i < BUF_SIZE/2; ++i) {
        cir_buffer.push_back(i);
    }
    EXPECT_EQUAL(cir_buffer.size(), BUF_SIZE/2);

    root_clk->getScheduler()->run(1);

    for(uint32_t i = 0; i < BUF_SIZE/2; ++i) {
        cir_buffer.push_back(i);
    }
    EXPECT_EQUAL(cir_buffer.size(), BUF_SIZE);
    root_clk->getScheduler()->run(1);

    for(uint32_t i = 0; i < BUF_SIZE/2; ++i) {
        cir_buffer.push_back(i);
    }
    EXPECT_EQUAL(cir_buffer.size(), BUF_SIZE);
    root_clk->getScheduler()->run(1);

    rtn.enterTeardown();
}
#endif

void testStatsOutput()
{
    sparta::RootTreeNode rtn;
    sparta::Scheduler sched;
    sparta::ClockManager  cm(&sched);
    sparta::Clock::Handle root_clk;
    root_clk = cm.makeRoot(&rtn, "root_clk");
    rtn.setClock(root_clk.get());
    cm.normalize();
    sparta::Report r1("report 1", &rtn);

    sparta::StatisticSet stats(&rtn);
    sparta::CircularBuffer<uint32_t> b("buf_const_test", 10,
                                     root_clk.get(), &stats);
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

void testStruct()
{
    sparta::RootTreeNode rtn;
    sparta::Scheduler sched;
    sparta::ClockManager  cm(&sched);
    sparta::Clock::Handle root_clk;
    root_clk = cm.makeRoot(&rtn, "root_clk");
    rtn.setClock(root_clk.get());
    cm.normalize();
    sparta::Report r1("report 1", &rtn);

    sparta::StatisticSet stats(&rtn);
    struct Entry
    {
        uint64_t aval;
        bool bval;

        explicit Entry(uint64_t v, bool b)
        : aval(v)
        , bval(b)
        {
        }
    };
    sparta::CircularBuffer<Entry> b("buf_struct_test", 10,
                                     root_clk.get(), &stats);

    b.push_back(Entry(4, true));
    b.push_back(Entry(15, false));

    EXPECT_EQUAL(b.begin()->aval, 4);

    auto i = b.begin();
    EXPECT_EQUAL(i->bval, true);

    ++i;
    EXPECT_EQUAL(i->aval, 15);
    EXPECT_EQUAL(i->bval, false);
}

int main()
{
    testPushBack();
    testForwardIterators();
    testReverseIterators();
    testEraseInsert();
    testStatsOutput();
    testStruct();

    // Cannot test collection until we move to gcc4.9 or higher.  Bug
    // in gcc -- gets confused on the const iterators
#if SPARTA_GCC_VERSION > 40800
    testCollection();
#endif

    REPORT_ERROR;
    return ERROR_CODE;
}
