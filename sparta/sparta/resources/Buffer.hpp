// <Buffer.h> -*- C++ -*-


/**
 * \file   Buffer.hpp
 * \brief  Defines the Buffer class used for buffering data
 *
 */

#pragma once

#include <cinttypes>
#include <vector>
#include <algorithm>
#include <type_traits>

#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/MetaStructs.hpp"
#include "sparta/statistics/CycleHistogram.hpp"
#include "sparta/statistics/StatisticInstance.hpp"
#include "sparta/statistics/StatisticDef.hpp"
#include "sparta/collection/IterableCollector.hpp"
#include "sparta/statistics/Counter.hpp"
#include "sparta/utils/IteratorTraits.hpp"

namespace sparta
{

    /**
     * \class Buffer
     * \brief A data structure allowing appending at the end, beginning, or middle, but
     * erase anywhere with collapse
     *
     * The Buffer allows a user to append data to the end, beginning,
     * or middle of the buffer, and erase anywhere.  The Buffer will
     * collapse on empty entries unlike the sparta::Array.
     *
     * The Buffer acks like a standard container via public push_back,
     * insert, and erase methods.  The BufferIterator can be used as
     * an index into the Buffer, and maintains knowledge internally of
     * its location in the Buffer, as well whether or not it still
     * represents a valid entry.
     *
     * \warning Once an entry has been appended with appendEntry
     * method, the index with that data can only be erased via
     * the erase(BufferIterator&), and not the erase(uint32_t).
     *
     *
     * Example:
     * \code
     * Buffer<uint32_t> buffer;
     * Buffer<uint32_t>::iterator entry = buffer.push_back(3);
     * Buffer<uint32_t>::iterator entry2 = buffer.push_back(5);
     * buffer.push_back(1);
     *
     * assert(buffer.read(2) == 1);
     *
     * buffer.erase(1);
     * buffer.erase(entry2); // THROWS an exception since the location that entry2 represented.
     * // was erased.
     *
     * Buffer<uint32_t>::BufferIterator e_copy(entry);
     * buffer.erase(e_copy);
     * // THROWS expection b/c the data represented by e_copy and entry
     * // are invalid after the line above
     * buffer.erase(entry);
     *
     * \endcode
     *
     * \tparam DataT The data type contained in the Buffer
     */
    template <class DataT>
    class Buffer
    {
    public:

        // A typedef for this Buffer's type. This is useful for my
        // subclasses, BufferIterator & EntryValidator
        typedef Buffer<DataT> BufferType;

        // Typedef for the DataT
        typedef DataT value_type;

        // Typedef for size_type
        typedef uint32_t size_type;

    private:

        /**
         * \struct DataPointer
         * \brief A DataPointer is a position in the Buffer's data pool.
         * Each position or DataPointer has a value_type member, an int representing
         * its current physical index if valid, and a pointer to the next invalid/free
         * position in the data pool.
         */
        struct DataPointer {
        private:
            typename std::aligned_storage<sizeof(value_type), alignof(value_type)>::type object_memory_;

        public:
            DataPointer() { }

            DataPointer(DataPointer &&orig) {
                ::memcpy(&object_memory_, &orig.object_memory_, sizeof(object_memory_));
                data = reinterpret_cast<value_type*>(&object_memory_);
            }

            // No copies, only moves
            DataPointer(const DataPointer &) = delete;

            template<typename U>
            void allocate(U && dat) {
                data = new (&object_memory_) value_type(std::forward<U>(dat));
            }

            value_type * data         = nullptr;
            DataPointer* next_free    = nullptr;
            uint32_t     physical_idx = 0; /*!< What index does this data currently reside */
        };
        //Forward Declaration
        struct DataPointerValidator;

