// <CircularBuffer.h> -*- C++ -*-


/**
 * \file   CircularBuffer.hpp
 *
 * \brief  Defines the CircularBuffer class
 *
 */

#pragma once

#include <cinttypes>
#include <list>
#include <limits>
#include <algorithm>

#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/statistics/CycleCounter.hpp"
#include "sparta/statistics/StatisticInstance.hpp"
#include "sparta/statistics/StatisticDef.hpp"
#include "sparta/collection/IterableCollector.hpp"
#include "sparta/statistics/Counter.hpp"
#include "sparta/utils/IteratorTraits.hpp"

namespace sparta
{

    /**
     * \class CircularBuffer
     *
     * \brief A data structure allowing appending at the end,
     * beginning, or insert in the middle, but erase anywhere with
     * collapse
     *
     * The CircularBuffer allows a user to append data to the end or
     * the beginning of the buffer, or insert into the middle of the
     * CircularBuffer, and erase anywhere.  The CircularBuffer will
     * collapse on empty entries unlike the sparta::Array.
     *
     * The CircularBuffer acks like a standard container via public
     * push_back, insert, and erase methods.  The
     * CircularBufferIterator can be used as an index into the
     * CircularBuffer, and maintains knowledge internally of its
     * location in the CircularBuffer, as well whether or not it still
     * represents a valid entry.
     *
     * Where the CircularBuffer differs from the standard Buffer is
     * that the CircularBuffer has no concept of "full", meaning it
     * will wrap around and overwrite older entries.  For example, if
     * the CircularBuffer is 10 entries in size, the user can append
     * to the CircularBuffer 11 times without error as the 11th append
     * will simply overwrite the original first entry.  Buffer, on the
     * other hand, will assert.
     *
     * Iterator behavior:
     *
     * - On push_back, existing iterators pointing to non-replaced
     *   values are still valid
     * - A push_back that causes a "wrap-around" will invalidate
     *   those iterators pointing to older entries being replaced
     * - erase will invalidate ALL iterators
     * - insert will invalidate ALL iterators
     *
     * Example:
     * \code
     * CircularBuffer<uint32_t> circularbuffer;
     * circularbuffer.push_back(3);
     * circularbuffer.push_back(5);
     * circularbuffer.push_back(1);
     *
     * sparta_assert(*(circularbuffer.begin()) == 3);
     * sparta_assert(*(circularbuffer.rbegin()) == 1);
     *
     * auto it = circularbuffer.begin();
     * CircularBuffer.erase(it);
     * sparta_assert(!it.isValid());
     *
     * \endcode
     *
     *
     * \tparam DataT The data type contained in the CircularBuffer.
     */
    template <class DataT>
    class CircularBuffer
    {
    public:

        // A typedef for this CircularBuffer's type. This is useful for my
        // subclasses, CircularBufferIterator & EntryValidator
        typedef CircularBuffer<DataT> CircularBufferType;

        // Typedef for the DataT
        typedef DataT value_type;

        // Typedef for size_type
        typedef uint32_t size_type;

    private:

        struct CircularBufferData
        {
            template<typename U = value_type>
            CircularBufferData(U && dat,
                               const uint64_t window_idx) :
                data(std::forward<U>(dat)),
                window_idx(window_idx)
            {}
            CircularBufferData(const uint64_t window_idx) :
                window_idx(window_idx)
            {}
            value_type data{};   // The data supplied by the user
            uint64_t window_idx; // The location in the validity
                                 // window of the CB.  Serves as a
                                 // fast check for validity of
                                 // outstanding iterator
        };
        using CircularBufferDataStorage = std::list<CircularBufferData>;
        typedef typename CircularBufferDataStorage::iterator       InternalCircularBufferIterator;
        typedef typename CircularBufferDataStorage::const_iterator InternalCircularBufferConstIterator;

