
#include <limits>
#include <iostream>
#include <iomanip>
#include <cstdint>

#include "sparta/utils/BoundedValue.hpp"
#include "sparta/utils/SpartaTester.hpp"

TEST_INIT;

int main()
{
    //! Full test coverage of BoundedValue<int> type.
    {
        //! Parameterized Constructors for BoundedValue<int> from all convertible types.
        {
            //! The 4 variables are BV type, inital value, upper bound and lower bound.
            //! Case when all 4 variables are integral and of the same sign(signed).
            int data = 214789;
            auto should_throw = [&data](){sparta::utils::BoundedValue<int> val_int(data, 20, 400000); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            //! The 4 variables are BV type, inital value, upper bound and lower bound.
            //! Case when all 4 variables are integral and of the same sign(signed).
            int data = 15;
            int16_t lb = 10;
            int32_t ub = 20;
            auto should_throw = [&](){sparta::utils::BoundedValue<int> val_int(data, lb, ub); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            //! Case when all 4 variables are integral and of the same sign(unsigned).
            //! The 4 variables are BV type, inital value, upper bound and lower bound.
            uint32_t data = 214789;
            auto should_throw = [&data](){sparta::utils::BoundedValue<uint64_t> val_int(data, 20ull, 400000ull); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            //! Case when all 4 variables are integral and of the same sign(unsigned).
            //! The 4 variables are BV type, inital value, upper bound and lower bound.
            uint16_t data = 15;
            uint16_t lb = 10;
            uint32_t ub = 20;
            auto should_throw = [&](){sparta::utils::BoundedValue<uint32_t> val_int(data, lb, ub); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            //! Case when BV type, inital value and one bound are unsigned but another bound is signed.
            uint32_t data = 214789;
            auto should_throw = [&data](){sparta::utils::BoundedValue<uint64_t> val_int(data, 20, 400000ull); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            //! Case when BV type, inital value and one bound are unsigned but another bound is signed.
            uint16_t data = 15;
            int16_t lb = 10;
            uint32_t ub = 20;
            auto should_throw = [&](){sparta::utils::BoundedValue<uint32_t> val_int(data, lb, ub); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            //! Case when BV type and one of the bounds is unsigned but initial value and one of the bounds are signed.
            int32_t data = 214789;
            auto should_throw = [&data](){sparta::utils::BoundedValue<uint64_t> val_int(data, 20, 214788ull); (void)val_int;};
            EXPECT_THROW(should_throw());
        }
        {
            //! Case when BV type and one of the bounds is unsigned but initial value and one of the bounds are signed.
            int16_t data = 15;
            uint16_t lb = 10;
            int32_t ub = 20;
            auto should_throw = [&](){sparta::utils::BoundedValue<uint32_t> val_int(data, lb, ub); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            //! Case when BV type, lower and upper bounds are signed but initial value is unsigned.
            uint32_t data = 214;
            auto should_throw = [&data](){sparta::utils::BoundedValue<int64_t> val_int(data, 20, 400000); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            //! Case when BV type, lower and upper bounds are signed but initial value is unsigned.
            uint16_t data = 15;
            int16_t lb = 10;
            int32_t ub = 20;
            auto should_throw = [&](){sparta::utils::BoundedValue<int32_t> val_int(data, lb, ub); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            //! Case when BV type and initial value is signed but one of the bounds is unsigned.
            int32_t data = 214;
            auto should_throw = [&data](){sparta::utils::BoundedValue<int64_t> val_int(data, 20ull, 400000); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            //! Case when BV type and initial value is signed but one of the bounds is unsigned.
            int16_t data = 15;
            uint16_t lb = 10;
            int32_t ub = 20;
            auto should_throw = [&](){sparta::utils::BoundedValue<int32_t> val_int(data, lb, ub); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            //! Case when BV type is signed but inital value and one of the bounds is unsigned.
            uint32_t data = 214;
            auto should_throw = [&data](){sparta::utils::BoundedValue<int64_t> val_int(data, 20ull, 400000); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            //! Case when BV type is signed but inital value and one of the bounds is unsigned.
            uint16_t data = 15;
            uint16_t lb = 10;
            int32_t ub = 20;
            auto should_throw = [&](){sparta::utils::BoundedValue<int32_t> val_int(data, lb, ub); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        //! Parameterized construction from int data.
        {
            int data = 214789;
            auto should_throw = [&data](){sparta::utils::BoundedValue<int> val_int(data); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        //! Parameterized construction from char data.
        {
            char data = '2';
            auto should_throw = [&data](){sparta::utils::BoundedValue<int> val_int(data); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        //! Parameterized construction from bool data.
        {
            bool data = true;
            auto should_throw = [&data](){sparta::utils::BoundedValue<int> val_int(data); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        //! Parameterized construction from short data.
        {
            short data = 32761;
            auto should_throw = [&data](){sparta::utils::BoundedValue<int> val_int(data); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        //! Parameterized construction from long data.
        {
            long data = 2199037;
            auto should_throw = [&data](){sparta::utils::BoundedValue<int> val_int(data); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        //! Parameterized construction from long long data.
        {
            long long data = -9374854321ull;
            auto should_throw = [&data](){sparta::utils::BoundedValue<int> val_int(data); (void)val_int;};
            EXPECT_THROW(should_throw());
        }
        //! Parameterized construction from int8_t data.
        {
            int8_t data = '{';
            auto should_throw = [&data](){sparta::utils::BoundedValue<int> val_int(data); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        //! Parameterized construction from int16_t data.
        {
            int16_t data = -32001;
            auto should_throw = [&data](){sparta::utils::BoundedValue<int> val_int(data); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        //! Parameterized construction from int32_t data.
        {
            int32_t data = -2147483648;
            auto should_throw = [&data](){sparta::utils::BoundedValue<int> val_int(data); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        //! Parameterized construction from int64_t data.
        {
            int64_t data = -9983475801ull;
            auto should_throw = [&data](){sparta::utils::BoundedValue<int> val_int(data); (void)val_int;};
            EXPECT_THROW(should_throw());
        }
        //! Parameterized construction from intmax_t data.
        {
            intmax_t data = 9333277;
            auto should_throw = [&data](){sparta::utils::BoundedValue<int> val_int(data, 0, 94444444); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        //! Parameterized construction from unsigned int data.
        {
            unsigned int data = 217734789;
            auto should_throw = [&data](){sparta::utils::BoundedValue<int> val_int(data, 14, 217734790); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        //! Parameterized construction from unsigned char data.
        {
            unsigned char data = '!';
            auto should_throw = [&data](){sparta::utils::BoundedValue<int> val_int(data, 33, 34); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        //! Parameterized construction from unsigned short data.
        {
            unsigned short data = 65535;
            auto should_throw = [&data](){sparta::utils::BoundedValue<int> val_int(data, -1, 70000); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        //! Parameterized construction from unsigned long data.
        {
            unsigned long data = 2147890;
            auto should_throw = [&data](){sparta::utils::BoundedValue<int> val_int(data, -3232, 2222222); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        //! Parameterized construction from unsigned long long data.
        {
            unsigned long long data = 18446744073709551614ul;
            auto should_throw = [&data](){sparta::utils::BoundedValue<int> val_int(data, 0, 10); (void)val_int;};
            EXPECT_THROW(should_throw());
        }
        //! Parameterized construction from uint8_t data.
        {
            uint8_t data = 'v';
            auto should_throw = [&data](){sparta::utils::BoundedValue<int> val_int(data, -118, 117); (void)val_int;};
            EXPECT_THROW(should_throw());
        }
        //! Parameterized construction from uint16_t data.
        {
            uint16_t data = 100;
            auto should_throw = [&data](){sparta::utils::BoundedValue<int> val_int(data, -99, 99); (void)val_int;};
            EXPECT_THROW(should_throw());
        }
        //! Parameterized construction from uint32_t data.
        {
            uint32_t data = 3147483640ul;
            auto should_throw = [&data](){sparta::utils::BoundedValue<int> val_int(data, 10, 11); (void)val_int;};
            EXPECT_THROW(should_throw());
        }
        //! Parameterized construction from uint64_t data.
        {
            uint64_t data = 12444147483640ull;
            auto should_throw = [&data](){sparta::utils::BoundedValue<int> val_int(data, 321, 321321312); (void)val_int;};
            EXPECT_THROW(should_throw());
        }
        //! Parameterized construction from uintmax_t data.
        {
            uintmax_t data = 400000000000ull;
            auto should_throw = [&data](){sparta::utils::BoundedValue<int> val_int(data, -32132, 89798798); (void)val_int;};
            EXPECT_THROW(should_throw());
        }

        //! Copy Constructors for BoundedValue<int> from all convertible types.
        {
            int data = 214789;
            sparta::utils::BoundedValue<int> val_rhs(data);
            auto should_throw = [&val_rhs](){sparta::utils::BoundedValue<int> val_int(val_rhs); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            char data = '2';
            sparta::utils::BoundedValue<char> val_rhs(data);
            auto should_throw = [&val_rhs](){sparta::utils::BoundedValue<int> val_int(val_rhs); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            bool data = true;
            sparta::utils::BoundedValue<bool> val_rhs(data);
            auto should_throw = [&val_rhs](){sparta::utils::BoundedValue<int> val_int(val_rhs); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            short data = 32761;
            sparta::utils::BoundedValue<short> val_rhs(data);
            auto should_throw = [&val_rhs](){sparta::utils::BoundedValue<int> val_int(val_rhs); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            long data = 2199037;
            sparta::utils::BoundedValue<long> val_rhs(data, 0, 2298001);
            auto should_throw = [&val_rhs](){sparta::utils::BoundedValue<int> val_int(val_rhs); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            long long data = -9374854321ull;
            sparta::utils::BoundedValue<long long> val_rhs(data);
            auto should_throw = [&val_rhs](){sparta::utils::BoundedValue<int> val_int(val_rhs); (void)val_int;};
            EXPECT_THROW(should_throw());
        }
        {
            int8_t data = '{';
            sparta::utils::BoundedValue<int8_t> val_rhs(data);
            auto should_throw = [&val_rhs](){sparta::utils::BoundedValue<int> val_int(val_rhs); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            int16_t data = -32001;
            sparta::utils::BoundedValue<int16_t> val_rhs(data);
            auto should_throw = [&val_rhs](){sparta::utils::BoundedValue<int> val_int(val_rhs); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            int32_t data = -2147483648;
            sparta::utils::BoundedValue<int32_t> val_rhs(data);
            auto should_throw = [&val_rhs](){sparta::utils::BoundedValue<int> val_int(val_rhs); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            int64_t data = -9983475801ull;
            sparta::utils::BoundedValue<int64_t> val_rhs(data);
            auto should_throw = [&val_rhs](){sparta::utils::BoundedValue<int> val_int(val_rhs); (void)val_int;};
            EXPECT_THROW(should_throw());
        }
        {
            intmax_t data = 9333277;
            sparta::utils::BoundedValue<intmax_t> val_rhs(data);
            auto should_throw = [&val_rhs](){sparta::utils::BoundedValue<int> val_int(val_rhs); (void)val_int;};
            EXPECT_THROW(should_throw());
        }
        {
            unsigned int data = 217734789;
            sparta::utils::BoundedValue<unsigned int> val_rhs(data);
            auto should_throw = [&val_rhs](){sparta::utils::BoundedValue<int> val_int(val_rhs); (void)val_int;};
            EXPECT_THROW(should_throw());
        }
        {
            unsigned char data = '!';
            sparta::utils::BoundedValue<unsigned char> val_rhs(data);
            auto should_throw = [&val_rhs](){sparta::utils::BoundedValue<int> val_int(val_rhs); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            unsigned short data = 65535;
            sparta::utils::BoundedValue<unsigned short> val_rhs(data);
            auto should_throw = [&val_rhs](){sparta::utils::BoundedValue<int> val_int(val_rhs); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            unsigned long data = 2147890;
            sparta::utils::BoundedValue<unsigned long> val_rhs(data, 10, 2310090);
            auto should_throw = [&val_rhs](){sparta::utils::BoundedValue<int> val_int(val_rhs); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            unsigned long long data = 18446744073709551614ul;
            sparta::utils::BoundedValue<unsigned long long> val_rhs(data);
            auto should_throw = [&val_rhs](){sparta::utils::BoundedValue<int> val_int(val_rhs); (void)val_int;};
            EXPECT_THROW(should_throw());
        }
        {
            uint8_t data = 'v';
            sparta::utils::BoundedValue<uint8_t> val_rhs(data);
            auto should_throw = [&val_rhs](){sparta::utils::BoundedValue<int> val_int(val_rhs); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            uint16_t data = 100;
            sparta::utils::BoundedValue<uint16_t> val_rhs(data);
            auto should_throw = [&val_rhs](){sparta::utils::BoundedValue<int> val_int(val_rhs); (void)val_int;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            uint32_t data = 3147483640ul;
            sparta::utils::BoundedValue<uint32_t> val_rhs(data);
            auto should_throw = [&val_rhs](){sparta::utils::BoundedValue<int> val_int(val_rhs); (void)val_int;};
            EXPECT_THROW(should_throw());
        }
        {
            uint64_t data = 12444147483640ull;
            sparta::utils::BoundedValue<uint64_t> val_rhs(data);
            auto should_throw = [&val_rhs](){sparta::utils::BoundedValue<int> val_int(val_rhs); (void)val_int;};
            EXPECT_THROW(should_throw());
        }
        {
            uintmax_t data = 400000000000ull;
            sparta::utils::BoundedValue<uintmax_t> val_rhs(data);
            auto should_throw = [&val_rhs](){sparta::utils::BoundedValue<int> val_int(val_rhs); (void)val_int;};
            EXPECT_THROW(should_throw());
        }

        //! Copy Assignment operator for BoundedValue<int> from all convertible types.
        {
            int data = 214789;
            sparta::utils::BoundedValue<int> val_rhs(data);
            auto should_throw = [&val_rhs](){
                                    sparta::utils::BoundedValue<int> val_int(5);
                                    val_int = val_rhs;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            char data = '2';
            sparta::utils::BoundedValue<char> val_rhs(data);
            auto should_throw = [&val_rhs](){
                                    sparta::utils::BoundedValue<int> val_int(10);
                                    val_int = val_rhs;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            bool data = true;
            sparta::utils::BoundedValue<bool> val_rhs(data);
            auto should_throw = [&val_rhs](){
                                    sparta::utils::BoundedValue<int> val_int(23);
                                    val_int = val_rhs;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            short data = 32761;
            sparta::utils::BoundedValue<short> val_rhs(data);
            auto should_throw = [&val_rhs](){
                                    sparta::utils::BoundedValue<int> val_int(34);
                                    val_int = val_rhs;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            long data = 2199037;
            sparta::utils::BoundedValue<long> val_rhs(data);
            auto should_throw = [&val_rhs](){
                                    sparta::utils::BoundedValue<int> val_int(90);
                                    val_int = val_rhs;};
            EXPECT_THROW(should_throw());
        }
        {
            long long data = -9374854321ull;
            sparta::utils::BoundedValue<long long> val_rhs(data);
            auto should_throw = [&val_rhs](){
                                    sparta::utils::BoundedValue<int> val_int(43);
                                    val_int = val_rhs;};
            EXPECT_THROW(should_throw());
        }
        {
            int8_t data = '{';
            sparta::utils::BoundedValue<int8_t> val_rhs(data);
            auto should_throw = [&val_rhs](){
                                    sparta::utils::BoundedValue<int> val_int(989);
                                    val_int = val_rhs;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            int16_t data = -32001;
            sparta::utils::BoundedValue<int16_t> val_rhs(data);
            auto should_throw = [&val_rhs](){
                                    sparta::utils::BoundedValue<int> val_int(11276);
                                    val_int = val_rhs;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            int32_t data = -2147483648;
            sparta::utils::BoundedValue<int32_t> val_rhs(data);
            auto should_throw = [&val_rhs](){
                                    sparta::utils::BoundedValue<int> val_int(32432);
                                    val_int = val_rhs;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            int64_t data = -9983475801ull;
            sparta::utils::BoundedValue<int64_t> val_rhs(data);
            auto should_throw = [&val_rhs](){
                                    sparta::utils::BoundedValue<int> val_int(324324);
                                    val_int = val_rhs;};
            EXPECT_THROW(should_throw());
        }
        {
            intmax_t data = 933327746376432ull;
            sparta::utils::BoundedValue<intmax_t> val_rhs(data);
            auto should_throw = [&val_rhs](){
                                    sparta::utils::BoundedValue<int> val_int(0);
                                    val_int = val_rhs;};
            EXPECT_THROW(should_throw());
        }
        {
            unsigned int data = 2177347899ull;
            sparta::utils::BoundedValue<unsigned int> val_rhs(data);
            auto should_throw = [&val_rhs](){
                                    sparta::utils::BoundedValue<int> val_int(-21);
                                    val_int = val_rhs;};
            EXPECT_THROW(should_throw());
        }
        {
            unsigned char data = '!';
            sparta::utils::BoundedValue<unsigned char> val_rhs(data);
            auto should_throw = [&val_rhs](){
                                    sparta::utils::BoundedValue<int> val_int(-34);
                                    val_int = val_rhs;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            unsigned short data = 65535;
            sparta::utils::BoundedValue<unsigned short> val_rhs(data);
            auto should_throw = [&val_rhs](){
                                    sparta::utils::BoundedValue<int> val_int(-334);
                                    val_int = val_rhs;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            unsigned long data = 2147890;
            sparta::utils::BoundedValue<unsigned long> val_rhs(data, 10, 2310090);
            auto should_throw = [&val_rhs](){
                                    sparta::utils::BoundedValue<int> val_int(-909);
                                    val_int = val_rhs;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            unsigned long long data = 18446744073709551614ul;
            sparta::utils::BoundedValue<unsigned long long> val_rhs(data);
            auto should_throw = [&val_rhs](){
                                    sparta::utils::BoundedValue<int> val_int(-112);
                                    val_int = val_rhs;};
            EXPECT_THROW(should_throw());
        }
        {
            uint8_t data = 'v';
            sparta::utils::BoundedValue<uint8_t> val_rhs(data);
            auto should_throw = [&val_rhs](){
                                    sparta::utils::BoundedValue<int> val_int(-5544);
                                    val_int = val_rhs;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            uint16_t data = 100;
            sparta::utils::BoundedValue<uint16_t> val_rhs(data);
            auto should_throw = [&val_rhs](){
                                    sparta::utils::BoundedValue<int> val_int(-3298);
                                    val_int = val_rhs;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            uint32_t data = 3147483640ul;
            sparta::utils::BoundedValue<uint32_t> val_rhs(data);
            auto should_throw = [&val_rhs](){
                                    sparta::utils::BoundedValue<int> val_int(-4343);
                                    val_int = val_rhs;};
            EXPECT_THROW(should_throw());
        }
        {
            uint64_t data = 12444147483640ull;
            sparta::utils::BoundedValue<uint64_t> val_rhs(data);
            auto should_throw = [&val_rhs](){
                                    sparta::utils::BoundedValue<int> val_int(-9999);
                                    val_int = val_rhs;};
            EXPECT_THROW(should_throw());
        }
        {
            uintmax_t data = 400000000000ull;
            sparta::utils::BoundedValue<uintmax_t> val_rhs(data);
            auto should_throw = [&val_rhs](){
                                    sparta::utils::BoundedValue<int> val_int(-2321321);
                                    val_int = val_rhs;};
            EXPECT_THROW(should_throw());
        }

        //! Assignment operator for BoundedValue<int> from all convertible types.
        {
            auto should_throw = [](){
                                    int data = 214789;
                                    sparta::utils::BoundedValue<int> val_int(5);
                                    val_int = data;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            auto should_throw = [](){
                                    char data = '2';
                                    sparta::utils::BoundedValue<int> val_int(10);
                                    val_int = data;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            auto should_throw = [](){
                                    bool data = true;
                                    sparta::utils::BoundedValue<int> val_int(23);
                                    val_int = data;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            auto should_throw = [](){
                                    short data = 32761;
                                    sparta::utils::BoundedValue<int> val_int(34);
                                    val_int = data;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            auto should_throw = [](){
                                    long data = 2199037;
                                    sparta::utils::BoundedValue<int> val_int(90);
                                    val_int = data;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            auto should_throw = [](){
                                    long long data = -9374854321ull;
                                    sparta::utils::BoundedValue<int> val_int(43);
                                    val_int = data;};
            EXPECT_THROW(should_throw());
        }
        {
            auto should_throw = [](){
                                    int8_t data = '{';
                                    sparta::utils::BoundedValue<int> val_int(989);
                                    val_int = data;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            auto should_throw = [](){
                                    int16_t data = -32001;
                                    sparta::utils::BoundedValue<int> val_int(11276);
                                    val_int = data;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            auto should_throw = [](){
                                    int32_t data = -2147483648;
                                    sparta::utils::BoundedValue<int> val_int(32432);
                                    val_int = data;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            auto should_throw = [](){
                                    int64_t data = -9983475801ull;
                                    sparta::utils::BoundedValue<int> val_int(324324);
                                    val_int = data;};
            EXPECT_THROW(should_throw());
        }
        {
            auto should_throw = [](){
                                    intmax_t data = 933327746376432ull;
                                    sparta::utils::BoundedValue<int> val_int(0);
                                    val_int = data;};
            EXPECT_THROW(should_throw());
        }
        {
            auto should_throw = [](){
                                    unsigned int data = 2177347899ull;
                                    sparta::utils::BoundedValue<int> val_int(-21);
                                    val_int = data;};
            EXPECT_THROW(should_throw());
        }
        {
            auto should_throw = [](){
                                    unsigned char data = '!';
                                    sparta::utils::BoundedValue<int> val_int(-34);
                                    val_int = data;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            auto should_throw = [](){
                                    unsigned short data = 65535;
                                    sparta::utils::BoundedValue<int> val_int(-334);
                                    val_int = data;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            auto should_throw = [](){
                                    unsigned long data = 2147890;
                                    sparta::utils::BoundedValue<int> val_int(-909);
                                    val_int = data;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            auto should_throw = [](){
                                    unsigned long long data = 18446744073709551614ul;
                                    sparta::utils::BoundedValue<int> val_int(-112);
                                    val_int = data;};
            EXPECT_THROW(should_throw());
        }
        {
            auto should_throw = [](){
                                    uint8_t data = 'v';
                                    sparta::utils::BoundedValue<int> val_int(-5544);
                                    val_int = data;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            auto should_throw = [](){
                                    uint16_t data = 100;
                                    sparta::utils::BoundedValue<int> val_int(-3298);
                                    val_int = data;};
            EXPECT_NOTHROW(should_throw());
        }
        {
            auto should_throw = [](){
                                    uint32_t data = 3147483640ul;
                                    sparta::utils::BoundedValue<int> val_int(-4343);
                                    val_int = data;};
            EXPECT_THROW(should_throw());
        }
        {
            auto should_throw = [](){
                                    uint64_t data = 12444147483640ull;
                                    sparta::utils::BoundedValue<int> val_int(-9999);
                                    val_int = data;};
            EXPECT_THROW(should_throw());
        }
        {
            auto should_throw = [](){
                                    uintmax_t data = 400000000000ull;
                                    sparta::utils::BoundedValue<int> val_int(-2321321);
                                    val_int = data;};
            EXPECT_THROW(should_throw());
        }

        //! Tests for various logical, mathematical and shorthand methods.
        {
            //! Check != operator
            {
                sparta::utils::BoundedValue<int> val_x(9, -10, 10);
                sparta::utils::BoundedValue<int> val_y(4, -5, 5);
                EXPECT_TRUE(val_x != 8);
                EXPECT_TRUE(val_y != 5);
                sparta::utils::BoundedValue<int> val_z(-99, -100, 10);
                sparta::utils::BoundedValue<int> val_p(-1, -500, 10);
                EXPECT_TRUE(val_p != val_z);
            }

            //! Check comparison operators.
            {
                sparta::utils::BoundedValue<int> val_x(9, -10, 10);
                sparta::utils::BoundedValue<int> val_y(4, -5, 5);
                EXPECT_FALSE(val_x < 8);
                EXPECT_TRUE(val_y < 5);
                EXPECT_FALSE(val_x > 18);
                EXPECT_FALSE(val_y > 5);
                EXPECT_TRUE(val_x >= 9);
                EXPECT_FALSE(val_y <= -2);
                EXPECT_FALSE(val_x == val_y);
                sparta::utils::BoundedValue<int> val_z(-99, -100, 10);
                sparta::utils::BoundedValue<int> val_p(-1, -500, 10);
                EXPECT_TRUE(val_p != val_z);
                EXPECT_FALSE(val_p == val_z);
                EXPECT_FALSE(val_z > val_p);
                EXPECT_FALSE(val_z >= val_p);
                EXPECT_TRUE(val_z < val_p);
                EXPECT_TRUE(val_z <= val_p);
            }

            //! Check Logical operators.
            {
                sparta::utils::BoundedValue<int> val_x(0, -1, 1);
                sparta::utils::BoundedValue<int> val_y(1, -1, 1);
                EXPECT_EQUAL(!val_x, 1);
                EXPECT_EQUAL(val_x, 0);
                EXPECT_TRUE(val_x or 1);
                EXPECT_FALSE(val_x and 1);
                EXPECT_EQUAL(val_x, !val_y);
                EXPECT_TRUE(val_x or val_y);
                EXPECT_FALSE(val_x and val_y);
            }

            //! Check post/pre increment/decrement operators.
            {
                sparta::utils::BoundedValue<int> val_x(0, -5, 5);
                EXPECT_EQUAL(++val_x, 1);
                EXPECT_EQUAL(val_x++, 1);
                EXPECT_EQUAL(val_x, 2);
                EXPECT_EQUAL(--val_x, 1);
                EXPECT_EQUAL(val_x--, 1);
                EXPECT_EQUAL(val_x, 0);
                sparta::utils::BoundedValue<int> val_y(0, -1, 1);
                EXPECT_EQUAL(++val_y, 1);
                EXPECT_THROW(++val_y);
                sparta::utils::BoundedValue<int> val_z(0, -2, 2);
                EXPECT_EQUAL(++val_z, 1);
                EXPECT_EQUAL(val_z++, 1);
                EXPECT_EQUAL(val_z, 2);
                EXPECT_THROW(++val_y);
                EXPECT_EQUAL(--val_z, 1);
                EXPECT_EQUAL(--val_z, 0);
                EXPECT_EQUAL(val_z--, 0);
                EXPECT_EQUAL(val_z, -1);
                EXPECT_EQUAL(--val_z, -2);
                EXPECT_THROW(--val_z);
            }

            {
                //! Check arithmetic operators.
                sparta::utils::BoundedValue<int> val_x(2, 0, 10);
                sparta::utils::BoundedValue<int> val_y(-4, -10, 10);
                EXPECT_EQUAL(-val_x, -2);
                EXPECT_EQUAL(val_x, 2);
                EXPECT_EQUAL(-val_y, 4);
                EXPECT_EQUAL(val_y, -4);
                {
                    sparta::utils::BoundedValue<int> val_p(val_x + val_y, -2, 0);
                    sparta::utils::BoundedValue<int> val_q(val_x - val_y, 0, 6);
                    EXPECT_EQUAL(val_p, -2);
                    EXPECT_EQUAL(val_q, 6);
                }

                {
                    sparta::utils::BoundedValue<int> val_p(val_x * val_y, -9999, 0);
                    sparta::utils::BoundedValue<int> val_q(val_x / val_y, -23, 32);
                    EXPECT_EQUAL(val_p, -8);
                    EXPECT_EQUAL(val_q, 0);
                }

                //! Check bitshift operators.
                {
                    sparta::utils::BoundedValue<int> val_x(2, -10, 10);
                    sparta::utils::BoundedValue<int> val_y(4, -5, 5);
                    sparta::utils::BoundedValue<int> val_p(val_y % val_x, -2, 2);
                    sparta::utils::BoundedValue<int> val_q(val_x >> 1, 0, 6);
                    EXPECT_EQUAL(val_p, 0);
                    EXPECT_EQUAL(val_q, 1);
                    val_p = val_q << 1;
                    EXPECT_EQUAL(val_q << 1, 2);
                    EXPECT_EQUAL(val_q, 1);
                    EXPECT_EQUAL(val_p, 2);
                }

                //! Check shorthand arithmetic operators.
                {
                    sparta::utils::BoundedValue<uint32_t> val_x(34, 10, 40);
                    EXPECT_THROW(val_x += 7ull);
                    EXPECT_THROW(val_x += 8ull);
                    EXPECT_NOTHROW(val_x += 6ull);
                    sparta::utils::BoundedValue<uint16_t> val_y(7, 3, 17);
                    EXPECT_NOTHROW(val_y += -4);
                    val_y += 4;
                    EXPECT_THROW(val_y += -5);
                    sparta::utils::BoundedValue<uint32_t> val_z(7, 3, 10);
                    EXPECT_NOTHROW(val_z -= 4);
                    val_z += 4;
                    EXPECT_THROW(val_z -= 5);
                    EXPECT_NOTHROW(val_z -= -3);
                    val_z -= 3;
                    EXPECT_THROW(val_z -= -4);
                    EXPECT_THROW(val_z -= -5);
                }
            }
        }
    }

    REPORT_ERROR;
    return ERROR_CODE;
}
