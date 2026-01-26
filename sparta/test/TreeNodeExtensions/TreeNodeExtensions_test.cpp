#include "sparta/sparta.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/TreeNodeExtensions.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/kernel/SleeperThread.hpp"
#include "sparta/app/Simulation.hpp"
#include "sparta/app/CommandLineSimulator.hpp"
#include "sparta/utils/SpartaTester.hpp"

#include <boost/algorithm/string.hpp>

/*!
 * \file TreeNodeExtensions_test.cpp
 * \brief Tests for TreeNode extensions:
 *  - No simulation, just TreeNode's.
 *  - Simulation, but no CommandLineSimulator.
 *  - Simulation with CommandLineSimulator.
 *  - Backwards compatibility test with factory registration in buildTree_().
 */

TEST_INIT

// User-defined tree node extension class. YAML extension file
// provides "color" and "shape" parameters, e.g. "green circle",
// "blue square", and "black diamond". Also has a YAML parameter
// "trail_name". The last parameter "trail_closed" is not given
// in the YAML file, but is added to the extension's parameter
// set when the extension is created.
class SkiTrailExtension : public sparta::ExtensionsParamsOnly
{
public:
    static constexpr auto NAME = "ski_trail";

    std::string getClassName() const override
    {
        return NAME;
    }

private:
    // Extra parameter added to the ParameterSet that is not
    // provided in the YAML extension file.
    std::unique_ptr<sparta::Parameter<bool>> trail_closed_;

    // The base class will clobber together whatever parameter values it
    // found in the yaml file, and give us a chance to add custom parameters
    // to the same set.
    void postCreate() override
    {
        auto param_set = getParameters();
        trail_closed_ = std::make_unique<sparta::Parameter<bool>>(
            "trail_closed", false, "Is this trail closed to the public right now?", param_set);
    }
};

REGISTER_TREE_NODE_EXTENSION(SkiTrailExtension);

// Most of the tests will use the "global_meta" extension
// without a factory to ensure that no-factory use cases
// work as designed. There is a backwards compatibility
// test that will register a factory for this extension
// after the ExtensionsParamsOnly has already been created
// to ensure that legacy use cases work, where factories
// are commonly registered in buildTree_() since the macro
// did not exist in the first implementation.
class GlobalMetadata : public sparta::ExtensionsParamsOnly
{
public:
    // Do not provide constexpr NAME, as it was not required
    // for legacy use.

    // Do not provide getClassName(), as it was not required
    // for legacy use.

private:
    void postCreate() override
    {
        auto ps = getParameters();

        // Add all supported data types, both scalar / vector / nested vector.
        int_scalar_ = std::make_unique<sparta::Parameter<int>>(
            "int_scalar", 0, "An integer scalar parameter", ps);
        int_vector_ = std::make_unique<sparta::Parameter<std::vector<int>>>(
            "int_vector", std::vector<int>{}, "An integer vector parameter", ps);
        neg_int_scalar_ = std::make_unique<sparta::Parameter<int>>(
            "neg_int_scalar", 0, "A negative integer scalar parameter", ps);
        neg_int_vector_ = std::make_unique<sparta::Parameter<std::vector<int>>>(
            "neg_int_vector", std::vector<int>{}, "A negative integer vector parameter", ps);
        double_scalar_ = std::make_unique<sparta::Parameter<double>>(
            "double_scalar", 0.0, "A double scalar parameter", ps);
        double_vector_ = std::make_unique<sparta::Parameter<std::vector<double>>>(
            "double_vector", std::vector<double>{}, "A double vector parameter", ps);
        string_scalar_ = std::make_unique<sparta::Parameter<std::string>>(
            "string_scalar", "", "A string scalar parameter", ps);
        string_vector_ = std::make_unique<sparta::Parameter<std::vector<std::string>>>(
            "string_vector", std::vector<std::string>{}, "A string vector parameter", ps);
        hex_scalar_ = std::make_unique<sparta::Parameter<int>>(
            "hex_scalar", 0, "A hexadecimal scalar parameter", ps);
        hex_vector_ = std::make_unique<sparta::Parameter<std::vector<int>>>(
            "hex_vector", std::vector<int>{}, "A hexadecimal vector parameter", ps);
        string_nested_vectors_ = std::make_unique<sparta::Parameter<std::vector<std::vector<std::string>>>>(
            "string_nested_vectors", std::vector<std::vector<std::string>>{},
            "A nested vector of strings parameter", ps);
        int_nested_vectors_ = std::make_unique<sparta::Parameter<std::vector<std::vector<uint32_t>>>>(
            "int_nested_vectors", std::vector<std::vector<uint32_t>>{},
            "A nested vector of integers parameter", ps);
    }

