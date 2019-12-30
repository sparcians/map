/*!
 * \file Report_triggers.cpp
 * \brief Test for report trigger functionality
 */

#include "sparta/report/Report.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaTester.hpp"

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/report/Report.hpp"
#include "sparta/report/ReportRepository.hpp"
#include "sparta/app/ReportDescriptor.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/statistics/Counter.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/trigger/ExpressionTrigger.hpp"
#include "sparta/report/SubContainer.hpp"
#include "sparta/report/format/ReportHeader.hpp"
#include "sparta/app/ReportConfigInspection.hpp"

#include <iostream>

TEST_INIT;

using sparta::Scheduler;
using sparta::Counter;
using sparta::TreeNode;
using sparta::Report;
using sparta::ReportRepository;
using sparta::StatisticSet;
using sparta::RootTreeNode;
using sparta::ClockManager;
using sparta::Clock;
using sparta::NotificationSource;
using sparta::SubContainer;
using sparta::report::format::ReportHeader;

#define PRINT_ENTER_TEST \
  std::cout << std::endl; \
  std::cout << "*************************************************************" \
            << "*** Beginning '" << __FUNCTION__ << "'" \
            << "*************************************************************" \
            << std::endl;

struct FileDeleter {
    ~FileDeleter() {
        for (const auto & fname : files) {
            remove(fname.c_str());
        }
    }
    void add(const std::string & fname) {
        files.emplace_back(fname);
    }
private:
    std::vector<std::string> files;
};

/*!
 * \brief Positive tests in this file have the same general form for verifying
 * data values with various types of report triggers. As an example, say we have
 * a subreport SR1 whose start trigger is defined as: "notif.sourceA != 99"
 *
 * Each of the unit tests has vectors of expected values that highlight a data
 * point where a START or STOP should have occurred for the subreport. Here is
 * one such example:
 *
 *     std::vector<double> expected_values_sr1 = {
 *         0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 0, 1, 2, 3, 4, 5, 6, 7
 *                                                   ^
 *                                               54 != 99
 *                                         (notif.sourceA != 99)
 *                           (SR1:start -> computation window resets to zero)
 *     };
 *
 * This means that the 14th value had a notification payload value that WAS NOT
 * equal to 99, which was the condition for starting the subreport. Here is the
 * payload vector that goes with the above expected values (taken from one of the
 * unit tests in this file):
 *
 *     std::vector<uint64_t> payloadsA(20, 99);
 *     payloadsA[13] = 54;
 *
 * Which is just "shorthand" for:
 *
 *     std::vector<uint64_t> payload_values = {
 *         99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 54, 99, 99, 99, 99, 99, 99, 99
 *     };                                                       ^
 *                                                              :
 * Let's widen the expected values vector to line it up...      :
 *                                                              :
 *     std::vector<double> expected_values_sr1 = {              :
 *          0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12,  0,  1,  2,  3,  4,  5,  6,  7
 *                                                              ^
 *                                                          54 != 99
 *                                                   (notif.sourceA != 99)
 *                                     (SR1:start -> computation window resets to zero)
 *     };
 *
 * Each individual test tries to line up input and expected output vector
 * data values like this for clarity.
 *
 *                  a, b, c, d, e, f, g, ...
 *                        ^
 *                   (comparison)
 *               (condition satisfied)
 *                 (trigger result)
 */

/*!
 * \brief Verify invalid YAML contents throw errors as expected
 */
void run_negative_tests()
{
    PRINT_ENTER_TEST

    RootTreeNode root("top");
    TreeNode core0(&root, "core0", "Core 0");
    TreeNode core1(&root, "core1", "Core 1");
    StatisticSet sset0(&core0);
    StatisticSet sset1(&core1);

    Report r("Test");
    r.setContext(&root);

    std::cout <<
        "  [negative] Attempt to parse an expression "
        "that does not resolve to anything valid" << std::endl;
    EXPECT_THROW_MSG_CONTAINS(r.addFile("report_opts_negative1.yaml");,
        "The following trigger expression could not be parsed:");

    std::cout <<
        "  [negative] Expression that contains both && and || (unsupported)" << std::endl;
    EXPECT_THROW_MSG_CONTAINS(r.addFile("report_opts_negative2.yaml");,
        "Encountered trigger expression containing both '&&' and '||':");

    std::cout <<
        "  [negative] Unsupported comparison operator for counter triggers" << std::endl;
    EXPECT_THROW_MSG_CONTAINS(r.addFile("report_opts_negative3.yaml");,
        "CounterTrigger's only support '>=' since they respond to monotonically "
        "increasing counter values.");
    EXPECT_THROW_MSG_CONTAINS(r.addFile("report_opts_negative3b.yaml");,
        "CounterTrigger's only support '>=' since they respond to monotonically "
        "increasing counter values.");
    EXPECT_THROW_MSG_CONTAINS(r.addFile("report_opts_negative3c.yaml");,
        "CounterTrigger's only support '>=' since they respond to monotonically "
        "increasing counter values.");
    EXPECT_THROW_MSG_CONTAINS(r.addFile("report_opts_negative3d.yaml");,
        "CounterTrigger's only support '>=' since they respond to monotonically "
        "increasing counter values.");
    EXPECT_THROW_MSG_CONTAINS(r.addFile("report_opts_negative3e.yaml");,
        "CounterTrigger's only support '>=' since they respond to monotonically "
        "increasing counter values.");

    std::cout <<
        "  [negative] Unrecognized comparison operator for any trigger" << std::endl;
    EXPECT_THROW_MSG_CONTAINS(r.addFile("report_opts_negative4.yaml");,
        "Unable to parse the following notification:");

    {
        struct ClientCode {
            void respond() {
                std::cout << "Hello world!" << std::endl;
            }
        };

        ClientCode client;
        sparta::SpartaHandler cb = sparta::SpartaHandler::from_member<ClientCode, &ClientCode::respond>(
            &client, "ClientCode::respond");

        std::unique_ptr<sparta::trigger::ExpressionTrigger> trigger(
            new sparta::trigger::ExpressionTimeTrigger("MyNanoSecondTrigger", cb, "350 ns", nullptr));

        EXPECT_THROW_MSG_CONTAINS(trigger->reschedule();,
            "cannot be rescheduled since it is currently active");

        EXPECT_THROW_MSG_CONTAINS(trigger.reset(new sparta::trigger::ExpressionTimeTrigger(
                                      "MyCrazyUnitsTrigger", cb, "1400 crazies", nullptr));,
            "Unrecognized units found in what appeared to be a time-based expression");

        EXPECT_THROW_MSG_CONTAINS(trigger.reset(new sparta::trigger::ExpressionTimeTrigger(
                                      "AttemptedZeroTimeTarget", cb, "0 ns", nullptr));,
            "You may not specify a target time of 0");
    }

    root.enterTeardown();
}

/*!
 * \brief Independently reset computation windows using a single report definition YAML
 */
void independent_computation_windows_basic()
{
    PRINT_ENTER_TEST

    RootTreeNode root("top");
    TreeNode core0(&root, "core0", "Core 0");
    TreeNode core1(&root, "core1", "Core 1");
    StatisticSet sset0(&core0);
    StatisticSet sset1(&core1);

    Scheduler scheduler("test");
    std::shared_ptr<sparta::Clock> root_clk(
        std::make_shared<sparta::Clock>("test_clock", &scheduler));
    scheduler.finalize();
    root.setClock(root_clk.get());
    core0.setClock(root_clk.get());
    core1.setClock(root_clk.get());

    Counter core0_counter(&sset0, "c0", "Counter 0", Counter::COUNT_NORMAL);
    Counter core1_counter(&sset1, "c1", "Counter 1", Counter::COUNT_NORMAL);

    const std::string report_def = R"(
content:
    subreport:
        name: "SR1, [start @ 5]"
        trigger:
            start: "core0.stats.c0 >= 5"
        core0:
            autopopulate: true
    subreport:
        name: "SR2, [start @ 10]"
        trigger:
            start: "core1.stats.c1 >= 10"
        core1:
            autopopulate: true
)";

    Report r("Test");
    r.setContext(&root);
    r.addDefinitionString(report_def);

    //Expected values for subreport #1 (computation window resets at 5)
    std::vector<double> expected_values_sr1 = {
        0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
//                     ^
//                    >=5
//           ("core0.stats.c0 >= 5")
    };
    //Expected values for subreport #2 (computation window resets at 10)
    std::vector<double> expected_values_sr2 = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9
//                                    ^
//                                   >=10
//                          ("core1.stats.c1 >= 10")
    };

    for (size_t loop_idx = 0; loop_idx < 20; ++loop_idx) {
        scheduler.run(1, true);

        EXPECT_EQUAL(r.getSubreport(0).getStatistic("c0").getValue(), expected_values_sr1[loop_idx]);
        EXPECT_EQUAL(r.getSubreport(1).getStatistic("c1").getValue(), expected_values_sr2[loop_idx]);

        ++core0_counter;
        ++core1_counter;
    }

    root.enterTeardown();
}

/*!
 * \brief Start and stop report subtree computation windows at different ticks
 */
