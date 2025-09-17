// <DatabaseCheckpointer> -*- C++ -*-

#include "sparta/serialization/checkpoint/DatabaseCheckpointer.hpp"
#include "sparta/serialization/checkpoint/DatabaseCheckpointQuery.hpp"
#include "simdb/apps/AppRegistration.hpp"
#include "simdb/schema/SchemaDef.hpp"
#include "simdb/pipeline/AsyncDatabaseAccessor.hpp"
#include "simdb/pipeline/Pipeline.hpp"
#include "simdb/pipeline/elements/Function.hpp"
#include "simdb/pipeline/elements/Buffer.hpp"
#include "simdb/utils/Compress.hpp"

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>

namespace sparta::serialization::checkpoint
{

using tick_t = typename CheckpointBase::tick_t;
using chkpt_id_t = typename CheckpointBase::chkpt_id_t;
using checkpoint_type = DatabaseCheckpoint;
using checkpoint_ptr = std::shared_ptr<DatabaseCheckpoint>;
using checkpoint_ptrs = std::vector<checkpoint_ptr>;
using EvictedChkptIDs = std::vector<chkpt_id_t>;

DatabaseCheckpointer::DatabaseCheckpointer(simdb::DatabaseManager* db_mgr, TreeNode& root, Scheduler* sched) :
    Checkpointer(root, sched),
    chkpt_query_(std::make_shared<DatabaseCheckpointQuery>(this, db_mgr, root, sched)),
    db_mgr_(db_mgr),
    snap_thresh_(DEFAULT_SNAPSHOT_THRESH),
    next_chkpt_id_(checkpoint_type::MIN_CHECKPOINT),
    num_alive_checkpoints_(0),
    num_alive_snapshots_(0),
    num_dead_checkpoints_(0)
{
}

void DatabaseCheckpointer::defineSchema(simdb::Schema& schema)
{
    using dt = simdb::SqlDataType;

    auto& window_bytes = schema.addTable("ChkptWindowBytes");
    window_bytes.addColumn("WindowBytes", dt::blob_t);

    auto& window_ids = schema.addTable("ChkptWindowIDs");
    window_ids.addColumn("ChkptWindowBytesID", dt::int32_t);
    window_ids.addColumn("ChkptID", dt::int32_t);
    window_ids.createIndexOn("ChkptID");
    //window_ids.disableAutoIncPrimaryKey();

    auto& window_ticks = schema.addTable("ChkptWindowTicks");
    window_ticks.addColumn("ChkptWindowBytesID", dt::int32_t);
    window_ticks.addColumn("StartTick", dt::int32_t);
    window_ticks.addColumn("EndTick", dt::int32_t);
    window_ticks.createCompoundIndexOn({"StartTick", "EndTick"});
    //window_ticks.disableAutoIncPrimaryKey();
}

std::unique_ptr<simdb::pipeline::Pipeline> DatabaseCheckpointer::createPipeline(
    simdb::pipeline::AsyncDatabaseAccessor* db_accessor)
{
    auto pipeline = std::make_unique<simdb::pipeline::Pipeline>(db_mgr_, NAME);

    // Task 1: Clone an entire checkpoint window (snapshot plus all deltas until next snapshot)
    auto clone_window = simdb::pipeline::createTask<simdb::pipeline::Function<void, checkpoint_ptrs>>(
        [this](simdb::ConcurrentQueue<checkpoint_ptrs>& out, bool simulation_terminating) mutable -> bool
        {
            std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

            bool sent = false;

            auto send_window = [&]() {
                auto window = std::move(chkpt_windows_.front());
                chkpt_windows_.pop_front();

                checkpoint_ptrs chkpts;
                for (auto id : window) {
                    auto it = chkpts_cache_.find(id);
                    if (it == chkpts_cache_.end()) {
                        throw CheckpointError("Invalid checkpoint - has been deleted");
                    }
                    const auto& c = it->second;
                    if (chkpts.empty() && !c->isSnapshot()) {
                        throw CheckpointError("Invalid checkpoint - first in window is not a snapshot");
                    } else if (!chkpts.empty() && c->isSnapshot()) {
                        throw CheckpointError("Invalid checkpoint - only one snapshot per window");
                    }

                    chkpts.emplace_back(c->clone());
                }

                if (!chkpts.empty()) {
                    out.emplace(std::move(chkpts));
                    sent = true;
                }
            };

            // Note the >2 is to ensure we always have at least one complete window
            // in the cache for fast APIs on very recent checkpoints. The second
            // window may be partial so we can't send it yet.
            while (chkpt_windows_.size() > 2) {
                send_window();
            }

            // If we are terminating, send all remaining windows.
            while (!chkpt_windows_.empty() && simulation_terminating) {
                send_window();
            }

            return sent;
        }
    );

    // Task 2: Add the IDs of all checkpoints in this window
    auto add_chkpt_ids = simdb::pipeline::createTask<simdb::pipeline::Function<checkpoint_ptrs, ChkptWindow>>(
        [](checkpoint_ptrs&& chkpts,
           simdb::ConcurrentQueue<ChkptWindow>& windows,
           bool /*simulation_terminating*/)
        {
            uint64_t start_tick = std::numeric_limits<uint64_t>::max();
            uint64_t end_tick = 0;

            ChkptWindow window;
            window.chkpts = std::move(chkpts);
            for (auto& chkpt : window.chkpts) {
                window.chkpt_ids.push_back(chkpt->getID());
                start_tick = std::min(start_tick, chkpt->getTick());
                end_tick = std::max(end_tick, chkpt->getTick());
            }
            window.start_tick = start_tick;
            window.end_tick = end_tick;
            windows.emplace(std::move(window));
        }
    );

    // Task 3: Serialize a checkpoint window into a char buffer
    auto window_to_bytes = simdb::pipeline::createTask<simdb::pipeline::Function<ChkptWindow, ChkptWindowBytes>>(
        [](ChkptWindow&& window,
           simdb::ConcurrentQueue<ChkptWindowBytes>& window_bytes,
           bool /*simulation_terminating*/)
        {
            ChkptWindowBytes bytes;
            boost::iostreams::back_insert_device<std::vector<char>> inserter(bytes.chkpt_bytes);
            boost::iostreams::stream<boost::iostreams::back_insert_device<std::vector<char>>> os(inserter);
            boost::archive::binary_oarchive oa(os);
            oa << window;
            os.flush();

            for (const auto& chkpt : window.chkpts) {
                bytes.chkpt_ids.push_back(chkpt->getID());
            }

            bytes.start_tick = window.start_tick;
            bytes.end_tick = window.end_tick;
            window_bytes.emplace(std::move(bytes));
        }
    );

    // Task 4: Perform zlib compression on the checkpoint window bytes
    auto zlib_bytes = simdb::pipeline::createTask<simdb::pipeline::Function<ChkptWindowBytes, ChkptWindowBytes>>(
        [](ChkptWindowBytes&& bytes_in,
           simdb::ConcurrentQueue<ChkptWindowBytes>& bytes_out,
           bool /*simulation_terminating*/)
        {
            ChkptWindowBytes compressed;
            compressed.chkpt_ids = std::move(bytes_in.chkpt_ids);
            simdb::compressData(bytes_in.chkpt_bytes, compressed.chkpt_bytes);
            bytes_out.emplace(std::move(compressed));
        }
    );

    // Task 5: Write to the database
    auto write_to_db = db_accessor->createAsyncWriter<DatabaseCheckpointer, ChkptWindowBytes, EvictedChkptIDs>(
        [](ChkptWindowBytes&& bytes_in,
           simdb::ConcurrentQueue<EvictedChkptIDs>& evicted_ids,
           simdb::pipeline::AppPreparedINSERTs* tables,
           bool /*simulation_terminating*/)
        {
            auto bytes_inserter = tables->getPreparedINSERT("ChkptWindowBytes");
            bytes_inserter->setColumnValue(0, bytes_in.chkpt_bytes);
            auto bytes_id = bytes_inserter->createRecord();

            auto chkpt_ids_inserter = tables->getPreparedINSERT("ChkptWindowIDs");
            chkpt_ids_inserter->setColumnValue(0, bytes_id);
            for (auto id : bytes_in.chkpt_ids) {
                chkpt_ids_inserter->setColumnValue(1, (int)id);
                chkpt_ids_inserter->createRecord();
            }

            auto chkpt_ticks_inserter = tables->getPreparedINSERT("ChkptWindowTicks");
            chkpt_ticks_inserter->setColumnValue(0, bytes_id);
            chkpt_ticks_inserter->setColumnValue(1, (int)bytes_in.start_tick);
            chkpt_ticks_inserter->setColumnValue(2, (int)bytes_in.end_tick);
            chkpt_ticks_inserter->createRecord();

            evicted_ids.emplace(std::move(bytes_in.chkpt_ids));
        }
    );

    // Task 6: Perform cache eviction after a window of checkpoints has been written to SimDB
    auto evict_from_cache = simdb::pipeline::createTask<simdb::pipeline::Function<EvictedChkptIDs, void>>(
        [this](EvictedChkptIDs&& evicted_ids, bool simulation_terminating) mutable
        {
            // TODO cnyce: We are allocating and deallocating a LOT of checkpoints.
            // See if we can reuse a pool of them. Could also try to just add a pool
            // to the VectorStorage::Segment class.
            std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

            for (auto id : evicted_ids) {
                if (id == head_id_) {
                    // Never evict the head checkpoint
                    continue;
                }

                if (auto it = chkpts_cache_.find(id); it != chkpts_cache_.end()) {
                    it->second->flagDecached();
                    if (findCheckpoint(id).lock() != nullptr) {
                        throw CheckpointError("Internal error - checkpoint should be marked as decached");
                    }
                }
            }
        }
    );

    *clone_window >> *add_chkpt_ids >> *window_to_bytes >> *zlib_bytes >> *write_to_db >> *evict_from_cache;

    pipeline_ = pipeline.get();

    pipeline->createTaskGroup("CheckpointPipeline")
        ->addTask(std::move(clone_window))
        ->addTask(std::move(add_chkpt_ids))
        ->addTask(std::move(window_to_bytes))
        ->addTask(std::move(zlib_bytes))
        ->addTask(std::move(evict_from_cache));

    return pipeline;
}

uint32_t DatabaseCheckpointer::getSnapshotThreshold() const noexcept
{
    return snap_thresh_;
}

void DatabaseCheckpointer::setSnapshotThreshold(uint32_t thresh) noexcept
{
    snap_thresh_ = thresh;
}

uint64_t DatabaseCheckpointer::getTotalMemoryUse() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);
    uint64_t mem = 0;
    for (const auto& [id, chkpt] : chkpts_cache_) {
        mem += chkpt->getTotalMemoryUse();
    }
    return mem;
}