        /**
         * \class CircularBufferIterator
         * \brief A struct that represents an entry in the CircularBuffer.
         * The struct can be queried at any time for the accurate index
         * of the item in the CircularBuffer.
         *
         * The CircularBufferIterator will throw an exception when
         * accessed if the entry represented by this
         * CircularBufferIterator in the CircularBuffer is no longer
         * valid.  This indicates that the original data at that
         * location has been overwritten.
         *
         * CircularBufferIterator also responds to comparison
         * operators. The entry's locations in the CircularBuffer will
         * be compared, NOT the data.
         *
         */
        template <bool is_const_iterator = true>
        class CircularBufferIterator :
            public utils::IteratorTraits<std::bidirectional_iterator_tag, value_type>
        {
        private:
            // Constant to indicate constness
            static constexpr bool is_const_iterator_type = is_const_iterator;

            friend class CircularBuffer<value_type>;
            typedef typename std::conditional<is_const_iterator,
                                              const value_type &, value_type &>::type DataReferenceType;
            typedef typename std::conditional<is_const_iterator,
                                              const CircularBufferType *, CircularBufferType *>::type CircularBufferPointerType;

            // Pointer to the CircularBuffer which created this entry
            CircularBufferPointerType attached_circularbuffer_ = nullptr;

            typedef typename std::conditional<is_const_iterator,
                                              InternalCircularBufferConstIterator,
                                              InternalCircularBufferIterator>::type SpecificCircularBufferIterator;
            SpecificCircularBufferIterator circularbuffer_entry_;

            //! The validity ID
            uint64_t window_idx_ = std::numeric_limits<uint64_t>::max();

            /**
             * \brief Internally construct an iterator
             * \param CircularBuffer a pointer to the underlaying CircularBuffer.
             * \param index The index this iterator points to in the CircularBuffer
             */
            CircularBufferIterator(CircularBufferPointerType circularbuffer,
                                   SpecificCircularBufferIterator entry,
                                   const uint64_t window_idx) :
                attached_circularbuffer_(circularbuffer),
                circularbuffer_entry_(entry),
                window_idx_(window_idx)
            {}

            // Used by the CircularBuffer to get the position in the
            // internal CircularBuffer
            SpecificCircularBufferIterator getInternalBufferEntry_() const {
                return circularbuffer_entry_;
            }

        public:

            /**
             * \brief Deleted default constructor
             */
            CircularBufferIterator() = default;

            /**
             * \brief a copy constructor that allows for implicit conversion from a
             * regular iterator to a const_iterator.
             */
            CircularBufferIterator(const CircularBufferIterator<false> & iter) :
                attached_circularbuffer_(iter.attached_circularbuffer_),
                circularbuffer_entry_(iter.circularbuffer_entry_),
                window_idx_(iter.window_idx_)
            {}

            /**
             * \brief Assignment operator
             * The copy also alerts the validator_ item that another CircularBufferIterator is
             * now attached to it.
             */
            CircularBufferIterator& operator=(const CircularBufferIterator& rhs) = default;

            /// override the comparison operator.
            bool operator<(const CircularBufferIterator& rhs) const
            {
                sparta_assert(attached_circularbuffer_ == rhs.attached_circularbuffer_,
                            "Cannot compare CircularBufferIterators created by different CircularBuffers.");
                return window_idx_ > rhs.window_idx_;
            }

            /// override the comparison operator.
            bool operator>(const CircularBufferIterator& rhs) const
            {
                sparta_assert(attached_circularbuffer_ == rhs.attached_circularbuffer_,
                            "Cannot compare CircularBufferIterators created by different CircularBuffers.");
                return window_idx_ < rhs.window_idx_;
            }

            /// override the comparison operator.
            bool operator==(const CircularBufferIterator& rhs) const
            {
                sparta_assert(attached_circularbuffer_ == rhs.attached_circularbuffer_,
                            "Cannot compare CircularBufferIterators created by different CircularBuffers.");
                return (window_idx_ == rhs.window_idx_);
            }

            /// override the not equal operator.
            bool operator!=(const CircularBufferIterator& rhs) const
            {
                sparta_assert(attached_circularbuffer_ == rhs.attached_circularbuffer_,
                            "Cannot compare CircularBufferIterators created by different CircularBuffers.");
                return !operator==(rhs);
            }

            /// Checks validity of the iterator
            /// \return Returns false if the iterator is not valid
            bool isValid() const
            {
                if(attached_circularbuffer_) {
                    return attached_circularbuffer_->isValidIterator_(window_idx_);
                }
                return false;
            }

            /// override the dereferencing operator
            DataReferenceType operator* () {
                sparta_assert(attached_circularbuffer_,
                            "This iterator is not attached to a CircularBuffer. Was it initialized?");
                sparta_assert(isValid(), "Iterator is not valid for dereferencing");
                return circularbuffer_entry_->data;
            }
            value_type* operator->()
            {
                sparta_assert(attached_circularbuffer_,
                            "This iterator is not attached to a CircularBuffer. Was it initialized?");
                sparta_assert(isValid(), "Iterator is not valid for dereferencing");
                return &circularbuffer_entry_->data;
            }
            const value_type* operator->() const
            {
                sparta_assert(attached_circularbuffer_,
                            "This iterator is not attached to a CircularBuffer. Was it initialized?");
                sparta_assert(isValid(), "Iterator is not valid for dereferencing");
                return &circularbuffer_entry_->data;
            }

            /** brief Move the iterator forward to point to next element in queue ; PREFIX
             */
            CircularBufferIterator & operator++() {
                sparta_assert(attached_circularbuffer_,
                            "This iterator is not attached to a CircularBuffer. Was it initialized?");
                if(isValid()) {
                    ++window_idx_;
                    ++circularbuffer_entry_;
                }
                else {
                    sparta_assert(!"Attempt to increment an invalid iterator");
                }
                return *this;
            }

            /// Move the iterator forward to point to next element in queue ; POSTFIX
            CircularBufferIterator operator++ (int) {
                CircularBufferIterator buf_iter(*this);
                operator++();
                return buf_iter;
            }

            /// Move the iterator to point to prev element in queue ; POSTFIX
            CircularBufferIterator & operator-- ()
            {
                sparta_assert(attached_circularbuffer_, "The iterator is not attached to a CircularBuffer. Was it initialized?");
                if(attached_circularbuffer_->isValidIterator_(window_idx_ - 1)) {
                    --window_idx_;
                    --circularbuffer_entry_;
                }
                else {
                    sparta_assert(!"Attempt to decrement an iterator beyond bounds or that is invalid");
                }
                return *this;
            }

            /// Move the iterator to point to prev element in queue ; POSTFIX
            CircularBufferIterator  operator-- (int) {
                CircularBufferIterator buf_iter(*this);
                operator--();
                return buf_iter;
            }

            /**
             * Make CircularBufferIterator<true> a friend class of CircularBufferIterator<false>
             * So const iterator can access non-const iterators private members
             */
            friend class CircularBufferIterator<true>;
        };

