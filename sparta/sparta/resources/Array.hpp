// <Array.hpp> -*- C++ -*-

/**
 * \file Array.hpp
 * \brief Defines the Array class
 *
 */
#pragma once

#include <cinttypes>
#include <vector>
#include <string>
#include <memory>

#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/statistics/CycleHistogram.hpp"
#include "sparta/collection/IterableCollector.hpp"
#include "sparta/utils/IteratorTraits.hpp"

namespace sparta
{
    /**
     * \enum ArrayType
     * \brief Defines how a sparta::Array should behave. The array will have
     *        different features depending on the type.
     */
    enum class ArrayType {
        NORMAL, /**< The array does NOT allow access or maintain information about age. */
        AGED    /**< The array allows fuctions that require a concept of age in each entry */
    };

    /**
     * \class Array
     *
     * \brief Array is essentially a fixed size vector, maintains a
     *        concept of validity of its indexes, and provides access
     *        via stl iteration and general get methods.
     * \tparam DataT the data type stored in the Array.
     * \tparam ArrayT the type of array ArrayType::AGED vs ArrayType::NORMAL
     *
     * The Array class maintains a list of elements, constrained to
     * the number of entries, allowing a user to add/remove entries
     * from the middle, beginning, or end without collapsing.  If the
     * template parameter ArrayType::AGED is provided (default), the
     * array will keep track of the age of the internal components.
     *
     * The methods begin() and end() will return iterators to the
     * array, with begin() always returning an iterator pointing to
     * index 0 and end() pointing beyond it.  The iterator \b might \b
     * not be pointing to valid data, so a call to isValid() on the
     * iterator is required before dereferencing.
     *
     * To iterator over the aged list, use the methods abegin() and
     * aend().
     */
    template<class DataT, ArrayType ArrayT = ArrayType::AGED>
    class Array
    {
    private:
        //! A struct for a position in our array.
        struct ArrayPosition{
            ArrayPosition() :
                valid(false),
                to_validate(false){}

            template<typename U>
            explicit ArrayPosition(U && data) :
                data(std::forward<U>(data)){}

            bool valid;
            bool to_validate;
            DataT data;
            std::list<uint32_t>::iterator list_pointer;
            uint64_t age_id = 0;
        };

        //! Typedef for the underlaying vector used as the basis of the Array
        typedef std::vector<ArrayPosition> ArrayVector;

        //! The array type
        typedef Array<DataT, ArrayT>       FullArrayType;

    public:

        //! The data type, STL style
        typedef DataT    value_type;

        //! Expected typedef for the data that will be stored in this structure.
        typedef DataT    DataType;

        //! Typedef for size_type
        typedef uint32_t size_type;

        //! Typedef for a list of indexes in age order.
        typedef std::list<uint32_t>        AgedList;

        /**
         * \brief An iterator struct for this array.
         *
         * The iterator is a forward iterator and responds to post
         * increment operators.  The iterator will also wrap over the
         * array when it reaches the end.  The iterator can be queried
         * for the validity of the position.  The iterator provides
         * public access to resetting the index it points to.
         * Iterators can be dereferenced to provide the data at the
         * index for which they point.
         */
        template<bool is_const_iterator = true>
        struct ArrayIterator : public utils::IteratorTraits<std::forward_iterator_tag, value_type>
        {
        private:

            typedef typename std::conditional<is_const_iterator,
                                              const value_type &,
                                              value_type &>::type  DataReferenceType;

            typedef typename std::conditional<is_const_iterator,
                                              const FullArrayType *,
                                              FullArrayType *>::type   ArrayPointerType;
            friend FullArrayType;

            /// Private constructor, only the array can create iterators.
            ArrayIterator(ArrayPointerType array,
                          uint32_t start_index,
                          bool is_aged,
                          bool is_circular,
                          bool is_aged_walk) :
                index_(start_index),
                array_(array),
                is_aged_(is_aged),
                is_circular_(is_circular),
                is_aged_walk_(is_aged_walk)
            {}

            /// Cast operator so aged arrays of non-integer data types can
            /// have their iterators be cast to integers
            operator uint32_t() const {
                return index_;
            }

