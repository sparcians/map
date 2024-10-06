
#include "sparta/statistics/ContextCounter.hpp"

#include <cinttypes>
#include <iostream>

#include "sparta/statistics/CounterBase.hpp"
#include "sparta/statistics/Counter.hpp"
#include "sparta/statistics/ReadOnlyCounter.hpp"
#include "sparta/statistics/CycleCounter.hpp"
#include "sparta/statistics/WeightedContextCounter.hpp"

#include "sparta/events/Event.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/StartupEvent.hpp"

#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/statistics/StatisticInstance.hpp"
#include "sparta/simulation/ResourceTreeNode.hpp"
#include "sparta/simulation/RootTreeNode.hpp"
#include "sparta/statistics/Counter.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/report/Report.hpp"
#include "sparta/report/format/Text.hpp"


TEST_INIT

void generateReport(sparta::RootTreeNode * root);

// Example of current usage
// num_uops_retired_re4_ (stat_set,
//                        "num_uops_retired",
//                        "The total number of uops retired by this core. Incremented in RE4",
//                        sparta::Counter::COUNT_NORMAL,
//                        sparta::InstrumentationNode::VIS_NORMAL+1),

void testCounters()
{
    std::cout << "********************************************************************************" << std::endl;
    std::cout << "Testing sparta::Counters..." << std::endl;
    std::cout << "********************************************************************************" << std::endl;
    sparta::Scheduler sched;
    sparta::Clock clk("clock", &sched);
    sparta::RootTreeNode root;
    root.setClock(&clk); // Set clock within configuration phase
    sparta::TreeNode dummy(&root, "dummy", "A dummy device");
    sparta::StatisticSet cset(&dummy);

    // Double context with context weights
    sparta::WeightedContextCounter<sparta::Counter> weighted_counter(
        &cset, "weighted_context", "This is a weighted context counter", 2,
        sparta::CounterBase::COUNT_NORMAL,
        sparta::InstrumentationNode::VIS_NORMAL);

    EXPECT_EQUAL(weighted_counter.numContexts(), 2);

    ++weighted_counter.context(0);
    ++weighted_counter.context(0);
    ++weighted_counter.context(0);
    ++weighted_counter.context(1);
    ++weighted_counter.context(1);

    // Unweighted average
    const double expected_unweighted_avg = (3 * 1.0 + 2 * 1.0) / 2;
    EXPECT_EQUAL(weighted_counter.calculateWeightedAverage(),
                 expected_unweighted_avg);

    // Weighted average
    weighted_counter.assignContextWeights({1.5, 4.5});
    const double expected_weighted_avg   = (3 * 1.5 + 2 * 4.5) / 2;
    EXPECT_EQUAL(weighted_counter.calculateWeightedAverage(),
                 expected_weighted_avg);

    // Single context
    sparta::ContextCounter<sparta::Counter> single_context(&cset, "single_context", "This is a single context", 1,
                                                       // Counter arguments sans the stat set and description
                                                       "context",
                                                       sparta::CounterBase::COUNT_LATEST,
                                                       sparta::InstrumentationNode::VIS_NORMAL);

    ++single_context.context(0);
    ++single_context.context(0);
    ++single_context.context(0);
    ++single_context.context(0);

    sparta::StatisticInstance si(&single_context);
    EXPECT_EQUAL(si.getValue(), 4);
    EXPECT_THROW(++single_context.context(1));

    // Double context
    sparta::ContextCounter<sparta::Counter> double_context(&cset, "double_context", "This is a double context", 2,
                                                       // Counter arguments sans the stat set and description
                                                       "thread",
                                                       sparta::CounterBase::CounterBehavior::COUNT_LATEST,
                                                       sparta::InstrumentationNode::VIS_NORMAL);
    ++double_context.context(0);
    ++double_context.context(0);
    ++double_context.context(0);
    ++double_context.context(0);
    ++double_context.context(1);
    ++double_context.context(1);
    ++double_context.context(1);
    ++double_context.context(1);

    EXPECT_THROW(++double_context.context(2));

    sparta::StatisticInstance di(&double_context);
    EXPECT_EQUAL(di.getValue(), 8);

    // Triple
    sparta::ContextCounter<sparta::Counter> triple_context(&cset, "triple_context", "This is a triple context", 3,
                                                       // Counter arguments sans the stat set and description
                                                       "thread",
                                                       sparta::CounterBase::CounterBehavior::COUNT_LATEST,
                                                       sparta::InstrumentationNode::VIS_NORMAL);
    ++triple_context.context(0);
    ++triple_context.context(0);
    ++triple_context.context(0);
    ++triple_context.context(0);
    ++triple_context.context(1);
    ++triple_context.context(1);
    ++triple_context.context(1);
    ++triple_context.context(1);
    ++triple_context.context(2);

    EXPECT_THROW(++double_context.context(3));

    sparta::StatisticInstance ti(&triple_context);
    EXPECT_EQUAL(ti.getValue(), 9);

    // Triple context, with a specific expression
    sparta::ContextCounter<sparta::Counter> triple_context_with_expression(&cset,
                                                                       "triple_context_with_expression",
                                                                       "This is a triple context w/ custom expression (t0+t1+t2)/3", 3,
                                                                       sparta::StatisticDef::ExpressionArg("(thread0+thread1+thread2)/3"),
                                                                       // Counter arguments sans the stat set and description
                                                                       "thread",
                                                                       sparta::CounterBase::CounterBehavior::COUNT_LATEST,
                                                                       sparta::InstrumentationNode::VIS_NORMAL);
    ++triple_context_with_expression.context(0);
    ++triple_context_with_expression.context(0);
    ++triple_context_with_expression.context(0);
    ++triple_context_with_expression.context(0);
    ++triple_context_with_expression.context(1);
    ++triple_context_with_expression.context(1);
    ++triple_context_with_expression.context(1);
    ++triple_context_with_expression.context(1);
    ++triple_context_with_expression.context(2);

    EXPECT_EQUAL(triple_context_with_expression.context(0).get(), 4);
    EXPECT_EQUAL(triple_context_with_expression.context(1).get(), 4);
    EXPECT_EQUAL(triple_context_with_expression.context(2).get(), 1);
    // (4 + 4 + 1) / 3 = 3
    sparta::StatisticInstance ti_with_expression(&triple_context_with_expression);
    EXPECT_EQUAL(ti_with_expression.getValue(), 3);


    // Print current counter set by the ostream insertion operator
    std::cout << cset << std::endl;
    std::cout << root.renderSubtree(-1, true) << std::endl;

    generateReport(&root);

    // Jump through the phases for now. Other tests adequately test the tree-building phases.
    root.enterConfiguring();
    std::cout << "\nCONFIGURING" << std::endl;

    root.enterFinalized();
    EXPECT_TRUE(root.isFinalized());
    sched.finalize();
    std::cout << "\nFINALIZED" << std::endl;

    root.enterTeardown();
}


