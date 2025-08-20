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

struct ChkptWindow {
    std::vector<chkpt_id_t> chkpt_ids;
    checkpoint_ptrs chkpts;

    // TODO cnyce: Try to avoid use of unique_ptr. Everything is already movable
    // and has default constructors.
    template <typename Archive>
    void serialize(Archive& ar, const unsigned int /*version*/) {
        ar & chkpt_ids;

        if (chkpts.empty()) {
            // We are loading checkpoint window from disk
            chkpts.reserve(chkpt_ids.size());
            for (size_t i = 0; i < chkpt_ids.size(); ++i) {
                chkpts.emplace_back(new DatabaseCheckpoint);
                ar & *chkpts.back();
            }
        }

        else {
            // We are saving a checkpoint window to disk
            for (auto& chkpt : chkpts) {
                ar & *chkpt;
            }
        }
    }
};

struct ChkptWindowBytes {
    std::vector<chkpt_id_t> chkpt_ids;
    std::vector<char> chkpt_bytes;
};

using EvictedChkptIDs = std::vector<chkpt_id_t>;

DatabaseCheckpointer::DatabaseCheckpointer(simdb::DatabaseManager* db_mgr, TreeNode& root, Scheduler* sched) :
    Checkpointer(root, sched),
    chkpt_query_(std::make_shared<DatabaseCheckpointQuery>(db_mgr, root, sched)),
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
    window_ids.disableAutoIncPrimaryKey();
}

std::unique_ptr<simdb::pipeline::Pipeline> DatabaseCheckpointer::createPipeline(
    simdb::pipeline::AsyncDatabaseAccessor* db_accessor)
{
    auto pipeline = std::make_unique<simdb::pipeline::Pipeline>(db_mgr_, NAME);

    // Task 1: Clone the next checkpoint from the cache to send down pipeline
    auto feed_pipeline = simdb::pipeline::createTask<simdb::pipeline::Function<void, checkpoint_ptr>>(
        [this](simdb::ConcurrentQueue<checkpoint_ptr>& out, bool /*simulation_terminating*/) mutable
        {
            checkpoint_ptr next_chkpt;
            if (cloneNextPipelineHeadCheckpoint_(next_chkpt)) {
                out.emplace(std::move(next_chkpt));
                return true;
            }
            return false;
        }
    );

    // Task 2: Buffer snapshots and their deltas into checkpoint windows
    const auto window_len = getSnapshotThreshold();
    const auto flush_partial = true;
    auto create_window = simdb::pipeline::createTask<simdb::pipeline::Buffer<checkpoint_ptr>>(window_len, flush_partial);

    // Task 3: Add the IDs of all checkpoints in this window
    auto add_chkpt_ids = simdb::pipeline::createTask<simdb::pipeline::Function<checkpoint_ptrs, ChkptWindow>>(
        [](checkpoint_ptrs&& chkpts,
           simdb::ConcurrentQueue<ChkptWindow>& windows,
           bool /*simulation_terminating*/)
        {
            ChkptWindow window;
            window.chkpts = std::move(chkpts);
            for (auto& chkpt : window.chkpts) {
                window.chkpt_ids.push_back(chkpt->getID());
            }
            windows.emplace(std::move(window));
        }
    );

    // Task 4: Serialize a checkpoint window into a char buffer
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

            window_bytes.emplace(std::move(bytes));
        }
    );

    // Task 5: Perform zlib compression on the checkpoint window bytes
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

    // Task 6: Write to the database
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
                chkpt_ids_inserter->setColumnValue(1, id);
                chkpt_ids_inserter->createRecord();
            }

            evicted_ids.emplace(std::move(bytes_in.chkpt_ids));
        }
    );

    // Task 7: Perform cache eviction after a window of checkpoints has been written to SimDB
    auto evict_from_cache = simdb::pipeline::createTask<simdb::pipeline::Function<EvictedChkptIDs, void>>(
        [this](EvictedChkptIDs&& evicted_ids, bool /*simulation_terminating*/) mutable
        {
            for (auto id : evicted_ids) {
                sparta_assert(id != head_id_);
                sparta_assert(id != current_id_);

                // TODO cnyce: We are allocating and deallocating a LOT of checkpoints.
                // See if we can reuse a pool of them. Could also try to just add a pool
                // to the VectorStorage::Segment class.
                std::lock_guard<std::recursive_mutex> lock(mutex_);
                chkpts_cache_.erase(id);
            }
        }
    );

    *feed_pipeline >> *create_window >> *add_chkpt_ids >> *window_to_bytes >> *zlib_bytes >> *write_to_db >> *evict_from_cache;

    pipeline->createTaskGroup("CheckpointPipeline")
        ->addTask(std::move(feed_pipeline))
        ->addTask(std::move(create_window))
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
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    uint64_t mem = 0;
    for (const auto& [id, chkpt] : chkpts_cache_) {
        mem += chkpt->getTotalMemoryUse();
    }
    mem += chkpt_query_->getTotalMemoryUse();
    return mem;
}

