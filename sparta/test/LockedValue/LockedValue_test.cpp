
#include "sparta/utils/LockedValue.hpp"
#include "sparta/utils/ValidValue.hpp"
#include "sparta/utils/SpartaTester.hpp"

#include <iostream>

TEST_INIT
#define PRINT_ENTER_TEST \
  std::cout << std::endl; \
  std::cout << "*************************************************************" \
            << "*** Beginning '" << __FUNCTION__ << "'" \
            << "*************************************************************" \
            << std::endl;

namespace {
    template <typename T>
    bool lockedValueCompare(
        sparta::utils::LockedValue<sparta::utils::ValidValue<T>> & lhs,
        sparta::utils::ValidValue<T> & rhs)
    {
        return (lhs.getValue() == rhs);
    }
}

void testLockedValue(){
    PRINT_ENTER_TEST;
    namespace ru = sparta::utils;

    uint16_t data_1 = uint16_t();
    uint32_t data_2 = 32;
    uint64_t data_3 = 64;
    uint64_t data_4 = 128;
    double   data_5 = 3.14;
    uint32_t data_6 = 512;

    ru::LockedValue<uint16_t> lv_1;
    ru::LockedValue<uint32_t> lv_2(data_2);
    ru::LockedValue<uint64_t> lv_3(data_3, false);
    ru::LockedValue<uint64_t> lv_4(data_4, true);
    ru::LockedValue<double>   lv_5;
    ru::LockedValue<uint32_t> lv_6 = {data_6, false};

    EXPECT_NOTHROW(lv_5.lock());
    EXPECT_NOTHROW(lv_5.lock());
    EXPECT_TRUE(lv_5.isLocked());
    EXPECT_NOTHROW(lv_5.getValue());
    EXPECT_THROW(lv_5 = data_5);

    EXPECT_FALSE(lv_1.isLocked());
    EXPECT_FALSE(lv_2.isLocked());
    EXPECT_FALSE(lv_3.isLocked());
    EXPECT_TRUE(lv_4.isLocked());
    EXPECT_FALSE(lv_6.isLocked());

    EXPECT_NOTHROW(lv_1.getValue());
    EXPECT_NOTHROW(lv_2.getValue());
    EXPECT_NOTHROW(lv_3.getValue());
    EXPECT_NOTHROW(lv_4.getValue());
    EXPECT_NOTHROW(lv_6.getValue());

    EXPECT_EQUAL(lv_1.getValue(), data_1);
    EXPECT_EQUAL(lv_2.getValue(), data_2);
    EXPECT_EQUAL(lv_3.getValue(), data_3);
    EXPECT_EQUAL(lv_4.getValue(), data_4);
    EXPECT_EQUAL(lv_6.getValue(), data_6);

    EXPECT_EQUAL(lv_1, data_1);
    EXPECT_EQUAL(lv_2, data_2);
    EXPECT_EQUAL(lv_3, data_3);
    EXPECT_EQUAL(lv_4, data_4);
    EXPECT_EQUAL(lv_6, data_6);

    data_1 = 4;
    data_2 = 8;
    data_3 = 16;
    data_6 = 256;
    EXPECT_NOTHROW(lv_1 = data_1);
    EXPECT_NOTHROW(lv_2 = data_2);
    EXPECT_NOTHROW(lv_3 = data_3);
    EXPECT_THROW(lv_4 = data_4);
    EXPECT_NOTHROW(lv_6 = data_6);

    EXPECT_EQUAL(lv_1.getValue(), data_1);
    EXPECT_EQUAL(lv_2.getValue(), data_2);
    EXPECT_EQUAL(lv_3.getValue(), data_3);
    EXPECT_EQUAL(lv_4.getValue(), data_4);
    EXPECT_EQUAL(lv_6.getValue(), data_6);

    EXPECT_NOTHROW(lv_3.lock());
    EXPECT_NOTHROW(lv_6.lock());
    EXPECT_TRUE(lv_6.isLocked());
    EXPECT_THROW(lv_6 = data_6);

    EXPECT_FALSE(lv_1.isLocked());
    EXPECT_TRUE(lv_3.isLocked());
    EXPECT_TRUE(lv_3.isLocked());
    EXPECT_TRUE(lv_4.isLocked());

    EXPECT_THROW(lv_3 = data_3);
    EXPECT_NOTHROW(lv_3.lock());

    data_2 = 12;
    EXPECT_NOTHROW(lv_2.setAndLock(data_2));
    EXPECT_EQUAL(lv_2, data_2);
    EXPECT_EQUAL(lv_2.getValue(), data_2);
    EXPECT_TRUE(lv_2.isLocked());
    EXPECT_THROW(lv_2 = data_2);
    EXPECT_THROW(lv_2.setAndLock(data_2));

    data_1 = 512;
    EXPECT_NOTHROW(lv_1 = data_1);
    EXPECT_FALSE(lv_1.isLocked());
    EXPECT_TRUE(lv_1 == data_1);

    data_1 = 256;
    EXPECT_NOTHROW(lv_1.setAndLock(data_1));
    EXPECT_TRUE(lv_1 == data_1);
    EXPECT_TRUE(lv_1.isLocked());
    EXPECT_THROW(lv_1.setAndLock(data_1));
    EXPECT_THROW(lv_1 = data_1);
    EXPECT_EQUAL(lv_1, data_1);
    EXPECT_EQUAL(lv_1.getValue(), data_1);
}

