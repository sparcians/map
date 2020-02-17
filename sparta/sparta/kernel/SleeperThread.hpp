
/**
 * \file SleeperThread.hpp
 *
 *
 */

#ifndef __SLEEPER_THREAD_H__
#define __SLEEPER_THREAD_H__

#include "sparta/kernel/SleeperThreadBase.hpp"

namespace sparta{
/**
 * \class SleeperThread
 * \brief A singleton class that when enabled, is responsible for asserting that the attached schedulers
 *        are progressing in tick time on a
 *        reasonable basis, and for checking whether we should time out of simulation.
 *
 * This class spawns a new thread at finalization time, and reaps
 * the thread at destruction.
 *
 * The class has pause() and unpause() methods that should be
 * called when the scheduler is not running, to avoid
 * assertions about scheduler progress when the scheduler is not
 * actually running!
 *
 *
 * <H2> Simulation timeout and Infinite Loop Protection </H2>
 *
 * The SleepThread is smart to try and detect infinite loops or no
 * activity during simulation. There is a background thread that wakes
 * up ever 30 seconds or so and asserts that simulation has advanced
 * in ticks. If the scheduler has not advanced in ticks, an exception
 * is thrown.
 *
 * For some cases, this special sleep thread should be turned off. If
 * simulation will not be advancing in ticks frequently or are doing
 * special debugging. To disable it,
 *
 * \code
 * // Disable infinite loop protection.
 * sparta::SleeperThread::getInstance()->disableInfiniteLoopProtection();
 * \endcode
 *
 * \note This should be called before the call to the scheduler's
 *       run() method. Though this will disable the thread at any
 *       time, even if called after run. The thread will be killed.
 *
 * There is also a command line option to disable it as well.
 * \code
 * --disable-infinite-loop-protection
 * \endcode
 *
 *
 * \note Both turning off the sleeper thread and setting the sleep
 *       interval must be done before scheduler finalization.
 *
 * \note This class does introduce a new thread into the simulator. If you do not want this, explicity call SleeperThread::getInstance()->neverCreateAThread();
 *
 */
class SleeperThread : public SleeperThreadBase
{

private:

    //! Used as an entry point for the new thread.
    static void* invokeThread_(void* context)
    {
        static_cast<SleeperThread*>(context)->sleeperThreadCtx_();
        return nullptr;
    }


