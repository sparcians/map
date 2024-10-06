#include <inttypes.h>
#include <iostream>

#include "sparta/sparta.hpp"
#include "sparta/utils/Enum.hpp"
#include "sparta/utils/SpartaTester.hpp"

TEST_INIT

using sparta::utils::Enum;
using namespace std;

enum class _Bar
{
    __FIRST = 0,
    A = 0,
    B,
    C,
    __LAST
};
template<>
const unique_ptr<string[]> Enum<_Bar>::names_(new string[static_cast<uint32_t>(_Bar::__LAST) + 1]);

Enum<_Bar>                BarType(_Bar::A, "A", _Bar::B, "B", _Bar::C, "C");
typedef Enum<_Bar>::Value BarValueType;

BarValueType func(const BarValueType& bar)
{
    cout << "func::" << string(bar) << endl;
    return bar;
}

void func2()
{
    BarValueType    array[static_cast<uint32_t>(_Bar::__LAST)];
    array[BarType(_Bar::A)] = _Bar::C;
    array[BarType(_Bar::B)] = _Bar::B;
    array[BarType(_Bar::C)] = _Bar::A;

    uint32_t n = 0;
    string names[] = {"C", "B", "A"};
    for (auto i = BarType.begin(); i != BarType.end(); ++i) {
        EXPECT_TRUE(array[*i] == 2 - n);
        EXPECT_TRUE(string(array[*i]) == names[n]);
        cout << "func2::" << string(array[*i]) << endl;
        ++n;
    }
}

int main()
{
    string names[] = {"A", "B", "C"};

    EXPECT_TRUE(func(_Bar::A) == 0);
    EXPECT_TRUE(string(func(_Bar::A)) == "A");
    EXPECT_EQUAL(BarType.size(), 3);

    func2();

    uint32_t n = 0;
    for (auto i : BarType) {
        EXPECT_TRUE(i == n);
        EXPECT_TRUE(string(i) == names[n]);
        EXPECT_TRUE(uint32_t(BarType(names[n])) == n);
        cout << string(i) << ":" << i << endl;
        ++n;
    }

    EXPECT_TRUE(BarType("A") == _Bar::A);

    try {
        EXPECT_FALSE(BarType("Foo"));
    } catch (Enum<_Bar>::UnknownNameException &) {
        EXPECT_TRUE(true);
        cout << "Caught UnknownNameException" << endl;
    };

    switch (_Bar(BarType("B")))
    {
        case _Bar::B:
            EXPECT_TRUE(true);
            break;
        default:
            EXPECT_FALSE(true);
    }
    REPORT_ERROR;

    return ERROR_CODE;
}
