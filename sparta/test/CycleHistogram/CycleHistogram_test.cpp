// <CycleHistogram_test> -*- C++ -*-

/**
 * \file CycleHistogram_test
 * \brief A test that creates a producer and consumer, and
 * then runs some test cases on CycleHistogramTreeNode and CycleHistogramStandalone
 */

#define ARGOS_TESTING = 1;

#include <string>
#include <iostream>

#include "sparta/kernel/Scheduler.hpp"
#include "sparta/sparta.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ResourceFactory.hpp"
#include "sparta/events/Event.hpp"
#include "sparta/events/StartupEvent.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/statistics/StatisticInstance.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/statistics/CycleHistogram.hpp"

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

double calculateStDev(const std::vector<uint64_t>& histogram_vector) {
    sparta_assert(!histogram_vector.empty());
    const double sum = std::accumulate(histogram_vector.begin(),
        histogram_vector.end(), 0.0);
    const double mean = sum / histogram_vector.size();
    double accum = 0.0;
    std::for_each(histogram_vector.begin(), histogram_vector.end(),
        [&](const uint64_t item) {
        accum += (item - mean) * (item - mean);
    });
    return std::sqrt(accum / (histogram_vector.size() - 1));
}

double getMeanBinCount(const std::vector<uint64_t>& histogram_vector) {
    sparta_assert(!histogram_vector.empty());
    const double sum = std::accumulate(histogram_vector.begin(),
        histogram_vector.end(), 0.0);
    return sum / histogram_vector.size();
};

TEST_INIT;

#define PRINT_ENTER_TEST  \
  std::cout << std::endl; \
  std::cout << "*************************************************************" \
            << "*** Beginning '" << __FUNCTION__ << "'" \
            << "*************************************************************" \
            << std::endl;

