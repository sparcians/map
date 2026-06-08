// This is a dedicated test for an Argos collection bug.
// The data structures to collect which draw out this bug look like this:
//
//   class FetchOp
//       virtual ~FetchOp() = default;
//       uint64_t getUID() const;
//       uint64_t getFetchStart() const;  // HEX
//       uint64_t getFetchEnd() const;    // HEX
//
//   class FetchPairDef : public sparta::PairDefinition<FetchOp>
//       SPARTA_ADDPAIR("DID", &FetchOp::getUID)
//       SPARTA_ADDPAIR("start_pc", &FetchOp::getFetchStart, HEX)
//       SPARTA_ADDPAIR("end_pc", &FetchOp::getFetchEnd, HEX)
//
//
//
//   class ArchFetchOp : public FetchOp
//
//   class ArchBranchFetchOp
//       std::shared_ptr<ArchFetchOp> fetch_;
//       const auto & getFetchOp() const { return fetch_; }
//       SPARTA_FLATTEN(&ArchBranchFetchOp::getFetchOp)
//       (ignore "branch" for now - bug is reproducible without it)
//
//
//
// The collected object is a sparta::Queue<std::shared_ptr<ArchBranchFetchOp>>
// (whose enableCollection() internally builds an IterableCollector over the queue).
//
// The bug: SPARTA_FLATTEN points at a member (getFetchOp) whose static type is a
// *derived* class (ArchFetchOp), but the PairDefinition + accessor method pointers
// live on the *base* class (FetchOp). In populateFromEntityUtility_() the
// pointer+method-pointer overload requires is_same<class-of-method-ptr, pointee>,
// which is false for (FetchOp vs ArchFetchOp). The chain falls through to a
// "return false" path, so the flattened (DID/start_pc/end_pc) fields are NEVER
// written to the blob stream even though the serialized schema advertises them.
//
// IMPORTANT - why the trailing non-flattened "dummy" field is required:
// If EVERY field of the type goes through the broken flatten path, then NOTHING is
// written to the blob. The python smoke test then has nothing to deserialize and
// passes trivially (masking the bug). ArchBranchFetchOp therefore also collects a
// plain, non-flattened uint32_t "dummy" (getDummy() -> 404). Of the 4 pair fields,
// the first 3 (the flattened ones) hit "return false" and write nothing, while the
// 4th writes 404. Now the byte stream is shorter than the schema claims and is
// misaligned, so the deserializer bombs. Do NOT remove "dummy" - it is what makes
// the truncation observable.
//
// This C++ test is EXPECTED TO EXIT CLEANLY. The bug is latent in the emitted
// database. To confirm the bug, read the db back with the python deserializer:
//
//   1. Run this test to produce bug.db
//   2. conda activate sparta2
//   3. python3 ~/simdb-collector-v3/python/argos/viewer/model/blob_iterator.py bug.db
//
// That blob_iterator.py smoke test should immediately fail on the field-count /
// byte-stream mismatch.
//
// No DB/bytes validation is done here on purpose - this first step only isolates
// the bug. Fixing + validation come later.

#include "sparta/sparta.hpp"
#include "sparta/app/Simulation.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/kernel/SleeperThread.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/pairs/SpartaKeyPairs.hpp"
#include "sparta/resources/Queue.hpp"

#include <memory>

TEST_INIT;

//
// Base op with a PairDefinition. Note: getters are NON-virtual and the
// PairDefinition is defined on this base type.
//
class FetchPairDef;

class FetchOp
{
public:
    using SpartaPairDefinitionType = FetchPairDef;

    FetchOp(uint64_t uid, uint64_t start_pc, uint64_t end_pc)
        : uid_(uid)
        , start_pc_(start_pc)
        , end_pc_(end_pc)
    {}

    virtual ~FetchOp() = default;

    uint64_t getUID() const { return uid_; }
    uint64_t getFetchStart() const { return start_pc_; }
    uint64_t getFetchEnd() const { return end_pc_; }

private:
    uint64_t uid_;
    uint64_t start_pc_;
    uint64_t end_pc_;
};

class FetchPairDef : public sparta::PairDefinition<FetchOp>
{
public:
    FetchPairDef() : sparta::PairDefinition<FetchOp>() {
        SPARTA_INVOKE_PAIRS(FetchOp);
    }
    SPARTA_REGISTER_PAIRS(SPARTA_ADDPAIR("DID",      &FetchOp::getUID),
                          SPARTA_ADDPAIR("start_pc", &FetchOp::getFetchStart, std::ios::hex),
                          SPARTA_ADDPAIR("end_pc",   &FetchOp::getFetchEnd,   std::ios::hex))
};

