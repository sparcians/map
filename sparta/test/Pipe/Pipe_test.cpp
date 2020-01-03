
#include <iostream>
#include <cstring>

#include "sparta/simulation/Clock.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/resources/Pipe.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/sparta.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/PayloadEvent.hpp"

TEST_INIT;
#define PIPEOUT_GEN

/*
 * This test creates a producer and a consumer for two staged pipes.
 * The purpose of the test is to make sure that the data written to
 * the pipe from the producer is made available to consumer in the
 * same amount of time as indicated by the pipe stages.  The producer
 * will send the time appended as the data through the pipe.
 */

#define TEST_MANUAL_UPDATE

class DummyObj
{
public:
    DummyObj(sparta::Pipe<uint32_t> & p2) : p2_(p2) {}

    void writeToPipe()
    {
        p2_.writePS(0, 20);
    }

private:

    sparta::Pipe<uint32_t> & p2_;
};

int main ()
{
    sparta::Scheduler sched;
    sparta::RootTreeNode rtn;
    sparta::ClockManager cm(&sched);
    sparta::Clock::Handle root_clk;
    root_clk = cm.makeRoot(&rtn, "root_clk");
    cm.normalize();
    rtn.setClock(root_clk.get());

    sparta::Pipe<uint32_t> pipe1("PipeUno", 10, root_clk.get());
    sparta::Pipe<uint32_t> pipe2("PipeDos", 5, root_clk.get());

    DummyObj dum_obj(pipe2);
    sparta::EventSet es(&rtn);
    sparta::Event<> ev_dummy(&es, "ev_dummy",
                           CREATE_SPARTA_HANDLER_WITH_OBJ(DummyObj, &dum_obj, writeToPipe));

    EXPECT_EQUAL(pipe2.capacity(), 5);
    pipe2.resize(10);
    EXPECT_EQUAL(pipe2.capacity(), 10);

#ifndef TEST_MANUAL_UPDATE
    pipe1.performOwnUpdates();
#endif

     // sparta::log::Tap scheduler_debug(sparta::TreeNode::getVirtualGlobalNode(),
     //                                sparta::log::categories::DEBUG, std::cout);

#ifdef PIPEOUT_GEN
    pipe1.enableCollection(&rtn);
    pipe2.enableCollection<sparta::SchedulingPhase::PostTick>(&rtn);
#endif
    sparta::PayloadEvent<uint32_t>
        ev(&es, "dummy_ev",
           CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(sparta::Pipe<uint32_t>, &pipe1, push_front, uint32_t));

    rtn.enterConfiguring();
    rtn.enterFinalized();

#ifdef PIPEOUT_GEN
    sparta::collection::PipelineCollector pc("testPipe", 1000000,
                                           root_clk.get(), &rtn);
#endif
    sched.finalize();

#ifdef PIPEOUT_GEN
    EXPECT_THROW(pipe2.resize(5));
    EXPECT_EQUAL(pipe2.capacity(), 10); // Make sure it really didn't get resized
    pc.startCollection(&rtn);
#endif

    // Check initials
    EXPECT_EQUAL(pipe1.capacity(), 10);
    EXPECT_EQUAL(pipe1.size(), 0);

    // Do an push_front
    //pipe1.push_front(1);
    ev.preparePayload(1)->schedule(sparta::Clock::Cycle(0));

    sched.run(1, true);

    EXPECT_EQUAL(pipe1.size(), 0);
    sched.run(1, true);
#ifdef TEST_MANUAL_UPDATE
    pipe1.update();
#endif
    EXPECT_EQUAL(pipe1.size(), 1);
    EXPECT_TRUE(pipe1.isAnyValid());
    EXPECT_TRUE(pipe1.isValid(0));
    for(uint32_t i = 1; i < pipe1.capacity(); ++i) {
        EXPECT_FALSE(pipe1.isValid(i));
    }
    EXPECT_FALSE(pipe1.isLastValid());
    EXPECT_EQUAL(pipe1.read(0), 1);

    // Advance the pipe
    for(uint32_t i = 1; i < pipe1.capacity(); ++i) {
        sched.run(1, true);
#ifdef TEST_MANUAL_UPDATE
        pipe1.update();
#endif

    }
    EXPECT_TRUE(pipe1.isLastValid());
    EXPECT_EQUAL(pipe1.readLast(), 1);

    // Test some bad things
    EXPECT_THROW(pipe1.read(1));    // Should throw -- bad read
    EXPECT_THROW(pipe1.read(2));    // Should throw -- bad read
    EXPECT_THROW(pipe1.read(1024));    // Should throw -- bad read
    EXPECT_NOTHROW(pipe1.push_front(4));
    EXPECT_THROW(pipe1.push_front(5));  // A double push_front
    EXPECT_THROW(pipe1.invalidatePS(6));

    sched.run(1, true);
#ifdef TEST_MANUAL_UPDATE
    pipe1.update();
#endif

    std::cout << "Pipe num entries: " << pipe1.numValid() << std::endl;

    sparta::Pipe<uint32_t>::iterator it = pipe1.begin();
    sparta::Pipe<uint32_t>::iterator eit = pipe1.end();
    uint32_t i = 0;
    while(it != eit) {
        if(it.isValid()) {
            std::cout << "Pipe contents@" << i << ": " << *it << std::endl;
        }
        else {
            std::cout << "Nothing      @" << i << std::endl;
        }
        ++i;
        ++it;
    }

    EXPECT_FALSE(pipe1.isLastValid());
    EXPECT_THROW(pipe1.readLast());    // Should throw -- bad read

    // A '4' was written above and is not in stage 0.
    EXPECT_EQUAL(pipe1.read(0), 4);
    // Invalidate it this cycle
    EXPECT_NOTHROW(pipe1.invalidatePS(0));
    EXPECT_FALSE(pipe1.isValid(0));

    // Write some data in a random place
    pipe1.writePS(4, 23);
    EXPECT_EQUAL(pipe1.read(4), 23);
    EXPECT_EQUAL(pipe1.size(), 1);

    pipe1.writePS(5,77);
    EXPECT_EQUAL(pipe1.read(5), 77);
    EXPECT_EQUAL(pipe1.size(), 2);

    // Define flush criteria
    uint32_t flush_criteria = 77;
    pipe1.flushIf(flush_criteria);
    EXPECT_EQUAL(pipe1.size(), 1);
    EXPECT_EQUAL(pipe1.read(4), 23);
    EXPECT_FALSE(pipe1.isValid(5));

    pipe1.writePS(5, 12);
    EXPECT_EQUAL(pipe1.read(5), 12);
    EXPECT_EQUAL(pipe1.size(), 2);
    pipe1.writePS(3, 12);
    EXPECT_EQUAL(pipe1.read(3), 12);
    EXPECT_EQUAL(pipe1.size(), 3);

    // Changing flush criteria and flushing
    flush_criteria = 12;
    pipe1.flushIf(flush_criteria);
    EXPECT_EQUAL(pipe1.size(), 1);
    EXPECT_EQUAL(pipe1.read(4), 23);
    EXPECT_FALSE(pipe1.isValid(5));
    EXPECT_FALSE(pipe1.isValid(3));

    pipe1.writePS(6, 19);
    EXPECT_EQUAL(pipe1.read(6), 19);
    EXPECT_EQUAL(pipe1.size(), 2);
    pipe1.writePS(9, 19);
    EXPECT_EQUAL(pipe1.read(9), 19);
    EXPECT_EQUAL(pipe1.size(), 3);

    // Change flush criteria and write your own comparator for flushing
    flush_criteria = 19;
    pipe1.flushIf([flush_criteria](const uint32_t val) -> bool
                              {
                                  return flush_criteria == val;
                              });
    EXPECT_EQUAL(pipe1.size(), 1);
    EXPECT_EQUAL(pipe1.read(4), 23);
    EXPECT_FALSE(pipe1.isValid(6));
    EXPECT_FALSE(pipe1.isValid(9));

    pipe1.writePS(3, 10);
    pipe1.writePS(5, 9);
    pipe1.writePS(6, 12);
    pipe1.writePS(8, 15);

    // A different predicate function which checks if item is in a given range to flush
    uint32_t lower_bound = 5;
    uint32_t upper_bound = 20;
    pipe1.flushIf([lower_bound, upper_bound](const uint32_t val) -> bool
                              {
                                  return (val >= lower_bound) && (val <= upper_bound);
                              });
    EXPECT_EQUAL(pipe1.size(), 1);
    EXPECT_EQUAL(pipe1.read(4), 23);
    EXPECT_FALSE(pipe1.isValid(3));
    EXPECT_FALSE(pipe1.isValid(5));
    EXPECT_FALSE(pipe1.isValid(6));
    EXPECT_FALSE(pipe1.isValid(8));

    // Flush the 23 from index 4
    pipe1.flushPS(4);
    EXPECT_EQUAL(pipe1.size(), 0);
    EXPECT_FALSE(pipe1.isValid(4));

    pipe1.append(23);
    EXPECT_THROW(pipe1.flushPS(-1U));
    pipe1.flushAppend();
    EXPECT_EQUAL(pipe1.numValid(), 0);

    // Flush everything
    pipe1.flushAll();
    EXPECT_THROW(pipe1.read(4));
    EXPECT_EQUAL(pipe1.size(), 0);

    EXPECT_FALSE(pipe1.isAnyValid());
    pipe1.push_front(42);
    EXPECT_TRUE(pipe1.isAnyValid());
    sched.run(1, true);
#ifdef TEST_MANUAL_UPDATE
    pipe1.update();
#endif

    EXPECT_TRUE(pipe1.isAnyValid());
    pipe1.flushAll();
    EXPECT_FALSE(pipe1.isAnyValid());

    for(uint32_t i = 0; i < pipe1.capacity(); ++i) {
        EXPECT_FALSE(pipe1.isValid(i));
        EXPECT_THROW(pipe1.read(i));
    }
    EXPECT_THROW(pipe1.readLast());    // Should throw -- bad read

    // This shouldn't change
    EXPECT_EQUAL(pipe1.capacity(), 10);

    pipe1.push_front(2);
#ifdef TEST_MANUAL_UPDATE
    pipe1.update();
#else
    sched.run(1, true);
#endif

    pipe1.push_front(3);
    for(uint32_t i = 0; i < pipe1.capacity() + 1; ++i) {
#ifdef TEST_MANUAL_UPDATE
        pipe1.update();
#endif
        sched.run(1, true);
    }

    // Used to test pipeout size
    pipe2.push_front(10);
    pipe2.performOwnUpdates();

    sched.run(pipe2.capacity() + 1, true);

    pipe2.writePS(0, 20);
    pipe2.performOwnUpdates();

    sched.run(pipe2.capacity() + 1, true);

    // Test clearing
    EXPECT_EQUAL(pipe2.size(), 0);
    pipe2.push_front(10);
    EXPECT_EQUAL(pipe2.size(), 0);
    sched.run(2, true);
    EXPECT_EQUAL(pipe2.size(), 1);
    pipe2.clear();
    EXPECT_EQUAL(pipe2.size(), 0);
    sched.run(1, true);
    EXPECT_EQUAL(pipe2.size(), 0);

    pipe2.push_front(10);
    pipe2.clear();
    sched.run(1, true);
    EXPECT_EQUAL(pipe2.size(), 0);

    pipe2.writePS(0, 20);
    EXPECT_EQUAL(pipe2.size(), 1);
    pipe2.clear();
    EXPECT_EQUAL(pipe2.size(), 0);
    sched.run(1, true);
    EXPECT_EQUAL(pipe2.size(), 0);

    rtn.enterTeardown();
#ifdef PIPEOUT_GEN
    pc.destroy();
#endif

    // Returns error if one
    REPORT_ERROR;
    return ERROR_CODE;
}