void independent_start_stop_computation_windows()
{
    PRINT_ENTER_TEST

    RootTreeNode root("top");
    TreeNode core0(&root, "core0", "Core 0");
    TreeNode core1(&root, "core1", "Core 1");
    StatisticSet sset0(&core0);
    StatisticSet sset1(&core1);

    Scheduler scheduler("test");
    std::shared_ptr<sparta::Clock> root_clk(
        std::make_shared<sparta::Clock>("test_clock", &scheduler));
    scheduler.finalize();
    root.setClock(root_clk.get());
    core0.setClock(root_clk.get());
    core1.setClock(root_clk.get());

    Counter core0_counter(&sset0, "c0", "Counter 0", Counter::COUNT_NORMAL);
    Counter core1_counter(&sset1, "c1", "Counter 1", Counter::COUNT_NORMAL);

    const std::string report_def = R"(
content:
    subreport:
        name: "SR1, [start @ 7, end @ 12]"
        trigger:
            start: "core0.stats.c0 >= 7"
            stop:  "core0.stats.c0 >= 12"
        core0:
            autopopulate: true
    subreport:
        name: "SR2, [start @ 4, end @ 13]"
        trigger:
            start: "core1.stats.c1 >= 4"
            stop:  "core1.stats.c1 >= 13"
        core1:
            autopopulate: true
)";

    Report r("Test");
    r.setContext(&root);
    r.addDefinitionString(report_def);

    //Expected values for subreport #1 (computation window resets at 7, ends at 12)
    std::vector<double> expected_values_sr1 = {
        0, 1, 2, 3, 4, 5, 6, 0, 1, 2, 3, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5
//                           ^              ^
//                          >=7            >=12
//   START @ "core0.stats.c0 >= 7"      STOP @ "core0.stats.c0 >= 12"
    };
    //Expected values for subreport #2 (computation window resets at 4, ends at 13)
    std::vector<double> expected_values_sr2 = {
        0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9, 9, 9, 9, 9, 9, 9
//                  ^                          ^
//                 >=4                        >=13
//   START @ "core0.stats.c0 >= 4"      STOP @ "core0.stats.c0 >= 13"
    };

    for (size_t loop_idx = 0; loop_idx < 20; ++loop_idx) {
        scheduler.run(1, true);

        EXPECT_EQUAL(r.getSubreport(0).getStatistic("c0").getValue(), expected_values_sr1[loop_idx]);
        EXPECT_EQUAL(r.getSubreport(1).getStatistic("c1").getValue(), expected_values_sr2[loop_idx]);

        ++core0_counter;
        ++core1_counter;
    }

    root.enterTeardown();
}

/*!
 * \brief Control computation windows with a notification source
 */
void notif_triggered_computation_windows()
{
    PRINT_ENTER_TEST

    RootTreeNode root("top");
    TreeNode core0(&root, "core0", "Core 0");
    TreeNode core1(&root, "core1", "Core 1");
    StatisticSet sset0(&core0);
    StatisticSet sset1(&core1);

    Scheduler scheduler("test");
    std::shared_ptr<sparta::Clock> root_clk(
        std::make_shared<sparta::Clock>("test_clock", &scheduler));
    scheduler.finalize();
    root.setClock(root_clk.get());
    core0.setClock(root_clk.get());
    core1.setClock(root_clk.get());

    Counter core0_counter(&sset0, "c0", "Counter 0", Counter::COUNT_NORMAL);
    Counter core1_counter(&sset1, "c1", "Counter 1", Counter::COUNT_NORMAL);

    const std::string report_def = R"(
content:
    subreport:
        name: "SR1, [start @ notif == 1, end @ notif == 4]"
        trigger:
            start: "notif.core_zero_notification_source == 1"
            stop:  "notif.core_zero_notification_source == 4"
        core0:
            autopopulate: true
    subreport:
        name: "SR2, [start @ notif == 6, end @ notif == 7]"
        trigger:
            start: "notif.core_one_notification_source == 6"
            stop:  "notif.core_one_notification_source == 7"
        core1:
            autopopulate: true
    subreport:
        name: 'SR3'
        trigger:
            start: 'notif.lots_of_payload_matches == 7 && notif.one_match_after_string_of_matches == 0'
        core0:
            autopopulate: true
)";

    NotificationSource<uint64_t> notifySR1(&core0,
        "core_zero_notification_source",
        "Test notification source for subreport #1 in this tree",
        "core_zero_notification_source");

    NotificationSource<uint64_t> notifySR2(&core1,
        "core_one_notification_source",
        "Test notification source for subreport #2 in this tree",
        "core_one_notification_source");

    NotificationSource<uint64_t> notifySR3a(&root,
        "lots_of_payload_matches",
        "Test notification source for subreport #3 in this tree",
        "lots_of_payload_matches");

    NotificationSource<uint64_t> notifySR3b(&root,
        "one_match_after_string_of_matches",
        "Test notification source for subreport #3 in this tree",
        "one_match_after_string_of_matches");

    Report r("Test");
    r.setContext(&root);
    r.addDefinitionString(report_def);

    //Let's fire off notification source values at every iteration - it should
    //not have any effect until the '==' target value computation is true
    std::vector<uint64_t> payloads = {
        9, 2, 6, 5, 3, 1, 8, 4, 7, 7, 7, 1, 3, 2, 9, 0, 1, 1, 2, 7
/*
Payloads:
  core0                ^     ^
                      (1)   (4)
  core1       ^                 ^
             (6)               (7)
*/
    };

    //Expected values for subreport #1
    std::vector<double> expected_values_sr1 = {
        0, 1, 2, 3, 4, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2
//                     ^     ^
//                    ==1   ==4
//  START @ "payload == 1"  STOP @ "payload == 4"
    };
    //Expected values for subreport #2
    std::vector<double> expected_values_sr2 = {
        0, 1, 0, 1, 2, 3, 4, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6
//            ^                 ^
//           ==6               ==7
//  START @ "payload == 6"  STOP @ "payload == 7"
    };

    std::vector<uint64_t> payloadsSR3 = {
        5, 5, 5, 5, 5, 5, 7, 7, 7, 5, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9
//                        ^  :  :     ^
//                           :  :     ^---------- this is the expected start
//                           X  X
//                          (these two, despite being ==7 matches,
//                           have already hit - they should no longer
//                           have any effect on the report's start window)
    };
    std::vector<double> expected_values_sr3 = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9
    };

    for (size_t loop_idx = 0; loop_idx < 20; ++loop_idx) {
        scheduler.run(1, true);

        notifySR1.postNotification(payloads[loop_idx]);
        notifySR2.postNotification(payloads[loop_idx]);
        notifySR3a.postNotification(payloadsSR3[loop_idx]);
        notifySR3b.postNotification(payloadsSR3[loop_idx]);

        EXPECT_EQUAL(r.getSubreport(0).getStatistic("c0").getValue(), expected_values_sr1[loop_idx]);
        EXPECT_EQUAL(r.getSubreport(1).getStatistic("c1").getValue(), expected_values_sr2[loop_idx]);
        EXPECT_EQUAL(r.getSubreport(2).getStatistic("c0").getValue(), expected_values_sr3[loop_idx]);

        ++core0_counter;
        ++core1_counter;
    }

    root.enterTeardown();
}

/*!
 * \brief Test all supported comparison operations for notification-based triggers
 */
void notification_source_comparison_ops()
{
    PRINT_ENTER_TEST

    RootTreeNode root("top");
    TreeNode child0(&root, "child0", "Child 0");
    TreeNode child1(&root, "child1", "Child 1");
    TreeNode child2(&root, "child2", "Child 2");
    TreeNode child3(&root, "child3", "Child 3");
    TreeNode child4(&root, "child4", "Child 4");

    StatisticSet sset0(&child0);
    StatisticSet sset1(&child1);
    StatisticSet sset2(&child2);
    StatisticSet sset3(&child3);
    StatisticSet sset4(&child4);

    Scheduler scheduler("test");
    std::shared_ptr<sparta::Clock> root_clk(
        std::make_shared<sparta::Clock>("test_clock", &scheduler));
    scheduler.finalize();
    root.setClock(root_clk.get());
    child0.setClock(root_clk.get());
    child1.setClock(root_clk.get());
    child2.setClock(root_clk.get());
    child3.setClock(root_clk.get());
    child4.setClock(root_clk.get());

    Counter c0(&sset0, "c0", "Counter 0", Counter::COUNT_NORMAL);
    Counter c1(&sset1, "c1", "Counter 1", Counter::COUNT_NORMAL);
    Counter c2(&sset2, "c2", "Counter 2", Counter::COUNT_NORMAL);
    Counter c3(&sset3, "c3", "Counter 3", Counter::COUNT_NORMAL);
    Counter c4(&sset4, "c4", "Counter 4", Counter::COUNT_NORMAL);

    const std::string report_def = R"(
content:
    subreport:
        name: "SR1, [start @ notify != 99]"
        trigger:
            start: "notif.sourceA != 99"
        child0:
            autopopulate: true
    subreport:
        name: "SR2, [start @ notif >= 104]"
        trigger:
            start: "notif.sourceB >= 104"
        child1:
            autopopulate: true
    subreport:
        name: "SR3, [start @ notif <= 33]"
        trigger:
            start: "notif.sourceC <= 33"
        child2:
            autopopulate: true
    subreport:
        name: "SR4, [start @ notif > 75]"
        trigger:
            start: "notif.sourceD > 75"
        child3:
            autopopulate: true
    subreport:
        name: "SR5, [start @ notif < 68]"
        trigger:
            start: "notif.sourceE < 68"
        child4:
            autopopulate: true
)";

    NotificationSource<uint64_t> notifySR1(&root,
        "sourceA",
        "Test notification source for operation '!='",
        "sourceA");

    NotificationSource<uint64_t> notifySR2(&root,
        "sourceB",
        "Test notification source for operation '>='",
        "sourceB");

    NotificationSource<uint64_t> notifySR3(&root,
        "sourceC",
        "Test notification source for operation '<='",
        "sourceC");

    NotificationSource<uint64_t> notifySR4(&root,
        "sourceD",
        "Test notification source for operation '>'",
        "sourceD");

    NotificationSource<uint64_t> notifySR5(&root,
        "sourceE",
        "Test notification source for operation '<'",
        "sourceE");

    Report r("Test");
    r.setContext(&root);
    r.addDefinitionString(report_def);

    std::vector<uint64_t> payloadsA(20, 99);
    payloadsA[13] = 54;
//                        payloadsA = {
//     99, 99, 99, ...................., 99, 99, 54, 99, 99, 99, 99, 99, 99
//                                                ^
//                                           (54 != 99)

    std::vector<double> expected_values_sr1 = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 0, 1, 2, 3, 4, 5, 6, 7
