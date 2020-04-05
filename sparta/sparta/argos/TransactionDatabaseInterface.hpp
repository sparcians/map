// <TransactionDatabaseInterface> -*- C++ -*-

#pragma once

// Required to enable std::this_thread::sleep_for
#pragma once
#define _GLIBCXX_USE_NANOSLEEP
#endif

#include <vector>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <memory>
#include <thread>
#include <mutex>
#include <chrono>
#include <functional>
#include <list>
#include <ctime>

#include "sparta/argos/TransactionInterval.hpp"
#include "sparta/argos/PipelineDataCallback.hpp"
#include "sparta/argos/Reader.hpp"
#include "sparta/argos/transaction_structures.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/TimeManager.hpp"

namespace sparta {
    namespace argos {

class TransactionDatabaseInterface
{
public:

    /*!
     * \brief Transaction type
     */
    typedef transactionInterval<uint64_t> Transaction;

    typedef Transaction* interval_ptr;
    typedef uint32_t interval_idx;
    typedef const interval_idx const_interval_idx;

    /*!
     * \brief Constant representing ID Of NO transaction
     */
    static const_interval_idx NO_TRANSACTION = 0xffffffff;

    /*!
     * \brief Stop after exceeding this threshold.
     * A low threshold is required for testing the sliding window algorithm
     */
    //const uint64_t MEMORY_THRESHOLD_BYTES = 1000000000; // 1 GB
    //const uint64_t MEMORY_THRESHOLD_BYTES = 100000000; // 100 MB
    const uint64_t MEMORY_THRESHOLD_BYTES = 500000000; // 500 MB

    /*!
     * \brief Background thread sleep period between checks
     */
    const uint32_t BACKGROUND_THREAD_SLEEP_MS = 100;

    /*!
     * \brief Interval between DB update checks in seconds
     */
    const time_t DB_UPDATE_INTERVAL_S = 10;

private:

    /*!
     * \brief Window range definition
     */
    struct Window {
        uint64_t start;
        uint64_t end;
    };

    /*!
     * \brief Node containing a chunk of data, which may be sparsely populated
     */
    class Node
    {
    public:

        /*!
         * \brief Data stored at a single tick
         */
        struct TickData {

            /*!
             * \brief Constructor
             * \param tick_offset Tick index of this object - relative to containing node's start
             * tick
             * \param num_locations Number of locations in this TransactionDatabase
             * \param prev Previous TickData (if being inserted after another)
             * \param next Next TickData (if being inserted before another)
             */
            TickData(uint64_t _tick_offset, uint32_t num_locations,
                     const TickData* prev = nullptr, const TickData* next = nullptr) :
                tick_offset(_tick_offset),
                data(new interval_idx[num_locations])
            {
                sparta_assert(!prev || prev->tick_offset < _tick_offset);
                sparta_assert(!next || next->tick_offset > _tick_offset);

                // Auto-fill each item in the array based on prev and next TickDatas. If there is a
                // prev and next it means that this is being inserted into the Node::tick_content
                // list.

                if(!prev && !next){
                    memset(&(data[0]), NO_TRANSACTION, sizeof(interval_idx) * num_locations); // "null" all pointers
                    return; // early-out, no existing transactions to look at
                }

                for(uint32_t x = 0; x < num_locations; ++x){
                    if(prev && next && (prev->data[x] == next->data[x])){
                        // Has prev AND has same next or no next
                        data[x] = prev->data[x];
                    }else{
                        // No prev or no next or they are different. This must be empty.
                        data[x] = NO_TRANSACTION;
                    }
                }
            }

            /*!
             * \brief This tick's tick offset
             */
            uint64_t tick_offset;

            /*!
             * \brief Indices into all_intervals_ for this tick
             */
            std::unique_ptr<interval_idx[]> data;
        };

    private:

        /*!
         * \brief All intervals allocated in this Node
         */
        std::vector<Transaction> all_intervals_;

        bool should_del_; //!< Should this node be deleted when next possible

        const uint64_t start_inclusive;
        const uint64_t end_exclusive;
        const uint32_t num_locations;
        uint64_t transaction_bytes_; //!< Bytes used for transactions
        volatile bool complete_; //!< Has this node been completely populated

        /*!
         * \brief Transaction pointers per location for each relevant tick
         */
        std::list<TickData> tick_content;

        /*!
         * \brief How sparsely the node stores its content (e.g. how ticks are
         * skipped)
         */
        uint32_t sparseness;

        /*!
         * \brief How many entries were overwritten because they already had a
         * transaction pointer
         */
        uint32_t overwrites;

        /*!
         * \brief Acquired when this block is being populated. To read it, this
         * mutex must acquired
         */
        mutable std::mutex loading_mutex_;

    public:

        /*!
         * \brief Constructor
         * \param num_locations Number of locations to support. Must be > 0
         */
        Node(uint64_t start_inc, uint64_t size, uint32_t _num_locations) :
            should_del_(false),
            start_inclusive(start_inc),
            end_exclusive(start_inc + size),
            num_locations(_num_locations),
            transaction_bytes_(0),
            complete_(false),
            sparseness(size),
            overwrites(0)
        {
            sparta_assert(size > 0);
            loading_mutex_.lock();
            sparta_assert(num_locations > 0,
                              "A transaction database node requires a location count of 1 or more");
            all_intervals_.reserve(512);

            //tick_content.reset(new interval_idx[size * num_locations]);

            // If NO_TRANSACTION is changed, which it shouldn't be, update the memset in TickData
            sparta_assert(NO_TRANSACTION == 0xffffffff);
            //memset(&(tick_content[0]), 0xffffffff, sizeof(interval_idx) * size * num_locations); // null all pointers

            // Insert the first node at tick-offset = 0 so that there always data to walk.
            tick_content.emplace_back(0, num_locations, nullptr, nullptr);
            sparseness--;
        }

        /*!
         * \brief Destructor
         */
        ~Node()
        {
            if(!complete_){
                // Someone else may be manipulating this. so wait for it
                loading_mutex_.lock();
            }
            loading_mutex_.unlock();
        }

        /*!
         * \brief Gets the mutex associated with loading data into this node.
         * \note To load data into this node or read data from it, the mutex
         * must be held. This allows node loading from a background thread while
         * the forground thread queries the entire transaction database. Only
         * when data is requested from a node (or it must be deleted), does the
         * foreground thread have to wait for this mutex
         */
        std::mutex& getMutex() { return loading_mutex_; }

        /*!
         * \brief Dumps the content of this node where each row is a TickData
         * \param location_start First location to show for each row.
         * \param location_end Exclusive end of locaiton range to show. This
         * location will not be shown. This value is always clamped to maximum
         * number of locations. If < location_start, nothing will be shown.
         * \param tick_entry_limit Number of tick entries to show. 0 if
         * unlimited. Value is always clamped to total number of tick entries
         */
        void dumpContent(std::ostream& o, uint32_t location_start=0, uint32_t location_end=0, uint32_t tick_entry_limit=0) const {
            auto itr = tick_content.begin();
            uint32_t tick_entries = 0;
            uint32_t real_loc_limit = location_end > 0 ? location_end : ~(decltype(real_loc_limit))0;
            if(real_loc_limit > num_locations){
                real_loc_limit = num_locations;
            }
            o << std::setw(8) << "location: ";
            for(uint32_t loc = location_start; loc < real_loc_limit; ++loc){
                o << std::dec << std::setw(4) << loc << ' ';
            }
            o << '\n';
            for(; itr != tick_content.end(); ++itr){
                o << std::dec << std::setw(8) << itr->tick_offset << ": ";
                for(uint32_t loc = location_start; loc < real_loc_limit; ++loc){
                    if(itr->data[loc] == NO_TRANSACTION){
                        o << std::hex << std::setw(4) << "..." << ' ';
                    }else{
                        o << std::hex << std::setw(4) << itr->data[loc] << ' ';
                    }
                }
                o << '\n';
                tick_entries++;
                if(tick_entry_limit != 0 && tick_entries >= tick_entry_limit){
                    break;
                }
            }
            if(itr != tick_content.end()){
                o << "more...\n";
            }

            o << "Up to 20 transactions in location range\n";
            for(uint32_t tids = 0; tids < 20; ++tids){
                if(tids >= all_intervals_.size()){
                    break;
                }
                auto trans = all_intervals_[tids];
                o << "Transaction " << std::dec << trans.transaction_ID << " loc="
                  << std::dec << trans.location_ID << " @ [" << trans.getLeft() << ','
                  << trans.getRight() << ")\n";
            }
        }

