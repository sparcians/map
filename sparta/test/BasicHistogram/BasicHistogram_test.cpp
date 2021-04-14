// <BasicHistogram_test> -*- C++ -*-

/**
 * \file BasicHistogram_test
 * \brief A test that creates BasicHistogram and
 * then runs some test cases for functionality verification.
 */
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/statistics/BasicHistogram.hpp"
#include "sparta/sparta.hpp"
#include "sparta/utils/SpartaTester.hpp"

TEST_INIT;

int main()
{
    sparta::Scheduler scheduler("BasicHistogram_test");
    sparta::Clock clk("clk", &scheduler);
    sparta::RootTreeNode rtn("root");
    rtn.setClock(&clk);

    sparta::StatisticSet ss(&rtn);
    sparta::BasicHistogram<int64_t, false> histogram(ss, "test", "Test", {0, 4, 8});

    for(std::size_t i = -1; i < 12; ++i){
        histogram.addValue(i);
    }

    sparta::BasicHistogram<int64_t, true> faulting_histogram(ss, "test2", "Faulting test", {0, 4, 8});
    EXPECT_THROW(faulting_histogram.addValue(-1));
    EXPECT_NOTHROW(faulting_histogram.addValue(0));

    using ThrowHistogram = sparta::BasicHistogram<int64_t, true>;
    EXPECT_THROW(ThrowHistogram nonsorted_histogram(ss, "test3", "Not sorted test", {12, 0, 4, 8}));

    rtn.enterConfiguring();
    rtn.enterFinalized();
    EXPECT_NOTHROW(rtn.validatePreRun());
    scheduler.finalize();

    rtn.enterTeardown();

    std::cout << "Test passed" << std::endl;
    return 0;
}
