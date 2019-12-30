
#include "sparta/pairs/SpartaKeyPairs.hpp"
#include "sparta/sparta.hpp"
#include "sparta/utils/SpartaTester.hpp"
using namespace sparta;

uint32_t some_arbitrary_data;

/*
 * Create a dummy collector that just writes new collected strings
 * as a line in standard out
 *
 * This would be like ManualCollector or something in pipeline collection.
 */
template <class CollectedEntityType>
class TestCollector : public PairCollector<CollectedEntityType>
{
    using PairCollector<CollectedEntityType>::getPEventLogVector;
    using PairCollector<CollectedEntityType>::turnOn_;
    using PairCollector<CollectedEntityType>::turnOff_;
    using PairCollector<CollectedEntityType>::defaultCollect_;
public:
    void turnOn()
    {
        turnOn_();
    }
    void turnOff()
    {
        turnOff_();
    }

    template <typename... Targs>
    void collect(const Targs&... pos_args)
    {
        defaultCollect_(pos_args...);
    }
protected:


    // We had a dirty collection, we can do our work here.
    virtual void generateCollectionString_() override final
    {
        for(auto& pair : getPEventLogVector())
        {
            std::cout << pair.first << " = " << pair.second << " : ";
        }
        std::cout << std::endl;
    }


};



/*
 * We have a class that we are going to collect
 */
class A{
    friend class CollectedA;
public:
    A(int val, const std::string& q) :
        i(val),
        x(q)
    {}

    void setX(std::string val) { x = val; }
    A(const A&) = delete;
private:
    int geti_() const { return i; }
    std::string getx_() const {return x; }
    // functions returning reference should be fine,
    // but we are going to remove the reference in our framework.
    const std::string& getrefx_() const { return x; }
    const int i;
    std::string x;

};

/*
 * The user creates a class to represent the attributes
 * of A that they wish to collect
 */
class CollectedA : public PairDefinition<A>
{
public:
    // The creator of CollectedA needs to friend A, b/c you can't inherit friendlyness
    friend class A;
    typedef A TypeCollected;

    CollectedA() :
        PairDefinition<A>()
    {
        // The user must define which attributes it would like to capture.
        addPair("i_val", &A::geti_);
        addPair("x_val", &A::getx_);
        addPair("xref_val", &A::getrefx_);
        addPositionalPairArg<uint32_t>(std::string("pos_1"));
    }

};

class MyPositionOnlyPairDef : public PairDefinition<NoEntity>
{
public:
    typedef sparta::NoEntity TypeCollected;

    MyPositionOnlyPairDef() :
        PairDefinition<TypeCollected>()
    {
        addPositionalPairArg<uint32_t>(std::string("pos_1"));
        addPositionalPairArg<uint32_t>(std::string("any_namepos_2"));
    }
};



/*
 * Create 3 different instances of our class, and
 * collect them.
 */
int main()
{

    TestCollector<CollectedA> my_collector;
    TestCollector<MyPositionOnlyPairDef> pos_collector;
    TestCollector<CollectedA> another_collector;
    another_collector.addPositionalPairArg<std::string>(std::string("dynamic_extra_arg"));
    another_collector.turnOn();
    pos_collector.turnOn();
    my_collector.turnOn();



    A a(0, "test0");
    A a1(1,"test1");
    A a2(2,"test2");

    pos_collector.collect(1, std::string("string_not_actually_type_checked"));

    another_collector.collect(a, 5, std::string("this is a test"));
    my_collector.collect(a, 10);
    my_collector.collect(a1, 7);
    my_collector.collect(a1, 10);
    another_collector.collect(a1, 2, std::string("this is ANOTHEr test"));
    a1.setX(std::string("changed_str"));
    my_collector.collect(a1, 12);
    my_collector.collect(a2, 32);


    REPORT_ERROR;
    return ERROR_CODE;
}
