#include "sparta/sparta.hpp"
#include "sparta/statistics/ExpressionParser.hpp"
#include "sparta/statistics/Expression.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/statistics/Counter.hpp"
#include "sparta/statistics/StatisticDef.hpp"

#include <vector>
#include <string>
#include <iostream>

using sparta::statistics::expression::Expression;
using sparta::statistics::expression::ExpressionParser;
using sparta::statistics::expression::ReferenceVariable;

#include "sparta/utils/SpartaTester.hpp"

TEST_INIT;

class Params : public sparta::ParameterSet
{
public:
    Params (sparta::TreeNode * n) :
        sparta::ParameterSet (n)
    {
    }

    PARAMETER (uint32_t, foo, 4, "param foo")
    PARAMETER (double, bar, 5.5, "param bar")
    PARAMETER (std::string, fiz, "1.0", "param fiz")
    PARAMETER (std::vector<uint32_t>, buz, std::vector<uint32_t>(1), "param buz")
};

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    //assert(argc > 1);
    //std::ifstream in(argv[1], std::ios::in);

    //std::string storage; // We will read the contents here.
    //if(in.good()){
    //    in.unsetf(std::ios::skipws); // No white space skipping!
    //    std::copy(
    //              std::istream_iterator<char>(in),
    //              std::istream_iterator<char>(),
    //              std::back_inserter(storage));
    //}else{
    //    storage = argv[1];
    //}

    //std::cout << "Parsing \"" << storage << "\"" << std::endl;


    // Non-Tree Expressions
    sparta::TreeNode foo("foo", "dummy node for expressions until they can be evaluated with no context");

    // Good expressions
    EXPECT_EQUAL(Expression("1", &foo).evaluate(), 1);
    EXPECT_EQUAL(Expression("1+2+3", &foo).evaluate(), 6);
    EXPECT_EQUAL(Expression("1+-2-3", &foo).evaluate(), -4);
    EXPECT_EQUAL(Expression("1+2*3", &foo).evaluate(), 7);
    EXPECT_EQUAL(Expression("log2(122+2*3)", &foo).evaluate(), 7);
    EXPECT_EQUAL(Expression("inf", &foo).evaluate(), std::numeric_limits<double>::infinity());
    EXPECT_EQUAL(Expression("ifnan(1/0, 5)", &foo).evaluate(), 5);
    EXPECT_EQUAL(Expression("ifnan(inf, 5)", &foo).evaluate(), 5);
    EXPECT_EQUAL(Expression("ifnan(nan, 5)", &foo).evaluate(), 5);
    EXPECT_EQUAL(Expression("ifnan(1, 5)", &foo).evaluate(), 1);
    EXPECT_EQUAL(Expression("cond(1, 2, 3)", &foo).evaluate(), 2);
    EXPECT_EQUAL(Expression("cond(0.00001, 2, 3)", &foo).evaluate(), 2);
    EXPECT_EQUAL(Expression("cond(-1, 2, 3)", &foo).evaluate(), 2);
    EXPECT_EQUAL(Expression("cond(0, 2, 3)", &foo).evaluate(), 3);
    EXPECT_EQUAL(Expression("cond(is_greater(0, 1), 2, 3)", &foo).evaluate(), 3);
    EXPECT_EQUAL(Expression("cond(is_lesser(0, 1), 2, 3)", &foo).evaluate(), 2);
    EXPECT_EQUAL(Expression("cond(is_equal(0, 1), 2, 3)", &foo).evaluate(), 3);
    EXPECT_EQUAL(Expression("cond(is_not_equal(0, 1), 2, 3)", &foo).evaluate(), 2);
    EXPECT_EQUAL(Expression("cond(is_greater_equal(0, 1), 2, 3)", &foo).evaluate(), 3);
    EXPECT_EQUAL(Expression("cond(is_lesser_equal(0, 0), 2, 3)", &foo).evaluate(), 2);
    EXPECT_EQUAL(Expression("cond(logical_and(0, 1), 2, 3)", &foo).evaluate(), 3);
    EXPECT_EQUAL(Expression("cond(logical_or(0, 1), 2, 3)", &foo).evaluate(), 2);

    // Test all functions
    //! \todo Test supported expression functions

    // Test all constants
    //! \todo Test supported expression constants

    // Unparsable Expressions
    EXPECT_THROW(Expression("fiz", &foo));
    EXPECT_THROW(Expression("2-*1", &foo));
    EXPECT_THROW(Expression("2-/1", &foo));
    EXPECT_THROW(Expression("2//1", &foo));
    EXPECT_THROW(Expression("2***1", &foo));
    EXPECT_THROW(Expression("(", &foo));
    EXPECT_THROW(Expression("(2", &foo));
    EXPECT_THROW(Expression(")", &foo));
    EXPECT_THROW(Expression("2)", &foo));
    EXPECT_THROW(Expression("(2+)", &foo));
    EXPECT_THROW(Expression("2+", &foo));
    EXPECT_THROW(Expression("(2)+3)", &foo));

    // Construct some long-lived expressions referring to the tree
    // These will outlive the tree and be destructed AND evaluated afterward
    // to ensure that things do not crash
    std::unique_ptr<Expression> outer_scope_expr_1;

    // Block containing the Tree to be destroyed before the above expressions
    {

        sparta::RootTreeNode top("top","A Tree Node");
        sparta::TreeNode c(&top, "decoy", "Non-Counter, Non-Stat decoy Node");
        sparta::Scheduler sched;
        sparta::Clock::Handle clk_h(new sparta::Clock("parent_clk", &sched));
        sparta::Clock clk("clk", clk_h, 4.75);
        top.setClock(&clk);

        sparta::StatisticSet cset(&top);
        sparta::Counter ca(&cset, "a", "Counter A", sparta::Counter::COUNT_NORMAL);
        sparta::Counter cb(&cset, "b", "Counter B", sparta::Counter::COUNT_NORMAL);
        sparta::Counter cc(&cset, "c", "Counter B", sparta::Counter::COUNT_NORMAL);

        sparta::TreeNode foo(&top, "foo", "Foo Node");
        Params pset(&foo);
        sparta::StatisticSet sset(&foo);
        sparta::StatisticDef sa(&sset, "a", "Statistic A", &cset, "1+2");
        sparta::StatisticDef sb(&sset, "b", "Statistic B", &cset, "a + b");
        sparta::StatisticDef sc(&sset, "c", "Statistic B", &cset, "b ** a");

        double var1 = 0;
        sparta::StatisticDef sd(&sset, "d", "Statistic D", &cset,
                              Expression(5) * Expression(new ReferenceVariable("variable", var1)));
        var1 = 100;

        sparta::StatisticDef se(&sset, "e", "Statistic E", &cset, "5*g_ticks");
        sparta::StatisticDef sf(&sset, "cycles", "Statistic F", &cset, "cycles");
        sparta::StatisticDef sg(&sset, "paramtest1", "Statistic G", &sset, ".params.foo*b");
        sparta::StatisticDef sh(&sset, "paramtest2", "Statistic H", &sset, ".params.bar*c");
        sparta::StatisticDef si(&sset, "paramtest3", "Statistic I", &sset, ".params.fiz*c");
        sparta::StatisticDef sj(&sset, "paramtest4", "Statistic J", &sset, ".params.buz*c");
        sparta::StatisticDef sk(&sset, "freq_mhz", "Statistic K", &sset, "freq_mhz");

        top.enterConfiguring();
        top.enterFinalized();

        sched.finalize();

        outer_scope_expr_1.reset(new Expression("foo.stats.a", &top));

        // Block of things destroyed before the tree
        {
            // Expression printing
            Expression ex_printable("1-2+abs(-3)", &foo);
            std::cout << ex_printable << " = " << ex_printable.evaluate() << std::endl;
            EXPECT_EQUAL(ex_printable.evaluate(), 2);

            // Build an expression with nodes (no string parsing)
            Expression ex_printable2 = Expression(2) / 5;
            std::cout << ex_printable2 << " = " << ex_printable2.evaluate() << std::endl;
            EXPECT_EQUAL(ex_printable2.evaluate(), 0.4);

            // Build an expression with nodes (no string parsing) and a reference that will be updated
            double var_ref = 2;
            Expression ex_printable3 = Expression(2.5) * Expression(new ReferenceVariable("variable", var_ref));
            std::cout << ex_printable3 << " = " << ex_printable3.evaluate() << std::endl;
            EXPECT_EQUAL(ex_printable3.evaluate(), 5);
            var_ref = 4; // Update reference
            std::cout << ex_printable3 << " = " << ex_printable3.evaluate() << std::endl;
            EXPECT_EQUAL(ex_printable3.evaluate(), 10);

            // Use the simpler syntax for refering to sdtats
            std::vector<const sparta::TreeNode*> used;
            Expression ex_printable4 = Expression(2.5) + Expression(&se, used);
            std::cout << ex_printable4 << " = " << ex_printable4.evaluate() << std::endl;
            EXPECT_EQUAL(ex_printable4.evaluate(), 2.5);

            // Increment counters before declaring expressions (stats)
            ca+=3;
            cb+=2;
            cc+=1;

            sched.run(11, true);

            EXPECT_EQUAL(ex_printable4.evaluate(), 57.5);

            // Create some expressions
            Expression a("top.stats.a", top.getSearchScope());
            Expression b("top.stats.b", top.getSearchScope());
            Expression c("top.stats.c", top.getSearchScope());
            Expression d("top.stats.c*g_ticks", top.getSearchScope());
            Expression e("c*cycles", &cset);

            sparta::StatisticInstance si_ca(&ca);
            sparta::StatisticInstance si_cb(static_cast<sparta::TreeNode*>(&cb)); // construct from generic
            sparta::StatisticInstance si_cc(&cc);

            sparta::StatisticInstance si_sa(&sa);
            sparta::StatisticInstance si_sb(static_cast<sparta::TreeNode*>(&sb)); // construct from generic
            sparta::StatisticInstance si_sc(&sc);
            sparta::StatisticInstance si_sd(&sd);
            sparta::StatisticInstance si_se(&se);
            sparta::StatisticInstance si_sf(&sf);
            sparta::StatisticInstance si_sg(&sg);
            sparta::StatisticInstance si_sh(&sh);
            EXPECT_THROW(sparta::StatisticInstance si_si(&si));
            EXPECT_THROW(sparta::StatisticInstance si_sj(&sj));
            sparta::StatisticInstance si_sk(&sk);

            // Increment counters after declaring expressions so that some nonzero
            // values can be read from them
            ca+=3;
            cb+=2;
            cc+=1;

            sched.run(10, true);

            var1 = 2; // Updated here

            // Evaluate expressions
            EXPECT_EQUAL(a.evaluate(), 3);
            EXPECT_EQUAL(b.evaluate(), 2);
            EXPECT_EQUAL(c.evaluate(), 1);
            EXPECT_EQUAL(d.evaluate(), 10); // delta 10 ticks * delta 1 cc
            EXPECT_EQUAL(e.evaluate(), 10); // delta 10 cycles * delta 1 cc

            // evaluateAbsolute evaluation (do not use deltas)
            //EXPECT_EQUAL(a.evaluateAbsolute(), 6);
            //EXPECT_EQUAL(b.evaluateAbsolute(), -4);
            //EXPECT_EQUAL(c.evaluateAbsolute(), 2);

            EXPECT_EQUAL(si_ca.getValue(), 3);
            EXPECT_EQUAL(si_cb.getValue(), 2);
            EXPECT_EQUAL(si_cc.getValue(), 1);

            EXPECT_EQUAL(si_sa.getValue(), 3);
            EXPECT_EQUAL(si_sb.getValue(), 5);
            EXPECT_EQUAL(si_sc.getValue(), 8);
            EXPECT_EQUAL(si_sd.getValue(), 10);
            EXPECT_EQUAL(si_se.getValue(), 50);
            EXPECT_EQUAL(si_sf.getValue(), 10); // 10 ticks
            EXPECT_EQUAL(si_sg.getValue(), 20);
            EXPECT_EQUAL(si_sh.getValue(), 44);
            EXPECT_EQUAL(si_sk.getValue(), 4.75);

            std::cout << si_sh.getValue() << std::endl;

            // Bad expressions symbols
            EXPECT_THROW(Expression("foo.stats.a", nullptr).evaluate()); // No context
            EXPECT_THROW(Expression("decoy", &top).evaluate()); // Not a counter/statdef
            EXPECT_THROW(Expression("top", &top).evaluate()); // top is not a child of top
            EXPECT_THROW(Expression("nonexistant", &top).evaluate()); // no node with this name in top
        }

        top.enterTeardown();

        // At this point, the tree will be destructed
    }

    // Delete after tree is torn down.
    // It is not safe to print this
    outer_scope_expr_1.reset();


    // Done

    REPORT_ERROR;

    return ERROR_CODE;
}
