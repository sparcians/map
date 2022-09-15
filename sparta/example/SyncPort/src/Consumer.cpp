// <Consumer.cpp> -*- C++ -*-

#include "Consumer.hpp"
#include "MessageCategories.hpp"
#include "sparta/events/StartupEvent.hpp"

const char * Consumer::name = "consumer";

Consumer::Consumer (sparta::TreeNode * node,
                    const ConsumerParameterSet * p) :
    sparta::Unit(node, name),
    num_producers_(p->num_producers),
    consumer_log_(node, message_categories::INFO, "Consumer Info Messages")
{
    (void)p;

    // Set up the producer go outports -- these are the ports used to
    // signal the producers that this consumer is ready for it.
    for(uint32_t i = 0; i < num_producers_; ++i) {
        std::stringstream str;
        str << "producer" << i << "_go_port";
        producer_go_port_.emplace_back(new sparta::SignalOutPort(&unit_port_set_, str.str()));
    }

    // Register callback to receive data on the InPort
    consumer_in_port_.
        registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Consumer, receiveData_, uint32_t));

    // Get the ball rolling
    sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(Consumer, signalNextProducer_));

}

void Consumer::signalNextProducer_()
{
    // Tell the next producer to go
    producer_go_port_[current_producer_]->send();
    ++current_producer_;
    if(current_producer_ == num_producers_) {
        current_producer_ = 0;
    }
}

void Consumer::receiveData_(const uint32_t & dat)
{
    sparta_assert(arrived_data_.isValid() == false,
                  "Somehow, data wasn't cleared in this consumer: " << getName());
    arrived_data_ = dat;

    // Schedule a consumption this cycle
    ev_data_arrived_.schedule();

    // Signal the next producer
    signalNextProducer_();
}

void Consumer::dataArrived_()
{
    if(SPARTA_EXPECT_FALSE(consumer_log_)) {
        consumer_log_ << "Got data '" << arrived_data_ << "' on cycle: " << getClock()->currentCycle();
    }
    ++num_consumed_;
    arrived_data_.clearValid();
}
