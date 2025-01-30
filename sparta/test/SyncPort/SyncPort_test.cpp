

#include "sparta/ports/SyncPort.hpp"
#include "sparta/ports/PortSet.hpp"

#include "sparta/utils/SpartaTester.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/kernel/DAG.hpp"
#include "sparta/simulation/RootTreeNode.hpp"
#include "sparta/simulation/Resource.hpp"
#include "sparta/simulation/ResourceTreeNode.hpp"
#include "sparta/simulation/ResourceFactory.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/events/Event.hpp"
#include "sparta/log/Tap.hpp"
#include "sparta/collection/PipelineCollector.hpp"

#include <memory>
#include <cinttypes>
#include <iostream>

TEST_INIT

// Be verbose -- very verbose
// #define MAKE_NOISE

namespace
{

// Data sent across the links
typedef uint32_t DataType;

// Tick and Data tuple
typedef std::pair<uint64_t, DataType> TickAndData;

#define now() getClock()->getScheduler()->getCurrentTick()

sparta::Scheduler::Tick syncToRisingEdge(sparta::Scheduler::Tick tick, sparta::Clock::Cycle period) {
    sparta::Scheduler::Tick edge = tick;
    if ((edge % period) != 0) {
        edge += period;
        edge = (edge / period) * period;
    }
    return edge;
}

//////////////////////////////////////////////////////////////////////
//
// This class does most of the checking for SyncPort
// It has both an input and output connection

class Unit : public sparta::Resource
{
public:
    class ParameterSet : public sparta::ParameterSet
    {
    public:
        ParameterSet(sparta::TreeNode* tn) : sparta::ParameterSet(tn) {}
    };

    Unit(sparta::TreeNode* node, const Unit::ParameterSet*);
    ~Unit();

    // Called before simulation by the testing framework to
    //    - Send commands on the SyncOut port to the other Unit
    //    - Calculate the expected input Ticks and Data from the other Unit
    void schedule_commands(double other_clk_mhz);

    // Callback method for input data.  In this method we check:
    //    - The data arrives at the expected tick
    //    - The data is the expected value
    void cmd_callback(const DataType & dat);

    // Callback for data.  This method is to test that the command is
    // received before the data.  The data is ignored.
    void data_callback(const char &);

    // Self-scheduled method.  In this method we check:
    //    - The SyncPort's events can be ordered with other events
    void doWork();

    sparta::PortSet ps;

    // These are the classes we're actually testing
    sparta::SyncOutPort<DataType> out_cmd;
    sparta::SyncOutPort<char>     out_data;
    sparta::SyncInPort<DataType>  in_cmd;
    sparta::SyncInPort<char>      in_data; // just to test precedence


    // Global count of the total times a destructor was called; use to
    // sanity check that our end-of-simulation checks are actually done.
    static uint32_t num_destructors_called;

    static constexpr const char* name = "Unit";

private:
    // Internal data-structure to track when data should arrive, and what
    // it should be.  The handling is such that:
    //    - cmd_callback() uses the front item to ensure data arrived correctly
    //    - doWork() pops the front item - this ensures we get the correct
    //          # of calls to doWork()
    //    - ~Unit() checks that the list is empty - this ensures we got all
    //          the data expected.
    std::list<TickAndData> expected_data;

    // Event set for the unit
    sparta::EventSet ev_set;

    // Self-scheduling event
    sparta::UniqueEvent<> ev_do_work;

    // Last tick that doWork() was called
    uint64_t dowork_run_tick = 0;

    // Last tick that data was received
    uint64_t last_data_received_tick = 0;

    // Total commands that should be scheduled in both directions across
    // the interfaces
    static const uint64_t NUM_COMMANDS_TO_SEND = 1000;

    // Data MUST come in after the command
    bool got_data_ = false;
    bool got_cmd_  = false;

    void clearFlags();
    sparta::Event<> clear_flags_{&ev_set, "clear_flags", CREATE_SPARTA_HANDLER(Unit, clearFlags)};
};

uint32_t Unit::num_destructors_called = 0;

//////////////////////////////////////////////////////////////////////
//
// This class sets up a single system for testing

class TestSystem
{
public:

    // Creates a new system with two Units, arbitrarily named 'master' and
    // 'slave'.  Parameters passed are the master/slave frequencies
    TestSystem(double master_frequency_mhz, double slave_frequency_mhz);

