
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/collection/PipelineCollector.hpp"
#include "sparta/collection/Collectable.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/events/Event.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/StartupEvent.hpp"
#include "sparta/log/Tap.hpp"
#include "sparta/pairs/SpartaKeyPairs.hpp"
#include "yaml-cpp/yaml.h"

/*
 * This test basically tests the Flattening of Nested Pairs in Collectable
 * classes while collection is on. The purpose of this test is to be certain
 * that the Flattening runs correctly on any depth of Nested Classes. This
 * test is also required to make sure that not only we can flatten nested
 * pairs, but also, we are collecting the correct values of the Nested classes.
 *
 * For testing that we are flattening the nested pairs and collecting the correct
 * values, we are using a Debugging API called sparta::Collectable::dumpNameValuePairs()
 * which takes as the only parameter, an object of the type we are collecting and returns
 * all the name value pairs we want to collect in a nicely formatted string.
 *
 * We take that object and register its own name value pairs, and then we start
 * the Flattening process, going one level deeper into the Nested classes and
 * processing its pairs and so on, going deeper and deeper till hit the base class
 * or the standalone class.
 *
 * For this test, I am flattening nested pairs with a maximum degree of 8 and
 * this works of N levels.
*/

/*
 * This is the Level 1 class or Standalone class.
 * This class does not have any Nested Pair Classes
 * inside of itself. This is the most basic and simplest
 * case of Pair Collection.
*/
class Level_1_PairDef;
class Level_1{
public:
    using type = Level_1_PairDef;
    Level_1(uint64_t uid, uint64_t vaddr, uint64_t raddr,
            const std::vector<uint16_t>& vec) :
            uid_(uid), vaddr_(vaddr), raddr_(raddr), vec_(vec) {}
    uint64_t getUid() const { return uid_; }
    uint64_t getVaddr() const { return vaddr_; }
    uint64_t getRaddr() const { return raddr_; }
    std::vector<uint16_t> getVec() const {  return vec_; }
private:
    uint64_t uid_;
    uint64_t vaddr_;
    uint64_t raddr_;
    std::vector<uint16_t> vec_;
};
typedef std::shared_ptr<Level_1> Level_1_Ptr;

class Level_1_PairDef : public sparta::PairDefinition<Level_1>{
public:

    Level_1_PairDef() : PairDefinition<Level_1>(){
        SPARTA_INVOKE_PAIRS(Level_1);
    }
    SPARTA_REGISTER_PAIRS(SPARTA_ADDPAIR("uid", &Level_1::getUid),
                        SPARTA_ADDPAIR("vaddr", &Level_1::getVaddr),
                        SPARTA_ADDPAIR("raddr", &Level_1::getRaddr),
                        SPARTA_ADDPAIR("vector", &Level_1::getVec));
};

inline std::ostream& operator << (std::ostream& os, const std::vector<uint16_t>& vec){
    for(const auto& items : vec){
        os << items << " ";
    }
    return os;
}

/*
 * This is the Level 2 class which contains its own Name Value pairs
 * as well as an instance or pointer to instance of Level 1 class, which
 * means, Level 1 Class is nested inside this class. So, Level 2 classes
 * Name Value pairs consists of its own Name Value pairs + Level 1 Name Value
 * pairs.
*/
class Level_2_PairDef;
class Level_2{
public:
    using type = Level_2_PairDef;
    enum class TargetUnit : std::uint8_t{
        ALU0,
        ALU1,
        FPU,
        BR,
        LSU,
        ROB,
        N_TARGET_UNITS};
    Level_2(const Level_1_Ptr& ptr, uint32_t latency,
            bool complete, Level_2::TargetUnit unit) :
            level_1_ptr_(ptr), latency_(latency),
            complete_(complete), unit_(unit) {}

    const Level_1_Ptr & getNestedPtr() const { return level_1_ptr_; }
    uint32_t getLatency() const { return latency_; }
    bool getComplete() const { return complete_; }
    const TargetUnit& getUnit() const { return unit_; }
private:
    Level_1_Ptr level_1_ptr_ = nullptr;
    uint32_t latency_;
    bool complete_;
    TargetUnit unit_;
};
typedef std::shared_ptr<Level_2> Level_2_Ptr;