        /*!
         * \brief Get the content string from dumpContent
         */
        std::string getContentString(uint32_t location_start=0, uint32_t location_end=0, uint32_t tick_entry_limit=0) const {
            std::stringstream ss;
            dumpContent(ss, location_start, location_end, tick_entry_limit);
            return ss.str();
        }

        /*!
         * \brief Returns the memory used by this node including the node itself
         * and anything allocated my the node.
         */
        uint64_t getSizeInBytes() const
        {
            return sizeof(*this) +
                //(end_exclusive - start_inclusive) * num_locations * sizeof(interval_idx) +
                (tick_content.size() * (num_locations * sizeof(interval_idx) + sizeof(TickData))) +
                transaction_bytes_;
        }

        /*!
         * \brief Get low inclusive endpoint of this node
         */
        uint64_t getStartInclusive() const {
            return start_inclusive;
        }

        /*!
         * \brief Get high exclusive endpoint of this node
         */
        uint64_t getEndExclusive() const {
            return end_exclusive;
        }

        /*!
         * \brief Mark this node as completed and release the loading mutex.
         * The thread that constructs this node must do this
         */
        void markComplete() {
            complete_ = true;
            loading_mutex_.unlock();
        }

        /*!
         * \brief Is this node fully loaded. If false,  it means that it has not
         * been loaded or there was an error loading it.
         * \nhote This is NOT a worker-thread synchronization mechanism
         */
        bool isComplete() const {
            return complete_;
        }

        /*!
         * \brief Add a new transaction to this node
         * \param time_Start inclusive start tick
         * \param time_End exclusive end tick
         */
        template<typename... _Args>
        void addTransaction(Transaction::IntervalDataT time_Start,
                            Transaction::IntervalDataT time_End,
                            uint16_t control_Process_ID,
                            uint64_t transaction_ID,
                            uint32_t location_ID,
                            _Args&&... __args)
        {
            // Interpret the time_End differently depending on whether the
            // endpoint is inclusive or exclusive. This is a property of the
            // pipeline collector.
            const bool IS_TRANSACTION_END_INCLUSIVE = false;
            uint64_t transaction_exclusive_end = time_End;
            if(transaction_exclusive_end > time_Start){
                transaction_exclusive_end -= (uint64_t)IS_TRANSACTION_END_INCLUSIVE; // Compute exclusive endpoint
            }
            sparta_assert(transaction_exclusive_end > start_inclusive);

            // Ensure this location fits within this Node.
            sparta_assert(time_Start < end_exclusive);

            // Clamp end to the valid range for this node
            if(transaction_exclusive_end > end_exclusive){
                transaction_exclusive_end = end_exclusive;
            }

            //uint16_t loc_id = location_ID % num_locations;
            auto loc_id = location_ID;
            sparta_assert(loc_id < num_locations,
                        "Encountered a transaction with location ID=" << loc_id
                        << " when the database window was initialized expecting only "
                        << num_locations << " locations");

            // Track a copy locally in this node
            all_intervals_.emplace_back(time_Start,
                                        time_End, // Use original ending
                                        control_Process_ID,
                                        transaction_ID,
                                        location_ID,
                                        __args...);
            transaction_bytes_ += all_intervals_.back().getSizeInBytes();

            // Index of this transaction within the node-scoped transaction list
            auto trans_pos = all_intervals_.size() - 1;

            // Place pointers to this transaction in every relevant slot
            uint64_t start_cycle_offset = std::max<uint64_t>(time_Start, start_inclusive) - start_inclusive;
            const uint64_t trans_end_exclusive = std::min<uint64_t>(transaction_exclusive_end, end_exclusive);
            const uint64_t end_entry_offset = trans_end_exclusive - start_inclusive;

            // Insert the tick data
            sparta_assert(IS_TRANSACTION_END_INCLUSIVE == false); // Some assumptions of the following routine assume exclusive endpoints
            TickData* prev_td = nullptr;
            bool marked_start = false;
            bool marked_ending = false;
            const bool single_tick_entry = end_entry_offset - start_cycle_offset == 1;
            //! \todo Remove this linear search. Maybe turn it into a binary search?
            //! Or insert backward from end point which will be close to most revent endpoint
            auto itr = tick_content.begin();
            for(; itr != tick_content.end(); ++itr){
                TickData& td = *itr;

                if(td.tick_offset < start_cycle_offset){
                    // Continue - haven't reached first cycle in the transaction
                }else if(td.tick_offset == start_cycle_offset){
                    // Reached a TickData matching the start of this transaction
                    if(td.data[loc_id] != NO_TRANSACTION){
                        overwrites++;
                    }
                    td.data[loc_id] = trans_pos;
                    marked_start = true;
                }else{
                    // td.tick_offset > start_cycle_offset
                     if(td.tick_offset > start_cycle_offset){
                        if(!marked_start){
                            // Passed the start of this transaction while missing the start point. Need to
                            // insert a TickData at the start of the transaction
                            auto new_itr = tick_content.emplace(itr, start_cycle_offset, num_locations, prev_td, &td);
                            prev_td = &(*new_itr);
                            new_itr->data[loc_id] = trans_pos;
                            sparseness--;
                            marked_start = true;
                        }
                    }

                    if(td.tick_offset == end_entry_offset - 1){
                        // Reached final cycle in the transaction
                        // Just update the pointer in this TickData
                        if(td.data[loc_id] != NO_TRANSACTION && td.data[loc_id] != loc_id){
                            overwrites++;
                        }
                        td.data[loc_id] = trans_pos;
                        marked_ending = true;

                    }else if(td.tick_offset >= end_entry_offset){
                        // Reached the (exclusive end) of the transaction or later
                        // Nothing to update here if it already exists. If the ending was never
                        // marked (had an entry added in a TickData), then a TickData must be
                        // inserted now immediately before this tick
                        // Mark last tick INSIDE transaction if not marked and start != ending
                        // single-tick entries are a special case where the last tick of the
                        // transacation IS the startpoint
                        if(!marked_ending && !single_tick_entry){
                            // Insert the new tick which pulls from prev or next.
                            auto new_itr = tick_content.emplace(itr, end_entry_offset-1, num_locations, prev_td, &td);
                            prev_td = &(*new_itr);
                            new_itr->data[loc_id] = trans_pos;
                            sparseness--;
                        }

                        // Need to indicate that the transaction is no longer in this location at
                        // end_entry_offset. If there was aready an entry here, assume it has another
                        // transaction or NO_TRANSACTION at this location.
                        if(td.tick_offset > end_entry_offset){
                            // Insert a new TickData before this indicating that there is NO transaction
                            // in this location after this transaction ends. This new tick data must be
                            // populated with pointers for its locations where the next and previous
                            // TickDatas have the same transaction. We're inserting a TickData in the
                            // middle of a transactions that was sparsely indicated in the file.
                            // Insert the new tick which pulls from prev and next.
                            // Note that td.data[loc_id] will already be NO_TRANSACTION
                            if(end_entry_offset < end_exclusive){ // Do not insert at exclusive endpoint of this NODE
                                auto new_itr = tick_content.emplace(itr, end_entry_offset, num_locations, prev_td, &td);
                                prev_td = &(*new_itr);
                                sparta_assert(new_itr->data[loc_id] == NO_TRANSACTION);
                                sparseness--;
                            }
                        }

                        break;

                    }else{
                        // Current tick-data is before the inclusive end of the transaction. Update
                        // it to point to this transaction
                        if(td.data[loc_id] != NO_TRANSACTION && td.data[loc_id] != loc_id){
                            overwrites++;
                        }
                        td.data[loc_id] = trans_pos;
                    }
                }

                prev_td = &td;
            }

            if(time_Start == time_End){
                // This is a degenerate case and will not properly be handled by the insertion
                // algorithm above. The else-case asumptions about marking the start and attempting
                // to mark the last tick are invalid.
            }else{

                // If the loop terminated because the iterator hit the end. Insert a new TickContent
                if(itr != tick_content.end()){
                    sparta_assert(marked_start,
                                      "Somehow made it through a transaction insertion into node \""
                                      << stringize() << "\" without marking the start in a TickData. "
                                      "Transaction " << std::dec
                                      << transaction_ID << " loc=" << std::dec << location_ID
                                      << " @ [" << time_Start << ',' << time_End << ")\n");
                }else{
                    // Finished iterating all the TickDatas. Add remaining entries

                    if(!marked_start){
                        // Passed the start of this transaction while missing the start point. Need to
                        // insert a TickData at the start of the transaction
                        auto new_itr = tick_content.emplace(itr, start_cycle_offset, num_locations, prev_td, nullptr);
                        prev_td = &(*new_itr);
                        new_itr->data[loc_id] = trans_pos;
                        sparseness--;
                        marked_start = true;
                    }

                    // Mark last tick INSIDE transaction if not marked and start != ending
                    if(!marked_ending && !single_tick_entry){
                        // Allow insertion at end_exclusive (no later)
                        uint64_t entry_tick = end_entry_offset-1;
                        const auto num_ticks_in_node = end_exclusive - start_inclusive;
                        if(entry_tick > num_ticks_in_node){
                            entry_tick = num_ticks_in_node;
                        }
                        auto new_itr = tick_content.emplace(itr, entry_tick, num_locations, prev_td, nullptr);
                        prev_td = &(*new_itr);
                        new_itr->data[loc_id] = trans_pos;
                        sparseness--;
                    }

                    if(end_entry_offset < end_exclusive){ // Do not insert at exclusive endpoint. It will never be accessed
                        auto new_itr = tick_content.emplace(itr, end_entry_offset, num_locations, prev_td, nullptr);
                        sparta_assert(new_itr->data[loc_id] == NO_TRANSACTION);
                        sparseness--;
                    }
                }
            }

            // Sanity check transactions for repeats (this is expensive)
            int64_t last_off = -1;
            itr = tick_content.begin();
            for(; itr != tick_content.end(); ++itr){
                sparta_assert((int64_t)itr->tick_offset > last_off);
                last_off = (int64_t) itr->tick_offset;
            }

            // Dump data for a range of locations. Here, after each insertion
            // The dump can show how the list of tick-datas was modified from
            // the last insertion.
            //dumpContent(std::cout, 4566, 4567, 0);

            //interval_idx * ref_ptr = &(tick_content[(start_cycle_offset*num_locations) + loc_id]);
            //sparta_assert(ref_ptr >= &(tick_content[0]));
            //sparta_assert(ref_ptr < &(tick_content[0]) + (end_exclusive - start_inclusive) * num_locations);
            //uint32_t slots = 0;
            //for(; ref_ptr < &(tick_content[0]) + (end_entry_offset * num_locations); ref_ptr += num_locations){
            //    *ref_ptr = trans_pos;
            //    ++slots;
            //    sparta_assert(ref_ptr < &(tick_content[0]) + (end_exclusive - start_inclusive) * num_locations);
            //}
            //std::cout << "number of slots for " << location_ID << " @ [" << time_Start
            //    << ',' << time_End << ") in " << stringize() << " = " << slots << std::endl;
        }

