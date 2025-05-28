// <Simulation.cpp> -*- C++ -*-


#include <iostream>


#include "ExampleSimulation.hpp"
#include "Core.hpp"
#include "CPUFactory.hpp"

#include "sparta/simulation/Clock.hpp"
#include "sparta/utils/TimeManager.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/TreeNodeExtensions.hpp"
#include "sparta/trigger/ContextCounterTrigger.hpp"
#include "sparta/utils/StringUtils.hpp"
#include "sparta/statistics/CycleHistogram.hpp"
#include "sparta/statistics/Histogram.hpp"
#include "sparta/statistics/HistogramFunctionManager.hpp"
#include "sparta/utils/SpartaTester.hpp"

#include "Fetch.hpp"
#include "Decode.hpp"
#include "Rename.hpp"
#include "Dispatch.hpp"
#include "Execute.hpp"
#include "LSU.hpp"
#include "ROB.hpp"
#include "FlushManager.hpp"
#include "Preloader.hpp"
#include "CustomHistogramStats.hpp"

// UPDATE
#include "BIU.hpp"
#include "MSS.hpp"

namespace sparta {

  // Example parameter set used to reproduce write-final-config
  class IntParameterSet : public ParameterSet {
  public:
      IntParameterSet(TreeNode * parent) :
          ParameterSet(parent),
          int_param_(new Parameter<uint32_t>(
              "baz", 0, "Example parameter set to reproduce bug"))
      {
          addParameter_(int_param_.get());
      }

      uint32_t read() const {
          return int_param_->getValue();
      }

  private:
      std::unique_ptr<Parameter<uint32_t>> int_param_;
  };

  // Dummy node class used together with IntParameterSet to
  // reproduce write-final-config bug
  class Baz : public TreeNode {
  public:
      Baz(TreeNode* parent,
          const std::string & desc) :
        TreeNode(parent, "baz_node", "BazGroup", 0, desc)
      {
          baz_.reset(new IntParameterSet(this));
      }

      void readParams() {
          std::cout << "  Node '" << getLocation()
                    << "' has parameter 'baz' with a value set to "
                    << baz_->read() << std::endl;
          auto ext = getExtension("baz_ext");
          if(ext) {
              std::cout << "That's the ticket: "
                        << ext->getParameters()->getParameterValueAs<std::string>("ticket_") << std::endl;
          }
      }

  private:
      std::unique_ptr<IntParameterSet> baz_;
  };

}

class CircleExtensions : public sparta::ExtensionsParamsOnly
{
public:
    CircleExtensions() : sparta::ExtensionsParamsOnly() {}
    virtual ~CircleExtensions() {}

    void doSomethingElse() const {
        std::cout << "Invoking a method that is unknown to the sparta::TreeNode object, "
                     "even though 'this' object was created by, and currently owned by, "
                     "a specific tree node.";
    }

private:

    // Note: this parameter is NOT in the yaml config file,
    // but subclasses can provide any parameter type supported
    // by sparta::Parameter<T> which may be too complicated to
    // clearly describe using simple yaml syntax
    std::unique_ptr<sparta::Parameter<double>> degrees_;

    // The base class will clobber together whatever parameter values it
    // found in the yaml file, and give us a chance to add custom parameters
    // to the same set
    virtual void postCreate() override {
        sparta::ParameterSet * ps = getParameters();
        degrees_.reset(new sparta::Parameter<double>(
            "degrees_", 360.0, "Number of degrees in a circle", ps));
    }
};

double calculateAverageOfInternalCounters(
    const std::vector<const sparta::CounterBase*> & counters)
{
    double agg = 0;
    for (const auto & ctr : counters) {
        agg += ctr->get();
    }
    return agg / counters.size();
}

