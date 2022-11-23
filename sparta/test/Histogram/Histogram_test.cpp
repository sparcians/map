// <Histogram_test> -*- C++ -*-

/**
 * \file Histogram_test
 * \brief A test that creates HistogramStandalone and HistogramTreeNode and
 * then runs some test cases for functionality verification.
 */

#include <string>
#include <iostream>

#include "sparta/kernel/Scheduler.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/Tree.hpp"
#include "sparta/statistics/Histogram.hpp"
#include "sparta/statistics/CycleHistogram.hpp"
#include "sparta/sparta.hpp"
#include "sparta/utils/SpartaTester.hpp"

TEST_INIT

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

int main() {
    sparta::Scheduler scheduler("Histogram_test");
    sparta::Clock clk("clk", &scheduler);
    sparta::RootTreeNode rtn("root");
    rtn.setClock(&clk);

    sparta::HistogramTreeNode histogram_tn(&rtn, "Histogram_tn_1", "Histogram Tree Node 1",
        1, 10, 2);

    EXPECT_EQUAL(histogram_tn.getNumBins(), 5);
    EXPECT_EQUAL(histogram_tn.getHistogramUpperValue(), 10);
    EXPECT_EQUAL(histogram_tn.getHistogramLowerValue(), 1);
    EXPECT_EQUAL(histogram_tn.getNumValuesPerBin(), 2);
    for(std::size_t i = 0; i < 12; ++i){
        histogram_tn.addValue(i);
    }

    // Representation of bins of this histogram.
    std::vector<uint64_t> histogram_vector {2, 2, 2, 2, 2, 1, 1};
    EXPECT_EQUAL(calculateStDev(histogram_vector), histogram_tn.getStandardDeviation());
    EXPECT_EQUAL(getMeanBinCount(histogram_vector), histogram_tn.getMeanBinCount());
    const auto& bin_vector = histogram_tn.getRegularBin();
    std::for_each(bin_vector.begin(), bin_vector.end(),
        [&histogram_vector](const sparta::Counter& c){
        static std::size_t i {0};
        EXPECT_EQUAL(static_cast<double>(c), histogram_vector[i++]);
    });
    EXPECT_EQUAL(histogram_tn.getUnderflowBin(), 1u);
    EXPECT_EQUAL(histogram_tn.getOverflowBin(), 1u);
    double total_vals = histogram_tn.getAggValues();
    EXPECT_EQUAL(histogram_tn.getUnderflowProbability(), static_cast<double>(1)/total_vals);
    EXPECT_EQUAL(histogram_tn.getOverflowProbability(), static_cast<double>(1)/total_vals);
    const auto& bin_prob_vector = histogram_tn.recomputeRegularBinProbabilities();
    std::for_each(bin_vector.begin(), bin_vector.end(),
        [&bin_prob_vector, &total_vals](const sparta::Counter& c){
        static std::size_t i {0};
        EXPECT_EQUAL(bin_prob_vector[i++], static_cast<double>(c)/total_vals);
    });

    sparta::HistogramTreeNode histogram_tn_2(&rtn, "Histogram_tn_2", "Histogram Tree Node 2",
        5, 20, 4);
    EXPECT_EQUAL(histogram_tn_2.getNumBins(), 4);
    EXPECT_EQUAL(histogram_tn_2.getHistogramUpperValue(), 20);
    EXPECT_EQUAL(histogram_tn_2.getHistogramLowerValue(), 5);
    EXPECT_EQUAL(histogram_tn_2.getNumValuesPerBin(), 4);
    std::vector<uint64_t> fill_vector_values {5, 6, 7, 8, 9, 10, 20, 15, 18, 4, 45, 9};
    for(auto i : fill_vector_values){
        histogram_tn_2.addValue(i);
    }

    // Representation of bins of this histogram.
    std::vector<uint64_t> histogram_vector_2 {4, 3, 1, 2, 1, 1};
    EXPECT_EQUAL(calculateStDev(histogram_vector_2), histogram_tn_2.getStandardDeviation());
    EXPECT_EQUAL(getMeanBinCount(histogram_vector_2), histogram_tn_2.getMeanBinCount());
    const auto& bin_vector_2 = histogram_tn_2.getRegularBin();
    std::for_each(bin_vector_2.begin(), bin_vector_2.end(),
        [&histogram_vector_2](const sparta::Counter& c){
        static std::size_t i {0};
        EXPECT_EQUAL(static_cast<double>(c), histogram_vector_2[i++]);
    });
    EXPECT_EQUAL(histogram_tn_2.getUnderflowBin(), 1u);
    EXPECT_EQUAL(histogram_tn_2.getOverflowBin(), 1u);
    double total_vals_2 = histogram_tn_2.getAggValues();
    EXPECT_EQUAL(histogram_tn_2.getUnderflowProbability(), static_cast<double>(1)/total_vals_2);
    EXPECT_EQUAL(histogram_tn_2.getOverflowProbability(), static_cast<double>(1)/total_vals_2);
    const auto& bin_prob_vector_2 = histogram_tn_2.recomputeRegularBinProbabilities();
    std::for_each(bin_vector_2.begin(), bin_vector_2.end(),
        [&bin_prob_vector_2, total_vals_2](const sparta::Counter& c){
        static std::size_t i {0};
        EXPECT_EQUAL(bin_prob_vector_2[i++], static_cast<double>(c)/total_vals_2);
    });

    rtn.enterConfiguring();
    rtn.enterFinalized();
    EXPECT_NOTHROW(rtn.validatePreRun());
    scheduler.finalize();

    rtn.enterTeardown();
    return 0;
}
