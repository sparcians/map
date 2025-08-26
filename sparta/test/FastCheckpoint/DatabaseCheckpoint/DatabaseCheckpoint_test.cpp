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
    app_mgr.postInit(0, nullptr);
    app_mgr.openPipelines();

    auto& dbcp = *app_mgr.getApp<DatabaseCheckpointer>();
    dbcp.setSnapshotThreshold(100);

    root.enterConfiguring();
    root.enterFinalized();
    sched.finalize();
    EXPECT_EQUAL(sched.getCurrentTick(), 0); // Unfinalized sched at tick 0

    // CHECKPOINT: HEAD
    DatabaseCheckpointer::chkpt_id_t head_id;
    EXPECT_NOTHROW(dbcp.createHead());
    head_id = dbcp.getHeadID();
    EXPECT_EQUAL(head_id, 0);

    // Checkpoints 1 through 10000. Save a few of the register values
    // with their checkpoint IDs so we can verify the correct registers
    // after rolling back to previous checkpoints.
    std::vector<DatabaseCheckpointer::chkpt_id_t> chkpt_ids;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(1, 10);

    for (uint32_t i = 1; i <= 10000; ++i) {
        r1->write<uint32_t>(i * 5ul);
        r2->write<uint32_t>(i % 5ul);
        sched.run(1, true, false);
        EXPECT_EQUAL(i, sched.getCurrentTick());
        EXPECT_EQUAL(i, dbcp.getCurrentTick());

        DatabaseCheckpointer::chkpt_id_t id;
        EXPECT_NOTHROW(id = dbcp.createCheckpoint());
        EXPECT_EQUAL(id, i);

        if (distrib(gen) == 5) {
            chkpt_ids.push_back(id);
        }
    }

    // Shuffle up the checkpoint IDs and wait a bit before we start
    // loading checkpoints and verifying the registers.
    std::shuffle(chkpt_ids.begin(), chkpt_ids.end(), gen);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    for (auto id : chkpt_ids) {
        EXPECT_NOTHROW(dbcp.loadCheckpoint(id));

        auto chkpt = dbcp.cloneCheckpoint(id);
        uint32_t expected_r1 = id * 5ul;
        EXPECT_EQUAL(r1->read<uint32_t>(), expected_r1);

        uint32_t expected_r2 = id % 5ul;
        EXPECT_EQUAL(r2->read<uint32_t>(), expected_r2);

        auto expected_tick = dbcp.getCurrentTick();
        EXPECT_EQUAL(sched.getCurrentTick(), expected_tick);
        EXPECT_EQUAL(chkpt->getTick(), expected_tick);
    }

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
