
#include <inttypes.h>
#include <iostream>

#include "sparta/sparta.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/Resource.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/log/NotificationSource.hpp"

/*!
 * \file Notification_test.cpp
 * \brief Test for notifications
 */

TEST_INIT

typedef sparta::NotificationSourceBase::ObservationStateChange ObservationStateChange;

struct NotificationPayload
{
    int dummy;
};

uint32_t NUM_NOTI_CALLBACKS_RECEIVED = 0;
struct DummyObserver
{
    void callback(const NotificationPayload&) {NUM_NOTI_CALLBACKS_RECEIVED++;}
};


struct TestObserverStateWatcher
{
    void soleObserverRegistered(sparta::NotificationSourceBase const &, uint32_t)
    {
        // needn't do anything
        EXPECT_REACHED();
    }
};

// We need this helper method because passing in templated function calls to
// EXPECT_THROW macro confuses the gcc 4.7 compiler.
void registerNotiHelper(sparta::TreeNode& node, DummyObserver& observer, const std::string& noti)
{
    node.registerForNotification<NotificationPayload,
                                 DummyObserver,
                                 &DummyObserver::callback>(&observer, noti);
}


int main()
{
    { // Scope to this block

        sparta::RootTreeNode node("dummy", "A dummy node");
        sparta::TreeNode private_node("dummy_private", "private child node under dummy");
        sparta::TreeNode node2(&private_node, "node2", "normal node under private_node");
        private_node.makeSubtreePrivate();
        sparta::NotificationSource<NotificationPayload> noti(&node, "noti", "group", 0, "Notification node", "notix");
        sparta::NotificationSource<NotificationPayload> noti_private(&private_node, "noti_private", "group", 0, "Notification node", "notix_private");
        sparta::NotificationSource<NotificationPayload> noti2(&node2, "noti2", "group", 0, "Notification node", "notix");
        EXPECT_NOTHROW(node.enterConfiguring());
        EXPECT_NOTHROW(node.enterFinalized());
        uint32_t sole_obs_regs = 0;
        uint32_t obs_regs = 0;
        uint32_t sole_obs_deregs = 0;
        uint32_t obs_deregs = 0;

        auto handleSoleObserverReg = [&](sparta::NotificationSourceBase const &, uint32_t ) -> void
        {
            sole_obs_regs++;
        };

        sparta::NotificationSourceBase::ObservationStateCallback nscb0(noti,
                                                           ObservationStateChange::SOLE_OBSERVER_REGISTERING,
                                                           handleSoleObserverReg);

        auto handleObserverReg = [&](sparta::NotificationSourceBase const &, uint32_t ) -> void
        {
            obs_regs++;
        };

        sparta::NotificationSourceBase::ObservationStateCallback nscb1(noti,
                                                           ObservationStateChange::OBSERVER_REGISTERING,
                                                           handleObserverReg);

        auto handleSoleObserverDereg = [&](sparta::NotificationSourceBase const &, uint32_t ) -> void
        {
            sole_obs_deregs++;
        };

        sparta::NotificationSourceBase::ObservationStateCallback nscb2(noti,
                                                           ObservationStateChange::SOLE_OBSERVER_DEREGISTERING,
                                                           handleSoleObserverDereg);

        auto handleObserverDereg = [&](sparta::NotificationSourceBase const &, uint32_t ) -> void
        {
            obs_deregs++;
        };

        std::unique_ptr<sparta::NotificationSourceBase::ObservationStateCallback> nscb3(
                 new sparta::NotificationSourceBase::ObservationStateCallback(noti,
                                                                            ObservationStateChange::OBSERVER_DEREGISTERING,
                                                                            handleObserverDereg));

        DummyObserver dummy_observer0;
        DummyObserver dummy_observer1;
        DummyObserver dummy_observer2;
        DummyObserver dummy_observer3;

        node.registerForNotification<NotificationPayload,
                                     DummyObserver,
                                     &DummyObserver::callback>(&dummy_observer0, "notix");

        EXPECT_EQUAL(sole_obs_regs, 1);
        EXPECT_EQUAL(obs_regs, 1);

        node.registerForNotification<NotificationPayload,
                                     DummyObserver,
                                     &DummyObserver::callback>(&dummy_observer1, "notix");

        EXPECT_EQUAL(sole_obs_regs, 1);
        EXPECT_EQUAL(obs_regs, 2);

        node.registerForNotification<NotificationPayload,
                                     DummyObserver,
                                     &DummyObserver::callback>(&dummy_observer2, "notix");


        EXPECT_EQUAL(sole_obs_regs, 1);
        EXPECT_EQUAL(obs_regs, 3);

        // This line below does not compile with GCC 4.7 or 5.2. The macro is not expanded correctly, so we've
        // wrapped the call in a global helper function. The preprocessor appears to get confused on the template
        // arguments.
        //EXPECT_THROW(node.registerForNotification<NotificationPayload, DummyObserver, &DummyObserver::callback>(&dummy_observer3, "notix_private"));
        // We should not be able to register for a notification generated at or below a private node from it's public parent.
        EXPECT_THROW(registerNotiHelper(node, dummy_observer3, "notix_private"));


        // Post just to test correct observer registration

        NotificationPayload p;
        noti.postNotification(p);
        noti_private.postNotification(p);
        noti2.postNotification(p);
        // observer0-3 are registerd for the "notix" notification.
        // This notification gets generated by a node in the private tree
        // and at "node", we should not get callbacks for the one in the private
        // tree.
        EXPECT_EQUAL(NUM_NOTI_CALLBACKS_RECEIVED, 3);
        // Deregister observers

        node.deregisterForNotification<NotificationPayload,
                                       DummyObserver,
                                       &DummyObserver::callback>(&dummy_observer1, "notix");

        EXPECT_EQUAL(sole_obs_regs, 1);
        EXPECT_EQUAL(obs_regs, 3);
        EXPECT_EQUAL(sole_obs_deregs, 0);
        EXPECT_EQUAL(obs_deregs, 1);

        node.deregisterForNotification<NotificationPayload,
                                       DummyObserver,
                                       &DummyObserver::callback>(&dummy_observer2, "notix");

        EXPECT_EQUAL(sole_obs_regs, 1);
        EXPECT_EQUAL(obs_regs, 3);
        EXPECT_EQUAL(sole_obs_deregs, 0);
        EXPECT_EQUAL(obs_deregs, 2);

        // Remove deregister state change callback - should cause no errors
        nscb3.reset();

        node.deregisterForNotification<NotificationPayload,
                                       DummyObserver,
                                       &DummyObserver::callback>(&dummy_observer0, "notix");

        EXPECT_EQUAL(sole_obs_regs, 1);
        EXPECT_EQUAL(obs_regs, 3);
        EXPECT_EQUAL(sole_obs_deregs, 1);
        EXPECT_EQUAL(obs_deregs, 2); // Not incremented third time due to removed callback

        // Register using a class member function

        TestObserverStateWatcher test_obs_state_watcher;

        sparta::NotificationSourceBase::ObservationStateCallback nscb4(noti,
                                                                     ObservationStateChange::SOLE_OBSERVER_REGISTERING,
                                                                     std::bind(&TestObserverStateWatcher::soleObserverRegistered,
                                                                               &test_obs_state_watcher,
                                                                               std::placeholders::_1,
                                                                               std::placeholders::_2));


        // Register again to get the TestObserverStateWatcher it's callback
        node.registerForNotification<NotificationPayload,
                                     DummyObserver,
                                     &DummyObserver::callback>(&dummy_observer0, "notix");

        EXPECT_EQUAL(sole_obs_regs, 2);
        EXPECT_EQUAL(obs_regs, 4);
        EXPECT_EQUAL(sole_obs_deregs, 1);
        EXPECT_EQUAL(obs_deregs, 2);

        // Post just to test correct observer registration

        noti.postNotification(p);

        ENSURE_ALL_REACHED(1); // Just the TestObserverStateWatcher callback
        EXPECT_NOTHROW(node.enterTeardown());
    }

    // Done

    REPORT_ERROR;

    return ERROR_CODE;
}