        /*
         * Custom class for reverse iterator to support validity checking
         */
        template <typename iter_type>
        class CircularBufferReverseIterator : public std::reverse_iterator<iter_type>
        {
        public:
            typedef typename std::conditional<iter_type::is_const_iterator_type,
                                              InternalCircularBufferConstIterator,
                                              InternalCircularBufferIterator>::type SpecificCircularBufferIterator;
            SpecificCircularBufferIterator circularbuffer_entry_;

            explicit CircularBufferReverseIterator(const iter_type & it) :
                std::reverse_iterator<iter_type>(it)
            {}

            template<class U >
            CircularBufferReverseIterator(const std::reverse_iterator<U> & other) :
                std::reverse_iterator<iter_type>(other)
            {}

            bool isValid() const {
                auto it = std::reverse_iterator<iter_type>::base();
                try {
                    return (--it).isValid();
                }
                catch(...) {
                    return false;
                }
                return false;
            }
        private:
            friend class CircularBuffer<value_type>;
            SpecificCircularBufferIterator getInternalBufferEntry_() const {
                auto it = std::reverse_iterator<iter_type>::base();
                return (--it).circularbuffer_entry_;
            }
        };

    public:

        /// Typedef for regular iterator
        typedef CircularBufferIterator<false>                 iterator;

