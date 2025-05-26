
#include <inttypes.h>
#include <iostream>

#include "sparta/sparta.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/DynamicResourceTreeNode.hpp"
#include "sparta/simulation/Resource.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/parsers/ConfigParserYAML.hpp"
#include "sparta/parsers/ConfigEmitterYAML.hpp"
#include "sparta/utils/StringManager.hpp"
#include "sparta/simulation/TreeNodePrivateAttorney.hpp"

/*!
 * \file TreeNode_test.cpp
 * \brief Test for TreeNode, parameters, and simple parsing of configuration files
 */

TEST_INIT

//! Simple device which defines its own parameter set object.
class SimpleDevice : public sparta::Resource
{
public:

    static int num_simpledevices_torn_down;

    class ParameterSet; // Forward declared so our constructor can be at the top of this class def

    // Declaring as constexpr allows inline static const char*/float/double members
    static constexpr const char* name="SimpleDevice";

    SimpleDevice(sparta::TreeNode * node,
                 const SimpleDevice::ParameterSet * params) :
        sparta::Resource(node)
    {
        // All parameters are ignored
        params->param0.ignore();
        params->param1.ignore();
        params->param2.ignore();
        params->param3.ignore();
        params->param4.ignore();
        params->param5.ignore();
        params->param6.ignore();
        params->param7.ignore();
        params->param8.ignore();
        params->param9.ignore();
        params->param10.ignore();
        params->param11.ignore();
        params->similar.ignore();
        params->similar00.ignore();
        params->similar01.ignore();
        params->similar02.ignore();
        params->similar10.ignore();
        params->similar11.ignore();
        params->similar12.ignore();
        params->manual_uint32vec.ignore();
        params->manual_strvec.ignore();
        params->manual_int32_neg.ignore();
        params->manual_int32_pos.ignore();
        params->manual_double.ignore();
        params->manual_bool.ignore();
        params->manual_str.ignore();
        params->strvecvec.ignore();
        params->strvecvecvec.ignore();
        params->intvecvecvec.ignore();
    }

    class ParameterSet : public sparta::ParameterSet
    {
    public:
        ParameterSet(sparta::TreeNode* parent) :
            sparta::ParameterSet(parent)
        {
            param1.setNumericDisplayBase(sparta::utils::BASE_HEX);
            param6.setNumericDisplayBase(sparta::utils::BASE_HEX);
            param7.setNumericDisplayBase(sparta::utils::BASE_OCT);

        }

        PARAMETER(bool, param0, false, "Should be printed as 'false'")
        PARAMETER(uint32_t, param1, 1, "Docstring for param1")
        PARAMETER(uint32_t, param2, 2, "Docstring for param2")
        PARAMETER(std::vector<uint32_t>, param3, std::vector<uint32_t>(), "Docstring for param3")
        PARAMETER(std::vector<std::string>, param4, std::vector<std::string>(), "Docstring for param4")

        PARAMETER(std::vector<double>, param5, std::vector<double>(), "desc")
        PARAMETER(uint32_t, param6, 0, "hex number")
        PARAMETER(uint32_t, param7, 0, "oct number")
        PARAMETER(std::vector<uint32_t>, param8, std::vector<uint32_t>({1,2,3,4,5,6}), "uint32 vector with long default") // Ensure that config file writes shorter value
        PARAMETER(std::string, param9, "", "parameter nine") // store empty in 1 param, set to string-with-spaces in another
        PARAMETER(uint32_t, param10, 0xbad, "parameter ten")
        PARAMETER(uint32_t, param11, 0xbad, "parameter eleven")
        PARAMETER(int64_t,  param12, 0xbad, "parameter twelve")
        PARAMETER(int64_t,  param13, 0xbad, "parameter thirteen")

        PARAMETER(uint32_t, similar,   0x00, "similar parameters with a very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very very long description.")
        PARAMETER(uint32_t, similar00, 0x00, "similar parameters")
        PARAMETER(uint32_t, similar01, 0x01, "similar parameters")
        PARAMETER(uint32_t, similar02, 0x02, "similar parameters")
        PARAMETER(uint32_t, similar10, 0x10, "similar parameters")
        PARAMETER(uint32_t, similar11, 0x11, "similar parameters")
        PARAMETER(uint32_t, similar12, 0x12, "similar parameters")

        VOLATILE_PARAMETER(std::vector<uint32_t>, manual_uint32vec, std::vector<uint32_t>(), "desc")
        VOLATILE_PARAMETER(std::vector<std::string>, manual_strvec, std::vector<std::string>(), "desc")
        PARAMETER(int32_t, manual_int32_neg, 0, "desc")
        PARAMETER(int32_t, manual_int32_pos, 0, "desc")
        PARAMETER(double, manual_double, 0.0, "desc")
        PARAMETER(bool, manual_bool, false, "desc")
        PARAMETER(std::string, manual_str, "default string", "desc")

        PARAMETER(std::vector<std::vector<std::string>>, strvecvec, {},
                  "2d vector of strings")
        PARAMETER(std::vector<std::vector<std::vector<std::string>>>, strvecvecvec, {},
                  "3d vector of strings")
        PARAMETER(std::vector<std::vector<std::vector<int>>>, intvecvecvec, {},
                  "3d vector of ints")
    };

    void simulationTerminating_() override {
        std::cout << "Simulation termination called" << std::endl;
        EXPECT_EQUAL(num_simpledevices_torn_down, 0);
        EXPECT_REACHED();
    }

    // Callback when tearing down
    void onStartingTeardown_() override {
        std::cout << "Starting Teardown of SimpleDevice x" << ++num_simpledevices_torn_down << std::endl;
        EXPECT_REACHED();
    }

};

int SimpleDevice::num_simpledevices_torn_down = 0;

class ResourceWithDynamicChildren;

//! A dynamically created resource (created by ResourceWithDynamicChildren)
class DynResource : public sparta::Resource
{
public:
    // Declaring as constexpr allows inline static const char*/float/double members
    static constexpr const char* name="DynResource";

    DynResource(sparta::TreeNode * node, // Node containing this resource
                const SimpleDevice::ParameterSet * params // Parameters for this resource (happen to be the same as parents')
                ) :
        sparta::Resource(node)
    {
        (void) params;

        EXPECT_NOTHROW( EXPECT_NOTEQUAL( node->getParent()->getChildAs<sparta::ParameterSet>("params"), nullptr) );

        // Guaranteed a clock at this point
        EXPECT_NOTEQUAL(node->getClock(), nullptr);

        // Should have a parent (in this test it will)
        EXPECT_NOTHROW( EXPECT_NOTEQUAL( node->getParent(), nullptr ));

        // Parent will have its resource already (if parent has a resource) even
        // though this code is called within the stack of the parent resource
        // constructor
        EXPECT_NOTHROW( EXPECT_NOTEQUAL( node->getParent()->getResourceAs<ResourceWithDynamicChildren>(), nullptr ));
    }
};

//! A dynamically created resource (created by ResourceWithDynamicChildren)
class DynResourceWithCustomParams : public sparta::Resource
{
public:

    class CustomParams : public sparta::ParameterSet
    {
    public:
        CustomParams(sparta::TreeNode* parent) :
            sparta::ParameterSet(parent)
        {
        }

        PARAMETER(int32_t, example_custom_param, 0, "desc")
    };

    // Declaring as constexpr allows inline static const char*/float/double members
    static constexpr const char* name="DynResource";

    DynResourceWithCustomParams(sparta::TreeNode * node, // Node containing this resource
                                const CustomParams * params // Parameters for this resource (happen to be the same as parents')
                                ) :
        sparta::Resource(node)
    {
        // Access custom parameter
        //params->example_custom_param.ignore();
        EXPECT_EQUAL(params->example_custom_param, 1234567);

        // Parameters should be temporary and parentless
        EXPECT_EQUAL(params->getParent(), node);
    }
};

// ILLEGAL TYPE & FACTORY - does not inherit from sparta::Resource
//class Foo {
//public:
//    static constexpr const char* name="Foo";
//
//    Foo(sparta::TreeNode*, sparta::ParameterSet const *)
//    {;}
//};
//sparta::ResourceFactory<Foo, sparta::ParameterSet> x;

