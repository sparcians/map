/*!
 * \file StandaloneThread_test1.cpp
 * \brief Negative test for SimDB's AsyncTaskEval class.
 *
 * This test runs in its own dedicated CPP main() since it verifies
 * exceptions based on static counter values. Running this negative
 * test together in the same unit test program as other tests would
 * likely cause unexpected behavior.
 */

#include "simdb/test/SimDBTester.hpp"

#include "simdb/async/AsyncTaskEval.hpp"

TEST_INIT

using simdb::AsyncTaskEval;

//! Helper task which does nothing when called up on
//! the worker thread. Used for exception testing.
class NoOpTask : public simdb::WorkerTask
{
public:
    ~NoOpTask() = default;
private:
    void completeTask() override final {
    }
};

void testWorkerThreadUsageExceptions()
{
    simdb::TimerThread::enableStressTesting();
    std::vector<std::unique_ptr<AsyncTaskEval>> task_threads;
    EXPECT_EQUAL(AsyncTaskEval::getCurrentNumTaskThreadsCreated(), 0);

    //Start by maxing out all of the task threads that we *are* allowed to create.
    size_t task_thread_count = 0;
    while (task_thread_count < AsyncTaskEval::getMaxTaskThreadsAllowed()) {
        EXPECT_NOTHROW(task_threads.emplace_back(new AsyncTaskEval));
        EXPECT_NOTHROW(
            task_threads.back()->addWorkerTask(
                std::unique_ptr<simdb::WorkerTask>(new NoOpTask))
        );
        ++task_thread_count;
    }

    //Now try to make just one more... this should throw.
    //  (not yet, but soon)
    EXPECT_NOTHROW(task_threads.emplace_back(new AsyncTaskEval));

    //If we attempt to put any bit of work on this last
    //worker thread, it should throw. The reason why it
    //throws in the call to addWorkerTask() and not from
    //its constructor is that the actual worker thread
    //is not instantiated until the first task is placed
    //in the work queue via addWorkerTask()
    EXPECT_THROW(
        task_threads.back()->addWorkerTask(
            std::unique_ptr<simdb::WorkerTask>(new NoOpTask))
    );

    task_threads.clear();
    simdb::TimerThread::disableStressTesting();
}

int main()
{
    testWorkerThreadUsageExceptions();

    REPORT_ERROR;
    return ERROR_CODE;
}
