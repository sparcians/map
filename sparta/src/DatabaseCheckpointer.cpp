// <DatabaseCheckpointer> -*- C++ -*-

#include "sparta/serialization/checkpoint/DatabaseCheckpointer.hpp"
#include "simdb/apps/AppRegistration.hpp"
#include "simdb/schema/SchemaDef.hpp"
#include "simdb/pipeline/AsyncDatabaseAccessor.hpp"
#include "simdb/pipeline/Pipeline.hpp"
#include "simdb/pipeline/elements/Function.hpp"
#include "simdb/pipeline/elements/Buffer.hpp"
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

using tick_t = typename CheckpointBase::tick_t;
using chkpt_id_t = typename CheckpointBase::chkpt_id_t;
using window_id_t = typename DatabaseCheckpointer::window_id_t;

DatabaseCheckpointer::DatabaseCheckpointer(simdb::DatabaseManager* db_mgr, TreeNode& root, Scheduler* sched) :
    Checkpointer(root, sched),
    db_mgr_(db_mgr),
    next_chkpt_id_(checkpoint_type::MIN_CHECKPOINT)
{
}

void DatabaseCheckpointer::defineSchema(simdb::Schema& schema)
{
    using dt = simdb::SqlDataType;

    auto& windows = schema.addTable("ChkptWindows");
    windows.addColumn("WindowID", dt::uint64_t);
    windows.addColumn("WindowBytes", dt::blob_t);
    windows.addColumn("StartChkpID", dt::uint64_t);
    windows.addColumn("EndChkpID", dt::uint64_t);
    windows.addColumn("StartTick", dt::uint64_t);
    windows.addColumn("EndTick", dt::uint64_t);
    windows.createIndexOn("WindowID");
    windows.createCompoundIndexOn({"StartChkpID", "EndChkpID"});
    windows.createCompoundIndexOn({"StartTick", "EndTick"});
    windows.disableAutoIncPrimaryKey();
}

/// Local helper to find an item in a pipeline queue, validating that if
/// found, it is the only item in the queue.
template <typename Iter, typename Pred>
Iter find_unique_if(Iter begin, Iter end, Pred pred)
{
    auto it = std::find_if(begin, end, pred);
    if (it != end) {
        auto dup = std::find_if(std::next(it), end, pred);
        sparta_assert(dup == end, "Multiple matches found when only one was allowed");
    }
    return it;
}

/// Local helper to remove an item from a std::deque in constant time,
/// assuming we do not care about the order of items in the deque.
template <typename T>
void fast_remove_from_deque(std::deque<T>& deq, typename std::deque<T>::iterator it)
{
    sparta_assert(!deq.empty());
    sparta_assert(it != deq.end(), "Cannot remove invalid iterator from deque");
    std::iter_swap(it, deq.end() - 1); // Swap with last
    deq.pop_back();                    // Remove last in constant time
}