            /// Get access on a non-const iterator
            DataReferenceType getAccess_(std::false_type) {
                return array_->access(index_);
            }

            /// Get access on a const iterator
            DataReferenceType getAccess_(std::true_type) const {
                return array_->read(index_);
            }

            // Allow the creation of a const iterator from non-const
            friend ArrayIterator<false>;
        public:
            /// Empty, invalid iterator
            ArrayIterator() = default;

            /// copy construction is fair game from non-const to const
            ArrayIterator(const ArrayIterator<false> & other) :
                index_(other.index_),
                array_(other.array_),
                is_aged_(other.is_aged_),
                is_circular_(other.is_circular_),
                is_aged_walk_(other.is_aged_walk_)
            {}

            ArrayIterator& operator=(const ArrayIterator& other) = default;

            /// Reset the iterator to an invalid value
            void reset()
            {
                index_ = sparta::notNull(array_)->capacity();
            }

            /// Determine whether this iterator has been initialized with a valid index
            bool isIndexValid() const
            {
                return (index_ < sparta::notNull(array_)->capacity());
            }

            /// What index does our iterator currently represent.
            uint32_t getIndex() const
            {
                sparta_assert(index_ != sparta::notNull(array_)->invalid_entry_,
                              "Cannot operate on an unitialized iterator.");
                return index_;
            }

            /// Determine whether this array entry pointed to by this iterator is valid
            bool isValid() const
            {
                return (array_ != nullptr) &&
                    (index_ != array_->invalid_entry_) &&
                    (array_->isValid(index_));
            }

            /**
             * \brief determine if the data at this iterator was
             * written before the data at another index.
             * \param other the other index.
             * \return true if the data at this iterator is older than
             * the data at other.
             */
            bool isOlder(const uint32_t idx) const
            {
                sparta_assert(index_ < sparta::notNull(array_)->capacity(),
                            "Cannot operate on an uninitialized iterator.");
                return array_->isOlder(index_, idx);
            }

            /// Overload isOlder to accept another iterator instead of an index.
            bool isOlder(const ArrayIterator & other) const
            {
                return isOlder(other.index_);
            }

            /**
             * \brief determine if the data at this iterator
             * was written to more recently than another
             * index.
             * \param idx the index of the other entry to be
             * compared.
             * \return true if the data at this iterator
             * was written to more frequently than the
             * data at idx.
             */
            bool isYounger(const uint32_t idx) const
            {
                sparta_assert(index_ < sparta::notNull(array_)->capacity(),
                            "Cannot operate on an uninitialized iterator.");
                return sparta::notNull(array_)->isYounger(index_, idx);
            }

            /// Overload isYounger to accept another iterator instead of index.
            bool isYounger(const ArrayIterator & other) const
            {
                return isYounger(other.index_);
            }

            /// comparison operators
            /// NOTE: operator< and operator> were removed b/c they have no use case.
            bool operator<(const ArrayIterator &rhs) const {
                return isYounger(rhs);
            }

            bool operator==(const ArrayIterator<true>& rhs) const
            {
                return (rhs.index_ == index_) && (rhs.array_ == array_);
            }

            bool operator==(const ArrayIterator<false> & rhs)
            {
                return (rhs.index_ == index_) && (rhs.array_ == array_);
            }

            bool operator==(const uint32_t & rhs) const
            {
                return (rhs == index_);
            }

            bool operator!=(const ArrayIterator& rhs) const
            {
                return (rhs.index_ != index_) || (rhs.array_ != array_);
            }

            /// support the dereference operator, non-const
            DataReferenceType operator*() {
                sparta_assert(index_ < sparta::notNull(array_)->capacity(),
                            "Cannot operate on an uninitialized iterator.");
                // return the data at our location in the array.
                return getAccess_(std::integral_constant<bool, is_const_iterator>());
            }

            /// support the dereference operator, const
            DataReferenceType operator*() const {
                sparta_assert(index_ < sparta::notNull(array_)->capacity(),
                            "Cannot operate on an uninitialized iterator.");
                // return the data at our location in the array.
                return getAccess_(std::integral_constant<bool, is_const_iterator>());
            }

