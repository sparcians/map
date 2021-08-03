// <main.cpp> -*- C++ -*-


#include "sparta/sparta.hpp"

#include <iostream>
#include <sstream>
#include <inttypes.h>

#include "sparta/kernel/Scheduler.hpp"
#include "sparta/simulation/RootTreeNode.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/PayloadEvent.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/events/Event.hpp"
#include "sparta/events/Precedence.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/utils/MetaStructs.hpp"
#include "sparta/events/GlobalOrderingPoint.hpp"

TEST_INIT;

class EventHandler
{
public:
    void handler() {
        ++got_dataless_event;
    }
    uint32_t got_dataless_event = 0;

    template<class T>
    void handler(const T &dat) {
        ++got_data_event;
        last_dat = uint32_t(dat);
    }

    void ensureUniqueness() {
        auto scheduler = uevent_->getScheduler();
        EXPECT_TRUE(scheduler->getCurrentTick() != last_seen_);
        last_seen_ = scheduler->getCurrentTick();

        // This should throw, since the event is already executing (it's this method!)
        EXPECT_THROW(uevent_->schedule());
        //uevent_->schedule();
        ++uevent_call_count_;
    }

    void setUniqueEvent(sparta::UniqueEvent<> * uevent) {
        uevent_ = uevent;
    }

    sparta::UniqueEvent<> * uevent_ = nullptr;
    uint32_t uevent_call_count_ = 0;

    uint32_t got_data_event = 0;
    uint32_t last_dat = 0;
    sparta::Scheduler::Tick last_seen_ = 0;
};

class MyPayload
{
public:
    MyPayload (const uint32_t v) : a_val_(v) {}

    bool isItAMatch(const MyPayload & other) const {
        return a_val_ == other.a_val_;
    }

private:
    uint32_t a_val_;
};

//#define COMPILE_TEST 1

void runEventsNegativeTests()
{
    sparta::Scheduler scheduler;
    sparta::Clock clk("clock", &scheduler);
    EXPECT_TRUE(scheduler.getCurrentTick() == 0); //unfinalized sched at tick 0
    EXPECT_TRUE(scheduler.isRunning() == 0);

    sparta::RootTreeNode rtn;
    sparta::EventSet event_set(&rtn);
    event_set.setClock(&clk);
    EventHandler ev_handler;

    // This will throw due to creation of an event with a handler that
    // does not take an argument.
    EXPECT_THROW(sparta::PayloadEvent<uint32_t>
                 pld_data_event(&event_set, "bad_event",
                                CREATE_SPARTA_HANDLER_WITH_OBJ(EventHandler,
                                                             &ev_handler, handler), 1));

    sparta::Event<sparta::SchedulingPhase::PortUpdate>
        port_up_prod0(&event_set, "port_up__prod0",
                      CREATE_SPARTA_HANDLER_WITH_OBJ(EventHandler, &ev_handler, handler), 1);
    sparta::Event<sparta::SchedulingPhase::PortUpdate>
        port_up_prod1(&event_set, "port_up__prod1",
                      CREATE_SPARTA_HANDLER_WITH_OBJ(EventHandler, &ev_handler, handler), 1);
    sparta::Event<sparta::SchedulingPhase::Tick>
        tick_cons0(&event_set, "tick_cons0",
                   CREATE_SPARTA_HANDLER_WITH_OBJ(EventHandler, &ev_handler, handler), 1);
    sparta::Event<sparta::SchedulingPhase::Tick>
        tick_cons1(&event_set, "tick_cons1",
                   CREATE_SPARTA_HANDLER_WITH_OBJ(EventHandler, &ev_handler, handler), 1);

    EXPECT_THROW(sparta::EventGroup(port_up_prod0, port_up_prod1) >>
                 sparta::EventGroup(tick_cons0, tick_cons1));

    scheduler.finalize();
    rtn.enterConfiguring();
    rtn.enterFinalized();
    rtn.enterTeardown();
}

