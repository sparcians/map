
/**
 * \file   Scoreboard_test.cpp
 * \brief  This is the testbench for sparta::Scoreboard.
 *
 * It is intended to show all the use cases of sparta::Scoreboard
 */

#include "sparta/resources/Scoreboard.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/sparta.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/events/UniqueEvent.hpp"

TEST_INIT;

enum Units
    {
        ALU0,
        ALU1,
        LSU,
        FPU,
        TST_NUM_UNITS
    };

const char * UNIT_NAMES [TST_NUM_UNITS] =
    {
        "ALU0",
        "ALU1",
        "LSU",
        "FPU"
    };

const char * SB_NAMES[2] =
    {
        "sb_integer",
        "sb_float"
    };

// Forwarding table for GPRs - entry [i][j] is the forwarding latency from
// unit i to unit j.

sparta::Scoreboard::ScoreboardParameters::LatencyMatrixParameterType GPR_FORWARDING_MATRIX =
    {   // FROM
        //  |
        //  V
        {""     ,"ALU0", "ALU1",   "LSU",   "FPU"}, // <- TO
        {"ALU0",    "0",    "5",     "1",     "3"},
        {"ALU1",   "10",    "0",     "1",     "3"},
        {"LSU",     "1",    "1",     "0",     "1"},
        {"FPU",     "3",    "3",     "1",     "0"}
    };


// A placeholder
class RenameUnit : public sparta::Unit
{
public:
    static constexpr char name[] = "Rename";

    class RenameUnitParameters : public sparta::ParameterSet
    {
    public:
        explicit RenameUnitParameters(sparta::TreeNode *n) :
            sparta::ParameterSet(n)
        { }
    };

    RenameUnit(sparta::TreeNode * n,
               const RenameUnitParameters * params) :
        sparta::Unit(n)
    {
        (void) params;
    }
};

class ExeUnit : public sparta::Unit
{
    struct InstructionToBeExecuted {

        InstructionToBeExecuted(ExeUnit * exeunit,
                                const sparta::Scoreboard::RegisterBitMask & my_consumer_bits,
                                const sparta::Scoreboard::RegisterBitMask & my_producer_bits) :
            exeunit_(exeunit),
            my_consumer_bits_(my_consumer_bits),
            my_producer_bits_(my_producer_bits)
        {}

        void instructionReady() {
            exeunit_->setInstructionReady(this);
        }

        void waitForReady(sparta::ScoreboardView * view) {
            if(!already_waiting_) {
                view->registerReadyCallback(my_consumer_bits_, 0,
                                            [this](const sparta::Scoreboard::RegisterBitMask&){ this->instructionReady(); });
                already_waiting_ = true;
            }
        }

        const sparta::Scoreboard::RegisterBitMask& getProducerBits() const {
            return my_producer_bits_;
        }

        const sparta::Scoreboard::RegisterBitMask& getConsumerBits() const {
            return my_consumer_bits_;
        }

    private:
        ExeUnit * exeunit_;
        bool already_waiting_ = false;
        sparta::Scoreboard::RegisterBitMask my_consumer_bits_;
        sparta::Scoreboard::RegisterBitMask my_producer_bits_;
    }
    // [0] is the producer, [1] is the consumer
        waiting_instruction_[2] = { {this, {0b1000}, {0b10000}}, {this, {0b11000}, {0b100000}}};

public:
    static constexpr char name[] = "ExeUnit";

    class ExeUnitParameters : public sparta::ParameterSet
    {
    public:
        explicit ExeUnitParameters(sparta::TreeNode *n) :
            sparta::ParameterSet(n)
        {
        }
        PARAMETER(std::string, sb_unit_type, "", "The unit type (integer, fp, vector)")
        PARAMETER(bool, producer, true, "True if this unit is the producer")
    };

    ExeUnit(sparta::TreeNode * n, const ExeUnitParameters * params) :
        sparta::Unit(n),
        sb_unit_type_(params->sb_unit_type),
        producer_(params->producer)
    {
        if(producer_) {
            advance_.reset(new sparta::UniqueEvent<>
                           (getEventSet(), "exe_advance_producer", CREATE_SPARTA_HANDLER(ExeUnit, advanceUnit<0>), 1));
        }
        else {
            advance_.reset(new sparta::UniqueEvent<>
                           (getEventSet(), "exe_advance_consumer", CREATE_SPARTA_HANDLER(ExeUnit, advanceUnit<1>), 1));
        }
        advance_->setContinuing(false); // don't keep the scheduler alive
        sparta::StartupEvent(n, CREATE_SPARTA_HANDLER(ExeUnit, setupScoreboards));
    }

