# Sparta HDF5 Logging Example

This is C++ code copied from a simulator that shows how one could capture hdf5 data with a specific schema. In this case, two tables are populated to capture events associated with training a perceptron-based branch predictor.

After integrating code like this into a Sparta-based simulator, command line options must be used to enable its logging. If this `HeatMapTracer` class were instantiated with a `reference_node` pointing to _"top.core0.fetch"_, then enabling the capture of data through this instance would be done with the following Sparta simulator command line options:

    <simulator> --simdb-dir /tmp/my-hdf5-output --simdb-enabled-components top.core0.fetch -- <traces to run if applicable>

Use the following code as a guide only. Since it was copied from a simulator and slightly modified it may not compile as-is. The exact data being logged by this code will almost certainly serve little use for other applications.

~~~~
#include "sparta/TreeNode.h"
#include "sparta/Clock.h"
#include "sparta/log/MessageSource.h"
#include "sparta/ParameterSet.h"
#include "sparta/DatabaseInterface.h"
#include "simdb/ObjectManager.h"
#include "simdb/ObjectRef.h"
#include "simdb/TableRef.h"
#include "simdb/Errors.h"
#include "simdb/impl/hdf5/HDF5ConnProxy.h"
#include "simdb/schema/DatabaseRoot.h"

#include <iostream>
#include <array>
#include <functional>
#include <utility>

// Substitute for enum from original simulator
enum class BranchState {
    EMPTY,
    AT,
    ANT,
    TNT
};


/**
 * Trace and log shp branch training and heatmap information through simdb to an HDF5 file.
 */
class HeatMapTracer {

    // SimDB HDF5 interface

    std::shared_ptr<simdb::ObjectManager::ObjectDatabase> obj_db;
    std::unique_ptr<simdb::TableRef> tbl_training_events;
    std::unique_ptr<simdb::TableRef> tbl_weight_updates;


    // Branch training event states

    uint32_t next_training_event_index = 0;
    uint32_t next_weight_update_index = 0;


    // Constant branch training heatmap attributes

    uint32_t tables;
    uint32_t rows;
    uint32_t banks;

public:

    /**
     * Construct an SHP HeatMap binary trace logger
     * @param sim Simulation instance (for accessing database)
     * @param tables Number of tables which will be addressed
     * @param rows Number of rows which will be addressed
     * @param banks Number of banks which will be addressed
     * @note: If row sizes vary between tables, use the number of rows in the largest table. Same for banks
     */
    HeatMapTracer(sparta::TreeNode const * reference_node, uint32_t _tables, uint32_t _rows, uint32_t _banks)
        : tables(_tables), rows(_rows), banks(_banks)
    {
        // Register some configuration for logging. This can be done in a global setup function if one exists.

        // Make sure HDF5 logging is associated with our 'BranchPredictor' namespace.
        // Note that this is case-insensitive so it will show up in the h5 file as "branchpredictor.<tablename>".
        // Note that we generally want to call this only once. If two instances of this HeatMapTracer were created,
        // then they would both be populating to the 'branchpredictor' namespace in the same tables. This is currently
        // an unresolved issue. Adding meta-tables and table-name suffixes is one potential solution.
        REGISTER_SIMDB_NAMESPACE(branchpredictor, HDF5);

        // Define our schema in a function.
        // If this is called a second time (or more), simdb is supposed to create the union of the tables found in each
        // schema. If any tables with identical names are found in both schemas but the table structure differs, it is
        // an error! This functionality is untested.
        REGISTER_SIMDB_SCHEMA_BUILDER(branchpredictor, [](simdb::Schema & schema) {
            using dt = simdb::ColumnDataType;

            // Branch training event. A row is written each time shp is trained on a new branch. This is typically
            // followed by rows in "weight_updates" row for each table access associated with this training event.
            schema.addTable("training_events")
                    .addColumn("trn_idx",                      dt::uint32_t) // Index of this event
                    .addColumn("cycle",                        dt::uint64_t)
                    .addColumn("pc",                           dt::uint64_t)
                    .addColumn("correct",                      dt::uint8_t)
                    .addColumn("taken",                        dt::uint8_t)
                    .addColumn("tgt",                          dt::uint64_t)
                    .addColumn("yout",                         dt::int32_t)
                    .addColumn("bias_at_lookup",               dt::int16_t)
                    .addColumn("theta_at_training",            dt::int32_t)
                    .addColumn("bias_at_training",             dt::int16_t)
                    .addColumn("shpq_weights_found",           dt::uint8_t)
                    .addColumn("dynamic_state",                dt::int8_t)
                    .addColumn("indirect",                     dt::uint8_t)
                    .addColumn("uncond",                       dt::uint8_t)
                    .addColumn("instructions",                 dt::uint64_t)

                    // Index of latest weight update as of this branch.
                    // If this branch has no weight update events, this points to the
                    // next weight update index in the table following this branch
                    .addColumn("latest_weight_update_index",   dt::uint32_t);

            // Weight update events. A row will be written each time a weight cell in the shp table is updated.
            schema.addTable("weight_updates")
                    .addColumn("wup_idx",       dt::uint32_t) // Index of this event
                    .addColumn("table",         dt::int32_t) // table affected by this update
                    .addColumn("row",           dt::int32_t) // row affected by this update
                    .addColumn("bank",          dt::int32_t) // bank affected by this update
                    .addColumn("lookup_weight", dt::int16_t)
                    .addColumn("new_weight",    dt::int16_t)
                    .addColumn("d_weight",      dt::int16_t)
                    .addColumn("d_unique",      dt::int8_t)
                    .addColumn("thrash_1",      dt::int8_t)
                    .addColumn("write",         dt::int8_t)
                    .addColumn("branch_index",  dt::uint32_t); // Index for branch training event

            // Some meta-data about this heatmap
            schema.addTable("bp-meta")
                    .addColumn("min_row",       dt::int32_t)
                    .addColumn("max_row",       dt::int32_t)
                    .addColumn("min_table",     dt::int32_t)
                    .addColumn("max_table",     dt::int32_t)
                    .addColumn("min_bank",      dt::int32_t)
                    .addColumn("max_bank",      dt::int32_t);

        });

        // Get our database connection if logging is enabled for this node (fetch). Otherwise returns null (for now).
        // Later this might return a proxy that allows logging for this node to be turned on and off throughout
        // simulation instead of checking once here at construction.
        obj_db = GET_DB_FOR_COMPONENT(branchpredictor, reference_node);

        if (obj_db) {
            // Store references to particular tables so we don't have to look them up in the critical path.
            tbl_training_events = obj_db->getTable("training_events");
            tbl_weight_updates = obj_db->getTable("weight_updates");

            // TODO: Will need HDF5 support for storing table,row,bank automatically
            auto meta = obj_db->getTable("bp-meta");
            meta->createObjectWithVals(0, rows - 1, 0, tables - 1, 0, banks - 1);
        }
    }

