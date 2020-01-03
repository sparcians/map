// <Audience> -*- C++ -*-


#include <iostream>
#include <inttypes.h>
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/simulation/Audience.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/events/Event.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/PayloadEvent.hpp"

TEST_INIT;

using namespace sparta;

class Observer
{
public:
    Observer(const std::string& name):
        name_(name),
        activations_(0),
        expected_data_(0)
    {}

    void activate()
    {
        std::cout << "Observer(" << name_ << ")::activate()" << std::endl;
        ++activations_;
    }

    template<class DataType>
    void activate(const DataType &dat)
    {
        std::cout << "Observer(" << name_ << ")::activate<>(" << dat << ")" << std::endl;
        EXPECT_EQUAL(dat, expected_data_);
        ++activations_;
        ++expected_data_;
    }

    uint32_t getActivations() const
    {
        return activations_;
    }

private:
    std::string name_;
    uint32_t    activations_;
    uint32_t    expected_data_;
};

int main()
{
    sparta::Scheduler sched;
    sparta::RootTreeNode rtn;
    sparta::Clock clk("clock", &sched);
    rtn.setClock(&clk);

    EXPECT_TRUE(sched.getCurrentTick() == 0);
    EXPECT_TRUE(sched.isRunning() == 0);

    sparta::EventSet es(&rtn);

    Observer obs("Foo");

    Event<> e_proto(&es, "e_proto",
                    CREATE_SPARTA_HANDLER_WITH_OBJ(Observer, &obs, activate));

    e_proto.setScheduleableClock(&clk);
    e_proto.setScheduler(clk.getScheduler());
    PayloadEvent<uint32_t> p_proto(&es, "p_proto",
                                   CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(Observer, &obs, activate, uint32_t));

    p_proto.getScheduleable().setScheduleableClock(&clk);
    p_proto.getScheduleable().setScheduler(clk.getScheduler());
    sched.finalize();

    Audience  aud;
    Audience  pay_aud;

    //aud.enroll(efact.makeEvent());
    for (uint32_t i = 0; i < 10; ++i) {
        aud.enroll(&e_proto);
        aud.notify();
        pay_aud.enroll(p_proto.preparePayload(i));
    }
    pay_aud.delayedNotify(10);

    for (uint32_t i = 0; i < 11; ++i) {
        sched.printNextCycleEventTree(std::cout, 0, 0, 1);
        sched.run(1);
    }
    rtn.enterTeardown();


    EXPECT_EQUAL(obs.getActivations(), 20);
    REPORT_ERROR;
}