    ~TestSystem();

    sparta::Scheduler * getScheduler() {
        return &sched;
    }

private:
    sparta::RootTreeNode rtn;

    sparta::Scheduler sched;
    sparta::ClockManager cm{&sched};
    sparta::Clock::Handle root_clk;
    sparta::Clock::Handle master_clk;
    sparta::Clock::Handle slave_clk;

    sparta::ResourceFactory<Unit, Unit::ParameterSet> rfact;

    std::unique_ptr<sparta::ResourceTreeNode> master_tn;
    std::unique_ptr<sparta::ResourceTreeNode> slave_tn;
    std::unique_ptr<sparta::collection::PipelineCollector> pc;
};

//////////////////////////////////////////////////////////////////////

Unit::Unit(sparta::TreeNode* node, const Unit::ParameterSet*) :
    sparta::Resource(node),
    ps(node),
    out_cmd(&ps, "out_cmd", node->getClock()),
    out_data(&ps, "out_data", node->getClock()),
    in_cmd(&ps, "in_cmd", node->getClock()),
    in_data(&ps, "in_data", node->getClock()),
    ev_set(node),
    ev_do_work(&ev_set, "unit_do_work_event", CREATE_SPARTA_HANDLER(Unit, doWork))
{
    out_cmd.enableCollection(node);
    in_cmd.enableCollection(node);

    in_cmd.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Unit, cmd_callback, DataType));
    in_data.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Unit, data_callback, char));
}

//////////////////////////////////////////////////////////////////////

Unit::~Unit()
{
    std::cout << "Destructing '" << getName() << "'\n";
    if (!EXPECT_TRUE(expected_data.empty())) {
        std::cout << "ERROR: " << getName() << ": still expecting " << expected_data.size() << " more data beat(s)\n";
    }
    num_destructors_called++;
}

//////////////////////////////////////////////////////////////////////

void Unit::clearFlags()
{
    got_data_ = false;
    got_cmd_ = false;
}

void Unit::data_callback(const char &)
{
    got_data_ = true;
    EXPECT_TRUE(got_cmd_);
}

void Unit::cmd_callback(const DataType & dat)
{
#ifdef MAKE_NOISE
    std::cout << getName() << ": Got data '" << dat << "' at "
              << now() << ", cycle " << getClock()->currentCycle() << "\n";
#endif

    got_cmd_ = true;
    EXPECT_FALSE(got_data_);
    clear_flags_.schedule();

    if (!EXPECT_TRUE(dowork_run_tick < now())) {
        std::cout << "ERROR: " << getName() << ": tick should not have run this time quantum; now==" << now() << "\n";
    }
    if (!EXPECT_TRUE(getClock()->isPosedge())) {
        std::cout << "ERROR: " << getName() << ": data arrived at non-posedge tick: " << now() << "\n";
    }
    if (!EXPECT_TRUE(now() > last_data_received_tick)) {
        std::cout << "ERROR: " << getName() << ": received multiple data beats at tick: " << now() << "\n";
    }

    if (!EXPECT_FALSE(expected_data.empty())) {
        std::cout << "ERROR: " << getName() << ": Data arrived when none was expected\n";
    }
    else {
        if (!EXPECT_EQUAL(expected_data.front().first,now())) {
            std::cout << "ERROR: " << getName() << ": expected data at " << expected_data.front().first
                      << ", but got data at " << now() << "\n";
        }
        if (!EXPECT_EQUAL(expected_data.front().second, dat)) {
            std::cout << "ERROR: " << getName() << ": expected data " << expected_data.front().second
                      << ", but got data " << dat << "\n";
        }
    }

    // This should be done by SyncPort now
    ev_do_work.schedule(sparta::Clock::Cycle(0));
    last_data_received_tick = now();
}

//////////////////////////////////////////////////////////////////////

