

#include "Consumer.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/events/Event.hpp"

#define NON_CONT_THRESHOLD 100

Consumer::Consumer (sparta::TreeNode * rtn,
                    sparta::DataInPort<double> * delay0,
                    sparta::DataInPort<double> * delay1,
                    sparta::DataInPort<double> * delay10,
                    sparta::DataInPort<double> * delay1_non_continuing,
                    sparta::SignalInPort * signal_port,
                    sparta::Clock *clk) :
    sparta::Resource("Consumer", clk),
    root_(rtn),
    delay0_(delay0),
    delay1_(delay1),
    delay10_(delay10),
    delay1_non_continuing_(delay1_non_continuing),
    signal_port_(signal_port),
    delay1_size_(1),
    delay10_size_(10),
    delay1_time_(0),
    delay10_time_(0),
    num_times_(0),
    clk_(clk)
{
    delay0_->registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Consumer, myDelay0EventCallback, double));
    delay0_->registerConsumerEvent(delay0_receive_event_.getScheduleable());
    delay0_->registerConsumerEvent(delay0_receive_event_port_update_);

    delay1_->registerConsumerEvent(delay1_receive_event_);
    delay1_->registerConsumerEvent(delay1_receive_event_update_phase_);

    delay10_->registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA
                                      (Consumer, myDelay10EventHandler, double));

    // Can't register another one...  test this
    EXPECT_THROW(delay10_->registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA
                                           (Consumer, myDelay10EventHandler, double)));
    delay10_->registerConsumerEvent(delay10_receive_event_);

    signal_port_->registerConsumerHandler(CREATE_SPARTA_HANDLER(Consumer, mySignalEventCallback));

    // Should throw 'cause SignalPorts do not take handlers with 1 argument
    EXPECT_THROW(signal_port_->registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Consumer, myDelay10EventHandler, double)));

    // Should throw 'cause SignalPorts only support one handler
    EXPECT_THROW(signal_port_->registerConsumerHandler(CREATE_SPARTA_HANDLER(Consumer, mySignalEventHandler)));

    delay1_non_continuing_->registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Consumer, myNonContinuingEventCallback, double));


    // This shouldn't compile
    //delay1_non_continuing_->registerConsumerEvent(bad_event_);
    //*delay1_non_continuing_ >> bad_event_;

    // This really doesn't compile.
    // sparta::Scheduleable & sched = bad_event_;
    // delay1_non_continuing_->registerEvent(sched);
}

void Consumer::myDelay0EventCallbackPortUpdate ()
{
    sparta_assert(!"This is a zero-cycle port and this method, "
                "which is on PortUpdate Phase got called.  This is bad");
}

void Consumer::myDelay0EventCallback(const double &)
{
}

void Consumer::myDelay1EventCallbackPortUpdate ()
{
    port_update_call_made_ = true;
}

void Consumer::myDelay1EventCallback ()
{
    EXPECT_TRUE(delay1_->dataReceivedThisCycle());
    EXPECT_TRUE(delay1_->dataReceived());
    EXPECT_TRUE(delay1_->isDriven());
    EXPECT_TRUE(delay1_->isDriven(0));

    // The myDelay1EventCallbackPortUpdate should have been called first automatically
    EXPECT_TRUE(port_update_call_made_);
    port_update_call_made_ = false;

    if(delay1_time_ != 0) {
        EXPECT_EQUAL((getClock()->currentCycle() - delay1_time_), delay1_size_);
    }
    delay1_time_ = getClock()->currentCycle();

    EXPECT_TRUE(delay1_->getReceivedTimeStamp() == delay1_time_);

    std::cout << "Consumer: Read, Delay1: " << delay1_->pullData() << " on cycle "
              << clk_->currentCycle() << std::endl;

    EXPECT_FALSE(delay1_->dataReceivedThisCycle());
    EXPECT_FALSE(delay1_->dataReceived());
    EXPECT_THROW(delay1_->pullData());

}

void Consumer::myDelay10EventHandler(const double & dat)
{
    EXPECT_FALSE(delay10_dat_.isValid());
    delay10_dat_ = dat;
    delay10_receive_event_.schedule(sparta::Clock::Cycle(0));
}

void Consumer::myDelay10EventCallback ()
{
    // The handler, that sets the data SHOULD be called before this
    // event callback
    EXPECT_TRUE(delay10_dat_.isValid());

    double time_appended = delay10_->pullData();
    EXPECT_EQUAL(time_appended, delay10_dat_.getValue());

    if(delay10_time_ != 0) {
        EXPECT_EQUAL((getClock()->currentCycle() - time_appended), delay10_size_);
    }
    delay10_time_ = getClock()->currentCycle();

    std::cout << "Consumer: Read, Delay10: " << time_appended << " on cycle "
              << clk_->currentCycle() << std::endl;
    delay10_dat_.clearValid();
}

void Consumer::myNonContinuingEventCallback(const double &)
{
    std::cout << __FUNCTION__ << ": Got data: "
              << delay1_non_continuing_->pullData() << std::endl;
    ++num_times_got_non_continuing_data_;
    sparta_assert(num_times_got_non_continuing_data_ < NON_CONT_THRESHOLD);
}


void Consumer::mySignalEventCallback ()
{
    ++num_times_;
    std::cout << "Signal EventCallback got called: " << num_times_ << std::endl;
}
