#include <inttypes.h>
#include <cstdio>
#include <iostream>
#include <stack>
#include <ctime>
#include <array>

#include "sparta/sparta.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/log/Tap.hpp"
#include "sparta/log/Destination.hpp"
#include "sparta/functional/Register.hpp"
#include "sparta/functional/RegisterSet.hpp"
#include "sparta/memory/MemoryObject.hpp"
#include "sparta/serialization/checkpoint/DatabaseCheckpointer.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "simdb/utils/TickTock.hpp"

#include "simdb/apps/AppManager.hpp"
#include "simdb/sqlite/DatabaseManager.hpp"
#include "simdb/pipeline/Pipeline.hpp"

/*!
 * \file DatabaseCheckpoint_test.cpp
 * \brief Test for SimDB-backed Checkpoints
 *
 * This is modified from FastCheckpoint_test.cpp.
 *
 * Register is built on DataView and RegisterSet is built on ArchData.
 * The DataView test performs extensive testing so some test-cases related
 * to register sizes and layouts may be omitted from this test.
 */

TEST_INIT

using sparta::Register;
using sparta::RegisterSet;
using sparta::RootTreeNode;
using sparta::memory::MemoryObject;
using sparta::memory::BlockingMemoryObjectIFNode;
using sparta::serialization::checkpoint::DatabaseCheckpointer;
using sparta::serialization::checkpoint::DatabaseCheckpoint;

static const uint16_t HINT_NONE=0;

//! Some register and field definition tables
Register::Definition reg_defs[] = {
    { 0, "reg0", Register::GROUP_NUM_NONE, "", Register::GROUP_IDX_NONE, "reg desc", 1,
        {}, {}, nullptr, Register::INVALID_ID, 0, nullptr, HINT_NONE, 0 },
    { 1, "reg1", Register::GROUP_NUM_NONE, "", Register::GROUP_IDX_NONE, "reg desc", 2,
        {}, {}, nullptr, Register::INVALID_ID, 0, nullptr, HINT_NONE, 0 },
    { 2, "reg2", Register::GROUP_NUM_NONE, "", Register::GROUP_IDX_NONE, "reg desc", 4,
        {}, {}, nullptr, Register::INVALID_ID, 0, nullptr, HINT_NONE, 0 },
    { 3, "reg3", Register::GROUP_NUM_NONE, "", Register::GROUP_IDX_NONE, "reg desc", 8,
        {}, {}, nullptr, Register::INVALID_ID, 0, nullptr, HINT_NONE, 0 },
    { 4, "reg4", Register::GROUP_NUM_NONE, "", Register::GROUP_IDX_NONE, "reg desc", 16,
        {}, {}, nullptr, Register::INVALID_ID, 0, nullptr, HINT_NONE, 0 },
    Register::DEFINITION_END
};

//! Dummy device
class DummyDevice : public sparta::TreeNode
{
public:
    DummyDevice(sparta::TreeNode* parent) :
        sparta::TreeNode(parent, "dummy", "", sparta::TreeNode::GROUP_IDX_NONE, "dummy node for checkpoint test")
    {}
};

