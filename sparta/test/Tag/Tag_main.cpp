#include <inttypes.h>
#include <iostream>

#include "sparta/sparta.hpp"
#include "sparta/utils/Tag.hpp"
#include "sparta/utils/SpartaTester.hpp"

TEST_INIT

using namespace sparta;
using namespace std;

void test_it()
{
    Tag t;
    Tag t_cpy(t);
    Tag t1;
    Tag t1_cpy(t1);
    Tag u(&t);
    Tag u_cpy(u);
    Tag v(&t);
    Tag v_cpy(v);
    Tag w(&t1);
    Tag w_cpy(w);

    // EXPECT_EQUAL(t, "1");
    // EXPECT_EQUAL(t1, "2");
    // EXPECT_EQUAL(u, "1.1");
    // EXPECT_EQUAL(v, "1.2");
    // EXPECT_EQUAL(w, "2.1");

    EXPECT_EQUAL(t, t_cpy);
    EXPECT_EQUAL(t1, t1_cpy);
    EXPECT_EQUAL(u, u_cpy);
    EXPECT_EQUAL(v, v_cpy);
    EXPECT_EQUAL(w, w_cpy);
}

int main()
{
    for(uint32_t i = 0; i < 1000000; ++i) {
         test_it();
         sparta::Tag::resetGlobalSeqNum();
    }

    Tag t;
    Tag t1;
    Tag u(&t);
    Tag v(&t);
    Tag w(&u);

    cout << "T: " << t << endl;
    cout << "T1: " << t1 << endl;
    cout << "U: " << u << endl;
    cout << "V: " << v << endl;
    cout << "W: " << w << endl;

    EXPECT_EQUAL(t, "1");
    EXPECT_EQUAL(t1, "2");
    EXPECT_EQUAL(u, "1.1");
    EXPECT_EQUAL(v, "1.2");
    EXPECT_EQUAL(w, "1.1.1");

    REPORT_ERROR;

    return ERROR_CODE;
}
