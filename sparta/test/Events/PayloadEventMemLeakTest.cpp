

#include "sparta/events/EventSet.hpp"
#include "sparta/events/PayloadEvent.hpp"
#include "sparta/utils/SpartaTester.hpp"
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

int main()
{
    sparta::Scheduler    scheduler;
    sparta::Clock        clk("clock", &scheduler);
    sparta::RootTreeNode rtn;
    sparta::EventSet     event_set(&rtn);
    event_set.setClock(&clk);

    EventHandler ev_handler;
    sparta::PayloadEvent<uint32_t>
        pld_data_event(&event_set, "good_event",
                       CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(EventHandler,
                                                                &ev_handler,
                                                                handler, uint32_t), 0);

    scheduler.finalize();
    rtn.enterConfiguring();
    rtn.enterFinalized();

    constexpr bool exacting_run = true;
    constexpr bool measure_run_time = false;

    pld_data_event.preparePayload(0)->schedule();
    scheduler.run(1, exacting_run, measure_run_time);
    pld_data_event.preparePayload(1)->schedule();
    scheduler.run(1, exacting_run, measure_run_time);

    rtn.enterTeardown();

    REPORT_ERROR;
    return ERROR_CODE;
}
