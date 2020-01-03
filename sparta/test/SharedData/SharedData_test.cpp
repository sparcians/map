

#include <iostream>
#include <cstring>

#include "sparta/resources/SharedData.hpp"
#include "sparta/simulation/RootTreeNode.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/kernel/Scheduler.hpp"

#include "sparta/utils/SpartaTester.hpp"


TEST_INIT;

struct auto_update
{
    auto_update(sparta::Scheduler * _sched) :
        sched(_sched)
    {}

    template<class T>
    void operator()(T &) const {
        sched->run();
    }
    sparta::Scheduler * sched;
};

struct manual_update
{
    template<class T>
    void operator()(T &sd) const {
        sd.update();
    }
};


template<class T>
void test_sddata(T & sdata, std::function<void(T &)> & f_update)
{
    EXPECT_TRUE(sdata.isValid());
    EXPECT_FALSE(sdata.isValidNS());

    EXPECT_NOTHROW(sdata.access());
    EXPECT_NOTHROW(sdata.read());
    EXPECT_THROW(sdata.accessNS());
    EXPECT_THROW(sdata.readNS());

    EXPECT_NOTHROW(sdata.clear());
    EXPECT_NOTHROW(sdata.clearNS());
    EXPECT_NOTHROW(sdata.clearPS());

    sdata.write(11); // it goes to 11, so it's better

    EXPECT_FALSE(sdata.isValid());
    EXPECT_TRUE (sdata.isValidNS());

    EXPECT_THROW(sdata.access());
    EXPECT_THROW(sdata.read());
    EXPECT_NOTHROW(sdata.accessNS());
    EXPECT_NOTHROW(sdata.readNS());
    EXPECT_EQUAL(sdata.readNS(), 11);

    f_update(sdata);

    EXPECT_TRUE(sdata.isValid());
    EXPECT_FALSE(sdata.isValidNS());

    EXPECT_NOTHROW(sdata.access());
    EXPECT_NOTHROW(sdata.read());
    EXPECT_THROW(sdata.accessNS());
    EXPECT_THROW(sdata.readNS());

    sdata.clearPS();

    EXPECT_FALSE(sdata.isValid());
    EXPECT_FALSE(sdata.isValidNS());

    EXPECT_THROW(sdata.access());
    EXPECT_THROW(sdata.read());
    EXPECT_THROW(sdata.accessNS());
    EXPECT_THROW(sdata.readNS());

    sdata.write(14);

    EXPECT_FALSE(sdata.isValid());
    EXPECT_TRUE(sdata.isValidNS());

    EXPECT_THROW(sdata.access());
    EXPECT_THROW(sdata.read());
    EXPECT_NOTHROW(sdata.accessNS());
    EXPECT_NOTHROW(sdata.readNS());

    f_update(sdata);

    EXPECT_TRUE(sdata.isValid());
    EXPECT_FALSE(sdata.isValidNS());

    EXPECT_NOTHROW(sdata.access());
    EXPECT_NOTHROW(sdata.read());
    EXPECT_THROW(sdata.accessNS());
    EXPECT_THROW(sdata.readNS());

    sdata.write(15);

    EXPECT_TRUE(sdata.isValid());
    EXPECT_TRUE(sdata.isValidNS());

    EXPECT_NOTHROW(sdata.access());
    EXPECT_NOTHROW(sdata.read());
    EXPECT_NOTHROW(sdata.accessNS());
    EXPECT_NOTHROW(sdata.readNS());

    EXPECT_EQUAL(sdata.read(),   14);
    EXPECT_EQUAL(sdata.readNS(), 15);

    f_update(sdata);

    EXPECT_EQUAL(sdata.read(),   15);
    EXPECT_THROW(sdata.readNS());

    f_update(sdata);
}

int main ()
{
    sparta::RootTreeNode rtn;
    sparta::Scheduler sched;
    sparta::ClockManager cm(&sched);
    sparta::Clock::Handle root_clk;
    root_clk = cm.makeRoot(&rtn, "root_clk");
    cm.normalize();
    rtn.setClock(root_clk.get());

    ////////////////////////////////////////////////////////////////////////////////
    // Create testing objects and finalize configuration
    sparta::SharedData<uint32_t>          sdata1("sdata_auto", root_clk.get());
    sparta::SharedData<uint32_t, true>    sdata2("sdata_man", root_clk.get());

    rtn.enterConfiguring();
    rtn.enterFinalized();

    sched.finalize();
    sched.run(1, true, false);

    ////////////////////////////////////////////////////////////////////////////////
    // Enter testing

    // This will not compile since manual_update == false
    // sdata1.update();
    std::function<void(sparta::SharedData<uint32_t> &)> a_update = auto_update(&sched);
    test_sddata(sdata1, a_update);

    std::function<void(sparta::SharedData<uint32_t, true> &)> m_update = manual_update();
    test_sddata(sdata2, m_update);

    ////////////////////////////////////////////////////////////////////////////////
    // Teardown
    rtn.enterTeardown();

    // Returns error if one
    REPORT_ERROR;
    return ERROR_CODE;
}