            /// support -> operator.
            value_type* operator->()
            {
                sparta_assert(index_ < sparta::notNull(array_)->capacity(),
                            "Cannot operate on an uninitialized iterator.");
                return std::addressof(getAccess_(std::integral_constant<bool, is_const_iterator>()));
            }

            const value_type* operator->() const
            {
                sparta_assert(index_ < sparta::notNull(array_)->capacity(),
                            "Cannot operate on an uninitialized iterator.");
                return std::addressof(getAccess_(std::integral_constant<bool, is_const_iterator>()));
            }

            /// pre-increment operator.
            ArrayIterator & operator++()
            {
                sparta_assert(index_ != sparta::notNull(array_)->invalid_entry_,
                            "Cannot operate on an uninitialized iterator.");
                if(is_aged_walk_) {
                    if(!sparta::notNull(array_)->getNextOldestIndex(index_)) {
                        index_ = (sparta::notNull(array_)->getOldestIndex());
                    }
                }
                else {
                    ++index_;
                    if(index_ == sparta::notNull(array_)->capacity()) {
                        index_ = 0;
                    }
                }
                if(is_circular_) { return *this; }

                // Aged iterators will act circular if the array is
                // filled with valid entries.
                if(is_aged_)
                {
                    // We've wrapped around -- we're done.
                    if(*this == sparta::notNull(array_)->abegin()) {
                        *this = sparta::notNull(array_)->aend();
                    }
                    else {
                        // If the iterator is not pointing to a valid
                        // entry, try again.
                        if(!isValid()) {
                            this->operator++();
                        }
                    }
                }
                else {
                    if(*this == sparta::notNull(array_)->begin()) {
                        // We've wrapped around and have hit begin.  We're at
                        // the end now.
                        *this = sparta::notNull(array_)->end();
                    }
                }
                return *this;
            }

            /// post-increment operator.
            ArrayIterator operator++(int)
            {
                iterator old_iter(*this);
                // do the same functionality as the pre increment.
                operator++();
                // return the state before the increment.
                return old_iter;
            }

        private:
            uint32_t index_ = std::numeric_limits<uint32_t>::max(); /*!< The index that this iterator points to. */
            ArrayPointerType array_ = nullptr; /*!< The parent Array that created this iterator. */
            bool is_aged_      = false; //!< Is this an aged iterator?
            bool is_circular_  = false; //!< Is this a circular iterator?
            bool is_aged_walk_ = false; //!< Should iterator walk in age-order?
        };

        /// Typedef for regular iterator
        typedef ArrayIterator<false> iterator;

        /// Typedef for constant iterator
        typedef ArrayIterator<true>  const_iterator;

        /**
         * \brief Get an iterator that is circular on the Array (has no end())
         * \param idx Where to start from
         * \return iterator type that a ++ will never == end()
         */
        iterator getCircularIterator(uint32_t idx=0)
        {
            constexpr bool is_aged = false;
            constexpr bool is_circular = true;
            constexpr bool is_aged_walk = false;
            return iterator(this, idx, is_aged, is_circular, is_aged_walk);
        }

        /// Provide a method to get an uninitialized iterator.
        iterator getUnitializedIterator()
        {
            constexpr bool is_aged = false;
            constexpr bool is_circular = false;
            constexpr bool is_aged_walk = false;
            return iterator(this, invalid_entry_, is_aged, is_circular, is_aged_walk);
        }

        /// \brief STL-like begin operation, starts at index 0 (ignores valid bit).
        /// \return iterator to the beginning of the Array
       iterator begin() {
            constexpr bool is_aged = false;
            constexpr bool is_circular = false;
            constexpr bool is_aged_walk = false;
            return iterator(this, 0, is_aged, is_circular, is_aged_walk);
        }

        /// \brief STL-like begin operation, const, starts at index 0 (ignores valid bit).
        /// \return iterator to the beginning of the Array
        const_iterator begin() const {
            constexpr bool is_aged = false;
            constexpr bool is_circular = false;
            constexpr bool is_aged_walk = false;
            return const_iterator(this, 0, is_aged, is_circular, is_aged_walk);
        }

        /// \brief STL-like end operation.
        /// \return iterator to the end of the Array, past the last element
        iterator end() {
            constexpr bool is_aged = false;
            constexpr bool is_circular = false;
            constexpr bool is_aged_walk = false;
            return iterator(this, invalid_entry_, is_aged, is_circular, is_aged_walk);
        }

