

#include "sparta/sparta.hpp"

#include <iostream>
#include <inttypes.h>
#include <memory>
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/PayloadEvent.hpp"
#include "sparta/simulation/State.hpp"
#include "sparta/utils/Enum.hpp"
#include "sparta/utils/SpartaTester.hpp"

TEST_INIT

using namespace std;
using namespace sparta;

//____________________________________________________________
// STATE TYPE DECLARATIONS
class Uop;

typedef enum {
   OPER_NOTREADY = 0,
   __FIRST = OPER_NOTREADY,
   OPER_READY,
   OPER_SPECREADY,
   N_OPER_STATE,
   __LAST = N_OPER_STATE
} OperandState;

enum class uOpState {
    UOP_NOTREADY = 0,
    __FIRST = UOP_NOTREADY,
    UOP_READY,
    UOP_SPECREADY,
    N_UOP_STATE,
    __LAST = N_UOP_STATE
};

//____________________________________________________________
// OPERAND
class Operand
{
    typedef State<OperandState>    StateType;

public:
    class Decorations;

public:
    Operand(const string& name, Uop* const uop);

    void reset()
    {
        state_.reset(OPER_NOTREADY);
    }

    void markReady()
    {
        state_ = OPER_READY;
        // Careful here! Need to clear markers before setting
        // to assure count thresholds inside Monitor::signal
        // are not exceeded
        uop_spec_ready_marker_->clear();
        uop_not_ready_marker_->clear();
        uop_ready_marker_->set();
    }

    void markSpecReady()
    {
        state_ = OPER_SPECREADY;
        // Careful here! Need to clear markers before setting
        // to assure count thresholds inside Monitor::signal
        // are not exceeded
        uop_ready_marker_->clear();
        uop_not_ready_marker_->clear();
        uop_spec_ready_marker_->set();
    }

    void markNotReady()
    {
        state_ = OPER_NOTREADY;
        // Careful here! Need to clear markers before setting
        // to assure count thresholds inside Monitor::signal
        // are not exceeded
        uop_ready_marker_->clear();
        uop_spec_ready_marker_->clear();
        uop_not_ready_marker_->set();
    }

    const StateType& getState() const
    {
        return state_;
    }

    template <class EventT>
    void observe(const OperandState& state_id, const EventT & ev)
    {
        return state_.observe(state_id, ev);
    }

    void print(ostream &os) const
    {
        os << "Operand[" << name_ << "]";
    }

private:
    string                      name_;
    StateType                   state_;
    Uop*                        const uop_;

    State<uOpState>::Marker    *uop_ready_marker_;    // For example
    State<uOpState>::Marker    *uop_not_ready_marker_;    // For example
    State<uOpState>::Marker    *uop_spec_ready_marker_;    // For example
};

class Operand::Decorations {
    // Unit-specific data here
};

//____________________________________________________________
// uOp
class Uop
{
    typedef map<const string, Operand*>     OperandList;
public:
    typedef State<uOpState>                 StateType;

public:
    Uop(const string& name = ""):
        name_(name)
    {
        // Extraction stuff goes here
        src_["a"] = new Operand("a", this);
        src_["b"] = new Operand("b", this);
        src_["c"] = new Operand("c", this);
    }

    ~Uop()
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

    void reset()
    {
        for (auto oi = src_.begin(); oi != src_.end(); ++oi)
        {
            oi->second->reset();
        }

        for (auto oi = dest_.begin(); oi != dest_.end(); ++oi)
        {
            oi->second->reset();
        }

        state_.reset(uOpState::UOP_NOTREADY);
    }

    Operand* getSource(const string& name)
    {
        return getOperand_(name, src_);
    }

    Operand* getDest(const string& name)
    {
        return getOperand_(name, dest_);
    }

    uint32_t numSources() const
    {
        return src_.size();
    }

    uint32_t numDests() const
    {
        return dest_.size();
    }

    StateType& getState()
    {
        return state_;
    }

    StateType::Marker *newStateMarker(const uOpState& state_id)
    {
        return state_.newMarker(state_id);
    }

    template <class EventT>
    void observe(const uOpState& state_id, const EventT & ev)
    {
        return state_.observe(state_id, ev);
    }

    void print(ostream &os) const
    {
        os << "uOp[" << name_ << "]";
    }

private:
    Operand* getOperand_(const string& name, const OperandList& olist) const
    {
        auto oi = olist.find(name);
        assert(oi != olist.end());
        return oi->second;
    }

private:
    string name_;
    StateType   state_;
    OperandList src_;
    OperandList dest_;
};

//____________________________________________________________
// METHODS
Operand::Operand(const string& name, Uop* const uop):
        name_(name),
        uop_(uop)
{
    uop_not_ready_marker_ = uop_->newStateMarker(uOpState::UOP_NOTREADY);
    uop_ready_marker_ = uop_->newStateMarker(uOpState::UOP_READY);
    uop_spec_ready_marker_ = uop_->newStateMarker(uOpState::UOP_SPECREADY);
}

ostream& operator<<(ostream &os, const Operand* op)
{
    op->print(os);
    return os;
}

ostream& operator<<(ostream &os, const Uop* op)
{
    op->print(os);
    return os;
}