inline std::ostream & operator<<(std::ostream & os, const Level_2::TargetUnit & unit) {
    switch(unit)
    {
        case Level_2::TargetUnit::ALU0:
        os << "ALU0";
        break;
        case Level_2::TargetUnit::ALU1:
        os << "ALU1";
        break;
        case Level_2::TargetUnit::FPU:
        os << "FPU";
        break;
        case Level_2::TargetUnit::BR:
        os << "BR";
        break;
        case Level_2::TargetUnit::LSU:
        os << "LSU";
        break;
        case Level_2::TargetUnit::ROB:
        os << "ROB";
        break;
        case Level_2::TargetUnit::N_TARGET_UNITS:
        os << "ERROR!!!";
        break;
    }
    return os;
}

class Level_2_PairDef : public sparta::PairDefinition<Level_2>{
public:
    Level_2_PairDef() : PairDefinition<Level_2>(){
        SPARTA_INVOKE_PAIRS(Level_2);
    }
    SPARTA_REGISTER_PAIRS(SPARTA_ADDPAIR("latency", &Level_2::getLatency),
                        SPARTA_ADDPAIR("complete", &Level_2::getComplete),
                        SPARTA_FLATTEN(&Level_2::getNestedPtr),
                        SPARTA_ADDPAIR("unit", &Level_2::getUnit));
};

/*
 * This is the Level 3 class which contains its own Name Value pairs
 * as well as an instance or pointer to instance of Level 2 class, which
 * means, Level 2 Class is nested inside this class. So, Level 3 classes
 * Name Value pairs consists of its own Name Value pairs + Level 2 Name Value
 * pairs.
*/
class Level_3_PairDef;
class Level_3{
public:
    using type = Level_3_PairDef;
    enum class MMUState : std::uint8_t {
        NO_ACCESS,
        MISS,
        HIT,
        NUM_STATES};
    enum class CacheState : std::uint8_t {
        NO_ACCESS,
        MISS,
        HIT,
        NUM_STATES};

    Level_3(const Level_2_Ptr& ptr, Level_3::MMUState mmustate,
            Level_3::CacheState cachestate) :
            level_2_ptr_(ptr), mmustate_(mmustate),
            cachestate_(cachestate) {}
    const Level_2_Ptr & getNestedPtr() const { return level_2_ptr_; }
    const MMUState& getMMUState() const { return mmustate_; }
    const CacheState& getCacheState() const { return cachestate_; }
private:
    Level_2_Ptr level_2_ptr_ = nullptr;
    MMUState mmustate_;
    CacheState cachestate_;
};
typedef std::shared_ptr<Level_3> Level_3_Ptr;

inline std::ostream & operator<<(std::ostream & os, const Level_3::MMUState & mmuaccessstate){
    switch(mmuaccessstate){
        case Level_3::MMUState::NO_ACCESS:
        os << "no_access";
        break;
        case Level_3::MMUState::MISS:
        os << "miss";
        break;
        case Level_3::MMUState::HIT:
        os << "hit";
        break;
        default:
        os << "N/A";
    }
    return os;
}

inline std::ostream & operator<<(std::ostream & os, const Level_3::CacheState & cacheaccessstate){
    switch(cacheaccessstate){
        case Level_3::CacheState::NO_ACCESS:
        os << "no_access";
        break;
        case Level_3::CacheState::MISS:
        os << "miss";
        break;
        case Level_3::CacheState::HIT:
        os << "hit";
        break;
        default:
        os << "N/A";
    }
    return os;
}

class Level_3_PairDef : public sparta::PairDefinition<Level_3>{
public:
    Level_3_PairDef() : PairDefinition<Level_3>(){
        SPARTA_INVOKE_PAIRS(Level_3);
    }
    SPARTA_REGISTER_PAIRS(SPARTA_ADDPAIR("mmu", &Level_3::getMMUState),
                        SPARTA_FLATTEN(&Level_3::getNestedPtr),
                        SPARTA_ADDPAIR("cache", &Level_3::getCacheState));
};