void Unit::doWork()
{
#ifdef MAKE_NOISE
    std::cout << getName() << ": Inside doWork at " << now() << ", cycle " << getClock()->currentCycle() << "\n";
#endif

    if (!EXPECT_TRUE(getClock()->isPosedge())) {
        std::cout << "ERROR: " << getName() << ": doWork scheduled at non-posedge time: " << now() << "\n";
    }
    if (!EXPECT_EQUAL(last_data_received_tick, now())) {
        std::cout << "ERROR: " << getName() << ": doWork() wasn't run the same tick as data arrived: now==" << now() << "\n";
    }
    if (!EXPECT_FALSE(expected_data.empty())) {
        std::cout << "ERROR: " << getName() << ": doWork() scheduled without any data to consume\n";
    }
    else {
        expected_data.pop_front();
    }

    dowork_run_tick = now();
}

//////////////////////////////////////////////////////////////////////

void Unit::schedule_commands(double other_clk_mhz)
{
    const uint64_t src_clk_period = getClock()->getPeriod();
    const uint64_t dst_clk_period = sparta::ClockManager::getClockPeriodFromFrequencyMhz(other_clk_mhz);
    const uint64_t time_at_start  = now();

    // Delay factors:
    //  Slow to fast - safe to send every slow cycle (source)
    //  Fast to slow - space every slow cycle (dest)
    uint64_t src_delay_factor = 1; // Assume slow to fast and check
    uint64_t dst_delay_factor = 1;
    if (dst_clk_period > src_clk_period) {
        src_delay_factor = ((dst_clk_period + (src_clk_period-1)) / src_clk_period);
    } else if (dst_clk_period < src_clk_period) {
        dst_delay_factor = ((src_clk_period + (dst_clk_period-1)) / dst_clk_period);
    }

#ifdef MAKE_NOISE
    std::cout << getName() << ": src_delay_factor=" << src_delay_factor
              << " dst_delay_factor=" << dst_delay_factor << " (dst_clk_period="
              << dst_clk_period << ", src_clk_period=" << src_clk_period <<")\n";
#endif

    // Schedule the outgoing commands
    for (uint32_t idx = 0; idx < NUM_COMMANDS_TO_SEND; idx++) {

        // Arbitrary offset added to make data not appear as ticks
        DataType data = idx + 10000000;
        uint64_t src_delay = idx * src_delay_factor;
        out_data.send('x', src_delay);  // test to see if the data arrives before the command -- it should not
        out_cmd.send(data, src_delay);

#ifdef MAKE_NOISE
        // Calculate when this data will arrive at the destination
        //  This is the send delay plus the destination period synced to
        // the rising edge of the destination clock.
        //  Syncing this again to the rising edge of the source clock gives
        // the next cycle at which we can send.
        uint64_t src_data_arrival_tick = src_delay * src_clk_period + dst_clk_period;
        src_data_arrival_tick = syncToRisingEdge(src_data_arrival_tick, dst_clk_period);
        src_data_arrival_tick = syncToRisingEdge(src_data_arrival_tick, src_clk_period);
        std::cout << getName() << ": sending data '" << data << "' at tick '"
                  << src_delay * src_clk_period << "' expecting arrival at '" << src_data_arrival_tick << "'\n";
#endif
        if (!EXPECT_FALSE(out_cmd.isReady())) {
            std::cout << "ERROR: " << getName() << ": should never be ready this cycle (idx=" << idx << ")\n";
        }

        // Compute the return arrival data, which is based on the destination
        // sending this same index data back to us
        uint64_t dst_delay = idx * dst_delay_factor;
        uint64_t next_data_arrival_tick = dst_delay * dst_clk_period + src_clk_period + time_at_start;
        if ((next_data_arrival_tick % src_clk_period) != 0) { // Sync to rising edge
            next_data_arrival_tick += src_clk_period;
            next_data_arrival_tick = (next_data_arrival_tick / src_clk_period) * src_clk_period;
        }

        expected_data.push_back(TickAndData(next_data_arrival_tick, data));
#ifdef MAKE_NOISE
        std::cout << getName() << ": expecting data '" << data << "' at tick '" << next_data_arrival_tick << "'\n";
#endif

    }
}

//////////////////////////////////////////////////////////////////////