        /// \brief STL-like end operation, const.
        /// \return iterator to the end of the Array, past the last element
        const_iterator end() const {
            constexpr bool is_aged = false;
            constexpr bool is_circular = false;
            constexpr bool is_aged_walk = false;
            return const_iterator(this, invalid_entry_, is_aged, is_circular, is_aged_walk);
        }

        /// \brief STL-like begin operation, starts at the oldest valid index.
        /// \return iterator to the oldest entry in the array
        iterator abegin() {
            constexpr bool is_aged = true;
            constexpr bool is_circular = false;
            constexpr bool is_aged_walk = true;
            if(numValid()) {
                return iterator(this, getOldestIndex().getIndex(), is_aged, is_circular, is_aged_walk);
            }
            return  iterator(this, invalid_entry_, is_aged, is_circular, is_aged_walk);
        }

        /// \brief STL-like begin operation, const, starts at the oldest
        ///        valid index.
        /// \return iterator to the oldest entry in the array
        const_iterator abegin() const {
            constexpr bool is_aged = true;
            constexpr bool is_circular = false;
            constexpr bool is_aged_walk = true;;
            if(numValid()) {
                return getOldestIndex();
            }
            return  const_iterator(this, invalid_entry_, is_aged, is_circular, is_aged_walk);
        }

        /// \brief STL-like end operation.
        /// \return iterator to the element past the youngest entry
        iterator aend() {
            constexpr bool is_aged = true;
            constexpr bool is_circular = false;
            constexpr bool is_aged_walk = true;
            return iterator(this, invalid_entry_, is_aged, is_circular, is_aged_walk);
        }

        /// \brief STL-like end operation, const.
        /// \return iterator to the element one past the youngest entry
        const_iterator aend() const {
            constexpr bool is_aged = true;
            constexpr bool is_circular = false;
            constexpr bool is_aged_walk = true;
            return const_iterator(this, invalid_entry_, is_aged, is_circular, is_aged_walk);
        }

        /**
         * \brief Construct an array.
         * \param name The name of the buffer
         * \param num_entries The number of entries this buffer can hold
         * \param clk The clock this buffer belongs to
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
         *          AUTO_VISIBILITY resolves to
         *          CONTAINER_DEFAULT_VISIBILITY which at the time of
         *          writing this comment is set to VIS_HIDDEN. If you
         *          rely on the stats from this container you should
         *          explicitly set the visibility.
         */
        Array(const std::string& name, uint32_t num_entries, const Clock* clk,
              StatisticSet* statset = nullptr,
              InstrumentationNode::visibility_t stat_vis_general = InstrumentationNode::AUTO_VISIBILITY,
              InstrumentationNode::visibility_t stat_vis_detailed= InstrumentationNode::VIS_HIDDEN,
              InstrumentationNode::visibility_t stat_vis_max = InstrumentationNode::AUTO_VISIBILITY,
              InstrumentationNode::visibility_t stat_vis_avg = InstrumentationNode::AUTO_VISIBILITY);

        //! Virtual destructor.
        virtual ~Array() {
            clear();
        }

        /*!
         * \brief Name of this resource.
         */
        const std::string & getName() const {
            return name_;
        }

        /**
         * \brief Determine whether an index is currently valid.
         * \return true if valid.
         */
        bool isValid(const uint32_t idx) const {
            return valid_index_set_.find(idx) != valid_index_set_.end();
        }

        /**
         * \brief Read (only) the data at an index.
         * \param idx the index to access.
         * \return A const reference to the data.
         */
        const DataT& read(const uint32_t idx) const {
            sparta_assert(isValid(idx), "On Array " << name_
                        << " Cannot read from an invalid index. Idx:" << idx);
            return array_[idx].data;
        }

        /**
         * \brief Access (writeable) the data at a position.
         * \param idx The index to access.
         */
        DataT& access(const uint32_t idx) {
            sparta_assert(isValid(idx), "On Array " << name_
                        << " Cannot read from an invalid index. Idx:" << idx);
            return (array_[idx].data);
        }

