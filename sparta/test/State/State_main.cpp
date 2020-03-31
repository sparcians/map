#include "sparta/sparta.hpp"

#include <iostream>
#include <inttypes.h>
#include <memory>
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/events/Event.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/PayloadEvent.hpp"
#include "sparta/simulation/State.hpp"
#include "sparta/utils/Enum.hpp"
#include "sparta/utils/SpartaTester.hpp"

TEST_INIT;

//____________________________________________________________
// STATE TYPE DECLARATIONS
class Uop;

enum class OperandState{
    OPER_INIT = 0,
    __FIRST = OPER_INIT,
    OPER_READY,
    N_OPER_STATE,
    __LAST = N_OPER_STATE
} ;

enum class uOpState {
    UOP_INIT = 0,
    __FIRST = UOP_INIT,
    UOP_READY,
    N_UOP_STATE,
    __LAST = N_UOP_STATE
} ;

namespace OperandType
{
    const uint32_t A = 0;
    const uint32_t B = 1;
    const uint32_t C = 2;
}

//____________________________________________________________
// OPERAND
class Operand
{
    typedef sparta::State<OperandState> StateType;
    typedef sparta::State<bool, void, 32> BoolStateType;

public:
    class Decorations;

public:
    Operand(const std::string& name, Uop* const uop);

    void reset()
    {
        state_.reset(OperandState::OPER_INIT);
        flag_.reset();
    }

    void markReady()
    {
        state_ = OperandState::OPER_READY;
        uop_ready_marker_->set();
    }

    void setFlag(const bool flg = true)
    {
        flag_ = flg;
    }

    const BoolStateType& getFlag() const
    {
        return flag_;
    }

    const StateType& getState() const
    {
        return state_;
    }

    StateType& getState()
    {
        return state_;
    }

    const OperandState & getCurrentState() const
    {
        return state_.getEnumValue();
    }

    template<class EventT>
    void observeState(const OperandState& state_id, const EventT & ev_handle) {
        state_.observe(state_id, ev_handle);
    }

    template<class EventT>
    void withdrawState(const OperandState& state_id, const EventT & ev_handle) {
        state_.withdraw(state_id, ev_handle);
    }

    // StateType::AudienceType& getStateAudience(const OperandState& state_id)
    // {
    //     return state_.getAudience(state_id);
    // }

    template<class EventT>
    void observeFlag(const EventT & ev_handle) {
        flag_.observe(true, ev_handle);
    }

    // BoolStateType::AudienceType& getFlagAudience(const bool flg = true)
    // {
    //     return flag_.getAudience(flg);
    // }

    void print(std::ostream &os) const
    {
        os << "Operand[" << name_ << "]";
    }

private:
    std::string                      name_;
    StateType                   state_;
    BoolStateType               flag_;
    Uop*                        const uop_;

    sparta::State<uOpState>::Marker     *uop_ready_marker_;    // For example

    //Decorations                 *decor_;
};

class Operand::Decorations {
    // Unit-specific data here
};

//____________________________________________________________
// uOp
class Uop
{
public:
    typedef sparta::State<uOpState>                 StateType;
    class Decorations;

public:
    Uop(const std::string& name = ""):
        name_(name)
    {
        // state_.declareValue(UOP_INIT);
        // state_.declareValue(UOP_READY);

        // Extraction stuff goes here
        src_[OperandType::A].reset(new Operand("a", this));
        src_[OperandType::B].reset(new Operand("b", this));
        src_[OperandType::C].reset(new Operand("c", this));
    }

    Uop(const Uop& other) :
        name_{other.name_},
        state_{other.state_}{
            for(size_t i = 0; i < 3; ++i){
                src_[i] = std::make_unique<Operand>(*other.src_[i]);
            }
        }

    void reset()
    {
        for (auto & oi : src_)
        {
            if(oi) {
                oi->reset();
            }
        }

        state_.reset(uOpState::UOP_INIT);
    }

    Operand* getSource(const uint32_t& type)
    {
        return src_[type].get();
    }

    const StateType& getState() const
    {
        return state_;
    }

    StateType& getState()
    {
        return state_;
    }

    const uOpState & getCurrentState() const
    {
        return state_.getEnumValue();
    }

    StateType::Marker *newStateMarker(const uOpState& state_id)
    {
        return state_.newMarker(state_id);
    }

    template<class EventT>
    void observeState(const uOpState& state_id, const EventT & ev_handle) {
        state_.observe(state_id, ev_handle);
    }

