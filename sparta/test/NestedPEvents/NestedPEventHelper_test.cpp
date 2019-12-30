/*!
 * \file NestedPEventHelper_test.cpp
 * \brief Test for Nested Pevents generation.
 */

#include <iostream>
#include <utility>

#include "sparta/log/MessageSource.hpp"
#include "sparta/sparta.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/pevents/PeventTrigger.hpp"
#include "sparta/pevents/NestedPeventCollector.hpp"
#include "sparta/pevents/PeventController.hpp"
#include "sparta/pevents/PEventHelper.hpp"
#include "sparta/pairs/SpartaKeyPairs.hpp"
#include "sparta/utils/MetaTypeList.hpp"

class Derived_1;
class Derived_2;
class Derived_3;
class Derived_4;

/*
 * Base class
 */
class Base {
public:

    //! This is the only thing Modelers must
    //  do for the Flatenning Virtual Base Pointer
    //  to work. They must use a TypeList and use
    //  create_t method to pushback all the Derived
    //  types that are there. Any time there is a new
    //  derived type from this Base class, that class
    //  must be appended to this TypeList.
    using derived_type_list = MetaTypeList::create_t<Derived_1, Derived_2, Derived_3, Derived_4>;

    //! The Base class must be pure virtual or abstract.
    //  Just being polymorphic does not cut it, because
    //  many classes contain virtual destructors without
    //  inheritance.
    virtual ~Base() = 0;
};

//! Define the Virtual Destructor to satisfy the Linker.
Base::~Base() {}

enum class EnumClass {
    STAGE_0 = 0,
    __FIRST = STAGE_0,
    STAGE_1,
    STAGE_2,
    STAGE_3,
    __LAST };

inline std::ostream & operator << (std::ostream & os, const EnumClass & obj) {
    switch(obj) {
        case EnumClass::STAGE_0:
            os << "STAGE_0";
            break;
        case EnumClass::STAGE_1:
            os << "STAGE_1";
            break;
        case EnumClass::STAGE_2:
            os << "STAGE_2";
            break;
        case EnumClass::STAGE_3:
            os << "STAGE_3";
            break;
        case EnumClass::__LAST:
            throw sparta::SpartaException("__LAST cannot be a valid enum state.");
    }
    return os;
}

class DPPairDef;
class DrawPacket {
public:
    using type = DPPairDef;
    DrawPacket(uint16_t val1, uint32_t val2, double val3,
        const EnumClass & val4, uint64_t val5, std::string val6) :
        val_1_(val1), val_2_(val2), val_3_(val3),
        val_4_(val4), val_5_(val5), val_6_(val6) {};

    uint16_t getVal1() const { return val_1_; }
    uint32_t getVal2() const { return val_2_; }
    double getVal3() const { return val_3_; }
    EnumClass getVal4() const { return val_4_; }
    std::pair<uint64_t, std::string> getPair() const { return std::make_pair(val_5_, val_6_); }
    virtual ~DrawPacket() {}
private:
    uint16_t val_1_;
    uint32_t val_2_;
    double val_3_;
    EnumClass val_4_;
    uint64_t val_5_;
    std::string val_6_;
};

class DPPairDef : public sparta::PairDefinition<DrawPacket> {
public:
    DPPairDef() : sparta::PairDefinition<DrawPacket>(){
        SPARTA_INVOKE_PAIRS(DrawPacket);
    }

    SPARTA_REGISTER_PAIRS(SPARTA_ADDPAIR("Draw-P_Val_1", &DrawPacket::getVal1, std::ios::hex),
                        SPARTA_ADDPAIR("Draw_P_Val_2", &DrawPacket::getVal2, std::ios::hex),
                        SPARTA_ADDPAIR("Draw-P_Val_3", &DrawPacket::getVal3),
                        SPARTA_ADDPAIR("Draw-P_Val_4", &DrawPacket::getVal4),
                        SPARTA_ADDPAIR("Draw-P_Val_5", &DrawPacket::getPair),
                        SPARTA_ADDPAIR("Draw-P_Val_6", &DrawPacket::getPair));
};

//! Forward Declaration of class.
class Derived_1PairDef;

//! Class Derived_1.
class Derived_1 : public Base {
public:
    using type = Derived_1PairDef;
    Derived_1(uint16_t val1, uint32_t val2,
        double val3, std::string val4,
            const std::shared_ptr<DrawPacket> & ptr) :
        val_1_(val1), val_2_(val2), val_3_(val3), val_4_(val4), val_5_(ptr) {}
    uint16_t getVal1() const { return val_1_; }
    uint32_t getVal2() const { return val_2_; }
    std::pair<double, std::string> getPairs() const { return std::make_pair(val_3_, val_4_); }
    const std::shared_ptr<DrawPacket> & getDP() const { return val_5_; }
    virtual ~Derived_1() {}
private:
    uint16_t val_1_;
    uint32_t val_2_;
    double val_3_;
    std::string val_4_;
    std::shared_ptr<DrawPacket> val_5_ = nullptr;
};