    HeatMapTracer() = delete;
    HeatMapTracer(const HeatMapTracer&) = delete;
    HeatMapTracer& operator=(const HeatMapTracer&) = delete;

    // Is logging through this object enabled
    bool isEnabled() const {
        return obj_db != nullptr;
    }

    // Log a branch training event
    void logTrain(sparta::Clock::Cycle cycle,
                  uint64_t pc,
                  int8_t correct, // was prediction correct
                  int8_t taken, // was branch actually taken
                  uint64_t target, // Target (if indirect ?)
                  int32_t yout, // yout at prediction time
                  int16_t bias_at_lookup, // bias at lookup-time
                  int32_t theta_at_train, // theta at training-time
                  int16_t bias_at_train, // bias at training-time
                  int8_t shpq_weights_found, // Were weights found in SHPQ
                  BranchState dynamic_state,
                  int8_t indirect,
                  int8_t uncond,
                  uint64_t instructions) // instruction number
    {
        if (!isEnabled())
            return;

        // Encode the dynamic_state enum into an int. SimDB+HDF5 does not understand enums right now
        int8_t dstate;
        switch (dynamic_state) {
            case BranchState::EMPTY: dstate = 0; break;
            case BranchState::AT:    dstate = 1; break;
            case BranchState::ANT:   dstate = 2; break;
            case BranchState::TNT:   dstate = 3; break;
            default:
                sparta_assert_context(0, "Unknown dynamic state: " << static_cast<int>(dynamic_state));
        }

        // Note: arguments and types must exactly match schema for this table. There is no implicit conversion!
        tbl_training_events->createObjectWithVals(
            next_training_event_index++,
            (uint64_t) cycle,
            pc,
            correct,
            taken,
            (uint64_t)target,
            yout,
            bias_at_lookup,
            theta_at_train,
            bias_at_train,
            shpq_weights_found,
            dstate,
            indirect,
            uncond,
            instructions,
            next_weight_update_index
            );
    }

    // Log a weight update event (after the associated branch training event)
    void logWrite(int32_t table, int32_t row, int32_t bank, int16_t lookup_weight, int16_t new_weight, int16_t d_weight,
                  int8_t d_unique, int8_t thrash_1)
    {
        if (!isEnabled())
            return;

        // Note: arguments and types must exactly match schema for this table. There is no implicit conversion!
        tbl_weight_updates->createObjectWithVals(
            next_weight_update_index++,
            table,
            row,
            bank,
            lookup_weight,
            new_weight,
            d_weight,
            d_unique,
            thrash_1,
            (int8_t)1, // Save 1 for write to make it easier to track writes to each shp table cell
            next_training_event_index - 1 // Tracking latest training event
            );
    }
};
