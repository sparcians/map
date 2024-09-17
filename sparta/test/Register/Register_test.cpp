
#include <inttypes.h>
#include <iostream>

#include <boost/timer/timer.hpp>

#include "sparta/sparta.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/functional/Register.hpp"
#include "sparta/functional/RegisterSet.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/utils/Utils.hpp"

/* sparta::RegisterSet::getArchData() has been removed. Until the functionality
 * it provides have been replaced by other means, tests relying on it are not
 * compiled if REGISTER_SET_GET_ARCH_DATA_REMOVED is defined. */
#define REGISTER_SET_GET_ARCH_DATA_REMOVED

/*!
 * \file main.cpp
 * \brief Test for Register
 *
 * Register is built on DataView and RegisterSet is built on ArchData.
 * The DataView test performs extensive testing so some test-cases related
 * to register sizes and layouts may be omitted from this test.
 */

TEST_INIT

using namespace sparta;

//
// Some register and field definition tables
//

const char* reg1_aliases[] = { "regnum1", "firstreg", 0 };

const uint64_t MEDIUM_DEFAULT[1] = { 0xabacadabab0220cc };
const uint64_t ALTERNATING_DEFAULT[] = { 0xaaaaaaaaaaaaaaaa, 0xaaaaaaaaaaaaaaaa, 0xaaaaaaaaaaaaaaaa, 0xaaaaaaaaaaaaaaaa,
                                         0xaaaaaaaaaaaaaaaa, 0xaaaaaaaaaaaaaaaa, 0xaaaaaaaaaaaaaaaa, 0xaaaaaaaaaaaaaaaa };
const uint16_t HINT_READ_ONLY = 0xabcd;

Register::Definition reg_defs[] = {
    { 0,  "reg1",     1,                        "A", 0,                        "reg 1 description",         4, { { "field1", "this is field 1. It is 2 bits", 0, 1 },
                                                                                                                 { "field2", "this is field 2. It is 4 bits", 0, 3 },
                                                                                                                 { "field3", "this is field 3. It is 3 bits and overlaps field1 and field2", 1, 3 } },
                                                                                                                         {Register::BANK_IDX_DEFAULT},
                                                                                                                                  reg1_aliases,       Register::INVALID_ID, 0, nullptr, 0, 0 },
    { 1,  "medium",   2,                        "B", 0,                        "register that is 8 bytes",  8,  {},      {6},     nullptr,            Register::INVALID_ID, 0, (const uint8_t*) MEDIUM_DEFAULT, 0, 0 },
    { 101,"medium2",  2,                        "B", 0,                        "register that is 8 bytes",  8,  {},      {5},     nullptr,            Register::INVALID_ID, 0, (const uint8_t*) MEDIUM_DEFAULT, 0, 0 },
    { 102,"medium3",  2,                        "B", 0,                        "register that is 8 bytes",  8,  {},      {4},     nullptr,            Register::INVALID_ID, 0, (const uint8_t*) MEDIUM_DEFAULT, 0, 0 },
    { 2,  "large",    2,                        "B", 1,                        "register that is 16 bytes", 16, { { "b15_00",   "A", 0,   15  },
                                                                                                                  { "b31_16",   "B", 16,  31  },
                                                                                                                  { "b47_32",   "C", 32,  47  },
                                                                                                                  { "b63_48",   "D", 48,  63  },
                                                                                                                  { "b79_64",   "E", 64,  79  },
                                                                                                                  { "b95_80",   "F", 80,  95  },
                                                                                                                  { "b111_96",  "G", 96,  111 },
                                                                                                                  { "b127_112", "H", 112, 127 },
                                                                                                                  { "middle",   "I", 56,  71  } },
      {},      nullptr,            Register::INVALID_ID, 0, nullptr, HINT_READ_ONLY, 0 },
    { 3,  "sprXXa",   4,                        "D", 2,                        "example SPR",               4,  { { "b07_00", "LSB", 0, 7 },
                                                                                                                  { "b15_08", "less significant", 8, 15 },
                                                                                                                  { "b19_12", "middle", 12, 19 },
                                                                                                                  { "b27_03", "other", 3, 27 },
                                                                                                                  { "b23_16", "more significant", 16, 23 },
                                                                                                                  { "b31_24", "MSB", 24, 31 } },
                                                                                                                         {0,1,2}, nullptr,            Register::INVALID_ID, 0, nullptr, 0, 0 },
    { 301,"sprXXb",   4,                        "D", 2,                        "example SPR",               4,  {  },    {4,5,6}, nullptr,            Register::INVALID_ID, 0, nullptr, 0, 0 },
    { 4,  "small",    Register::GROUP_NUM_NONE, "",  Register::GROUP_IDX_NONE, "1 byte reg",                1,  {},      {},      nullptr,            Register::INVALID_ID, 0, nullptr, 0, 0 },
    { 5,  "large_x0", Register::GROUP_NUM_NONE, "",  Register::GROUP_IDX_NONE, "subset of large",           4,  {},      {},      nullptr,            2, 0x0, nullptr, 0, 0 },
    { 6,  "large_x4", Register::GROUP_NUM_NONE, "",  Register::GROUP_IDX_NONE, "subset of large",           4,  {},      {},      nullptr,            2, 0x4, nullptr, 0, 0 },
    { 7,  "large_x8", Register::GROUP_NUM_NONE, "",  Register::GROUP_IDX_NONE, "subset of large",           4,  {},      {},      nullptr,            2, 0x8, nullptr, 0, 0 },
    { 8,  "large_xC", Register::GROUP_NUM_NONE, "",  Register::GROUP_IDX_NONE, "subset of large",           4,  {},      {},      nullptr,            2, 0xc, nullptr, 0, 0 },
    { 9,  "large_x3", Register::GROUP_NUM_NONE, "",  Register::GROUP_IDX_NONE, "misaligned subset of large",4,  {},      {},      nullptr,            2, 0x3, nullptr, 0, 0 },
    { 10, "large_x6", Register::GROUP_NUM_NONE, "",  Register::GROUP_IDX_NONE, "misaligned subset of large",4,  {},      {},      nullptr,            2, 0x6, nullptr, 0, 0 },

    // Test write-mask
    { 11, "wm_01",    Register::GROUP_NUM_NONE, "",  Register::GROUP_IDX_NONE, "partially masked",          4,  { {"b03_00",  "A", 0,  3,   true },
                                                                                                                  {"b09_05",  "B", 5,  9,   true },
                                                                                                                  {"b15_12",  "C", 12, 15,  false} },  {},      nullptr,            Register::INVALID_ID, 0, (const uint8_t*) ALTERNATING_DEFAULT, 0, 0 },
    { 12, "wm_02",    Register::GROUP_NUM_NONE, "",  Register::GROUP_IDX_NONE, "fully unwritable",          8,  { {"b63_00",  "A", 0,  63,  true } },  {},      nullptr,            Register::INVALID_ID, 0, (const uint8_t*) ALTERNATING_DEFAULT, 0, 0 },
    { 13, "wm_03",    Register::GROUP_NUM_NONE, "",  Register::GROUP_IDX_NONE, "fully writable",            16, { {"b127_64", "A", 64, 127, false},
                                                                                                                  {"b63_00",  "B", 0,  63,  false} },  {},      nullptr,            Register::INVALID_ID, 0, (const uint8_t*) ALTERNATING_DEFAULT, 0, 0 },
    { 14, "wm_04",    Register::GROUP_NUM_NONE, "",  Register::GROUP_IDX_NONE, "mask spans u64s",           16, { {"b65_13",  "A", 13, 65,  true },
                                                                                                                  {"b67_64",  "B", 64, 67,  false} },  {},      nullptr,            Register::INVALID_ID, 0, (const uint8_t*) ALTERNATING_DEFAULT, 0, 0 },

    // Test very huge register
    { 15, "huge",     Register::GROUP_NUM_NONE, "",  Register::GROUP_IDX_NONE, "register that is 32 bytes", 32, {},      {},      nullptr,            Register::INVALID_ID, 0, nullptr, 0, 0 },
    Register::DEFINITION_END
};

RegisterProxy::Definition proxy_defs[] = {
    { 302,"sprxx",    4,                        "D", 2,                        "example SPR PROXY"},
    RegisterProxy::DEFINITION_END
};


//! Dummy devivce
class DummyDevice : public sparta::TreeNode
{
public:

    DummyDevice(sparta::TreeNode* parent) :
        sparta::TreeNode(parent, "dummy", "", sparta::TreeNode::GROUP_IDX_NONE, "dummy node for register test")
    {}
};


template <typename RegReadSizeT>
class RegPostWriteObserver
{
public:
    uint32_t writes_1;
    uint32_t writes_2;
    RegReadSizeT pre;
    RegReadSizeT post;

    RegPostWriteObserver() :
        writes_1(0), writes_2(0), pre(0), post(0)
    { }

    void expect(RegReadSizeT expect_pre, RegReadSizeT expect_post) {
        pre = expect_pre;
        post = expect_post;
    }

    void registerForCb1(RegisterBase *r)
    {
        r->getPostWriteNotificationSource().REGISTER_FOR_THIS(callback1);
        // r->getPostWriteNotificationSource().REGISTER_FOR_THIS(callbackTemplate<int, int>);
        // r->getPostWriteNotificationSource().registerForThis<RegPostWriteObserver, &RegPostWriteObserver<RegReadSizeT>::callback1>(this);
        //r->getPostWriteNotificationSource().registerForThis<RegPostWriteObserver, &RegPostWriteObserver::callbackTemplate<int, int>>(this);
    }

    void deregisterForCb1(RegisterBase *r)
    {
        r->getPostWriteNotificationSource().DEREGISTER_FOR_THIS(callback1);
        //r->getPostWriteNotificationSource().deregisterForThis<RegPostWriteObserver, &RegPostWriteObserver::callback1>(this);
    }

    void registerForCb2(RegisterBase *r)
    {
        r->getPostWriteNotificationSource().REGISTER_FOR_THIS(callback2);
    }

