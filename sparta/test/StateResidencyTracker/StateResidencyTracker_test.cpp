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
        return state_.getSpan();
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
        return state_.getSpan();
    }
};

int main()
{
    sparta::Scheduler sched;
    EXPECT_TRUE(sched.getCurrentTick() == 0);
    EXPECT_TRUE(sched.isRunning() == false);
    EXPECT_TRUE(sched.getElapsedTicks() == 0);
    sched.finalize();

    sparta::tracker::StatePoolManager::getInstance().enableTracking();

    Operand observer_1("Foo");
    Uop observer_2("Bar");

    sched.run(10, true);
    EXPECT_TRUE(sched.getCurrentTick() == 9);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_INIT);
    EXPECT_TRUE(observer_1.getTimeDuration() == 9);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_INIT);
    EXPECT_TRUE(observer_2.getTimeDuration() == 9);

    observer_1.setOperandState(OperandState::OPER_READY);
    observer_2.setUopState(UopState::UOP_READY);

    sched.run(5, true);
    EXPECT_TRUE(sched.getCurrentTick() == 14);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_READY);
    EXPECT_TRUE(observer_1.getTimeDuration() == 5);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_READY);
    EXPECT_TRUE(observer_2.getTimeDuration() == 5);

    observer_1.setOperandState(OperandState::OPER_WAIT);
    observer_2.setUopState(UopState::UOP_WAIT);

    sched.run(17, true);
    EXPECT_TRUE(sched.getCurrentTick() == 31);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_WAIT);
    EXPECT_TRUE(observer_1.getTimeDuration() == 17);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_WAIT);
    EXPECT_TRUE(observer_2.getTimeDuration() == 17);

    observer_1.setOperandState(OperandState::OPER_RETIRE);
    observer_2.setUopState(UopState::UOP_RETIRE);

    sched.run(2, true);
    EXPECT_TRUE(sched.getCurrentTick() == 33);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_RETIRE);
    EXPECT_TRUE(observer_1.getTimeDuration() == 2);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_RETIRE);
    EXPECT_TRUE(observer_2.getTimeDuration() == 2);

    observer_1.reset();
    observer_2.reset();

    EXPECT_TRUE(sched.getCurrentTick() == 33);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_INIT);
    EXPECT_TRUE(observer_1.getTimeDuration() == 0);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_INIT);
    EXPECT_TRUE(observer_2.getTimeDuration() == 0);

    {
        const std::vector<sparta::Scheduler::Tick> agg_vec = {9, 5, 17, 2, 0};
        EXPECT_TRUE(observer_1.getState().getRawData() == agg_vec);
        EXPECT_TRUE(observer_2.getState().getRawData() == agg_vec);
    }

    sched.run(24, true);
    EXPECT_TRUE(sched.getCurrentTick() == 57);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_INIT);
    EXPECT_TRUE(observer_1.getTimeDuration() == 24);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_INIT);
    EXPECT_TRUE(observer_2.getTimeDuration() == 24);

    observer_1.setOperandState(OperandState::OPER_WAIT);
    observer_2.setUopState(UopState::UOP_WAIT);

    sched.run(1, true);
    EXPECT_TRUE(sched.getCurrentTick() == 58);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_WAIT);
    EXPECT_TRUE(observer_1.getTimeDuration() == 1);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_WAIT);
    EXPECT_TRUE(observer_2.getTimeDuration() == 1);

    observer_1.setOperandState(OperandState::OPER_RETIRE);
    observer_2.setUopState(UopState::UOP_RETIRE);

    sched.run(47, true);
    EXPECT_TRUE(sched.getCurrentTick() == 105);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_RETIRE);
    EXPECT_TRUE(observer_1.getTimeDuration() == 47);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_RETIRE);
    EXPECT_TRUE(observer_2.getTimeDuration() == 47);

    observer_1.setOperandState(OperandState::OPER_READY);
    observer_2.setUopState(UopState::UOP_READY);

    sched.run(1, true);
    EXPECT_TRUE(sched.getCurrentTick() == 106);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_READY);
    EXPECT_TRUE(observer_1.getTimeDuration() == 1);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_READY);
    EXPECT_TRUE(observer_2.getTimeDuration() == 1);

    observer_1.reset();
    observer_2.reset();

    EXPECT_TRUE(sched.getCurrentTick() == 106);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_INIT);
    EXPECT_TRUE(observer_1.getTimeDuration() == 0);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_INIT);
    EXPECT_TRUE(observer_2.getTimeDuration() == 0);

    {
        const std::vector<sparta::Scheduler::Tick> agg_vec = {33, 6, 18, 49, 0};
        EXPECT_TRUE(observer_1.getState().getRawData() == agg_vec);
        EXPECT_TRUE(observer_2.getState().getRawData() == agg_vec);
    }

    sched.run(603, true);
    EXPECT_TRUE(sched.getCurrentTick() == 709);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_INIT);
    EXPECT_TRUE(observer_1.getTimeDuration() == 603);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_INIT);
    EXPECT_TRUE(observer_2.getTimeDuration() == 603);

    observer_1.setOperandState(OperandState::OPER_WAIT);
    observer_2.setUopState(UopState::UOP_WAIT);

    sched.run(11, true);
    EXPECT_TRUE(sched.getCurrentTick() == 720);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_WAIT);
    EXPECT_TRUE(observer_1.getTimeDuration() == 11);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_WAIT);
    EXPECT_TRUE(observer_2.getTimeDuration() == 11);

    observer_1.setOperandState(OperandState::OPER_RETIRE);
    observer_2.setUopState(UopState::UOP_RETIRE);

    sched.run(201, true);
    EXPECT_TRUE(sched.getCurrentTick() == 921);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_RETIRE);
    EXPECT_TRUE(observer_1.getTimeDuration() == 201);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_RETIRE);
    EXPECT_TRUE(observer_2.getTimeDuration() == 201);

    observer_1.setOperandState(OperandState::OPER_READY);
    observer_2.setUopState(UopState::UOP_READY);

    sched.run(99, true);
    EXPECT_TRUE(sched.getCurrentTick() == 1020);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_READY);
    EXPECT_TRUE(observer_1.getTimeDuration() == 99);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_READY);
    EXPECT_TRUE(observer_2.getTimeDuration() == 99);

    observer_1.reset();
    observer_2.reset();

    EXPECT_TRUE(sched.getCurrentTick() == 1020);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_INIT);
    EXPECT_TRUE(observer_1.getTimeDuration() == 0);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_INIT);
    EXPECT_TRUE(observer_2.getTimeDuration() == 0);

    {
        const std::vector<sparta::Scheduler::Tick> agg_vec = {636, 105, 29, 250, 0};
        EXPECT_TRUE(observer_1.getState().getRawData() == agg_vec);
        EXPECT_TRUE(observer_2.getState().getRawData() == agg_vec);
    }

    sched.run(78, true);
    EXPECT_TRUE(sched.getCurrentTick() == 1098);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_INIT);
    EXPECT_TRUE(observer_1.getTimeDuration() == 78);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_INIT);
    EXPECT_TRUE(observer_2.getTimeDuration() == 78);

    observer_1.setOperandState(OperandState::OPER_WAIT);
    observer_2.setUopState(UopState::UOP_WAIT);

    sched.run(1, true);
    EXPECT_TRUE(sched.getCurrentTick() == 1099);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_WAIT);
    EXPECT_TRUE(observer_1.getTimeDuration() == 1);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_WAIT);
    EXPECT_TRUE(observer_2.getTimeDuration() == 1);

    observer_1.setOperandState(OperandState::OPER_RETIRE);
    observer_2.setUopState(UopState::UOP_RETIRE);

    sched.run(39, true);
    EXPECT_TRUE(sched.getCurrentTick() == 1138);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_RETIRE);
    EXPECT_TRUE(observer_1.getTimeDuration() == 39);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_RETIRE);
    EXPECT_TRUE(observer_2.getTimeDuration() == 39);

    observer_1.setOperandState(OperandState::OPER_READY);
    observer_2.setUopState(UopState::UOP_READY);

    sched.run(2, true);
    EXPECT_TRUE(sched.getCurrentTick() == 1140);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_READY);
    EXPECT_TRUE(observer_1.getTimeDuration() == 2);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_READY);
    EXPECT_TRUE(observer_2.getTimeDuration() == 2);

    observer_1.reset();
    observer_2.reset();

    EXPECT_TRUE(sched.getCurrentTick() == 1140);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_INIT);
    EXPECT_TRUE(observer_1.getTimeDuration() == 0);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_INIT);
    EXPECT_TRUE(observer_2.getTimeDuration() == 0);

    {
        const std::vector<sparta::Scheduler::Tick> agg_vec = {714, 107, 30, 289, 0};
        EXPECT_TRUE(observer_1.getState().getRawData() == agg_vec);
        EXPECT_TRUE(observer_2.getState().getRawData() == agg_vec);
    }

    sched.run(909, true);
    EXPECT_TRUE(sched.getCurrentTick() == 2049);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_INIT);
    EXPECT_TRUE(observer_1.getTimeDuration() == 909);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_INIT);
    EXPECT_TRUE(observer_2.getTimeDuration() == 909);

    observer_1.setOperandState(OperandState::OPER_WAIT);
    observer_2.setUopState(UopState::UOP_WAIT);

    sched.run(17, true);
    EXPECT_TRUE(sched.getCurrentTick() == 2066);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_WAIT);
    EXPECT_TRUE(observer_1.getTimeDuration() == 17);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_WAIT);
    EXPECT_TRUE(observer_2.getTimeDuration() == 17);

    observer_1.setOperandState(OperandState::OPER_RETIRE);
    observer_2.setUopState(UopState::UOP_RETIRE);

    sched.run(63, true);
    EXPECT_TRUE(sched.getCurrentTick() == 2129);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_RETIRE);
    EXPECT_TRUE(observer_1.getTimeDuration() == 63);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_RETIRE);
    EXPECT_TRUE(observer_2.getTimeDuration() == 63);

    observer_1.setOperandState(OperandState::OPER_READY);
    observer_2.setUopState(UopState::UOP_READY);

    sched.run(4, true);
    EXPECT_TRUE(sched.getCurrentTick() == 2133);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_READY);
    EXPECT_TRUE(observer_1.getTimeDuration() == 4);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_READY);
    EXPECT_TRUE(observer_2.getTimeDuration() == 4);

    observer_1.reset();
    observer_2.reset();

    EXPECT_TRUE(sched.getCurrentTick() == 2133);

    EXPECT_TRUE(observer_1.getCurrentState() == OperandState::OPER_INIT);
    EXPECT_TRUE(observer_1.getTimeDuration() == 0);
    EXPECT_TRUE(observer_2.getCurrentState() == UopState::UOP_INIT);
    EXPECT_TRUE(observer_2.getTimeDuration() == 0);

    {
        const std::vector<sparta::Scheduler::Tick> agg_vec = {1623, 111, 47, 352, 0};
        EXPECT_TRUE(observer_1.getState().getRawData() == agg_vec);
        EXPECT_TRUE(observer_2.getState().getRawData() == agg_vec);
    }

    return 0;
}
