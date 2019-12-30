/*!
 * \file Thread_test.cpp
 * \brief Test for SimDB threading utilities
 */

#include "simdb/test/SimDBTester.h"

#include "simdb/async/TimerThread.h"
#include "simdb/async/ConcurrentQueue.h"
#include "simdb/async/AsyncTaskEval.h"

#include <math.h>
#include <iomanip>
#include <iostream>
#include <vector>

TEST_INIT;

using simdb::TimerThread;
using simdb::AsyncTaskEval;
using simdb::ConcurrentQueue;

#define PRINT_ENTER_TEST \
  std::cout << std::endl; \
  std::cout << "*************************************************************" \
            << "*** Beginning '" << __FUNCTION__ << "'" \
            << "*************************************************************" \
            << std::endl;

/*!
 * \brief Simple counter that asynchronously increments
 * an integer at fixed intervals. Note that these methods
 * do not use a mutex since the data accessed on the main
 * thread and the timer thread is just a size_t and thus
 * is inherently thread safe.
 */
class TimedCounter
{
public:
    explicit TimedCounter(const double interval_seconds) :
        timed_eval_(interval_seconds, this)
    {}

    size_t getCount() const {
        return count_;
    }

    void start() {
        timed_eval_.start();
    }

    void stop() {
        timed_eval_.stop();
    }

private:
    void execute_() {
        ++count_;
    }

    //! TimerThread implementation. Calls back into the
    //! TimedCounter::execute_() method at regular intervals
    //! on a background thread.
    class TimedEval : public TimerThread
    {
    public:
        TimedEval(const double interval_seconds,
                  TimedCounter * timed_counter) :
            TimerThread(TimerThread::Interval::FIXED_RATE,
                        interval_seconds),
            timed_counter_(timed_counter)
        {}

    private:
        void execute_() override {
            timed_counter_->execute_();
        }

        TimedCounter *const timed_counter_;
    };

    size_t count_ = 0;
    TimedEval timed_eval_;
    friend class TimedEval;
};

//! Test basic functionality of the TimerThread class
void testTimerThreadBasic()
{
    PRINT_ENTER_TEST

    //Set up a simple counter that increments every 250ms
    TimedCounter counter(0.250);
    const size_t expected_count = 10;
    size_t current_count = 0;
    size_t last_printed_current_count = 0;

    //Flag to help protect this test from running forever
    //in the event of a bug in the TimerThread code
    bool forced_exit = false;

    //Start the timer and wait until it reaches the expected count
    current_count = counter.getCount();
    EXPECT_EQUAL(current_count, 0);
    counter.start();
    while (current_count < expected_count) {
        if (forced_exit) {
            break;
        }
        if (current_count != last_printed_current_count) {
            std::cout << "Current count is " << current_count << "\n";
            last_printed_current_count = current_count;
        }
        current_count = counter.getCount();
    }

    //Cap the loop above to a few hundred seconds. In case it
    //goes haywire, at least the unit test will be killed in
    //a reasonable amount of time.
    std::thread forced_exit_thread([&]() {
        size_t sleep_count = 0;
        while (current_count < expected_count) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            ++sleep_count;
            if (sleep_count > 100) {
                forced_exit = true;
                break;
            }
        }
    });

    counter.stop();
    forced_exit_thread.join();
    EXPECT_FALSE(forced_exit);
}

//! Single-producer, single-consumer ConcurrentQueue test
void testConcurrentQueue()
{
    PRINT_ENTER_TEST

    ConcurrentQueue<size_t> queue;
    const size_t data_num_elements = 1000000;

    std::vector<size_t> recovered_data;
    bool keep_consuming = true;

    EXPECT_EQUAL(queue.size(), 0);

    //Let's start the consumer thread first to give the 'sleep_for'
    //call a better chance of getting hit
    std::thread consumer([&queue, &recovered_data, &keep_consuming]() {
        size_t item = 0;
        //Enter an infinite loop. We only break out of this when
        //we retrieve all the elements from the ConcurrentQueue,
        //or we are forced to stop (because we're actually stuck
        //in an infinite loop - the test will still fail, but we'll
        //break out of this while loop)
        while (keep_consuming) {
            if (queue.try_pop(item)) {
                recovered_data.emplace_back(item);
            } else {
                //Back off a little bit to give the producer a
                //chance to write some more data into the queue.
                //This reduces contention and mimics what we would
                //want to do in production code, as opposed to
                //spinning over the try_pop() method.
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }
        }

        //The producer is done writing data into the queue,
        //but that doesn't mean the queue is empty at this
        //time. We could have been asleep in the while loop
        //above when a few more items went into the queue,
        //for instance. Flush the queue if needed to get all
        //the data.
        while (queue.try_pop(item)) {
            recovered_data.emplace_back(item);
        }
    });

    //Randomly create some test data
    std::vector<size_t> test_data;
    test_data.reserve(data_num_elements);
    for (size_t idx = 0; idx < data_num_elements; ++idx) {
        test_data.emplace_back(rand());
    }

    //Start putting those random data values into the queue
    std::thread producer([&queue, &test_data](){
        for (size_t idx = 0; idx < data_num_elements; ++idx) {
            queue.push(test_data[idx]);
        }
    });

    //Go until the source values are all spent, and sent
    //into the queue
    producer.join();

    //Flip the switch that tells the consumer it can break
    //out of its infinite loop. It will greedily get any
    //leftover data out of the queue if there is any.
    keep_consuming = false;

    //Wait until the consumer thread is done, and then
    //check all the recovered data values against the
    //source values that were originally sent into the
    //queue.
    consumer.join();

    EXPECT_EQUAL(test_data, recovered_data);
    EXPECT_EQUAL(queue.size(), 0);

    //Make sure the emplace() method is doing the right thing
    typedef std::tuple<std::string, std::string, size_t> CustomerInfo;
    ConcurrentQueue<CustomerInfo> customers;
    customers.emplace("Bob", "Thompson", 41);
    customers.emplace("Alice", "Smith", 29);

    CustomerInfo customer1, customer2;
    EXPECT_TRUE(customers.try_pop(customer1));
    EXPECT_TRUE(customers.try_pop(customer2));

    EXPECT_EQUAL(std::get<0>(customer1), std::string("Bob"));
    EXPECT_EQUAL(std::get<1>(customer1), std::string("Thompson"));
    EXPECT_EQUAL(std::get<2>(customer1), 41);

    EXPECT_EQUAL(std::get<0>(customer2), std::string("Alice"));
    EXPECT_EQUAL(std::get<1>(customer2), std::string("Smith"));
    EXPECT_EQUAL(std::get<2>(customer2), 29);
}

int main()
{
    srand(time(0));

    testTimerThreadBasic();
    testConcurrentQueue();

    REPORT_ERROR;
    return ERROR_CODE;
}
