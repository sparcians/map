// <Queue.hpp> -*- C++ -*-


/**
 * \file   Queue.hpp
 * \brief  Defines the Queue class used for queuing data
 *
 */

#ifndef __QUEUE_EXP_H__
#define __QUEUE_EXP_H__

#include <inttypes.h>
#include <vector>
#include <type_traits>

#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/ports/Port.hpp"
#include "sparta/statistics/CycleHistogram.hpp"
#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/collection/IterableCollector.hpp"
#include "sparta/statistics/InstrumentationNode.hpp"

namespace sparta
{
    /**
     * \class Queue
     * \brief A data structure that allows appending at the back and
     * invalidating from the front.
     *
     * The Queue allows user to push data to the back of the queue and
     * pop it from the front.
     *
     * The queue does not manage any type of state delaying. In order
     * to use the queue as a present state/next state queue, the user
     * should use delays when writing to the queue's ports or
     * listening to the queue's ports.
     *
     * The only precedence that the Queue follows is that
     * invalidations precede writes.
     *
     * The queue can also be used without the port mechanism via
     * public methods push and pop. The pop method is special in that
     * it will return an iterator which points to that entry in
     * Queue. At any time the queue entry can be queried for that
     * data's location in the queue via its public getIndex() method.
     *
     * Example usage
     * \code
     * Queue<uint32_t> queue;
     * Queue<uint32_t>::iterator entry = queue.push(5);
     * Queue<uint32_t>::iterator entry2 = queue.push(52);
     * // where is the entry?
     * uint32_t = entry.getIndex();
     * assert(entry.getIndex() == 0);
     * assert(entry2.getIndex() == 1);
     * assert(queue.read(entry.getIndex() == 5));
     *
     * // QueueIterator's respond to comparison operators. Their index is compared.
     * assert(entry < entry2);
     *
     *
     * queue.pop();
     * assert(entry2.getIndex() == 0);
     * \endcode
     *
     */
    template <class DataT>
    class Queue
    {
        /// Update the passed index value to reflect the current
        /// position of the tail.  The tail always represents what the
        /// client considers index of zero
        uint32_t convertIndex_(uint32_t idx) const
        {
            idx = (vector_size_-1)&(idx+current_zero_pos_);
            return idx;
        }

        // Convert the physical index to a logical index using the
        // Queue's understanding of it's physical zero's location
        uint32_t convertPhysicalIndex_(uint32_t physical_idx) const
        {
            return offsetIndexBy_(physical_idx, -current_zero_pos_);
        }

        /// Find the next value that is greater than or equal to the
        /// paramenter
        uint32_t nextPowerOfTwo_(uint32_t val) const
        {
            if(val < 2) {
                return 1ull;
            }
            return 1ull << ((sizeof(uint64_t) * 8) - __builtin_clzll(val - 1ull));
        }

        /// Increment an index value. It is important to make sure
        /// that the index rolls over to zero if it goes above the
        /// bounds of our list.
        uint32_t incrementIndexValue_(uint32_t val) const
        {
            val = (vector_size_-1)&(val+1);
            return val;
        }

        /// Decrement an index value. It is important to make sure
        /// that the index rolls over to zero if it goes above the
        /// bounds of our list.
        uint32_t decrementIndexValue_(uint32_t val) const
        {
            val = (vector_size_-1)&(val-1);
            return val;
        }

        uint32_t offsetIndexBy_(uint32_t val, int offset) const
        {
            return (vector_size_-1)&(val+offset);
        }

        /// Determine whether or not an index value is currently
        /// within the bounds of valid objects
        bool isIndexInValidRange_(uint32_t idx)const
        {
            // We convert idx to the user's perception of idx.
            // then make sure it is smaller than the total_valid_ entries the queue holds
            uint32_t check = offsetIndexBy_(idx, -current_zero_pos_);
            if(check < total_valid_){
                return true;
            }
            else{
                return false;
            }
        }

        uint64_t numOngoingInvalidations_()const { return ongoing_total_invalidations_; }
    public:

        /// Typedef for the DataT
        using value_type = DataT;