void binsOneThroughThree()
{
    sparta::Scheduler scheduler("test");

    sparta::Clock clk("clock", &scheduler);
    sparta::RootTreeNode rtn("root");
    rtn.setClock(&clk);

    sparta::ResourceFactory<DummyDevice, DummyDevice::ParameterSet> rfact;
    sparta::ResourceTreeNode dummy(&rtn, "dummy", "dummy node", &rfact);
    sparta::StatisticSet sset(&dummy);

    sparta::CycleHistogramTreeNode   cycle_histogram_tn(&rtn,  "cycle_histogram_tn", "Cycle Histogram Tree Node",  1, 3, 1, 2);
    sparta::CycleHistogramStandalone cycle_histogram_sa(&sset, &clk, "cycle_histogram_sa", "Cycle Histogram Standalone", 1, 7, 2);

    rtn.enterConfiguring();
    rtn.enterFinalized();
    EXPECT_NOTHROW(rtn.validatePreRun());

    sparta::StatisticDef *tn_avg = nullptr;
    EXPECT_NOTHROW(tn_avg = rtn.getChildAs<sparta::StatisticDef>("cycle_histogram_tn.stats.weighted_avg"));
    EXPECT_TRUE(tn_avg);
    sparta::StatisticInstance si_avg(tn_avg);

    sparta::StatisticDef *tn_nonzero_avg = nullptr;
    EXPECT_NOTHROW(tn_nonzero_avg = rtn.getChildAs<sparta::StatisticDef>(
        "cycle_histogram_tn.stats.weighted_nonzero_avg"));
    EXPECT_TRUE(tn_nonzero_avg);
    sparta::StatisticInstance si_nonzero_avg(tn_nonzero_avg);

    sparta::StatisticDef *tn_fullness = nullptr;
    EXPECT_NOTHROW(tn_fullness = rtn.getChildAs<sparta::StatisticDef>(
        "cycle_histogram_tn.stats.full"));
    EXPECT_TRUE(tn_fullness);
    sparta::StatisticInstance si_fullness(tn_fullness);

    sparta::StatisticDef *tn_fullness_probability = nullptr;
    EXPECT_NOTHROW(tn_fullness_probability = rtn.getChildAs<sparta::StatisticDef>(
        "cycle_histogram_tn.stats.full_probability"));
    EXPECT_TRUE(tn_fullness_probability);
    sparta::StatisticInstance si_fullness_probability(tn_fullness_probability);

    scheduler.finalize();

    std::cout << sset << std::endl;

    // proceed to tick 1, nothing should happen, but time advancement
    scheduler.run(1, true, false);

    EXPECT_EQUAL(cycle_histogram_tn.getNumBins(), 3);
    EXPECT_EQUAL(cycle_histogram_sa.getNumBins(), 4);

    sparta::CycleCounter *tn_1  = nullptr, *tn_2  = nullptr, *tn_3  = nullptr;
    sparta::CycleCounter *tn_uf = nullptr, *tn_of = nullptr, *tn_tt = nullptr;
    sparta::CycleCounter *sa_6  = nullptr, *sa_7  = nullptr;
    sparta::Counter      *tn_mx = nullptr;

    EXPECT_NOTHROW(tn_uf = rtn.getChildAs<sparta::CycleCounter>("cycle_histogram_tn.stats.UF"));
    EXPECT_NOTHROW(tn_1  = rtn.getChildAs<sparta::CycleCounter>("cycle_histogram_tn.stats.cycle_count1"));
    EXPECT_NOTHROW(tn_2  = rtn.getChildAs<sparta::CycleCounter>("cycle_histogram_tn.stats.cycle_count2"));
    EXPECT_NOTHROW(tn_3  = rtn.getChildAs<sparta::CycleCounter>("cycle_histogram_tn.stats.cycle_count3"));
    EXPECT_NOTHROW(tn_of = rtn.getChildAs<sparta::CycleCounter>("cycle_histogram_tn.stats.OF"));
    EXPECT_NOTHROW(tn_tt = rtn.getChildAs<sparta::CycleCounter>("cycle_histogram_tn.stats.total"));
    EXPECT_NOTHROW(tn_mx = rtn.getChildAs<sparta::Counter>("cycle_histogram_tn.stats.max_value"));
    EXPECT_TRUE(tn_uf);
    EXPECT_TRUE(tn_1);
    EXPECT_TRUE(tn_2);
    EXPECT_TRUE(tn_3);
    EXPECT_TRUE(tn_of);
    EXPECT_TRUE(tn_tt);
    EXPECT_TRUE(tn_mx);

    EXPECT_NOTHROW(sa_6  = sset.getCounterAs<sparta::CycleCounter>("cycle_histogram_sa_bin_5_6"));
    EXPECT_NOTHROW(sa_7  = sset.getCounterAs<sparta::CycleCounter>("cycle_histogram_sa_count7"));
    EXPECT_NOTEQUAL(sa_6,  nullptr);
    EXPECT_NOTEQUAL(sa_7,  nullptr);

    scheduler.run(111);
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_1->get(),  0);
    EXPECT_EQUAL(tn_2->get(),  111);
    EXPECT_EQUAL(tn_3->get(),  0);
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 111);

    cycle_histogram_tn.setValue(3);
    EXPECT_EQUAL(tn_mx->get(), 3);
    scheduler.run(222);
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_1->get(),  0);
    EXPECT_EQUAL(tn_2->get(),  111);
    EXPECT_EQUAL(tn_3->get(),  222);
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 333);

    cycle_histogram_tn.setValue(0);
    EXPECT_EQUAL(tn_mx->get(), 3);
    scheduler.run(333);
    EXPECT_EQUAL(tn_uf->get(), 333);
    EXPECT_EQUAL(tn_1->get(),  0);
    EXPECT_EQUAL(tn_2->get(),  111);
    EXPECT_EQUAL(tn_3->get(),  222);
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 666);

    cycle_histogram_tn.addValue(4);
    EXPECT_EQUAL(tn_mx->get(), 4);
    scheduler.run(444);
    EXPECT_EQUAL(tn_uf->get(), 333);
    EXPECT_EQUAL(tn_1->get(),    0);
    EXPECT_EQUAL(tn_2->get(),  554); // 111 + 443
    EXPECT_EQUAL(tn_3->get(),  222);
    EXPECT_EQUAL(tn_of->get(),   1); // addValue!
    EXPECT_EQUAL(tn_tt->get(),1110);

    cycle_histogram_tn.addValue(1);
    EXPECT_EQUAL(tn_mx->get(), 4);
    scheduler.run(1);
    EXPECT_EQUAL(tn_uf->get(), 333);
    EXPECT_EQUAL(tn_1->get(),    1); // addValue!
    EXPECT_EQUAL(tn_2->get(),  554); // 111 + 443 + 0
    EXPECT_EQUAL(tn_3->get(),  222);
    EXPECT_EQUAL(tn_of->get(),   1);
    EXPECT_EQUAL(tn_tt->get(),1111);

    cycle_histogram_tn.addValue(5);
    EXPECT_EQUAL(tn_mx->get(), 5);
    scheduler.run(1);
    EXPECT_EQUAL(tn_uf->get(), 333);
    EXPECT_EQUAL(tn_1->get(),    1);
    EXPECT_EQUAL(tn_2->get(),  554); // 111 + 443 + 0 + 0
    EXPECT_EQUAL(tn_3->get(),  222);
    EXPECT_EQUAL(tn_of->get(),   2); // addValue!
    EXPECT_EQUAL(tn_tt->get(),1112);

    const auto cal = (1.0 * tn_uf->get() + 1 * tn_1->get() + 2 *tn_2->get() + 3 * tn_3->get() + 3 * tn_of->get() ) / tn_tt->get();
    EXPECT_EQUAL(si_avg.getValue(), cal);

    const auto nonzero_cal = cal;
    EXPECT_EQUAL(si_nonzero_avg.getValue(), nonzero_cal);

    const auto fullness_cal = (tn_3->get() + tn_of->get());
    EXPECT_EQUAL(si_fullness.getValue(), fullness_cal);

    const auto fullness_prob_cal = fullness_cal / static_cast<double>(tn_tt->get());
    EXPECT_EQUAL(si_fullness_probability.getValue(), fullness_prob_cal);

    // Representation of bins of this histogram.
    std::vector<uint64_t> histogram_vector {1, 554, 222, 2, 333};
    EXPECT_WITHIN_TOLERANCE(calculateStDev(histogram_vector), cycle_histogram_tn.getStandardDeviation(), 1E-6);
    EXPECT_EQUAL(getMeanBinCount(histogram_vector), cycle_histogram_tn.getMeanBinCount());
    const auto& bin_vector = cycle_histogram_tn.getRegularBin();
    std::for_each(bin_vector.begin(), bin_vector.end(),
        [&histogram_vector](const sparta::CycleCounter& c){
        static std::size_t i {0};
        EXPECT_EQUAL(static_cast<double>(c), histogram_vector[i++]);
    });
    EXPECT_EQUAL(cycle_histogram_tn.getUnderflowBin(), 333u);
    EXPECT_EQUAL(cycle_histogram_tn.getOverflowBin(), 2u);
    double total_vals = cycle_histogram_tn.getAggCycles();
    EXPECT_EQUAL(cycle_histogram_tn.getUnderflowProbability(), static_cast<double>(333)/total_vals);
    EXPECT_EQUAL(cycle_histogram_tn.getOverflowProbability(), static_cast<double>(2)/total_vals);
    const auto& bin_prob_vector = cycle_histogram_tn.recomputeRegularBinProbabilities();
    std::for_each(bin_vector.begin(), bin_vector.end(),
        [&bin_prob_vector, &total_vals](const sparta::CycleCounter& c){
        static std::size_t i {0};
        EXPECT_EQUAL(bin_prob_vector[i++], static_cast<double>(c)/total_vals);
    });

    //it's now safe to tear down our dummy tree
    rtn.enterTeardown();
}