class ResourceWithDynamicChildren : public sparta::Resource
{
    // Destruct after child3_
    sparta::ResourceFactory<SimpleDevice, SimpleDevice::ParameterSet> sd_fact_;
    sparta::ResourceFactory<DynResourceWithCustomParams, DynResourceWithCustomParams::CustomParams> drwcp_fact_;

    std::unique_ptr<sparta::TreeNode> child1_;
    std::unique_ptr<sparta::TreeNode> child2_;
    std::unique_ptr<sparta::TreeNode> child3_;
    std::unique_ptr<sparta::TreeNode> child4_;

public:
    // Declaring as constexpr allows inline static const char*/float/double members
    static constexpr const char* name="ResourceWithDynamicChildren";

    ResourceWithDynamicChildren(sparta::TreeNode * node,
                                const SimpleDevice::ParameterSet * params) :
        sparta::Resource(node)
    {
        EXPECT_NOTHROW(node->getChildAs<sparta::ParameterSet>("params"));

        child1_.reset(new sparta::DynamicResourceTreeNode<DynResource,
                                                        SimpleDevice::ParameterSet>
                          (node, // this is parent
                           "child", // new child node name
                           "Dynamically created child node",
                           params)); // Constructor takes same parameters as this resource. Could be a copy or modification though

        // It does NOT immediately have a resource, so it throws.
        EXPECT_THROW(child1_->getResource());


        // Create another child
        auto n = new sparta::DynamicResourceTreeNode<DynResource,
                                                   SimpleDevice::ParameterSet>
                    (node, // this is parent
                     "child2", // new child node name
                     "Dynamically created child node",
                     params); // Constructor takes same parameters as this resource. Could be a copy or modification though
        child2_.reset(n);
        n->finalize(); // Create a resource for it

        // It immediately has a resource!
        EXPECT_NOTHROW( EXPECT_NOTEQUAL(child2_->getResource(), nullptr) );


        // Create a ResourceTreeNode here and let the tree-walking finalize it
        // later (after returning from this ctor).
        auto rtn = new sparta::ResourceTreeNode(node, "child3", "Dynamically created child node", &sd_fact_);
        child3_.reset(rtn);
        EXPECT_THROW(rtn->getResource());

        // Create a ResourceTreeNode here with a custom parameter set that is
        // populated now. These parameters are not configuratable during
        // simulator configuration (i.e. through command line parameters and
        // configuration files), but can be manually set here
        EXPECT_EQUAL(child4_.get(), nullptr);
        sparta::ResourceTreeNode* rtn2 = new sparta::ResourceTreeNode(node,
                                                                  "child4",
                                                                  "Dynamically created child node with params",
                                                                  &drwcp_fact_);
        child4_.reset(rtn2);

        // Set a parameter the awkward way (using a string)
        EXPECT_NOTHROW(rtn2->getParameterSet()->getParameter("example_custom_param")->setValueFromString("9999"));

        // Set a parameter with slightly more ease
        EXPECT_NOTHROW(rtn2->getParameterSet()->getParameterAs<int32_t>("example_custom_param") = 1111);
        EXPECT_THROW(rtn2->getParameterSet()->getParameterAs<uint32_t>("example_custom_param") = 1111); // ERROR; wrong Type

        // Set a parameter easily with compile-time name and type checking.
        // Requires we get the parameter set of the right type from the new
        // child ResourceTreeNode. Then its parameters can be accessed directly
        // as members. Getting the ParameterSet is the only thing that can fail
        // at runtime in this case - and only if the template type given below
        // is wrong.
        DynResourceWithCustomParams::CustomParams* cps = nullptr;
        EXPECT_NOTHROW(cps = rtn2->getChildAs<DynResourceWithCustomParams::CustomParams*>(sparta::ParameterSet::NODE_NAME));
        EXPECT_NOTEQUAL(cps, nullptr);
        if(cps){
            // Compile-time safety check
            EXPECT_NOTHROW(cps->example_custom_param = 1234567);
        }

        // Finalize this RTN and check its resource
        EXPECT_NOTHROW(rtn2->finalize());
        EXPECT_NOTHROW(child4_->getResource());

        // Ignore all paraters that we will not read
        params->param0.ignore();
        params->param1.ignore();
        params->param2.ignore();
        params->param3.ignore();
        params->param4.ignore();
        params->param5.ignore();
        params->param6.ignore();
        params->param7.ignore();
        params->param8.ignore();
        params->param9.ignore();
        params->param10.ignore();
        params->param11.ignore();
        params->param12.ignore();
        params->param13.ignore();
        params->similar.ignore();
        params->similar00.ignore();
        params->similar01.ignore();
        params->similar02.ignore();
        params->similar10.ignore();
        params->similar11.ignore();
        params->similar12.ignore();
        params->manual_uint32vec.ignore();
        params->manual_strvec.ignore();
        params->manual_int32_neg.ignore();
        params->manual_int32_pos.ignore();
        params->manual_double.ignore();
        params->manual_bool.ignore();
        params->manual_str.ignore();
        params->strvecvec.ignore();
        params->strvecvecvec.ignore();
        params->intvecvecvec.ignore();
    }
};


class SimpleDevice3 : public sparta::Resource
{
public:
    // Declaring as constexpr allows inline static const char*/float/double members
    static constexpr const char* name="SimpleDevice3";

    class ParameterSet { // Cannot cast to sparta::ParameterSet
    public:
        ParameterSet(sparta::TreeNode*){};
    };

    SimpleDevice3(sparta::TreeNode * node,
                 const ParameterSet * params) :
        sparta::Resource(node)
    {
        (void) params;
    }
};

// Simple device that is a leaf in the device tree.
// There are no special implications of being a leaf on the resource class def
class LeafDevice : public sparta::Resource
{
public:
        // Declaring as constexpr allows inline static const char*/float/double members
    static constexpr const char* name="LeafDevice";

    class ParameterSet : public sparta::ParameterSet
    {
    public:
        ParameterSet(sparta::TreeNode* n) :
            sparta::ParameterSet(n)
        { }
    };

    LeafDevice(sparta::TreeNode * node,
               const ParameterSet * params) :
        sparta::Resource(node)
    {
        (void) params;
    }
};

// A dummy device that will try to use it's treenode to find a private sibling in the tree
// during construction. It should not be able to!
class FindAPrivateNodeDevice : public sparta::Resource
{
public:
    static constexpr const char* name="FindAPrivateNodeDevice";
    FindAPrivateNodeDevice(sparta::TreeNode* node,
                           const sparta::ParameterSet*) :
        sparta::Resource(node)
    {
        // sanity check that i was created with the node i expected.
        EXPECT_EQUAL(node->getName(), "a_public");
        // i an "a_private" sibling that I cannot access due to privacy level.
        // node->getParent()->getChild("a_private");
        EXPECT_NOTHROW(node->getParent()->getChild("a1_public"));
        EXPECT_THROW(node->getParent()->getChild("a1_public")->getChild("a_private"));
    }
};


class SimpleDeviceFactory : public sparta::ResourceFactory<SimpleDevice, SimpleDevice::ParameterSet>
{
};

typedef sparta::ResourceFactory<ResourceWithDynamicChildren, SimpleDevice::ParameterSet> SimpleDeviceFactory2;

class SimpleDeviceFactory3 : public sparta::ResourceFactory<SimpleDevice, SimpleDevice::ParameterSet>
{
    // Hold onto a resource factory for the children
    sparta::ResourceFactory<LeafDevice> leaf_child_;

public:

    SimpleDeviceFactory3() :
        sparta::ResourceFactory<SimpleDevice, SimpleDevice::ParameterSet>()
    { }

    // Invoked at construction of device tree for node n
    // Called during construction of each instance of a ResourceTreeNode referring to this factory
    // instance
    virtual void createSubtree(sparta::ResourceTreeNode* n) override {
        // Create a ResourceTreeNode called leaf as a child which will construct a LeafDevice.
        // We will find and delete this node in deleteSubtree when n is being destroyed
        new sparta::ResourceTreeNode(n, "leaf", "A leaf child ResourceTreeNode", &leaf_child_);
    }