    void deregisterForCb2(RegisterBase *r)
    {
        r->getPostWriteNotificationSource().DEREGISTER_FOR_THIS(callback2);
    }

    void callback1(const sparta::TreeNode& origin, const sparta::TreeNode& obs_pt, const Register::PostWriteAccess& data) {
        (void) origin;
        (void) obs_pt;
        //std::cout << "Callback for POST-WRITE register " << *data.reg
        //          << " from node " << origin << " observed on " << obs_pt << std::endl;
        EXPECT_EQUAL(data.prior->read<RegReadSizeT>(), pre);
        EXPECT_EQUAL(data.final->read<RegReadSizeT>(), post);
        writes_1++;
    }

    void callback2(const Register::PostWriteAccess& data) {
        EXPECT_EQUAL(data.prior->read<RegReadSizeT>(), pre);
        EXPECT_EQUAL(data.final->read<RegReadSizeT>(), post);
        writes_2++;
    }

    // Used to test a template type
    template<class T1, class T2>
    void callbackTemplate(const sparta::TreeNode&, const sparta::TreeNode&, const Register::PostWriteAccess&) { }

};

template <typename RegReadSizeT>
class FastRegPostWriteObserver
{
public:
    uint32_t writes;

    FastRegPostWriteObserver() :
        writes(0)
    { }

    void registerFor(RegisterBase *r)
    {
        r->getPostWriteNotificationSource().REGISTER_FOR_THIS(callback);
    }

    void deregisterFor(RegisterBase *r)
    {
        r->getPostWriteNotificationSource().DEREGISTER_FOR_THIS(callback);
    }

    void callback(const sparta::TreeNode& origin, const sparta::TreeNode& obs_pt, const Register::PostWriteAccess& data){
        (void) origin;
        (void) obs_pt;
        (void) data;
        writes++;
    }
};

template <typename RegReadSizeT>
class RegReadObserver
{
public:
    uint32_t reads;
    RegReadSizeT expected;

    RegReadObserver() :
        reads(0), expected(0)
    { }

    void expect(RegReadSizeT _expected) {
        expected = _expected;
    }

    void registerFor(RegisterBase *r)
    {
        r->getReadNotificationSource().REGISTER_FOR_THIS(callback);
        //r->getReadNotificationSource().registerForThis<RegReadObserver, &RegReadObserver::callback>(this);
        //r->getReadNotificationSource().REGISTER_FOR_NOTIFICATION(callback,
        //                                                         sparta::Register::ReadAccess,
        //                                                         r->getReadNotificationSource().getNotificationName());
    }

    void deregisterFor(RegisterBase *r)
    {
        r->getReadNotificationSource().DEREGISTER_FOR_THIS(callback);
        //r->getReadNotificationSource().deregisterForThis<RegReadObserver, &RegReadObserver::callback>(this);
    }

    void callback(const sparta::TreeNode& origin, const sparta::TreeNode& obs_pt, const Register::ReadAccess& data){
        (void) origin;
        (void) obs_pt;
        //std::cout << "Callback for READ from register " << *data.reg
        //          << " from node " << origin << " observed on " << obs_pt << std::endl;
        EXPECT_EQUAL(data.value->read<RegReadSizeT>(), expected);
        reads++;
    }
};

// Helper for testing out notification registration
template <typename T>
class CallbackDummy
{
public:
    void callback1(const sparta::TreeNode& origin, const sparta::TreeNode& obs_pt, const T& data) {
        (void) origin;
        (void) obs_pt;
        (void) data;
    }
    void callback2(const T& data) {
        (void) data;
    }
};

void dumpRegisterDefnsToJSON(const std::string& filename, Register::Definition* defs)
{
    std::ofstream json_file(filename);
    if (!json_file.is_open()) {
        throw SpartaException("Failed to open file for writing: ") << filename;
    }

    json_file << "[\n";
    for (RegisterBase::Definition* def = defs; def->name != nullptr; ++def) {
        json_file << "  {\n";
        json_file << "    \"name\": \"" << def->name << "\",\n";
        json_file << "    \"num\": " << def->id << ",\n";
        json_file << "    \"desc\": \"" << def->desc << "\",\n";
        json_file << "    \"size\": " << def->bytes << ",\n";

        json_file << "    \"aliases\": [";
        if (def->aliases == nullptr) {
            json_file << "],\n";
        } else {
            json_file << "\n";
            for (auto alias = def->aliases; alias != nullptr; ++alias) {
                json_file << "      \"" << *alias << "\"";
                if (*(alias + 1) != nullptr) {
                    json_file << ",";
                }
                json_file << "\n";
            }
            json_file << "    ],\n";
        }

        json_file << "    \"fields\": {\n";
        for (const auto& field : def->fields) {
            json_file << "      \"" << field.name << "\": {\n";
            json_file << "        \"desc\": \"" << field.desc << "\",\n";
            json_file << "        \"low_bit\": " << field.low_bit << ",\n";
            json_file << "        \"high_bit\": " << field.high_bit << ",\n";
            json_file << "        \"readonly\": " << std::boolalpha << field.read_only << "\n";
            json_file << "      }";
            if (&field != &def->fields.back()) {
                json_file << ",";
            }
            json_file << "\n";
        }
        json_file << "    },\n";

        json_file << "    \"group_name\": \"" << def->group << "\",\n";
        json_file << "    \"group_num\": " << def->group_num << "\n";

        json_file << "  }";
        if ((def + 1)->name != nullptr) {
            json_file << ",";
        }
        json_file << "\n";
    }

    json_file << "]\n";
}

//! Issue 89, test field register writes to large fields
void testFieldRegisterWrite(bool use_json = false)
{
    Register::ident_type new_regid = 1000; // Start counting at some unique ID

    // Register with large fields
    Register::Definition good_reg_defs[] = {
        {new_regid++, "fp_reg",   Register::GROUP_NUM_NONE, "", Register::GROUP_IDX_NONE, "description", 8,
         {{"dp", "Double precision", 0, 63}, {"sp", "single precision", 0, 31}}, {}, 0, Register::INVALID_ID, 0, nullptr, 0,0},
        Register::DEFINITION_END
    };
    RootTreeNode root;
    DummyDevice good_dummy(&root);
    std::unique_ptr<RegisterSet> regs;
    if (!use_json) {
        regs = RegisterSet::create(&good_dummy, good_reg_defs);
    } else {
        dumpRegisterDefnsToJSON("reg_defs.json", good_reg_defs);
        regs = RegisterSet::create(&good_dummy, "reg_defs.json");
    }
    regs->getRegister("fp_reg")->getField("sp")->write(1);
    regs->getRegister("fp_reg")->getField("dp")->write(1);

    EXPECT_EQUAL(regs->getRegister("fp_reg")->getField("sp")->read(), 1);
    EXPECT_EQUAL(regs->getRegister("fp_reg")->getField("dp")->read(), 1);

    regs->getRegister("fp_reg")->getField("sp")->write(0xffffffff);
    regs->getRegister("fp_reg")->getField("dp")->write(0xffffffffffffffff);

    EXPECT_EQUAL(regs->getRegister("fp_reg")->getField("sp")->read(), 0xffffffff);
    EXPECT_EQUAL(regs->getRegister("fp_reg")->getField("dp")->read(), 0xffffffffffffffff);

    root.enterTeardown();
}


//! Load up some good regs from a table
void testGoodRegs(bool use_json = false)
{

    Register::ident_type new_regid = 1000; // Start counting at some unique ID

    // Dummy Good Registers
    Register::Definition good_reg_defs[] = {
        {new_regid++, "dummy_long_x5",   Register::GROUP_NUM_NONE, "", Register::GROUP_IDX_NONE, "description", 64, {}, {}, 0, Register::INVALID_ID, 0, nullptr, 0,0},
        {new_regid++, "dummy_long_x4",   Register::GROUP_NUM_NONE, "", Register::GROUP_IDX_NONE, "description", 32, {}, {}, 0, Register::INVALID_ID, 0, nullptr, 0,0},
        {new_regid++, "dummy_long_x3",   Register::GROUP_NUM_NONE, "", Register::GROUP_IDX_NONE, "description", 16, {}, {}, 0, Register::INVALID_ID, 0, nullptr, 0,0},
        {new_regid++, "dummy_long_long", Register::GROUP_NUM_NONE, "", Register::GROUP_IDX_NONE, "description", 8,  {}, {}, 0, Register::INVALID_ID, 0, nullptr, 0,0},
        {new_regid++, "dummy_long",      Register::GROUP_NUM_NONE, "", Register::GROUP_IDX_NONE, "description", 4,  {}, {}, 0, Register::INVALID_ID, 0, nullptr, 0,0},
        {new_regid++, "dummy_short",     Register::GROUP_NUM_NONE, "", Register::GROUP_IDX_NONE, "description", 2,  {}, {}, 0, Register::INVALID_ID, 0, nullptr, 0,0},
        {new_regid++, "dummy_byte",      Register::GROUP_NUM_NONE, "", Register::GROUP_IDX_NONE, "description", 1,  {}, {}, 0, Register::INVALID_ID,  0, nullptr, 0,0},
        Register::DEFINITION_END
    };

    RootTreeNode root;
    DummyDevice good_dummy(&root);
    std::unique_ptr<RegisterSet> good_regs;
    if (!use_json) {
        good_regs = RegisterSet::create(&good_dummy, good_reg_defs);
    } else {
        dumpRegisterDefnsToJSON("reg_defs.json", good_reg_defs);
        good_regs = RegisterSet::create(&good_dummy, "reg_defs.json");
    }
#ifndef REGISTER_SET_GET_ARCH_DATA_REMOVED
    EXPECT_TRUE(good_regs->getArchData().isLaidOut());
    std::cout << "Layout of good dummy regs:" << std::endl;
    good_regs->getArchData().dumpLayout(std::cout);
    std::cout << std::endl;
#endif

    root.enterTeardown();
}

