
#include <inttypes.h>
#include <iostream>

#include "sparta/sparta.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/utils/SpartaTester.hpp"

TEST_INIT;

using sparta::Clock;
using sparta::Scheduler;
using namespace std;

static void test_ratioed_clocks()
{
    sparta::Scheduler sched;
    sparta::ClockManager  m(&sched);

    Clock::Handle c_root  = m.makeRoot();
    Clock::Handle c_12    = m.makeClock("C12", c_root, 1, 2);
    Clock::Handle c_23    = m.makeClock("C23", c_root, 2, 3);
    Clock::Handle c_23_12 = m.makeClock("C23_12", c_23, 1, 2);
    Clock::Handle c_23_23 = m.makeClock("C23_23", c_23, 2, 3);

    cout << string(*c_root) << endl;
    cout << string(*c_12) << endl;
    cout << string(*c_23) << endl;
    cout << string(*c_23_12) << endl;
    cout << string(*c_23_23) << endl;

    uint32_t norm = m.normalize();
    EXPECT_TRUE(norm == 4);

    cout << "Norm(Global LCM): " << norm << endl;
    cout << endl;

    EXPECT_TRUE(c_root->getPeriod() == 4);
    EXPECT_TRUE(c_12->getPeriod() == 8);
    EXPECT_TRUE(c_23->getPeriod() == 6);
    EXPECT_TRUE(c_23_12->getPeriod() == 12);
    EXPECT_TRUE(c_23_23->getPeriod() == 9);

    cout << m << endl;

    for (Scheduler::Tick i = 0; i < 50; ++i) {
        cout << "TICK: " << i << endl;
        cout << "\tc_root: " << c_root->getCycle(i) << endl;
        cout << "\tc_12: " << c_12->getCycle(i) << endl;
        cout << "\tc_23: " << c_23->getCycle(i) << endl;
        cout << "\tc_23_12: " << c_23_12->getCycle(i) << endl;
        cout << "\tc_23_23: " << c_23_23->getCycle(i) << endl;
        cout << endl;
    }

    for (Clock::Cycle c = 0; c < 20; ++c) {
        Scheduler::Tick t = c_root->getTick(c);
        cout << "ROOT Cycle: " << c << "(tick: " << t << ")" << endl;
        cout << "\tc_root: " << c_root->getCycle(t) << endl;
        cout << "\tc_12: " << c_12->getCycle(t) << endl;
        cout << "\tc_23: " << c_23->getCycle(t) << endl;
        cout << "\tc_23_12: " << c_23_12->getCycle(t) << endl;
        cout << "\tc_23_23: " << c_23_23->getCycle(t) << endl;
        cout << endl;
    }

}

static uint64_t convert_mhz_to_ps(double frequency_mhz)
{
    uint64_t period_ps = (1.0 / frequency_mhz) * 1000.0 * 1000.0;
    return period_ps;
}

static void test_frequency_clocks()
{
    sparta::Scheduler sched;
    sparta::ClockManager  m(&sched);

    Clock::Handle c_root  = m.makeRoot();
    Clock::Handle c_333   = m.makeClock("C12", c_root, 333.3333);
    Clock::Handle c_400   = m.makeClock("C23", c_root, 400.0000);
    Clock::Handle c_666   = m.makeClock("C23_12", c_400,  666.666);

    m.normalize();

    cout << string(*c_root) << endl;
    cout << string(*c_333) << endl;
    cout << string(*c_400) << endl;
    cout << string(*c_666) << endl;

    EXPECT_TRUE(c_root->getPeriod() == 1);
    EXPECT_TRUE(c_root->getFrequencyMhz() == 0.0);

    EXPECT_TRUE(c_333->getFrequencyMhz() == 333.3333);
    EXPECT_TRUE(c_333->getPeriod() == 3000);

    EXPECT_TRUE(c_400->getFrequencyMhz() == 400.0);
    EXPECT_TRUE(c_400->getPeriod() == 2500);

    EXPECT_TRUE(c_666->getFrequencyMhz() == 666.666);
    EXPECT_TRUE(c_666->getPeriod() == 1500);

}

int main()
{
    test_ratioed_clocks();

    test_frequency_clocks();

    REPORT_ERROR;

    return ERROR_CODE;
}
