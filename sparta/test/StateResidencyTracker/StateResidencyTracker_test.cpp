#include <iostream>
#include <inttypes.h>
#include <memory>
#include "sparta/sparta.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/events/Event.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/PayloadEvent.hpp"
#include "sparta/simulation/State.hpp"
#include "sparta/utils/Enum.hpp"
#include "sparta/utils/SpartaTester.hpp"

TEST_INIT;

// Dummy enum class OperandState
enum class OperandState : uint32_t {
    OPER_INIT = 0,
    __FIRST = OPER_INIT,
    OPER_READY,
    OPER_WAIT,
    OPER_RETIRE,
    __LAST
};

// Dummy op class which contains a sparta::State
// which we will be tracking.
class Operand {
private:
    typedef sparta::State<OperandState> StateType;
    const std::string name_;
    StateType state_;

public:
    Operand(const std::string& name) :
        name_(name),
        state_(OperandState::OPER_INIT) {}

    void reset() {
        state_.setValue(OperandState::OPER_INIT);
    }

    const StateType& getState() const {
        return state_;
    }

    const OperandState & getCurrentState() const {
        return state_.getEnumValue();
    }

    void setOperandState(const OperandState& state) {
        state_.setValue(state);
    }

    sparta::Scheduler::Tick getTimeDuration() const {
        return state_.getTimeInState();
    }
};

// Dummy enum class UopState
enum class UopState : uint64_t {
    UOP_INIT = 0,
    __FIRST = UOP_INIT,
    UOP_READY,
    UOP_WAIT,
    UOP_RETIRE,
    __LAST
};

// Dummy op class which contains a sparta::State
// which we will be tracking.
class Uop {
private:
    typedef sparta::State<UopState> StateType;
    const std::string name_;
    StateType state_;

public:
    Uop(const std::string& name) :
        name_(name),
        state_(UopState::UOP_INIT) {}

    void reset() {
        state_.setValue(UopState::UOP_INIT);
    }

    const StateType& getState() const {
        return state_;
    }

    const UopState & getCurrentState() const {
        return state_.getEnumValue();
    }

    void setUopState(const UopState& state) {
        state_.setValue(state);
    }

    sparta::Scheduler::Tick getTimeDuration() const {
        return state_.getTimeInState();
    }
};