        /// Typedef for the QueueType
        typedef Queue<value_type> QueueType;

        // Typedef for size_type
        typedef uint32_t size_type;

        /**
         * \class QueueIterator
         * \brief Class that alows queue elements to be accessed like a normal stl iterator.
         * Queue iterator is a bidirectional sequential iterator
         * isValid() method checks the validity of the iterator.
         * This method can be used to check if the data in Queue has not yet been popped out
         * Increment, decrement , dereferencing,less than and greater than operations are provided
         *
         */
        template <bool is_const_iterator = true>
        class QueueIterator : public std::iterator<std::bidirectional_iterator_tag, value_type>
        {
        private:
            typedef typename std::conditional<is_const_iterator,
                                              const value_type &, value_type &>::type DataReferenceType;
            typedef typename std::conditional<is_const_iterator,
                                              const QueueType * , QueueType * >::type QueuePointerType;
        public:

            /**
             * \brief construct.
             * \param queue  a pointer to the underlaying queue.
             * \param physical_idx a pointer to the entry in data array
             * \param unique_id this helps to keep track of its validity.
             */
            QueueIterator(QueuePointerType queue, uint32_t physical_idx, uint64_t unique_id) :
                attached_queue_(queue),
                physical_idx_(physical_idx),
                unique_id_(unique_id)
            {}
            /**
             * \brief construct.
             * \param q  a pointer to the underlaying queue.
             * \param begin_end if true iterator points to tail, if false points to head
             */
            QueueIterator(QueuePointerType q, bool begin_end):
                attached_queue_(q)
            {
                if(begin_end){
                    physical_idx_ = q->current_tail_idx_;
                    unique_id_ = q->next_unique_id_ - attached_queue_->total_valid_;
                }else{
                    physical_idx_ = q->current_write_idx_;
                    unique_id_ = q->next_unique_id_;
                }
            }

            /* Make QueueIterator<true> a friend class of QueueIterator<false>
             * So const iterator can access non-const iterators private members
             */
            friend class QueueIterator<true>;

            /** Copy constructor.
             * Allows for implicit conversion from a regular iterator to a const_iterator
             */
            QueueIterator(const QueueIterator<false> & iter) :
                attached_queue_(iter.attached_queue_),
                physical_idx_(iter.physical_idx_),
                unique_id_(iter.unique_id_)
            {}

            /** Copy constructor.
             * Allows for implicit conversion from a regular iterator to a const_iterator
             */
            QueueIterator(const QueueIterator<true> & iter) :
                attached_queue_(iter.attached_queue_),
                physical_idx_(iter.physical_idx_),
                unique_id_(iter.unique_id_)
            {}

            /**
             * \brief Assignment operator
             * The copy also alerts the validator_ item that another QueueIterator is
             * now attached to it.
             */
            QueueIterator& operator=(const QueueIterator& rhs)
            {
                attached_queue_ = rhs.attached_queue_;
                physical_idx_ = rhs.physical_idx_;
                unique_id_ = rhs.physical_idx_;
                return *this;
            }

            /// overload the comparison operator.
            bool operator<(const QueueIterator& rhs) const
            {
                sparta_assert(attached_queue_ == rhs.attached_queue_,
                            "Cannot compare QueueIterators created by different Queues");
                return getIndex() < rhs.getIndex();
            }

            /// overload the comparison operator.
            bool operator>(const QueueIterator& rhs) const
            {
                sparta_assert(attached_queue_ == rhs.attached_queue_,
                            "Cannot compare QueueIterators created by different Queues");
                return getIndex() > rhs.getIndex();
            }

            /// overload the comparison operator.
            bool operator==(const QueueIterator& rhs) const
            {
                sparta_assert(attached_queue_ == rhs.attached_queue_,
                            "Cannot compare QueueIterators created by different Queues");
                return getIndex() == rhs.getIndex();
            }

            /// overload the comparison operator.
            bool operator!=(const QueueIterator& rhs) const {
                return !(*this == rhs);
            }

