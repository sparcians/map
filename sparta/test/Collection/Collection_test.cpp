

#include "sparta/collection/Collectable.hpp"
#include "sparta/collection/PipelineCollector.hpp"

#include "sparta/utils/SpartaTester.hpp"

#include "sparta/simulation/Clock.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/kernel/Scheduler.hpp"

#include <iostream>

struct EmptyData {};
std::ostream & operator<<(std::ostream & os, const EmptyData &) {
    return os;
}

void testEmptyCollection()
{

    // Test communication between blocks using ports
    sparta::Scheduler sched;
    sparta::ClockManager cm(&sched);
    sparta::RootTreeNode rtn;
    sparta::Clock::Handle root_clk;
    root_clk = cm.makeRoot(&rtn, "root_clk");
    cm.normalize();
    rtn.setClock(root_clk.get());

    sparta::collection::Collectable<EmptyData> collector(&rtn, "empty_collection_test");

    rtn.enterConfiguring();
    rtn.enterFinalized();

    sparta::collection::PipelineCollector pc("emptyPipe", 1000000,
                                             root_clk.get(), &rtn);

    sched.finalize();

    // Order matters -- need to finalize the scheduler before we start
    // collecting.
    pc.startCollection(&rtn);

    EmptyData dat;

    collector.collect(dat);
    collector.collect(dat);
    collector.collect(dat);
    collector.collect(dat);
    collector.collect(dat);
    collector.collect(dat);
    collector.collect(dat);
    collector.collect(dat);
    collector.collect(dat);
    collector.collect(dat);
    sched.run(1, true);

    rtn.enterTeardown();
    pc.destroy();

    std::fstream record_file("emptyPiperecord.bin", std::fstream::in | std::fstream::binary );
    EXPECT_TRUE(record_file.peek() == std::ifstream::traits_type::eof());
}

int main()
{
    testEmptyCollection();

    REPORT_ERROR;
    return ERROR_CODE;
}
