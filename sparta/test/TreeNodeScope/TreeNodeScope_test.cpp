
#include <inttypes.h>
#include <iostream>
#include <set>
#include <iterator>

#include "sparta/sparta.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/Resource.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/simulation/TreeNodePrivateAttorney.hpp"
#include "sparta/log/NotificationSource.hpp"
TEST_INIT;

class TestFixture
{
public:
    TestFixture()
    : top("top")
    , node1(&top, "node1", "node1")
    , node2(&node1, "node2", "node2")
    {
    }

    ~TestFixture()
    {
        top.enterTeardown();
    }

    sparta::RootTreeNode top;

    sparta::TreeNode node1;
    sparta::TreeNode node2;
};

static void testCanGetTopWhenNoScopeIsDefined()
{
    TestFixture tf;

    auto scope_root = tf.top.getScopeRoot();
    EXPECT_EQUAL(&tf.top, scope_root);

    scope_root = tf.node1.getScopeRoot();
    EXPECT_EQUAL(&tf.top, scope_root);

    scope_root = tf.node2.getScopeRoot();
    EXPECT_EQUAL(&tf.top, scope_root);
}

static void testGetScopeRootReturnsExplicitlyDefinedScopeRoot()
{
    TestFixture tf;

    tf.node1.setScopeRoot();

    auto scope_root = tf.node1.getScopeRoot();
    EXPECT_EQUAL(scope_root, &tf.node1);

    scope_root = tf.node2.getScopeRoot();
    EXPECT_EQUAL(scope_root, &tf.node1);
}

static void testGetRootReturnsTopDespiteScope()
{
    TestFixture tf;

    tf.node1.setScopeRoot();

    EXPECT_EQUAL(&tf.top, tf.top.getRoot());
    EXPECT_EQUAL(&tf.top, tf.node1.getRoot());
    EXPECT_EQUAL(&tf.top, tf.node2.getRoot());
}

int main()
{
    testCanGetTopWhenNoScopeIsDefined();
    testGetScopeRootReturnsExplicitlyDefinedScopeRoot();
    testGetRootReturnsTopDespiteScope();

    REPORT_ERROR;
    return ERROR_CODE;
}