    template<class EventT>
    void withdrawState(const uOpState& state_id, const EventT & ev_handle) {
        state_.withdraw(state_id, ev_handle);
    }

    void print(std::ostream &os) const
    {
        os << "uOp[" << name_ << "]";
    }

private:
    std::string name_;
    StateType   state_;
    std::array<std::unique_ptr<Operand>, 3> src_;

    //Decorations *decor_;
};

class Uop::Decorations {
    // Unit-specific data here
};

//____________________________________________________________
// METHODS
Operand::Operand(const std::string& name, Uop* const uop):
    name_(name),
    uop_(uop)
{
    // state_.declareValue(OPER_INIT);
    // state_.declareValue(OPER_READY);
    uop_ready_marker_ = uop_->newStateMarker(uOpState::UOP_READY);
}

std::ostream& operator<<(std::ostream &os, const Operand* op)
{
    op->print(os);
    return os;
}

std::ostream& operator<<(std::ostream &os, const Uop* op)
{
    op->print(os);
    return os;
}

//____________________________________________________________
// OBSERVER
class Observer
{
public:
    Observer(const std::string& name):
        name_(name),
        activations_(0)
    {}


    void activate()
    {
        std::cout << "Observer(" << name_ << ")::activate()" << std::endl;
        ++activations_;
    }

    template<class DataType>
    void activate(const DataType& dat)
    {
        std::cout << "Observer(" << name_ << ")::activate<>(" << dat << ")" << std::endl;
        ++activations_;
    }

    uint32_t getActivations() const
    {
        return activations_;
    }

private:
    std::string name_;
    uint32_t    activations_;
};

//____________________________________________________________
// MISC TYPES
enum class enStateID {
    FETCHED = 0,
    __FIRST = FETCHED,
    DECODED,
    COMPLETE,
    RETIRED,
    __LAST,
};
//template<>
//const std::unique_ptr<std::string[]>
    //sparta::utils::Enum<enStateID>::names_(new std::string[static_cast<uint32_t>(enStateID::__LAST) + 1]);

// What does this do???
// void speedTest()
// {
//     Observer                obs("Foo");
//     PayloadEvent<Uop*>      e_uop_proto(nullptr, "e_uop",
//                                         CREATE_SPARTA_PAYLOAD_EVENT_FOR_OBJ(Observer, &obs, activate, Uop*));
//     PayloadEvent<Operand*>  e_op_proto0(nullptr, "e_op_proto0", CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(Observer, &obs, activate, Operand*));
//     PayloadEvent<Operand*>  e_op_proto1(nullptr, "e_op_proto1", CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(Observer, &obs, activate, Operand*));
//     PayloadEvent<Operand*>  e_op_proto2(nullptr, "e_op_proto2", CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(Observer, &obs, activate, Operand*));

//     for(uint32_t i = 0; i < 1000000; ++i) {
//         Uop                     uop("uop");
//         e_uop_proto->setData(&uop);
//         Operand                 *a = uop.getSource(OperandType::A);
//         e_op_proto[0]->setData(a);
//         Operand                 *b = uop.getSource(OperandType::B);
//         e_op_proto[1]->setData(b);
//         Operand                 *c = uop.getSource(OperandType::C);
//         e_op_proto[2]->setData(c);
//     }
// }

// void testCopyConstruct()
// {
//     enum class StateCopyTest {
//         COPIED_STATE1,
//         __FIRST = COPIED_STATE1,
//         COPIED_STATE2,
//         __LAST = COPIED_STATE2
//     };

//     sparta::State<StateCopyTest> orig_state;
//     sparta::State<StateCopyTest> new_state(orig_state);
//     EXPECT_TRUE(orig_state.getValue() == StateCopyTest::COPIED_STATE1);
//     EXPECT_TRUE(new_state.getValue() == StateCopyTest::COPIED_STATE1);

//     orig_state.setValue(StateCopyTest::COPIED_STATE2);
//     sparta::State<StateCopyTest> new_state2(orig_state);
//     EXPECT_TRUE(orig_state.getValue() == StateCopyTest::COPIED_STATE2);
//     EXPECT_TRUE(new_state2.getValue() == StateCopyTest::COPIED_STATE2);
// }

