// <TemporaryRunController.hpp> -*- C++ -*-

#pragma once

#include "sparta/kernel/SpartaHandler.hpp"
#include "sparta/kernel/Scheduler.hpp"

namespace sparta {
    class Clock;

    namespace app {
        class Simulation;
    }

    namespace statistics {
        class StreamController;
    }

namespace control {

/*!
 * \brief Temporary run control interface
 * \note Assumes single core. Blocking runs abortable by ctrl+C
 */
class TemporaryRunControl {
    sparta::app::Simulation* sim_;
    sparta::Scheduler* sched_;
    sparta::SpartaHandler icount_end_handler_;

    /*!
     * \brief Find a clock in the simulation's clock tree using its name
     */
    sparta::Clock* findClock_(const std::string& clk_name) const;

    /*!
     * \brief Callback for end of run
     */
    void runIcountEnd_();

public:
    TemporaryRunControl() = delete;
    TemporaryRunControl(const TemporaryRunControl&) = delete;
    TemporaryRunControl(TemporaryRunControl&&) = delete;
    TemporaryRunControl& operator==(const TemporaryRunControl&) = delete;

    TemporaryRunControl(sparta::app::Simulation* sim, sparta::Scheduler* sched);

    ~TemporaryRunControl();

    void setStreamController(const std::shared_ptr<statistics::StreamController> & controller);

    std::shared_ptr<statistics::StreamController> & getStreamController();

    uint64_t getCurrentCycle(const std::string& clk_name) const;

    uint64_t getCurrentCycle(const sparta::Clock* clk=nullptr) const;

    uint64_t getCurrentInst() const;

    sparta::Scheduler::Tick getCurrentTick() const;

    /*!
     * \brief Run up to I instructions from current instruction count
     */
    void runi(uint64_t instruction_max=std::numeric_limits<uint64_t>::max());

    //void runi_abs(uint64_t instruction_max_absolute=std::numeric_limits<uint64_t>::max()) {
    //    // Set up icount trigger
    //    sim->runRaw_(sparta::Scheduler::INDEFINITE);
    //    // Destroy icount trigger
    //}

    /*!
     * \brief Run up to C cycles from the current cycle count
     * \param cycles_max Maximum number of cycles to execute in the call
     * \param clk_name Clock on which to count cycles
     */
    void runc(uint64_t cycles_max, const std::string& clk_name);

    /*!
     * \brief Run up to C cycles from the current cycle count
     * \param cycles_max Maximum number of cycles to execute in the call
     * \param clk Pointer to sparta clock on which to count cycles
     * \todo Return a value indicating whether the target cycles were reached!
     */
    void runc(uint64_t cycles_max, const sparta::Clock* clk=nullptr);

    /*!
     * \brief Run unconstrained. Triggers may end the run, hoewever
     */
    void run();

    //void stop() {
    //    // Not really applicable in a blocking run control.
    //}

    /*!
     * \brief Handle a Ctrl+C from PythonInterpreter. Stop the scheduler on the
     * next tick edge.
     */
    void asyncStop();

private:

    /*!
     * \brief Stub for running the simulator
     */
    void runStub_(sparta::Scheduler::Tick ticks);

    //! Statistics stream controller. Used for starting/stopping listener
    //! objects, and forcing data flushes.
    std::shared_ptr<statistics::StreamController> stream_controller_;
};

} // namespace control
} // namespace sparta

#pragma once