        /**
         * \brief Return the oldest index in the array.
         * \param nth Is the nth oldest entry you are looking for.
         * nth=0 is the oldest entry, nth = 1 is the second oldest, etc..
         * \return the index of the nth oldest entry.
         * \warning this method may be expensive for larger nth values. Low values
         * should be very quick.
         *
         * \note This method is only accessible if the template parameter
         *       FullArrayType == AGED
         *
         */
        const_iterator getOldestIndex(const uint32_t nth=0) const
        {
            sparta_assert(ArrayT == ArrayType::AGED,
                        "Only AgedArray types have public member function getOldestIndex");
            sparta_assert(nth < num_valid_,
                        "The array does not have enough elements to find the nth oldest index");

            constexpr bool is_aged = true;
            constexpr bool is_circular = false;
            constexpr bool is_aged_walk = true;

            // Since our data_list_ always adds new items to the
            // front.  The oldest data is actually kept at the end of
            // the list.  We can iterate from the end to find the nth
            // oldest item.
            auto it = aged_list_.rbegin();
            uint32_t idx = invalid_entry_;
            for(uint32_t i = 0; i <= nth; ++i)
            {
                idx = *it;
                ++it;
            }
            // Double check that we are returning the user a valid
            // index.  We have failed if it isn't.
            sparta_assert(isValid(idx));
            return const_iterator(this, idx, is_aged, is_circular, is_aged_walk);
        }

        /**
         * \brief provide access the youngest index in the array.
         * \param nth Is the nth youngest entry to be found. nth=0 is
         *            the youngest, nth=1 is the second youngest, etc.
         * \return the index of the nth youngest index.
         * \warning this method may get expensive for larger nth values.
         *
         * \note This method is only accessible if the template parameter
         *       FullArrayType == AGED.
         */
        const_iterator getYoungestIndex(const uint32_t nth=0) const
        {
            sparta_assert(ArrayT == ArrayType::AGED,
                        "Only AgedArray types provide access to public member"
                        " function getYoungestIndex");
            sparta_assert(nth < num_valid_,
                        "The array does not have enough elements to find the nth youngest index");
            constexpr bool is_aged = true;
            constexpr bool is_circular = false;
            constexpr bool is_aged_walk = true;

            // Define idx, and set it to a value surely beyond the bounds of our array.
            uint32_t idx = invalid_entry_;
            auto it = aged_list_.begin();
            for(uint32_t i = 0; i <= nth; ++i)
            {
                idx = *it;
                ++it;
            }
            // Make sure we found something valid.
            sparta_assert(isValid(idx));
            return const_iterator(this, idx, is_aged, is_circular, is_aged_walk);
        }

        /**
         * \brief Sets the input argument to the index containing the
         *  location of the next oldest item after input argument.
         *  If the input argument is the youngest index, we return false.
         */
        bool getNextOldestIndex(uint32_t & prev_idx) const {
            auto it = std::find(aged_list_.begin(), aged_list_.end(), prev_idx);
            if(it == aged_list_.begin()) {
                return false;
            }
            --it;
            prev_idx = *it;
            return true;
        }

        /**
         * \brief Provide access to our aged_list_ internals.
         */
        const AgedList & getAgedList() const
        {
            sparta_assert(ArrayT == ArrayType::AGED);
            return aged_list_;
        }

        /**
         * \brief Return the maximum number of elements this Array can
         *        hold.
         * \return The number of positions this array was constructed with
         */
        size_type capacity() const {
            return num_entries_;
        }

        /**
         * \brief The number of valid entries contained.
         * \return The number of valid elements in the array
         */
        size_type numValid() const {
            return num_valid_;
        }

        //! \return numValid() -- function for stl compatibility.
        size_type size() const {
            return numValid();
        }

        /**
         * \brief The number of free entries.
         * \return The number of free spaces in the array.
         */
        size_type numFree() const {
            sparta_assert(num_entries_ >= num_valid_);
            return num_entries_ - num_valid_;
        }

        /**
         * \brief Invalidate data at an iterator position.
         * \param iter An iterator used to invalidate -- must be valid
         */
        void erase(const iterator& iter) {
            sparta_assert(iter.isValid());
            erase(iter.getIndex());
        }