            /// Pre-Increment operator
            QueueIterator & operator++()
            {
                physical_idx_ = attached_queue_->incrementIndexValue_(physical_idx_);
                ++unique_id_;
                return *this;
            }

            /// Post-Increment iterator
            QueueIterator operator++(int)
            {
                QueueIterator it(*this);
                operator++();
                return it;
            }

            /// Pre-decrement iterator
            QueueIterator & operator--()
            {
                physical_idx_ = attached_queue_->decrementIndexValue_(physical_idx_);
                --unique_id_;
                return *this;
            }

            /// Post-decrement iterator
            QueueIterator operator-- (int){
                QueueIterator it(*this);
                operator--();
                return it;
            }

            /// Dereferencing operator
            DataReferenceType operator* ()
            {
                sparta_assert(getIndex()<attached_queue_->total_valid_, "Not a valid Iterator");
                return getAccess_(std::integral_constant<bool, is_const_iterator>());
            }

            ///support -> operator
            value_type* operator->()
            {
                sparta_assert(getIndex()<attached_queue_->total_valid_, "Not a valid Iterator");
                return std::addressof(getAccess_(std::integral_constant<bool, is_const_iterator>()));
            }
            
            const value_type* operator->() const
            {
                sparta_assert(getIndex()<attached_queue_->total_valid_, "Not a valid Iterator");
                return std::addressof(getAccess_(std::integral_constant<bool, is_const_iterator>()));
            }

            /// Checks validity of iterator
            /// \return Returns true if iterator is valid else false
            bool isValid() const {
                if(unique_id_ >= attached_queue_->numOngoingInvalidations_()
                   && attached_queue_ != nullptr
                   && physical_idx_ < attached_queue_->vector_size_)
                {
                    return true;
                } else {
                    return false;
                }
            }

            /// Get the accurate logical index of this entry in the queue.
            uint32_t getIndex() const
            {
                sparta_assert(unique_id_ >= attached_queue_->numOngoingInvalidations_(),
                            "Cannot get index. This QueueIterator does not represent a valid entry in the Queue");
                sparta_assert(attached_queue_ != nullptr,
                            "Cannot get index. No Queue is attatched with this QueueEntree");
                sparta_assert(physical_idx_ < attached_queue_->vector_size_, "Not a valid Queue Iterator" );
                return attached_queue_->convertPhysicalIndex_(physical_idx_);
            }

        private:

            QueuePointerType attached_queue_;
            uint32_t physical_idx_;
            uint64_t unique_id_;

            /// Get access on a non-const iterator
            DataReferenceType getAccess_(std::false_type) const {
                return attached_queue_->access(getIndex());
            }

            /// Get access on a const iterator
            DataReferenceType getAccess_(std::true_type) const {
                return attached_queue_->read(getIndex());
            }
        };

        /// Typedef for regular iterator
        typedef QueueIterator<false> iterator;

        /// Typedef for constant iterator
        typedef QueueIterator<true> const_iterator;