int main()
{
    //Negative tests and other ("positive") unit tests
    //should not share a scheduler...

    //Negative tests:
    runEventsNegativeTests();

    //Positive tests:
    sparta::Scheduler scheduler;
    sparta::Clock clk("clock", &scheduler);
    EXPECT_TRUE(scheduler.getCurrentTick() == 0); //unfinalized sched at tick 0
    EXPECT_TRUE(scheduler.isRunning() == 0);

    sparta::RootTreeNode rtn;
    sparta::EventSet event_set(&rtn);
    rtn.setClock(&clk);
    event_set.setClock(&clk);
    EventHandler ev_handler;

    sparta::GlobalOrderingPoint gop (&rtn, "test_gop");
    sparta::GlobalOrderingPoint gop2(&rtn, "test_gop2");

    sparta::PayloadEvent<uint32_t>
        pld_data_event(&event_set, "good_event",
                       CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(EventHandler,
                                                              &ev_handler,
                                                              handler, uint32_t), 0);

    sparta::Event<> event(&event_set, "simple_event",
                        CREATE_SPARTA_HANDLER_WITH_OBJ(EventHandler, &ev_handler, handler), 1);

    sparta::UniqueEvent<> uevent(&event_set, "unique_event",
                               CREATE_SPARTA_HANDLER_WITH_OBJ(EventHandler, &ev_handler, ensureUniqueness),
                               0);
    sparta::UniqueEvent<> uevent2(&event_set, "unique_event2",
                               CREATE_SPARTA_HANDLER_WITH_OBJ(EventHandler, &ev_handler, ensureUniqueness),
                               0);
    sparta::Event<> event2(&event_set, "simple_event2",
                         CREATE_SPARTA_HANDLER_WITH_OBJ(EventHandler, &ev_handler, handler), 1);
    sparta::Event<> event3(&event_set, "simple_event3",
                         CREATE_SPARTA_HANDLER_WITH_OBJ(EventHandler, &ev_handler, handler), 1);

    sparta::Event<sparta::SchedulingPhase::Update>
        event4(&event_set, "simple_event4",
               CREATE_SPARTA_HANDLER_WITH_OBJ(EventHandler, &ev_handler, handler), 1);

    sparta::Event<sparta::SchedulingPhase::Update>
        event5(&event_set, "simple_event5",
               CREATE_SPARTA_HANDLER_WITH_OBJ(EventHandler, &ev_handler, handler), 1);

    sparta::PayloadEvent<uint32_t, sparta::SchedulingPhase::Update>
        pld_data_event2_ru(&event_set, "good_event2",
                           CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(EventHandler,
                                                                  &ev_handler,
                                                                  handler, uint32_t), 1);
    sparta::PayloadEvent<uint32_t, sparta::SchedulingPhase::Update>
        pld_data_event3_ru(&event_set, "good_event3",
                           CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(EventHandler,
                                                                  &ev_handler,
                                                                  handler, uint32_t), 1);
    // These should compile
    pld_data_event >> gop;
    uevent >> gop;
    event >> gop;

    gop2 >> event2;
    gop2 >> uevent2;
    gop2 >> pld_data_event2_ru;

    // This should compile
    event4 >> pld_data_event2_ru;
    pld_data_event2_ru >> event5;
    event2 >> event3;
    uevent >> event;
    pld_data_event2_ru >> pld_data_event3_ru;

    sparta::Event<sparta::SchedulingPhase::Update>
        prod0(&event_set, "simple_prod0",
               CREATE_SPARTA_HANDLER_WITH_OBJ(EventHandler, &ev_handler, handler), 1);
    sparta::Event<sparta::SchedulingPhase::Update>
        prod1(&event_set, "simple_prod1",
               CREATE_SPARTA_HANDLER_WITH_OBJ(EventHandler, &ev_handler, handler), 1);
    sparta::Event<sparta::SchedulingPhase::Update>
        prod2(&event_set, "simple_prod2",
               CREATE_SPARTA_HANDLER_WITH_OBJ(EventHandler, &ev_handler, handler), 1);

    sparta::Event<sparta::SchedulingPhase::Update>
        cons0(&event_set, "simple_cons0",
               CREATE_SPARTA_HANDLER_WITH_OBJ(EventHandler, &ev_handler, handler), 1);
    sparta::Event<sparta::SchedulingPhase::Update>
        cons1(&event_set, "simple_cons1",
               CREATE_SPARTA_HANDLER_WITH_OBJ(EventHandler, &ev_handler, handler), 1);
    sparta::Event<sparta::SchedulingPhase::Update>
        cons2(&event_set, "simple_cons2",
               CREATE_SPARTA_HANDLER_WITH_OBJ(EventHandler, &ev_handler, handler), 1);

    sparta::PayloadEvent<uint32_t, sparta::SchedulingPhase::Update>
        pld_data_event_group(&event_set, "ple_group_test",
                           CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(EventHandler,
                                                                  &ev_handler,
                                                                  handler, uint32_t), 1);

    sparta::EventGroup(pld_data_event_group, pld_data_event2_ru) >> event;

    prod0 >> sparta::EventGroup(cons0, cons1, cons2);
    sparta::EventGroup(prod1, prod2) >> sparta::EventGroup(cons0, cons1);
    sparta::EventGroup(prod1, prod2) >> cons2;

    sparta::EventGroup(pld_data_event_group) >> cons2;
    prod2 >> sparta::EventGroup(pld_data_event_group);
    sparta::EventGroup(prod0, prod1) >> sparta::EventGroup(pld_data_event_group);

    // Make sure basic ostream>> operations work even with the given
    // operations>> operations for phasing.
    std::string input("1 2 3");
    uint32_t *a,b,c;
    a = &b;
    std::istringstream stream(input);
    stream >> *a >> b >> c;

#ifdef COMPILE_TEST
    // Each of these are compile errors!
    prod0 >> sparta::EventGroup(cons0, event3, cons1); // this won't compile -- event3 is in the wrong phase
    event4 >> event2;
    event2 >> pld_data_event2_ru;
    pld_data_event2_ru >> pld_data_event;
    uevent >> pld_data_event2_ru;
    pld_data_event2_ru >> event2;
    pld_data_event2_ru >> uevent;
#endif

    // Test unique PTR issues
    std::unique_ptr<sparta::Event<sparta::SchedulingPhase::Tick>> event_in_ptr
        (new sparta::Event<sparta::SchedulingPhase::Tick>
         (&event_set, "event_in_ptr",
          CREATE_SPARTA_HANDLER_WITH_OBJ(EventHandler, &ev_handler, handler), 1));

    std::unique_ptr<sparta::UniqueEvent<>> uevent_in_ptr
        (new sparta::UniqueEvent<>
         (&event_set, "uevent_in_ptr",
          CREATE_SPARTA_HANDLER_WITH_OBJ(EventHandler, &ev_handler, ensureUniqueness), 0));

    std::unique_ptr<sparta::PayloadEvent<uint32_t>> pld_data_event_group_in_ptr
        (new sparta::PayloadEvent<uint32_t>
         (&event_set, "pld_data_event_group_in_ptr",
          CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(EventHandler,
                                                 &ev_handler,
                                                 handler, uint32_t), 1));

    static_assert(std::is_same<MetaStruct::remove_any_pointer_t<std::unique_ptr<sparta::Event<sparta::SchedulingPhase::Update>>>,
                  sparta::Event<sparta::SchedulingPhase::Update>>::value == true, "Why didn't this work?");

    static_assert(std::is_base_of<sparta::EventNode,
                  MetaStruct::remove_any_pointer_t<std::unique_ptr<sparta::UniqueEvent<>>>>::value == true, "Why didn't this work?");

    event_in_ptr >> event;
    event_in_ptr >> uevent_in_ptr;
    pld_data_event_group_in_ptr >> uevent_in_ptr;


    // Test vector initialization
    std::list<sparta::UniqueEvent<>> uevents;
    //uevents.reserve(4);
    uevents.emplace_back(&event_set, "unique_event_list_test",
                         CREATE_SPARTA_HANDLER_WITH_OBJ(EventHandler, &ev_handler, ensureUniqueness),
                         0);

    // Test vector initialization
    sparta::UniqueEvent<> uevents2[2] = {{&event_set, "unique_event_list_test_2",
                                        CREATE_SPARTA_HANDLER_WITH_OBJ(EventHandler, &ev_handler, ensureUniqueness),
                                        0},
                                       {&event_set, "unique_event_list_test_3",
                                        CREATE_SPARTA_HANDLER_WITH_OBJ(EventHandler, &ev_handler, ensureUniqueness),
                                        0},
                                       };
    (void) uevents2;

    scheduler.finalize();
    rtn.enterConfiguring();
    rtn.enterFinalized();

    uint32_t payload = 4;

    // Test preparing a payload, then dropping it.  The number of
    // outstanding events should still be zero
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 0);
    pld_data_event.preparePayload(payload);
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 0);

    pld_data_event.preparePayload(payload)->schedule();
    const bool exacting_run = true;
    // sparta::log::Tap scheduler_debug(sparta::TreeNode::getVirtualGlobalNode(),
    //                                sparta::log::categories::DEBUG, std::cout);
    // This should be a compile error, which it currently is
    // pld_data_event.schedule();
    scheduler.run(2, exacting_run);

    EXPECT_EQUAL(ev_handler.got_data_event, 1);

    const uint32_t max_events = 10;
    for(uint32_t i = 0; i < max_events; ++i) {
        event.schedule(i & 0x1);
        scheduler.run(1, exacting_run);
    }
    scheduler.run(1, exacting_run);

    EXPECT_EQUAL(ev_handler.got_dataless_event, max_events);

    for(uint32_t i = 0; i < max_events; ++i) {
        pld_data_event.preparePayload(payload)->schedule(i & 0x1);
    }
    uint32_t event_count = max_events + 1;
    scheduler.run(2, exacting_run);
    EXPECT_EQUAL(ev_handler.got_data_event, event_count);

    ev_handler.setUniqueEvent(&uevent);
    uevent.schedule();
    uevent.schedule();
    uevent.schedule();
    uevent.schedule();
    uevent.schedule(1);
    uevent.schedule(1);
    uevent.schedule(1);
    uevent.schedule(1);
    uevent.schedule(2);
    uevent.schedule(10);

    EXPECT_TRUE(uevent.isScheduled(0));
    EXPECT_TRUE(uevent.isScheduled(1));
    EXPECT_TRUE(uevent.isScheduled(2));
    EXPECT_TRUE(uevent.isScheduled(10));
    EXPECT_FALSE(uevent.isScheduled(100));

    scheduler.run(2, exacting_run);

    // See if the scheduled event @ cycle 10 is still scheduled
    EXPECT_TRUE(uevent.isScheduled(8));

    scheduler.run(100, exacting_run);
    EXPECT_TRUE(ev_handler.uevent_call_count_ == 4);

    ////////////////////////////////////////////////////////////////////////////////
    // Test cancelling events
    // sparta::log::Tap scheduler_debug(sparta::TreeNode::getVirtualGlobalNode(),
    //                                sparta::log::categories::DEBUG, std::cout);

    uevent.schedule(1);
    uevent.schedule(1);
    uevent.schedule(1);
    uevent.schedule(1);
    uevent.schedule(2);
    uevent.schedule(3);
    EXPECT_TRUE(uevent.isScheduled(1));
    EXPECT_TRUE(uevent.isScheduled(2));
    EXPECT_TRUE(uevent.isScheduled(3));
    uevent.cancel(1);
    EXPECT_FALSE(uevent.isScheduled(1));
    EXPECT_TRUE(uevent.isScheduled(2));
    EXPECT_TRUE(uevent.isScheduled(3));
    uevent.cancel(2);
    EXPECT_FALSE(uevent.isScheduled(2));
    uevent.schedule(1);
    uevent.schedule(2);
    uevent.schedule(3);
    uevent.cancel();
    EXPECT_FALSE(uevent.isScheduled(1));
    EXPECT_FALSE(uevent.isScheduled(2));
    EXPECT_FALSE(uevent.isScheduled(3));
    scheduler.run(100, exacting_run);
    EXPECT_TRUE(ev_handler.uevent_call_count_ == 4);

    // Reset the event count for the payload event
    ev_handler.got_data_event = 0;
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 0);

    // Schedule a bunch of events for now and later...
    for(uint32_t i = 0; i < max_events; ++i) {
        pld_data_event.preparePayload(payload)->schedule(i & 0x1);
        event.schedule(i & 0x1);
    }

    // shouldn't change just 'cause of scheduling
    EXPECT_EQUAL(ev_handler.got_data_event, 0);

    scheduler.clearEvents();
    scheduler.run(2, exacting_run);

    // Shouldn't change -- events got blasted.
    EXPECT_EQUAL(ev_handler.got_data_event, 0);

    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 0);

    pld_data_event.preparePayload(payload);

    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 0);

    sparta::ScheduleableHandle handle = pld_data_event.preparePayload(payload);
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 1);
    EXPECT_TRUE(handle != nullptr);
    EXPECT_FALSE(handle == nullptr);
    handle = nullptr;
    EXPECT_TRUE(handle == nullptr);
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 0);

    handle = pld_data_event.preparePayload(payload);
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 1);
    handle->schedule();
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 1);

    scheduler.run(1, exacting_run);
    EXPECT_EQUAL(ev_handler.got_data_event, 1);

    handle = pld_data_event.preparePayload(payload);
    handle->schedule();
    handle = pld_data_event.preparePayload(payload);
    handle->schedule(1);
    handle = pld_data_event.preparePayload(payload);
    handle->schedule(2);

    // Cancel all of them
    pld_data_event.cancel();
    // There will still be one event outstanding from the
    // pld_data_event's POV since the handle still points to it.
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 1);
    handle = nullptr;  // Clear that outstanding event
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 0);
    pld_data_event.cancel();
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 0);

    scheduler.run(3, exacting_run);
    EXPECT_EQUAL(ev_handler.got_data_event, 1);
    // sparta::log::Tap scheduler_debug(sparta::TreeNode::getVirtualGlobalNode(),
    //                                sparta::log::categories::DEBUG, std::cout);

    // Test cancelIf
    handle = pld_data_event.preparePayload(10);
    handle->schedule();
    handle = nullptr;
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 1);
    pld_data_event.cancelIf(uint32_t(2));
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 1);
    scheduler.run(1, exacting_run);
    EXPECT_EQUAL(ev_handler.got_data_event, 2);

    pld_data_event.preparePayload(10)->schedule();
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 1);
    pld_data_event.cancelIf(uint32_t(10));
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 0);
    scheduler.run(1, exacting_run);
    EXPECT_EQUAL(ev_handler.got_data_event, 2);

    // Test cancelIf with a function
    handle = pld_data_event.preparePayload(1234);
    handle->schedule();
    handle = nullptr;
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 1);
    const uint32_t val_to_cancel = 1234;
    pld_data_event.cancelIf([] (const uint32_t & val_to_test) -> bool
                            {
                                if(val_to_test == val_to_cancel) {
                                    return true;
                                }
                                return false;
                            });
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 0);
    scheduler.run(2, exacting_run);
    EXPECT_EQUAL(ev_handler.got_data_event, 2);

    MyPayload mple(val_to_cancel);
    handle = pld_data_event.preparePayload(val_to_cancel);
    handle->schedule(1);
    handle = nullptr;
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 1);
    auto matchCompareFunc = std::bind(&MyPayload::isItAMatch, mple, std::placeholders::_1);
    pld_data_event.cancelIf(matchCompareFunc);
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 0);
    scheduler.run(2, exacting_run);
    EXPECT_EQUAL(ev_handler.got_data_event, 2);

    pld_data_event.preparePayload(10)->schedule(1);
    pld_data_event.preparePayload(20)->schedule(2);
    pld_data_event.preparePayload(30)->schedule(3);
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 3);
    pld_data_event.cancel(sparta::Clock::Cycle(2));
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 2);
    scheduler.run(4, exacting_run);
    EXPECT_EQUAL(ev_handler.got_data_event, 4);
    EXPECT_EQUAL(ev_handler.last_dat, 30);

    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 0);
    auto pde_sched = pld_data_event.preparePayload(66);
    pld_data_event.cancelIf(66);
    EXPECT_THROW(pde_sched->schedule(1));
    pde_sched = pld_data_event.preparePayload(66);
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 1);
    pde_sched->cancel();
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 1);
    pde_sched = nullptr;
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 0);

    // Performance test
    // for(uint32_t i = 0; i < 10000000; ++i) {
    //     pld_data_event.preparePayload(10)->schedule(1);
    //     scheduler.run(1, exacting_run);
    // }

    // Test confirmIf
    handle = pld_data_event.preparePayload(10);
    handle->schedule();
    handle = nullptr;
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 1);
    EXPECT_EQUAL(pld_data_event.confirmIf(uint32_t(10)), true);
    EXPECT_EQUAL(pld_data_event.confirmIf(uint32_t(7)), false);
    scheduler.run(1, exacting_run);
    EXPECT_EQUAL(ev_handler.last_dat, 10);

    // Test confirmIf with a function
    handle = pld_data_event.preparePayload(1234);
    handle->schedule(1);
    handle = nullptr;
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 1);
    uint32_t val_to_confirm = 1234;
    auto confirm = [&val_to_confirm](const uint32_t & val_to_test)->bool {
                            return (val_to_test == val_to_confirm);
                       };
    EXPECT_EQUAL(pld_data_event.confirmIf(confirm), true);
    val_to_confirm = 1;
    EXPECT_EQUAL(pld_data_event.confirmIf(confirm), false);
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 1);
    scheduler.run(2, exacting_run);
    EXPECT_EQUAL(ev_handler.last_dat, 1234);

    // Test getHandleIf
    handle = pld_data_event.preparePayload(10);
    handle->schedule();
    handle = nullptr;
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 1);

    std::vector<sparta::Scheduleable*> eh_vector = pld_data_event.getHandleIf(10);
    EXPECT_EQUAL(eh_vector.size(), 1);
    eh_vector = pld_data_event.getHandleIf(1234);
    EXPECT_EQUAL(eh_vector.empty(), true);
    scheduler.run(1, exacting_run);
    EXPECT_EQUAL(ev_handler.last_dat, 10);

    // Test getHandleIf with a function
    handle = pld_data_event.preparePayload(1234);
    handle->schedule();
    handle = nullptr;
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 1);
    val_to_confirm = 1234;
    eh_vector = pld_data_event.getHandleIf(confirm);
    EXPECT_EQUAL(eh_vector.size(), 1);
    val_to_confirm = 1;
    eh_vector = pld_data_event.getHandleIf(confirm);
    EXPECT_EQUAL(eh_vector.empty(), true);
    EXPECT_EQUAL(pld_data_event.getNumOutstandingEvents(), 1);
    scheduler.run(2, exacting_run);
    EXPECT_EQUAL(ev_handler.last_dat, 1234);


    rtn.enterTeardown();

    REPORT_ERROR;
    return ERROR_CODE;
}