uint64_t DatabaseCheckpointer::getContentMemoryUse() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);
    uint64_t mem = 0;
    for (const auto& [id, chkpt] : chkpts_cache_) {
        mem += chkpt->getContentMemoryUse();
    }
    return mem;
}

void DatabaseCheckpointer::deleteCheckpoint(chkpt_id_t)
{
    throw CheckpointError("Explicit checkpoint deletion is not supported by DatabaseCheckpointer");
}

void DatabaseCheckpointer::loadCheckpoint(chkpt_id_t id)
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    if (auto c = getCurrent_(); !c || (c && c->getID() == id)) {
        return;
    }

    auto chkpt = (id == head_id_) ? findCheckpoint(id).lock() : cloneCheckpoint(id);
    if (!chkpt) {
        throw CheckpointError("There is no checkpoint with ID ") << id;
    }

    chkpt->load(getArchDatas());

    // Delete all future checkpoints past this one. Do this from the cache
    // as well as from the database.
    auto next_ids = chkpt->getNextIDs();
    if (!next_ids.empty()) {
        if (next_ids.size() != 1) {
            throw CheckpointError("DatabaseCheckpointer does not support multiple checkpoint branches");
        }
        deleteCheckpoint_(next_ids[0]);
    }

    // Detach future checkpoints from this one as they are deleted.
    chkpt->next_ids_.clear();

    // Move current to this checkpoint.
    setCurrent_(chkpt.get());

    // Add this checkpoint to the cache if not the head checkpoint.
    // The head checkpoint is always in the cache.
    if (id != head_id_) {
        addToCache_(std::move(chkpt));
    }

    // Increasing-by-one, starting-at-zero checkpoint IDs guarantee we can do this:
    num_alive_checkpoints_ = id + 1;
    next_chkpt_id_ = id + 1;

    // Restore scheduler tick number
    if (sched_) {
        sched_->restartAt(getCurrentTick());
    }
}

