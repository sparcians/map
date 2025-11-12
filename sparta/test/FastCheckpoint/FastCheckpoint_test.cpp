#include <inttypes.h>
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
#include "sparta/serialization/checkpoint/FastCheckpointer.hpp"
#include "sparta/serialization/checkpoint/Checkpointable.hpp"

#include "sparta/utils/SpartaTester.hpp"

/*!
 * \file FastCheckpoint_test.cpp
 * \brief Test for Fast (non-persistent) Checkpoints
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

using sparta::LE; // Little
using sparta::BE; // Big

using sparta::serialization::checkpoint::FastCheckpointer;
using sparta::serialization::checkpoint::DeltaCheckpoint;

static const uint16_t HINT_NONE=0;

//
// Some register and field definition tables
//

Register::Definition reg_defs[] = {
    { 0, "reg0", Register::GROUP_NUM_NONE, "", Register::GROUP_IDX_NONE, "reg desc", 1,  {}, {}, nullptr, Register::INVALID_ID, 0, nullptr, HINT_NONE, 0 },
    { 1, "reg1", Register::GROUP_NUM_NONE, "", Register::GROUP_IDX_NONE, "reg desc", 2,  {}, {}, nullptr, Register::INVALID_ID, 0, nullptr, HINT_NONE, 0 },
    { 2, "reg2", Register::GROUP_NUM_NONE, "", Register::GROUP_IDX_NONE, "reg desc", 4,  {}, {}, nullptr, Register::INVALID_ID, 0, nullptr, HINT_NONE, 0 },
    { 3, "reg3", Register::GROUP_NUM_NONE, "", Register::GROUP_IDX_NONE, "reg desc", 8,  {}, {}, nullptr, Register::INVALID_ID, 0, nullptr, HINT_NONE, 0 },
    { 4, "reg4", Register::GROUP_NUM_NONE, "", Register::GROUP_IDX_NONE, "reg desc", 16, {}, {}, nullptr, Register::INVALID_ID, 0, nullptr, HINT_NONE, 0 },
    Register::DEFINITION_END
};


//! Dummy device
class DummyDevice : public sparta::TreeNode
{
public:

    DummyDevice(sparta::TreeNode* parent, uint16_t dum_id) :
        sparta::TreeNode(parent, "dummy" + std::to_string(dum_id), "", sparta::TreeNode::GROUP_IDX_NONE, "dummy node for register test"),
        checkpointables_(parent),
        checkpoint_struct_(checkpointables_.allocateCheckpointable<CheckpointStruct>(dum_id, "Hello")),
        checkpoint_int_(checkpointables_.allocateCheckpointable<uint64_t>(dum_id)),
        initial_struct_values_(dum_id, "Hello"),
        initial_int_value_(checkpoint_int_)
    {
    }

    void changeCPStates()
    {
        ++checkpoint_int_;
        ++checkpoint_struct_.checkpoint_int;
        ++checkpoint_struct_.checkpoint_float;
        if (checkpoint_struct_.checkpoint_int > 1) {
            ::strcpy(checkpoint_struct_.str, "There");
        }
        else {
            ::strcpy(checkpoint_struct_.str, "World");
        }
    }

    void printState() const {
        std::cout << getName() << ":\n\t"
                  << checkpoint_int_ << ", "
                  << checkpoint_struct_.checkpoint_int << ", "
                  << checkpoint_struct_.checkpoint_float << ", "
                  << checkpoint_struct_.str << std::endl;
    }

    void checkAgainstInitial() const {
        EXPECT_EQUAL(checkpoint_int_, initial_int_value_);
        EXPECT_EQUAL(checkpoint_struct_.checkpoint_int, initial_struct_values_.checkpoint_int);
        EXPECT_EQUAL(checkpoint_struct_.checkpoint_float, initial_struct_values_.checkpoint_float);
        EXPECT_TRUE(::strcmp(checkpoint_struct_.str, initial_struct_values_.str) == 0);
    }

private:
    sparta::Checkpointable checkpointables_;

    struct CheckpointStruct {
        CheckpointStruct(int id, const char * init_str) :
            checkpoint_int(id),
            checkpoint_float(id)
        {
            ::strcpy(str, init_str);
        }

        uint32_t checkpoint_int = 0;
        float    checkpoint_float = 0;
        char str[1024];
    };

    CheckpointStruct & checkpoint_struct_;
    uint64_t & checkpoint_int_;

    CheckpointStruct initial_struct_values_;
    const uint64_t initial_int_value_ = std::numeric_limits<uint64_t>::max();
};

//! \brief General test for checkpointing behavior. Creates/deletes/loads, etc.
void generalTest()
{
    sparta::Scheduler sched;
    RootTreeNode clocks("clocks");
    sparta::Clock clk(&clocks, "clock", &sched);

    // Create a tree with some register sets and a memory
    RootTreeNode root;
    DummyDevice dummy(&root, 0);
    std::unique_ptr<RegisterSet> rset(RegisterSet::create(&dummy, reg_defs));
    auto r1 = rset->getRegister("reg2");
    DummyDevice dummy2(&dummy, 1);
    std::unique_ptr<RegisterSet> rset2(RegisterSet::create(&dummy2, reg_defs));
    auto r2 = rset2->getRegister("reg2");
    assert(r1 != r2);
    MemoryObject mem_obj(&dummy2, // underlying ArchData is associated and checkpointed through with this node.
                         64, // 64B blocks
                         4096, // 4k size
                         0xcc, // fill with conspicuous bytes
                         1 // 1 byte of fill
                         );
    BlockingMemoryObjectIFNode mem_if(&dummy2, // Parent node
                                      "mem", // Name
                                      "Memory interface",
                                      nullptr, // associated translation interface
                                      mem_obj);

    // Print current register set by the ostream insertion operator
    std::cout << *rset << std::endl;

    // Create a checkpointer
    FastCheckpointer fcp(root, &sched);
    fcp.setSnapshotThreshold(5);

    root.enterConfiguring();
    root.enterFinalized();

    // Set up checkpointing (after tree finalization)
    EXPECT_EQUAL(sched.getCurrentTick(), 0); //unfinalized sched at tick 0

    // CHECKPOINT: HEAD
    r1->write<uint32_t>(0x0);
    r2->write<uint32_t>(0x1);
    uint8_t buf[32];
    memset(buf, 0x12, sizeof(buf));
    mem_if.write(0x100, 32, buf);

    FastCheckpointer::chkpt_id_t head_id;

    EXPECT_NOTHROW(fcp.createHead());
    EXPECT_THROW(fcp.createHead()); // Already has a head
    head_id = fcp.getHeadID();
    std::cout << "Register set @ cp-head" << std::endl;
    std::cout << *rset << std::endl << std::endl;

    dummy .printState();
    dummy2.printState();

    EXPECT_NOTEQUAL(fcp.getHead(), nullptr);
    EXPECT_EQUAL(fcp.getHead()->getID(), head_id);
    EXPECT_EQUAL(fcp.getCurrentID(), head_id); // Current is head because head is only checkpoint

    // CHECKPOINT: 1

    r1->write<uint32_t>(0x1);
    memset(buf, 0x34, sizeof(buf));
    mem_if.write(0x100, 32, buf);
    // NO CHANGE in r2 here // r2->write<uint32_t>(0x2);
    FastCheckpointer::chkpt_id_t first_id = 0;

    dummy.changeCPStates();
    dummy2.changeCPStates();

    EXPECT_NOTHROW(first_id = fcp.createCheckpoint());

    EXPECT_EQUAL(fcp.getCurrentID(), first_id);
    EXPECT_EQUAL(fcp.getCurrentID(), 1);
    std::cout << "Register set @ cp1" << std::endl;
    std::cout << *rset << std::endl << std::endl;
    dummy .printState();
    dummy2.printState();

    sched.finalize(); // Note that checkpoints could be created before this!

    // proceed to tick 1, nothing should happen, but time advancement
    sched.run(1, true, false);

    sched.run(10, true);

    // Scheduler's tick is zero-based
    EXPECT_EQUAL(sched.getCurrentTick(), 11);

    // CHECKPOINT: 2

    r1->write<uint32_t>(0x2);
    r2->write<uint32_t>(0x3);
    memset(buf, 0x56, sizeof(buf));
    mem_if.write(0x100, 32, buf);
    FastCheckpointer::chkpt_id_t second_id;

    dummy.changeCPStates();
    dummy2.changeCPStates();

    EXPECT_NOTHROW(second_id = fcp.createCheckpoint());

    EXPECT_EQUAL(fcp.getCurrentID(), second_id);
    EXPECT_EQUAL(fcp.getCurrentID(), 2);
    std::cout << "Register set @ cp2" << std::endl;
    std::cout << *rset << std::endl << std::endl;
    dummy .printState();
    dummy2.printState();

    sparta::Scheduler::Tick curtick = sched.getCurrentTick();
    sched.restartAt(curtick - 1); // Travel back in time (on the scheduler without telling the checkpointer)
    EXPECT_THROW(fcp.createCheckpoint()); // Cannot add checkpoint in the past (less than tick of current)
    sched.restartAt(curtick);

    dummy.changeCPStates();
    dummy2.changeCPStates();
    dummy .printState();
    dummy2.printState();

    // Note: To properly change the scheduler time without loading a checkpoint,
    // use Checkpointer::forgetCurrent() after changing time in the scheduler

    sched.run(10, true);
    // Scheduler's tick is zero-based
    EXPECT_EQUAL(sched.getCurrentTick(), 21);

    // Go back in time to cycle 1
    EXPECT_NOTHROW(fcp.loadCheckpoint(first_id));
    EXPECT_EQUAL(fcp.getCurrentID(), first_id);

    std::cout << "Register set @ cp1 (restored)" << std::endl;
    std::cout << *rset << std::endl << std::endl;
    EXPECT_EQUAL(r1->read<uint32_t>(), 0x1);
    EXPECT_EQUAL(r2->read<uint32_t>(), 0x1); // r2 was not written between head and cp1
    dummy.printState();
    dummy2.printState();
    dummy.checkAgainstInitial();
    dummy2.checkAgainstInitial();

    // proceed to tick 1, nothing should happen, but time advancement
    sched.run(1, true, false);

    EXPECT_EQUAL(sched.getCurrentTick(), 1);
    sched.run(2, true);
    EXPECT_EQUAL(sched.getCurrentTick(), 3);



    // CHECKPOINTS at time 3-9

    r1->write<uint32_t>(0x39);
    r2->write<uint32_t>(0x3a);
    FastCheckpointer::chkpt_id_t third_id = 0;

    EXPECT_NOTHROW(third_id = fcp.createCheckpoint());

    dummy.changeCPStates();
    dummy2.changeCPStates();

    EXPECT_EQUAL(fcp.getCurrentID(), third_id);
    EXPECT_EQUAL(fcp.getCurrentID(), 3);
    std::cout << "Register set @ cp3" << std::endl;
    std::cout << *rset << std::endl << std::endl;

    // Create some more checkpoints to test threshold
    const uint32_t NUM_CHECKS_IN_LOOP = 6;
    FastCheckpointer::chkpt_id_t chpts_b1[NUM_CHECKS_IN_LOOP];
    for(uint32_t i = 0; i < NUM_CHECKS_IN_LOOP; ++i){

        chpts_b1[i] = fcp.createCheckpoint();
        sched.run(1, true);
        EXPECT_EQUAL(sched.getCurrentTick(), 3+i+1);
        dummy.changeCPStates();
        dummy2.changeCPStates();
    }

    EXPECT_EQUAL(sched.getCurrentTick(), 3 + NUM_CHECKS_IN_LOOP);


    // Go back in time to cycle 5
    assert(NUM_CHECKS_IN_LOOP > 2);
    EXPECT_NOTHROW(fcp.loadCheckpoint(chpts_b1[2]));
    EXPECT_EQUAL(fcp.getCurrentID(), chpts_b1[2]);
    std::cout << "Register set @ cp" << chpts_b1[2] << " (restored)" << std::endl;
    std::cout << *rset << std::endl << std::endl;
    EXPECT_EQUAL(r1->read<uint32_t>(), 0x39);
    EXPECT_EQUAL(r2->read<uint32_t>(), 0x3a);

    EXPECT_EQUAL(sched.getCurrentTick(), 5);


    // CHECKPOINTS at time 5-11

    // Create some more checkpoints in a branch from here
    //FastCheckpointer::chkpt_id_t chpts_b2[NUM_CHECKS_IN_LOOP];
    r1->write<uint32_t>(0x511);
    r2->write<uint32_t>(0x512);
    for(uint32_t i = 0; i < NUM_CHECKS_IN_LOOP; ++i){

        /* chpts_b2[i] = */ fcp.createCheckpoint();
        sched.run(1, true);
        EXPECT_EQUAL(sched.getCurrentTick(), 5+i+1);
    }

    EXPECT_EQUAL(sched.getCurrentTick(), 5 + NUM_CHECKS_IN_LOOP);

    // Write memory
    memset(buf, 0xfff, sizeof(buf));
    mem_if.write(0x100, 32, buf);


    // Go back in time to the head (cycle 1)

    EXPECT_NOTHROW(fcp.loadCheckpoint(head_id));
    EXPECT_EQUAL(fcp.getCurrentID(), head_id);
    std::cout << "Register set @ cp-head (restored)" << std::endl;
    std::cout << *rset << std::endl << std::endl;
    EXPECT_EQUAL(r1->read<uint32_t>(0), 0x0);
    EXPECT_EQUAL(r2->read<uint32_t>(0), 0x1);
    uint8_t compare[32];
    memset(buf, 0, sizeof(buf));
    memset(compare, 0x12, sizeof(buf));
    mem_if.read(0x100, 32, buf);
    EXPECT_TRUE(memcmp(buf, compare, 32) == 0); // Checkpoint did not work if value is 0xfff

    // proceed to tick 1, nothing should happen, but time advancement
    sched.run(1, true, false);

    EXPECT_EQUAL(sched.getCurrentTick(), 1);


    // CHECKPOINTS at time 1-7

    // Create some more checkpoints in a branch from here
    //FastCheckpointer::chkpt_id_t chpts_b3[NUM_CHECKS_IN_LOOP];
    r1->write<uint32_t>(0x17);
    r2->write<uint32_t>(0x18);
    for(uint32_t i = 0; i < NUM_CHECKS_IN_LOOP; ++i){

        /* chpts_b3[i] = */ fcp.createCheckpoint();
        sched.run(1, true);
        EXPECT_EQUAL(sched.getCurrentTick(), 1+i+1);
    }

    EXPECT_EQUAL(sched.getCurrentTick(), NUM_CHECKS_IN_LOOP+1);

    std::cout << "\nCheckpoint Tree:" << std::endl;
    fcp.dumpTree(std::cout);
    std::cout << std::endl;
    fcp.dumpList(std::cout);

    std::cout << "\nCheckpoint Tree:" << std::endl;
    fcp.getCheckpointsAt(16);
    EXPECT_EQUAL(fcp.getCheckpoints().size(), 22);
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 22);
    EXPECT_EQUAL(fcp.getNumSnapshots(), 4);
    EXPECT_EQUAL(fcp.getNumDeltas(), 18);
    EXPECT_EQUAL(fcp.getCheckpointChain(12)[4], 5);
    EXPECT_EQUAL((fcp.findLatestCheckpointAtOrBefore(19, second_id)), fcp.findCheckpoint(second_id));


    // Delete some checkpoints

    EXPECT_EQUAL(fcp.getNumDeadCheckpoints(), 0);
    std::cout << "Deleting " << chpts_b1[3] << std::endl;
    fcp.deleteCheckpoint(chpts_b1[3]);
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 21);
    EXPECT_EQUAL(fcp.getNumDeadCheckpoints(), 1);
    fcp.dumpTree(std::cout);
    std::cout << std::endl;

    std::cout << "Deleting " << 18 << std::endl;
    fcp.deleteCheckpoint(18);
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 20);
    EXPECT_EQUAL(fcp.getNumDeadCheckpoints(), 2);
    fcp.dumpTree(std::cout);
    std::cout << std::endl;

    std::cout << "Deleting " << 2 << std::endl;
    fcp.deleteCheckpoint(2); // Should actually be deleted now
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 19);
    EXPECT_EQUAL(fcp.getNumDeadCheckpoints(), 2);
    fcp.dumpTree(std::cout);
    std::cout << std::endl;

    std::cout << "Deleting " << 15 << std::endl;
    fcp.deleteCheckpoint(15); // Should actually deleted now
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 18);
    EXPECT_EQUAL(fcp.getNumDeadCheckpoints(), 2);
    fcp.dumpTree(std::cout);
    std::cout << std::endl;

    // Delete 6, which is a delta preceeding 2 snapshots.
    // It should be immediately deleted
    std::cout << "Deleting " << 6 << std::endl;
    fcp.deleteCheckpoint(6); // Should actually deleted now
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 17);
    EXPECT_EQUAL(fcp.getNumDeadCheckpoints(), 2); // 7 & 18 are dead but still in the chain
    fcp.dumpTree(std::cout);
    std::cout << std::endl;

    std::cout << "\nCheckpoint Tree (After deletions):" << std::endl;
    fcp.dumpTree(std::cout);
    std::cout << std::endl;
    fcp.dumpList(std::cout);
    std::cout << "\nCheckpoint Data" << std::endl;
    fcp.dumpData(std::cout);
    std::cout << std::endl;
    std::cout << "\nAnnotated Checkpoint Data" << std::endl;
    fcp.dumpAnnotatedData(std::cout);
    std::cout << std::endl;


    // Look at a restore chain

    auto* cp20 = (fcp.findCheckpoint(20));
    auto rc20 = cp20->getRestoreChain();
    EXPECT_EQUAL(rc20.size(), 6); // 0 -> 16 -> 17 -> * -> 19 -> 20
    std::cout << "\nRestore chain for cp 20:" << std::endl;
    cp20->dumpRestoreChain(std::cout);
    std::cout << std::endl;

    // Test end-of-chain bug.
    //  1. write 0000 to reg
    //  2. Create checkpoint I (initial)
    //  3. Write aaaa to reg
    //  4. Create Checkpoint A
    //  5. Create Checkpoint C
    //  6. Delete Checkpoint C
    //  7. Delete Checkpoint A [Causes loss of checkpoint C data]
    //  8. Create Checkpoint B
    //  9. Load Checkpoint B
    //  A. Read 0000 from reg [INCORRECT value. Should read aaaa])

    std::deque<uint32_t> continues;

    r1->write<uint32_t>(0x0000);

    auto cpP = fcp.createCheckpoint();
    (void) cpP;
    r1->write<uint32_t>(0xaaaa);

    auto cpA = fcp.createCheckpoint();
    ////r1->write<uint32_t>(0xbbbb);
    std::cout << "Dumping restore chain for cpA (" << cpA << ")" << std::endl;
    fcp.findCheckpoint(cpA)->dumpRestoreChain(std::cout);
    std::cout << std::endl;
    continues.clear();
    fcp.dumpBranch(std::cout,
                   cpP,
                   0,
                   0,
                   continues);
    std::cout << std::endl;
    ///r1->write<uint32_t>(0xcccc);
    auto cpC = fcp.createCheckpoint();
    //////fcp.deleteCheckpoint(cpA);
    std::cout << "Dumping restore chain for cpC (" << cpC << ")" << std::endl;
    fcp.findCheckpoint(cpC)->dumpRestoreChain(std::cout);
    std::cout << std::endl;
    continues.clear();
    fcp.dumpBranch(std::cout,
                   cpP,
                   0,
                   0,
                   continues);
    std::cout << std::endl;
    std::cout << " Deleting C" << std::endl;
    fcp.deleteCheckpoint(cpC);
    continues.clear();
    fcp.dumpBranch(std::cout,
                   cpP,
                   0,
                   0,
                   continues);
    std::cout << std::endl;
    std::cout << " Deleting A" << std::endl;
    fcp.deleteCheckpoint(cpA);
    continues.clear();
    fcp.dumpBranch(std::cout,
                   cpP,
                   0,
                   0,
                   continues);
    std::cout << std::endl;

    auto cpB = fcp.createCheckpoint();
    fcp.loadCheckpoint(cpB);
    std::cout << "Dumping restore chain for cpB (" << cpB << ")" << std::endl;
    fcp.findCheckpoint(cpB)->dumpRestoreChain(std::cout);
    std::cout << std::endl;
    continues.clear();
    fcp.dumpBranch(std::cout,
                   cpP,
                   0,
                   0,
                   continues);
    std::cout << std::endl;
    EXPECT_EQUAL(r1->read<uint32_t>(), 0xaaaa);

    // Print out some other info
    float mem_per_chkpt = fcp.getTotalMemoryUse() / 1000.0f / fcp.getNumCheckpoints();
    float data_per_chkpt = fcp.getContentMemoryUse() / 1000.0f / fcp.getNumCheckpoints();
    std::cout << "\nMemory Use by this checkpointer: " << fcp.getTotalMemoryUse() / 1000.0f << " MB (" << mem_per_chkpt << " KB per chkpt)" << std::endl;
    std::cout << "Average content per checkpoint: " << data_per_chkpt << " KB per chkpt)" << std::endl;
    std::cout << "Total checkpoints created by this checkpointer: " << fcp.getTotalCheckpointsCreated() << std::endl;
    std::cout << "Current checkpoints: " << fcp.getNumCheckpoints() << std::endl;
    std::cout << "snapshots: " << fcp.getNumSnapshots() << std::endl;
    std::cout << "deltas: " << fcp.getNumDeltas() << std::endl;
    std::cout << "dead: " << fcp.getNumDeadCheckpoints() << std::endl;
    std::cout << "\n\n";

    std::cout << "ArchData associations: " << std::endl;
    root.validateArchDataAssociations(); // Check for unassociated ArchDatas or ArchDatas associated with unattached nodes
    root.dumpArchDataAssociations(std::cout);
    std::cout << "\n\n";

    // Teardown

    root.enterTeardown();
    clocks.enterTeardown();
}


