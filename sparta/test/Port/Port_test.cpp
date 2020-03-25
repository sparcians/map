

#include <iostream>
#include <cstring>
#include <memory>

#include "Producer.hpp"
#include "Consumer.hpp"

#include "sparta/simulation/Clock.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/sparta.hpp"
#include "sparta/ports/PortSet.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/ports/SignalPort.hpp"

#include "sparta/sparta.hpp"

TEST_INIT;

/*
 * This test creates a producer and a consumer for Ports (not
 * including SyncPort -- different test).
 */

void testPortCancels_();
void tryDAGIssue_(bool);

int main ()
{
    // Test DAG issues due to incorrect port phasing
    tryDAGIssue_(true);
    tryDAGIssue_(false);

    // Test port cancels
    testPortCancels_();

    // Test communication between blocks using ports
    sparta::Scheduler sched;
    sparta::Clock clk("clock", &sched);
    sparta::RootTreeNode rtn;
    rtn.setClock(&clk);

    sparta::PortSet ps(&rtn, "bogus_port_set");
    sparta::DataInPort<double> delay0_in(&ps, "delay0_in", sparta::SchedulingPhase::Tick, 0);
    sparta::DataInPort<double> delay1_in(&ps, "delay1_in");
    sparta::DataInPort<double> delay10_in(&ps, "delay10_in");

    sparta::DataOutPort<double> delay0_out(&ps, "delay0_out");
    sparta::DataOutPort<double> delay1_out(&ps, "delay1_out");
    sparta::DataOutPort<double> delay10_out(&ps, "delay10_out");

    sparta::SignalInPort signal_in(&ps, "signal_in");
    sparta::SignalOutPort signal_out(&ps, "signal_out");

    sparta::SignalInPort      signal_prec_check(&ps, "signal_prec_check");
    sparta::DataInPort<double> precedence_check(&ps, "prec_check");
    signal_in.precedes(signal_prec_check);
    delay10_in.precedes(precedence_check);


    sparta::SignalOutPort unbound_signal_out(&ps, "unbound_signal_out");
    sparta::DataOutPort<double> unbound_data_out(&ps, "unbound_data_out");

    // Set up a port group where there is constant data flowing, but
    // shouldn't keep the scheduler alive
    sparta::DataOutPort<double> delay1_out_non_continuing(&ps, "delay1_out_non_continuing");
    delay1_out_non_continuing.setContinuing(false);
    sparta::DataInPort<double>  delay1_in_non_continuing(&ps, "delay1_in_non_continuing");

    sparta::TreeNode prod_tn(&rtn, "producer", "producer");
    std::unique_ptr<Producer> p(new Producer(&prod_tn,
                                             &delay0_out,
                                             &delay1_out,
                                             &delay10_out,
                                             &delay1_out_non_continuing,
                                             &signal_out,
                                             &clk));

    sparta::TreeNode cons_tn(&rtn, "consumer", "consumer");
    std::unique_ptr<Consumer> c(new Consumer(&cons_tn,
                                             &delay0_in,
                                             &delay1_in,
                                             &delay10_in,
                                             &delay1_in_non_continuing,
                                             &signal_in,
                                             &clk));

    EXPECT_THROW(unbound_signal_out.send());
    EXPECT_THROW(unbound_data_out.send(1.0));

    delay0_out.bind(delay0_in);
    delay1_out.bind(delay1_in);
    delay10_out.bind(delay10_in);
    signal_out.bind(signal_in);
    delay1_out_non_continuing.bind(delay1_in_non_continuing);

    // Try some binding rules
    sparta::SignalInPort  signal_bind_in(&ps, "signal_bind_in");
    sparta::SignalOutPort signal_bind_out(&ps, "signal_bind_out");
    signal_bind_out.bind(signal_bind_in);
    EXPECT_THROW(signal_bind_in.bind(signal_bind_out));
    sparta::DataInPort<double>  delay0_bind_in(&ps, "delay0_bind_in");
    sparta::DataOutPort<double> delay1_bind_out(&ps, "delay1_bind_out");
    delay0_bind_in.bind(delay1_bind_out);
    EXPECT_THROW(delay1_bind_out.bind(delay0_bind_in));

    rtn.enterConfiguring();
    rtn.enterFinalized();
    sched.finalize();


    // sparta::log::Tap scheduler_debug(sparta::TreeNode::getVirtualGlobalNode(),
    //                                sparta::log::categories::DEBUG, std::cout);

    //The scheduler must have been turned on before scheduling events.
    p->scheduleTests();
    sched.run();

    EXPECT_EQUAL(c->getNumTimes(), 10);

    // Make sure all of the ports are no longer driven
    EXPECT_FALSE(delay1_in.isDriven());
    EXPECT_FALSE(delay1_out.isDriven());
    EXPECT_FALSE(delay10_in.isDriven());
    EXPECT_FALSE(delay10_out.isDriven());

    rtn.enterTeardown();

    // Returns error if one
    REPORT_ERROR;
    return ERROR_CODE;
}