        /**
         * \class BufferIterator
         * \brief A struct that represents an entry in the Buffer.
         * The struct can be queried at any time for the accurate index
         * of the item in the Buffer.
         *
         * The BufferIterator will throw an exception when accessed if
         * the entry represented by this BufferIterator in the Buffer
         * is no longer valid.
         *
         * BufferIterator also responds to comparison operators. The
         * entry's locations in the Buffer will be compared.
         *
         */
        template <bool is_const_iterator = true>
        class BufferIterator : public utils::IteratorTraits<std::bidirectional_iterator_tag, value_type>
        {
        private:
            friend class Buffer<value_type>;
            typedef typename std::conditional<is_const_iterator,
                                              const value_type &, value_type &>::type DataReferenceType;
            typedef typename std::conditional<is_const_iterator,
                                              const BufferType *, BufferType *>::type BufferPointerType;
            typedef typename std::conditional<is_const_iterator,
                                              const DataPointer *, DataPointer *>::type DataPointerType;
            /**
             * \brief Get the accurate index of this iterators position in the Buffer.
             * \return the accurate index of the entry in the buffer.
             *
             * This aides in lexicographical comparisons
             */
            uint32_t getIndex_() const {
                if(buffer_entry_ == nullptr) {
                    return attached_buffer_->capacity();
                }
                return buffer_entry_->physical_idx;
            }

            //! a pointer to the Buffer which created this entry
            BufferPointerType attached_buffer_ = nullptr;

            //! The DataPointer
            DataPointerType   buffer_entry_ = nullptr;

            /**
             * \brief construct.
             * \param buffer a pointer to the underlaying buffer.
             * \param index The index this iterator points to in the Buffer
             */
            BufferIterator(BufferPointerType buffer, DataPointerType entry) :
                attached_buffer_(buffer),
                buffer_entry_(entry)
            {}


        public:

            /**
             * \brief Deleted default constructor
             */
            BufferIterator() = default;

            /**
             * \brief a copy constructor that allows for implicit conversion from a
             * regular iterator to a const_iterator.
             */
            BufferIterator(const BufferIterator<false> & iter) :
                attached_buffer_(iter.attached_buffer_),
                buffer_entry_(iter.buffer_entry_)
            {}

            /**
             * \brief a copy constructor that allows for implicit conversion from a
             * regular iterator to a const_iterator.
             */
            BufferIterator(const BufferIterator<true> & iter) :
                attached_buffer_(iter.attached_buffer_),
                buffer_entry_(iter.buffer_entry_)
            {}

            /**
             * \brief Assignment operator
             */
            BufferIterator& operator=(const BufferIterator&) = default;

            /// override the comparison operator.
            bool operator<(const BufferIterator& rhs) const
            {
                sparta_assert(attached_buffer_ == rhs.attached_buffer_, "Cannot compare BufferIterators created by different buffers.");
                return getIndex_() < rhs.getIndex_();
            }

            /// override the comparison operator.
            bool operator>(const BufferIterator& rhs) const
            {
                sparta_assert(attached_buffer_ == rhs.attached_buffer_, "Cannot compare BufferIterators created by different buffers.");
                return getIndex_() > rhs.getIndex_();
            }

            /// override the comparison operator.
            bool operator==(const BufferIterator& rhs) const
            {
                sparta_assert(attached_buffer_ == rhs.attached_buffer_, "Cannot compare BufferIterators created by different buffers.");
                return (buffer_entry_ == rhs.buffer_entry_);
            }

            /// override the not equal operator.
            bool operator!=(const BufferIterator& rhs) const
            {
                sparta_assert(attached_buffer_ == rhs.attached_buffer_, "Cannot compare BufferIterators created by different buffers.");
                return !operator==(rhs);
            }

            /// Checks validity of the iterator
            /// \return Returns false if the iterator is not valid
            bool isValid() const
            {
                if(buffer_entry_ != nullptr) {
                    return attached_buffer_->validator_->isValid(buffer_entry_);
                }
                return false;
            }

            /// override the dereferencing operator
            DataReferenceType operator* () const {
                sparta_assert(attached_buffer_, "The iterator is not attached to a buffer. Was it initialized?");
                sparta_assert(isValid(), "Iterator is not valid for dereferencing");
                return *(buffer_entry_->data);
            }

            //! Overload the class-member-access operator.
            value_type * operator -> () {
                sparta_assert(attached_buffer_, "The iterator is not attached to a buffer. Was it initialized?");
                sparta_assert(isValid(), "Iterator is not valid for dereferencing");
                return buffer_entry_->data;
            }