void DatabaseCheckpointer::createPipeline(simdb::pipeline::PipelineManager* pipeline_mgr)
{
    pipeline_mgr_ = pipeline_mgr;
    auto pipeline = pipeline_mgr_->createPipeline(NAME);
    auto db_accessor = pipeline_mgr_->getAsyncDatabaseAccessor();

    // Task 1: Serialize a checkpoint window into a char buffer
    auto window_to_bytes = simdb::pipeline::createTask<simdb::pipeline::Function<ChkptWindow, ChkptWindowBytes>>(
        [](ChkptWindow&& window,
           simdb::ConcurrentQueue<ChkptWindowBytes>& window_bytes,
           bool /*force_flush*/)
        {
            if (window.ignore || window.chkpts.empty()) {
                // This window was marked for deletion during a snoop operation.
                return simdb::pipeline::RunnableOutcome::NO_OP;
            }

            ChkptWindowBytes bytes;
            boost::iostreams::back_insert_device<std::vector<char>> inserter(bytes.chkpt_bytes);
            boost::iostreams::stream<boost::iostreams::back_insert_device<std::vector<char>>> os(inserter);
            boost::archive::binary_oarchive oa(os);
            oa << window;
            os.flush();

            bytes.start_chkpt_id = window.start_chkpt_id;
            bytes.end_chkpt_id = window.end_chkpt_id;
            bytes.start_tick = window.start_tick;
            bytes.end_tick = window.end_tick;
            window_bytes.emplace(std::move(bytes));

            return simdb::pipeline::RunnableOutcome::DID_WORK;
        }
    );

    // Task 2: Perform zlib compression on the checkpoint window bytes
    auto zlib_bytes = simdb::pipeline::createTask<simdb::pipeline::Function<ChkptWindowBytes, ChkptWindowBytes>>(
        [](ChkptWindowBytes&& bytes_in,
           simdb::ConcurrentQueue<ChkptWindowBytes>& bytes_out,
           bool /*force_flush*/)
        {
            if (bytes_in.ignore || bytes_in.chkpt_bytes.empty()) {
                // This window was marked for deletion during a snoop operation.
                return simdb::pipeline::RunnableOutcome::NO_OP;
            }

            std::vector<char> compressed_bytes;
            simdb::compressData(bytes_in.chkpt_bytes, compressed_bytes);
            std::swap(bytes_in.chkpt_bytes, compressed_bytes);
            bytes_out.emplace(std::move(bytes_in));

            return simdb::pipeline::RunnableOutcome::DID_WORK;
        }
    );

    // Task 3: Handle any pending window deletions right after deleteCheckpoint_()
    // is done running in the main thread.
    auto handle_deletions = db_accessor->createAsyncReader<chkpt_id_t, void>(
        [this](chkpt_id_t&& id,
               simdb::DatabaseManager* db_mgr,
               bool /*force_flush*/)
        {
            sparta_assert(db_mgr->isInTransaction());
            auto start_win_id = getWindowID_(id);

            // DELETE FROM ChkptWindows WHERE WindowID > start_win_id
            auto query = db_mgr->createQuery("ChkptWindows");
            query->addConstraintForUInt64("WindowID", simdb::Constraints::GREATER, start_win_id);
            query->deleteResultSet();

            // Now update the window containing start_win_id to remove checkpoints >= id
            query->resetConstraints();
            query->addConstraintForUInt64("WindowID", simdb::Constraints::EQUAL, start_win_id);

            std::vector<char> compressed_window_bytes;
            query->select("WindowBytes", compressed_window_bytes);

            auto results = query->getResultSet();
            if (results.getNextRecord()) {
                // DELETE FROM ChkptWindows WHERE WindowID = start_win_id
                query->deleteResultSet();

                // Deserialize the window
                auto window = deserializeWindow_(compressed_window_bytes);

                // Remove checkpoints >= id
                auto new_end = std::remove_if(window->chkpts.begin(), window->chkpts.end(),
                    [id](const std::shared_ptr<checkpoint_type>& chkpt) {
                        return chkpt->getID() >= id;
                    });
                window->chkpts.erase(new_end, window->chkpts.end());

                // Send down the pipeline
                if (!window->chkpts.empty()) {
                    ChkptWindow send_window;
                    send_window.start_chkpt_id = window->chkpts.front()->getID();
                    send_window.end_chkpt_id = window->chkpts.back()->getID();
                    send_window.start_tick = window->chkpts.front()->getTick();
                    send_window.end_tick = window->chkpts.back()->getTick();
                    send_window.chkpts = std::move(window->chkpts);
                    pipeline_head_->emplace(std::move(send_window));
                }
            }

            return simdb::pipeline::RunnableOutcome::DID_WORK;
        }
    );

    // Task 4: Write to the database
    auto write_to_db = db_accessor->createAsyncWriter<DatabaseCheckpointer, ChkptWindowBytes, void>(
        [this](ChkptWindowBytes&& bytes_in,
               simdb::pipeline::AppPreparedINSERTs* tables,
               bool /*force_flush*/)
        {
            sparta_assert(db_mgr_->isInTransaction());
            if (bytes_in.ignore || bytes_in.chkpt_bytes.empty()) {
                // This window was marked for deletion during a snoop operation.
                return simdb::pipeline::RunnableOutcome::NO_OP;
            }

            auto window_inserter = tables->getPreparedINSERT("ChkptWindows");

            utils::ValidValue<uint64_t> win_id;
            for (chkpt_id_t cid = bytes_in.start_chkpt_id; cid <= bytes_in.end_chkpt_id; ++cid) {
                auto window_id = getWindowID_(cid);
                if (!win_id.isValid()) {
                    win_id = window_id;
                } else if (win_id != window_id) {
                    throw CheckpointError("Checkpoint window has inconsistent window IDs");
                }
            }

            window_inserter->setColumnValue(0, win_id.getValue());
            window_inserter->setColumnValue(1, bytes_in.chkpt_bytes);
            window_inserter->setColumnValue(2, bytes_in.start_chkpt_id);
            window_inserter->setColumnValue(3, bytes_in.end_chkpt_id);
            window_inserter->setColumnValue(4, bytes_in.start_tick);
            window_inserter->setColumnValue(5, bytes_in.end_tick);
            window_inserter->createRecord();

            return simdb::pipeline::RunnableOutcome::DID_WORK;
        }
    );

    *window_to_bytes >> *zlib_bytes >> *write_to_db;

    pipeline_head_ = window_to_bytes->getTypedInputQueue<ChkptWindow>();
    pending_deletions_head_ = handle_deletions->getTypedInputQueue<chkpt_id_t>();

    pipeline_flusher_ = std::make_unique<simdb::pipeline::RunnableFlusher>(
        *db_mgr_, window_to_bytes, zlib_bytes, handle_deletions, write_to_db);

    // Assign snoopers to allow us to "peek" into the pipeline to retrieve/delete
    // checkpoints on demand during findCheckpoint() or deleteCheckpoint_().
    // Snoopers effectively allow us to "extend" the cache in the sense that
    // it is still in memory, though accessing from the pipeline is slower
    // primarily due to us needing to "undo" pipeline changes e.g. serialization
    // and compression.
    // -------------------------------------------------------------------------------
    pipeline_flusher_->assignQueueSnooper<ChkptWindow>(
        *window_to_bytes,
        [this](std::deque<ChkptWindow>& queue) -> simdb::SnooperCallbackOutcome
        {
            if (snoop_win_id_for_retrieval_.isValid()) {
                return handleLoadWindowIntoCacheSnooper_(queue);
            }

            sparta_assert(snoop_chkpt_id_for_deletion_.isValid(),
                          "Snooper called but we are not set up to snoop any checkpoint ID or window ID");

            return handleDeleteCheckpointSnooper_(queue);
        }
    );

    pipeline_flusher_->assignQueueSnooper<ChkptWindowBytes>(
        *zlib_bytes,
        [this](std::deque<ChkptWindowBytes>& queue) -> simdb::SnooperCallbackOutcome
        {
            // Decompression is not needed since we are snooping the zlib task's input queue
            // so it hasn't run yet.
            constexpr bool requires_decompression = false;

            if (snoop_win_id_for_retrieval_.isValid()) {
                return handleLoadWindowIntoCacheSnooper_(queue, requires_decompression);
            }

            sparta_assert(snoop_chkpt_id_for_deletion_.isValid(),
                          "Snooper called but we are not set up to snoop any checkpoint ID or window ID");

            return handleDeleteCheckpointSnooper_(queue, requires_decompression);
        }
    );

    pipeline_flusher_->assignQueueSnooper<ChkptWindowBytes>(
        *write_to_db,
        [this](std::deque<ChkptWindowBytes>& queue) -> simdb::SnooperCallbackOutcome
        {
            // Decompression needed since we are snooping the DB writer task's input queue
            // which is the zlib task's output queue. So compression has already run.
            constexpr bool requires_decompression = true;

            if (snoop_win_id_for_retrieval_.isValid()) {
                return handleLoadWindowIntoCacheSnooper_(queue, requires_decompression);
            }

            sparta_assert(snoop_chkpt_id_for_deletion_.isValid(),
                          "Snooper called but we are not set up to snoop any checkpoint ID or window ID");

            return handleDeleteCheckpointSnooper_(queue, requires_decompression);
        }
    );

    pipeline->createTaskGroup("CheckpointPipeline")
        ->addTask(std::move(window_to_bytes))
        ->addTask(std::move(zlib_bytes));

    db_accessor_ = db_accessor;
}