//! Dummy device
class DummyDevice
{
public:
    DummyDevice(sparta::TreeNode* node) :
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


void testCycleCounters()
{
    std::cout << "********************************************************************************" << std::endl;
    std::cout << "Testing sparta::CycleCounters..." << std::endl;
    std::cout << "********************************************************************************" << std::endl;
    sparta::Scheduler sched;
    sparta::Clock clk("clock", &sched);
    sparta::RootTreeNode root;
    root.setClock(&clk); // Set clock within configuration phase
    sparta::TreeNode dummy(&root, "dummy", "A dummy device");
    sparta::StatisticSet cset(&dummy);

    DummyDevice dd(&dummy);

    // Single context
    sparta::ContextCounter<sparta::CycleCounter> single_context(&cset, "single_context", "This is a single context", 1,
                                                                // CycleCounter arguments sans the stat set and description
                                                                "context",
                                                                sparta::CounterBase::COUNT_LATEST, &clk,
                                                                sparta::InstrumentationNode::VIS_NORMAL);

    single_context.context(0).startCounting();

    sparta::StatisticInstance si(&single_context);
    EXPECT_EQUAL(si.getValue(), 0);
    EXPECT_THROW(single_context.context(1).startCounting());

    // Double context
    sparta::ContextCounter<sparta::CycleCounter> double_context(&cset, "double_context", "This is a double context", 2,
                                                                // CycleCounter arguments sans the stat set and description
                                                                "thread",
                                                                sparta::CounterBase::CounterBehavior::COUNT_LATEST, &clk,
                                                                sparta::InstrumentationNode::VIS_NORMAL);
    double_context.context(0).startCounting();
    double_context.context(1).startCounting();

    EXPECT_THROW(double_context.context(2).startCounting());

    sparta::StatisticInstance di(&double_context);
    EXPECT_EQUAL(di.getValue(), 0);

    // Triple
    sparta::ContextCounter<sparta::CycleCounter> triple_context(&cset, "triple_context", "This is a triple context", 3,
                                                                // CycleCounter arguments sans the stat set and description
                                                                "thread",
                                                                sparta::CounterBase::CounterBehavior::COUNT_LATEST, &clk,
                                                                sparta::InstrumentationNode::VIS_NORMAL);
    triple_context.context(0).startCounting();
    triple_context.context(1).startCounting();
    triple_context.context(2).startCounting();

    EXPECT_THROW(double_context.context(3).startCounting());

    sparta::StatisticInstance ti(&triple_context);
    EXPECT_EQUAL(ti.getValue(), 0);

    // Print current counter set by the ostream insertion operator
    std::cout << cset << std::endl;
    std::cout << root.renderSubtree(-1, true) << std::endl;

    generateReport(&root);

    // Jump through the phases for now. Other tests adequately test the tree-building phases.
    root.enterConfiguring();
    std::cout << "\nCONFIGURING" << std::endl;

    root.enterFinalized();
    sched.finalize();
    EXPECT_TRUE(root.isFinalized());
    std::cout << "\nFINALIZED" << std::endl;

    dd.dummyCallback();

    // proceed to tick 1, nothing should happen, but time advancement
    sched.run(1, true, false);

    // Now, clock everything 1 cycle
    sched.run(3); // 1, 2, 3, ends at 4
    single_context.context(0).stopCounting(); // 4 cycles have elapsed

    generateReport(&root);

    EXPECT_EQUAL(si.getValue(), 3);
    EXPECT_EQUAL(di.getValue(), 6);
    EXPECT_EQUAL(ti.getValue(), 9);


    root.enterTeardown();
}

// This does not compile -- on purpose
// void testReadOnlyCounters()
// {
//     std::cout << "********************************************************************************" << std::endl;
//     std::cout << "Testing sparta::CycleCounters..." << std::endl;
//     std::cout << "********************************************************************************" << std::endl;
//     sparta::Scheduler sched;
//     sparta::Clock clk("clock", &sched);
//     sparta::RootTreeNode root;
//     root.setClock(&clk); // Set clock within configuration phase
//     sparta::TreeNode dummy(&root, "dummy", "A dummy device");
//     sparta::StatisticSet cset(&dummy);

//     DummyDevice dd(&dummy);

//     sparta::CounterBase::counter_type counter1 = 0;
//     // sparta::CounterBase::counter_type counter2 = 0;
//     // sparta::CounterBase::counter_type counter3 = 0;
//     // sparta::CounterBase::counter_type counter4 = 0;
//     // sparta::CounterBase::counter_type counter5 = 0;
//     // sparta::CounterBase::counter_type counter6 = 0;

//     // Single context
//     sparta::ContextCounter<sparta::ReadOnlyCounter> single_context(&cset, "single_context", "This is a single context", 1,
//                                                                // ReadOnlyCounter arguments sans the stat set and description
//                                                                "context",
//                                                                sparta::CounterBase::COUNT_LATEST, &counter1);

//     ++counter1;
//     EXPECT_EQUAL(single_context.context(0).get(), 1);

//     // Print current counter set by the ostream insertion operator
//     std::cout << cset << std::endl;
//     std::cout << root.renderSubtree(-1, true) << std::endl;

//     generateReport(&root);

//     // Jump through the phases for now. Other tests adequately test the tree-building phases.
//     root.enterConfiguring();
//     std::cout << "\nCONFIGURING" << std::endl;

//     root.enterFinalized();
//     EXPECT_TRUE(root.isFinalized());
//     sched.finalize();
//     std::cout << "\nFINALIZED" << std::endl;

//     root.enterTeardown();

// }

int main()
{
    testCounters();
    testCycleCounters();
    // testReadOnlyCounters(); // Will not compile -- ReadOnlyCounters not supported

    REPORT_ERROR;
    return ERROR_CODE;
}


void generateReport(sparta::RootTreeNode * root)
{
    sparta::Report r1("report 1", root);
    auto subreport_gen_fxn = [](const sparta::TreeNode* tn,
                                std::string& rep_name,
                                bool& make_child_sr,
                                uint32_t report_depth) -> bool {
                                 (void) report_depth;

                                 make_child_sr = true;

                                 // Note: Cannot currently test for DynamicResourceTreeNode without
                                 // knowing its template types. DynamicResourceTreeNode will need to
                                 // have a base class that is not TreeNode which can be used here.
                                 if(dynamic_cast<const sparta::ResourceTreeNode*>(tn) != nullptr
                                    || dynamic_cast<const sparta::RootTreeNode*>(tn) != nullptr
                                    || tn->hasChild(sparta::StatisticSet::NODE_NAME)){
                                     rep_name = tn->getLocation(); // Use location as report name
                                     return true;
                                 }
                                 return false;
                             };

    r1.addSubtree(root->getSearchScope(), // Subtree (including) n
                  subreport_gen_fxn,      // Generate subtrees at specific nodes
                  nullptr,                // Do not filter branches
                  nullptr,                // Do not filter leaves
                  true,                   // Add Counters
                  true,                   // Add StatisticDefs
                  -1);                    // Max recursion depth

    sparta::report::format::Text summary_fmt(&r1);
    summary_fmt.setValueColumn(summary_fmt.getRightmostNameColumn());
    summary_fmt.setReportPrefix("");
    summary_fmt.setQuoteReportNames(false);
    summary_fmt.setWriteContentlessReports(false);
    summary_fmt.setShowSimInfo(false); // No need to summarize the simulator here
    summary_fmt.setShowDescriptions(true);
    std::cout << summary_fmt << std::endl;;
}