        std::string stringize() const {
            std::stringstream ss;
            ss << "<Node [" << start_inclusive << ',' << end_exclusive << ") trans="
                << all_intervals_.size();
            if(should_del_){
                ss << " deleteme";
            }
            if(false == complete_){
                ss << " loading";
            }
            if(false == complete_){
                ss << " incomplete";
            }
            ss << ' ' << "tdatas:" << tick_content.size();
            ss << ' ' << "sparse:" << sparseness << "(" << std::setprecision(4) << 100.*float(sparseness)/(end_exclusive-start_inclusive) << "%)";
            ss << ' ' << "overwr:" << overwrites;
            ss << ' ' << std::fixed << getSizeInBytes() / 1000000.0 << " MB>";
            return ss.str();
        }

        /*!
         * \brief Flags this node for deletion. Node can then be freed within a
         * query or by a worker thread if needed
         */
        void flagForDeletion() {
            should_del_ = true;
        }

        /*!
         * \brief Should this node be deleted when convenient?
         * \return True if this block is not currently being loaded and was
         * flagged for deletion
         */
        bool canDelete() const {
            if(complete_){
                return should_del_;
            }
            return false;
        }

        /*!
         * \brief Gets an iterator pointing to the TickData associated witih an
         * absolute tick number of the nearest earlier TickData for that tick
         * number.
         * \return Iterator that is not equivalent to getTickDataEnd. This is
         * allowed because the presence of at least 1 TickData is guaranteed at
         * construction.
         */
        std::list<TickData>::const_iterator getTickData(uint64_t abstime) const {
            sparta_assert(abstime >= start_inclusive && abstime < end_exclusive,
                              "tick (" << abstime << ") being queried is not within range of node "
                              << stringize());
            uint64_t t = abstime - start_inclusive;

            // Mutiple TickDatas Guaranteed by Node constructor. Required for following code to work
            sparta_assert(tick_content.size() > 0);

            // Find the locations
            auto itr = tick_content.cbegin();
            sparta_assert(itr != tick_content.end());
            std::list<TickData>::const_iterator prev_itr = itr;
            while(itr != tick_content.cend()){
                if(itr->tick_offset == t){
                    return itr;
                }
                if(itr->tick_offset > t){
                    return prev_itr;
                }
                prev_itr = itr;
                ++itr;
            }

            return prev_itr;

            //// Constructor guarantees 1 or more locations
            //const_interval_idx * ref_ptr = &(tick_content[(t*num_locations)]);
            //sparta_assert(ref_ptr >= &(tick_content[0]));
            //sparta_assert(ref_ptr < &(tick_content[0]) + (end_exclusive - start_inclusive) * num_locations);
            //return ref_ptr;
        }

        /*!
         * \brief Returns the end iterator associated with the tick data in this
         * Node. This can be compared to the result of getTickData.
         */
        std::list<TickData>::const_iterator getTickDataEnd() const {
            return tick_content.end();
        }

        /*!
         * \bried Returns a vector containing all intervals known to this node.
         * This can be indexed by offsets stored at each location
         */
        const std::vector<Transaction>& getIntervals() const {
            return all_intervals_;
        }
    };


    /*!
     * \brief Manages argos reader callbacks and dumps transaction data into an
     * ordered list of nodes
     */
    class SmartReader : public PipelineDataCallback
    {
        /*!
         * \brief Reader used to get intervals from the event file
         */
        Reader event_reader_;


        /*!
         * \brief Pointer to nodes to be populated from reader callbacks
         */
        std::vector<Node*> const * load_to_nodes_;

        /*!
         * \brief Mutex that must be locked loading through the reader
         */
        std::mutex reader_load_mutex_;

    public:

        /*!
         * \brief Constructor
         * \param file_prefix Prefix of the argos database that will be opened
         * \post Handle to argos database identified by \a file_prefix is open
         */
        SmartReader(const std::string& file_prefix) :
            event_reader_(file_prefix, this),
            load_to_nodes_(nullptr),
            reader(event_reader_)
        {;}

        const Reader& reader;

        /*!
         * \brief Resets the query state on the reader
         */
        void resetQueryState()
        {
            event_reader_.clearLock();
        }

        /*!
         * \brief Locks the non-recursive mutex for the current thread
         */
        void lock() { reader_load_mutex_.lock(); }

        /*!
         * \brief Unlocks the non-recursive mutex for the current thread
         */
        void unlock() { reader_load_mutex_.unlock(); }

