// <GlobalEvent_test.cpp> -*- C++ -*-


#include "sparta/sparta.hpp"

#include <iostream>
#include <inttypes.h>

#include "sparta/events/GlobalEvent.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/utils/SpartaTester.hpp"

#define TEST_DEAD_GLOBAL_EVENT

TEST_INIT;

class EventHandler
{
public:
    EventHandler() :
        handle(CREATE_SPARTA_HANDLER(EventHandler, update_))
    {}

    sparta::SpartaHandler handle;

private:
    void update_(void) { std::cout << "Update event!\n"; }
};

bool deadLatchIsUpdated = false;

class DLatch
{
public:
    DLatch(const sparta::Clock * clk,
           bool init_state) :
        state_(init_state),
        ev_update_(clk, CREATE_SPARTA_HANDLER(DLatch, normalUpdate_))
    {}

    void driveLatch(bool dat) { next_state_ = dat; }
    bool readLatch() const { return state_; }

    void update(sparta::Clock::Cycle delay) { ev_update_.schedule(delay); }

    void resetToNormalLatchUpdateHandler()
    {
        if (!is_normal_update_) {
            ev_update_.resetHandler(CREATE_SPARTA_HANDLER(DLatch, normalUpdate_));
            is_normal_update_ = true;
        }
    }
    void resetToDeadLatchUpdateHandler()
    {
        if (is_normal_update_) {
            ev_update_.resetHandler(CREATE_SPARTA_HANDLER(DLatch, deadLatchUpdate_));
            is_normal_update_ = false;
        }
    }

private:
    void normalUpdate_(void) {
        std::cout << "Update Latch (normally)!\n";
        state_ = next_state_;
    }

    void deadLatchUpdate_(void) {
        std::cout << "Update Dead Latch!\n";
        deadLatchIsUpdated = true;
    }

    bool state_ = false;
    bool next_state_ = false;
    bool is_normal_update_ = true;
    sparta::GlobalEvent<> ev_update_;
};

int main()
{
    sparta::Scheduler sched;
    sparta::Clock clk("clk", &sched);
    sparta::RootTreeNode rtn("test_root");
    sparta::EventSet event_set(&rtn);
    event_set.setClock(&clk);

    std::cout << "\nTEST START\n" << std::endl;


    EventHandler ev_handler1;
    sparta::GlobalEvent<sparta::SchedulingPhase::Update> ev_gbl_1{&clk, ev_handler1.handle};

    std::unique_ptr<DLatch> dlatch(new DLatch(&clk, 0));



    sched.finalize();
    rtn.enterConfiguring();
    rtn.enterFinalized();



    ev_gbl_1.schedule(1);
    sched.run(2, true);

    dlatch->driveLatch(1);
    EXPECT_EQUAL(dlatch->readLatch(), 0);


#ifdef TEST_DEAD_GLOBAL_EVENT
    dlatch->resetToDeadLatchUpdateHandler();
#else
    dlatch->resetToNormalLatchUpdateHandler();
#endif

    dlatch->update(2);
    sched.run(1, true);

#ifdef TEST_DEAD_GLOBAL_EVENT
    dlatch.reset(nullptr);
#endif

    sched.run(1, true);
#ifdef TEST_DEAD_GLOBAL_EVENT
    EXPECT_FALSE(deadLatchIsUpdated);
#else
    EXPECT_EQUAL(dlatch->readLatch(), 1);
#endif

    rtn.enterTeardown();

    REPORT_ERROR;
    return ERROR_CODE;
}