//____________________________________________________________
// OBSERVER
class Observer
{
public:
    Observer(const string& name):
        name_(name),
        activations_(0)
    {}


    void activate()
    {
        cout << "Observer(" << name_ << ")::activate()" << endl;
        ++activations_;
    }

    template<class DataType>
    void activate(const DataType& dat)
    {
        cout << "Observer(" << name_ << ")::activate<>(" << dat << ")" << endl;
        ++activations_;
    }

    uint32_t getActivations() const
    {
        return activations_;
    }

private:
    string name_;
    uint32_t    activations_;
};

//____________________________________________________________
// MONITOR
class MyMonitor : public State<uOpState>::Monitor
{
    using Monitor::EnumTValueType;
public:
    MyMonitor(const std::string& name, Uop& uop, State<uOpState>& subj):
        name_(name),
        uop_(uop),
        subj_(subj)
    {
        subj.attachMonitor(uOpState::UOP_READY, this);
        subj.attachMonitor(uOpState::UOP_SPECREADY, this);
        subj.attachMonitor(uOpState::UOP_NOTREADY, this);
    }

    virtual ~MyMonitor() = default;

    //void actReady(const StateValue<ValueType>& sv)
    void signalSet(const Monitor::EnumTValueType &, State<uOpState>::MetaDataTPtr )
    {
        cout << "MyMonitor'" << name_ << "'::signalSet()" << endl;
        if (subj_.complete(uOpState::UOP_READY)) {
            subj_.setValue(uOpState::UOP_READY);
        } else if ((subj_.numMarks(uOpState::UOP_READY) +
                    subj_.numMarks(uOpState::UOP_SPECREADY)) == uop_.numSources()) {
            subj_.setValue(uOpState::UOP_SPECREADY);
        }
        else {
            std::cout << subj_.numMarks(uOpState::UOP_READY) << " " <<
                subj_.numMarks(uOpState::UOP_SPECREADY) << std::endl;
        }
    }

    void release()
    {
        subj_.detachMonitor(uOpState::UOP_READY, this);
        subj_.detachMonitor(uOpState::UOP_SPECREADY, this);
        subj_.detachMonitor(uOpState::UOP_NOTREADY, this);
    }

private:
    std::string     name_;
    Uop&            uop_;
    State<uOpState> & subj_;
};


//____________________________________________________________
// MAIN
int main()
{
    sparta::Scheduler sched;
    Clock clk("clock", &sched);
    EXPECT_TRUE(sched.getCurrentTick() == 0); //unfinalized sched at tick 0
    EXPECT_TRUE(sched.isRunning() == 0);

    sparta::RootTreeNode rtn;
    rtn.setClock(&clk);

    Observer                        obs("Foo");

    sparta::EventSet es(&rtn);

    PayloadEvent<Uop*>     e_uop(&es, "e_uop", CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(Observer, &obs, activate, Uop*));
    PayloadEvent<Operand*> e_op0(&es, "e_op0", CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(Observer, &obs, activate, Operand*));
    PayloadEvent<Operand*> e_op1(&es, "e_op1", CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(Observer, &obs, activate, Operand*));
    PayloadEvent<Operand*> e_op2(&es, "e_op2", CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(Observer, &obs, activate, Operand*));

    sched.finalize();
    sched.printNextCycleEventTree(cout, 0, 0);

    Uop                     uop("uop");
    MyMonitor               mon1("Mon1", uop, uop.getState());
    MyMonitor               mon2("Mon2", uop, uop.getState());
    MyMonitor               mon3("Mon3", uop, uop.getState());
    mon2.release();

    Operand                 *a = uop.getSource("a");
    Operand                 *b = uop.getSource("b");
    Operand                 *c = uop.getSource("c");

    EXPECT_TRUE(a->getState() == OPER_NOTREADY);
    EXPECT_TRUE(b->getState() == OPER_NOTREADY);
    EXPECT_TRUE(c->getState() == OPER_NOTREADY);
    EXPECT_TRUE(uop.getState() == uOpState::UOP_NOTREADY);

    uop.observe(uOpState::UOP_SPECREADY, e_uop.preparePayload(&uop));
    a->observe(OPER_READY, e_op0.preparePayload(a));
    b->observe(OPER_SPECREADY, e_op1.preparePayload(b));
    c->observe(OPER_SPECREADY, e_op2.preparePayload(c));
    c->observe(OPER_READY, e_op2.preparePayload(c));

    a->markReady();
    b->markSpecReady();
    c->markSpecReady();
    EXPECT_TRUE(c->getState() == OPER_SPECREADY);
    EXPECT_TRUE(uop.getState() == uOpState::UOP_SPECREADY);
    uop.observe(uOpState::UOP_SPECREADY, e_uop.preparePayload(&uop));
    c->markReady();

    EXPECT_TRUE(a->getState() == OPER_READY);
    EXPECT_TRUE(b->getState() == OPER_SPECREADY);
    EXPECT_TRUE(c->getState() == OPER_READY);
    EXPECT_TRUE(uop.getState() == uOpState::UOP_SPECREADY);

    sched.run(100);
    EXPECT_EQUAL(obs.getActivations(), 6);
    REPORT_ERROR;
    rtn.enterTeardown();
    return ERROR_CODE;
}
