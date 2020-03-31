#include "sparta/sparta.hpp"

#include <iostream>
#include <inttypes.h>
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/events/Event.hpp"
#include "sparta/SubjectState.hpp"
#include "sparta/utils/SpartaTester.hpp"

TEST_INIT;

using namespace sparta;

//____________________________________________________________
// STATE TYPE DECLARATIONS
class uOp;

typedef enum {
   OPER_INIT,
   OPER_READY,
   N_OPER_STATE
} OperandState;

typedef enum {
    UOP_INIT,
    UOP_READY,
    N_UOP_STATE
} uOpState;


//____________________________________________________________
// OPERAND
class Operand
{
    typedef SubjectState<Operand*, OperandState>    StateType;
    typedef SubjectState<Operand*, bool>            BoolStateType;

public:
    class Decorations;

public:
    Operand(const std::string& name, uOp* const uop);

    void markReady()
    {
        state_ = OPER_READY;
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

    StateType::AudienceType& getStateAudience(const OperandState& state_id)
    {
        return state_.getAudience(state_id);
    }

    BoolStateType::AudienceType& getFlagAudience(const bool flg = true)
    {
        return flag_.getAudience(flg);
    }

    void print(std::ostream &os) const
    {
        os << "Operand[" << name_ << "]";
    }

private:
    std::string                      name_;
    StateType                        state_;
    BoolStateType                    flag_;
    uOp*                             const uop_;

    SubjectState<uOp*, uOpState>::MarkerType    *uop_ready_marker_;    // For example

    Decorations                 *decor_;
};

class Operand::Decorations {
    // Unit-specific data here
};

//____________________________________________________________
// uOp
class uOp
{
    typedef SubjectState<uOp*, uOpState>               StateType;
    typedef std::map<const std::string, Operand*>     OperandList;
public:
    class Decorations;

public:
    uOp(const std::string& name = ""):
        name_(name),
        state_(this)
    {
        state_.declareValue(UOP_INIT);
        state_.declareValue(UOP_READY);

        // Extraction stuff goes here
        src_["a"] = new Operand("a", this);
        src_["b"] = new Operand("b", this);
        src_["c"] = new Operand("c", this);
    }

    ~uOp()
    {
        for (auto oi = src_.begin(); oi != src_.end(); ++oi)
        {
            delete oi->second;
        }

        for (auto oi = dest_.begin(); oi != dest_.end(); ++oi)
        {
            delete oi->second;
        }
    }

    Operand* getSource(const std::string& name)
    {
        return getOperand_(name, src_);
    }

    Operand* getDest(const std::string& name)
    {
        return getOperand_(name, dest_);
    }

    const StateType& getState() const
    {
        return state_;
    }

    StateType::MarkerType *newStateMarker(const uOpState& state_id)
    {
        return state_.newMarker(state_id);
    }

    StateType::AudienceType& getStateAudience(const uOpState& state_id)
    {
        return state_.getAudience(state_id);
    }

    void print(std::ostream &os) const
    {
        os << "uOp[" << name_ << "]";
    }

private:
    Operand* getOperand_(const std::string& name, const OperandList& olist) const
    {
        auto oi = olist.find(name);
        assert(oi != olist.end());
        return oi->second;
    }

private:
    std::string name_;
    StateType   state_;
    OperandList src_;
    OperandList dest_;
    Decorations *decor_;
};

class uOp::Decorations {
    // Unit-specific data here
};

//____________________________________________________________
// METHODS
Operand::Operand(const std::string& name, uOp* const uop):
        name_(name),
        state_(this),
        flag_(this),
        uop_(uop)
{
    state_.declareValue(OPER_INIT);
    state_.declareValue(OPER_READY);
    uop_ready_marker_ = uop_->newStateMarker(UOP_READY);
}

std::ostream& operator<<(std::ostream &os, const Operand* op)
{
    op->print(os);
    return os;
}

std::ostream& operator<<(std::ostream &os, const uOp* op)
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

int main()
{
    Clock clk("clock");
    EXPECT_TRUE(Scheduler::getScheduler()->getCurrentTick() == 0);
    EXPECT_TRUE(Scheduler::getScheduler()->isRunning() == 0);
    Scheduler::getScheduler()->finalize();

    Observer                        obs("Foo");

    SpartaHandler              uop_handler = CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(Observer, &obs, activate, uOp*);

    SpartaHandler              oper_handler = CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(Observer, &obs, activate, Operand*);

    Scheduler::getScheduler()->printNextCycleEventTree(std::cout, 0, 0);

    uOp                     uop("uop");
    Operand                 *a = uop.getSource("a");
    Operand                 *b = uop.getSource("b");
    Operand                 *c = uop.getSource("c");

    EXPECT_TRUE(a->getState() == OPER_INIT);
    EXPECT_TRUE(b->getState() == OPER_INIT);
    EXPECT_TRUE(c->getState() == OPER_INIT);
    EXPECT_TRUE(uop.getState() == UOP_INIT);
    EXPECT_TRUE(a->getFlag().isClear());

    uop.getStateAudience(UOP_READY).enroll(uop_handler, clk);
    a->getStateAudience(OPER_READY).enroll(oper_handler, clk);
    b->getStateAudience(OPER_READY).enroll(oper_handler, clk);
    c->getStateAudience(OPER_READY).enroll(oper_handler, clk);
    a->getFlagAudience().enroll(oper_handler, clk);

    a->markReady();
    b->markReady();
    c->markReady();
    a->setFlag();

    EXPECT_TRUE(a->getState() == OPER_READY);
    EXPECT_TRUE(b->getState() == OPER_READY);
    EXPECT_TRUE(c->getState() == OPER_READY);
    EXPECT_TRUE(uop.getState() == UOP_READY);
    EXPECT_TRUE(a->getFlag().isSet());

    Scheduler::getScheduler()->run(100);
    EXPECT_EQUAL(obs.getActivations(), 5);
    return ERROR_CODE;
}