ExampleSimulator::ExampleSimulator(const std::string& topology,
                                   sparta::Scheduler & scheduler,
                                   uint32_t num_cores,
                                   uint64_t instruction_limit,
                                   bool show_factories) :
    sparta::app::Simulation("sparta_core_example", &scheduler),
    cpu_topology_(topology),
    num_cores_(num_cores),
    instruction_limit_(instruction_limit),
    show_factories_(show_factories)
{
    // Set up the CPU Resource Factory to be available through ResourceTreeNode
    getResourceSet()->addResourceFactory<core_example::CPUFactory>();

    // Set up all node extension factories to be available during the simulation
    //    - This is only needed for parameter sets that also want to add some methods
    //      to their tree node extension, and/or for those that want to extend node
    //      parameter sets with more complicated sparta::Parameter<T> data types
    addTreeNodeExtensionFactory_("circle", [](){return new CircleExtensions;});

    // Initialize example simulation controller
    controller_.reset(new ExampleSimulator::ExampleController(this));
    setSimulationController_(controller_);

    // Register a custom calculation method for 'combining' a context counter's
    // internal counters into one number. In this example simulator, let's just
    // use an averaging function called "avg" which we can then invoke from report
    // definition YAML files.
    sparta::trigger::ContextCounterTrigger::registerContextCounterCalcFunction(
        "avg", &calculateAverageOfInternalCounters);
}

void ExampleSimulator::registerStatCalculationFcns_()
{
    REGISTER_HISTOGRAM_STAT_CALC_FCN(CycleHistogramTreeNode, stdev_x3);
    REGISTER_HISTOGRAM_STAT_CALC_FCN(CycleHistogramTreeNode, fraction_coverage_greaterThan2StdDev);
    REGISTER_HISTOGRAM_STAT_CALC_FCN(CycleHistogramTreeNode, fraction_coverage_mean_p_StdDev_mean_p_2StdDev);
    REGISTER_HISTOGRAM_STAT_CALC_FCN(CycleHistogramTreeNode, fraction_coverage_mean_mean_p_StdDev);
    REGISTER_HISTOGRAM_STAT_CALC_FCN(CycleHistogramTreeNode, fraction_coverage_mean_m_StdDev_mean);
    REGISTER_HISTOGRAM_STAT_CALC_FCN(CycleHistogramTreeNode, fraction_coverage_mean_m_2StdDev_mean_m_StdDev);
    REGISTER_HISTOGRAM_STAT_CALC_FCN(CycleHistogramTreeNode, fraction_coverage_lesserThan2StdDev);
    REGISTER_HISTOGRAM_STAT_CALC_FCN(HistogramTreeNode, stdev_x3_h);
    REGISTER_HISTOGRAM_STAT_CALC_FCN(HistogramTreeNode, fraction_coverage_greaterThan2StdDev_h);
    REGISTER_HISTOGRAM_STAT_CALC_FCN(HistogramTreeNode, fraction_coverage_mean_p_StdDev_mean_p_2StdDev_h);
    REGISTER_HISTOGRAM_STAT_CALC_FCN(HistogramTreeNode, fraction_coverage_mean_mean_p_StdDev_h);
    REGISTER_HISTOGRAM_STAT_CALC_FCN(HistogramTreeNode, fraction_coverage_mean_m_StdDev_mean_h);
    REGISTER_HISTOGRAM_STAT_CALC_FCN(HistogramTreeNode, fraction_coverage_mean_m_2StdDev_mean_m_StdDev_h);
    REGISTER_HISTOGRAM_STAT_CALC_FCN(HistogramTreeNode, fraction_coverage_lesserThan2StdDev_h);
}

ExampleSimulator::~ExampleSimulator()
{
    getRoot()->enterTeardown(); // Allow deletion of nodes without error now
    if (on_triggered_notifier_registered_) {
        getRoot()->DEREGISTER_FOR_NOTIFICATION(
            onTriggered_, std::string, "sparta_expression_trigger_fired");
    }
}

//! Get the resource factory needed to build and bind the tree
auto ExampleSimulator::getCPUFactory_() -> core_example::CPUFactory*{
    auto sparta_res_factory = getResourceSet()->getResourceFactory("cpu");
    auto cpu_factory = dynamic_cast<core_example::CPUFactory*>(sparta_res_factory);
    return cpu_factory;
}