        /**
         * \brief Construct a queue
         * \param name The name of the queue
         * \param num_entries The number of entries this queue can hold
         * \param clk The clock this queue belongs to
         * \param statset The Counter set to register read-only
         *                counters; default nullptr
         *
         * \param stat_vis_general Sets the visibility of the stat
         *                         counters for the 0th and last index
         *                         of the utilization counts, so the
         *                         empty and full counts.
         *
         * \param stat_vis_detailed Sets the visibility of the stat
         *                          counts between 0 and the last
         *                          index. i.e. more detailed than the
         *                          general stats, default VIS_HIDDEN
         *
         * \param stat_vis_max Sets the visibility for a stat that
         *                     contains the maximum utilization for
         *                     this buffer. The default is AUTO_VISIBILITY.
         *
         * \param stat_vis_avg Sets the visibility for a stat that
         *                     contains the weighted utilization
         *                     average for this buffer. The default is
         *                     AUTO_VISIBILITY.
         *
         * \warning By default the stat_vis_* options are set to
         *          AUTO_VISIBILITY, for this structure
         *          AUTO_VISIBILITY resolves to SPARTA_CONTAINER_DEFAULT
         *          which at the time of writing this comment is set
         *          to VIS_HIDDEN. If you rely on the stats from this
         *          container you should explicity set the visibility.
         */
        Queue(const std::string & name,
              const uint32_t num_entries,
              const Clock * clk,
              StatisticSet * statset = nullptr,
              InstrumentationNode::visibility_t stat_vis_general = InstrumentationNode::AUTO_VISIBILITY,
              InstrumentationNode::visibility_t stat_vis_detailed= InstrumentationNode::VIS_HIDDEN,
              InstrumentationNode::visibility_t stat_vis_max     = InstrumentationNode::AUTO_VISIBILITY,
              InstrumentationNode::visibility_t stat_vis_avg     = InstrumentationNode::AUTO_VISIBILITY) :
            num_entries_(num_entries),
            vector_size_(nextPowerOfTwo_(num_entries*2)),
            name_(name),
            collector_(nullptr)
        {

            if(statset)
            {
                utilization_.reset(new CycleHistogramStandalone(statset, clk,
                                                                name + "_utilization",
                                                                name + " occupancy histogram",
                                                                0, num_entries, 1, 0,
                                                                stat_vis_general,
                                                                stat_vis_detailed,
                                                                stat_vis_max,
                                                                stat_vis_avg));
            }

            // Make the queue twice as large and a power of two to
            // allow a complete invalidation followed by a complete
            // population
            queue_data_.reset((value_type *)malloc(sizeof(value_type) * vector_size_));
        }

        ~Queue(){
            clear();
        }

        /// No copies, no moves
        Queue(const Queue<value_type> & ) = delete;

        /// Deleting default assignment operator to prevent copies
        Queue &operator=(const Queue<value_type> &) = delete;

        /**
         * \brief Determine if data at the index is valid
         * \param idx The index to determine validity
         * \return true if valid; false otherwise
         *
         */
        bool isValid(uint32_t idx)const {
            return idx < total_valid_;
        }

        /**
         * \brief Read and return the data at the given index, const reference
         * \param idx The index to read
         * \return The data to return at the given index (const reference)
         */
        const value_type & read(uint32_t idx) const {
            sparta_assert(isValid(idx), "Cannot read an invalid index");

            idx = convertIndex_(idx);
            return queue_data_[idx];
        }

        /**
         * \brief Read and return the data at the given index, reference, non-const method
         * \param idx The index to read
         * \return The data to return at the given index (reference)
         *
         * Use the read() equivalent for const access
         */
        value_type & access(uint32_t idx) {
            sparta_assert(isValid(idx), "Cannot read an invalid index");
            idx = convertIndex_(idx);
            return queue_data_[idx];
        }

        /**
         * \brief Read and return the data at the front(oldest element), reference
         * \return The data at the front  (reference)
         */
        value_type & front() const {
            return queue_data_[current_zero_pos_];
        }

        /**
         * \brief Read and return the last pushed in element(newest element), reference
         * \return The data at the back  (reference)
         */
        value_type & back() const {
            sparta_assert(size() != 0);
            const uint32_t index = decrementIndexValue_(current_write_idx_);
            sparta_assert(index < vector_size_);
            return queue_data_[index];
        }

        /**
         * \brief Return the fixed size of this queue
         * \return The size of this queue
         */
        uint32_t capacity() const {
            return num_entries_;
        }

        /**
         * \brief Return the number of valid entries
         * \return The number of valid entries.  Does not subtract entries invalidated \a this cycle
         */
        size_type size() const {
            return total_valid_;
        }
        /**
         * \brief Return the number of free entries.
         * \return The number of free entries
         *
         * Does not take into account the number of invalidated entries \a this cycle
         */
        size_type numFree() const {
            return (capacity() - total_valid_);
        }

        /**
         * \brief Return if the queue is empty or not.
         * \return True, if the queue is empty
         *
         * Does not take into account the number of invalidated entries \a this cycle
         */
        bool empty() const {
            return (total_valid_ == 0);
        }

