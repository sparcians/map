// <DatabaseCheckpointer> -*- C++ -*-

#include "sparta/serialization/checkpoint/DatabaseCheckpointer.hpp"
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

std::unique_ptr<simdb::pipeline::Pipeline> DatabaseCheckpointer::createPipeline(
    simdb::pipeline::AsyncDatabaseAccessor* db_accessor)
{
    auto pipeline = std::make_unique<simdb::pipeline::Pipeline>(db_mgr_, NAME);

    // Task 1: Package up checkpoints into a checkpoint window
    auto create_window = simdb::pipeline::createTask<simdb::pipeline::Function<checkpoint_ptrs, ChkptWindow>>(
        [](checkpoint_ptrs&& chkpts,
           simdb::ConcurrentQueue<ChkptWindow>& windows,
           bool /*force_flush*/)
        {
            ChkptWindow window;
            window.start_chkpt_id = chkpts.front()->getID();
            window.end_chkpt_id = chkpts.back()->getID();
            window.start_tick = chkpts.front()->getTick();
            window.end_tick = chkpts.back()->getTick();
            window.chkpts = std::move(chkpts);
            windows.emplace(std::move(window));
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

            bytes.start_chkpt_id = window.start_chkpt_id;
            bytes.end_chkpt_id = window.end_chkpt_id;
            bytes.start_tick = window.start_tick;
            bytes.end_tick = window.end_tick;
            window_bytes.emplace(std::move(bytes));
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
        }
    );

    // Task 4: Write to the database
    auto write_to_db = db_accessor->createAsyncWriter<DatabaseCheckpointer, ChkptWindowBytes, void>(
        [this](ChkptWindowBytes&& bytes_in,
               simdb::pipeline::AppPreparedINSERTs* tables,
               bool /*force_flush*/)
        {
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
        }
    );

    *create_window >> *window_to_bytes >> *zlib_bytes >> *write_to_db;

    pipeline_head_ = create_window->getTypedInputQueue<checkpoint_ptrs>();

    pipeline_flusher_ = std::make_unique<simdb::pipeline::RunnableFlusher>(
        *db_mgr_, create_window, window_to_bytes, zlib_bytes, write_to_db);

    pipeline->createTaskGroup("CheckpointPipeline")
        ->addTask(std::move(create_window))
        ->addTask(std::move(window_to_bytes))
        ->addTask(std::move(zlib_bytes));

    return pipeline;
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
    pipeline_flusher_->flush();
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

uint32_t DatabaseCheckpointer::getNumCheckpoints() noexcept
{
    return next_chkpt_id_;
}

uint32_t DatabaseCheckpointer::getNumSnapshots() noexcept
{
    return next_chkpt_id_ ? getWindowID_(next_chkpt_id_) + 1 : 0;
}

uint32_t DatabaseCheckpointer::getNumDeltas() noexcept
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

    // Now delete from the database
    pipeline_flusher_->flush();

    db_mgr_->safeTransaction([&]() {
        // DELETE FROM ChkptWindows WHERE WindowID > start_win_id
        auto query = db_mgr_->createQuery("ChkptWindows");
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
                pipeline_head_->emplace(std::move(window->chkpts));
            }
        }
    });
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
            pipeline_head_->emplace(std::move(window));
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
        checkpoint_ptrs window_chkpts = getWindowFromDatabase_(win_id);
        if (window_chkpts.empty() && must_succeed) {
            throw CheckpointError("Could not find checkpoint window with ID ") << win_id;
        }
        chkpts_cache_[win_id] = std::move(window_chkpts);
    }

    bool success = false;
    for (const auto& chkpt : chkpts_cache_[win_id]) {
        if (chkpt->getID() == chkpt_id) {
            success = true;
            break;
        }
    }

    if (!success && must_succeed) {
        throw CheckpointError("Could not find checkpoint with ID ") << chkpt_id;
    }

    touchWindow_(win_id);
    evictWindowsIfNeeded_();
    return success;
}

std::vector<std::shared_ptr<DatabaseCheckpoint>> DatabaseCheckpointer::getWindowFromDatabase_(window_id_t win_id)
{
    std::vector<std::shared_ptr<DatabaseCheckpoint>> window_chkpts;
    pipeline_flusher_->flush();

    db_mgr_->safeTransaction([&]() {
        auto query = db_mgr_->createQuery("ChkptWindows");
        query->addConstraintForUInt64("WindowID", simdb::Constraints::EQUAL, win_id);

        std::vector<char> compressed_window_bytes;
        query->select("WindowBytes", compressed_window_bytes);

        auto results = query->getResultSet();
        if (results.getNextRecord()) {
            std::unique_ptr<ChkptWindow> window_restored = deserializeWindow_(compressed_window_bytes);
            sparta_assert(window_restored && !window_restored->chkpts.empty());
            window_chkpts = std::move(window_restored->chkpts);
        }
    });

    return window_chkpts;
}

std::unique_ptr<ChkptWindow> DatabaseCheckpointer::deserializeWindow_(const std::vector<char>& compressed_window_bytes) const
{
    std::vector<char> window_bytes;
    simdb::decompressData(compressed_window_bytes, window_bytes);

    auto window_restored = std::make_unique<ChkptWindow>();
    boost::iostreams::basic_array_source<char> device(window_bytes.data(), window_bytes.size());
    boost::iostreams::stream<boost::iostreams::basic_array_source<char>> is(device);
    boost::archive::binary_iarchive ia(is);
    ia >> *window_restored;

    for (auto& chkpt : window_restored->chkpts) {
        chkpt->checkpointer_ = const_cast<DatabaseCheckpointer*>(this);
    }

    return window_restored;
}

void DatabaseCheckpointer::forEachCheckpoint_(const std::function<void(const DatabaseCheckpoint*)>& cb)
{
    // Flush the pipeline so that every checkpoint is either in our cache or on disk.
    // There is no guarantee that the cache has newer checkpoints than the database,
    // since many APIs load old windows into the cache and "mix them together" with
    // whatever is already in the cache (new and old).
    pipeline_flusher_->flush();

    {
        std::lock_guard<std::recursive_mutex> lock(cache_mutex_);

        // Gather up all checkpoint IDs from our cache
        for (const auto& [win_id, window] : chkpts_cache_) {
            for (const auto& chkpt : window) {
                cb(chkpt.get());
            }
        }
    }

    // Query the database for any other checkpoints
    db_mgr_->safeTransaction([&]() {
        auto query = db_mgr_->createQuery("ChkptWindows");

        std::vector<char> compressed_window_bytes;
        query->select("WindowBytes", compressed_window_bytes);

        auto results = query->getResultSet();
        while (results.getNextRecord())
        {
            auto window = deserializeWindow_(compressed_window_bytes);
            for (const auto& chkpt : window->chkpts) {
                cb(chkpt.get());
            }
        }
    });
}

REGISTER_SIMDB_APPLICATION(DatabaseCheckpointer);

} // namespace sparta::serialization::checkpoint
