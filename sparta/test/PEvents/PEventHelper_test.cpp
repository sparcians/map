#include <tuple>
#include <array>
#include <iostream>
#include <utility>

#include "sparta/log/MessageSource.hpp"
#include "sparta/sparta.hpp"
#include "sparta/pevents/PEventHelper.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/pevents/PeventTrigger.hpp"
#include "sparta/pevents/PeventCollector.hpp"
#include "sparta/pevents/PeventController.hpp"

using namespace sparta;

/*
 * We have a class that we are going to collect
 */
class A{
    friend class CollectedA;
public:
    A(int val, int lav, int foo, double bar, const std::string& q) :
        i(val),
        j(lav),
        k(foo),
        l(bar),
        x(q) {}

    void setX(std::string val) {
        x = val;
    }

private:

    int geti_() const { return i; }
    int getj_() const { return j; }
    int getk_() const { return k; }
    double  getl_() const { return l; }
    std::string getx_() const {return x; }

    // functions returning reference should be fine,
    // but we are going to remove the reference in our framework.
    const std::string& getrefx_() const { return x; }
    const int i;
    const int j;
    const int k;
    const double l;
    std::string x;
};

/*
 * The user creates a class to represent the attributes
 * of A that they wish to collect
 */
class CollectedA : public PairDefinition<A> {
public:

    typedef A TypeCollected;

    CollectedA() : PairDefinition<A>() {

        // The user must define which attributes it would like to capture.
        //bool hex = true;
        addPEventsPair("i_val_hex", &A::geti_, std::ios_base::hex);
        addPEventsPair("j_val_oct", &A::getj_, std::ios_base::oct);
        addPEventsPair("k_val_dec", &A::getk_);
        addPEventsPair("l_val_fixed", &A::getl_, std::ios_base::fixed);
        addPEventsPair("l_val_scientific", &A::getl_, std::ios_base::scientific);
        addPEventsPair("x_val", &A::getx_);
        addPEventsPair("xref_val", &A::getrefx_);
    }
};

int main() {

    RootTreeNode root("root", "root node");
    TreeNode child("child", "child node");
    root.addChild(&child);
    sparta::Scheduler sched;
    Clock clk("clock", &sched);

    // ------ PeventCollector test ----
    pevents::PeventCollector<CollectedA> decode_pevent("DECODE", &child, &clk);
    pevents::PeventCollector<CollectedA> pair_pevent("RETIRE", &child, &clk);

    // create a pevent with an extra positional arg.
    pevents::PeventCollector<CollectedA> my_pevent("MY_EVENT", &child, &clk);
    my_pevent.addPositionalPairArg<uint32_t>("extra_arg");

    pevents::PeventCollector<CollectedA> pair_verbose_pevent("RETIRE", &child, &clk, true);
    bool verbose_tap = false;
    pevents::PeventCollectorController controller;
    controller.cacheTap("pair.log", "RETIRE", verbose_tap);
    controller.cacheTap("all.log", "ALL", !verbose_tap);
    controller.finalize(&root);
    trigger::PeventTrigger trigger(&root);
    trigger.go();
    A a(1000, 78, 52, 0.01, "test0");
    pair_pevent.collect(a);
    pair_verbose_pevent.collect(a);
    decode_pevent.collect(a);
    my_pevent.collect(a, 32);
    pair_pevent.isCollecting();
    log::MessageSource logger_pevent_(&root, "regress", "LSU PEvents");
    log::Tap tap(TreeNode::getVirtualGlobalNode(), "regress", "log.log");

    //Try to make sure the PEvent stuff compiles
    pevents::PEvent<int, int, std::string> p("NAME", logger_pevent_, &clk, std::string("first_param"), std::string("second_param"), "third_param");
    p.setAttrs(5, 3, std::string("some string"));
    p.setAttr<int, 1>(300);
    p.fireEvent();
    p.setAsHex({0});
    p.setFormatFlags(0, pevents::FormatFlags(std::ios::hex), pevents::FormatFlags(std::ios::dec), "0x", "");

    //p.setFormatLength(0, 8, std::ios::left, '0');
    p.fireEvent(1000, 3000, "another string");

    //p.setFormatFlags(2, "\"", "\"");
    p.setAsStrings({2});
    p.setAsHex({1});
    EXPECT_THROW(p.fireEvent(23, 15, "something else"));

    root.enterTeardown();

    REPORT_ERROR;
    return ERROR_CODE;
}