uint32_t DatabaseCheckpointer::getSnapshotThreshold() const
{
    return snap_thresh_;
}

void DatabaseCheckpointer::setSnapshotThreshold(uint32_t thresh)
{
    sparta_assert(!snap_thresh_.isValid(), "Snapshot threshold cannot be changed once set.");
    sparta_assert(thresh > 1, "Snapshot threshold must be greater than 1");
    snap_thresh_ = thresh;
}

void DatabaseCheckpointer::setMaxCachedWindows(uint32_t max_windows)
{
    sparta_assert(!max_cached_windows_.isValid(), "Max cached windows cannot be changed once set.");
    sparta_assert(max_windows > 0, "Max cached windows must be greater than 0");
    max_cached_windows_ = max_windows;
}

uint64_t DatabaseCheckpointer::getTotalMemoryUse() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    // Only add up the memory use from the cache.
    uint64_t mem = 0;
    for (const auto& [win_id, window] : chkpts_cache_) {
        for (const auto& chkpt : window) {
            mem += chkpt->getTotalMemoryUse();
        }
    }
    return mem;
}

uint64_t DatabaseCheckpointer::getContentMemoryUse() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    // Only add up the memory use from the cache.
    uint64_t mem = 0;
    for (const auto& [win_id, window] : chkpts_cache_) {
        for (const auto& chkpt : window) {
            mem += chkpt->getContentMemoryUse();
        }
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

    if (auto c = getCurrent_(); !c || c->getID() == id) {
        return;
    }

    // Disable the pipeline for the duration of this function. Even if
    // we find the checkpoint in the cache, we still have to flush all
    // future checkpoints / windows from the pipeline. We use a snooper
    // to do this.
    //
    // Also note that there is a scopedDisableAll() inside findCheckpoint(),
    // but that becomes a no-op since we are already disabled here.
    // The "false" flag here means "do not bother putting the threads to
    // sleep too". Just the thread's runnables will be disabled. Waiting
    // for the threads to ACK the sleep request can take milliseconds
    // which is too long for loading checkpoints.
    auto disabler = pipeline_mgr_->scopedDisableAll(false);

    auto chkpt = findCheckpoint(id, true);
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

    // Increasing-by-one, starting-at-zero checkpoint IDs guarantee we can do this:
    next_chkpt_id_ = id + 1;

    // Restore scheduler tick number
    if (sched_) {
        sched_->restartAt(getCurrentTick());
    }
}

void DatabaseCheckpointer::preTeardown()
{
    // Send every window down the pipeline and flush it.
    evictWindowsIfNeeded_(true);
    pipeline_flusher_->waterfallFlush();
}

std::vector<chkpt_id_t> DatabaseCheckpointer::getCheckpointsAt(tick_t t)
{
    std::unordered_set<chkpt_id_t> ids;

    forEachCheckpoint_([t, &ids](const DatabaseCheckpoint* chkpt) {
        if (chkpt->getTick() == t) {
            ids.insert(chkpt->getID());
        }
    });

    std::vector<chkpt_id_t> ret(ids.begin(), ids.end());
    std::sort(ret.begin(), ret.end());
    return ret;
}

std::vector<chkpt_id_t> DatabaseCheckpointer::getCheckpoints()
{
    std::unordered_set<chkpt_id_t> ids;

    forEachCheckpoint_([&ids](const DatabaseCheckpoint* chkpt) {
        ids.insert(chkpt->getID());
    });

    std::vector<chkpt_id_t> ret(ids.begin(), ids.end());
    std::sort(ret.begin(), ret.end());
    return ret;
}