class Derived_1PairDef : public sparta::PairDefinition<Derived_1> {
public:
    Derived_1PairDef() : sparta::PairDefinition<Derived_1>(){
        SPARTA_INVOKE_PAIRS(Derived_1);
    }

    SPARTA_REGISTER_PAIRS(SPARTA_ADDPAIR("D1_Val_1", &Derived_1::getVal1, std::ios::hex),
                        SPARTA_ADDPAIR("D1_Val_2", &Derived_1::getVal2),
                        SPARTA_ADDPAIR("D1_Val_3", &Derived_1::getPairs),
                        SPARTA_ADDPAIR("D1_Val_4", &Derived_1::getPairs),
                        SPARTA_FLATTEN(&Derived_1::getDP));
};

//! Forward Declaration of class.
class Derived_2PairDef;

//! Class Derived_2.
class Derived_2 : public Base {
public:
    using type = Derived_2PairDef;
    Derived_2(uint16_t val1, uint32_t val2) :
        val_1_(val1), val_2_(val2) {}
    uint16_t getVal1() const { return val_1_; }
    uint32_t getVal2() const { return val_2_; }
    virtual ~Derived_2() {}
private:
    uint16_t val_1_;
    uint32_t val_2_;
};

class Derived_2PairDef : public sparta::PairDefinition<Derived_2> {
public:
    Derived_2PairDef() : sparta::PairDefinition<Derived_2>(){
        SPARTA_INVOKE_PAIRS(Derived_2);
    }

    SPARTA_REGISTER_PAIRS(SPARTA_ADDPAIR("D2_Val_1", &Derived_2::getVal1, std::ios::hex),
                        SPARTA_ADDPAIR("D2_Val_2", &Derived_2::getVal2));
};

//! Forward Declaration of class.
class Derived_3PairDef;

//! Class Derived_3.
class Derived_3 : public Base {
public:
    using type = Derived_3PairDef;
    Derived_3(uint16_t val1, uint32_t val2, double val3, std::string val4, std::string val5) :
        val_1_(val1), val_2_(val2), val_3_(val3), val_4_(val4), val_5_(val5) {}
    uint16_t getVal1() const { return val_1_; }
    uint32_t getVal2() const { return val_2_; }
    std::pair<double, std::string> getPairs() const { return std::make_pair(val_3_, val_4_); }
    std::string getString() const { return val_5_; }
    virtual ~Derived_3() {}
private:
    uint16_t val_1_;
    uint32_t val_2_;
    double val_3_;
    std::string val_4_;
    std::string val_5_;
};

class Derived_3PairDef : public sparta::PairDefinition<Derived_3> {
public:
    Derived_3PairDef() : sparta::PairDefinition<Derived_3>(){
        SPARTA_INVOKE_PAIRS(Derived_3);
    }

    SPARTA_REGISTER_PAIRS(SPARTA_ADDPAIR("D3_Val_1", &Derived_3::getVal1, std::ios::oct),
                        SPARTA_ADDPAIR("D3_Val_2", &Derived_3::getVal2, std::ios::hex),
                        SPARTA_ADDPAIR("D3_Val_3", &Derived_3::getPairs),
                        SPARTA_ADDPAIR("D3_Val_4", &Derived_3::getPairs),
                        SPARTA_ADDPAIR("D3_Val_5", &Derived_3::getString));
};

//! Forward Declaration of class.
class Derived_4PairDef;

//! Class Derived_3.
class Derived_4 : public Base {
public:
    using type = Derived_4PairDef;

    Derived_4(bool b_v, uint16_t val1, uint32_t val2, float val3,
        double dv, std::string val4, std::string val5) :
        bool_val_(b_v), val_1_(val1), val_2_(val2),
        val_3_(val3), d_v_(dv), val_4_(val4), val_5_(val5) {}
    bool getBV() const { return bool_val_; }
    uint16_t getVal1() const { return val_1_; }
    uint32_t getVal2() const { return val_2_; }
    float getFV() const { return val_3_; }
    std::pair<double, std::string> getPairs() const { return std::make_pair(d_v_, val_4_); }
    std::string getString() const { return val_5_; }
    virtual ~Derived_4() {}
private:
    bool bool_val_;
    uint16_t val_1_;
    uint32_t val_2_;
    float val_3_;
    double d_v_;
    std::string val_4_;
    std::string val_5_;
};

