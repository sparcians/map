
/**
 * \file   Pipeline_test.cpp
 * \brief  This is the testbench for sparta::Pipeline.
 *
 * It is intended to show all the use cases of sparta::Pipeline
 */

#include <iostream>
#include <cstring>
#include "sparta/resources/Pipeline.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/utils/SpartaTester.hpp"

TEST_INIT;
#define PIPEOUT_GEN

#define TEST_MANUAL_UPDATE

struct dummy_struct
{
    uint16_t int16_field;
    uint32_t int32_field;
    std::string s_field;

    dummy_struct() = default;
    dummy_struct(const uint16_t int16_field, const uint32_t int32_field, const std::string &s_field) : 
        int16_field{int16_field},
        int32_field{int32_field},
        s_field{s_field} {}
};
std::ostream &operator<<(std::ostream &os, const dummy_struct &obj)
{
    os << obj.int16_field << " " << obj.int32_field << obj.s_field << "\n";
    return os;
}

class DummyClass
{
public:
    void stage0_PU_Handle0(void) { std::cout << "  Stage[0]: handler(PortUpdate)\n"; }
    void stage0_F_Handle0(void) { std::cout << "  Stage[0]: handler0(Flush)\n"; }
    void stage0_F_Handle1(void) { std::cout << "  P2Stage[0]: handler1(Flush)\n"; }
    void stage0_T_Handle0(void) { std::cout << "  Stage[0]: handler0(Tick)\n"; }
    void stage0_T_Handle1(void) { std::cout << "  Stage[0]: handler1(Tick)\n"; }
    void stage0_T_Handle2(void) { std::cout << "  Stage[0]: handler2(Tick)\n"; }
    void stage1_PU_Handle0(void) { std::cout << "  Stage[1]: handler(PortUpdate)\n"; }
    void stage1_F_Handle0(void) { std::cout << "  Stage[1]: handler0(Flush)\n"; }
    void stage1_F_Handle1(void) { std::cout << "  P2Stage[1]: handler1(Flush)\n"; }
    void stage1_T_Handle0(void) { std::cout << "  Stage[1]: handler0(Tick)\n"; }
    void stage2_PU_Handle0(void) { std::cout << "  Stage[2]: handler(PortUpdate)\n"; }
    void stage2_F_Handle0(void) { std::cout << "  Stage[2]: handler0(Flush)\n"; }
    void stage2_F_Handle1(void) { std::cout << "  P2Stage[2]: handler1(Flush)\n"; }
    void stage2_T_Handle0(void) { std::cout << "  Stage[2]: handler0(Tick)\n"; }
    void stage2_PT_Handle0(void) { std::cout << "  Stage[2]: handler0(PostTick)\n"; }
    void stage2_PT_Handle1(void) { std::cout << "  Stage[2]: handler1(PostTick)\n"; }
    void stage3_PU_Handle0(void) { std::cout << "  Stage[3]: handler(PortUpdate)\n"; }
    void stage3_T_Handle0(void) { std::cout << "  Stage[3]: handler0(Tick)\n"; }
    void stage4_PU_Handle0(void) { std::cout << "  Stage[4]: handler(PortUpdate)\n"; }
    void stage4_F_Handle0(void) { std::cout << "  Stage[4]: handler(Flush)\n"; }
    void stage4_T_Handle0(void) { std::cout << "  Stage[4]: handler0(Tick)\n"; }

    void task0(void) { std::cout << "  Stage[3]: producer(Tick)\n"; }
    void task1(void) { std::cout << "  Stage[0]: producer(PortUpdate)\n"; }
    template<typename DataT>
    void task2(const DataT & dat) { std::cout << "  Stage[2]: consumer(Tick, " << dat << ")\n"; }
    template<typename DataT>
    void task3(const DataT & dat) { std::cout << "  Stage[4]: consumer(Flush: " << dat << ")\n"; }
};

template<typename T>
class DummyClass2
{
public:
    DummyClass2() = delete;
    DummyClass2(sparta::Pipeline<T>* ptr) :
        pipeline_ptr_(ptr)
    {}

    void flushAll() {
        auto& pipeline = (*pipeline_ptr_);
        std::cout << "Flush all pipeline stages\n";
        pipeline.flushAllStages();
    }

    void flushOne() {
        auto& pipeline = (*pipeline_ptr_);
        auto iter = pipeline.begin();
        auto stage_id = 0;
        while (iter != pipeline.end()) {
            if (iter.isValid()) {
                std::cout << "Flush pipeline stage[" << stage_id << "]\n";
                pipeline.flushStage(iter);
                break;
            }
            ++iter;
            ++stage_id;
        }
    }

    void flushOne(const uint32_t& stage_id) {
        auto& pipeline = (*pipeline_ptr_);
        if (pipeline.isValid(stage_id)) {
            std::cout << "Flush pipeline stage[" << stage_id << "]\n";
            pipeline.flushStage(stage_id);
        }
    }

private:
    sparta::Pipeline<T>* pipeline_ptr_;
};

class PipelineEntryObj
{
public:
    PipelineEntryObj() = default;
    PipelineEntryObj(const uint32_t & id, const std::string & str) : id_(id), name_(str) {}
    const uint32_t & getID() const { return id_; }
    const std::string & getName() const { return name_; }
    void print() const
    {
        //std::cout << "PipelineEntryObj: ID(" << id_ << "), Name(" << name_ << ")\n";
    }

private:
    uint32_t id_ = 0;
    std::string name_ = "default";
};

// This operator needs to be defined for pipeline collection
std::ostream & operator<<(std::ostream & os, const PipelineEntryObj & obj)
{
    obj.print();
    return os;
}

template<typename Pipe>
void runCycle(Pipe &pipe, sparta::Scheduler * sched)
{
#ifdef TEST_MANUAL_UPDATE
    pipe.update();
#endif
    sched->run(1, true, false);
}

void testPipelineContinuingEvent() {
    sparta::Scheduler scheduler;
    sparta::Clock clk("clock", &scheduler);
    EXPECT_TRUE(scheduler.getCurrentTick() == 1);
    EXPECT_TRUE(scheduler.isRunning() == 0);
    sparta::RootTreeNode rtn;
    rtn.setClock(&clk);
    sparta::Pipeline<uint64_t> examplePipeline1("myFirstSpartaPipeline", 5, &clk);
    sparta::Pipeline<dummy_struct> examplePipeline2("mySecondSpartaPipeline", 5, &clk);
    sparta::Pipeline<dummy_struct> examplePipeline3("myThirdSpartaPipeline", 5, &clk);
    EXPECT_EQUAL(examplePipeline1.capacity(), 5);
    EXPECT_EQUAL(examplePipeline2.capacity(), 5);
    EXPECT_EQUAL(examplePipeline3.capacity(), 5);
    // some opportunistic testing of the continuing feature for the pipeline unique event
    scheduler.finalize();
    rtn.enterConfiguring();
    rtn.enterFinalized();
    examplePipeline1.performOwnUpdates();
    examplePipeline2.performOwnUpdates();
    examplePipeline3.performOwnUpdates();

    //scheduler.printNextCycleEventTree(std::cerr);
    EXPECT_EQUAL(scheduler.getNextContinuingEventTime(), 0);

    EXPECT_FALSE(examplePipeline1.isAnyValid());
    EXPECT_FALSE(examplePipeline2.isAnyValid());
    EXPECT_FALSE(examplePipeline3.isAnyValid());

    // make the pipeline updater event continuing, very important for some models
    examplePipeline1.setContinuing(true);
    examplePipeline2.setContinuing(true);
    examplePipeline3.setContinuing(true);

    // add an event and let it move through a couple stages, the update event should still be scheduled
    examplePipeline1.append(42);
    auto dummy_1 = dummy_struct(1, 2, "ABC");
    auto dummy_2 = dummy_struct(11, 21, "ABCD");
    examplePipeline2.append(std::move(dummy_1));
    examplePipeline3.append(dummy_2);
    EXPECT_TRUE(dummy_1.s_field.size() == 0);
    EXPECT_TRUE(dummy_2.s_field == "ABCD");
    scheduler.run(2, true);

    // that update event keeps the scheduler from being finished
    EXPECT_FALSE(scheduler.isFinished());

    // clear that event out of there
    scheduler.run();

    // make the update event continuing now
    examplePipeline1.setContinuing(false);
    examplePipeline2.setContinuing(false);
    examplePipeline3.setContinuing(false);

    // add another event, move it one stage in
    examplePipeline1.append(84);
    auto dummy_3 = dummy_struct(3, 4, "DEF");
    auto dummy_4 = dummy_struct(31, 41, "DEFG");
    examplePipeline2.append(std::move(dummy_3));
    examplePipeline3.append(dummy_4);
    EXPECT_TRUE(dummy_3.s_field.size() == 0);
    EXPECT_TRUE(dummy_4.s_field == "DEFG");
    scheduler.run(1, true);

    // verify that the update event doesn't count toward keeping the
    // scheduler from being finished the scheduler will tell the
    // pipeline that there are queued events that have the continuing
    // flag set
    EXPECT_TRUE(scheduler.isFinished());

    rtn.enterTeardown();
}