        /*!
         * \brief Load a range of data from the reader to a specific set of nodes
         * \param start Start reading from the file at this tick. This should
         * generally be chunk aligned to avoid excess reading
         * \param enbd Stop reading from the file at this tick. This should
         * generally be chunk aligned to avoid excess reading
         * \param load_to vector of Node* indicating which nodes should receive the
         * data being loaded. Because of the nature of file reading, more than 1
         * node's worth of data must be read to see all transaction's that overlap
         * the node (since they are sorted by end-time).
         * \note This is a blocking call
         * \note load_to must not be modified externally while within this call.
         * \pre this reader must be locked via lock by the thread calling this
         * method.
         */
        void loadDataToNodes(uint64_t start, uint64_t end, std::vector<Node*> const * load_to)
        {
            sparta_assert(load_to != nullptr, "cannot loadDataToNodes with a null load_to vector")
            load_to_nodes_ = load_to;
            event_reader_.getWindow(start, end);
            load_to_nodes_ = nullptr;
        }

        bool isUpdated()
        {
            return event_reader_.isUpdated();
        }

        void ackUpdated()
        {
            lock();
            event_reader_.ackUpdated();
            unlock();
        }

    private:

        /*!
         * \brief Add a transaction to this smart reader
         */
        template<typename... _Args>
        void addTransaction_(Transaction::IntervalDataT time_Start,
                             Transaction::IntervalDataT time_End,
                             uint16_t control_Process_ID,
                             uint64_t transaction_ID,
                             uint32_t location_ID,
                             uint16_t flags,
                             _Args&&... __args)
        {
            //std::cout << "Interval " << std::setw(6) << location_ID << " @ ["
            //          << time_Start << ',' << time_End << ")" << std::endl;

            // Guaranteed not nullptr
            std::vector<Node*>::const_iterator node_itr = load_to_nodes_->begin();

            while(node_itr != load_to_nodes_->end()){
                // Note: Assuming exclusove right endpoint of transactions
                // Stop iterating when a node is encountered that starts after this
                // transaction ends
                if((*node_itr)->getStartInclusive() >= time_End){
                    break;
                }else if((*node_itr)->getEndExclusive() > time_Start){
                    // This case should only accept nodes that contain part of this
                    // transaction. findNode can return a node preceeding this
                    // transaction.
                    // Furthermore, only incomplete nodes will be loaded
                    if(false == (*node_itr)->isComplete()){
                        (*node_itr)->addTransaction(time_Start,
                                                    time_End,
                                                    control_Process_ID,
                                                    transaction_ID,
                                                    location_ID,
                                                    flags,
                                                    __args...);
                    }
                }
                ++node_itr;
            }
        }

        /*!
         * \brief Callback from PipelineDataCallback
         */
        void foundTransactionRecord(transaction_t* loc) {
            addTransaction_(loc->time_Start,loc->time_End,loc->control_Process_ID,
                            loc->transaction_ID,loc->location_ID,loc->flags);
        }
        void foundInstRecord(instruction_t* loc) {
            addTransaction_(loc->time_Start,loc->time_End,loc->control_Process_ID,
                            loc->transaction_ID,loc->location_ID,loc->flags,
                            (uint16_t)loc->parent_ID,loc->operation_Code,loc->virtual_ADR,
                            loc->real_ADR);
        }
        void foundMemRecord(memoryoperation_t* loc) {
            addTransaction_(loc->time_Start,loc->time_End,loc->control_Process_ID,
                            loc->transaction_ID,loc->location_ID,loc->flags,
                            (uint16_t)loc->parent_ID,loc->virtual_ADR,loc->real_ADR);
        }
        void foundAnnotationRecord(annotation_t* loc) {
            addTransaction_(loc->time_Start,loc->time_End,loc->control_Process_ID,
                            loc->transaction_ID,loc->location_ID,loc->flags,
                            (uint16_t)loc->parent_ID,loc->length,(const char*)loc->annt);
        }

        void foundPairRecord(pair_t* loc) {
            addTransaction_(loc->time_Start,loc->time_End,loc->control_Process_ID,
                            loc->transaction_ID,loc->location_ID,loc->flags,
                            (uint16_t)loc->parent_ID, loc->length, loc->pairId, loc->sizeOfVector,
                            loc->valueVector, loc->nameVector,
                            loc->stringVector, loc->delimVector);
        }


    } smart_reader_;

    /*!
     * \brief Type for Node list. This must be a list so that objects are
     * perserved
     */
    typedef std::list<Node> node_list_t;

    typedef node_list_t::iterator node_iterator_t;
    typedef node_list_t::const_iterator node_const_iterator_t;

    const std::string file_prefix_; //!< Database filename prefix

    const uint32_t num_locations_; //!< Number of locations in the database

    /*!
     * \brief Data nodes currently help in this class
     * \warning This must be a list type
     */
    node_list_t nodes_;

    /*!
     * \brief Currently within a query?
     */
    bool in_query_;

    /*!
     * \brief Currently loaded window
     */
    Window window_;

    /*!
     * \brief Last query range
     */
    Window last_query_;

    const uint64_t start_tick_; //!< Inclusive
    uint64_t end_tick_; //!< Exclusive

    /*!
     * \brief Size of a chunk in the transaction bd file. Nodes must be
     * contained within a single chunk each
     */
    uint64_t chunk_size_;

    uint64_t node_size_; //!< Size of a node in this window

    /*!
     * \brief Background loader thread
     */
    std::thread background_loader_;

    /*!
     * \brief Mutex that must be locked when manipulating the node list and window
     */
    mutable std::recursive_mutex node_list_mutex_;

    /*!
     * \brief Should the background thread exit?
     */
    volatile bool background_thread_should_exit_;

    /*!
     * \brief Is this interface in verbose mode
     */
    bool verbose_;

    /*!
     * \brief Is this interface polling for database updates
     */
    bool update_enabled_;

    /*!
     * \brief Incremented every time this interface detects a database update
     */
    uint64_t update_ready_;

    /*!
     * \brief Timestamp of last check for DB updates
     */
    time_t last_updated_;

public:

    /*!
     * \brief Callback function signature for each tick encountered during a
     * cycle
     * \null values for the pointers (other than user_data) indicate no data at
     * the specified tick
     */
    typedef void (*callback_fxn)(void* user_data,
                                 uint64_t tick,
                                 const_interval_idx * location_contents,
                                 const TransactionDatabaseInterface::Transaction* transactions,
                                 uint32_t num_locations);

    TransactionDatabaseInterface() = delete;
    TransactionDatabaseInterface(const TransactionDatabaseInterface&) = delete;
    const TransactionDatabaseInterface& operator=(const TransactionDatabaseInterface&) = delete;

    /*!
     * \brief Constructor
     * \param file_prefix Path and prefix of database files
     * \param num_locations Number of locations in the tree
     */
    TransactionDatabaseInterface(const std::string& file_prefix, uint32_t num_locations, bool update_enabled = false) :
        smart_reader_(file_prefix),
        file_prefix_(file_prefix),
        num_locations_(num_locations),
        in_query_(false),
        window_{0,0},
        last_query_{0,0},
        start_tick_(smart_reader_.reader.getCycleFirst()),
        end_tick_(smart_reader_.reader.getCycleLast()),
        chunk_size_(smart_reader_.reader.getChunkSize()),
        node_size_(0),
        background_thread_should_exit_(false),
        verbose_(false),
        update_enabled_(update_enabled),
        update_ready_(0),
        last_updated_(0)
    {
        // Find a node size that is an even integer division of chunk_size_
        const uint64_t MAX_NODE_SIZE = 200000;
        node_size_ = chunk_size_;
        for(uint32_t i = 1; i < 2000; ++i){
            uint64_t temp_size = chunk_size_ / i;
            if(temp_size * i == chunk_size_){
                if(temp_size <= MAX_NODE_SIZE){
                    node_size_ = temp_size;
                    break;
                }
            }
        }
        sparta_assert(node_size_ >= 100,
                          "Size of node could not be determined. Heartbeat ("
                          << chunk_size_ << ") is not a multiple of 100");
        if(node_size_ > MAX_NODE_SIZE){
            std::cerr << "Warning: unable to find a suitable node size evenly divisible by chunk "
                "size (" << chunk_size_ << ")" << std::endl;
        }

        // DO NOT Load an initial window
        // This is just a waste of time whenever the user wants to start at a non-zero tick and
        // because of how close construction of this class tends to be to its usage, there is almost
        // no opportunity to preload data.
        //uint64_t load_end = std::min(start_tick_+1000, end_tick_);
        //load_(start_tick_, load_end);

        // Start background thread
        background_loader_ = std::thread(std::bind(&TransactionDatabaseInterface::backgroundLoader_, this));
    }