    //! This is where the infinite loop protector thread lives almost
    // it's whole life.
    void sleeperThreadCtx_()
    {
        // mark the time we should timeout each scheduler.
        std::vector<std::chrono::nanoseconds> timeout_times; //! The time at which we should timeout a scheduler.
        if (timeout_enabled_)
        {
            for (auto scheduler : schedulers_)
            {
                timeout_times.emplace_back(scheduler->getRunWallTime<std::chrono::nanoseconds>() + timeout_time_);
            }
        }

        struct timespec sleep_timespec;
        // This thread keeps going until the SleeperThread destructs.
        while(keep_going_)
        {
            // sleep then check if main thread progressed.

            // wait on the condition variable or for 30 seconds.
            // This is necessary so for short runs of simulation,
            // runs that are only a few seconds, when simulation is
            // over, we can reap this thread without waiting the full
            // 30 seconds.
            clock_gettime(CLOCK_REALTIME, &sleep_timespec);
            sleep_timespec.tv_sec += sleep_interval_.count();
            pthread_cond_timedwait(&sleeper_cond_,
                                   &sleeper_mutex_,
                                   &sleep_timespec);

            // pause if necessary.
            pthread_mutex_lock(&pause_mutex_);


            // ------------------------------------------
            // check if we should timeout.
            if (timeout_enabled_)
            {
                std::chrono::nanoseconds runtime;
                uint32_t i = 0;
                for (auto scheduler : schedulers_)
                {

                    // whether we should use wall time or the processes cpu time.
                    if(timeout_clock_is_wall_)
                    {
                        runtime = scheduler->getRunWallTime<std::chrono::nanoseconds>();
                    }
                    else
                    {
                        runtime = scheduler->getRunCpuTime<std::chrono::nanoseconds>();
                    }
                    // we need to timeout.
                    if( runtime > timeout_times[i])
                    {
                        if(clean_timeout_)
                        {
                            std::cout << "Timeout reached. Stopping simulation cleanly." << std::endl;
                            // clearly stopRunning is not const since it changes the scheduler.
                            // so explicity allow this
                            // Notice my loop will call stopRunning on all the schedulers assigned eventually.
                            const_cast<Scheduler*>(scheduler)->stopRunning();
                            // Notice that we do not need to set keep_going_ to false here.
                            // we could I guess, it should not hurt. But it is the scheduler's
                            // responsibility to stop and reap this thread, and that also means ending
                            // this loop.
                        }
                        else
                        {
                            // We need to stop the scheduler first.. maybe. to be safe.
                            const_cast<Scheduler*>(scheduler)->stopRunning();
                            std::cerr << "Timeout reached. Exiting immediately" << std::endl;
                            throw SpartaException("Simulation timeout reached!");
                        }
                    }
                }
            }

            // ----------------------------------------
            // Now check if we need to exit because of an infinite loop.

            if(protect_loop_enabled_)
            {
                uint32_t idx = 0;
                for (auto scheduler : schedulers_)
                {
                    // check if we progressed.
                    std::chrono::seconds new_run_time_seconds = scheduler->getRunCpuTime<std::chrono::seconds>();
                    if(new_run_time_seconds - last_time_check_ >= sleep_interval_)
                    {
                        if(scheduler->isRunning() && scheduler->getCurrentTick() == prev_ticks_[idx])
                        {
                            // We throw an exception.
                            std::cerr << SPARTA_CURRENT_COLOR_RED
                                      << "Loop Detected. Scheduler has not progressed in time for a while!"
                                      << SPARTA_CURRENT_COLOR_NORMAL << std::endl;
                            std::cerr << "Next Continuing Event time: "
                                      << scheduler->getNextContinuingEventTime() << std::endl;

                            scheduler->printNextCycleEventTree(std::cerr, 0, 0, prev_ticks_[idx]);
                            const Scheduleable* current_event = scheduler->getCurrentFiringEvent();
                            std::cerr << SPARTA_CURRENT_COLOR_MAGENTA << " --> Scheduler: Currentlty Firing "
                                      << current_event->getLabel()
                                      << " at tick: " << scheduler->getCurrentTick() << std::endl;
                            throw SpartaException("Infinite loop was detected during simulation!");
                        }

                        prev_ticks_[idx] = scheduler->getCurrentTick();
                        last_time_check_ = new_run_time_seconds;
                    }
                    ++idx;
                }
            }


            pthread_mutex_unlock(&pause_mutex_);
        }


    }


public:

    SleeperThread() = default;

    /**
     * \brief easy way of implementing the singleton.
     */
    static SleeperThreadBase* getInstance()
    {
        if (sleeper_thread_ == nullptr) {
            sleeper_thread_.reset(new SleeperThread);
        }
        return sleeper_thread_.get();
    }

    /*!
     * \brief Before *ever* calling the static getInstance() method
     * above, you can disable the sleeper thread by calling this
     * method first.
     *
     *     int main()
     *     {
     *         sparta::SleeperThread::disableForever();
     *         sparta::SleeperThread * sleeper = sparta::SleeperThread::getInstance();
     *
     *         sleeper->foo(); //...no effect
     *         sleeper->bar(); //...no effect
     *     }
     */
    static void disableForever()
    {
        if (sleeper_thread_ != nullptr) {
            throw SpartaException(
                "You may not call the SleeperThread::disableForever() \n"
                "method at any time after calling SleeperThread::getInstance(). \n"
                "If you want to disable this singleton entirely, it is suggested \n"
                "that you do so before even creating a simulation object.");
        }
        sleeper_thread_.reset(new NullSleeperThread);
    }

