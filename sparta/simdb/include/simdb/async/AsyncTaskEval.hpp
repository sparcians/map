// <AsyncTaskEval> -*- C++ -*-

#pragma once

#include "simdb/async/TimerThread.hpp"
#include "simdb/async/ConcurrentQueue.hpp"
#include "simdb/ObjectManager.hpp"
#include "simdb/Errors.hpp"

#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <functional>

namespace simdb {

/*!
 * \brief Base class used for all tasks that are given
 * to the worker task queue. The completeTask() method
 * will be called when this task's turn is up on the
 * worker thread.
 */
class WorkerTask
{
public:
    virtual ~WorkerTask() {}
    virtual void completeTask() = 0;

    uint64_t getId() const {
        return id_;
    }

protected:
    WorkerTask() : id_(incId_())
    {}

private:
    static uint64_t incId_() {
        static uint64_t id = 0;
        return id++;
    }
    const uint64_t id_;
};

/*!
 * \brief Specialized worker task used in order to break
 * out of the consumer thread without synchronously asking
 * it to do so.
 */
class WorkerInterrupt : public WorkerTask
{
protected:
    //! WorkerTask implementation
    void completeTask() override {
        throw InterruptException();
    }
};

/*!
 * \brief If you want to register your class objects for
 * pre-flush notifications from the AsyncTaskEval, you
 * should subclass from this Notifiable class.
 *
 * The notifyTaskQueueAboutToFlush() method will get
 * called right before end-of-simulation task queue
 * flushes.
 */
class Notifiable
{
public:
    virtual ~Notifiable() = default;

    std::weak_ptr<Notifiable> getWeakPtr() {
        return std::weak_ptr<Notifiable>(self_ptr_);
    }

    virtual void notifyTaskQueueAboutToFlush() = 0;

protected:
    Notifiable() : self_ptr_(this, [](Notifiable*){})
    {}

private:
    std::shared_ptr<Notifiable> self_ptr_;
};

/*!
 * \brief Thread-safe task queue. Used together with the
 * AsyncTaskEval class to create a single-producer, single-
 * consumer queue of work requests.
 *
 *   -> main thread (producer) ==> (work to do) ==>  (put in queue)
 *   -> main thread (producer) ==> (work to do) ==>  (put in queue)
 *                                                         |
 *                                                         |
 *                                                   [work packet]
 *                                                   [work packet]
 *                                                        ...
 *
 *   -> work thread (consumer) ==================> ^^^^^^^^^^^^^^^^^
 *                                                (consume data queue)
 *                                                         |
 *                                              [lots of work at once]
 *                                                       /   \
 *                                                     [Database]
 */
class WorkerTaskQueue
{
public:
    //! Add a task you wish to evaluate off the main thread
    void addTask(std::unique_ptr<WorkerTask> task) {
        task_queue_.emplace(task.release());
    }

    //! Evaluate every task that has been queued up. This is
    //! typically called by a worker thread, but may be called
    //! from the main thread at synchronization points like
    //! simulation pause/stop.
    void flushQueue() {
        std::unique_ptr<WorkerTask> task;
        while (task_queue_.try_pop(task)) {
            task->completeTask();
        }
    }

private:
    // Give the AsyncTaskController the ability to pop work
    // packets out of this queue, and evaluate them itself.
    // This is needed for scenarios where there are several
    // ObjectManager's writing to different database files,
    // yet all WorkerTask's are in this single queue:
    //
    //     ObjMgrA ... TaskA -
    //                        |
    //                         ----> [taskA1, taskB1, taskA2, ...]
    //                        |
    //     ObjMgrB ... TaskB -
    //
    // It needs to "deinterleave" the tasks so that it can
    // do two separate database transactions (two *in this
    // example* - there could be more):
    //
    //     ObjMgrA.safeTransaction([&]() {
    //         for task in TaskQueueA {
    //             task->completeTask()
    //         }
    //     });
    //
    //     ObjMgrB.safeTransaction([&]() {
    //         for task in TaskQueueB {
    //             task->completeTask()
    //         }
    //     });
    bool popQueue_(std::unique_ptr<WorkerTask> & task) {
        return task_queue_.try_pop(task);
    }
    friend class AsyncTaskController;