uint32_t DatabaseCheckpointer::getNumCheckpoints() const noexcept
{
    return next_chkpt_id_;
}

uint32_t DatabaseCheckpointer::getNumSnapshots() const noexcept
{
    return next_chkpt_id_ ? getWindowID_(next_chkpt_id_) + 1 : 0;
}

uint32_t DatabaseCheckpointer::getNumDeltas() const noexcept
{
    return getNumCheckpoints() - getNumSnapshots();
}

std::deque<chkpt_id_t> DatabaseCheckpointer::getCheckpointChain(chkpt_id_t id)
{
    std::deque<chkpt_id_t> chain;
    if (!getHead()) {
        return chain;
    }

    if (hasCheckpoint(id)) {
        // This checkpointer guarantees a linear chain of checkpoints with no gaps.
        // While we could also walk backwards using getPrevID(), load checkpoints
        // into memory, and call getID() on each of them, the result of doing that
        // would effectively load every window into our cache only to dump most of
        // them (LRU). The cache could very well end up being 100% full of very old
        // checkpoints, thus slowing down further API calls to reload newer windows
        // into the cache.
        do {
            chain.push_back(id);
        } while (id-- > 0);
    } else {
        throw CheckpointError("There is no checkpoint with ID ") << id;
    }

    return chain;
}

std::shared_ptr<DatabaseCheckpoint> DatabaseCheckpointer::findCheckpoint(chkpt_id_t id, bool must_exist)
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    if (id >= next_chkpt_id_) {
        if (must_exist) {
            throw CheckpointError("There is no checkpoint with ID ") << id;
        }
        return nullptr;
    }

    if (!ensureWindowLoaded_(id, must_exist)) {
        return nullptr;
    }

    auto win_id = getWindowID_(id);
    auto& window = chkpts_cache_[win_id];
    sparta_assert(!window.empty());

    // Find the checkpoint in the window in constant time, noting that
    // the window will have checkpoints in ascending order by ID with
    // no gaps.
    auto snapshot_id = window.front()->getID();
    auto& chkpt = window.at(id - snapshot_id);
    sparta_assert(chkpt->getID() == id);
    return chkpt;
}

std::shared_ptr<DatabaseCheckpoint> DatabaseCheckpointer::findLatestCheckpointAtOrBefore(tick_t tick, chkpt_id_t from)
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    auto chkpt = findCheckpoint(from, true);
    do {
        if (chkpt->getTick() <= tick) {
            break;
        }
        chkpt = findCheckpoint(chkpt->getPrevID());
    } while (chkpt);

    return (chkpt && chkpt->getTick() <= tick) ? chkpt : nullptr;
}

bool DatabaseCheckpointer::hasCheckpoint(chkpt_id_t id) noexcept
{
    return findCheckpoint(id) != nullptr;
}

void DatabaseCheckpointer::dumpRestoreChain(std::ostream& o, chkpt_id_t id)
{
    auto rc = getRestoreChain(id);
    while (true) {
        auto chkpt = findCheckpoint(rc.top());
        rc.pop();

        if (chkpt->isSnapshot()) {
            o << "(";
        }
        o << chkpt->getID();
        if (chkpt->isSnapshot()) {
            o << ")";
        }
        if (rc.empty()) {
            break;
        } else {
            o << " --> ";
        }
    }
}

std::stack<chkpt_id_t> DatabaseCheckpointer::getHistoryChain(chkpt_id_t id)
{
    ensureWindowLoaded_(id, true);

    std::stack<chkpt_id_t> chain;
    do {
        chain.push(id);
    } while (id-- > 0);

    return chain;
}

std::stack<chkpt_id_t> DatabaseCheckpointer::getRestoreChain(chkpt_id_t id)
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    ensureWindowLoaded_(id, true);
    auto win_id = getWindowID_(id);
    auto& window = chkpts_cache_[win_id];
    sparta_assert(!window.empty());

    std::stack<chkpt_id_t> chain;
    auto snapshot_id = window.front()->getID();
    do {
        chain.push(id);
    } while (id-- > snapshot_id);

    return chain;
}

std::vector<chkpt_id_t> DatabaseCheckpointer::getNextIDs(chkpt_id_t id)
{
    auto chkpt = findCheckpoint(id, true);
    return chkpt->getNextIDs();
}

uint32_t DatabaseCheckpointer::getDistanceToPrevSnapshot(chkpt_id_t id) noexcept
{
    return getRestoreChain(id).size() - 1;
}

bool DatabaseCheckpointer::isSnapshot(chkpt_id_t id) noexcept
{
    auto chkpt = findCheckpoint(id, true);
    return chkpt->isSnapshot();
}

std::string DatabaseCheckpointer::stringize() const
{
    std::stringstream ss;
    ss << "<DatabaseCheckpointer on " << getRoot().getLocation() << '>';
    return ss.str();
}

void DatabaseCheckpointer::dumpList(std::ostream& o)
{
    std::map<chkpt_id_t, std::string> chkpt_strings;

    forEachCheckpoint_([&chkpt_strings](const DatabaseCheckpoint* chkpt) {
        chkpt_strings[chkpt->getID()] = chkpt->stringize();
    });

    for (const auto& [id, str] : chkpt_strings) {
        o << str << "\n";
    }
    o << std::flush;
}

