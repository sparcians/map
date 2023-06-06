
#include <inttypes.h>
#include <iostream>

#include <boost/timer/timer.hpp>

#include "sparta/sparta.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/statistics/Counter.hpp"
#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/statistics/StatisticDef.hpp"
#include "sparta/statistics/StatisticInstance.hpp"
#include "sparta/tree/filter/Parser.hpp"
#include "sparta/statistics/InstrumentationNode.hpp"
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
using sparta::InstrumentationNode;
using sparta::tree::filter::Expression;


int main()
{
    /*
     *                 a
     *           _____/ \_____________
     *          /                     \
     *         b                       c
     *      __/ \_____________          \
     *     /          \       \          \
     *    d            e     stats      stats
     *    |            |       |          |
     *  stats        stats   ctr_b1     ctr_c1
     *    |            |
     *    + ctr_d1  ctr_e1
     *    + ctr_d2
     *    + ctr_d3
     *    + stat_d1
     */

    // Place into a tree
    RootTreeNode root;
    TreeNode a(&root, "a", "A dummy node");
    StatisticSet ssa(&a);
    TreeNode b(&a, "b", "A dummy node");
    StatisticSet ssb(&b);
    TreeNode c(&a, "c", "A dummy node");
    StatisticSet ssc(&c);
    TreeNode d(&b, "d", "A dummy node");
    StatisticSet ssd(&d);
    TreeNode e(&b, "e", "A dummy node");
    StatisticSet sse(&e);

    Counter cd1(&ssd, "ctr_d1", "A Counter", Counter::COUNT_NORMAL);
    Counter cd2(&ssd, "ctr_d2", "A Counter", Counter::COUNT_NORMAL);
    Counter cd3(&ssd, "ctr_d3", "A Counter", Counter::COUNT_NORMAL);
    Counter ce1(&sse, "ctr_e1", "A Counter", Counter::COUNT_NORMAL);
    Counter cb1(&ssb, "ctr_b1", "A Counter", Counter::COUNT_NORMAL);
    Counter cc1(&ssc, "ctr_c1", "A Counter", Counter::COUNT_NORMAL);

    StatisticDef sd1(&ssd, "stat_d1", "A Stat", &ssd, "ctr_d1 + ctr_d2");

    cd1.addTag("foo");
    cd1.addTag("bar");
    cd1.addTag("fizbin");
    cd1.addTag("fizbin2");

    sparta::tree::filter::Parser tfp;
    sparta::tree::filter::Expression ex;

    const bool traceout = true;

    // Check case sensitivity
    ex = tfp.parse("vis:NORMAL");

    // Check a simple expression
    ex = tfp.parse("VIS:normal");
    std::cout << "  1 " << ex << std::endl;
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    // Check more complex expression
    ex = tfp.parse("vis:normal && true");
    std::cout << "  2 " << ex << std::endl;
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    // Check C++ creation of expressions
    ex = Expression(InstrumentationNode::VIS_NORMAL);
    ex |= Expression(InstrumentationNode::VIS_SUMMARY, Expression::VISCOMP_EQ);
    std::cout << "  3 " << ex << std::endl;
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    // Check friendlier && syntax for C++ expression creation
    ex = Expression(InstrumentationNode::VIS_NORMAL) && Expression(InstrumentationNode::VIS_SUMMARY,
                                                                   Expression::VISCOMP_LT);
    std::cout << "  4 " << ex << std::endl;
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    // Check some other operations
    ex = tfp.parse("fAlSe");
    EXPECT_FALSE(ex.valid(&cd1, traceout));

    ex = tfp.parse("TrUe");
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    ex = tfp.parse("false ^^ true");
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    ex = tfp.parse("true ^^ true");
    EXPECT_FALSE(ex.valid(&cd1, traceout));

    ex = tfp.parse("false || true");
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    ex = tfp.parse("false && true");
    EXPECT_FALSE(ex.valid(&cd1, traceout));

    ex = tfp.parse("true && true");
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    ex = tfp.parse("<=vis:summary");
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    ex = tfp.parse("<=vis:100m");
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    ex = tfp.parse(">vis:100m");
    EXPECT_FALSE(ex.valid(&cd1, traceout));

    ex = tfp.parse("<vis:0x100g");
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    ex = tfp.parse(">=vis:hidden");
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    ex = tfp.parse(">vis:hidden && < vis : summary");
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    ex = tfp.parse("vis:hidden || vis : normal");
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    EXPECT_THROW(tfp.parse("vis:normal && <= type:counter")); // Can't do relative comparisons on type

    ex = tfp.parse("vis:normal && (type:counter || tYpE:STAT)");
    EXPECT_TRUE(ex.valid(&cd1, traceout));
    EXPECT_TRUE(ex.valid(&sd1, traceout));

    ex = tfp.parse(">vis:99999999 && <vis:100000001");
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    // Check tag matching
    EXPECT_THROW(ex = tfp.parse(">tag:foo"););
    EXPECT_THROW(ex = tfp.parse("<tag:foo"););

    ex = tfp.parse("tag:foo");
    std::cout << "tag:foo -> " << ex << std::endl;
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    ex = tfp.parse("not tag:foo");
    EXPECT_FALSE(ex.valid(&cd1, traceout));

    ex = tfp.parse("==tag:foo");
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    ex = tfp.parse("!=tag:foo");
    EXPECT_FALSE(ex.valid(&cd1, traceout));

    ex = tfp.parse("tag:buz || tag:nope");
    EXPECT_FALSE(ex.valid(&cd1, traceout));

    ex = tfp.parse("tag:foo && tag:nope");
    EXPECT_FALSE(ex.valid(&cd1, traceout));

    // No partial tag matches without regex
    ex = tfp.parse("tag:fiz");
    EXPECT_FALSE(ex.valid(&cd1, traceout));

    // Case sensitivity
    ex = tfp.parse("tag:Foo");
    EXPECT_FALSE(ex.valid(&cd1, traceout));

    // Regex success
    ex = tfp.parse("regex tag:fiz.*");
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    // Regex fail
    ex = tfp.parse("regex tag:fuz.*");
    EXPECT_FALSE(ex.valid(&cd1, traceout));

    // Regex endpoints
    ex = tfp.parse("regex tag:^izbi$");
    EXPECT_FALSE(ex.valid(&cd1, traceout));

    ex = tfp.parse("regex tag:izbi"); // String containment is not a match by default
    EXPECT_FALSE(ex.valid(&cd1, traceout));

    ex = tfp.parse("regex tag:^.izbin$"); // String containment is not a match by default
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    ex = tfp.parse("regex tag:.izbin"); // String containment is not a match by default
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    // Quoted regexes
    EXPECT_NOTHROW(ex = tfp.parse("regex tag:\"foo bar\"");) // Just needs to parse spaces
    EXPECT_FALSE(ex.valid(&cd1, traceout));

    EXPECT_NOTHROW(ex = tfp.parse("regex tag:\"fizbin\"");)
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    EXPECT_NOTHROW(ex = tfp.parse("regex tag:'foo bar'");) // Just needs to not throw
    EXPECT_FALSE(ex.valid(&cd1, traceout));

    EXPECT_NOTHROW(ex = tfp.parse("regex tag:'fizbin'");)
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    EXPECT_NOTHROW(ex = tfp.parse("regex tag:'fiz.+'");)
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    EXPECT_NOTHROW(ex = tfp.parse("regex tag:fiz.+");)
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    EXPECT_NOTHROW(ex = tfp.parse("true && regex tag:fiz.+");)
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    EXPECT_NOTHROW(ex = tfp.parse("regex tag:fiz.+ && not regex tag:buz.+");)
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    EXPECT_NOTHROW(ex = tfp.parse("regex tag:fiz.+ && not regex tag:.*buz.*");)
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    // Check "not" precedence
    EXPECT_NOTHROW(ex = tfp.parse("not regex tag:.*buz.* && regex tag:fiz.+");)
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    EXPECT_NOTHROW(ex = tfp.parse("(not regex tag:.*buz.* ) && regex tag:fiz.+");)
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    EXPECT_NOTHROW(ex = tfp.parse("(not regex tag:'.*buz.*') && regex tag:fiz.+");)
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    EXPECT_NOTHROW(ex = tfp.parse("name:ctr_d1");)
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    EXPECT_NOTHROW(ex = tfp.parse("regex name:.*ctr_d1.*");)
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    EXPECT_NOTHROW(ex = tfp.parse("== name:ctr_d1");)
    EXPECT_TRUE(ex.valid(&cd1, traceout));

    EXPECT_NOTHROW(ex = tfp.parse("!= name:notthename");)
    EXPECT_TRUE(ex.valid(&cd1, traceout));


    // Done

    // Report errors before drawing trees in case any nodes were attached which
    // should not have been.
    REPORT_ERROR;


    // Render Tree for information purposes

    std::cout << "The tree from the top with builtins: " << std::endl << root.renderSubtree(-1, traceout) << std::endl;

    root.enterTeardown();

    return ERROR_CODE;
}
