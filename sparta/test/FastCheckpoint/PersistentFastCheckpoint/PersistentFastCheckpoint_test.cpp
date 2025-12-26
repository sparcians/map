
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
#include "sparta/serialization/checkpoint/PersistentFastCheckpointer.hpp"
#include "sparta/serialization/checkpoint/FILEStream.hpp"

#include "sparta/utils/SpartaTester.hpp"

/*!
 * \file PersistentFastCheckpoint_test.cpp
 * \brief Test for Persistent Checkpoints
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
using sparta::serialization::checkpoint::PersistentFastCheckpointer;

static const uint16_t HINT_NONE=0;

//
// Some register and field definition tables
//

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

//! \brief General test for saving and loading PersistentFastCheckpoints
void generalTest()
{
    sparta::Scheduler sched;
    RootTreeNode clocks("clocks");
    sparta::Clock clk(&clocks, "clock", &sched);

    // Create a tree with some register sets and a memory
    RootTreeNode root;
    DummyDevice dummy(&root);
    std::unique_ptr<RegisterSet> rset(RegisterSet::create(&dummy, reg_defs));
    auto r1 = rset->getRegister("reg2");
    DummyDevice dummy2(&dummy);
    std::unique_ptr<RegisterSet> rset2(RegisterSet::create(&dummy2, reg_defs));
    auto r2 = rset2->getRegister("reg2");
    assert(r1 != r2);
    MemoryObject mem_obj(&dummy2, // underlying ArchData is associated and checkpointed through with this node.
                         64,      // 64B blocks
                         4096,    // 4k size
                         0xcc,    // fill with conspicuous bytes
                         1        // 1 byte of fill
                         );
    BlockingMemoryObjectIFNode mem_if(&dummy2, // Parent node
                                      "mem",   // Name
                                      "Memory interface",
                                      nullptr, // associated translation interface
                                      mem_obj);

    // Create the checkpointer

    PersistentFastCheckpointer pfcp(root, &sched);
    pfcp.setSnapshotThreshold(0);  // All checkpoints to be snapshots

    root.enterConfiguring();
    root.enterFinalized();

    // CHECKPOINT: HEAD
    r1->write<uint32_t>(0x0);
    r2->write<uint32_t>(0x1);
    uint8_t buf[32];
    uint8_t compare[32];
    memset(buf, 0x12, sizeof(buf));
    mem_if.write(0x100, 32, buf);

    EXPECT_NOTHROW(pfcp.createHead());

    // SAVE CHECKPOINT 1: Stored in data file "chkpt1"

    r1->write<uint32_t>(0x1);
    memset(buf, 0x34, sizeof(buf));
    mem_if.write(0x100, 32, buf);
    EXPECT_NOTHROW(pfcp.save("chkpt1"));
    sched.finalize();
    sched.run(10, true);

    // SAVE CHECKPOINT 2: Stored in data file "chkpt2"

    r1->write<uint32_t>(0x2);
    r2->write<uint32_t>(0x3);
    memset(buf, 0x56, sizeof(buf));
    mem_if.write(0x100, 32, buf);
    EXPECT_NOTHROW(pfcp.save("chkpt2"));
    sched.run(10, true);

    // SAVE CHECKPOINT 3: Stored in compressed data file "chkpt3.xz"

    r1->write<uint32_t>(0x4);
    r2->write<uint32_t>(0x5);
    memset(buf, 0x78, sizeof(buf));
    mem_if.write(0x100, 32, buf);
    FILE *pipeout = popen("xz -6 - > chkpt3.xz", "w");
    EXPECT_NOTEQUAL(pipeout, nullptr);
    {
        FILEOstream fos(pipeout);
        EXPECT_NOTHROW(pfcp.save(fos.getStream()));
    }
    EXPECT_EQUAL(pclose(pipeout), 0);
    sched.run(10, true);

    // SAVE CHECKPOINT 4: Stored in compressed data file "chkpt4.gz"

    r1->write<uint32_t>(0x6);
    r2->write<uint32_t>(0x7);
    memset(buf, 0x9a, sizeof(buf));
    mem_if.write(0x100, 32, buf);
    pipeout = popen("gzip - > chkpt4.gz", "w");
    EXPECT_NOTEQUAL(pipeout, nullptr);
    {
        FILEOstream fos(pipeout);
        EXPECT_NOTHROW(pfcp.save(fos.getStream()));
    }
    EXPECT_EQUAL(pclose(pipeout), 0);

    // RESTORE CHECKPOINT 2: Stored in data file "chkpt2"

    EXPECT_NOTHROW(pfcp.restore("chkpt2"));
    EXPECT_EQUAL(r1->read<uint32_t>(), 0x2);
    EXPECT_EQUAL(r2->read<uint32_t>(), 0x3);
    memset(buf, 0, sizeof(buf));
    memset(compare, 0x56, sizeof(buf));
    mem_if.read(0x100, 32, buf);
    EXPECT_TRUE(memcmp(buf, compare, 32) == 0);


    // RESTORE CHECKPOINT 4: Stored in compressed data file "chkpt4.gz"

    FILE *pipein = popen("gzip -d -c chkpt4.gz", "r");
    EXPECT_NOTEQUAL(pipein, nullptr);
    {
        FILEIstream fis(pipein);
        EXPECT_NOTHROW(pfcp.restore(fis.getStream()));
    }
    EXPECT_EQUAL(pclose(pipein), 0);
    EXPECT_EQUAL(r1->read<uint32_t>(), 0x6);
    EXPECT_EQUAL(r2->read<uint32_t>(), 0x7);
    memset(buf, 0, sizeof(buf));
    memset(compare, 0x9a, sizeof(buf));
    mem_if.read(0x100, 32, buf);
    EXPECT_TRUE(memcmp(buf, compare, 32) == 0);

    // RESTORE CHECKPOINT 3: Stored in compressed data file "chkpt3.xz"

    pipein = popen("xz -d -c chkpt3.xz", "r");
    EXPECT_NOTEQUAL(pipein, nullptr);
    {
        FILEIstream fis(pipein);
        EXPECT_NOTHROW(pfcp.restore(fis.getStream()));
    }
    EXPECT_EQUAL(pclose(pipein), 0);
    EXPECT_EQUAL(r1->read<uint32_t>(), 0x4);
    EXPECT_EQUAL(r2->read<uint32_t>(), 0x5);
    memset(buf, 0, sizeof(buf));
    memset(compare, 0x78, sizeof(buf));
    mem_if.read(0x100, 32, buf);
    EXPECT_TRUE(memcmp(buf, compare, 32) == 0);

    // RESTORE CHECKPOINT 1: Stored in file "chkpt1"

    EXPECT_NOTHROW(pfcp.restore("chkpt1"));
    EXPECT_EQUAL(r1->read<uint32_t>(), 0x1);
    EXPECT_EQUAL(r2->read<uint32_t>(), 0x1);
    memset(buf, 0, sizeof(buf));
    memset(compare, 0x34, sizeof(buf));
    mem_if.read(0x100, 32, buf);
    EXPECT_TRUE(memcmp(buf, compare, 32) == 0);
}

int main() {
    std::unique_ptr<sparta::log::Tap> warn_cerr(new sparta::log::Tap(sparta::TreeNode::getVirtualGlobalNode(),
                                                                 sparta::log::categories::WARN,
                                                                 std::cerr));

    std::unique_ptr<sparta::log::Tap> warn_file(new sparta::log::Tap(sparta::TreeNode::getVirtualGlobalNode(),
                                                                 sparta::log::categories::WARN,
                                                                 "persistent-fast-checkpointer-warnings.log"));

    generalTest();

    REPORT_ERROR;

    return ERROR_CODE;
}