        /// Typedef for constant iterator
        typedef CircularBufferIterator<true>                  const_iterator;

        /// Typedef for regular reverse iterator
        typedef CircularBufferReverseIterator<const_iterator> const_reverse_iterator;

        /// Typedef for constant reverse iterator
        typedef CircularBufferReverseIterator<iterator>       reverse_iterator;

        /**
         * \brief Construct a CircularBuffer
         * \param name The name of the CircularBuffer
         * \param max_size The number of entries this CircularBuffer can hold before wrapping
         * \param clk The clock this CircularBuffer is associated with -- used for CycleCounter
         * \param statset Pointer to the counter set to register
         *                utilization counts; default nullptr. This
         *                works for timed and untimed.
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
         *          VIS_SPARTA_DEFAULT, for this structure
         *          VIS_SPARTA_DEFAULT resolves to
         *          SPARTA_CONTAINER_DEFAULT which at the time of
         *          writing this comment is set to VIS_HIDDEN. If you
         *          rely on the stats from this container you should
         *          explicity set the visibility.
         */
        CircularBuffer(const std::string & name,
                       const uint32_t max_size,
                       const Clock * clk,
                       StatisticSet * statset = nullptr,
                       InstrumentationNode::visibility_t stat_vis_general = InstrumentationNode::AUTO_VISIBILITY,
                       InstrumentationNode::visibility_t stat_vis_detailed = InstrumentationNode::VIS_HIDDEN,
                       InstrumentationNode::visibility_t stat_vis_max = InstrumentationNode::AUTO_VISIBILITY,
                       InstrumentationNode::visibility_t stat_vis_avg = InstrumentationNode::AUTO_VISIBILITY);

        /// No copies allowed for CircularBuffer
        CircularBuffer(const CircularBuffer<value_type> & ) = delete;

        /// No copies, no moves allowed for CircularBuffer
        CircularBuffer &operator=(const CircularBuffer<value_type> &) = delete;

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
            collector_.
                reset(new collection::IterableCollector<CircularBuffer<DataT> >(parent, getName(),
                                                                                this, capacity()));
        }

        //! Get this CircularBuffer's name
        std::string getName() const {
            return name_;
        }

        /**
         * \brief Determine if data at the index is valid
         * \param idx The index to determine validity
         * \return true if valid; false otherwise
         *
         */
        bool isValid(uint32_t idx) const {
            return idx < size();
        }

        /**
         * \brief Return the fixed size of this CircularBuffer
         * \return The size of this CircularBuffer
         */
        size_type capacity() const {
            return max_size_;
        }

        /**
         * \brief Return the number of valid entries
         * \return The number of valid entries.  Does not subtract entries erased \a this cycle
         */
        size_type size() const {
            return num_valid_;
        }

        /**
         * \brief Return the number of free entries.
         * \return The number of free entries
         *
         * Does not take into account the number of erased entries \a this cycle
         */
        size_type numFree() const {
            return capacity() - size();
        }

        /**
         * \brief Append data to the end of CircularBuffer, and return a CircularBufferIterator
         * \param dat Data to be pushed back into the CircularBuffer
         * \return a CircularBufferIterator created to represent the object appended.
         *
         * Append data to the end of CircularBuffer, and return a CircularBufferIterator
         * for the location appeneded. Untimed CircularBuffers will have the
         * data become valid immediately.
         */
        void push_back(const value_type& dat)
        {
            push_backImpl_(dat);
        }

        /**
         * \brief Append data to the end of CircularBuffer, and return a CircularBufferIterator
         * \param dat Data to be pushed back into the CircularBuffer
         * \return a CircularBufferIterator created to represent the object appended.
         *
         * Append data to the end of CircularBuffer, and return a CircularBufferIterator
         * for the location appeneded. Untimed CircularBuffers will have the
         * data become valid immediately.
         */
        void push_back(value_type&& dat)
        {
            push_backImpl_(std::move(dat));
        }

