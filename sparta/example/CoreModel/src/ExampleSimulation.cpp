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

template <typename DataT>
void validateParameter(const sparta::ParameterSet & params,
                       const std::string & param_name,
                       const DataT & expected_value)
{
    if (!params.hasParameter(param_name)) {
        return;
    }
    const DataT actual_value = params.getParameterValueAs<DataT>(param_name);
    if (actual_value != expected_value) {
        throw sparta::SpartaException("Invalid extension parameter encountered:\n")
            << "\tParameter name:             " << param_name
            << "\nParameter value (actual):   " << actual_value
            << "\nParameter value (expected): " << expected_value;
    }
}

template <typename DataT>
void validateParameter(const sparta::ParameterSet & params,
                       const std::string & param_name,
                       const std::set<DataT> & expected_values)
{
    bool found = false;
    for (const auto & expected : expected_values) {
        try {
            found = false;
            validateParameter<DataT>(params, param_name, expected);
            found = true;
            break;
        } catch (...) {
        }
    }

    if (!found) {
        throw sparta::SpartaException("Invalid extension parameter "
                                  "encountered for '") << param_name << "'";
    }
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
        sparta::TreeNode * dispatch = getRoot()->getChild("cpu.core0.dispatch", false);
        if (dispatch) {
            sparta::TreeNode::ExtensionsBase * extensions = dispatch->getExtension("user_data");

            // If present, validate the parameter values as given in the extension / configuration file
            if (extensions != nullptr) {
                const sparta::ParameterSet * dispatch_prms = extensions->getParameters();
                sparta_assert(dispatch_prms != nullptr);
                validateParameter<std::string>(*dispatch_prms, "when_", "buildTree_");
                validateParameter<std::string>(*dispatch_prms, "why_", "checkAvailability");
            }

            // There might be an extension given in --extension-file that is not found
            // at all in any --config-file given at the command prompt. Verify that if
            // present, the value is as expected.
            extensions = dispatch->getExtension("square");
            if (extensions != nullptr) {
                const sparta::ParameterSet * dispatch_prms = extensions->getParameters();
                sparta_assert(dispatch_prms != nullptr);
                validateParameter<std::string>(*dispatch_prms, "edges_", "4");
            }
        }

        // See if there are any extensions for the alu0/alu1 nodes
        sparta::TreeNode * alu0 = getRoot()->getChild("cpu.core0.alu0");
        sparta::TreeNode * alu1 = getRoot()->getChild("cpu.core0.alu1");
        if (alu0) {
            sparta::TreeNode::ExtensionsBase * extensions = alu0->getExtension("difficulty");
            if (extensions != nullptr) {
                const sparta::ParameterSet * alu0_prms = extensions->getParameters();
                sparta_assert(alu0_prms != nullptr);

                validateParameter<std::string>(*alu0_prms, "color_", "black");
                validateParameter<std::string>(*alu0_prms, "shape_", "diamond");
            }
        }
        if (alu1) {
            sparta::TreeNode::ExtensionsBase * extensions = alu1->getExtension("difficulty");
            if (extensions != nullptr) {
                const sparta::ParameterSet * alu1_prms = extensions->getParameters();
                sparta_assert(alu1_prms != nullptr);

                validateParameter<std::string>(*alu1_prms, "color_", "green");
                validateParameter<std::string>(*alu1_prms, "shape_", "circle");
            }
        }

        // Once again, ask for a named extension for a tree node that was just created.
        // The difference here is that the 'circle' extension also has a factory associated
        // with it.
        sparta::TreeNode * fpu = getRoot()->getChild("cpu.core0.fpu", false);
        if (fpu) {
            sparta::TreeNode::ExtensionsBase * extensions = fpu->getExtension("circle");

            // If present, validate the parameter values as given in the extension / configuration file
            if (extensions != nullptr) {
                const sparta::ParameterSet * fpu_prms = extensions->getParameters();
                sparta_assert(fpu_prms != nullptr);

                validateParameter<std::string>(*fpu_prms, "color_", "green");
                validateParameter<std::string>(*fpu_prms, "shape_", "round");
                validateParameter<double>     (*fpu_prms, "degrees_", 360.0);

                // While most of the 'circle' extensions are given in --config-file options,
                // there might be more parameters added in with --extension-file, so let's check
                validateParameter<std::string>(*fpu_prms, "edges_", "0");

                // We know the subclass type, so we should be able to safely dynamic cast
                // to that type and call methods on it
                const CircleExtensions * circle_subclass = dynamic_cast<const CircleExtensions*>(extensions);
                circle_subclass->doSomethingElse();
            }
        }
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

    // To get better coverage of the pipeline collector, we will use different clock
    // domains for each core when collection is enabled.
    auto pc = getSimulationConfiguration()->pipeline_collection_file_prefix != sparta::app::NoPipelineCollectionStr;
    if (pc && num_cores_ > 1) {
        auto root_clk = getClockManager().getRoot();
        for (uint32_t core_idx = 1; core_idx < num_cores_; ++core_idx) {
            const std::string clk_name = "core" + std::to_string(core_idx) + "_clk";
            auto core_clk = getClockManager().makeClock(clk_name, root_clk);
            auto core = getRoot()->getChild("cpu.core" + std::to_string(core_idx));
            core_clk->setPeriod(root_clk->getPeriod() * (core_idx + 1));
            core->setClock(core_clk.get());
        }
    }

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

void ExampleSimulator::validateTreeNodeExtensions_()
{
    // From the yaml file, the 'cat' extension had parameters 'name_' and 'language_'
    sparta::TreeNode * core_tn = getRoot()->getChild("cpu.core0.lsu");
    if (core_tn == nullptr) {
        return;
    }
    sparta::TreeNode::ExtensionsBase * cat_base = core_tn->getExtension("cat");
    if (cat_base == nullptr) {
        return;
    }
    sparta::ParameterSet * cat_prms = cat_base->getParameters();

    validateParameter<std::string>(*cat_prms, "name_", "Tom");

    // The expected "meow" parameter value, given in a --config-file, may have
    // been overridden in a provided --extension-file
    validateParameter<std::string>(*cat_prms, "language_", {"meow", "grrr"});

    // Same goes for the 'mouse' extension...
    sparta::TreeNode::ExtensionsBase * mouse_base = core_tn->getExtension("mouse");
    if (mouse_base == nullptr) {
        return;
    }
    sparta::ParameterSet * mouse_prms = mouse_base->getParameters();

    validateParameter<std::string>(*mouse_prms, "name_", "Jerry");
    validateParameter<std::string>(*mouse_prms, "language_", "squeak");

    // Another extension called 'circle' was put on a different tree node...
    sparta::TreeNode * fpu_tn = getRoot()->getChild("cpu.core0.fpu");
    if (fpu_tn == nullptr) {
        return;
    }
    sparta::TreeNode::ExtensionsBase * circle_base = fpu_tn->getExtension("circle");
    if (circle_base == nullptr) {
        return;
    }
    sparta::ParameterSet * circle_prms = circle_base->getParameters();

    // The 'circle' extension had 'color_' and 'shape_' parameters given in the yaml file:
    validateParameter<std::string>(*circle_prms, "color_", "green");
    validateParameter<std::string>(*circle_prms, "shape_", "round");

    // That subclass also gave a parameter value not found in the yaml file at all:
    validateParameter<double>(*circle_prms, "degrees_", 360.0);

    // Further, the 'circle' extension gave a subclass factory for the CircleExtensions class...
    // so we should be able to dynamic_cast to the known type:
    const CircleExtensions * circle_subclass = dynamic_cast<const CircleExtensions*>(circle_base);
    circle_subclass->doSomethingElse();

    // Lastly, verify that there are no issues with putting extensions on the 'top' node
    sparta::TreeNode * top_node = getRoot();
    if (top_node == nullptr) {
        return;
    }
    sparta::TreeNode::ExtensionsBase * top_extensions = top_node->getExtension("apple");
    if (top_extensions == nullptr) {
        return;
    }
    sparta::ParameterSet *top_prms = top_extensions->getParameters();
    validateParameter<std::string>(*top_prms, "color_", "red");

    // The 'core0.lsu' node has two named extensions, so asking that node for
    // unqualified extensions (no name specified) should throw
    try {
        core_tn->getExtension();
        throw sparta::SpartaException("Expected an exception to be thrown for unqualified "
                                  "call to TreeNode::getExtension()");
    } catch (...) {
    }

    // While the 'core0.fpu' node only had one extension, so we should be able to
    // access it without giving any particular name
    sparta::TreeNode::ExtensionsBase * circle_base_by_default = fpu_tn->getExtension();
    circle_prms = circle_base_by_default->getParameters();

    validateParameter<std::string>(*circle_prms, "color_", "green");
    validateParameter<std::string>(*circle_prms, "shape_", "round");
    validateParameter<double>(*circle_prms, "degrees_", 360.0);

    // Check to see if additional parameters were added to this tree node's extension
    // (--config-file and --extension-file options can be given at the same time, and
    // we should have access to the merged result of both ParameterTree's)
    if (circle_prms->getNumParameters() > 3) {
        validateParameter<std::string>(*circle_prms, "edges_", "0");
    }

    // Verify that we can work with extensions on 'top.core0.dispatch.baz_node', which
    // was added to this example simulator to reproduce bug
    sparta::TreeNode * baz_node = getRoot()->getChild("cpu.core0.dispatch.baz_node", false);
    if (baz_node) {
        sparta::TreeNode::ExtensionsBase * extensions = baz_node->getExtension("baz_ext");
        if (extensions) {
            const sparta::ParameterSet * baz_prms = extensions->getParameters();
            sparta_assert(baz_prms != nullptr);
            validateParameter<std::string>(*baz_prms, "ticket_", "663");
        }
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
