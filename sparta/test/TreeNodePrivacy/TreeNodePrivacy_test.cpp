
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
    , root(&top, "root", "root")
    , pub_tn(&root, "pub", "pub")
    , pri_tn(&root, "pri", "pri")
    , pub_child1_tn(&pub_tn, "child1", "child1")
    , pub_child2_tn(&pub_tn, "child2", "child2")
    , pri_child1_tn(&pri_tn, "child1", "child1")
    , pri_child2_tn(&pri_tn, "child2", "child2")
    , pub_noti(&pub_tn, "pub_noti", "pub_noti", "pub_noti")
    , pri_noti(&pri_tn, "pri_noti", "pri_noti", "pri_noti")
    {
        pri_tn.makeSubtreePrivate();
    }

    ~TestFixture()
    {
        top.enterTeardown();
    }

    sparta::RootTreeNode top;

    /* Sparta adds children to RootTreeNode that we don't want for this test. The
     * not having to deal with those children, this test will only operate on the
     * tree rooted at the root (the node defined below). */
    sparta::TreeNode root;

    sparta::TreeNode pub_tn;
    sparta::TreeNode pri_tn;

    sparta::TreeNode pub_child1_tn;
    sparta::TreeNode pub_child2_tn;

    sparta::TreeNode pri_child1_tn;
    sparta::TreeNode pri_child2_tn;

    struct DummyPayload { };

    sparta::NotificationSource<DummyPayload> pub_noti;
    sparta::NotificationSource<DummyPayload> pri_noti;
};

/* REGISTER_FOR_NOTIFICATION implicitly requires a this pointer and can
 * therefore only be called in method of a class. */
class RegisterForNotification
{
public:
    RegisterForNotification(sparta::TreeNode &node, const char *name)
    {
        node.REGISTER_FOR_NOTIFICATION(
            handleNotification, TestFixture::DummyPayload, name);
    }

    void handleNotification(const TestFixture::DummyPayload &payload)
    {
        (void)payload;
    }
};

#define EXPECT_CAN_GET_CHILD(node, path) do {   \
    auto child = node.getChild(path);           \
    EXPECT_NOTEQUAL(child, nullptr);            \
} while (0)

#define EXPECT_CANNOT_GET_CHILD(node, path) do {    \
    EXPECT_THROW(node.getChild(path));              \
} while (0)

static void findAllTreeNodes(sparta::TreeNode *node, std::set<std::string> &paths)
{
    for (auto child : node->getChildren()) {
        paths.insert(child->getLocation());
        findAllTreeNodes(child, paths);
    }
}

static int countAllTreeNodes(sparta::TreeNode *node)
{
    int count = 0;
    for (auto child : sparta::TreeNodePrivateAttorney::getAllChildren(node)) {
        count += 1 + countAllTreeNodes(child);
    }
    return count;
}

static void testCanOnlyGetChildAtSamePrivacyLevel()
{
    TestFixture tf;

    EXPECT_CAN_GET_CHILD(tf.root, "pub");
    EXPECT_CAN_GET_CHILD(tf.root, "pub.child1");
    EXPECT_CAN_GET_CHILD(tf.root, "pub.child2");

    EXPECT_CANNOT_GET_CHILD(tf.root, "pri");
    EXPECT_CANNOT_GET_CHILD(tf.root, "pri.child1");
    EXPECT_CANNOT_GET_CHILD(tf.root, "pri.child2");

    EXPECT_CAN_GET_CHILD(tf.pri_tn, "child1");
    EXPECT_CAN_GET_CHILD(tf.pri_tn, "child2");
}

static void testCanOnlyGetChildrenAtSamePrivacyLevel()
{
    TestFixture tf;

    std::set<std::string> pub_node_paths{ "top.root.pub",
                                          "top.root.pub.child1",
                                          "top.root.pub.child2",
                                          "top.root.pub.pub_noti" };
    std::set<std::string> pri_node_paths{ "top.root.pri.child1",
                                          "top.root.pri.child2",
                                          "top.root.pri.pri_noti" };

    std::set<std::string> result;
    findAllTreeNodes(&tf.root, result);
    EXPECT_TRUE(result == pub_node_paths);

    result.clear();
    findAllTreeNodes(&tf.pri_tn, result);
    EXPECT_TRUE(result == pri_node_paths);
}

static void testCanOnlyRegisterForNotificationsAtSamePrivacyLevel()
{
    TestFixture tf;

    EXPECT_NOTHROW(RegisterForNotification(tf.root, "pub_noti"));
    EXPECT_EQUAL(tf.pub_noti.getNumObservers(), 1);

    EXPECT_THROW(RegisterForNotification(tf.root, "pri_noti"));
    EXPECT_EQUAL(tf.pri_noti.getNumObservers(), 0);

    EXPECT_NOTHROW(RegisterForNotification(tf.pri_tn, "pri_noti"));
    EXPECT_EQUAL(tf.pri_noti.getNumObservers(), 1);
}

static void testCanAccessAllNodesWithAttorney()
{
    TestFixture tf;
    EXPECT_EQUAL(countAllTreeNodes(&tf.root), 8);
}

int main()
{
    testCanOnlyGetChildAtSamePrivacyLevel();
    testCanOnlyGetChildrenAtSamePrivacyLevel();
    testCanOnlyRegisterForNotificationsAtSamePrivacyLevel();
    testCanAccessAllNodesWithAttorney();

    REPORT_ERROR;
    return ERROR_CODE;
}
