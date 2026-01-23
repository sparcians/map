#include "sparta/sparta.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/TreeNodeExtensions.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/kernel/SleeperThread.hpp"
#include "sparta/app/Simulation.hpp"
#include "sparta/app/CommandLineSimulator.hpp"
#include "sparta/utils/SpartaTester.hpp"

#include <boost/algorithm/string/split.hpp>

/*!
 * \file TreeNodeExtensions_test.cpp
 * \brief Tests for TreeNode extensions:
 *  - No simulation, just TreeNode's.
 *  - Simulation, but no CommandLineSimulator.
 *  - Simulation with CommandLineSimulator.
 */

TEST_INIT

// User-defined tree node extension class. YAML extension file
// provides "color" and "shape" parameters, e.g. "green circle",
// "blue square", and "black diamond". Also has a YAML parameter
// "name" which is the name of the trail. The last parameter
// "trail_closed" is not given in the YAML file, but is added
// to the extension's parameter set when the extension is created.
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

class TestTree
{
public:
    TestTree()
        : top_("top")
        , node1_(&top_, "node1", "node1")
        , node2_(&node1_, "node2", "node2")
        , node3_(&node2_, "node3", "node3")
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
};

class TestSimulator : public sparta::app::Simulation
{
public:
    TestSimulator(sparta::Scheduler & sched)
        : sparta::app::Simulation("TestExtensionsSim", &sched)
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
    }

    void configureTree_() override
    {
        // Ensure factory registration is disallowed by now. Must be TREE_BUILDING.
        EXPECT_THROW(addTreeNodeExtensionFactory_("dummy", []() { return new SkiTrailExtension; }))

        // Unrequire all virtual parameters so we can get past exceptions from
        // Simulation::checkAllVirtualParamsRead_
        //
        // We are going to read all the parameters in the TestExtensions() method
        // but that occurs after CommandLineSimulator::populateSimulation() where
        // these exceptions come from.
        if (auto cfg = getSimulationConfiguration()) {
            cfg->getUnboundParameterTree().getRoot()->unrequire();
            cfg->getArchUnboundParameterTree().getRoot()->unrequire();
            cfg->getExtensionsUnboundParameterTree().getRoot()->unrequire();
        }
    }

    void bindTree_() override
    {
        // Ensure factory registration is disallowed by now. Must be TREE_BUILDING.
        EXPECT_THROW(addTreeNodeExtensionFactory_("dummy", []() { return new SkiTrailExtension; }))
    }

    // Miscellaneous nodes to free at destruction
    std::vector<std::unique_ptr<sparta::TreeNode>> to_free_;
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
        top->createExtensions("ski_trails.yaml", {}, true);
    }

    // Validate the extensions created from the YAML files. Note that we
    // call getExtension(name, false) so Sparta knows NOT to create the
    // extension if it doesn't exist.
    auto node1 = top->getChild("node1");
    auto node2 = node1->getChild("node2");
    auto node3 = node2->getChild("node3");

    auto top_ext = top->getExtension("ski_trail", false);
    auto node1_ext = node1->getExtension("ski_trail", false);
    auto node2_ext = node2->getExtension("ski_trail", false);
    auto node3_ext = node3->getExtension("ski_trail", false);

    EXPECT_NOTEQUAL(top_ext, nullptr);
    EXPECT_NOTEQUAL(node1_ext, nullptr);
    EXPECT_NOTEQUAL(node2_ext, nullptr);
    EXPECT_EQUAL(node3_ext, nullptr);

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
        top->createExtensions("global_meta.yaml", {}, true);
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
    };

    verif_global_meta(top_global_meta_ext);

    // Up to now, node3 does not have any extensions. Test on-demand extension creation
    // with a registered factory by calling getExtension(name, true).
    node3_ext = node3->getExtension("ski_trail", true);

    // The created extension should be of type SkiTrailExtension with one parameter.
    // The SkiTrailExtension::postCreate() method adds the "trail_closed" parameter
    // manually.
    EXPECT_NOTEQUAL(dynamic_cast<SkiTrailExtension*>(node3_ext), nullptr);
    EXPECT_EQUAL(node3_ext->getParameters()->getNumParameters(), 1);
    EXPECT_EQUAL(node3_ext->getParameters()->getParameterValueAs<bool>("trail_closed"), false);

    // Now test on-demand extension creation without a registered factory.
    // Without a factory, this always fails to create the extension by design.
    node3_ext = node3->getExtension("global_meta", true);
    EXPECT_EQUAL(node3_ext, nullptr);
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

// Test: Simulation with CommandLineSimulator.
void TestExtensionsWithCommandLineSim(const std::string & cmdline_args)
{
    std::vector<std::string> args;
    boost::split(args, cmdline_args, boost::is_any_of(" "));
    args.insert(args.begin(), "./TreeNodeExtensions_test");

    std::vector<char*> cargs;
    for (auto & arg : args) {
        char* s = const_cast<char*>(arg.c_str());
        cargs.push_back(s);
    }

    int argc = (int)cargs.size();
    char** argv = cargs.data();

    sparta::app::DefaultValues DEFAULTS;
    const char USAGE[] = "example usage";

    sparta::SimulationInfo::getInstance() =
        sparta::SimulationInfo("TreeNodeExtensions_test", argc, argv, "v0.0.0", "", {});

    sparta::app::CommandLineSimulator cls(USAGE, DEFAULTS);

    // Parse command line options and configure simulator
    int err_code = 0;
    EXPECT_NOTHROW(EXPECT_EQUAL(cls.parse(argc, argv, err_code), true));

    // Create the simulator
    sparta::Scheduler scheduler;
    TestSimulator sim(scheduler);
    cls.populateSimulation(&sim);

    // Run tree node extensions test
    TestExtensions(sim.getRoot(), true /*Command-line sim*/);
}

int main(int argc, char** argv)
{
    sparta::SleeperThread::disableForever();

    TestExtensionsWithoutSim();
    TestExtensionsWithStandaloneSim();

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

    REPORT_ERROR;
    return ERROR_CODE;
}