            value_type const * operator -> () const {
                sparta_assert(attached_buffer_, "The iterator is not attached to a buffer. Was it initialized?");
                sparta_assert(isValid(), "Iterator is not valid for dereferencing");
                return buffer_entry_->data;
            }

            /** brief Move the iterator forward to point to next element in queue ; PREFIX
             */
            BufferIterator & operator++() {
                sparta_assert(attached_buffer_, "The iterator is not attached to a buffer. Was it initialized?");
                if(isValid()) {
                    uint32_t idx = buffer_entry_->physical_idx;
                    ++idx;
                    if(attached_buffer_->isValid(idx)) {
                        buffer_entry_ = attached_buffer_->buffer_map_[idx];
                    }
                    else {
                        buffer_entry_ = nullptr;
                    }
                } else {
                    sparta_assert(attached_buffer_->numFree() > 0, "Incrementing the iterator to entry that is not valid");
                }
                return *this;
            }

            /// Move the iterator forward to point to next element in queue ; POSTFIX
            BufferIterator operator++ (int) {
                BufferIterator buf_iter(*this);
                operator++();
                return buf_iter;
            }

            /// Move the iterator to point to prev element in queue ; POSTFIX
            BufferIterator & operator-- ()
            {
                sparta_assert(attached_buffer_, "The iterator is not attached to a buffer. Was it initialized?");
                if(isValid()) {
                    uint32_t idx = buffer_entry_->physical_idx;
                    --idx;
                    if(attached_buffer_->isValid(idx)) {
                        buffer_entry_ = attached_buffer_->buffer_map_[idx];
                    }
                    else {
                        sparta_assert(idx < attached_buffer_->capacity(), "Decrementing the iterator results in buffer underrun");
                        buffer_entry_ = nullptr;
                    }
                }
                else if (attached_buffer_->size()) {
                    buffer_entry_ = attached_buffer_->buffer_map_[attached_buffer_->size()-1];
                }
                return *this;
            }

            /// Move the iterator to point to prev element in queue ; POSTFIX
            BufferIterator  operator-- (int) {
                BufferIterator buf_iter(*this);
                operator--();
                return buf_iter;
            }

            /**
             * Make BufferIterator<true> a friend class of BufferIterator<false>
             * So const iterator can access non-const iterators private members
             */
            friend class BufferIterator<true>;
        };

    public:

        /// Typedef for regular iterator
        typedef BufferIterator<false>                      iterator;

        /// Typedef for constant iterator
        typedef BufferIterator<true>                       const_iterator;

        /// Typedef for regular reverse iterator
        typedef std::reverse_iterator<const_iterator>      const_reverse_iterator;

        /// Typedef for constant reverse iterator
        typedef std::reverse_iterator<iterator>            reverse_iterator;

        /**
         * \brief Construct a buffer
         * \param name The name of the buffer
         * \param num_entries The number of entries this buffer can hold
         * \param clk The clock this Buffer is associated; used for internal counters
         *
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
        Buffer(const std::string & name,
               const uint32_t num_entries,
               const Clock * clk,
               StatisticSet * statset = nullptr,
               InstrumentationNode::visibility_t stat_vis_general = InstrumentationNode::AUTO_VISIBILITY,
               InstrumentationNode::visibility_t stat_vis_detailed = InstrumentationNode::VIS_HIDDEN,
               InstrumentationNode::visibility_t stat_vis_max = InstrumentationNode::AUTO_VISIBILITY,
               InstrumentationNode::visibility_t stat_vis_avg = InstrumentationNode::AUTO_VISIBILITY);

        /// No copies allowed for Buffer
        Buffer(const Buffer<value_type> & ) = delete;

        /// No copies allowed for Buffer
        Buffer &operator=(const Buffer<value_type> &) = delete;

        //! Move Constructor to allow moves
        Buffer(Buffer<value_type> &&);

        //! Clear (and destruct the Buffer's contents)
        ~Buffer() { clear(); }

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
            return idx < size();
        }

        /**
         * \brief Read and return the data at the given index, const reference
         * \param idx The index to read
         * \return The data to return at the given index (const reference)
         */
        const value_type & read(uint32_t idx) const {
            sparta_assert(isValid(idx));
            return *(buffer_map_[idx]->data);
        }