TestSystem::TestSystem(double master_frequency_mhz, double slave_frequency_mhz)
{
    bool scheduler_debug = false;

    root_clk   = cm.makeRoot(&rtn, "root_clk");
    master_clk = cm.makeClock("master_clk", root_clk, master_frequency_mhz);
    slave_clk  = cm.makeClock("slave_clk", root_clk, slave_frequency_mhz);

    master_tn.reset(new sparta::ResourceTreeNode(&rtn, "master", "master", &rfact));
    master_tn->setClock(master_clk.get());

    slave_tn.reset(new sparta::ResourceTreeNode(&rtn, "slave", "slave", &rfact));
    slave_tn->setClock(slave_clk.get());

    rtn.enterConfiguring();
    cm.normalize();
    std::cout << "master:" << std::string(*master_clk) << "\n";
    std::cout << "slave:" << std::string(*slave_clk) << "\n";

    rtn.enterFinalized();

    if (scheduler_debug) {
        sched.getDAG()->print(std::cout);
    }

    Unit * master_unit = master_tn->getResourceAs<Unit*>();
    sparta_assert (master_unit !=0);

    Unit * slave_unit = slave_tn->getResourceAs<Unit*>();
    sparta_assert (slave_unit !=0);

    master_unit->in_cmd.setPortDelay(static_cast<sparta::Clock::Cycle>(1));
    slave_unit->in_cmd.setPortDelay(static_cast<sparta::Clock::Cycle>(1));
    master_unit->in_data.setPortDelay(static_cast<sparta::Clock::Cycle>(1));
    slave_unit->in_data.setPortDelay(static_cast<sparta::Clock::Cycle>(1));

    master_unit->in_cmd.bind(&slave_unit->out_cmd);
    master_unit->out_data.bind(&slave_unit->in_data);
    //slave_unit->out_cmd.bind(&master_unit->in_cmd);

    slave_unit->in_cmd.bind(&master_unit->out_cmd);
    slave_unit->out_data.bind(&master_unit->in_data);
    //master_unit->out_cmd.bind(&slave_unit->in_cmd);

    slave_unit->in_cmd.precedes(slave_unit->in_data);
    master_unit->in_cmd.precedes(master_unit->in_data);

    pc.reset(new sparta::collection::PipelineCollector("testPipe", {}, 10, &rtn, nullptr));
    sched.finalize();

    // Align the scheduler to the rising edge of both clocks
    while(!(master_clk->isPosedge() && slave_clk->isPosedge())) {
        sched.run(1, true, false); // exacting_run = true, measure time = false
    }
    pc->startCollecting();
    //sparta::log::Tap scheduler_tap(sparta::Scheduler::getScheduler(), "debug", "sched_cmds.out");

    master_unit->schedule_commands(slave_frequency_mhz);
    slave_unit->schedule_commands(master_frequency_mhz);
}

//////////////////////////////////////////////////////////////////////
//

TestSystem::~TestSystem()
{
    pc->stopCollecting();
    rtn.enterTeardown();
    sched.restartAt(0);
}

//////////////////////////////////////////////////////////////////////
//
// This class is source for checking for SyncPort isDriven

class Source : public sparta::Resource
{
public:
    class ParameterSet : public sparta::ParameterSet
    {
    public:
        ParameterSet(sparta::TreeNode* tn) : sparta::ParameterSet(tn) {}
    };

    Source(sparta::TreeNode* node, const Source::ParameterSet*);
    ~Source();

    // Called before simulation by the testing framework to
    //    - Send commands on the SyncOut port to the destn
    //    - Calculate the expected input Ticks and Data from the destn
    void scheduleCommands();

    // Self-scheduled method.  In this method we check:
    //    - The SyncPort's events can be ordered with other events
    void doWork();

    sparta::PortSet ps_;

    // These are the classes we're actually testing
    sparta::SyncOutPort<char>     out_data_;

    static constexpr const char* name = "Source";

private:
    // Event set for the unit
    sparta::EventSet ev_set_;

    // Self-scheduling event
    sparta::UniqueEvent<> ev_do_work_;

    // Total commands that should be scheduled in both directions across
    // the interfaces
    static const uint64_t NUM_COMMANDS_TO_SEND = 10;
};

//////////////////////////////////////////////////////////////////////

Source::Source(sparta::TreeNode* node, const Source::ParameterSet*) :
    sparta::Resource(node),
    ps_(node),
    out_data_(&ps_, "out_data", node->getClock()),
    ev_set_(node),
    ev_do_work_(&ev_set_, "source_do_work_event", CREATE_SPARTA_HANDLER(Source, doWork))
{

}

//////////////////////////////////////////////////////////////////////

Source::~Source()
{
    std::cout << "Destructing '" << getName() << "'\n";
}

