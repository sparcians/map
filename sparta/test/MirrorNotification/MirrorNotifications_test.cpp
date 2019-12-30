#include <inttypes.h>
#include <iostream>
#include <sstream>
#include <exception>

#include "sparta/sparta.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/Resource.hpp"
#include "sparta/utils/SpartaTester.hpp"

/*!
 * \file TreeNodeShadowing_test.cpp
 * \brief Test for TreeNode with shadowing features.
 */
using namespace sparta;
TEST_INIT;

class ProxyResource : public Resource
{
public:
    ProxyResource(TreeNode* parent) :
        Resource(parent),
        noti(parent, "noti", "noti source", "noti")
    {}
    MirrorNotificationSource<uint32_t> noti;
};

template<class NotiT=uint32_t>
class ConcreteResource : public Resource
{
public:
    ConcreteResource(TreeNode* parent, const std::string& name) :
        Resource(parent, name),
        noti(parent, "noti", "noti source", "noti")
    {}

    void post()
    {
        noti.postNotification(NotiT());
    }
    NotificationSource<NotiT> noti;
};

class ExternalListenerResource : public Resource
{
public:
    ExternalListenerResource(TreeNode* node) :
        Resource(node),
        node_(node)
    {}

    void onBindTreeLate_() override final
    {
        node_->getParent()->REGISTER_FOR_NOTIFICATION(handleNotiCB_,
                                                      uint32_t,
                                                      "noti");
    }
    void handleNotiCB_(const uint32_t&)
    {
        std::cout << "Yay! ExternalListenerResource::handleNotiCB_ invoked\n";
        num_times_cb_invoked_++;
    }

    uint32_t getNumInvoked() { return num_times_cb_invoked_; }
private:
    uint32_t num_times_cb_invoked_ = 0;
    TreeNode* node_ = nullptr;
};

int main()
{
    {
        /**
         * Build a simple tree that looks like
         *                        top
         *                         |
         *                       core
         *           /            |(private)   \(private)      \
         *          a_shadow    a1_impl      a2_impl          external_listener
         *           |            |            |
         *       shadow_noti_a   noti_a       noti_a
         */
        RootTreeNode top("top");
        TreeNode core(&top, "core", "random core");

        TreeNode a1_impl(&core, "a1_impl", "a concrete a impl");
        ConcreteResource<uint32_t> a1_impl_resource(&a1_impl, "a1_impl_resource");
        a1_impl.makeSubtreePrivate();

        TreeNode a2_impl(&core, "a2_impl", "a concrete a impl");
        ConcreteResource<uint32_t> a2_impl_resource(&a2_impl, "a2_impl_resource");
        // add a non-shadowable notification to make sure we don't shadow it.
        NotificationSource<uint32_t> noti2(&a2_impl, "blah",
                                           "blah", "blah");
        a2_impl.makeSubtreePrivate();
        // NotificationSource<uint32_t> noti2(&a2_impl, "blah", "blah", "blah");
        TreeNode a_shadow(&core, "a_shadow", "a shadow core");
        ProxyResource a_shadow_rc(&a_shadow);
        a_shadow_rc.noti.addLink(static_cast<TreeNode*>(&a1_impl_resource.noti), "");
        a_shadow_rc.noti.addLink(static_cast<TreeNode*>(&a2_impl_resource.noti), "");

        TreeNode external_listener(&core,
                                   "external_listener",
                                   "external_listener");
        ExternalListenerResource external_listener_rc(&external_listener);

        top.enterConfiguring();
        top.enterFinalized();
        std::cout << top.renderSubtree() << "\n";
        top.bindTreeLate();
        top.validatePreRun();
        a1_impl_resource.post();
        EXPECT_EQUAL(external_listener_rc.getNumInvoked(), 1);
        top.enterTeardown();
        std::cout << "Finished test part 1\n";
    }

    {
        /**
         * This is very similar to the code above, but an extra notification exists
         * that is not in all private trees. So we expect an error.
         * Build a simple tree that looks like
         *                        top
         *                         |
         *                       core
         *           /            |(private)   \(private)    \
         *          a_shadow    a1_impl      a2_impl        external_listener
         *           |            |    \        |
         *       shadow_noti_a   noti_a blah   noti_a
         */
        RootTreeNode top("top");
        TreeNode core(&top, "core", "random core");

        TreeNode a1_impl(&core, "a1_impl", "a concrete a impl");
        ConcreteResource<uint32_t> a1_impl_resource(&a1_impl, "a1_impl_resource");
        a1_impl.makeSubtreePrivate();

        TreeNode a2_impl(&core, "a2_impl", "a concrete a impl");
        // Notice here that the concrete resource gets uint64_t instead of uint32_t.
        // This means the payload types are different than the MirrorNotiticationSource
        // in the shadow tree, so we expect the addLink call to fail.
        ConcreteResource<uint64_t> a2_impl_resource(&a2_impl, "a2_impl_resource");
        a2_impl.makeSubtreePrivate();
        // A shadow notification source that never gets linked to shadow anyone,
        // so the validatePreRun() should detect this and fail.
        MirrorNotificationSource<uint32_t> noti2(&a2_impl, "blah",
                                                 "blah", "blah");
        TreeNode a_shadow(&core, "a_shadow", "a shadow core");
        ProxyResource a_shadow_rc(&a_shadow);
        a_shadow_rc.noti.addLink(static_cast<TreeNode*>(&a1_impl_resource.noti), "");
        // We expect this next addLink call to fail because the payload types are different.
        EXPECT_THROW(a_shadow_rc.noti.addLink(static_cast<TreeNode*>(&a2_impl_resource.noti), ""));
        // Build the listener off to the side to register for the a_shadow_rc.noti
        // this should fail though during bindTreeLate() because the concrete nodes
        // have different notification types.
        TreeNode external_listener(&core,
                                   "external_listener",
                                   "external_listener");
        ExternalListenerResource external_listener_rc(&external_listener);

        top.enterConfiguring();
        top.enterFinalized();
        top.bindTreeLate();
        EXPECT_THROW(top.validatePreRun());
        std::cout << top.renderSubtree() << "\n";
        top.enterTeardown();
        std::cout << "Finished test part 2\n";
    }
    REPORT_ERROR;
}
