

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

    // The payload event that send a pointer
    sparta::PayloadEvent<sparta::SpartaSharedPointer<uint32_t>> pld_ptr_event(&event_set, "pld_ptr_event", CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(EventHandler, &ev_handler, handler, sparta::SpartaSharedPointer<uint32_t>), 0);

    // The payload event that send a data containing a shared pointer
    sparta::PayloadEvent<PointerWrapper> pld_ptr_wrapper_event(&event_set, "pld_ptr_wrapper_event", CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(EventHandler, &ev_handler, handler, PointerWrapper), 0);

    // unique pointer to the event
    std::unique_ptr<sparta::PayloadEvent<sparta::SpartaSharedPointer<uint32_t>>> pld_event_ptr = std::make_unique<sparta::PayloadEvent<sparta::SpartaSharedPointer<uint32_t>>>(&event_set, "pld_event_ptr", CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(EventHandler, &ev_handler, handler, sparta::SpartaSharedPointer<uint32_t>), 0);

    // Monitor the memory usage of the shared pointer
    sparta::SpartaSharedPointerAllocator<uint32_t> int_shared_pointer_allocator(1, 1);

    scheduler.finalize();
    rtn.enterConfiguring();
    rtn.enterFinalized();

    constexpr bool exacting_run = true;
    constexpr bool measure_run_time = false;

    // schedule the event with a shared pointer
    pld_ptr_event.preparePayload(sparta::allocate_sparta_shared_pointer<uint32_t>(int_shared_pointer_allocator, 0))->schedule();
    // Deliver the event
    scheduler.run(3, exacting_run, measure_run_time);
    EXPECT_FALSE(pld_ptr_event.isScheduled()); // The event is already finished
    // The shared pointer is not stored anywhere other than the event payload
    EXPECT_FALSE(int_shared_pointer_allocator.hasOutstandingObjects());  


    // schedule the event with a shared pointer
    pld_ptr_wrapper_event.preparePayload(PointerWrapper(sparta::allocate_sparta_shared_pointer<uint32_t>(int_shared_pointer_allocator, 0)))->schedule();
    // Deliver the event
    scheduler.run(3, exacting_run, measure_run_time);
    EXPECT_FALSE(pld_ptr_wrapper_event.isScheduled()); // The event is already finished
    // The shared pointer is not stored anywhere other than the event payload
    EXPECT_FALSE(int_shared_pointer_allocator.hasOutstandingObjects());  

    pld_event_ptr->preparePayload(sparta::allocate_sparta_shared_pointer<uint32_t>(int_shared_pointer_allocator, 0))->schedule(3);

    rtn.enterTeardown();

    // check the payload of the outstanding event is destroyed
    pld_event_ptr.reset();
    EXPECT_FALSE(int_shared_pointer_allocator.hasOutstandingObjects());  

    

    REPORT_ERROR;
    return ERROR_CODE;
}