/*! \brief Helper for stackTest
 *
 *  Pops stack until the desired checkpoint is reached.
 *
 *  This logic belongs in a Simulation class
 */
void restoreCheckpoint(std::stack<FastCheckpointer::chkpt_id_t>& ckpts,
                       FastCheckpointer& fcp,
                       sparta::Scheduler* sched,
                       FastCheckpointer::chkpt_id_t to_restore) {
    assert(sched);

    while(1){
        if(ckpts.empty()){
            throw sparta::serialization::checkpoint::CheckpointError("Could not find checkpoint ID ")
                << to_restore << " in the checkpoints stack during the stack test/example";
        }
        if(ckpts.top() == to_restore){
            std::cout << "Restoring chkpt " << ckpts.top() << std::endl;
            fcp.loadCheckpoint(to_restore);
            // Do not pop
            break;
        }else{
            // Pop all checkpoints later than desired restore point
            std::cout << "Popping chkpt " << ckpts.top() << std::endl;
            fcp.deleteCheckpoint(ckpts.top());
            ckpts.pop();
        }
    }
}


//! \brief Uses a stack to keep track of checkpoint IDs much like GPro would
void stackTest()
{
    std::cout << "Checkpoint test" << std::endl;
    sparta::Scheduler csched;

    sparta::Scheduler* sched = &csched;
    sched->finalize();
    sched->restartAt(1);

    sparta::Clock clk("clock", sched);

    // Place into a tree
    RootTreeNode root;
    DummyDevice dummy(&root, 0);
    std::unique_ptr<RegisterSet> rset(RegisterSet::create(&dummy, reg_defs));
    auto r = rset->getRegister("reg2");

    // Print current register set by the ostream insertion operator
    std::cout << r << std::endl;

    // Create checkpointer

    FastCheckpointer fcp(root, sched);
    fcp.setSnapshotThreshold(5);

    root.enterConfiguring();
    root.enterFinalized();

    // Stack for checkpoints

    std::stack<FastCheckpointer::chkpt_id_t> ckpts;

    // t=1
    EXPECT_EQUAL(sched->getCurrentTick(), 1); // Expected to start at t=1, or further comparisons will fail
    sched->run(10, true);

    // cp1 (0 is head and not in the stack)
    ckpts.push(fcp.createCheckpoint());
    EXPECT_EQUAL(fcp.getHeadID(), 0);
    EXPECT_EQUAL(ckpts.top(), 1);

    sched->run(10, true);

    // cp2, t=21
    ckpts.push(fcp.createCheckpoint());

    sched->run(10, true);

    // cp3, t=31
    ckpts.push(fcp.createCheckpoint());

    sched->run(10, true);

    // cp4, t=41
    ckpts.push(fcp.createCheckpoint());

    sched->run(10, true);

    // cp5, t=51
    ckpts.push(fcp.createCheckpoint());

    sched->run(10, true);

    // cp6, t=61
    ckpts.push(fcp.createCheckpoint());

    fcp.dumpTree(std::cout);
    std::cout << std::endl;

    // Restore cp3
    restoreCheckpoint(ckpts, fcp, sched, 3);
    EXPECT_EQUAL(sched->getCurrentTick(), 31); // At tick 31
    EXPECT_EQUAL(ckpts.size(), 3); // 3 remaining

    fcp.dumpTree(std::cout);
    std::cout << std::endl;

    sched->run(10, true);

    // cp7, t=41
    ckpts.push(fcp.createCheckpoint());

    sched->run(10, true);

    // cp8, t=51
    ckpts.push(fcp.createCheckpoint());

    sched->run(10, true);

    // cp9, t=61
    ckpts.push(fcp.createCheckpoint());

    sched->run(10, true);

    // cp10, t=71
    ckpts.push(fcp.createCheckpoint());

    fcp.dumpTree(std::cout);
    std::cout << std::endl;

    restoreCheckpoint(ckpts, fcp, sched, 8);
    EXPECT_EQUAL(sched->getCurrentTick(), 51); // At tick 51
    EXPECT_EQUAL(ckpts.size(), 5); // 5 remaining

    fcp.dumpTree(std::cout);
    std::cout << std::endl;


    // Teardown

    root.enterTeardown();
}

