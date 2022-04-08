
#include <inttypes.h>
#include <limits>
#include <iostream>

#include "sparta/sparta.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/report/Report.hpp"
#include "sparta/report/format/CSV.hpp"
#include "sparta/report/format/BasicHTML.hpp"
#include "sparta/report/format/Text.hpp"
#include "sparta/report/format/Gnuplot.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/statistics/Counter.hpp"
#include "sparta/statistics/ReadOnlyCounter.hpp"
#include "sparta/statistics/CycleCounter.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/log/Tap.hpp"
#include "sparta/utils/SpartaTester.hpp"

/*!
 * \file main.cpp
 * \brief Test for Report
 */

TEST_INIT

using sparta::Scheduler;
using sparta::Counter;
using sparta::ReadOnlyCounter;
using sparta::CycleCounter;
using sparta::TreeNode;
using sparta::Report;
using sparta::StatisticDef;
using sparta::StatisticInstance;
using sparta::StatisticSet;
using sparta::RootTreeNode;
using sparta::ClockManager;
using sparta::Clock;

const uint64_t BIG_COUNTER_VAL = 100000;


/*!
 * \brief Generate a tree creating a set of statisticdefs which generate an
 * exception when finalizing
 */
void tryStatisticDef1()
{
    RootTreeNode root("dummy_top");
    TreeNode core0(&root, "core0", "Core 0");
    TreeNode core1(&root, "core1", "Core 1");
    StatisticSet sset0(&core0);
    StatisticSet sset1(&core1);

    Counter c1(&sset0, "c1", "Counter 1", Counter::COUNT_NORMAL);

    StatisticDef sd1(&sset0, "s1", "Statistic Description", &sset0, "c1");

    // Illegal
    StatisticDef sda(&sset0, "sA", "Statistic Description", &sset0, "s1/sA"); // Self reference

    root.enterConfiguring();
    root.enterFinalized();
    EXPECT_THROW(try{
            root.validatePreRun();
        }catch(...){
            root.enterTeardown();
            throw;
        });
}

/*!
 * \brief Generate a tree creating a set of statisticdefs which generate an
 * exception when finalizing
 */
void tryStatisticDef2()
{
    RootTreeNode root("dummy_top");
    TreeNode core0(&root, "core0", "Core 0");
    TreeNode core1(&root, "core1", "Core 1");
    StatisticSet sset0(&core0);
    StatisticSet sset1(&core1);

    Counter c1(&sset0, "c1", "Counter 1", Counter::COUNT_NORMAL);

    StatisticDef sd1(&sset0, "s1", "Statistic Description", &sset0, "c1");


    // Illegal
    StatisticDef sdb(&sset0, "sB", "Statistic Description", &sset0, "sC"); // 2-node Cyclic reference swith sB->sC->sB
    StatisticDef sdc(&sset0, "sC", "Statistic Description", &sset0, "sB"); // 2-node Cyclic reference with above

    root.enterConfiguring();
    root.enterFinalized();
    EXPECT_THROW(try{
                     root.validatePreRun();
                 }catch(...){
                     root.enterTeardown();
                     throw;
                 });
}

/*!
 * \brief Generate a tree creating a set of statisticdefs which generate an
 * exception when finalizing
 */
void tryStatisticDef3()
{
    RootTreeNode root("dummy_top");
    TreeNode core0(&root, "core0", "Core 0");
    TreeNode core1(&root, "core1", "Core 1");
    StatisticSet sset0(&core0);
    StatisticSet sset1(&core1);

    Counter c1(&sset0, "c1", "Counter 1", Counter::COUNT_NORMAL);

    StatisticDef sd1(&sset0, "s1", "Statistic Description", &sset0, "c1");


    // Illegal
    StatisticDef sdd(&sset0, "sD", "Statistic Description", &sset0, "sE"); // 3-node Cyclic reference swith sD->sE->sF->sD
    StatisticDef sde(&sset0, "sE", "Statistic Description", &sset0, "sF"); // 3-node Cyclic reference with above
    StatisticDef sdf(&sset0, "sF", "Statistic Description", &sset0, "sD"); // 3-node Cyclic reference with above

    root.enterConfiguring();
    root.enterFinalized();
    EXPECT_THROW(try{
                     root.validatePreRun();
                 }catch(...){
                     root.enterTeardown();
                     throw;
                 });
}