//
// Derived op. It has NO PairDefinition of its own - it inherits
// SpartaPairDefinitionType (= FetchPairDef) from FetchOp. This derived-but-base-
// pairdef shape is what triggers the bug when flattened.
//
class ArchFetchOp : public FetchOp
{
public:
    ArchFetchOp(uint64_t uid, uint64_t start_pc, uint64_t end_pc)
        : FetchOp(uid, start_pc, end_pc)
    {}
};

//
// Enclosing op that flattens a pointer to the *derived* type.
//
class ArchBranchFetchOpPairDef;

class ArchBranchFetchOp
{
public:
    using SpartaPairDefinitionType = ArchBranchFetchOpPairDef;

    explicit ArchBranchFetchOp(const std::shared_ptr<ArchFetchOp> & fetch)
        : fetch_(fetch)
    {}

    const std::shared_ptr<ArchFetchOp> & getFetchOp() const { return fetch_; }

    uint32_t getDummy() const { return 404; }

private:
    std::shared_ptr<ArchFetchOp> fetch_;
};

class ArchBranchFetchOpPairDef : public sparta::PairDefinition<ArchBranchFetchOp>
{
public:
    ArchBranchFetchOpPairDef() : sparta::PairDefinition<ArchBranchFetchOp>() {
        SPARTA_INVOKE_PAIRS(ArchBranchFetchOp);
    }
    SPARTA_REGISTER_PAIRS(SPARTA_FLATTEN(&ArchBranchFetchOp::getFetchOp),
                          SPARTA_ADDPAIR("dummy", &ArchBranchFetchOp::getDummy))
};

using ArchBranchFetchOpPtr = std::shared_ptr<ArchBranchFetchOp>;

static uint64_t nextUID()
{
    static uint64_t uid = 0;
    return ++uid;
}

static ArchBranchFetchOpPtr createBranchFetchOp()
{
    const uint64_t uid = nextUID();
    const uint64_t start_pc = 0x1000 + uid * 0x40;
    const uint64_t end_pc = start_pc + 0x10;
    auto fetch = std::make_shared<ArchFetchOp>(uid, start_pc, end_pc);
    return std::make_shared<ArchBranchFetchOp>(fetch);
}

//
// ---------------------------------------------------------------------------
// Multi-level flatten case (generalized fix).
//
// Here the flatten chain crosses a base/derived boundary at a NON-terminal hop:
//
//   OuterOp -> flatten(getMidOp -> shared_ptr<ArchMidOp>)        (ArchMidOp : MidOp)
//     MidOp -> flatten(getFetchOp -> shared_ptr<ArchFetchOp>)    (ArchFetchOp : FetchOp)
//       FetchOp -> DID / start_pc / end_pc
//     MidOp -> mid
//   OuterOp -> outer_dummy
//
// The pack for an inner field is (getMidOp, getFetchOp, getUID). The middle hop
// dereferences a shared_ptr<ArchMidOp> and calls &MidOp::getFetchOp (a proper
// base method) with MORE method pointers still to follow -- the non-last
// base-of recurse case that the generalized fix adds.
// ---------------------------------------------------------------------------
//
class MidPairDef;

class MidOp
{
public:
    using SpartaPairDefinitionType = MidPairDef;

    MidOp(const std::shared_ptr<ArchFetchOp> & fetch, uint64_t mid)
        : fetch_(fetch)
        , mid_(mid)
    {}

    virtual ~MidOp() = default;

    const std::shared_ptr<ArchFetchOp> & getFetchOp() const { return fetch_; }
    uint64_t getMid() const { return mid_; }

private:
    std::shared_ptr<ArchFetchOp> fetch_;
    uint64_t mid_;
};

class MidPairDef : public sparta::PairDefinition<MidOp>
{
public:
    MidPairDef() : sparta::PairDefinition<MidOp>() {
        SPARTA_INVOKE_PAIRS(MidOp);
    }
    SPARTA_REGISTER_PAIRS(SPARTA_FLATTEN(&MidOp::getFetchOp),
                          SPARTA_ADDPAIR("mid", &MidOp::getMid))
};

//
// Derived middle op - inherits MidPairDef (= PairDefinition<MidOp>).
//
class ArchMidOp : public MidOp
{
public:
    ArchMidOp(const std::shared_ptr<ArchFetchOp> & fetch, uint64_t mid)
        : MidOp(fetch, mid)
    {}
};

