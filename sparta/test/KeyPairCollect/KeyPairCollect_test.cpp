
#include "sparta/pairs/SpartaKeyPairs.hpp"
#include "sparta/sparta.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include <algorithm>
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
        defaultCollect_((simdb::StreamBuffer*)nullptr, pos_args...);
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

struct SchemaInner {
    double d = 0.0;
    enum class Unit : uint8_t { Alpha = 0, Beta = 1 } unit = Unit::Alpha;

    double getD() const { return d; }
    Unit getUnit() const { return unit; }
};

struct SchemaOuter {
    std::string name;
    uint64_t value = 0;
    SchemaInner inner;

    const std::string & getName() const { return name; }
    uint64_t getValue() const { return value; }
    const SchemaInner & getInner() const { return inner; }
};

class CollectedSchemaOuter : public PairDefinition<SchemaOuter>
{
public:
    friend struct SchemaOuter;
    typedef SchemaOuter TypeCollected;

    CollectedSchemaOuter() : PairDefinition<SchemaOuter>()
    {
        addPair("name", &SchemaOuter::getName);
        addPair("value", &SchemaOuter::getValue);
        addPair("d", &SchemaOuter::getInner, &SchemaInner::getD);
        addPair("unit", &SchemaOuter::getInner, &SchemaInner::getUnit);
    }
};

static std::string expectedUint64Dtype()
{
    return (sizeof(uint64_t) == sizeof(unsigned long)) ? std::string("unsigned long")
                                                         : std::string("unsigned long long");
}

static void expectSchemasEqual(
    const std::vector<std::pair<std::string, std::string>> & got,
    const std::vector<std::pair<std::string, std::string>> & expected)
{
    EXPECT_EQUAL(got.size(), expected.size());
    const std::size_t n = std::min(got.size(), expected.size());
    for(std::size_t i = 0; i < n; ++i) {
        EXPECT_EQUAL(got[i].first, expected[i].first);
        EXPECT_EQUAL(got[i].second, expected[i].second);
    }
}

/*
 * Create 3 different instances of our class, and
 * collect them.
 */
int main()
{
    TEST_INIT;

    TestCollector<CollectedA> my_collector;
    TestCollector<MyPositionOnlyPairDef> pos_collector;
    TestCollector<CollectedA> another_collector;
    another_collector.addPositionalPairArg<std::string>(std::string("dynamic_extra_arg"));
    another_collector.turnOn();
    pos_collector.turnOn();
    my_collector.turnOn();

    {
        const std::vector<std::pair<std::string, std::string>> expected_collected_a = {
            {"i_val", "int"},
            {"x_val", "string"},
            {"xref_val", "string"},
            {"pos_1", "unsigned int"},
        };
        expectSchemasEqual(my_collector.getFlattenedFieldNameAndDtypeSchema(), expected_collected_a);
    }

    TestCollector<CollectedSchemaOuter> schema_collector;
    {
        const std::string u64 = expectedUint64Dtype();
        const std::vector<std::pair<std::string, std::string>> expected_nested = {
            {"name", "string"},
            {"value", u64},
            {"d", "double"},
            {"unit", "string"}, // TODO cnyce: come back here when enums are
                                // stored by their underlying int, not their
                                // TinyStrings ID
        };
        expectSchemasEqual(schema_collector.getFlattenedFieldNameAndDtypeSchema(), expected_nested);
    }

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

    {
        const std::vector<std::pair<std::string, std::string>> expected_with_dynamic = {
            {"i_val", "int"},
            {"x_val", "string"},
            {"xref_val", "string"},
            {"pos_1", "unsigned int"},
            {"dynamic_extra_arg", "string"},
        };
        expectSchemasEqual(another_collector.getFlattenedFieldNameAndDtypeSchema(), expected_with_dynamic);
    }

    REPORT_ERROR;
    return ERROR_CODE;
}