std::vector<chkpt_id_t> DatabaseCheckpointer::getCheckpointsAt(tick_t t) const
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    std::unordered_set<chkpt_id_t> results;
    for (const auto& [id, chkpt] : chkpts_cache_) {
        if (chkpt->getTick() == t && !chkpt->isFlaggedDeleted()) {
            results.insert(id);
        }
    }

    for (auto id : chkpt_query_->getCheckpointsAt(t)) {
        results.insert(id);
    }

    std::vector<chkpt_id_t> chkpts(results.begin(), results.end());
    std::sort(chkpts.begin(), chkpts.end());
    return chkpts;
}

std::vector<chkpt_id_t> DatabaseCheckpointer::getCheckpoints() const
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    std::unordered_set<chkpt_id_t> results;
    for (const auto& [id, chkpt] : chkpts_cache_) {
        if (!chkpt->isFlaggedDeleted()) {
            results.insert(id);
        }
    }

    //TODO cnyce: Put this back when the cache is actually purged
    //for (auto id : chkpt_query_->getCheckpoints()) {
    //    results.insert(id);
    //}

    std::vector<chkpt_id_t> chkpts(results.begin(), results.end());
    std::sort(chkpts.begin(), chkpts.end());
    return chkpts;
}

uint32_t DatabaseCheckpointer::getNumCheckpoints() const noexcept
{
    return num_alive_checkpoints_;
}