class OuterPairDef;

class OuterOp
{
public:
    using SpartaPairDefinitionType = OuterPairDef;

    explicit OuterOp(const std::shared_ptr<ArchMidOp> & mid)
        : mid_(mid)
    {}

    const std::shared_ptr<ArchMidOp> & getMidOp() const { return mid_; }

    uint32_t getDummy() const { return 909; }

private:
    std::shared_ptr<ArchMidOp> mid_;
};

class OuterPairDef : public sparta::PairDefinition<OuterOp>
{
public:
    OuterPairDef() : sparta::PairDefinition<OuterOp>() {
        SPARTA_INVOKE_PAIRS(OuterOp);
    }
    SPARTA_REGISTER_PAIRS(SPARTA_FLATTEN(&OuterOp::getMidOp),
                          SPARTA_ADDPAIR("outer_dummy", &OuterOp::getDummy))
};

using OuterOpPtr = std::shared_ptr<OuterOp>;

static OuterOpPtr createOuterOp()
{
    const uint64_t uid = nextUID();
    const uint64_t start_pc = 0x1000 + uid * 0x40;
    const uint64_t end_pc = start_pc + 0x10;
    auto fetch = std::make_shared<ArchFetchOp>(uid, start_pc, end_pc);
    auto mid = std::make_shared<ArchMidOp>(fetch, 0x5000 + uid);
    return std::make_shared<OuterOp>(mid);
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

void TestBranchFetchOp(int argc, char** argv)
{
    sparta::Scheduler sched;
    TestSimulator sim(sched);
    sparta::RootTreeNode& rtn = *sim.getRoot();

    sparta::app::SimulationConfiguration sim_config;
    auto& simdb_config = sim_config.simdb_config;
    simdb_config.setSimExecutable(argv[0]);
    simdb_config.enableApp("argos-collector");
    simdb_config.setAppDatabase("argos-collector", "bug.db");

    sim.configure(argc, argv, &sim_config, false);
    sim.buildTree();
    sim.configureTree();

    sparta::StatisticSet queue_stats(&rtn);
    sparta::Queue<ArchBranchFetchOpPtr> queue("collect_me", 8, rtn.getClock(), &queue_stats);
    queue.enableCollection(&rtn);
    sim.finalizeTree();

    constexpr size_t heartbeat = 3;
    sparta::collection::PipelineCollector pc("testPipe", heartbeat, rtn.getClock(), &rtn);
    sim.finalizeFramework();
    pc.startCollection(&rtn);

    sched.run(1);

    queue.push(createBranchFetchOp());
    sched.run(1);

    queue.push(createBranchFetchOp());
    sched.run(1);

    queue.push(createBranchFetchOp());
    sched.run(1);

    pc.destroy();
    sim.postProcessingLastCall();
}

void TestMultiLevelFlatten(int argc, char** argv)
{
    sparta::Scheduler sched;
    TestSimulator sim(sched);
    sparta::RootTreeNode& rtn = *sim.getRoot();

    sparta::app::SimulationConfiguration sim_config;
    auto& simdb_config = sim_config.simdb_config;
    simdb_config.setSimExecutable(argv[0]);
    simdb_config.enableApp("argos-collector");
    simdb_config.setAppDatabase("argos-collector", "bug_multilevel.db");

    sim.configure(argc, argv, &sim_config, false);
    sim.buildTree();
    sim.configureTree();

    sparta::StatisticSet queue_stats(&rtn);
    sparta::Queue<OuterOpPtr> queue("collect_me", 8, rtn.getClock(), &queue_stats);
    queue.enableCollection(&rtn);
    sim.finalizeTree();

    constexpr size_t heartbeat = 3;
    sparta::collection::PipelineCollector pc("testPipe", heartbeat, rtn.getClock(), &rtn);
    sim.finalizeFramework();
    pc.startCollection(&rtn);

    sched.run(1);

    queue.push(createOuterOp());
    sched.run(1);

    queue.push(createOuterOp());
    sched.run(1);

    queue.push(createOuterOp());
    sched.run(1);

    pc.destroy();
    sim.postProcessingLastCall();
}

int main(int argc, char** argv)
{
    sparta::SleeperThread::disableForever();
    TestBranchFetchOp(argc, argv);
    TestMultiLevelFlatten(argc, argv);

    REPORT_ERROR;
    return ERROR_CODE;
}