        /*!
         * \brief Insert the given data before the given iterator
         * \param entry The interator to insert the data before
         * \param dat   The data to insert
         */
        iterator insert(const iterator & entry, const value_type& dat)
        {
            return insertEntry_(entry, dat);
        }

        /*!
         * \brief Insert the given data before the given iterator
         * \param entry The interator to insert the data before
         * \param dat   The data to insert
         */
        iterator insert(const iterator & entry, value_type&& dat)
        {
            return insertEntry_(entry, std::move(dat));
        }

        /*!
         * \brief Insert the given data before the given iterator
         * \param entry The interator to insert the data before
         * \param dat   The data to insert
         */
        iterator insert(const const_iterator & entry, const value_type& dat)
        {
            return insertEntry_(entry, dat);
        }

        /*!
         * \brief Insert the given data before the given iterator
         * \param entry The interator to insert the data before
         * \param dat   The data to insert
         */
        iterator insert(const const_iterator & entry, value_type&& dat)
        {
            return insertEntry_(entry, std::move(dat));
        }

        /**
         * \brief erase the index at which the entry exists in the CircularBuffer.
         * \param entry a reference to the entry to be erased.
         */
        void erase(const_iterator entry)
        {
            sparta_assert(entry.attached_circularbuffer_ == this,
                        "Cannot erase an entry created by another CircularBuffer");
            eraseEntry_(entry);
        }

        /**
         * \brief erase the index at which the entry exists in the CircularBuffer.
         * \param entry a reference to the entry to be erased.
         */
        void erase(const_reverse_iterator entry)
        {
            sparta_assert(entry.base().attached_circularbuffer_ == this,
                        "Cannot erase an entry created by another CircularBuffer");
            eraseEntry_(entry);
        }
        void erase(reverse_iterator entry) {
            erase(const_reverse_iterator(entry));
        }

        /**
         * \brief Empty the contents of the CircularBuffer
         *
         */
        void clear()
        {
            circularbuffer_data_.clear();
            num_valid_ = 0;
            start_idx_ = end_idx_;
            updateUtilizationCounters_();
        }

        /**
         * \brief Get the iterator pointing to the oldest entry of
         *        CircularBuffer
         *
         * \return Iterator pointing to oldest element in CircularBuffer
         */
        iterator begin() {
            if(size()) {
                return iterator(this,
                                circularbuffer_data_.begin(),
                                circularbuffer_data_.begin()->window_idx);
            }
            return end();
        }

        /**
         * \brief Returns an iterator referring to past-the-end of the
         *        newest element in the CircularBuffer
         */
        iterator end() {
            return iterator(this, circularbuffer_data_.end(), end_idx_);
        }

        /**
         * \brief Get the iterator pointing to the oldest entry of
         *        CircularBuffer
         *
         * \return Iterator pointing to oldest element in CircularBuffer
         */
        const_iterator begin() const {
            if(size()) {
                return const_iterator(this,
                                      circularbuffer_data_.begin(),
                                      circularbuffer_data_.begin()->window_idx);
            }
            return end();
        }

        /**
         * \brief Returns an iterator referring to past-the-end of the
         *        newest element in the CircularBuffer
         */
        const_iterator end() const {
            return const_iterator(this, circularbuffer_data_.end(), end_idx_);
        }

        /**
         * \brief Get the iterator pointing to the oldest entry of
         *        CircularBuffer
         *
         * \return Iterator pointing to oldest element in CircularBuffer
         */
        reverse_iterator rbegin(){
            return reverse_iterator(end());
        }

        /**
         * \brief Returns an iterator referring to past-the-end of the
         *        newest element in the CircularBuffer
         */
        reverse_iterator rend() { return reverse_iterator(begin()); }

        /**
         * \brief Get the iterator pointing to the oldest entry of
         *        CircularBuffer
         *
         * \return Iterator pointing to oldest element in CircularBuffer
         */
        const_reverse_iterator rbegin() const {
            return const_reverse_iterator(end());
        }

