

#include "sparta/events/EventSet.hpp"
#include "sparta/events/PayloadEvent.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/utils/SpartaSharedPointerAllocator.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/simulation/RootTreeNode.hpp"

TEST_INIT

class EventHandler
{
public:

    template<class T>
    void handler(const T &dat) {
    }

    void handler() {
    }
};

// The struct that contains a shared pointer
struct PointerWrapper 
{
public:
    PointerWrapper() : ptr(nullptr) {}
    PointerWrapper(sparta::SpartaSharedPointer<uint32_t> ptr) : ptr(ptr) {}
    
    sparta::SpartaSharedPointer<uint32_t> ptr;
};

int main()
{
    sparta::Scheduler    scheduler;
    sparta::Clock        clk("clock", &scheduler);
    sparta::RootTreeNode rtn;
    sparta::EventSet     event_set(&rtn);
    event_set.setClock(&clk);
    EventHandler ev_handler;

    // The payload event that send a data containing a shared pointer
    sparta::PayloadEvent<PointerWrapper> pld_data_event(&event_set, "event", CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(EventHandler, &ev_handler, handler, PointerWrapper), 0);

    // Monitor the memory usage of the shared pointer
    sparta::SpartaSharedPointerAllocator<uint32_t> int_shared_pointer_allocator(1, 1);

    scheduler.finalize();
    rtn.enterConfiguring();
    rtn.enterFinalized();

    constexpr bool exacting_run = true;
    constexpr bool measure_run_time = false;

    // schedule the event with a shared pointer
    pld_data_event.preparePayload(PointerWrapper(sparta::allocate_sparta_shared_pointer<uint32_t>(int_shared_pointer_allocator, 0)))->schedule();

    // Deliver the event
    scheduler.run(3, exacting_run, measure_run_time);

    EXPECT_FALSE(pld_data_event.isScheduled()); // The event is already finished

    // The shared pointer is not stored anywhere other than the event payload
    EXPECT_FALSE(int_shared_pointer_allocator.hasOutstandingObjects());  

    rtn.enterTeardown();

    REPORT_ERROR;
    return ERROR_CODE;
}