void tryDAGIssue_(bool failit)
{
    //
    // Test presume_zero_delay.  The following code will cause a DAG cycle
    //
    /*
             ------.      .-------
                   |      |
              out1 > ---- > in1 (0-cycle)
               ^   |      |  |
               |   |      |  v
             prod  |      | cons
               ^   |      |  |  send(one_cycle)
               |   |      |  v
    (0-cycle) in2  < ---- < out2
                   |      |
               ----.      .----------
    */
    sparta::Scheduler sched;
    sparta::Clock    clk("dummy", &sched);
    sparta::PortSet  ps(nullptr);
    sparta::EventSet es(nullptr);
    ps.setClock(&clk);
    es.setClock(&clk);
    sparta::Event<>  zero_delay_cons(&es, "zero_delay_cons", sparta::SpartaHandler("dummy"));
    sparta::Event<>  zero_delay_prod(&es, "zero_delay_prod", sparta::SpartaHandler("dummy"));

    // If failit == false, we create the OutPorts to not presume a
    // zero delay, which makes the DAG happy.
    bool presume_zero_delay = failit;

    sparta::DataInPort<bool>  zero_delay_in1 (&ps, "zero_delay_in1");
    sparta::DataOutPort<bool> zero_delay_out1(&ps, "zero_delay_out1", presume_zero_delay);
    sparta::DataInPort<bool>  zero_delay_in2 (&ps, "zero_delay_in2", sparta::SchedulingPhase::Tick, 0);
    sparta::DataOutPort<bool> zero_delay_out2(&ps, "zero_delay_out2", presume_zero_delay);

    // Test a situation (issue #15) where the user presumed a
    // zero-delay InPort and attempted to set precedence on a non-zero
    // delay Inport.  This will assert as the zero delay Port is on
    // the Tick phase and the non-zero delay inport is on the Update
    // phase (by default in BOTH cases).
    std::unique_ptr<sparta::DataInPort<bool>> one_delay_in;
    if(presume_zero_delay) {
        one_delay_in.reset(new sparta::DataInPort<bool>(&ps, "one_delay_in", 1));
        EXPECT_THROW(zero_delay_in2.precedes(*(one_delay_in.get())));
    }

    zero_delay_in1.registerConsumerEvent(zero_delay_cons);
    zero_delay_out1.registerProducingEvent(zero_delay_prod);
    zero_delay_in2.registerConsumerEvent(zero_delay_prod);
    zero_delay_out2.registerProducingEvent(zero_delay_cons);
    zero_delay_out1.registerProducingPort(zero_delay_in2);
    sparta::bind(zero_delay_out1, zero_delay_in1);
    sparta::bind(zero_delay_out2, zero_delay_in2);

    // Cannot register a producing event/port after binding
    EXPECT_THROW(zero_delay_out1.registerProducingEvent(zero_delay_prod));
    EXPECT_THROW(zero_delay_in2.registerConsumerEvent(zero_delay_prod));
    EXPECT_THROW(zero_delay_out1.registerProducingPort(zero_delay_in2));

    if(failit) {
        // This will throw a DAG exception
        EXPECT_THROW(sched.finalize());
    }
    else {

        // Create a class with methods called from inside the
        // Scheduler
        struct forceDagIssue
        {
            sparta::DataOutPort<bool> & zero_delay_out1;

            // This method is called from the force_dag_issue event
            // below and will be called during the Tick phase, which
            // is bad since the zero_delay_in1 port has a registered
            // handler on the PortUpdate phase.  See the assert in the
            // send_ methods of DataInPort or SignalInPort
            void forceIt() {
                // This should cause the Port to barf
                EXPECT_THROW(zero_delay_out1.send(true, 0));
            }

            void getIt(const bool &) {
                sparta_assert(!"I should have never been called");
            }
        };
        forceDagIssue fdi{zero_delay_out1};
        sparta::Event<> force_dag_issue(&es, "force_dag_issue",
                                      CREATE_SPARTA_HANDLER_WITH_OBJ(forceDagIssue, &fdi, forceIt));
        zero_delay_in1.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(forceDagIssue, &fdi, getIt, bool));

        // Make sure we can register a handler in a later phase...
        EXPECT_NOTHROW(zero_delay_in2.registerConsumerHandler
                       (CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(forceDagIssue, &fdi, getIt, bool)));

        EXPECT_NOTHROW(sched.finalize());
        force_dag_issue.schedule(1);
        sched.run(2);
    }

    // Reset for next tests
    sched.reset();
}

class Receiver
{
public:
    Receiver(sparta::PortSet * ps) : ps_(ps) {
        receiver_pt.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Receiver, getSomeData, uint32_t));
    }

    void getSomeData(const uint32_t & dat) {
        received_dat = dat;
    }

    sparta::PortSet * ps_ = nullptr;
    sparta::DataInPort<uint32_t> receiver_pt {ps_, "receiver", 0};
    sparta::utils::ValidValue<uint32_t> received_dat;
};