        /**
         * \brief Returns an iterator referring to past-the-end of the
         *        newest element in the CircularBuffer
         */
        const_reverse_iterator rend() const { return const_reverse_iterator(begin());}

        /**
         * \brief Returns the data at the given index.  Will assert if
         * out of range
         *
         * \note This method is not const due to a bug in GCC.  See
         * the data type in CircularBufferIterator
         */
        value_type operator[](const uint32_t idx) {
            sparta_assert(idx < size(), "Index out of range");
            auto it = this->begin();
            std::advance(it, idx);
            return *it;
        }

    private:

        // Used by the internal iterator type to see if it's still
        // valid
        bool isValidIterator_(uint32_t window_idx) const {
            return (window_idx >= start_idx_ && window_idx < end_idx_);
        }

        template<typename EntryIteratorT>
        void eraseEntry_(const EntryIteratorT & entry)
        {
            sparta_assert(entry.isValid());
            circularbuffer_data_.erase(entry.getInternalBufferEntry_());
            --num_valid_;
            invalidateIndexes_();
        }

        template<typename U>
        void push_backImpl_(U&& dat)
        {
            circularbuffer_data_.emplace(circularbuffer_data_.end(), std::forward<U>(dat), end_idx_);
            ++end_idx_;
            if(num_valid_ == max_size_) {
                circularbuffer_data_.pop_front();
                ++start_idx_;
            }
            else {
                ++num_valid_;
            }

            // XXX Remove when testing complete.  This is an O(n) operation
            sparta_assert(circularbuffer_data_.size() <= size());

            updateUtilizationCounters_();
        }

        template<typename EntryIteratorT, typename U>
        iterator insertEntry_(const EntryIteratorT & entry, U&& dat)
        {
            // If the buffer is empty, the iterator is not valid, so
            // just do a push_back
            if(circularbuffer_data_.empty()) {
                push_back(std::forward<U>(dat));
                return begin();
            }
            sparta_assert(entry.isValid(),
                        "Cannot insert into Circularbuffer at given iterator");
            auto it = circularbuffer_data_.insert(entry.getInternalBufferEntry_(),
                                                  CircularBufferData(std::forward<U>(dat), end_idx_));
            ++num_valid_;
            invalidateIndexes_();
            return iterator(this, it, it->window_idx);
        }

        void invalidateIndexes_() {
            // To invalidate any and all outstanding iterators, move
            // the start/end indexes outside the current window.  Do
            // not set the start_idx_ to the old end_idx_ as any older
            // "end" iterator would be considered valid (equals the
            // start_idx_)
            start_idx_ = ++end_idx_;
            for(auto & it : circularbuffer_data_) {
                it.window_idx = end_idx_;
                ++end_idx_;
            }
        }

        void updateUtilizationCounters_() {
            // Update Counters
            if(!utilization_count_.empty()) {
                if(previous_valid_entry_ != num_valid_) {
                    utilization_count_[previous_valid_entry_]->stopCounting();
                    utilization_count_[num_valid_]->startCounting();
                    previous_valid_entry_ = num_valid_;
                }
                if (num_valid_ > utilization_max_->get()) {
                    utilization_max_->set(num_valid_);
                }
            }
        }

        const size_type            max_size_;            /*!< The number of entries this CircularBuffer can hold */
        const std::string          name_;                /*!< The name of this CircularBuffer */
        CircularBufferDataStorage  circularbuffer_data_; /*!< A vector list of pointers to all the items active in the CircularBuffer */

        size_type       num_valid_       = 0;  /*!< A tally of valid items */
        uint64_t        start_idx_       = 0;  //!< The CircularBuffer is implemented like a sliding window.
                                               //!  This is the first element of that window
        uint64_t        end_idx_         = 0;  //!< The CircularBuffer is implemented like a sliding window.
                                               //!   This is the last element in that window