/*
 * This is the Level 4 class which contains its own Name Value pairs
 * as well as an instance or pointer to instance of Level 3 class, which
 * means, Level 3 Class is nested inside this class. So, Level 4 classes
 * Name Value pairs consists of its own Name Value pairs + Level 3 Name Value
 * pairs.
*/
class Level_4_PairDef;
class Level_4{
public:
    using type = Level_4_PairDef;
    enum class IssuePriority : std::uint8_t {
        HIGHEST,
        CACHE_RELOAD,   // Receive mss ack, waiting for cache re-access
        CACHE_PENDING,  // Wait for another outstanding miss finish
        MMU_RELOAD,     // Receive for mss ack, waiting for mmu re-access
        MMU_PENDING,    // Wait for another outstanding miss finish
        NEW_DISP,       // Wait for new issue
        LOWEST,
        NUM_OF_PRIORITIES
    };
    enum class IssueState : std::uint8_t {
        READY,          // Ready to be issued
        ISSUED,         // On the flight somewhere inside Load/Store Pipe
        NOT_READY,      // Not ready to be issued
        NUM_STATES
    };

    Level_4(const Level_3_Ptr& ptr, Level_4::IssuePriority rank,
            Level_4::IssueState state) :
            level_3_ptr_(ptr), rank_(rank), state_(state) {}
    const Level_3_Ptr & getNestedPtr() const { return level_3_ptr_; }
    const IssuePriority& getRank() const { return rank_; }
    const IssueState& getState() const { return state_; }
private:
    Level_3_Ptr level_3_ptr_ = nullptr;
    IssuePriority rank_;
    IssueState state_;
};
typedef std::shared_ptr<Level_4> Level_4_Ptr;

inline std::ostream& operator<<(std::ostream& os, const Level_4::IssuePriority& rank){
    switch(rank){
        case Level_4::IssuePriority::HIGHEST:
        os << "highest";
        break;
        case Level_4::IssuePriority::CACHE_RELOAD:
        os << "$_reload";
        break;
        case Level_4::IssuePriority::CACHE_PENDING:
        os << "$_pending";
        break;
        case Level_4::IssuePriority::MMU_RELOAD:
        os << "mmu_reload";
        break;
        case Level_4::IssuePriority::MMU_PENDING:
        os << "mmu_pending";
        break;
        case Level_4::IssuePriority::NEW_DISP:
        os << "new_disp";
        break;
        case Level_4::IssuePriority::LOWEST:
        os << "lowest";
        break;
        default:
        os << "N/A";
    }
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const Level_4::IssueState& state){
    // Print instruction issue state
    switch(state){
        case Level_4::IssueState::NOT_READY:
        os << "not_ready";
        break;
        case Level_4::IssueState::READY:
        os << "ready";
        break;
        case Level_4::IssueState::ISSUED:
        os << "issued";
        break;
        default:
        os << "N/A";
    }
    return os;
}

class Level_4_PairDef : public sparta::PairDefinition<Level_4>{
public:
    Level_4_PairDef() : PairDefinition<Level_4>(){
        SPARTA_INVOKE_PAIRS(Level_4);
    }
    SPARTA_REGISTER_PAIRS(SPARTA_ADDPAIR("rank", &Level_4::getRank),
                        SPARTA_FLATTEN(&Level_4::getNestedPtr),
                        SPARTA_ADDPAIR("state", &Level_4::getState));
};

/*
 * This is the Level 5 class which contains its own Name Value pairs
 * as well as an instance or pointer to instance of Level 4 class, which
 * means, Level 4 Class is nested inside this class. So, Level 5 classes
 * Name Value pairs consists of its own Name Value pairs + Level 4 Name Value
 * pairs.
*/
class Level_5_PairDef;
class Level_5{
public:
    using type = Level_5_PairDef;
    enum class Mnemonic : std::uint8_t {
        ADC,
        CLZ,
        ADD,
        CMN,
        VABA,
        CMP,
        SUB
    };

    Level_5(const Level_4_Ptr& ptr, Level_5::Mnemonic mnemonic) :
            level_4_ptr_(ptr), mnemonic_(mnemonic) {}
    const Level_4_Ptr & getNestedPtr() const { return level_4_ptr_; }
    const Mnemonic& getMnemonic() const { return mnemonic_; }
private:
    Level_4_Ptr level_4_ptr_ = nullptr;
    Mnemonic mnemonic_;
};
typedef std::shared_ptr<Level_5> Level_5_Ptr;