/*!
 * \brief Generate a tree creating a set of statisticdefs which generate an
 * exception when finalizing
 */
void tryStatisticDef4()
{
    RootTreeNode root("dummy_top");
    TreeNode core0(&root, "core0", "Core 0");
    TreeNode core1(&root, "core1", "Core 1");
    StatisticSet sset0(&core0);
    StatisticSet sset1(&core1);

    Counter c1(&sset0, "c1", "Counter 1", Counter::COUNT_NORMAL);

    StatisticDef sd1(&sset0, "s1", "Statistic Description", &sset0, "c1");


    // Illegal
    StatisticDef sdg(&sset0, "sG", "Statistic Description", &sset0, "sD"); // Reference to a cycle
    StatisticDef sdd(&sset0, "sD", "Statistic Description", &sset0, "sE"); // 3-node Cyclic reference swith sD->sE->sF->sD
    StatisticDef sde(&sset0, "sE", "Statistic Description", &sset0, "sF"); // 3-node Cyclic reference with above
    StatisticDef sdf(&sset0, "sF", "Statistic Description", &sset0, "sD"); // 3-node Cyclic reference with above

    root.enterConfiguring();
    root.enterFinalized();
    EXPECT_THROW(try{
                     root.validatePreRun();
                 }catch(...){
                     root.enterTeardown();
                     throw;
                 });
}

/*!
 * \brief Try a report to make sure its stat def computes only for a window
 */
void tryReport0()
{
    sparta::Scheduler sched;
    sparta::ClockManager  m(&sched);
    Clock::Handle c_root  = m.makeRoot();
    RootTreeNode root(sched.getSearchScope());
    root.setClock(c_root.get());
    TreeNode core0(&root, "core0", "Core 0");
    StatisticSet sset0(&core0);

    Counter c1(&sset0, "c1", "Counter 1 (NORMAL VIS)",  Counter::COUNT_NORMAL, sparta::InstrumentationNode::VIS_NORMAL);
    Counter c2(&sset0, "c2", "Counter 2 (SUMMARY VIS)", Counter::COUNT_NORMAL, sparta::InstrumentationNode::VIS_SUMMARY);
    StatisticDef sd1(&sset0, "s1", "Statistic Description", &sset0, "c1/c2", StatisticDef::VS_PERCENTAGE);

    Report r1("report 1", &root);
    r1.add(root.getChild("core0.stats.s1"));

    root.enterConfiguring();
    root.enterFinalized();
    sched.finalize();

    // proceed to tick 1, nothing should happen, but time advancement
    sched.run(1, true, false);

    c1 += 2;
    c2 += 4;

    sched.run(20, true);
    r1.start();
    std::cout << r1 << std::endl; // 0/0
    const auto hopefully_nan = r1.getStatistic(0).getValue();
    EXPECT_TRUE(hopefully_nan != hopefully_nan); // NaN != NaN

    c1 += 2;
    c2 += 4;

    std::cout << r1 << std::endl;
    EXPECT_EQUAL(r1.getStatistic(0).getValue(), 0.5);

    sched.run(20, true);

    root.enterTeardown();
}

void tryReportWithOptions(bool option_exists)
{
    sparta::Scheduler sched;
    RootTreeNode root(sched.getSearchScope());

    sparta::ClockManager  m(&sched);
    Clock::Handle c_root  = m.makeRoot();
    root.setClock(c_root.get());

    TreeNode core0(&root, "core0", "Core 0");
    StatisticSet sset0(&core0);
    Counter c1(&sset0, "c1", "Counter 1 (NORMAL VIS)",  Counter::COUNT_NORMAL, sparta::InstrumentationNode::VIS_NORMAL);
    Counter c2(&sset0, "c2", "Counter 2 (SUMMARY VIS)", Counter::COUNT_NORMAL, sparta::InstrumentationNode::VIS_SUMMARY);
    std::unique_ptr<Counter> c3;
    if(option_exists) {
        c3.reset(new Counter(&sset0, "c3", "Counter 3 (SUMMARY VIS)", Counter::COUNT_NORMAL, sparta::InstrumentationNode::VIS_SUMMARY));
    }
    Counter c4(&sset0, "c4", "Counter 4 (SUMMARY VIS)", Counter::COUNT_NORMAL, sparta::InstrumentationNode::VIS_SUMMARY);

    StatisticDef sd1(&sset0, "s1", "Statistic Description", &sset0, "c1/c2", StatisticDef::VS_PERCENTAGE);
    Report r1("report 1", &root);
    r1.add(root.getChild("core0.stats.s1"));
    r1.addFile("test_report_options.yaml", true); // verbose

    r1.start();

    root.enterConfiguring();
    root.enterFinalized();

    c1 += 2;
    c2 += 4;
    if(c3) {
        *c3 += 5;
    }

    if(option_exists) {
        sparta::report::format::Text txt(&r1, "test_report_out_options.txt", std::ios::out);
        txt.setShowSimInfo(false);
        txt.write();
        EXPECT_FILES_EQUAL("test_report_out_options.txt", "test_report_out_options.txt.EXPECTED");
    }
    else {
        sparta::report::format::Text txt(&r1, "test_report_out_no_options.txt", std::ios::out);
        txt.setShowSimInfo(false);
        txt.write();
        EXPECT_FILES_EQUAL("test_report_out_no_options.txt", "test_report_out_no_options.txt.EXPECTED");
    }

    root.enterTeardown();
}