    void setupScoreboards() {
        my_scoreboard_view_.reset
            (new sparta::ScoreboardView(getContainer()->getName(), // ALU0, ALU1, LSU, FPU, etc
                                        sb_unit_type_,             // integer, fp, vector
                                        getContainer()));          // Used to find the Scoreboard
        advance_->schedule();
    }

    bool checkBit(const sparta::Scoreboard::RegisterBitMask & bits) {
        return my_scoreboard_view_->isSet(bits);
    }

    void setInstructionReady(InstructionToBeExecuted * inst) {
        const auto is_set = my_scoreboard_view_->isSet(inst->getConsumerBits());
        EXPECT_TRUE(is_set);
        EXPECT_TRUE(time_ready_ == 0);
        time_ready_ = getClock()->currentCycle();
    }

    template<uint32_t idx>
    void advanceUnit() {
        if(const auto is_set = my_scoreboard_view_->isSet(waiting_instruction_[idx].getConsumerBits()); is_set) {
            // Instruction is ready -- "execute" it and propagate the producer bits
            my_scoreboard_view_->setReady(waiting_instruction_[idx].getProducerBits());
        }
        else {
            ++time_waiting_on_producer_;
            waiting_instruction_[idx].waitForReady(my_scoreboard_view_.get());
            advance_->schedule();
        }
    }

    uint32_t getTimeWaitingOnProducer() const {
        return time_waiting_on_producer_;
    }

    sparta::Clock::Cycle getTimeReady() const {
        return time_ready_;
    }

private:

    uint32_t time_waiting_on_producer_ = 0;
    sparta::Clock::Cycle time_ready_ = 0;

    std::unique_ptr<sparta::ScoreboardView> my_scoreboard_view_;
    const std::string sb_unit_type_;
    const bool producer_;
    std::unique_ptr<sparta::UniqueEvent<>> advance_;
};

void testLatencyTableSetting()
{
    sparta::RootTreeNode rtn;
    sparta::Scheduler sched;
    sparta::ClockManager cm(&sched);
    sparta::Clock::Handle root_clk;
    root_clk = cm.makeRoot(&rtn, "root_clk");
    cm.normalize();
    rtn.setClock(root_clk.get());

    sparta::ResourceFactory<sparta::Scoreboard,
                            sparta::Scoreboard::ScoreboardParameters> fact;

    sparta::ResourceTreeNode sbtn(&rtn,
                                  "int_sb",
                                  sparta::TreeNode::GROUP_NAME_NONE,
                                  sparta::TreeNode::GROUP_IDX_NONE,
                                  "Test scoreboard",
                                  &fact);

    sparta::Scoreboard::ScoreboardParameters * params =
        dynamic_cast<sparta::Scoreboard::ScoreboardParameters *>(sbtn.getParameterSet());

    std::cerr << "--- Expected ERRORS BEGIN ---" << std::endl;

    // It's empty
    std::string errors;
    EXPECT_FALSE(params->latency_matrix.validateDependencies(&sbtn, errors));

    // It's too small
    params->latency_matrix =
        sparta::Scoreboard::ScoreboardParameters::LatencyMatrixParameterType{{}};
    EXPECT_FALSE(params->latency_matrix.validateDependencies(&sbtn, errors));

    // Too simple
    params->latency_matrix =
        sparta::Scoreboard::ScoreboardParameters::LatencyMatrixParameterType
        {
            {"1", "2"},
            {"1", "2"}
        };
    EXPECT_FALSE(params->latency_matrix.validateDependencies(&sbtn, errors));

    // From/To's don't line up
    params->latency_matrix =
        sparta::Scoreboard::ScoreboardParameters::LatencyMatrixParameterType
        {
            {"",  "X", "Y"},
            {"Y", "1", "1"},
            {"X", "1", "1"}
        };
    EXPECT_FALSE(params->latency_matrix.validateDependencies(&sbtn, errors));

    // Missing column
    params->latency_matrix =
        sparta::Scoreboard::ScoreboardParameters::LatencyMatrixParameterType
        {
            {"",  "X", "Y"},
            {"Y", "1"},
            {"X", "1", "1"}
        };
    EXPECT_FALSE(params->latency_matrix.validateDependencies(&sbtn, errors));

    // Bad integer
    params->latency_matrix =
        sparta::Scoreboard::ScoreboardParameters::LatencyMatrixParameterType
        {
            {"",  "X", "Y"},
            {"X", "1", "f"},
            {"Y", "1", "t"}
        };
    EXPECT_FALSE(params->latency_matrix.validateDependencies(&sbtn, errors));

    std::cerr << "--- Expected ERRORS END ---" << std::endl;

    // Test a good one
    params->latency_matrix = GPR_FORWARDING_MATRIX;
    EXPECT_TRUE(params->latency_matrix.validateDependencies(&sbtn, errors));

    rtn.enterConfiguring();

    rtn.enterFinalized();

    rtn.enterTeardown();
}

