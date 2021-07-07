// <EnumCycleHistogram_test> -*- C++ -*-

/**
 * \file EnumCycleHistogram_test
 * \brief A test that creates a producer and consumer, and
 * then runs some test cases on EnumCycleHistogramTreeNode.
 */

#include <iostream>
#include <inttypes.h>
#include <memory>

#include "sparta/simulation/Clock.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/sparta.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/events/Event.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/PayloadEvent.hpp"
#include "sparta/events/StartupEvent.hpp"
#include "sparta/simulation/ResourceFactory.hpp"
#include "sparta/utils/Enum.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/statistics/StatisticInstance.hpp"
#include "sparta/statistics/EnumCycleHistogram.hpp"
#include "sparta/utils/SpartaException.hpp"

TEST_INIT;

#define PRINT_ENTER_TEST \
  std::cout << std::endl; \
  std::cout << "*************************************************************" \
            << "*** Beginning '" << __FUNCTION__ << "'" \
            << "*************************************************************" \
            << std::endl;

// Dummy enum class OperandState
enum class OperandState : uint32_t{
    OPER_INIT = 0,
    __FIRST = OPER_INIT,
    OPER_READY,
    OPER_WAIT,
    OPER_RETIRE,
    __LAST
};

// Dummy enum class UopState
enum class UopState : uint64_t{
    UOP_INIT = 0,
    __FIRST = UOP_INIT,
    UOP_READY,
    UOP_WAIT,
    UOP_RETIRE,
    UOP_RESET,
    __LAST
};

// Overloaded ostream operator for enum class UopState
inline std::ostream & operator<<(std::ostream& os, const UopState& uop_state){
    switch(uop_state){
    case UopState::UOP_INIT:
        os << "UOP_INIT";
        return os;
    case UopState::UOP_READY:
        os << "UOP_READY";
        return os;
    case UopState::UOP_WAIT:
        os << "UOP_WAIT";
        return os;
    case UopState::UOP_RETIRE:
        os << "UOP_RETIRE";
        return os;
    case UopState::UOP_RESET:
        os << "UOP_RESET";
        return os;
    default:
        throw sparta::SpartaException("Unable to identify enum state constant.");
    }
}

// Dummy enum class MMUState
enum class MMUState : uint16_t{
    NO_ACCESS = 0,
    __FIRST = NO_ACCESS,
    MISS,
    HIT,
    RETIRE,
    __LAST
};

// Initialize static name array of sparta::utils::Enum<MMUState>
template<>
const std::unique_ptr<std::string[]> sparta::utils::Enum<MMUState>::names_(new std::string[static_cast<uint16_t>(MMUState::__LAST) + 1]);

// Fill the static name array of sparta::utils::Enum<MMUState>
sparta::utils::Enum<MMUState> MMUEnumType(MMUState::NO_ACCESS, "MMUSTATE_NO_ACCESS",
                                        MMUState::MISS, "MMUSTATE_MISS",
                                        MMUState::HIT, "MMUSTATE_HIT",
                                        MMUState::RETIRE, "MMUSTATE_RETIRE");

//! Dummy device
class DummyDevice : public sparta::Resource
{
public:

    static constexpr const char* name = "DummyDevice";

    class ParameterSet : public sparta::ParameterSet
    {
    public:
        ParameterSet(sparta::TreeNode* tn) : sparta::ParameterSet(tn) {}
    };