void deletionTest1()
{
    sparta::Scheduler sched;
    RootTreeNode clocks("clocks");
    sparta::Clock clk(&clocks, "clock", &sched);

    // Create a tree with some register sets and a memory
    RootTreeNode root;
    DummyDevice dummy(&root, 0);
    std::unique_ptr<RegisterSet> rset(RegisterSet::create(&dummy, reg_defs));
    auto r1 = rset->getRegister("reg2");
    DummyDevice dummy2(&dummy, 1);
    std::unique_ptr<RegisterSet> rset2(RegisterSet::create(&dummy2, reg_defs));
    auto r2 = rset2->getRegister("reg2");
    assert(r1 != r2);
    MemoryObject mem_obj(&dummy2, // underlying ArchData is associated and checkpointed through with this node.
                         64, // 64B blocks
                         4096, // 4k size
                         0xcc, // fill with conspicuous bytes
                         1 // 1 byte of fill
                         );
    BlockingMemoryObjectIFNode mem_if(&dummy2, // Parent node
                                      "mem", // Name
                                      "Memory interface",
                                      nullptr, // associated translation interface
                                      mem_obj);

    // Print current register set by the ostream insertion operator
    std::cout << *rset << std::endl;


    // Create a checkpointer

    FastCheckpointer fcp(root, &sched);
    fcp.setSnapshotThreshold(5);

    root.enterConfiguring();
    root.enterFinalized();
    sched.finalize();


    // CHECKPOINT: HEAD
    r1->write<uint32_t>(0x0);
    r2->write<uint32_t>(0x1);
    uint8_t buf[32];
    memset(buf, 0x12, sizeof(buf));
    mem_if.write(0x100, 32, buf);

    //FastCheckpointer::chkpt_id_t head_id;

    fcp.setSnapshotThreshold(5);

    EXPECT_NOTHROW(fcp.createHead());

    auto c1 = fcp.createCheckpoint();
    fcp.loadCheckpoint(c1);
    fcp.deleteCheckpoint(c1);
    auto c2 = fcp.createCheckpoint();
    fcp.loadCheckpoint(c2);
    fcp.deleteCheckpoint(c2);
    auto c3 = fcp.createCheckpoint();
    fcp.loadCheckpoint(c3);
    fcp.deleteCheckpoint(c3);
    auto c4 = fcp.createCheckpoint();
    fcp.loadCheckpoint(c4);
    fcp.deleteCheckpoint(c4);
    auto c5 = fcp.createCheckpoint();
    fcp.loadCheckpoint(c5);
    fcp.deleteCheckpoint(c5);
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 1); // Just the head is left
    EXPECT_EQUAL(fcp.getNumDeadCheckpoints(), 5); // 1-5
    auto c6 = fcp.createCheckpoint(); // SNAPSHOT
    fcp.loadCheckpoint(c6);
    fcp.deleteCheckpoint(c6);

    std::cout << "deletionTest1 end state" << std::endl;
    std::cout << "num chkpts " << fcp.getNumCheckpoints() << std::endl;
    std::cout << "num snaps  " << fcp.getNumSnapshots() << std::endl;
    std::cout << "num deltas " << fcp.getNumDeltas() << std::endl;
    std::cout << "num dead   " << fcp.getNumDeadCheckpoints() << std::endl;


    // Result should be just 1 real checkpoint (head):
    // "-> 0 (s) -> [ *6 (s) ]"
    fcp.dumpTree(std::cout);
    std::cout << std::endl;

    EXPECT_EQUAL(fcp.getNumCheckpoints(), 1); // just the head
    EXPECT_EQUAL(fcp.getNumDeadCheckpoints(), 1); // just c6 (the snapshot)

    // Teardown
    root.enterTeardown();
    clocks.enterTeardown();
}