    /*!
     * \brief Destructor
     */
    ~TransactionDatabaseInterface()
    {
        background_thread_should_exit_ = true;
        if(background_loader_.get_id() != std::thread::id()){
            background_loader_.join();
        }
    }

    /*!
     * \brief Sets the current verbose logging state of this interface
     * \param verbose New verbose logging state
     */
    void setVerbose(bool verbose) { verbose_ = verbose;}

    /*!
     * \brief Gets the current verbose logging state of this interface
     */
    bool getVerbose() const { return verbose_; }

    /*!
     * \brief Gets the number of ticks in each node
     */
    uint32_t getNodeLength() const { return node_size_; }

    /*!
     * \brief Gets the number of ticks in each heartbeat
     */
    uint64_t getChunkSize() const { return chunk_size_; }

    /*!
     * \brief Resets any temporary query state.
     * \note this is mainly a debugging feature
     */
    void resetQueryState() {

        // TODO: lock access
        in_query_ = false;
        smart_reader_.resetQueryState(); // In case exception occurred during reading
    }

    /*!
     * \brief Returns the inclusive start point of the last query made.
     */
    uint64_t getLastQueryStart() const {
        return last_query_.start;
    }

    /*!
     * \brief Returns the exclusive end point of the last query made. This is
     * equal to 0 if no query has been made yet
     */
    uint64_t getLastQueryEnd() const {
        return last_query_.end;
    }

    /*!
     * \brief Completely unload all nodes and reset the window.
     * Database connection is still maintained. The next query will reload any
     * necessary data
     */
    void unload() {
        std::lock_guard<std::recursive_mutex> lock(node_list_mutex_);

        window_.start = 0;
        window_.end = 0;

        nodes_.clear();
    }

    /*!
     * \brief Perform a range query
     * \param _start_inclusive_inclusive Start tick of query (inclusive)
     * \param _end_inclusive_inclusive End tick of query (exclusive)
     * \param cb Callback funtion for each cycle at which query results are
     * found. Callback will be invoked with \a user_data as the first argument.
     * Callback will be invoked for all ticks in the requested range. It is the
     * callback's responsibility to filter. If there is no data in the
     * transaction database for a given tick, the callback location_content and
     * transaction pointers will be nullptr and num_locations must be ignored.
     * \param user_data user-defined data pointer. Given as first argument in
     * \a cb function. This is often a class pointer to which the method is
     * forwarded
     * \param modify_tracking Treat the current query as the new query range.
     * Normally, the last query is used to predict the next data that will be
     * needed. If a small query is made after a large query (and inside that
     * previous query's range), some data from the large query may be dropped in
     * the background. Normally these large queries should have their data held
     * in memory because the next query will tend to be that range shifted by a
     * few ticks in either direction. To preven small queries within these
     * ranges from breaking this prediction, modify_tracking can be set to false
     * Data outside this new range may be unloaded and replaced with data closer
     * to one endpoint of the range. If false, data will NOT be loaded or
     * unloaded, but the queried range must be a subset of the prior query range.
     * \pre Must not be call from within a callback of another query.
     */
    void query(uint64_t _start_inclusive,
               uint64_t _end_inclusive,
               callback_fxn cb,
               void* user_data=nullptr,
               bool modify_tracking=true)
    {
        std::lock_guard<std::recursive_mutex> lock(node_list_mutex_);

        sparta_assert(_end_inclusive >= _start_inclusive,
                          "end point in query must be >= start point");

        // Filter invalid accesses
        if(_start_inclusive >= end_tick_ || _end_inclusive < start_tick_){
            return;
        }

        // Clamp to file range
        uint64_t start_inclusive = std::max(_start_inclusive, start_tick_);
        uint64_t end_exclusive = std::min(_end_inclusive+1, end_tick_);

        // This is to prevent recursion when querying from within callback
        // functions. This has nothing to do with thread safety.
        sparta_assert(false == in_query_,
                          "Cannot query transaction database from within another query. If a query "
                          "threw an exception, use resetQueryState() to recover before the next "
                          "query")

        if(!modify_tracking){
            // Ensure that this query range is inside the last_query range
            sparta_assert(start_inclusive >= window_.start,
                              "Pipeout database query with modify_tracking=false was not inside "
                              "prior query range. query start = " << start_inclusive << " while "
                              "previous (loaded) start = " << window_.start);
            sparta_assert(end_exclusive <= window_.end,
                              "Pipeout database query with modify_tracking=false was not inside "
                              "prior query range. query end exclusive = " << end_exclusive
                              << " while previous (loaded) end exclusive = " << window_.end);
        }

        // Set in query after all non-fatal exceptions
        in_query_ = true;

        if(modify_tracking){
            // Store this latest query for later use. Ensure this is modified and
            // read only with node_list_mutex acquired
            last_query_.start = start_inclusive;
            last_query_.end = end_exclusive;

            // Ensure that necessary data is loaded immediately.
            if(start_inclusive < window_.start || end_exclusive > window_.end) {

                load_(start_inclusive,
                      end_exclusive); // exclusive end
            }
        }

        uint64_t t = _start_inclusive;
        node_iterator_t itr = findNode(t);

        if(itr != nodes_.end()){
            // Now make some callbacks with no data for the start of the
            // requested range up to the first block found
            while(t < itr->getStartInclusive()){
                cb(user_data, t, nullptr, nullptr, 0);
                ++t;
            }

            //itr->dumpContent(std::cout, 890, 905, 0);
        }

        while(itr != nodes_.end()){
            //std::cout << "Looking at node " << itr->stringize() << std::endl;
            // Assuming exclusive right endpoint of transactions

            if(itr->getStartInclusive() <= t){
                // Calculate limit for iterating in this node
                uint64_t endpoint_exclusive = itr->getEndExclusive();
                if(endpoint_exclusive >= end_exclusive){
                    endpoint_exclusive = end_exclusive;
                }

                // Iterate through necessary ticks in this node.
                // Wait for it to be loaded first
                if(itr->getMutex().try_lock() == false){
                    std::cout << "*** Waiting for node to finish loading..." << itr->stringize() << std::endl;
                    itr->getMutex().lock();
                }

                auto tick_itr = itr->getTickData(t);
                const auto tick_itr_end = itr->getTickDataEnd();
                sparta_assert(tick_itr != tick_itr_end);
                const Node::TickData* td = &(*tick_itr);
                sparta_assert(td->tick_offset + itr->getStartInclusive() <= t);
                while(t < endpoint_exclusive && tick_itr != itr->getTickDataEnd()){
                    if(t > tick_itr->tick_offset + itr->getStartInclusive()){
                        // Current callback tick has passed this tick iterator.
                        tick_itr++;
                    }
                    if(tick_itr != tick_itr_end && tick_itr->tick_offset + itr->getStartInclusive() <= t){
                        // Current callback tick has caught up with the tick iterator. Point
                        // "td" to the current iterator's TickData because it is at or before
                        // the current callback time (t)
                        td = &(*tick_itr);
                    }

                    // VERY SLOW SANITY CHECKING.
                    // Ensures all valid transactions for the current callback tick (t).
                    // Re-enable only for debugging.
                    //for(uint32_t loc = 0; loc < num_locations_; loc++){
                    //    interval_idx idx = td->data[loc];
                    //    if(idx != NO_TRANSACTION){
                    //        const Transaction* trans = &itr->getIntervals()[idx];
                    //        sparta_assert(trans->getLeft() <= t && trans->getRight() > t);
                    //    }
                    //}

                    cb(user_data, t, td->data.get(), &itr->getIntervals()[0], num_locations_);
                    ++t;
                }

                // Finish up callbacks with null data because there is no more tick data in this node
                if(tick_itr == itr->getTickDataEnd()){
                    while(t < endpoint_exclusive){
                        cb(user_data, t, nullptr, nullptr, 0);
                        ++t;
                    }
                }

                itr->getMutex().unlock();

                // Has t passed the end time of the query within the file range?
                // Note that the query endpoint is inclusive, so t must get to end_exclusive
                if(t >= end_exclusive){
                    sparta_assert(t == end_exclusive);

                    // Now make some callbacks with no data for the rest of the
                    // requested range so that the viewer can just blank out the
                    // remaining transactions
                    while(t <= _end_inclusive){
                        cb(user_data, t, nullptr, nullptr, 0);
                        ++t;
                    }

                    in_query_ = false;

                    verifyValidWindow_(); // Check that the window was properly updated

                    return; // Done
                }

                // Falling through to here meant that iteration reached the end
                // of this node
                sparta_assert(t == itr->getEndExclusive());
            }else{
                in_query_ = false;

                // This probably indicates that window was wrong and something
                // should have been loaded for this query.
                sparta_assert(false,
                                  "Exceeded end of blocks at " << t << " where block start is "
                                  << itr->getStartInclusive());
            }
            ++itr;
        }

        in_query_ = false;
        sparta_assert(0,
                          "Unexpected end of iteration when querying for " << _start_inclusive
                          << " to " << _end_inclusive << " clamped down to [" << start_inclusive
                          << ", " << end_exclusive << ")");
    }