        /**
         * \brief read the entry at the BufferIterator's location
         * \param entry the BufferIterator to read from.
         */
        const value_type & read(const const_iterator & entry) const
        {
            return read(entry.getIndex_());
        }

        /**
         * \brief read the entry at the BufferIterator's location
         * \param entry the BufferIterator to read from.
         */
        const value_type & read(const const_reverse_iterator & entry) const
        {
            return read(entry.base().getIndex_());
        }

        /**
         * \brief Read and return the data at the given index, reference
         * \param idx The index to read
         * \return The data to return at the given index (reference)
         * logarithmic time complexity on average
         */
        value_type & access(uint32_t idx) {
            sparta_assert(isValid(idx));
            return *(buffer_map_[idx]->data);
        }

        /**
         * \brief Read and return the data at the given BufferIterator's location, reference
         * \param entry the BufferIterator to read from.
         */
        value_type & access(const const_iterator & entry) {
            return access(entry.getIndex_());
        }

        /**
         * \brief Read and return the data at the given BufferIterator's location, reference
         * \param entry the BufferIterator to read from.
         */
        value_type & access(const const_reverse_iterator & entry) {
            return access(entry.base().getIndex_());
        }

        /**
         * \brief Read and return the data at the bottom of the Buffer
         * \return The data to return at the given index (reference)
         * logarithmic time complexity on average
         */
        value_type & accessBack() {
            sparta_assert(isValid(num_valid_ - 1));
            return *(buffer_map_[num_valid_ - 1]->data);
        }