class Derived_4PairDef : public sparta::PairDefinition<Derived_4> {
public:
    Derived_4PairDef() : sparta::PairDefinition<Derived_4>(){
        SPARTA_INVOKE_PAIRS(Derived_4);
    }

    SPARTA_REGISTER_PAIRS(SPARTA_ADDPAIR("D4_Val_1", &Derived_4::getBV),
                        SPARTA_ADDPAIR("D4_Val_2", &Derived_4::getVal1, std::ios::hex),
                        SPARTA_ADDPAIR("D4_Val_3", &Derived_4::getVal2, std::ios::oct),
                        SPARTA_ADDPAIR("D4_Val_4", &Derived_4::getFV),
                        SPARTA_ADDPAIR("D4_Val_5", &Derived_4::getPairs),
                        SPARTA_ADDPAIR("D4_Val_6", &Derived_4::getPairs),
                        SPARTA_ADDPAIR("D4_Val_7", &Derived_4::getString));
};
//! Forward Declaration of class.
class CollectedA;

/*
 * We have a class that we are going to collect
 */
class A {
public:
    using type = CollectedA;
    A(uint16_t val, uint16_t lav, uint32_t foo,
        uint64_t bar, const std::string& q,
            const std::shared_ptr<Base> & bp) :
        i(val), j(lav), k(foo), l(bar), x(q), b(bp) {}
    void setX(std::string val) { x = val; }
    uint16_t geti_() const { return i; }
    uint16_t getj_() const { return j; }
    uint32_t getk_() const { return k; }
    uint64_t  getl_() const { return l;}
    std::string getx_() const { return x; }
    const std::shared_ptr<Base> & getBP() const { return b; }
    const std::string& getrefx_() const { return x; }

private:
    const uint16_t i;
    const uint16_t j;
    const uint32_t k;
    const uint64_t l;
    std::string x;

    //! Generic Base pointer.
    std::shared_ptr<Base> b = nullptr;
};

typedef std::shared_ptr<A> A_Ptr;

/*
 * The user creates a class to represent the attributes
 * of A that they wish to collect
 */
class CollectedA : public sparta::PairDefinition<A> {
public:
    CollectedA() : sparta::PairDefinition<A>(){
        SPARTA_INVOKE_PAIRS(A);
    }

    SPARTA_REGISTER_PAIRS(SPARTA_ADDPAIR("i_val_", &A::geti_),
                        SPARTA_ADDPAIR("j_val_", &A::getj_),
                        SPARTA_ADDPAIR("k_val_", &A::getk_),
                        SPARTA_ADDPAIR("l_val_", &A::getl_, std::ios::hex),
                        SPARTA_ADDPAIR("x_val_", &A::getx_),
                        SPARTA_FLATTEN(&A::getBP));
};

class CollectedB;
class B {
public:
    using type = CollectedB;

    B(const A_Ptr& ptr, uint16_t val, uint16_t lav,
        uint32_t foo, uint64_t bar, const std::string& q,
            std::unique_ptr<Base> bp) :
        a_ptr_(ptr), i(val), j(lav), k(foo), l(bar), x(q),
        b(std::move(bp)) {}
    void setX(std::string val) { x = val; }
    const A_Ptr & getNestedPtr() const { return a_ptr_; }
    uint16_t geti_() const { return i; }
    uint16_t getj_() const { return j; }
    uint32_t getk_() const { return k; }
    uint64_t  getl_() const { return l; }
    std::string getx_() const { return x; }
    const char* getCharP_() const { return "Hello World!"; }
    const std::string& getrefx_() const { return x; }
    const std::unique_ptr<Base> & getBP() const { return b; }
private:
    A_Ptr a_ptr_;
    const uint16_t i;
    const uint16_t j;
    const uint32_t k;
    const uint64_t l;
    std::string x;

    //! Generic Base pointer.
    std::unique_ptr<Base> b = nullptr;
};

/*
 * The user creates a class to represent the attributes
 * of B that they wish to collect
 */
class CollectedB : public sparta::PairDefinition<B> {
public:
    CollectedB() : sparta::PairDefinition<B>(){
        SPARTA_INVOKE_PAIRS(B);
    }