    std::unique_ptr<sparta::Parameter<int>> int_scalar_;
    std::unique_ptr<sparta::Parameter<std::vector<int>>> int_vector_;
    std::unique_ptr<sparta::Parameter<int>> neg_int_scalar_;
    std::unique_ptr<sparta::Parameter<std::vector<int>>> neg_int_vector_;
    std::unique_ptr<sparta::Parameter<double>> double_scalar_;
    std::unique_ptr<sparta::Parameter<std::vector<double>>> double_vector_;
    std::unique_ptr<sparta::Parameter<std::string>> string_scalar_;
    std::unique_ptr<sparta::Parameter<std::vector<std::string>>> string_vector_;
    std::unique_ptr<sparta::Parameter<int>> hex_scalar_;
    std::unique_ptr<sparta::Parameter<std::vector<int>>> hex_vector_;
    std::unique_ptr<sparta::Parameter<std::vector<std::vector<std::string>>>> string_nested_vectors_;
    std::unique_ptr<sparta::Parameter<std::vector<std::vector<uint32_t>>>> int_nested_vectors_;
};

class TestTree
{
public:
    TestTree()
        : top_("top")
        , node1_(&top_, "node1", "node1")
        , node2_(&node1_, "node2", "node2")
        , node3_(&node2_, "node3", "node3")
        , node4_(&node3_, "node4", "node4")
    {}

    sparta::RootTreeNode * getRoot()
    {
        return &top_;
    }

    ~TestTree()
    {
        top_.enterTeardown();
    }

private:
    sparta::RootTreeNode top_;
    sparta::TreeNode node1_;
    sparta::TreeNode node2_;
    sparta::TreeNode node3_;
    sparta::TreeNode node4_;
};

class TestSimulator : public sparta::app::Simulation
{
public:
    TestSimulator(sparta::Scheduler & sched, bool check_legacy_use = false)
        : sparta::app::Simulation("TestExtensionsSim", &sched)
        , check_legacy_use_(check_legacy_use)
    {}

    ~TestSimulator()
    {
        getRoot()->enterTeardown();
    }

private:
    void buildTree_() override
    {
        auto node1 = new sparta::TreeNode(getRoot(), "node1", "node1");
        to_free_.emplace_back(node1);

        auto node2 = new sparta::TreeNode(node1, "node2", "node2");
        to_free_.emplace_back(node2);

        auto node3 = new sparta::TreeNode(node2, "node3", "node3");
        to_free_.emplace_back(node3);

        auto node4 = new sparta::TreeNode(node3, "node4", "node4");
        to_free_.emplace_back(node4);

        // The new way to use extensions is to register factories with
        // the REGISTER_TREE_NODE_EXTENSION macro. This ensures that
        // factories can be used when we create extensions in the
        // Simulation::configure() / RootTreeNode::createExtensions()
        // methods.
        //
        // Though extensions have been redesigned, existing simulators
        // commonly register factories in buildTree_() instead, which
        // occurs after Simulation::configure(). To check backwards
        // compatibility, register a factory now for the "global_meta"
        // extension. The existing extension at this time is of final
        // class type ExtensionsParamsOnly. When we register the factory,
        // the existing ExtensionsParamsOnly extension will be automatically
        // replaced with a GlobalMetadata extension object.
        if (check_legacy_use_) {
            getRoot()->addExtensionFactory("global_meta", []() { return new GlobalMetadata; });
        }
    }

    void configureTree_() override
    {
        // Ensure factory registration is disallowed by now. Must be TREE_BUILDING.
        EXPECT_THROW(addTreeNodeExtensionFactory_("dummy", []() { return new SkiTrailExtension; }))
    }

    void bindTree_() override
    {
        // Ensure factory registration is disallowed by now. Must be TREE_BUILDING.
        EXPECT_THROW(addTreeNodeExtensionFactory_("dummy", []() { return new SkiTrailExtension; }))
    }

    // Miscellaneous nodes to free at destruction
    std::vector<std::unique_ptr<sparta::TreeNode>> to_free_;

    // Flag saying whether to check legacy use of extensions for backwards compatibility
    const bool check_legacy_use_;
};