uint64_t DatabaseCheckpointer::getContentMemoryUse() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    uint64_t mem = 0;
    for (const auto& [id, chkpt] : chkpts_cache_) {
        mem += chkpt->getTotalMemoryUse();
    }
    mem += chkpt_query_->getContentMemoryUse();
    return mem;
}

void DatabaseCheckpointer::deleteCheckpoint(chkpt_id_t id)
{
    if (!hasCheckpoint(id)) {
        throw CheckpointError("Could not delete checkpoint ID=")
            << id << " because no checkpoint by this ID was found";
    }

    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (auto it = chkpts_cache_.find(id); it != chkpts_cache_.end()) {
        checkpoint_type* chkpt = it->second.get();

        // Allow deletion and change ID to UNIDENTIFIED_CHECKPOINT.
        // This is still part of a chain though until there are no
        // dependencies on it.
        if (!chkpt->isFlaggedDeleted()) {
            num_dead_checkpoints_++;
            if (chkpt->isSnapshot()) {
                num_alive_snapshots_--;
            }
            num_alive_checkpoints_--;
            chkpt->flagDeleted();
        }

        // Delete this and all contiguous previous checkpoint which were
        // flagged deleted if possible. Stop if current_ is encountered
        cleanupChain_(chkpt->getID());
    }

    chkpt_query_->deleteCheckpoint(id);
}

void DatabaseCheckpointer::loadCheckpoint(chkpt_id_t id)
{
    auto chkpt = findCheckpoint(id);
    chkpt->load(getArchDatas());

    // Move current to another checkpoint. Anything between head and the
    // old current_ is fair game for removal if allowed
    checkpoint_type* rmv = static_cast<checkpoint_type*>(getCurrent_());
    setCurrent_(chkpt.get());
    addToCache_(std::move(chkpt));

    // Restore scheduler tick number
    if (sched_) {
        sched_->restartAt(getCurrentTick());
    }

    // Remove all checkpoints which can be. Stop if the new current_ is
    // encountered again.
    // Note that is is OK if current_ was moved to a later position in
    // the chain. No important checkpoints will be removed. The
    // important thing is never to remove current_.
    cleanupChain_(rmv->getID());
}

std::vector<chkpt_id_t> DatabaseCheckpointer::getCheckpointsAt(tick_t t) const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    std::vector<chkpt_id_t> results;
    for (const auto& [id, chkpt] : chkpts_cache_) {
        if (chkpt->getTick() == t && !chkpt->isFlaggedDeleted()) {
            results.push_back(id);
        }
    }

    for (auto id : chkpt_query_->getCheckpointsAt(t)) {
        results.push_back(id);
    }

    return results;
}

std::vector<chkpt_id_t> DatabaseCheckpointer::getCheckpoints() const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    std::vector<chkpt_id_t> results;
    for (const auto& [id, chkpt] : chkpts_cache_) {
        if (!chkpt->isFlaggedDeleted()) {
            results.push_back(id);
        }
    }

    for (auto id : chkpt_query_->getCheckpoints()) {
        results.push_back(id);
    }

    return results;
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
    std::lock_guard<std::recursive_mutex> lock(mutex_);

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

std::unique_ptr<DatabaseCheckpoint> DatabaseCheckpointer::findLatestCheckpointAtOrBefore(tick_t tick, chkpt_id_t from)
{
    if (!hasCheckpoint(from)) {
        throw CheckpointError("There is no checkpoint with ID ") << from;
    }

    std::lock_guard<std::recursive_mutex> lock(mutex_);

    auto id = from;
    do {
        auto chkpt = findCheckpoint(id);
        if (chkpt->getTick() <= tick) {
            break;
        }
        id = chkpt->getPrevID();
    } while (id != checkpoint_type::UNIDENTIFIED_CHECKPOINT);

    return findCheckpoint(id);
}

