
#include <iostream>
#include <inttypes.h>
#include "sparta/sparta.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/resources/Queue.hpp"
#include "sparta/report/Report.hpp"

#include <boost/timer/timer.hpp>
#include <vector>
#include "sparta/statistics/StatisticInstance.hpp"
#include "sparta/statistics/CycleCounter.hpp"
TEST_INIT;

#define PIPEOUT_GEN

void testStatsOutput();

int main()
{
    sparta::Scheduler sched;
    sparta::RootTreeNode rtn;
    sparta::ClockManager cm(&sched);
    sparta::Clock::Handle root_clk;
    root_clk = cm.makeRoot(&rtn, "root_clk");
    cm.normalize();

    sparta::StatisticSet queue10_stats(&rtn);

    sparta::Queue<double> queue10_untimed("queue10_untimed", 10,
                                        root_clk.get(),
                                        &queue10_stats);
    rtn.setClock(root_clk.get());

#ifdef PIPEOUT_GEN
    queue10_untimed.enableCollection(&rtn);
#endif

    rtn.enterConfiguring();
    rtn.enterFinalized();

#ifdef PIPEOUT_GEN
    sparta::collection::PipelineCollector pc("testPipe", 1000000,
                                           root_clk.get(), &rtn);
#endif

    sched.finalize();

#ifdef PIPEOUT_GEN
    pc.startCollection(&rtn);
#endif

    ////////////////////////////////////////////////////////////
    sched.run(1);

    queue10_untimed.push(1234.5);
    EXPECT_TRUE(queue10_untimed.size() == 1);

    sched.run(1);

    EXPECT_TRUE(queue10_untimed.size() == 1);

    EXPECT_EQUAL(queue10_untimed.front(), 1234.5);
    EXPECT_EQUAL(queue10_untimed.back(), 1234.5);

    for(uint32_t i = 0; i < 9; ++i) {
        const double val = 0.5 + i;
        queue10_untimed.push(val);
        EXPECT_EQUAL(queue10_untimed.back(), val);
    }
    sparta::Queue<double>::iterator queue10_untimes_iter= queue10_untimed.begin();

    EXPECT_EQUAL(*queue10_untimes_iter, 1234.5);
    queue10_untimes_iter++;

    uint32_t i = 0;
    for(; queue10_untimes_iter < queue10_untimed.end();queue10_untimes_iter++){
        EXPECT_EQUAL(*queue10_untimes_iter, i + 0.5);
        i++;
    }
    queue10_untimes_iter = queue10_untimed.begin();
    EXPECT_NOTHROW (
                    *queue10_untimes_iter  = 1234.51;
                    EXPECT_EQUAL(*queue10_untimes_iter, 1234.51);
                    *queue10_untimes_iter  = 1234.5;
                    );

    sparta::Queue<double>::const_iterator queue10_untimed_const_iter= queue10_untimed.begin();
    EXPECT_EQUAL(*queue10_untimed_const_iter, 1234.5);
    queue10_untimed_const_iter++;

    i = 0;
    for(; queue10_untimed_const_iter < queue10_untimed.end();queue10_untimed_const_iter++){
        EXPECT_EQUAL(*queue10_untimed_const_iter, i + 0.5);
        i++;
    }
    queue10_untimed_const_iter = queue10_untimed.begin();
#if 0
    try{
        *queue10_untimed_const_iter  = 1234.51;
        sparta_assert(EXPECT_EQUAL(*queue10_untimed_const_iter, 1234.51), "Const Iterator just modified the data!!");
    }
    catch(...){

    }
#endif

    EXPECT_EQUAL(queue10_untimed.size(), 10);
    sched.run(1);
    EXPECT_EQUAL(queue10_untimed.size(), 10);

    uint32_t half = queue10_untimed.size()/2;
    for(uint32_t i = 0; i < half; ++i) {
        queue10_untimed.pop();
    }
    EXPECT_EQUAL(queue10_untimed.size(), 5);
    sched.run(1);

    while(queue10_untimed.size() != 0) {
        queue10_untimed.pop();
    }
    EXPECT_EQUAL(queue10_untimed.size(), 0);
    sched.run(1);
    EXPECT_EQUAL(queue10_untimed.size(), 0);

    // Test clear()
    for(uint32_t i = 0; i < queue10_untimed.capacity(); ++i) {
        queue10_untimed.push(i);
        EXPECT_EQUAL(queue10_untimed.back(), i);
        EXPECT_EQUAL(queue10_untimed.front(), 0);
    }
    EXPECT_EQUAL(queue10_untimed.size(), 10);

    queue10_untimed.clear();

    // Do it again.
    for(uint32_t i = 0; i < queue10_untimed.capacity(); ++i) {
        queue10_untimed.push(i);
        EXPECT_EQUAL(queue10_untimed.back(), i);
        EXPECT_EQUAL(queue10_untimed.front(), 0);
    }
    EXPECT_EQUAL(queue10_untimed.size(), 10);
    auto bit = queue10_untimed.begin();

    EXPECT_EQUAL(queue10_untimed.read(0), 0);
    EXPECT_EQUAL(queue10_untimed.access(0), 0);

    queue10_untimed.clear();

    EXPECT_EQUAL(queue10_untimed.size(), 0);
    EXPECT_TRUE(queue10_untimed.begin() == queue10_untimed.end());
    EXPECT_FALSE(bit.isValid());
    EXPECT_NOTHROW(bit++);
    EXPECT_FALSE(bit.isValid());
    EXPECT_THROW(*bit);


    for(uint32_t i = 0; i < queue10_untimed.capacity(); ++i) {
        queue10_untimed.push(i);
    }
    EXPECT_EQUAL(queue10_untimed.size(), 10);

    auto eit = queue10_untimed.end();
    EXPECT_NOTHROW(eit--);
    EXPECT_EQUAL(*eit, 9);

    // Test pop_back(), oldest (front -> 0,1,2,3,4,5,6,7,8,9 <- newest (back)
    for(uint32_t i = (queue10_untimed.capacity() - 1); i != 0; --i) {
        EXPECT_EQUAL(queue10_untimed.back(), i);
        queue10_untimed.pop_back();
    }
    EXPECT_EQUAL(queue10_untimed.size(), 1);
    EXPECT_EQUAL(queue10_untimed.front(), 0);
    EXPECT_EQUAL(queue10_untimed.back(), 0);

    queue10_untimed.pop_back();
    EXPECT_EQUAL(queue10_untimed.size(), 0);


    for(uint32_t i = 0; i < queue10_untimed.capacity(); ++i) {
        queue10_untimed.push(i);
    }
    // for(auto v : queue10_untimed) {
    //     std::cout << v << std::endl;
    // }
    // std::cout << std::endl;

    for(uint32_t i = (queue10_untimed.capacity() / 2); i != 0; --i) {
        queue10_untimed.pop_back();
    }

    // for(auto v : queue10_untimed) {
    //     std::cout << v << std::endl;
    // }
    // std::cout << std::endl;
    for(uint32_t i = 0; i < queue10_untimed.capacity() / 2; ++i) {
        queue10_untimed.push(i+5);
    }

    // for(auto v : queue10_untimed) {
    //     std::cout << v << std::endl;
    // }
    // std::cout << std::endl;

    auto it = std::begin(queue10_untimed);
    EXPECT_EQUAL(*it, 0);
    ++it;
    EXPECT_EQUAL(*it, 1);
    ++it;
    EXPECT_EQUAL(*it, 2);
    ++it;
    EXPECT_EQUAL(*it, 3);
    ++it;
    EXPECT_EQUAL(*it, 4);
    ++it;
    EXPECT_EQUAL(*it, 5);
    ++it;
    EXPECT_EQUAL(*it, 6);
    ++it;
    EXPECT_EQUAL(*it, 7);
    ++it;
    EXPECT_EQUAL(*it, 8);
    ++it;
    EXPECT_EQUAL(*it, 9);

    testStatsOutput();

    rtn.enterTeardown();
#ifdef PIPEOUT_GEN
    pc.destroy();
#endif

    REPORT_ERROR;
    return ERROR_CODE;
}

void testStatsOutput()
{
    sparta::Scheduler sched;
    sparta::RootTreeNode rtn;
    sparta::ClockManager cm(&sched);
    sparta::Clock::Handle root_clk;
    root_clk = cm.makeRoot(&rtn, "root_clk");
    cm.normalize();
    sparta::Report r1("report 1", &rtn);

    sparta::StatisticSet stats(&rtn);
    sparta::Queue<uint32_t> b("buf_const_test", 10,
                            root_clk.get(),
                            &stats);
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