inline std::ostream& operator<<(std::ostream& os, const Level_5::Mnemonic& mnemonic){
    switch(mnemonic){
        case Level_5::Mnemonic::ADC:
        os << "adc";
        break;
        case Level_5::Mnemonic::CLZ:
        os << "clz";
        break;
        case Level_5::Mnemonic::ADD:
        os << "add";
        break;
        case Level_5::Mnemonic::CMN:
        os << "cmn";
        break;
        case Level_5::Mnemonic::VABA:
        os << "vaba";
        break;
        case Level_5::Mnemonic::CMP:
        os << "cmp";
        break;
        case Level_5::Mnemonic::SUB:
        os << "sub";
        break;
        default:
        os << "N/A";
    }
    return os;
}

class Level_5_PairDef : public sparta::PairDefinition<Level_5>{
public:
    Level_5_PairDef() : PairDefinition<Level_5>(){
        SPARTA_INVOKE_PAIRS(Level_5);
    }
    SPARTA_REGISTER_PAIRS(SPARTA_ADDPAIR("mnemonic", &Level_5::getMnemonic),
                        SPARTA_FLATTEN(&Level_5::getNestedPtr));
};

/*
 * This is the Level 6 class which contains its own Name Value pairs
 * as well as an instance or pointer to instance of Level 5 class, which
 * means, Level 5 Class is nested inside this class. So, Level 6 classes
 * Name Value pairs consists of its own Name Value pairs + Level 5 Name Value
 * pairs.
*/
class Level_6_PairDef;
class Level_6{
public:
    using type = Level_6_PairDef;
    Level_6(const Level_5_Ptr& ptr, uint16_t randomValue) :
            level_5_ptr_(ptr), randomValue_(randomValue) {}
    const Level_5_Ptr & getNestedPtr() const { return level_5_ptr_; }
    const uint16_t& getRandomValue() const { return randomValue_; }
private:
    Level_5_Ptr level_5_ptr_ = nullptr;
    uint16_t randomValue_;
};
typedef std::shared_ptr<Level_6> Level_6_Ptr;

class Level_6_PairDef : public sparta::PairDefinition<Level_6>{
public:
    Level_6_PairDef() : PairDefinition<Level_6>(){
        SPARTA_INVOKE_PAIRS(Level_6);
    }
    SPARTA_REGISTER_PAIRS(SPARTA_ADDPAIR("RandomValue", &Level_6::getRandomValue),
                        SPARTA_FLATTEN(&Level_6::getNestedPtr));
};

/*
 * This is the Level 7 class which contains its own Name Value pairs
 * as well as an instance or pointer to instance of Level 6 class, ehich
 * means, Level 6 Class is nested inside this class. So, Level 7 classes
 * Name Value pairs consists of its own Name Value pairs + Level 6 Name Value
 * pairs.
*/
class Level_7_PairDef;
class Level_7{
public:
    using type = Level_7_PairDef;
    Level_7(const Level_6_Ptr& ptr, uint16_t randomValue,
            bool a, uint64_t b) :
            level_6_ptr_(ptr), randomValue_(randomValue),
            pair_(std::make_pair(a, b)) {}
    const Level_6_Ptr & getNestedPtr() const { return level_6_ptr_; }
    const uint16_t& getRandomValue() const { return randomValue_; }
    const std::pair<bool, uint64_t>& getPair() const { return pair_; }
private:
    Level_6_Ptr level_6_ptr_ = nullptr;
    uint16_t randomValue_;
    std::pair<bool, uint64_t> pair_;
};
typedef std::shared_ptr<Level_7> Level_7_Ptr;

class Level_7_PairDef : public sparta::PairDefinition<Level_7>{
public:
    Level_7_PairDef() : PairDefinition<Level_7>(){
        SPARTA_INVOKE_PAIRS(Level_7);
    }
    SPARTA_REGISTER_PAIRS(SPARTA_ADDPAIR("SomeValue", &Level_7::getRandomValue),
                        SPARTA_FLATTEN(&Level_7::getNestedPtr),
                        SPARTA_ADDPAIR("ran1", &Level_7::getPair),
                        SPARTA_ADDPAIR("ran2", &Level_7::getPair));
};