void deletionTest2()
{
    sparta::Scheduler sched;
    RootTreeNode clocks("clocks");
    sparta::Clock clk(&clocks, "clock", &sched);

    // Create a tree with some register sets and a memory
    RootTreeNode root;
    DummyDevice dummy(&root, 0);
    std::unique_ptr<RegisterSet> rset(RegisterSet::create(&dummy, reg_defs));
    auto r1 = rset->getRegister("reg2");
    DummyDevice dummy2(&dummy, 1);
    std::unique_ptr<RegisterSet> rset2(RegisterSet::create(&dummy2, reg_defs));
    auto r2 = rset2->getRegister("reg2");
    assert(r1 != r2);
    MemoryObject mem_obj(&dummy2, // underlying ArchData is associated and checkpointed through with this node.
                         64, // 64B blocks
                         4096, // 4k size
                         0xcc, // fill with conspicuous bytes
                         1 // 1 byte of fill
                         );
    BlockingMemoryObjectIFNode mem_if(&dummy2, // Parent node
                                      "mem", // Name
                                      "Memory interface",
                                      nullptr, // associated translation interface
                                      mem_obj);

    // Print current register set by the ostream insertion operator
    std::cout << *rset << std::endl;


    // Create a checkpointer

    FastCheckpointer fcp(root, &sched);
    fcp.setSnapshotThreshold(5);

    root.enterConfiguring();
    root.enterFinalized();


    // CHECKPOINT: HEAD
    r1->write<uint32_t>(0x0);
    r2->write<uint32_t>(0x1);
    uint8_t buf[32];
    memset(buf, 0x12, sizeof(buf));
    mem_if.write(0x100, 32, buf);

    //FastCheckpointer::chkpt_id_t head_id;

    fcp.setSnapshotThreshold(5);

    EXPECT_NOTHROW(fcp.createHead());

    auto c1 = fcp.createCheckpoint();
    fcp.deleteCheckpoint(c1);
    auto c2 = fcp.createCheckpoint();
    fcp.deleteCheckpoint(c2);
    auto c3 = fcp.createCheckpoint();
    fcp.deleteCheckpoint(c3);
    auto c4 = fcp.createCheckpoint();
    fcp.deleteCheckpoint(c4);
    auto c5 = fcp.createCheckpoint();
    // DO NOT DELETE 5
    auto c6 = fcp.createCheckpoint(); // SNAPSHOT
    // DO NOT DELETE 6
    EXPECT_EQUAL(fcp.getNumCheckpoints(), 3); // just head,c5,c6 (the snapshot)
    EXPECT_EQUAL(fcp.getNumDeadCheckpoints(), 4); // just 1,2,3,4

    // 1-5 should still exist.

    std::cout << "num chkpts " << fcp.getNumCheckpoints() << std::endl;
    std::cout << "num snaps  " << fcp.getNumSnapshots() << std::endl;
    std::cout << "num deltas " << fcp.getNumDeltas() << std::endl;
    std::cout << "num dead   " << fcp.getNumDeadCheckpoints() << std::endl;

    fcp.dumpTree(std::cout);
    std::cout << std::endl;


    fcp.deleteCheckpoint(c6); // Should do nothing

    fcp.dumpTree(std::cout);
    std::cout << std::endl;

    fcp.deleteCheckpoint(c5); // Should free c1-c5

    std::cout << "deletionTest2 end state" << std::endl;
    fcp.dumpTree(std::cout);
    std::cout << std::endl;

    EXPECT_EQUAL(fcp.getNumCheckpoints(), 1); // just the head
    EXPECT_EQUAL(fcp.getNumDeadCheckpoints(), 1); // just c6 (the snapshot)

    // Teardown
    root.enterTeardown();
    clocks.enterTeardown();
}