void binsZeroThroughThree()
{
    sparta::Scheduler scheduler("test");

    sparta::Clock clk("clock", &scheduler);
    sparta::RootTreeNode rtn("root");
    rtn.setClock(&clk);

    sparta::ResourceFactory<DummyDevice, DummyDevice::ParameterSet> rfact;
    sparta::ResourceTreeNode dummy(&rtn, "dummy", "dummy node", &rfact);
    sparta::StatisticSet sset(&dummy);

    sparta::CycleHistogramTreeNode   cycle_histogram_tn(&rtn,  "cycle_histogram_tn", "Cycle Histogram Tree Node",  0, 3, 1, 2);
    sparta::CycleHistogramStandalone cycle_histogram_sa(&sset, &clk, "cycle_histogram_sa", "Cycle Histogram Standalone", 1, 7, 2);

    rtn.enterConfiguring();
    rtn.enterFinalized();
    EXPECT_NOTHROW(rtn.validatePreRun());

    sparta::StatisticDef *tn_avg = nullptr;
    EXPECT_NOTHROW(tn_avg = rtn.getChildAs<sparta::StatisticDef>("cycle_histogram_tn.stats.weighted_avg"));
    EXPECT_TRUE(tn_avg);
    sparta::StatisticInstance si_avg(tn_avg);

    sparta::StatisticDef *tn_nonzero_avg = nullptr;
    EXPECT_NOTHROW(tn_nonzero_avg = rtn.getChildAs<sparta::StatisticDef>(
        "cycle_histogram_tn.stats.weighted_nonzero_avg"));
    EXPECT_TRUE(tn_nonzero_avg);
    sparta::StatisticInstance si_nonzero_avg(tn_nonzero_avg);

    sparta::StatisticDef *tn_fullness = nullptr;
    EXPECT_NOTHROW(tn_fullness = rtn.getChildAs<sparta::StatisticDef>(
        "cycle_histogram_tn.stats.full"));
    EXPECT_TRUE(tn_fullness);
    sparta::StatisticInstance si_fullness(tn_fullness);

    sparta::StatisticDef *tn_fullness_probability = nullptr;
    EXPECT_NOTHROW(tn_fullness_probability = rtn.getChildAs<sparta::StatisticDef>(
        "cycle_histogram_tn.stats.full_probability"));
    EXPECT_TRUE(tn_fullness_probability);
    sparta::StatisticInstance si_fullness_probability(tn_fullness_probability);

    scheduler.finalize();

    std::cout << sset << std::endl;

    // proceed to tick 1, nothing should happen, but time advancement
    scheduler.run(1, true, false);

    EXPECT_EQUAL(cycle_histogram_tn.getNumBins(), 4);
    EXPECT_EQUAL(cycle_histogram_sa.getNumBins(), 4);

    sparta::CycleCounter *tn_0  = nullptr;
    sparta::CycleCounter *tn_1  = nullptr, *tn_2  = nullptr, *tn_3  = nullptr;
    sparta::CycleCounter *tn_uf = nullptr, *tn_of = nullptr, *tn_tt = nullptr;
    sparta::CycleCounter *sa_6  = nullptr, *sa_7  = nullptr;
    sparta::Counter      *tn_mx = nullptr;

    EXPECT_NOTHROW(tn_uf = rtn.getChildAs<sparta::CycleCounter>("cycle_histogram_tn.stats.UF"));
    EXPECT_NOTHROW(tn_0  = rtn.getChildAs<sparta::CycleCounter>("cycle_histogram_tn.stats.cycle_count0"));
    EXPECT_NOTHROW(tn_1  = rtn.getChildAs<sparta::CycleCounter>("cycle_histogram_tn.stats.cycle_count1"));
    EXPECT_NOTHROW(tn_2  = rtn.getChildAs<sparta::CycleCounter>("cycle_histogram_tn.stats.cycle_count2"));
    EXPECT_NOTHROW(tn_3  = rtn.getChildAs<sparta::CycleCounter>("cycle_histogram_tn.stats.cycle_count3"));
    EXPECT_NOTHROW(tn_of = rtn.getChildAs<sparta::CycleCounter>("cycle_histogram_tn.stats.OF"));
    EXPECT_NOTHROW(tn_tt = rtn.getChildAs<sparta::CycleCounter>("cycle_histogram_tn.stats.total"));
    EXPECT_NOTHROW(tn_mx = rtn.getChildAs<sparta::Counter>("cycle_histogram_tn.stats.max_value"));
    EXPECT_TRUE(tn_uf);
    EXPECT_TRUE(tn_0);
    EXPECT_TRUE(tn_1);
    EXPECT_TRUE(tn_2);
    EXPECT_TRUE(tn_3);
    EXPECT_TRUE(tn_of);
    EXPECT_TRUE(tn_tt);
    EXPECT_TRUE(tn_mx);

    EXPECT_NOTHROW(sa_6  = sset.getCounterAs<sparta::CycleCounter>("cycle_histogram_sa_bin_5_6"));
    EXPECT_NOTHROW(sa_7  = sset.getCounterAs<sparta::CycleCounter>("cycle_histogram_sa_count7"));
    EXPECT_NOTEQUAL(sa_6,  nullptr);
    EXPECT_NOTEQUAL(sa_7,  nullptr);

    scheduler.run(111);
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_0->get(),  0);
    EXPECT_EQUAL(tn_1->get(),  0);
    EXPECT_EQUAL(tn_2->get(),  111);
    EXPECT_EQUAL(tn_3->get(),  0);
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 111);

    cycle_histogram_tn.setValue(3);
    EXPECT_EQUAL(tn_mx->get(), 3);
    scheduler.run(222);
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_0->get(),  0);
    EXPECT_EQUAL(tn_1->get(),  0);
    EXPECT_EQUAL(tn_2->get(),  111);
    EXPECT_EQUAL(tn_3->get(),  222);
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 333);

    cycle_histogram_tn.setValue(0);
    EXPECT_EQUAL(tn_mx->get(), 3);
    scheduler.run(333);
    EXPECT_EQUAL(tn_uf->get(), 0);
    EXPECT_EQUAL(tn_0->get(),  333);
    EXPECT_EQUAL(tn_1->get(),  0);
    EXPECT_EQUAL(tn_2->get(),  111);
    EXPECT_EQUAL(tn_3->get(),  222);
    EXPECT_EQUAL(tn_of->get(), 0);
    EXPECT_EQUAL(tn_tt->get(), 666);

    cycle_histogram_tn.addValue(4);
    EXPECT_EQUAL(tn_mx->get(), 4);
    scheduler.run(444);
    EXPECT_EQUAL(tn_uf->get(),   0);
    EXPECT_EQUAL(tn_0->get(),  333);
    EXPECT_EQUAL(tn_1->get(),    0);
    EXPECT_EQUAL(tn_2->get(),  554); // 111 + 443
    EXPECT_EQUAL(tn_3->get(),  222);
    EXPECT_EQUAL(tn_of->get(),   1); // addValue!
    EXPECT_EQUAL(tn_tt->get(),1110);

    cycle_histogram_tn.addValue(1);
    EXPECT_EQUAL(tn_mx->get(), 4);
    scheduler.run(1);
    EXPECT_EQUAL(tn_uf->get(),   0);
    EXPECT_EQUAL(tn_0->get(),  333);
    EXPECT_EQUAL(tn_1->get(),    1); // addValue!
    EXPECT_EQUAL(tn_2->get(),  554); // 111 + 443 + 0
    EXPECT_EQUAL(tn_3->get(),  222);
    EXPECT_EQUAL(tn_of->get(),   1);
    EXPECT_EQUAL(tn_tt->get(),1111);

    cycle_histogram_tn.addValue(5);
    EXPECT_EQUAL(tn_mx->get(), 5);
    scheduler.run(1);
    EXPECT_EQUAL(tn_uf->get(),   0);
    EXPECT_EQUAL(tn_0->get(),  333);
    EXPECT_EQUAL(tn_1->get(),    1);
    EXPECT_EQUAL(tn_2->get(),  554); // 111 + 443 + 0 + 0
    EXPECT_EQUAL(tn_3->get(),  222);
    EXPECT_EQUAL(tn_of->get(),   2); // addValue!
    EXPECT_EQUAL(tn_tt->get(),1112);

    const auto cal = (1.0 * tn_uf->get() + 0 * tn_0->get() + 1 * tn_1->get() + 2 *tn_2->get() + 3 * tn_3->get() + 3 * tn_of->get() ) / tn_tt->get();
    EXPECT_EQUAL(si_avg.getValue(), cal);

    const auto nonzero_cal = (1.0 * tn_uf->get() + 1 * tn_1->get() + 2 *tn_2->get() + 3 * tn_3->get() + 3 * tn_of->get() ) / (tn_tt->get() - tn_0->get());
    EXPECT_EQUAL(si_nonzero_avg.getValue(), nonzero_cal);

    EXPECT_NOTEQUAL(cal, nonzero_cal);

    const auto fullness_cal = (tn_3->get() + tn_of->get());
    EXPECT_EQUAL(si_fullness.getValue(), fullness_cal);

    const auto fullness_prob_cal = fullness_cal / static_cast<double>(tn_tt->get());
    EXPECT_EQUAL(si_fullness_probability.getValue(), fullness_prob_cal);

    // Representation of bins of this histogram.
    std::vector<uint64_t> histogram_vector {333, 1, 554, 222, 0, 2};
    EXPECT_WITHIN_TOLERANCE(calculateStDev(histogram_vector), cycle_histogram_tn.getStandardDeviation(), 1E-6);
    EXPECT_EQUAL(getMeanBinCount(histogram_vector), cycle_histogram_tn.getMeanBinCount());
    const auto& bin_vector = cycle_histogram_tn.getRegularBin();
    std::for_each(bin_vector.begin(), bin_vector.end(),
        [&histogram_vector](const sparta::CycleCounter& c){
        static std::size_t i {0};
        EXPECT_EQUAL(static_cast<double>(c), histogram_vector[i++]);
    });
    EXPECT_EQUAL(cycle_histogram_tn.getUnderflowBin(), 0u);
    EXPECT_EQUAL(cycle_histogram_tn.getOverflowBin(), 2u);
    double total_vals = cycle_histogram_tn.getAggCycles();
    EXPECT_EQUAL(cycle_histogram_tn.getUnderflowProbability(), 0);
    EXPECT_EQUAL(cycle_histogram_tn.getOverflowProbability(), static_cast<double>(2)/total_vals);
    const auto& bin_prob_vector = cycle_histogram_tn.recomputeRegularBinProbabilities();
    std::for_each(bin_vector.begin(), bin_vector.end(),
        [&bin_prob_vector, &total_vals](const sparta::CycleCounter& c){
        static std::size_t i {0};
        EXPECT_EQUAL(bin_prob_vector[i++], static_cast<double>(c)/total_vals);
    });

    //it's now safe to tear down our dummy tree
    rtn.enterTeardown();
}

int main()
{
    binsOneThroughThree();
    binsZeroThroughThree();

    ENSURE_ALL_REACHED(0);
    REPORT_ERROR;
    return ERROR_CODE;
}
