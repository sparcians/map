// <Simulation.h> -*- C++ -*-


#pragma once

#include "sparta/app/Simulation.hpp"
#include "sparta/trigger/ExpiringExpressionTrigger.hpp"

namespace sparta {
    class Baz;
    class ParameterSet;
}
namespace sparta_simdb {
    class DatabaseTester;
}

namespace sparta { namespace trigger { class ExpressionTrigger; } }
namespace core_example{ class CPUFactory; }

/*!
 * \brief ExampleSimulator which builds the model and configures it
 */
class ExampleSimulator : public sparta::app::Simulation
{
public:

    /*!
     * \brief Construct ExampleSimulator
     * \param num_cores Number of cores to instantiate any nodes
     *                  created which match the description as they
     *                  are created
     * \param instruction_limit The maximum number of instructions to
     *                          run.  0 means no limit
     * \param show_factories Print the registered factories to stdout
     */
    ExampleSimulator(const std::string& topology,
                     sparta::Scheduler & scheduler,
                     uint32_t num_cores=1, uint64_t instruction_limit=0,
                     bool show_factories = false);

    // Tear it down
    virtual ~ExampleSimulator();

private:

    //////////////////////////////////////////////////////////////////////
    // Setup

    //! Method to register (cycle)histogram nodes with custom user
    //! methods using the REGISTER_HISTOGRAM_STAT_CALC_FCN
    void registerStatCalculationFcns_();

    //! Build the tree with tree nodes, but does not instantiate the
    //! unit yet
    void buildTree_() override;

    //! Configure the tree and apply any last minute parameter changes
    void configureTree_() override;

    //! The tree is now configured, built, and instantiated.  We need
    //! to bind things together.
    void bindTree_() override;

    //! Callback fired when a report warmup period has elapsed
    void reportGenerationStarted_(const uint64_t &);
    bool report_generation_fired_ = false;

    //! Additional nodes and parameters required to reproduce bug
    std::unique_ptr<sparta::Baz> dispatch_baz_;
    std::unique_ptr<sparta::Baz> fpu_baz_;

    //! This method is used to support command line options like --report-warmup-icount
    const sparta::CounterBase* findSemanticCounter_(CounterSemantic sem) const override;

    //////////////////////////////////////////////////////////////////////
    // Runtime

    //! Custom callbacks for simulation control
    class ExampleController : public sparta::app::Simulation::SimulationController
    {
    public:
        explicit ExampleController(const sparta::app::Simulation * sim);

    private:
        virtual void pause_    (const sparta::app::Simulation * sim) override;
        virtual void resume_   (const sparta::app::Simulation * sim) override;
        virtual void terminate_(const sparta::app::Simulation * sim) override;

        void customEatCallback_();
        void customSleepCallback_();
    };

    //! Name of the topology to build
    std::string cpu_topology_;

    //! Number of cores in this simulator. Temporary startup option
    const uint32_t num_cores_;

    //! Instruction limit (set up -i option on command line)
    const uint64_t instruction_limit_;

    std::vector<sparta::TreeNode*> tlb_nodes_;

    /*!
     * \brief A notification source for testing purposes
     */
    std::unique_ptr<sparta::NotificationSource<uint64_t>> testing_notification_source_;
    std::unique_ptr<sparta::trigger::ExpressionTrigger> random_number_trigger_;
    void postRandomNumber_();

    /*!
     * \brief A notification source for testing purposes (toggle triggers specifically)
     */
    std::unique_ptr<sparta::NotificationSource<uint64_t>> toggle_trigger_notification_source_;
    std::unique_ptr<sparta::trigger::ExpressionTrigger> toggle_notif_trigger_;
    void postToToggleTrigger_();

    /*!
     * \brief Trigger which adds a table to the 'Stats' database namespace
     * during simulation, and a TableProxy object referring to that table
     * which is cached before the table actually exists in the schema.
     */
    sparta::trigger::ExpiringExpressionTrigger lazy_table_create_trigger_;
    simdb::TableProxy * lazy_table_proxy_ = nullptr;
    void addToStatsSchema_();
    void addToLazySchemaTable_();

    /*!
     * \brief Notification source and dedicated warmup listeners used to mimic
     * legacy report start events.
     */
    std::unique_ptr<sparta::NotificationSource<uint64_t>> legacy_warmup_report_starter_;
    std::vector<std::unique_ptr<sparta::trigger::ExpressionTrigger>> core_warmup_listeners_;
    uint32_t num_cores_still_warming_up_ = 0;
    void onLegacyWarmupNotification_();

    /*!
     * \brief An "on triggered" callback for testing purposes
     */
    void onTriggered_(const std::string & msg);
    bool on_triggered_notifier_registered_ = false;

    /*!
     * \brief If present, test tree node extensions
     */
    void validateTreeNodeExtensions_();

    /*!

     * \brief Get the factory for topology build
     */
    auto getCPUFactory_() -> core_example::CPUFactory*;

    /*!
     * \brief Optional flag to print registered factories to console
     */
    bool show_factories_;

    /* \brief Flag which enables SimDB-related code to run for
     * interactive performance benchmarks / comparison. False
     * by default so that we don't impact unit testing / smoke
     * testing times for all regression test runs.
     */
    bool simdb_perf_async_ctrl_enabled_ = false;

    /*!
     * \brief Tester class which holds onto data structures that
     * are randomly generated and written to SimDB during the
     * simulation, verifying the contents at the end of the
     * simulation for accuracy. Defined in this class' CPP file.
     */
    std::shared_ptr<sparta_simdb::DatabaseTester> simdb_tester_;
};