int main ()
{
    testPipelineContinuingEvent();

    sparta::Scheduler sched;
    sparta::RootTreeNode rtn;
    sparta::ClockManager cm(&sched);
    sparta::Clock::Handle root_clk;
    root_clk = cm.makeRoot(&rtn, "root_clk");
    cm.normalize();
    rtn.setClock(root_clk.get());

    sparta::log::Tap t2(root_clk.get()->getScheduler(), "debug", "scheduler.log.debug");

    // Insert test for Pipeline from here
    sparta::EventSet es(&rtn);
    DummyClass dummyObj1;


    ////////////////////////////////////////////////////////////////////////////////
    // User-defined Event
    ////////////////////////////////////////////////////////////////////////////////

    // User Event0: Tick phase, unique event
    sparta::UniqueEvent<> ev_task0_tick
        (&es, "ev_task0_tick", CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, task0));

    // User Event1: PortUpdate phase, unique event
    sparta::UniqueEvent<sparta::SchedulingPhase::PortUpdate> ev_task1_port
        (&es, "ev_task1_port", CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, task1));

    // User Event2: Tick phase, payload event
    sparta::PayloadEvent<uint32_t> ev_task2_tick
        (&es, "ev_task2_tick",
            CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(DummyClass, &dummyObj1, task2, uint32_t));

    // User Event3: Flush phase, payload event
    sparta::PayloadEvent<std::string, sparta::SchedulingPhase::Flush> ev_task3_flush
        (&es, "ev_task3_flush",
            CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(DummyClass, &dummyObj1, task3, std::string));

    ////////////////////////////////////////////////////////////////////////////////
    // Pipeline construction
    ////////////////////////////////////////////////////////////////////////////////

    sparta::Pipeline<uint64_t> examplePipeline1("myFirstSpartaPipeline", 5, root_clk.get());
    EXPECT_EQUAL(examplePipeline1.capacity(), 5);

    sparta::Pipeline<PipelineEntryObj> examplePipeline2("mySecondSpartaPipeline", 20, root_clk.get());

    sparta::Pipeline<uint64_t> examplePipeline3("myThirdSpartaPipeline", 5, root_clk.get());

    sparta::Pipeline<bool> examplePipeline4("myFourthSpartaPipeline", 5, root_clk.get());

    sparta::Pipeline<uint64_t> examplePipeline5("myFifthSpartaPipeline", 5, root_clk.get());

    sparta::Pipeline<uint64_t> examplePipeline6("mySixthSpartaPipeline", 5, root_clk.get());

    sparta::Pipeline<uint64_t> examplePipeline7("mySeventhSpartaPipeline", 2, root_clk.get());

    sparta::Pipeline<bool> stwr_pipe("STWR_Pipe", 5, root_clk.get());

    DummyClass2<uint64_t> dummyObj2(&examplePipeline6);
    // User Event4: Flush phase, unique event
    sparta::UniqueEvent<sparta::SchedulingPhase::Flush> ev_flush_all
        (&es, "ev_flush_all", CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass2<uint64_t>, &dummyObj2, flushAll));

    // User Event5: Flush phase, unique event
    sparta::UniqueEvent<sparta::SchedulingPhase::Flush> ev_flush_first_one
        (&es, "ev_flush_first_one", CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass2<uint64_t>, &dummyObj2, flushOne));

    // User Event6: Flush phase, payload event
    sparta::PayloadEvent<uint32_t, sparta::SchedulingPhase::Flush> ev_flush_one
        (&es, "ev_flush_one", CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(DummyClass2<uint64_t>, &dummyObj2, flushOne, uint32_t));

#ifdef PIPEOUT_GEN
    examplePipeline1.enableCollection<sparta::SchedulingPhase::Collection>(&rtn);
    examplePipeline2.enableCollection<sparta::SchedulingPhase::Update>(&rtn);
    examplePipeline3.enableCollection<sparta::SchedulingPhase::Collection>(&rtn);
    examplePipeline4.enableCollection<sparta::SchedulingPhase::Collection>(&rtn);
    examplePipeline5.enableCollection<sparta::SchedulingPhase::Collection>(&rtn);
    examplePipeline6.enableCollection<sparta::SchedulingPhase::Collection>(&rtn);
    examplePipeline7.enableCollection<sparta::SchedulingPhase::Collection>(&rtn);
    stwr_pipe.enableCollection<sparta::SchedulingPhase::Collection>(&rtn);