void deletionTest3()
{
    RootTreeNode clocks("clocks");
    sparta::Scheduler sched;
    sparta::Clock clk(&clocks, "clock", &sched);

    // Create a tree with some register sets and a memory
    RootTreeNode root;
    DummyDevice dummy(&root, 0);
    std::unique_ptr<RegisterSet> rset(RegisterSet::create(&dummy, reg_defs));
    auto r1 = rset->getRegister("reg2");
    DummyDevice dummy2(&dummy, 1);
    std::unique_ptr<RegisterSet> rset2(RegisterSet::create(&dummy2, reg_defs));
    auto r2 = rset2->getRegister("reg2");
    assert(r1 != r2);
    MemoryObject mem_obj(&dummy2, // underlying ArchData is associated and checkpointed through with this node.
                         64, // 64B blocks
                         4096, // 4k size
                         0xcc, // fill with conspicuous bytes
                         1 // 1 byte of fill
                         );
    BlockingMemoryObjectIFNode mem_if(&dummy2, // Parent node
                                      "mem", // Name
                                      "Memory interface",
                                      nullptr, // associated translation interface
                                      mem_obj);

    // Print current register set by the ostream insertion operator
    std::cout << *rset << std::endl;


    // Create a checkpointer

    FastCheckpointer fcp(root, &sched);
    fcp.setSnapshotThreshold(5);

    root.enterConfiguring();
    root.enterFinalized();
    sched.finalize();

    // CHECKPOINT: HEAD
    r1->write<uint32_t>(0x0);
    r2->write<uint32_t>(0x1);
    uint8_t buf[32];
    memset(buf, 0x12, sizeof(buf));
    mem_if.write(0x100, 32, buf);

    //FastCheckpointer::chkpt_id_t head_id;

    fcp.setSnapshotThreshold(5);

    EXPECT_NOTHROW(fcp.createHead());

    auto c1 = fcp.createCheckpoint();
    fcp.deleteCheckpoint(c1);
    auto c2 = fcp.createCheckpoint();
    fcp.deleteCheckpoint(c2);
    auto c3 = fcp.createCheckpoint();
    fcp.deleteCheckpoint(c3);
    auto c4 = fcp.createCheckpoint();
    fcp.deleteCheckpoint(c4);
    auto c5 = fcp.createCheckpoint();
    fcp.deleteCheckpoint(c5);
    auto c6 = fcp.createCheckpoint(); // SNAPSHOT
    (void) c6;

    // NOTE: Snapshot should ause c1-c5 to be removed permanently

    std::cout << "num chkpts " << fcp.getNumCheckpoints() << std::endl;
    std::cout << "num snaps  " << fcp.getNumSnapshots() << std::endl;
    std::cout << "num deltas " << fcp.getNumDeltas() << std::endl;
    std::cout << "num dead   " << fcp.getNumDeadCheckpoints() << std::endl;

    std::cout << "deletionTest3 end state" << std::endl;
    fcp.dumpTree(std::cout);
    std::cout << std::endl;

    EXPECT_EQUAL(fcp.getNumCheckpoints(), 2); // just head,c6 (the snapshot)
    EXPECT_EQUAL(fcp.getNumDeadCheckpoints(), 0); // Creation of snapshot should have removed all

    // Teardown
    root.enterTeardown();
    clocks.enterTeardown();
}