        /**
         * \brief Return the fixed size of this buffer
         * \return The size of this buffer
         */
        size_type capacity() const {
            return num_entries_;
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
         * \brief Append data to the end of Buffer, and return a BufferIterator
         * \param dat Data to be pushed back into the buffer
         * \return a BufferIterator created to represent the object appended.
         *
         * Append data to the end of Buffer, and return a BufferIterator
         * for the location appeneded. Untimed Buffers will have the
         * data become valid immediately.
         */
        iterator push_back(const value_type& dat)
        {
            return push_backImpl_(dat);
        }

        /**
         * \brief Append data to the end of Buffer, and return a BufferIterator
         * \param dat Data to be pushed back into the buffer
         * \return a BufferIterator created to represent the object appended.
         *
         * Append data to the end of Buffer, and return a BufferIterator
         * for the location appeneded. Untimed Buffers will have the
         * data become valid immediately.
         */
        iterator push_back(value_type&& dat)
        {
            return push_backImpl_(std::move(dat));
        }

        /**
         * \brief Insert the item BEFORE the given index.
         * \param idx The index to insert the new item before
         * \param dat The data to insert
         * \return A BufferIterator for the new location.
         *
         * As an example, if the buffer contains:
         * \code
         * [a, b, c]
         * \endcode
         *
         * an insert(1, w) becomes
         *
         * \code
         * [a, w, b, c]
         * \endcode
         *
         * */
        iterator insert(uint32_t idx, const value_type& dat)
        {
            return insertImpl_(idx, dat);
        }

        /**
         * \brief Insert the item BEFORE the given index.
         * \param idx The index to insert the new item before
         * \param dat The data to insert
         * \return A BufferIterator for the new location.
         *
         * As an example, if the buffer contains:
         * \code
         * [a, b, c]
         * \endcode
         *
         * an insert(1, w) becomes
         *
         * \code
         * [a, w, b, c]
         * \endcode
         *
         * */
        iterator insert(uint32_t idx, value_type&& dat)
        {
            return insertImpl_(idx, std::move(dat));
        }

        //! Do an insert before a BufferIterator see insert method above
        iterator insert(const const_iterator & entry, const value_type& dat)
        {
            return insert(entry.getIndex_(), dat);
        }

        //! Do an insert before a BufferIterator see insert method above
        iterator insert(const const_iterator & entry, value_type&& dat)
        {
            return insert(entry.getIndex_(), std::move(dat));
        }

        //! Do an insert before a BufferIterator see insert method above
        iterator insert(const const_reverse_iterator & entry, const value_type& dat)
        {
            return insert(entry.base().getIndex_(), dat);
        }

        //! Do an insert before a BufferIterator see insert method above
        iterator insert(const const_reverse_iterator & entry, value_type&& dat)
        {
            return insert(entry.base().getIndex_(), std::move(dat));
        }

        /**
         * \brief erase a position in the Buffer immediately.
         * \param idx the index to be erased.
         *
         * In a un-TimedBuffer, invalidations immediately changes the
         * indexes in the buffer using this function.  Therefore, it's
         * recommended that erases are performed using a
         * BufferIterator.
         *
         * \warning This method will throw an exception if the index
         *          can be represented by an existing BufferIterator. If
         *          a BufferIterator has been created, the
         *          erase(BufferIterator&) should be used.
         */
        void erase(const uint32_t& idx)
        {
            // Make sure we are invalidating an already valid object.
            sparta_assert(idx < size(), "Cannot erase an index that is not already valid");

            // Do the invalidation immediately
            // 1. Move the free space pointer to the erased position.
            // 2. Call the DataT's destructor
            // 3. Set the current free space pointer's next to the old free position
            DataPointer* oldFree = free_position_;
            free_position_ = buffer_map_[idx];
            free_position_->data->~value_type();
            free_position_->next_free = oldFree;

            // Mark DataPointer as invalid
            validator_->detachDataPointer(free_position_);

            // Shift all the positions above the invalidation in the map one space down.
            uint32_t i = idx;
            sparta_assert(num_valid_ > 0);
            const uint32_t top_idx_of_buffer = num_valid_ - 1;
            while(i < top_idx_of_buffer)
            {
                // assert that we are not going to do an invalid read.
                sparta_assert(i + 1 < num_entries_);
                buffer_map_[i] = buffer_map_[i + 1];
                buffer_map_[i]->physical_idx = i;

                // Shift the indexes in the address map.
                address_map_[i] = address_map_[i + 1];
                ++i;
            }

            // the entry at the old num_valid_ in the map now points to nullptr
            buffer_map_[top_idx_of_buffer] = nullptr;

            // Remove this entry of the address map as it becomes a free position.
            address_map_.erase(top_idx_of_buffer);

            // update counts.
            --num_valid_;
            updateUtilizationCounters_();
        }

        /**
         * \brief erase the index at which the entry exists in the Buffer.
         * \param entry a reference to the entry to be erased.
         */
        void erase(const const_iterator& entry)
        {
            sparta_assert(entry.attached_buffer_ == this, "Cannot erase an entry created by another Buffer");
            // erase the index in the actual buffer.
            erase(entry.getIndex_());
        }

        /**
         * \brief erase the index at which the entry exists in the Buffer.
         * \param entry a reference to the entry to be erased.
         */
        void erase(const const_reverse_iterator& entry)
        {
            sparta_assert(entry.base().attached_buffer_ == this, "Cannot erase an entry created by another Buffer");
            // erase the index in the actual buffer.
            erase(entry.base().getIndex_());
        }

        /**
         * \brief Empty the contents of the Buffer
         *
         */
        void clear()
        {
            num_valid_ = 0;
            std::for_each(buffer_map_.begin(), buffer_map_.end(),
                          [] (auto map_entry)
                          {
                              if(map_entry) {
                                  map_entry->data->~value_type();
                              }
                          });
            std::fill(buffer_map_.begin(), buffer_map_.end(), nullptr);
            for(uint32_t i = 0; i < data_pool_size_ - 1; ++i) {
                data_pool_[i].next_free = &data_pool_[i + 1];
            }
            data_pool_[data_pool_size_ - 1].next_free = &data_pool_[data_pool_size_ - 1];
            free_position_ = &data_pool_[0];
            first_position_ = &data_pool_[0];
            validator_->clear();
            address_map_.clear();
            updateUtilizationCounters_();
        }

        /**
         * \brief Query if the buffer is empty
         *
         */
        bool empty() const
        {
            return num_valid_ == 0;
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
            collector_.
                reset(new collection::IterableCollector<Buffer<DataT> >(parent, getName(),
                                                                        this, capacity()));
        }

        /**
         * \brief Get the iterator pointing to the beginning of Buffer
         * \return Iterator pointing to first element in buffer
         */
        iterator begin(){
            if(size()) {
                sparta_assert(buffer_map_[0]);
                return iterator(this, buffer_map_[0]);
            }
            return end();
        }

        /**
         * \brief Returns an iterator referring to past-the-end element in the
         *        Buffer container
         */
        iterator end() { return iterator(this, nullptr);}

        /**
         * \brief Get the const_iterator pointing to the begin of Buffer
         * \return const_iterator pointing to first element in buffer
         */
        const_iterator begin() const {
            if(size()) {
                return const_iterator(this, buffer_map_[0]);
            }
            return end();
        }

        /**
         * \brief Returns a const_iterator referring to past-the-end element in the
         *        Buffer container
         */
        const_iterator end() const { return const_iterator(this, nullptr);}

        /**
         * \brief Get the iterator pointing to the pass-the-end element of Buffer
         * \return Revese iterator pointing to last element in buffer
         */
        reverse_iterator rbegin(){
            return reverse_iterator(end());
        }

        /**
         * \brief Returns an reverse iterator referring to starting element in the
         *        Buffer container
         */
        reverse_iterator rend() { return reverse_iterator(begin()); }

        /**
         * \brief Get the const_reverse_iterator pointing to the pass-the-end of Buffer
         * \return const_reverse_iterator pointing to first element in buffer
         */
        const_reverse_iterator rbegin() const {
            return const_reverse_iterator(end());
        }

        /**
         * \brief Returns a const_reverse_iterator referring to start element in the
         *        Buffer container
         */
        const_reverse_iterator rend() const { return const_reverse_iterator(begin());}

        /**
         * \brief Makes the Buffer grow beyond its capacity.
         *  The buffer grows by adding new entries in its internal vectors.
         *  The number of new entries it adds defaults to
         *  the value 1, each time it resizes itself.
         */
        void makeInfinite(const uint32_t resize_delta = 1) {
            is_infinite_mode_ = true;
            resize_delta_ = resize_delta;
        }

    private:

        typedef std::vector<DataPointer>  DataPool;
        typedef std::vector<DataPointer*> PointerList;

        struct DataPointerValidator
        {
            //! The data_pool_ pointer which points to the actual
            //  data_pool_ of the Buffer class.
            const DataPool * data_pool_;
            std::vector<uint32_t>  validator_; /*!< Keeps track of validity of iterator. */
            size_type getIndex_(const DataPointer * dp)const {
                auto i = (dp - &(*data_pool_)[0]);
                return static_cast<size_type>(i);
            }

            DataPointerValidator(const Buffer &b):
                data_pool_(&b.data_pool_),
                validator_(b.num_entries_, 0)
            {}

            void attachDataPointer(const DataPointer* dp){
                validator_[getIndex_(dp)] = 1;
            }

            bool isValid(const DataPointer * dp) const {
                return bool(validator_[getIndex_(dp)]);
            }

            void detachDataPointer(DataPointer * dp) {
                validator_[getIndex_(dp)] = 0;
            }

            void clear() {
                std::fill(validator_.begin(), validator_.end(), 0);
            }

            /**
             * \brief Resize the validator vector to new size.
             *  Make the internal data_pool_ pointer point to the current
             *  data_pool_ instance of the Buffer class.
             */
            void resizeIteratorValidator(const uint32_t resize_delta,
                                         const DataPool & data_pool) {
                validator_.resize(validator_.capacity() + resize_delta);
                data_pool_ = &data_pool;
            }
        };

        void updateUtilizationCounters_() {
            // Update Counters
            if(utilization_) {
                utilization_->setValue(num_valid_);
            }
        }

        /**
         * \brief Resize the buffer_map_ and data_pool_.
         *  This method is used to resize and repopulate
         *  the Buffer class's internal buffer_map_ and
         *  data_pool_.
         */
        void resizeInternalContainers_() {

            // Assert that the Buffer class is in Infinite-Mode.
            sparta_assert(is_infinite_mode_, "The Buffer class must be in Infinite-Mode in order to resize itself.");

            // We do not resize if there are available slots in buffer.
            if(numFree() != 0) {
                return;
            }

            // Resize the buffer_map_ with the amount provided by user.
            buffer_map_.resize(buffer_map_.capacity() + resize_delta_);

            // The number of entries the buffer can hold is its capacity.
            num_entries_ = buffer_map_.capacity();

            // Resize the data_pool_ to twice the capacity of the buffer_map_.
            data_pool_.resize(num_entries_ * 2);

            // The number of entries the pool can hold is its capacity.
            data_pool_size_ = data_pool_.capacity();


            // Each entry in data_pool_ should have their next free position
            // pointer point to the slot to its right.
            for(uint32_t i = 0; i < data_pool_size_ - 1; ++i) {
                data_pool_[i].next_free = &data_pool_[i + 1];
            }

            // The last entry should point to itself as the next free position.
            data_pool_[data_pool_size_ - 1].next_free = &data_pool_[data_pool_size_ - 1];

            // The first position should point to the first entry.
            first_position_ = &data_pool_[0];

            // The free position should point to the location according to
            // the number of entries in the buffer.
            free_position_ = &data_pool_[num_valid_];

            // Make all the pointers in buffer_map_ point to the appropriate indexes.
            for(uint32_t i = 0; i < num_valid_; ++i) {
                buffer_map_[i] = &data_pool_[address_map_[i]];
            }

            // Resize the validator vector and relink the validator data pool.
            validator_->resizeIteratorValidator(resize_delta_, data_pool_);
        }

        template<typename U>
        iterator push_backImpl_(U&& dat)
        {

            // Check to see if the vectors need to be resized and relinked.
            if(SPARTA_EXPECT_FALSE(is_infinite_mode_)) {
                resizeInternalContainers_();
            }
            sparta_assert(numFree(), "Buffer exhausted");
            sparta_assert(free_position_ != nullptr);
            free_position_->allocate(std::forward<U>(dat));
            free_position_->physical_idx = num_valid_;

            // Create the entry to be returned.
            iterator entry(this, free_position_);

            // Do the append now.  We can do this with different logic
            // that does not require a process.
            buffer_map_[num_valid_] = free_position_;

            // Store the index in the data_pool_ to which current
            // free_position_ points to. We need to relink all these
            // pointers once the data_pool_ resizes.
            address_map_[num_valid_] =
                static_cast<uint32_t>(free_position_ - &data_pool_[0]);
            //Mark this data pointer as valid
            validator_->attachDataPointer(free_position_);
            ++num_valid_;
            free_position_ = free_position_->next_free;
            updateUtilizationCounters_();

            return entry;
        }

        template<typename U>
        iterator insertImpl_(uint32_t idx, U&& dat)
        {
            // Check to see if the vectors need to be resized and relinked.
            if(SPARTA_EXPECT_FALSE(is_infinite_mode_)) {
                resizeInternalContainers_();
            }
            sparta_assert(numFree(), "Buffer '" << getName() << "' exhausted");
            sparta_assert(idx <= num_valid_, "Buffer '" << getName()
                          << "': Cannot insert before a non valid index");
            sparta_assert(free_position_ != nullptr);
            free_position_->allocate(std::forward<U>(dat));
            free_position_->physical_idx = idx;

            //Mark this data pointer as valid
            validator_->attachDataPointer(free_position_);

            // Create the entry to be returned.
            iterator entry(this, free_position_);

            //Shift all the positions above idx in the map one space down.
            uint32_t i = num_valid_;
            while(i > idx)
            {
                //assert that we are not going to do an invalid read.
                buffer_map_[i] = buffer_map_[i - 1];
                buffer_map_[i]->physical_idx = i ;

                // Shift the indexes in the map.
                address_map_[i] = address_map_[i - 1];
                --i;
            }

            buffer_map_[idx] = free_position_;

            // Store the index in the data_pool_ to which current
            // free_position_ points to. We need to relink all these
            // pointers once the data_pool_ resizes.
            address_map_[num_valid_] =
                static_cast<uint32_t>(free_position_ - &data_pool_[0]);
            ++num_valid_;
            free_position_ = free_position_->next_free;
            updateUtilizationCounters_();
            return entry;
        }

        std::string name_;
        const Clock * clk_ = nullptr;
        size_type num_entries_;       /*!< The number of entries this buffer can hold */
        PointerList buffer_map_; /*!< A vector list of pointers to all the items active in the buffer */
        size_type data_pool_size_;    /*!< The number of elements our data_pool_ can hold*/
        DataPool data_pool_; /*!< A vector twice the size of our Buffer size limit that is filled with pointers for our data.*/

        DataPointer*  free_position_  = 0;  /*!< A pointer to a free position in our data_pool_ */
        DataPointer*  first_position_  = 0; /*!< A pointer to a first position in our data_pool_; used for lower bound check */
        size_type     num_valid_      = 0;  /*!< A tally of valid items */
        std::unique_ptr<DataPointerValidator> validator_;    /*!< Checks the validity of DataPointer */

        //////////////////////////////////////////////////////////////////////
        // Counters
        std::unique_ptr<sparta::CycleHistogramStandalone> utilization_;

        //////////////////////////////////////////////////////////////////////
        // Collectors
        std::unique_ptr<collection::IterableCollector<Buffer<value_type> > > collector_;

        //! Flag which tells various methods if infinite_mode is turned on or not.
        //  The behaviour of these methods change accordingly.
        bool is_infinite_mode_ {false};

        //! The amount by which the internal vectors should grow.
        //  The additional amount of entries the vector must allocate when resizing.
        sparta::utils::ValidValue<uint32_t> resize_delta_;

        //! A map which holds indexes from buffer_map_ to indexes in data_pool_.
        std::unordered_map<uint32_t, uint32_t> address_map_;
    };