#endif

    ////////////////////////////////////////////////////////////////////////////////
    // Pipeline stage handler registration
    ////////////////////////////////////////////////////////////////////////////////

    /*
     * examplePipeline1: Precedence Chain Setup per Stage
     * stage[0]: producer(PortUpdate) --> handler(PortUpdate) --> handler1(Tick) --> handler0(Tick) --> handler2(Tick)
     * stage[1]:
     * stage[2]: handler(Tick) --> consumer(Tick) --> handler0(PostTick) --> handler1(PostTick)
     * stage[3]: producer(Tick) --> handler(Tick)
     * stage[4]: handler(Flush) --> consumer(Flush)
     */

    // examplePipeline1 Stage[0] handler: PortUpdate phase
    EXPECT_NOTHROW(
        examplePipeline1.registerHandlerAtStage<sparta::SchedulingPhase::PortUpdate>
            (0, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage0_PU_Handle0)));

    // examplePipeline1 Stage[0] handler: Tick phase
    EXPECT_NOTHROW(
        examplePipeline1.registerHandlerAtStage<sparta::SchedulingPhase::Tick>
            (0, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage0_T_Handle1)));

    // examplePipeline1 Stage[0] handler: Tick phase
    EXPECT_NOTHROW(
        examplePipeline1.registerHandlerAtStage<sparta::SchedulingPhase::Tick>
            (0, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage0_T_Handle0)));

    // examplePipeline1 Stage[0] handler: Tick phase
    EXPECT_NOTHROW(
        examplePipeline1.registerHandlerAtStage<sparta::SchedulingPhase::Tick>
            (0, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage0_T_Handle2)));

    // examplePipeline1 Stage[2] handler: Tick phase
    EXPECT_NOTHROW(
        examplePipeline1.registerHandlerAtStage<sparta::SchedulingPhase::Tick>
            (2, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage2_T_Handle0)));

    // examplePipeline1 Stage[2] handler: PostTick phase
    EXPECT_NOTHROW(
        examplePipeline1.registerHandlerAtStage<sparta::SchedulingPhase::PostTick>
            (2, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage2_PT_Handle0)));

    // examplePipeline1 Stage[2] handler: PostTick phase
    EXPECT_NOTHROW(
        examplePipeline1.registerHandlerAtStage<sparta::SchedulingPhase::PostTick>
            (2, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage2_PT_Handle1)));

    // examplePipeline1 Stage[3] handler: Tick phase
    EXPECT_NOTHROW(
        examplePipeline1.registerHandlerAtStage
            (3, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage3_T_Handle0)));

    // examplePipeline1 Stage[4] handler: Flush phase
    EXPECT_NOTHROW(
        examplePipeline1.registerHandlerAtStage<sparta::SchedulingPhase::Flush>
            (4, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage4_F_Handle0)));

    // Attempt to register handlers for a non-existing stage
    EXPECT_THROW(examplePipeline1.registerHandlerAtStage
        (5, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage4_F_Handle0)));

    /*
     * examplePipeline3: Precedence Chain Setup per Stage
     * stage[2]:handler(Flush) --> stage[1]:handler(Flush) --> stage[0]:handler(Flush)
     */

    // examplePipeline3 Stage[0] handler: Flush phase
    EXPECT_NOTHROW(
        examplePipeline3.registerHandlerAtStage<sparta::SchedulingPhase::Flush>
            (0, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage0_F_Handle0)));

    // examplePipeline3 Stage[1] handler: Flush phase
    EXPECT_NOTHROW(
        examplePipeline3.registerHandlerAtStage<sparta::SchedulingPhase::Flush>
            (1, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage1_F_Handle0)));

    // examplePipeline3 Stage[2] handler: Flush phase
    EXPECT_NOTHROW(
        examplePipeline3.registerHandlerAtStage<sparta::SchedulingPhase::Flush>
            (2, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage2_F_Handle0)));

    /*
     * examplePipeline4: Precedence Chain Setup per Stage
     * stage[0]:handler(Flush) --> stage[1]:handler(Flush) --> stage[2]:handler(Flush)
     */

    // examplePipeline4 Stage[0] handler: Flush phase
    EXPECT_NOTHROW(
        examplePipeline4.registerHandlerAtStage<sparta::SchedulingPhase::Flush>
            (0, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage0_F_Handle1)));

    // examplePipeline4 Stage[1] handler: Flush phase
    EXPECT_NOTHROW(
        examplePipeline4.registerHandlerAtStage<sparta::SchedulingPhase::Flush>
            (1, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage1_F_Handle1)));

    // examplePipeline4 Stage[2] handler: Flush phase
    EXPECT_NOTHROW(
        examplePipeline4.registerHandlerAtStage<sparta::SchedulingPhase::Flush>
            (2, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage2_F_Handle1)));

    /*
     * examplePipeline5: Precedence Chain Setup per Stage
     * stage[0]:handler0(Tick)
     * stage[1]:handler0(Tick)
     * stage[2]:handler0(Tick)
     * stage[3]:handler0(Tick)
     * stage[4]:handler0(Tick)
     */

    // examplePipeline5 Stage[0] handler: Tick phase
    EXPECT_NOTHROW(
        examplePipeline5.registerHandlerAtStage<sparta::SchedulingPhase::Tick>
            (0, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage0_T_Handle0)));

    // examplePipeline5 Stage[1] handler: Tick phase
    EXPECT_NOTHROW(
        examplePipeline5.registerHandlerAtStage<sparta::SchedulingPhase::Tick>
            (1, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage1_T_Handle0)));

    // examplePipeline5 Stage[2] handler: Tick phase
    EXPECT_NOTHROW(
        examplePipeline5.registerHandlerAtStage<sparta::SchedulingPhase::Tick>
            (2, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage2_T_Handle0)));

    // examplePipeline5 Stage[3] handler: Tick phase
    EXPECT_NOTHROW(
        examplePipeline5.registerHandlerAtStage<sparta::SchedulingPhase::Tick>
            (3, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage3_T_Handle0)));

    // examplePipeline5 Stage[4] handler: Tick phase
    EXPECT_NOTHROW(
        examplePipeline5.registerHandlerAtStage<sparta::SchedulingPhase::Tick>
            (4, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage4_T_Handle0)));

    /*
     * examplePipeline6: Precedence Chain Setup per Stage
     * stage[0]: handler0(PortUpdate) --> handler0(Tick)
     * stage[1]: handler0(PortUpdate) --> handler0(Tick)
     * stage[2]: handler0(PortUpdate) --> handler0(Tick)
     * stage[3]: handler0(Tick)
     * stage[4]: handler0(Tick)
     */

    // examplePipeline6 Stage[0] handler: PortUpdate phase
    EXPECT_NOTHROW(
        examplePipeline6.registerHandlerAtStage<sparta::SchedulingPhase::PortUpdate>
            (0, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage0_PU_Handle0)));

    // examplePipeline6 Stage[0] handler: Tick phase
    EXPECT_NOTHROW(
        examplePipeline6.registerHandlerAtStage<sparta::SchedulingPhase::Tick>
            (0, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage0_T_Handle0)));

    // examplePipeline6 Stage[1] handler: PortUpdate phase
    EXPECT_NOTHROW(
        examplePipeline6.registerHandlerAtStage<sparta::SchedulingPhase::PortUpdate>
            (1, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage1_PU_Handle0)));

    // examplePipeline6 Stage[1] handler: Tick phase
    EXPECT_NOTHROW(
        examplePipeline6.registerHandlerAtStage<sparta::SchedulingPhase::Tick>
            (1, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage1_T_Handle0)));

    // examplePipeline6 Stage[2] handler: PortUpdate phase
    EXPECT_NOTHROW(
        examplePipeline6.registerHandlerAtStage<sparta::SchedulingPhase::PortUpdate>
            (2, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage2_PU_Handle0)));

    // examplePipeline6 Stage[2] handler: Tick phase
    EXPECT_NOTHROW(
        examplePipeline6.registerHandlerAtStage<sparta::SchedulingPhase::Tick>
            (2, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage2_T_Handle0)));

    // examplePipeline6 Stage[3] handler: Tick phase
    EXPECT_NOTHROW(
        examplePipeline6.registerHandlerAtStage<sparta::SchedulingPhase::Tick>
            (3, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage3_T_Handle0)));

    // examplePipeline6 Stage[4] handler: Tick phase
    EXPECT_NOTHROW(
        examplePipeline6.registerHandlerAtStage<sparta::SchedulingPhase::Tick>
            (4, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage4_T_Handle0)));


    rtn.enterConfiguring();
    rtn.enterFinalized();

#ifdef PIPEOUT_GEN
    sparta::collection::PipelineCollector pc("examplePipeline1", 1000000, root_clk.get(), &rtn);