/*
 * This is the Level 8 class which contains its own Name Value pairs
 * as well as an instance or pointer to instance of Level 7 class, which
 * means, Level 7 Class is nested inside this class. So, Level 8 classes
 * Name Value pairs consists of its own Name Value pairs + Level 7 Name Value
 * pairs.
*/
class Level_8_PairDef;
class Level_8{
public:
    using type = Level_8_PairDef;
    Level_8(const Level_7_Ptr& ptr, uint16_t randomValue,
            uint32_t a, uint32_t b) :
            level_7_ptr_(ptr), randomValue_(randomValue),
            pair_(std::make_pair(a, b)) {}
    const Level_7_Ptr & getNestedPtr() const { return level_7_ptr_; }
    const uint16_t& getRandomValue() const { return randomValue_; }
    const std::pair<uint32_t, uint32_t>& getPair() const { return pair_; }
private:
    Level_7_Ptr level_7_ptr_ = nullptr;
    uint16_t randomValue_;
    std::pair<uint32_t, uint32_t> pair_;
};
typedef std::shared_ptr<Level_3> Level_3_Ptr;

class Level_8_PairDef : public sparta::PairDefinition<Level_8>{
public:
    Level_8_PairDef() : PairDefinition<Level_8>(){
        SPARTA_INVOKE_PAIRS(Level_8);
    }
    SPARTA_REGISTER_PAIRS(SPARTA_ADDPAIR("ArbitaryValue", &Level_8::getRandomValue),
                        SPARTA_FLATTEN(&Level_8::getNestedPtr),
                        SPARTA_ADDPAIR("val1", &Level_8::getPair),
                        SPARTA_ADDPAIR("val2", &Level_8::getPair));
};