    SPARTA_REGISTER_PAIRS(SPARTA_ADDPAIR("a_val_", &B::geti_, std::ios::oct),
                        SPARTA_ADDPAIR("b_val_", &B::getj_, std::ios::hex),
                        SPARTA_ADDPAIR("c_val_", &B::getk_),
                        SPARTA_ADDPAIR("d_val_", &B::getl_),
                        SPARTA_ADDPAIR("e_val_", &B::getx_),
                        SPARTA_ADDPAIR("char_pointer_", &B::getCharP_),
                        SPARTA_FLATTEN(&B::getBP),
                        SPARTA_FLATTEN(&B::getNestedPtr));
};

class LambdaCollectPD;
class LambdaCollect {
public:
    using type = LambdaCollectPD;
    LambdaCollect(uint16_t i, uint32_t j, uint32_t k) : i{i}, j{j}, k{k} {}
    uint16_t getI() const { return i; };
    uint32_t getJ() const { return j; };
    uint32_t getK() const { return k; };
    std::function<uint32_t ()> const testLambda_f {[this]()->uint32_t{
            return this->getI() + this->getJ() - this->getK(); }};
private:
    const uint16_t i;
    const uint32_t j;
    const uint32_t k;
};

/*
 * The user creates a class to represent the attributes
 * of LambdaCollect that they wish to collect. Users can create
 * lambdas representing any mathematical/statistical expression
 * using member-fields of this class and collect it for pevent/pipeline.
 * Users no longer needs to make every SPARTA_ADDPAIR() function pointer a method
 * in the class. Users can now collect values from expressions built on the fly.
 */
auto lambda_1 = []()->int{ return 100; };
auto lambda_2 = []()->int{ return 212; };
auto lambda_3 = []()->double{ return 90.223 + 1.09; };
auto lambda_4 = []()->double{ return 3.145 + 577; };
auto lambda_5 = [](const LambdaCollect & l)->uint32_t{
    return l.getI() + l.getJ(); };
auto lambda_6 = [](const LambdaCollect & l)->uint32_t{
    return l.getI() * l.getJ(); };
auto lambda_7 = [](const LambdaCollect & l)->uint32_t{
    return l.getI() - l.getJ() + 20; };
auto lambda_8 = [](const LambdaCollect & l)->double{
    return ((l.getI() / l.getJ()) + (90 * 88.21)); };
auto lambda_9 = [](const LambdaCollect & l)->double{
    return ((l.getI() / l.getJ()) + (90 * l.getK()+88.21)); };
auto lambda_10 = [](const LambdaCollect & l)->double{
    return (l.getI() / (l.getJ() * l.getK()) + (90/l.getI() * 88.21
    + (l.getK() ^ l.getJ()))); };

std::function<int ()> const f_1 = lambda_1;
std::function<int ()> const f_2 = lambda_2;
std::function<double ()> const f_3 = lambda_3;
std::function<double ()> const f_4 = lambda_4;
std::function<uint32_t (const LambdaCollect &)> const f_5 = lambda_5;
std::function<uint32_t (const LambdaCollect &)> const f_6 = lambda_6;
std::function<uint32_t (const LambdaCollect &)> const f_7 = lambda_7;
std::function<uint32_t (const LambdaCollect &)> const f_8 = lambda_8;
std::function<uint32_t (const LambdaCollect &)> const f_9 = lambda_9;
std::function<uint32_t (const LambdaCollect &)> const f_10 = lambda_10;

class LambdaCollectPD : public sparta::PairDefinition<LambdaCollect> {
public:
    LambdaCollectPD() : sparta::PairDefinition<LambdaCollect>(){
        SPARTA_INVOKE_PAIRS(LambdaCollect);
    }

    SPARTA_REGISTER_PAIRS(SPARTA_ADDPAIR("lambda_random_val_", f_1, std::ios::hex),
                        SPARTA_ADDPAIR("lambda_random_2_val_", f_2, std::ios::hex),
                        SPARTA_ADDPAIR("lambda_d_val_", f_3, std::ios::hex),
                        SPARTA_ADDPAIR("lambda_d+t_val_", f_4, std::ios::hex),
                        SPARTA_ADDPAIR("lambda_i+j_val_", f_5, std::ios::hex),
                        SPARTA_ADDPAIR("lambda_i*j_val_", f_6, std::ios::hex),
                        SPARTA_ADDPAIR("lambda_i-j+t_val_", f_7, std::ios::hex),
                        SPARTA_ADDPAIR("lambda_i/j+t*d_val_", f_8, std::ios::hex),
                        SPARTA_ADDPAIR("lambda_i/j+t*k+d_val_", f_9, std::ios::hex),
                        SPARTA_ADDPAIR("lambda_i/j*k+t/i*d+k^j_val_", f_10, std::ios::hex),
                        SPARTA_ADDPAIR("member_lambda_i+j-k_val_", &LambdaCollect::testLambda_f, std::ios::hex));
};