        /**
         * \brief Invalidate the entry at a certain index.
         * \param idx The index to invalidate.
         */
        void erase(const uint32_t& idx)
        {
            sparta_assert(isValid(idx), "Cannot invalidate a non valid index.");
            sparta_assert(num_valid_ > 0);

            // Just set the data to invalid.
            array_[idx].valid = false;
            --num_valid_;
            // Remove the index from our aged list.
            if(ArrayT == ArrayType::AGED)
            {
                aged_list_.erase(array_[idx].list_pointer);
            }

            // Update occupancy counter.
            if(utilization_)
            {
                utilization_->setValue(num_valid_);
            }
            array_[idx].~ArrayPosition();
            valid_index_set_.erase(idx);
        }

        /**
         * \brief Clear the array of all data.
         */
        void clear()
        {
            std::for_each(valid_index_set_.begin(), valid_index_set_.end(), [this](const auto index){
                array_[index].~ArrayPosition();
            });
            valid_index_set_.clear();
            aged_list_.clear();
            num_valid_ = 0;
            if(utilization_)
            {
                utilization_->setValue(num_valid_);
            }
        }

        /**
         * \brief Write data to the array.
         * \param idx The index to write at
         * \param dat The data to write at that index.
         */
        void write(const uint32_t idx, const DataT& dat)
        {
            writeImpl_(idx, dat);
        }

        /**
         * \brief Write data to the array.
         * \param idx The index to write at
         * \param dat The data to write at that index.
         */
        void write(const uint32_t idx, DataT&& dat)
        {
            writeImpl_(idx, std::move(dat));
        }

        /**
         * \brief Write data at an iterator position.
         * \param iter The iterator pointing to the data
         * \param dat The data to write at the iterator location
         *
         * This will write to the location at \a iter, whether
         * the iterator is valid or not.
         */
        void write(const iterator& iter, const DataT& dat)
        {
            write(iter.getIndex(), dat);
        }

        /**
         * \brief Write data at an iterator position.
         * \param iter The iterator pointing to the data
         * \param dat The data to write at the iterator location
         *
         * This will write to the location at \a iter, whether
         * the iterator is valid or not.
         */
        void write(const iterator& iter, DataT&& dat)
        {
            write(iter.getIndex(), std::move(dat));
        }

        /**
         * \brief Determine if an index was written (using write()) to the
         * Array after another index.
         * \param lhs an index to compare.
         * \param rhs an index to compare.
         * \return true if lhs was written more recently than rhs to the array.
         */
        bool isYounger(uint32_t lhs, uint32_t rhs)
        {
            sparta_assert(lhs != rhs);
            sparta_assert(lhs < num_entries_ && rhs < num_entries_,
                          "Cannot compare age on an index outside the bounds of the array");
            return array_[lhs].age_id > array_[rhs].age_id;
        }

        /**
         * \brief Determine if an index was written (using write()) to the
         * Array before another index.
         * \param lhs an index to compare.
         * \param rhs an index to compare.
         * \return true if lhs was written at less recently than rhs was written at.
         */
        bool isOlder(uint32_t lhs, uint32_t rhs)
        {
            sparta_assert(lhs != rhs);
            sparta_assert(lhs < num_entries_ && rhs < num_entries_,
                          "Cannot compare age on an index outside the bounds of the array");
            return  array_[lhs].age_id < array_[rhs].age_id;
        }

        /**
         * \brief Set up a auto-collector for this Array.
         * \param parent the parent tree node under which to create the collection
         */
        void enableCollection(TreeNode* parent)
        {
            // Create the collector instance of the appropriate type.
            sparta_assert(parent != nullptr);
            collector_.
                reset(new collection::IterableCollector<FullArrayType,
                                                        SchedulingPhase::Collection, true>
                      (parent, name_, this, capacity()));

            if(ArrayT == ArrayType::AGED) {
                age_collector_.reset(new collection::IterableCollector<AgedArrayCollectorProxy>
                                     (parent, name_ + "_age_ordered",
                                      &aged_array_col_, capacity()));
            }
        }

    private:

        class AgedArrayCollectorProxy
        {
        public:
            //! Typedef for size_type
            typedef uint32_t size_type;

            AgedArrayCollectorProxy(FullArrayType * array) : array_(array) { }

