
#include <inttypes.h>
#include <iostream>

#include "sparta/sparta.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/RootTreeNode.hpp"
#include "sparta/simulation/Resource.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/kernel/Scheduler.hpp"
/*!
 * \file main.cpp
 * \brief Test for Resource assertions
 */


TEST_INIT;

//! Simple device which defines its own parameter set object.
class SimpleDevice : public sparta::Resource
{
public:

    class ParameterSet; // Forward declared so our constructor can be at the top of this class def

    // Declaring as constexpr allows inline static const char*/float/double members
    static constexpr const char* name="SimpleDevice";

    SimpleDevice(sparta::TreeNode * node,
                 const SimpleDevice::ParameterSet * params) :
        sparta::Resource(node)
    {
        // All parameters are ignored
        params->foo.ignore();

        // Within a sparta::Resource
        sparta_assert_context(0, "Resource Assertion");
    }

    class ParameterSet : public sparta::ParameterSet
    {
    public:
        ParameterSet(sparta::TreeNode* parent) :
            sparta::ParameterSet(parent)
        {
        }

        PARAMETER(bool, foo, false, "A Parameter")
    };
};

class FooClass
{
public:
    FooClass()
    {
        // Within non-sparta class with no getClock method
        sparta_assert_context(0, "Foo Assertion");
    }
};

class BarClass
{
public:

    void getClock() const {};

    BarClass()
    {
        // COMPILE ERROR BECAUSE getClock is the wrong signature
        //sparta_assert_context(0, "Bar Assertion");
    }
};

class FizClass
{
public:
    sparta::Clock clk_;

    // Note the non-const method
    sparta::Clock* getClock() {return &clk_;};

    FizClass(sparta::Scheduler & sched_) :
        clk_("dummy_clock", &sched_)
    {
        // Within non-sparta class with non-const getClock returning non-const Clock*
        sparta_assert_context(0, "Fiz Assertion");
    }
};

class BinClass : public sparta::TreeNode
{
public:

    BinClass(TreeNode* root) :
        TreeNode(root, "bin", "The BinClass")
    {
        // Within TreeNode subclass with const
        sparta_assert_context(0, "Bin Assertion");
    }
};

class BuzClass
{
public:

    sparta::Scheduler sched_;
    sparta::Clock clk_;
    const sparta::Clock* getClock() const {return &clk_;};

    BuzClass() :
        clk_("dummy_clock", &sched_)
    {
    }

    // Note that this is a const method
    void causeAssertion() const {
        // Within non-sparta class with non-const getClock returning const Clock*
        sparta_assert_context(0, "Buz Assertion");
    }
};

class BizClass
{
public:
    sparta::Scheduler sched_;
    sparta::Clock clk_;

    // Note the non-const method with const return
    const sparta::Clock* getClock() {return &clk_;};

    BizClass() :
        clk_("dummy_clock", &sched_)
    {
        // Within non-sparta class with const getClock returning const Clock*
        sparta_assert_context(0, "Biz Assertion");
    }
};

int main()
{
    { // Scope to this block
        sparta::Scheduler sched;
        sparta::RootTreeNode root;
        sparta::Clock clk("clock", &sched);
        sparta::TreeNode dummy(&root, "dummy", "dummy node");
        SimpleDevice::ParameterSet ps(&dummy);
        ps.foo = true;

        EXPECT_TRUE(sched.getCurrentTick() == 0); //unfinalized sched at tick 0
        EXPECT_TRUE(sched.isRunning() == 0);
        sched.finalize();

        // proceed to tick 1, nothing should happen, but time advancement
        sched.run(1, true, false);

        sched.run(100, true);


        EXPECT_THROW_MSG_CONTAINS(SimpleDevice dev(&dummy, &ps),
                                  "0: Resource Assertion: in file:");

        EXPECT_THROW_MSG_CONTAINS(SimpleDevice dev(&root, &ps),
                                  "ResourceAssert_test.cpp', on line: 37 within resource at: top (no clock associated) (no scheduler associated)");

        EXPECT_THROW_MSG_CONTAINS(FooClass foo,
                                  "ResourceAssert_test.cpp', on line: 58 (from non-sparta context at (no scheduler associated))");

        EXPECT_THROW_MSG_CONTAINS(FizClass fiz(sched),
                                  "ResourceAssert_test.cpp', on line: 87 at cycle: 101 tick: 101");

        EXPECT_THROW_MSG_CONTAINS(BinClass bin(&root),
                                  "ResourceAssert_test.cpp', on line: 99 within TreeNode: top.bin (no clock associated) (no scheduler associated)");

        EXPECT_THROW_MSG_CONTAINS(BuzClass buz; buz.causeAssertion(),
                                  "ResourceAssert_test.cpp', on line: 119 at cycle: 0 tick: 0");

        EXPECT_THROW_MSG_CONTAINS(BizClass biz,
                                  "0: Biz Assertion: in file:");

        // Done

        root.enterTeardown();

        REPORT_ERROR;
    }

    // Diagnostic printing of all unfreed TreeNodes. A few are expected
    std::cout << "\nUnfreed TreeNodes (some globals expected)" << std::endl;
    std::cout << sparta::TreeNode::formatAllNodes() << std::endl;

    return ERROR_CODE;
}
