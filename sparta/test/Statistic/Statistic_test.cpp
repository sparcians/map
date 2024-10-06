
#include <inttypes.h>
#include <iostream>

#include <boost/timer/timer.hpp>

#include "sparta/sparta.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/statistics/Counter.hpp"
#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/statistics/StatisticDef.hpp"
#include "sparta/statistics/StatisticInstance.hpp"
#include "sparta/utils/Utils.hpp"
#include "sparta/utils/SpartaTester.hpp"


/*!
 * \file main.cpp
 * \brief Test for StatisticSet, StatisticDef, and StatisticInstance
 */

TEST_INIT

using sparta::StatisticDef;
using sparta::StatisticInstance;
using sparta::StatisticSet;
using sparta::RootTreeNode;
using sparta::TreeNode;
using sparta::Counter;


int main()
{
    // Place into a tree
    RootTreeNode root;
    sparta::Scheduler sched;
    sparta::Clock clk(&root, "clock", &sched);
    root.setClock(&clk);
    TreeNode dummy(&root, "dummy", "A dummy node");
    StatisticSet sset(&root);
    StatisticSet cset(&dummy);
    Counter ctr(&cset, "a", "Counter A", Counter::COUNT_NORMAL);
    EXPECT_TRUE(sset.isAttached()); // Ensure that node constructed with parent arg is properly attached

    // Print current register set by the ostream insertion operator
    std::cout << sset << std::endl;

    // Illegal StatisticDefs
    EXPECT_THROW(StatisticDef sd(&root, "bad_stat", "Illegally added", &root, "dummy.stats.a")); // Should not allow adding to a parent which is not a StatisticSet

    // Ok StatisticDefs
    StatisticDef sd1(&sset, "sd1", "Statistic Description", &root, "dummy.stats.a");

    // More Counters
    Counter ctrb(&sset, "b", "Statistic Description", Counter::COUNT_NORMAL);

    // Ensure counters can be added to vectors (with reallocation and moving)
    std::vector<StatisticDef> stat_vec;
    for(auto i : {1,2,3,4,5,6,7,8,9}){
        std::stringstream name;
        name << "C_" << i;
        stat_vec.emplace_back(&sset, name.str(), "groupc", 1000+i, "C Stat", &sset, "1"); // dummy expression
        std::cout << "The tree after " << name.str() << " at " << i << std::endl
                  << sset.renderSubtree(-1, true) << std::endl;
    }

    std::vector<StatisticDef> moved_stat_vec(std::move(stat_vec));
    EXPECT_EQUAL(moved_stat_vec.size(), 9);
    EXPECT_EQUAL(stat_vec.size(), 0);

    // Attempt to access moved counters
    EXPECT_EQUAL(moved_stat_vec.at(2).getName(), "C_3");
    EXPECT_NOTHROW(sset.getChildAs<StatisticDef>("C_3"));
    EXPECT_NOTHROW(sset.getStatisticDef("C_3")->getName());
    EXPECT_EQUAL(moved_stat_vec.at(8).getName(), "C_9");

    // from top.dummy.stats, refer to 'a' and then 'top.stats.b', then 'top.dummy.stats.a'
    StatisticDef sd2(&sset, "sd2", "Neighbor-accessing stat", &cset, "a + ..stats.b + .stats.a");

    // from top.stats, refer to 'sd1' and then 'top.dummy.stats.a'
    StatisticDef sd3(&sset, "sd3", "Neighbor-accessing stat", &sset, "sd1 + .dummy.stats.a");

    // Issue 245, bug for statistic set move
    TreeNode dummy2(&root, "dummy2", "A second dummy node");
    sparta::StatisticSet moved_stats_set(&dummy2);
    sparta::Counter orig_counter(&moved_stats_set, "moved_stat", "A stat to be moved", Counter::COUNT_NORMAL);
    EXPECT_EQUAL(moved_stats_set.getNumCounters(), 1);
    sparta::Counter moved_counter(std::move(orig_counter));
    EXPECT_EQUAL(moved_stats_set.getNumCounters(), 1);

    // Hmmm... how does this work?
    auto & created_stat =
        moved_stats_set.createCounter<sparta::Counter>("another_moved_stat", "Another stat to be moved", Counter::COUNT_NORMAL);
    EXPECT_EQUAL(moved_stats_set.getNumCounters(), 2);
    sparta::Counter new_moved_counter(std::move(created_stat));
    (void) new_moved_counter;
    EXPECT_EQUAL(moved_stats_set.getNumCounters(), 2);

    // Finalize tree
    root.enterConfiguring();
    root.enterFinalized();

    // Ok StatisticInstances
    StatisticInstance si1(&sd1);
    std::cout << si1 << " " << si1.getExpressionString() << std::endl;
    StatisticInstance si2(&sd2);
    std::cout << si2 << " " << si2.getExpressionString() << std::endl;
    StatisticInstance si3(&sd3);
    std::cout << si3 << " " << si3.getExpressionString() << std::endl;


    // Done

    // Report errors before drawing trees in case any nodes were attached which
    // should not have been.
    REPORT_ERROR;


    // Render Tree for information purposes

    std::cout << "The tree from the top with builtins: " << std::endl << root.renderSubtree(-1, true) << std::endl;
    std::cout << "The tree from the top without builtins: " << std::endl << root.renderSubtree() << std::endl;
    std::cout << "The tree from sset: " << std::endl << sset.renderSubtree(-1, true);

    root.enterTeardown();

    REPORT_ERROR;

    return ERROR_CODE;
}