int main()
{
    sparta::Scheduler sched;
    sparta::Clock clk("clock", &sched);
    EXPECT_TRUE(sched.getCurrentTick() == 1);
    EXPECT_TRUE(sched.isRunning() == 0);

    Observer                        obs("Foo");
    sparta::RootTreeNode rtn;
    rtn.setClock(&clk);
    sparta::EventSet es(&rtn);

    sparta::PayloadEvent<Uop*>      e_uop_proto(&es, "e_uop", CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(Observer, &obs, activate, Uop*));
    sparta::PayloadEvent<Operand*>  e_op_proto0(&es, "e_op_proto0", CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(Observer, &obs, activate, Operand*));
    sparta::PayloadEvent<Operand*>  e_op_proto1(&es, "e_op_proto1", CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(Observer, &obs, activate, Operand*));
    sparta::PayloadEvent<Operand*>  e_op_proto2(&es, "e_op_proto2", CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(Observer, &obs, activate, Operand*));

    rtn.enterConfiguring();
    rtn.enterFinalized();
    sched.finalize();

    sched.printNextCycleEventTree(std::cout, 0, 0);

    Uop                     uop("uop");
    Operand                 *a = uop.getSource(OperandType::A);
    Operand                 *b = uop.getSource(OperandType::B);
    Operand                 *c = uop.getSource(OperandType::C);

    sched.run(1);
    EXPECT_TRUE(a->getCurrentState() == OperandState::OPER_INIT);
    EXPECT_TRUE(b->getCurrentState() == OperandState::OPER_INIT);
    EXPECT_TRUE(c->getCurrentState() == OperandState::OPER_INIT);
    EXPECT_TRUE(uop.getCurrentState() == uOpState::UOP_INIT);
    EXPECT_TRUE(a->getFlag().isClear());

    Operand copy_a {*a};
    Operand copy_b {*b};
    Operand copy_c {*c};
    Uop copy_uop{uop};

    EXPECT_TRUE(copy_a.getCurrentState() == OperandState::OPER_INIT);
    EXPECT_TRUE(copy_b.getCurrentState() == OperandState::OPER_INIT);
    EXPECT_TRUE(copy_c.getCurrentState() == OperandState::OPER_INIT);
    EXPECT_TRUE(copy_uop.getCurrentState() == uOpState::UOP_INIT);
    EXPECT_TRUE(copy_a.getFlag().isClear());

    sched.run(2);
    uop.observeState(uOpState::UOP_READY, e_uop_proto.preparePayload(&uop));
    a->observeState(OperandState::OPER_READY, e_op_proto0.preparePayload(a));
    b->observeState(OperandState::OPER_READY, e_op_proto1.preparePayload(b));
    c->observeState(OperandState::OPER_READY, e_op_proto2.preparePayload(c));
    a->observeFlag(e_op_proto0.preparePayload(a));

    copy_uop.observeState(uOpState::UOP_READY, e_uop_proto.preparePayload(&copy_uop));
    copy_a.observeState(OperandState::OPER_READY, e_op_proto0.preparePayload(&copy_a));
    copy_b.observeState(OperandState::OPER_READY, e_op_proto1.preparePayload(&copy_b));
    copy_c.observeState(OperandState::OPER_READY, e_op_proto2.preparePayload(&copy_c));
    copy_a.observeFlag(e_op_proto0.preparePayload(&copy_a));

    a->markReady();
    b->markReady();
    c->markReady();
    a->setFlag();

    copy_a.markReady();
    copy_b.markReady();
    copy_c.markReady();
    copy_a.setFlag();

    // Re-enroll since we don't have persistent audience anymore
    sched.run(3);
    uop.observeState(uOpState::UOP_READY, e_uop_proto.preparePayload(&uop));
    a->observeState(OperandState::OPER_READY, e_op_proto0.preparePayload(a));
    copy_a.observeState(OperandState::OPER_READY, e_op_proto0.preparePayload(&copy_a));


    sched.run(4);
    EXPECT_TRUE(a->getCurrentState() == OperandState::OPER_READY);
    EXPECT_TRUE(b->getCurrentState() == OperandState::OPER_READY);
    EXPECT_TRUE(c->getState() == OperandState::OPER_READY);
    EXPECT_TRUE(uop.getCurrentState() == uOpState::UOP_READY);
    EXPECT_TRUE(a->getFlag().isSet());
    EXPECT_TRUE(copy_a.getCurrentState() == OperandState::OPER_READY);
    EXPECT_TRUE(copy_b.getCurrentState() == OperandState::OPER_READY);
    EXPECT_TRUE(copy_c.getState() == OperandState::OPER_READY);
    EXPECT_TRUE(copy_a.getFlag().isSet());

    uop.reset();
    sched.run(5);
    EXPECT_TRUE(a->getCurrentState() == OperandState::OPER_INIT);
    EXPECT_TRUE(b->getCurrentState() == OperandState::OPER_INIT);
    EXPECT_TRUE(c->getCurrentState() == OperandState::OPER_INIT);
    EXPECT_TRUE(uop.getCurrentState() == uOpState::UOP_INIT);
    EXPECT_TRUE(a->getFlag().isClear());

    uop.getState().setMarkedThreshold(uOpState::UOP_READY, 3);

    a->markReady();

    sched.run(6);
    EXPECT_TRUE(a->getCurrentState() == OperandState::OPER_READY);
    EXPECT_TRUE(b->getCurrentState() == OperandState::OPER_INIT);
    EXPECT_TRUE(c->getCurrentState() == OperandState::OPER_INIT);
    EXPECT_TRUE(uop.getCurrentState() == uOpState::UOP_READY);
    EXPECT_TRUE(a->getFlag().isClear());

    sched.run(100);
    EXPECT_EQUAL(obs.getActivations(), 11);

    // Test withdraw feature
    uop.reset();
    sched.run(7);
    EXPECT_TRUE(a->getCurrentState() == OperandState::OPER_INIT);
    EXPECT_TRUE(b->getCurrentState() == OperandState::OPER_INIT);
    EXPECT_TRUE(c->getCurrentState() == OperandState::OPER_INIT);
    EXPECT_TRUE(uop.getCurrentState() == uOpState::UOP_INIT);
    EXPECT_TRUE(a->getFlag().isClear());

    auto uop_ehandle = e_uop_proto.preparePayload(&uop);
    auto operand_ehandle = e_op_proto0.preparePayload(a);

    uop.observeState(uOpState::UOP_READY, uop_ehandle);
    a->observeState(OperandState::OPER_READY, operand_ehandle);

    sched.run(8);
    EXPECT_TRUE(a->getState() == OperandState::OPER_INIT);
    EXPECT_TRUE(uop.getCurrentState() == uOpState::UOP_INIT);

    sched.run(9);
    uop.withdrawState(uOpState::UOP_READY, uop_ehandle);
    a->withdrawState(OperandState::OPER_READY, operand_ehandle);

    a->markReady();

    sched.run(100);
    // No new activations should be seen
    EXPECT_EQUAL(obs.getActivations(), 11);

    // StateSet tests...
    // utils::Enum<enStateID> StateID(enStateID::FETCHED, "Fetched",
    //                                enStateID::DECODED, "Decoded",
    //                                enStateID::COMPLETE, "Complete",
    //                                enStateID::RETIRED, "Retired");
    // typedef utils::Enum<enStateID>::Value StateIDValue;

    sparta::State<enStateID>     ss;
    EXPECT_TRUE(ss.isClear(enStateID::FETCHED ));
    EXPECT_TRUE(ss.isClear(enStateID::DECODED ));
    EXPECT_TRUE(ss.isClear(enStateID::COMPLETE));
    EXPECT_TRUE(ss.isClear(enStateID::RETIRED ));

    ss = enStateID::FETCHED;
    ss = enStateID::DECODED;
    ss = enStateID::RETIRED;
    ss = enStateID::COMPLETE;

    EXPECT_TRUE(ss.isSet(enStateID::FETCHED));
    EXPECT_TRUE(ss.isSet(enStateID::DECODED));
    EXPECT_TRUE(ss.isSet(enStateID::COMPLETE));
    EXPECT_TRUE(ss.isSet(enStateID::RETIRED));

    EXPECT_TRUE(ss == enStateID::COMPLETE);
    EXPECT_FALSE(ss == enStateID::FETCHED);
    EXPECT_FALSE(ss == enStateID::DECODED);
    EXPECT_FALSE(ss == enStateID::RETIRED);

    ss.reset();
    EXPECT_TRUE(ss.isClear(enStateID::FETCHED ));
    EXPECT_TRUE(ss.isClear(enStateID::DECODED ));
    EXPECT_TRUE(ss.isClear(enStateID::COMPLETE));
    EXPECT_TRUE(ss.isClear(enStateID::RETIRED ));

    sched.run(100);
    EXPECT_EQUAL(obs.getActivations(), 11);

    //testCopyConstruct();

    //speedTest();

    rtn.enterTeardown();

    REPORT_ERROR;
    return ERROR_CODE;
}