void ExampleSimulator::buildTree_()
{
    // TREE_BUILDING Phase.  See sparta::PhasedObject::TreePhase
    // Register all the custom stat calculation functions with (cycle)histogram nodes
    registerStatCalculationFcns_();

    auto cpu_factory = getCPUFactory_();

    // Set the cpu topology that will be built
    cpu_factory->setTopology(cpu_topology_, num_cores_);

    // Create a single CPU
    sparta::ResourceTreeNode* cpu_tn = new sparta::ResourceTreeNode(getRoot(),
                                                                "cpu",
                                                                sparta::TreeNode::GROUP_NAME_NONE,
                                                                sparta::TreeNode::GROUP_IDX_NONE,
                                                                "CPU Node",
                                                                cpu_factory);
    to_delete_.emplace_back(cpu_tn);

    // Tell the factory to build the resources now
    cpu_factory->buildTree(getRoot());

    // Print the registered factories
    if(show_factories_){
        std::cout << "Registered factories: \n";
        for(const auto& f : getCPUFactory_()->getResourceNames()){
            std::cout << "\t" << f << std::endl;
        }
    }

    // Validate tree node extensions during tree building
    for(uint32_t i = 0; i < num_cores_; ++i){
        const std::string dispatch_loc = "cpu.core" + std::to_string(i) + ".dispatch";
        const std::string alu0_loc = "cpu.core" + std::to_string(i) + ".alu0";
        const std::string alu1_loc = "cpu.core" + std::to_string(i) + ".alu1";
        const std::string fpu_loc = "cpu.core" + std::to_string(i) + ".fpu";

        // user_data.when_ (dispatch)
        if (auto prm = getExtensionParameter_<std::string>(getRoot()->getChild(dispatch_loc), "when_", "user_data")) {
            prm->addDependentValidationCallback([](std::string & val, const sparta::TreeNode*) -> bool {
                return val == "buildTree_";
            }, "Parameter 'when_' should be 'buildTree_'");
        }

        // user_data.why_ (dispatch)
        if (auto prm = getExtensionParameter_<std::string>(getRoot()->getChild(dispatch_loc), "why_", "user_data")) {
            prm->addDependentValidationCallback([](std::string & val, const sparta::TreeNode*) -> bool {
                return val == "checkAvailability";
            }, "Parameter 'why_' should be 'checkAvailability'");
        }

        // square.edges_ (dispatch)
        if (auto prm = getExtensionParameter_<std::string>(getRoot()->getChild(dispatch_loc), "edges_", "square")) {
            prm->addDependentValidationCallback([](std::string & val, const sparta::TreeNode*) -> bool {
                return val == "4";
            }, "Parameter 'edges_' should be '4'");
        }

        // difficulty.color_ (alu0)
        if (auto prm = getExtensionParameter_<std::string>(getRoot()->getChild(alu0_loc), "color_", "difficulty")) {
            prm->addDependentValidationCallback([](std::string & val, const sparta::TreeNode*) -> bool {
                return val == "black";
            }, "Parameter 'color_' should be 'black'");
        }

        // difficulty.shape_ (alu0)
        if (auto prm = getExtensionParameter_<std::string>(getRoot()->getChild(alu0_loc), "shape_", "difficulty")) {
            prm->addDependentValidationCallback([](std::string & val, const sparta::TreeNode*) -> bool {
                return val == "diamond";
            }, "Parameter 'shape_' should be 'diamond'");
        }

        // difficulty.color_ (alu1)
        if (auto prm = getExtensionParameter_<std::string>(getRoot()->getChild(alu1_loc), "color_", "difficulty")) {
            prm->addDependentValidationCallback([](std::string & val, const sparta::TreeNode*) -> bool {
                return val == "green";
            }, "Parameter 'color_' should be 'green'");
        }

        // difficulty.shape_ (alu1)
        if (auto prm = getExtensionParameter_<std::string>(getRoot()->getChild(alu1_loc), "shape_", "difficulty")) {
            prm->addDependentValidationCallback([](std::string & val, const sparta::TreeNode*) -> bool {
                return val == "circle";
            }, "Parameter 'shape_' should be 'circle'");
        }

        // circle.color_ (fpu)
        if (auto prm = getExtensionParameter_<std::string>(getRoot()->getChild(fpu_loc), "color_", "circle")) {
            prm->addDependentValidationCallback([](std::string & val, const sparta::TreeNode*) -> bool {
                return val == "green";
            }, "Parameter 'color_' should be 'green'");
        }

        // circle.shape_ (fpu)
        if (auto prm = getExtensionParameter_<std::string>(getRoot()->getChild(fpu_loc), "shape_", "circle")) {
            prm->addDependentValidationCallback([](std::string & val, const sparta::TreeNode*) -> bool {
                return val == "round";
            }, "Parameter 'shape_' should be 'round'");
        }

        // circle.degrees_ (fpu)
        if (auto prm = getExtensionParameter_<double>(getRoot()->getChild(fpu_loc), "degrees_", "circle")) {
            prm->addDependentValidationCallback([](double & val, const sparta::TreeNode*) -> bool {
                return val == 360.0;
            }, "Parameter 'degrees_' should be 360.0");
        }

        // circle.edges_ (fpu)
        if (auto prm = getExtensionParameter_<std::string>(getRoot()->getChild(fpu_loc), "edges_", "circle")) {
            prm->addDependentValidationCallback([](std::string & val, const sparta::TreeNode*) -> bool {
                return val == "0";
            }, "Parameter 'edges_' should be '0'");
        }

        // user-specified extension class
        getExtension_<CircleExtensions>(getRoot()->getChild(fpu_loc), "circle")->doSomethingElse();
    }

    // Attach two tree nodes to get the following:
    //   top
    //     core0
    //       dispatch
    //         baz_node
    //           params
    //             baz
    //       fpu
    //         baz_node
    //           params
    //             baz
    //
    // This is needed to reproduce a write-final-config bug where an arch file
    // specifies 'top.core0.*.baz_node.params.baz: 300' and the ConfigEmitterYAML
    // ends up throwing an exception due to the '*' which tripped up the tree node
    // extensions code.
    auto dispatch = getRoot()->getChild("cpu.core0.dispatch");
    auto fpu = getRoot()->getChild("cpu.core0.fpu");

    dispatch_baz_.reset(new sparta::Baz(
        dispatch, "Dummy node under top.cpu.core0.dispatch (to reproduce a SPARTA bug)"));

    fpu_baz_.reset(new sparta::Baz(
        fpu, "Dummy node under top.cpu.core0.fpu (to reproduce a SPARTA bug)"));
}