    virtual void onBuilding(sparta::ResourceTreeNode* n) override {
        createSubtree(n);
    }

    // Invoked at teardown of device tree at node n
    // Called for each instance of a ResourceTreeNode referring to this factory instance
    virtual void deleteSubtree(sparta::ResourceTreeNode* n) override {
        // false argument to getChildAs means return null and do not throw if not found.
        sparta::ResourceTreeNode* leaf = n->getChildAs<sparta::ResourceTreeNode>("leaf", false);
        delete leaf;

        // Alternatively, one could use a map of n->children to track all children allocated for
        // each node n and delete all children in the list associated with the n argument of
        // deleteSubtree
    }
};

int main()
{
    { // Scope to this block
        SimpleDeviceFactory fact; // Factory can be subclassed trivially
        SimpleDeviceFactory2 fact2; // Or typedefed
        SimpleDeviceFactory3 fact3; // Or subclassed and overridden (adds a ResourceTreeNode child during finalization)
        sparta::ResourceFactory<SimpleDevice> fact4; // ParameterSet figured out automatically
        sparta::ResourceFactory<SimpleDevice, SimpleDevice::ParameterSet> fact5; // Manually declared
        sparta::ResourceFactory<ResourceWithDynamicChildren, SimpleDevice::ParameterSet> fact6; // Re-use of someone else's Parameters
        sparta::ResourceFactory<FindAPrivateNodeDevice, sparta::ParameterSet> fact_find_a_private;
        //sparta::ResourceFactory<ResourceWithDynamicChildren> fact6; // compile error - ResourceWithDynamicChildren has no ParameterSet member
        //sparta::ResourceFactory<SimpleDevice3> fact7; // compile error - SimpleDevice3 has a ParameterSet but it is not a Parameter subclass

        sparta::Scheduler sched;
        sparta::Clock clk("clock", &sched);

        // Create Tree Nodes (not resources)

        // Bad node construction            node name  , node group , group idx, desc,   factory
        EXPECT_THROW(sparta::ResourceTreeNode("for",                               "desc", &fact)); // name is reserved keyword in Python or SPARTA
        EXPECT_THROW(sparta::ResourceTreeNode("a__b",                              "desc", &fact)); // double-underscore is disallowed
        EXPECT_THROW(sparta::ResourceTreeNode("bad%nameA",                         "desc", &fact)); // name contains non-(alphanum or _)
        EXPECT_THROW(sparta::ResourceTreeNode("0badnameB",                         "desc", &fact)); // name begins with digit
        EXPECT_THROW(sparta::ResourceTreeNode("goodnameC", "",          0,         "desc", &fact)); // group=="" && idx==0
        EXPECT_THROW(sparta::ResourceTreeNode("goodnameD", "for",       0,         "desc", &fact)); // group is reserved keyword in Pyhton or SPARTA
        EXPECT_THROW(sparta::ResourceTreeNode("goodnameE", "a__b",      0,         "desc", &fact)); // double-underscore is disallowed
        EXPECT_THROW(sparta::ResourceTreeNode("goodnameF", "bad%group", 0,         "desc", &fact)); // group contains non-(alphanum or _)
        EXPECT_THROW(sparta::ResourceTreeNode("goodnameG", "badgroup0", 0,         "desc", &fact)); // group ends in digit
        EXPECT_THROW(sparta::ResourceTreeNode("goodnameH", "0badgroup", 0,         "desc", &fact)); // group begins with digit

        // Good construction
        sparta::ResourceTreeNode dummy("dummy", "desc", &fact);
        sparta::RootTreeNode top("top");
        sparta::ResourceTreeNode a("a", "", sparta::TreeNode::GROUP_IDX_NONE, "The A node", &fact);
        sparta::ResourceTreeNode cant_see_me("cant_see_me", "", sparta::TreeNode::GROUP_IDX_NONE,
                                           "a private node", &fact);
        cant_see_me.makeSubtreePrivate();
        sparta::TreeNode a1 (&cant_see_me, "a1_public",
                           "A public node under a private subtree where private subtree is rooted at cant_see_me");
        sparta::TreeNode a_private(&a1, "a_private",
                                 "a private node under cant_see_me");
        a_private.makeSubtreePrivate();
        sparta::ResourceTreeNode a_public(&cant_see_me, "a_public",
                                        "a public node under cant_see_me", &fact_find_a_private);
        sparta::ResourceTreeNode b("b", "b_group", 0, "The B node", &fact2);
        sparta::ResourceTreeNode b1("b1", "b_group", 1, "The B1 node", &fact3);
        EXPECT_NOTHROW(b1.addAlias("b_one")); // Exercise addAlias (singular)

        // Good construction (but will fail to add to tree below);
        sparta::ResourceTreeNode b_dup1("b", "b_group", 1, "The B duplicate node", &fact); // duplicate name
        sparta::ResourceTreeNode b_dup2("b_dup", "b_group", 0, "The B duplicate node", &fact); // duplicate group index
        sparta::ResourceTreeNode b_dup3("b_dup", "The B duplicate node", &fact); // will have duplicate aliases

        sparta::TreeNode::node_uid_type last_uid;
        last_uid = dummy.getNodeUID();
        EXPECT_TRUE(top.getNodeUID() > last_uid);    last_uid = top.getNodeUID();
        EXPECT_TRUE(a.getNodeUID() > last_uid);      last_uid = a.getNodeUID();
        EXPECT_TRUE(b.getNodeUID() > last_uid);      last_uid = b.getNodeUID();
        EXPECT_TRUE(b1.getNodeUID() > last_uid);     last_uid = b1.getNodeUID();
        EXPECT_TRUE(b_dup1.getNodeUID() > last_uid); last_uid = b_dup1.getNodeUID();
        EXPECT_TRUE(b_dup2.getNodeUID() > last_uid); last_uid = b_dup2.getNodeUID();
        EXPECT_TRUE(b_dup3.getNodeUID() > last_uid); last_uid = b_dup3.getNodeUID();

        // Configure some nodes (before attaching)
        sparta::TreeNode::AliasVector aliases;
        aliases.push_back("b_one");
        aliases.push_back("dumb");
        aliases.push_back("dumber");
        aliases.push_back("dumbest");
        EXPECT_NOTHROW(b_dup3.addAliases(aliases)); // Exersize addAliases (multiple)
        EXPECT_THROW(b_dup3.addAliases(aliases)); // Cannot re-add same aliases

        EXPECT_TRUE(top.isAttached() == true); // Top node is always "attached"
        EXPECT_TRUE(a.isAttached() == false);
        EXPECT_TRUE(b.isAttached() == false);

        EXPECT_EQUAL(top.getRoot(), &top);
        EXPECT_EQUAL(a.getRoot(), &a);
        EXPECT_EQUAL(b.getRoot(), &b);
        EXPECT_EQUAL(b1.getRoot(), &b1);
        EXPECT_EQUAL(a.getParameterSet()->getRoot(), &a); // Self is root until it has a parent


        /* Build Tree
         * -----------------------------------------------------
         *
         *                      top (tag1, tag2)
         *                      /
         *                     a (tag1, tag3)                          \
         *             _______/ \_______________________
         *            /               \                 \               cant_see_me (private)
         *           /                 \                 params
         *          /                   \
         *  (tag1) b [adds b_group]      b1 [adds b_one] (tag2, tag3)
         *          \                     \
         *           params                params
         *
         * -----------------------------------------------------
         * *All nodes except top have parameters
         *
         */

        // Add some tags
        EXPECT_NOTHROW(top.addTag("tag1"));
        EXPECT_NOTHROW(top.addTag("tag2"));
        EXPECT_NOTHROW(  a.addTags(std::vector<std::string>{"tag1", "tag3"}));
        EXPECT_NOTHROW(  b.addTag("tag1"));
        EXPECT_NOTHROW( b1.addTag("tag2"));
        EXPECT_NOTHROW( b1.addTag("tag3"));
        EXPECT_NOTHROW(a.addTag("a_tag"));
        EXPECT_NOTHROW(a_private.addTag("a_private_tag"));

        EXPECT_THROW(top.addTag("tag1"));
        EXPECT_THROW(top.addTag("tag2"));
        EXPECT_THROW(  a.addTag("tag1"));
        EXPECT_THROW(  a.addTag("tag3"));
        EXPECT_THROW(  b.addTag("tag1"));
        EXPECT_THROW( b1.addTag("tag2"));
        EXPECT_THROW( b1.addTag("tag3"));

        // Check tag table status
        EXPECT_EQUAL(top.getTags().size(), 2);
        EXPECT_TRUE(top.hasTag("tag1"));
        EXPECT_TRUE(top.hasTag(sparta::StringManager::getStringManager().internString("tag2")));
        EXPECT_FALSE(top.hasTag("tag3"));
        EXPECT_EQUAL(  a.getTags().size(), 3);
        EXPECT_EQUAL(  b.getTags().size(), 1);
        EXPECT_EQUAL( b1.getTags().size(), 2);

        // Check the locations (TreeNode::getLocation)
        EXPECT_EQUAL(top.getLocation(), "top");
        EXPECT_EQUAL(a.getLocation(), "~a");
        EXPECT_EQUAL(b.getLocation(), "~b");
        EXPECT_EQUAL(b1.getLocation(), "~b1");

        // Invalid construction
        EXPECT_THROW(a.addChild(&a)); // ERROR: self-child
        EXPECT_THROW(dummy.addChild(&top)); // ERROR: top cannot be a child

        // Legal building
        EXPECT_NOTHROW(  a.addChild(&b));
        EXPECT_NOTHROW(top.addChild(cant_see_me));
        // We should be able to see this private child since we have
        // not yet finalized the tree.
        EXPECT_EQUAL(sparta::TreeNodePrivateAttorney::getAllChildren(top).size(), 2);
        EXPECT_EQUAL(b.getLocation(), "~a.b");
        EXPECT_EQUAL(a.getChildAs<sparta::ResourceTreeNode>("b"), &b ); // Lazy template arg
        EXPECT_EQUAL(a.getChildAs<sparta::ResourceTreeNode*>("b"), &b); // Accurate template arg
        EXPECT_EQUAL((static_cast<const sparta::TreeNode*>(&a)->getChildAs<sparta::ResourceTreeNode>("b")), &b ); // Lazy template arg
        EXPECT_EQUAL((static_cast<const sparta::TreeNode*>(&a)->getChildAs<sparta::ResourceTreeNode*>("b")), &b); // Accurate template arg
        EXPECT_EQUAL(a.getLocation(), "~a");;

        EXPECT_NOTHROW(top.addChild(&a));
        EXPECT_EQUAL(b.getLocation(), "top.a.b");
        EXPECT_NOTHROW(  a.addChild(&b1));

        // This test is no longer valid.  The subtrees are not created
        // via the factories immediately on construction.  They are
        // now done on configuration to allow factory subtrees to use extensions.
        // EXPECT_NOTHROW(b1.getChild("leaf")); // Created by b1

        // Illegal building (fails)
        EXPECT_THROW(b.addChild(&b)); // ERROR: self-child
        EXPECT_THROW(a.addChild(&b1)); // ERROR: already attached
        EXPECT_THROW(b.addChild(&a)); // ERROR: cycle (b already child of a)
        EXPECT_THROW(a.addChild(&b_dup1)); // name collides with 'b'
        EXPECT_NOTHROW(a.getChild(b_dup1.getName())); // Collided with 'b'. Should be OK
        EXPECT_THROW(a.addChild(&b_dup2)); // group index collides with 'b' (b_group)
        EXPECT_THROW(a.getChild(b_dup2.getName())); // Should not have added
        EXPECT_THROW(a.addChild(&b_dup3)); // Alias collides with 'b' (b_one)
        EXPECT_THROW(a.getChild(b_dup3.getName())); // Should not have added

        EXPECT_EQUAL(top.isBuilt(), false);
        EXPECT_EQUAL(top.isConfigured(), false);
        EXPECT_EQUAL(top.isFinalizing(), false);
        EXPECT_EQUAL(top.isFinalized(), false);

        EXPECT_FALSE(top.isBuilt());
        EXPECT_FALSE(a.isBuilt());
        EXPECT_FALSE(b.isBuilt());
        EXPECT_FALSE(b1.isBuilt());

        EXPECT_FALSE(top.isFinalized());
        EXPECT_FALSE(a.isFinalized());
        EXPECT_FALSE(b.isFinalized());
        EXPECT_FALSE(b1.isFinalized());

        // For the moment, both of these cases acceptable
        //EXPECT_THROW(sparta::TreeNode(&a, "b_group", "okgroupA", 0, "desc")); // name collides with existing group
        //EXPECT_THROW(sparta::TreeNode(&a, "oknameA", "b", 0, "desc")); // group collides with existing name

        // Given the b->params, try to find 'a'
        sparta::TreeNode* b_p = b.getParameterSet();
        sparta::TreeNode * ances = b_p->findAncestorByType<SimpleDevice>();
        EXPECT_TRUE(ances != nullptr);
        EXPECT_EQUAL(ances->getName(), "a");
        sparta::TreeNode *ancestor_by_tag = b_p->findAncestorByTag("a_tag");
        EXPECT_TRUE(ancestor_by_tag != nullptr);
        EXPECT_EQUAL(ancestor_by_tag->getName(), "a");

        std::cout << "The tree from the top (with builtin groups): " << std::endl << top.renderSubtree(-1, true) << std::endl;

        EXPECT_EQUAL(b.getRecursiveNodeCount<sparta::ParameterBase>(), 31);
        EXPECT_EQUAL(a.getRecursiveNodeCount<sparta::ParameterBase>(), 31 * 3);

        std::vector<sparta::TreeNode*> r;
        r.clear();
        EXPECT_EQUAL(top.findChildrenByTag("tag1", r, -1), 3);
        EXPECT_EQUAL(r.size(), 3);
        // r.clear(); // delibrately DO NOT CLEAR to ensure that tags are added
        EXPECT_EQUAL(top.findChildrenByTag("tag2", r, -1), 2);
        EXPECT_EQUAL(r.size(), 5); // 2 plus previous 3
        r.clear();
        EXPECT_EQUAL(top.findChildrenByTag("a_private_tag", r), 1);
        r.clear();
        EXPECT_EQUAL(top.findChildrenByTag("tag3", r, -1), 2);
        EXPECT_EQUAL(r.size(), 2);
        r.clear();
        EXPECT_EQUAL(top.findChildrenByTag("tag_nonsense", r, -1), 0);
        EXPECT_EQUAL(r.size(), 0);
        r.clear();
        EXPECT_EQUAL(top.findChildrenByTag("tag1", r, 0), 1); // Just self
        EXPECT_EQUAL(r.size(), 1);
        r.clear();
        EXPECT_EQUAL(top.findChildrenByTag("tag1", r, 1), 2); // Self and immediate children
        EXPECT_EQUAL(r.size(), 2);
        r.clear();
        EXPECT_EQUAL(top.findChildrenByTag("tag1", r, 2), 3); // Self and 2 levels of children
        EXPECT_EQUAL(r.size(), 3);
        r.clear();
        EXPECT_EQUAL(top.findChildrenByTag("tag3", r, 1), 1); // Self and immediate children
        EXPECT_EQUAL(r.size(), 1);
        r.clear();
        EXPECT_EQUAL(top.findChildrenByTag("tag_nonsense", r, -1), 0); // Self and immediate children
        EXPECT_EQUAL(r.size(), 0);
        r.clear();
        EXPECT_EQUAL(a.findChildrenByTag("tag1", r, -1), 2);
        EXPECT_EQUAL(r.size(), 2);
        r.clear();
        EXPECT_EQUAL(a.findChildrenByTag("tag2", r, -1), 1);
        EXPECT_EQUAL(r.size(), 1);
        r.clear();
        EXPECT_EQUAL(a.findChildrenByTag("tag_nonsense", r, -1), 0);
        EXPECT_EQUAL(r.size(), 0);
        r.clear();
        EXPECT_EQUAL(b.findChildrenByTag("tag1", r, -1), 1);
        EXPECT_EQUAL(r.size(), 1);
        r.clear();
        EXPECT_EQUAL(b.findChildrenByTag("tag2", r, -1), 0);
        EXPECT_EQUAL(r.size(), 0);


        // Check Tree State

        EXPECT_EQUAL(top.getRoot(), &top);
        EXPECT_EQUAL(a.getRoot(), &top);
        EXPECT_EQUAL(b.getRoot(), &top);
        EXPECT_EQUAL(b1.getRoot(), &top);
        EXPECT_EQUAL(a.getParameterSet()->getRoot(), &top);
        EXPECT_TRUE(a.getName() == "a");
        EXPECT_TRUE(b.getName() == "b");
        EXPECT_TRUE(a.getDesc() == "The A node");
        EXPECT_TRUE(b.getDesc() == "The B node");
        EXPECT_TRUE(a.getGroup() == sparta::TreeNode::GROUP_NAME_NONE);
        EXPECT_TRUE(b.getGroup() == "b_group");
        EXPECT_TRUE(b1.getGroup() == "b_group");
        EXPECT_TRUE(b.getGroupIdx() == 0);
        EXPECT_TRUE(b1.getGroupIdx() == 1);
        EXPECT_TRUE(top.getParent() == 0);
        EXPECT_TRUE(a.getParent() == &top);
        EXPECT_TRUE(b.getParent() == &a);
        EXPECT_TRUE(top.hasImmediateChild(&a));
        EXPECT_NOTHROW(std::ignore = top.getChildren().at(0));
        EXPECT_NOTHROW(std::ignore = a.getChildren().at(0));
        EXPECT_NOTHROW(std::ignore = a.getChildren().at(1));
        EXPECT_NOTHROW(std::ignore = a.getChildren().at(2));
        EXPECT_THROW(std::ignore = a.getChildren().at(3));
        EXPECT_NOTHROW(std::ignore = b.getChildren().at(0));
        EXPECT_THROW(std::ignore = b.getChildren().at(1)); // No dynamically created child YET
        std::vector<std::string> idents;
        a.getChildrenIdentifiers(idents);
        EXPECT_EQUAL(idents.size(), (size_t)7);
        std::cout << "A idents: " << idents << std::endl;
        EXPECT_NOTHROW(a.getAs<sparta::TreeNode*>());
        EXPECT_NOTHROW(a.getAs<sparta::TreeNode>());
        EXPECT_NOTHROW(static_cast<const sparta::TreeNode*>(&a)->getAs<sparta::TreeNode*>());
        EXPECT_NOTHROW(static_cast<const sparta::TreeNode*>(&a)->getAs<sparta::TreeNode>());
        EXPECT_EQUAL(a.getAs<sparta::TreeNode*>(), a.getAs<sparta::TreeNode>()); // getAs always gets you a pointer
        EXPECT_EQUAL(a.getAs<sparta::TreeNode*>(), &a);
        EXPECT_NOTHROW(((const sparta::TreeNode*)&a)->getAs<sparta::TreeNode>());
        EXPECT_NOTHROW(a.getAs<sparta::ResourceTreeNode>());
        EXPECT_NOTHROW(top.getAs<sparta::RootTreeNode>());
        EXPECT_NOTHROW(top.getAs<sparta::TreeNode>());
        EXPECT_NOTHROW(((sparta::TreeNode*)&top)->getAs<sparta::TreeNode>());
        EXPECT_THROW(a.getAs<sparta::RootTreeNode>());
        EXPECT_THROW(top.getAs<sparta::GlobalTreeNode>());
        EXPECT_THROW(top.getAs<sparta::ResourceTreeNode>());
        EXPECT_THROW(a.getAs<sparta::ParameterBase>());

        // Test getChild on non-existant nodes w/ must_exist as both true and false
        EXPECT_THROW(top.getChild("no.there.is.no.node.by.this.name")); //must_exist=true
        EXPECT_FALSE(top.getChild("no.there.is.no.node.by.this.name", false)); // must_exist=false

        // Do NOT use index operator on children. STL optimized has no bounds
        // checking with operator[].
        //EXPECT_THROW(top.getChildren()[1]);
        //EXPECT_THROW(a.getChildren()[1]);
        //EXPECT_THROW(b.getChildren()[0]);

        EXPECT_TRUE(a.getChildren().at(0)->isBuiltin());
        EXPECT_TRUE(a.getChildren()[1] == &b);
        EXPECT_TRUE(a.getChildren()[2] == &b1);
        EXPECT_TRUE(b.getChildren().size() == 1);
        EXPECT_TRUE(top.getNumChildren() == 2); // Includes added 'descenant_attached' notification
        EXPECT_TRUE(cant_see_me.getNumChildren() == 3);
        EXPECT_TRUE(a.getNumChildren() == 3);
        EXPECT_TRUE(b.getNumChildren() == 1);
        EXPECT_NOTHROW(top.getChildAt(0));
        EXPECT_NOTHROW(a.getChildAt(0));
        EXPECT_NOTHROW(a.getChildAt(1));
        EXPECT_NOTHROW(a.getChildAt(2));
        EXPECT_NOTHROW(top.getChildAt(1)); // Ok. top has 2 children because of 'a' and 'descendant_attached'
        EXPECT_THROW(top.getChildAt(3)); // ERROR: No child here
        EXPECT_THROW(a.getChildAt(3)); // ERROR: No child here
        EXPECT_NOTHROW(b.getChildAt(0));
        EXPECT_THROW(b.getChildAt(1)); // ERROR: No children
        EXPECT_TRUE(top.hasImmediateChild(&a));
        EXPECT_TRUE(a.getChildren().at(0)->isBuiltin());
        EXPECT_TRUE(a.getChildAt(1) == &b);
        EXPECT_TRUE(a.getChildAt(2) == &b1);
        EXPECT_TRUE(top.getLocation() == "top");
        EXPECT_TRUE(a.getLocation() == "top.a");
        EXPECT_TRUE(b.getLocation() == "top.a.b");
        EXPECT_TRUE(b1.getLocation() == "top.a.b1");
        EXPECT_TRUE(top.getDisplayLocation() == "top");
        EXPECT_TRUE(a.getDisplayLocation() == "top.a");
        EXPECT_TRUE(b.getDisplayLocation() == "top.a.b");
        EXPECT_TRUE(b1.getDisplayLocation() == "top.a.b1");
        EXPECT_TRUE(top.isAttached());
        EXPECT_TRUE(a.isAttached());
        EXPECT_TRUE(b.isAttached());
        EXPECT_TRUE(b1.isAttached());

        EXPECT_NOTHROW(top.getChild("a"));
        EXPECT_NOTHROW(top.getChild("a.b"));
        EXPECT_NOTHROW(a.getChild("b"));
        //std::vector<TreeNode*> vec;
        //EXPECT_NOTHROW(top.findChildren("a.*", vec));



        // Find by pattern
        // (More search testing in sparta/test/Params)
        std::vector<sparta::TreeNode*> found;

        // Find immediate child
        found.clear();
        EXPECT_EQUAL((top.findChildren("a", found)), (uint32_t)1);
        EXPECT_EQUAL(found.size(), (size_t)1);
        EXPECT_NOTHROW( EXPECT_TRUE(found.at(0) == &a) );
        std::cout << "result of search for \"a\": " << found << std::endl << std::endl;

        // Should only find the public children.
        found.clear();
        EXPECT_EQUAL(top.findChildren("*", found), (uint32_t)2);

        // Find immediate parent
        found.clear();
        EXPECT_EQUAL((b.findChildren(".", found)), (uint32_t)1);
        EXPECT_EQUAL(found.size(), (size_t)1);
        EXPECT_NOTHROW( EXPECT_TRUE(found.at(0) == &a) );
        std::cout << "result of search for \".\": " << found << std::endl << std::endl;

        // Find by alias
        found.clear();
        EXPECT_EQUAL((top.findChildren("a.b_one", found)), (uint32_t)1);
        EXPECT_EQUAL(found.size(), (size_t)1);
        EXPECT_NOTHROW( EXPECT_TRUE(found.at(0) == &b1) );
        std::cout << "result of search for \"a.b_one\": " << found << std::endl << std::endl;

        // Find ancestor, then immediate child
        found.clear();
        EXPECT_EQUAL((b1.findChildren("..a", found)), (uint32_t)1);
        EXPECT_EQUAL(found.size(), (size_t)1);
        std::cout << "result of search for \"..a\": " << found << std::endl << std::endl;

        // Find ancestor, then deep child
        found.clear();
        EXPECT_EQUAL((b1.findChildren("..a.b1.params.param0", found)), (uint32_t)1);
        EXPECT_EQUAL(found.size(), (size_t)1);
        std::cout << "result of search for \"..a.b1.params.param0\": " << found << std::endl << std::endl;

        // Find ancestor, then deep child, then ancestor
        found.clear();
        EXPECT_EQUAL((b1.findChildren("..a.b1.params.param0....b", found)), (uint32_t)1);
        EXPECT_EQUAL(found.size(), (size_t)1);
        EXPECT_NOTHROW( EXPECT_TRUE(found.at(0) == &b) );
        std::cout << "result of search for \"..a.b1.params.param0....b\": " << found << std::endl << std::endl;

        // Find ancestor, then deep child, immediate ancestor
        found.clear();
        EXPECT_EQUAL((b1.findChildren("..a.b1.params.param0..", found)), (uint32_t)1);
        EXPECT_EQUAL(found.size(), (size_t)1);
        EXPECT_NOTHROW( EXPECT_TRUE(found.at(0) == (sparta::TreeNode*)b1.getParameterSet()) );
        std::cout << "result of search for \"..a.b1.params.param0..\": " << found << std::endl << std::endl;

        // Find all children
        found.clear();
        EXPECT_EQUAL((a.findChildren("*", found)), (uint32_t)3);
        EXPECT_EQUAL(found.size(), (size_t)3);
        EXPECT_TRUE(std::find(found.begin(), found.end(), &b) != found.end());
        EXPECT_TRUE(std::find(found.begin(), found.end(), &b1) != found.end()); // Should be found twice
        EXPECT_TRUE(std::find(found.begin(), found.end(), (sparta::TreeNode*)a.getParameterSet()) != found.end());
        std::cout << "result of search for \"*\": " << found << std::endl;
        for(sparta::TreeNode* tn : found){
            std::cout << tn << std::endl;
        }

        // Find specific children
        found.clear();
        // Should find b1.params twice (once through b1 and once through aliase b_one
        EXPECT_EQUAL((top.findChildren("a.b+.par*", found)), (uint32_t)2);
        EXPECT_EQUAL(found.size(), (size_t)2);
        EXPECT_NOTHROW( EXPECT_TRUE(found.at(0) == (sparta::TreeNode*)b1.getParameterSet()) );
        EXPECT_NOTHROW( EXPECT_TRUE(found.at(0) == (sparta::TreeNode*)b1.getParameterSet()) );
        std::cout << "result of search for \"a.b+.par\": " << found << std::endl << std::endl;


        EXPECT_THROW(top.enterFinalized()); // Cannot skip configuration stage

        EXPECT_FALSE(top.isFinalized());
        EXPECT_FALSE(a.isFinalized());
        EXPECT_FALSE(b.isFinalized());
        EXPECT_FALSE(b1.isFinalized());

        // Tree building complete.
        // Enter configuration phase

        EXPECT_NOTHROW(top.enterConfiguring());
        EXPECT_EQUAL(top.isBuilt(), true);
        EXPECT_EQUAL(top.isConfigured(), false);
        EXPECT_EQUAL(top.isFinalizing(), false);
        EXPECT_EQUAL(top.isFinalized(), false);
        EXPECT_NOTHROW(b1.getChild("leaf"));

        EXPECT_TRUE(top.isBuilt());
        EXPECT_TRUE(a.isBuilt());
        EXPECT_TRUE(b.isBuilt());
        EXPECT_TRUE(b1.isBuilt());

        // Configure Clocks

        a.setClock(&clk);
        cant_see_me.setClock(&clk);
        b.setClock(&clk);
        b1.setClock(&clk);

        EXPECT_THROW(a.setClock(&clk)); // ERROR: Already has a clock
        EXPECT_THROW(b.setClock(&clk)); // ERROR: Already has a clock


        // Consume Parameters from Config File(s)

        sparta::ConfigParser::YAML param_file("test.json", {"./"}); // Load (#includes are deferred)
        EXPECT_NOTHROW(param_file.consumeParameters(&top, false)); // Apply


        // Configure Params Manually

        sparta::ParameterSet* a_params = a.getParameterSet();
        EXPECT_TRUE(a_params != 0);
        sparta::ParameterSet* b_params = b.getParameterSet();
        EXPECT_TRUE(b_params != 0);

        SimpleDevice::ParameterSet* a_sps = dynamic_cast<SimpleDevice::ParameterSet*>(a_params);
        EXPECT_TRUE(a_sps != 0);
        SimpleDevice::ParameterSet* b_sps = dynamic_cast<SimpleDevice::ParameterSet*>(b_params);
        EXPECT_TRUE(b_sps != 0);


        // Configure some params manually
        // (naming convention "manual_" is just a hint for this test)

        std::vector<uint32_t> v1 = a_sps->manual_uint32vec;
        v1.clear();
        v1 << 1 << 2 << 3; // someday: = {1,2,3};
        a_sps->manual_uint32vec = v1; // Write after read

        std::vector<std::string> v2 = a_sps->manual_strvec;
        v2.clear();
        v2 << "a" << "b" << "c"; // someday: = {"1", "2", "3"};
        a_sps->manual_strvec = v2;

        a_sps->manual_int32_neg = -1;
        a_sps->manual_int32_pos = 1;
        a_sps->manual_double = 1.0;
        a_sps->manual_bool = true;
        a_sps->manual_str = "set";

        // Perform some manual validation

        // Pretend to be a Unit consuming const parameters
        const SimpleDevice::ParameterSet* a_sps_cv = dynamic_cast<SimpleDevice::ParameterSet*>(a_params);

        EXPECT_TRUE(a_sps_cv->param1 == 12);
        EXPECT_TRUE(a_sps_cv->param2 == 34);
        std::vector<uint32_t> atv1; atv1 << 5 << 6 << 7 << 8;
        EXPECT_TRUE(a_sps_cv->param3 == atv1);
        std::vector<std::string> atv2; atv2 << "e" << "eff" << "gee" << "h";
        EXPECT_TRUE(a_sps_cv->param4 == atv2);
        std::vector<double> atv3; atv3 << 1.0 << 1.1 << 2 << 3 << 5.5;
        std::cout << a_sps_cv->param5 << " VS " << atv3 << std::endl;
        EXPECT_TRUE(a_sps_cv->param5 == atv3);
        EXPECT_TRUE(a_sps_cv->param6 == 0xdeadbeef);
        EXPECT_TRUE(a_sps_cv->param6.getValueAsString() == "0xdeadbeef"); // from setNumericDisplayBase
        EXPECT_TRUE(a_sps_cv->param7 == 070);
        EXPECT_TRUE(a_sps_cv->param7.getValueAsString() == "070"); // from setNumericDisplayBase
        std::vector<uint32_t> atv4; atv4 << 0xa1 << 0xb2 << 0xc3;
        std::cout << a_sps_cv->param8 << " VS " << atv4 << std::endl;
        EXPECT_TRUE(a_sps_cv->param8 == atv4);
        EXPECT_TRUE(a_sps_cv->param9 == "string with spaces");
        EXPECT_EQUAL(a_sps_cv->param10, 0xc001);
        EXPECT_TRUE(a_sps_cv->param11 == 0xbad);
        EXPECT_TRUE(a_sps_cv->param12 == -4003002001);
        EXPECT_TRUE(a_sps_cv->param13 == -6005004003002001);
        EXPECT_TRUE(a_sps_cv->similar == 0x5000);
        EXPECT_TRUE(a_sps_cv->similar00 == 0x5000);
        EXPECT_TRUE(a_sps_cv->similar01 == 0x5000);
        EXPECT_TRUE(a_sps_cv->similar02 == 0x5000);
        EXPECT_TRUE(a_sps_cv->similar10 == 0x5000);
        EXPECT_TRUE(a_sps_cv->similar11 == 0x5000);
        EXPECT_TRUE(a_sps_cv->similar12 == 0x5000);

        std::vector<uint32_t> atv5; atv5 << 1 << 2 << 3;
        EXPECT_TRUE(a_sps_cv->manual_uint32vec == atv5);
        std::vector<std::string> atv6; atv6 << "a" << "b" << "c";
        EXPECT_TRUE(a_sps_cv->manual_strvec == atv6);
        EXPECT_TRUE(a_sps_cv->manual_int32_neg == -1);
        EXPECT_TRUE(a_sps_cv->manual_int32_pos == 1);
        EXPECT_TRUE(a_sps_cv->manual_double == 1.0);
        EXPECT_TRUE(a_sps_cv->manual_bool == true);
        EXPECT_TRUE(a_sps_cv->manual_str == "set");

        EXPECT_EQUAL(((decltype(a_sps_cv->strvecvec)::value_type)a_sps_cv->strvecvec).size(), 3);
        EXPECT_EQUAL(a_sps_cv->strvecvec,
                     decltype(a_sps_cv->strvecvec)::value_type({{"a", "hey", "there"},
                                                                {"b", "friend"},
                                                                {"c"}}));

        EXPECT_EQUAL(((decltype(a_sps_cv->strvecvecvec)::value_type)a_sps_cv->strvecvecvec).size(), 5);
        std::cout << a_sps_cv->strvecvecvec.getValueAsString() << std::endl;
        std::vector<std::vector<std::vector<std::string>>> other_3d_vec({{{"a"}, {"b"}, {"c"}},
                                                                   {{"d", "e", "f"}},
                                                                   {{"g"}, {"h", "i"}},
                                                                   {{}},
                                                                   {{"j"}}
                                                                   });
        std::cout << other_3d_vec << std::endl;
        EXPECT_EQUAL(a_sps_cv->strvecvecvec, other_3d_vec);

        const SimpleDevice::ParameterSet* b_sps_cv = dynamic_cast<SimpleDevice::ParameterSet*>(b_params);

        EXPECT_TRUE(b_sps_cv->param1 == 56);
        EXPECT_TRUE(b_sps_cv->param2 == 78);
        std::vector<uint32_t> btv1; btv1 << 1 << 2 << 3 << 4;
        EXPECT_TRUE(b_sps_cv->param3 == btv1);
        std::vector<std::string> btv2; btv2 << "a" << "b" << "cee" << "dee";
        EXPECT_TRUE(b_sps_cv->param4 == btv2);

        std::vector<double> btv3;
        EXPECT_TRUE(b_sps_cv->param5 == btv3);
        EXPECT_TRUE(b_sps_cv->param6 == 0);

        EXPECT_EQUAL(b_sps_cv->param10, 0xa1);
        EXPECT_EQUAL(b_sps_cv->param11, 0xc001);
        EXPECT_TRUE(b_sps_cv->param12 == -4003002001);
        EXPECT_TRUE(b_sps_cv->param13 == -6005004003002001);
        EXPECT_TRUE(b_sps_cv->similar == 4003002001);
        EXPECT_TRUE(b_sps_cv->similar00 == 0x50);
        EXPECT_TRUE(b_sps_cv->similar01 == 0x50);
        EXPECT_TRUE(b_sps_cv->similar02 == 0x50);
        EXPECT_TRUE(b_sps_cv->similar10 == 0x51);
        EXPECT_TRUE(b_sps_cv->similar11 == 0x51);
        EXPECT_TRUE(b_sps_cv->similar12 == 0x51);

        EXPECT_TRUE(b_sps_cv->manual_uint32vec == (std::vector<uint32_t>()));
        EXPECT_TRUE(b_sps_cv->manual_strvec == (std::vector<std::string>()));
        EXPECT_TRUE(b_sps_cv->manual_int32_neg == 0);
        EXPECT_TRUE(b_sps_cv->manual_int32_pos == 0);
        EXPECT_TRUE(b_sps_cv->manual_double == 0);
        EXPECT_TRUE(b_sps_cv->manual_bool == false);
        EXPECT_TRUE(b_sps_cv->manual_str == "default string");


        const SimpleDevice::ParameterSet* b1_sps_cv = dynamic_cast<SimpleDevice::ParameterSet*>(b1.getParameterSet());
        EXPECT_EQUAL(b1_sps_cv->similar00, 0x50);
        EXPECT_EQUAL(b1_sps_cv->similar01, 0x50);
        EXPECT_EQUAL(b1_sps_cv->similar02, 0x50);
        EXPECT_EQUAL(b1_sps_cv->similar10, 0x10);
        EXPECT_EQUAL(b1_sps_cv->similar11, 0x11);
        EXPECT_EQUAL(b1_sps_cv->similar12, 0x12);

        // Resource Factory Checks

        EXPECT_TRUE(a.getResourceType() == "SimpleDevice");
        EXPECT_TRUE(b.getResourceType() == "ResourceWithDynamicChildren");


        // Printing TreeNodes and Parameters

        std::cout << top << std::endl;
        std::cout << a << std::endl;
        std::cout << b << std::endl;

        std::cout << &top << std::endl;
        std::cout << &a << std::endl;
        std::cout << &b << std::endl;

        // Print as reference.
        std::cout << a.getParameterSet() << std::endl;
        std::cout << b.getParameterSet() << std::endl;


        // Show the Parameters and read counts
        // Also print as ParameterSet* instead of ref.
        std::cout << *a.getParameterSet() << std::endl;
        std::cout << *b.getParameterSet() << std::endl;

        // Print out child identifiers for a few levels

        std::vector<std::string> idents2;
        top.getChildrenIdentifiers(idents2);
        std::cout << "Children+Aliases of top: " << idents2 << std::endl;

        a.getChildrenIdentifiers(idents2);
        std::cout << "Children+Aliases of a: " << idents2 << std::endl;

        a.getParameterSet()->getChildrenIdentifiers(idents2);
        std::cout << "Children+Aliases of a.params: " << idents2 << std::endl;

        b.getChildrenIdentifiers(idents2);
        std::cout << "Children+Aliases of b: " << idents2 << std::endl;

        b.getParameterSet()->getChildrenIdentifiers(idents2);
        std::cout << "Children+Aliases of b.params: " << idents2 << std::endl;



        // Store and Reload the parameter tree many times then diff the result.
        // YAML reads and writes should be stable once they've passed through the
        // system. We're looking for things like dropped nodes, float/double
        // encoding issues, and anything else.

        const char* filename_orig = "dummy.yaml.orig";
        const char* filename_new = "dummy.yaml.new";

        // Inital number of reads
        const uint32_t num_reads = a_sps_cv->strvecvecvec.getReadCount();

        // Store Parameter Tree in file first. Compare with this later
        sparta::ConfigEmitter::YAML param_out(filename_orig,
                                            true); // verbose

        EXPECT_NOTHROW(param_out.addParameters(&top, nullptr, true)); // verbose

        // Ensure taht the read count on this crazy parameter has not changed
        // when emitting the YAML
        EXPECT_EQUAL(num_reads, a_sps_cv->strvecvecvec.getReadCount());

        // Reset read counts on all parameters
        std::function<void (sparta::TreeNode*)> recurs_reset = [&](sparta::TreeNode* n) -> void {
            try{
                n->getAs<sparta::ParameterSet>()->resetWriteCounts();
                n->getAs<sparta::ParameterSet>()->resetReadCounts();
            }catch(...){
                // Done
            }
            for(auto child : n->getChildren()){
                recurs_reset(child);
            }
        };

        // Write terse parameter file
        for(uint32_t i=0; i<20; ++i){

            // Store Parameter Tree
            sparta::ConfigEmitter::YAML param_out(filename_new,
                                                false); // Terse
            EXPECT_NOTHROW(param_out.addParameters(&top, nullptr, false));

            // Reset read counts to write them again
            recurs_reset(&top);

            // Reload Stored Parameter Tree
            sparta::ConfigParser::YAML param_in(filename_new, {});
            EXPECT_NOTHROW(param_in.consumeParameters(&top, false));
        }

        // Write verbose parameter file
        for(uint32_t i=0; i<20; ++i){

            // Store Parameter Tree
            sparta::ConfigEmitter::YAML param_out(filename_new,
                                                true); // Verbose
            EXPECT_NOTHROW(param_out.addParameters(&top, nullptr, false));

            // Reset read counts to write them again
            recurs_reset(&top);

            // Reload Stored Parameter Tree
            sparta::ConfigParser::YAML param_in(filename_new, {});
            EXPECT_NOTHROW(param_in.consumeParameters(&top, false));
        }

        // Compare files

        EXPECT_FILES_EQUAL(filename_orig, filename_new);


        // Finalize Tree (no more configuration)

        EXPECT_NOTHROW(top.enterFinalized());


        // No longer valid because of changes in TreeNode::addTag
        //EXPECT_THROW(top.addTag("uniqe_tag_name"));
        //EXPECT_THROW(a.addTag("unique_tag_name"));
        //EXPECT_THROW(b.addTag("unique_tag_name"));

        EXPECT_EQUAL(top.isBuilt(), true);
        EXPECT_EQUAL(top.isConfigured(), true);
        EXPECT_EQUAL(top.isFinalizing(), false);
        EXPECT_EQUAL(top.isFinalized(), true);
        // We have 2 public, and 2 private children
        // If you are confused why two, the extra nodes come from the notification sources
        // added to the resource.
        EXPECT_EQUAL(top.getChildren().size(), 2);
        EXPECT_TRUE(cant_see_me.getNumChildren() == 3);
        found.clear();
        EXPECT_EQUAL(cant_see_me.findChildren("*", found), 3);
        found.clear();
        EXPECT_EQUAL(top.findChildren("cant_see_me.a_private", found), 0);
        found.clear();
        EXPECT_EQUAL(top.findChildrenByTag("a_private_tag", found), 0);
        found.clear();
        EXPECT_EQUAL(top.findChildren("*", found), (uint32_t)2);
        EXPECT_EQUAL(top.findChildren("*.params", found), (uint32_t)1);
        EXPECT_NOTHROW(b1.getChild("leaf"));
        EXPECT_EQUAL(b1.getChild("leaf")->isFinalized(), true);

        sparta::Resource* res = nullptr;
        EXPECT_NOTHROW(res = a.getResourceAs<SimpleDevice>());
        EXPECT_THROW( EXPECT_EQUAL(a.getResourceAs<DynResource>(), nullptr) ); // Wrong type
        EXPECT_THROW( EXPECT_EQUAL(a.getResourceAs<ResourceWithDynamicChildren>(), nullptr) ); // Wrong type
        EXPECT_NOTHROW( EXPECT_EQUAL(a.getResourceAs<SimpleDevice>(), res) );
        EXPECT_NOTHROW( EXPECT_EQUAL(a.getResourceAs<SimpleDevice*>(), res) );
        EXPECT_NOTHROW( EXPECT_EQUAL(((const sparta::TreeNode&)a).getResourceAs<SimpleDevice>(), res) );
        EXPECT_NOTHROW( EXPECT_EQUAL(((const sparta::TreeNode&)a).getResourceAs<SimpleDevice*>(), res) );
        EXPECT_NOTHROW( EXPECT_EQUAL(a.getAs<sparta::ResourceTreeNode>()->getResource(), res); )
        EXPECT_NOTHROW( EXPECT_EQUAL(((const sparta::TreeNode&)a).getAs<sparta::ResourceTreeNode>()->getResource(), res) );
        EXPECT_NOTHROW( EXPECT_EQUAL(a.getAs<sparta::ResourceTreeNode>()->getResourceAs<SimpleDevice>(), res) );
        EXPECT_NOTHROW( EXPECT_EQUAL(a.getResource(), res) );
        EXPECT_NOTHROW( EXPECT_EQUAL(a.getResourceAs<SimpleDevice>(), res) );

        EXPECT_THROW(top.enterFinalized()); // ERROR: Already finalized
        EXPECT_THROW(top.enterConfiguring()); // ERROR: Already finalized

        EXPECT_TRUE(top.isFinalized());
        EXPECT_TRUE(a.isFinalized());
        EXPECT_TRUE(b.isFinalized());
        EXPECT_TRUE(b1.isFinalized());

        // Show dynamically reated children (in Resource constructors)

        EXPECT_NOTHROW(b.getChild("child")); // dynamically created in resource ctor during finalization
        EXPECT_NOTEQUAL(b.getChild("child"), nullptr);
        EXPECT_NOTHROW( EXPECT_NOTEQUAL(b.getChild("child")->getResource(), nullptr) );

        EXPECT_NOTHROW(b.getChild("child2")); // dynamically created in resource ctor during finalization
        EXPECT_NOTEQUAL(b.getChild("child2"), nullptr);
        EXPECT_NOTHROW( EXPECT_NOTEQUAL(b.getChild("child2")->getResource(), nullptr) );

        EXPECT_NOTHROW(b.getChild("child3")); // dynamically created in resource ctor during finalization
        EXPECT_NOTEQUAL(b.getChild("child3"), nullptr);
        EXPECT_NOTHROW( EXPECT_NOTEQUAL(b.getChild("child3")->getResource(), nullptr) ); // Ensure resource was created by the framework later

        EXPECT_NOTHROW(b.getChild("child4")); // dynamically created in resource ctor during finalization
        EXPECT_NOTEQUAL(b.getChild("child4"), nullptr);
        EXPECT_NOTHROW( EXPECT_NOTEQUAL(b.getChild("child4")->getResource(), nullptr) );

        // Print out the tree at different levels with different options

        std::cout << "The tree from the top: " << std::endl << top.renderSubtree() << std::endl;
        std::cout << "The tree from a (max_depth=2): " << std::endl << a.renderSubtree(2) << std::endl;
        std::cout << "The tree from a (max_depth=0): " << std::endl << a.renderSubtree(0) << std::endl;
        std::cout << "The tree from the top (with builtin groups): " << std::endl << top.renderSubtree(-1, true) << std::endl;


        std::cout << "StringManager content ("
                  << sparta::StringManager::getStringManager().getNumStrings()
                  << " strings):" << std::endl;
        sparta::StringManager::getStringManager().dumpStrings(std::cout);

        ENSURE_ALL_REACHED(0); // None before teardown
        EXPECT_EQUAL(SimpleDevice::num_simpledevices_torn_down, 0);

        EXPECT_NOTHROW(top.enterTeardown());

        ENSURE_ALL_REACHED(1); // Resource::onStartingTeardown_
        EXPECT_EQUAL(SimpleDevice::num_simpledevices_torn_down, 4);

        EXPECT_EQUAL(top.isBuilt(), true);
        EXPECT_EQUAL(top.isConfigured(), true);
        EXPECT_EQUAL(top.isFinalizing(), false);
        EXPECT_EQUAL(top.isFinalized(), false);
        EXPECT_EQUAL(top.isTearingDown(), true);

        EXPECT_NOTHROW(b1.getChild("leaf"));
        EXPECT_EQUAL(b1.getChild("leaf")->isFinalized(), false);
        EXPECT_EQUAL(b1.getChild("leaf")->isTearingDown(), true);
    }

    // Diagnostic printing of all unfreed TreeNodes. A few are expected
    std::cout << "\nUnfreed TreeNodes (some globals expected)" << std::endl;
    std::cout << sparta::TreeNode::formatAllNodes() << std::endl;

    // Done

    REPORT_ERROR;

    return ERROR_CODE;
}
