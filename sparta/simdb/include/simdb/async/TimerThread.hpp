// <TimerThread> -*- C++ -*-

#pragma once

#include "simdb/Errors.hpp"

#include <atomic>
#include <cstdint>
#include <chrono>
#include <iostream>
#include <thread>
#include <memory>
#include <stdexcept>
#include <vector>

namespace simdb {

/*!
 * \brief Interruption class. We put one of these into the
 * AsyncTaskEval's work queue. This throws an exception
 * when the worker thread gets to this item in the queue,
 * and this exception type is caught in order to break
 * out of the infinite consumer loop.
 */
class InterruptException : public std::exception
{
public:
    const char * what() const noexcept override {
        return "Infinite consumer loop has been interrupted";
    }

private:
    //Not to be created by anyone but the WorkerInterrupt
    InterruptException() = default;
    friend class WorkerInterrupt;
};

/*!
 * \brief Thread utility used for fixed-interval execution
 * of asynchronous tasks.
 */
class TimerThread
{
public:
    //! Types of timer intervals. Currently, this utility
    //! only supports fixed-rate execution.
    enum class Interval : int8_t {
        FIXED_RATE
    };

    //! Give the timer the fixed wall clock interval in
    //! seconds. Your execute_() method will be called
    //! every 'n' seconds like so:
    //!
    //! \code
    //!   class HelloWorld : public TimerThread {
    //!   public:
    //!     HelloWorld() : TimerThread(2.5)
    //!     {}
    //!   private:
    //!     void execute_() override {
    //!       std::cout << "Hello, world!" << std::endl;
    //!     }
    //!   };
    //!
    //!   HelloWorld obj;
    //!   obj.start();
    //!     // "Hello, world!" printed after 2.5 seconds
    //!     // "Hello, world!" printed after 5.0 seconds
    //!     // "Hello, world!" printed after 7.5 seconds
    //! \endcode
    //!
    //! Notes:
    //!   - The execute_() method is called for the very first
    //!     time after the interval has elapsed. It is not called
    //!     immediately from the TimerThread constructor.
    //!   - The timer interval is not guaranteed and may vary
    //!     at runtime and show different intervals from one
    //!     program execution to another.
    //!   - If your execute_() callback takes more than the
    //!     interval specified, your callback will be called
    //!     again immediately. It will not sleep before calling
    //!     your method.
    TimerThread(const Interval interval, const double seconds) :
        interval_seconds_(seconds)
    {
        (void) interval;
    }

    //! When the timer goes out of scope, the execute_() callbacks
    //! will be stopped.
    virtual ~TimerThread() {
        if (stress_testing_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        stop();
    }

    static uint64_t getMaxTaskThreadsAllowed() {
        return TimerThread::max_task_threads_allowed_;
    }

    static uint64_t getCurrentNumTaskThreadsCreated() {
        return TimerThread::current_num_task_threads_;
    }

    //! Bugs uncovered in this class will typically be sporadic
    //! due to timing of the background thread(s). This method
    //! is used for testing purposes only. It injects pauses to
    //! purposely draw out otherwise very sporadic bugs.
    //!
    //! \warning Calling this method in production code will
    //! slow down execution of your program.
    static void enableStressTesting() {
        stress_testing_ = true;
    }

    //! Disable stress testing. See the comment above about
    //! this test-only setting.
    static void disableStressTesting() {
        stress_testing_ = false;
    }

    //! Call this method from the main thread to start
    //! timed execution of your execute_() method.
    void start() {
        if (thread_ == nullptr) {
            is_running_ = true;
            thread_.reset(new std::thread([&]() {
                start_();
            }));

            if (TimerThread::current_num_task_threads_ >=
                TimerThread::max_task_threads_allowed_)
            {
                throw DBException(
                    "Too many task thread objects have been created (the current "
                    "limit is ") << TimerThread::max_task_threads_allowed_ << ")";
            }

            ++TimerThread::current_num_task_threads_;
        }
    }

    //! Call this method from the main thread to stop
    //! timed execution of your execute_() method.
    //! You may NOT call this method from inside your
    //! execute_() callback, or the timer thread will
    //! not be able to be torn down.
    void stop() {
        is_running_ = false;
        if (thread_ != nullptr) {
            thread_->join();
            thread_.reset();

            if (TimerThread::current_num_task_threads_ > 0) {
                --TimerThread::current_num_task_threads_;
            }
        }
    }

    //! Ask if this timer is currently executing at
    //! regular intervals on the background thread.
    //! This does not mean that it is in the middle
    //! of calling the client background thread code,
    //! just that the thread itself is still alive.
    bool isRunning() const {
        return thread_ != nullptr;
    }

private:
    //! The timer's delayed start callback
    void start_() {
        sleepUntilIntervalEnd_();
        intervalFcn_();
    }

    //! The timer's own interval callback which includes
    //! your execute_() implementation and sleep duration
    //! calculation.
    void intervalFcn_() {
        while (is_running_) {
            //Get the time before calling the user's code
            const Time interval_start = getCurrentTime_();

            try {
                execute_();
            } catch (const InterruptException &) {
                is_running_ = false;
                continue;
            }

            //Take the amount of time it took to execute the user's
            //code, and use that info to sleep for the amount of time
            //that puts the next call to execute_() close to the fixed
            //interval.
            auto interval_end = getCurrentTime_();
            std::chrono::duration<double> user_code_execution_time =
                interval_end - interval_start;

            const double num_seconds_into_this_interval =
                user_code_execution_time.count();
            sleepUntilIntervalEnd_(num_seconds_into_this_interval);
        }
    }

    //! Go to sleep until the current time interval has expired.
    //!
    //!     |----------------|----------------|----------------|
    //!     ^
    //! (sleeps until........^)
    //!
    //!     |----------------|----------------|----------------|
    //!                           ^
    //!                       (sleeps until...^)
    void sleepUntilIntervalEnd_(const double offset_seconds = 0) {
        const double sleep_seconds = interval_seconds_ - offset_seconds;
        if (sleep_seconds > 0) {
            auto sleep_ms = std::chrono::milliseconds(static_cast<uint64_t>(sleep_seconds * 1000));
            std::this_thread::sleep_for(sleep_ms);
        }
    }

    typedef std::chrono::time_point<std::chrono::high_resolution_clock> Time;

    //! Get the current time. Used in sleep_for calculation.
    Time getCurrentTime_() const {
        return std::chrono::high_resolution_clock::now();
    }

    //! User-supplied method which will be called at regular
    //! intervals on a worker thread.
    virtual void execute_() = 0;

    const double interval_seconds_;
    std::unique_ptr<std::thread> thread_;
    bool is_running_ = false;

    static constexpr uint64_t max_task_threads_allowed_ = 2;
    static std::atomic<uint64_t> current_num_task_threads_;
    static bool stress_testing_;
};

} // namespace simdb