void testLockedValidValue(){
    PRINT_ENTER_TEST;
    namespace ru = sparta::utils;

    ru::ValidValue<uint16_t> data_1;
    ru::ValidValue<uint32_t> data_2 = 32;
    ru::ValidValue<uint64_t> data_3 = 64;
    ru::ValidValue<uint64_t> data_4 = 128;
    ru::ValidValue<double>   data_5 = 3.14;

    ru::LockedValue<ru::ValidValue<uint16_t>> lv_1;
    ru::LockedValue<ru::ValidValue<uint32_t>> lv_2(data_2);
    ru::LockedValue<ru::ValidValue<uint64_t>> lv_3(data_3, false);
    ru::LockedValue<ru::ValidValue<uint64_t>> lv_4(data_4, true);
    ru::LockedValue<ru::ValidValue<double>>   lv_5;

    EXPECT_NOTHROW(lv_5.lock());
    EXPECT_NOTHROW(lv_5.lock());
    EXPECT_TRUE(lv_5.isLocked());
    EXPECT_NOTHROW(lv_5.getValue());
    EXPECT_THROW(lv_5 = data_5);

    EXPECT_FALSE(lv_1.isLocked());
    EXPECT_FALSE(lv_2.isLocked());
    EXPECT_FALSE(lv_3.isLocked());
    EXPECT_TRUE(lv_4.isLocked());

    EXPECT_NOTHROW(lv_1.getValue());
    EXPECT_THROW(lv_1.getValue().getValue());
    EXPECT_NOTHROW(lv_2.getValue());
    EXPECT_NOTHROW(lv_3.getValue());
    EXPECT_NOTHROW(lv_4.getValue());

    EXPECT_THROW(lockedValueCompare(lv_1, data_1));
    EXPECT_EQUAL(lv_2.getValue(), data_2);
    EXPECT_EQUAL(lv_3.getValue(), data_3);
    EXPECT_EQUAL(lv_4.getValue(), data_4);
    EXPECT_THROW(lockedValueCompare(lv_1, data_1));
    EXPECT_EQUAL(lv_2, data_2);
    EXPECT_EQUAL(lv_3, data_3);
    EXPECT_EQUAL(lv_4, data_4);

    data_1 = 4;
    data_2 = 8;
    data_3 = 16;
    EXPECT_NOTHROW(lv_1 = data_1);
    EXPECT_NOTHROW(lv_2 = data_2);
    EXPECT_NOTHROW(lv_3 = data_3);
    EXPECT_THROW(lv_4 = data_4);

    EXPECT_EQUAL(lv_1.getValue(), data_1);
    EXPECT_EQUAL(lv_2.getValue(), data_2);
    EXPECT_EQUAL(lv_3.getValue(), data_3);
    EXPECT_EQUAL(lv_4.getValue(), data_4);

    EXPECT_NOTHROW(lv_3.lock());

    EXPECT_FALSE(lv_1.isLocked());
    EXPECT_TRUE(lv_3.isLocked());
    EXPECT_TRUE(lv_3.isLocked());
    EXPECT_TRUE(lv_4.isLocked());
    EXPECT_THROW(lv_3 = data_3);

    EXPECT_NOTHROW(lv_3.lock());

    data_2 = 12;
    EXPECT_NOTHROW(lv_2.setAndLock(data_2));
    EXPECT_EQUAL(lv_2, data_2);
    EXPECT_EQUAL(lv_2.getValue(), data_2);
    EXPECT_TRUE(lv_2.isLocked());
    EXPECT_THROW(lv_2 = data_2);
    EXPECT_THROW(lv_2.setAndLock(data_2));

    data_1 = 512;
    EXPECT_NOTHROW(lv_1 = data_1);
    EXPECT_FALSE(lv_1.isLocked());
    EXPECT_TRUE(lv_1 == data_1);

    data_1 = 256;
    EXPECT_NOTHROW(lv_1.setAndLock(data_1));
    EXPECT_TRUE(lv_1 == data_1);
    EXPECT_TRUE(lv_1.isLocked());
    EXPECT_THROW(lv_1.setAndLock(data_1));
    EXPECT_THROW(lv_1 = data_1);
    EXPECT_EQUAL(lv_1, data_1);
    EXPECT_EQUAL(lv_1.getValue(), data_1);
}

int main(){
    testLockedValue();
    testLockedValidValue();
    REPORT_ERROR;
    return ERROR_CODE;
}