void DatabaseCheckpointer::dumpData(std::ostream& o)
{
    std::map<chkpt_id_t, std::string> chkpt_strings;

    forEachCheckpoint_([&chkpt_strings](const DatabaseCheckpoint* chkpt) {
        std::ostringstream oss;
        chkpt->dumpData(oss);
        chkpt_strings[chkpt->getID()] = oss.str();
    });

    for (const auto& [id, str] : chkpt_strings) {
        o << str << "\n";
    }
    o << std::flush;
}

void DatabaseCheckpointer::dumpAnnotatedData(std::ostream& o)
{
    std::map<chkpt_id_t, std::string> chkpt_strings;

    forEachCheckpoint_([&chkpt_strings](const DatabaseCheckpoint* chkpt) {
        std::ostringstream oss;
        oss << chkpt->stringize() << "\n";
        chkpt->dumpData(oss);
        chkpt_strings[chkpt->getID()] = oss.str();
    });

    for (const auto& [id, str] : chkpt_strings) {
        o << str << "\n";
    }
    o << std::flush;
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

    throw CheckpointError("DatabaseCheckpointer::traceValue() not implemented");
}

bool DatabaseCheckpointer::isCheckpointCached(chkpt_id_t id) const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);
    const auto win_id = getWindowID_(id);
    const auto it = chkpts_cache_.find(win_id);
    return it != chkpts_cache_.end();
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
}

chkpt_id_t DatabaseCheckpointer::createCheckpoint_(bool force_snapshot)
{
    if (force_snapshot) {
        throw CheckpointError("DatabaseCheckpointer does not support forced snapshots");
    }

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
    return current->getID();
}

void DatabaseCheckpointer::deleteCheckpoint_(chkpt_id_t id)
{
    // Throw if trying to delete head checkpoint
    if (id == getHeadID()) {
        throw CheckpointError("Cannot delete head checkpoint with ID ") << id;
    }

    window_id_t start_win_id = getWindowID_(id);
    window_id_t end_win_id = getWindowID_(next_chkpt_id_ - 1);

    for (window_id_t win_id = start_win_id; win_id <= end_win_id; ++win_id) {
        auto it = chkpts_cache_.find(win_id);
        if (it != chkpts_cache_.end()) {
            if (win_id == start_win_id) {
                // Only delete checkpoints in this window >= id
                auto& window = it->second;
                auto new_end = std::remove_if(window.begin(), window.end(),
                    [id](const std::shared_ptr<checkpoint_type>& chkpt) {
                        return chkpt->getID() >= id;
                    });

                window.erase(new_end, window.end());
                if (window.empty()) {
                    chkpts_cache_.erase(it);
                }
            } else {
                // Delete the entire window
                chkpts_cache_.erase(it);
            }
        }
    }

    // Set up the snooper to look for checkpoints in the pipeline
    // that should be removed.
    snoop_chkpt_id_for_deletion_ = id;
    pipeline_flusher_->snoopAll();
    snoop_chkpt_id_for_deletion_.clearValid();

    // Forward this deletion to the database thread task instead
    // of running DB work in the main thread.
    pending_deletions_head_->push(id);
}

void DatabaseCheckpointer::dumpCheckpointNode_(const chkpt_id_t id, std::ostream& o)
{
    static std::string SNAPSHOT_NOTICE = "(s)";

    auto chkpt = findCheckpoint(id, true);
    o << chkpt->getID();
    if (chkpt->isSnapshot()) {
        o << ' ' << SNAPSHOT_NOTICE;
    }
}

void DatabaseCheckpointer::setHead_(DatabaseCheckpoint* head)
{
    const auto id = head->getID();
    sparta_assert(id != checkpoint_type::UNIDENTIFIED_CHECKPOINT);
    sparta_assert(head_id_ == checkpoint_type::UNIDENTIFIED_CHECKPOINT);

    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);
    Checkpointer::setHead_(head);
    head_id_ = id;
}

void DatabaseCheckpointer::setCurrent_(DatabaseCheckpoint* current)
{
    const auto id = current->getID();
    sparta_assert(id != checkpoint_type::UNIDENTIFIED_CHECKPOINT);

    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);
    Checkpointer::setCurrent_(current);
    current_id_ = id;
}

void DatabaseCheckpointer::addToCache_(std::shared_ptr<checkpoint_type> chkpt)
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    const auto win_id = chkpt->getID() / (snap_thresh_ + 1);
    auto& window = chkpts_cache_[win_id];
    sparta_assert(window.empty() || window.back()->getID() == chkpt->getID() - 1,
                  "Checkpoints must be added in ID order with no gaps");
    window.emplace_back(std::move(chkpt));
    touchWindow_(win_id);
    evictWindowsIfNeeded_();
}

window_id_t DatabaseCheckpointer::getWindowID_(chkpt_id_t id) const
{
    return id / (snap_thresh_ + 1);
}

void DatabaseCheckpointer::touchWindow_(window_id_t id)
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    auto it = lru_map_.find(id);
    if (it != lru_map_.end()) {
        lru_list_.erase(it->second);
    }
    lru_list_.push_front(id);
    lru_map_[id] = lru_list_.begin();
}