    /**
     * \brief Set the timeout.
     * \param time_out In nanoseconds, the timeout period
     * \param clean_exit Set whether we want to exit cleanly or throw
     *                   an exception.
     * \param wall_clock Whether we should use wall time or the processes cpu time.
     */
    void setTimeout_(const std::chrono::nanoseconds time_out,
                     const bool clean_exit,
                     const bool wall_clock) override
    {
        timeout_time_ = time_out;
        timeout_enabled_ = (time_out.count() != 0);
        clean_timeout_ = clean_exit;
        timeout_clock_is_wall_ = wall_clock;
    }

    //! Override the default sleep interval
    void setInfLoopSleepInterval(const std::chrono::seconds& interval) override
    {
        sleep_interval_ = interval;
    }

    //! Instruct not to check for infinite loops.
    void disableInfiniteLoopProtection() override
    {
        protect_loop_enabled_ = false;
        sparta_assert(enabled_ == false, "SleeperThread was already finalized! Cannot disableInfiniteLoopProtection at this point!");
    }

    /**
     * \brief enforce that absolutely do not create an extra thread.
     * This class is smart enough to only create a thread if there is a timeout or infinite loop protection but
     * this is a guarantee switch that will enforce the thread never spawns. This way if someone trys to set a timeout or enable infinite loop protection
     * an exception is thrown.
     */
    void neverCreateAThread() override
    {
        sparta_assert(enabled_ == false, "SleeperThread was already finalized! It's too late to tell me not to create a thread!");
        never_create_a_thread_ = true;
    }

    /**
     * \brief Initialize the Sleeper thread
     * The sleeper thread does not actually spawn a new thread unless this function is called.
     */
    void finalize()  override
    {
        sparta_assert(enabled_ == false);
        enabled_ = true;
        main_thread_id_ = pthread_self();

        // we are only going to kick off the sleeper thread if
        // we are gonna use it.
        if (!never_create_a_thread_)
        {
            if (timeout_enabled_ || protect_loop_enabled_)
            {
                thread_spawned_ = true;
                // initialize some pthread stuff
                pthread_condattr_t cnd_attrs;
                sparta_assert(pthread_condattr_init(&cnd_attrs) == 0);
                //sparta_assert(pthread_condattr_setclock(&cnd_attrs, CLOCK_REALTIME) ==0 );
                sparta_assert(pthread_cond_init(&sleeper_cond_, &cnd_attrs)==0);
                sparta_assert(pthread_mutex_init(&sleeper_mutex_, nullptr)==0);

                //std::cout << " spawning sleeper thread. " << std::endl;

                // kick off the thread.
                if(pthread_create(&protector_thread_, 0, &SleeperThread::invokeThread_, this) != 0)
                {
                    throw SpartaException("Scheduler failed to setup watchdog thread in the background");
                }

            }
        }
        else
        {
            // throw an exception if someone tried to use the timeout features while we are guaranteeing that the SleeperThread never spawns.
            if (timeout_enabled_)
            {
                throw SpartaException("Cannot set a simulation timeout because the SleeperThread was instructed to never spawn an extra thread!");
            }
        }
    }

    /**
     * \brief by invoking this method you are signing up the sleeper
     * thread, if the SleeperThread is enabled, to check this scheduler as well.
     */
    void attachScheduler(const Scheduler* scheduler) override
    {
        schedulers_.emplace_back(scheduler);
        prev_ticks_.emplace_back(0);
    }

