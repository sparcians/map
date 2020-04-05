
#pragma once

#include "sparta/kernel/Scheduler.hpp"
#include "sparta/events/Scheduleable.hpp"

namespace sparta {

/*!
 * \brief Base class for all SleeperThread implementations
 */
class SleeperThreadBase
{
private:
    //! Implementation-specific setTimeout() code
    virtual void setTimeout_(
        const std::chrono::nanoseconds time_out,
        const bool clean_exit,
        const bool wall_clock) = 0;

public:
    virtual ~SleeperThreadBase() {}

    /**
     * \brief Set the timeout. Setting this means a sleeper
     * thread will exist.
     * \param duration The duration of the run.
     * \param clean_exit set whether we want to exit cleanly or throw
     * an exception.
     */
    template <typename DurationT>
    void setTimeout(const DurationT& duration,
                    const bool clean_exit,
                    const bool wall_clock)
    {
        auto time_out = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
        setTimeout_(time_out, clean_exit, wall_clock);
    }

    //! Override the default sleep interval
    virtual void setInfLoopSleepInterval(
        const std::chrono::seconds & interval) = 0;

    //! Instruct not to check for infinite loops.
    virtual void disableInfiniteLoopProtection() = 0;

    //! Enforce that extra threads are never created
    virtual void neverCreateAThread() = 0;

    //! Initialize the sleeper thread. The sleeper thread does not
    //! actually spawn a new thread unless this function is called.
    virtual void finalize() = 0;

    //! By invoking this method you are signing up the sleeper
    //! thread to check this scheduler as well.
    virtual void attachScheduler(
        const Scheduler * scheduler) = 0;

    /*!
     * \brief Remove a scheduler from the SleeperThread. Schedulers call this
     * method from their destructors, so this method is typically not called
     * explicitly by other classes, such as sparta::app::Simulation.
     *
     * \return Returns true if the scheduler was removed, false if the scheduler
     * was not recognized or could not be removed for any other reason.
     */
    virtual bool detachScheduler(
        const Scheduler * scheduler,
        const bool throw_if_scheduler_not_found = true) = 0;

    //! Force the sleeper thread to pause. Should be called before the
    //! Scheduler exits running.
    virtual void pause() = 0;

    //! Release the sleeper thread to continue.
    virtual void unpause() = 0;
};

/*!
 * \brief SleeperThread null implementation. This is used when the
 * SleeperThread is to be disabled entirely.
 */
class NullSleeperThread : public SleeperThreadBase
{
public:
    void setInfLoopSleepInterval(const std::chrono::seconds &) override {}
    void disableInfiniteLoopProtection() override {}
    void neverCreateAThread() override {}
    void finalize() override {}
    void attachScheduler(const Scheduler *) override {}
    bool detachScheduler(const Scheduler *, const bool) override { return false; }
    void pause() override {}
    void unpause() override {}
    void setTimeout_(const std::chrono::nanoseconds, const bool, const bool) override {}
};

} // namespace sparta

