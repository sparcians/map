

#include "Producer.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/ports/Port.hpp"
#include "sparta/utils/SpartaTester.hpp"

Producer::Producer (sparta::TreeNode * rtn,
                    sparta::DataOutPort<double> * delay0,
                    sparta::DataOutPort<double> * delay1,
                    sparta::DataOutPort<double> * delay10,
                    sparta::DataOutPort<double> * delay1_non_continuing,
                    sparta::SignalOutPort * signalout,
                    sparta::Clock * clk) :
    root_(rtn),
    delay0_(delay0),
    delay1_(delay1),
    delay10_(delay10),
    delay1_non_continuing_(delay1_non_continuing),
    signal_out_(signalout),
    clk_(clk)
{
    non_continuing_port_driver_.setContinuing(false);
    delay0_->registerProducingEvent(delay_write_ev_);
}

Producer::~Producer () { }

void Producer::scheduleTests()
{
    delay_write_ev_.schedule(1);
    non_continuing_port_driver_.schedule(1);
}

void Producer::writeDelays ()
{
    uint32_t i = 0;
    while (i < 10) {
        double data = clk_->currentCycle() + i;
        std::cout << "Writing (delay1): " << data
                  << " on cycle " << clk_->currentCycle() << std::endl;
        delay1_->send(data, 1 + i);
        std::cout << "Writing (delay10): " << data
                  << " on cycle " << clk_->currentCycle() << std::endl;
        delay10_->send(data, 10 + i);
        EXPECT_TRUE(delay10_->isDriven(10 + i));
        EXPECT_TRUE(delay10_->isDriven());
        ++i;
        signal_out_->send(i);
    }

    EXPECT_FALSE(delay0_->isDriven());
    EXPECT_FALSE(delay0_->isDriven(0));

    // Send something 0-cycle
    delay0_->send(10.0, 0);

    // The 0-delay port delivers the data immediately.  Can't tell if
    // it were driven.
    EXPECT_FALSE(delay0_->isDriven());
    EXPECT_FALSE(delay0_->isDriven(0));
}

// Send the value 10 across the non-continuing port basically forever.
// Schedule the non_continuing_port_driver_ for the next cycle always.
void Producer::driveNonContinuingPort()
{
    delay1_non_continuing_->send(10, 1);
    non_continuing_port_driver_.schedule(1);
    sparta_assert(++non_cont_count_ < 10000);
}
