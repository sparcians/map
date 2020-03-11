

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

struct dummy_struct
{
    uint16_t int16_field;
    uint32_t int32_field;
    std::string s_field;

    dummy_struct() = default;
    dummy_struct(const uint16_t int16_field, const uint32_t int32_field, const std::string &s_field) : 
        int16_field{int16_field},
        int32_field{int32_field},
        s_field{s_field} {}
};
std::ostream &operator<<(std::ostream &os, const dummy_struct &obj)
{
    os << obj.int16_field << " " << obj.int32_field << obj.s_field << "\n";
    return os;
}

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

template<>
void test_sddata(sparta::SharedData<dummy_struct, true>& sdata,
    std::function<void(sparta::SharedData<dummy_struct, true>&)> & f_update)
{
    auto dummy_1 = dummy_struct(1, 2, "ABC");
    auto dummy_2 = dummy_struct(3, 4, "DEF");
    auto dummy_3 = dummy_struct(5, 6, "GHI");

    EXPECT_TRUE(sdata.isValid());
    EXPECT_FALSE(sdata.isValidNS());

    EXPECT_NOTHROW(sdata.access());
    EXPECT_NOTHROW(sdata.read());
    EXPECT_THROW(sdata.accessNS());
    EXPECT_THROW(sdata.readNS());

    EXPECT_NOTHROW(sdata.clear());
    EXPECT_NOTHROW(sdata.clearNS());
    EXPECT_NOTHROW(sdata.clearPS());

    sdata.write(std::move(dummy_1));
    EXPECT_TRUE(dummy_1.s_field.size() == 0);

    EXPECT_FALSE(sdata.isValid());
    EXPECT_TRUE (sdata.isValidNS());

    EXPECT_THROW(sdata.access());
    EXPECT_THROW(sdata.read());
    EXPECT_NOTHROW(sdata.accessNS());
    EXPECT_NOTHROW(sdata.readNS());
    EXPECT_EQUAL(sdata.readNS().s_field, "ABC");

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

    // Test copy
    sdata.write(dummy_2);
    EXPECT_TRUE(dummy_2.s_field == "DEF");

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

    sdata.write(std::move(dummy_3));
    EXPECT_TRUE(dummy_3.s_field.size() == 0);

    EXPECT_TRUE(sdata.isValid());
    EXPECT_TRUE(sdata.isValidNS());

    EXPECT_NOTHROW(sdata.access());
    EXPECT_NOTHROW(sdata.read());
    EXPECT_NOTHROW(sdata.accessNS());
    EXPECT_NOTHROW(sdata.readNS());

    EXPECT_EQUAL(sdata.read().s_field, "DEF");
    EXPECT_EQUAL(sdata.readNS().s_field, "GHI");

    f_update(sdata);

    EXPECT_EQUAL(sdata.read().s_field, "GHI");
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
    sparta::SharedData<dummy_struct, true>    sdata3("sdata_pf", root_clk.get());

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

    std::function<void(sparta::SharedData<dummy_struct, true> &)> pf_update = manual_update();
    test_sddata(sdata3, pf_update);

    ////////////////////////////////////////////////////////////////////////////////
    // Teardown
    rtn.enterTeardown();

    // Returns error if one
    REPORT_ERROR;
    return ERROR_CODE;
}