    DummyDevice(sparta::TreeNode* node,
                const DummyDevice::ParameterSet*) :
        sparta::Resource(node),
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

void testOpStateHistogram(){
    PRINT_ENTER_TEST

    sparta::Scheduler scheduler("test");
    sparta::Clock clk("clock", &scheduler);
    sparta::RootTreeNode rtn("root");
    rtn.setClock(&clk);

    sparta::ResourceFactory<DummyDevice, DummyDevice::ParameterSet> rfact;
    sparta::ResourceTreeNode dummy(&rtn, "dummy", "dummy node", &rfact);
    sparta::StatisticSet sset(&dummy);

    sparta::EnumCycleHistogram<OperandState> op_state_histogram_tn(&rtn,
                                                              "op_state_histogram_tn",
                                                              "Enum Cycle Histogram for Op-State");
    rtn.enterConfiguring();
    rtn.enterFinalized();
    EXPECT_NOTHROW(rtn.validatePreRun());

    sparta::StatisticDef *tn_avg = nullptr;
    EXPECT_NOTHROW(tn_avg = rtn.getChildAs<sparta::StatisticDef>("op_state_histogram_tn.stats.weighted_avg"));
    EXPECT_TRUE(tn_avg);
    sparta::StatisticInstance si_avg(tn_avg);

    sparta::StatisticDef *tn_nonzero_avg = nullptr;
    EXPECT_NOTHROW(tn_nonzero_avg = rtn.getChildAs<sparta::StatisticDef>(
        "op_state_histogram_tn.stats.weighted_nonzero_avg"));
    EXPECT_TRUE(tn_nonzero_avg);
    sparta::StatisticInstance si_nonzero_avg(tn_nonzero_avg);

    sparta::StatisticDef *tn_fullness = nullptr;
    EXPECT_NOTHROW(tn_fullness = rtn.getChildAs<sparta::StatisticDef>(
        "op_state_histogram_tn.stats.full"));
    EXPECT_TRUE(tn_fullness);
    sparta::StatisticInstance si_fullness(tn_fullness);

    sparta::StatisticDef *tn_fullness_probability = nullptr;
    EXPECT_NOTHROW(tn_fullness_probability = rtn.getChildAs<sparta::StatisticDef>(
        "op_state_histogram_tn.stats.full_probability"));
    EXPECT_TRUE(tn_fullness_probability);
    sparta::StatisticInstance si_fullness_probability(tn_fullness_probability);

    scheduler.finalize();

    // proceed to tick 1, nothing should happen, but time advancement
    scheduler.run(1, true, false);

    EXPECT_EQUAL(op_state_histogram_tn.getHistogramUpperValue(), 3);
    EXPECT_EQUAL(op_state_histogram_tn.getHistogramLowerValue(), 0);
    EXPECT_EQUAL(op_state_histogram_tn.getNumBins(), 4);
    EXPECT_EQUAL(op_state_histogram_tn.getNumValuesPerBin(), 1);

    sparta::CycleCounter *tn_0  = nullptr, *tn_1  = nullptr, *tn_2  = nullptr, *tn_3 = nullptr;
    sparta::CycleCounter *tn_uf = nullptr, *tn_of = nullptr, *tn_tt = nullptr;
    sparta::Counter      *tn_mx = nullptr;

    // This enum class has no overloaded << operator for name decoration.
    // Hence, statistic definition names will be generated using the default behaviour.
    EXPECT_NOTHROW(tn_uf = rtn.getChildAs<sparta::CycleCounter>("op_state_histogram_tn.stats.UF"));
    EXPECT_NOTHROW(tn_0  = rtn.getChildAs<sparta::CycleCounter>("op_state_histogram_tn.stats.cycle_count0"));
    EXPECT_NOTHROW(tn_1  = rtn.getChildAs<sparta::CycleCounter>("op_state_histogram_tn.stats.cycle_count1"));
    EXPECT_NOTHROW(tn_2  = rtn.getChildAs<sparta::CycleCounter>("op_state_histogram_tn.stats.cycle_count2"));
    EXPECT_NOTHROW(tn_3  = rtn.getChildAs<sparta::CycleCounter>("op_state_histogram_tn.stats.cycle_count3"));
    EXPECT_NOTHROW(tn_of = rtn.getChildAs<sparta::CycleCounter>("op_state_histogram_tn.stats.OF"));
    EXPECT_NOTHROW(tn_tt = rtn.getChildAs<sparta::CycleCounter>("op_state_histogram_tn.stats.total"));
    EXPECT_NOTHROW(tn_mx = rtn.getChildAs<sparta::Counter>("op_state_histogram_tn.stats.max_value"));
    EXPECT_TRUE(tn_uf);
    EXPECT_TRUE(tn_0);
    EXPECT_TRUE(tn_1);
    EXPECT_TRUE(tn_2);
    EXPECT_TRUE(tn_3);
    EXPECT_TRUE(tn_of);
    EXPECT_TRUE(tn_tt);
    EXPECT_TRUE(tn_mx);

    scheduler.run(111); // Counts on idle value [Bucket 0]
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_0->get(),  111); // 0 + 111
    EXPECT_EQUAL(tn_1->get(),  0);
    EXPECT_EQUAL(tn_2->get(),  0);
    EXPECT_EQUAL(tn_3->get(),  0);
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 111); // 0 + 111
    EXPECT_EQUAL(tn_mx->get(), 0);

    scheduler.run(111); // Counts on idle value [Bucket 0]
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_0->get(),  222); // 111 + 111
    EXPECT_EQUAL(tn_1->get(),  0);
    EXPECT_EQUAL(tn_2->get(),  0);
    EXPECT_EQUAL(tn_3->get(),  0);
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 222); // 111 + 111
    EXPECT_EQUAL(tn_mx->get(), 0);

    op_state_histogram_tn.startCounting(OperandState::OPER_READY); // Set value to Bucket 1
    EXPECT_EQUAL(tn_mx->get(), 1);
    scheduler.run(222);
    op_state_histogram_tn.stopCounting(OperandState::OPER_READY);
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_0->get(),  222); // no change
    EXPECT_EQUAL(tn_1->get(),  222); // 0 + 222
    EXPECT_EQUAL(tn_2->get(),  0);
    EXPECT_EQUAL(tn_3->get(),  0);
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 444); // 222 + 222
    EXPECT_EQUAL(tn_mx->get(), 1);

    op_state_histogram_tn.startCounting(OperandState::OPER_WAIT); // add value of 1 cycle to Bucket 2
    scheduler.run(1);
    op_state_histogram_tn.stopCounting(OperandState::OPER_WAIT);
    EXPECT_EQUAL(tn_mx->get(), 2);
    scheduler.run(332);
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_0->get(),  554); // 222 + (332)
    EXPECT_EQUAL(tn_1->get(),  222); // no change
    EXPECT_EQUAL(tn_2->get(),  1); // 0 + 1
    EXPECT_EQUAL(tn_3->get(),  0);
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 777); // 444 + 333
    EXPECT_EQUAL(tn_mx->get(), 2);

    op_state_histogram_tn.startCounting(OperandState::OPER_RETIRE); // add value of 1 cycle to Bucket 3
    EXPECT_EQUAL(tn_mx->get(), 3);
    scheduler.run(1);
    op_state_histogram_tn.stopCounting(OperandState::OPER_RETIRE);
    scheduler.run(443);
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_0->get(),  997); // 554 + (443)
    EXPECT_EQUAL(tn_1->get(),  222); // no change
    EXPECT_EQUAL(tn_2->get(),  1); // no change
    EXPECT_EQUAL(tn_3->get(),  1); // 0 + 1
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 1221); // 777 + 443 + 1
    EXPECT_EQUAL(tn_mx->get(), 3);

    op_state_histogram_tn.startCounting(OperandState::OPER_RETIRE); // start counting on Bucket 3
    EXPECT_EQUAL(tn_mx->get(), 3);
    scheduler.run(100);
    op_state_histogram_tn.stopCounting(OperandState::OPER_RETIRE); // stop counting on Bucket 3
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_0->get(),  997); // no change
    EXPECT_EQUAL(tn_1->get(),  222); // no change
    EXPECT_EQUAL(tn_2->get(),  1); // no change
    EXPECT_EQUAL(tn_3->get(),  101); // 1 + 100
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 1321); // 1222 + 100
    EXPECT_EQUAL(tn_mx->get(), 3);

    op_state_histogram_tn.startCounting(OperandState::OPER_WAIT); // start counting on Bucket 2
    EXPECT_EQUAL(tn_mx->get(), 3);
    scheduler.run(100);
    op_state_histogram_tn.stopCounting(OperandState::OPER_WAIT); // stop counting on Bucket 2
    scheduler.run(2); // counts on Bucket 0 [idle value]
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_0->get(),  999); // 997 + 2
    EXPECT_EQUAL(tn_1->get(),  222); // no change
    EXPECT_EQUAL(tn_2->get(),  101); // 1 + 100
    EXPECT_EQUAL(tn_3->get(),  101); // no change
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 1423); // 1321 + 102
    EXPECT_EQUAL(tn_mx->get(), 3);

    // Verify stat accuracy for weighted average
    const auto cal = ((0.0 * tn_uf->get())  +
                      (0.0 * tn_0->get())   +
                      (1.0 * tn_1->get())   +
                      (2.0 * tn_2->get())   +
                      (3.0 * tn_3->get())   +
                      (3.0 * tn_of->get())) / tn_tt->get();
    EXPECT_EQUAL(si_avg.getValue(), cal);

    // Verify stat accuracy for weighted non-zero average
    const auto nonzero_cal = ((1.0 * tn_1->get())   +
                              (2.0 * tn_2->get())   +
                              (3.0 * tn_3->get())   +
                              (3.0 * tn_of->get())) / (tn_tt->get() - tn_0->get());
    EXPECT_EQUAL(si_nonzero_avg.getValue(), nonzero_cal);

    // Verify stat accuracy for fullness
    const auto fullness_cal = (tn_3->get() + tn_of->get());
    EXPECT_EQUAL(si_fullness.getValue(), fullness_cal);

    // Verify stat accuracy for fullness probability
    const auto fullness_prob_cal = fullness_cal / static_cast<double>(tn_tt->get());
    EXPECT_EQUAL(si_fullness_probability.getValue(), fullness_prob_cal);

    //it's now safe to tear down our dummy tree
    rtn.enterTeardown();
}

