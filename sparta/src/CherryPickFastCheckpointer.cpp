#include "sparta/serialization/checkpoint/CherryPickFastCheckpointer.hpp"
#include "simdb/apps/AppManager.hpp"
#include "simdb/schema/SchemaDef.hpp"
#include "simdb/pipeline/AsyncDatabaseAccessor.hpp"
#include "simdb/utils/Compress.hpp"

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>

namespace sparta::serialization::checkpoint
{

using checkpoint_type = typename CherryPickFastCheckpointer::checkpoint_type;

CherryPickFastCheckpointer::CherryPickFastCheckpointer(simdb::DatabaseManager* db_mgr,
                                                       const std::vector<TreeNode*> & roots,
                                                       Scheduler* sched) :
    checkpointer_(roots, sched),
    db_mgr_(db_mgr)
{
}

void CherryPickFastCheckpointer::defineSchema(simdb::Schema& schema)
{
    using dt = simdb::SqlDataType;

    auto& windows = schema.addTable("ChkptWindows");
    windows.addColumn("WindowBytes", dt::blob_t);
    windows.addColumn("StartArchID", dt::uint64_t);
    windows.addColumn("EndArchID", dt::uint64_t);
    windows.addColumn("StartTick", dt::uint64_t);
    windows.addColumn("EndTick", dt::uint64_t);
    windows.createCompoundIndexOn({"StartArchID", "EndArchID", "StartTick", "EndTick"});
    windows.unsetPrimaryKey();

    auto& tick_runs = schema.addTable("TickRuns");
    tick_runs.addColumn("StartArchID", dt::uint64_t);
    tick_runs.addColumn("EndArchID", dt::uint64_t);
    tick_runs.addColumn("Tick", dt::uint64_t);
}

/// Process checkpoint windows on one thread
class ProcessStage : public simdb::pipeline::Stage
{
public:
    ProcessStage()
    {
        addInPort_<ChkptWindow>("input_window", input_queue_);
        addOutPort_<ChkptWindowBytes>("output_window_bytes", output_queue_);
    }

    using ChkptWindow = CherryPickFastCheckpointer::ChkptWindow;
    using ChkptWindowBytes = CherryPickFastCheckpointer::ChkptWindowBytes;

private:
    simdb::pipeline::PipelineAction run_(bool) override
    {
        ChkptWindow window_in;
        if (!input_queue_->try_pop(window_in)) {
            return simdb::pipeline::PipelineAction::SLEEP;
        }

        ChkptWindowBytes bytes_out;
        boost::iostreams::back_insert_device<std::vector<char>> inserter(bytes_out.chkpt_bytes);
        boost::iostreams::stream<boost::iostreams::back_insert_device<std::vector<char>>> os(inserter);
        boost::archive::binary_oarchive oa(os);
        oa << window_in;
        os.flush();

        bytes_out.start_arch_id = window_in.start_arch_id;
        bytes_out.end_arch_id = window_in.end_arch_id;
        bytes_out.start_tick = window_in.start_tick;
        bytes_out.end_tick = window_in.end_tick;

        // Silence the warning from the DeltaCheckpoint destructor
        for (auto & chkpt : window_in.checkpoints) {
            chkpt->flagDeleted();
        }

        std::vector<char> compressed_bytes;
        simdb::compressData(bytes_out.chkpt_bytes, compressed_bytes);
        std::swap(bytes_out.chkpt_bytes, compressed_bytes);
        output_queue_->emplace(std::move(bytes_out));

        return simdb::pipeline::PipelineAction::PROCEED;
    }

    simdb::ConcurrentQueue<ChkptWindow>* input_queue_ = nullptr;
    simdb::ConcurrentQueue<ChkptWindowBytes>* output_queue_ = nullptr;
};

/// Write to SQLite on dedicated database thread
class DatabaseStage : public simdb::pipeline::DatabaseStage<CherryPickFastCheckpointer>
{
public:
    DatabaseStage()
    {
        addInPort_<ChkptWindowBytes>("input_window_bytes", input_window_bytes_queue_);
        addInPort_<ArchIdsForTick>("input_arch_ids_for_tick", input_arch_ids_for_tick_queue_);
    }