    /*!
     * \brief Remove a scheduler from the SleeperThread. Schedulers call this
     * method from their destructors, so this method is typically not called
     * explicitly by other classes, such as sparta::app::Simulation.
     * \note If the scheduler passed in was never registered with a call to
     * SleeperThread::attachScheduler(), then an exception will be thrown,
     * UNLESS you specify the 'throw_if_scheduler_not_found' argument to be
     * false.
     * \return Returns true if the scheduler was removed, false if the scheduler
     * was not recognized or could not be removed for any other reason.
     */
    bool detachScheduler(const Scheduler* scheduler,
                         const bool throw_if_scheduler_not_found = true) override
    {
        auto iter = std::find(schedulers_.begin(), schedulers_.end(), scheduler);
        if (iter != schedulers_.end()) {
            schedulers_.erase(iter);
            return true;
        }
        if (throw_if_scheduler_not_found) {
            throw SpartaException("Unrecognized scheduler passed to SleeperThread::detachScheduler()");
        }
        return false;
    }

    //! Reap the thread that we spawned.
    ~SleeperThread()
    {
        // flip the boolean so the thread will exit it's loop.
        keep_going_ = false;
        // Better do this or the could potentially remain paused!!!
        unpause();
        if (thread_spawned_){

            // Now wake up the thread if it's asleep so it can exit, and we can reap it.
            pthread_cond_signal(&sleeper_cond_);

            // Now reap the thread.
            if(pthread_join(protector_thread_, 0) != 0)
            {
                sparta_abort("Scheduler failed to join with slave watchdog thread");
            }
        }
    }

    //! Force the sleeper thread to pause. Should be called before the Scheduler exits running
    void pause() override
    {
        if (thread_spawned_){
            paused_ = true;
            pthread_mutex_lock(&pause_mutex_);
        }
    }

    //! Release the sleeper thread to continue.
    void unpause() override
    {
        // b/c "Attempting to unlock the mutex if it was not locked results in undefined behavior"
        // linux.die.net/man/3/pthread_mutex_unlock
        if (thread_spawned_){
            if(paused_)
            {
                pthread_mutex_unlock(&pause_mutex_);
                paused_ = false;
            }
        }
    }

private:

    static std::unique_ptr<SleeperThreadBase> sleeper_thread_; //! Hold the singleton instance
    std::vector<const Scheduler*> schedulers_; //! A list of scheduler's we check up on
    std::vector<uint64_t> prev_ticks_; //! The previous tick from the last time we slept for the scheduler's we are checking on.

    bool never_create_a_thread_=false; //! Guarantee that a thread is never spawned.
    bool enabled_ = false; //! Has the singleton already been enabled.
    bool thread_spawned_ = false;
    pthread_t main_thread_id_; //! The thread id of the thread that constructed this class.
    std::chrono::seconds sleep_interval_ = std::chrono::seconds(30); //! The amount of time to sleep between checks for progress.
    bool protect_loop_enabled_ = true; //! Should we protect against loops
    pthread_t protector_thread_; //! thread id of the sleeper watchdog thread.
    pthread_mutex_t sleeper_mutex_; //! Used to put the thread to sleep.
    pthread_cond_t sleeper_cond_; //! Used to wake the thread up when simulation is done.


    std::chrono::seconds last_time_check_ = std::chrono::seconds::zero(); //! The last time we checked the scheduler's running time. in seconds.
    pthread_mutex_t pause_mutex_ = PTHREAD_MUTEX_INITIALIZER; //! Used to put the thread to sleep.
    bool paused_ = false; //! Are we paused or not.
    bool keep_going_ = true; //! A boolean to keep the spawned thread alive until we flip this boolean at destruction.

    bool clean_timeout_   = false; //! Should we exit cleaning on a timeout.
    bool timeout_enabled_ = false; //! Do we need to timeout at all.
    std::chrono::nanoseconds timeout_time_; //! when the current time wall time is > timeout_time_, then we should exit.
    bool timeout_clock_is_wall_ = false; //! Whether or not we are using a wall clock as the timer.

};

}
// __SLEEPER_THREAD_H__
#endif