int main() {
    sparta::RootTreeNode root("root", "root node");
    sparta::TreeNode child("child", "child node");
    root.addChild(&child);
    sparta::Clock clk("clock");

    // ------ NestedPeventCollector test ----
    sparta::pevents::NestedPeventCollector<A> decode_pevent("DECODE", &child, &clk);
    sparta::pevents::NestedPeventCollector<A> pair_pevent("RETIRE", &child, &clk);
    sparta::pevents::NestedPeventCollector<A> bar_pevent("PREFETCH", &child, &clk);
    sparta::pevents::NestedPeventCollector<B> nested_pevent("RENAME", &child, &clk);
    sparta::pevents::NestedPeventCollector<B> nested_pevent_2("FETCH", &child, &clk);
    sparta::pevents::NestedPeventCollector<LambdaCollect> lambda_pevent("EXECUTE", &child, &clk);

    // create a pevent with an extra positional arg.
    sparta::pevents::NestedPeventCollector<A> my_pevent("MY_EVENT", &child, &clk);
    my_pevent.addPositionalPairArg<uint32_t>("extra_arg");

    sparta::pevents::NestedPeventCollector<A> pair_verbose_pevent("RETIRE", &child, &clk, true);
    bool verbose_tap = false;
    sparta::pevents::PeventCollectorController controller;
    controller.cacheTap("pair.log", "DECODE", verbose_tap);
    controller.cacheTap("pair.log", "RETIRE", verbose_tap);
    controller.cacheTap("pair.log", "PREFETCH", verbose_tap);
    controller.cacheTap("pair.log", "RENAME", verbose_tap);
    controller.cacheTap("pair.log", "FETCH", verbose_tap);
    controller.cacheTap("pair.log", "EXECUTE", verbose_tap);
    controller.cacheTap("all.log", "ALL", !verbose_tap);
    controller.finalize(&root);
    sparta::trigger::PeventTrigger trigger(&root);
    trigger.go();

    //! Instantiate DrawPacket.
    auto dp = std::make_shared<DrawPacket>(619, 747, 0.0092, EnumClass::STAGE_2, 32189, "Hello.");

    //! Instantiate base pointers pointing to different Derived types.
    auto b1 = std::make_shared<Derived_1>(18, 19, 89.273, "World.", dp);
    auto b2 = std::make_shared<Derived_2>(20, 21);
    std::unique_ptr<Derived_3> b3(new Derived_3(66, 67, 189.3244, "PQRSTUV", "456789"));
    std::unique_ptr<Derived_4> b4(new Derived_4(false, 91, 2, 3.14783, 7221.21212, "NestedPevent", "Tester"));

    A object_a1(1000, 78, 52, 143, "test0", b1);
    A object_a2(25, 21, 43, 66, "test1", b2);
    B object_b1(std::make_shared<A>(object_a1), 1209, 55, 18, 97, "test2", std::move(b3));
    B object_b2(std::make_shared<A>(object_a2), 666, 747, 901877, 3217, "test3", std::move(b4));

    LambdaCollect l(199, 398, 572);

    pair_pevent.collect(object_a1);
    bar_pevent.collect(object_a2);
    pair_verbose_pevent.collect(object_a1);
    decode_pevent.collect(object_a1);
    my_pevent.collect(object_a1, 32);
    lambda_pevent.collect(l);
    pair_pevent.isCollecting();
    bar_pevent.isCollecting();
    nested_pevent.collect(object_b1);
    nested_pevent_2.collect(object_b2);
    sparta::log::MessageSource logger_pevent_(&root, "regress", "LSU PEvents");
    sparta::log::Tap tap(sparta::TreeNode::getVirtualGlobalNode(), "regress", "log.log");

    //Try to make sure the PEvent stuff compiles
    sparta::pevents::PEvent<int, int, std::string> p(
        "NAME", logger_pevent_, &clk, std::string("first_param"),
        std::string("second_param"), "third_param");
    p.setAttrs(5, 3, std::string("some string"));
    p.setAttr<int, 1>(300);
    p.fireEvent();
    p.setAsHex({0});
    p.setFormatFlags(0, sparta::pevents::FormatFlags(std::ios::hex),
        sparta::pevents::FormatFlags(std::ios::dec), "0x", "");
    p.fireEvent(1000, 3000, "another string");
    p.setAsStrings({2});
    p.setAsHex({1});
    EXPECT_THROW(p.fireEvent(23, 15, "something else"));

    root.enterTeardown();

    REPORT_ERROR;
    return ERROR_CODE;
}