#endif


    ////////////////////////////////////////////////////////////////////////////////
    // Pipeline stage handling event precedence setup
    ////////////////////////////////////////////////////////////////////////////////

    EXPECT_NOTHROW(examplePipeline1.setPrecedenceBetweenStage(3, 2));

    EXPECT_THROW(examplePipeline1.setPrecedenceBetweenStage(0, 0));
    EXPECT_NOTHROW(examplePipeline1.setPrecedenceBetweenStage(2, 0));
    EXPECT_THROW(examplePipeline1.setPrecedenceBetweenStage(0, 1));


    ////////////////////////////////////////////////////////////////////////////////
    // Pipeline stage producer event setup
    ////////////////////////////////////////////////////////////////////////////////

    EXPECT_NOTHROW(examplePipeline1.setProducerForStage(0, ev_task1_port));
    EXPECT_NOTHROW(examplePipeline1.setProducerForStage(0, ev_task2_tick));
    EXPECT_NOTHROW(examplePipeline1.setProducerForStage(3, ev_task0_tick));

    EXPECT_THROW(examplePipeline1.setProducerForStage(1, ev_task0_tick));
    EXPECT_THROW(examplePipeline1.setProducerForStage(2, ev_task1_port));


    ////////////////////////////////////////////////////////////////////////////////
    // Pipeline stage consumer event setup
    ////////////////////////////////////////////////////////////////////////////////

    EXPECT_NOTHROW(examplePipeline1.setConsumerForStage(2, ev_task2_tick));
    EXPECT_NOTHROW(examplePipeline1.setConsumerForStage(4, ev_task3_flush));

    EXPECT_THROW(examplePipeline1.setConsumerForStage(1, ev_task2_tick));
    EXPECT_THROW(examplePipeline1.setConsumerForStage(3, ev_task3_flush));


    ////////////////////////////////////////////////////////////////////////////////
    // Set precedence between two stages from diffferent Pipeline instances
    ////////////////////////////////////////////////////////////////////////////////

    EXPECT_THROW(examplePipeline3.setPrecedenceBetweenPipeline(2, examplePipeline3, 1));
    EXPECT_THROW(examplePipeline3.setPrecedenceBetweenPipeline(2, examplePipeline4, 4));
    EXPECT_THROW(examplePipeline3.setPrecedenceBetweenPipeline(4, examplePipeline4, 1));
    EXPECT_NOTHROW(examplePipeline4.setPrecedenceBetweenPipeline(2, examplePipeline3, 2));


    ////////////////////////////////////////////////////////////////////////////////
    // Pipeline default stage precedence setup
    ////////////////////////////////////////////////////////////////////////////////

    /*
     * Overall Precedence Chain Setup
     * stage[0]:producer(PortUpdate) --> stage[0]:handler(PortUpdate)
     * stage[4]:handler(Flush) --> stage[4]:consumer(Flush)
     * stage[2]:handler(Tick)-------------------------------> stage[0]:handler1(Tick) --> stage[0]:handler0(Tick) --> stage[0]:handler2(Tick) --> stage[3]:producer(Tick) --> stage[3]:handler(Tick)
     *                       \--> stage[2]:consumer(Tick)--/
     * stage[2]:handler0(PostTick) --> stage[2]:handler1(PostTick)
     */
    EXPECT_NOTHROW(examplePipeline1.setDefaultStagePrecedence(sparta::Pipeline<uint64_t>::Precedence::BACKWARD));
    EXPECT_NOTHROW(examplePipeline3.setDefaultStagePrecedence(sparta::Pipeline<uint64_t>::Precedence::BACKWARD));
    EXPECT_NOTHROW(examplePipeline4.setDefaultStagePrecedence(sparta::Pipeline<bool>::Precedence::FORWARD));
    EXPECT_NOTHROW(examplePipeline5.setDefaultStagePrecedence(sparta::Pipeline<uint64_t>::Precedence::BACKWARD));
    EXPECT_NOTHROW(examplePipeline6.setDefaultStagePrecedence(sparta::Pipeline<uint64_t>::Precedence::BACKWARD));


    ////////////////////////////////////////////////////////////////////////////////
    // Registered events access
    ////////////////////////////////////////////////////////////////////////////////

    // examplePipeline7 Stage[0] handler: PortUpdate phase
    EXPECT_NOTHROW(
        examplePipeline7.registerHandlerAtStage<sparta::SchedulingPhase::PortUpdate>
            (0, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage0_PU_Handle0)));

    // examplePipeline7 Stage[1] handler: PortUpdate phase
    EXPECT_NOTHROW(
        examplePipeline7.registerHandlerAtStage<sparta::SchedulingPhase::PortUpdate>
            (1, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage1_PU_Handle0)));

    // Each of the two stages should have 1 registered event for the PortUpdate phase
    EXPECT_EQUAL(examplePipeline7.getEventsAtStage(0, sparta::SchedulingPhase::PortUpdate).size(), 1);
    EXPECT_EQUAL(examplePipeline7.getEventsAtStage(1, sparta::SchedulingPhase::PortUpdate).size(), 1);

    // Add a registered Tick phase event for stage 2
    EXPECT_NOTHROW(
        examplePipeline7.registerHandlerAtStage<sparta::SchedulingPhase::Tick>
            (1, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage1_T_Handle0)));

    // Verify that stage 1 now has one Tick phase event registered
    uint32_t pipeline7_stage1_num_tick_events = 0;
    EXPECT_NOTHROW(pipeline7_stage1_num_tick_events =
                   examplePipeline7.getEventsAtStage(1, sparta::SchedulingPhase::Tick).size());
    EXPECT_EQUAL(pipeline7_stage1_num_tick_events, 1);

    // Add another stage 1 Tick phase event
    EXPECT_NOTHROW(
        examplePipeline7.registerHandlerAtStage<sparta::SchedulingPhase::Tick>
            (1, CREATE_SPARTA_HANDLER_WITH_OBJ(DummyClass, &dummyObj1, stage1_T_Handle0)));

    // Verify that stage 1 now has two Tick phase events registered
    pipeline7_stage1_num_tick_events = 0;
    EXPECT_NOTHROW(pipeline7_stage1_num_tick_events =
                   examplePipeline7.getEventsAtStage(1, sparta::SchedulingPhase::Tick).size());
    EXPECT_EQUAL(pipeline7_stage1_num_tick_events, 2);

    // Verify that we also see two Tick phase events registered when the phase is not
    // explicitly given (defaults to SchedulingPhase::Tick)
    pipeline7_stage1_num_tick_events = 0;
    EXPECT_NOTHROW(pipeline7_stage1_num_tick_events =
                   examplePipeline7.getEventsAtStage(1).size());
    EXPECT_EQUAL(pipeline7_stage1_num_tick_events, 2);

    sched.finalize();

#ifdef PIPEOUT_GEN
    pc.startCollection(&rtn);
#endif


    ////////////////////////////////////////////////////////////////////////////////
    // Pipeline Forward Progression Test
    ////////////////////////////////////////////////////////////////////////////////

    std::cout << "\n[START] Pipeline Forward Progression Test" << std::endl;

    uint32_t cyc_cnt = 0;
#ifndef TEST_MANUAL_UPDATE
    examplePipeline1.performOwnUpdates();
#endif


    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    // Append Pipeline
    examplePipeline1.append(19);

    // Run Cycle-0
    sched.run(1, true);
    EXPECT_FALSE(examplePipeline1.isValid(0));


    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
#ifdef TEST_MANUAL_UPDATE
    // Run Cycle-1(a)
    sched.run(1, true);
    examplePipeline1.update();
    ev_task1_port.schedule(sparta::Clock::Cycle(0));
    // Run Cycle-1(b) && Cycle-2(a)
    sched.run(1, true);
#else
    // Run Cycle-1
    ev_task1_port.schedule(sparta::Clock::Cycle(1));
    sched.run(1, true);
#endif
    // Test pipeline read/write using [] semantics
    EXPECT_EQUAL(examplePipeline1.numValid(), 1);
    EXPECT_TRUE(examplePipeline1.isValid(0));
    EXPECT_EQUAL(examplePipeline1[0], 19);
    EXPECT_NOTHROW(examplePipeline1[0] -= 5);
    EXPECT_THROW(examplePipeline1[5] = 100);


    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    examplePipeline1.append(20);
    // Run Cycle-2(b) && Cycle-3(a)
    runCycle(examplePipeline1, &sched);
    // Test pipeline forward progression and specific stage modification
    EXPECT_EQUAL(examplePipeline1.numValid(), 2);
    EXPECT_TRUE(examplePipeline1.isValid(0));
    EXPECT_EQUAL(examplePipeline1[0], 20);
    EXPECT_TRUE(examplePipeline1.isValid(1));
    EXPECT_EQUAL(examplePipeline1[1], 14);
    EXPECT_NOTHROW(examplePipeline1.writeStage(0, 25));


    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    examplePipeline1.append(21);
