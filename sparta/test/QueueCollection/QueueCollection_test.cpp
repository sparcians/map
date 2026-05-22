#include "sparta/sparta.hpp"
#include "sparta/app/Simulation.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/kernel/SleeperThread.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/resources/Queue.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/utils/SpartaSharedPointerAllocator.hpp"

TEST_INIT;

enum class e_Color { RED, GREEN, BLUE, __INVALID__ };

std::ostream& operator<<(std::ostream& os, const e_Color color)
{
    switch (color) {
        case e_Color::RED:   os << "RED"; break;
        case e_Color::GREEN: os << "GREEN"; break;
        case e_Color::BLUE:  os << "BLUE"; break;
        case e_Color::__INVALID__: throw sparta::SpartaException("Invalid color enum");
    }
    return os;
}

template <typename T>
class ImplicitPOD
{
    static_assert(MetaStruct::is_pod_convertible_v<T>);

public:
    explicit ImplicitPOD(const T val) : val_(val) {}
    operator T() const { return val_; }
private:
    T val_;
};

class DummyStructPairDef;

class DummyStruct
{
    uint32_t uid_{nextUID_()};
    uint32_t latency_;
    uint64_t opcode_;
    e_Color color_;
    std::string mnemonic_;
    ImplicitPOD<uint32_t> imp_u32_;

    static uint32_t nextUID_() {
        static uint32_t uid = 0;
        return ++uid;
    }

public:
    using SpartaPairDefinitionType = DummyStructPairDef;

    DummyStruct(uint32_t lat, uint64_t opc, e_Color color, const std::string& mnemonic, uint32_t imp_pod)
        : latency_(lat)
        , opcode_(opc)
        , color_(color)
        , mnemonic_(mnemonic)
        , imp_u32_(imp_pod)
    {}

    uint32_t getUID() const { return uid_; }
    uint32_t getLatency() const { return latency_; }
    uint64_t getOpcode() const { return opcode_; }
    e_Color getColor() const { return color_; }
    const std::string& getMnemonic() const { return mnemonic_; }
    uint32_t getImplicitPOD() const { return imp_u32_; }
};

using DummyStructPtr = sparta::SpartaSharedPointer<DummyStruct>;
using DummyStructAlloc = sparta::SpartaSharedPointerAllocator<DummyStruct>;

class DummyStructPairDef : public sparta::PairDefinition<DummyStruct>
{
public:
    DummyStructPairDef() : sparta::PairDefinition<DummyStruct>() {
        SPARTA_INVOKE_PAIRS(DummyStruct);
    }
    SPARTA_REGISTER_PAIRS(SPARTA_ADDPAIR("DID",      &DummyStruct::getUID),
                          SPARTA_ADDPAIR("lat",      &DummyStruct::getLatency),
                          SPARTA_ADDPAIR("opc",      &DummyStruct::getOpcode, std::ios::hex),
                          SPARTA_ADDPAIR("color",    &DummyStruct::getColor),
                          SPARTA_ADDPAIR("mnemonic", &DummyStruct::getMnemonic),
                          SPARTA_ADDPAIR("imp",      &DummyStruct::getImplicitPOD))
};

DummyStructAlloc alloc(6, 3);
auto createRandomDummyStruct()
{
    uint32_t latency = rand() % 8 + 1;
    uint64_t opcode = rand();
    e_Color color = static_cast<e_Color>(rand() % (long)e_Color::__INVALID__);

    static const std::vector<std::string> MNEMONICS = {
        "add", "addi", "csrrwi", "li", "jlr"
    };
    const auto mnemonic = MNEMONICS[rand() % 5];
    uint32_t imp_pod = rand();
    return sparta::allocate_sparta_shared_pointer<DummyStruct>(alloc, latency, opcode, color, mnemonic, imp_pod);
}

class DummyStructNoPairDef
{
    uint32_t uid_{nextUID_()};
    uint32_t latency_;
    uint64_t opcode_;
    e_Color color_;
    std::string mnemonic_;

    static uint32_t nextUID_() {
        static uint32_t uid = 0;
        return ++uid;
    }

public:
    DummyStructNoPairDef(uint32_t lat, uint64_t opc, e_Color color, const std::string& mnemonic)
        : latency_(lat)
        , opcode_(opc)
        , color_(color)
        , mnemonic_(mnemonic)
    {}

    uint32_t getUID() const { return uid_; }
    uint32_t getLatency() const { return latency_; }
    uint64_t getOpcode() const { return opcode_; }
    e_Color getColor() const { return color_; }
    const std::string& getMnemonic() const { return mnemonic_; }
};

std::ostream& streamPairs(std::ostream& os)
{
    return os;
}

template <typename T, typename... Rest>
std::ostream& streamPairs(std::ostream& os, const char* field_name, const T& field_val, Rest&&... rest)
{
    os << field_name << "(" << field_val << ") ";
    return streamPairs(os, std::forward<Rest>(rest)...);
}

template <typename T>
std::ostream& streamPairs(std::ostream& os, const char* field_name, const T& field_val)
{
    return os << field_name << "(" << field_val << ")";
}