void testScoreboardRegistration()
{
    sparta::RootTreeNode rtn;
    sparta::Scheduler    sched;
    sparta::ClockManager cm(&sched);
    sparta::Clock::Handle root_clk;
    root_clk = cm.makeRoot(&rtn, "root_clk");
    cm.normalize();
    rtn.setClock(root_clk.get());

    sparta::TreeNode cpu(&rtn, "core", "Dummy CPU");

    sparta::ResourceFactory<sparta::Scoreboard,
                            sparta::Scoreboard::ScoreboardParameters> fact;

    sparta::ResourceTreeNode sbtn(&cpu,
                                  SB_NAMES[0],
                                  sparta::TreeNode::GROUP_NAME_NONE,
                                  sparta::TreeNode::GROUP_IDX_NONE,
                                  "Test scoreboard",
                                  &fact);

    sparta::Scoreboard::ScoreboardParameters * params =
        dynamic_cast<sparta::Scoreboard::ScoreboardParameters *>(sbtn.getParameterSet());
    params->latency_matrix = GPR_FORWARDING_MATRIX;

    rtn.enterConfiguring();
    rtn.enterFinalized();
    sched.finalize();

    // view from ALU0, integer
    sparta::ScoreboardView view(UNIT_NAMES[0], SB_NAMES[0], &sbtn);
    bool ready = false;
    auto ready_callback = [&ready](const sparta::Scoreboard::RegisterBitMask &) { ready = true; };

    sparta::Scoreboard::RegisterBitMask srcs;
    srcs[53] = true;
    srcs[54] = true;
    srcs[55] = true;

    view.registerReadyCallback(srcs, 0, ready_callback);
    EXPECT_FALSE(ready);

    // Set bit 53 as ready, leave 54 and 55 as not ready
    // The "instruction" should not be ready -- bits 54,
    // 55 are still not ready.
    sparta::Scoreboard::RegisterBitMask ready_bits;
    ready_bits[53] = true;
    view.setReady(ready_bits);
    ready_bits[53] = false;

    sched.run(100);
    const auto is_set_first = view.isSet(srcs);
    EXPECT_FALSE(is_set_first);
    EXPECT_FALSE(ready);

    // Set bit 54 as ready, leave 55 as not ready. The
    // "instruction" should not be ready -- bits 55 is
    // still not ready.
    ready_bits[54] = true;
    view.setReady(ready_bits);
    ready_bits[54] = false;

    sched.run(100);
    const auto is_set_second = view.isSet(srcs);
    EXPECT_FALSE(is_set_second);
    EXPECT_FALSE(ready);

    // Set bit 55 as ready, instruction should be ready!
    ready_bits[55] = true;
    view.setReady(ready_bits);
    ready_bits[55] = false;

    sched.run(100);
    const auto is_set_third = view.isSet(srcs);
    EXPECT_TRUE(is_set_third);
    EXPECT_TRUE(ready);

    rtn.enterTeardown();
}