//                                                ^
//                                            54 != 99
//                                  START @ "payload != 99"
    };

    std::vector<uint64_t> payloadsB(1, 101);
    for (size_t i = 0; i < 19; ++i) {
        payloadsB.push_back(payloadsB.back() + 1);
    }
//                        payloadsB = {
//      101, 102, 103, 104, 105, 106, ......................
//                       ^
//                 (104 >= 104)

    std::vector<double> expected_values_sr2 = {
          0,   1,   2,   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12, 13, 14, 15, 16
//                       ^
//                     >=104
//            START @ "payload >= 104"
    };

    std::vector<uint64_t> payloadsC(1, 38);
    for (size_t i = 0; i < 19; ++i) {
        payloadsC.push_back(payloadsC.back() - 1);
    }
//                        payloadsC = {
//     38, 37, ....., 33, 32, 31, .................
//                     ^
//                (33 <= 33)

    std::vector<double> expected_values_sr3 = {
        0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
//                     ^
//                    <=33
//           START @ "payload <= 33"
    };

    std::vector<uint64_t> payloadsD = {
        44, 75, 98, 65, 12, 56, 74, 101, 500, 32, 54, 87, 23, 89, 6, 8, 22, 654, 1, 77
//               ^
//           (98 > 75)
    };
    std::vector<double> expected_values_sr4 = {
         0,  1,  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17
//               ^
//              >75
//      START @ "payload > 75"
    };

    std::vector<uint64_t> payloadsE = {
        89, 78, 79, 102, 235, 68, 68, 68, 23, 68, 2342, 67, 45, 67, 33, 65, 7777, 234, 43, 9
//                                         ^
//                                     (23 < 68)
    };
    std::vector<double> expected_values_sr5 = {
         0,  1,  2,   3,   4,  5,  6,  7,  0,  1,    2,  3,  4,  5,  6,  7,    8,   9, 10, 11
//                                         ^
//                                        <68
//                                     (23 < 68)
//                               START @ "payload < 68"
    };

    for (size_t loop_idx = 0; loop_idx < 20; ++loop_idx) {
        scheduler.run(1, true);

        notifySR1.postNotification(payloadsA[loop_idx]);
        notifySR2.postNotification(payloadsB[loop_idx]);
        notifySR3.postNotification(payloadsC[loop_idx]);
        notifySR4.postNotification(payloadsD[loop_idx]);
        notifySR5.postNotification(payloadsE[loop_idx]);

        EXPECT_EQUAL(r.getSubreport(0).getStatistic("c0").getValue(), expected_values_sr1[loop_idx]);
        EXPECT_EQUAL(r.getSubreport(1).getStatistic("c1").getValue(), expected_values_sr2[loop_idx]);
        EXPECT_EQUAL(r.getSubreport(2).getStatistic("c2").getValue(), expected_values_sr3[loop_idx]);
        EXPECT_EQUAL(r.getSubreport(3).getStatistic("c3").getValue(), expected_values_sr4[loop_idx]);
        EXPECT_EQUAL(r.getSubreport(4).getStatistic("c4").getValue(), expected_values_sr5[loop_idx]);

        ++c0, ++c1, ++c2, ++c3, ++c4;
    }

    root.enterTeardown();
}

/*!
 * \brief Control computation windows with a referenced trigger (a subreport
 * can rely on another subreport's triggers for start/end windows)
 */
void reference_triggered_computation_windows()
{
    PRINT_ENTER_TEST

    RootTreeNode root("top");
    TreeNode core0(&root, "core0", "Core 0");
    TreeNode core1(&root, "core1", "Core 1");
    StatisticSet sset0(&core0);
    StatisticSet sset1(&core1);

    Scheduler scheduler("test");
    std::shared_ptr<sparta::Clock> root_clk(
        std::make_shared<sparta::Clock>("test_clock", &scheduler));
    scheduler.finalize();
    root.setClock(root_clk.get());
    core0.setClock(root_clk.get());
    core1.setClock(root_clk.get());

    Counter core0_counter(&sset0, "c0", "Counter 0", Counter::COUNT_NORMAL);
    Counter core1_counter(&sset1, "c1", "Counter 1", Counter::COUNT_NORMAL);

    const std::string report_def = R"(
content:
    subreport:
        name: "SR1, [start @ 6, end @ 13]"
        trigger:
            tag:   t0
            start: "core0.stats.c0 >= 6"
            stop:  "core0.stats.c0 >= 13"
        core0:
            autopopulate: true
    subreport:
        name: "SR2, [start when SR1 starts, end when SR1 ends]"
        trigger:
            start: t0.start
            stop:  t0.stop
        core1:
            autopopulate: true
)";

    Report r("Test");
    r.setContext(&root);
    r.addDefinitionString(report_def);

    //Expected values for subreport #1
    std::vector<double> expected_values_sr1 = {
        0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 6, 7, 7, 7, 7, 7, 7, 7
//                        ^                    ^
//                       >=6                  >=13
//      START @ "core0.stats.c0 >= 6"    STOP @ "core0.stats.c0 >= 13"
    };
    //Expected values for subreport #2
    std::vector<double> expected_values_sr2 =
        expected_values_sr1; //subreport #2 references subreport #1's triggers,
                             //so their resulting windows should be identical

    for (size_t loop_idx = 0; loop_idx < 20; ++loop_idx) {
        scheduler.run(1, true);

        EXPECT_EQUAL(r.getSubreport(0).getStatistic("c0").getValue(), expected_values_sr1[loop_idx]);
        EXPECT_EQUAL(r.getSubreport(1).getStatistic("c1").getValue(), expected_values_sr2[loop_idx]);

        ++core0_counter;
        ++core1_counter;
    }

    root.enterTeardown();
}

/*!
 * \brief Trigger the computation window start for a report using a
 * combination of AND conditions.
 */
void logical_AND_triggered_computation_windows()
{
    PRINT_ENTER_TEST

    RootTreeNode root("top");
    TreeNode core0(&root, "core0", "Core 0");
    TreeNode core1(&root, "core1", "Core 1");
    StatisticSet sset0(&core0);
    StatisticSet sset1(&core1);

    Scheduler scheduler("test");
    std::shared_ptr<sparta::Clock> root_clk(
        std::make_shared<sparta::Clock>("test_clock", &scheduler));
    scheduler.finalize();
    root.setClock(root_clk.get());
    core0.setClock(root_clk.get());
    core1.setClock(root_clk.get());

    Counter core0_counter(&sset0, "c0", "Counter 0", Counter::COUNT_NORMAL);
    Counter core1_counter(&sset1, "c1", "Counter 1", Counter::COUNT_NORMAL);

    const std::string report_def = R"(
content:
    subreport:
        name: "SR1, [start @ 6]"
        trigger:
            tag:   t0
            start: "core0.stats.c0 >= 6"
        core0:
            autopopulate: true
    subreport:
        name: "SR2, [start @ 9]"
        trigger:
            tag:   t1
            start: "core1.stats.c1 >= 9"
        core1:
            autopopulate: true
    subreport:
        name: "SR3, [start when t0 and t1 have both started]"
        trigger:
            start: "t0.start && t1.start"
        core*:
            autopopulate: true
)";

    Report r;
    r.setContext(&root);
    r.addDefinitionString(report_def);

    //Expected values for subreport #1
    std::vector<double> expected_values_sr1 = {
        0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13
//                        ^
//                       >=6
//         t0.START @ "core0.stats.c0 >= 6"
    };
    //Expected values for subreport #2
    std::vector<double> expected_values_sr2 = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10
//                                 ^
//                                >=9
//                  t1.START @ "core1.stats.c1 >= 9"
    };
    //Expected values for subreport #3
    std::vector<double> expected_values_sr3_cX = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10
//                                 ^
//                             >=6 && >=9
//                 sr3.START @ (t0.START && t1.START)
    };

    for (size_t loop_idx = 0; loop_idx < 20; ++loop_idx) {
        scheduler.run(1, true);

        EXPECT_EQUAL(r.getSubreport(0).getStatistic("c0").getValue(), expected_values_sr1[loop_idx]);
        EXPECT_EQUAL(r.getSubreport(1).getStatistic("c1").getValue(), expected_values_sr2[loop_idx]);

        EXPECT_EQUAL(r.getSubreport(2).getStatistic("c0").getValue(), expected_values_sr3_cX[loop_idx]);
        EXPECT_EQUAL(r.getSubreport(2).getStatistic("c1").getValue(), expected_values_sr3_cX[loop_idx]);

        ++core0_counter;
        ++core1_counter;
    }

    root.enterTeardown();
}

/*!
 * \brief Trigger the computation window start for a report using a
 * combination of OR conditions.
 */
void logical_OR_triggered_computation_windows()
{
    PRINT_ENTER_TEST

    RootTreeNode root("top");
    TreeNode core0(&root, "core0", "Core 0");
    TreeNode core1(&root, "core1", "Core 1");
    StatisticSet sset0(&core0);
    StatisticSet sset1(&core1);

    Scheduler scheduler("test");
    std::shared_ptr<sparta::Clock> root_clk(
        std::make_shared<sparta::Clock>("test_clock", &scheduler));
    scheduler.finalize();
    root.setClock(root_clk.get());
    core0.setClock(root_clk.get());
    core1.setClock(root_clk.get());

    Counter core0_counter(&sset0, "c0", "Counter 0", Counter::COUNT_NORMAL);
    Counter core1_counter(&sset1, "c1", "Counter 1", Counter::COUNT_NORMAL);

    const std::string report_def = R"(
content:
    subreport:
        name: "SR1, [start @ 6]"
        trigger:
            tag:   t0
            start: "core0.stats.c0 >= 6"
        core0:
            autopopulate: true
    subreport:
        name: "SR2, [start @ 9]"
        trigger:
            tag:   t1
            start: "core1.stats.c1 >= 9"
        core1:
            autopopulate: true
    subreport:
        name: "SR3, [start when either SR1 or SR2 has started]"
        trigger:
            start: "t0.start || t1.start"
        core*:
            autopopulate: true
)";

    Report r;
    r.setContext(&root);
    r.addDefinitionString(report_def);

    //Expected values for subreport #1
    std::vector<double> expected_values_sr1 = {
        0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13
//                        ^
//                       >=6
//         t0.START @ "core0.stats.c0 >= 6"
    };
    //Expected values for subreport #2
    std::vector<double> expected_values_sr2 = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10