void ExampleSimulator::configureTree_()
{
    validateTreeNodeExtensions_();

    // In TREE_CONFIGURING phase
    // Configuration from command line is already applied

    // Read these parameter values to avoid 'unread unbound parameter' exceptions:
    //   top.cpu.core0.dispatch.baz_node.params.baz
    //   top.cpu.core0.fpu.baz_node.params.baz
    dispatch_baz_->readParams();
    fpu_baz_->readParams();

    sparta::ParameterBase* max_instrs =
        getRoot()->getChildAs<sparta::ParameterBase>("cpu.core0.rob.params.num_insts_to_retire");

    // Safely assign as string for now in case parameter type changes.
    // Direct integer assignment without knowing parameter type is not yet available through C++ API
    if(instruction_limit_ != 0){
        max_instrs->setValueFromString(sparta::utils::uint64_to_str(instruction_limit_));
    }

    testing_notification_source_.reset(new sparta::NotificationSource<uint64_t>(
        this->getRoot()->getSearchScope()->getChild("top.cpu.core0.rob"),
        "testing_notif_channel",
        "Notification channel for testing purposes only",
        "testing_notif_channel"));

    toggle_trigger_notification_source_.reset(new sparta::NotificationSource<uint64_t>(
        getRoot()->getSearchScope()->getChild("top.cpu.core0.rob"),
        "stats_profiler",
        "Notification channel for testing report toggling on/off (statistics profiling)",
        "stats_profiler"));

    legacy_warmup_report_starter_.reset(new sparta::NotificationSource<uint64_t>(
        getRoot(),
        "all_threads_warmup_instruction_count_retired_re4",
        "Legacy notificiation channel for testing purposes only",
        "all_threads_warmup_instruction_count_retired_re4"));

    getRoot()->REGISTER_FOR_NOTIFICATION(
        onTriggered_, std::string, "sparta_expression_trigger_fired");
    on_triggered_notifier_registered_ = true;
}

