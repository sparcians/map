#include "sparta/serialization/checkpoint/CherryPickFastCheckpointer.hpp"
#include "sparta/utils/SpartaTester.hpp"

#include "sparta/functional/Register.hpp"
#include "sparta/functional/RegisterSet.hpp"
#include "sparta/memory/MemoryObject.hpp"
#include "sparta/simulation/Clock.hpp"

#include "simdb/apps/AppManager.hpp"
#include "simdb/sqlite/DatabaseManager.hpp"

TEST_INIT

using sparta::Register;
using sparta::RegisterSet;
using sparta::RootTreeNode;
using sparta::memory::MemoryObject;
using sparta::memory::BlockingMemoryObjectIFNode;
using sparta::serialization::checkpoint::CherryPickFastCheckpointer;
using chkpt_id_t = typename CherryPickFastCheckpointer::chkpt_id_t;

static const uint16_t HINT_NONE=0;

#define EXPECT_SNAPSHOT(id) EXPECT_TRUE(fcp.findCheckpoint(id)->isSnapshot());
#define EXPECT_DELTA(id) EXPECT_FALSE(fcp.findCheckpoint(id)->isSnapshot());

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

void RunCheckpointerTest()
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

    root.enterConfiguring();
    root.enterFinalized();
    sched.finalize();

    auto r1 = rset->getRegister("reg2");
    auto r2 = rset2->getRegister("reg2");
    sparta_assert(r1 != r2);

    r1->write<uint32_t>(0);
    r2->write<uint32_t>(0);

    simdb::AppManagers app_mgrs;
    auto& app_mgr = app_mgrs.getAppManager("test.db", true /*new file*/);

    // Setup...
    // Apps must be enabled prior to parameterizing their custom factories
    app_mgr.enableApp(CherryPickFastCheckpointer::NAME);

    // Now parameterize the factory
    const std::vector<sparta::TreeNode*> roots({&root});
    app_mgr.parameterizeAppFactory<CherryPickFastCheckpointer>(roots, &sched);

    app_mgr.createEnabledApps();
    app_mgr.createSchemas();
    app_mgr.initializePipelines();
    app_mgr.openPipelines();

    auto& dbcp = *app_mgr.getApp<CherryPickFastCheckpointer>();
    auto& fcp = dbcp.getFastCheckpointer();
    fcp.setSnapshotThreshold(5);

    // Run tests
    auto create_checkpoint = [&](bool force_snapshot = false)
    {
        // Writing to the registers before checkpointing is not really necessary,
        // since we are not validating the values after loadCheckpoint(). That
        // functionality is already covered by the FastCheckpointer test.
        uint32_t r1_val = rand();
        uint32_t r2_val = rand();

        r1->write<uint32_t>(r1_val);
        r2->write<uint32_t>(r2_val);

        auto chkpt_id = fcp.createCheckpoint(force_snapshot);
        return chkpt_id;
    };

    // Make sure calling commitCurrentBranch() does nothing when we have no checkpoints.
    dbcp.commitCurrentBranch();
    EXPECT_EQUAL(dbcp.getNumCheckpoints(), 0);
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 0);
    EXPECT_EQUAL(fcp.getNumSnapshots(), 0);
    EXPECT_EQUAL(fcp.getNumDeltas(), 0);

    // Calling commitCurrentBranch(true) to force a new head checkpoint should also
    // do nothing when we have no checkpoints at all.
    dbcp.commitCurrentBranch(true);
    EXPECT_EQUAL(dbcp.getNumCheckpoints(), 0);
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 0);
    EXPECT_EQUAL(fcp.getNumSnapshots(), 0);
    EXPECT_EQUAL(fcp.getNumDeltas(), 0);

    // Start by creating chain:
    //   S1->D1->D2->D3
    fcp.createHead();
    auto S1 = fcp.getHeadID();
    auto D1 = create_checkpoint();
    auto D2 = create_checkpoint();
    auto D3 = create_checkpoint();

    EXPECT_SNAPSHOT(S1);
    EXPECT_DELTA(D1);
    EXPECT_DELTA(D2);
    EXPECT_DELTA(D3);
    EXPECT_EQUAL(dbcp.getNumCheckpoints(), 0);

    // Try to serialize the current chain without forcing a new head checkpoint.
    // Since we only have 1 snapshot (S1), we should not be able to do this. It
    // is merely a no-op, not an exception.
    //
    // Current chain before calling this method:
    //   S1->D1->D2->D3
    dbcp.commitCurrentBranch();

    // Since the previous commit was a no-op, we should not have anything
    // in the database yet. The chain is still S1->D1->D2->D3
    EXPECT_EQUAL(dbcp.getNumCheckpoints(), 0);
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 4);
    EXPECT_EQUAL(fcp.getNumSnapshots(), 1);
    EXPECT_EQUAL(fcp.getNumDeltas(), 3);
    EXPECT_EQUAL(fcp.getCheckpointChain(D3).size(), 4);
    EXPECT_EQUAL(fcp.findCheckpoint(D3)->getRestoreChain().size(), 4);

    // Append two new checkpoints (deltas) onto S1
    auto D4 = create_checkpoint();
    auto D5 = create_checkpoint();

    EXPECT_DELTA(D4);
    EXPECT_DELTA(D5);

    // Current chain is now:
    //   S1->D1->D2->D3->D4->D5
    EXPECT_EQUAL(dbcp.getNumCheckpoints(), 0);
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 6);
    EXPECT_EQUAL(fcp.getNumSnapshots(), 1);
    EXPECT_EQUAL(fcp.getNumDeltas(), 5);

    // We should only have 1 snapshot available, so asking to save to
    // disk without forcing a new head checkpoint should still be a
    // no-op. Current chain before/after this no-op is thus:
    //   S1->D1->D2->D3->D4->D5
    dbcp.commitCurrentBranch();
    EXPECT_EQUAL(dbcp.getNumCheckpoints(), 0);
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 6);
    EXPECT_EQUAL(fcp.getNumSnapshots(), 1);
    EXPECT_EQUAL(fcp.getNumDeltas(), 5);
    EXPECT_EQUAL(fcp.getCheckpointChain(D5).size(), 6);
    EXPECT_EQUAL(fcp.findCheckpoint(D5)->getRestoreChain().size(), 6);

    // Now save to disk, but this time force a new head checkpoint.
    // Everything checkpointed thus far should be in the database,
    // and the FastCheckpointer should only have the new S2 snapshot
    // that we just forced.
    dbcp.commitCurrentBranch(true);
    auto S2 = fcp.getHeadID();
    EXPECT_SNAPSHOT(S2);

    // Current chain is now:
    //   S2
    EXPECT_EQUAL(dbcp.getNumCheckpoints(), 6);
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 1);
    EXPECT_EQUAL(fcp.getNumSnapshots(), 1);
    EXPECT_EQUAL(fcp.getNumDeltas(), 0);
    EXPECT_EQUAL(fcp.getCheckpointChain(S2).size(), 1);
    EXPECT_EQUAL(fcp.findCheckpoint(S2)->getRestoreChain().size(), 1);

    // Append 5 new delta checkpoints onto S2
    auto D6  = create_checkpoint();
    auto D7  = create_checkpoint();
    auto D8  = create_checkpoint();
    auto D9  = create_checkpoint();
    auto D10 = create_checkpoint();

    EXPECT_DELTA(D6);
    EXPECT_DELTA(D7);
    EXPECT_DELTA(D8);
    EXPECT_DELTA(D9);
    EXPECT_DELTA(D10);

    // Checkpoint chain is now:
    //   S2 -> D6->D7->D8->D9->D10
    EXPECT_EQUAL(dbcp.getNumCheckpoints(), 6);
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 6);
    EXPECT_EQUAL(fcp.getNumSnapshots(), 1);
    EXPECT_EQUAL(fcp.getNumDeltas(), 5);
    EXPECT_EQUAL(fcp.getCheckpointChain(D10).size(), 6);
    EXPECT_EQUAL(fcp.findCheckpoint(D10)->getRestoreChain().size(), 6);

    // Append new snapshot
    auto S3 = create_checkpoint();
    EXPECT_SNAPSHOT(S3);

    // Append 3 new delta checkpoints
    auto D11 = create_checkpoint();
    auto D12 = create_checkpoint();
    auto D13 = create_checkpoint();

    EXPECT_DELTA(D11);
    EXPECT_DELTA(D12);
    EXPECT_DELTA(D13);

    // Checkpoint chain is now:
    //   S2 -> D6->D7->D8->D9->D10 -> S3 -> D11->D12->D13
    EXPECT_EQUAL(dbcp.getNumCheckpoints(), 6);
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 10);
    EXPECT_EQUAL(fcp.getNumSnapshots(), 2);
    EXPECT_EQUAL(fcp.getNumDeltas(), 8);

    // If we serialize now without forcing a new head checkpoint, we should only
    // be able to write to disk S2 through D10, and S3 through D13 should remain
    // in the fast checkpointer with S3 as the new head checkpoint.
    dbcp.commitCurrentBranch();
    EXPECT_EQUAL(dbcp.getNumCheckpoints(), 12); // S1 through D10 (added S2 through D10)
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 4);   // S3 through D13 (must retain S3, did not force)
    EXPECT_EQUAL(fcp.getNumSnapshots(), 1);     // S3 only
    EXPECT_EQUAL(fcp.getNumDeltas(), 3);        // D11-D13
    EXPECT_EQUAL(fcp.getHeadID(), S3);
    EXPECT_EQUAL(fcp.getCheckpointChain(D13).size(), 4);
    EXPECT_EQUAL(fcp.findCheckpoint(D13)->getRestoreChain().size(), 4);

    // Create 2 new checkpoints off of D13, plus another snapshot S4
    auto D14 = create_checkpoint();
    auto D15 = create_checkpoint();
    auto S4  = create_checkpoint();

    EXPECT_DELTA(D14);
    EXPECT_DELTA(D15);
    EXPECT_SNAPSHOT(S4);

    EXPECT_EQUAL(dbcp.getNumCheckpoints(), 12); // S1 through D10 (same)
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 7);   // S3 -> D11->D12->D13->D14->D15 -> S4
    EXPECT_EQUAL(fcp.getNumSnapshots(), 2);     // S3 and S4
    EXPECT_EQUAL(fcp.getNumDeltas(), 5);        // D11-D15
    EXPECT_EQUAL(fcp.getHeadID(), S3);          // Head S3 should not have changed

    // Current chain is now:
    //   S3 -> D11->D12->D13->D14->D15 -> S4
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 7);

    // Now go back to S3 and create another branch of checkpoints (5 deltas
    // and another snapshot S5).
    fcp.loadCheckpoint(S3);
    auto D16 = create_checkpoint();
    auto D17 = create_checkpoint();
    auto D18 = create_checkpoint();
    auto D19 = create_checkpoint();
    auto D20 = create_checkpoint();
    auto S5  = create_checkpoint();

    EXPECT_DELTA(D16);
    EXPECT_DELTA(D17);
    EXPECT_DELTA(D18);
    EXPECT_DELTA(D19);
    EXPECT_DELTA(D20);
    EXPECT_SNAPSHOT(S5);

    // We now have two active branches in the fast checkpointer:
    //   S3 -> D11->D12->D13->D14->D15 -> S4
    //    |
    //    | -> D16->D17->D18->D19->D20 -> S5 (**current**)
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 13);
    EXPECT_EQUAL(fcp.getNumSnapshots(), 3);
    EXPECT_EQUAL(fcp.getNumDeltas(), 10);
    EXPECT_EQUAL(fcp.getCheckpointChain(S5).size(), 7);
    EXPECT_EQUAL(fcp.findCheckpoint(S5)->getRestoreChain().size(), 1);

    // Set snapshot S4 as the current checkpoint
    //
    //   S3 -> D11->D12->D13->D14->D15 -> S4 (**current**)
    //    |
    //    | -> D16->D17->D18->D19->D20 -> S5
    fcp.loadCheckpoint(S4);
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 13);
    EXPECT_EQUAL(fcp.getNumSnapshots(), 3);
    EXPECT_EQUAL(fcp.getNumDeltas(), 10);
    EXPECT_EQUAL(fcp.getCheckpointChain(S4).size(), 7);
    EXPECT_EQUAL(fcp.findCheckpoint(S4)->getRestoreChain().size(), 1);

    // Save to disk without forcing a new head checkpoint.
    // The only thing remaining in the fast checkpointer
    // is S4 (current), while the database got six new
    // checkpoints S3->D11->D12->D13->D14->D15
    dbcp.commitCurrentBranch();
    EXPECT_EQUAL(dbcp.getNumCheckpoints(), 18); // S1-S3, D1-D15
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 1);   // S4 only
    EXPECT_EQUAL(fcp.getNumSnapshots(), 1);     // S4 snapshot
    EXPECT_EQUAL(fcp.getNumDeltas(), 0);        // Nothing else but S4
    EXPECT_EQUAL(fcp.getHeadID(), S4);
    EXPECT_SNAPSHOT(fcp.getHeadID());
    EXPECT_SNAPSHOT(fcp.getCurrentID());
    EXPECT_EQUAL(fcp.getCheckpointChain(S4).size(), 1);
    EXPECT_EQUAL(fcp.findCheckpoint(S4)->getRestoreChain().size(), 1);

    // Try to serialize to disk without forcing a new head checkpoint,
    // which is a no-op here since the fast checkpointer is only holding
    // onto S4 (the only snapshot - it can't get rid of it).
    dbcp.commitCurrentBranch();
    EXPECT_EQUAL(dbcp.getNumCheckpoints(), 18);
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 1);
    EXPECT_EQUAL(fcp.getHeadID(), S4);
    EXPECT_SNAPSHOT(fcp.getHeadID());
    EXPECT_SNAPSHOT(fcp.getCurrentID());
    EXPECT_EQUAL(fcp.getCheckpointChain(S4).size(), 1);
    EXPECT_EQUAL(fcp.findCheckpoint(S4)->getRestoreChain().size(), 1);

    // With the fast checkpointer only having S4, create this branch:
    //   S4 -> D21->D22->D23->D24->D25 -> S6
    auto D21 = create_checkpoint();
    auto D22 = create_checkpoint();
    auto D23 = create_checkpoint();
    auto D24 = create_checkpoint();
    auto D25 = create_checkpoint();
    auto S6  = create_checkpoint();

    EXPECT_DELTA(D21);
    EXPECT_DELTA(D22);
    EXPECT_DELTA(D23);
    EXPECT_DELTA(D24);
    EXPECT_DELTA(D25);
    EXPECT_SNAPSHOT(S6);
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 7);

    // Commit the current branch without forcing a new head checkpoint.
    // S5 should then be the new head and all other 6 checkpoints should
    // be flushed to disk.
    dbcp.commitCurrentBranch();
    EXPECT_EQUAL(dbcp.getNumCheckpoints(), 24);
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 1);
    EXPECT_EQUAL(fcp.getHeadID(), S6);
    EXPECT_EQUAL(fcp.getCheckpointChain(S6).size(), 1);
    EXPECT_EQUAL(fcp.findCheckpoint(S6)->getRestoreChain().size(), 1);

    // Create this branch:
    //   S6 -> D26->D27->D28->D29->D30 -> S7
    auto D26 = create_checkpoint();
    auto D27 = create_checkpoint();
    auto D28 = create_checkpoint();
    auto D29 = create_checkpoint();
    auto D30 = create_checkpoint();
    auto S7  = create_checkpoint();

    EXPECT_DELTA(D26);
    EXPECT_DELTA(D27);
    EXPECT_DELTA(D28);
    EXPECT_DELTA(D29);
    EXPECT_DELTA(D30);
    EXPECT_SNAPSHOT(S7);
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 7);

    // Commit the current branch and force a new head snapshot S8
    dbcp.commitCurrentBranch(true);
    auto S8 = fcp.getHeadID();
    EXPECT_SNAPSHOT(S8);

    EXPECT_EQUAL(dbcp.getNumCheckpoints(), 31); // Got 7 more chkpts
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 1);   // S8 only
    EXPECT_EQUAL(fcp.getCheckpointChain(S8).size(), 1);
    EXPECT_EQUAL(fcp.findCheckpoint(S8)->getRestoreChain().size(), 1);

    // Create these two branches:
    //   S8 -> D31->D32 -> S9
    //               |
    //               |---> D33->D34 (current)
    auto D31 = create_checkpoint();
    auto D32 = create_checkpoint();
    auto S9  = create_checkpoint(true); // Force snapshot

    EXPECT_DELTA(D31);
    EXPECT_DELTA(D32);
    EXPECT_SNAPSHOT(S9);
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 4);

    fcp.loadCheckpoint(D32);
    auto D33 = create_checkpoint();
    auto D34 = create_checkpoint();

    EXPECT_DELTA(D33);
    EXPECT_DELTA(D34);
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 6);

    // Commit the current branch without forcing a new head checkpoint.
    // The remaining branch in the fast checkpointer should be:
    //   S8 -> D31->D32->D33->D34
    dbcp.commitCurrentBranch();
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 5);
    EXPECT_EQUAL(fcp.getHeadID(), S8);
    EXPECT_EQUAL(dbcp.getNumCheckpoints(), 31); // Did not get any new chkpts
    EXPECT_EQUAL(fcp.getCheckpointChain(D34).size(), 5);
    EXPECT_EQUAL(fcp.findCheckpoint(D34)->getRestoreChain().size(), 5);

    // Commit the current branch and force a new head checkpoint S10
    dbcp.commitCurrentBranch(true);
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 1);

    auto S10 = fcp.getHeadID();
    EXPECT_SNAPSHOT(S10);
    EXPECT_EQUAL(dbcp.getNumCheckpoints(), 36); // Got 5 more chkpts
    EXPECT_EQUAL(fcp.getCheckpointChain(S10).size(), 1);
    EXPECT_EQUAL(fcp.findCheckpoint(S10)->getRestoreChain().size(), 1);

    // Create these two branches:
    //   S10 -> D35->D36 -> S11 -> D37(current)->D38
    //                       |
    //                       |---> D39->D40
    auto D35 = create_checkpoint();
    auto D36 = create_checkpoint();
    auto S11 = create_checkpoint(true); // Force snapshot
    auto D37 = create_checkpoint();
    auto D38 = create_checkpoint();

    fcp.loadCheckpoint(S11);
    auto D39 = create_checkpoint();
    auto D40 = create_checkpoint();
    fcp.loadCheckpoint(D37);

    EXPECT_SNAPSHOT(S10);
    EXPECT_SNAPSHOT(S11);
    EXPECT_DELTA(D35);
    EXPECT_DELTA(D36);
    EXPECT_DELTA(D37);
    EXPECT_DELTA(D38);
    EXPECT_DELTA(D39);
    EXPECT_DELTA(D40);
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 8);
    EXPECT_EQUAL(fcp.getNumSnapshots(), 2);

    // Commit the current branch without forcing a new head checkpoint.
    // We should be left with:
    //   S11 -> D37       (fast checkpointer)
    //   S10 -> D35->D36  (added to DB)
    dbcp.commitCurrentBranch();
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 2);
    EXPECT_EQUAL(dbcp.getNumCheckpoints(), 39); // Got 3 new chkpts
    EXPECT_EQUAL(fcp.getCheckpointChain(D37).size(), 2);
    EXPECT_EQUAL(fcp.findCheckpoint(D37)->getRestoreChain().size(), 2);

    // Create these three branches:
    //   S11 -> D37->D41->D42
    //           |
    //           |-->D43->S12->D44->D45
    //                          |
    //                          |-->S13->D46 (current)
    auto D41 = create_checkpoint();
    auto D42 = create_checkpoint();
    fcp.loadCheckpoint(D37);
    auto D43 = create_checkpoint();
    auto S12 = create_checkpoint(true); // Force snapshot
    auto D44 = create_checkpoint();
    auto D45 = create_checkpoint();
    fcp.loadCheckpoint(D44);
    auto S13 = create_checkpoint(true); // Force snapshot
    auto D46 = create_checkpoint();

    EXPECT_SNAPSHOT(S12);
    EXPECT_SNAPSHOT(S13);
    EXPECT_DELTA(D41);
    EXPECT_DELTA(D42);
    EXPECT_DELTA(D43);
    EXPECT_DELTA(D44);
    EXPECT_DELTA(D45);
    EXPECT_DELTA(D46);
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 10);

    // Commit the current branch without forcing a new head checkpoint.
    // We should be left with:
    //   S13 -> D46                      (fast checkpointer)
    //   S11 -> D37->D43 -> S12 -> D44   (added to DB)
    dbcp.commitCurrentBranch();
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 2);
    EXPECT_EQUAL(fcp.getHeadID(), S13);
    EXPECT_EQUAL(dbcp.getNumCheckpoints(), 44); // Got 5 new chkpts
    EXPECT_EQUAL(fcp.getCheckpointChain(D46).size(), 2);
    EXPECT_EQUAL(fcp.findCheckpoint(D46)->getRestoreChain().size(), 2);

    // Finish
    app_mgrs.postSimLoopTeardown();
    root.enterTeardown();
    clocks.enterTeardown();
}

int main()
{
    RunCheckpointerTest();

    REPORT_ERROR;
    return ERROR_CODE;
}
