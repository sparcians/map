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

TEST_INIT

class TestBasicHistogram : public sparta::BasicHistogram<int64_t, false>
{
public:
    TestBasicHistogram(sparta::StatisticSet &sset,
                       const std::string &name,
                       const std::string &desc,
                       const std::vector<int64_t> &buckets) :
        BasicHistogram<int64_t, false>(sset, name, desc, buckets),
        sset_(sset)
    {
    }

    uint64_t get(size_t i) { return sset_.getCounters()[i]->get(); }
private:
    const sparta::StatisticSet &sset_;
};

int main()
{
    sparta::Scheduler scheduler("BasicHistogram_test");
    sparta::Clock clk("clk", &scheduler);
    sparta::RootTreeNode rtn("root");
    rtn.setClock(&clk);

    // requirements for building
    sparta::StatisticSet ss(&rtn);
    std::vector<int64_t> buckets{0, 4, 8};

    TestBasicHistogram histogram(ss, "test", "Test", buckets);
    for(int64_t i = -1; i < 4; ++i) {
        histogram.addValue(i);
    }
    EXPECT_EQUAL(histogram.get(0), 5);

    for(int64_t i = 4; i < 8; ++i) {
        histogram.addValue(i);
    }
    EXPECT_EQUAL(histogram.get(1), 4);

    for(int64_t i = 8; i < 12; ++i) {
        histogram.addValue(i);
    }
    EXPECT_EQUAL(histogram.get(2), 4);

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

    REPORT_ERROR;
    return ERROR_CODE;
}