int main()
{
    sparta::Scheduler sched;
    EXPECT_TRUE(sched.getCurrentTick() == 1);
    EXPECT_TRUE(sched.isRunning() == false);
    EXPECT_TRUE(sched.getElapsedTicks() == 0);
    sched.finalize();

    sparta::tracker::StatePoolManager::getInstance().enableTracking();
    sparta::tracker::StatePoolManager::getInstance().setScheduler(&sched);

    Operand observer_1("Foo");
    Uop     observer_2("Bar");

    sched.run(10, true, false);
    EXPECT_EQUAL(sched.getCurrentTick(), 11);
    EXPECT_EQUAL(sched.getElapsedTicks(), 10);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_INIT);
    EXPECT_EQUAL(observer_1.getTimeDuration(), 10);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_INIT);
    EXPECT_EQUAL(observer_2.getTimeDuration(), 10);

    observer_1.setOperandState(OperandState::OPER_READY);
    observer_2.setUopState(UopState::UOP_READY);

    sched.run(5, true, false);
    EXPECT_EQUAL(sched.getCurrentTick(), 16);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_READY);
    EXPECT_EQUAL(observer_1.getTimeDuration(), 5);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_READY);
    EXPECT_EQUAL(observer_2.getTimeDuration(), 5);

    observer_1.setOperandState(OperandState::OPER_WAIT);
    observer_2.setUopState(UopState::UOP_WAIT);

    sched.run(17, true, false);
    EXPECT_EQUAL(sched.getCurrentTick(), 33);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_WAIT);
    EXPECT_EQUAL(observer_1.getTimeDuration(), 17);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_WAIT);
    EXPECT_EQUAL(observer_2.getTimeDuration(), 17);

    observer_1.setOperandState(OperandState::OPER_RETIRE);
    observer_2.setUopState(UopState::UOP_RETIRE);

    sched.run(2, true, false);
    EXPECT_EQUAL(sched.getCurrentTick(), 35);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_RETIRE);
    EXPECT_EQUAL(observer_1.getTimeDuration(), 2);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_RETIRE);
    EXPECT_EQUAL(observer_2.getTimeDuration(), 2);

    observer_1.reset();
    observer_2.reset();

    EXPECT_EQUAL(sched.getCurrentTick(), 35);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_INIT);
    EXPECT_EQUAL(observer_1.getTimeDuration(), 0);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_INIT);
    EXPECT_EQUAL(observer_2.getTimeDuration(), 0);

    {
        const std::vector<sparta::Scheduler::Tick> agg_vec = {10, 5, 17, 2, 0};
        EXPECT_EQUAL(observer_1.getState().getRawAccumulatedTime(), agg_vec);
        EXPECT_EQUAL(observer_2.getState().getRawAccumulatedTime(), agg_vec);
    }

    sched.run(24, true, false);
    EXPECT_EQUAL(sched.getCurrentTick(), 59);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_INIT);
    EXPECT_EQUAL(observer_1.getTimeDuration(), 24);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_INIT);
    EXPECT_EQUAL(observer_2.getTimeDuration(), 24);

    observer_1.setOperandState(OperandState::OPER_WAIT);
    observer_2.setUopState(UopState::UOP_WAIT);

    sched.run(1, true, false);
    EXPECT_EQUAL(sched.getCurrentTick(), 60);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_WAIT);
    EXPECT_EQUAL(observer_1.getTimeDuration(), 1);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_WAIT);
    EXPECT_EQUAL(observer_2.getTimeDuration(), 1);

    observer_1.setOperandState(OperandState::OPER_RETIRE);
    observer_2.setUopState(UopState::UOP_RETIRE);

    sched.run(47, true, false);
    EXPECT_EQUAL(sched.getCurrentTick(), 107);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_RETIRE);
    EXPECT_EQUAL(observer_1.getTimeDuration(), 47);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_RETIRE);
    EXPECT_EQUAL(observer_2.getTimeDuration(), 47);

    observer_1.setOperandState(OperandState::OPER_READY);
    observer_2.setUopState(UopState::UOP_READY);

    sched.run(1, true, false);
    EXPECT_EQUAL(sched.getCurrentTick(), 108);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_READY);
    EXPECT_EQUAL(observer_1.getTimeDuration(), 1);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_READY);
    EXPECT_EQUAL(observer_2.getTimeDuration(), 1);

    observer_1.reset();
    observer_2.reset();

    EXPECT_EQUAL(sched.getCurrentTick(), 108);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_INIT);
    EXPECT_EQUAL(observer_1.getTimeDuration(), 0);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_INIT);
    EXPECT_EQUAL(observer_2.getTimeDuration(), 0);

    {
        const std::vector<sparta::Scheduler::Tick> agg_vec = {34, 6, 18, 49, 0};
        EXPECT_EQUAL(observer_1.getState().getRawAccumulatedTime(), agg_vec);
        EXPECT_EQUAL(observer_2.getState().getRawAccumulatedTime(), agg_vec);
    }

    sched.run(603, true, false);
    EXPECT_EQUAL(sched.getCurrentTick(), 711);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_INIT);
    EXPECT_EQUAL(observer_1.getTimeDuration(), 603);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_INIT);
    EXPECT_EQUAL(observer_2.getTimeDuration(), 603);

    observer_1.setOperandState(OperandState::OPER_WAIT);
    observer_2.setUopState(UopState::UOP_WAIT);

    sched.run(11, true, false);
    EXPECT_EQUAL(sched.getCurrentTick(), 722);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_WAIT);
    EXPECT_EQUAL(observer_1.getTimeDuration(), 11);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_WAIT);
    EXPECT_EQUAL(observer_2.getTimeDuration(), 11);

    observer_1.setOperandState(OperandState::OPER_RETIRE);
    observer_2.setUopState(UopState::UOP_RETIRE);

    sched.run(201, true, false);
    EXPECT_EQUAL(sched.getCurrentTick(), 923);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_RETIRE);
    EXPECT_EQUAL(observer_1.getTimeDuration(), 201);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_RETIRE);
    EXPECT_EQUAL(observer_2.getTimeDuration(), 201);

    observer_1.setOperandState(OperandState::OPER_READY);
    observer_2.setUopState(UopState::UOP_READY);

    sched.run(99, true, false);
    EXPECT_EQUAL(sched.getCurrentTick(), 1022);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_READY);
    EXPECT_EQUAL(observer_1.getTimeDuration(), 99);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_READY);
    EXPECT_EQUAL(observer_2.getTimeDuration(), 99);

    observer_1.reset();
    observer_2.reset();

    EXPECT_EQUAL(sched.getCurrentTick(), 1022);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_INIT);
    EXPECT_EQUAL(observer_1.getTimeDuration(), 0);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_INIT);
    EXPECT_EQUAL(observer_2.getTimeDuration(), 0);

    {
        const std::vector<sparta::Scheduler::Tick> agg_vec = {637, 105, 29, 250, 0};
        EXPECT_EQUAL(observer_1.getState().getRawAccumulatedTime(), agg_vec);
        EXPECT_EQUAL(observer_2.getState().getRawAccumulatedTime(), agg_vec);
    }

    sched.run(78, true, false);
    EXPECT_EQUAL(sched.getCurrentTick(), 1100);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_INIT);
    EXPECT_EQUAL(observer_1.getTimeDuration(), 78);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_INIT);
    EXPECT_EQUAL(observer_2.getTimeDuration(), 78);

    observer_1.setOperandState(OperandState::OPER_WAIT);
    observer_2.setUopState(UopState::UOP_WAIT);

    sched.run(1, true, false);
    EXPECT_EQUAL(sched.getCurrentTick(), 1101);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_WAIT);
    EXPECT_EQUAL(observer_1.getTimeDuration(), 1);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_WAIT);
    EXPECT_EQUAL(observer_2.getTimeDuration(), 1);

    observer_1.setOperandState(OperandState::OPER_RETIRE);
    observer_2.setUopState(UopState::UOP_RETIRE);

    sched.run(39, true, false);
    EXPECT_EQUAL(sched.getCurrentTick(), 1140);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_RETIRE);
    EXPECT_EQUAL(observer_1.getTimeDuration(), 39);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_RETIRE);
    EXPECT_EQUAL(observer_2.getTimeDuration(), 39);

    observer_1.setOperandState(OperandState::OPER_READY);
    observer_2.setUopState(UopState::UOP_READY);

    sched.run(2, true, false);
    EXPECT_EQUAL(sched.getCurrentTick(), 1142);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_READY);
    EXPECT_EQUAL(observer_1.getTimeDuration(), 2);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_READY);
    EXPECT_EQUAL(observer_2.getTimeDuration(), 2);

    observer_1.reset();
    observer_2.reset();

    EXPECT_EQUAL(sched.getCurrentTick(), 1142);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_INIT);
    EXPECT_EQUAL(observer_1.getTimeDuration(), 0);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_INIT);
    EXPECT_EQUAL(observer_2.getTimeDuration(), 0);

    {
        const std::vector<sparta::Scheduler::Tick> agg_vec = {715, 107, 30, 289, 0};
        EXPECT_EQUAL(observer_1.getState().getRawAccumulatedTime(), agg_vec);
        EXPECT_EQUAL(observer_2.getState().getRawAccumulatedTime(), agg_vec);
    }

    sched.run(909, true, false);
    EXPECT_EQUAL(sched.getCurrentTick(), 2051);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_INIT);
    EXPECT_EQUAL(observer_1.getTimeDuration(), 909);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_INIT);
    EXPECT_EQUAL(observer_2.getTimeDuration(), 909);

    observer_1.setOperandState(OperandState::OPER_WAIT);
    observer_2.setUopState(UopState::UOP_WAIT);

    sched.run(17, true, false);
    EXPECT_EQUAL(sched.getCurrentTick(), 2068);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_WAIT);
    EXPECT_EQUAL(observer_1.getTimeDuration(), 17);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_WAIT);
    EXPECT_EQUAL(observer_2.getTimeDuration(), 17);

    observer_1.setOperandState(OperandState::OPER_RETIRE);
    observer_2.setUopState(UopState::UOP_RETIRE);

    sched.run(63, true, false);
    EXPECT_EQUAL(sched.getCurrentTick(), 2131);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_RETIRE);
    EXPECT_EQUAL(observer_1.getTimeDuration(), 63);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_RETIRE);
    EXPECT_EQUAL(observer_2.getTimeDuration(), 63);

    observer_1.setOperandState(OperandState::OPER_READY);
    observer_2.setUopState(UopState::UOP_READY);

    sched.run(4, true, false);
    EXPECT_EQUAL(sched.getCurrentTick(), 2135);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_READY);
    EXPECT_EQUAL(observer_1.getTimeDuration(), 4);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_READY);
    EXPECT_EQUAL(observer_2.getTimeDuration(), 4);

    observer_1.reset();
    observer_2.reset();

    EXPECT_EQUAL(sched.getCurrentTick(), 2135);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_INIT);
    EXPECT_EQUAL(observer_1.getTimeDuration(), 0);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_INIT);
    EXPECT_EQUAL(observer_2.getTimeDuration(), 0);

    {
        const std::vector<sparta::Scheduler::Tick> agg_vec = {1624, 111, 47, 352, 0};
        EXPECT_EQUAL(observer_1.getState().getRawAccumulatedTime(), agg_vec);
        EXPECT_EQUAL(observer_2.getState().getRawAccumulatedTime(), agg_vec);
    }

    REPORT_ERROR
    return ERROR_CODE;
}