void ExampleSimulator::bindTree_()
{
    // In TREE_FINALIZED phase
    // Tree is finalized. Taps placed. No new nodes at this point
    // Bind appropriate ports

    //Tell the factory to bind all units
    auto cpu_factory = getCPUFactory_();
    cpu_factory->bindTree(getRoot());

    sparta::SpartaHandler cb = sparta::SpartaHandler::from_member<
        ExampleSimulator, &ExampleSimulator::postRandomNumber_>(
            this, "ExampleSimulator::postRandomNumber_");

    random_number_trigger_.reset(new sparta::trigger::ExpressionCounterTrigger(
        "RandomNumber", cb, "cpu.core0.rob.stats.total_number_retired 7500", false, this->getRoot()));

    toggle_notif_trigger_.reset(new sparta::trigger::ExpressionTimeTrigger(
        "ToggleNotif",
        CREATE_SPARTA_HANDLER(ExampleSimulator, postToToggleTrigger_),
        "1 ns",
        getRoot()));

    static const uint32_t warmup_multiplier = 1000;
    auto gen_expression = [](const uint32_t core_idx) {
        std::ostringstream oss;
        oss << "cpu.core" << core_idx << ".rob.stats.total_number_retired >= "
            << ((core_idx+1) * warmup_multiplier);
        return oss.str();
    };

    num_cores_still_warming_up_ = num_cores_;
    core_warmup_listeners_.reserve(num_cores_);

    for (uint32_t core_idx = 0; core_idx < num_cores_; ++core_idx) {
        core_warmup_listeners_.emplace_back(
            new sparta::trigger::ExpressionTrigger(
                "LegacyWarmupNotifications",
                CREATE_SPARTA_HANDLER(ExampleSimulator, onLegacyWarmupNotification_),
                gen_expression(core_idx),
                getRoot(),
                nullptr));
    }
}

void ExampleSimulator::onLegacyWarmupNotification_()
{
    sparta_assert(num_cores_still_warming_up_ > 0);
    --num_cores_still_warming_up_;
    if (num_cores_still_warming_up_ == 0) {
        legacy_warmup_report_starter_->postNotification(1);
    }
}

const sparta::CounterBase* ExampleSimulator::findSemanticCounter_(CounterSemantic sem) const {
    switch(sem){
    case CSEM_INSTRUCTIONS:
        return getRoot()->getChildAs<const sparta::CounterBase>("cpu.core0.rob.stats.total_number_retired");
        break;
    default:
        return nullptr;
    }
}

void ExampleSimulator::postRandomNumber_()
{
    const size_t random = rand() % 25;
    testing_notification_source_->postNotification(random);
    random_number_trigger_->reschedule();
}

void ExampleSimulator::postToToggleTrigger_()
{
    typedef std::pair<uint64_t,uint64_t> ValueCount;
    static std::queue<ValueCount> values;

    if (values.empty()) {
        values.push({0,15});
        values.push({1,25});
        values.push({0,15});
        values.push({1,25});
        values.push({0,15});

        ValueCount tmp = values.front();
        values.push(tmp);
    }

    if (values.front().second == 0) {
        values.pop();
        ValueCount tmp = values.front();
        values.push(tmp);
    } else {
        --values.front().second;
    }

    const ValueCount & current_value = values.front();
    const uint64_t value_to_post = current_value.first;
    toggle_trigger_notification_source_->postNotification(value_to_post);
    toggle_notif_trigger_->reschedule();
}

void ExampleSimulator::onTriggered_(const std::string & msg)
{
    std::cout << "     [trigger] " << msg << std::endl;
}

template <typename ParamT>
sparta::Parameter<ParamT>* ExampleSimulator::getExtensionParameter_(
    sparta::TreeNode* node,
    const std::string& param_name,
    const std::string& ext_name)
{
    if (!node) {
        return nullptr;
    }

    sparta::TreeNode::ExtensionsBase * ext = ext_name.empty() ?
        node->getExtension() :
        node->getExtension(ext_name);

    if (!ext) {
        return nullptr;
    }

    sparta::ParameterSet * params = ext->getParameters();
    if (!params) {
        return nullptr;
    }

    if (!params->hasParameter(param_name)) {
        return nullptr;
    }

    return &params->getParameterAs<ParamT>(param_name);
}