std::unique_ptr<DatabaseCheckpoint> DatabaseCheckpointer::findCheckpoint(chkpt_id_t id, bool must_exist) const
{
    if (!hasCheckpoint(id)) {
        throw CheckpointError("There is no checkpoint with ID ") << id;
    }

    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (auto it = chkpts_cache_.find(id); it != chkpts_cache_.end()) {
        return it->second->clone();
    }

    return chkpt_query_->findCheckpoint(id);
}

bool DatabaseCheckpointer::hasCheckpoint(chkpt_id_t id) const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (chkpts_cache_.find(id) != chkpts_cache_.end()) {
        return true;
    }

    return chkpt_query_->hasCheckpoint(id);
}

void DatabaseCheckpointer::dumpRestoreChain(std::ostream& o, chkpt_id_t id) const
{
    auto rc = getRestoreChain(id);
    while (true) {
        const auto chkpt = findCheckpoint(rc.top());
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
    std::lock_guard<std::recursive_mutex> lock(mutex_);

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
        auto chkpt = findCheckpoint(id);
        if (chkpt->isSnapshot()) {
            break;
        }
        id = chkpt->getPrevID();
    }
    return chkpts;
}

std::vector<chkpt_id_t> DatabaseCheckpointer::getNextIDs(chkpt_id_t id) const
{
    return findCheckpoint(id)->getNextIDs();
}

uint32_t DatabaseCheckpointer::getDistanceToPrevSnapshot(chkpt_id_t id) const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

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
    return findCheckpoint(id)->isSnapshot();
}

bool DatabaseCheckpointer::canDelete(chkpt_id_t id) const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    auto chkpt = findCheckpoint(id);
    if (!chkpt->isFlaggedDeleted()) {
        return false;
    }

    for (auto next_id : chkpt->getNextIDs()) {
        if (!canDelete(next_id) && !isSnapshot(next_id)) {
            return false;
        }
    }

    return false;
}

std::string DatabaseCheckpointer::stringize() const
{
    std::stringstream ss;
    ss << "<DatabaseCheckpointer on " << getRoot().getLocation() << '>';
    return ss.str();
}

void DatabaseCheckpointer::dumpList(std::ostream& o) const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    for (const auto& [id, chkpt] : chkpts_cache_) {
        o << chkpt->stringize() << std::endl;
    }

    chkpt_query_->dumpList(o);
}

void DatabaseCheckpointer::dumpData(std::ostream& o) const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    for (const auto& [id, chkpt] : chkpts_cache_) {
        chkpt->dumpData(o);
        o << std::endl;
    }

    chkpt_query_->dumpData(o);
}

void DatabaseCheckpointer::dumpAnnotatedData(std::ostream& o) const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

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

    if (current->isSnapshot()) {
        // Clean up starting with this snapshot and moving back.
        // May have an opportunity to free older deltas right now
        // (instead of upon next deletion)
        cleanupChain_(current->getID());
    }

    return current->getID();
}

void DatabaseCheckpointer::cleanupChain_(chkpt_id_t id)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    // In order to truly delete any checkpoints, we must traverse back
    // to the previous snapshot (or the head) and forward to the another
    // snapshot or the end of the chain.
    // ONLY if both of those points can be reached without encountering
    // a living checkpoint or the current checkpoint (forward
    // only) can the whole chain (including the leading shapshot) be
    // deleted.

    if (id == getHeadID()) {
        // Cannot delete head of checkpoint tree
        return;
    }

    // Walk forward to another snapshot or current
    const bool needed_later = (getCurrentID() == id) || recursForwardFindAlive_(id);
    if (needed_later) {
        // Cannot delete because a later living checkpoint (or current) depends on this.
        auto chkpt = findCheckpoint(id);
        if (chkpt->isSnapshot()) {
            // This snapshot is needed later. Move to previous delta and work from there.
            id = chkpt->getPrevID();
        } else {
            return; // This delta is needed. Therefore all preceeding deltas are needed.
        }
    }

    // Delete backward until current, head, or a non-flagged-deleted checkpoint is hit.
    // It is possible to fracture the checkpoint tree by deleting a segment
    // between two snapshots, so prev can end up with nothing leading up to it
    while (true) {
        if (id == checkpoint_type::UNIDENTIFIED_CHECKPOINT) {
            break;
        }

        if (id == getHeadID()) {
            break;
        }

        auto chkpt = findCheckpoint(id);
        if (!chkpt->isFlaggedDeleted()) {
            break;
        }

        // If the checkpoint to delete is the current checkpoint, then
        // We cannot just set current to the previous checkpoint because
        // we may have run forward and storing a checkpoint in the
        // future would depend on the checkpoint we are about to delete.
        // This could be fixed by requiring the next checkpoint to be a
        // spapshot. Instead, point to the flagged-deleted checkpoint
        // and do not delete
        if (getCurrentID() == id) {
            return;
        }

        auto prev = findCheckpoint(chkpt->getPrevID(), false);

        // If nothing later in the chain (tree) depends on d's data, it can be deleted.
        // This also patches the checkpoint tree around the deleted checkpoint
        //! \todo canDelete is recursive at worst and might benefit from optimization
        if (chkpt->canDelete()) {
            // Get checkpoint id regardless of whether alive or dead
            chkpt_id_t id = chkpt->getID();
            if (chkpt->isFlaggedDeleted()) {
                id = chkpt->getDeletedID();
            }

            num_dead_checkpoints_--;

            // Erase element in the cache/DB
            disconnectChainLink_(id);
        }

        // Continue until head is reached
        if (prev) {
            id = prev->getID();
        } else {
            break;
        }
    }
}

