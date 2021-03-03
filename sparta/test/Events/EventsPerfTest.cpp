

#include "sparta/events/EventSet.hpp"
#include "sparta/events/PayloadEvent.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/events/SingleCycleUniqueEvent.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/simulation/RootTreeNode.hpp"

TEST_INIT;

class EventHandler
{
public:

    template<class T>
    void handler(const T &dat) {
        ++got_data_event;
        last_dat = uint32_t(dat);
    }

    void handler() {
    }

    uint32_t got_data_event = 0;
    uint32_t last_dat = 0;
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
    // sparta::UniqueEvent<>
    //     pld_data_event(&event_set, "good_event",
    //                    CREATE_SPARTA_HANDLER_WITH_OBJ(EventHandler,
    //                                                   &ev_handler,
    //                                                   handler), 0);
    // sparta::SingleCycleUniqueEvent<>
    //     pld_data_event(&event_set, "good_event",
    //                    CREATE_SPARTA_HANDLER_WITH_OBJ(EventHandler,
    //                                                   &ev_handler,
    //                                                   handler));
    scheduler.finalize();
    rtn.enterConfiguring();
    rtn.enterFinalized();

    constexpr bool exacting_run = true;
    constexpr bool measure_run_time = false;
    for(uint32_t i = 0; i < 10000000; ++i)
    {
        pld_data_event.preparePayload(i)->schedule();
        pld_data_event.preparePayload(i+1)->schedule();
        pld_data_event.preparePayload(i+2)->schedule();
        pld_data_event.preparePayload(i+3)->schedule();
        pld_data_event.preparePayload(i+4)->schedule();
        pld_data_event.preparePayload(i+5)->schedule();
        pld_data_event.preparePayload(i+6)->schedule();
        pld_data_event.preparePayload(i+7)->schedule();
        pld_data_event.preparePayload(i+8)->schedule();
        pld_data_event.preparePayload(i+9)->schedule();
        scheduler.run(1, exacting_run, measure_run_time);
    }

    rtn.enterTeardown();

    REPORT_ERROR;
    return ERROR_CODE;
}
