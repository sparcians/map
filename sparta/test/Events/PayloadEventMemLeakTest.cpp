

#include "sparta/events/EventSet.hpp"
#include "sparta/events/PayloadEvent.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/simulation/RootTreeNode.hpp"

#include "sparta/utils/SpartaSharedPointerAllocator.hpp"

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
    sparta::SpartaSharedPointerAllocator<uint32_t> int_shared_pointer_allocator(1, 1);

    EventHandler ev_handler;
    sparta::PayloadEvent<PointerWrapper> pld_data_event(&event_set, "event", CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(EventHandler, &ev_handler, handler, PointerWrapper), 0);

    scheduler.finalize();
    rtn.enterConfiguring();
    rtn.enterFinalized();

    constexpr bool exacting_run = true;
    constexpr bool measure_run_time = false;

    pld_data_event.preparePayload(PointerWrapper(sparta::allocate_sparta_shared_pointer<uint32_t>(int_shared_pointer_allocator, 0)))->schedule();
    scheduler.run(3, exacting_run, measure_run_time);
    EXPECT_FALSE(int_shared_pointer_allocator.hasOutstandingObjects());

    rtn.enterTeardown();

    REPORT_ERROR;
    return ERROR_CODE;
}