void DatabaseCheckpointer::disconnectChainLink_(chkpt_id_t id)
{
    //TODO cnyce
    (void)id;
}

bool DatabaseCheckpointer::recursForwardFindAlive_(chkpt_id_t id) const
{
    const auto next_ids = getNextIDs(id);

    for (const auto next_id : next_ids) {
        auto chkpt = findCheckpoint(next_id);
        // Only check descendants for snapshot-ness
        if (chkpt->isSnapshot()) {
            // Found a live snapshot that ends this branch. chkpt is not needed
            // after this
            return false;
        }
        if (next_id == getCurrentID()) {
            // Found current in this search chain
            return true;
        }
        if (chkpt->isFlaggedDeleted() == false) {
            // Encountered a checkpoint later in the chain that still
            // depends on this.
            return true;
        }

        // Continue the search recursively
        if (recursForwardFindAlive_(next_id)) {
            return true;
        }
    }

    // Found nothing alive.
    return false;
}

void DatabaseCheckpointer::dumpCheckpointNode_(const chkpt_id_t id, std::ostream& o) const
{
    static std::string SNAPSHOT_NOTICE = "(s)";
    auto cp = findCheckpoint(id);

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
    return findCheckpoint(id)->getNextIDs();
}

void DatabaseCheckpointer::setHead_(CheckpointBase* head)
{
    setHeadID_(head->getID());
    Checkpointer::setHead_(head);
}

void DatabaseCheckpointer::setCurrent_(CheckpointBase* current)
{
    setCurrentID_(current->getID());
    Checkpointer::setCurrent_(current);
}

void DatabaseCheckpointer::setHeadID_(chkpt_id_t id)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    sparta_assert(id != checkpoint_type::UNIDENTIFIED_CHECKPOINT);
    sparta_assert(head_id_ == checkpoint_type::UNIDENTIFIED_CHECKPOINT);
    head_id_ = id;
}

void DatabaseCheckpointer::setCurrentID_(chkpt_id_t id)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    sparta_assert(id != checkpoint_type::UNIDENTIFIED_CHECKPOINT);
    current_id_ = id;
}

void DatabaseCheckpointer::addToCache_(std::shared_ptr<checkpoint_type> chkpt)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto id = chkpt->getID();
    chkpt_ids_for_pipeline_head_.push(id);

    auto& cp = chkpts_cache_[id];
    sparta_assert(!cp);
    cp = std::move(chkpt);
}

bool DatabaseCheckpointer::cloneNextPipelineHeadCheckpoint_(std::shared_ptr<checkpoint_type>& next)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (chkpt_ids_for_pipeline_head_.empty()) {
        return false;
    }

    auto next_id = chkpt_ids_for_pipeline_head_.front();
    chkpt_ids_for_pipeline_head_.pop();

    auto it = chkpts_cache_.find(next_id);
    sparta_assert(it != chkpts_cache_.end());

    auto& next_chkpt = it->second;
    next = next_chkpt->clone();
    return true;
}

REGISTER_SIMDB_APPLICATION(DatabaseCheckpointer);

} // namespace sparta::serialization::checkpoint