// Common test function for all three use cases:
//   - No simulation, just TreeNode's.
//   - Simulation, but no CommandLineSimulator.
//   - Simulation with CommandLineSimulator.
void TestExtensions(sparta::RootTreeNode * top, bool cmdline_sim)
{
    // Create extensions from ski_trails.yaml for non-command line simulations.
    // CommandLineSimulator test already did this.
    if (!cmdline_sim) {
        top->createExtensions("ski_trails.yaml", {} /*no search paths*/, true /*verbose*/);
    }

    // Validate the extensions created from the YAML files.
    auto node1 = top->getChild("node1");
    auto node2 = node1->getChild("node2");
    auto node3 = node2->getChild("node3");

    auto top_ext = top->getExtension("ski_trail");
    auto node1_ext = node1->getExtension("ski_trail");
    auto node2_ext = node2->getExtension("ski_trail");
    auto node3_ext = node3->getExtension("ski_trail");

    // top, node1, and node2 should have extensions from ski_trails.yaml.
    EXPECT_NOTEQUAL(top_ext, nullptr);
    EXPECT_NOTEQUAL(node1_ext, nullptr);
    EXPECT_NOTEQUAL(node2_ext, nullptr);

    // node3 should NOT have an extension yet (not in ski_trails.yaml)
    // except for the last --node-config-file command-line sim test.
    // Check the SimulationConfiguration's ptree for "top.node1.node2.node3"
    // to discern this case.
    bool expect_node3 = false;
    if (cmdline_sim) {
        auto sim = top->getSimulation();
        sparta_assert(sim != nullptr);
        auto sim_cfg = sim->getSimulationConfiguration();
        sparta_assert(sim_cfg != nullptr);

        auto & ptree = sim_cfg->getExtensionsUnboundParameterTree();
        constexpr bool must_be_leaf = false;
        auto node3_ptree_node = ptree.tryGet("top.node1.node2.node3", must_be_leaf);
        if (node3_ptree_node) {
            expect_node3 = true;
        }
    }

    if (expect_node3) {
        EXPECT_NOTEQUAL(node3_ext, nullptr);
    } else {
        EXPECT_EQUAL(node3_ext, nullptr);
    }

    auto verif_ski_trail = [](sparta::TreeNode::ExtensionsBase * extension,
                              const std::string & expected_trail_name,
                              const std::string & expected_color,
                              const std::string & expected_shape)
    {
        const auto actual_trail_name = extension->getParameterValueAs<std::string>("trail_name");
        const auto actual_color = extension->getParameterValueAs<std::string>("color");
        const auto actual_shape = extension->getParameterValueAs<std::string>("shape");

        EXPECT_EQUAL(expected_trail_name, actual_trail_name);
        EXPECT_EQUAL(expected_color, actual_color);
        EXPECT_EQUAL(expected_shape, actual_shape);
    };

    verif_ski_trail(top_ext, "Fuddle Duddle", "green", "circle");
    verif_ski_trail(node1_ext, "Escapade", "blue", "square");
    verif_ski_trail(node2_ext, "Devil's River", "black", "diamond");

    // For the command-line sim test, the top node has two extensions already.
    // That means we cannot call getExtension() without an explicit extension
    // name.
    if (cmdline_sim) {
        EXPECT_THROW(top->getExtension());
    }

    // For non-command line sim tests, the top node only has one extension
    // so far from ski_trails.yaml, so we should be able to call getExtension()
    // without an explicit extension name.
    else {
        EXPECT_EQUAL(top->getExtension(), top_ext);
    }

    // For all tests, node1 and node2 only have one extension each from
    // ski_trails.yaml, so we can call getExtension() without an explicit
    // extension name for each call.
    EXPECT_EQUAL(node1->getExtension(), node1_ext);
    EXPECT_EQUAL(node2->getExtension(), node2_ext);

    // Now add extensions from global_meta.yaml for non-command line simulations.
    // CommandLineSimulator test already did this.
    if (!cmdline_sim) {
        top->createExtensions("global_meta.yaml", {} /*no search paths*/, true /*verbose*/);
    }

    // Check getNumExtensions()
    EXPECT_EQUAL(top->getNumExtensions(), 2);

    // Check getAllExtensionNames()
    auto ext_names = top->getAllExtensionNames();
    auto expected_ext_names = std::set<std::string>({"ski_trail", "global_meta"});
    EXPECT_TRUE(ext_names == expected_ext_names);

    // Since "top" now has two extensions, we cannot call getExtension()
    // without an extension name since it is ambiguous.
    EXPECT_THROW(top->getExtension());

    // Validate the "global_meta" extension on "top". This extension contains
    // all supported data types, both scalar and vector.
    auto top_global_meta_ext = top->getExtension("global_meta");
    EXPECT_NOTEQUAL(top_global_meta_ext, nullptr);

    auto verif_global_meta = [](sparta::TreeNode::ExtensionsBase * extension)
    {
        const uint64_t expected_int_scalar = 5;
        const uint64_t actual_int_scalar = extension->getParameterValueAs<uint64_t>("int_scalar");
        EXPECT_EQUAL(expected_int_scalar, actual_int_scalar);

        const std::vector<uint64_t> expected_int_vector = {1,2,3};
        const std::vector<uint64_t> actual_int_vector = extension->getParameterValueAs<std::vector<uint64_t>>("int_vector");
        EXPECT_EQUAL(expected_int_vector, actual_int_vector);

        const int32_t expected_neg_int_scalar = -4;
        const int32_t actual_neg_int_scalar = extension->getParameterValueAs<int32_t>("neg_int_scalar");
        EXPECT_EQUAL(expected_neg_int_scalar, actual_neg_int_scalar);

        const std::vector<int32_t> expected_neg_int_vector = {-1,-2,-3};
        const std::vector<int32_t> actual_neg_int_vector = extension->getParameterValueAs<std::vector<int32_t>>("neg_int_vector");
        EXPECT_EQUAL(expected_neg_int_vector, actual_neg_int_vector);

        const double expected_double_scalar = 6.7;
        const double actual_double_scalar = extension->getParameterValueAs<double>("double_scalar");
        EXPECT_EQUAL(expected_double_scalar, actual_double_scalar);

        const std::vector<double> expected_double_vector = {1.1, 2.2, 3.3};
        const std::vector<double> actual_double_vector = extension->getParameterValueAs<std::vector<double>>("double_vector");
        EXPECT_EQUAL(expected_double_vector, actual_double_vector);

        const std::string expected_string_scalar = "foobar";
        const std::string actual_string_scalar = extension->getParameterValueAs<std::string>("string_scalar");
        EXPECT_EQUAL(expected_string_scalar, actual_string_scalar);

        const std::vector<std::string> expected_string_vector = {"hello", "world"};
        const std::vector<std::string> actual_string_vector = extension->getParameterValueAs<std::vector<std::string>>("string_vector");
        EXPECT_EQUAL(expected_string_vector, actual_string_vector);

        const uint64_t expected_hex_scalar = 0x12345;
        const uint64_t actual_hex_scalar = extension->getParameterValueAs<uint64_t>("hex_scalar");
        EXPECT_EQUAL(expected_hex_scalar, actual_hex_scalar);

        const std::vector<uint64_t> expected_hex_vector = {0x1, 0x2, 0x3};
        const std::vector<uint64_t> actual_hex_vector = extension->getParameterValueAs<std::vector<uint64_t>>("hex_vector");
        EXPECT_EQUAL(expected_hex_vector, actual_hex_vector);

        const std::vector<std::vector<std::string>> expected_string_nested_vectors = {
            {"a", "b", "c"},
            {"d", "e", "f"}
        };
        const std::vector<std::vector<std::string>> actual_string_nested_vectors =
            extension->getParameterValueAs<std::vector<std::vector<std::string>>>("string_nested_vectors");
        EXPECT_EQUAL(expected_string_nested_vectors, actual_string_nested_vectors);

        const std::vector<std::vector<uint32_t>> expected_int_nested_vectors = {
            {1, 2, 3},
            {4, 5, 6}
        };
        const std::vector<std::vector<uint32_t>> actual_int_nested_vectors =
            extension->getParameterValueAs<std::vector<std::vector<uint32_t>>>("int_nested_vectors");
        EXPECT_EQUAL(expected_int_nested_vectors, actual_int_nested_vectors);
    };

    verif_global_meta(top_global_meta_ext);

    // Up to now, node3 does not have any extensions. Test on-demand extension creation
    // with a registered factory.
    node3_ext = node3->createExtension("ski_trail");

    // The created extension should be of type SkiTrailExtension with one parameter.
    // The SkiTrailExtension::postCreate() method adds the "trail_closed" parameter
    // manually.
    EXPECT_NOTEQUAL(dynamic_cast<SkiTrailExtension*>(node3_ext), nullptr);
    EXPECT_EQUAL(node3_ext->getParameters()->getNumParameters(), 1);
    EXPECT_EQUAL(node3_ext->getParameters()->getParameterValueAs<bool>("trail_closed"), false);

    // Now test on-demand extension creation without a registered factory.
    // Without a factory, this always returns an ExtensionsParamsOnly object.
    node3_ext = node3->createExtension("global_meta");
    EXPECT_NOTEQUAL(dynamic_cast<sparta::ExtensionsParamsOnly*>(node3_ext), nullptr);

    // Since top.node1.node2.node3 did not have a "global_meta" extension in any
    // YAML file, the created extension should have zero parameters. However, this
    // only applies to tests without a global_meta factory registered in buildTree_().
    if (dynamic_cast<GlobalMetadata*>(node3_ext) != nullptr) {
        // Legacy use case with factory registered in buildTree_().
        // The created extension should be of type GlobalMetadata
        // with all supported parameters.
        EXPECT_EQUAL(node3_ext->getParameters()->getNumParameters(), 12);
    } else {
        // Normal use case without factory registered. The created extension should
        // be of type ExtensionsParamsOnly with zero parameters.
        EXPECT_EQUAL(node3_ext->getParameters()->getNumParameters(), 0);
    }

    // Calling createExtension() again for the same extension name
    // should return the same extension object.
    EXPECT_EQUAL(node3->createExtension("global_meta"), node3_ext);

    // Calling createExtension() with replacement should return a new
    // extension object, also with zero parameters, except for the
    // legacy use case with factory registered in buildTree_().
    auto old_ext_uuid = node3_ext->getUUID();
    node3_ext = node3->createExtension("global_meta", true /*replace*/);
    auto new_ext_uuid = node3_ext->getUUID();
    EXPECT_NOTEQUAL(new_ext_uuid, old_ext_uuid);

    if (dynamic_cast<GlobalMetadata*>(node3_ext) != nullptr) {
        // Legacy use case with factory registered in buildTree_().
        // The created extension should be of type GlobalMetadata
        // with all supported parameters.
        EXPECT_EQUAL(node3_ext->getParameters()->getNumParameters(), 12);
    } else {
        // Normal use case without factory registered. The created extension should
        // be of type ExtensionsParamsOnly with zero parameters.
        EXPECT_EQUAL(node3_ext->getParameters()->getNumParameters(), 0);
    }

    // --node-config-file test: node4 should get its extension
    // from node4_config.yaml.
    auto node4 = node3->getChild("node4");
    auto node4_ext = node4->getExtension("node_config");
    if (node4_ext) {
        const auto param_a = node4_ext->getParameterValueAs<uint32_t>("param_a");
        const auto param_b = node4_ext->getParameterValueAs<std::string>("param_b");
        const auto param_c = node4_ext->getParameterValueAs<std::vector<uint32_t>>("param_c");

        EXPECT_EQUAL(param_a, 10);
        EXPECT_EQUAL(param_b, "foobar");
        EXPECT_EQUAL(param_c, std::vector<uint32_t>({4,5,6}));
    }
}