    using ChkptWindowBytes = CherryPickFastCheckpointer::ChkptWindowBytes;
    using ArchIdsForTick = CherryPickFastCheckpointer::ArchIdsForTick;

private:
    simdb::pipeline::PipelineAction run_(bool) override
    {
        auto action = simdb::pipeline::PipelineAction::SLEEP;

        ChkptWindowBytes bytes_in;
        if (input_window_bytes_queue_->try_pop(bytes_in)) {
            auto window_inserter = getTableInserter_("ChkptWindows");
            window_inserter->setColumnValue(0, bytes_in.chkpt_bytes);
            window_inserter->setColumnValue(1, bytes_in.start_arch_id);
            window_inserter->setColumnValue(2, bytes_in.end_arch_id);
            window_inserter->setColumnValue(3, bytes_in.start_tick);
            window_inserter->setColumnValue(4, bytes_in.end_tick);
            window_inserter->createRecord();
            action = simdb::pipeline::PipelineAction::PROCEED;
        }

        ArchIdsForTick arch_ids_for_tick_in;
        if (input_arch_ids_for_tick_queue_->try_pop(arch_ids_for_tick_in)) {
            auto tick_inserter = getTableInserter_("TickRuns");
            tick_inserter->setColumnValue(0, arch_ids_for_tick_in.start_arch_id);
            tick_inserter->setColumnValue(1, arch_ids_for_tick_in.end_arch_id);
            tick_inserter->setColumnValue(2, arch_ids_for_tick_in.tick);
            tick_inserter->createRecord();
            action = simdb::pipeline::PipelineAction::PROCEED;
        }

        return action;
    }