//                                 ^
//                                >=9
//                  t1.START @ "core1.stats.c1 >= 9"
    };
    //Expected values for subreport #3
    std::vector<double> expected_values_sr3_cX = {
        0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13
//                        ^
//                    >=6 || >=9
//         sr3.START @ "t0.START || t1.START"
    };

    for (size_t loop_idx = 0; loop_idx < 20; ++loop_idx) {
        scheduler.run(1, true);

        EXPECT_EQUAL(r.getSubreport(0).getStatistic("c0").getValue(), expected_values_sr1[loop_idx]);
        EXPECT_EQUAL(r.getSubreport(1).getStatistic("c1").getValue(), expected_values_sr2[loop_idx]);

        EXPECT_EQUAL(r.getSubreport(2).getStatistic("c0").getValue(), expected_values_sr3_cX[loop_idx]);
        EXPECT_EQUAL(r.getSubreport(2).getStatistic("c1").getValue(), expected_values_sr3_cX[loop_idx]);

        ++core0_counter;
        ++core1_counter;
    }

    root.enterTeardown();
}

/*!
 * \brief Test several combinations of && and || in the same trigger expression, without tags
 */
void multi_sub_expressions_AND_OR()
{
    PRINT_ENTER_TEST

    RootTreeNode root("top");
    TreeNode core0(&root, "core0", "Core 0");

    StatisticSet sset0(&core0);

    Scheduler scheduler("test");
    std::shared_ptr<sparta::Clock> root_clk(
        std::make_shared<sparta::Clock>("test_clock", &scheduler));
    scheduler.finalize();
    root.setClock(root_clk.get());
    core0.setClock(root_clk.get());

    Counter cX(&sset0, "cX", "Counter X", Counter::COUNT_NORMAL);

    const std::string report_def = R"(
content:
    subreport:
        name: 'SR1'
        trigger:
            start: '(notif.A == 55 && notif.B > 70) || (notif.A < 40 && notif.D != 101)'
        core0:
            autopopulate: true

    subreport:
        name: 'SR2'
        trigger:
            start: '((notif.A < 12 || notif.B != 88) && notif.C == 30)'
        core0:
            autopopulate: true

    subreport:
        name: 'SR3'
        trigger:
            start: 'notif.A >= 900 || (notif.B < 33 && notif.C > 46 && notif.D == 90)'
        core0:
            autopopulate: true
)";

    const std::string bad_report_def = R"(
content:
    subreport:
        name: 'Using && and || without parentheses'
        trigger:
            start: 'notif.A == 5 && notif.B > 4 || notif.C < 89'
        core0:
            autopopulate: true
)";

    NotificationSource<uint64_t> notifyA(&root,
        "A", "Test notification source", "A");

    NotificationSource<uint64_t> notifyB(&root,
        "B", "Test notification source", "B");

    NotificationSource<uint64_t> notifyC(&root,
        "C", "Test notification source", "C");

    NotificationSource<uint64_t> notifyD(&root,
        "D", "Test notification source", "D");

    //Quick negative test...
    Report bad_report("Expect parse failure");
    bad_report.setContext(&root);
    EXPECT_THROW_MSG_CONTAINS(bad_report.addDefinitionString(bad_report_def);,
        "You may not use && and || in the same trigger expression "
        "without first grouping terms with parentheses");

    //Continue with the other tests
    Report r("Test");
    r.setContext(&root);
    r.addDefinitionString(report_def);

    //Push out a notification payload at every tick
    std::vector<uint64_t> payloadsA = {
         45,  46,  38, 30, 25,  10, 35, 45, 55, 65, 700, 800, 900, 950, 50, 40, 30, 20, 10,  0
//                  ^            ^           ^                 ^
//                (A<40)       (A<12)     (A==55)           (A>=900)
    };

    std::vector<uint64_t> payloadsB = {
         88,  88,  88, 88, 35,  50, 50, 14, 20, 20,  20,  20,  20,  20, 20, 20, 20, 20, 20, 20
//        ^                 ^            ^
//     (B>70)            (B!=88)       (B<33)
    };

    std::vector<uint64_t> payloadsC = {
          4,   5,   6, 10, 20,  30, 40, 45, 50, 60,   5,   5,   5,   5,  5,  5,  5,  5,  5,  5
//                               ^           ^
//                            (C==30)      (C>46)
    };

    std::vector<uint64_t> payloadsD = {
        101, 101, 101, 70, 101, 101, 90, 85, 80, 40, 101, 500, 500, 500, 75, 75, 75, 75, 75, 75
//                      ^             ^
//                   (D<80)        (D==90)
    };

//---                                                                                --------------
//        ^                                  ^                 (notif.A == 55 && notif.B > 70) || -
//                  ^   ^                                     (notif.A < 40 && notif.D != 101)    -
//                      *                                                                         -
//                      *                                                                         -
//                      * this is the expected start                                              -
//-------------------------------------------------------------------------------------------------
    std::vector<double> expected_values_sr1 = {
          0,   1,   2,  0,  1,   2,  3,  4,  5,  6,   7,   8,   9,  10, 11, 12, 13, 14, 15, 16
    };

//---                                                                                --------------
//                          ^    ^                             (notif.A < 12 || notif.B != 88) && -
//                               ^                                              notif.C == 30     -
//                               *                                                                -
//                               *                                                                -
//                               * this is the expected start                                     -
//-------------------------------------------------------------------------------------------------
    std::vector<double> expected_values_sr2 = {
          0,   1,   2,  3,  4,   0,  1,  2,  3,  4,   5,   6,   7,   8,  9, 10, 11, 12, 13, 14
    };

//---                                                                                --------------
//                                                             ^                notif.A >= 900 || -
//                                   ^   ^   ^                                   (notif.B < 33 && -
//                                                                                notif.C > 46 && -
//                                                                                notif.D == 90)  -
//                                           *
//                                           *
//                                           * this is the expected start
//-------------------------------------------------------------------------------------------------
    std::vector<double> expected_values_sr3 = {
          0,   1,   2,  3,  4,   5,  6,  7,  0,  1,   2,   3,   4,   5,  6,  7,  8,  9, 10, 11
    };

    //Run the simulation loop
    for (size_t loop_idx = 0; loop_idx < 20; ++loop_idx) {
        scheduler.run(1, true);

        notifyA.postNotification(payloadsA[loop_idx]);
        notifyB.postNotification(payloadsB[loop_idx]);
        notifyC.postNotification(payloadsC[loop_idx]);
        notifyD.postNotification(payloadsD[loop_idx]);

        EXPECT_EQUAL(r.getSubreport(0).getStatistic("cX").getValue(), expected_values_sr1[loop_idx]);
        EXPECT_EQUAL(r.getSubreport(1).getStatistic("cX").getValue(), expected_values_sr2[loop_idx]);
        EXPECT_EQUAL(r.getSubreport(2).getStatistic("cX").getValue(), expected_values_sr3[loop_idx]);

        ++cX;
    }

    root.enterTeardown();
}

/*!
 * \brief Add a specific test for the use case of:
 * START a subreport based on logical AND of two others' starts
 * STOP the same subreport based on logical OR of two others' stops
 */
