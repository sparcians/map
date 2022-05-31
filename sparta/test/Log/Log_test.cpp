// To make std::this_thread::sleep_for available
#define _GLIBCXX_USE_NANOSLEEP

#include <inttypes.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <limits>

#include "sparta/sparta.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/utils/LogUtils.hpp"
#include "sparta/log/Tap.hpp"
#include "sparta/log/MessageSource.hpp"

/*!
 * \file main.cpp
 * \brief Test for Logging in a simple tree
 */

TEST_INIT;

// Test for expected observation status
#define EXPECT_NUM_OBSERVATION_POINTS(ms, num_pts_expected) {              \
        if(num_pts_expected > 0){                                       \
            EXPECT_TRUE(ms.observed());                                 \
        }else{                                                          \
            EXPECT_FALSE(ms.observed());                                \
        }                                                               \
                                                                        \
        EXPECT_EQUAL(ms.getNumObservationPoints(), num_pts_expected);   \
                                                                        \
        /* Print observation points */                                  \
        const std::vector<sparta::TreeNode*>& obs_pts = ms.getObservationPoints(); \
        std::cout << "Observation points on " << ms << ":" << std::endl; \
        for(sparta::TreeNode* n : obs_pts){                               \
            std::cout << "  @ " << *n << std::endl;                     \
        }                                                               \
    }

// A TreeNode that creates a talkative resource that logs things at construction
// and destruction
class TalkativeTreeNode : public sparta::ResourceTreeNode
{
public:
    // Resource that says a log of things throug a log message source of
    // category "talk"
    class TalkativeResource : public sparta::Resource
    {
        sparta::log::MessageSource logger_;

    public:

        static constexpr char name[] = "TalkativeResource";

        class ParameterSet : public sparta::ParameterSet
        {
        public:
            ParameterSet(TreeNode* n) :
                sparta::ParameterSet(n)
            { }
        };

        TalkativeResource(TreeNode* n, const ParameterSet* ps) :
            sparta::Resource(n),
            logger_(n, "talk", "Talkative Node Log Messages")
        {
            (void) ps;
            logger_ << "Hi, I'm constructing";
        }

        ~TalkativeResource() {
            // This is actually quite a tricky part of the test.
            //
            logger_ << "Help, I'm destructing";
        }

        void onStartingTeardown_() override {
            logger_ << "Neato, I'm starting teardown";
        }
    };

    static sparta::ResourceFactory<TalkativeResource> talktive_res_fact;

    TalkativeTreeNode(const std::string& name,
                      const std::string& desc) :
        sparta::ResourceTreeNode(name, desc, &talktive_res_fact)
    {

    }
};

constexpr char TalkativeTreeNode::TalkativeResource::name[];
sparta::ResourceFactory<TalkativeTreeNode::TalkativeResource> TalkativeTreeNode::talktive_res_fact;