template <typename ExtensionT>
ExtensionT* ExampleSimulator::getExtension_(
    sparta::TreeNode* node,
    const std::string& ext_name)
{
    static_assert(std::is_base_of<sparta::TreeNode::ExtensionsBase, ExtensionT>::value,
                  "ExtensionT must be derived from sparta::TreeNode::ExtensionsBase");

    if (!node) {
        return nullptr;
    }

    return ext_name.empty() ?
        dynamic_cast<ExtensionT*>(node->getExtension()) :
        dynamic_cast<ExtensionT*>(node->getExtension(ext_name));
}

void ExampleSimulator::validateTreeNodeExtensions_()
{
    // cat.name_
    if (auto prm = getExtensionParameter_<std::string>(
        getRoot()->getChild("cpu.core0.lsu"), "name_", "cat"))
    {
        prm->addDependentValidationCallback(
            [](std::string & val, const sparta::TreeNode*) -> bool {
                return val == "Tom";
            }, "Parameter 'name_' should be 'Tom'");
    }

    // cat.language_
    if (auto prm = getExtensionParameter_<std::string>(
        getRoot()->getChild("cpu.core0.lsu"), "language_", "cat"))
    {
        prm->addDependentValidationCallback(
            [](std::string & val, const sparta::TreeNode*) -> bool {
                return val == "meow" || val == "grrr";
            }, "Parameter 'language_' should be 'meow' or 'grrr'");
    }

    // mouse.name_
    if (auto prm = getExtensionParameter_<std::string>(
        getRoot()->getChild("cpu.core0.lsu"), "name_", "mouse"))
    {
        prm->addDependentValidationCallback(
            [](std::string & val, const sparta::TreeNode*) -> bool {
                return val == "Jerry";
            }, "Parameter 'name_' should be 'Jerry'");
    }

    // mouse.language_
    if (auto prm = getExtensionParameter_<std::string>(
        getRoot()->getChild("cpu.core0.lsu"), "language_", "mouse"))
    {
        prm->addDependentValidationCallback(
            [](std::string & val, const sparta::TreeNode*) -> bool {
                return val == "squeak";
            }, "Parameter 'language_' should be 'squeak'");
    }

    // circle.color_
    if (auto prm = getExtensionParameter_<std::string>(
        getRoot()->getChild("cpu.core0.fpu"), "color_", "circle"))
    {
        prm->addDependentValidationCallback(
            [](std::string & val, const sparta::TreeNode*) -> bool {
                return val == "green";
            }, "Parameter 'color_' should be 'green'");
    }

    // circle.shape_
    if (auto prm = getExtensionParameter_<std::string>(
        getRoot()->getChild("cpu.core0.fpu"), "shape_", "circle"))
    {
        prm->addDependentValidationCallback(
            [](std::string & val, const sparta::TreeNode*) -> bool {
                return val == "round";
            }, "Parameter 'shape_' should be 'round'");
    }

    // circle.degrees_
    if (auto prm = getExtensionParameter_<double>(
        getRoot()->getChild("cpu.core0.fpu"), "degrees_", "circle"))
    {
        prm->addDependentValidationCallback(
            [](double & val, const sparta::TreeNode*) -> bool {
                return val == 360.0;
            }, "Parameter 'degrees_' should be 360.0");
    }

    // user-specified extension class
    getExtension_<CircleExtensions>(getRoot()->getChild("cpu.core0.fpu"), "circle")->doSomethingElse();

    // apple.color_
    if (auto prm = getExtensionParameter_<std::string>(
        getRoot(), "color_", "apple"))
    {
        prm->addDependentValidationCallback(
            [](std::string & val, const sparta::TreeNode*) -> bool {
                return val == "red";
            }, "Parameter 'color_' should be 'red'");
    }

    // The 'core0.lsu' node has two named extensions, so asking that node for
    // unqualified extensions (no name specified) should throw.
    //
    // Note that we still have to check if core0.lsu has multiple extensions,
    // since it will have zero in most example simulations unless --extension-file
    // was used.
    auto core0_lsu = getRoot()->getChild("cpu.core0.lsu");
    if (core0_lsu->getNumExtensions() > 1) {
        bool threw = false;
        try {
            getExtension_<>(core0_lsu);
        } catch (...) {
            threw = true;
        }

        if (!threw) {
            throw sparta::SpartaException("Expected an exception to be thrown for unqualified "
                                          "call to TreeNode::getExtension()");
        }
    }

    // <unnamed>.color_
    if (auto prm = getExtensionParameter_<std::string>(
        getRoot()->getChild("cpu.core0.fpu"), "color_"))
    {
        prm->addDependentValidationCallback(
            [](std::string & val, const sparta::TreeNode*) -> bool {
                return val == "green";
            }, "Parameter 'color_' should be 'green'");
    }

    // <unnamed>.shape_
    if (auto prm = getExtensionParameter_<std::string>(
        getRoot()->getChild("cpu.core0.fpu"), "shape_"))
    {
        prm->addDependentValidationCallback(
            [](std::string & val, const sparta::TreeNode*) -> bool {
                return val == "round";
            }, "Parameter 'shape_' should be 'round'");
    }

    // <unnamed>.degrees_
    if (auto prm = getExtensionParameter_<double>(
        getRoot()->getChild("cpu.core0.fpu"), "degrees_"))
    {
        prm->addDependentValidationCallback(
            [](double & val, const sparta::TreeNode*) -> bool {
                return val == 360.0;
            }, "Parameter 'degrees_' should be 360.0");
    }

    // <unnamed>.edges_
    if (auto prm = getExtensionParameter_<std::string>(
        getRoot()->getChild("cpu.core0.fpu"), "edges_"))
    {
        prm->addDependentValidationCallback(
            [](std::string & val, const sparta::TreeNode*) -> bool {
                return val == "0";
            }, "Parameter 'edges_' should be '0'");
    }

    // baz_ext.ticket_
    if (auto prm = getExtensionParameter_<std::string>(
        getRoot()->getChild("cpu.core0.dispatch.baz_node", false), "ticket_", "baz_ext"))
    {
        prm->addDependentValidationCallback(
            [](std::string & val, const sparta::TreeNode*) -> bool {
                return val == "663";
            }, "Parameter 'ticket_' should be '663'");
    }
}

