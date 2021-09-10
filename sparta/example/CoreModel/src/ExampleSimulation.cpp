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
#include "sparta/report/DatabaseInterface.hpp"
#include "simdb/schema/Schema.hpp"
#include "simdb/TableProxy.hpp"
#include "simdb/async/AsyncTaskEval.hpp"
#include "simdb/impl/sqlite/SQLiteConnProxy.hpp"
#include "simdb/impl/hdf5/HDF5ConnProxy.hpp"
#include "simdb/utils/uuids.hpp"
#include "simdb/utils/ObjectQuery.hpp"

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

namespace {

    // Struct for writing and verifying SQLite records.
    // See buildSchemaA() below.
    struct TestSQLiteSchemaA {
        struct Numbers {
            double First;
            double Second;
        };
        Numbers numbers;

        struct Metadata {
            std::string Name;
            double Value;
        };
        Metadata metadata;

        static TestSQLiteSchemaA createRandom() {
            TestSQLiteSchemaA s;
            s.numbers.First = rand() / 1000 * 3.14;
            s.numbers.Second = rand() / 1000 * 3.14;
            s.metadata.Name = simdb::generateUUID();
            s.metadata.Value = rand() / 1000 * 3.14;
            return s;
        }
    };

    // Another struct for writing and verifying SQLite
    // records. See buildSchemaB() below.
    struct TestSQLiteSchemaB {
        struct Strings {
            std::string First;
            std::string Second;
        };
        Strings strings;

        struct Metadata {
            std::string Name;
            std::string Value;
        };
        Metadata metadata;

        static TestSQLiteSchemaB createRandom() {
            TestSQLiteSchemaB s;
            s.strings.First = simdb::generateUUID();
            s.strings.Second = simdb::generateUUID();
            s.metadata.Name = simdb::generateUUID();
            s.metadata.Value = simdb::generateUUID();
            return s;
        }
    };

    // Struct for writing and verifying HDF5 records
    struct TestHDF5SchemaC {
        double x;
        double y;
        uint16_t z;

        static TestHDF5SchemaC createRandom() {
            TestHDF5SchemaC s;
            s.x = rand() / 1000 * 3.14;
            s.y = rand() / 1000 * 3.14;
            s.z = rand();
            return s;
        }
    };
}

namespace sparta_simdb {

    // Helper class which creates random SQLite / HDF5
    // structs for SimDB writes, and stores the structs
    // in memory too. The data will be read back from
    // the database at the end of simulation, and the
    // values retrieved from file will be compared with
    // the values that were stored in memory.
    class DatabaseTester {
    public:
        DatabaseTester() = default;
        ~DatabaseTester() = default;

        const TestSQLiteSchemaA & createAndStoreRecordForSQLiteSchemaA() {
            if (records_schemaA_.size() < 100) {
                indices_schemaA_.emplace_back(records_schemaA_.size());
                records_schemaA_.emplace_back(TestSQLiteSchemaA::createRandom());
                return records_schemaA_.back();
            } else {
                indices_schemaA_.emplace_back(rand() % records_schemaA_.size());
                return records_schemaA_[indices_schemaA_.back()];
            }
        }

        const TestSQLiteSchemaB & createAndStoreRecordForSQLiteSchemaB() {
            if (records_schemaB_.size() < 100) {
                indices_schemaB_.emplace_back(records_schemaB_.size());
                records_schemaB_.emplace_back(TestSQLiteSchemaB::createRandom());
                return records_schemaB_.back();
            } else {
                indices_schemaB_.emplace_back(rand() % records_schemaB_.size());
                return records_schemaB_[indices_schemaB_.back()];
            }
        }

        const TestHDF5SchemaC & createAndStoreRecordForHDF5SchemaC() {
            records_schemaC_.emplace_back(TestHDF5SchemaC::createRandom());
            return records_schemaC_.back();
        }

        const std::vector<TestSQLiteSchemaA> & getWrittenRecordsForSchemaA() const {
            return records_schemaA_;
        }

        const std::vector<TestSQLiteSchemaB> & getWrittenRecordsForSchemaB() const {
            return records_schemaB_;
        }