void testUopStateHistogram(){
    PRINT_ENTER_TEST

    sparta::Scheduler scheduler("test");
    sparta::Clock clk("clock", &scheduler);
    sparta::RootTreeNode rtn("root");
    rtn.setClock(&clk);

    sparta::ResourceFactory<DummyDevice, DummyDevice::ParameterSet> rfact;
    sparta::ResourceTreeNode dummy(&rtn, "dummy", "dummy node", &rfact);
    sparta::StatisticSet sset(&dummy);

    sparta::EnumCycleHistogram<UopState> uop_state_histogram_tn(&rtn,
                                                              "uop_state_histogram_tn",
                                                              "Enum Cycle Histogram for Uop-State");
    rtn.enterConfiguring();
    rtn.enterFinalized();
    EXPECT_NOTHROW(rtn.validatePreRun());

    sparta::StatisticDef *tn_avg = nullptr;
    EXPECT_NOTHROW(tn_avg = rtn.getChildAs<sparta::StatisticDef>("uop_state_histogram_tn.stats.weighted_avg"));
    EXPECT_TRUE(tn_avg);
    sparta::StatisticInstance si_avg(tn_avg);

    sparta::StatisticDef *tn_nonzero_avg = nullptr;
    EXPECT_NOTHROW(tn_nonzero_avg = rtn.getChildAs<sparta::StatisticDef>(
        "uop_state_histogram_tn.stats.weighted_nonzero_avg"));
    EXPECT_TRUE(tn_nonzero_avg);
    sparta::StatisticInstance si_nonzero_avg(tn_nonzero_avg);

    sparta::StatisticDef *tn_fullness = nullptr;
    EXPECT_NOTHROW(tn_fullness = rtn.getChildAs<sparta::StatisticDef>(
        "uop_state_histogram_tn.stats.full"));
    EXPECT_TRUE(tn_fullness);
    sparta::StatisticInstance si_fullness(tn_fullness);

    sparta::StatisticDef *tn_fullness_probability = nullptr;
    EXPECT_NOTHROW(tn_fullness_probability = rtn.getChildAs<sparta::StatisticDef>(
        "uop_state_histogram_tn.stats.full_probability"));
    EXPECT_TRUE(tn_fullness_probability);
    sparta::StatisticInstance si_fullness_probability(tn_fullness_probability);

    scheduler.finalize();

    // proceed to tick 1, nothing should happen, but time advancement
    scheduler.run(1, true, false);

    EXPECT_EQUAL(uop_state_histogram_tn.getHistogramUpperValue(), 4);
    EXPECT_EQUAL(uop_state_histogram_tn.getHistogramLowerValue(), 0);
    EXPECT_EQUAL(uop_state_histogram_tn.getNumBins(), 5);
    EXPECT_EQUAL(uop_state_histogram_tn.getNumValuesPerBin(), 1);

    sparta::CycleCounter *tn_0  = nullptr, *tn_1  = nullptr, *tn_2  = nullptr, *tn_3 = nullptr, *tn_4 = nullptr;
    sparta::CycleCounter *tn_uf = nullptr, *tn_of = nullptr, *tn_tt = nullptr;
    sparta::Counter      *tn_mx = nullptr;

    // This enum class has user-defined overloaded << operator for name decoration.
    // Hence, statistic definition names will be generated using the overloaded << oeprator generated names.
    EXPECT_NOTHROW(tn_uf =
        rtn.getChildAs<sparta::CycleCounter>("uop_state_histogram_tn.stats.UF"));
    EXPECT_NOTHROW(tn_0 =
        rtn.getChildAs<sparta::CycleCounter>("uop_state_histogram_tn.stats.cycle_countUOP_INIT"));
    EXPECT_NOTHROW(tn_1 =
        rtn.getChildAs<sparta::CycleCounter>("uop_state_histogram_tn.stats.cycle_countUOP_READY"));
    EXPECT_NOTHROW(tn_2 =
        rtn.getChildAs<sparta::CycleCounter>("uop_state_histogram_tn.stats.cycle_countUOP_WAIT"));
    EXPECT_NOTHROW(tn_3 =
        rtn.getChildAs<sparta::CycleCounter>("uop_state_histogram_tn.stats.cycle_countUOP_RETIRE"));
    EXPECT_NOTHROW(tn_4 =
        rtn.getChildAs<sparta::CycleCounter>("uop_state_histogram_tn.stats.cycle_countUOP_RESET"));
    EXPECT_NOTHROW(tn_of = rtn.getChildAs<sparta::CycleCounter>("uop_state_histogram_tn.stats.OF"));
    EXPECT_NOTHROW(tn_tt = rtn.getChildAs<sparta::CycleCounter>("uop_state_histogram_tn.stats.total"));
    EXPECT_NOTHROW(tn_mx = rtn.getChildAs<sparta::Counter>("uop_state_histogram_tn.stats.max_value"));
    EXPECT_TRUE(tn_uf);
    EXPECT_TRUE(tn_0);
    EXPECT_TRUE(tn_1);
    EXPECT_TRUE(tn_2);
    EXPECT_TRUE(tn_3);
    EXPECT_TRUE(tn_4);
    EXPECT_TRUE(tn_of);
    EXPECT_TRUE(tn_tt);
    EXPECT_TRUE(tn_mx);

    scheduler.run(111); // Counts on idle value [Bucket 0]
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_0->get(),  111); // 0 + 111
    EXPECT_EQUAL(tn_1->get(),  0);
    EXPECT_EQUAL(tn_2->get(),  0);
    EXPECT_EQUAL(tn_3->get(),  0);
    EXPECT_EQUAL(tn_4->get(),  0);
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 111); // 0 + 111
    EXPECT_EQUAL(tn_mx->get(), 0);

    scheduler.run(111); //  Counts on idle value [Bucket 0]
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_0->get(),  222); // 111 + 111
    EXPECT_EQUAL(tn_1->get(),  0);
    EXPECT_EQUAL(tn_2->get(),  0);
    EXPECT_EQUAL(tn_3->get(),  0);
    EXPECT_EQUAL(tn_4->get(),  0);
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 222); // 112 + 111
    EXPECT_EQUAL(tn_mx->get(), 0);

    uop_state_histogram_tn.startCounting(UopState::UOP_READY); // Counts on Bucket 1
    EXPECT_EQUAL(tn_mx->get(), 1);
    scheduler.run(222);
    uop_state_histogram_tn.stopCounting(UopState::UOP_READY);
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_0->get(),  222); // no change
    EXPECT_EQUAL(tn_1->get(),  222); // 0 + 222
    EXPECT_EQUAL(tn_2->get(),  0);
    EXPECT_EQUAL(tn_3->get(),  0);
    EXPECT_EQUAL(tn_4->get(),  0);
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 444); // 222 + 222
    EXPECT_EQUAL(tn_mx->get(), 1);

    uop_state_histogram_tn.startCounting(UopState::UOP_WAIT); // Counts on Bucket 2 for 1 cycle
    EXPECT_EQUAL(tn_mx->get(), 2);
    scheduler.run(1);
    uop_state_histogram_tn.stopCounting(UopState::UOP_WAIT);
    scheduler.run(332);
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_0->get(),  554); // 222 + (332)
    EXPECT_EQUAL(tn_1->get(),  222); // no change
    EXPECT_EQUAL(tn_2->get(),  1); // 0 + 1
    EXPECT_EQUAL(tn_3->get(),  0);
    EXPECT_EQUAL(tn_4->get(),  0);
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 777); // 444 + 333
    EXPECT_EQUAL(tn_mx->get(), 2);

    uop_state_histogram_tn.startCounting(UopState::UOP_RETIRE); // Counts on Bucket 3 for 1 cycle
    EXPECT_EQUAL(tn_mx->get(), 3);
    scheduler.run(1);
    uop_state_histogram_tn.stopCounting(UopState::UOP_RETIRE);
    scheduler.run(443);
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_0->get(),  997); // 554 + (443)
    EXPECT_EQUAL(tn_1->get(),  222); // no change
    EXPECT_EQUAL(tn_2->get(),  1); // no change
    EXPECT_EQUAL(tn_3->get(),  1); // 0 + 1
    EXPECT_EQUAL(tn_4->get(),  0);
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 1221); // 777 + 444
    EXPECT_EQUAL(tn_mx->get(), 3);

    uop_state_histogram_tn.startCounting(UopState::UOP_RETIRE); // Starts counting on Bucket 3
    EXPECT_EQUAL(tn_mx->get(), 3);
    scheduler.run(100);
    uop_state_histogram_tn.stopCounting(UopState::UOP_RETIRE); // Stops counting on Bucket 3
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_0->get(),  997); // no change
    EXPECT_EQUAL(tn_1->get(),  222); // no change
    EXPECT_EQUAL(tn_2->get(),  1);   // no change
    EXPECT_EQUAL(tn_3->get(),  101); // 1 + 100
    EXPECT_EQUAL(tn_4->get(),  0);
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 1321); // 1221 + 100
    EXPECT_EQUAL(tn_mx->get(), 3);

    uop_state_histogram_tn.startCounting(UopState::UOP_WAIT); // Starts counting on Bucket 2
    EXPECT_EQUAL(tn_mx->get(), 3);
    scheduler.run(100);
    uop_state_histogram_tn.stopCounting(UopState::UOP_WAIT); // Stops counting on Bucket 2
    scheduler.run(2); // Starts counting on idle bucket
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_0->get(),  999); // 997 + 2
    EXPECT_EQUAL(tn_1->get(),  222); // no change
    EXPECT_EQUAL(tn_2->get(),  101); // 1 + 100
    EXPECT_EQUAL(tn_3->get(),  101); // no change
    EXPECT_EQUAL(tn_4->get(),  0);
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 1423); // 1321 + 100 + 2
    EXPECT_EQUAL(tn_mx->get(), 3);

    uop_state_histogram_tn.startCounting(UopState::UOP_RESET); // Starts counting on Bucket 4
    EXPECT_EQUAL(tn_mx->get(), 4);
    scheduler.run(100);
    uop_state_histogram_tn.stopCounting(UopState::UOP_RESET); // Stops counting on Bucket 4
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_0->get(),  999); // no change
    EXPECT_EQUAL(tn_1->get(),  222); // no change
    EXPECT_EQUAL(tn_2->get(),  101); // no change
    EXPECT_EQUAL(tn_3->get(),  101); // no change
    EXPECT_EQUAL(tn_4->get(),  100); // 0 + 100
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 1523); // 1423 + 100
    EXPECT_EQUAL(tn_mx->get(), 4);

    scheduler.run(1); // Starts counting on idle bucket
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_0->get(),  1000); // 999 + 1
    EXPECT_EQUAL(tn_1->get(),  222); // no change
    EXPECT_EQUAL(tn_2->get(),  101); // no change
    EXPECT_EQUAL(tn_3->get(),  101); // no change
    EXPECT_EQUAL(tn_4->get(),  100); // no change
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 1524); // 1523 + 1
    EXPECT_EQUAL(tn_mx->get(), 4);

    // Verify stat accuracy for weighted average
    const auto cal = ((0.0 * tn_uf->get())  +
                      (0.0 * tn_0->get())   +
                      (1.0 * tn_1->get())   +
                      (2.0 * tn_2->get())   +
                      (3.0 * tn_3->get())   +
                      (4.0 * tn_4->get())   +
                      (4.0 * tn_of->get())) / tn_tt->get();
    EXPECT_EQUAL(si_avg.getValue(), cal);

    // Verify stat accuracy for weighted non-zero average
    const auto nonzero_cal = ((1.0 * tn_1->get())   +
                              (2.0 * tn_2->get())   +
                              (3.0 * tn_3->get())   +
                              (4.0 * tn_4->get())   +
                              (4.0 * tn_of->get())) / (tn_tt->get() - tn_0->get());
    EXPECT_EQUAL(si_nonzero_avg.getValue(), nonzero_cal);

    // Verify stat accuracy for fullness
    const auto fullness_cal = (tn_4->get() + tn_of->get());
    EXPECT_EQUAL(si_fullness.getValue(), fullness_cal);

    // Verify stat accuracy for fullness probability
    const auto fullness_prob_cal = fullness_cal / static_cast<double>(tn_tt->get());
    EXPECT_EQUAL(si_fullness_probability.getValue(), fullness_prob_cal);

    //it's now safe to tear down our dummy tree
    rtn.enterTeardown();
}