void reference_triggers_for_report_overlap()
{
    PRINT_ENTER_TEST

    RootTreeNode root("top");

    TreeNode core0(&root, "core0", "Core 0");
    TreeNode core1(&root, "core1", "Core 1");
    StatisticSet sset0(&core0);
    StatisticSet sset1(&core1);

    Scheduler scheduler("test");
    std::shared_ptr<sparta::Clock> root_clk(
        std::make_shared<sparta::Clock>("test_clock", &scheduler));
    scheduler.finalize();
    root.setClock(root_clk.get());
    core0.setClock(root_clk.get());
    core1.setClock(root_clk.get());

    Counter core0_counter(&sset0, "c0", "Counter 0", Counter::COUNT_NORMAL);
    Counter core1_counter(&sset1, "c1", "Counter 1", Counter::COUNT_NORMAL);

    const std::string report_def = R"(
content:
    subreport:
        name: "SR1, [start @ 4, end @ notif == 5]"
        trigger:
            tag:   t0
            start: "core0.stats.c0 >= 4"
            stop:  "notif.channel_foo_variable == 5"
        core0:
            autopopulate: true
    subreport:
        name: "SR2, [start @ 8, end @ notif == 2]"
        trigger:
            tag:   t1
            start: "core1.stats.c1 >= 8"
            stop:  "notif.channel_bar_variable == 2"
        core1:
            autopopulate: true
    subreport:
        name: "SR3, [overlap(t0,t1)]"
        trigger:
            start: "t0.start && t1.start"
            stop:  "t0.stop  || t1.stop "
        core*:
            autopopulate: true
)";

    NotificationSource<uint64_t> notifySR1(&root,
        "channel_foo_variable",
        "Test notification source for subreport #1 in this tree",
        "channel_foo_variable");

    NotificationSource<uint64_t> notifySR2(&root,
        "channel_bar_variable",
        "Test notification source for subreport #2 in this tree",
        "channel_bar_variable");

    Report r;
    r.setContext(&root);
    r.addDefinitionString(report_def);

    //Deliver a notification source payload at each iteration
    std::vector<uint64_t> payloadsSR1 = {
        8, 3, 9, 1, 3, 6, 3, 4, 4, 7, 5, 2, 3, 7, 3, 4, 1, 3, 2, 7
//                                    ^
//                                   ==5
//                         (t0.STOP @ payload == 5)
    };
    std::vector<uint64_t> payloadsSR2 = {
        3, 7, 5, 9, 7, 3, 9, 7, 8, 8, 3, 2, 7, 6, 4, 2, 3, 4, 5, 7
//                                       ^           X
//                                      ==2       (should
//                                              not matter!)
//
//                           (t1.STOP @ payload == 2)
    };

    //Expected values for subreport #1
    std::vector<double> expected_values_sr1 = {
        0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6
//                  ^                 ^
//                 >=4             notif==5
//             t0.START @         t0.STOP @
//      "core0.stats.c0 >= 4"   "payload == 5"
    };
    //Expected values for subreport #2
    std::vector<double> expected_values_sr2 = {
        0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3
//                              ^        ^
//                             >=8    notif==2
//                      t1.START @    t1.STOP @
//            "core1.stats.c1 >= 8"  "payload == 2"
    };
    //Expected values for subreport #3
    std::vector<double> expected_values_sr3_cX = {
        0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2
//                              ^     ^
//                       t0.start     t0.stop
//                             &&     ||
//                       t1.start     t1.stop
    };

    for (size_t loop_idx = 0; loop_idx < 20; ++loop_idx) {
        scheduler.run(1, true);

        notifySR1.postNotification(payloadsSR1[loop_idx]);
        notifySR2.postNotification(payloadsSR2[loop_idx]);

        EXPECT_EQUAL(r.getSubreport(0).getStatistic("c0").getValue(), expected_values_sr1[loop_idx]);
        EXPECT_EQUAL(r.getSubreport(1).getStatistic("c1").getValue(), expected_values_sr2[loop_idx]);

        EXPECT_EQUAL(r.getSubreport(2).getStatistic("c0").getValue(), expected_values_sr3_cX[loop_idx]);
        EXPECT_EQUAL(r.getSubreport(2).getStatistic("c1").getValue(), expected_values_sr3_cX[loop_idx]);

        ++core0_counter;
        ++core1_counter;
    }

    root.enterTeardown();
}

void top_level_report_computation_windows()
{
    PRINT_ENTER_TEST

    RootTreeNode root("top");

    TreeNode core0(&root, "core0", "Core 0");
    TreeNode core1(&root, "core1", "Core 1");
    StatisticSet sset0(&core0);
    StatisticSet sset1(&core1);

    Scheduler scheduler("test");
    std::shared_ptr<sparta::Clock> root_clk(
        std::make_shared<sparta::Clock>("test_clock", &scheduler));
    scheduler.finalize();
    root.setClock(root_clk.get());
    core0.setClock(root_clk.get());
    core1.setClock(root_clk.get());

    Counter core0_counter(&sset0, "c0", "Counter 0", Counter::COUNT_NORMAL);
    Counter core1_counter(&sset1, "c1", "Counter 1", Counter::COUNT_NORMAL);

    //Note that the destination file for all reports is "1", which
    //is understood by the ReportDescriptor class to mean std::cout

    const std::string multi_reports_def = R"(
content:

    report:
        trigger:
            tag:   t0
            start: "core0.stats.c0 >= 7"
            stop:  "core0.stats.c0 >= 15"
        pattern:   top.core0
        def_file:  core_stats.yaml
        dest_file: 1

    report:
        skip:      0
        trigger:
            tag:   t1
            start: "core1.stats.c1 >= 9"
            stop:  "core1.stats.c1 >= 13"
        pattern:   top.core1
        def_file:  core_stats.yaml
        dest_file: 1

    report:
        skip:      false
        trigger:
            start: "t0.start && t1.start"
            stop:  "t0.stop  || t1.stop"
        pattern:   top
        def_file:  top_stats.yaml
        dest_file: 1

    report:
        skip:      1
        pattern:   top
        def_file:  top_stats.yaml
        dest_file: 1

    report:
        skip:      true
        pattern:   top
        def_file:  top_stats.yaml
        dest_file: 1
)";

    sparta::app::ReportDescVec descriptors =
        sparta::app::createDescriptorsFromDefinitionString(
            multi_reports_def, &root);

    //There are three reports specified in the definition string
    //above, so we should have exactly three descriptors
    sparta_assert(descriptors.size() == 3);

    ReportRepository repository(&root);
    std::vector<Report*> reports;

    for (auto & desc : descriptors) {
        std::vector<TreeNode*> roots;
        std::vector<std::vector<std::string>> replacements;
        root.getSearchScope()->findChildren(desc.loc_pattern,
                                            roots,
                                            replacements);

        const std::string def_file = desc.def_file;
        auto directoryHandle = repository.createDirectory(desc);

        std::unique_ptr<Report> r(new Report("TestReport", roots[0]));
        r->addFileWithReplacements(def_file, replacements[0], false);
        reports.push_back(r.get());

        repository.addReport(directoryHandle, std::move(r));
        repository.commit(&directoryHandle);
        sparta_assert(directoryHandle != nullptr, "Directory commit failure!");
    }

    //We should have the same number of reports as the total number
    //of descriptors parsed out of the definition file
    sparta_assert(reports.size() == 3);

    //Expected values for report #1, counter 'c0'
    std::vector<double> expected_values_report1_c0 = {
        0, 1, 2, 3, 4, 5, 6, 0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 8, 8, 8
//                           ^                       ^
//                           7                       15
//      r1.START @ "core0.stats.c0 >= 7"    r1.STOP @ "core0.stats.c0 >= 15"
    };

    //Expected values for report #2, counter 'c1'
    std::vector<double> expected_values_report2_c1 = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 1, 2, 3, 4, 4, 4, 4, 4, 4, 4
//                                 ^           ^
//                                 9           13
//      r2.START @ "core1.stats.c1 >= 9"    r2.STOP @ "core1.stats.c1 >= 13"
    };

    //Expected values for report #3 (top); both counters 'c0' and 'c1'
    //should have the same value as this report tracks top.core* with
    //its own computation window
    std::vector<double> expected_values_report3 = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 1, 2, 3, 4, 4, 4, 4, 4, 4, 4
//                                 ^           ^
//                                 9           13
//      r3.START @ "t0.start && t1.start"   r3.STOP @ "t0.stop || t1.stop"
//                    >=7         >=9                    >=15       >=13
    };

    for (size_t loop_idx = 0; loop_idx < 20; ++loop_idx) {
        scheduler.run(1, true);

        //Reports for individual cores
        const double r1_actual = reports[0]->getStatistic("c0").getValue();
        const double r2_actual = reports[1]->getStatistic("c1").getValue();

        const double r1_expected = expected_values_report1_c0[loop_idx];
        const double r2_expected = expected_values_report2_c1[loop_idx];

        EXPECT_EQUAL(r1_actual, r1_expected);
        EXPECT_EQUAL(r2_actual, r2_expected);

        //Report for overlap
        const double r3_c0_actual = reports[2]->
            getSubreport(0).getStatistic("c0").getValue();
        const double r3_c1_actual = reports[2]->
            getSubreport(1).getStatistic("c1").getValue();
        const double r3_expected = expected_values_report3[loop_idx];

        EXPECT_EQUAL(r3_c0_actual, r3_expected);
        EXPECT_EQUAL(r3_c1_actual, r3_expected);

        ++core0_counter;
        ++core1_counter;
    }

    root.enterTeardown();
}

void top_level_report_update_periods()
{
    PRINT_ENTER_TEST

    RootTreeNode root("top");

    TreeNode core0(&root, "core0", "Core 0");
    TreeNode core1(&root, "core1", "Core 1");
    StatisticSet sset0(&core0);
    StatisticSet sset1(&core1);

    Scheduler scheduler("test");
    std::shared_ptr<sparta::Clock> root_clk(
        std::make_shared<sparta::Clock>("test_clock", &scheduler));
    scheduler.finalize();
    root.setClock(root_clk.get());
    core0.setClock(root_clk.get());
    core1.setClock(root_clk.get());

    Counter core0_counter(&sset0, "c0", "Counter 0", Counter::COUNT_NORMAL);
    Counter core1_counter(&sset1, "c1", "Counter 1", Counter::COUNT_NORMAL);

    const std::string multi_reports_def = R"(
content:
    report:
        name:      'Create a time trigger with nanoseconds units'
        trigger:
            tag:   t0
            start: "core0.stats.c0 >= 7"
            update-time: "5000 ns"
        pattern:   top.core0
        def_file:  core_stats.yaml
        dest_file: core0_statistics.csv
        format:    csv

    report:
        name:      'Create a time trigger with microseconds units'
        trigger:
            tag:   t1
            start: "core1.stats.c1 >= 13"
            update-time: "7.5 us"
        pattern:   top.core1
        def_file:  core_stats.yaml
        dest_file: core1_statistics.csv
        format:    csv

    report:
        name:      'Create a time trigger with picoseconds units'
        trigger:
            start: "t0.start && t1.start"
            update-time: "3300000 ps"
        pattern:   top
        def_file:  top_stats.yaml
        dest_file: overlapping_statistics.csv
        format:    csv

    report:
        name:      'Create a time trigger without specifying any units'
        trigger:
            start: t0.start
            update-time: 654321
        pattern:   top.core1
        def_file:  core_stats.yaml
        dest_file: 1
)";

    FileDeleter deleter;
    deleter.add("core0_statistics.csv");
    deleter.add("core1_statistics.csv");
    deleter.add("overlapping_statistics.csv");

    sparta::app::ReportDescVec descriptors =
        sparta::app::createDescriptorsFromDefinitionString(
            multi_reports_def, &root);

    ReportRepository repository(&root);
    std::vector<Report*> reports;

    for (auto & desc : descriptors) {
        std::vector<TreeNode*> roots;
        std::vector<std::vector<std::string>> replacements;
        root.getSearchScope()->findChildren(desc.loc_pattern,
                                            roots,
                                            replacements);

        auto directoryHandle = repository.createDirectory(desc);

        std::unique_ptr<Report> r(new Report("TestReport", roots[0]));
        r->addFileWithReplacements(desc.def_file, replacements[0], false);
        reports.push_back(r.get());

        repository.addReport(directoryHandle, std::move(r));
        repository.commit(&directoryHandle);
        sparta_assert(directoryHandle != nullptr, "Directory commit failure!");
    }

    for (size_t loop_idx = 0; loop_idx < 20; ++loop_idx) {
        scheduler.run(1, true);
        ++core0_counter;
        ++core1_counter;
    }

    //todo_update (what's the best way to verify the data??)

    root.enterTeardown();
}