        /**
         * \brief Empty the queue
         *
         * Removes all entries from the Queue
         */
        void clear()
        {
            auto idx = current_zero_pos_;
            while (idx != current_write_idx_)
            {
                queue_data_[idx].~value_type();
                idx = incrementIndexValue_(idx);
            }
            ongoing_total_invalidations_ += total_valid_;
            current_write_idx_ = 0;
            current_tail_idx_  = 0;
            num_to_be_invalidated_ = 0;
            total_valid_       = 0;
            current_zero_pos_  = 0;
            total_to_be_valid_ = 0;
            num_added_         = 0;
            top_valid_idx_     = 0;
            updateUtilizationCounters_();
        }

        /**
         * \brief Request that this queue begin collecting its
         *        contents for pipeline collection.
         * \param parent A pointer to the parent treenode for which to add
         *               Collectable objects under.
         * \note This only sets the Queue up for
         *       collection. collection must be started with an
         *       instatiation of the PipelineCollector
         */
        void enableCollection(TreeNode * parent) {
            collector_.reset(new collection::IterableCollector<Queue<value_type> >
                             (parent, name_, *this, capacity()));
        }

        /**
         * \brief push data to the Queue.
         * \param dat Data to be copied in
         * \return a copy of a QueueIterator that can be queired at
         * any time for this data's position in the queue.
         * \warning appends through via this method are immediately valid.
         */
        QueueIterator<false> push (const value_type & dat)
        {
            return pushImpl_(dat);
        }
        
        /**
         * \brief push data to the Queue.
         * \param dat Data to be moved in
         * \return a copy of a QueueIterator that can be queired at
         * any time for this data's position in the queue.
         * \warning appends through via this method are immediately valid.
         */
        QueueIterator<false> push (value_type && dat)
        {
            return pushImpl_(std::move(dat));
        }

        /**
         * \brief Pops the data at the front of the structure (oldest element)
         * After pop iterator always points to the last element.
         */
        void pop() {
            sparta_assert(isIndexInValidRange_(current_tail_idx_));

            // sparta_assert(idx == 0); THIS IS BROKEN IN EXAMPLE!
            // By incrementing the tail and num_to_be_invalidated_ index's we calculate
            // a valid index range during the compact.
            ++num_to_be_invalidated_;

            // our tail moves upward in the vector now.
            current_tail_idx_ = incrementIndexValue_(current_tail_idx_);

            // Destruct the items we are about to invalidate
            auto idx = current_zero_pos_;
            while (idx != current_tail_idx_)
            {
                queue_data_[idx].~value_type();
                idx = incrementIndexValue_(idx);
            }
            processInvalidations_();
        }

        /**
         * \brief Pops the data at the back of the structure (newest element)
         * After pop iterator always points to the last element.
         */
        void pop_back() {
            sparta_assert(isIndexInValidRange_(current_tail_idx_));

            // sparta_assert(idx == 0); THIS IS BROKEN IN EXAMPLE!
            // By incrementing the tail and num_to_be_invalidated_ index's we calculate
            // a valid index range during the compact.
            ++num_to_be_invalidated_;

            // our tail moves upward in the vector now.
            current_write_idx_ = decrementIndexValue_(current_write_idx_);
            queue_data_[current_write_idx_].~value_type();
            processInvalidations_();
        }

        /// \brief STL-like begin operation, starts at front (oldest element)
        /// \return iterator to the oldest element in Queue
        iterator begin(){ return iterator(this,true);}

        /// \brief STL - like end operation, starts at element one past head.
        /// \return Returns iterator pointing to past-the-end elemnt in Queue
        iterator end()  { return iterator(this,false);}

        /// \brief STL-like begin operation, starts at front (oldest element)
        /// \return const_iterator to the oldest element in Queue
        const_iterator begin() const { return const_iterator(this,true);}

        /// \brief STL - like end operation, starts at element one past head.
        /// \return Returns const_iterator pointing to past-the-end elemnt in Queue
        const_iterator end() const { return const_iterator(this,false);}

    private:
        struct DeleteToFree_{
            void operator()(void * x){
                free(x);
            }
        };