// Test: No simulation, just TreeNode's.
void TestExtensionsWithoutSim()
{
    TestTree tree;
    TestExtensions(tree.getRoot(), false /*Not a command-line sim*/);
}

// Test: Simulation, but no CommandLineSimulator.
void TestExtensionsWithStandaloneSim()
{
    sparta::Scheduler scheduler;
    TestSimulator sim(scheduler);

    // No CommandLineSimulator == no SimulationConfiguration.
    // Do not call Simulation::configure().
    sim.buildTree();
    sim.configureTree();
    sim.finalizeTree();
    sim.finalizeFramework();

    TestExtensions(sim.getRoot(), false /*Not a command-line sim*/);
}

// Helper to turn a single command line string into argc, argv
std::pair<int, char**> ParseArgs(const std::string & cmdline_args,
                                 std::vector<std::string> & args,
                                 std::vector<char*> & cargs)
{
    boost::split(args, cmdline_args, boost::is_any_of(" "));
    args.insert(args.begin(), "./TreeNodeExtensions_test");

    for (auto & arg : args) {
        char* s = const_cast<char*>(arg.c_str());
        cargs.push_back(s);
    }

    int argc = (int)cargs.size();
    char** argv = cargs.data();
    return {argc, argv};
}

// Helper to create a CommandLineSimulator
std::unique_ptr<sparta::app::CommandLineSimulator> CreateCommandLineSimulator(
    const int argc, char** argv)
{
    sparta::app::DefaultValues DEFAULTS;
    const char USAGE[] = "example usage";

    sparta::SimulationInfo::getInstance() =
        sparta::SimulationInfo("TreeNodeExtensions_test", argc, argv, "v0.0.0", "", {});

    auto cls = std::make_unique<sparta::app::CommandLineSimulator>(USAGE, DEFAULTS);

    // Parse command line options and configure simulator
    int err_code = 0;
    EXPECT_NOTHROW(EXPECT_EQUAL(cls->parse(argc, argv, err_code), true));

    return cls;
}