void counter_driven_update_intervals()
{
    PRINT_ENTER_TEST

    RootTreeNode root("top");

    TreeNode core0(&root, "core0", "Core 0");
    TreeNode core1(&root, "core1", "Core 1");
    StatisticSet sset0(&core0);
    StatisticSet sset1(&core1);

    Scheduler scheduler("test");
    std::shared_ptr<sparta::Clock> root_clk(
        std::make_shared<sparta::Clock>("test_clock", &scheduler));
    scheduler.finalize();
    root.setClock(root_clk.get());
    core0.setClock(root_clk.get());
    core1.setClock(root_clk.get());

    Counter core0_counter(&sset0, "c0", "Counter 0", Counter::COUNT_NORMAL);
    Counter core1_counter(&sset1, "c1", "Counter 1", Counter::COUNT_NORMAL);

    const std::string multi_reports_def = R"(
content:
    report:
        name:      'Counter trigger report updates (do not specify alignment)'
        trigger:
            start: 'core0.stats.c0 >= 4'
            update-count: 'core0.stats.c0 50'
        pattern:   top
        def_file:  top_stats.yaml
        dest_file: counter_updates.csv
        format:    csv

    report:
        name:      'Update trigger but no start trigger'
        trigger:
          update-count: 'core0.stats.c0 7'
        pattern:   top
        def_file:  top_stats.yaml
        dest_file: 1
)";

    FileDeleter deleter;
    deleter.add("counter_updates.csv");
    deleter.add("counter_updates_align.csv");
    deleter.add("counter_updates_noalign.csv");

    sparta::app::ReportDescVec descriptors =
        sparta::app::createDescriptorsFromDefinitionString(
            multi_reports_def, &root);

    ReportRepository repository(&root);
    std::vector<Report*> reports;

    for (auto & desc : descriptors) {
        std::vector<TreeNode*> roots;
        std::vector<std::vector<std::string>> replacements;
        root.getSearchScope()->findChildren(desc.loc_pattern,
                                            roots,
                                            replacements);

        auto directoryHandle = repository.createDirectory(desc);

        std::unique_ptr<Report> r(new Report("TestReport", roots[0]));
        r->addFileWithReplacements(desc.def_file, replacements[0], false);
        reports.push_back(r.get());

        repository.addReport(directoryHandle, std::move(r));
        repository.commit(&directoryHandle);
        sparta_assert(directoryHandle != nullptr, "Directory commit failure!");
    }

    for (size_t loop_idx = 0; loop_idx < 5000; ++loop_idx) {
        scheduler.run(1, true);
        ++core0_counter;
        ++core1_counter;
    }

    //todo_update (what's the best way to verify the data??)

    root.enterTeardown();
}

void cycle_driven_update_intervals()
{
    PRINT_ENTER_TEST

    RootTreeNode root("top");

    TreeNode core0(&root, "core0", "Core 0");
    TreeNode core1(&root, "core1", "Core 1");
    StatisticSet sset0(&core0);
    StatisticSet sset1(&core1);

    Scheduler scheduler("test");
    std::shared_ptr<sparta::Clock> root_clk(
        std::make_shared<sparta::Clock>("test_clock", &scheduler));
    scheduler.finalize();
    root.setClock(root_clk.get());

    sparta::ClockManager  m;
    Clock::Handle c0_clk = m.makeClock("FooClock", root_clk);
    Clock::Handle c1_clk = m.makeClock("BarClock", root_clk);
    core0.setClock(c0_clk.get());
    core1.setClock(c1_clk.get());

    Counter core0_counter(&sset0, "c0", "Counter 0", Counter::COUNT_NORMAL);
    Counter core1_counter(&sset1, "c1", "Counter 1", Counter::COUNT_NORMAL);

    const std::string multi_reports_def = R"(
content:
    report:
        name:      'Cycle trigger report updates (unnamed clock, so it should use the root clock)'
        trigger:
            start: 'core0.stats.c0 >= 4'
            update-cycles: 600
        pattern:   top
        def_file:  top_stats.yaml
        dest_file: cycle_updates_unnamed_clock.csv
        format:    csv
    report:
        name:      'Cycle trigger report updates (explicit clock name, core 0)'
        trigger:
            start: 'core0.stats.c0 >= 8'
            update-cycles: 'FooClock 450'
        pattern:   top.core0
        def_file:  core_stats.yaml
        dest_file: cycle_updates_named_clock_core0.csv
        format:    csv

    report:
        name:      'Cycle trigger report updates (explicit clock name, core 1)'
        trigger:
            start: 'core1.stats.c1 >= 13'
            update-cycles: 'BarClock 750'
        pattern:   top.core1
        def_file:  core_stats.yaml
        dest_file: cycle_updates_named_clock_core1.csv
        format:    csv

    report:
        name:      'Update trigger but no start trigger'
        trigger:
          update-cycles: 375
        pattern:   top
        def_file:  top_stats.yaml
        dest_file: 1
)";

    FileDeleter deleter;
    deleter.add("cycle_updates_unnamed_clock.csv");
    deleter.add("cycle_updates_named_clock_core0.csv");
    deleter.add("cycle_updates_named_clock_core1.csv");

    sparta::app::ReportDescVec descriptors =
        sparta::app::createDescriptorsFromDefinitionString(
            multi_reports_def, &root);

    ReportRepository repository(&root);
    std::vector<Report*> reports;

    for (auto & desc : descriptors) {
        std::vector<TreeNode*> roots;
        std::vector<std::vector<std::string>> replacements;
        root.getSearchScope()->findChildren(desc.loc_pattern,
                                            roots,
                                            replacements);

        auto directoryHandle = repository.createDirectory(desc);

        std::unique_ptr<Report> r(new Report("TestReport", roots[0]));
        r->addFileWithReplacements(desc.def_file, replacements[0], false);
        reports.push_back(r.get());

        repository.addReport(directoryHandle, std::move(r));
        repository.commit(&directoryHandle);
        sparta_assert(directoryHandle != nullptr, "Directory commit failure!");
    }

    for (size_t loop_idx = 0; loop_idx < 20; ++loop_idx) {
        scheduler.run(1, true);
        ++core0_counter;
        ++core1_counter;
    }

    //todo_update (what's the best way to verify the data??)

    root.enterTeardown();
}

void report_subcontainers()
{
    PRINT_ENTER_TEST

    typedef std::map<std::string, double> NamedDoubles;
    typedef std::set<std::string> UniqueStrings;

    NamedDoubles my_mapped_doubles;
    my_mapped_doubles["e"] = 2.71828;
    my_mapped_doubles["pi"] = 3.14159;

    UniqueStrings my_unique_strings;
    my_unique_strings.insert("fizz");
    my_unique_strings.insert("buzz");
    my_unique_strings.insert("fizzbuzz");

    SubContainer container;
    container.setContentByName("foo", my_mapped_doubles);
    container.setContentByName("bar", my_unique_strings);

    EXPECT_TRUE(container.hasContentNamed("foo"));
    EXPECT_TRUE(container.hasContentNamed("bar"));

    typedef std::vector<float> Floats;
    Floats & flts = container.getContentByName<Floats>("floats");
    EXPECT_TRUE(flts.empty());

    flts.push_back(4.4);
    flts.push_back(5.5);
    flts.push_back(6.6);
    EXPECT_EQUAL(container.getContentByName<Floats>("floats").size(), 3);

    {
        //Test double content
        auto & dbls = container.getContentByName<NamedDoubles>("foo");
        EXPECT_EQUAL(dbls["e"], 2.71828);
        EXPECT_EQUAL(dbls["pi"], 3.14159);

        //Add a new entry - these are references so we should not have
        //to call 'setContentByName()' again for the "foo" content
        dbls["abcd"] = 1234;
    }

    {
        //Test new double content
        auto & dbls = container.getContentByName<NamedDoubles>("foo");
        EXPECT_EQUAL(dbls["e"], 2.71828);
        EXPECT_EQUAL(dbls["pi"], 3.14159);
        EXPECT_EQUAL(dbls["abcd"], 1234);
    }

    //Test const API
    auto test_double = [](const SubContainer & sc,
                          const std::string & content_name,
                          const std::string & name,
                          const double expected) {
        auto & content = sc.getContentByName<NamedDoubles>(content_name);
        auto content_iter = content.find(name);
        if (content_iter == content.end()) {
            throw sparta::SpartaException("Invalid container value found for element named ") << name;
        }
        const double actual = content_iter->second;
        EXPECT_EQUAL(actual, expected);
    };

    test_double(container, "foo", "e", 2.71828);
    test_double(container, "foo", "pi", 3.14159);
    test_double(container, "foo", "abcd", 1234);

    //Test string content
    auto & strings = container.getContentByName<UniqueStrings>("bar");
    EXPECT_TRUE(strings.find("fizz") != strings.end());
    EXPECT_TRUE(strings.find("buzz") != strings.end());
    EXPECT_TRUE(strings.find("fizzbuzz") != strings.end());
    EXPECT_EQUAL(strings.size(), 3);
}