        template<typename U>
        QueueIterator<false> pushImpl_ (U && dat)
        {
            sparta_assert(current_write_idx_ <= vector_size_);
            // can't write more than the allowed items
            new (queue_data_.get() + current_write_idx_) value_type(std::forward<U>(dat));
            QueueIterator<false> new_entry(this, current_write_idx_, next_unique_id_);
            ++next_unique_id_;
            ++num_added_;
            // move the write index up.
            current_write_idx_ = incrementIndexValue_(current_write_idx_);
            // either process now or schedule for later at the correct precedence.
            processWrites_();
            return new_entry;
        }

        void updateUtilizationCounters_() {
            // Update Counters
            if(utilization_) {
                utilization_->setValue(total_valid_);
            }
        }

        /// Process any pending inserts.
        void processWrites_()
        {
            // update the valid count.
            total_valid_ += num_added_;

            // Make sure we did not overflow the size of the queue.
            sparta_assert(num_to_be_invalidated_ < total_valid_);
            sparta_assert(total_valid_ - num_to_be_invalidated_ <= num_entries_,
                        "Too many valid entries were added to the Queue: " << name_);

            // Figure out where the current top or head of our Queue is
            top_valid_idx_ = offsetIndexBy_(current_zero_pos_, total_valid_);

            // Only send an event if the queue goes from empty to not
            // empty
            num_added_ = 0;

            updateUtilizationCounters_();
        }

        /// Process any pending invalidations.
        void processInvalidations_()
        {
            // the zero position is the tail pos after a compact.
            current_zero_pos_ = current_tail_idx_;

            // Make sure we can do this.
            sparta_assert(total_valid_ + num_added_>= num_to_be_invalidated_, "Cannot invalidate more items than were"" valid in the Queue.");
            sparta_assert(total_valid_ >= num_to_be_invalidated_);
            total_valid_ = total_valid_ - num_to_be_invalidated_;

            // keep a running total of invalidations. Used for
            // asserting when QueueIterator's are valid.
            ongoing_total_invalidations_ = ongoing_total_invalidations_ + num_to_be_invalidated_;
            num_to_be_invalidated_ = 0;
            updateUtilizationCounters_();
        }

        const size_type num_entries_;   /*!< The number of entries this queue can hold */
        size_type current_write_idx_     = 0; /*!< The current free index for appending items */
        size_type current_tail_idx_      = 0; /*!< The current tail index for where the user can request invalidation */
        size_type num_to_be_invalidated_ = 0; /*!< The number of items that are requested to be invalidated this cycle*/
        size_type total_valid_           = 0; /*!< The number of actual valid entries*/
        size_type current_zero_pos_      = 0; /*!< The index in our vector that the user understands to be the Queue's 0 position */
        size_type total_to_be_valid_     = 0; /*!< The number of new items appended to the queue that will become valid after a compact */
        size_type num_added_             = 0; /*!< The number of items that have been added but not validated */
        size_type top_valid_idx_         = 0; /*!< The index in our vector of the highest valid index accurate after a compact.*/
        const size_type vector_size_;   /*!< The current size of our vector. Same as queue_data_.size()*/

        uint64_t next_unique_id_ = 0;              /*!< A counter to provide a new unique id for every append to the queue */
        uint64_t ongoing_total_invalidations_ = 0; /*!< A counter to count the total number of invalidations that have ever occured in this queue */

        const std::string name_; /*! The name of this queue */

        //////////////////////////////////////////////////////////////////////
        // Counters
        std::unique_ptr<sparta::CycleHistogramStandalone> utilization_;

        //////////////////////////////////////////////////////////////////////
        // Collectors
        std::unique_ptr<collection::IterableCollector<Queue<value_type> > > collector_;
    
        // Notice that our list for storing data is a dynamic array.
        // This is used instead of a stl vector to promote debug
        // performance.
        std::unique_ptr<DataT[], DeleteToFree_> queue_data_ = nullptr; /*!< The actual array that holds all of the Data in the queue, valid and invalid */
    };

}
// __QUEUE__H__
#endif