    ConcurrentQueue<std::unique_ptr<WorkerTask>> task_queue_;
};

class AsyncTaskEval;

/*!
 * \class AsyncTaskController
 *
 * \brief This class is used in order to allow more than
 * one database file to be written to in the same process.
 * Without this controller, you would put WorkerTask's onto
 * your ObjectManager's AsyncTaskEval, and there would be
 * a thread dedicated to that AsyncTaskEval to consume the
 * work for that database file. But a 1-to-1 link between
 * an ObjectManager and a thread (TimerThread) poses some
 * performance problems - too many threads for little or
 * no gain, being bottlenecked around disk I/O.
 *
 * To prevent performance from degrading, all ObjectManager's
 * can be added to just a single AsyncTaskController, and
 * they will all share the same background thread for their
 * individual tasks. See ObjectManager.h for more details
 * on how to link these two classes.
 */
class AsyncTaskController
{
public:
    explicit AsyncTaskController(const double interval_seconds = 0.1) :
        timed_eval_(new TimedEval(interval_seconds, this))
    {}

    ~AsyncTaskController() = default;

    //! Allow users of this thread object to register themselves
    //! for notifications that the worker queue is about to be
    //! flushed.
    void registerForPreFlushNotifications(Notifiable & notif) {
        pre_flush_listeners_.emplace_back(notif.getWeakPtr());
    }

    //! Send out a notification to all registered listeners that
    //! we are about to flush the worker queue.
    void emitPreFlushNotification();

    //! Force a synchronous flush of all WorkerTask's that
    //! are currently in this queue.
    void flushQueue() {
        flushQueues_();
    }

    //! Wait for the worker queue to be flushed / consumed,
    //! and stop the consumer thread.
    //!
    //! \warning DO NOT call this method from any WorkerTask
    //! subclass' completeTask() method. If the completeTask()
    //! method was being invoked from this controller's own
    //! consumer thread (which is usually the case), this
    //! method will hang. It is safest to call this method
    //! from code that you know is always on the main thread,
    //! for example in setup or teardown / post-processing
    //! code in a simulation.
    void stopThread() {
        //Put a special interrupt packet in the queue. This
        //does nothing but throw an interrupt exception when
        //its turn is up.
        std::unique_ptr<WorkerTask> interrupt(new WorkerInterrupt);
        task_queue_.addTask(std::move(interrupt));

        //Join the thread and just wait forever... (until the
        //exception is thrown).
        timed_eval_->stop();
    }

private:
    //! TimerThread implementation. Called at regular
    //! intervals on a worker thread.
    class TimedEval : public TimerThread
    {
    private:
        TimedEval(const double interval_seconds,
                  AsyncTaskController * task_eval) :
            TimerThread(TimerThread::Interval::FIXED_RATE,
                        interval_seconds),
            task_eval_(task_eval)
        {}

        void execute_() override {
            task_eval_->flushQueues_();
        }

        AsyncTaskController *const task_eval_;
        friend class AsyncTaskController;
    };

    //! Add the provided AsyncTaskEval. These AsyncTaskEval.'s are
    //! basically back pointers we can use to emit pre-flush
    //! notifications to any client code that registered itself
    //! for these notifications.
    void addTaskQueue_(AsyncTaskEval * task_queue)
    {
        client_task_queues_.insert(task_queue);
    }

    //! When a WorkerTask is added to an ObjectManager's task
    //! queue, if that ObjectManager belongs to this controller
    //! it will reroute the task to our work queue through this
    //! method.
    void addWorkerTask_(
        ObjectManager * sim_db,
        std::unique_ptr<WorkerTask> task)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        sim_dbs_by_task_id_[task->getId()] = sim_db;
        task_queue_.addTask(std::move(task));