#ifdef TEST_MANUAL_UPDATE
    examplePipeline1.update();
    ev_task2_tick.preparePayload(100)->schedule(sparta::Clock::Cycle(0));
    // Run Cycle-3(b) && Cycle-4(a)
    sched.run(1, true);
#else
    ev_task2_tick.preparePayload(100)->schedule(sparta::Clock::Cycle(1));
    // Run cycle-3
    sched.run(1, true);
#endif
    // Test pipeline forward progression
    EXPECT_EQUAL(examplePipeline1.numValid(), 3);
    EXPECT_TRUE(examplePipeline1.isValid(0));
    EXPECT_EQUAL(examplePipeline1[0], 21);
    EXPECT_TRUE(examplePipeline1.isValid(1));
    EXPECT_EQUAL(examplePipeline1[1], 25);
    EXPECT_TRUE(examplePipeline1.isValid(2));
    EXPECT_EQUAL(examplePipeline1[2], 14);


    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
#ifdef TEST_MANUAL_UPDATE
    examplePipeline1.update();
    ev_task0_tick.schedule(sparta::Clock::Cycle(0));
    // Run Cycle-4(b) && Cycle-5(a)
    sched.run(1, true);
#else
    ev_task0_tick.schedule(sparta::Clock::Cycle(1));
    // Run cycle-4
    sched.run(1, true);
#endif
    // Test pipeline forward progression
    EXPECT_EQUAL(examplePipeline1.numValid(), 3);
    EXPECT_TRUE(examplePipeline1.isValid(1));
    EXPECT_EQUAL(examplePipeline1[1], 21);
    EXPECT_TRUE(examplePipeline1.isValid(2));
    EXPECT_EQUAL(examplePipeline1[2], 25);
    EXPECT_TRUE(examplePipeline1.isValid(3));
    EXPECT_EQUAL(examplePipeline1[3], 14);


    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
#ifdef TEST_MANUAL_UPDATE
    examplePipeline1.update();
    ev_task3_flush.preparePayload("flushing")->schedule(sparta::Clock::Cycle(0));
    // Run Cycle-5(b) && Cycle-6(a)
    sched.run(1, true);
#else
    ev_task3_flush.preparePayload("flushing")->schedule(sparta::Clock::Cycle(1));
    // Run cycle-5
    sched.run(1, true);
#endif
    // Test pipeline forward progression
    EXPECT_EQUAL(examplePipeline1.numValid(), 3);
    EXPECT_TRUE(examplePipeline1.isValid(2));
    EXPECT_EQUAL(examplePipeline1[2], 21);
    EXPECT_TRUE(examplePipeline1.isValid(3));
    EXPECT_EQUAL(examplePipeline1[3], 25);
    EXPECT_TRUE(examplePipeline1.isValid(4));
    EXPECT_EQUAL(examplePipeline1[4], 14);


    while (cyc_cnt < examplePipeline1.capacity() + 3) {
        std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
        runCycle(examplePipeline1, &sched);
    }
    // Test pipeline forward progression
    EXPECT_EQUAL(examplePipeline1.numValid(), 1);
    EXPECT_TRUE(examplePipeline1.isLastValid());
    EXPECT_EQUAL(examplePipeline1[4], 21);

    // Run the last cycle (i.e. drain the pipeline)
    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline1, &sched);
    // Test pipeline draining
    EXPECT_FALSE(examplePipeline1.isAnyValid());
    EXPECT_EQUAL(examplePipeline1.size(), 0);

    std::cout << "[FINISH] Pipeline Forward Progression Test" << std::endl;

    ////////////////////////////////////////////////////////////////////////////////
    // Pipeline Stage Mutation & Invalidation Test
    ////////////////////////////////////////////////////////////////////////////////

    std::cout << "\n[START] Pipeline Stage Mutation & Invalidation Test" << std::endl;

    cyc_cnt = 0;
    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";

    EXPECT_NOTHROW(examplePipeline1.append(200));
    EXPECT_NOTHROW(examplePipeline1.writeStage(1, 100));
    EXPECT_NOTHROW(examplePipeline1.writeStage(2, 50));
    runCycle(examplePipeline1, &sched);
    // Test pipeline append and specific stage modification
    EXPECT_EQUAL(examplePipeline1.numValid(), 3);
    EXPECT_TRUE(examplePipeline1.isValid(0));
    EXPECT_TRUE(examplePipeline1.isValid(2));
    EXPECT_TRUE(examplePipeline1.isValid(3));
    EXPECT_THROW(examplePipeline1.isValid(5));


    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    EXPECT_NOTHROW(examplePipeline1.append(300));
    EXPECT_NOTHROW(examplePipeline1.invalidateStage(3));
    EXPECT_THROW(examplePipeline1.invalidateStage(1));
    runCycle(examplePipeline1, &sched);
    // Test pipeline specific stage modification
    EXPECT_EQUAL(examplePipeline1.numValid(), 3);
    EXPECT_TRUE(examplePipeline1.isValid(0));
    EXPECT_TRUE(examplePipeline1.isValid(1));
    EXPECT_TRUE(examplePipeline1.isValid(3));


    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    EXPECT_NOTHROW(examplePipeline1.flushStage(3));
    EXPECT_EQUAL(examplePipeline1.numValid(), 2);
    runCycle(examplePipeline1, &sched);
    // Test pipeline specific stage flushing
    EXPECT_EQUAL(examplePipeline1.numValid(), 2);
    EXPECT_TRUE(examplePipeline1.isValid(1));
    EXPECT_TRUE(examplePipeline1.isValid(2));


    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    EXPECT_NOTHROW(examplePipeline1.flushAllStages());
    EXPECT_EQUAL(examplePipeline1.numValid(), 0);
    runCycle(examplePipeline1, &sched);
    // Test whole pipeline flushing
    EXPECT_EQUAL(examplePipeline1.numValid(), 0);

    std::cout << "[FINISH] Pipeline Stage Mutation & Invalidation Test" << std::endl;


    ////////////////////////////////////////////////////////////////////////////////
    // Pipeline Stage Handling Event Activation/Deactivation Test
    ////////////////////////////////////////////////////////////////////////////////

    std::cout << "\n[START] Pipeline Stage Handling Event Activation/Deactivation Test\n";

    cyc_cnt = 0;
    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";

    // Test de-activation of pipeline stage handling events
    EXPECT_NOTHROW(examplePipeline1.append(1000));
    EXPECT_NOTHROW(examplePipeline1.deactivateEventAtStage(0));
    EXPECT_THROW(examplePipeline1.deactivateEventAtStage(1));
    EXPECT_THROW(examplePipeline1.activateEventAtStage(1));
    std::cout << "  NOTE: Stage[0] Event Hander is de-activated!\n";
    runCycle(examplePipeline1, &sched);
    EXPECT_EQUAL(examplePipeline1.numValid(), 1);
    EXPECT_TRUE(examplePipeline1.isValid(0));


    // Test re-activation of pipeline stage handling events
    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    EXPECT_NOTHROW(examplePipeline1.append(2000));
    EXPECT_NOTHROW(examplePipeline1.activateEventAtStage(0));
    std::cout << "  NOTE: Stage[0] Event Hander is re-activated!\n";
    runCycle(examplePipeline1, &sched);
    EXPECT_EQUAL(examplePipeline1.numValid(), 2);
    EXPECT_TRUE(examplePipeline1.isValid(0));
    EXPECT_TRUE(examplePipeline1.isValid(1));


    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline1, &sched);
    EXPECT_EQUAL(examplePipeline1.numValid(), 2);
    EXPECT_TRUE(examplePipeline1.isValid(1));
    EXPECT_TRUE(examplePipeline1.isValid(2));


    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    EXPECT_NOTHROW(examplePipeline1.deactivateEventAtStage(2));
    std::cout << "  NOTE: Stage[2] Event Hander is de-activated!\n";
    runCycle(examplePipeline1, &sched);
    EXPECT_EQUAL(examplePipeline1.numValid(), 2);
    EXPECT_TRUE(examplePipeline1.isValid(2));
    EXPECT_TRUE(examplePipeline1.isValid(3));


    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    EXPECT_NOTHROW(examplePipeline1.append(3000));
    EXPECT_NOTHROW(examplePipeline1.activateEventAtStage(2));
    std::cout << "  NOTE: Stage[2] Event Hander is re-activated!\n";
    runCycle(examplePipeline1, &sched);

    uint32_t offset = cyc_cnt;
    while (cyc_cnt < examplePipeline1.capacity() + offset) {
        std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline1, &sched);
    }
    EXPECT_EQUAL(examplePipeline1.numValid(), 0);


    std::cout << "[FINISH] Pipeline Stage Handling Event Activation/Deactivation Test\n";


    ////////////////////////////////////////////////////////////////////////////////
    // Pipeline Iterator Test
    ////////////////////////////////////////////////////////////////////////////////

    std::cout << "\n[START] Pipeline Iterator Test" << std::endl;

    sparta::Pipeline<PipelineEntryObj>::const_iterator iterForStage2 = examplePipeline2.begin();
    std::advance(iterForStage2, 2);

    cyc_cnt = 0;

