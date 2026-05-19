#include "sparta/sparta.hpp"
#include "sparta/app/Simulation.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/kernel/Scheduler.hpp"
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
    return os << "\n";
}

class DummyStructPairDef;

class DummyStruct
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
    using SpartaPairDefinitionType = DummyStructPairDef;

    DummyStruct(uint32_t lat, uint64_t opc, e_Color color, const std::string& mnemonic)
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
                          SPARTA_ADDPAIR("mnemonic", &DummyStruct::getMnemonic))
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
    return sparta::allocate_sparta_shared_pointer<DummyStruct>(alloc, latency, opcode, color, mnemonic);
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

int main(int argc, char** argv)
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

    REPORT_ERROR;
    return ERROR_CODE;
}