    simdb::ConcurrentQueue<ChkptWindowBytes>* input_window_bytes_queue_ = nullptr;
    simdb::ConcurrentQueue<ArchIdsForTick>* input_arch_ids_for_tick_queue_ = nullptr;
};

void CherryPickFastCheckpointer::createPipeline(simdb::pipeline::PipelineManager* pipeline_mgr)
{
    auto pipeline = pipeline_mgr->createPipeline(NAME, this);

    pipeline->addStage<ProcessStage>("process_events");
    pipeline->addStage<DatabaseStage>("write_events");
    pipeline->noMoreStages();

    pipeline->bind("process_events.output_window_bytes", "write_events.input_window_bytes");
    pipeline->noMoreBindings();

    // Store the pipeline ChkptWindow input queue (ProcessStage->DatabaseStage)
    chkpt_window_head_ = pipeline->getInPortQueue<ChkptWindow>("process_events.input_window");

    // Store the pipeline ArchIdsForTick input queue (directly to DatabaseStage)
    tick_runs_head_ = pipeline->getInPortQueue<ArchIdsForTick>("write_events.input_arch_ids_for_tick");

    // Create a flusher to flush the pipeline on demand
    pipeline_flusher_ = pipeline->createFlusher({"process_events", "write_events"});
}

void CherryPickFastCheckpointer::commitCurrentBranch(
    bool force_new_head_chkpt, std::vector<chkpt_id_t>* committed_checkpoints)
{
    committed_checkpoints_.clear();
    checkpointer_.squashCurrentBranch(*this, force_new_head_chkpt);
    if (committed_checkpoints) {
        *committed_checkpoints = committed_checkpoints_;
    }
}

void CherryPickFastCheckpointer::saveCheckpoints(checkpoint_ptrs&& checkpoints)
{
    sparta_assert(!checkpoints.empty());
    sparta_assert(checkpoints.front()->isSnapshot());
    sparta_assert(committed_checkpoints_.empty());

    ChkptWindow window;
    window.start_tick = ~0ull;
    window.end_tick = 0ull;

    for (auto& chkpt : checkpoints) {
        auto tick = std::min(window.start_tick, chkpt->getTick());
        window.start_tick = tick;

        tick = std::max(window.end_tick, chkpt->getTick());
        window.end_tick = tick;
    }

    window.start_arch_id = committed_arch_id_;
    window.end_arch_id = committed_arch_id_ + checkpoints.size() - 1;
    sparta_assert(window.end_arch_id >= window.start_arch_id);
    committed_arch_id_ += checkpoints.size();

    arch_id_t chkpt_arch_id = window.start_arch_id;
    for (const auto& chk : checkpoints) {
        const tick_t tick = chk->getTick();
        if (tick_runs_.empty()) {
            // First ever entry
            tick_runs_.push({chkpt_arch_id, chkpt_arch_id, tick});
            continue;
        }

        ArchIdsForTick& last = tick_runs_.back();

        if (last.tick == tick) {
            // Same tick → extend current run
            last.end_arch_id = chkpt_arch_id;
        } else {
            // Different tick → start new run
            tick_runs_.push({chkpt_arch_id, chkpt_arch_id, tick});
        }

        ++chkpt_arch_id;
    }

    for (const auto& chk : checkpoints) {
        committed_checkpoints_.push_back(chk->getID());
    }

    // Send all but the latest tick run. More checkpoints might come in
    // at the same tick as the latest tick run, and if we send the last
    // tick run to the DatabaseStage right now, we might end up with
    // more sqlite records than necessary. The tick_runs_ will be flushed
    // in preTeardown().
    //
    // TODO cnyce: Optimize this further with another assumption that
    // checkpoints may be taken at regular tick intervals. This is only
    // really optimized for use cases where the ticks don't advance much.
    while (tick_runs_.size() > 1) {
        auto tick_run = tick_runs_.front();
        tick_runs_.pop();
        tick_runs_head_->emplace(std::move(tick_run));
    }

    window.checkpoints = std::move(checkpoints);
    chkpt_window_head_->emplace(std::move(window));
}

void CherryPickFastCheckpointer::preTeardown()
{
    while (!tick_runs_.empty()) {
        auto tick_run = tick_runs_.front();
        tick_runs_.pop();
        tick_runs_head_->emplace(std::move(tick_run));
    }
}

CherryPickFastCheckpointer::DatabaseCheckpointLoader::DatabaseCheckpointLoader(
    simdb::DatabaseManager* db_mgr, const std::vector<TreeNode*>& roots) :
    db_mgr_(db_mgr),
    adatas_(enumerateArchDatas_(roots))
{
    auto query = db_mgr->createQuery("TickRuns");

    ArchIdsForTick tick_run;
    query->select("StartArchID", tick_run.start_arch_id);
    query->select("EndArchID", tick_run.end_arch_id);
    query->select("Tick", tick_run.tick);

    auto results = query->getResultSet();
    while (results.getNextRecord()) {
        tick_runs_.push_back(tick_run);
    }

    sparta_assert(!tick_runs_.empty());
    for (const auto& tick_run : tick_runs_) {
        num_checkpoints_ += tick_run.end_arch_id - tick_run.start_arch_id + 1;
    }
}

std::vector<ArchData*> CherryPickFastCheckpointer::DatabaseCheckpointLoader::enumerateArchDatas_(
    const std::vector<TreeNode*>& roots)
{
    std::vector<ArchData*> adatas;
    std::map<ArchData*, TreeNode*> adatas_helper;
    std::function<void(TreeNode*)> recurseAddArchData;
    recurseAddArchData = [&](TreeNode* n)
    {
        assert(n);
        auto assoc_adatas = n->getAssociatedArchDatas();
        for (auto ad : assoc_adatas) {
            if (ad != nullptr) {
                auto itr = adatas_helper.find(ad);
                if (itr != adatas_helper.end()) {
                    throw SpartaException("Found a second reference to ArchData ")
                        << ad << ". First reference found through " << itr->second->getLocation()
                        << " and second found through " << n->getLocation() << ". An ArchData"
                        << " should be findable through exactly 1 TreeNode";
                }
                adatas.push_back(ad);
                adatas_helper[ad] = n;
            }
        }

        for (auto child : TreeNodePrivateAttorney::getAllChildren(n)) {
            recurseAddArchData(child);
        }
    };

    for (auto r : roots) {
        recurseAddArchData(r);
    }

    return adatas;
}

checkpoint_type* CherryPickFastCheckpointer::DatabaseCheckpointLoader::getChkptFromWindow_(
    chkpt_id_t chkpt_id, tick_t tick, ChkptWindow& window)
{
    // To boost performance for use cases where the scheduler is not advanced, we can
    // leverage the fact that checkpoint IDs are monotonically increasing with no gaps.
    // If the ticks don't change, we'll do the lookup in O(1).
    if (chkpt_id >= window.start_arch_id) {
        auto chkpt_idx = chkpt_id - window.start_arch_id;
        if (chkpt_idx < window.checkpoints.size()) {
            auto& chkpt = window.checkpoints[chkpt_idx];
            if (chkpt->getTick() == tick) {
                return chkpt.get();
            }
        }
    }

    // Default to O(n) lookup.
    for (auto& chkpt : window.checkpoints) {
        if (chkpt_id == chkpt->getID() && tick == chkpt->getTick()) {
            return chkpt.get();
        }
    }

    // Not found.
    return nullptr;
}

checkpoint_type* CherryPickFastCheckpointer::DatabaseCheckpointLoader::getChkptFromCache_(
    chkpt_id_t chkpt_id, tick_t tick)
{
    return cached_window_ ? getChkptFromWindow_(chkpt_id, tick, *cached_window_) : nullptr;
}

checkpoint_type* CherryPickFastCheckpointer::DatabaseCheckpointLoader::getChkptFromDisk_(
    chkpt_id_t chkpt_id, tick_t tick)
{
    auto query = db_mgr_->createQuery("ChkptWindows");
    query->addConstraintForUInt64("StartArchID", simdb::Constraints::LESS_EQUAL, chkpt_id);
    query->addConstraintForUInt64("EndArchID", simdb::Constraints::GREATER_EQUAL, chkpt_id);
    query->addConstraintForUInt64("StartTick", simdb::Constraints::LESS_EQUAL, tick);
    query->addConstraintForUInt64("EndTick", simdb::Constraints::GREATER_EQUAL, tick);

    std::vector<char> compressed_window_bytes;
    query->select("WindowBytes", compressed_window_bytes);

    auto results = query->getResultSet();
    if (results.getNextRecord()) {
        // Should only be one window
        sparta_assert(!results.getNextRecord());

        // Undo zlib
        std::vector<char> uncompressed_window_bytes;
        simdb::decompressData(compressed_window_bytes, uncompressed_window_bytes);

        // Undo boost::serialization
        namespace bio = boost::iostreams;
        bio::array_source src(uncompressed_window_bytes.data(), uncompressed_window_bytes.size());
        bio::stream<bio::array_source> is(src);
        boost::archive::binary_iarchive ia(is);

        auto checkpoint_window = std::make_unique<ChkptWindow>();
        ia >> *checkpoint_window;

        checkpoint_type* exact_checkpoint = getChkptFromWindow_(chkpt_id, tick, *checkpoint_window);
        sparta_assert(exact_checkpoint); // Must succeed since we constrained the query

        // Assign prev/next checkpoints
        for (size_t i = 1; i < checkpoint_window->checkpoints.size(); ++i) {
            auto prev = checkpoint_window->checkpoints[i-1].get();
            auto next = checkpoint_window->checkpoints[i].get();
            next->setPrev(prev);
        }
        for (size_t i = 0; i < checkpoint_window->checkpoints.size() - 1; ++i) {
            auto prev = checkpoint_window->checkpoints[i].get();
            auto next = checkpoint_window->checkpoints[i+1].get();
            prev->addNext(next);
        }

        // Silence the warning from the DeltaCheckpoint destructor
        for (auto & chkpt : checkpoint_window->checkpoints) {
            chkpt->flagDeleted();
        }

        // Cache it and return the checkpoint
        std::swap(checkpoint_window, cached_window_);
        return exact_checkpoint;
    }

    return nullptr;
}

void CherryPickFastCheckpointer::DatabaseCheckpointLoader::loadCheckpoint_(
    uint64_t checkpoint_idx)
{
    // Find the arch ID and tick for this index
    utils::ValidValue<arch_id_t> arch_id;
    utils::ValidValue<tick_t> tick;

    uint64_t current_idx = 0;
    for (const auto& tick_run : tick_runs_) {
        if (checkpoint_idx < current_idx + (tick_run.end_arch_id - tick_run.start_arch_id + 1)) {
            arch_id = tick_run.start_arch_id + (checkpoint_idx - current_idx);
            tick = tick_run.tick;
            break;
        }
        current_idx += tick_run.end_arch_id - tick_run.start_arch_id + 1;
    }

    sparta_assert(arch_id.isValid() && tick.isValid(),
                  "Checkpoint index " << checkpoint_idx << " is out of range");

    checkpoint_type* chkpt = getChkptFromCache_(arch_id, tick);
    if (!chkpt) {
        chkpt = getChkptFromDisk_(arch_id, tick);
    }

    if (!chkpt) {
        throw SpartaException("Could not find checkpoint with arch ID ")
            << arch_id << " at tick " << tick << " (checkpoint index "
            << checkpoint_idx << ") in database " << db_mgr_->getDatabaseFilePath();
    }

    chkpt->load(adatas_);
}

bool CherryPickFastCheckpointer::DatabaseCheckpointReplayer::step()
{
    if (next_checkpoint_idx_ >= chkpt_loader_.getNumCheckpoints()) {
        return false; // Done replaying all checkpoints
    }

    chkpt_loader_.loadCheckpoint(next_checkpoint_idx_);
    ++next_checkpoint_idx_;
    return true;
}

size_t CherryPickFastCheckpointer::getNumCheckpoints() const
{
    pipeline_flusher_->flush();

    auto query = db_mgr_->createQuery("ChkptWindows");

    uint64_t start_arch_id, end_arch_id;
    query->select("StartArchID", start_arch_id);
    query->select("EndArchID", end_arch_id);

    auto results = query->getResultSet();
    uint64_t count = 0;
    while (results.getNextRecord()) {
        count += end_arch_id - start_arch_id + 1;
    }

    return count;
}

std::string CherryPickFastCheckpointer::stringize() const
{
    std::stringstream ss;
    ss << "<CherryPickFastCheckpointer on ";
    for (size_t i = 0; i < checkpointer_.getRoots().size(); ++i) {
        TreeNode* root = checkpointer_.getRoots()[i];
        if (i != 0) {
            ss << ", ";
        }
        ss << root->getLocation();
    }
    ss << '>';
    return ss.str();
}

} // namespace sparta::serialization::checkpoint