//! Try a series of bad register definitions
void testBadRegs()
{
    uint32_t sizes[] = {
        0,    // obviously 0-byte regs are not allowed
        3,    // non-power-of-2-count regs not allowed
        5,    // non-power-of-2-count regs not allowed
        9,    // Just to prove that odd-byte-count regs are rejected; not just primes
    };

    // Test each separately because ALL sizes must fail!
    for (uint32_t &sz : sizes) {
        // Dummy Illegal Registers
        Register::Definition bad_reg_defs[] = {
            {0, "x", Register::GROUP_NUM_NONE, "", Register::GROUP_IDX_NONE, "description", sz, {}, {}, 0, Register::INVALID_ID, 0, nullptr, 0,0},
            // Register::DEFINITION_END
        };

        RootTreeNode root;
        DummyDevice bad_dummy(&root);
        std::cout << sz << ", ";
        EXPECT_THROW(RegisterSet::create(&bad_dummy, bad_reg_defs));
        root.enterTeardown();
    }

    {
        const Register::group_num_type valid_group_num = 1;
        Register::Definition bad_1[] = {
            {0, "x", valid_group_num, Register::GROUP_NAME_NONE, 0, "description", 4, {}, {}, 0, Register::INVALID_ID, 0, nullptr, 0,0},
            Register::DEFINITION_END
        };

        RootTreeNode root;
        DummyDevice bad_dummy(&root);
        EXPECT_THROW(RegisterSet::create(&bad_dummy, bad_1));
        root.enterTeardown();
    }

    {
        Register::Definition bad_2[] = {
            {0, "x", Register::GROUP_NUM_NONE, "valid_name", 0, "description", 4, {}, {}, 0, Register::INVALID_ID, 0, nullptr, 0,0},
            Register::DEFINITION_END
        };

        RootTreeNode root;
        DummyDevice bad_dummy(&root);
        EXPECT_THROW(RegisterSet::create(&bad_dummy, bad_2));
        root.enterTeardown();
    }

    {
        Register::Definition bad_3[] = {
            {0, "x", 1, "group_name", Register::GROUP_IDX_NONE, "description", 4, {}, {}, 0, Register::INVALID_ID, 0, nullptr, 0,0},
            {0, "y", 1, "different_group_name_for_same_num", 0, "description", 4, {}, {}, 0, Register::INVALID_ID, 0, nullptr, 0,0},
            Register::DEFINITION_END
        };

        RootTreeNode root;
        DummyDevice bad_dummy(&root);
        EXPECT_THROW(RegisterSet::create(&bad_dummy, bad_3));
        root.enterTeardown();
    }

    // No group with banking info
    {
        Register::Definition bad_3[] = {
            {0, "x", 1, "group_name", Register::GROUP_IDX_NONE, "description", 4, {}, {1,2,3}, 0, Register::INVALID_ID, 0, nullptr, 0,0},
            Register::DEFINITION_END
        };

        RootTreeNode root;
        DummyDevice bad_dummy(&root);
        EXPECT_THROW(RegisterSet::create(&bad_dummy, bad_3));
        root.enterTeardown();
    }

    std::cout << std::endl;
}

#define NUM_TIMING_WRITES 100000000
template <typename WriteT, WriteT poke_val>
double timeWritesPlain(sparta::RegisterBase *r64)
{
    boost::timer::cpu_timer t;
    t.start();
    for(uint32_t i=0; i<NUM_TIMING_WRITES; ++i){
        r64->write<WriteT>(poke_val, 0);
    }
    t.stop();

    double wps = NUM_TIMING_WRITES/(t.elapsed().user/1000000000.0);
    return wps;
}

template <typename WriteT, WriteT poke_val>
double timeWritesWithNotification(sparta::RegisterBase *r64)
{
    FastRegPostWriteObserver<WriteT> rwo;
    rwo.registerFor(r64);
    double result = timeWritesPlain<WriteT, poke_val>(r64);
    rwo.deregisterFor(r64);
    EXPECT_EQUAL(rwo.writes, NUM_TIMING_WRITES);
    return result;
}

void timeWrites(sparta::RegisterBase *r64)
{
    // Time some writes
    double wps_plain = timeWritesPlain<uint64_t, 0xffffffffffffffff>(r64);
    double wps_noti = timeWritesWithNotification<uint64_t, 0xffffffffffffffff>(r64);

    // Ensure no write observers at the moment (for an accurate test)
    EXPECT_EQUAL(r64->getPostWriteNotificationSource().getNumObservers(), 0);

    std::cout << std::endl;
    std::cout << "writes per sec w/ 0 post-write observers: "
              << wps_plain << std::endl;
    std::cout << "writes per sec w/ 1 post-write delegate observer: "
              << wps_noti << std::endl;
    std::cout << "WPS delegate is " << 100.0 * (wps_noti / wps_plain) << "% of plain WPS"
              << std::endl;
    std::cout << std::endl;
}

class BankGetter {
public:

    Register::bank_idx_type bank = 0;

    /*!
     * \brief Determine the bank for the given register (proxy)
     */
    Register::bank_idx_type getBank(Register::group_num_type,
                                    Register::group_idx_type,
                                    const std::string*) {
        return bank;
    }
};