    /*!
     * \brief Get the inclusive start cycle in the event file
     */
    uint64_t getFileStart() const {
        return start_tick_;
    }

    /*!
     * \brief Get the exclusive end cycle in the event file
     */
    uint64_t getFileEnd() const {
        // We only need to acquire a lock if there's a chance that the value of end_tick_ could change.
        // This can only happen if update_enabled_ == true
        if(update_enabled_)
        {
            std::lock_guard<std::recursive_mutex> lock(node_list_mutex_);
        }
        return end_tick_;
    }

    /*!
     * \brief Get the inclusive start cycle in the currently loaded window
     */
    uint64_t getWindowStart() const {
        return window_.start;
    }

    /*!
     * \brief Get the exclusive end cycle in the currently loaded window
     */
    uint64_t getWindowEnd() const {
        return window_.end;
    }

    uint32_t getFileVersion() const {
        return smart_reader_.reader.getVersion();
    }

    std::ostream& writeNodeStates(std::ostream& o) const {
        std::lock_guard<std::recursive_mutex> lock(node_list_mutex_);

        uint32_t idx = 0;
        for(const auto& n : nodes_){
            o << std::setw(5) << idx << ' ' << n.stringize() << std::endl;
            ++idx;
        }
        return o;
    }

    std::string getNodeStates() const {
        std::stringstream ss;
        writeNodeStates(ss);
        return ss.str();
    }

    std::string getNodeDump(uint32_t node_idx, uint32_t location_start=0, uint32_t location_end=0, uint32_t tick_entry_limit=0) const {
        uint32_t idx = 0;
        for(const auto& n : nodes_){
            if(idx == node_idx){
                return n.getContentString(location_start, location_end, tick_entry_limit);
            }
            ++idx;
        }
        return "";
    }

    std::string stringize() const {
        std::lock_guard<std::recursive_mutex> lock(node_list_mutex_);

        std::stringstream ss;
        ss << "<TransactionDatabase \"" << file_prefix_
            << "\" total=[" << start_tick_ << ','
            << end_tick_ << ") window=[" << window_.start << ',' << window_.end << ") "
            << "lastq=[" << last_query_.start << "," << last_query_.end << ") "
            << std::fixed << getSizeInBytes()/1000000000.0 << " GB>";
        return ss.str();
    }

    uint64_t getSizeInBytes() const {
        std::lock_guard<std::recursive_mutex> lock(node_list_mutex_);

        uint64_t sum = 0;
        for(const auto& n : nodes_){
            sum += n.getSizeInBytes();
        }
        return sum;
    }

    bool isFileUpdated(bool force = false)
    {
        time_t cur_time = time(NULL);
        if(!force)
        {
            if((cur_time - last_updated_) >= DB_UPDATE_INTERVAL_S)
            {
                last_updated_ = cur_time;
            }
            else
            {
                return false;
            }
        }
        else
        {
            last_updated_ = cur_time;
        }
        bool result = smart_reader_.isUpdated();
        if(result)
        {
            smart_reader_.ackUpdated();
        }
        return result;
    }

    bool updateReady()
    {
        std::lock_guard<std::recursive_mutex> lock(node_list_mutex_);

        return update_ready_ > 0;
    }

    void ackUpdate()
    {
        std::lock_guard<std::recursive_mutex> lock(node_list_mutex_);

        if(update_ready_ > 0)
        {
            update_ready_--;
        }
    }

    void enableUpdate()
    {
        std::lock_guard<std::recursive_mutex> lock(node_list_mutex_);

        update_enabled_ = true;
    }

    void disableUpdate()
    {
        std::lock_guard<std::recursive_mutex> lock(node_list_mutex_);

        update_enabled_ = false;
    }

    void forceUpdate()
    {
        std::lock_guard<std::recursive_mutex> lock(node_list_mutex_);

        if(isFileUpdated(true))
        {
            end_tick_ = smart_reader_.reader.getCycleLast();
            unload();
        }
    }

private:

    /*!
     * \brief Backround thread loader
     */
    void backgroundLoader_() {
        // NOTE: cannot perform this check immediately since the background_loader_ object might not
        // be complete
        //// Ensue that this function is called only in the background loader thread
        //sparta_assert(background_loader_.get_id() == std::this_thread::get_id(),
        //            background_loader_.get_id() << " != " std::this_thread::get_id());

        while(1){
            std::this_thread::sleep_for(std::chrono::milliseconds(BACKGROUND_THREAD_SLEEP_MS));
            if(background_thread_should_exit_){
                return; // End of thread
            }

            if(node_list_mutex_.try_lock()){
                bool needs_update = false;
                if(update_enabled_ && isFileUpdated())
                {
                    end_tick_ = smart_reader_.reader.getCycleLast();
                    unload();
                    needs_update = true;
                }

                try{
                    sparta_assert(window_.start % node_size_ == 0);
                    sparta_assert(window_.end % node_size_ == 0);

                    // Pick an end to load on based on how close the last
                    // query window was to the loaded window edges
                    uint64_t low_distance;
                    if(last_query_.start <= start_tick_){
                        low_distance = 0; // Nothing to load
                    }else{
                        low_distance = last_query_.start - window_.start;
                    }
                    uint64_t high_distance;
                    if(last_query_.end >= end_tick_){
                        high_distance = 0; // Nothing to load
                    }else{
                        high_distance = window_.end - last_query_.end;
                    }

                    // Did we hit the memory ceiling for this window. If so, loading new nodes will
                    // be prevented unless there is a significant imbalance in the window size
                    // around the last query.
                    // Note that we're guaranteed to contain all necessary data from the last query
                    // in the window because that is handled in the foreground. Background loading
                    // (here) is for additional window loading only
                    const bool hit_memory_ceiling = getSizeInBytes() >= MEMORY_THRESHOLD_BYTES;

                    uint64_t load_node_pos = ~(uint64_t)0;
                    const uint64_t low_pos = window_.start - node_size_;
                    const uint64_t high_pos = window_.end;
                    const bool low_limited = window_.start <= start_tick_; // No more to load on the low-end, window reached start_tick
                    const bool high_limited = window_.end >= end_tick_; // No more to load on the high-end, window reached end_tick
                    if(low_distance < high_distance && !low_limited){
                        // Closer to low side and there is data to load here
                        // If we hit the memory ceiling, ensure that loading on the low-end of the
                        // window (and evicting the high end) will not make the high-end closer to
                        // the last query end and cause a ping-pong loading effect.
                        if(!hit_memory_ceiling || (low_distance + node_size_ < high_distance)){
                            load_node_pos = low_pos;
                        }
                    }else if(high_distance > 0 && !high_limited){
                        // Closer on high side and there is data to load here
                        // If we hit the memory ceiling, ensure that loading on the high-end of the
                        // window (and evicting the low end) will not make the low-end closer to the
                        // last query start and cause a ping-pong loading effect.
                        if(!hit_memory_ceiling || (high_distance + node_size_ < low_distance)){
                            load_node_pos = high_pos;
                        }
                    }

                    int64_t dist_diff;
                    //if(high_distance == ~(uint64_t)0){
                    //    if(low_distance == ~(uint64_t)0){
                    //        dist_diff = 0; // Full file is loaded. Do not evict anything
                    //    }else{
                    //        dist_diff = low_distance; // window reaches upper limit. Difference is lower_distance
                    //    }
                    //}else if(low_distance == ~(uint64_t)0){
                    //    dist_diff = high_distance;
                    //}else{
                        dist_diff = std::abs(((int64_t)high_distance) - ((int64_t)low_distance));
                    //}

                    if(load_node_pos < ~(uint64_t)0){
                        // Check whether it is feasible to add more nodes
                        if(hit_memory_ceiling){
                            // Exceeded memory threshold. May need to load new
                            // blocks, but others must be dropped first. Determine which
                            // to add and which can be dropped. First, a switch cannot be made
                            // unless one side is much further from the last query than the other.
                            // Otherwise, the extra node will be moved from the low to high end
                            // every time.

                            //std::cout << "|<--" << low_distance << "--[" << last_query_.start << ", "
                            //    << last_query_.end << "]--" << high_distance << "-->|      (dist_diff="
                            //    << dist_diff << ")" << std::endl;

                            if(nodes_.size() == 1 ||
                                dist_diff <= (int64_t)node_size_){
                                // Cannot possibly drop anything and maintain window, so cannot load anything new
                                load_node_pos = ~(uint64_t)0;
                            }else{
                                //if(verbose_){
                                //    std::cout << "(background)" << window_.start << "|<--" << low_distance << "--[" << last_query_.start << ", "
                                //        << last_query_.end << "]--" << high_distance << "-->|" << window_.end << "   (dist_diff="
                                //        << dist_diff << ")" << std::endl;
                                //}
                                // See if the opposite block is outside of the query range
                                if(load_node_pos == low_pos && (!high_limited || high_distance >= 2*node_size_)){
                                    // Get opposite node
                                    const node_iterator_t delitr = findNode(window_.end - 1);
                                    if(nodes_.end() != delitr){
                                        if(delitr->isComplete() && delitr->getStartInclusive() >= last_query_.end){
                                            if(verbose_){
                                                std::cout << "(background) removing node:"
                                                    << delitr->stringize() << std::endl;
                                            }
                                            window_.end = delitr->getStartInclusive(); // Move end lower
                                            nodes_.erase(delitr);
                                        }else{
                                            if(verbose_){
                                                std::cout << "(background) want to slide left, but cannot delete "
                                                    << delitr->stringize() << std::endl;
                                            }
                                            load_node_pos = ~(uint64_t)0; // Cannot delete this node
                                        }
                                    }else{
                                        if(verbose_){
                                            std::cout << "(background) want to slide left, but cannot find node containing "
                                                    << window_.end - 1 << std::endl;
                                        }
                                        load_node_pos = ~(uint64_t)0; // Cannot delete this node
                                    }
                                }else if(!low_limited || low_distance >= 2*node_size_){ // given: load_node_pos == high_pos
                                    node_iterator_t delitr = findNode(window_.start);
                                    if(nodes_.end() != delitr){
                                        if(delitr->isComplete() && delitr->getEndExclusive() <= last_query_.start){
                                            if(verbose_){
                                                std::cout << "(background) removing node:"
                                                    << delitr->stringize() << std::endl;
                                            }
                                            window_.start = delitr->getEndExclusive(); // Move start higher
                                            nodes_.erase(delitr);
                                        }else{
                                            if(verbose_){
                                                std::cout << "(background) want to slide right, but cannot delete "
                                                    << delitr->stringize() << std::endl;
                                            }
                                            load_node_pos = ~(uint64_t)0; // Cannot delete this node
                                        }
                                    }else{
                                        if(verbose_){
                                            std::cout << "(background) want to slide right, but cannot find node containing "
                                                    << window_.start << std::endl;
                                        }
                                        load_node_pos = ~(uint64_t)0; // Cannot delete this node
                                    }
                                }else{
                                    load_node_pos = ~(uint64_t)0;
                                }
                            }
                        }
                    }

                    if(load_node_pos < ~(uint64_t)0){
                        if(verbose_){
                            std::cout << "(background) memory use is " << getSizeInBytes() / 1000000000.0 << " GB" << std::endl;
                        }

                        node_iterator_t itr = findNode(load_node_pos);

                        nodes_.emplace(itr, load_node_pos, node_size_, num_locations_);
                        Node& added = *std::prev(itr);
                        if(verbose_){
                            std::cout << "(background) inserting Node  @ " << load_node_pos << " size " << node_size_ << std::endl;
                        }

                        // Update window to contain this new node
                        if(window_.start > load_node_pos){
                            window_.start = load_node_pos;
                        }

                        if(window_.end < load_node_pos + node_size_){
                            window_.end = load_node_pos + node_size_;
                        }

                        // Defferred read to avoid walking over the same chunks when
                        uint64_t chunk_start = chunk_size_ * (load_node_pos/chunk_size_);
                        if(verbose_){
                            std::cout << "(background) loading <CHUNK> @ " << chunk_start << " size " << chunk_size_ << std::endl;
                        }

                        node_list_mutex_.unlock();

                        std::vector<Node*> added_nodes{&added};
                        smart_reader_.lock();

                        double t_start = 0;
                        if(verbose_){
                            t_start = sparta::TimeManager::getTimeManager().getAbsoluteSeconds();
                        }

                        smart_reader_.loadDataToNodes(chunk_start, chunk_start + chunk_size_, &added_nodes);
                        if(verbose_){
                            auto t_delta = sparta::TimeManager::getTimeManager().getAbsoluteSeconds() - t_start;
                            std::cout << "(background)    took " << t_delta << " seconds" << std::endl;
                        }

                        added.markComplete();
                        if(verbose_){
                            std::cout << "(background) marking complete: " << added.stringize() << std::endl;
                        }

                        smart_reader_.unlock();

                        node_list_mutex_.lock();

                        if(verbose_){
                            std::cout << "(background) transactiondb: " << stringize() << std::endl;
                            std::cout << "(background) " << window_.start << "|<--" << low_distance << "--[" << last_query_.start << ", "
                                        << last_query_.end << "]--" << high_distance << "-->|" << window_.end << "   (dist_diff="
                                        << dist_diff << ")" << std::endl;
                        }
                    }
                }catch(...){
                    node_list_mutex_.unlock();
                    throw;
                }

                verifyValidWindow_();

                if(update_enabled_ && needs_update)
                {
                    update_ready_++;
                }

                node_list_mutex_.unlock();
            }
        }
    }

    /*!
     * \brief
     * \pre Requiresthe node_list_mutex_
     */
    void verifyValidWindow_()
    {
        // Verify that the update algorithm updated the window range correctly
        if(nodes_.size() > 1){
            sparta_assert(nodes_.begin()->getStartInclusive() == window_.start);
            sparta_assert(nodes_.rbegin()->getEndExclusive() == window_.end);
        }
    }

