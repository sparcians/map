// <Queue.hpp> -*- C++ -*-


/**
 * \file   Queue.hpp
 * \brief  Defines the Queue class used for queuing data
 *
 */

#pragma once

#include <cinttypes>
#include <vector>
#include <type_traits>

#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/ports/Port.hpp"
#include "sparta/statistics/CycleHistogram.hpp"
#include "sparta/statistics/StatisticSet.hpp"
#include "sparta/collection/IterableCollector.hpp"
#include "sparta/statistics/InstrumentationNode.hpp"
#include "sparta/utils/IteratorTraits.hpp"

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
    public:

        /// Typedef for the DataT
        using value_type = DataT;

        /// Typedef for the QueueType
        using QueueType = Queue<value_type>;

        // Typedef for size_type
        using size_type = uint32_t;

    private:

        //
        // How the Queue works internally.
        //
        // The queue will create storage for the next pow2 elements
        // for efficiency.  So, if the user creates a Queue of 12
        // elements, the internal storage will be 16.
        //
        // There are two pointers, each represents the _physical_
        // location in the array, not the logical.
        //
        //   current_write_idx_ -- the insertion point used by push()
        //   current_head_idx_  -- the top of the queue, used by pop()
        //
        // There is one dynamically created array for the data:
        //
        //   queue_data_ -- equal to capacity ^ next pow2
        //
        // At init time, current_head_idx_ == current_write_idx_.  As
        // elements are push()ed, the new objects is added and
        // current_write_idx_ is incremented.  When current_write_idx_
        // surpasses the end of the array, it will be wrapped to the
        // beginning -- as long as there are invalidations.  The
        // "distance" between current_write_idx_ and current_head_idx_
        // is always <= the capacity() of the queue.  The distance
        // between the indexes represents the size().
        //
        // Valid iterators are given an index into the queue, which is
        // used to retrieve the data at the given location in the
        // queue.  As the iterator is incremented, it re-validates the
        // index given, so it will always fall into one of two
        // locations: the next valid data element, or end()
        //
        // Reading/accessing the Queue from a modeler's POV is a
        // little different.  The modeler can index into the queue
        // starting from 0 -> size().  The Queue will need to
        // "convert" this logical index to the physical one.  For
        // example, 0 might not be the data at index 0 of the array,
        // but instead, index 6:
        //
        //          L  P
        //          2  0  x
        //          3  1  x
        //          4  2  x
        //          5  3    <- write idx
        //          6  4
        //          7  5
        //          0  6  x <- head
        //          1  7  x
        //
        // Another layout of queue during runtime:
        //
        //          L  P
        //          7  0
        //          0  1  x <- head
        //          1  2  x
        //          2  3  x
        //          3  4  x
        //          4  5  x
        //          5  6  x
        //          6  7    <- write idx

        /// Find the next value that is greater than or equal to the
        /// paramenter
        constexpr uint32_t nextPowerOfTwo_(uint32_t val) const
        {
            if(val < 2) { return 1ull; }
            return 1ull << ((sizeof(uint64_t) * 8) - __builtin_clzll(val - 1ull));
        }

        /// Roll (or wrap) the physical index
        uint32_t rollPhysicalIndex_(const uint32_t phys_idx) const {
            return (vector_size_ - 1) & (phys_idx);
        }

        /// Utility to convert a given logical index to a physical one
        uint32_t getPhysicalIndex_(const uint32_t logical_idx) const {
            return rollPhysicalIndex_(logical_idx + current_head_idx_);
        }

        /// Increment an index value. It is important to make sure
        /// that the index rolls over to zero if it goes above the
        /// bounds of our list.
        uint32_t incrementIndexValue_(const uint32_t val) const {
            return rollPhysicalIndex_(val + 1);
        }

        /// Decrement an index value. It is important to make sure
        /// that the index rolls over to zero if it goes above the
        /// bounds of our list.
        uint32_t decrementIndexValue_(const uint32_t val) const {
            return rollPhysicalIndex_(val - 1);
        }

        /// The QueueData
        struct QueueData {
            template<class DataTRef>
            QueueData(DataTRef && dat, uint64_t in_obj_id) :
                data(std::forward<DataTRef>(dat)),
                obj_id(in_obj_id)
            {}

            DataT data;
            uint64_t obj_id = 0;
        };

        /// Read the data at the physical localation, const
        const value_type & readPhysical_(const uint32_t phys_idx) const
        {
            return queue_data_[phys_idx].data;
        }

        /// Access the data at the physical localation, non-const
        value_type & accessPhysical_(const uint32_t phys_idx)
        {
            return queue_data_[phys_idx].data;
        }

        /// Convert the physical index to the logical one
        uint32_t physicalToLogical_(const uint32_t physical_idx) const {
            if(physical_idx == invalid_index_) { return invalid_index_; }
            // Neat trick from the previous author...
            return rollPhysicalIndex_(physical_idx - current_head_idx_);
        }

        /// Is the physical index within the current range
        bool isValidPhysical_(const uint32_t physical_idx) const
        {
            const auto log_idx = physicalToLogical_(physical_idx);
            return (log_idx < size());
        }

    public:

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
        class QueueIterator : public utils::IteratorTraits<std::bidirectional_iterator_tag, value_type>
        {
        private:
            using DataReferenceType = typename std::conditional<is_const_iterator,
                                                                const value_type &, value_type &>::type;
            using QueuePointerType =  typename std::conditional<is_const_iterator,
                                                                const QueueType * , QueueType * >::type;

            /**
             * \brief construct.
             * \param q  a pointer to the underlaying queue.
             * \param begin_itr if true iterator points to tail (begin()), if false points to head (end())
             */
            QueueIterator(QueuePointerType q, uint32_t physical_index, uint32_t obj_id) :
                attached_queue_(q),
                physical_index_(physical_index),
                obj_id_(obj_id)
            { }

            /// Only the Queue can attach itself
            friend class Queue<DataT>;

            /// True if this iterator is attached to a valid queue
            bool isAttached_() const { return nullptr != attached_queue_; }

        public:

            //! \brief Default constructor
            QueueIterator() = default;

            /* Make QueueIterator<true> a friend class of QueueIterator<false>
             * So const iterator can access non-const iterators private members
             */
            friend class QueueIterator<true>;

            /** Copy constructor.
             * Allows for implicit conversion from a regular iterator to a const_iterator
             */
            QueueIterator(const QueueIterator<false> & iter) :
                attached_queue_(iter.attached_queue_),
                physical_index_(iter.physical_index_),
                obj_id_(iter.obj_id_)
            {}

            /** Copy constructor.
             * Allows for implicit conversion from a regular iterator to a const_iterator
             */
            QueueIterator(const QueueIterator<true> & iter) :
                attached_queue_(iter.attached_queue_),
                physical_index_(iter.physical_index_),
                obj_id_(iter.obj_id_)
            {}

            /// Checks validity of iterator -- is it related to a
            /// Queue and points to a valid entry in the queue
            /// \return Returns true if iterator is valid else false
            bool isValid() const {
                if(nullptr == attached_queue_) { return false; }
                return attached_queue_->determineIteratorValidity_(this);
            }

            /**
             * \brief Assignment operator
             */
            QueueIterator& operator=(const QueueIterator& rhs) = default;

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
                return (physical_index_ == rhs.physical_index_) && (obj_id_ == rhs.obj_id_);
            }

            /// overload the comparison operator.
            bool operator!=(const QueueIterator& rhs) const {
                return !(*this == rhs);
            }

            /// Pre-Increment operator
            QueueIterator & operator++()
            {
                sparta_assert(isAttached_(), "This is an invalid iterator");
                attached_queue_->incrementIterator_(this);
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
                sparta_assert(isAttached_(), "This is an invalid iterator");
                attached_queue_->decrementIterator_(this);
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
                sparta_assert(isValid(), "This is an invalid iterator");
                return getAccess_(std::integral_constant<bool, is_const_iterator>());
            }

            ///support -> operator
            value_type* operator->()
            {
                sparta_assert(isValid(), "This is an invalid iterator");
                return std::addressof(getAccess_(std::integral_constant<bool, is_const_iterator>()));
            }

            const value_type* operator->() const
            {
                sparta_assert(isValid(), "This is an invalid iterator");
                return std::addressof(getAccess_(std::integral_constant<bool, is_const_iterator>()));
            }

            /// Get the logical index of this entry in the queue.
            /// This is expensive and should be avoided.  It makes
            /// better sense to simply retrieve the object directly
            /// from the iterator.
            uint32_t getIndex() const
            {
                sparta_assert(isAttached_(), "This is an invalid iterator");
                return attached_queue_->physicalToLogical_(physical_index_);
            }

        private:

            QueuePointerType attached_queue_ {nullptr};
            uint32_t physical_index_ = std::numeric_limits<uint32_t>::max();
            uint64_t obj_id_ = 0;

            /// Get access on a non-const iterator
            DataReferenceType getAccess_(std::false_type) const {
                return attached_queue_->accessPhysical_(physical_index_);
            }

            /// Get access on a const iterator
            DataReferenceType getAccess_(std::true_type) const {
                return attached_queue_->readPhysical_(physical_index_);
            }
        };

        /// Typedef for regular iterator
        using iterator = QueueIterator<false> ;

        /// Typedef for constant iterator
        using const_iterator = QueueIterator<true>;

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
            queue_data_.reset(static_cast<QueueData *>(malloc(sizeof(QueueData) * vector_size_)));
        }

        /// Destroy the Queue, clearing everything out
        ~Queue() { clear(); }

        /// No copies, no moves
        Queue(const Queue<value_type> & ) = delete;

        /// Deleting default assignment operator to prevent copies
        Queue &operator=(const Queue<value_type> &) = delete;

        /*!
         * \brief Name of this resource.
         */
        const std::string & getName() const {
            return name_;
        }

        /**
         * \brief Determine if data at the index is valid
         * \param idx The index to determine validity
         * \return true if valid; false otherwise
         *
         */
        bool isValid(uint32_t idx) const {
            return (idx < size());
        }

        /**
         * \brief Read and return the data at the given index, const reference
         * \param idx The index to read
         * \return The data to return at the given index (const reference)
         */
        const value_type & read(uint32_t idx) const {
            sparta_assert(isValid(idx), "Cannot read an invalid index");
            return readPhysical_(getPhysicalIndex_(idx));
        }

        /**
         * \brief Read and return the data at the given index, reference, non-const method
         * \param idx The index to read
         * \return The data to return at the given index (reference)
         *
         * Use the read() equivalent for const access
         */
        value_type & access(uint32_t idx) {
            sparta_assert(isValid(idx), name_ << ": Cannot read an invalid index");
            return accessPhysical_(getPhysicalIndex_(idx));
        }

        /**
         * \brief Read and return the data at the front(oldest element), reference
         * \return The data at the front  (reference)
         */
        value_type & front() const {
            sparta_assert(size() != 0, name_ << ": Trying to get front() on an empty Queue");
            return queue_data_[current_head_idx_].data;
        }

        /**
         * \brief Read and return the last pushed in element(newest element), reference
         * \return The data at the back  (reference)
         */
        value_type & back() const {
            sparta_assert(size() != 0, name_ << ": Trying to get back() on an empty Queue");
            uint32_t index = current_write_idx_;
            index = decrementIndexValue_(index);
            return queue_data_[index].data;
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
            auto idx = current_head_idx_;
            while (idx != current_write_idx_)
            {
                queue_data_[idx].data.~value_type();
                idx = incrementIndexValue_(idx);
            }
            current_write_idx_ = 0;
            current_head_idx_  = 0;
            total_valid_       = 0;
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
                             (parent, name_, this, capacity()));
        }

        /**
         * \brief push data to the Queue.
         * \param dat Data to be copied in
         * \return a copy of a QueueIterator that can be queired at
         * any time for this data's position in the queue.
         * \warning appends through via this method are immediately valid.
         */
        iterator push(const value_type & dat)
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
        iterator push(value_type && dat)
        {
            return pushImpl_(std::move(dat));
        }

        /**
         * \brief Pops the data at the front of the structure (oldest element)
         * After pop iterator always points to the last element.
         */
        void pop()
        {
            sparta_assert(total_valid_ != 0, name_ << ": Trying to pop an empty Queue");

            // Destruct the element at the head
            queue_data_[current_head_idx_].data.~value_type();

            // Our head moves upward
            current_head_idx_ = incrementIndexValue_(current_head_idx_);

            // Clean up
            processInvalidation_();
        }

        /**
         * \brief Pops the data at the back of the structure (newest element)
         * After pop iterator always points to the last element.
         */
        void pop_back()
        {
            sparta_assert(total_valid_ != 0, name_ << ": Trying to pop_back an empty Queue");

            // our tail moves upward in the vector now.
            current_write_idx_ = decrementIndexValue_(current_write_idx_);

            // Destroy the object at the current write index
            queue_data_[current_write_idx_].data.~value_type();

            // Clean up
            processInvalidation_();
        }

        /// \brief STL-like begin operation, starts at front (oldest element)
        /// \return iterator to the oldest element in Queue
        iterator begin() {
            if(SPARTA_EXPECT_FALSE(empty())) {
                return end();
            }
            else {
                return iterator(this, current_head_idx_, queue_data_[current_head_idx_].obj_id);
            }
        }

        /// \brief STL - like end operation, starts at element one past head.
        /// \return Returns iterator pointing to past-the-end elemnt in Queue
        iterator end()  { return iterator(this, invalid_index_, invalid_index_);}

        /// \brief STL-like begin operation, starts at front (oldest element)
        /// \return const_iterator to the oldest element in Queue
        const_iterator begin() const {
            if(SPARTA_EXPECT_FALSE(empty())) {
                return end();
            }
            else {
                return const_iterator(this, current_head_idx_, queue_data_[current_head_idx_].obj_id);
            }
        }

        /// \brief STL - like end operation, starts at element one past head.
        /// \return Returns const_iterator pointing to past-the-end elemnt in Queue
        const_iterator end() const { return const_iterator(this, invalid_index_, invalid_index_);}

    private:

        template<typename U>
        iterator pushImpl_ (U && dat)
        {
            sparta_assert(total_valid_ != capacity(), name_ << ": Queue is full");

            // can't write more than the allowed items
            sparta_assert(current_write_idx_ <= vector_size_);
            new (queue_data_.get() + current_write_idx_) QueueData(std::forward<U>(dat), ++obj_id_);

            // move the write index up.
            const uint32_t write_idx = current_write_idx_;
            current_write_idx_ = incrementIndexValue_(current_write_idx_);

            // either process now or schedule for later at the correct precedence.
            // update the valid count.
            total_valid_ += 1;

            updateUtilizationCounters_();

            return iterator(this, write_idx, obj_id_);
        }

        template<typename IteratorType>
        bool determineIteratorValidity_(IteratorType * itr) const
        {
            const auto physical_index = itr->physical_index_;

            if(physical_index == invalid_index_) {
                return false;
            }

            // Short cut... if we're empty, the iterator ain't valid
            if(empty()) { return false; }

            if(isValidPhysical_(physical_index)) {
                return (queue_data_[physical_index].obj_id == itr->obj_id_);
            }
            return false;
        }

        template<typename IteratorType>
        void decrementIterator_(IteratorType * itr) const
        {
            sparta_assert(itr->physical_index_ != 0, name_ <<
                          ": Iterator is not valid for decrementing");

            uint32_t physical_index = itr->physical_index_;

            // If it's the end iterator, go to the back - 1
            if(SPARTA_EXPECT_FALSE(physical_index == invalid_index_)) {
                const uint32_t phys_idx = decrementIndexValue_(current_write_idx_);
                itr->physical_index_ = phys_idx;
                itr->obj_id_ = queue_data_[phys_idx].obj_id;
            }
            else {
                // See if decrementing this iterator puts into the weeds.
                // If so, invalidate it.
                physical_index = decrementIndexValue_(physical_index);
                if(isValidPhysical_(physical_index)) {
                    itr->physical_index_ = physical_index;
                    itr->obj_id_ = queue_data_[physical_index].obj_id;
                }
                else {
                    itr->physical_index_ = invalid_index_;
                    itr->obj_id_ = invalid_index_;
               }
            }
        }

        template<typename IteratorType>
        void incrementIterator_(IteratorType * itr) const
        {
            uint32_t physical_index = itr->physical_index_;

            // See if incrementing this iterator puts it at the end.
            // If so, put it there.
            sparta_assert(physical_index != invalid_index_,
                          name_ << ": Trying to increment an invalid iterator");

            physical_index = incrementIndexValue_(physical_index);

            // See if the old logical index was valid.  We could be
            // incrementing to end()
            if(isValidPhysical_(physical_index))
            {
                // Safe to increment the physical_index
                itr->physical_index_ = physical_index;
                itr->obj_id_ = queue_data_[physical_index].obj_id;
            }
            else {
                // No longer valid index, but we could be rolling off
                // to the end().
                itr->physical_index_ = invalid_index_;
                itr->obj_id_ = invalid_index_;
            }
        }

        void updateUtilizationCounters_() {
            // Update Counters
            if(utilization_) {
                utilization_->setValue(total_valid_);
            }
        }

        /// Process any pending invalidations.
        void processInvalidation_()
        {
            sparta_assert(total_valid_ > 0);
            total_valid_ -= 1;
            updateUtilizationCounters_();
        }

        const size_type num_entries_;     /*!< The number of entries this queue can hold */
        size_type current_write_idx_ = 0; /*!< The current free index for appending items */
        size_type current_head_idx_  = 0; /*!< The current head index for where the user pops */
        size_type total_valid_       = 0; /*!< The number of actual valid entries*/
        const size_type vector_size_;     /*!< The current size of our vector. Same as queue_data_.size()*/
        const size_type invalid_index_ {vector_size_ + 1};   /*!< A number that represents the ending entry */
        const std::string name_; /*! The name of this queue */

        /// Increasing identifier to determine if old iterators are still valid ones.
        uint64_t obj_id_ = 0;

        //////////////////////////////////////////////////////////////////////
        // Counters
        std::unique_ptr<sparta::CycleHistogramStandalone> utilization_;

        //////////////////////////////////////////////////////////////////////
        // Collectors
        std::unique_ptr<collection::IterableCollector<Queue<value_type> > > collector_;

        // Notice that our list for storing data is a dynamic array.
        // This is used instead of a stl vector to promote debug
        // performance.
        /*! The actual array that holds all of the Data in the queue, valid and invalid */
        struct DeleteToFree_{
            void operator()(void * x){
                free(x);
            }
        };
        std::unique_ptr<QueueData[], DeleteToFree_> queue_data_;
    };

}