std::ostream& operator<<(std::ostream& os, const DummyStructNoPairDef& dummy)
{
    return streamPairs(os, "uid", dummy.getUID(),
                           "lat", dummy.getLatency(),
                           "opc", dummy.getOpcode(),
                           "color", dummy.getColor(),
                           "mnemonic", dummy.getMnemonic());
}

using DummyStructNoPairDefPtr = sparta::SpartaSharedPointer<DummyStructNoPairDef>;
using DummyStructNoPairDefAlloc = sparta::SpartaSharedPointerAllocator<DummyStructNoPairDef>;

DummyStructNoPairDefAlloc alloc_no_pair_def(6, 3);
auto createRandomDummyStructNoPairDef()
{
    uint32_t latency = rand() % 8 + 1;
    uint64_t opcode = rand();
    e_Color color = static_cast<e_Color>(rand() % (long)e_Color::__INVALID__);

    static const std::vector<std::string> MNEMONICS = {
        "add", "addi", "csrrwi", "li", "jlr"
    };
    const auto mnemonic = MNEMONICS[rand() % 5];
    return sparta::allocate_sparta_shared_pointer<DummyStructNoPairDef>(alloc_no_pair_def, latency, opcode, color, mnemonic);
}

class TestSimulator : public sparta::app::Simulation
{
public:
    TestSimulator(sparta::Scheduler & sched) :
        sparta::app::Simulation("TestSimulator", &sched)
    {}

    ~TestSimulator() {
        getRoot()->enterTeardown();
    }

private:
    void buildTree_() override {}
    void configureTree_() override {}
    void bindTree_() override {}
};

void TestDummyStruct(int argc, char** argv)
{
    sparta::Scheduler sched;
    TestSimulator sim(sched);
    sparta::RootTreeNode& rtn = *sim.getRoot();

    sparta::app::SimulationConfiguration sim_config;
    auto& simdb_config = sim_config.simdb_config;
    simdb_config.setSimExecutable(argv[0]);
    simdb_config.enableApp("argos-collector");
    simdb_config.setAppDatabase("argos-collector", "inst_queue.db");

    sim.configure(argc, argv, &sim_config, false);
    sim.buildTree();
    sim.configureTree();

    sparta::StatisticSet queue_stats(&rtn);
    sparta::Queue<DummyStructPtr> queue("dummy_struct", 8, rtn.getClock(), &queue_stats);
    queue.enableCollection(&rtn);
    sim.finalizeTree();

    constexpr size_t heartbeat = 3;
    sparta::collection::PipelineCollector pc("testPipe", heartbeat, rtn.getClock(), &rtn);
    sim.finalizeFramework();
    pc.startCollection(&rtn);

    sched.run(1);
    auto dummy1 = createRandomDummyStruct();
    auto dummy2 = createRandomDummyStruct();
    auto dummy3 = createRandomDummyStruct();

    queue.push(std::move(dummy1));
    sched.run(1);

    queue.push(std::move(dummy2));
    sched.run(1);

    queue.push(std::move(dummy3));
    sched.run(1);

    pc.destroy();
    sim.postProcessingLastCall();
}

void TestDummyStructNoPairDef(int argc, char** argv)
{
    sparta::Scheduler sched;
    TestSimulator sim(sched);
    sparta::RootTreeNode& rtn = *sim.getRoot();

    sparta::app::SimulationConfiguration sim_config;
    auto& simdb_config = sim_config.simdb_config;
    simdb_config.setSimExecutable(argv[0]);
    simdb_config.enableApp("argos-collector");
    simdb_config.setAppDatabase("argos-collector", "inst_queue_no_pair_def.db");

    sim.configure(argc, argv, &sim_config, false);
    sim.buildTree();
    sim.configureTree();

    sparta::StatisticSet queue_stats(&rtn);
    sparta::Queue<DummyStructNoPairDefPtr> queue("dummy_struct_no_pair_def", 8, rtn.getClock(), &queue_stats);
    queue.enableCollection(&rtn);
    sim.finalizeTree();

    constexpr size_t heartbeat = 3;
    sparta::collection::PipelineCollector pc("testPipe", heartbeat, rtn.getClock(), &rtn);
    sim.finalizeFramework();
    pc.startCollection(&rtn);

    sched.run(1);
    auto dummy1 = createRandomDummyStructNoPairDef();
    auto dummy2 = createRandomDummyStructNoPairDef();
    auto dummy3 = createRandomDummyStructNoPairDef();

    queue.push(std::move(dummy1));
    sched.run(1);

    queue.push(std::move(dummy2));
    sched.run(1);

    queue.push(std::move(dummy3));
    sched.run(1);

    pc.destroy();
    sim.postProcessingLastCall();
}

int main(int argc, char** argv)
{
    sparta::SleeperThread::disableForever();
    TestDummyStruct(argc, argv);
    TestDummyStructNoPairDef(argc, argv);

    REPORT_ERROR;
    return ERROR_CODE;
}
