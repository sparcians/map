#include "sparta/serialization/checkpoint/ScalableFastCheckpointer.hpp"
#include "simdb/apps/AppRegistration.hpp"
#include "simdb/schema/SchemaDef.hpp"
#include "simdb/pipeline/AsyncDatabaseAccessor.hpp"
#include "simdb/pipeline/elements/Function.hpp"
#include "simdb/utils/Compress.hpp"
#include "simdb/utils/TickTock.hpp"

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>

namespace sparta::serialization::checkpoint
{

ScalableFastCheckpointer::ScalableFastCheckpointer(simdb::DatabaseManager* db_mgr,
                                                   const std::vector<TreeNode*> & roots,
                                                   Scheduler* sched)
    : checkpointer_(std::make_unique<FastCheckpointer>(roots, sched)),
      db_mgr_(db_mgr)
{
}

void ScalableFastCheckpointer::defineSchema(simdb::Schema& schema)
{
    using dt = simdb::SqlDataType;

    auto& windows = schema.addTable("ChkptWindows");
    windows.addColumn("WindowBytes", dt::blob_t);
    windows.addColumn("StartArchID", dt::uint64_t);
    windows.addColumn("EndArchID", dt::uint64_t);
    windows.addColumn("StartTick", dt::uint64_t);
    windows.addColumn("EndTick", dt::uint64_t);
    windows.addColumn("NumCheckpoints", dt::int32_t);
    windows.createCompoundIndexOn({"StartArchID", "EndArchID"});
    windows.createCompoundIndexOn({"StartTick", "EndTick"});
    windows.disableAutoIncPrimaryKey();
}

void ScalableFastCheckpointer::createPipeline(simdb::pipeline::PipelineManager* pipeline_mgr)
{
    auto pipeline = pipeline_mgr->createPipeline(NAME);
    auto db_accessor = pipeline_mgr->getAsyncDatabaseAccessor();

    // Task 1: Give an auto-incrementing arch id to each incoming checkpoint window
    auto add_arch_ids = simdb::pipeline::createTask<simdb::pipeline::Function<ChkptWindow, ChkptWindow>>(
        [arch_id = uint64_t(0)]
        (ChkptWindow&& window_in,
         simdb::ConcurrentQueue<ChkptWindow>& window_out,
         bool /*force_flush*/) mutable
         {
             window_in.start_arch_id = arch_id;
             window_in.end_arch_id = arch_id + window_in.checkpoints.size() - 1;
             sparta_assert(window_in.end_arch_id >= window_in.start_arch_id);
             arch_id += window_in.checkpoints.size();
             window_out.emplace(std::move(window_in));
             return simdb::pipeline::RunnableOutcome::DID_WORK;
         }
    );

    // Task 2: Serialize a checkpoint window into a char buffer
    auto window_to_bytes = simdb::pipeline::createTask<simdb::pipeline::Function<ChkptWindow, ChkptWindowBytes>>(
        [](ChkptWindow&& window,
           simdb::ConcurrentQueue<ChkptWindowBytes>& window_bytes,
           bool /*force_flush*/)
        {
            ChkptWindowBytes bytes;
            boost::iostreams::back_insert_device<std::vector<char>> inserter(bytes.chkpt_bytes);
            boost::iostreams::stream<boost::iostreams::back_insert_device<std::vector<char>>> os(inserter);
            boost::archive::binary_oarchive oa(os);
            oa << window;
            os.flush();

            bytes.start_arch_id = window.start_arch_id;
            bytes.end_arch_id = window.end_arch_id;
            bytes.start_tick = window.start_tick;
            bytes.end_tick = window.end_tick;
            bytes.num_chkpts = window.checkpoints.size();
            window_bytes.emplace(std::move(bytes));

            // Silence the warning from the DeltaCheckpoint destructor
            for (auto & chkpt : window.checkpoints) {
                chkpt->flagDeleted();
            }

            return simdb::pipeline::RunnableOutcome::DID_WORK;
        }
    );

    // Task 3: Perform zlib compression on the checkpoint window bytes
    auto zlib_bytes = simdb::pipeline::createTask<simdb::pipeline::Function<ChkptWindowBytes, ChkptWindowBytes>>(
        [](ChkptWindowBytes&& bytes_in,
           simdb::ConcurrentQueue<ChkptWindowBytes>& bytes_out,
           bool /*force_flush*/)
        {
            std::vector<char> compressed_bytes;
            simdb::compressData(bytes_in.chkpt_bytes, compressed_bytes);
            std::swap(bytes_in.chkpt_bytes, compressed_bytes);
            bytes_out.emplace(std::move(bytes_in));

            return simdb::pipeline::RunnableOutcome::DID_WORK;
        }
    );

    // Task 4: Write to the database
    auto write_to_db = db_accessor->createAsyncWriter<ScalableFastCheckpointer, ChkptWindowBytes, void>(
        [this](ChkptWindowBytes&& bytes_in,
               simdb::pipeline::AppPreparedINSERTs* tables,
               bool /*force_flush*/)
        {
            auto window_inserter = tables->getPreparedINSERT("ChkptWindows");
            window_inserter->setColumnValue(0, bytes_in.chkpt_bytes);
            window_inserter->setColumnValue(1, bytes_in.start_arch_id);
            window_inserter->setColumnValue(2, bytes_in.end_arch_id);
            window_inserter->setColumnValue(3, bytes_in.start_tick);
            window_inserter->setColumnValue(4, bytes_in.end_tick);
            window_inserter->setColumnValue(5, (int)bytes_in.num_chkpts);
            window_inserter->createRecord();
            return simdb::pipeline::RunnableOutcome::DID_WORK;
        }
    );

    // Connect the pipeline tasks
    *add_arch_ids >> *window_to_bytes >> *zlib_bytes >> *write_to_db;

    // Store the pipeline input queue
    pipeline_head_ = add_arch_ids->getTypedInputQueue<ChkptWindow>();

    pipeline_flusher_ = std::make_unique<simdb::pipeline::RunnableFlusher>(
        *db_mgr_, add_arch_ids, window_to_bytes, zlib_bytes, write_to_db);

    pipeline->createTaskGroup("CheckpointPipeline")
        ->addTask(std::move(add_arch_ids))
        ->addTask(std::move(window_to_bytes))
        ->addTask(std::move(zlib_bytes));
}

void ScalableFastCheckpointer::preTeardown()
{
    // Warn if there are remaining uncommitted checkpoints on the current branch.
    if (auto current = checkpointer_->getCurrentID(); current != checkpoint_type::UNIDENTIFIED_CHECKPOINT) {
        auto chkpt_chain = checkpointer_->getCheckpointChain(current);
        if (!chkpt_chain.empty()) {
            std::cout << "WARNING: " << chkpt_chain.size() << " uncommitted checkpoints remain at "
                      << "end of simulation" << std::endl;
        }
    }
}

void ScalableFastCheckpointer::commitCurrentBranch(bool force_new_head_chkpt)
{
    checkpointer_->squashCurrentBranch(*this, force_new_head_chkpt);
}

void ScalableFastCheckpointer::saveCheckpoints(checkpoint_ptrs&& checkpoints)
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

size_t ScalableFastCheckpointer::getNumCheckpoints() const
{
    pipeline_flusher_->waterfallFlush();

    auto query = db_mgr_->createQuery("ChkptWindows");

    int count = 0;
    query->select("SUM(NumCheckpoints)", count);

    auto results = query->getResultSet();
    results.getNextRecord();
    return count;
}

std::string ScalableFastCheckpointer::stringize() const
{
    std::stringstream ss;
    ss << "<ScalableFastCheckpointer on ";
    for (size_t i = 0; i < checkpointer_->getRoots().size(); ++i) {
        TreeNode* root = checkpointer_->getRoots()[i];
        if (i != 0) {
            ss << ", ";
        }
        ss << root->getLocation();
    }
    ss << '>';
    return ss.str();
}

REGISTER_SIMDB_APPLICATION(ScalableFastCheckpointer);

} // namespace sparta::serialization::checkpoint