void RunCheckpointerTest(uint64_t initial_tick = 0)
{
    sparta::Scheduler sched;
    RootTreeNode clocks("clocks");
    sparta::Clock clk(&clocks, "clock", &sched);

    // Create a tree with some register sets and memory
    RootTreeNode root;

    DummyDevice dummy(&root);
    std::unique_ptr<RegisterSet> rset(RegisterSet::create(&dummy, reg_defs));

    DummyDevice dummy2(&dummy);
    std::unique_ptr<RegisterSet> rset2(RegisterSet::create(&dummy2, reg_defs));

    auto r1 = rset->getRegister("reg2");
    auto r2 = rset2->getRegister("reg2");
    assert(r1 != r2);
    r1->write<uint32_t>(0 * 5ul);
    r2->write<uint32_t>(0 % 5ul);

    simdb::DatabaseManager db_mgr("test.db", true);
    simdb::AppManager app_mgr(&db_mgr);

    // Setup...
    app_mgr.getAppFactory<DatabaseCheckpointer>()->setSpartaElems(root, &sched);
    app_mgr.enableApp(DatabaseCheckpointer::NAME);
    app_mgr.createEnabledApps();
    app_mgr.createSchemas();
    app_mgr.postInit(0, nullptr);
    app_mgr.openPipelines();

    auto& dbcp = *app_mgr.getApp<DatabaseCheckpointer>();
    dbcp.setSnapshotThreshold(10);
    dbcp.setMaxCachedWindows(10);

    root.enterConfiguring();
    root.enterFinalized();
    sched.finalize();
    EXPECT_EQUAL(sched.getCurrentTick(), 0);
    EXPECT_TRUE(dbcp.getCheckpointsAt(0).empty());
    EXPECT_EQUAL(dbcp.getNumCheckpoints(), 0);
    EXPECT_EQUAL(dbcp.getNumSnapshots(), 0);
    EXPECT_EQUAL(dbcp.getNumDeltas(), 0);
    EXPECT_TRUE(dbcp.getCheckpointChain(0).empty());

    // Advance the scheduler before taking the head checkpoint
    if (initial_tick > 0) {
        sched.run(initial_tick, true, false);
    }
    EXPECT_EQUAL(sched.getCurrentTick(), initial_tick);

    // CHECKPOINT: Head
    DatabaseCheckpointer::chkpt_id_t head_id;
    EXPECT_NOTHROW(dbcp.createHead());
    head_id = dbcp.getHeadID();
    EXPECT_NOTEQUAL(dbcp.getHead(), nullptr);
    EXPECT_EQUAL(head_id, dbcp.getHead()->getID());
    EXPECT_EQUAL(dbcp.getCurrentID(), head_id);
    EXPECT_EQUAL(dbcp.getCurrentTick(), initial_tick);
    EXPECT_TRUE(dbcp.isSnapshot(head_id));

    std::cout << dbcp.stringize() << std::endl;

    auto step_checkpointer = [&](DatabaseCheckpointer::chkpt_id_t expected_id, bool step_sched = true) {
        r1->write<uint32_t>(expected_id * 5ul);
        r2->write<uint32_t>(expected_id % 5ul);
        if (step_sched) {
            sched.run(1, true, false);
        }

        DatabaseCheckpointer::chkpt_id_t actual_id = DatabaseCheckpoint::UNIDENTIFIED_CHECKPOINT;
        EXPECT_NOTHROW(actual_id = dbcp.createCheckpoint());
        EXPECT_EQUAL(actual_id, expected_id);
        EXPECT_EQUAL(actual_id, dbcp.getCurrentID());
        EXPECT_EQUAL(dbcp.getNumCheckpoints(), expected_id + 1);

        // Should always have the head and current checkpoints in the cache
        EXPECT_TRUE(dbcp.isCheckpointCached(dbcp.getHeadID()));
        EXPECT_TRUE(dbcp.isCheckpointCached(dbcp.getCurrentID()));

        return actual_id;
    };

    auto verif_find_checkpoint = [&](DatabaseCheckpointer::chkpt_id_t id, bool must_exist = true) {
        std::shared_ptr<DatabaseCheckpoint> cp;
        EXPECT_NOTHROW(cp = dbcp.findCheckpoint(id, must_exist));
        if (cp) {
            EXPECT_EQUAL(cp->getID(), id);
            EXPECT_EQUAL(cp->getPrevID(), (id > 0) ? (id - 1) : DatabaseCheckpoint::UNIDENTIFIED_CHECKPOINT);
            EXPECT_EQUAL(cp->isSnapshot(), (id % (dbcp.getSnapshotThreshold() + 1)) == 0);

            if (cp->isSnapshot()) {
                EXPECT_EQUAL(cp->getDistanceToPrevSnapshot(), 0);
            } else {
                EXPECT_EQUAL(cp->getDistanceToPrevSnapshot(), id % (dbcp.getSnapshotThreshold() + 1));
            }
        }
        return cp;
    };

    auto verif_load_chkpt = [&](DatabaseCheckpointer::chkpt_id_t id) {
        EXPECT_NOTHROW(dbcp.loadCheckpoint(id));
        EXPECT_EQUAL(dbcp.getCurrentID(), id);
        EXPECT_EQUAL(dbcp.getNumCheckpoints(), id + 1);
        EXPECT_FALSE(dbcp.hasCheckpoint(id + 1));
        EXPECT_EQUAL(sched.getCurrentTick(), id + initial_tick);

        auto r1_val = r1->read<uint32_t>();
        auto r2_val = r2->read<uint32_t>();
        EXPECT_EQUAL(r1_val, id * 5ul);
        EXPECT_EQUAL(r2_val, id % 5ul);
    };

    auto wait_until_evicted = [&](DatabaseCheckpointer::chkpt_id_t id) {
        size_t num_tries = 0;
        while (dbcp.isCheckpointCached(id) && num_tries < 3) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            ++num_tries;
        }
        EXPECT_FALSE(num_tries == 3);
        EXPECT_FALSE(dbcp.isCheckpointCached(id));
    };

    // Ensure force_snapshot=true always throws. Not supported.
    EXPECT_THROW(dbcp.createCheckpoint(true));

    // Ensure traceValue() throws. Not supported.
    EXPECT_THROW(dbcp.traceValue(std::cout, dbcp.getCurrentID(), nullptr, 0, 4));

    // Create 1000 checkpoints, and periodically access an old one. Also
    // go to sleep sometimes to increase the chances we have to go to the
    // database to retrieve a checkpoint.
    for (uint32_t i = 1; i <= 1000; ++i) {
        // Step the scheduler and take a checkpoint
        step_checkpointer(i);

        // Access most recent from the cache directly
        verif_find_checkpoint(i);

        // Force some of the checkpoints to be retrieved from the database
        if (i % 100 == 0 && i > 250) {
            auto old_id = i - 100;
            wait_until_evicted(old_id);
            verif_find_checkpoint(old_id);
        }
    }

    // Nothing to test, just call dumpList/dumpData/dumpAnnotatedData.
    // Do this while we have a lot of checkpoints in the cache and
    // the database for max code coverage.
    //TODO cnyce: put this back
    //dbcp.dumpList(std::cout);
    //std::cout << std::endl;
    //dbcp.dumpData(std::cout);
    //std::cout << std::endl;
    //dbcp.dumpAnnotatedData(std::cout);
    //std::cout << std::endl;

    // Verify that cached / DB-recreated checkpoints are identical:
    //   1. Get the current checkpoint from the cache
    auto cached_cp1000 = dbcp.findCheckpoint(dbcp.getCurrentID());
    EXPECT_TRUE(dbcp.isCheckpointCached(cached_cp1000->getID()));

    //   2. Write a lot more checkpoints to force the oldest ones out of the cache
    for (uint32_t i = 1001; i <= 1500; ++i) {
        step_checkpointer(i);
    }
    wait_until_evicted(cached_cp1000->getID());

    //   3. Recreate the same checkpoint from the database
    EXPECT_FALSE(dbcp.isCheckpointCached(cached_cp1000->getID()));
    auto recreated_cp1000 = dbcp.findCheckpoint(cached_cp1000->getID());

    EXPECT_NOTEQUAL(cached_cp1000, nullptr);
    EXPECT_NOTEQUAL(recreated_cp1000, nullptr);

    if (cached_cp1000 && recreated_cp1000) {
        std::ostringstream oss1;
        std::ostringstream oss2;

        cached_cp1000->dumpData(oss1);
        recreated_cp1000->dumpData(oss2);

        EXPECT_EQUAL(oss1.str(), oss2.str());
        EXPECT_EQUAL(cached_cp1000->getTotalMemoryUse(), recreated_cp1000->getTotalMemoryUse());
        EXPECT_EQUAL(cached_cp1000->getContentMemoryUse(), recreated_cp1000->getContentMemoryUse());
        EXPECT_TRUE(cached_cp1000->getHistoryChain() == recreated_cp1000->getHistoryChain());
        EXPECT_TRUE(cached_cp1000->getRestoreChain() == recreated_cp1000->getRestoreChain());
        EXPECT_EQUAL(cached_cp1000->getPrevID(), recreated_cp1000->getPrevID());
        EXPECT_EQUAL(cached_cp1000->getNextIDs(), recreated_cp1000->getNextIDs());
        EXPECT_EQUAL(cached_cp1000->getTick(), recreated_cp1000->getTick());
        EXPECT_EQUAL(cached_cp1000->isSnapshot(), recreated_cp1000->isSnapshot());
        EXPECT_EQUAL(cached_cp1000->getDistanceToPrevSnapshot(), recreated_cp1000->getDistanceToPrevSnapshot());
    }

    // Load very recent checkpoints that are definitely in the cache
    for (size_t i = 1500; i > 1475; --i) {
        EXPECT_TRUE(dbcp.isCheckpointCached(i));
        verif_load_chkpt(i);
    }

    // Load checkpoints that have already been evicted from the cache
    for (size_t i = 250; i > 225; --i) {
        wait_until_evicted(i);
    }
    for (size_t i = 250; i > 225; --i) {
        verif_load_chkpt(i);
    }

    // Verify history chain
    auto hist_chain13 = dbcp.getHistoryChain(13);
    for (auto hist_id : {0,1,2,3,4,5,6,7,8,9,10,11,12,13}) {
        EXPECT_FALSE(hist_chain13.empty());
        EXPECT_EQUAL(hist_chain13.top(), hist_id);
        hist_chain13.pop();
    }
    EXPECT_TRUE(hist_chain13.empty());

    // Verify restore chain
    auto rest_chain13 = dbcp.getRestoreChain(13);
    for (auto rest_id : {11,12,13}) {
        EXPECT_FALSE(rest_chain13.empty());
        EXPECT_EQUAL(rest_chain13.top(), rest_id);
        rest_chain13.pop();
    }
    EXPECT_TRUE(rest_chain13.empty());

    // Verify distance to previous snapshot:
    //
    //    Checkpoint ID                 Snapshot?
    //    0                             Yes (head)
    //    1-10                          No
    //    11                            Yes
    //    12-21                         No
    //    22                            Yes
    //    23-32                         No
    //    33                            Yes
    std::shared_ptr<DatabaseCheckpoint> cp;
    EXPECT_NOTHROW(cp = dbcp.findCheckpoint(33, true));
    EXPECT_EQUAL(cp->getDistanceToPrevSnapshot(), 0);
    EXPECT_NOTHROW(cp = dbcp.findCheckpoint(32, true));
    EXPECT_EQUAL(cp->getDistanceToPrevSnapshot(), 10);
    EXPECT_NOTHROW(cp = dbcp.findCheckpoint(22, true));
    EXPECT_EQUAL(cp->getDistanceToPrevSnapshot(), 0);
    EXPECT_NOTHROW(cp = dbcp.findCheckpoint(5, true));
    EXPECT_EQUAL(cp->getDistanceToPrevSnapshot(), 5);

    // Nothing to test, just call dumpRestoreChain()
    dbcp.dumpRestoreChain(std::cout, 32);

    // Go back to checkpoint 1
    verif_load_chkpt(1);

    // Take 3 more checkpoints with IDs 2, 3, and 4
    step_checkpointer(2);
    step_checkpointer(3);
    step_checkpointer(4);

    // Go back to head
    verif_load_chkpt(head_id);

    // Take some checkpoints and ensure that the current ID is always increasing by 1 with no gaps
    step_checkpointer(1);
    step_checkpointer(2);
    step_checkpointer(3);
    verif_load_chkpt(2);
    verif_load_chkpt(1);
    verif_load_chkpt(head_id);

    // Ensure exception is thrown when loading a non-existent checkpoint
    EXPECT_THROW(dbcp.loadCheckpoint(9999));

    // Ensure findCheckpoint() throws when must_exist=true and checkpoint does not exist
    EXPECT_THROW(dbcp.findCheckpoint(9999, true));
    EXPECT_NOTHROW(dbcp.findCheckpoint(9999, false));

    // Create checkpoints 1-50.
    for (uint32_t i = 1; i <= 50; ++i) {
        step_checkpointer(i);
    }

    // Verify checkpoint chain: 0-50
    auto chain = dbcp.getCheckpointChain(dbcp.getCurrentID());
    EXPECT_EQUAL(chain.size(), 51);
    uint32_t chain_idx = 0;
    for (uint32_t i = 0; i <= 50; ++i) {
        EXPECT_EQUAL(chain[chain_idx++], 50-i);
    }

    // Sleep for a bit to flush the pipeline to ensure the checkpoint chain
    // can be retrieved from the database.
    std::this_thread::sleep_for(std::chrono::seconds(1));
    chain = dbcp.getCheckpointChain(dbcp.getCurrentID());
    EXPECT_EQUAL(chain.size(), 51);
    chain_idx = 0;
    for (uint32_t i = 0; i <= 50; ++i) {
        EXPECT_EQUAL(chain[chain_idx++], 50-i);
    }

    // Load checkpoint 45
    verif_load_chkpt(45);

    // Verify that checkpoints 46+ have been implicitly deleted
    EXPECT_FALSE(dbcp.hasCheckpoint(46));

    // Create checkpoints 46-55
    for (uint32_t i = 46; i <= 55; ++i) {
        step_checkpointer(i);
    }

    // Verify checkpoint chain: 0-55
    chain = dbcp.getCheckpointChain(dbcp.getCurrentID());
    EXPECT_EQUAL(chain.size(), 56);
    chain_idx = 0;
    for (uint32_t i = 0; i <= 55; ++i) {
        EXPECT_EQUAL(chain[chain_idx++], 55-i);
    }

    // Create checkpoints 56-58
    for (uint32_t i = 56; i <= 58; ++i) {
        step_checkpointer(i);
    }

    // Delete checkpoint always throws
    EXPECT_THROW(dbcp.deleteCheckpoint(57));

    // Create checkpoints 59-70
    for (uint32_t i = 59; i <= 70; ++i) {
        step_checkpointer(i);
    }

    // Load checkpoint 58
    verif_load_chkpt(58);

    // Verify all checkpoints: 0-58
    auto all_chkpts = dbcp.getCheckpoints();
    EXPECT_EQUAL(all_chkpts.size(), 59);
    EXPECT_EQUAL(dbcp.getNumCheckpoints(), 59);
    uint32_t idx = 0;
    for (uint32_t i = 0; i <= 58; ++i) {
        EXPECT_EQUAL(all_chkpts[idx++], i);
    }
    EXPECT_EQUAL(idx, all_chkpts.size());

    // Create checkpoints 59-75
    for (uint32_t i = 59; i <= 75; ++i) {
        step_checkpointer(i);
    }

    // Verify all checkpoints: 0-75
    all_chkpts = dbcp.getCheckpoints();
    EXPECT_EQUAL(all_chkpts.size(), 76);
    EXPECT_EQUAL(dbcp.getNumCheckpoints(), 76);
    idx = 0;
    for (uint32_t i = 0; i <= 75; ++i) {
        EXPECT_EQUAL(all_chkpts[idx++], i);
    }
    EXPECT_EQUAL(idx, all_chkpts.size());

    // Nothing to test, just call dumpRestoreChain()
    EXPECT_NOTHROW(dbcp.dumpRestoreChain(std::cout, 73));

    // Verify history chain up to current checkpoint
    size_t all_idx = 0;
    auto history_chain = dbcp.getHistoryChain(dbcp.getCurrentID());
    while (!history_chain.empty()) {
        EXPECT_EQUAL(history_chain.top(), all_chkpts[all_idx++]);
        history_chain.pop();
    }

    // Verify restore chain up to current checkpoint
    auto restore_chain = dbcp.getRestoreChain(dbcp.getCurrentID());
    auto id = restore_chain.top();
    restore_chain.pop();
    std::shared_ptr<DatabaseCheckpoint> chkpt;
    EXPECT_NOTHROW(chkpt = dbcp.findCheckpoint(id, true));
    auto c = chkpt;
    EXPECT_NOTEQUAL(c, nullptr);
    EXPECT_TRUE(c->isSnapshot());

    while (!restore_chain.empty()) {
        id = restore_chain.top();
        restore_chain.pop();
        EXPECT_NOTHROW(chkpt = dbcp.findCheckpoint(id, true));
        c = chkpt;
        EXPECT_NOTEQUAL(c, nullptr);
        EXPECT_FALSE(c->isSnapshot());
    }

    // To check the getCheckpointsAt() method, go back to the head
    // checkpoint. Then take a bunch of checkpoints at tick 1, 2, and 3.
    verif_load_chkpt(head_id);
    EXPECT_EQUAL(sched.getCurrentTick(), initial_tick);

    std::vector<DatabaseCheckpointer::chkpt_id_t> chkpts_at_1;
    for (uint32_t i = 1; i <= 300; ++i) {
        const bool step_sched = (i == 1);
        auto id = step_checkpointer(i, step_sched);
        EXPECT_EQUAL(sched.getCurrentTick(), 1 + initial_tick);
        chkpts_at_1.push_back(id);
    }

    std::vector<DatabaseCheckpointer::chkpt_id_t> chkpts_at_2;
    for (uint32_t i = 301; i <= 500; ++i) {
        const bool step_sched = (i == 301);
        auto id = step_checkpointer(i, step_sched);
        EXPECT_EQUAL(sched.getCurrentTick(), 2 + initial_tick);
        chkpts_at_2.push_back(id);
    }

    std::vector<DatabaseCheckpointer::chkpt_id_t> chkpts_at_3;
    for (uint32_t i = 501; i <= 700; ++i) {
        const bool step_sched = (i == 501);
        auto id = step_checkpointer(i, step_sched);
        EXPECT_EQUAL(sched.getCurrentTick(), 3 + initial_tick);
        chkpts_at_3.push_back(id);
    }

    EXPECT_EQUAL(dbcp.getCheckpointsAt(1 + initial_tick), chkpts_at_1);
    EXPECT_EQUAL(dbcp.getCheckpointsAt(2 + initial_tick), chkpts_at_2);
    EXPECT_EQUAL(dbcp.getCheckpointsAt(3 + initial_tick), chkpts_at_3);

    // Wait for the older checkpoints to be evicted and
    // verify getCheckpointsAt() again.
    wait_until_evicted(chkpts_at_1.back());
    wait_until_evicted(chkpts_at_2.back());

    EXPECT_EQUAL(dbcp.getCheckpointsAt(1 + initial_tick), chkpts_at_1);
    EXPECT_EQUAL(dbcp.getCheckpointsAt(2 + initial_tick), chkpts_at_2);
    EXPECT_EQUAL(dbcp.getCheckpointsAt(3 + initial_tick), chkpts_at_3);

    // Verify the findLatestCheckpointAtOrBefore() method.
    // Valid tick (2), invalid ID (9999)
    EXPECT_THROW(dbcp.findLatestCheckpointAtOrBefore(2, 9999));

    // Valid ID (1), but tick is before the head checkpoint
    if (initial_tick > 0) {
        EXPECT_EQUAL(dbcp.findLatestCheckpointAtOrBefore(initial_tick - 1, 1), nullptr);
    }

    // Valid tick (2), valid ID
    EXPECT_NOTHROW(chkpt = dbcp.findLatestCheckpointAtOrBefore(2 + initial_tick, chkpts_at_2.back()));
    EXPECT_EQUAL(chkpt->getID(), chkpts_at_2.back());
    EXPECT_EQUAL(chkpt->getTick(), 2 + initial_tick);

    // Valid tick (2), valid ID
    EXPECT_NOTHROW(chkpt = dbcp.findLatestCheckpointAtOrBefore(2 + initial_tick, chkpts_at_3.back()));
    EXPECT_EQUAL(chkpt->getID(), chkpts_at_2.back());
    EXPECT_EQUAL(chkpt->getTick(), 2 + initial_tick);

    // Verify that the head checkpoint is in the cache until simulation teardown.
    EXPECT_TRUE(dbcp.isCheckpointCached(head_id));

    // Finish
    app_mgr.postSimLoopTeardown();
    root.enterTeardown();
    clocks.enterTeardown();

    // Ensure that the head checkpoint is no longer in the cache
    EXPECT_FALSE(dbcp.isCheckpointCached(head_id));
}