    /*!
     * \brief Loads data necessary to populate the given range
     */
    void load_(uint64_t start_inclusive, uint64_t end_exclusive)
    {
        if(verbose_){
            std::cout << "(main) Attempting to load [" << start_inclusive << ", "
                      << end_exclusive << ")" << std::endl;
        }

        std::lock_guard<std::recursive_mutex> lock(node_list_mutex_);

        sparta_assert(start_inclusive >= start_tick_);
        sparta_assert(end_exclusive <= end_tick_);

        // Find nodes. Round down to nearest chunk to start
        //uint64_t chunk_start = uint64_t(start_inclusive / chunk_size_) * chunk_size_;
        const uint64_t load_start = uint64_t(start_inclusive / node_size_) * node_size_;

        // cur_pos is current loading tick. It increases in multiples of
        // chunk_size_. As loaded chunks are encountered or new chunks are loaded,
        // cur_pos is increased.
        uint64_t cur_pos = load_start;

        //std::cout << "Loading [" << start_inclusive << "," << end_exclusive << ")"
        //    << " => starting with "<<  chunk_start << std::endl;

        // Flag any nodes before the load range for deletion
        node_iterator_t itr = nodes_.begin();
        if(itr != nodes_.end()){
            // Window begins where there is valid data
            window_.start = itr->getStartInclusive();
            do{
                if(itr->getEndExclusive() <= cur_pos){
                    if(verbose_){
                        std::cout << "(main) Flagging for deletion: " << itr->stringize() << std::endl;
                    }
                    itr->flagForDeletion();
                }else{
                    // Stop walking through these nodes because this one is after
                    // (or overlaps) the current load position
                    break;
                }

                ++itr;
            }while(itr != nodes_.end());
        }else{
            // Window begins where data will first be loaded
            window_.start = cur_pos;
        }

        // Insert nodes until cur_pos reaches the end of the range
        std::vector<uint64_t> chunks_to_read; // Chunks containing newly created nodes
        std::vector<Node*> added_nodes;
        while(cur_pos < end_exclusive){

            // Incomplete nodes are ok. Just don't read their data
            //// Check for incomplete (BAD) nodes.
            //// These can occur if an exception occurs during loading
            //if(itr != nodes_.end() && itr->isComplete() == false){
            //    // Remove this incomplete node
            //    std::cerr << "Incomplete node encountered: " << itr->stringize()
            //        << ", removing and reloading." << std::endl;
            //    node_iterator_t temp = itr;
            //    ++itr; // Move to next, allowing a replacement to be added
            //    nodes_.erase(temp);
            //}

            // Add or skip the current element
            if(itr == nodes_.end() ||
                cur_pos < itr->getStartInclusive()){
                // Assuming exclusive right endpoint of transactions
                //nodes_.emplace(itr, Node(cur_pos, chunk_size_, num_locations_));
                sparta_assert(cur_pos % node_size_ == 0);
                nodes_.emplace(itr, cur_pos, node_size_, num_locations_);
                added_nodes.push_back(&*(std::prev(itr)));
                if(verbose_){
                    std::cout << "(main)  Inserting Node  @ " << cur_pos << " size " << node_size_ << std::endl;
                }

                // Update window
                if(window_.start > cur_pos){
                    window_.start = cur_pos;
                }

                // Defferred read to avoid walking over the same chunks when
                uint64_t chunk_start = chunk_size_ * (cur_pos/chunk_size_);
                if(chunks_to_read.size() == 0 or chunks_to_read[chunks_to_read.size() - 1] != chunk_start){
                    chunks_to_read.push_back(chunk_start);
                }
            }else{
                if(verbose_){
                    std::cout << "(main) Skipping insertion @ " << cur_pos << " ended="
                        << (itr==nodes_.end()) << ", node start=" << itr->getStartInclusive() << std::endl;
                }
                ++itr;
            }
            cur_pos += node_size_;
        }

        smart_reader_.lock();

        // Load the chunks which broadcast to 1 or more Nodes
        // Because node_size_ is an even division of chunk_size_, only the chunk
        // at chunk_start must be read
        for(uint64_t chunk_start : chunks_to_read) {
            double t_start = 0;
            if(verbose_){
                std::cout << "(main) Loading <CHUNK> @ " << chunk_start << " size " << chunk_start + chunk_size_ << std::endl;
                t_start = sparta::TimeManager::getTimeManager().getAbsoluteSeconds();
            }
            smart_reader_.loadDataToNodes(chunk_start, chunk_start + chunk_size_, &added_nodes);
            if(verbose_){
                auto t_delta = sparta::TimeManager::getTimeManager().getAbsoluteSeconds() - t_start;
                std::cout << "(main)    took " << t_delta << " seconds" << std::endl;
            }
        }

        // Mark fully loaded, newly constructed nodes as valid
        for(auto& n : added_nodes){
            n->markComplete();
            if(verbose_){
                std::cout << "(main) marking complete: " << n->stringize() << std::endl;
            }
        }

        smart_reader_.unlock();

        if(verbose_){
            std::cout << "(main) Added nodes marked as complete" << std::endl;
        }

        // Walk through nodes out of the load range and flag them for deletion
        if(itr != nodes_.end()){
            if(cur_pos == itr->getStartInclusive()){
                // Contiguous nodes, flag for deletion
                do{
                    if(verbose_){
                        std::cout << "(main) Flagging for deletion: " << itr->stringize() << std::endl;
                    }
                    itr->flagForDeletion();
                    cur_pos = itr->getEndExclusive();
                    ++itr;
                }while(itr != nodes_.end());
            }else{
                // Non-contiguous nodes. Gaps are not allowed. Remove immediately
                do{
                    node_iterator_t temp = itr++;
                    if(verbose_){
                        std::cout << "(main) Erasing non-contiguous node following data: "
                            << temp->stringize() << std::endl;
                    }
                    nodes_.erase(temp);
                }while(itr != nodes_.end());
            }
        }

        // New cur pos at the end of the data
        window_.end = cur_pos;

        // Verify window endpoints surround load endpoints
        sparta_assert(window_.start <= start_inclusive);
        sparta_assert(window_.end >= end_exclusive);

        // Clean up deleted nodes for now. Result must be a contiguous series of
        // nodes, implying that cleanup can only remove from the ends.
        // Also adjusts window_
        //
        // This will be done by a thread later and controlled by a memory and
        // idle threshold
        node_iterator_t n = nodes_.begin();
        bool keeper_encountered = false; // Have any nodes been kept yet?
        while(n != nodes_.end()){
            if(n->canDelete()){
                // Encountered a node flagged for deletion
                if(verbose_){
                    std::cout << "(main) CAN delete node " << n->stringize() << std::endl;
                }
                if(true == keeper_encountered){
                    // Move window end to beginning of this node
                    // This should be hit on the first node after a stream of 1
                    // or more nodes that were kept (not deleted)
                    if(window_.end > n->getStartInclusive()){
                        window_.end = n->getStartInclusive();
                    }
                }

                node_iterator_t temp = n;
                ++n;
                nodes_.erase(temp);
            }else{
                if(verbose_){
                    std::cout << "(main) can NOT delete node " << n->stringize() << std::endl;
                }
                // Encountered first node that will be kept in the list
                if(false == keeper_encountered){
                    // This is the first node to keep.
                    // Move window start to start of this node
                    window_.start = n->getStartInclusive();
                }

                ++n;
                keeper_encountered = true;
            }
        }

        // In the case where this happens (whicih it really shouldn't), reset
        // the window so the next load will be forced to load everything
        if(nodes_.size() == 0){
            window_.start = 0;
            window_.end = 0;
        }

        if(verbose_){
            std::cout << "(main) transactiondb: " << stringize() << std::endl;
            std::cout << "(main) " << window_.start << "|<--" << " ... " << "--[" << last_query_.start << ", "
                        << last_query_.end << "]--" << " ... " << "-->|" << window_.end << std::endl;
        }
    }

    /*!
     * \brief Finds first node containing tick or, if no nodes contain tick, the
     * first node node after the tick.
     */
    node_iterator_t findNode(uint64_t tick)
    {
        node_iterator_t n = nodes_.begin();
        if(n == nodes_.end()){
            return n;
        }
        if(n->getStartInclusive() > tick){
            return nodes_.begin();
        }
        for(; n != nodes_.end(); ++n){
            if(n->getEndExclusive() > tick){
                return n;
            }
        }
        return nodes_.end();
    }
};

    } // argos
} // sparta