void speedTest1()
{
    RootTreeNode clocks("clocks");
    sparta::Scheduler sched;
    sparta::Clock clk(&clocks, "clock", &sched);

    // Create a tree with some register sets and a memory
    RootTreeNode root;
    DummyDevice dummy(&root, 0);
    std::unique_ptr<RegisterSet> rset(RegisterSet::create(&dummy, reg_defs));
    auto r1 = rset->getRegister("reg2");
    DummyDevice dummy2(&dummy, 1);
    std::unique_ptr<RegisterSet> rset2(RegisterSet::create(&dummy2, reg_defs));
    auto r2 = rset2->getRegister("reg2");
    assert(r1 != r2);
    MemoryObject mem_obj(&dummy2, // underlying ArchData is associated and checkpointed through with this node.
                         64, // 64B blocks
                         4096, // 4k size
                         0xcc, // fill with conspicuous bytes
                         1 // 1 byte of fill
                         );
    BlockingMemoryObjectIFNode mem_if(&dummy2, // Parent node
                                      "mem", // Name
                                      "Memory interface",
                                      nullptr, // associated translation interface
                                      mem_obj);

    // Print current register set by the ostream insertion operator
    std::cout << *rset << std::endl;


    // Create a checkpointer

    FastCheckpointer fcp(root, &sched);
    fcp.setSnapshotThreshold(5);

    root.enterConfiguring();
    root.enterFinalized();
    sched.finalize();

    // CHECKPOINT: HEAD
    r1->write<uint32_t>(0x0);
    r2->write<uint32_t>(0x1);
    uint8_t buf[32];
    memset(buf, 0x12, sizeof(buf));
    mem_if.write(0x100, 32, buf);

    //FastCheckpointer::chkpt_id_t head_id;

    fcp.setSnapshotThreshold(20);

    EXPECT_NOTHROW(fcp.createHead());

    for(uint32_t i = 0; i < 500000; i++){
        r1->write<uint32_t>(0x0); // Write to dirty a page
        auto c = fcp.createCheckpoint();
        fcp.loadCheckpoint(c);
        fcp.deleteCheckpoint(c);
    }

    // Teardown
    root.enterTeardown();
    clocks.enterTeardown();
}