        const std::vector<TestHDF5SchemaC> & getWrittenRecordsForSchemaC() const {
            return records_schemaC_;
        }

        void verifyRecords(const std::string & db_file) const {
            simdb::ObjectManager obj_mgr(".");
            if (!obj_mgr.connectToExistingDatabase(db_file)) {
                return;
            }

            auto numeric_db = GET_DB_FROM_CURRENT_SIMULATION(NumericMeta);
            if (numeric_db) {
                auto values_query =
                    numeric_db->createObjectQueryForTable("Numbers");

                if (values_query) {
                    double first = 0, second = 0;
                    values_query->writeResultIterationsTo(
                        "First", &first, "Second", &second);

                    if (values_query->countMatches() != indices_schemaA_.size()) {
                        throw sparta::SpartaException("Could not verify SimDB records");
                    }

                    auto result_iter = values_query->executeQuery();
                    size_t record_idx = 0;
                    while (result_iter->getNext()) {
                        const auto & expected = records_schemaA_[indices_schemaA_[record_idx]];
                        if (first != expected.numbers.First) {
                            throw sparta::SpartaException("Could not verify SimDB records");
                        }
                        if (second != expected.numbers.Second) {
                            throw sparta::SpartaException("Could not verify SimDB records");
                        }
                        ++record_idx;
                    }
                }

                auto meta_query =
                    numeric_db->createObjectQueryForTable("Metadata");
                if (meta_query) {
                    std::string name;
                    double value = 0;
                    meta_query->writeResultIterationsTo("Name", &name, "Value", &value);

                    if (meta_query->countMatches() != indices_schemaA_.size()) {
                        throw sparta::SpartaException("Could not verify SimDB records");
                    }

                    auto result_iter = meta_query->executeQuery();
                    size_t record_idx = 0;
                    while (result_iter->getNext()) {
                        const auto & expected = records_schemaA_[indices_schemaA_[record_idx]];
                        if (name != expected.metadata.Name) {
                            throw sparta::SpartaException("Could not verify SimDB records");
                        }
                        if (value != expected.metadata.Value) {
                            throw sparta::SpartaException("Could not verify SimDB records");
                        }
                        ++record_idx;
                    }
                }
            }
        }

    private:
        std::vector<TestSQLiteSchemaA> records_schemaA_;
        std::vector<TestSQLiteSchemaB> records_schemaB_;
        std::vector<TestHDF5SchemaC> records_schemaC_;
        std::vector<uint16_t> indices_schemaA_;
        std::vector<uint16_t> indices_schemaB_;
        std::vector<uint16_t> indices_schemaC_;
    };
}

namespace {

    // Schema builder to test two simdb::ObjectManager's
    // bound to the same database file, separated in that
    // same file by their respective application name.
    // A third schema builder is for another ObjectManager,
    // though it will be used to write records to an HDF5
    // database, and therefore will be in its own file.
    // SimDB's worker thread should be able to keep them
    // separated into two groups: one group for the two
    // SQLite database connections, and one group only
    // serving the one HDF5 connection.
    //
    // Note that the two schema builders below have some
    // overlap in their table definitions: schemaA and
    // schemaB have some of the same table names, but
    // these tables have different column configurations.
    // This should not be a problem for ObjectManager
    // since it will use its unique application name
    // with the table names we give it to create a
    // unique schema inside the shared file, separated
    // from other applications tied to the same file.
    // The specific way in which the schemas are kept
    // separate in the file is not our concern; the
    // DbConnProxy subclasses take care of those
    // specifics.
    void buildSchemaA(simdb::Schema & schema)
    {
        using dt = simdb::ColumnDataType;

        schema.addTable("Numbers")
            .addColumn("First", dt::double_t)
            .addColumn("Second", dt::double_t);

        schema.addTable("Metadata")
            .addColumn("Name", dt::string_t)
            .addColumn("Value", dt::double_t);
    }

