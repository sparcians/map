// <TemporaryRunController> -*- C++ -*-


/*!
 * \file TemporaryRunController.cpp
 * \brief "sparta" python module
 */

#include <cstdint>
#include <memory>
#include <limits>
#include <string>
#include <vector>

#include "sparta/control/TemporaryRunController.hpp"
#include "sparta/app/Simulation.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/trigger/SingleTrigger.hpp"
#include "sparta/statistics/dispatch/streams/StreamController.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/statistics/CounterBase.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/kernel/SpartaHandler.hpp"
#include "sparta/simulation/TreeNode.hpp"

namespace sparta {
namespace control {

sparta::Clock* TemporaryRunControl::findClock_(const std::string& clk_name) const {
    sparta::Clock * runtime_clk = sim_->getRootClock();
    std::vector<TreeNode*> results;
    runtime_clk->findChildren(clk_name, results);
    //! \todo List all available clocks on error
    if(results.empty()) {
        if(runtime_clk->getName() == clk_name) {
            return runtime_clk;
        }
        throw SpartaException("Cannot find clock '" + clk_name + "'");
    }
    if(results.size() > 1) {
        throw SpartaException("Found multiple clocks named '" + clk_name + "'; please be more specific");

    }
    runtime_clk = dynamic_cast<sparta::Clock*>(results[0]);
    sparta_assert(runtime_clk != nullptr, "Unable to cast object found in the clock tree to a sparta Clock");
    return runtime_clk;
}

void TemporaryRunControl::runIcountEnd_() {
    //All run commands end up hitting this method when they are done. In
    //a sense, the simulation is being "paused" when it hits this code.
    //Asynchronous report SI clients should be forced to sync up / flush
    //their streams / buffers / etc.
    if (stream_controller_) {
        stream_controller_->processStreams();
    }

    //! \todo This stopRunning may need to skip or allow scheduler to finish up it's trigger
    //! stuff for this cycle. This may not have an ideal implementation
    sched_->stopRunning();
}

TemporaryRunControl::TemporaryRunControl(sparta::app::Simulation* sim, sparta::Scheduler* sched) :
    sim_(sim),
    sched_(sim->getScheduler()),
    icount_end_handler_(sparta::SpartaHandler::from_member<TemporaryRunControl, &TemporaryRunControl::runIcountEnd_>
                       (this, "TemporaryRunControl::runIcountEnd_"))
{
    (void) sched;
    sparta_assert(sched_,
        "cannot construct a TemporaryRunControl object with a null Scheduler pointer");
    sparta_assert(sim,
        "cannot construct a TemporaryRunControl object with a null Simulation pointer");
}

TemporaryRunControl::~TemporaryRunControl()
{
    //If we have a live report stream controller, flush any pending
    //SI data and shutdown any consumer threads that may be going on.
    if (stream_controller_) {
        stream_controller_->stopStreaming();
    }
}

void TemporaryRunControl::setStreamController(
    const std::shared_ptr<statistics::StreamController> & controller)
{
    stream_controller_ = controller;
}

std::shared_ptr<statistics::StreamController> &
    TemporaryRunControl::getStreamController()
{
    return stream_controller_;
}

uint64_t TemporaryRunControl::getCurrentCycle(const std::string& clk_name) const {
    const sparta::Clock * runtime_clk = findClock_(clk_name);
    return runtime_clk->currentCycle();
}

uint64_t TemporaryRunControl::getCurrentCycle(const sparta::Clock* clk) const {
    if(clk != nullptr){
        return clk->currentCycle();
    }else{
        return sim_->getRootClock()->currentCycle();
    }
}

uint64_t TemporaryRunControl::getCurrentInst() const {
    const CounterBase* ictr = sim_->findSemanticCounter(sparta::app::Simulation::CSEM_INSTRUCTIONS);
    if(!ictr){
        throw SpartaException("Cannot proceed with a run instruction count limit because no "
                            "intruction counter semantic was found. Simulator must implement: "
                            "sparta::app::Simulation::findSemanticCounter(CSEM_INSTRUCTIONS)");
    }
    return ictr->get();
}

sparta::Scheduler::Tick TemporaryRunControl::getCurrentTick() const {
    const sparta::Clock * runtime_clk = sim_->getRootClock();
    return runtime_clk->currentTick();
}

void TemporaryRunControl::runi(uint64_t instruction_max) {
    // Set up icount trigger
    //! \todo Fix this support for 2 or more HW threads/cores. Caller will need
    //! to select a core (or set of cores) to run. This is the responsibility of
    //! the run control interface to manage and provide a python interface
    //! control this across multiple cores simultaneously.
    const CounterBase* ictr = sim_->findSemanticCounter(sparta::app::Simulation::CSEM_INSTRUCTIONS);
    if(!ictr){
        throw SpartaException("Cannot proceed with a run instruction count limit because no "
                            "intruction counter semantic was found. Simulator must implement: "
                            "sparta::app::Simulation::findSemanticCounter(CSEM_INSTRUCTIONS)");
    }

    // Setup the trigger
    sparta::trigger::CounterTrigger trig("RunInstructionCount",
                                       icount_end_handler_,
                                       ictr,
                                       instruction_max + ictr->get());
    if(instruction_max == std::numeric_limits<uint64_t>::max()){
        trig.deactivate(); // Remove trigger on infinite run
    }

    runStub_(sparta::Scheduler::INDEFINITE);
}

void TemporaryRunControl::runc(uint64_t cycles_max, const std::string& clk_name) {
    if(cycles_max == 0){
        return;
    }

    if(cycles_max == sparta::Scheduler::INDEFINITE) {
        runStub_(sparta::Scheduler::INDEFINITE);
        return;
    }

    const sparta::Clock * runtime_clk = findClock_(clk_name);
    //const uint64_t ticks_max = runtime_clk->getTick(cycles_max);

    sparta::trigger::CycleTrigger trig("RunCycleCount",
                                     icount_end_handler_,
                                     runtime_clk);
    if(cycles_max != std::numeric_limits<uint64_t>::max()){
        trig.prepRelative(runtime_clk, cycles_max);
        trig.set();
    }
    runStub_(sparta::Scheduler::INDEFINITE);
}

void TemporaryRunControl::runc(uint64_t cycles_max, const sparta::Clock* clk) {
    if(cycles_max == 0){
        return;
    }

    if(cycles_max == sparta::Scheduler::INDEFINITE) {
        runStub_(sparta::Scheduler::INDEFINITE);
        return;
    }

    const sparta::Clock * runtime_clk;
    if(clk != nullptr){
        runtime_clk = clk;
    }else{
        runtime_clk = sim_->getRootClock();
    }

    sparta::trigger::CycleTrigger trig("RunCycleCount",
                                     icount_end_handler_,
                                     runtime_clk);
    if(cycles_max != std::numeric_limits<uint64_t>::max()){
        trig.prepRelative(runtime_clk, cycles_max);
        trig.set();
    }
    runStub_(sparta::Scheduler::INDEFINITE);
}

void TemporaryRunControl::run() {
    runStub_(sparta::Scheduler::INDEFINITE);
}

void TemporaryRunControl::asyncStop() {
    sim_->asyncStop();
}

void TemporaryRunControl::runStub_(sparta::Scheduler::Tick ticks) {
    if (stream_controller_) {
        //All run methods go through runStub_(), so let's tell our
        //stream controller to open its connections with its clients.
        //Note that this only has effect when called the first time,
        //and will be short-circuited with each subsequent call.
        stream_controller_->startStreaming();
    }
    //! \todo Detect that the sparta PythonInterpreter's signal handler was
    //! replaced and complain if it was since it will probably cause problems.
    sim_->runRaw(ticks);
}

} // namespace control
} // namespace sparta