void report_header_overwrite()
{
    PRINT_ENTER_TEST

    auto remove_whitespace = [](const std::string & orig) {
        std::string no_whitespace(orig);
        boost::erase_all(no_whitespace, " ");
        boost::replace_all(no_whitespace, "#", "# ");
        return no_whitespace;
    };

    ReportHeader header;
    header.set("EmpID",    12345u);
    header.set("First", "John");
    header.set("Last",  "Doe");

    //Header names cannot have any whitespace
    EXPECT_THROW(header.set("white space for string",   "some_value"));
    EXPECT_THROW(header.set("white space for integral", 100u));

    //Integral and string header info cannot have the same variable name
    EXPECT_THROW(header.set("EmpID", "some_value"));
    EXPECT_THROW(header.set("First", 78u));

    //Commit the valid key-value pairs to the stream
    std::ostringstream oss;
    header.attachToStream(oss);
    header.writeHeaderToStreams();

    //Append some statistics...
    const std::string stats = "3, 5, 2, 7, 4, 5 \n "
                              "6, 7, 5, 8, 2, 1 \n";
    oss << stats;

    const std::string actual1 = oss.str();
    const std::string expected1 =
        "# EmpID=12345, First=John, Last=Doe \n" + stats;
    EXPECT_EQUAL(remove_whitespace(actual1), remove_whitespace(expected1));

    //Now that we have written the header to the stream, we cannot
    //overwrite string data or attempt to add new string data
    EXPECT_THROW(header.set("First",   "Jane"));
    EXPECT_THROW(header.set("new_key", "some_value"));

    //But we should be able to change integral header info in-place
    header.set("EmpID", 6789u);

    const std::string actual2 = oss.str();
    const std::string expected2 =
        "# EmpID=6789, First=John, Last=Doe \n" + stats;
    EXPECT_EQUAL(remove_whitespace(actual2), remove_whitespace(expected2));

    header.detachFromStream(oss);

    //Start another stream, putting some content in it before writing the header
    std::ostringstream oss2;
    const std::string pre_header = "# some, random, text\n";
    oss2 << pre_header;
    header.attachToStream(oss2);
    header.writeHeaderToStreams();

    //Append some stats
    oss2 << stats;
    const std::string actual3 = oss2.str();
    const std::string expected3 =
        pre_header + "# EmpID=6789, First=John, Last=Doe \n" + stats;
    EXPECT_EQUAL(remove_whitespace(actual3), remove_whitespace(expected3));

    //Overwrite some integral header data
    header.set("EmpID", 5555u);

    //Verify the overwritten header data
    const std::string actual4 = oss2.str();
    const std::string expected4 =
        pre_header + "# EmpID=5555, First=John, Last=Doe \n" + stats;
    EXPECT_EQUAL(remove_whitespace(actual4), remove_whitespace(expected4));

    header.detachFromStream(oss2);

    //Verify that we cannot connect a header writer to stdout
    EXPECT_THROW(header.attachToStream(std::cout));
}

void trigger_internals()
{
    PRINT_ENTER_TEST

    RootTreeNode root("top");
    TreeNode core0(&root, "core0", "Core 0");
    TreeNode core1(&root, "core1", "Core 1");
    StatisticSet sset0(&core0);
    StatisticSet sset1(&core1);

    Scheduler scheduler("test");
    sparta::Clock clk("test_clock", &scheduler);
    scheduler.finalize();
    root.setClock(&clk);
    core0.setClock(&clk);
    core1.setClock(&clk);

    Counter core0_counter(&sset0, "c0", "Counter 0", Counter::COUNT_NORMAL);
    Counter core1_counter(&sset1, "c1", "Counter 1", Counter::COUNT_NORMAL);

    NotificationSource<uint64_t> notifier(&root,
        "foo", "Test notification source", "foo");
    (void) notifier;

    struct ClientCode {
        void respond() {
            std::cout << "Hello world!" << std::endl;
        }
        sparta::SpartaHandler getHandler() {
            return CREATE_SPARTA_HANDLER(ClientCode, respond);
        }
    };

    ClientCode client;
    sparta::SpartaHandler cb = client.getHandler();

    auto get_total_internal_triggers = [](
        const sparta::trigger::ExpressionTrigger::ExpressionTriggerInternals & internals)
    {
        return internals.num_counter_triggers_ +
               internals.num_cycle_triggers_ +
               internals.num_time_triggers_ +
               internals.num_notif_triggers_;
    };

    {
        const std::string expression = "core0.stats.c0 >= 5";
        sparta::trigger::ExpressionTrigger trigger("Dummy", cb, expression, &root, nullptr);
        EXPECT_EQUAL(trigger.getInternals().num_counter_triggers_, 1);
        EXPECT_EQUAL(get_total_internal_triggers(trigger.getInternals()), 1);
    }

    {
        const std::string expression = "core0.stats.c0 >= 5 || core1.stats.c1 >= 7";
        sparta::trigger::ExpressionTrigger trigger("Dummy", cb, expression, &root, nullptr);
        EXPECT_EQUAL(trigger.getInternals().num_counter_triggers_, 2);
        EXPECT_EQUAL(get_total_internal_triggers(trigger.getInternals()), 2);
    }

    {
        const std::string expression = "core0.stats.c0 65";
        sparta::trigger::ExpressionCounterTrigger trigger("Dummy", cb, expression, false, &root);
        EXPECT_EQUAL(trigger.getInternals().num_counter_triggers_, 1);
        EXPECT_EQUAL(get_total_internal_triggers(trigger.getInternals()), 1);
    }

    {
        const std::string expression = "125";
        sparta::trigger::ExpressionCycleTrigger trigger("Dummy", cb, expression, &root);
        EXPECT_EQUAL(trigger.getInternals().num_cycle_triggers_, 1);
        EXPECT_EQUAL(get_total_internal_triggers(trigger.getInternals()), 1);
    }

    {
        const std::string expression = "1.5 ns";
        sparta::trigger::ExpressionTimeTrigger trigger("Dummy", cb, expression, &root);
        EXPECT_EQUAL(trigger.getInternals().num_time_triggers_, 1);
        EXPECT_EQUAL(get_total_internal_triggers(trigger.getInternals()), 1);
    }

    {
        const std::string expression = "notif.foo == 5";
        sparta::trigger::ExpressionTrigger trigger("Dummy", cb, expression, &root, nullptr);
        EXPECT_EQUAL(trigger.getInternals().num_notif_triggers_, 1);
        EXPECT_EQUAL(get_total_internal_triggers(trigger.getInternals()), 1);
    }

    {
        const std::string expression = "notif.foo <= 88 || notif.foo <= 22";
        sparta::trigger::ExpressionTrigger trigger("Dummy", cb, expression, &root, nullptr);
        EXPECT_EQUAL(trigger.getInternals().num_notif_triggers_, 2);
        EXPECT_EQUAL(get_total_internal_triggers(trigger.getInternals()), 2);
    }

    root.enterTeardown();
}

void cumulative_statistics_start_from_zero()
{
    PRINT_ENTER_TEST

    RootTreeNode root("top");

    TreeNode core0(&root, "core0", "Core 0");
    StatisticSet sset0(&core0);

    Scheduler scheduler("test");
    sparta::Clock clk("test_clock", &scheduler);
    core0.setClock(&clk);

    Counter core0_counter(&sset0, "c0", "Counter 0", Counter::COUNT_NORMAL);
    scheduler.finalize();

    const std::string multi_reports_def = R"(
content:

    report:
        trigger:
            start: "core0.stats.c0 >= 7"
        pattern:   top.core0
        def_file:  core_stats.yaml
        dest_file: out.csv
        format:    csv_cumulative
)";

    sparta::app::ReportDescVec descriptors =
        sparta::app::createDescriptorsFromDefinitionString(
            multi_reports_def, &root);

    //There is one report specified in the definition string
    //above, so we should have exactly one descriptor
    sparta_assert(descriptors.size() == 1);

    ReportRepository repository(&root);
    std::vector<Report*> reports;

    for (auto & desc : descriptors) {
        std::vector<TreeNode*> roots;
        std::vector<std::vector<std::string>> replacements;
        root.getSearchScope()->findChildren(desc.loc_pattern,
                                            roots,
                                            replacements);

        const std::string def_file = desc.def_file;
        auto directoryHandle = repository.createDirectory(desc);

        std::unique_ptr<Report> r(new Report("TestReport", roots[0]));
        r->addFileWithReplacements(def_file, replacements[0], false);
        reports.push_back(r.get());

        repository.addReport(directoryHandle, std::move(r));
        repository.commit(&directoryHandle);
        sparta_assert(directoryHandle != nullptr, "Directory commit failure!");
    }

    //We should have the same number of reports as the total number
    //of descriptors parsed out of the definition file
    sparta_assert(reports.size() == 1);

    //Start or restart all reports. If already started, this should
    //not have any effect on the underlying statistics since this
    //is 'csv_cumulative' mode.
    auto restart_reports = [&reports]() {
        for (auto r : reports) {
            r->start();
        }
    };

    //Expected values for report #1, counter 'c0'
    std::vector<double> expected_values_report1_c0 = {
        0, 1, 2, 3, 4, 5, 6, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12
//                           ^
//                           7
//      r1.START @ "core0.stats.c0 >= 7"
    };

    const size_t num_sim_steps = expected_values_report1_c0.size();
    for (size_t loop_idx = 0; loop_idx < num_sim_steps; ++loop_idx) {
        scheduler.run(1, true);

        //Reports for individual cores
        const double r1_actual = reports[0]->getStatistic("c0").getValue();
        const double r1_expected = expected_values_report1_c0[loop_idx];
        EXPECT_EQUAL(r1_actual, r1_expected);

        //Try to restart the reports... no effect!
        if (loop_idx > 7) {
            restart_reports();
        }

        //Increment the counter
        ++core0_counter;
    }

    root.enterTeardown();
}