void DatabaseCheckpointer::evictWindowsIfNeeded_(bool force_flush)
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    uint32_t num_windows_to_evict = 0;

    if (force_flush) {
        num_windows_to_evict = lru_list_.size();
    } else if (lru_list_.size() > max_cached_windows_) {
        num_windows_to_evict = lru_list_.size() - max_cached_windows_;
    }

    if (num_windows_to_evict == 0) {
        return;
    }

    while (num_windows_to_evict > 0) {
        // Evict the least recently used window
        const auto win_id = lru_list_.back();

        // Unless we are flushing, do not evict the window containing
        // the current checkpoint or the head checkpoint
        if (!force_flush) {
            auto current = getCurrent_();
            auto current_win_id = getWindowID_(current->getID());

            auto head = getHead_();
            auto head_win_id = getWindowID_(head->getID());

            // If the current or head checkpoint is in this window, skip eviction.
            // Decrement the number of windows to evict since we are skipping this one.
            if (current_win_id == win_id || head_win_id == win_id) {
                sparta_assert(num_windows_to_evict-- > 0);
                touchWindow_(win_id);
                continue;
            }
        }

        lru_list_.pop_back();
        lru_map_.erase(win_id);

        // Send the window down the pipeline for writing to the database
        auto& window = chkpts_cache_[win_id];
        if (!window.empty()) {
            ChkptWindow send_window;
            send_window.start_chkpt_id = window.front()->getID();
            send_window.end_chkpt_id = window.back()->getID();
            send_window.start_tick = window.front()->getTick();
            send_window.end_tick = window.back()->getTick();
            send_window.chkpts = std::move(window);
            pipeline_head_->emplace(std::move(send_window));
        }

        // Cleanup
        chkpts_cache_.erase(win_id);
        sparta_assert(num_windows_to_evict-- > 0);
    }
}

bool DatabaseCheckpointer::ensureWindowLoaded_(chkpt_id_t chkpt_id, bool must_succeed)
{
    std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

    window_id_t win_id = getWindowID_(chkpt_id);
    if (chkpts_cache_.find(win_id) == chkpts_cache_.end()) {
        return loadWindowIntoCache_(win_id, must_succeed);
    }

    auto& window = chkpts_cache_[win_id];
    if (window.empty()) {
        if (must_succeed) {
            throw CheckpointError("Checkpoint window with ID ") << win_id << " is empty";
        }
        chkpts_cache_.erase(win_id);
        return false;
    }

    auto snapshot_id = window.front()->getID();
    if (chkpt_id < snapshot_id || chkpt_id > window.back()->getID()) {
        if (must_succeed) {
            throw CheckpointError("Checkpoint ID ") << chkpt_id
                << " is not in the loaded checkpoint window with ID " << win_id
                << " which contains checkpoints from " << snapshot_id
                << " to " << window.back()->getID();
        }
        return false;
    }

    auto& chkpt = window.at(chkpt_id - snapshot_id);
    sparta_assert(chkpt->getID() == chkpt_id);

    touchWindow_(win_id);
    evictWindowsIfNeeded_();
    return true;
}

std::unique_ptr<ChkptWindow> DatabaseCheckpointer::deserializeWindow_(
    const std::vector<char>& window_bytes,
    bool requires_decompression) const
{
    if (window_bytes.empty()) {
        return nullptr;
    }

    const char* data_ptr = nullptr;
    size_t data_size = 0;

    std::vector<char> decompressed_bytes;
    if (requires_decompression) {
        simdb::decompressData(window_bytes, decompressed_bytes);
        data_ptr = decompressed_bytes.data();
        data_size = decompressed_bytes.size();
    } else {
        data_ptr = window_bytes.data();
        data_size = window_bytes.size();
    }

    auto window_restored = std::make_unique<ChkptWindow>();
    boost::iostreams::basic_array_source<char> device(data_ptr, data_size);
    boost::iostreams::stream<boost::iostreams::basic_array_source<char>> is(device);
    boost::archive::binary_iarchive ia(is);
    ia >> *window_restored;

    for (auto& chkpt : window_restored->chkpts) {
        chkpt->checkpointer_ = const_cast<DatabaseCheckpointer*>(this);
    }

    return window_restored;
}