int main()
{
    std::unique_ptr<sparta::log::Tap> warn_cerr(new sparta::log::Tap(sparta::TreeNode::getVirtualGlobalNode(),
                                                                 sparta::log::categories::WARN,
                                                                 std::cerr));

    std::unique_ptr<sparta::log::Tap> warn_file(new sparta::log::Tap(sparta::TreeNode::getVirtualGlobalNode(),
                                                                 sparta::log::categories::WARN,
                                                                 "warnings.log"));

    generalTest();
    stackTest();
    deletionTest1();
    deletionTest2();
    deletionTest3();

    clock_t start = clock();
    std::array<clock_t, 5> times{{0,0,0,0,0}};
    for(uint32_t i = 0; i < times.size(); i++){
        clock_t istart = clock();
        speedTest1();
        clock_t idelta = clock() - istart;
        times[i] = idelta;
    }
    clock_t delta = clock() - start;
    std::cout << "Speed text " << times.size() << " iterations took " << float(delta) / CLOCKS_PER_SEC << "s" << std::endl;
    for(uint32_t i = 0; i < times.size(); i++){
        std::cout << "iter " << i << " = " << float(times[i]) / CLOCKS_PER_SEC << "s" << std::endl;
    }

    sparta::log::DestinationManager::dumpDestinations(std::cout);

    // Done

    REPORT_ERROR;

    return ERROR_CODE;
}