        if (!timed_eval_->isRunning()) {
            timed_eval_->start();
        }
    }

    //! TimerThread (base class) implementation. Called
    //! periodically on a background thread.
    void flushQueues_() {
        std::unique_ptr<WorkerTask> task;

        //This controller has just a single queue of work requests.
        //The queue can have work that belongs to more than one
        //database file. For example, simultaneous SPARTA statistics
        //logging (SQLite) and branch prediction (HDF5) are completely
        //independent, yet they share this one queue (and one thread).
        //We "deinterleave" the queued work so that we can put all
        //of one database's WorkerTask into its own safeTransaction(),
        //then do the same for the next ObjectManager we are serving,
        //and so on.
        std::unordered_map<
            ObjectManager*,
            std::vector<std::unique_ptr<WorkerTask>>> db_tasks;

        //There can be WorkerTask's that were added to our queue
        //without any ObjectManager to go with it. An example of
        //when this could happen is a simulator that pushes a
        //post-simulation flush/interrupt WorkerTask into the
        //queue, even though nobody ever made an ObjectManager
        //(there were no --report options, no -z options, etc.)
        //
        //Making calling code keep track of whether they can do
        //something as harmless as flushing or interrupting an
        //empty task queue is not very user-friendly. And there
        //is also always a chance that this async task queue is
        //being used for non-SimDB background work (reusing SimDB
        //for its threading utilities without needing a database
        //for anything).
        std::vector<std::unique_ptr<WorkerTask>> no_db_tasks;

        {
            //Grab our mutex to protect the sim_dbs_by_task_id_ member variable
            std::lock_guard<std::recursive_mutex> lock(mutex_);

            while (task_queue_.popQueue_(task)) {
                auto simdb_iter = sim_dbs_by_task_id_.find(task->getId());
                if (simdb_iter == sim_dbs_by_task_id_.end()) {
                    no_db_tasks.emplace_back(task.release());
                } else {
                    const auto id = task->getId();
                    db_tasks[sim_dbs_by_task_id_[id]].emplace_back(task.release());
                }
            }
        }

        std::unordered_map<
            std::string,
            std::vector<std::unique_ptr<WorkerTask>>> db_tasks_by_db_file;

        std::unordered_map<
            std::string,
            ObjectManager*> db_conns_by_db_file;

        //Perform the deinterleave. We'll be left with one queue (vector)
        //of WorkerTask's for each database file (ObjectManager) currently
        //in use.
        for (auto & db_task_queue : db_tasks) {
            const auto & db_file = db_task_queue.first->getDatabaseFile();
            auto & src_tasks = db_task_queue.second;
            auto & dst_tasks = db_tasks_by_db_file[db_file];

            for (auto & src_task : src_tasks) {
                dst_tasks.emplace_back(src_task.release());
            }
            db_conns_by_db_file[db_file] = db_task_queue.first;
        }

        assert(db_tasks_by_db_file.size() == db_conns_by_db_file.size());

        //Perform a high-level safeTransaction() for each database
        //currently in use, and inside each transaction, only evaluate
        //the WorkerTask's that belong to the ObjectManager that is
        //invoking the safeTransaction()
        auto task_queue_iter = db_tasks_by_db_file.begin();
        auto db_conn_iter = db_conns_by_db_file.begin();

        //For example, say that we have three databases in use,
        //and there are work requests for all of them right here...
        //ObjMgrA, ObjMgrB, and ObjMgrC.
        while (task_queue_iter != db_tasks_by_db_file.end()) {
            db_conn_iter->second->safeTransaction([&]() {
                try {
                    //Say we are inside the safeTransaction()
                    //for ObjMgrB. Every WorkerTask in this
                    //vector that we are looping over here
                    //is strictly for ObjMgrB's database.
                    for (auto & current_task : task_queue_iter->second) {
                        current_task->completeTask();
                    }
                } catch (const InterruptException &) {
                    //Do not report this "exception". It does
                    //not mean anything went wrong; interrupts
                    //are purposely put in the queue when the
                    //queue (or its owning class) is asked to
                    //stop the thread.
                }
            });

            //In the above example, we just committed the
            //safeTransaction() for ObjMgrB... the next
            //iteration in this while loop would be for
            //ObjMgrC/ObjMgrA (it's a map keyed off of
            //raw pointers, so there is no real ordering)
            ++task_queue_iter;
            ++db_conn_iter;
        }

        //Note that there is no safeTransaction() wrapping these
        //last WorkerTask's, since they are not associated with
        //any ObjectManager.
        try {
            for (auto & current_task : no_db_tasks) {
                current_task->completeTask();
            }
        } catch (const InterruptException &) {
            timed_eval_->stop();
        }
    }