bool DatabaseCheckpointer::loadWindowIntoCache_(window_id_t win_id, bool must_succeed)
{
    // Nothing to do if already in the cache.
    if (chkpts_cache_.find(win_id) != chkpts_cache_.end()) {
        touchWindow_(win_id);
        return true;
    }

    // Disable the pipelines so we can safely snoop the task queues.
    auto disabler = pipeline_mgr_->scopedDisableAll(false);

    snoop_win_id_for_retrieval_ = win_id;
    auto outcome = pipeline_flusher_->snoopAll();
    snoop_win_id_for_retrieval_.clearValid();

    if (outcome.found()) {
        sparta_assert(outcome.num_hits == 1);

        // Snoopers should have loaded the window into the cache. Note also that
        // if found in the pipeline, it would have been removed from the pipeline
        // as well (though there isn't a fast sparta_assert to check that).
        sparta_assert(chkpts_cache_.find(win_id) != chkpts_cache_.end());

        // Bump the window ID in the LRU list and evict windows if needed.
        touchWindow_(win_id);
        evictWindowsIfNeeded_();

        return true;
    }

    // Now try to load from the database. If found, we will add it to the cache
    // and delete the window from the database. The checkpointer design requires
    // that windows only live in one place (cache / pipeline / database).
    //
    // Final note: since the pipelines are disabled, we do not have to worry
    // about using safeTransaction as this thread is the only one accessing
    // the database.
    auto query = db_mgr_->createQuery("ChkptWindows");
    query->addConstraintForUInt64("WindowID", simdb::Constraints::EQUAL, win_id);

    std::vector<char> compressed_window_bytes;
    query->select("WindowBytes", compressed_window_bytes);

    auto results = query->getResultSet();
    if (results.getNextRecord()) {
        std::unique_ptr<ChkptWindow> window_restored = deserializeWindow_(compressed_window_bytes);
        sparta_assert(window_restored && !window_restored->chkpts.empty());

        auto start_win_id = getWindowID_(window_restored->chkpts.front()->getID());
        auto end_win_id = getWindowID_(window_restored->chkpts.back()->getID());
        sparta_assert(start_win_id == end_win_id && start_win_id == win_id,
                      "Checkpoint window has inconsistent window IDs");

        // Add to the cache, bump the window ID in the LRU list, and evict
        // windows if needed.
        chkpts_cache_[win_id] = std::move(window_restored->chkpts);
        touchWindow_(win_id);
        evictWindowsIfNeeded_();

        // Now delete from the database.
        query->deleteResultSet();

        return true;
    }

    if (must_succeed) {
        throw CheckpointError("Could not find checkpoint window with ID ") << win_id;
    }
    return false;
}

simdb::SnooperCallbackOutcome DatabaseCheckpointer::handleLoadWindowIntoCacheSnooper_(
    std::deque<ChkptWindow>& queue)
{
    // Check the windows in the queue for the one we are looking for.
    // If found, take it out of the queue and add it to the cache,
    // deleting it from the pipeline.
    auto it = find_unique_if(queue.begin(), queue.end(),
        [this](const ChkptWindow& window)
        {
            if (window.ignore) {
                return false;
            }

            auto start_win_id = getWindowID_(window.start_chkpt_id);
            auto end_win_id = getWindowID_(window.end_chkpt_id);
            sparta_assert(start_win_id == end_win_id,
                          "Checkpoint window has inconsistent window IDs");

            return (start_win_id == snoop_win_id_for_retrieval_.getValue());
        });

    if (it != queue.end()) {
        checkpoint_ptrs chkpts = std::move(it->chkpts);
        sparta_assert(!chkpts.empty(),
                      "Checkpoint window cannot be empty");

        fast_remove_from_deque(queue, it);
        chkpts_cache_[snoop_win_id_for_retrieval_.getValue()] = std::move(chkpts);
        return simdb::SnooperCallbackOutcome::FOUND_STOP;
    }

    // Continue looking in the next task's queue
    return simdb::SnooperCallbackOutcome::NOT_FOUND_CONTINUE;
}

simdb::SnooperCallbackOutcome DatabaseCheckpointer::handleLoadWindowIntoCacheSnooper_(
    std::deque<ChkptWindowBytes>& queue,
    bool requires_decompression)
{
    // Check the windows in the queue for the one we are looking for.
    // If found, take it out of the queue and add it to the cache,
    // deleting it from the pipeline.
    auto it = find_unique_if(queue.begin(), queue.end(),
        [this](const ChkptWindowBytes& bytes)
        {
            if (bytes.ignore) {
                return false;
            }

            auto start_win_id = getWindowID_(bytes.start_chkpt_id);
            auto end_win_id = getWindowID_(bytes.end_chkpt_id);
            sparta_assert(start_win_id == end_win_id,
                          "Checkpoint window has inconsistent window IDs");

            return (start_win_id == snoop_win_id_for_retrieval_.getValue());
        });

    if (it != queue.end()) {
        // "Undo" boost::serialization for this item and add it to the cache.
        // Honor the "requires_decompression" flag since this method is called
        // during pre-zlib and post-zlib task snoopers.
        checkpoint_ptrs chkpts = deserializeWindow_(it->chkpt_bytes, requires_decompression)->chkpts;

        sparta_assert(!chkpts.empty(),
                      "Checkpoint window cannot be empty");

        fast_remove_from_deque(queue, it);
        chkpts_cache_[snoop_win_id_for_retrieval_.getValue()] = std::move(chkpts);
        return simdb::SnooperCallbackOutcome::FOUND_STOP;
    }

    // Continue looking in the next task's queue
    return simdb::SnooperCallbackOutcome::NOT_FOUND_CONTINUE;
}