// Test: Simulation with CommandLineSimulator.
void TestExtensionsWithCommandLineSim(const std::string & cmdline_args)
{
    std::vector<std::string> args;
    std::vector<char*> cargs;
    auto [argc, argv] = ParseArgs(cmdline_args, args, cargs);
    auto cls = CreateCommandLineSimulator(argc, argv);

    // Create the simulator
    sparta::Scheduler scheduler;
    TestSimulator sim(scheduler);
    cls->populateSimulation(&sim);

    // Run tree node extensions test
    TestExtensions(sim.getRoot(), true /*command-line sim*/);
}

// Test: Backwards compatibility checks
void TestExtensionsWithLegacyUse(const std::string & cmdline_args)
{
    std::vector<std::string> args;
    std::vector<char*> cargs;
    auto [argc, argv] = ParseArgs(cmdline_args, args, cargs);
    auto cls = CreateCommandLineSimulator(argc, argv);

    // Create the simulator
    sparta::Scheduler scheduler;
    TestSimulator sim(scheduler, true /*check legacy use*/);
    cls->populateSimulation(&sim);

    // Run tree node extensions test
    TestExtensions(sim.getRoot(), true /*command-line sim*/);
}

int main(int argc, char** argv)
{
    sparta::SleeperThread::disableForever();

    // No simulator, just TreeNode's -------------------------------------------------
    TestExtensionsWithoutSim();

    // Simulator, but no CommandLineSimulator ----------------------------------------
    TestExtensionsWithStandaloneSim();

    // Simulator, with CommandLineSimulator ------------------------------------------
    TestExtensionsWithCommandLineSim(
        "--extension-file ski_trails.yaml --extension-file global_meta.yaml --write-final-config final.yaml");

    TestExtensionsWithCommandLineSim(
        "--config-file final.yaml --config-search-dir .");

    TestExtensionsWithCommandLineSim(
        "--arch ski_trails.yaml --arch-search-dir . --config-file global_meta.yaml --write-final-config final.yaml");

    TestExtensionsWithCommandLineSim(
        "--config-file final.yaml --arch-search-dir .");

    TestExtensionsWithCommandLineSim(
        "--arch ski_trails.yaml --arch-search-dir . --extension-file global_meta.yaml --write-final-config final.yaml");

    TestExtensionsWithCommandLineSim(
        "--config-file final.yaml --arch-search-dir .");

    TestExtensionsWithCommandLineSim(
        "--config-file ski_trails.yaml --config-search-dir . --extension-file global_meta.yaml --write-final-config final.yaml");

    TestExtensionsWithCommandLineSim(
        "--config-file final.yaml --config-search-dir .");

    TestExtensionsWithCommandLineSim(
        "--config-file ski_trails.yaml --config-file global_meta.yaml --config-search-dir . --write-final-config final.yaml");

    TestExtensionsWithCommandLineSim(
        "--config-file final.yaml --config-search-dir .");

    TestExtensionsWithCommandLineSim(
        "--config-file final.yaml --node-config-file top node4_config.yaml --config-search-dir . --write-final-config final.yaml");

    TestExtensionsWithCommandLineSim(
        "--config-file final.yaml --config-search-dir .");

    // Backwards compatibility checks ------------------------------------------------
    TestExtensionsWithLegacyUse(
        "--extension-file ski_trails.yaml --extension-file global_meta.yaml --write-final-config final.yaml");

    TestExtensionsWithLegacyUse(
        "--config-file final.yaml --config-search-dir .");

    REPORT_ERROR;
    return ERROR_CODE;
}