void testMMUStateHistogram(){
    PRINT_ENTER_TEST

    sparta::Scheduler scheduler("test");
    sparta::Clock clk("clock", &scheduler);
    sparta::RootTreeNode rtn("root");
    rtn.setClock(&clk);

    sparta::ResourceFactory<DummyDevice, DummyDevice::ParameterSet> rfact;
    sparta::ResourceTreeNode dummy(&rtn, "dummy", "dummy node", &rfact);
    sparta::StatisticSet sset(&dummy);

    sparta::EnumCycleHistogram<sparta::utils::Enum<MMUState>>
        mmu_state_histogram_tn(&rtn, "mmu_state_histogram_tn", "Enum Cycle Histogram for MMUState");

    rtn.enterConfiguring();
    rtn.enterFinalized();
    EXPECT_NOTHROW(rtn.validatePreRun());

    sparta::StatisticDef *tn_avg = nullptr;
    EXPECT_NOTHROW(tn_avg = rtn.getChildAs<sparta::StatisticDef>("mmu_state_histogram_tn.stats.weighted_avg"));
    EXPECT_TRUE(tn_avg);
    sparta::StatisticInstance si_avg(tn_avg);

    sparta::StatisticDef *tn_nonzero_avg = nullptr;
    EXPECT_NOTHROW(tn_nonzero_avg = rtn.getChildAs<sparta::StatisticDef>(
        "mmu_state_histogram_tn.stats.weighted_nonzero_avg"));
    EXPECT_TRUE(tn_nonzero_avg);
    sparta::StatisticInstance si_nonzero_avg(tn_nonzero_avg);

    sparta::StatisticDef *tn_fullness = nullptr;
    EXPECT_NOTHROW(tn_fullness = rtn.getChildAs<sparta::StatisticDef>(
        "mmu_state_histogram_tn.stats.full"));
    EXPECT_TRUE(tn_fullness);
    sparta::StatisticInstance si_fullness(tn_fullness);

    sparta::StatisticDef *tn_fullness_probability = nullptr;
    EXPECT_NOTHROW(tn_fullness_probability = rtn.getChildAs<sparta::StatisticDef>(
        "mmu_state_histogram_tn.stats.full_probability"));
    EXPECT_TRUE(tn_fullness_probability);
    sparta::StatisticInstance si_fullness_probability(tn_fullness_probability);

    scheduler.finalize();

    // proceed to tick 1, nothing should happen, but time advancement
    scheduler.run(1, true, false);

    EXPECT_EQUAL(mmu_state_histogram_tn.getHistogramUpperValue(), 3);
    EXPECT_EQUAL(mmu_state_histogram_tn.getHistogramLowerValue(), 0);
    EXPECT_EQUAL(mmu_state_histogram_tn.getNumBins(), 4);
    EXPECT_EQUAL(mmu_state_histogram_tn.getNumValuesPerBin(), 1);

    sparta::CycleCounter *tn_0  = nullptr, *tn_1  = nullptr, *tn_2  = nullptr, *tn_3 = nullptr;
    sparta::CycleCounter *tn_uf = nullptr, *tn_of = nullptr, *tn_tt = nullptr;
    sparta::Counter      *tn_mx = nullptr;

    // This histogram is templated on sparta::utils::Enum<EnumT>.
    // Hence, the user does not need to define an overloaded << operator for generating names.
    // Hence, statistic definition names will be generated using the names that were mapped
    // to enum constants by user.
    EXPECT_NOTHROW(tn_uf = rtn.getChildAs<sparta::CycleCounter>("mmu_state_histogram_tn.stats.UF"));
    EXPECT_NOTHROW(tn_0  =
        rtn.getChildAs<sparta::CycleCounter>("mmu_state_histogram_tn.stats.cycle_countMMUSTATE_NO_ACCESS"));
    EXPECT_NOTHROW(tn_1  =
        rtn.getChildAs<sparta::CycleCounter>("mmu_state_histogram_tn.stats.cycle_countMMUSTATE_MISS"));
    EXPECT_NOTHROW(tn_2  =
        rtn.getChildAs<sparta::CycleCounter>("mmu_state_histogram_tn.stats.cycle_countMMUSTATE_HIT"));
    EXPECT_NOTHROW(tn_3  =
        rtn.getChildAs<sparta::CycleCounter>("mmu_state_histogram_tn.stats.cycle_countMMUSTATE_RETIRE"));
    EXPECT_NOTHROW(tn_of = rtn.getChildAs<sparta::CycleCounter>("mmu_state_histogram_tn.stats.OF"));
    EXPECT_NOTHROW(tn_tt = rtn.getChildAs<sparta::CycleCounter>("mmu_state_histogram_tn.stats.total"));
    EXPECT_NOTHROW(tn_mx = rtn.getChildAs<sparta::Counter>("mmu_state_histogram_tn.stats.max_value"));
    EXPECT_TRUE(tn_uf);
    EXPECT_TRUE(tn_0);
    EXPECT_TRUE(tn_1);
    EXPECT_TRUE(tn_2);
    EXPECT_TRUE(tn_3);
    EXPECT_TRUE(tn_of);
    EXPECT_TRUE(tn_tt);
    EXPECT_TRUE(tn_mx);

    scheduler.run(111); // Counts on idle value [Bucket 0]
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_0->get(),  111); // 0 + 111
    EXPECT_EQUAL(tn_1->get(),  0);
    EXPECT_EQUAL(tn_2->get(),  0);
    EXPECT_EQUAL(tn_3->get(),  0);
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 111); // 0 + 111
    EXPECT_EQUAL(tn_mx->get(), 0);

    scheduler.run(111); // Counts on idle value [Bucket 0]
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_0->get(),  222); // 111 + 111
    EXPECT_EQUAL(tn_1->get(),  0);
    EXPECT_EQUAL(tn_2->get(),  0);
    EXPECT_EQUAL(tn_3->get(),  0);
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 222); // 111 + 111
    EXPECT_EQUAL(tn_mx->get(), 0);

    mmu_state_histogram_tn.startCounting(MMUState::MISS); // Set value to Bucket 1
    EXPECT_EQUAL(tn_mx->get(), 1);
    scheduler.run(222);
    mmu_state_histogram_tn.stopCounting(MMUState::MISS);
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_0->get(),  222); // no change
    EXPECT_EQUAL(tn_1->get(),  222); // 0 + 222
    EXPECT_EQUAL(tn_2->get(),  0);
    EXPECT_EQUAL(tn_3->get(),  0);
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 444); // 222 + 222
    EXPECT_EQUAL(tn_mx->get(), 1);

    mmu_state_histogram_tn.startCounting(MMUState::HIT); // add value of 1 cycle to Bucket 2
    scheduler.run(1);
    mmu_state_histogram_tn.stopCounting(MMUState::HIT);
    EXPECT_EQUAL(tn_mx->get(), 2);
    scheduler.run(332);
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_0->get(),  554); // 222 + (332)
    EXPECT_EQUAL(tn_1->get(),  222); // no change
    EXPECT_EQUAL(tn_2->get(),  1); // 0 + 1
    EXPECT_EQUAL(tn_3->get(),  0);
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 777); // 445 + 332
    EXPECT_EQUAL(tn_mx->get(), 2);

    mmu_state_histogram_tn.startCounting(MMUState::RETIRE); // add value of 1 cycle to Bucket 3
    EXPECT_EQUAL(tn_mx->get(), 3);
    scheduler.run(1);
    mmu_state_histogram_tn.stopCounting(MMUState::RETIRE);
    scheduler.run(443);
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_0->get(),  997); // 554 + (443)
    EXPECT_EQUAL(tn_1->get(),  222); // no change
    EXPECT_EQUAL(tn_2->get(),  1); // no change
    EXPECT_EQUAL(tn_3->get(),  1); // 0 + 1
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 1221); // 778 + 443
    EXPECT_EQUAL(tn_mx->get(), 3);

    mmu_state_histogram_tn.startCounting(MMUState::RETIRE); // start counting on Bucket 3
    EXPECT_EQUAL(tn_mx->get(), 3);
    scheduler.run(100);
    mmu_state_histogram_tn.stopCounting(MMUState::RETIRE); // stop counting on Bucket 3
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_0->get(),  997); // no change
    EXPECT_EQUAL(tn_1->get(),  222); // no change
    EXPECT_EQUAL(tn_2->get(),  1); // no change
    EXPECT_EQUAL(tn_3->get(),  101); // 1 + 100
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 1321); // 1221 + 100
    EXPECT_EQUAL(tn_mx->get(), 3);

    mmu_state_histogram_tn.startCounting(MMUState::HIT); // start counting on Bucket 2
    EXPECT_EQUAL(tn_mx->get(), 3);
    scheduler.run(100);
    mmu_state_histogram_tn.stopCounting(MMUState::HIT); // stop counting on Bucket 2
    scheduler.run(2); // counts on Bucket 0 [idle value]
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_0->get(),  999); // 997 + 2
    EXPECT_EQUAL(tn_1->get(),  222); // no change
    EXPECT_EQUAL(tn_2->get(),  101); // 1 + 100
    EXPECT_EQUAL(tn_3->get(),  101); // no change
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 1423); // 1321 + 102
    EXPECT_EQUAL(tn_mx->get(), 3);

    // Verify stat accuracy for weighted average
    const auto cal = ((0.0 * tn_uf->get())  +
                      (0.0 * tn_0->get())   +
                      (1.0 * tn_1->get())   +
                      (2.0 * tn_2->get())   +
                      (3.0 * tn_3->get())   +
                      (3.0 * tn_of->get())) / tn_tt->get();
    EXPECT_EQUAL(si_avg.getValue(), cal);

    // Verify stat accuracy for weighted non-zero average
    const auto nonzero_cal = ((1.0 * tn_1->get())   +
                              (2.0 * tn_2->get())   +
                              (3.0 * tn_3->get())   +
                              (3.0 * tn_of->get())) / (tn_tt->get() - tn_0->get());
    EXPECT_EQUAL(si_nonzero_avg.getValue(), nonzero_cal);

    // Verify stat accuracy for fullness
    const auto fullness_cal = (tn_3->get() + tn_of->get());
    EXPECT_EQUAL(si_fullness.getValue(), fullness_cal);

    // Verify stat accuracy for fullness probability
    const auto fullness_prob_cal = fullness_cal / static_cast<double>(tn_tt->get());
    EXPECT_EQUAL(si_fullness_probability.getValue(), fullness_prob_cal);

    //it's now safe to tear down our dummy tree
    rtn.enterTeardown();
}

int main() {
    testOpStateHistogram();
    testUopStateHistogram();
    testMMUStateHistogram();

    REPORT_ERROR;
    return ERROR_CODE;
}