simdb::SnooperCallbackOutcome DatabaseCheckpointer::handleDeleteCheckpointSnooper_(
    std::deque<ChkptWindow>& queue)
{
    // Look for any windows in the queue that can be fully deleted.
    // Some snoop operations may be deleting many windows in the
    // pipeline, so we just set a flag to ignore them when they
    // are processed on the pipeline threads.
    for (auto& window : queue) {
        auto start_chkpt_id = window.start_chkpt_id;
        if (start_chkpt_id >= snoop_chkpt_id_for_deletion_.getValue()) {
            window.ignore = true;
        }
    }

    // Now look for any windows that may need to be partially deleted.
    // This is the case where our checkpoint to delete is inside a window.
    // Note that only one (if any) window can be partially deleted,
    // since windows do not overlap.
    auto it = find_unique_if(queue.begin(), queue.end(),
        [this](const ChkptWindow& window)
        {
            if (window.ignore) {
                return false;
            }

            return (window.start_chkpt_id < snoop_chkpt_id_for_deletion_.getValue() &&
                    window.end_chkpt_id >= snoop_chkpt_id_for_deletion_.getValue());
        });

    if (it != queue.end()) {
        auto chkpt_it = std::find_if(it->chkpts.begin(), it->chkpts.end(),
            [this](const checkpoint_ptr& chkpt)
            {
                return (chkpt->getID() == snoop_chkpt_id_for_deletion_.getValue());
            });

        sparta_assert(chkpt_it != it->chkpts.end(),
                        "Partially deleted checkpoint not found in its window");

        // Erase from this checkpoint to the end of the window
        it->chkpts.erase(chkpt_it, it->chkpts.end());

        // Add this window back to the cache if it still has checkpoints
        if (!it->chkpts.empty()) {
            auto win_id = getWindowID_(it->start_chkpt_id);
            chkpts_cache_[win_id] = std::move(it->chkpts);
            touchWindow_(win_id);
        }

        // Remove this window from the pipeline
        fast_remove_from_deque(queue, it);
    }

    // Always snoop the whole pipeline for more deletions
    return simdb::SnooperCallbackOutcome::NOT_FOUND_CONTINUE;
}

simdb::SnooperCallbackOutcome DatabaseCheckpointer::handleDeleteCheckpointSnooper_(
    std::deque<ChkptWindowBytes>& queue,
    bool requires_decompression)
{
    // Look for any windows in the queue that can be fully deleted.
    // Some snoop operations may be deleting many windows in the
    // pipeline, so we just set a flag to ignore them when they
    // are processed on the pipeline threads.
    for (auto& bytes : queue) {
        if (bytes.start_chkpt_id >= snoop_chkpt_id_for_deletion_.getValue()) {
            bytes.ignore = true;
        }
    }

    // Now look for any windows that may need to be partially deleted.
    // This is the case where our checkpoint to delete is inside a window.
    // Note that only one (if any) window can be partially deleted,
    // since windows do not overlap.
    auto it = find_unique_if(queue.begin(), queue.end(),
        [this](const ChkptWindowBytes& bytes)
        {
            if (bytes.ignore) {
                return false;
            }

            return (bytes.start_chkpt_id < snoop_chkpt_id_for_deletion_.getValue() &&
                    bytes.end_chkpt_id >= snoop_chkpt_id_for_deletion_.getValue());
        });

    if (it != queue.end()) {
        // "Undo" boost::serialization for this item to get its checkpoints.
        // Honor the "requires_decompression" flag since this method is called
        // during pre-zlib and post-zlib task snoopers.
        auto window = deserializeWindow_(it->chkpt_bytes, requires_decompression);

        auto chkpt_it = std::find_if(window->chkpts.begin(), window->chkpts.end(),
            [this](const checkpoint_ptr& chkpt)
            {
                return (chkpt->getID() == snoop_chkpt_id_for_deletion_.getValue());
            });

        sparta_assert(chkpt_it != window->chkpts.end(),
                        "Partially deleted checkpoint not found in its window");

        // Erase from this checkpoint to the end of the window
        window->chkpts.erase(chkpt_it, window->chkpts.end());

        // Add this window back to the cache if it still has checkpoints
        if (!window->chkpts.empty()) {
            auto win_id = getWindowID_(window->start_chkpt_id);
            chkpts_cache_[win_id] = std::move(window->chkpts);
            touchWindow_(win_id);
        }

        // Remove this window from the pipeline
        fast_remove_from_deque(queue, it);
    }

    // Always snoop the whole pipeline for more deletions
    return simdb::SnooperCallbackOutcome::NOT_FOUND_CONTINUE;
}

void DatabaseCheckpointer::forEachCheckpoint_(const std::function<void(const DatabaseCheckpoint*)>& cb)
{
    // Flush the pipeline so that every checkpoint is either in our cache or on disk.
    // There is no guarantee that the cache has newer checkpoints than the database,
    // since many APIs load old windows into the cache and "mix them together" with
    // whatever is already in the cache (new and old).
    pipeline_flusher_->waterfallFlush();

    {
        std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

        // Invoke the callback for all checkpoints in the cache
        for (const auto& [win_id, window] : chkpts_cache_) {
            for (const auto& chkpt : window) {
                sparta_assert(chkpt->getID() < next_chkpt_id_);
                cb(chkpt.get());
            }
        }
    }

    // Invoke the callback for all checkpoints on disk
    db_mgr_->safeTransaction([&]() {
        auto query = db_mgr_->createQuery("ChkptWindows");

        std::vector<char> compressed_window_bytes;
        query->select("WindowBytes", compressed_window_bytes);

        auto results = query->getResultSet();
        while (results.getNextRecord())
        {
            auto window = deserializeWindow_(compressed_window_bytes);
            for (const auto& chkpt : window->chkpts) {
                if (chkpt->getID() < next_chkpt_id_) {
                    cb(chkpt.get());
                }
            }
        }
    });
}

REGISTER_SIMDB_APPLICATION(DatabaseCheckpointer);

} // namespace sparta::serialization::checkpoint