#ifndef TEST_MANUAL_UPDATE
    examplePipeline2.performOwnUpdates();
#endif


    examplePipeline2.append(PipelineEntryObj());
    EXPECT_FALSE(iterForStage2.isValid());
#ifdef TEST_MANUAL_UPDATE
    // Run Cycle-0(a)
    sched.run(1, true);
    examplePipeline2.update();
    // Run Cycle-0(b) && Cycle-1(a)
    sched.run(1, true);
#else
    // Run Cycle-0
    sched.run(1, true);
#endif
    cyc_cnt++;
    EXPECT_FALSE(iterForStage2.isValid());


    // Test pipeline append and forward progression with user-defined entry object
    for (uint32_t i = 0; i < examplePipeline2.capacity(); i++) {
        if (cyc_cnt%3 == 0) {
            examplePipeline2.append(PipelineEntryObj(i, "newPipelineObj"));

            EXPECT_TRUE(iterForStage2.isValid());
            EXPECT_NOTHROW((*iterForStage2).getID());
        }
        else {
            EXPECT_FALSE(iterForStage2.isValid());
            EXPECT_THROW((*iterForStage2).getID());
        }

        runCycle(examplePipeline2, &sched);
        cyc_cnt++;
    }


    // Test operator* and operator-> of pipeline iterator
    auto iter = examplePipeline2.begin();
    uint32_t valid_cnt = 0;
    for (uint32_t i = 0; iter != examplePipeline2.end(); i++) {
        if (iter.isValid()) {
            std::cout << "Pipeline Stage[" << i << "]: "
                      << "ObjectID(" << iter->getID() << "), "
                      << "ObjectName(" << (*iter).getName() << ")\n";
            valid_cnt++;
        }
        iter++;
    }


    std::cout << "[FINISH] Pipeline Iterator Test" << std::endl;


    ////////////////////////////////////////////////////////////////////////////////
    // Cross Pipeline Precedence Setup Test
    ////////////////////////////////////////////////////////////////////////////////

    std::cout << "\n[START] Cross Pipeline Precedence Setup Test" << std::endl;

    examplePipeline3.performOwnUpdates();
    examplePipeline4.performOwnUpdates();

    // Payload count is set to equal to number of registered handlers
    uint32_t payload_cnt = 3;
    for (uint32_t i = 0; i < examplePipeline3.capacity() + payload_cnt; i++) {
        std::cout << "Cycle[" << i << "]:\n";
        if (i < payload_cnt) {
            examplePipeline3.append(1);
            examplePipeline4.append(true);
        }
        sched.run(2, true);
    }
    std::cout << "[FINISH] Cross Pipeline Precedence Setup Test" << std::endl;


    ////////////////////////////////////////////////////////////////////////////////
    // Pipeline Stall/Restart Handling Test
    ////////////////////////////////////////////////////////////////////////////////

    std::cout << "\n[START] Pipeline Stall/Restart Handling Test\n";

#ifndef TEST_MANUAL_UPDATE
    examplePipeline5.performOwnUpdates();