uint32_t DatabaseCheckpointer::getNumSnapshots() const noexcept
{
    return num_alive_snapshots_;
}

uint32_t DatabaseCheckpointer::getNumDeltas() const noexcept
{
    return getNumCheckpoints() - getNumSnapshots();
}

uint32_t DatabaseCheckpointer::getNumDeadCheckpoints() const noexcept
{
    return num_dead_checkpoints_;
}

std::deque<chkpt_id_t> DatabaseCheckpointer::getCheckpointChain(chkpt_id_t id) const
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    std::deque<chkpt_id_t> chain;
    if (!getHead()) {
        return chain;
    }

    if (!hasCheckpoint(id)) {
        throw CheckpointError("There is no checkpoint with ID ") << id;
    }

    auto it = chkpts_cache_.find(id);
    while (it != chkpts_cache_.end()) {
        chain.push_back(id);
        id = it->second->getPrevID();
        it = chkpts_cache_.find(id);
    }

    while (id != checkpoint_type::UNIDENTIFIED_CHECKPOINT) {
        chain.push_back(id);
        id = chkpt_query_->getPrevID(id);
    }

    return chain;
}

std::shared_ptr<DatabaseCheckpoint> DatabaseCheckpointer::findLatestCheckpointAtOrBefore(tick_t tick, chkpt_id_t from)
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    auto id = from;
    do {
        auto chkpt = cloneCheckpoint(id);
        if (chkpt->getTick() <= tick) {
            break;
        }
        id = chkpt->getPrevID();
    } while (id != checkpoint_type::UNIDENTIFIED_CHECKPOINT);

    return cloneCheckpoint(id);
}

std::weak_ptr<DatabaseCheckpoint> DatabaseCheckpointer::findCheckpoint(chkpt_id_t id) const
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    if (auto it = chkpts_cache_.find(id); it != chkpts_cache_.end()) {
        if (it->second->isFlaggedDeleted() || it->second->isFlaggedDecached()) {
            return std::weak_ptr<DatabaseCheckpoint>();
        }
        return it->second;
    }

    return std::weak_ptr<DatabaseCheckpoint>();
}

std::shared_ptr<DatabaseCheckpoint> DatabaseCheckpointer::cloneCheckpoint(chkpt_id_t id, bool must_exist) const
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    if (auto it = chkpts_cache_.find(id); it != chkpts_cache_.end()) {//TODO cnyce: && !it->second->isFlaggedDecached()) {
        return it->second->clone();
    }

    auto chkpt = chkpt_query_->findCheckpoint(id);
    if (!chkpt && must_exist) {
        throw CheckpointError("There is no checkpoint with ID ") << id;
    } else if (!chkpt) {
        return nullptr;
    }

    chkpt->checkpointer_ = const_cast<DatabaseCheckpointer*>(this);
    return chkpt;
}

bool DatabaseCheckpointer::hasCheckpoint(chkpt_id_t id) const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    if (auto it = chkpts_cache_.find(id); it != chkpts_cache_.end()) {
        return !it->second->isFlaggedDeleted();
    }

    return chkpt_query_->hasCheckpoint(id);
}