    ////////////////////////////////////////////////////////////////////////////////
    // Definitions...
    template<class DataT>
    Buffer<DataT>::Buffer(const std::string & name,
                          uint32_t num_entries,
                          const Clock * clk,
                          StatisticSet * statset,
                          InstrumentationNode::visibility_t stat_vis_general,
                          InstrumentationNode::visibility_t stat_vis_detailed,
                          InstrumentationNode::visibility_t stat_vis_max,
                          InstrumentationNode::visibility_t stat_vis_avg) :
        name_(name),
        clk_(clk),
        num_entries_(num_entries),
        data_pool_size_(num_entries* 2)
    {
        if(statset)
        {
            utilization_.reset(new CycleHistogramStandalone(statset, clk_,
                                                            name_ + "_utilization",
                                                            name_ + " occupancy histogram",
                                                            0, num_entries, 1, 0,
                                                            stat_vis_general,
                                                            stat_vis_detailed,
                                                            stat_vis_max,
                                                            stat_vis_avg));
        }

        buffer_map_.resize(num_entries_);
        data_pool_.resize(data_pool_size_);

        // Must set the validator before you clear
        validator_.reset(new DataPointerValidator(*this));
        clear();
    }

    //! Move Constructor to allow moves
    template<typename DataT>
    Buffer<DataT>::Buffer(Buffer<DataT>&& rval) :
        name_(std::move(rval.name_)),
        clk_(rval.clk_),
        num_entries_(rval.num_entries_),
        buffer_map_(std::move(rval.buffer_map_)),
        data_pool_size_(rval.data_pool_size_),
        data_pool_(std::move(rval.data_pool_)),
        free_position_(rval.free_position_),
        first_position_(rval.first_position_),
        num_valid_(rval.num_valid_),
        validator_(new DataPointerValidator(*this)),
        utilization_(std::move(rval.utilization_)),
        collector_(std::move(rval.collector_)),
        is_infinite_mode_(rval.is_infinite_mode_),
        resize_delta_(std::move(rval.resize_delta_)),
        address_map_(std::move(rval.address_map_)){
        rval.clk_ = nullptr;
        rval.num_entries_ = 0;
        rval.data_pool_size_ = 0;
        rval.free_position_ = nullptr;
        rval.first_position_ = nullptr;
        rval.num_valid_ = 0;
        rval.utilization_ = nullptr;
        rval.collector_ = nullptr;
        validator_->validator_ = std::move(rval.validator_->validator_);
        if(collector_) {
            collector_->reattach(this);
        }
    }
}