#endif

    cyc_cnt = 0;


    std::cout << "Append pipeline with data[=1000]\n";
    EXPECT_NOTHROW(examplePipeline5.append(1000));

    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline5, &sched);
    EXPECT_EQUAL(examplePipeline5.numValid(), 1);
    EXPECT_TRUE(examplePipeline5.isValid(0));


    std::cout << "Append pipeline with data[=2000]\n";
    EXPECT_NOTHROW(examplePipeline5.append(2000));

    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline5, &sched);
    EXPECT_EQUAL(examplePipeline5.numValid(), 2);
    EXPECT_TRUE(examplePipeline5.isValid(0));
    EXPECT_TRUE(examplePipeline5.isValid(1));


    std::cout << "Append pipeline with data[=3000]\n";
    EXPECT_NOTHROW(examplePipeline5.append(3000));

    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline5, &sched);
    EXPECT_EQUAL(examplePipeline5.numValid(), 3);
    EXPECT_TRUE(examplePipeline5.isValid(0));
    EXPECT_TRUE(examplePipeline5.isValid(1));
    EXPECT_TRUE(examplePipeline5.isValid(2));


    std::cout << "Stall stage[1] for 2 cycles\n";
    EXPECT_NOTHROW(examplePipeline5.stall(1, 2));
    EXPECT_TRUE(examplePipeline5.isStalledOrStalling());
    EXPECT_TRUE(examplePipeline5.isStalledOrStallingAtStage(0));
    EXPECT_FALSE(examplePipeline5.isStalledOrStallingAtStage(2));
    // Attempt to do back-to-back stall in the same cycle is forbidden
    EXPECT_THROW(examplePipeline5.stall(2, 2));
    EXPECT_THROW(examplePipeline5.stall(0, 2));

    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline5, &sched);
    EXPECT_EQUAL(examplePipeline5.numValid(), 3);
    EXPECT_TRUE(examplePipeline5.isValid(0));
    EXPECT_TRUE(examplePipeline5.isValid(1));
    EXPECT_TRUE(examplePipeline5.isValid(3));
    EXPECT_EQUAL(examplePipeline5[0], 3000);
    EXPECT_EQUAL(examplePipeline5[1], 2000);
    EXPECT_EQUAL(examplePipeline5[3], 1000);


    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    // Attempt to stall being stalled pipeline is forbidden
    EXPECT_THROW(examplePipeline5.stall(2, 2));
    EXPECT_THROW(examplePipeline5.stall(0, 2));
    runCycle(examplePipeline5, &sched);
    EXPECT_EQUAL(examplePipeline5.numValid(), 3);
    EXPECT_TRUE(examplePipeline5.isValid(0));
    EXPECT_TRUE(examplePipeline5.isValid(1));
    EXPECT_TRUE(examplePipeline5.isValid(4));
    EXPECT_EQUAL(examplePipeline5[0], 3000);
    EXPECT_EQUAL(examplePipeline5[1], 2000);
    EXPECT_EQUAL(examplePipeline5[4], 1000);


    std::cout << "Stall stage[0] for 1 more cycles\n";
    // Test stalling a stage that is about to restart
    EXPECT_NOTHROW(examplePipeline5.stall(0, 1));

    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    // Test writting into the stage that is about to restart
    EXPECT_NOTHROW(examplePipeline5.writeStage(1, 2500));
    runCycle(examplePipeline5, &sched);
    EXPECT_EQUAL(examplePipeline5.numValid(), 2);
    EXPECT_TRUE(examplePipeline5.isValid(0));
    EXPECT_TRUE(examplePipeline5.isValid(2));
    EXPECT_EQUAL(examplePipeline5[0], 3000);
    EXPECT_EQUAL(examplePipeline5[2], 2500);


    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline5, &sched);
    EXPECT_EQUAL(examplePipeline5.numValid(), 2);
    EXPECT_TRUE(examplePipeline5.isValid(1));
    EXPECT_TRUE(examplePipeline5.isValid(3));


    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline5, &sched);
    EXPECT_EQUAL(examplePipeline5.numValid(), 2);
    EXPECT_TRUE(examplePipeline5.isValid(2));
    EXPECT_TRUE(examplePipeline5.isValid(4));


    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline5, &sched);
    EXPECT_EQUAL(examplePipeline5.numValid(), 1);
    EXPECT_TRUE(examplePipeline5.isValid(3));


    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline5, &sched);
    EXPECT_EQUAL(examplePipeline5.numValid(), 1);
    EXPECT_TRUE(examplePipeline5.isValid(4));


    offset = cyc_cnt;
    while (cyc_cnt < examplePipeline5.capacity() + offset) {
        std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
        runCycle(examplePipeline5, &sched);
    }
    EXPECT_EQUAL(examplePipeline5.numValid(), 0);

    // pipeline stall with bubble crushing
    std::cout << "Pipeline Stall with bubble crushing Test\n";

    std::cout << "Append pipeline with data[=1000]\n";
    EXPECT_NOTHROW(examplePipeline5.append(1000));
    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline5, &sched);
    EXPECT_TRUE(examplePipeline5.isValid(0));
    EXPECT_EQUAL(examplePipeline5.numValid(), 1);

    std::cout << "Append pipeline with data[=2000]\n";
    EXPECT_NOTHROW(examplePipeline5.append(2000));
    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline5, &sched);
    EXPECT_TRUE(examplePipeline5.isValid(0));
    EXPECT_TRUE(examplePipeline5.isValid(1));
    EXPECT_EQUAL(examplePipeline5.numValid(), 2);

    // bubble
    std::cout << "Insert bubble\n";
    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline5, &sched);
    EXPECT_TRUE(!examplePipeline5.isValid(0));
    EXPECT_TRUE(examplePipeline5.isValid(1));
    EXPECT_TRUE(examplePipeline5.isValid(2));
    EXPECT_EQUAL(examplePipeline5.numValid(), 2);

    // now stall
    std::cout << "Stall (insert bubble)\n";
    EXPECT_NOTHROW(examplePipeline5.stall(2, 1, true));
    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline5, &sched);
    // should be unchanged
    EXPECT_TRUE(!examplePipeline5.isValid(0));
    EXPECT_TRUE(examplePipeline5.isValid(1));
    EXPECT_TRUE(examplePipeline5.isValid(2));
    EXPECT_EQUAL(examplePipeline5.numValid(), 2);

    // push
    std::cout << "Append pipeline with data[=3000]\n";
    EXPECT_NOTHROW(examplePipeline5.append(3000));
    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline5, &sched);
    EXPECT_TRUE(examplePipeline5.isValid(0));
    EXPECT_TRUE(!examplePipeline5.isValid(1)); // bubble advances
    EXPECT_TRUE(examplePipeline5.isValid(2));
    EXPECT_TRUE(examplePipeline5.isValid(3));
    EXPECT_EQUAL(examplePipeline5.numValid(), 3);

    // stall again
    EXPECT_NOTHROW(examplePipeline5.stall(3, 1, true));
    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline5, &sched);
    EXPECT_TRUE(!examplePipeline5.isValid(0));
    EXPECT_TRUE(examplePipeline5.isValid(1)); // bubble crushed
    EXPECT_TRUE(examplePipeline5.isValid(2));
    EXPECT_TRUE(examplePipeline5.isValid(3));
    EXPECT_EQUAL(examplePipeline5.numValid(), 3);

    // stall and push
    std::cout << "Stall pipeline stage 3\n";
    EXPECT_NOTHROW(examplePipeline5.stall(3, 1, true));
    std::cout << "Append pipeline with data[=4000]\n";
    EXPECT_NOTHROW(examplePipeline5.append(4000));
    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline5, &sched);
    EXPECT_TRUE(examplePipeline5.isValid(0));
    EXPECT_TRUE(examplePipeline5.isValid(1));
    EXPECT_TRUE(examplePipeline5.isValid(2));
    EXPECT_TRUE(examplePipeline5.isValid(3));
    EXPECT_EQUAL(examplePipeline5.numValid(), 4);

    // allow pipeline to drain
    offset = cyc_cnt + 1;
    while (cyc_cnt < examplePipeline5.capacity() + offset) {
        std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
        runCycle(examplePipeline5, &sched);
    }
    EXPECT_EQUAL(examplePipeline5.numValid(), 0);

    // Test issue where an item is pushed to the top, then stalled at
    // the end, but then something new is added, bubbles crushed and
    // the entry seems to be valid across all pipeline stages
    //
    examplePipeline5.append(1234);
    for (uint32_t cnt = 0; cnt < examplePipeline5.capacity(); ++cnt) {
        runCycle(examplePipeline5, &sched);
    }
    EXPECT_EQUAL(examplePipeline5.numValid(), 1);
    EXPECT_TRUE(examplePipeline5.isLastValid());
    EXPECT_EQUAL(examplePipeline5[examplePipeline5.capacity() - 1], 1234);

    // now stall.  1234 should remain at the last stage in the pipeline
    examplePipeline5.stall(examplePipeline5.capacity() - 1, 1);
    runCycle(examplePipeline5, &sched);
    EXPECT_EQUAL(examplePipeline5.numValid(), 1);
    EXPECT_TRUE(examplePipeline5.isLastValid());
    EXPECT_EQUAL(examplePipeline5[examplePipeline5.capacity() - 1], 1234);

    const bool crush_bubbles = true;
    examplePipeline5.append(4321);
    examplePipeline5.stall(examplePipeline5.capacity() - 1, 1, crush_bubbles);
    runCycle(examplePipeline5, &sched);
    EXPECT_EQUAL(examplePipeline5.numValid(), 2);
    EXPECT_TRUE(examplePipeline5.isLastValid());
    EXPECT_EQUAL(examplePipeline5[examplePipeline5.capacity() - 1], 1234);
    EXPECT_EQUAL(examplePipeline5[0], 4321);

    // Start with this:
    // [4321, x, x, x, 1234]
    // Get the pipeline to look like this:
    // [x, x, x, 4321, 1234]
    for(uint32_t i = 0; i < (examplePipeline5.capacity() - 2); ++i) {
        EXPECT_EQUAL(examplePipeline5[i], 4321);
        EXPECT_TRUE(examplePipeline5.isValid(i));
        examplePipeline5.stall(examplePipeline5.capacity() - 1, 1, crush_bubbles);
        runCycle(examplePipeline5, &sched);
        EXPECT_EQUAL(examplePipeline5.numValid(), 2);
        EXPECT_TRUE(examplePipeline5.isLastValid());
        EXPECT_EQUAL(examplePipeline5[examplePipeline5.capacity() - 1], 1234);
        EXPECT_FALSE(examplePipeline5.isValid(i));
    }
    EXPECT_FALSE(examplePipeline5.isValid(0));
    EXPECT_FALSE(examplePipeline5.isValid(1));
    EXPECT_FALSE(examplePipeline5.isValid(2));
    EXPECT_TRUE (examplePipeline5.isValid(3));
    EXPECT_TRUE (examplePipeline5.isValid(4));


#ifndef TEST_MANUAL_UPDATE
    stwr_pipe.performOwnUpdates();