void DatabaseCheckpointer::dumpRestoreChain(std::ostream& o, chkpt_id_t id) const
{
    auto rc = getRestoreChain(id);
    while (true) {
        const auto chkpt = cloneCheckpoint(rc.top());
        rc.pop();
        if (chkpt->isSnapshot()) {
            o << '(';
        }
        if (chkpt->getID() == checkpoint_type::UNIDENTIFIED_CHECKPOINT) {
            o << "*" << chkpt->getDeletedID();
        } else {
            o << chkpt->getID();
        }
        if (chkpt->isSnapshot()) {
            o << ')';
        }
        if (rc.empty()) {
            break;
        } else {
            o << " --> ";
        }
    }
}

std::stack<chkpt_id_t> DatabaseCheckpointer::getHistoryChain(chkpt_id_t id) const
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    std::stack<chkpt_id_t> chain;
    auto it = chkpts_cache_.find(id);
    while (it != chkpts_cache_.end()) {
        chain.push(id);
        id = it->second->getPrevID();
        it = chkpts_cache_.find(id);
    }

    while (id != checkpoint_type::UNIDENTIFIED_CHECKPOINT) {
        chain.push(id);
        id = chkpt_query_->getPrevID(id);
    }

    return chain;
}

std::stack<chkpt_id_t> DatabaseCheckpointer::getRestoreChain(chkpt_id_t id) const
{
    // Build stack up to last snapshot
    std::stack<chkpt_id_t> chkpts;
    while (true) {
        chkpts.push(id);
        auto chkpt = cloneCheckpoint(id);
        if (chkpt->isSnapshot()) {
            break;
        }
        id = chkpt->getPrevID();
    }
    return chkpts;
}

std::vector<chkpt_id_t> DatabaseCheckpointer::getNextIDs(chkpt_id_t id) const
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    if (auto it = chkpts_cache_.find(id); it != chkpts_cache_.end()) {
        return it->second->getNextIDs();
    }

    return chkpt_query_->getNextIDs(id);
}

uint32_t DatabaseCheckpointer::getDistanceToPrevSnapshot(chkpt_id_t id) const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    uint32_t dist = 0;
    auto it = chkpts_cache_.find(id);
    while (it != chkpts_cache_.end()) {
        if (it->second->isSnapshot()) {
            return dist;
        }
        id = it->second->getPrevID();
        it = chkpts_cache_.find(id);
        ++dist;
    }

    // Note that we only evict entire checkpoint "windows" from the cache,
    // which means the cache never has "partial" windows like:
    //
    //   Snapshot threshold: 10     (window length)
    //                              (1 snapshot, 9 deltas)
    //
    //   Cache:              DB:
    //   3,4,5,6,7,8,9,10    1,2    <-- never going to happen
    //
    //   Cache:              DB:
    //   21-30               1-20   <-- always like this ("full" windows only)
    //
    // This means we either can answer the API question entirely using the
    // cache or entirely using the DB. That is why the line of code below
    // is not something like:
    //
    //     return dist + chkpt_query_->getDistanceToPrevSnapshot(id);

    return chkpt_query_->getDistanceToPrevSnapshot(id);
}

bool DatabaseCheckpointer::isSnapshot(chkpt_id_t id) const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    if (auto it = chkpts_cache_.find(id); it != chkpts_cache_.end()) {
        return it->second->isSnapshot();
    }

    return chkpt_query_->isSnapshot(id);
}

bool DatabaseCheckpointer::canDelete(chkpt_id_t id) const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    if (auto it = chkpts_cache_.find(id); it != chkpts_cache_.end()) {
        if (!it->second->isFlaggedDeleted()) {
            return false;
        }

        for (auto next_id : it->second->getNextIDs()) {
            if (!canDelete(next_id) && !isSnapshot(next_id)) {
                return false;
            }
        }

        return true;
    }

    return chkpt_query_->canDelete(id);
}

std::string DatabaseCheckpointer::stringize() const
{
    std::stringstream ss;
    ss << "<DatabaseCheckpointer on " << getRoot().getLocation() << '>';
    return ss.str();
}

void DatabaseCheckpointer::dumpList(std::ostream& o) const
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    for (const auto& [id, chkpt] : chkpts_cache_) {
        o << chkpt->stringize() << std::endl;
    }

    chkpt_query_->dumpList(o);
}