            typedef FullArrayType::iterator iterator;

            FullArrayType::iterator begin() const {
                return array_->abegin();
            }

            FullArrayType::iterator end() const {
                return array_->aend();
            }

            uint32_t size() const {
                return array_->size();
            }

        private:
            FullArrayType * array_ = nullptr;
        };

        /**
         * \brief Provide access to our aged_list_ internals. This is useful for the
         * ArrayCollector to have access too.
         */
        const AgedList & getInternalAgedList_() const
        {
            sparta_assert(ArrayT == ArrayType::AGED);
            return aged_list_;
        }

        template<typename U>
        void writeImpl_(const uint32_t idx, U&& dat)
        {
            sparta_assert(idx < num_entries_,
                        "Cannot write to an index outside the bounds of the array.");
            sparta_assert(valid_index_set_.find(idx) == valid_index_set_.end(),
                        "It is illegal write over an already valid index.");

            // Since we are not timed. Write the data and validate it,
            // then do pipeline collection.
            new (array_.get() + idx) ArrayPosition(std::forward<U>(dat));
            valid_index_set_.insert(idx);

            // Timestamp the entry in the array, for fast age comparison between two indexes.
            array_[idx].age_id = next_age_id_;
            ++next_age_id_;

            array_[idx].valid = true;
            ++num_valid_;

            // Maintain our age order if we are an aged array.
            if(ArrayT == ArrayType::AGED)
            {
                // To maintain aged items, add the index to the front
                // of a list.
                aged_list_.push_front(idx);
                array_[idx].list_pointer = aged_list_.begin();
            }

            // Update occupancy counter.
            if(utilization_)
            {
                utilization_->setValue(num_valid_);
            }
        }

        const std::string name_;

        struct DeleteToFree_{
            void operator()(void * x){
                free(x);
            }
        };

        // The size of our array.
        const size_type num_entries_;
        const uint32_t invalid_entry_{num_entries_ + 1};

        // The number of valid positions after last update.
        size_type num_valid_;

        // A vector that holds all of our data, contains valid and
        // invalid data.
        std::unique_ptr<ArrayPosition[], DeleteToFree_> array_ = nullptr;

        // Set to hold valid indexes at a time
        std::unordered_set<uint32_t> valid_index_set_ {};

        // The aged list.
        AgedList aged_list_;
        AgedArrayCollectorProxy aged_array_col_{this};

        // A counter used to assign a unique age id to every newly
        // written valid entry to the array for fast age comparisons
        // between indexes.
        uint64_t next_age_id_;

        //////////////////////////////////////////////////////////////////////
        // Counters
        std::unique_ptr<sparta::CycleHistogramStandalone> utilization_;

        ////////////////////////////////////////////////////////////
        // Collectors
        std::unique_ptr<collection::IterableCollector<FullArrayType,
                                                      SchedulingPhase::Collection,
                                                      true>> collector_;
        std::unique_ptr<collection::IterableCollector<AgedArrayCollectorProxy> > age_collector_;
    };

    template<class DataT, ArrayType ArrayT>
    Array<DataT, ArrayT>::Array(const std::string& name,
                                uint32_t num_entries,
                                const Clock* clk,
                                StatisticSet* statset,
                                InstrumentationNode::visibility_t stat_vis_general,
                                InstrumentationNode::visibility_t stat_vis_detailed,
                                InstrumentationNode::visibility_t stat_vis_max,
                                InstrumentationNode::visibility_t stat_vis_avg) :
        name_(name),
        num_entries_(num_entries),
        num_valid_(0),
        next_age_id_(0)
    {
        // Set up some vector's of a default size
        // to work as the underlying implementation structures of our array.
        array_.reset(static_cast<ArrayPosition *>(malloc(sizeof(ArrayPosition) * num_entries_)));

        if(statset)
        {
            utilization_.reset(new CycleHistogramStandalone(statset, clk,
                                                            name + "_utilization",
                                                            name + " occupancy histogram",
                                                            0, num_entries_, 1, 0,
                                                            stat_vis_general,
                                                            stat_vis_detailed,
                                                            stat_vis_max,
                                                            stat_vis_avg));
        }
    }

}