#endif
    std::cout << "Append stwr pipeline with data[=true]\n";
    EXPECT_NOTHROW(stwr_pipe.append(true));
    runCycle(stwr_pipe, &sched);
    runCycle(stwr_pipe, &sched);
    runCycle(stwr_pipe, &sched);
    runCycle(stwr_pipe, &sched);
    runCycle(stwr_pipe, &sched);
    stwr_pipe.stall(4, 1, true);
    EXPECT_NOTHROW(stwr_pipe.writeStage(0, false));
    runCycle(stwr_pipe, &sched);
    EXPECT_TRUE(!stwr_pipe.isValid(0));
    EXPECT_NOTHROW(stwr_pipe.writeStage(0, true));

    // drain pipe
    for (uint32_t i = 0; i < stwr_pipe.capacity(); ++i)
        runCycle(stwr_pipe, &sched);

    std::cout << "[FINISH] Pipeline Stall/Restart Handling Test" << std::endl;


    ////////////////////////////////////////////////////////////////////////////////
    // Pipeline Flush Handling Test
    ////////////////////////////////////////////////////////////////////////////////

    std::cout << "\n[START] Pipeline Flush Handling Test\n";

#ifndef TEST_MANUAL_UPDATE
    examplePipeline6.performOwnUpdates();
#endif

    cyc_cnt = 0;


    std::cout << "Append pipeline with data[=1000]\n";
    EXPECT_NOTHROW(examplePipeline6.append(1000));

    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline6, &sched);
    EXPECT_EQUAL(examplePipeline6.numValid(), 1);
    EXPECT_TRUE(examplePipeline6.isValid(0));


    std::cout << "Append pipeline with data[=2000]\n";
    EXPECT_NOTHROW(examplePipeline6.append(2000));

    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline6, &sched);
    EXPECT_EQUAL(examplePipeline6.numValid(), 2);
    EXPECT_TRUE(examplePipeline6.isValid(0));
    EXPECT_TRUE(examplePipeline6.isValid(1));


    std::cout << "Append pipeline with data[=3000]\n";
    EXPECT_NOTHROW(examplePipeline6.append(3000));

    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline6, &sched);
    EXPECT_EQUAL(examplePipeline6.numValid(), 3);
    EXPECT_TRUE(examplePipeline6.isValid(0));
    EXPECT_TRUE(examplePipeline6.isValid(1));
    EXPECT_TRUE(examplePipeline6.isValid(2));


    // Test flushing all pipeline stages
    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
#ifdef TEST_MANUAL_UPDATE
    examplePipeline6.update();
    EXPECT_NOTHROW(ev_flush_all.schedule(sparta::Clock::Cycle(0)));
    sched.run(1, true);
#else
    EXPECT_NOTHROW(ev_flush_all.schedule(sparta::Clock::Cycle(1)));
    sched.run(1, true);
#endif
    EXPECT_EQUAL(examplePipeline6.numValid(), 0);


    std::cout << "Append pipeline with data[=1000]\n";
    EXPECT_NOTHROW(examplePipeline6.append(1000));

    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline6, &sched);
    EXPECT_EQUAL(examplePipeline6.numValid(), 1);
    EXPECT_TRUE(examplePipeline6.isValid(0));


    std::cout << "Append pipeline with data[=2000]\n";
    EXPECT_NOTHROW(examplePipeline6.append(2000));

    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline6, &sched);
    EXPECT_EQUAL(examplePipeline6.numValid(), 2);
    EXPECT_TRUE(examplePipeline6.isValid(0));
    EXPECT_TRUE(examplePipeline6.isValid(1));


    std::cout << "Append pipeline with data[=3000]\n";
    EXPECT_NOTHROW(examplePipeline6.append(3000));

    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline6, &sched);
    EXPECT_EQUAL(examplePipeline6.numValid(), 3);
    EXPECT_TRUE(examplePipeline6.isValid(0));
    EXPECT_TRUE(examplePipeline6.isValid(1));
    EXPECT_TRUE(examplePipeline6.isValid(2));


    std::cout << "Stall stage[1] for 2 cycles\n";
    EXPECT_NOTHROW(examplePipeline6.stall(1, 2));

    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline6, &sched);
    EXPECT_EQUAL(examplePipeline6.numValid(), 3);
    EXPECT_TRUE(examplePipeline6.isValid(0));
    EXPECT_TRUE(examplePipeline6.isValid(1));
    EXPECT_TRUE(examplePipeline6.isValid(3));


    // Test flushing pipeline stage before stall-causing stage;
    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
#ifdef TEST_MANUAL_UPDATE
    examplePipeline6.update();
    EXPECT_NOTHROW(ev_flush_one.preparePayload(0)->schedule(sparta::Clock::Cycle(0)));
    sched.run(1, true);
#else
    EXPECT_NOTHROW(ev_flush_one.preparePayload(0)->schedule(sparta::Clock::Cycle(1)));
    sched.run(1, true);
#endif
    EXPECT_EQUAL(examplePipeline6.numValid(), 2);
    EXPECT_FALSE(examplePipeline6.isValid(0));
    EXPECT_TRUE(examplePipeline6.isValid(1));
    EXPECT_TRUE(examplePipeline6.isValid(4));


    std::cout << "Append pipeline with data[=1000]\n";
    EXPECT_NOTHROW(examplePipeline6.append(1000));

    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline6, &sched);
    EXPECT_EQUAL(examplePipeline6.numValid(), 2);
    EXPECT_TRUE(examplePipeline6.isValid(0));
    EXPECT_TRUE(examplePipeline6.isValid(2));


    std::cout << "Append pipeline with data[=2000]\n";
    EXPECT_NOTHROW(examplePipeline6.append(2000));

    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline6, &sched);
    EXPECT_EQUAL(examplePipeline6.numValid(), 3);
    EXPECT_TRUE(examplePipeline6.isValid(0));
    EXPECT_TRUE(examplePipeline6.isValid(1));
    EXPECT_TRUE(examplePipeline6.isValid(3));


    std::cout << "Stall stage[1] for 3 cycles\n";
    EXPECT_NOTHROW(examplePipeline6.stall(1, 3));

    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline6, &sched);
    EXPECT_EQUAL(examplePipeline6.numValid(), 3);
    EXPECT_TRUE(examplePipeline6.isValid(0));
    EXPECT_TRUE(examplePipeline6.isValid(1));
    EXPECT_TRUE(examplePipeline6.isValid(4));


    // Test flushing pipeline stage which is alsp a stall-causing stage
    // Expect the pipeline to restart next cycle
    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
#ifdef TEST_MANUAL_UPDATE
    examplePipeline6.update();
    EXPECT_NOTHROW(ev_flush_one.preparePayload(1)->schedule(sparta::Clock::Cycle(0)));
    sched.run(1, true);
#else
    EXPECT_NOTHROW(ev_flush_one.preparePayload(1)->schedule(sparta::Clock::Cycle(1)));
    sched.run(1, true);
#endif
    EXPECT_EQUAL(examplePipeline6.numValid(), 1);
    EXPECT_TRUE(examplePipeline6.isValid(0));
    EXPECT_FALSE(examplePipeline6.isValid(1));
    EXPECT_EQUAL(examplePipeline6[0], 2000);

    std::cout << "Append pipeline with data[=3000]\n";
    EXPECT_NOTHROW(examplePipeline6.append(3000));


    std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
    runCycle(examplePipeline6, &sched);
    EXPECT_EQUAL(examplePipeline6.numValid(), 2);
    EXPECT_TRUE(examplePipeline6.isValid(0));
    EXPECT_TRUE(examplePipeline6.isValid(1));
    EXPECT_EQUAL(examplePipeline6[0], 3000);
    EXPECT_EQUAL(examplePipeline6[1], 2000);


    while (examplePipeline6.numValid()) {
        std::cout << "Cycle[" << cyc_cnt++ << "]:\n";
        runCycle(examplePipeline6, &sched);
    }

    std::cout << "[FINISH] Pipeline Flush Handling Test\n" << std::endl;



    rtn.enterTeardown();

#ifdef PIPEOUT_GEN
    pc.destroy();
#endif

    // Returns error if one
    REPORT_ERROR;
    return ERROR_CODE;
}