void DatabaseCheckpointer::dumpData(std::ostream& o) const
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    for (const auto& [id, chkpt] : chkpts_cache_) {
        chkpt->dumpData(o);
        o << std::endl;
    }

    chkpt_query_->dumpData(o);
}

void DatabaseCheckpointer::dumpAnnotatedData(std::ostream& o) const
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    for (const auto& [id, chkpt] : chkpts_cache_) {
        o << chkpt->stringize() << std::endl;
        chkpt->dumpData(o);
        o << std::endl;
    }

    chkpt_query_->dumpAnnotatedData(o);
}

void DatabaseCheckpointer::traceValue(
    std::ostream& o,
    chkpt_id_t id,
    const ArchData* container,
    uint32_t offset,
    uint32_t size)
{
    (void)o;
    (void)id;
    (void)container;
    (void)offset;
    (void)size;

    sparta_assert(false, "Not implemented");
}

void DatabaseCheckpointer::createHead_()
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    tick_t tick = 0;
    if (sched_) {
        tick = sched_->getCurrentTick();
    }

    if (getHead()) {
        throw CheckpointError("Cannot create head at ")
            << tick << " because a head already exists in this checkpointer";
    }
    if (getRoot().isFinalized() == false) {
        CheckpointError exc("Cannot create a checkpoint until the tree is finalized. Attempting to checkpoint from node ");
        exc << getRoot().getLocation() << " at tick ";
        if (sched_) {
            exc << tick;
        }else{
            exc << "<no scheduler>";
        }
        throw exc;
    }

    std::shared_ptr<checkpoint_type> chkpt(new checkpoint_type(
        getRoot(), getArchDatas(), next_chkpt_id_++, tick,
        nullptr, true, this));

    setHead_(chkpt.get());
    setCurrent_(chkpt.get());
    addToCache_(std::move(chkpt));

    num_alive_checkpoints_++;
    num_alive_snapshots_++;
}

chkpt_id_t DatabaseCheckpointer::createCheckpoint_(bool force_snapshot)
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    bool is_snapshot;
    checkpoint_type* prev;

    if (next_chkpt_id_ == checkpoint_type::UNIDENTIFIED_CHECKPOINT) {
        throw CheckpointError("Exhausted all ")
            << checkpoint_type::UNIDENTIFIED_CHECKPOINT << " possible checkpoint IDs. "
            << "This is likely a gross misuse of checkpointing";
    }

    // Caller guarantees a head
    sparta_assert(getHead() != nullptr);

    tick_t tick;
    if (sched_) {
        tick = sched_->getCurrentTick();
    } else {
        tick = 0;
    }

    if (sched_ && (tick < getHead()->getTick())) {
        throw CheckpointError("Cannot create a new checkpoint at tick ")
            << tick << " because this tick number is smaller than the tick number of the head checkpoint at: "
            << getHead()->getTick() << ". The head checkpoint cannot be reset once created, so it should be done "
            << "at the start of simulation before running. The simulator front-end should do this so this must "
            << "likely be fixed in the simulator.";
    }

    if (nullptr == getCurrent_()) {
        // Creating a delta from the head
        prev = static_cast<checkpoint_type*>(getHead_());
        is_snapshot = false;
    } else {
        if (sched_ && (tick < getCurrent_()->getTick())) {
            throw CheckpointError("Current tick number from sparta scheduler (")
                << tick << " ) is less than the current checkpoint's tick number ("
                << getCurrent_()->getTick() << " To create a checkpoint with an earlier tick number, an "
                << "older checkpoint having a tick number <= the tick number specified here must first be "
                << "loaded";
        }

        // Find latest checkpoint <= tick
        prev = static_cast<checkpoint_type*>(getCurrent_());
        is_snapshot = prev->getDistanceToPrevSnapshot() >= getSnapshotThreshold();
    }

    std::shared_ptr<checkpoint_type> chkpt(new checkpoint_type(
        getRoot(), getArchDatas(), next_chkpt_id_++, tick,
        prev, force_snapshot || is_snapshot, this));

    auto current = chkpt.get();
    setCurrent_(current);
    addToCache_(std::move(chkpt));
    num_alive_checkpoints_++;
    num_alive_snapshots_ += (current->isSnapshot() == true) ? 1 : 0;

    return current->getID();
}