    void buildSchemaB(simdb::Schema & schema)
    {
        using dt = simdb::ColumnDataType;

        schema.addTable("Strings")
            .addColumn("First", dt::string_t)
            .addColumn("Second", dt::string_t);

        schema.addTable("Metadata")
            .addColumn("Name", dt::string_t)
            .addColumn("Value", dt::string_t);
    }

    void buildSchemaC(simdb::Schema & schema)
    {
        using dt = simdb::ColumnDataType;

        schema.addTable("Numbers")
            .addField("x", dt::double_t, FOFFSET(TestHDF5SchemaC,x))
            .addField("y", dt::double_t, FOFFSET(TestHDF5SchemaC,y))
            .addField("z", dt::uint16_t, FOFFSET(TestHDF5SchemaC,z));
    }

    simdb::DbConnProxy * createSQLiteProxy()
    {
        return new simdb::SQLiteConnProxy;
    }

    simdb::DbConnProxy * createHDF5Proxy()
    {
        return new simdb::HDF5ConnProxy;
    }
}

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
          checkDbAccess();
      }

      void checkDbAccess(const bool stop_checking = false) {
          if (stop_checking_db_access_) {
              return;
          }
          if (auto dbconn = GET_DB_FOR_COMPONENT(Stats, this)) {
              //Run a simple query against the database just to verify
              //the connection is open and accepting requests
              (void) dbconn->findObject("ObjectManagersInDatabase", 1);
              stop_checking_db_access_ = stop_checking;
          }
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
      bool stop_checking_db_access_ = false;
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

void tryAccessSimDB()
{
    if (auto dbconn = GET_DB_FROM_CURRENT_SIMULATION(Stats)) {
        //Run a simple query against the database just to verify
        //the connection is open and accepting requests
        (void) dbconn->findObject("ObjectManagersInDatabase", 1);
    }
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
    show_factories_(show_factories),
    simdb_tester_(std::make_shared<sparta_simdb::DatabaseTester>())
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

    //SQLite namespaces: NumericMeta & StringMeta
    REGISTER_SIMDB_NAMESPACE(NumericMeta, SQLite);
    REGISTER_SIMDB_SCHEMA_BUILDER(NumericMeta, buildSchemaA);

    REGISTER_SIMDB_NAMESPACE(StringMeta,  SQLite);
    REGISTER_SIMDB_SCHEMA_BUILDER(StringMeta, buildSchemaB);

    //HDF5 namespace: NumericVals
    REGISTER_SIMDB_NAMESPACE(NumericVals, HDF5);
    REGISTER_SIMDB_SCHEMA_BUILDER(NumericVals, buildSchemaC);

    //Proxy factory registration
    REGISTER_SIMDB_PROXY_CREATE_FUNCTION(HDF5, createHDF5Proxy);
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

    if (simdb_perf_async_ctrl_enabled_) {
        std::set<std::string> simdb_files;
        if (auto dbconn = GET_DB_FOR_COMPONENT(NumericMeta, this)) {
            simdb_files.insert(dbconn->getDatabaseFile());
        }

        for (const auto & db_file : simdb_files) {
            simdb_tester_->verifyRecords(db_file);
        }
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
    //Context-aware SimDB access
    std::pair<std::string, std::string> sqlite_db_files;
    if (auto dbconn = GET_DB_FOR_COMPONENT(NumericMeta, this)) {
        const TestSQLiteSchemaA data = simdb_tester_->
            createAndStoreRecordForSQLiteSchemaA();

        const double first = data.numbers.First;
        const double second = data.numbers.Second;
        dbconn->getTable("Numbers")->createObjectWithArgs(
            "First", first, "Second", second);

        const std::string meta_name = data.metadata.Name;
        const double meta_value = data.metadata.Value;
        dbconn->getTable("Metadata")->createObjectWithArgs(
            "Name", meta_name, "Value", meta_value);

        sqlite_db_files.first = dbconn->getDatabaseFile();

        //Verification of the two records we just made above
        //will occur at the end of the simulation.
    }

    if (auto dbconn = GET_DB_FOR_COMPONENT(StringMeta, this)) {
        const TestSQLiteSchemaB data = simdb_tester_->
            createAndStoreRecordForSQLiteSchemaB();

        const std::string first = data.strings.First;
        const std::string second = data.strings.Second;
        dbconn->getTable("Strings")->createObjectWithArgs(
            "First", first, "Second", second);

        const std::string meta_name = data.metadata.Name;
        const std::string meta_value = data.metadata.Value;
        dbconn->getTable("Metadata")->createObjectWithArgs(
            "Name", meta_name, "Value", meta_value);

        sqlite_db_files.second = dbconn->getDatabaseFile();

        //Verification of the two records we just made above
        //will occur at the end of the simulation.
    }

    //Both of the ObjectManager's used above should have put the
    //created records into the same file.
    sparta_assert(sqlite_db_files.first == sqlite_db_files.second);

    //Context-unaware SimDB access
    tryAccessSimDB();

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

    simdb_perf_async_ctrl_enabled_ = sparta::IsFeatureValueEnabled(
        getFeatureConfiguration(), "simdb-perf-async-ctrl") > 0;
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

    lazy_table_create_trigger_.reset(new sparta::trigger::ExpressionTrigger(
        "DelayedTableCreate",
        CREATE_SPARTA_HANDLER(ExampleSimulator, addToStatsSchema_),
        "top.cpu.core0.rob.stats.total_number_retired >= 12000",
        getRoot()->getSearchScope(),
        nullptr));

    if (auto db_root = GET_DB_FROM_CURRENT_SIMULATION(Stats)) {
        lazy_table_proxy_ = db_root->getConditionalTable("Lazy");
        sparta_assert(lazy_table_proxy_ != nullptr);
        sparta_assert(lazy_table_proxy_->getTable() == nullptr);
    }

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

    if (dispatch_baz_) {
        dispatch_baz_->checkDbAccess(true);
    }

    if (!simdb_perf_async_ctrl_enabled_) {
        return;
    }

    using ObjectDatabase = simdb::ObjectManager::ObjectDatabase;

    // In the SimDB-related code below, note that GET_DB_FOR_COMPONENT is
    // returning a unique_ptr<ObjectDatabase>, not a shared_ptr.
    //
    // The ability to request database connections and get unique_ptr's
    // back is important because it demonstrates that different parts
    // of the simulator can write data into the same database, into their
    // own namespace's schema, sharing the same worker thread (which is
    // just implementation detail, but it's important for performance and
    // scalability) with no coordination required between the simulator
    // components / call sites.
    //
    // Also note that we have a mixture of DB writes going on here. There
    // are two separate physical database files: one is SQLite, and the
    // other is HDF5. The SQLite file has two namespaces in it, named
    // NumericMeta and StringMeta; the HDF5 file just has one namespace
    // in it called NumericVals. These namespaces, their database formats,
    // and the namespace schema definition was registered with SimDB from
    // the ExampleSimulator's constructor earlier on.

    if (auto obj_db = GET_DB_FOR_COMPONENT(NumericMeta, this)) {
        // Helper class which writes a data record on the worker thread
        class TestWriter : public simdb::WorkerTask
        {
        public:
            TestWriter(ObjectDatabase * obj_db,
                       sparta_simdb::DatabaseTester * db_tester) :
                obj_db_(obj_db),
                simdb_tester_(db_tester)
            {}

            void completeTask() override {
              const TestSQLiteSchemaA data = simdb_tester_->
                  createAndStoreRecordForSQLiteSchemaA();

                obj_db_->getTable("Numbers")->createObjectWithArgs(
                    "First", data.numbers.First,
                    "Second", data.numbers.Second);

                obj_db_->getTable("Metadata")->createObjectWithArgs(
                    "Name", data.metadata.Name,
                    "Value", data.metadata.Value);
            }

        private:
            ObjectDatabase * obj_db_ = nullptr;
            sparta_simdb::DatabaseTester * simdb_tester_ = nullptr;
        };

        std::unique_ptr<simdb::WorkerTask> task(new TestWriter(
            obj_db, simdb_tester_.get()));
        obj_db->getTaskQueue()->addWorkerTask(std::move(task));
    }

    if (auto obj_db = GET_DB_FOR_COMPONENT(StringMeta, this)) {
        // Helper class which writes a data record on the worker thread
        class TestWriter : public simdb::WorkerTask
        {
        public:
            TestWriter(ObjectDatabase * obj_db,
                       sparta_simdb::DatabaseTester * db_tester) :
                obj_db_(obj_db),
                simdb_tester_(db_tester)
            {}

            void completeTask() override {
                const TestSQLiteSchemaB data = simdb_tester_->
                    createAndStoreRecordForSQLiteSchemaB();

                obj_db_->getTable("Strings")->createObjectWithArgs(
                    "First", data.strings.First,
                    "Second", data.strings.Second);

                obj_db_->getTable("Metadata")->createObjectWithArgs(
                    "Name", data.metadata.Name,
                    "Value", data.metadata.Value);
            }

        private:
            ObjectDatabase * obj_db_ = nullptr;
            sparta_simdb::DatabaseTester * simdb_tester_ = nullptr;
        };

        std::unique_ptr<simdb::WorkerTask> task(new TestWriter(
            obj_db, simdb_tester_.get()));
        obj_db->getTaskQueue()->addWorkerTask(std::move(task));
    }

    if (auto obj_db = GET_DB_FOR_COMPONENT(NumericVals, this)) {
        // Helper class which writes a data record on the worker thread
        class TestWriter : public simdb::WorkerTask
        {
        public:
            TestWriter(ObjectDatabase * obj_db,
                       sparta_simdb::DatabaseTester * db_tester) :
                obj_db_(obj_db),
                simdb_tester_(db_tester)
            {}

            void completeTask() override {
                const TestHDF5SchemaC data = simdb_tester_->
                    createAndStoreRecordForHDF5SchemaC();

                obj_db_->getTable("Numbers")->createObjectWithVals(
                    data.x, data.y, data.z);
            }

        private:
            ObjectDatabase * obj_db_ = nullptr;
            sparta_simdb::DatabaseTester * simdb_tester_ = nullptr;
        };

        std::unique_ptr<simdb::WorkerTask> task(new TestWriter(
            obj_db, simdb_tester_.get()));
        obj_db->getTaskQueue()->addWorkerTask(std::move(task));
    }
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

void ExampleSimulator::addToStatsSchema_()
{
    if (auto db_root = getDatabaseRoot()) {
        if (auto db_namespace = db_root->getNamespace("Stats")) {
            db_namespace->addToSchema([&](simdb::Schema & schema) {
                using dt = simdb::ColumnDataType;

                schema.addTable("Lazy")
                    .addColumn("Foo", dt::string_t)
                    .addColumn("Bar", dt::int32_t);
            });

            lazy_table_create_trigger_.reset(new sparta::trigger::ExpressionTrigger(
                "DelayedTableCreate",
                CREATE_SPARTA_HANDLER(ExampleSimulator, addToLazySchemaTable_),
                "top.cpu.core0.rob.stats.total_number_retired >= 40000",
                getRoot()->getSearchScope(),
                nullptr));
        }
    }
}

void ExampleSimulator::addToLazySchemaTable_()
{
    if (lazy_table_proxy_->isWritable()) {
        const std::string foo = "hello_world";
        const int bar = 45;

        auto recordA = lazy_table_proxy_->getTable()->createObjectWithArgs(
            "Foo", foo, "Bar", bar);

        auto db_root = GET_DB_FROM_CURRENT_SIMULATION(Stats);
        sparta_assert(db_root != nullptr);

        auto recordB = db_root->getTable("Lazy")->createObjectWithArgs(
            "Foo", foo, "Bar", bar);

        sparta_assert(recordA->getPropertyString("Foo") ==
                    recordB->getPropertyString("Foo"));

        sparta_assert(recordA->getPropertyInt32("Bar") ==
                    recordB->getPropertyInt32("Bar"));
    }
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

// Since the FlushManager does not have a subsequent source file
namespace core_example {
    constexpr char FlushManager::name[];
}