        //////////////////////////////////////////////////////////////////////
        // Counters
        std::vector<std::unique_ptr<sparta::CycleCounter>> utilization_count_;
        std::unique_ptr<StatisticDef> weighted_utilization_avg_;
        std::unique_ptr<StatisticInstance> avg_instance_;
        std::unique_ptr<sparta::Counter> utilization_max_;

        // The last valid entry (to close out a utilization)
        uint32_t previous_valid_entry_ = 0;

        //////////////////////////////////////////////////////////////////////
        // Collectors
        std::unique_ptr<collection::IterableCollector<CircularBuffer<value_type> > > collector_;
    };

    ////////////////////////////////////////////////////////////////////////////////
    // Definitions...
    template<class DataT>
    CircularBuffer<DataT>::CircularBuffer(const std::string & name,
                                          uint32_t max_size,
                                          const Clock * clk,
                                          StatisticSet * statset,
                                          InstrumentationNode::visibility_t stat_vis_general,
                                          InstrumentationNode::visibility_t stat_vis_detailed,
                                          InstrumentationNode::visibility_t stat_vis_max,
                                          InstrumentationNode::visibility_t stat_vis_avg) :
        max_size_(max_size),
        name_(name)
    {
        if(statset)
        {
            std::stringstream expression_numerator;
            std::stringstream expression_denum;
            expression_denum << "( ";
            expression_numerator << "( ";
            // The size is 0 entries up to max_size
            for(uint32_t i = 0; i < max_size_ + 1; ++i)
            {
                std::stringstream str;
                str << name << "_util_cnt" << i;

                InstrumentationNode::visibility_t visibility = stat_vis_detailed;

                if (i == 0 || i == (max_size_)) {
                    if(stat_vis_general == InstrumentationNode::AUTO_VISIBILITY)
                    {
                        visibility = InstrumentationNode::CONTAINER_DEFAULT_VISIBILITY;
                    }
                    else{
                        visibility = stat_vis_general;
                    }
                }
                else // we are internal counts for different levels of utilization.
                {
                    if(stat_vis_detailed == InstrumentationNode::AUTO_VISIBILITY)
                    {
                        visibility = InstrumentationNode::CONTAINER_DEFAULT_VISIBILITY;
                    }
                    else{
                        visibility = stat_vis_detailed;
                    }
                }

                utilization_count_.
                    emplace_back(new CycleCounter(statset,
                                                  str.str(),
                                                  name + "_utilization_count", i,
                                                  "Entry Utilization Counts of " + name,
                                                  CounterBase::COUNT_NORMAL, clk, visibility));
                expression_numerator << "( "<< i << " * " << name << "_util_cnt" << i << " )";
                expression_denum << name << "_util_cnt" << i;
                if( i != max_size_ )
                {
                    expression_numerator << " + ";
                    expression_denum << " + ";
                }

            }
            expression_numerator << " )";
            expression_denum << " )";
            utilization_count_[0]->startCounting();

            if(stat_vis_avg == InstrumentationNode::AUTO_VISIBILITY)
            {
                stat_vis_avg = InstrumentationNode::DEFAULT_VISIBILITY;
            }
            // add a statasticdef to the set for the weighted average    expression_numerator.str() + "/" + expression_denum.str()
            weighted_utilization_avg_.reset(new StatisticDef(statset,
                                                             name + "_utilization_weighted_avg",
                                                             "Calculate the weighted average of the CircularBuffer's utilization",
                                                             statset,
                                                             expression_numerator.str() + " / " + expression_denum.str(),
                                                             sparta::StatisticDef::VS_ABSOLUTE, stat_vis_avg));

            // Add a counter to track the maximum utilization
            if(stat_vis_max == InstrumentationNode::AUTO_VISIBILITY)
            {
                stat_vis_max = InstrumentationNode::DEFAULT_VISIBILITY;
            }
            utilization_max_.reset(new Counter(statset,
                                               name + "_utilization_max",
                                               "The maximum utilization",
                                               CounterBase::COUNT_LATEST,
                                               stat_vis_max));
        }
    }



}