ExampleSimulator::ExampleController::ExampleController(
    const sparta::app::Simulation * sim) :
    sparta::app::Simulation::SimulationController(sim)
{
    sparta::app::Simulation::SimulationController::addNamedCallback_(
        "eat", CREATE_SPARTA_HANDLER(ExampleController, customEatCallback_));

    sparta::app::Simulation::SimulationController::addNamedCallback_(
        "sleep", CREATE_SPARTA_HANDLER(ExampleController, customSleepCallback_));
}

void ExampleSimulator::ExampleController::pause_(const sparta::app::Simulation * sim)
{
    std::cout << "  [control] Controller PAUSE method has been called for simulation '"
              << sim->getSimName() << "'" << std::endl;
}

void ExampleSimulator::ExampleController::resume_(const sparta::app::Simulation * sim)
{
    std::cout << "  [control] Controller RESUME method has been called for simulation '"
              << sim->getSimName() << "'" << std::endl;
}

void ExampleSimulator::ExampleController::terminate_(const sparta::app::Simulation * sim)
{
    std::cout << "  [control] Controller TERMINATE method has been called for simulation '"
              << sim->getSimName() << "'" << std::endl;
    const_cast<sparta::Scheduler*>(sim->getScheduler())->stopRunning();
}

void ExampleSimulator::ExampleController::customEatCallback_()
{
    std::cout << "  [control] Controller CUSTOM method has been called ('eat')" << std::endl;
}

void ExampleSimulator::ExampleController::customSleepCallback_()
{
    std::cout << "  [control] Controller CUSTOM method has been called ('sleep')" << std::endl;
}