void DatabaseCheckpointer::deleteCheckpoint_(chkpt_id_t id)
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    // Purge all future checkpoints from chkpt_windows_.
    for (auto it = chkpt_windows_.begin(); it != chkpt_windows_.end(); ++it) {
        auto& window = *it;

        // Because IDs are monotonically increasing, we can skip windows
        if (window.empty() || id < window.front()) {
            // ID cannot be in this or any future window
            chkpt_windows_.erase(it, chkpt_windows_.end());
            break;
        }

        if (id > window.back()) {
            // ID cannot be in this window, continue searching
            continue;
        }

        // ID must be within this window
        auto pos = std::find(window.begin(), window.end(), id);
        if (pos != window.end()) {
            window.erase(pos, window.end());
            if (window.empty()) {
                it = chkpt_windows_.erase(it);
            } else {
                ++it;
            }
            if (it != chkpt_windows_.end()) {
                chkpt_windows_.erase(it, chkpt_windows_.end());
            }
            break;
        }
    }

    // Purge from the database
    chkpt_query_->deleteCheckpoint(id);

    // Purge from the cache
    while (true) {
        auto it = chkpts_cache_.find(id);
        if (it == chkpts_cache_.end()) {
            break;
        }

        auto next_ids = it->second->getNextIDs();
        it->second->flagDeleted();

        if (!next_ids.empty()) {
            if (next_ids.size() != 1) {
                throw CheckpointError("DatabaseCheckpointer does not support multiple checkpoint branches");
            }
            id = next_ids[0];
        } else {
            break;
        }
    }
}

void DatabaseCheckpointer::dumpCheckpointNode_(const chkpt_id_t id, std::ostream& o) const
{
    static std::string SNAPSHOT_NOTICE = "(s)";

    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    checkpoint_ptr chkpt_ptr;
    if (auto it = chkpts_cache_.find(id); it != chkpts_cache_.end()) {
        chkpt_ptr = it->second;
    } else {
        chkpt_ptr = chkpt_query_->findCheckpoint(id);
    }

    auto cp = chkpt_ptr.get();

    // Draw data for this checkpoint
    if (cp->isFlaggedDeleted()) {
        o << cp->getDeletedRepr();
    }else{
        o << cp->getID();
    }
    // Show that this is a snapshot
    if (cp->isSnapshot()) {
        o << ' ' << SNAPSHOT_NOTICE;
    }
}

std::vector<chkpt_id_t> DatabaseCheckpointer::getNextIDs_(chkpt_id_t id) const
{
    return getNextIDs(id);
}

void DatabaseCheckpointer::setHead_(CheckpointBase* head)
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);
    setHeadID_(head->getID());
    Checkpointer::setHead_(head);
}

void DatabaseCheckpointer::setCurrent_(CheckpointBase* current)
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);
    setCurrentID_(current->getID());
    Checkpointer::setCurrent_(current);
}

void DatabaseCheckpointer::setHeadID_(chkpt_id_t id)
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);
    sparta_assert(id != checkpoint_type::UNIDENTIFIED_CHECKPOINT);
    sparta_assert(head_id_ == checkpoint_type::UNIDENTIFIED_CHECKPOINT || head_id_ == id);
    head_id_ = id;
}

void DatabaseCheckpointer::setCurrentID_(chkpt_id_t id)
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);
    sparta_assert(id != checkpoint_type::UNIDENTIFIED_CHECKPOINT);
    current_id_ = id;

    // If we are moving current_, see if we can evict any pending IDs
    while (!pending_eviction_ids_.empty()) {
        auto id = pending_eviction_ids_.front();
        pending_eviction_ids_.pop();
        if (id == current_id_) {
            pending_eviction_ids_.push(id);
        } else if (auto it = chkpts_cache_.find(id); it != chkpts_cache_.end()) {
            it->second->flagDecached();
        }
    }
}

void DatabaseCheckpointer::addToCache_(std::shared_ptr<checkpoint_type> chkpt)
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    auto id = chkpt->getID();
    chkpts_cache_[id] = chkpt;

    if (!chkpt_windows_.empty() && !chkpt_windows_.back().empty() && chkpt_windows_.back().back() == id) {
        return;
    }

    if (chkpt->isSnapshot()) {
        chkpt_windows_.emplace_back();
    }

    auto& window = chkpt_windows_.back();
    if (window.empty() || window.back() != id) {
        window.push_back(id);
    }
}

REGISTER_SIMDB_APPLICATION(DatabaseCheckpointer);

} // namespace sparta::serialization::checkpoint
