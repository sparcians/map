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
        sparta::TreeNode(parent, "dummy", "", sparta::TreeNode::GROUP_IDX_NONE, "dummy node for register test")
    {}
};

//! General test for saving and loading checkpoints to/from SimDB
void generalTest()
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
    dbcp.setSnapshotThreshold(9);

    root.enterConfiguring();
    root.enterFinalized();
    sched.finalize();
    EXPECT_EQUAL(sched.getCurrentTick(), 0);

    // CHECKPOINT: Head
    DatabaseCheckpointer::chkpt_id_t head_id;
    EXPECT_NOTHROW(dbcp.createHead());
    head_id = dbcp.getHeadID();
    EXPECT_NOTEQUAL(dbcp.getHead(), nullptr);
    EXPECT_EQUAL(head_id, dbcp.getHead()->getID());
    EXPECT_EQUAL(dbcp.getCurrentID(), head_id);
    EXPECT_EQUAL(dbcp.getCurrentTick(), 0);

    auto step_checkpointer = [&](uint32_t i) {
        r1->write<uint32_t>(i * 5ul);
        r2->write<uint32_t>(i % 5ul);
        sched.run(1, true, false);
        //EXPECT_EQUAL(i, sched.getCurrentTick());
        //EXPECT_EQUAL(i, dbcp.getCurrentTick());

        DatabaseCheckpointer::chkpt_id_t id;
        EXPECT_NOTHROW(id = dbcp.createCheckpoint());
        EXPECT_EQUAL(id, i);
        EXPECT_EQUAL(id, dbcp.getCurrentID());
        return id;
    };

    // Create 1000 checkpoints, and periodically access an old one. Also
    // go to sleep sometimes to increase the chances we have to go to the
    // database to retrieve a checkpoint.
    for (uint32_t i = 1; i <= 100; ++i) {
        step_checkpointer(i);

        // Access most recent from the cache directly
        auto cached_cp = dbcp.findCheckpoint(i).lock();
        EXPECT_NOTEQUAL(cached_cp, nullptr);
        if (cached_cp) {
            EXPECT_EQUAL(cached_cp->getID(), i);
            EXPECT_EQUAL(cached_cp->getPrevID(), i - 1);
        }

        // Access an old one, which may or may not be in the cache
        if (rand() % 10 == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 50));
            auto old_id = static_cast<uint64_t>(rand() % i);
            auto old_cp = dbcp.cloneCheckpoint(old_id);
            EXPECT_NOTEQUAL(old_cp, nullptr);
            if (old_cp) {
                EXPECT_EQUAL(old_cp->getID(), old_id);
                EXPECT_EQUAL(old_cp->getPrevID(), old_id - 1);
            }
        }
    }

    auto verif_load_chkpt = [&](DatabaseCheckpointer::chkpt_id_t id) {
        EXPECT_NOTHROW(dbcp.loadCheckpoint(id));
        EXPECT_EQUAL(dbcp.getCurrentID(), id);
        EXPECT_EQUAL(dbcp.getNumCheckpoints(), id + 1);
        EXPECT_EQUAL(sched.getCurrentTick(), id);
        // TODO cnyce: verify registers
    };

    // Load very recent checkpoints that are definitely in the cache
    verif_load_chkpt(100);
    verif_load_chkpt(99);
    verif_load_chkpt(95);
    verif_load_chkpt(90);
    verif_load_chkpt(89);

    // Load checkpoints that have probably already been evicted from the cache
    std::this_thread::sleep_for(std::chrono::seconds(1));
    verif_load_chkpt(49);
    verif_load_chkpt(45);
    verif_load_chkpt(40);
    verif_load_chkpt(39);

    step_checkpointer(40);
    step_checkpointer(41);
    step_checkpointer(42);
    verif_load_chkpt(40);

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
    EXPECT_THROW(dbcp.cloneCheckpoint(9999));
    EXPECT_NOTHROW(dbcp.cloneCheckpoint(9999, false));

    // Create checkpoints 1-50. Keep a clone of checkpoint 3 for later.
    std::unique_ptr<DatabaseCheckpoint> clone3;
    for (uint32_t i = 1; i <= 50; ++i) {
        step_checkpointer(i);
        if (i == 3) {
            clone3 = dbcp.findCheckpoint(3).lock()->clone();
        }
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
    // TODO cnyce: EXPECT_FALSE(dbcp.hasCheckpoint(46));

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
    // TODO cnyce: relax this restriction?
    EXPECT_THROW(dbcp.deleteCheckpoint(57));

    // Create checkpoints 59-70
    for (uint32_t i = 59; i <= 70; ++i) {
        step_checkpointer(i);
    }

    // Load checkpoint 58
    verif_load_chkpt(58);

    // Finish
    app_mgr.postSimLoopTeardown();
    root.enterTeardown();
    clocks.enterTeardown();
    return;



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

    uint32_t all_idx = 0;
    for (uint32_t i = 0; i <= 45; ++i) {
        EXPECT_EQUAL(all_chkpts[all_idx++], i);
    }
    for (uint32_t i = 51; i <= 56; ++i) {
        EXPECT_EQUAL(all_chkpts[all_idx++], i);
    }
    EXPECT_EQUAL(all_chkpts[all_idx++], 58);
    for (uint32_t i = 71; i <= 75; ++i) {
        EXPECT_EQUAL(all_chkpts[all_idx++], i);
    }
    EXPECT_EQUAL(all_idx, all_chkpts.size());
    all_idx = 0;

    // Nothing to test, just call dumpRestoreChain()
    dbcp.dumpRestoreChain(std::cout, 73);

    // Verify history chain up to current checkpoint
    auto history_chain = dbcp.getHistoryChain(dbcp.getCurrentID());
    while (!history_chain.empty()) {
        EXPECT_EQUAL(history_chain.top(), all_chkpts[all_idx++]);
        history_chain.pop();
    }
    all_idx = 0;

    // Verify restore chain up to current checkpoint
    auto restore_chain = dbcp.getRestoreChain(dbcp.getCurrentID());
    auto id = restore_chain.top();
    restore_chain.pop();
    std::weak_ptr<DatabaseCheckpoint> chkpt;
    EXPECT_NOTHROW(chkpt = dbcp.findCheckpoint(id));
    auto c = chkpt.lock();
    EXPECT_NOTEQUAL(c, nullptr);
    EXPECT_TRUE(c->isSnapshot());

    while (!restore_chain.empty()) {
        id = restore_chain.top();
        restore_chain.pop();
        EXPECT_NOTHROW(chkpt = dbcp.findCheckpoint(id));
        c = chkpt.lock();
        EXPECT_NOTEQUAL(c, nullptr);
        EXPECT_FALSE(c->isSnapshot());
    }

    // Verify that cached checkpoints are clonable
    auto cache73 = dbcp.findCheckpoint(73).lock();
    auto clone73 = dbcp.cloneCheckpoint(73);

    std::ostringstream cache_oss;
    std::ostringstream clone_oss;

    cache73->dumpData(cache_oss);
    clone73->dumpData(clone_oss);

    EXPECT_EQUAL(cache_oss.str(), clone_oss.str());
    EXPECT_EQUAL(cache73->getTotalMemoryUse(), clone73->getTotalMemoryUse());
    EXPECT_EQUAL(cache73->getContentMemoryUse(), clone73->getContentMemoryUse());
    EXPECT_TRUE(cache73->getHistoryChain() == clone73->getHistoryChain());
    EXPECT_TRUE(cache73->getRestoreChain() == clone73->getRestoreChain());
    EXPECT_EQUAL(cache73->getPrevID(), clone73->getPrevID());
    EXPECT_EQUAL(cache73->getNextIDs(), clone73->getNextIDs());
    EXPECT_EQUAL(cache73->getTick(), clone73->getTick());
    EXPECT_EQUAL(cache73->isSnapshot(), clone73->isSnapshot());
    EXPECT_EQUAL(cache73->getDistanceToPrevSnapshot(), clone73->getDistanceToPrevSnapshot());

    // Wait until checkpoint 3 is evicted from cache
    uint32_t num_tries = 0;
    while (dbcp.findCheckpoint(3).lock() != nullptr) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        EXPECT_NOTEQUAL(++num_tries, 100); // 1-second timeout
    }

    // Ask the checkpointer to retrieve checkpoint 3 from the database
    auto dbchkpt3 = dbcp.cloneCheckpoint(3);
    EXPECT_EQUAL(dbchkpt3->getID(), clone3->getID());

    // Verify that the database checkpoint matches the original clone of 3
    std::ostringstream clone3_oss, dbchkpt3_oss;
    clone3->dumpData(clone3_oss);
    dbchkpt3->dumpData(dbchkpt3_oss);
    EXPECT_EQUAL(clone3_oss.str(), dbchkpt3_oss.str());

    // Verify history chain for a db-recreated checkpoint
    auto hist_chain3 = dbcp.getHistoryChain(3);
    for (auto hist_id : {3,2,1,0}) {
        EXPECT_FALSE(hist_chain3.empty());
        EXPECT_EQUAL(hist_chain3.top(), hist_id);
        hist_chain3.pop();
    }

    // Verify restore chain for a db-recreated checkpoint
    auto rest_chain3 = dbcp.getRestoreChain(3);
    for (auto rest_id : {3,2,1,0}) {
        EXPECT_FALSE(rest_chain3.empty());
        EXPECT_EQUAL(rest_chain3.top(), rest_id);
        rest_chain3.pop();
    }

    // Verify distance to previous snapshot for a db-recreated checkpoint
    EXPECT_EQUAL(dbchkpt3->getDistanceToPrevSnapshot(), 3);

    // Nothing to test, just call dumpRestoreChain()
    dbcp.dumpRestoreChain(std::cout, 3);

    // Nothing to test, just call dumpList/dumpData/dumpAnnotatedData
    dbcp.dumpList(std::cout);
    std::cout << std::endl;
    dbcp.dumpData(std::cout);
    std::cout << std::endl;
    dbcp.dumpAnnotatedData(std::cout);
    std::cout << std::endl;

    // Load checkpoint 8 and verify registers
    EXPECT_NOTHROW(dbcp.loadCheckpoint(8));
    EXPECT_EQUAL(r1->read<uint32_t>(), 40ul);  // 8 * 5
    EXPECT_EQUAL(r2->read<uint32_t>(), 3ul);   // 8 % 5
    EXPECT_EQUAL(sched.getCurrentTick(), 8);
    EXPECT_EQUAL(dbcp.getNumCheckpoints(), 9);

    // Finish...
    app_mgr.postSimLoopTeardown();
}

int main()
{
    std::unique_ptr<sparta::log::Tap> warn_cerr(new sparta::log::Tap(
        sparta::TreeNode::getVirtualGlobalNode(),
        sparta::log::categories::WARN,
        std::cerr));

    std::unique_ptr<sparta::log::Tap> warn_file(new sparta::log::Tap(
        sparta::TreeNode::getVirtualGlobalNode(),
        sparta::log::categories::WARN,
        "warnings.log"));

    generalTest();

    REPORT_ERROR;
    return ERROR_CODE;
}