int main()
{
    sparta::Scheduler sched;
    sparta::RootTreeNode root_node("root");
    sparta::RootTreeNode root_clks("clocks", "Clock Tree Root", root_node.getSearchScope());
    sparta::ClockManager cm(&sched);
    sparta::Clock::Handle root_clk = cm.makeRoot(&root_clks);
    sparta::Clock::Handle clk_1000000 = cm.makeClock("clk_1000000", root_clk, 1000000.0);
    sparta::Clock::Handle clk_100000 = cm.makeClock("clk_100000", root_clk, 100000.0);
    sparta::Clock::Handle clk_10000 = cm.makeClock("clk_10000", root_clk, 10000.0);
    sparta::Clock::Handle clk_1000 = cm.makeClock("clk_1000", root_clk, 1000.0);
    sparta::Clock::Handle clk_100 = cm.makeClock("clk_100", root_clk, 100.0);
    sparta::Clock::Handle clk_10 = cm.makeClock("clk_10", root_clk, 10.0);
    cm.normalize();
    root_node.setClock(root_clk.get());
    sparta::TreeNode obj1000000_tn(&root_node, "obj1000000", "obj1000000 desc");
    sparta::TreeNode obj100000_tn(&root_node, "obj100000", "obj100000 desc");
    sparta::TreeNode obj10000_tn(&root_node, "obj10000", "obj10000 desc");
    sparta::TreeNode obj1000_tn(&root_node, "obj1000", "obj1000 desc");
    sparta::TreeNode obj100_tn(&root_node, "obj100", "obj100 desc");
    sparta::TreeNode obj10_tn(&root_node, "obj10", "obj10 desc");
    obj1000000_tn.setClock(clk_1000000.get());
    obj100000_tn.setClock(clk_100000.get());
    obj10000_tn.setClock(clk_10000.get());
    obj1000_tn.setClock(clk_1000.get());
    obj100_tn.setClock(clk_100.get());
    obj10_tn.setClock(clk_10.get());

    std::vector<uint16_t> vec {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::shared_ptr<Level_1> l_1 {std::make_shared<Level_1>(12, 1024, 4966, vec)};
    sparta::collection::Collectable<Level_1> Level_1_Collector(&obj1000000_tn, "level1_0");
    std::string expectedLogString = "uid(12) vaddr(1024) raddr(4966) vector([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]) ";
    EXPECT_EQUAL(Level_1_Collector.dumpNameValuePairs(*l_1), expectedLogString);

    std::shared_ptr<Level_2> l_2 {std::make_shared<Level_2>(l_1, 4, true, Level_2::TargetUnit::FPU)};
    sparta::collection::Collectable<Level_2> Level_2_Collector(&obj100000_tn, "level2_0");
    expectedLogString = "latency(4) complete(true) uid(12) vaddr(1024) raddr(4966) vector([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]) unit(FPU) ";
    EXPECT_EQUAL(Level_2_Collector.dumpNameValuePairs(*l_2), expectedLogString);

    std::shared_ptr<Level_3> l_3 {std::make_shared<Level_3>(l_2, Level_3::MMUState::MISS, Level_3::CacheState::HIT)};
    sparta::collection::Collectable<Level_3> Level_3_Collector(&obj10000_tn, "level3_0");
    expectedLogString = "mmu(miss) latency(4) complete(true) uid(12) vaddr(1024) raddr(4966) vector([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]) unit(FPU) cache(hit) ";
    EXPECT_EQUAL(Level_3_Collector.dumpNameValuePairs(*l_3), expectedLogString);

    std::shared_ptr<Level_4> l_4 {std::make_shared<Level_4>(l_3, Level_4::IssuePriority::CACHE_RELOAD, Level_4::IssueState::NOT_READY)};
    sparta::collection::Collectable<Level_4> Level_4_Collector(&obj1000_tn, "level4_0");
    expectedLogString = "rank($_reload) mmu(miss) latency(4) complete(true) uid(12) vaddr(1024) raddr(4966) vector([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]) unit(FPU) cache(hit) state(not_ready) ";
    EXPECT_EQUAL(Level_4_Collector.dumpNameValuePairs(*l_4), expectedLogString);

    std::shared_ptr<Level_5> l_5 {std::make_shared<Level_5>(l_4, Level_5::Mnemonic::ADC)};
    sparta::collection::Collectable<Level_5> Level_5_Collector(&obj100_tn, "level5_0");
    expectedLogString = "mnemonic(adc) rank($_reload) mmu(miss) latency(4) complete(true) uid(12) vaddr(1024) raddr(4966) vector([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]) unit(FPU) cache(hit) state(not_ready) ";
    EXPECT_EQUAL(Level_5_Collector.dumpNameValuePairs(*l_5), expectedLogString);

    std::shared_ptr<Level_6> l_6 {std::make_shared<Level_6>(l_5, 1991)};
    sparta::collection::Collectable<Level_6> Level_6_Collector(&obj10_tn, "level6_0");
    expectedLogString = "RandomValue(1991) mnemonic(adc) rank($_reload) mmu(miss) latency(4) complete(true) uid(12) vaddr(1024) raddr(4966) vector([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]) unit(FPU) cache(hit) state(not_ready) ";
    EXPECT_EQUAL(Level_6_Collector.dumpNameValuePairs(*l_6), expectedLogString);

    std::shared_ptr<Level_7> l_7 {std::make_shared<Level_7>(l_6, 2018, true, 714)};
    sparta::collection::Collectable<Level_7> Level_7_Collector(&obj10_tn, "level7_0");
    expectedLogString =
    "SomeValue(2018) RandomValue(1991) mnemonic(adc) rank($_reload) mmu(miss) latency(4) complete(true) uid(12) vaddr(1024) raddr(4966) vector([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]) unit(FPU) cache(hit) state(not_ready) ran1(true) ran2(714) ";
    EXPECT_EQUAL(Level_7_Collector.dumpNameValuePairs(*l_7), expectedLogString);

    std::shared_ptr<Level_8> l_8 {std::make_shared<Level_8>(l_7, 2017, 18, 69)};
    sparta::collection::Collectable<Level_8> Level_8_Collector(&obj10_tn, "level8_0");
    expectedLogString =
    "ArbitaryValue(2017) SomeValue(2018) RandomValue(1991) mnemonic(adc) rank($_reload) mmu(miss) latency(4) complete(true) uid(12) vaddr(1024) raddr(4966) vector([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]) unit(FPU) cache(hit) state(not_ready) ran1(true) ran2(714) val1(18) val2(69) ";
    EXPECT_EQUAL(Level_8_Collector.dumpNameValuePairs(*l_8), expectedLogString);

    root_node.enterTeardown();
    root_clks.enterTeardown();

    REPORT_ERROR;
    return ERROR_CODE;
}