class Sender
{
public:
    Sender(sparta::PortSet * ps) : ps_(ps) {}

    void sendSomeData(const uint32_t & dat, sparta::Clock::Cycle delay) {
        sender_pt.send(dat, delay);
    }

    sparta::PortSet * ps_ = nullptr;
    sparta::DataOutPort<uint32_t> sender_pt{ps_, "sender"};
};

void testPortCancels_()
{
    sparta::Scheduler sched;
    sparta::Clock    clk("dummy", &sched);
    sparta::PortSet  ps(nullptr);
    ps.setClock(&clk);
    Receiver receiver(&ps);
    Sender   sender(&ps);

    sparta::bind(receiver.receiver_pt, sender.sender_pt);
    EXPECT_NOTHROW(sched.finalize());

    // Send some data, make sure it's received
    sender.sendSomeData(1, 0);
    EXPECT_FALSE(receiver.received_dat.isValid());
    sched.run(1);
    EXPECT_TRUE(receiver.received_dat.isValid());
    receiver.received_dat.clearValid();

    // Test InPort cancel, zero cycle
    sender.sendSomeData(1, 0);
    sender.sender_pt.cancel();
    EXPECT_FALSE(receiver.received_dat.isValid());
    sched.run(1);
    EXPECT_FALSE(receiver.received_dat.isValid());

    // Test InPort cancel, same data, zero to many cycles
    sender.sendSomeData(1, 0);
    sender.sendSomeData(1, 1);
    sender.sendSomeData(1, 2);
    sender.sender_pt.cancel();
    EXPECT_FALSE(receiver.received_dat.isValid());
    sched.run(4);
    EXPECT_FALSE(receiver.received_dat.isValid());

    // Test InPort cancel, different data, zero to many cycles
    sender.sendSomeData(1, 0);
    sender.sendSomeData(2, 1);
    sender.sendSomeData(3, 2);
    sender.sender_pt.cancel();
    EXPECT_FALSE(receiver.received_dat.isValid());
    sched.run(4);
    EXPECT_FALSE(receiver.received_dat.isValid());

    // Test InPort cancel, different data, zero to many cycles,
    // canceling on data with the value 2.
    const uint32_t data = 0;
    const sparta::Clock::Cycle delay = 0;
    sender.sendSomeData(data + 1, delay + 0);
    sender.sendSomeData(data + 2, delay + 1); // to be canceled
    sender.sendSomeData(data + 3, delay + 2);
    sender.sender_pt.cancelIf(uint32_t(2));
    EXPECT_FALSE(receiver.received_dat.isValid());
    sched.run(1);
    EXPECT_TRUE(receiver.received_dat.isValid());
    EXPECT_EQUAL(receiver.received_dat, uint32_t(1));
    sched.run(2);
    EXPECT_TRUE(receiver.received_dat.isValid());
    EXPECT_EQUAL(receiver.received_dat, uint32_t(3));
    receiver.received_dat.clearValid();

    // Test the dynamic function call
    sender.sendSomeData(data + 1, delay + 0);
    sender.sendSomeData(data + 2, delay + 1); // to be canceled
    sender.sendSomeData(data + 3, delay + 2);

    uint32_t cancel_criteria = 2;
    sender.sender_pt.cancelIf([cancel_criteria](const uint32_t val) -> bool
                              {
                                  return cancel_criteria == val;
                              });

    EXPECT_FALSE(receiver.received_dat.isValid());
    sched.run(1);
    EXPECT_TRUE(receiver.received_dat.isValid());
    EXPECT_EQUAL(receiver.received_dat, uint32_t(1));
    sched.run(2);
    EXPECT_TRUE(receiver.received_dat.isValid());
    EXPECT_EQUAL(receiver.received_dat, uint32_t(3));
    receiver.received_dat.clearValid();
    sched.run(1);
    EXPECT_FALSE(receiver.received_dat.isValid());

    // Test the dynamic function call on the OutPort
    sender.sendSomeData(data + 1, delay + 0);
    sender.sendSomeData(data + 2, delay + 1); // to be canceled
    sender.sendSomeData(data + 3, delay + 2);

    receiver.receiver_pt.cancelIf([cancel_criteria](const uint32_t val) -> bool
                                  {
                                      return cancel_criteria == val;
                                  });

    EXPECT_FALSE(receiver.received_dat.isValid());
    sched.run(1);
    EXPECT_TRUE(receiver.received_dat.isValid());
    EXPECT_EQUAL(receiver.received_dat, uint32_t(1));
    sched.run(2);
    EXPECT_TRUE(receiver.received_dat.isValid());
    EXPECT_EQUAL(receiver.received_dat, uint32_t(3));
    receiver.received_dat.clearValid();
    sched.run(1);
    EXPECT_FALSE(receiver.received_dat.isValid());


    // Reset for next tests
    sched.reset();
}