int main()
{
    // Testing a member function as a bank function callback
    {
        RootTreeNode root;
        DummyDevice dummy(&root);
        BankGetter bg_instance;
        std::unique_ptr<RegisterSet> rset(
            RegisterSet::create(&dummy,
                                reg_defs,
                                proxy_defs,
                                std::bind(&BankGetter::getBank,
                                          &bg_instance,
                                          std::placeholders::_1,
                                          std::placeholders::_2,
                                          std::placeholders::_3)));
        EXPECT_EQUAL(rset->getCurrentBank(4, 2, nullptr), 0);
        bg_instance.bank = 5;
        EXPECT_EQUAL(rset->getCurrentBank(4, 2, nullptr), 5);

        root.enterTeardown();
    }

    // Insantiation of Registers

    // Callback for register set to get the current bank
    sparta::Register::bank_idx_type cur_bank = 0;
    bool check_group_info = false;
    Register::group_num_type expected_group_num = 0;
    Register::group_idx_type expected_group_idx = 0;
    std::string expected_name = "";
    auto get_bank_fxn = [&](Register::group_num_type grp,
                            Register::group_idx_type idx,
                            const std::string* name_ptr) {
        if(check_group_info){
            EXPECT_EQUAL(grp, expected_group_num);
            EXPECT_EQUAL(idx, expected_group_idx);
            EXPECT_NOTEQUAL(name_ptr, nullptr);
            if(name_ptr){
                EXPECT_EQUAL(*name_ptr, expected_name);
            }
        }
        return cur_bank;
    };


    // Place into a tree
    RootTreeNode root;
    DummyDevice dummy(&root);
    std::unique_ptr<RegisterSet> rset(
        RegisterSet::create(&dummy, reg_defs, proxy_defs, get_bank_fxn));
    EXPECT_TRUE(rset->isAttached()); // Ensure that node constructed with parent arg is properly attached

    // Print current register set by the ostream insertion operator
    std::cout << *rset << std::endl;

    // Print current register set by iteration
    for (const auto r : rset->getRegisters()) {
        std::cout << *r << std::endl;
    }
    std::cout << std::endl;

#ifndef REGISTER_SET_GET_ARCH_DATA_REMOVED
    EXPECT_TRUE(rset->getArchData().getLineSize() >= 64); // Need at least 64 Byte lines for later tests
    EXPECT_TRUE(rset->getArchData().getLineSize() < 8192); // Expects less than 8192 Byte lines in later tests. Can be larger, but some tests in this file would need to be updated
#endif

    // Child Register lookup:
    // by name
    RegisterBase *large=0, *med=0, *notareg=0, *sprxxa=0, *sprxxb, *small=0, *huge=0;
    EXPECT_NOTHROW(large = rset->getRegister("large"));
    EXPECT_NOTEQUAL(large, nullptr); // (also tests the tester by comparing w/ nullptr on right)
    EXPECT_NOTEQUAL(nullptr, large); // (also tests the tester by comparing w/ nullptr on left)
    EXPECT_EQUAL(large->getID(), (Register::ident_type)2);
    EXPECT_NOTHROW(med = rset->getRegister("medium"));
    EXPECT_NOTEQUAL(med, (Register*)nullptr);
    EXPECT_EQUAL(med->getID(), (Register::ident_type)1);
    EXPECT_THROW(rset->getRegister("there_is_no_register_by_this_name_here_or_anywhere")); // No reg by this name here
    EXPECT_NOTHROW(sprxxa = rset->getRegister("sprXXa"));
    EXPECT_NOTEQUAL(sprxxa, (Register*)nullptr);
    EXPECT_NOTHROW(sprxxb = rset->getRegister("sprXXb"));
    EXPECT_NOTEQUAL(sprxxb, (Register*)nullptr);
    EXPECT_NOTHROW(small = rset->getRegister("small"));
    EXPECT_NOTEQUAL(small, (Register*)nullptr);
    EXPECT_NOTHROW(huge = rset->getRegister("huge"));
    EXPECT_NOTEQUAL(huge, (Register*)nullptr);

    EXPECT_EQUAL(rset->getRegister("reg1")->getGroupNum(), 1);
    EXPECT_EQUAL(rset->getRegister("medium")->getGroupNum(), 2);
    EXPECT_EQUAL(rset->getRegister("large")->getGroupNum(), 2);
    EXPECT_EQUAL(rset->getRegister("large")->getHintFlags(), HINT_READ_ONLY);
    EXPECT_EQUAL(rset->getRegister("sprXXa")->getGroupNum(), 4);
    EXPECT_EQUAL(rset->getRegister("small")->getGroupNum(), Register::GROUP_NUM_NONE);
    EXPECT_EQUAL(rset->getRegister("huge")->getGroupNum(), Register::GROUP_NUM_NONE);
    EXPECT_EQUAL(rset->getRegister("reg1")->getGroup(), "A");
    EXPECT_EQUAL(rset->getRegister("medium")->getGroup(), "B");
    EXPECT_EQUAL(rset->getRegister("large")->getGroup(), "B");
    EXPECT_EQUAL(rset->getRegister("sprXXa")->getGroup(), "D");
    EXPECT_EQUAL(rset->getRegister("small")->getGroup(), Register::GROUP_NAME_NONE);
    EXPECT_EQUAL(rset->getRegister("huge")->getGroup(), Register::GROUP_NAME_NONE);
    EXPECT_TRUE(rset->canLookupRegister(1, 0)); // reg1 is in banks {0}
    EXPECT_TRUE(rset->canLookupRegister(2, 0, 6));  // medium is in banks {6}
    EXPECT_FALSE(rset->canLookupRegister(2, 0, 0));
    EXPECT_FALSE(rset->canLookupRegister(2, 0, 3));
    EXPECT_TRUE(rset->canLookupRegister(2, 0, 4)); // medium3 is in banks {4}
    EXPECT_TRUE(rset->canLookupRegister(2, 0, 5)); // medium2 is in banks {5}
    EXPECT_FALSE(rset->canLookupRegister(2, 0, 7));
    EXPECT_TRUE(rset->canLookupRegister(2, 1)); // large is in banks {} (all)
    EXPECT_TRUE(rset->canLookupRegister(2, 1, 0));
    EXPECT_TRUE(rset->canLookupRegister(2, 1, 1));
    EXPECT_TRUE(rset->canLookupRegister(2, 1, 5));
    EXPECT_TRUE(rset->canLookupRegister(2, 1, 6));
    EXPECT_FALSE(rset->canLookupRegister(2, 1, 7));
    EXPECT_TRUE(rset->canLookupRegister(4, 2)); // sprXXa is in banks {0,1,2}
    EXPECT_TRUE(rset->canLookupRegister(4, 2, 1)); // sprXXa is in banks {0,1,2}
    EXPECT_FALSE(rset->canLookupRegister(4, 2, 3)); // sprXXa nor sprXXb isnot in bank 3
    EXPECT_TRUE(rset->canLookupRegister(4, 2, 4)); // sprXXb is in banks {4,5,6}
    EXPECT_TRUE(rset->canLookupRegister(4, 2, 5)); // sprXXb is in banks {4,5,6}
    EXPECT_FALSE(rset->canLookupRegister(4, 2, 7));
    EXPECT_FALSE(rset->canLookupRegister(1, 1));
    EXPECT_FALSE(rset->canLookupRegister(2, 2));
    EXPECT_FALSE(rset->canLookupRegister(0, 1));
    EXPECT_FALSE(rset->canLookupRegister(3, 0));
    EXPECT_FALSE(rset->canLookupRegister(3, 1));
    EXPECT_FALSE(rset->canLookupRegister(4, 0));
    EXPECT_FALSE(rset->canLookupRegister(4, 1));
    EXPECT_FALSE(rset->canLookupRegister(4, 3));
    EXPECT_EQUAL(rset->lookupRegister(1, 0), rset->getRegister("reg1"));
    EXPECT_TRUE(rset->lookupRegister(1, 0)->isBanked());
    EXPECT_TRUE(rset->lookupRegister(1, 0)->isInBank(0));
    EXPECT_FALSE(rset->lookupRegister(1, 0)->isInBank(1));
    EXPECT_FALSE(rset->lookupRegister(1, 0)->isInBank(6));
    EXPECT_EQUAL(rset->lookupRegister(2, 0), nullptr);
    EXPECT_EQUAL(rset->lookupRegister(2, 0, 6), rset->getRegister("medium"));
    EXPECT_EQUAL(rset->lookupRegister(2, 0, 5), rset->getRegister("medium2"));
    EXPECT_EQUAL(rset->lookupRegister(2, 0, 4), rset->getRegister("medium3"));;
    EXPECT_EQUAL(rset->lookupRegister(2, 1), rset->getRegister("large"));
    EXPECT_FALSE(rset->lookupRegister(2, 1)->isBanked());
    EXPECT_TRUE(rset->lookupRegister(2, 1)->isInBank(0));
    EXPECT_TRUE(rset->lookupRegister(2, 1)->isInBank(6));
    EXPECT_TRUE(rset->lookupRegister(2, 1)->isInBank(7)); // unbanked reg is in all bank
    EXPECT_EQUAL(rset->lookupRegister(4, 2), rset->getRegister("sprXXa"));
    EXPECT_EQUAL(rset->lookupRegister(4, 2, 2), rset->getRegister("sprXXa"));
    EXPECT_EQUAL(rset->lookupRegister(4, 2, 4), rset->getRegister("sprXXb"));
    EXPECT_EQUAL(rset->lookupRegister(4, 2, 6), rset->getRegister("sprXXb"));
    EXPECT_TRUE(rset->lookupRegister(4, 2)->isBanked());
    EXPECT_TRUE(rset->lookupRegister(4, 2)->isInBank(0));
    EXPECT_TRUE(rset->lookupRegister(4, 2)->isInBank(1));
    EXPECT_FALSE(rset->lookupRegister(4, 2)->isInBank(3));
    EXPECT_NOTHROW(rset->getRegister(1, 0));
    EXPECT_THROW(rset->getRegister(2, 0));
    EXPECT_NOTHROW(rset->getRegister(2, 0, 6));
    EXPECT_NOTHROW(rset->getRegister(2, 1));
    EXPECT_NOTHROW(rset->getRegister(4, 2));
    EXPECT_THROW(rset->getRegister(1, 1)); // Does not exist
    EXPECT_THROW(rset->getRegister(2, 2)); // Does not exist
    EXPECT_THROW(rset->getRegister(0, 1)); // Does not exist
    EXPECT_THROW(rset->getRegister(3, 0)); // Does not exist
    EXPECT_THROW(rset->getRegister(3, 1)); // Does not exist
    EXPECT_THROW(rset->getRegister(4, 0)); // Does not exist
    EXPECT_THROW(rset->getRegister(4, 1)); // Does not exist
    EXPECT_THROW(rset->getRegister(4, 3)); // Does not exist
    EXPECT_EQUAL(rset->getGroupSize(0), 0);
    EXPECT_EQUAL(rset->getGroupSize(1), 1);
    EXPECT_EQUAL(rset->getGroupSize(2), 1);
    EXPECT_EQUAL(rset->getGroupSize(3), 0);
    EXPECT_EQUAL(rset->getGroupSize(4), 1);
    EXPECT_EQUAL(rset->getGroupSize(5), 0);

    std::cout << "Maskings" << std::endl;
    std::cout << "wm_01 write-mask: " << rset->getRegister("wm_01")->getWriteMaskAsByteString() << std::endl;
    std::cout << "wm_01 write-mask: " << rset->getRegister("wm_01")->getWriteMaskAsBitString() << std::endl;
    EXPECT_EQUAL(rset->getRegister("wm_01")->getWriteMask<uint16_t>(1), 0xffff);
    EXPECT_EQUAL(rset->getRegister("wm_01")->getWriteMask<uint16_t>(0), 0xfc10); // masked b03_00 b09_05
    EXPECT_EQUAL(rset->getRegister("wm_01")->getWriteMask<uint32_t>(), 0xfffffc10);
    EXPECT_THROW(rset->getRegister("wm_01")->getWriteMask<uint64_t>());
    EXPECT_THROW(rset->getRegister("wm_01")->getWriteMask<uint32_t>(1));
    EXPECT_THROW(rset->getRegister("wm_01")->getWriteMask<uint16_t>(3));
    EXPECT_THROW(rset->getRegister("wm_01")->getWriteMask<uint16_t>(65535));
    EXPECT_TRUE(rset->getRegister("wm_01")->getField("b03_00")->isReadOnly());
    EXPECT_TRUE(rset->getRegister("wm_01")->getField("b09_05")->isReadOnly());
    std::cout << "wm_02 write-mask: " << rset->getRegister("wm_02")->getWriteMaskAsByteString() << std::endl;
    std::cout << "wm_02 write-mask: " << rset->getRegister("wm_02")->getWriteMaskAsBitString() << std::endl;
    EXPECT_FALSE(rset->getRegister("wm_01")->getField("b15_12")->isReadOnly());
    EXPECT_EQUAL(rset->getRegister("wm_02")->getWriteMask<uint64_t>(), 0x0000000000000000);
    std::cout << "wm_03 write-mask: " << rset->getRegister("wm_03")->getWriteMaskAsByteString() << std::endl;
    std::cout << "wm_03 write-mask: " << rset->getRegister("wm_03")->getWriteMaskAsBitString() << std::endl;
    EXPECT_EQUAL(rset->getRegister("wm_03")->getWriteMask<uint64_t>(1), 0xffffffffffffffff);
    EXPECT_EQUAL(rset->getRegister("wm_03")->getWriteMask<uint64_t>(), 0xffffffffffffffff);
    std::cout << "wm_04 write-mask: " << rset->getRegister("wm_04")->getWriteMaskAsByteString() << std::endl;
    EXPECT_EQUAL(rset->getRegister("wm_04")->getWriteMaskAsByteString(),
                 "ff ff ff ff ff ff ff fc 00 00 00 00 00 00 1f ff");
    std::cout << "wm_04 write-mask: " << rset->getRegister("wm_04")->getWriteMaskAsBitString() << std::endl;
    EXPECT_EQUAL(rset->getRegister("wm_04")->getWriteMaskAsBitString(),
                 "11111111 11111111 11111111 11111111 11111111 11111111 11111111 11111100 00000000 00000000 00000000 00000000 00000000 00000000 00011111 11111111");
    EXPECT_EQUAL(rset->getRegister("wm_04")->getWriteMask<uint64_t>(1), 0xfffffffffffffffc);
    EXPECT_EQUAL(rset->getRegister("wm_04")->getWriteMask<uint64_t>(), 0x0000000000001fff);

    // Test bank construction
    EXPECT_EQUAL(rset->getNumBanks(), 7);
    EXPECT_TRUE(rset->canLookupRegister(2, 0, 6)); // "medium"

    rset->dumpBanks(std::cout);


    // Register printing by pointer
    std::cout << "Registers: "
              << large << " "
              << med << " "
              << notareg << std::endl;

    // Register printing by value/reference
    if(large){
        std::cout << *large << std::endl;
    }
    if(med){
        std::cout << *med << std::endl;
    }

    // Printing
    //
    //! \todo register printing by group
    // TODO
    // by group + index
    // TODO
    // by name expression
    // TODO

    // Procedural addition of fields to register
    // Create good fields
    Register::Field *starting=0, *almost_ending=0, *ending=0, *spanning=0,
                    *notafield=0, *span_large=0, *span_largest=0;
    //EXPECT_NOTHROW(full = large->addField(Register::Field::Definition{"full_field", "description for this field", 0, 127}));
    Register::Field::Definition f_starting{"starting_field", "description for this field", 0, 0};
    EXPECT_NOTHROW(starting = large->addField(f_starting));
    Register::Field::Definition f_ending{"ending_field", "description for this field", 127, 127};
    EXPECT_NOTHROW(ending = large->addField(f_ending));
    Register::Field::Definition f_almost_ending{"almost_ending_field", "description for this field", 124, 125};
    EXPECT_NOTHROW(almost_ending = large->addField(f_almost_ending));
    Register::Field::Definition f_spanning{"spanning_field", "description for this field", 63, 64};
    EXPECT_NOTHROW(spanning = large->addField(f_spanning)); // Spans 64b boundary. This is an edge case for field reading/writing
    Register::Field::Definition f_span_large{"spanning_field_large", "description for this field", 60, 75};
    EXPECT_NOTHROW(span_large = large->addField(f_span_large)); // Spans 64b with more bits boundary. This is an edge case for field reading/writing
    Register::Field::Definition f_span_largest{"spanning_field_largest", "description for this field", 33, 90};
    EXPECT_NOTHROW(span_largest = large->addField(f_span_largest)); // Spans 64b with more bits boundary and does not start or end on a nibble. This is an edge case for field reading/writing

    // Field printing by pointer
    std::cout << "Fields: "
              << starting << " "
              << ending << " "
              << spanning << " "
              << notafield << std::endl;

    // Field printing by value/reference
    if(starting){
        std::cout << *starting << std::endl;
    }
    if(ending){
        std::cout << *ending << std::endl;
    }
    if(spanning){
        std::cout << *spanning << std::endl;
    }


    // Create illegal fields
    EXPECT_EQUAL(large->getNumBits(), (uint32_t)128);
    Register::Field::Definition f_illegal{"illegal_generated_field_1", "description for this field", 0, 128};
    EXPECT_THROW(large->addField(f_illegal)); // Field larger than register

    // Field lookup
    EXPECT_NOTEQUAL(large->getField("starting_field"), (Register::Field*)nullptr);
    EXPECT_NOTEQUAL(large->getField("ending_field"), (Register::Field*)nullptr);
    EXPECT_NOTEQUAL(large->getField("spanning_field"), (Register::Field*)nullptr);
    EXPECT_THROW(large->getField("field_name_that_shouldnt_exist"));

    // Procedural addition of aliases to register (NOT ALLOWED)
    EXPECT_TRUE(large->getParent());
    EXPECT_THROW(large->addAlias("alias_name_that_shouldnt_exist")); // Already has a parent node; Cannot add aliases

    EXPECT_EQUAL(root.getPhase(), sparta::TreeNode::TreePhase::TREE_BUILDING);
    EXPECT_FALSE(root.isBuilt()); // Make sure we didn't accidentally move away from the TREE_BUILDING phase
    EXPECT_FALSE(root.isConfigured());
    EXPECT_FALSE(root.isFinalizing());
    EXPECT_FALSE(root.isFinalized());
    root.enterConfiguring();

    EXPECT_EQUAL(root.getPhase(), sparta::TreeNode::TreePhase::TREE_CONFIGURING);
    EXPECT_TRUE(root.isBuilt());
    EXPECT_FALSE(root.isConfigured());
    EXPECT_FALSE(root.isFinalizing());
    EXPECT_FALSE(root.isFinalized());
    std::cout << root.renderSubtree(-1, true) << std::endl;
    root.enterFinalized();

    EXPECT_EQUAL(root.getPhase(), sparta::TreeNode::TreePhase::TREE_FINALIZED);
    EXPECT_TRUE(root.isBuilt());
    EXPECT_TRUE(root.isConfigured());
    EXPECT_FALSE(root.isFinalizing());
    EXPECT_TRUE(root.isFinalized());

    root.bindTreeEarly();
    root.bindTreeLate();

    // Construct some good and bad regs to test out size constraints
    testFieldRegisterWrite();      // Create registers directly
    testFieldRegisterWrite(true);  // Create registers from JSON
    testGoodRegs();                // Create registers directly
    testGoodRegs(true);            // Create registers from JSON
    testBadRegs();


    // Register I/O

    // Check the Notifications on the Registers
    sparta::NotificationSourceBase* b;
    b = &med->getPostWriteNotificationSource();
    EXPECT_EQUAL(b->getNotificationID(), sparta::StringManager::getStringManager().internString("post_write"));
    EXPECT_EQUAL(b->getNotificationName(), "post_write");
    EXPECT_TRUE(b->getNotificationType() == typeid(sparta::Register::PostWriteAccess));
    EXPECT_EQUAL(b->getNotificationTypeName(), "sparta::RegisterBase::PostWriteAccess");

    b = &med->getReadNotificationSource();
    EXPECT_EQUAL(b->getNotificationID(), sparta::StringManager::getStringManager().internString("post_read"));
    EXPECT_EQUAL(b->getNotificationName(), "post_read");
    EXPECT_TRUE(b->getNotificationType() == typeid(sparta::Register::ReadAccess));
    EXPECT_EQUAL(b->getNotificationTypeName(), "sparta::RegisterBase::ReadAccess");

    std::cout << "Possible Notifications for register 'med':" << std::endl;
    med->dumpPossibleNotifications(std::cout);
    std::cout << "Possible Notifications for " << *rset << std::endl;
    rset->dumpPossibleNotifications(std::cout);

    std::vector<sparta::TreeNode::NotificationInfo> infos;
    EXPECT_EQUAL(med->getPossibleSubtreeNotifications(infos), 2); // post_write and post_read notifications per register
    EXPECT_EQUAL(infos.size(), 2);
    EXPECT_EQUAL(rset->getPossibleSubtreeNotifications(infos), 2*rset->getNumRegisters());
    EXPECT_EQUAL(infos.size(), 2 + 2*rset->getNumRegisters()); // 2 notis per register. 2 already in infos.

    std::cout << "NotificationSources for register 'med':" << std::endl;
    med->dumpLocatedNotificationSources(std::cout);
    std::cout << "NotificationSources for " << *rset << std::endl;
    rset->dumpLocatedNotificationSources(std::cout);

    std::vector<sparta::TreeNode*> srcs;
    srcs.clear();
    EXPECT_EQUAL(med->locateNotificationSources<sparta::Register::PostWriteAccess>(srcs), 1);
    EXPECT_EQUAL(srcs.size(), 1);
    EXPECT_EQUAL(rset->locateNotificationSources<sparta::Register::PostWriteAccess>(srcs),
                 rset->getNumRegisters()); // 1 notis per register. 1 already in infos
    EXPECT_EQUAL(srcs.size(), 1 + rset->getNumRegisters());

    srcs.clear();
    EXPECT_EQUAL(med->locateNotificationSources<sparta::Register::ReadAccess>(srcs), 1);
    EXPECT_EQUAL(srcs.size(), 1);
    EXPECT_EQUAL(rset->locateNotificationSources<sparta::Register::ReadAccess>(srcs),
                 rset->getNumRegisters()); // 1 notis per register. 1 already in infos
    EXPECT_EQUAL(srcs.size(), 1 + rset->getNumRegisters());

    srcs.clear();
    EXPECT_EQUAL(med->locateNotificationSources(srcs), 2);
    EXPECT_EQUAL(srcs.size(), 2);
    EXPECT_EQUAL(rset->locateNotificationSources(srcs),
                 2*rset->getNumRegisters()); // 2 notis per register. 2 already in infos
    EXPECT_EQUAL(srcs.size(), 2 + 2*rset->getNumRegisters());

    srcs.clear();
    EXPECT_EQUAL((med->locateNotificationSources(srcs, "post_write")), 1);
    EXPECT_EQUAL(srcs.size(), 1);
    EXPECT_EQUAL((rset->locateNotificationSources(srcs, "post_write")),
                 rset->getNumRegisters()); // 1 notis per register. 1 already in infos
    EXPECT_EQUAL(srcs.size(), 1 + rset->getNumRegisters());

    srcs.clear();
    EXPECT_EQUAL((med->locateNotificationSources(srcs, "post_read")), 1);
    EXPECT_EQUAL(srcs.size(), 1);
    EXPECT_EQUAL((rset->locateNotificationSources(srcs, "post_read")),
                 rset->getNumRegisters()); // 1 notis per register. 1 already in infos
    EXPECT_EQUAL(srcs.size(), 1 + rset->getNumRegisters());

    srcs.clear();
    EXPECT_EQUAL((med->locateNotificationSources(srcs, "not_a_notification_name")), 0);
    EXPECT_EQUAL(srcs.size(), 0);
    EXPECT_EQUAL((rset->locateNotificationSources(srcs, "not_a_notification_name")),
                 0);
    EXPECT_EQUAL(srcs.size(), 0);

    // Callback dummy instantiation of type which does not occur at this node
    CallbackDummy<uint32_t> cbc;
    static_assert(std::is_same<decltype(cbc), CallbackDummy<uint32_t>>::value, "same?");
    //EXPECT_THROW((rset->registerForNotification<uint32_t, decltype(cbc),           &(decltype(cbc)::callback)>(&cbc, "", true)));
    //EXPECT_THROW((rset->registerForNotification<uint32_t, decltype(cbc), &(CallbackDummy<uint32_t>::callback)>(&cbc, "a_notification", true)));
    //EXPECT_THROW((rset->registerForNotification<uint32_t, decltype(cbc), &(CallbackDummy<uint32_t>)::callback>(&cbc, "a_notification", true)));
    EXPECT_THROW((rset->registerForNotification<uint32_t, decltype(cbc), &CallbackDummy<uint32_t>::callback1>(&cbc, "a_notification", true)));

    CallbackDummy<sparta::Register::PostWriteAccess> write_cb;
    EXPECT_NOTHROW((rset->registerForNotification<sparta::Register::PostWriteAccess, decltype(write_cb), &CallbackDummy<sparta::Register::PostWriteAccess>::callback1>(&write_cb, "")));
    EXPECT_NOTHROW((rset->deregisterForNotification<sparta::Register::PostWriteAccess, decltype(write_cb), &CallbackDummy<sparta::Register::PostWriteAccess>::callback1>(&write_cb, "")));
    EXPECT_THROW((rset->registerForNotification<sparta::Register::PostWriteAccess, decltype(write_cb), &CallbackDummy<sparta::Register::PostWriteAccess>::callback1>(&write_cb, "not_a_notification")));

    EXPECT_NOTHROW((rset->registerForNotification<sparta::Register::PostWriteAccess, decltype(write_cb), &CallbackDummy<sparta::Register::PostWriteAccess>::callback2>(&write_cb, "")));
    EXPECT_NOTHROW((rset->deregisterForNotification<sparta::Register::PostWriteAccess, decltype(write_cb), &CallbackDummy<sparta::Register::PostWriteAccess>::callback2>(&write_cb, "")));
    EXPECT_THROW((rset->registerForNotification<sparta::Register::PostWriteAccess, decltype(write_cb), &CallbackDummy<sparta::Register::PostWriteAccess>::callback2>(&write_cb, "not_a_notification")));


    // Put observers on some registers read and write
    RegPostWriteObserver<uint64_t> rwo;
    EXPECT_EQUAL(med->getPostWriteNotificationSource().getNumObservers(), 0);
    rwo.registerForCb1(med);
    EXPECT_EQUAL(med->getPostWriteNotificationSource().getNumObservers(), 1);
    rwo.registerForCb2(med);
    EXPECT_EQUAL(med->getPostWriteNotificationSource().getNumObservers(), 2);
    RegReadObserver<uint64_t> rro;
    rro.registerFor(med);

    // Test default value without reset
    std::cout << "\nWriting 1 byte to " << med << std::endl;
    rwo.expect(0xabacadabab0220cc, 0xabacadabab0220ff);
    EXPECT_NOTHROW((med->write<uint8_t>(0xff)));

    // Test default value with reset
    med->reset();
    std::cout << "\nWriting 1 byte to " << med << std::endl;
    rwo.expect(0xabacadabab0220cc, 0xabacadabab0220ff);
    EXPECT_NOTHROW((med->write<uint8_t>(0xff)));

    // Simple writes and reads
    // Registers
    std::cout << "\nWriting to " << med << std::endl;
    EXPECT_EQUAL(med->getNumBits(), (Register::size_type)64);
    rwo.expect(med->peek<uint64_t>(), 0xffffffffffffffff);
    EXPECT_NOTHROW((med->write<uint64_t>(0xffffffffffffffff)));
    std::cout << " now: " << med << std::endl;
    rwo.expect(0xffffffffffffffff, 0xeeeeeeeeffffffff);
    EXPECT_NOTHROW((med->write<uint32_t>(0xeeeeeeee, 1))); // 1-0 (MSB)
    std::cout << " now: " << med << std::endl;
    rwo.expect(0xeeeeeeeeffffffff, 0xddddeeeeffffffff);
    EXPECT_NOTHROW((med->write<uint16_t>(0xdddd, 3))); // 3-0 (MSB)
    std::cout << " now: " << med << std::endl;

    rwo.deregisterForCb1(med);
    EXPECT_EQUAL(med->getPostWriteNotificationSource().getNumObservers(), 1);
    rwo.expect(0xddddeeeeffffffff, 0xddddeeeeddddffff);
    EXPECT_NOTHROW((med->write<uint16_t>(0xdddd, 1))); // 3-2
    std::cout << " now: " << med << std::endl;

    rwo.deregisterForCb2(med);
    EXPECT_EQUAL(med->getPostWriteNotificationSource().getNumObservers(), 0);
    rwo.expect(0xddddeeeeddddffff, 0xccddeeeeddddffff);
    EXPECT_NOTHROW((med->write<uint8_t>(0xcc, 7))); // 7-0 (MSB)
    std::cout << " now: " << med << std::endl;

    rwo.registerForCb2(med);
    EXPECT_EQUAL(med->getPostWriteNotificationSource().getNumObservers(), 1);
    rwo.expect(0xccddeeeeddddffff, 0xccddeeeeccddffff);
    EXPECT_NOTHROW((med->write<uint8_t>(0xcc, 3))); // 7-4
    std::cout << " now: " << med << std::endl;

    rro.expect(0xccddeeeeccddffff);
    std::cout << " have: " << std::hex << med->read<uint64_t>() << " expect: " << 0xccddeeeeccddffff << std::endl;
    EXPECT_EQUAL((med->read<uint64_t>()), 0xccddeeeeccddffff);
    std::cout << "Medium Register: " << std::endl << med->renderSubtree(-1, true);

    rwo.deregisterForCb2(med);
    EXPECT_EQUAL(med->getPostWriteNotificationSource().getNumObservers(), 0);
    rro.deregisterFor(med);

    EXPECT_EQUAL(rwo.writes_1, 5);
    EXPECT_EQUAL(rwo.writes_2, 7);
    EXPECT_EQUAL(rro.reads, 2);

    // Test large register (128b)
    std::cout << "\nWriting to " << large << std::endl;
    EXPECT_EQUAL(large->getNumBits(), (Register::size_type)128);
    EXPECT_NOTHROW((large->write<uint64_t>(0xffffffffffffffff)));
    std::cout << " now: " << large << std::endl;
    EXPECT_NOTHROW((large->write<uint32_t>(0xeeeeeeee, 1))); // 1-0 (MSB)
    std::cout << " now: " << large << std::endl;
    EXPECT_NOTHROW((large->write<uint16_t>(0xdddd, 3))); // 3-0 (MSB)
    std::cout << " now: " << large << std::endl;
    EXPECT_NOTHROW((large->write<uint16_t>(0xdddd, 1))); // 3-2
    std::cout << " now: " << large << std::endl;
    EXPECT_NOTHROW((large->write<uint8_t>(0xcc, 7))); // 7-0 (MSB)
    std::cout << " now: " << large << std::endl;
    EXPECT_NOTHROW((large->write<uint8_t>(0xcc, 3))); // 7-4
    std::cout << " now: " << large << std::endl;
    std::cout << " have: " << std::hex << large->read<uint64_t>() << " expect: " << 0xccddeeeeccddffff << std::endl;
    EXPECT_EQUAL((large->read<uint64_t>()), 0xccddeeeeccddffff);
    std::cout << "Large Register: " << std::endl << large->renderSubtree(-1, true);

    // Test huge register (256b)
    std::cout << "\nWriting to " << huge << std::endl;
    EXPECT_EQUAL(huge->getNumBits(), (Register::size_type)256);
    EXPECT_NOTHROW((huge->write<uint64_t>(0xffffffffffffffff)));
    EXPECT_EQUAL(huge->read<uint64_t>(), 0xffffffffffffffff);
    EXPECT_NOTHROW((huge->write<uint32_t>(0xeeeeeeee, 1))); // 1-0 (MSB)
    EXPECT_EQUAL(huge->read<uint32_t>(1), 0xeeeeeeee);
    EXPECT_NOTHROW((huge->write<uint16_t>(0xdddd, 3))); // 3-0 (MSB)
    EXPECT_EQUAL(huge->read<uint16_t>(3), 0xdddd);
    EXPECT_NOTHROW((huge->write<uint16_t>(0xdddd, 1))); // 3-2
    EXPECT_EQUAL(huge->read<uint16_t>(1), 0xdddd);
    EXPECT_NOTHROW((huge->write<uint8_t>(0xcc, 7))); // 7-0 (MSB)
    EXPECT_EQUAL(huge->read<uint8_t>(7), 0xcc);
    EXPECT_NOTHROW((huge->write<uint8_t>(0xcc, 3))); // 7-4
    EXPECT_EQUAL(huge->read<uint8_t>(3), 0xcc);
    std::cout << " have: " << std::hex << huge->read<uint64_t>() << " expect: " << 0xccddeeeeccddffff << std::endl;
    EXPECT_EQUAL((huge->read<uint64_t>()), 0xccddeeeeccddffff);
    std::cout << "Huge Register: " << std::endl << huge->renderSubtree(-1, true);

    // Test notifications on fields
    auto sprXXa = rset->getRegister("sprXXa");
    RegPostWriteObserver<uint32_t> rwo2;
    rwo2.registerForCb1(sprXXa);
    RegReadObserver<uint32_t> rro2;
    rro2.registerFor(sprXXa);

    uint32_t mask = ( ((1 << 20) - 1) & ~((1 << 12) - 1) );
    rwo2.expect(sprXXa->peek<uint32_t>(), (sprXXa->peek<uint32_t>() & ~mask) | (5 << 12));
    sprXXa->getField("b19_12")->write(5);
    sprXXa->getField("b19_12")->poke(6);
    sprXXa->getField("b19_12")->poke(6);
    rro2.expect(sprXXa->peek<uint32_t>());
    EXPECT_EQUAL(sprXXa->getField("b19_12")->read(), 6);
    EXPECT_EQUAL(sprXXa->getField("b19_12")->peek(), 6);
    EXPECT_EQUAL(sprXXa->getField("b19_12")->peek(), 6);

    EXPECT_EQUAL(rwo2.writes_1, 1);
    EXPECT_EQUAL(rro2.reads, 1);
    rwo2.deregisterForCb1(sprXXa);
    rro2.deregisterFor(sprXXa);

    // Test byte/value printing
    std::cout << large->getValueAsByteString() << std::endl;
    std::cout << large->getValueAsByteString() << std::endl;

    // Accessing subset registers
    std::cout << "Subset registers:" << std::endl;
#ifndef REGISTER_SET_GET_ARCH_DATA_REMOVED
    rset->getArchData().dumpLayout(std::cout);
#endif
    uint64_t large_val[2] = {large->read<uint64_t>(0), large->read<uint64_t>(1)}; // C++11 is convenient
    EXPECT_EQUAL((rset->getRegister("large_x0")->read<uint32_t>()), (uint32_t)((large_val[0] & 0xffffffff)));
    EXPECT_EQUAL((rset->getRegister("large_x4")->read<uint32_t>()), (uint32_t)((large_val[0] & 0xffffffff00000000) >> 32));
    EXPECT_EQUAL((rset->getRegister("large_x8")->read<uint32_t>()), (uint32_t)((large_val[1] & 0xffffffff)));
    EXPECT_EQUAL((rset->getRegister("large_xC")->read<uint32_t>()), (uint32_t)((large_val[1] & 0xffffffff00000000) >> 32));
    EXPECT_EQUAL((rset->getRegister("large_x3")->read<uint32_t>()), (uint32_t)((large_val[0] & 0xffffffff000000) >> 24));
    uint32_t read_large_x6 = (rset->getRegister("large_x6")->read<uint32_t>());
    uint32_t expected_large_x6 = (uint32_t)( ((large_val[0] & 0xffff000000000000) >> (6*8))
                                             | ((large_val[1] & 0xffff) << (2*8)));
    std::cout << std::hex << read_large_x6 << " wanted: " << expected_large_x6 << " ("
              << rset->getRegister("large_x6")->getNumBytes() << " B)" <<  std::endl;
    EXPECT_EQUAL(read_large_x6, expected_large_x6);

    // Fields at start of this 128b reg, end of reg, and spanning the 64bit boundary in the middle.
    EXPECT_EQUAL(starting->read(), (uint64_t)0b1);
    EXPECT_EQUAL(ending->read(), (uint64_t)0b1);
    EXPECT_EQUAL(almost_ending->read(), (uint64_t)0b00);
    EXPECT_EQUAL(spanning->read(), (uint64_t)0b01);

    // Larger fields spanning the 64b boundary
    large->write<uint64_t>(0x0123456789abcdef, 1); // Write at offset of 8bytes.
    std::cout << "Large Register: " << std::endl << large->renderSubtree(-1, true);
    EXPECT_EQUAL(span_large->read(), (uint64_t)0xdefc); // 0xdefc
    EXPECT_EQUAL(span_largest->read(), (uint64_t)0xd5e6f7e66ef777); // 0xd5e6f7e66ef777

    // Use an example SPR accessing some fields
    sprxxa->write<uint32_t>(0xbbccddee); // LE mem = [+0:ee, +1:dd, +2:cc, +3:bb]
    // (reading fields)
    std::cout << std::hex << sprxxa->read<uint32_t>() << std::dec << std::endl;
    EXPECT_EQUAL(sprxxa->getField("b07_00")->read(), (uint64_t)0xee);
    std::cout << std::hex << sprxxa->getField("b07_00")->read() << std::dec << std::endl; // ee
    EXPECT_EQUAL(sprxxa->getField("b15_08")->read(), (uint64_t)0xdd);
    std::cout << std::hex << sprxxa->getField("b15_08")->read() << std::dec << std::endl; // dd
    EXPECT_EQUAL(sprxxa->getField("b19_12")->read(), (uint64_t)0xcd);
    std::cout << std::hex << sprxxa->getField("b19_12")->read() << std::dec << std::endl; // cd
    EXPECT_EQUAL(sprxxa->getField("b27_03")->read(), (uint64_t)0x1799bbd);
    std::cout << std::hex << sprxxa->getField("b27_03")->read() << std::dec << std::endl; // 1799bbd
    EXPECT_EQUAL(sprxxa->getField("b23_16")->read(), (uint64_t)0xcc);
    std::cout << std::hex << sprxxa->getField("b23_16")->read() << std::dec << std::endl; // cc
    EXPECT_EQUAL(sprxxa->getField("b31_24")->read(), (uint64_t)0xbb);
    std::cout << std::hex << sprxxa->getField("b31_24")->read() << std::dec << std::endl; // bb

    // (writing fields in a 32b reg)
    sprxxa->write<uint32_t>(0xffffffff);
    sprxxa->getField("b07_00")->write(0xef);
    std::cout << "sprxxa: " << sprxxa << std::endl;
    sprxxa->getField("b15_08")->write(0xbe);
    std::cout << "sprxxa: " << sprxxa << std::endl;
    sprxxa->getField("b23_16")->write(0xad);
    std::cout << "sprxxa: " << sprxxa << std::endl;
    sprxxa->getField("b31_24")->write(0xde);
    std::cout << "sprxxa: " << sprxxa << std::endl;
    sprxxb->write<uint32_t>(0x00c0ffee);
    std::cout << "sprxxa: " << sprxxa << std::endl;
    EXPECT_EQUAL((sprxxa->read<uint32_t>()), (uint32_t)0xdeadbeef);
    EXPECT_EQUAL((sprxxb->read<uint32_t>()), (uint32_t)0x00c0ffee);

    // (writing fields in a 128b reg)
    large->write<uint64_t>(0xfafafafafafafafa);
    large->write<uint64_t>(0xfafafafafafafafa, 1);
    large->getField("b15_00")->write(0xbeef);
    std::cout << "large: " << large << std::endl;
    EXPECT_EQUAL((large->read<uint64_t>(0)), 0xfafafafafafabeef_u64);
    large->getField("b31_16")->write(0xdead);
    std::cout << "large: " << large << std::endl;
    EXPECT_EQUAL((large->read<uint64_t>(0)), 0xfafafafadeadbeef_u64);
    large->getField("b47_32")->write(0xffee);
    std::cout << "large: " << large << std::endl;
    EXPECT_EQUAL((large->read<uint64_t>(0)), 0xfafaffeedeadbeef_u64);
    large->getField("b63_48")->write(0x00c0);
    std::cout << "large: " << large << std::endl;
    EXPECT_EQUAL((large->read<uint64_t>(0)), 0x00c0ffeedeadbeef_u64);

    large->getField("b79_64")->write(0xc0de);
    std::cout << "large: " << large << std::endl;
    EXPECT_EQUAL((large->read<uint64_t>(1)), 0xfafafafafafac0de_u64);
    large->getField("b95_80")->write(0xc001);
    std::cout << "large: " << large << std::endl;
    EXPECT_EQUAL((large->read<uint64_t>(1)), 0xfafafafac001c0de_u64);
    large->getField("b111_96")->write(0xba1e);
    std::cout << "large: " << large << std::endl;
    EXPECT_EQUAL((large->read<uint64_t>(1)), 0xfafaba1ec001c0de_u64);
    large->getField("b127_112")->write(0xcab5);
    std::cout << "large: " << large << std::endl;
    EXPECT_EQUAL((large->read<uint64_t>(1)), 0xcab5ba1ec001c0de_u64);

    // (write field spanning 2 64b items in a register)
    large->getField("middle")->write(0x50da);
    std::cout << "large: " << large << std::endl;
    EXPECT_EQUAL((large->read<uint64_t>(0)), 0xdac0ffeedeadbeef_u64);
    EXPECT_EQUAL((large->read<uint64_t>(1)), 0xcab5ba1ec001c050_u64);


    // Write to masked registers
    EXPECT_EQUAL(rset->getRegister("wm_01")->read<uint32_t>(), 0xaaaaaaaa);
    EXPECT_NOTHROW(rset->getRegister("wm_01")->write<uint32_t>(0xffffffff));
    EXPECT_EQUAL(rset->getRegister("wm_01")->read<uint32_t>(), 0xfffffeba);

    EXPECT_EQUAL(rset->getRegister("wm_02")->read<uint64_t>(), 0xaaaaaaaaaaaaaaaa);
    EXPECT_NOTHROW(rset->getRegister("wm_02")->write<uint64_t>(0xffffffffffffffff));
    EXPECT_EQUAL(rset->getRegister("wm_02")->read<uint64_t>(), 0xaaaaaaaaaaaaaaaa);

    EXPECT_EQUAL(rset->getRegister("wm_03")->read<uint64_t>(), 0xaaaaaaaaaaaaaaaa);
    EXPECT_EQUAL(rset->getRegister("wm_03")->read<uint64_t>(1), 0xaaaaaaaaaaaaaaaa);
    EXPECT_NOTHROW(rset->getRegister("wm_03")->write<uint64_t>(0xffffffffffffffff, 1));
    EXPECT_NOTHROW(rset->getRegister("wm_03")->write<uint64_t>(0xffffffffffffffff));
    EXPECT_EQUAL(rset->getRegister("wm_03")->read<uint64_t>(1), 0xffffffffffffffff);
    EXPECT_EQUAL(rset->getRegister("wm_03")->read<uint64_t>(0), 0xffffffffffffffff);

    std::cout << " wm_04 val " << rset->getRegister("wm_04")->getValueAsByteString() << std::endl;
    std::cout << " wm_04 wm  " << rset->getRegister("wm_04")->getWriteMaskAsByteString() << std::endl;
    EXPECT_EQUAL(rset->getRegister("wm_04")->read<uint64_t>(), 0xaaaaaaaaaaaaaaaa);
    EXPECT_EQUAL(rset->getRegister("wm_04")->read<uint64_t>(1), 0xaaaaaaaaaaaaaaaa);
    EXPECT_NOTHROW(rset->getRegister("wm_04")->write<uint64_t>(0xffffffffffffffff, 1));
    std::cout << " wm_04 = " << rset->getRegister("wm_04") << std::endl;
    EXPECT_NOTHROW(rset->getRegister("wm_04")->write<uint64_t>(0xffffffffffffffff));
    std::cout << " wm_04 = " << rset->getRegister("wm_04") << std::endl;
    EXPECT_EQUAL(rset->getRegister("wm_04")->read<uint64_t>(1), 0xfffffffffffffffe);
    EXPECT_EQUAL(rset->getRegister("wm_04")->read<uint64_t>(0), 0xaaaaaaaaaaaabfff);
    EXPECT_NOTHROW(rset->getRegister("wm_04")->getField("b65_13")->write(0x1fffffff)); // Read-only
    std::cout << " wm_04 = " << rset->getRegister("wm_04") << std::endl;
    EXPECT_EQUAL(rset->getRegister("wm_04")->read<uint64_t>(1), 0xfffffffffffffffe);
    EXPECT_EQUAL(rset->getRegister("wm_04")->read<uint64_t>(0), 0xaaaaaaaaaaaabfff);

    EXPECT_NOTHROW(rset->getRegister("wm_04")->getField("b67_64")->write(0x0)); // Read-only
    std::cout << " wm_04 = " << rset->getRegister("wm_04") << std::endl;
    EXPECT_EQUAL(rset->getRegister("wm_04")->read<uint64_t>(1), 0xfffffffffffffff2);
    EXPECT_EQUAL(rset->getRegister("wm_04")->read<uint64_t>(0), 0xaaaaaaaaaaaabfff);


    // Illegal reads and writes
    // (regs)
    const uint32_t max_index = ~(uint32_t)0;
    std::cout << "\nIllegal access tests: max_index: 0x" << std::hex
              << max_index << std::dec << std::endl;
    EXPECT_NOTHROW(small->read<uint8_t>(0));
    EXPECT_NOTHROW(small->write<uint8_t>(0, 0));
    EXPECT_THROW(small->read<uint8_t>(1));
    EXPECT_THROW(small->write<uint8_t>(0, 1));
    EXPECT_THROW(small->read<uint8_t>(255));
    EXPECT_THROW(small->write<uint8_t>(0, 255));
    EXPECT_THROW(small->read<uint8_t>(max_index));
    EXPECT_THROW(small->write<uint8_t>(0, max_index));
    EXPECT_THROW(small->read<uint16_t>());
    EXPECT_THROW(small->write<uint16_t>(0));
    EXPECT_THROW(small->read<uint16_t>(1));
    EXPECT_THROW(small->write<uint16_t>(0, 1));
    EXPECT_THROW(small->read<uint16_t>(255));
    EXPECT_THROW(small->write<uint16_t>(0, 255));
    EXPECT_THROW(small->read<uint16_t>(max_index));
    EXPECT_THROW(small->write<uint16_t>(0, max_index));
    EXPECT_THROW(small->read<uint32_t>());
    EXPECT_THROW(small->write<uint32_t>(0));
    EXPECT_THROW(small->read<uint32_t>(1));
    EXPECT_THROW(small->write<uint32_t>(0, 1));
    EXPECT_THROW(small->read<uint32_t>(max_index));
    EXPECT_THROW(small->write<uint32_t>(0, max_index));
    EXPECT_THROW(small->read<uint64_t>());
    EXPECT_THROW(small->write<uint64_t>(0));
    EXPECT_THROW(small->read<uint64_t>(1));
    EXPECT_THROW(small->write<uint64_t>(0, 1));
    EXPECT_THROW(small->read<uint64_t>(max_index));
    EXPECT_THROW(small->write<uint64_t>(0, max_index));

    // (fields)
    EXPECT_EQUAL(span_largest->getNumBits(), 58u); // bits 33-90 = 58 b
    EXPECT_THROW(span_largest->write((uint64_t)1 << span_largest->getNumBits())); // Value too wide
    EXPECT_THROW(span_largest->write(0x400000000000000)); // Same value as above: too wide
    EXPECT_THROW(span_largest->write(0xffffffffffffffff)); // Value way too wide
    EXPECT_NOTHROW(span_largest->write(0)); // Ok
    EXPECT_NOTHROW(span_largest->write((1 << span_largest->getNumBits()) - 1)); // OK

    EXPECT_EQUAL(starting->getNumBits(), 1u); // bits 0-0 = 1 b
    EXPECT_THROW(starting->write(2)); // Too wide
    EXPECT_THROW(starting->write(0xffffffffffffffff)); // Value wya too wide
    EXPECT_NOTHROW(starting->write(0)); // Ok
    EXPECT_NOTHROW(starting->write((uint64_t)(1 << starting->getNumBits()) - 1)); // OK

    EXPECT_EQUAL(span_large->getNumBits(), 16u); // bits 60-75 = 16 b
    EXPECT_THROW(span_large->write((uint64_t)1 << span_large->getNumBits())); // Value too wide
    EXPECT_THROW(span_large->write(0x100000)); // Same value as above: too wide
    EXPECT_THROW(span_large->write(0xffffffffffffffff)); // Value way too wide
    EXPECT_NOTHROW(span_large->write(0)); // Ok
    EXPECT_NOTHROW(span_large->write(((uint64_t)1 << span_large->getNumBits()) - 1)); // OK

    // RegisterSet Banking
    cur_bank = 1;
    EXPECT_EQUAL(rset->getCurrentBank(0, 0, nullptr), cur_bank);
    cur_bank = 2;
    EXPECT_EQUAL(rset->getCurrentBank(0, 0, nullptr), cur_bank);

    // Test current-bank callback
    check_group_info = true;
    expected_group_num = 4;
    expected_group_idx = 2;
    expected_name = "sprxx";
    cur_bank = 2;
    // Invokes the callback, which checks for group 4, idx 2 (from sprxx proxy)
    EXPECT_NOTHROW(rset->getRegisterProxy("sprxx").getCurrentRegister());
    check_group_info = false;

    // RegisterProxy testing
    // Use pointers to hold result of getRegisterProxy
    EXPECT_NOTHROW(cur_bank = 0; \
                   auto prox = &rset->getRegisterProxy("sprxx"); \
                   EXPECT_EQUAL(prox->getCurrentRegister(), sprxxa); \
                   cur_bank = 4; \
                   prox = &rset->getRegisterProxy("sprxx"); \
                   EXPECT_EQUAL(prox->getCurrentRegister(), sprxxb););

    // Test proxy lookup
    cur_bank = 0;
    EXPECT_NOTHROW(EXPECT_EQUAL(rset->getRegisterProxy("sprxx").getCurrentRegister(), sprxxa));
    cur_bank = 1;
    auto prx = &rset->getRegisterProxy("sprxx");
    EXPECT_NOTHROW(EXPECT_EQUAL(prx->getCurrentRegister(), sprxxa));
    cur_bank = 2;
    EXPECT_NOTHROW(EXPECT_EQUAL(rset->getRegisterProxy("sprxx").getCurrentRegister(), sprxxa));
    cur_bank = 3; // no sprxx in bank 3
    EXPECT_THROW(prx->getCurrentRegister());
    cur_bank = 4;
    EXPECT_NOTHROW(EXPECT_EQUAL(prx->getCurrentRegister(), sprxxb));
    cur_bank = 5;
    EXPECT_NOTHROW(EXPECT_EQUAL(prx->getCurrentRegister(), sprxxb));
    auto &prx2 = rset->getRegisterProxy("sprxx");
    cur_bank = 6;
    EXPECT_NOTHROW(EXPECT_EQUAL(prx2.getCurrentRegister(), sprxxb));
    EXPECT_EQUAL(prx2.tryGetCurrentRegister(), sprxxb);
    cur_bank = 7; // no sprxx in bank 7
    EXPECT_THROW(prx2.getCurrentRegister());
    EXPECT_EQUAL(prx2.tryGetCurrentRegister(), nullptr);

    // Proxy Printouts
    {
        auto &prx3 = rset->getRegisterProxy("sprxx");
        std::cout << &prx3 << std::endl;
        std::cout << prx3 << std::endl;
    }

    // Get a proxy to some non-proxy register
    auto &prx_reg1 = rset->getRegisterProxy("reg1");
    cur_bank = 0;
    EXPECT_NOTHROW(EXPECT_EQUAL(prx_reg1.getCurrentRegister(), rset->getRegister("reg1")));
    cur_bank = 1000001;
    EXPECT_NOTHROW(EXPECT_EQUAL(prx_reg1.getCurrentRegister(), rset->getRegister("reg1")));
    auto &prx_reg2 = rset->getRegisterProxy("small");
    EXPECT_NOTHROW(EXPECT_EQUAL(prx_reg2.getCurrentRegister(), rset->getRegister("small")));

    // Render Tree

    std::cout << "The tree from the top with builtins: " << std::endl << root.renderSubtree(-1, true) << std::endl;
    std::cout << "The tree from the top without builtins: " << std::endl << root.renderSubtree() << std::endl;
    std::cout << "The tree from regs: " << std::endl << rset->renderSubtree(-1, true);
    std::cout << "The tree from large: " << std::endl << large->renderSubtree(-1, true);
    std::cout << "The tree from med: " << std::endl << med->renderSubtree(-1, true);


    // Get Timing on some register pokes and print results
    timeWrites(med);

    // Test register dmi
    EXPECT_EQUAL(sprxxa->peek<uint32_t>(), 0xdeadbeef);    // establish known val in register
    sprxxa->dmiWrite<uint32_t>(0xfeedface);                // write directly to register's backing store
    EXPECT_EQUAL(sprxxa->peek<uint32_t>(), 0xfeedface);    // "normal" peek
    sprxxa->poke<uint32_t>(0xc5acce55);                    // "normal" poke
    EXPECT_EQUAL(sprxxa->dmiRead<uint32_t>(), 0xc5acce55); // read directly from register's backing store

    root.enterTeardown();

    // Done

    REPORT_ERROR;

    return ERROR_CODE;
}