    std::unordered_map<
        uint64_t,
        ObjectManager*> sim_dbs_by_task_id_;

    std::unordered_set<
        AsyncTaskEval*> client_task_queues_;

    WorkerTaskQueue task_queue_;
    std::vector<std::weak_ptr<Notifiable>> pre_flush_listeners_;
    std::unique_ptr<TimedEval> timed_eval_;
    mutable std::recursive_mutex mutex_;
    friend class AsyncTaskEval;
    friend class TimedEval;
};

/*!
 * \brief Use this class to evaluate code asynchronously.
 *
 * IMPORTANT: Every one of these objects will get their own
 * background thread. Don't create too many of them! One
 * of these objects can serve an unlimited number of
 * WorkerTask's, so typically you will only create one
 * AsyncTaskEval object, and add all of your tasks to it
 * one by one during simulation.
 *
 * \note There is a default limit for the total number of
 * these objects you can make. Query the methods to find
 * out if there is room for another:
 *
 *    - getMaxTaskThreadsAllowed()
 *    - getCurrentNumTaskThreadsCreated()
 */
class AsyncTaskEval
{
public:
    //! Construct a task evaluator that will execute every
    //! 'interval_seconds' that you specify.
    explicit AsyncTaskEval(const double interval_seconds = 0.1) :
        task_queue_(new WorkerTaskQueue),
        timed_eval_(new TimedEval(interval_seconds, this))
    {}

    ~AsyncTaskEval() {
        timed_eval_->stop();
    }

    static uint64_t getMaxTaskThreadsAllowed() {
        return TimerThread::getMaxTaskThreadsAllowed();
    }

    static uint64_t getCurrentNumTaskThreadsCreated() {
        return TimerThread::getCurrentNumTaskThreadsCreated();
    }

    //! Give this AsyncTaskEval shared ownership of the provided
    //! database object. This is used in order to group together
    //! WorkerTask items on the background thread and put them
    //! inside larger periodic safeTransaction() calls. For some
    //! database (DbConnProxy) implementations such as SQLite,
    //! this typically results in much faster throughput for
    //! database writes.
    //!  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .
    //!
    //!   Main thread          Worker thread
    //! ===============      =================
    //!                        (interval 1) ====>> safeTransaction([&]() {
    //!     --> task1                                  task1->completeTask()
    //!     --> task2                                  task2->completeTask()
    //!     --> task3                                  task3->completeTask()
    //!     -->  ...                               })
    //!     -->  ...
    //!                           ......
    //!
    //!                        (interval 2) ====>> safeTransaction([&]() {
    //!     --> task137                                task137->completeTask()
    //!     --> task138                                task138->completeTask()
    //!     -->  ...                               })
    //!     -->  ...
    //!                           ......
    //!
    void setSimulationDatabase(ObjectManager * obj_mgr) {
        sim_db_ = obj_mgr;
    }

    //! Tell this task queue to forward all future WorkerTask's
    //! it is given into the shared AsyncTaskController you provide.
    //! If this AsyncTaskEval already had launched its own consumer
    //! thread, it will be torn down. The controller object will start
    //! its own thread that we can leverage.
    bool addToTaskController(AsyncTaskController * ctrl) {
        if (ctrl == nullptr || sim_db_ == nullptr) {
            return false;
        }

        if (sim_db_ && sim_db_->getDbConn()) {
            flushQueue();
            if (timed_eval_->isRunning()) {
                stop_();
            }
        }

        task_controller_ = ctrl;
        task_controller_->addTaskQueue_(this);

        return true;
    }

