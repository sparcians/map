#include "sparta/serialization/checkpoint/CherryPickFastCheckpointer.hpp"
#include "simdb/apps/AppRegistration.hpp"
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

CherryPickFastCheckpointer::CherryPickFastCheckpointer(simdb::DatabaseManager* db_mgr,
                                                   const std::vector<TreeNode*> & roots,
                                                   Scheduler* sched)
    : checkpointer_(roots, sched),
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
    windows.addColumn("NumCheckpoints", dt::int32_t);
    windows.createCompoundIndexOn({"StartArchID", "EndArchID", "StartTick", "EndTick"});
    windows.disableAutoIncPrimaryKey();
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

        window_in.start_arch_id = arch_id_;;
        window_in.end_arch_id = arch_id_ + window_in.checkpoints.size() - 1;
        sparta_assert(window_in.end_arch_id >= window_in.start_arch_id);
        arch_id_ += window_in.checkpoints.size();

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
        bytes_out.num_chkpts = window_in.checkpoints.size();

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
    uint64_t arch_id_ = 0;
};

/// Write to SQLite on dedicated database thread
class DatabaseStage : public simdb::pipeline::DatabaseStage<CherryPickFastCheckpointer>
{
public:
    DatabaseStage()
    {
        addInPort_<ChkptWindowBytes>("input_window_bytes", input_queue_);
    }

    using ChkptWindowBytes = CherryPickFastCheckpointer::ChkptWindowBytes;

private:
    simdb::pipeline::PipelineAction run_(bool) override
    {
        ChkptWindowBytes bytes_in;
        if (input_queue_->try_pop(bytes_in)) {
            auto window_inserter = getTableInserter_("ChkptWindows");
            window_inserter->setColumnValue(0, bytes_in.chkpt_bytes);
            window_inserter->setColumnValue(1, bytes_in.start_arch_id);
            window_inserter->setColumnValue(2, bytes_in.end_arch_id);
            window_inserter->setColumnValue(3, bytes_in.start_tick);
            window_inserter->setColumnValue(4, bytes_in.end_tick);
            window_inserter->setColumnValue(5, (int)bytes_in.num_chkpts);
            window_inserter->createRecord();
            return simdb::pipeline::PipelineAction::PROCEED;
        }

        return simdb::pipeline::PipelineAction::SLEEP;
    }

    simdb::ConcurrentQueue<ChkptWindowBytes>* input_queue_ = nullptr;
};

void CherryPickFastCheckpointer::createPipeline(simdb::pipeline::PipelineManager* pipeline_mgr)
{
    auto pipeline = pipeline_mgr->createPipeline(NAME, this);

    pipeline->addStage<ProcessStage>("process_events");
    pipeline->addStage<DatabaseStage>("write_events");
    pipeline->noMoreStages();

    pipeline->bind("process_events.output_window_bytes", "write_events.input_window_bytes");
    pipeline->noMoreBindings();

    // Store the pipeline input queue
    pipeline_head_ = pipeline->getInPortQueue<ChkptWindow>("process_events.input_window");

    // Create a flusher to flush the pipeline on demand
    pipeline_flusher_ = pipeline->createFlusher({"process_events", "write_events"});
}

void CherryPickFastCheckpointer::commitCurrentBranch(bool force_new_head_chkpt)
{
    checkpointer_.squashCurrentBranch(*this, force_new_head_chkpt);
}

void CherryPickFastCheckpointer::saveCheckpoints(checkpoint_ptrs&& checkpoints)
{
    sparta_assert(!checkpoints.empty());
    sparta_assert(checkpoints.front()->isSnapshot());

    ChkptWindow window;
    window.start_tick = ~0ull;
    window.end_tick = 0ull;

    for (auto& chkpt : checkpoints) {
        auto tick = std::min(window.start_tick, chkpt->getTick());
        window.start_tick = tick;

        tick = std::max(window.end_tick, chkpt->getTick());
        window.end_tick = tick;
    }

    window.checkpoints = std::move(checkpoints);
    pipeline_head_->emplace(std::move(window));
}

size_t CherryPickFastCheckpointer::getNumCheckpoints() const
{
    pipeline_flusher_->flush();

    auto query = db_mgr_->createQuery("ChkptWindows");

    int count = 0;
    query->select("SUM(NumCheckpoints)", count);

    auto results = query->getResultSet();
    results.getNextRecord();
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

REGISTER_SIMDB_APPLICATION(CherryPickFastCheckpointer);

} // namespace sparta::serialization::checkpoint
