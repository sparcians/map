

#include "Producer.hpp"
#include "MessageCategories.hpp"

const char * Producer::name = "producer";

Producer::Producer (sparta::TreeNode * node,
                    const ProducerParameterSet * p) :
    sparta::Resource(node, name),
    port_set_(node),
    event_set_(node),
    max_ints_to_send_(p->max_ints_to_send),
    stat_set_(node),
    producer_info_(node, message_categories::INFO, "Producer Info Messages")
{
    // Register a go-handler when the consumer sends a go request
    producer_go_port_.registerConsumerHandler(CREATE_SPARTA_HANDLER(Producer, produceData_));
    ProducerParameterSet* pnc = const_cast<ProducerParameterSet*>(p);
    pnc->test_param = pnc->test_param + 1;
    std::cout << " Modify test_b Producer(): " << pnc->test_param.getValue() << std::endl;
    p->arch_override_test_param.ignore();

}

void Producer::produceData_()
{
    if(current_ints_count_ < max_ints_to_send_)
    {
        if(SPARTA_EXPECT_FALSE(producer_info_)) {
            producer_info_ << "Producer: " << getName() << "@" <<  getContainer()->getLocation()
                           << " Sending " << current_ints_count_ << std::endl;
        }

        // Sent the integer to the listening consumers
        producer_out_port_.send(current_ints_count_);
        ++current_ints_count_;
        ++num_produced_;
    }
    else if(SPARTA_EXPECT_FALSE(producer_info_)) {
        producer_info_ << "Producer: " << getName() << "@" << getContainer()->getLocation()
                       << " Is done sending data " << current_ints_count_;
    }
}