void ProfileLoadCheckpoint(DatabaseCheckpointer::chkpt_id_t load_id)
{
    sparta::Scheduler sched;
    RootTreeNode clocks("clocks");
    sparta::Clock clk(&clocks, "clock", &sched);

    // Create a tree with some register sets and memory
    RootTreeNode root;

    DummyDevice dummy(&root);
    std::unique_ptr<RegisterSet> rset(RegisterSet::create(&dummy, reg_defs));

    DummyDevice dummy2(&dummy);
    std::unique_ptr<RegisterSet> rset2(RegisterSet::create(&dummy2, reg_defs));

    auto r1 = rset->getRegister("reg2");
    auto r2 = rset2->getRegister("reg2");
    assert(r1 != r2);
    r1->write<uint32_t>(0);
    r2->write<uint32_t>(0);

    simdb::DatabaseManager db_mgr("test.db", true);
    simdb::AppManager app_mgr(&db_mgr);

    // Setup...
    app_mgr.getAppFactory<DatabaseCheckpointer>()->setSpartaElems(root, &sched);
    app_mgr.enableApp(DatabaseCheckpointer::NAME);
    app_mgr.createEnabledApps();
    app_mgr.createSchemas();
    app_mgr.postInit(0, nullptr);
    app_mgr.openPipelines();

    auto& dbcp = *app_mgr.getApp<DatabaseCheckpointer>();
    dbcp.setSnapshotThreshold(10);
    dbcp.setMaxCachedWindows(10);

    root.enterConfiguring();
    root.enterFinalized();
    sched.finalize();

    dbcp.createHead();

    auto step_checkpointer = [&](DatabaseCheckpointer::chkpt_id_t expected_id) {
        r1->write<uint32_t>(expected_id * 5ul);
        r2->write<uint32_t>(expected_id % 5ul);
        sched.run(1, true, false);

        DatabaseCheckpointer::chkpt_id_t actual_id = DatabaseCheckpoint::UNIDENTIFIED_CHECKPOINT;
        EXPECT_NOTHROW(actual_id = dbcp.createCheckpoint());
        EXPECT_EQUAL(actual_id, expected_id);
        EXPECT_EQUAL(actual_id, dbcp.getCurrentID());
        EXPECT_EQUAL(dbcp.getNumCheckpoints(), expected_id + 1);

        // Should always have the head and current checkpoints in the cache
        EXPECT_TRUE(dbcp.isCheckpointCached(dbcp.getHeadID()));
        EXPECT_TRUE(dbcp.isCheckpointCached(dbcp.getCurrentID()));

        return actual_id;
    };

    // Quickly create 1000 checkpoints. This fills up the pipeline to help bash edge cases.
    for (uint32_t i = 1; i <= 1000; ++i) {
        step_checkpointer(i);
    }

    // Load the checkpoint
    {
        const std::string profile_desc = "loadCheckpoint(" + std::to_string(load_id) + ")";
        PROFILE_BLOCK(profile_desc.c_str());
        dbcp.loadCheckpoint(load_id);
    }

    // Finish
    app_mgr.postSimLoopTeardown();
    root.enterTeardown();
    clocks.enterTeardown();

    // Now that the cache / pipeline / DB has been fully flushed, verify
    // that any checkpoint >load_id **cannot** be found anywhere.
    EXPECT_FALSE(dbcp.hasCheckpoint(load_id + 1));
}

int main()
{
    auto warn_cerr = std::make_unique<sparta::log::Tap>(
        sparta::TreeNode::getVirtualGlobalNode(),
        sparta::log::categories::WARN,
        std::cerr);

    auto warn_file = std::make_unique<sparta::log::Tap>(
        sparta::TreeNode::getVirtualGlobalNode(),
        sparta::log::categories::WARN,
        "warnings.log");

    // Run the test with initial scheduler tick = 0,
    // i.e. head checkpoint at tick 0
    //RunCheckpointerTest(0);

    // Run the test with initial scheduler tick = 10,
    // i.e. head checkpoint at tick 10
    //RunCheckpointerTest(10);

    // Measure elapsed times for loading checkpoints
    // that either on disk or in the pipeline, but
    // either way they are not in the cache. Importantly,
    // we want the checkpointer to have about the same
    // performance to load disk checkpoints regardless.
    uint32_t load_id = 900;
    while (load_id > 0) {
        ProfileLoadCheckpoint(load_id);
        load_id -= 100;
    }

    REPORT_ERROR;
    return ERROR_CODE;
}