void testScoreboardUnitCreation()
{
    sparta::RootTreeNode  rtn;
    sparta::Scheduler     scheduler;
    sparta::ClockManager  cm(&scheduler);
    sparta::Clock::Handle root_clk;
    root_clk = cm.makeRoot(&rtn, "root_clk");
    cm.normalize();
    rtn.setClock(root_clk.get());

    sparta::TreeNode cpu(&rtn, "core", "Dummy CPU");

    using SBResourceFactory      = sparta::ResourceFactory<sparta::Scoreboard,
                                                           sparta::Scoreboard::ScoreboardParameters>;
    using RenameResourceFactory  = sparta::ResourceFactory<RenameUnit,
                                                          RenameUnit::RenameUnitParameters>;
    using ExeUnitResourceFactory = sparta::ResourceFactory<ExeUnit, ExeUnit::ExeUnitParameters>;

    SBResourceFactory      sb_fact;
    RenameResourceFactory  rename_fact;
    ExeUnitResourceFactory exe_unit_fact;

    sparta::ResourceTreeNode rename(&cpu, "rename",
                                    sparta::TreeNode::GROUP_NAME_NONE,
                                    sparta::TreeNode::GROUP_IDX_NONE,
                                    "Test Rename",
                                    &rename_fact);

    sparta::ResourceTreeNode sbtn(&rename,
                                  SB_NAMES[0],
                                  sparta::TreeNode::GROUP_NAME_NONE,
                                  sparta::TreeNode::GROUP_IDX_NONE,
                                  "Test scoreboard",
                                  &sb_fact);

    auto * sb_params =
        dynamic_cast<sparta::Scoreboard::ScoreboardParameters *>(sbtn.getParameterSet());
    sb_params->latency_matrix = GPR_FORWARDING_MATRIX;

    sparta::ResourceTreeNode exeunit(&cpu, UNIT_NAMES[0], // ALU0
                                     sparta::TreeNode::GROUP_NAME_NONE,
                                     sparta::TreeNode::GROUP_IDX_NONE,
                                     "Test exeunit",
                                     &exe_unit_fact);

    auto * exe_params = dynamic_cast<ExeUnit::ExeUnitParameters *>(exeunit.getParameterSet());
    exe_params->sb_unit_type = SB_NAMES[0];

    sparta::ResourceTreeNode exeunit2(&cpu, UNIT_NAMES[1], // ALU1
                                     sparta::TreeNode::GROUP_NAME_NONE,
                                     sparta::TreeNode::GROUP_IDX_NONE,
                                     "Test exeunit",
                                     &exe_unit_fact);

    auto * exe_params2 = dynamic_cast<ExeUnit::ExeUnitParameters *>(exeunit2.getParameterSet());
    exe_params2->sb_unit_type = SB_NAMES[0];
    exe_params2->producer = false;

    rtn.enterConfiguring();
    rtn.enterFinalized();
    scheduler.finalize();

    // run 1 tick exactly
    constexpr bool exacting_run = true;
    constexpr bool measure_run_time = false;
    scheduler.run(1, exacting_run, measure_run_time);

    sparta::Scoreboard * master_sb = sbtn.getResourceAs<sparta::Scoreboard>();
    ExeUnit * alu0 = exeunit.getResourceAs<ExeUnit*>();
    ExeUnit * alu1 = exeunit2.getResourceAs<ExeUnit*>();

    // Since the SB's initial values are ready for the arch registers
    // (32 of them), for this test, we clear them on purpose
    master_sb->clearBits({0xFFFFFFFF});

    // Test setting of the scoreboard
    uint64_t sb_bit = 0b0001;
    master_sb->set({sb_bit});  // Should set the bit immediately and propagate
    EXPECT_TRUE(alu0->checkBit({sb_bit}));
    EXPECT_TRUE(alu1->checkBit({sb_bit}));

    sb_bit |= 0b0010;
    master_sb->set({sb_bit});  // Should set the bit immediately and propagate
    EXPECT_TRUE(alu0->checkBit({sb_bit}));
    EXPECT_TRUE(alu1->checkBit({sb_bit}));

    uint64_t sb_bit3 = 0b0100;
    EXPECT_FALSE(alu0->checkBit({sb_bit3}));
    EXPECT_FALSE(alu1->checkBit({sb_bit3}));

    // Run the kernel to get the exe units executing
    scheduler.run(10, exacting_run, measure_run_time);

    // Both units are blocked waiting on the first instruction (in
    // ALU0) to get its consumer operands
    EXPECT_EQUAL(alu0->getTimeWaitingOnProducer(), 10);
    EXPECT_EQUAL(alu1->getTimeWaitingOnProducer(), 10);

    // This should get the producer instruction rolling in ALU0
    master_sb->set({0b1000});
    scheduler.run(1, exacting_run, measure_run_time);
    EXPECT_EQUAL(alu0->getTimeReady(), 11); // 12 ticks/cycles have elapsed (0 -> 11)

    EXPECT_EQUAL(alu0->getTimeWaitingOnProducer(), 10);
    EXPECT_EQUAL(alu1->getTimeWaitingOnProducer(), 11);

    // The GPR forwarding matrix has ALU0 -> ALU1 producer to consumer
    // as 5 cycles.  The rest of the test relies on that.
    assert(GPR_FORWARDING_MATRIX[1][2] == std::string("5"));

    for(uint32_t i = 0; i < 4; ++i) {
        scheduler.run(1, exacting_run, measure_run_time);
        EXPECT_EQUAL(alu0->getTimeWaitingOnProducer(), 10);
        EXPECT_EQUAL(alu1->getTimeWaitingOnProducer(), 12 + i);
    }

    EXPECT_EQUAL(alu1->getTimeReady(), 0);
    scheduler.run(1, exacting_run, measure_run_time);

    // On cycle 12, ALU0 setReady for ALU1; 5 cycle delay + 12 == 17 (tick 0 -> 16)
    EXPECT_EQUAL(alu1->getTimeReady(), 16);

    // Nothing else should happen
    scheduler.run();

    rtn.enterTeardown();
}