int main()
{
    // Observe all warnings
    sparta::log::Tap warnings(sparta::TreeNode::getVirtualGlobalNode(),
                            sparta::log::categories::WARN,
                            std::cerr);
    sparta::Scheduler sched;
    Report r("Report 0", nullptr, &sched); // Report which outlives the tree
    sched.finalize();

    // proceed to tick 1, nothing should happen, but time advancement
    sched.run(1, true, false);

    // Ok StatisticDefs to declare, but not to instantiate or evaluate
    tryStatisticDef1();
    tryStatisticDef2();
    tryStatisticDef3();
    tryStatisticDef4();

    tryReport0(); // Increments scheduler by 40
    tryReportWithOptions(true);
    tryReportWithOptions(false);

    { // Test object scope (to ensure teardown works)
        sparta::Scheduler sched;
        // Place into a tree which is in the same search scope as scheduler
        RootTreeNode root(sched.getSearchScope());
        TreeNode core0(&root, "core0", "Core 0");
        TreeNode core1(&root, "core1", "Core 1");
        StatisticSet sset0(&core0);
        StatisticSet sset1(&core1);
        EXPECT_TRUE(sset0.isAttached()); // Ensure that node constructed with parent arg is properly attached

        // Create and attach some clocks to be referenced in the statistics
        sparta::ClockManager  m(&sched);
        Clock::Handle c_root  = m.makeRoot();
        Clock::Handle c_half  = m.makeClock("half", c_root, 1, 2);
        Clock::Handle c_third = m.makeClock("third", c_root, 1, 3);
        uint32_t norm = m.normalize();
        std::cout << "ClockManager Norm(Global LCM): " << norm << std::endl;

        root.setClock(c_root.get());
        core0.setClock(c_half.get());
        core1.setClock(c_third.get());

        std::cout << sset0 << std::endl;

        // Ok Counters in tree
        Counter c1(&sset0, "c1", "Counter 1 (NORMAL VIS)",  Counter::COUNT_NORMAL, sparta::InstrumentationNode::VIS_NORMAL);
        Counter c2(&sset0, "c2", "Counter 2 (SUMMARY VIS)", Counter::COUNT_NORMAL, sparta::InstrumentationNode::VIS_SUMMARY);
        Counter c3(&sset0, "c3", "Counter 3 (HIDDEN VIS)",  Counter::COUNT_NORMAL, sparta::InstrumentationNode::VIS_HIDDEN);
        uint64_t c4_val = 0;
        ReadOnlyCounter c4(&sset0, "c4", "Counter 4 (NORMAL VIS)", Counter::COUNT_NORMAL, &c4_val);
        uint64_t c5_val = 5000;
        ReadOnlyCounter c5(&sset0, "c5", "Counter 5 (NORMAL VIS)", Counter::COUNT_LATEST, &c5_val, sparta::InstrumentationNode::VIS_NORMAL);

        Counter c1_c1(&sset1, "c1", "Counter 1", Counter::COUNT_NORMAL);
        Counter c1_c2(&sset1, "c2", "Counter 2", Counter::COUNT_NORMAL);
        Counter c1_c3(&sset1, "c3", "Counter 3", Counter::COUNT_NORMAL);
        Counter c1_c4(&sset1, "c4", "Counter 4", Counter::COUNT_NORMAL);

        // Ok StatisticDefs in tree
        StatisticDef sd1(&sset0, "s1", "Statistic Description", &sset0, "c1", StatisticDef::VS_PERCENTAGE);
        StatisticDef sd2(&sset0, "s2", "Statistic Description", &sset0, "c2", StatisticDef::VS_FRACTIONAL);
        StatisticDef sd3(&sset0, "s3", "Statistic Description", &core0, "stats.c3/stats.s4", StatisticDef::VS_ABSOLUTE); // Stat-reference
        StatisticDef sd4(&sset0, "s4", "Statistic Description", &sset0, "log2(16)/4+c3**c4"); // Expression on counters

        TreeNode dummy(&core0, "dummy", "Dummy node fore testing subtree-depth limits");
        StatisticSet sset_dummy(&dummy);
        Counter dummy_c1(&sset_dummy, "c1", "Counter 1 in dummy", Counter::COUNT_NORMAL, Counter::VIS_SUMMARY);
        Counter dummy_c2(&sset_dummy, "c2", "Counter 2 in dummy", Counter::COUNT_NORMAL, Counter::VIS_NORMAL);

        // Invalid StatisticDefs
        EXPECT_THROW(StatisticDef sd5(&sset0, "s5", "Statistic Description", &sset0, "1", StatisticDef::VS_INVALID));

        // Ok StatisticDef to ensure that cycle-detector is not overly aggressive
        StatisticDef sd_nocycle(&sset0, "s5", "Statistic Description", &sset0, "c4+c4*c4/c4"); // Non-cyclic

        EXPECT_EQUAL(c1.getVisibility(), sparta::InstrumentationNode::VIS_NORMAL);
        EXPECT_EQUAL(c2.getVisibility(), sparta::InstrumentationNode::VIS_SUMMARY);
        EXPECT_EQUAL(c3.getVisibility(), sparta::InstrumentationNode::VIS_HIDDEN);
        EXPECT_EQUAL(c4.getVisibility(), sparta::InstrumentationNode::VIS_NORMAL);


        // Finalization

        root.enterConfiguring();
        root.enterFinalized();
        sched.finalize();

        // proceed to tick 1, nothing should happen, but time advancement
        sched.run(1, true, false);

        EXPECT_NOTHROW( StatisticInstance sg_ok(&sd_nocycle); );


        // Ok StatisticInstance
        StatisticInstance si1(&sd1);


        // Report 1

        // Given RootTreeNode* root (top)
        // Start a report parsed relative to this context node.
        Report r1("report 1", &root);
        r1.add(root.getChild("core0.stats.s1"));
        r1.add(root.getChild("core0.stats.c1"));
        r1.add(root.getChildAs<StatisticDef>("core0.stats.s2"));
        r1.add(root.getChildAs<Counter>("core0.stats.c2"));


        // Report 0

        r.setContext(root.getChild("core0.stats"));
        r.add("c1"); // top.stats.bar

        r.setContext(root.getChild("core0.stats"));
        r.add("s1"); // top.stats.bar

        std::cout << "The tree from the top with builtins: " << std::endl
                  << root.renderSubtree(-1, true) << std::endl;

        // Creates more stat instances based on this report
        // Load the file based on the root context
        r.setContext(root.getSearchScope());
        std::cout << "\n\nr before adding file:\n" << r << std::endl;
        r.addFile("test_report.yaml",
                  true); // verbose
        std::cout << "\n\nr after adding file:\n" << r << std::endl;

        EXPECT_TRUE(r.hasStatistic("stat3"));
        EXPECT_NOTHROW( EXPECT_NOTEQUAL(r.getStatistic("stat3").getCounter(), nullptr));
        EXPECT_NOTHROW( EXPECT_EQUAL(r.getStatistic("stat3").getCounter(), core0.getChild("stats.c3")));
        EXPECT_THROW(r.add(core0.getChild("stats.s2"), "stat3")); // "stat3" key exists in this report
        EXPECT_THROW(r.add(core0.getChild("stats.does_not_exist"), "unique_stat_name")); // Cannot find this stat

        // Add to a report with convenient call chaining
        r.setContext(&core0);
        r.add("stats.s2")
             (core0.getChild("stats.s3"))
             ("stats.s4")
             ("cycles(stats.c1)") // Unnamed expression
             ("cycles") // Unnamed expression
             ;

        EXPECT_EQUAL(r.getSubreportDepth(), 1);
        EXPECT_EQUAL(r.getNumStatistics(), 13);
        EXPECT_EQUAL(r.getRecursiveNumStatistics(), 32);
        EXPECT_EQUAL(r.getNumSubreports(), 2);

        std::cout << "r\n" << r << std::endl;


        // Report 3

        Report r3(r); // Copy of r
        r3.setName("Report 3");
        EXPECT_EQUAL(r3.getName(), "Report 3");
        r3.setContext(&root);
        r3.add("core0.stats.c4"); // Add something NOT contained in r
        EXPECT_EQUAL(r3.getSubreportDepth(), 1);
        EXPECT_EQUAL(r3.getNumStatistics(), 14);
        EXPECT_EQUAL(r3.getRecursiveNumStatistics(), 33);
        EXPECT_EQUAL(r3.getNumSubreports(), 2);

        std::cout << "r3\n" << r3 << std::endl;


        // Report 4

        Report r4("Report 4", nullptr, &sched);
        r4.copyFromReport(r); // Copy of r
        Report& r4_1 = r4.addSubreport("Report 4.1");
        r4_1.add(core0.getChild("stats.c1"));
        EXPECT_EQUAL(r4.getSubreportDepth(), 1);
        EXPECT_EQUAL(r4.getNumStatistics(), 13);
        EXPECT_EQUAL(r4.getRecursiveNumStatistics(), 33);
        EXPECT_EQUAL(r4.getNumSubreports(), 3);

        Report& r4_2 = r4.addSubreport(r3); // Subreport
        r4_2.add(core0.getChild("stats.c2"));
        EXPECT_EQUAL(r4.getSubreportDepth(), 2);
        EXPECT_EQUAL(r4.getNumStatistics(), 13);
        EXPECT_EQUAL(r4.getRecursiveNumStatistics(), 67);
        EXPECT_EQUAL(r4.getNumSubreports(), 4);
        EXPECT_EQUAL(r4_2.getSubreportDepth(), 1);
        EXPECT_EQUAL(r4_2.getNumStatistics(), 15);
        EXPECT_EQUAL(r4_2.getRecursiveNumStatistics(), 34);
        EXPECT_EQUAL(r4_2.getNumSubreports(), 2);

        std::cout << "r4\n" << r4 << std::endl;

        // Report Ignore
        Report r_ignore;
        r_ignore.setContext(root.getSearchScope());
        r_ignore.addFile("test_ignore.yaml", true);
        EXPECT_THROW(r_ignore.addFile("test_ignore_fail.yaml"));


        // Report Wildcard

        std::cout << "The tree right before adding r5: " << std::endl
                  << root.renderSubtree(-1, true) << std::endl;
        Report r5;
        r5.setContext(&root);
        r5.addFile("test_report_wildcard.yaml");

        std::cout << "r5\n" << r5 << std::endl;

        {
            Report r6;
            r6.setContext(&root);
            EXPECT_THROW( r6.addFile("test_report_topreport_ILLEGAL.yaml"); );

            Report r7;
            r7.setContext(&root);
            EXPECT_THROW( r7.addFile("test_report_topsubreport_ILLEGAL.yaml"); );
        }

        // Report using Autopopulate feature

        Report r6;
        r6.setContext(root.getSearchScope());
        r6.addFile("test_autopopulate.yaml");


        // Report using a string

        const std::string report_def =
R"(name: "String-based report Autopopulation Test"
style:
    decimal_places: 3
content:
    top:
        subreport:
            name: Summary
            content:
                autopopulate:
                    attributes: vis:summary
                    max_report_depth: 0
                    max_recursion_depth: 2 # + leaves in ".core0.stats"
        subreport:
            name: All stats
            style:
                collapsible_children: no
            content:
                autopopulate:
                    attributes: "!=vis:hidden && !=vis:summary"
                    max_report_depth: 1
    scheduler:
        subreport:
            name: scheduler
            content:
                autopopulate : ""
        )";

        Report r7;
        r7.setContext(root.getSearchScope());
        r7.addDefinitionString(report_def);

        Report r8;
        r8.setContext(root.getSearchScope());
        EXPECT_EQUAL(sched.getElapsedTicks(), 0);
        EXPECT_EQUAL(r8.getStart(), 0);
        r8.addFile("test_autopopulate_multireport.yaml");

        Report r9;
        r9.setContext(root.getSearchScope());
        r9.addFile("test_report_multi_nested.yaml");

        // issue #311: referring to pre-defined expressions
        const std::string report_def2 =
R"(name: "String-based report Autopopulation Test"
content:
    top:
        subreport:
            name: Self Referring
            content:
                core0.stats:
                      "c1 + c2": c1_plus_c2
                      "c1_plus_c2 + cycles": c1_plus_c2_plus_cycles
        )";

        Report r10;
        r10.setContext(root.getSearchScope());
        r10.addDefinitionString(report_def2);

        // Create a report formatter to which we will append data over time
        sparta::report::format::CSV periodic_csv(&r5, "test_periodic.csv", std::ios::out);
        periodic_csv.write();

        Report r10_empty;
        sparta::report::format::CSV empty_report_csv(&r10_empty, "empty.csv", std::ios::out);
        empty_report_csv.write();

        Report r1_cp(r);
        r1_cp.setContext(root.getSearchScope());
        r1_cp.addFile("test_csv_subreport_test.yaml");
        sparta::report::format::CSV r1_subreport_test(&r1_cp, "test_csv_subreport.csv", std::ios::out);
        r1_subreport_test.write();

        // Run simulation a while

        sched.run(20, true); // Run UP TO tick 20, but not tick 20
        ++c1;
        c2 += 2;
        c3 += 3;
        c4_val += 4;
        r.start();
        std::cout << r << std::endl;
        EXPECT_EQUAL(r.getStatistic(0).getValue(), 0);
        periodic_csv.update();

        sched.run(20, true); // Run UP TO tick 40, but not tick 40
        ++c1;
        c2 += 2;
        c3 += 3;
        c4_val += 4;
        std::cout << r << std::endl;
        EXPECT_EQUAL(r.getStatistic(0).getValue(), 1);
        periodic_csv.update();

        // Update c5 before ending the report
        c5_val = BIG_COUNTER_VAL;
        EXPECT_EQUAL(c5.get(), BIG_COUNTER_VAL);
        EXPECT_EQUAL(r.getStatistic("stat5").getValue(), BIG_COUNTER_VAL); // Must be isntantaneous value, NOT delta because c5 is a COUNT_LATEST

        r.end();
        std::cout << "Ended report\n" << r << std::endl;
        EXPECT_EQUAL(r.getStatistic(0).getValue(), 1);

        sched.run(20, true);
        ++c1;
        c2 += 2;
        c3 += 3;
        c4_val += 4;
        std::cout << r << std::endl;
        EXPECT_EQUAL(r.getStatistic(0).getValue(), 1); // Same value because report ended
        periodic_csv.update();


        // Write report to a few files

        // dumb "dump" of report directly (no formatter)
        std::ofstream("test_report_out", std::ios::out) << r;
        std::ofstream("test_wildcard_report_out", std::ios::out) << r5;

        // Write formatted report to ostream using operator<<
        sparta::report::format::BasicHTML html_1(&r);
        html_1.setShowSimInfo(false);
        std::ofstream("test_report_out.html", std::ios::out) << html_1;

        sparta::report::format::Gnuplot gplt_1(&r);
        std::ofstream("test_report_out.gplt", std::ios::out) << gplt_1;


        // Write report using the "write" function of a formatter
        std::ofstream out_html("test_report_out2.html", std::ios::out);
        sparta::report::format::BasicHTML html_2(&r, out_html);
        html_2.setShowSimInfo(false);
        html_2.write();

        // Write to file based on filenamep
        sparta::report::format::BasicHTML wcr_html(&r5,
                                                 "test_wildcard_report_out.html",
                                                 std::ios::out);
        wcr_html.setShowSimInfo(false);
        wcr_html.write();

        // Write formatter using writeTo Note that this needs a clear
        {std::ofstream("test_report_out.csv", std::ios::out);} // Clear
        sparta::report::format::CSV r_csv(&r);
        r_csv.writeTo("test_report_out.csv");

        // Write using temporary formatter
        sparta::report::format::Text txt(&r, "test_report_out.txt", std::ios::out);
        txt.setShowSimInfo(false);
        txt.write();

        // Write using temporary formatter
        std::ofstream wildcard_out_csv("test_wildcard_report_out.csv", std::ios::out);
        sparta::report::format::CSV(&r5).writeToStream(wildcard_out_csv);

        // Write autopopualted report using HTML
        sparta::report::format::Text txt_6(&r6);
        txt_6.setShowSimInfo(false);
        std::ofstream("test_autopopulate.txt", std::ios::out) << txt_6;

        sparta::report::format::BasicHTML html_6(&r6);
        html_6.setShowSimInfo(false);
        std::ofstream("test_autopopulate.html", std::ios::out) << html_6;

        // Write string-specified autopopualted report using HTML
        sparta::report::format::BasicHTML html_4(&r7);
        html_4.setShowSimInfo(false);
        std::ofstream("test_autopopulate_from_string.html", std::ios::out) << html_4;

        // Write string-specified autopopualted report using HTML
        sparta::report::format::Text txt_8(&r8);
        txt_8.setShowSimInfo(false);
        std::ofstream("test_autopopulate_multireport.txt", std::ios::out) << txt_8;

        // Write string-specified autopopualted report with extra subreports using HTML
        sparta::report::format::Text txt_9(&r9);
        txt_9.setShowSimInfo(false);
        std::ofstream("test_autopopulate_multi_nested.txt", std::ios::out) << txt_9;

        // Check output files

        EXPECT_FILES_EQUAL("test_report_out",                    "test_report_out.EXPECTED");
        EXPECT_FILES_EQUAL("test_wildcard_report_out",           "test_wildcard_report_out.EXPECTED");
        EXPECT_FILES_EQUAL("test_report_out.gplt",               "test_report_out.gplt.EXPECTED");
        EXPECT_FILES_EQUAL("test_report_out.html",               "test_report_out.html.EXPECTED");
        EXPECT_FILES_EQUAL("test_report_out2.html",              "test_report_out2.html.EXPECTED");
        EXPECT_FILES_EQUAL("test_wildcard_report_out.html",      "test_wildcard_report_out.html.EXPECTED");
        EXPECT_FILES_EQUAL("test_report_out.csv",                "test_report_out.csv.EXPECTED");
        EXPECT_FILES_EQUAL("test_report_out.txt",                "test_report_out.txt.EXPECTED");
        EXPECT_FILES_EQUAL("test_wildcard_report_out.csv",       "test_wildcard_report_out.csv.EXPECTED");
        EXPECT_FILES_EQUAL("test_autopopulate.txt",              "test_autopopulate.txt.EXPECTED");
        EXPECT_FILES_EQUAL("test_autopopulate.html",             "test_autopopulate.html.EXPECTED");
        EXPECT_FILES_EQUAL("test_autopopulate_from_string.html", "test_autopopulate_from_string.html.EXPECTED");
        EXPECT_FILES_EQUAL("test_autopopulate_multireport.txt",  "test_autopopulate_multireport.txt.EXPECTED");
        EXPECT_FILES_EQUAL("test_autopopulate_multi_nested.txt", "test_autopopulate_multi_nested.txt.EXPECTED");
        EXPECT_FILES_EQUAL("test_periodic.csv",                  "test_periodic.csv.EXPECTED")

        // Print out some info about the report
        std::cout << "Context            : " << r.getContext() << std::endl;
        std::cout << "Name               : " << r.getName() << std::endl;
        std::cout << "Start              : " << r.getStart() << std::endl;
        std::cout << "End                : " << r.getEnd() << std::endl;
        const auto& subreps = r.getSubreports();
        (void) subreps;
        std::cout << "Num subreports     : " << r.getNumSubreports() << std::endl;
        std::cout << "Subreport depth    : " << r.getSubreportDepth() << std::endl;
        const std::vector<sparta::statistics::stat_pair_t>& immediate_stats = r.getStatistics();
        (void) immediate_stats;
        std::cout << "Num immediate stats: " << r.getNumStatistics() << std::endl;
        std::cout << "Num recursive stats: " << r.getRecursiveNumStatistics() << std::endl;


        // Render Tree for information purposes

        std::cout << "The tree from the top with builtins: " << std::endl
                  << root.renderSubtree(-1, true) << std::endl;
        std::cout << "The tree from the top without builtins: " << std::endl
                  << root.renderSubtree() << std::endl;
        std::cout << "The tree from sset0: " << std::endl
                  << sset0.renderSubtree(-1, true);

        root.enterTeardown();
    }

    // Done

    // Make sure this does not crash - disregard actual content for now
    EXPECT_NOTHROW( std::ofstream("immortal_report.txt", std::ios::out) << r; );

    // Report errors before drawing trees in case any nodes were attached which
    // should not have been.
    REPORT_ERROR;

    return ERROR_CODE;
}