    //! Allow users of this thread object to register themselves
    //! for notifications that the worker queue is about to be
    //! flushed.
    void registerForPreFlushNotifications(Notifiable & notif) {
        pre_flush_listeners_.emplace_back(notif.getWeakPtr());
    }

    //! Send out a notification to all registered listeners that
    //! we are about to flush the worker queue.
    void emitPreFlushNotification() {
        for (auto & listener : pre_flush_listeners_) {
            if (auto callback_obj = listener.lock()) {
                callback_obj->notifyTaskQueueAboutToFlush();
            }
        }
    }

    //! Add a task for asynchronous eval. This will start
    //! the worker thread if it is the first added task.
    void addWorkerTask(std::unique_ptr<WorkerTask> task) {
        if (task_controller_ != nullptr) {
            task_controller_->addWorkerTask_(sim_db_, std::move(task));
        } else {
            task_queue_->addTask(std::move(task));
            if (!timed_eval_->isRunning()) {
                timed_eval_->start();
            }
        }
    }

    //! Evaluate all pending tasks.
    void flushQueue() {
        //If this consumer is going to a database, wrap the
        //entire flush in a high-level transaction. This
        //typically gives much better performance than
        //doing database commits one at a time.
        if (sim_db_ != nullptr) {
            sim_db_->safeTransaction([&]() {
                task_queue_->flushQueue();
            });
        } else {
            task_queue_->flushQueue();
        }
    }

    //! Stop the consumer thread. This triggers a flush on
    //! the queue of tasks, evaluating each completeTask()
    //! method as it does so.
    //!
    //! IMPORTANT: Do not try to call 'stop()' from inside
    //! any of your WorkerTask's completeTask() methods or
    //! the thread will wait forever and block whatever
    //! thread you call this method from (typically the
    //! main thread).
    void stopThread() {
        stop_();
    }

private:
    //! TimerThread implementation. Called at regular
    //! intervals on a worker thread.
    class TimedEval : public TimerThread
    {
    private:
        TimedEval(const double interval_seconds,
                  AsyncTaskEval * task_eval) :
            TimerThread(TimerThread::Interval::FIXED_RATE,
                        interval_seconds),
            task_eval_(task_eval)
        {}

        void execute_() override {
            task_eval_->flushQueue();
        }

        AsyncTaskEval *const task_eval_;
        friend class AsyncTaskEval;
    };

    //! Teardown consumer / work thread.
    void stop_() {
        if (task_controller_ != nullptr) {
            task_controller_->stopThread();
        } else {
            //Put a special interrupt packet in the queue. This
            //does nothing but throw an interrupt exception when
            //its turn is up.
            std::unique_ptr<WorkerTask> interrupt(new WorkerInterrupt);
            task_queue_->addTask(std::move(interrupt));

            //Join the thread and just wait forever... (until the
            //exception is thrown).
            timed_eval_->stop();
        }
    }

    AsyncTaskController * task_controller_ = nullptr;
    std::unique_ptr<WorkerTaskQueue> task_queue_;
    ObjectManager * sim_db_ = nullptr;
    std::unique_ptr<TimedEval> timed_eval_;
    std::vector<std::weak_ptr<Notifiable>> pre_flush_listeners_;
    friend class TimedEval;
};

//! Send out a notification to all registered listeners that
//! we are about to flush the worker queue. Implemented down
//! here so we can call AsyncTaskEval methods.
inline void AsyncTaskController::emitPreFlushNotification()
{
    for (auto & listener : pre_flush_listeners_) {
        if (auto callback_obj = listener.lock()) {
            callback_obj->notifyTaskQueueAboutToFlush();
        }
    }
    for (auto & task_queue : client_task_queues_) {
        task_queue->emitPreFlushNotification();
    }
}

} // namespace simdb