void testScoreboardClearing()
{
    sparta::RootTreeNode rtn;
    sparta::Scheduler    sched;
    sparta::ClockManager cm(&sched);
    sparta::Clock::Handle root_clk;
    root_clk = cm.makeRoot(&rtn, "root_clk");
    cm.normalize();
    rtn.setClock(root_clk.get());

    sparta::TreeNode cpu(&rtn, "core", "Dummy CPU");

    sparta::ResourceFactory<sparta::Scoreboard,
                            sparta::Scoreboard::ScoreboardParameters> fact;

    sparta::ResourceTreeNode sbtn(&cpu,
                                  SB_NAMES[0],
                                  sparta::TreeNode::GROUP_NAME_NONE,
                                  sparta::TreeNode::GROUP_IDX_NONE,
                                  "Test scoreboard",
                                  &fact);

    sparta::Scoreboard::ScoreboardParameters * params =
        dynamic_cast<sparta::Scoreboard::ScoreboardParameters *>(sbtn.getParameterSet());
    params->latency_matrix = GPR_FORWARDING_MATRIX;

    rtn.enterConfiguring();
    rtn.enterFinalized();
    sparta::Scoreboard * master_sb = sbtn.getResourceAs<sparta::Scoreboard>();
    sparta::ScoreboardView view(UNIT_NAMES[0], SB_NAMES[0], &sbtn);

    auto is_set = view.isSet({0b1000});
    EXPECT_TRUE(is_set);
    master_sb->clearBits({0xFFFFFFFF});

    is_set = view.isSet({0b1000});
    EXPECT_FALSE(is_set);

    master_sb->set({0b1000});
    is_set = view.isSet({0b1000});
    EXPECT_TRUE(is_set);

    master_sb->set({0b1100});
    master_sb->set({0b1000});
    is_set = view.isSet({0b1100});
    EXPECT_TRUE(is_set);

    master_sb->set({0b11});
    master_sb->set({0b1000});
    is_set = view.isSet({0b1111});
    EXPECT_TRUE(is_set);

    master_sb->clearBits({0b0100});
    master_sb->set({0b1000});
    is_set = view.isSet({0b0100});
    EXPECT_FALSE(is_set);

    master_sb->clearBits({0b0100});
    master_sb->set({0b1000});
    is_set = view.isSet({0b0100});
    EXPECT_FALSE(is_set);

    rtn.enterTeardown();
}

void testPrintBits()
{
    sparta::Scoreboard::RegisterBitMask some_bits(0b011000110011);
    const std::string some_bits_expected = sparta::printBitSet(some_bits);
    EXPECT_EQUAL(some_bits_expected, "[0-1,4-5,9-10]");
}


int main ()
{
    testLatencyTableSetting();

    testScoreboardRegistration();

    testScoreboardUnitCreation();

    testScoreboardClearing();

    testPrintBits();

    REPORT_ERROR;
    return ERROR_CODE;
}
