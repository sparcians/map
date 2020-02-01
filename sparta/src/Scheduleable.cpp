// <Scheduleable.cpp> -*- C++ -*-


#include <string>

#include "sparta/kernel/DAG.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/events/Scheduleable.hpp"
#include "sparta/kernel/Vertex.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/kernel/SpartaHandler.hpp"
#include "sparta/events/SchedulingPhases.hpp"

namespace sparta
{

    const Scheduleable::PrecedenceGroup Scheduleable::INVALID_GROUP = 0xFFFFFFFF;

    Scheduleable::PrecedenceSetup & Scheduleable::PrecedenceSetup::operator=(Scheduler * scheduler)
    {
        sparta_assert(!scheduler_ || scheduler_ == scheduler);
        const bool is_first_assignment = !scheduler_ && scheduler;
        scheduler_ = scheduler;
        if (is_first_assignment) {
            scheduleable_->setVertex();
            scheduleable_->onSchedulerAssignment_();
        }
        return *this;
    }

    Scheduleable::Scheduleable(const SpartaHandler & consumer_event_handler,
                               Clock::Cycle delay, SchedulingPhase sched_phase) :
        consumer_event_handler_(consumer_event_handler),
        label_(consumer_event_handler_.getName()),
        delay_(delay),
        sched_phase_(sched_phase){
    }

    void Scheduleable::setLabel(const char * label) {
        label_ = label;
        if(vertex_) {
            vertex_->setLabel(label);
        }
    }

    void Scheduleable::setVertex() {
        sparta_assert(scheduler_);
        vertex_ = scheduler_->getDAG()->newFactoryVertex(label_, scheduler_, false);
        vertex_->setScheduleable(this);
    }

    void Scheduleable::precedes(Scheduleable & w, const std::string & label) {
        sparta_assert(scheduler_);
        DAG * dag = scheduler_->getDAG();
        sparta_assert(dag->isFinalized() == false,
                    "You cannot set precedence during a running simulation (i.e., the DAG is finalized)");
        dag->link(this->vertex_, w.vertex_, label);
    }

    void Scheduleable::precedes(Vertex & w, const std::string & label) const{
        sparta_assert(scheduler_);
        DAG * dag = scheduler_->getDAG();
        sparta_assert(dag->isFinalized() == false,
                    "You cannot set precedence during a running simulation (i.e., the DAG is finalized)");
        dag->link(this->vertex_, &w, label);
    }

    bool Scheduleable::unlink(Scheduleable *w)
    {
        sparta_assert(scheduler_);
        sparta_assert(w != this);
        DAG * dag = scheduler_->getDAG();
        return dag->unlink(this->vertex_, w->vertex_);
    }

    bool Scheduleable::isOrphan() const{
        return  ((this->vertex_ == nullptr) || (this->vertex_->isOrphan()));
    }

#ifndef DO_NOT_DOCUMENT
    void Scheduleable::setupDummyPrecedence_ThisMethodToGoAwayOnceDaveAddsPhaseSupportToDAG()
    {

        sparta_assert(scheduler_);
        if (scheduler_->isFinalized()) {
            return;
        }

        auto dag = scheduler_->getDAG();

        switch(sched_phase_)
        {
        case SchedulingPhase::Trigger:
            this->precedes(dag->getGOPoint("Trigger"));
            break;
        case SchedulingPhase::Update:
            dag->getGOPoint("Trigger")->link(this->vertex_);
            this->precedes(dag->getGOPoint("Update"));
            break;
        case SchedulingPhase::PortUpdate:
            dag->getGOPoint("Update")->link(this->vertex_);
            this->precedes(dag->getGOPoint("PortUpdate"));
            break;
        case SchedulingPhase::Flush:
            dag->getGOPoint("PortUpdate")->link(this->vertex_);
            this->precedes(dag->getGOPoint("Flush"));
            break;
        case SchedulingPhase::Collection:
            dag->getGOPoint("Flush")->link(this->vertex_);
            this->precedes(dag->getGOPoint("Collection"));
            break;
        case SchedulingPhase::Tick:
            dag->getGOPoint("Collection")->link(this->vertex_);
            this->precedes(dag->getGOPoint("Tick"));
            break;
        case SchedulingPhase::PostTick:
            dag->getGOPoint("Tick")->link(this->vertex_);
            this->precedes(dag->getGOPoint("PostTick"));
            break;
        case SchedulingPhase::Invalid:
            sparta_assert(!"Should not have gotten here");
            break;
            // No Default! to cause compile time errors
        }
    }
    // DO_NOT_DOCUMENT
#endif


}//End namespace
