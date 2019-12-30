// <Ports.cpp> -*- C++ -*-


#include "Dummy_device.hpp"
#include <iostream>
#include <cstring>
#include "sparta/simulation/Clock.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/utils/SpartaTester.hpp"

template<class EnumClassT>
std::string generateHistogramString(EnumClassT state_enum, std::vector<uint32_t> & values)
{
    std::stringstream histo_string;
    std::string name = std::string("state_timer_unit_1_histogram_set_") + typeid(state_enum).name() + "_state_" + std::to_string(static_cast<uint32_t>(state_enum));
    histo_string << "\t" <<  name << "[ UF ] = " << std::to_string(values[0]) << std::endl;
    for (uint32_t i=0; i<=5; ++i) {
        histo_string << "\t" << name
            << "[ " << 0 << "-" << i << " ] = "
            << std::to_string(values[i+1]) << std::endl;
    }
    histo_string << "\t" << name << "[ OF ] = " << std::to_string(values[7]) << std::endl;
    return histo_string.str();
}

int main()
{
    // Setup the DummyDevices
    sparta::RootTreeNode rtn;
    sparta::TreeNode     device_tn(&rtn, "dummy_device1", "Dummy Device TreeNode");
    sparta::TreeNode     device_tn1(&rtn, "dummy_device2", "Dummy Device TreeNode");
    sparta::Clock        clk("clock");
    rtn.setClock(&clk);
    sparta::PortSet ps(&rtn, "out_ports");
    std::unique_ptr<DummyDeviceParams> dummy_dev_params_(new DummyDeviceParams(&device_tn));
    std::unique_ptr<DummyDevice>       dummy_device1(new DummyDevice(&device_tn, dummy_dev_params_.get(), 1, &clk));
    std::unique_ptr<DummyDevice>       dummy_device2(new DummyDevice(&device_tn1, dummy_dev_params_.get(), 2, &clk));
    sparta::DataOutPort<DummyOpPtr> a_delay_out (&ps, "a_delay_out");
    sparta::bind(rtn.getChildAs<sparta::Port>("dummy_device2.ports.in_port"),
               rtn.getChildAs<sparta::Port>("dummy_device1.ports.out_port"));
    sparta::bind(rtn.getChildAs<sparta::Port>("dummy_device1.ports.in_port"), a_delay_out);

    std::vector<uint32_t> histo_val;

    ////////////////////////////////////
    // 1.Test StateTimerUnit constructor
    ////////////////////////////////////

    sparta::StateTimerUnit state_timer_unit_1(&rtn, "state_timer_unit_1", "state_timer_unit_1", 2, 0, 5, 1, DummyState1::__LAST, DummyState2::__LAST);
    std::cout << "state_timer_unit_1 created with state set: DummyState1, DummyState2" << std::endl;

    EXPECT_THROW(sparta::StateTimerUnit state_timer_unit_1(&rtn, "state_timer_unit_1", "state_timer_unit_1",
                2, 0, 5, 1, DummyState1::__LAST,DummyState1::__LAST));  // Should not add same state set more than once

    rtn.enterConfiguring();
    rtn.enterFinalized();
    clk.getScheduler()->finalize();


    ///////////////////////////
    // 2.Test timer stand along
    ///////////////////////////
    {
        std::shared_ptr<sparta::StateTimerUnit::StateTimer> state_timer_1;
        std::shared_ptr<sparta::StateTimerUnit::StateTimer> state_timer_2;
        // cycle 1
        clk.getScheduler()->run(1);
        std::cout << std::endl << "Cycle: " << clk.currentCycle()<< "--------------------------------------------------" << std::endl;

        // allocate timers
        state_timer_1 = state_timer_unit_1.allocateStateTimer();
        std::cout << "state_timer_1 allocate" << std::endl;
        state_timer_2 = state_timer_unit_1.allocateStateTimer();
        std::cout << "state_timer_2 allocate" << std::endl;

        std::shared_ptr<sparta::StateTimerUnit::StateTimer> state_timer_3;
        EXPECT_NOTHROW(state_timer_3 = state_timer_unit_1.allocateStateTimer());  // Warning for allocating more than the initial number of timers: 2.

        // start some states
        *state_timer_1 = DummyState1::DS1_1;
        std::cout << "state_timer_1 State: DummyState1::DS1_1 ("<< static_cast<uint32_t>(DummyState1::DS1_1) <<"), start" << std::endl;
        *state_timer_1 = DummyState2::DS2_1;
        std::cout << "state_timer_1 State: DummyState2::DS2_1 ("<< static_cast<uint32_t>(DummyState2::DS2_1) <<") , start" << std::endl;
        *state_timer_2 = DummyState1::DS1_1;
        std::cout << "state_timer_2 State: DummyState1::DS1_1 ("<< static_cast<uint32_t>(DummyState1::DS1_1) <<"), start" << std::endl;

        EXPECT_THROW(*state_timer_1 = DummyState3::DS3_1);   // Can not start state set that is not added

        // dynamic query
        std::cout << "dynamicQuery() count [0-0] and above incresase by 3 since 3 timers in the pool all have 0 for all states" << std::endl;

        std::cout << "DummyState1::DS1_1 values:" << std::endl;
        std::cout << state_timer_unit_1.dynamicQuery(DummyState1::DS1_1) << std::endl;
        std::cout << "DummyState1::DS1_2 values:" << std::endl;
        std::cout << state_timer_unit_1.dynamicQuery(DummyState1::DS1_2) << std::endl;
        std::cout << "DummyState1::DS2_1 values:" << std::endl;
        std::cout << state_timer_unit_1.dynamicQuery(DummyState2::DS2_1) << std::endl;

        histo_val = {0,3,3,3,3,3,3,3};
        EXPECT_EQUAL(state_timer_unit_1.dynamicQuery(DummyState1::DS1_1),generateHistogramString(DummyState1::DS1_1, histo_val));
        EXPECT_EQUAL(state_timer_unit_1.dynamicQuery(DummyState1::DS1_2),generateHistogramString(DummyState1::DS1_2, histo_val));
        EXPECT_EQUAL(state_timer_unit_1.dynamicQuery(DummyState2::DS2_1),generateHistogramString(DummyState2::DS2_1, histo_val));
        EXPECT_THROW(*state_timer_1 = DummyState1::DS1_1);   // State already started.

        // cycle 2
        clk.getScheduler()->run(1);
        std::cout << std::endl << "Cycle: " << clk.currentCycle()<< "--------------------------------------------------" << std::endl;

        // cycle 3
        clk.getScheduler()->run(1);
        std::cout << std::endl << "Cycle: " << clk.currentCycle()<< "--------------------------------------------------" << std::endl;

        *state_timer_1 = DummyState1::DS1_2;
        std::cout << "state_timer_1 State: DummyState1::DS1_2 ("<< static_cast<uint32_t>(DummyState1::DS1_2) <<"), start" << std::endl;

        // cycle 4
        clk.getScheduler()->run(1);
        std::cout << std::endl << "Cycle: " << clk.currentCycle()<< "--------------------------------------------------" << std::endl;

        state_timer_2->endState(DummyState1::DS1_1);
        std::cout << "state_timer_2 State: DummyState1::DS1_1 ("<< static_cast<uint32_t>(DummyState1::DS1_1) <<"), end" << std::endl;

        // cycle 5
        clk.getScheduler()->run(1);
        std::cout << std::endl << "Cycle: " << clk.currentCycle()<< "--------------------------------------------------" << std::endl;

        // cycle 6
        clk.getScheduler()->run(1);
        std::cout << std::endl << "Cycle: " << clk.currentCycle()<< "--------------------------------------------------" << std::endl;

        std::cout << "dynamicQuery()" << std::endl;

        std::cout << "DummyState1::DS1_1 values:" << std::endl;
        std::cout << state_timer_unit_1.dynamicQuery(DummyState1::DS1_1) << std::endl;
        std::cout << "DummyState1::DS1_2 values:" << std::endl;
        std::cout << state_timer_unit_1.dynamicQuery(DummyState1::DS1_2) << std::endl;
        std::cout << "DummyState1::DS2_1 values:" << std::endl;
        std::cout << state_timer_unit_1.dynamicQuery(DummyState2::DS2_1) << std::endl;

        histo_val = {0,4,4,5,6,6,6,6};
        EXPECT_EQUAL(state_timer_unit_1.dynamicQuery(DummyState1::DS1_1),generateHistogramString(DummyState1::DS1_1, histo_val));
        histo_val = {0,5,5,5,6,6,6,6};
        EXPECT_EQUAL(state_timer_unit_1.dynamicQuery(DummyState1::DS1_2),generateHistogramString(DummyState1::DS1_2, histo_val));
        histo_val = {0,5,5,5,5,5,6,6};
        EXPECT_EQUAL(state_timer_unit_1.dynamicQuery(DummyState2::DS2_1),generateHistogramString(DummyState2::DS2_1, histo_val));

    }
    std::cout << "Release timers by out of scope. Histograms updated [0-0] and above incresase by 3 for all states, due to 3 timer released." << std::endl;

    //////////////////////////
    // 3.Test timer in DummyOp
    //////////////////////////

    std::cout << std::endl << "Start Timer Test in DummyOp-------------------------------------------------" << std::endl;
    {
        std::shared_ptr<sparta::StateTimerUnit::StateTimer> state_timer_1;
        std::shared_ptr<sparta::StateTimerUnit::StateTimer> state_timer_2;
        // cycle 7
        clk.getScheduler()->run(1);

        // allocate timer for dummy_op_1 at cycle 7, and send it to dummy_device1
        DummyOpPtr dummy_op_1 = DummyOpPtr(new DummyOp(1));
        state_timer_1 = state_timer_unit_1.allocateStateTimer();
        dummy_op_1->setTimer(state_timer_1);
        a_delay_out.send(dummy_op_1);
        std::cout << "state_timer_1 allocate for dummy_op_1" << " at Cycle: "<< clk.currentCycle() << std::endl;
        std::cout << "dummy_op_1 sent to dummy_device1" << " at Cycle: "<< clk.currentCycle() << std::endl;

        // dynamic query
        std::cout << "dynamicQuery()" << " at Cycle: "<< clk.currentCycle() << "[0-0] and above incresase by 1 due to 1 timer allocated" << std::endl;

        std::cout << "DummyState1::DS1_1 values:" << std::endl;
        std::cout << state_timer_unit_1.dynamicQuery(DummyState1::DS1_1) << std::endl;
        std::cout << "DummyState1::DS1_2 values:" << std::endl;
        std::cout << state_timer_unit_1.dynamicQuery(DummyState1::DS1_2) << std::endl;
        std::cout << "DummyState1::DS2_1 values:" << std::endl;
        std::cout << state_timer_unit_1.dynamicQuery(DummyState2::DS2_1) << std::endl;

        histo_val = {0,8,8,9,10,10,10,10};
        EXPECT_EQUAL(state_timer_unit_1.dynamicQuery(DummyState1::DS1_1),generateHistogramString(DummyState1::DS1_1, histo_val));
        histo_val = {0,9,9,9,10,10,10,10};
        EXPECT_EQUAL(state_timer_unit_1.dynamicQuery(DummyState1::DS1_2),generateHistogramString(DummyState1::DS1_2, histo_val));
        histo_val = {0,9,9,9,9,9,10,10};
        EXPECT_EQUAL(state_timer_unit_1.dynamicQuery(DummyState2::DS2_1),generateHistogramString(DummyState2::DS2_1, histo_val));


        // cycle 8
        clk.getScheduler()->run(1);

        // allocate timer for dummy_op_2 at cycle 8, and send it to dummy_device1
        DummyOpPtr dummy_op_2 = DummyOpPtr(new DummyOp(2));
        state_timer_2 = state_timer_unit_1.allocateStateTimer();
        dummy_op_2->setTimer(state_timer_2);
        a_delay_out.send(dummy_op_2);
        std::cout << "state_timer_2 allocate for dummy_op_2" << " at Cycle: "<< clk.currentCycle() << std::endl;
        std::cout << "dummy_op_2 sent to dummy_device1" << " at Cycle: "<< clk.currentCycle() << std::endl;

        //cycle 9
        clk.getScheduler()->run(1);

        //cycle 10
        clk.getScheduler()->run(1);

        //cycle 11
        clk.getScheduler()->run(1);

        // cycle 12
        clk.getScheduler()->run(1);

        // cycle 13
        clk.getScheduler()->run(1);

        // cycle 14
        clk.getScheduler()->run(1);

        // dynamic query
        std::cout << "dynamicQuery()" << " at Cycle: "<< clk.currentCycle() << std::endl;

        std::cout << "DummyState1::DS1_1 values:" << std::endl;
        std::cout << state_timer_unit_1.dynamicQuery(DummyState1::DS1_1) << std::endl;
        std::cout << "DummyState1::DS1_2 values:" << std::endl;
        std::cout << state_timer_unit_1.dynamicQuery(DummyState1::DS1_2) << std::endl;
        std::cout << "DummyState1::DS2_1 values:" << std::endl;
        std::cout << state_timer_unit_1.dynamicQuery(DummyState2::DS2_1) << std::endl;

        histo_val = {0,10,10,11,12,12,12,12};
        EXPECT_EQUAL(state_timer_unit_1.dynamicQuery(DummyState1::DS1_1),generateHistogramString(DummyState1::DS1_1, histo_val));
        histo_val = {0,11,11,11,12,12,12,12};
        EXPECT_EQUAL(state_timer_unit_1.dynamicQuery(DummyState1::DS1_2),generateHistogramString(DummyState1::DS1_2, histo_val));
        histo_val = {0,9,9,11,11,11,12,12};
        EXPECT_EQUAL(state_timer_unit_1.dynamicQuery(DummyState2::DS2_1),generateHistogramString(DummyState2::DS2_1, histo_val));


        // teardown will automatically release inflight StateTimers
        rtn.enterTeardown();
    }
    return 0;
}