//////////////////////////////////////////////////////////////////////

void Source::scheduleCommands()
{
    // not driven in this cycle
    EXPECT_FALSE(out_data_.isDriven());

    out_data_.send('x');
    EXPECT_TRUE(out_data_.isDriven());

    EXPECT_TRUE(out_data_.isDriven(getClock()->currentCycle()));

    sparta::Clock::Cycle clk_gap = out_data_.computeNextAvailableCycleForSend(0,1);

    for(uint32_t idx=1; idx<=NUM_COMMANDS_TO_SEND; idx++)
    {
        sparta::Clock::Cycle delay_cycles = idx * clk_gap;
        auto driven_cycles = delay_cycles;

        EXPECT_FALSE(out_data_.isDriven(delay_cycles));

        while(out_data_.isDriven(driven_cycles)) driven_cycles++;

        // send after delay (source)cycles
        out_data_.send('y', delay_cycles);

        EXPECT_TRUE(out_data_.isDriven(delay_cycles));

        // trigger event in delay cycles
        ev_do_work_.schedule(delay_cycles);
    }
}

void Source::doWork()
{
    //std::cout << __func__ << " isDriven=" << EXPECT_TRUE(out_data.isDriven())
    //          << " source cycle=" << getClock()->currentCycle()
    //          << std::endl;
}
//////////////////////////////////////////////////////////////////////
//
// This destination class for checking for SyncPort isDriven

class Destn : public sparta::Resource
{
public:
    class ParameterSet : public sparta::ParameterSet
    {
    public:
        ParameterSet(sparta::TreeNode* tn) : sparta::ParameterSet(tn) {}
    };

    Destn(sparta::TreeNode* node, const Destn::ParameterSet*);
    ~Destn();

    // Callback for data.  This method is to test that the command is
    // received before the data.  The data is ignored.
    void dataCallback(const char &);

    void doWork() {};

    sparta::PortSet ps_;

    // These are the classes we're actually testing
    sparta::SyncInPort<char>     in_data_;

    static constexpr const char* name = "Destn";

private:
    // Event set for the unit
    sparta::EventSet ev_set_;

    // Self-scheduling event
    sparta::UniqueEvent<> ev_do_work_;
};

//////////////////////////////////////////////////////////////////////

Destn::Destn(sparta::TreeNode* node, const Destn::ParameterSet*) :
    sparta::Resource(node),
    ps_(node),
    in_data_(&ps_, "in_data", node->getClock()),
    ev_set_(node),
    ev_do_work_(&ev_set_, "destn_do_work_event", CREATE_SPARTA_HANDLER(Destn, doWork))
{
    in_data_.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Destn, dataCallback, char));
}

//////////////////////////////////////////////////////////////////////

Destn::~Destn()
{
    std::cout << "Destructing '" << getName() << "'\n";
}

void Destn::dataCallback(const char &data)
{
    //std::cout << "Destination got " << data << " in cycle " << getClock()->currentCycle() << std::endl;
}

//////////////////////////////////////////////////////////////////////
//
// This class sets up a single system for testing

class TestSystem2
{
public:

    // Creates a new system with two Units, arbitrarily named 'master' and
    // 'slave'.  Parameters passed are the master/slave frequencies
    TestSystem2(double master_frequency_mhz, double slave_frequency_mhz);

    ~TestSystem2();

    sparta::Scheduler * getScheduler() {
        return &sched;
    }

private:
    sparta::RootTreeNode rtn;

    sparta::Scheduler sched;
    sparta::ClockManager cm{&sched};
    sparta::Clock::Handle root_clk;
    sparta::Clock::Handle master_clk;
    sparta::Clock::Handle slave_clk;

    sparta::ResourceFactory<Source, Source::ParameterSet> src_rfact;
    sparta::ResourceFactory<Destn, Destn::ParameterSet>   dstn_rfact;

    std::unique_ptr<sparta::ResourceTreeNode> master_tn;
    std::unique_ptr<sparta::ResourceTreeNode> slave_tn;
};

//////////////////////////////////////////////////////////////////////