int main()
{
    // Tap which outlives the tree to capture destructors
    sparta::log::Tap* a_tap_all = nullptr;

    // Scope all of the tests
    {
        // Build Tree
        sparta::RootTreeNode top("top");
        sparta::TreeNode a("a", "A node");
        sparta::TreeNode b("b", "B node");
        sparta::TreeNode c("c", "C node");
        sparta::TreeNode d("d", "D node");
        sparta::TreeNode e("e", "E node");
        sparta::TreeNode f("f", "F node");

        // It is important that g is destructed last to test destruction-time
        // log messages where the above TreeNodes have been destructed
        TalkativeTreeNode g("g", "G node");
        sparta::Scheduler sched;
        sparta::Clock clk("clock", &sched);
        g.setClock(&clk);


        /* Build Tree
         * -------------------------------------------------------------------------------------------------------------------------------
         *
         *                                                   (global virtual) ========= global_tap_warn_1 "warning"    global_warn.log.basic
         *                                                           .                  global_tap_warn_2 "warning"    warn.log.basic
         *                                                           .
         *                                                          top =============== top_tap_warn      "warning"    top_warn.log.basic
         *                                                           |                  top_tap_all       ""           all.log.basic
         *                                                           |
         *                                                           |
         *                                                           a  =============== a_tap_all         ""           all_log.basic
         *                                                          /|\                 a_tap_mycategory  "mycategory" a_out.log.basic
         *                                                         / | \                a_tap_stder       ""           std::cerr
         *                                                      __/  |  \__             a_tap_stder2      ""           std::cerr
         *                                                     /     |     \            a_tap_empty       "nonexist"   empty.log
         *                                                    /      |      \
         *                                                   /       |       \
         * b_tap_mycategory "mycategory" b_out.log.basic == b        d        e ======= e_tap_mycategory  "mycategory" e_out.log.basic
         *                                                  |                 .
         *                                                  |                 .
         * c_tap_mycategory "mycategory" c_out.log.basic == c                 g
         *                                                                     ^
         *                                                                      \
         *                                                                       g added later
         *
         * -------------------------------------------------------------------------------------------------------------------------------
         */

        top.addChild(a);
        a.addChild(b);
        b.addChild(c);
        a.addChild(d);
        a.addChild(e);

        // [optional] Tap on global pseudo-node capturing warnings. Must capture warnings from MessageSources created after this
        // Note: Breaks a lot of checks in this test, but is an example of how to log everything
        //sparta::log::Tap tap_everything(sparta::TreeNode::getVirtualGlobalNode(), "", "everything.log.basic");

        // Tap on global pseudo-node capturing warnings. Must capture warnings from MessageSources created after this
        sparta::log::Tap global_tap_warn_1(sparta::TreeNode::getVirtualGlobalNode(), sparta::log::categories::WARN, "global_warn.log.basic");

        // Try some invalid message source declarations
        EXPECT_THROW(sparta::log::MessageSource dummy(&c, "mycategory", ""));     // No desc
        EXPECT_THROW(sparta::log::MessageSource dummy(&c, "_cat",       "desc")); // Invalid category
        EXPECT_THROW(sparta::log::MessageSource dummy(&c, "8cat",       "desc"));
        EXPECT_THROW(sparta::log::MessageSource dummy(&c, "ok__then",   "desc")); // double-underscore in category

        // Try some invalid taps
        //EXPECT_THROW(sparta::log::Tap dummy(&c, "mycategory", 6)); // what kind of destination is 6!?
        //EXPECT_THROW(sparta::log::Tap dummy(&c, "mycategory", nullptr)); // What goes to NULL
        //EXPECT_THROW(sparta::log::Tap dummy(&c, "mycategory", &top)); // Arbitrary pointer is not a valid destination
        EXPECT_THROW(sparta::log::Tap dummy(&c, "mycategory", "/path/that/does/not/exist")); // This path does not exist (presumably)
        EXPECT_THROW(sparta::log::Tap dummy(&c, "mycategory", "/tmp/directory/")); // Cannot open a directory

        // Create valid message sources
        sparta::log::MessageSource c_src_mycategory(&c, "mycategory", "Messages generated by node c");
        sparta::log::MessageSource d_src_othercategory(&d, "other_category", "Messages generated by node d which will only be observed by a catch-all");
        sparta::log::MessageSource log_utils_test(&c, "hexutils", "Messages generated using logging utils");
        sparta::log::Tap           tap_log_utils(&c, "hexutils", "hex_output.basic");

        EXPECT_FALSE(c_src_mycategory.observed());
        EXPECT_FALSE(d_src_othercategory.observed());

        // Create a Message Source on an unattached node

        EXPECT_FALSE(g.isAttached());
        sparta::log::MessageSource g_src_mycategory(&g, "mycategory", "Messages generated by node g");
        sparta::log::MessageSource g_src_warn(&g, sparta::log::categories::WARN, "Warning messages from g");

        EXPECT_FALSE(g_src_mycategory.observed());
        EXPECT_TRUE(g_src_warn.observed()); // By global_tap_warn1
        g_src_mycategory << "This message SHOULD NEVER BE OBSERVED"; // Nothing observing mycategory above g (not part of tree)

        // Tap on global pseudo-node capturing warnings
        sparta::log::Tap global_tap_warn_2(sparta::TreeNode::getVirtualGlobalNode(), sparta::log::categories::WARN, "warn.log.basic");

        EXPECT_NUM_OBSERVATION_POINTS(g_src_mycategory, 0);
        EXPECT_NUM_OBSERVATION_POINTS(g_src_warn, 1); // by global_tap_warn_2

        g_src_warn << "This warning SHOULD be observed by warn.log.basic and global_warn.log.basic and that is it!"; // Node is not in tree

        EXPECT_FALSE(c_src_mycategory.observed());
        EXPECT_FALSE(d_src_othercategory.observed());
        EXPECT_FALSE(g_src_mycategory.observed());

        std::vector<sparta::TreeNode::NotificationInfo> infos;

        std::cout << "\nLocal possible notifications on a:" << std::endl;
        a.dumpPossibleNotifications(std::cout); // Notifications which can be generated by node 'a'
        infos.clear();
        EXPECT_EQUAL(a.getPossibleNotifications(infos), 0);
        EXPECT_EQUAL(infos.size(), 0);

        std::cout << "\nSubtree possible notifications on a:" << std::endl;
        a.dumpPossibleSubtreeNotifications(std::cout); // Notifications which can be generated by node 'a' or subtree
        infos.clear();
        EXPECT_EQUAL(a.getPossibleSubtreeNotifications(infos), 3); // c_src_mycategory, d_scr_mycategory, log_utils_test
        EXPECT_EQUAL(infos.size(), 3);

        // Tap during building (before configuring)

        sparta::log::Tap c_tap_mycategory(&c, "mycategory", "c_out.log.basic");
        sparta::log::Tap e_tap_mycategory(&e, "mycategory", "e_out.log.basic");

        EXPECT_NUM_OBSERVATION_POINTS(c_src_mycategory, 1); // by c_tap_mycategory
        EXPECT_NUM_OBSERVATION_POINTS(d_src_othercategory, 0);
        EXPECT_NUM_OBSERVATION_POINTS(g_src_mycategory, 0); // NOTE: g not yet added to tree


        a_tap_all = new sparta::log::Tap(&a, sparta::StringManager::getStringManager().EMPTY, "all.log.basic");
        sparta::log::Tap top_tap_warn(&top, sparta::log::categories::WARN, "top_warn.log.basic");
        sparta::log::Tap top_tap_all(&top, "", "all.log.basic");

        EXPECT_NUM_OBSERVATION_POINTS(c_src_mycategory, 3); // by c_tap_mycategory, top_tap_all, a_tap_all
        EXPECT_NUM_OBSERVATION_POINTS(d_src_othercategory, 2); // by top_tap_all, a_tap_all
        EXPECT_NUM_OBSERVATION_POINTS(g_src_mycategory, 0);

        c_src_mycategory << "Test Message 1" << " with some numbers like " << 1 << " and " << 2 << ". Neat";

        // Adding child (g) which already has a message source (g_src_mycategory) to tree.
        // Added a message source (g) after tap on parent (e)
        // Ensure that e_tap_mycategory gets messages from g_src_mycategory
        e.addChild(g);

        EXPECT_NUM_OBSERVATION_POINTS(c_src_mycategory, 3); // by c_tap_mycategory, top_tap_all, a_tap_all
        EXPECT_NUM_OBSERVATION_POINTS(d_src_othercategory, 2); // by top_tap_all, a_tap_all
        EXPECT_NUM_OBSERVATION_POINTS(g_src_mycategory, 3); // by e_tap_mycategory, a_tap_all, top_tap_all,


        // Tap during configuring

        top.enterConfiguring();
        sparta::log::MessageSource c_src_all(&c, "", "not_examined.log"); // Legal to create create during configuring

        sparta::log::Tap b_tap_mycategory(&b, "mycategory", "b_out.log.basic");

        EXPECT_NUM_OBSERVATION_POINTS(c_src_mycategory, 4); // c_tap_mycategory, top_tap_all, a_tap_all, b_tap_mycategory
        EXPECT_NUM_OBSERVATION_POINTS(d_src_othercategory, 2); // by top_tap_all, a_tap_all
        EXPECT_NUM_OBSERVATION_POINTS(g_src_mycategory, 3); // e_tap_mycategory, a_tap_all, top_tap_all


        // Tap after finalization

        top.enterFinalized();
        EXPECT_THROW(sparta::log::MessageSource c_src_mycategory(&c, "", "desc")); // Cannot create during finalization

        sparta::log::Tap a_tap_mycategory(&a, "mycategory", "a_out.log.basic");
        sparta::log::Tap a_tap_stder(&a, sparta::log::categories::NONE, std::cerr); // Direct everything to cerr
        sparta::log::Tap a_tap_stder2(&a, sparta::log::categories::NONE, std::cerr); // Duplicate earlier, just to make sure we have one stderr Destination
        sparta::log::Tap a_tap_empty(&a, "nonexist", "empty.log"); // Write nothing to this guy
        sparta::log::Tap* a_tap_removed = new sparta::log::Tap(&a, sparta::log::categories::NONE, "a_removed.log.basic");

        // Tap using category lists
        sparta::log::Tap a_tap_allcats(&a, "*", "a_allcats.log"); // Write everything
        sparta::log::Tap a_tap_warnmycat(&a, "warning,mycategory", "a_warnmycat.log"); // Write warning and mycategory
        sparta::log::Tap a_tap_noduplicate(&a, "*,warning, warning ", "a_nodups.log"); // Write everything. Prove no duplicates and parsing functionality
        sparta::log::Tap a_tap_wildparse(&a, " +category ", "a_cats_wildcard.log"); // Wild-card based parsing

        EXPECT_THROW(sparta::TreeNode::parseNotificationNameString("foo bar")); // Not allowed - must be comma separated
        std::cout << sparta::TreeNode::parseNotificationNameString(" +category ") << std::endl;

        EXPECT_NUM_OBSERVATION_POINTS(c_src_mycategory, 4); // by c_tap_mycategory, top_tap_all, a_tap_all, b_tap_mycategory, a_tap_mycategory, a_tap_stder, a_tap_stder2, a_tap_removed
        EXPECT_NUM_OBSERVATION_POINTS(d_src_othercategory, 2); // top_tap_all, a_tap_all, a_tap_stder, a_tap_stder2, a_tap_removed
        EXPECT_NUM_OBSERVATION_POINTS(g_src_mycategory, 3); // e_tap_mycategory, a_tap_all, top_tap_all, a_tap_mycategory, a_tap_stder, a_tap_stder2, a_tap_removed


        // Allow the clock to move in the log files
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));


        // Generate some test messages

        // These messages are observed by both a_tap_all and top_tap_all to "all.log.basic", which should contain 1 copy.
        c_src_mycategory << "Message from C in category 'mycategory'";
        c_src_mycategory << "Another message from C in category 'mycategory' with a new\nline char in the middle that should be converted to a \"\"";

        {
            auto msg = g_src_mycategory.emit("Message from G. ");
            msg << "Should be seen by e_tap_mycategory, a_tap_mycategory";
        } // msg posted at end of its lifetime

        {
            sparta::log::MessageSource::LogObject msg(d_src_othercategory);
            msg << "Message from D. Should be seen by top_tap_all, a_tap_all, and a_tap_stder";

            sparta::log::MessageSource::LogObject msg_to_cancel(d_src_othercategory);
            msg_to_cancel << "Message from D that is CANCELED! Should NOT be seen by top_tap_all, a_tap_all, and a_tap_stder";
            msg_to_cancel.cancel(); // Do not emit!
        } // msg posted at end of its lifetime


        // Remove a tap and gen some more messages
        delete a_tap_removed; // Removed

        EXPECT_NUM_OBSERVATION_POINTS(c_src_mycategory, 4); // by c_tap_mycategory, top_tap_all, a_tap_all, b_tap_mycategory, a_tap_mycategory, a_tap_stder, a_tap_stder2
        EXPECT_NUM_OBSERVATION_POINTS(d_src_othercategory, 2); // top_tap_all, a_tap_all, a_tap_stder, a_tap_stder2
        EXPECT_NUM_OBSERVATION_POINTS(g_src_mycategory, 3); // e_tap_mycategory, a_tap_all, top_tap_all, a_tap_mycategory, a_tap_stder, a_tap_stder2


        // It is important that this does not segfault because of the deletion of a_tap_removed
        c_src_mycategory << "Another message from C in category 'mycategory' after removing the tap to a_removed.log.basic";
        g_src_mycategory << "Message from G. Should be seen by e_tap_mycategory but NOT a_removed";

        g_src_warn << "Another warning after removing the temporary tap on a";

        // Use the global logger.
        // It is dangerous to do this within anything statically constructed
        sparta::log::MessageSource::getGlobalWarn() << "global warning message";
        sparta::log::MessageSource::getGlobalDebug() << "global debug message";

        // Check TreeNode statuses

        EXPECT_TRUE(c_src_mycategory.canGenerateNotification(typeid(sparta::log::Message), ""));
        EXPECT_TRUE(c_src_mycategory.canGenerateNotification(typeid(sparta::log::Message), "mycategory"));
        EXPECT_TRUE(c_src_mycategory.canGenerateNotification(typeid(sparta::log::Message), sparta::StringManager::getStringManager().internString("mycategory")));

        EXPECT_FALSE(top.canGenerateNotification(typeid(sparta::log::Message), "")); // Is not a message source
        EXPECT_FALSE(c_src_mycategory.canGenerateNotification(typeid(sparta::log::Message), "not_a_category")); // Category not used here
        EXPECT_FALSE(c_src_mycategory.canGenerateNotification(typeid(sparta::log::Message), sparta::StringManager::getStringManager().internString("not_a_category"))); // Category not used here

        EXPECT_TRUE(top.canSubtreeGenerateNotification(typeid(sparta::log::Message), ""));
        EXPECT_TRUE(top.canSubtreeGenerateNotification(typeid(sparta::log::Message), "mycategory"));
        EXPECT_TRUE(d.canSubtreeGenerateNotification(typeid(sparta::log::Message), "")); // d has a source

        EXPECT_FALSE(d.canSubtreeGenerateNotification(typeid(sparta::log::Message), "mycategory")); // D source is different category
        EXPECT_FALSE(top.canSubtreeGenerateNotification(typeid(sparta::log::Message), "not_a_category")); // Category not used here


        // Print out the tree at different levels with different options

        std::cout << "The tree from the top: " << std::endl << top.renderSubtree(-1, true) << std::endl;

        std::cout << "\nLogging destination list\n";
        sparta::log::DestinationManager::dumpDestinations(std::cout);

        std::cout << "\nLogging destination file extensions\n";
        sparta::log::DestinationManager::dumpFileExtensions(std::cout);

        // Test HEX macros
        const uint64_t val = std::numeric_limits<uint64_t>::max();
        log_utils_test << HEX  (val, 16);
        log_utils_test << HEX8 (val);
        log_utils_test << HEX16(val);

        // Ensure that there are no duplicate destinations by counting
        // warn.log, cerr, a_out.log, b_out.log, c_out.log, e_out.log,
        // a_removed.log, top_tap_warn.log, all.log.basic, empty.log
        // a_allcats.log, a_warnmycat.log global_warn.log.basic, a_nodups.log,
        // a_cats_wildcard.log, hex_output.basic
        EXPECT_EQUAL(sparta::log::DestinationManager::getNumDestinations(), 16);

        top.enterTeardown();

    } // End of test content scope. Destruction occurs now

    // Finally delete the last tap, which outlives the destruction of the entire
    // tree
    delete a_tap_all;


    // Look at output files (note that the last messages arrive during tree
    // destruction)

    EXPECT_FILES_EQUAL("global_warn.log.basic.EXPECTED","global_warn.log.basic");
    EXPECT_FILES_EQUAL("warn.log.basic.EXPECTED",       "warn.log.basic");
    EXPECT_FILES_EQUAL("a_out.log.basic.EXPECTED",      "a_out.log.basic");
    EXPECT_FILES_EQUAL("b_out.log.basic.EXPECTED",      "b_out.log.basic");
    EXPECT_FILES_EQUAL("c_out.log.basic.EXPECTED",      "c_out.log.basic");
    EXPECT_FILES_EQUAL("e_out.log.basic.EXPECTED",      "e_out.log.basic");
    EXPECT_FILES_EQUAL("a_removed.log.basic.EXPECTED",  "a_removed.log.basic");
    EXPECT_FILES_EQUAL("top_warn.log.basic.EXPECTED",   "top_warn.log.basic");
    EXPECT_FILES_EQUAL("all.log.basic.EXPECTED",        "all.log.basic");
    EXPECT_FILES_EQUAL("empty.log.EXPECTED",            "empty.log");
    EXPECT_FILES_EQUAL("a_allcats.log.EXPECTED",        "a_allcats.log");
    EXPECT_FILES_EQUAL("a_warnmycat.log.EXPECTED",      "a_warnmycat.log");
    EXPECT_FILES_EQUAL("a_nodups.log.EXPECTED",         "a_nodups.log");
    EXPECT_FILES_EQUAL("a_cats_wildcard.log.EXPECTED",  "a_cats_wildcard.log");

    // Done

    REPORT_ERROR;

    return ERROR_CODE;
}