void report_trigger_config_inspection()
{
    const std::string pattern = "_global";
    const std::string def_file = "simple_stats.yaml";
    const std::string dest_file = "out.csv";
    const std::string format = "csv";

    {
        //Create a descriptor with no triggers at all, and verify the
        //hasAnyReportTriggers() method returns the expected answer
        sparta::app::ReportDescriptor rd(pattern, def_file, dest_file, format);
        EXPECT_EQUAL(rd.getDescriptorPattern(), pattern);
        EXPECT_EQUAL(rd.getDescriptorDefFile(), def_file);
        EXPECT_EQUAL(rd.getDescriptorDestFile(), dest_file);
        EXPECT_EQUAL(rd.getDescriptorFormat(), format);
        EXPECT_FALSE(sparta::app::hasAnyReportTriggers(&rd));
    }

    {
        sparta::app::ReportDescriptor rd(pattern, def_file, dest_file, format);
        std::unordered_map<std::string, std::string> triggers;

        //Make sure we start out with no triggers, then after we add a
        //start trigger, verify the hasStartTrigger() method returns the
        //expected answer
        EXPECT_FALSE(sparta::app::hasStartTrigger(&rd));
        triggers = {{"start", "top.core0.rob.stats.total_number_retired >= 1000"}};
        rd.extensions_["trigger"] = triggers;
        EXPECT_TRUE(sparta::app::hasStartTrigger(&rd));
        rd.extensions_.erase("trigger");

        //Make sure we don't have an update trigger yet, then after we
        //add an update trigger, verify the hasUpdateTrigger() method
        //returns the expected answer
        EXPECT_FALSE(sparta::app::hasUpdateTrigger(&rd));
        triggers = {{"update-time", "2 ns"}};
        rd.extensions_["trigger"] = triggers;
        EXPECT_TRUE(sparta::app::hasUpdateTrigger(&rd));
        rd.extensions_.erase("trigger");

        //Make sure we don't have a stop trigger yet, then after we
        //add a stop trigger, verify the hasStopTrigger() method
        //returns the expected answer
        EXPECT_FALSE(sparta::app::hasStopTrigger(&rd));
        triggers = {{"stop", "top.core0.rob.stats.total_number_retired >= 9000"}};
        rd.extensions_["trigger"] = triggers;
        EXPECT_TRUE(sparta::app::hasStopTrigger(&rd));
        rd.extensions_.erase("trigger");

        //Make sure we don't have a toggle trigger yet, then after we
        //add a toggle trigger, verify the hasToggleTrigger() method
        //returns the expected answer
        EXPECT_FALSE(sparta::app::hasToggleTrigger(&rd));
        triggers = {{"whenever", "notif.testing_notif_channel < 100"}};
        rd.extensions_["trigger"] = triggers;
        EXPECT_TRUE(sparta::app::hasToggleTrigger(&rd));
        auto whenever_trig_expr = sparta::app::getTriggerExpression(&rd, "whenever");
        EXPECT_TRUE(whenever_trig_expr.isValid());
        EXPECT_EQUAL(whenever_trig_expr.getValue(), "notif.testing_notif_channel<100");
        rd.extensions_.erase("trigger");

        //Make sure we don't have an on-demand trigger yet, then after we
        //add an on-demand trigger, verify the hasOnDemandTrigger() method
        //returns the expected answer
        EXPECT_FALSE(sparta::app::hasOnDemandTrigger(&rd));
        triggers = {{"update-whenever", "notif.testing_notif_channel == 999"}};
        rd.extensions_["trigger"] = triggers;
        EXPECT_TRUE(sparta::app::hasOnDemandTrigger(&rd));
        auto whenever_trig_notif_name = sparta::app::getNotifSourceForUpdateTrigger(&rd);
        EXPECT_TRUE(whenever_trig_notif_name.isValid());
        EXPECT_EQUAL(whenever_trig_notif_name.getValue(), "testing_notif_channel");
        rd.extensions_.erase("trigger");

        auto update_count_trig_expr = sparta::app::getTriggerExpression(&rd, "update-count");
        EXPECT_FALSE(update_count_trig_expr.isValid());

        auto update_cycles_trig_expr = sparta::app::getTriggerExpression(&rd, "update-cycles");
        EXPECT_FALSE(update_cycles_trig_expr.isValid());

        auto update_time_trig_expr = sparta::app::getTriggerExpression(&rd, "update-time");
        EXPECT_FALSE(update_time_trig_expr.isValid());

        auto start_trig_notif_name = sparta::app::getNotifSourceForStartTrigger(&rd);
        EXPECT_FALSE(start_trig_notif_name.isValid());

        auto stop_trig_notif_name = sparta::app::getNotifSourceForStopTrigger(&rd);
        EXPECT_FALSE(stop_trig_notif_name.isValid());
    }

    {
        sparta::app::ReportDescriptor rd(pattern, def_file, dest_file, format);
        std::unordered_map<std::string, std::string> triggers;

        //Verify that we start out with no notification source triggers
        //for start, update, and stop
        EXPECT_FALSE(sparta::app::hasNotifSourceStartTriggerNamed(&rd, "foobar"));
        EXPECT_FALSE(sparta::app::hasNotifSourceUpdateTriggerNamed(&rd, "foobar"));
        EXPECT_FALSE(sparta::app::hasNotifSourceStopTriggerNamed(&rd, "foobar"));

        //Add a notification source start trigger, and verify that it is
        //the only trigger type that is tied to a notification source event
        triggers["start"] = "notif.foobar != 40";
        rd.extensions_["trigger"] = triggers;
        EXPECT_TRUE(sparta::app::hasNotifSourceStartTriggerNamed(&rd, "foobar"));
        EXPECT_FALSE(sparta::app::hasNotifSourceUpdateTriggerNamed(&rd, "foobar"));
        EXPECT_FALSE(sparta::app::hasNotifSourceStopTriggerNamed(&rd, "foobar"));

        //Now add a notification source update trigger, and verify that we
        //have both a start and an update trigger tied to a notification
        //source event
        triggers["update-whenever"] = "notif.foobar == 333";
        rd.extensions_["trigger"] = triggers;
        EXPECT_TRUE(sparta::app::hasNotifSourceStartTriggerNamed(&rd, "foobar"));
        EXPECT_TRUE(sparta::app::hasNotifSourceUpdateTriggerNamed(&rd, "foobar"));
        EXPECT_FALSE(sparta::app::hasNotifSourceStopTriggerNamed(&rd, "foobar"));

        //Add a notification source stop trigger, and verify that all three
        //trigger types are tied to notification source events
        triggers["stop"] = "notif.foobar == 200";
        rd.extensions_["trigger"] = triggers;
        EXPECT_TRUE(sparta::app::hasNotifSourceStartTriggerNamed(&rd, "foobar"));
        EXPECT_TRUE(sparta::app::hasNotifSourceUpdateTriggerNamed(&rd, "foobar"));
        EXPECT_TRUE(sparta::app::hasNotifSourceStopTriggerNamed(&rd, "foobar"));

        //Before we start removing any triggers, make sure we are told that
        //"No, there are no notification source triggers named 'fizbiz'"
        EXPECT_FALSE(sparta::app::hasNotifSourceStartTriggerNamed(&rd, "fizbiz"));
        EXPECT_FALSE(sparta::app::hasNotifSourceUpdateTriggerNamed(&rd, "fizbiz"));
        EXPECT_FALSE(sparta::app::hasNotifSourceStopTriggerNamed(&rd, "fizbiz"));

        //Remove just the start trigger, and verify that we still have the
        //original update and stop triggers tied to notification source events
        triggers.erase("start");
        rd.extensions_["trigger"] = triggers;
        EXPECT_FALSE(sparta::app::hasNotifSourceStartTriggerNamed(&rd, "foobar"));
        EXPECT_TRUE(sparta::app::hasNotifSourceUpdateTriggerNamed(&rd, "foobar"));
        EXPECT_TRUE(sparta::app::hasNotifSourceStopTriggerNamed(&rd, "foobar"));

        //Remove the update trigger next, and verify that we are only left
        //with the stop trigger tied to a notification source event
        triggers.erase("update-whenever");
        rd.extensions_["trigger"] = triggers;
        EXPECT_FALSE(sparta::app::hasNotifSourceStartTriggerNamed(&rd, "foobar"));
        EXPECT_FALSE(sparta::app::hasNotifSourceUpdateTriggerNamed(&rd, "foobar"));
        EXPECT_TRUE(sparta::app::hasNotifSourceStopTriggerNamed(&rd, "foobar"));

        //Remove the stop trigger, and verify that all triggers are gone
        triggers.erase("stop");
        rd.extensions_["trigger"] = triggers;
        EXPECT_FALSE(sparta::app::hasNotifSourceStartTriggerNamed(&rd, "foobar"));
        EXPECT_FALSE(sparta::app::hasNotifSourceUpdateTriggerNamed(&rd, "foobar"));
        EXPECT_FALSE(sparta::app::hasNotifSourceStopTriggerNamed(&rd, "foobar"));
        EXPECT_FALSE(sparta::app::hasAnyReportTriggers(&rd));
    }
}

int main()
{
    independent_computation_windows_basic();

    independent_start_stop_computation_windows();

    notif_triggered_computation_windows();

    notification_source_comparison_ops();

    reference_triggered_computation_windows();

    logical_AND_triggered_computation_windows();

    logical_OR_triggered_computation_windows();

    multi_sub_expressions_AND_OR();

    reference_triggers_for_report_overlap();

    top_level_report_computation_windows();

    top_level_report_update_periods();

    counter_driven_update_intervals();

    cycle_driven_update_intervals();

    report_subcontainers();

    report_header_overwrite();

    trigger_internals();

    cumulative_statistics_start_from_zero();

    report_trigger_config_inspection();

    REPORT_ERROR;
    return ERROR_CODE;
}