TestSystem2::TestSystem2(double master_frequency_mhz, double slave_frequency_mhz)
{
    bool scheduler_debug = false;

    root_clk   = cm.makeRoot(&rtn, "root_clk");
    master_clk = cm.makeClock("master_clk", root_clk, master_frequency_mhz);
    slave_clk  = cm.makeClock("slave_clk", root_clk, slave_frequency_mhz);

    master_tn.reset(new sparta::ResourceTreeNode(&rtn, "master", "master", &src_rfact));
    master_tn->setClock(master_clk.get());

    slave_tn.reset(new sparta::ResourceTreeNode(&rtn, "slave", "slave", &dstn_rfact));
    slave_tn->setClock(slave_clk.get());

    rtn.enterConfiguring();
    cm.normalize();
    std::cout << "master:" << std::string(*master_clk) << "\n";
    std::cout << "slave:" << std::string(*slave_clk) << "\n";

    rtn.enterFinalized();

    if (scheduler_debug) {
        sched.getDAG()->print(std::cout);
    }

    Source * master_unit = master_tn->getResourceAs<Source*>();
    sparta_assert (master_unit !=0);

    Destn * slave_unit = slave_tn->getResourceAs<Destn*>();
    sparta_assert (slave_unit !=0);

    slave_unit->in_data_.setPortDelay(static_cast<sparta::Clock::Cycle>(1));

    master_unit->out_data_.bind(&slave_unit->in_data_);

    sched.finalize();

    // Align the scheduler to the rising edge of both clocks
    while(!(master_clk->isPosedge() && slave_clk->isPosedge())) {
        sched.run(1, true, false); // exacting_run = true, measure time = false
    }

    master_unit->scheduleCommands();
}

//////////////////////////////////////////////////////////////////////
//

TestSystem2::~TestSystem2()
{
    rtn.enterTeardown();
    sched.restartAt(0);
}

//////////////////////////////////////////////////////////////////////
// Run a single test for a clock crossing over the two frequencies specifed
//

void run_isDriven_test(double master_frequency_mhz, double slave_frequency_mhz)
{
    std::unique_ptr<TestSystem2> ts(new TestSystem2(master_frequency_mhz, slave_frequency_mhz));

    ts->getScheduler()->run();
    ts.reset();
}

//////////////////////////////////////////////////////////////////////
// Run a single test for a clock crossing over the two frequencies specifed
//

void run_test(double master_frequency_mhz, double slave_frequency_mhz)
{
    std::unique_ptr<TestSystem> ts(new TestSystem(master_frequency_mhz, slave_frequency_mhz));

    //sparta::log::Tap scheduler_tap(sparta::Scheduler::getScheduler(), "debug", "sched.out");

    ts->getScheduler()->run();
    ts.reset();

    if (!EXPECT_EQUAL(Unit::num_destructors_called, 2)) {
        std::cout << "ERROR: " << "run_test()"
                  << ": didn't see 2 units destructed; saw "
                  << Unit::num_destructors_called << "\n";
    }
    Unit::num_destructors_called = 0;
}

}

//////////////////////////////////////////////////////////////////////
// Main

int main()
{
     // sparta::log::Tap scheduler_debug(sparta::TreeNode::getVirtualGlobalNode(),
     //                               sparta::log::categories::DEBUG, std::cout);

    // Same frequency
    run_test(400, 400);

    // 2:1 ratio
    run_test(400, 200);

    // faster-to-slower shouldn't matter, but swapping just in case
    run_test(200, 400);

    // Non-integer ratio
    run_test(400, 333.3333);

    // >2x difference with non-integer ratio
    run_test(1933.33333, 800);

    // Very large difference with non integer ratio
    run_test(1933.33333, 25.25);

    // // Close clocks
    // run_test(400, 401);

    // Same frequency
    run_isDriven_test(400, 400);

    // 2:1 ratio
    run_isDriven_test(400, 200);

    // faster-to-slower shouldn't matter, but swapping just in case
    run_isDriven_test(200, 400);

    // Non-integer ratio
    run_isDriven_test(400, 333.3333);
    run_isDriven_test(333.3333, 400);

    // >2x difference with non-integer ratio
    run_isDriven_test(1933.33333, 800);
    run_isDriven_test(800, 1933.33333);

    // Very large difference with non integer ratio
    run_isDriven_test(1933.33333, 25.25);
    run_isDriven_test(25.25, 1933.33333);

    // Returns error
    REPORT_ERROR;
    return ERROR_CODE;
}
