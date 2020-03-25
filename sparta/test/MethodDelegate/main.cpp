
#include <inttypes.h>
#include <iostream>

#include "sparta/kernel/SpartaHandler.hpp"
#include "sparta/utils/SpartaTester.hpp"

TEST_INIT;

class MyMethods
{
public:

    MyMethods()
    {
        //uint32_t val = 5;

        nonconstmethod();

        // No use case for this
        // sparta::SpartaHandler constmethod =
        //     sparta::SpartaHandler::from_member<const MyMethods, &MyMethods::ConstMethod>(this);
        // (void)constmethod;

        // sparta::SpartaHandler nonconstptrmethod =
        //     sparta::SpartaHandler::from_member<MyMethods, uint32_t *,
        //                                         &MyMethods::NonConstPtrMethod>(this);
        // nonconstptrmethod(&val);

        // sparta::SpartaHandler constptrmethod =
        //     sparta::SpartaHandler::from_member<MyMethods,
        //                                         const uint32_t *,
        //                                         &MyMethods::ConstPtrMethod>(this);
        // constptrmethod(&val);

        // sparta::SpartaHandler constptrconstmethod =
        //     sparta::SpartaHandler::from_member<MyMethods,
        //                                         const uint32_t *,
        //                                         &MyMethods::ConstPtrConstMethod>(this);
        // (void)constptrconstmethod;
        // sparta::SpartaHandler nonconstrefmethod =
        //     sparta::SpartaHandler::from_member<MyMethods, uint32_t &,
        //                                         &MyMethods::NonConstRefMethod>(this);
        // nonconstrefmethod(val);

        const uint32_t val2 = 6;
        constrefmethod(&val2);

        // sparta::SpartaHandler constrefconstmethod =
        //     sparta::SpartaHandler::from_member<MyMethods, const uint32_t &,
        //                                         &MyMethods::ConstRefConstMethod>(this);
        // (void)constrefconstmethod;

    }

    virtual ~MyMethods() = default;

    void NonConstMethod() { ++executed; }
    void ConstMethod() const { std::cout << "This is ConstMethod()\n"; }

    virtual void VirtMethod() = 0;

    void NonConstPtrMethod(uint32_t *val) { std::cout << "This is val: " << *val << std::endl; }
    void ConstPtrMethod(const uint32_t *val) { std::cout << "This is val: " << *val << std::endl; }
    void ConstPtrConstMethod(const uint32_t *) const {}

    void NonConstRefMethod(uint32_t &val) { std::cout << "This is val: " << val << std::endl; }
    void ConstRefMethod(const uint32_t &val) { std::cout << "This is val: " << val << std::endl; }
    void ConstRefConstMethod(const uint32_t &val) const { std::cout << "This is val: " << val << std::endl; }

    uint32_t executed = 0;
    sparta::SpartaHandler nonconstmethod{sparta::SpartaHandler::from_member<MyMethods, &MyMethods::NonConstMethod>(this)};
    sparta::SpartaHandler constmethod{sparta::SpartaHandler::from_member<MyMethods, &MyMethods::ConstMethod>(this)};
    sparta::SpartaHandler constrefmethod{sparta::SpartaHandler::from_member_1<MyMethods, uint32_t, &MyMethods::ConstRefMethod>(this)};
    sparta::SpartaHandler constrefconstmethod{sparta::SpartaHandler::from_member_1<MyMethods, uint32_t, &MyMethods::ConstRefConstMethod>(this)};
};

void MyMethods::VirtMethod() { ++executed; }


class MyDerMethods : public MyMethods
{
public:
    void VirtMethod() { MyMethods::VirtMethod(); }
};

int main(int argc, char ** argv)
{
    MyDerMethods der_meth;
    MyMethods & mm = der_meth;

    const uint32_t UPPER = strtoul(argv[1], 0, 10);


    if(argc > 2) {
        std::cout << "Delegate" << std::endl;
        for(uint32_t i = 0; i < UPPER; ++i) {
            mm.nonconstmethod();
            mm.constmethod();
            mm.constrefmethod();
            mm.constrefconstmethod();
        }
    }
    else {
        std::cout << "Virt method" << std::endl;

        for(uint32_t i = 0; i < UPPER; ++i) {
            mm.VirtMethod();
        }
    }

    std::cout << "This is executed: " << mm.executed << std::endl;

    REPORT_ERROR;
    return ERROR_CODE;
}
