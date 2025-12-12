// <ArchData.hpp> -*- C++ -*-

#pragma once

#include <iostream>
#include <ios>
#include <iomanip>
#include <algorithm>
#include <math.h>
#include <list>
#include <cstring>
#include <unordered_map>

#include "sparta/utils/StaticInit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/TieredMap.hpp"
#include "sparta/utils/Utils.hpp"
#include "sparta/utils/ByteOrder.hpp"
#include "sparta/functional/ArchDataSegment.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"


namespace sparta
{
    /*!
     * \brief Contains a set of contiguous line of architectural data which can
     * be referred to by any architected object model.
     * When layout() is called, reserves space for each element in a registered
     * set of ArchDataSegment instances at offsets within Lines within the
     * ArchData.
     *
     * \note Layout can only occur once. New segments cannot be added once
     * layout is completed because the layout must remain constant between
     * each save/restore.
     *
     */
    class ArchData
    {
        ArchData(const ArchData &) = delete;
        ArchData(ArchData &&) = delete;
        ArchData& operator=(const ArchData &) = delete;
        ArchData& operator=(ArchData &&) = delete;

    public:

        /*!
         * \brief Allow initialization of statics through this helper
         */
        friend class SpartaStaticInitializer;

        class Line;

        //! \name Access Types
        //! @{
        ////////////////////////////////////////////////////////////////////////

        typedef ArchDataSegment::offset_type offset_type; //! Represents offsets into this ArchData
        typedef offset_type line_idx_type; //! ArchData line index
        typedef std::vector<ArchDataSegment*> SegmentList; //! List of ArchDataSegment

        typedef std::list<Line*> LineList; //! List of Line pointers.
        typedef TieredMap<line_idx_type, Line*> LineMap;

        /*!
         * \brief Helper map for quick lookup from ArchDataSegment::ident_type
         * to an ArchDataSegment*.
         */
        typedef std::unordered_map<ArchDataSegment::ident_type, ArchDataSegment*> LayoutHelperMap;

        /*!
         * \brief Vector of ArchDataSegment pointers.
         *
         * Used to ensure consistent ordering of segments during layout on
         * different hosts. LayoutHelperMap losts ordering information and
         * is not reliably consistent for different versions of boost or
         * compilers.
         */
        typedef std::vector<ArchDataSegment*> LayoutHelperVector;

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Constants
        //! @{
        ////////////////////////////////////////////////////////////////////////

        static const offset_type DEFAULT_LINE_SIZE = 512; // //! Default line size in bytes of an ArchData line.

        /*!
         * \brief ArchData construction line size maximum in bytes.
         *
         * Must be less than or equal to this value.
         * An ArchData line size of 0 implies infinite line size, which is not
         * constrained to this limit.
         */
        static const offset_type MAX_LINE_SIZE = 0x80000000;

        /*!
         * \brief Default initial fill value for an ArchData when allocated
         */
        static const uint64_t DEFAULT_INITIAL_FILL = 0xcc;

        /*!
         * \brief Number of of bytes from DEFAULT_INITIAL_FILL to use as a default
         */
        static const uint16_t DEFAULT_INITIAL_FILL_SIZE = 1;

        /*!
         * \brief Invalid line index
         */
        static const line_idx_type INVALID_LINE_IDX = std::numeric_limits<line_idx_type>::max();

        ////////////////////////////////////////////////////////////////////////
        //! @}


        /*!
         * \brief Fill a buffer with a fill pattern
         * \param buf Buffer to fil
         * \param size Bytes to write in the buffer
         * \param fill Fill pattern
         * \param fill_val_size Number of bytes in the fill pattern to write to
         * \a buf before repeating
         * \param fill_pattern_offset See fillWith_
         */
        static void fillValue(uint8_t* buf, uint32_t size, uint64_t fill, uint16_t fill_val_size, uint16_t fill_pattern_offset=0) {
            switch(fill_val_size) {
                case 1:
                    memset(buf, fill, size); // Initialze with fill_val_size (pattern offset does not matter)
                    break;
                case 2:
                    fillWith_<uint16_t>(buf, size, static_cast<uint16_t>(fill), fill_pattern_offset);
                    break;
                case 4:
                    fillWith_<uint32_t>(buf, size, static_cast<uint32_t>(fill), fill_pattern_offset);
                    break;
                case 8:
                    fillWith_<uint64_t>(buf, size, static_cast<uint64_t>(fill), fill_pattern_offset);
                    break;
                default:
                    throw SpartaException("Failed to fill ArchData Line with fill value ")
                        << std::hex << fill << " because fill value size was " << std::dec
                        << fill_val_size;
            }
        }

        /*!
         * \brief Line object which composes part of an entire ArchData.
         *
         * Data of line is always allocated when line is constructed
         * unless a pool_ptr is given at construction.
         *
         * Allocated line is dirty by default. An ArchData should not allocate
         * a line being read if the initial value is known.
         */
        class Line
        {
        public:

            static constexpr char QUICK_CHECKPOINT_PREFIX[] = "<L>"; //!< Prefix for Line checkpoint entries.
            static const uint32_t QUICK_CHECKPOINT_OFFSET_SIZE = 7; //!< Size of offset and size entries in quick checkpoint

            /*!
             * \brief Line constructor
             * \param idx Index of this line
             * \param offset Effective address offset of this line from the
             * starting memory range of the owning ArchData.
             * \param size bytes contained in this line. Assumes this is a power
             * of 2 and greather than 0.
             * \param initial Inital value to fill all data in this line with.
             * The data is set regardless of whether memory comes from a pool
             * or is allocated here.
             * \param initial_val_size Size of initial value in bytes
             * \param pool_ptr optional pointer into pool data which contains
             * <size> number of bytes reserved exclusively for this Line.
             * ArchData can allocate one pool for each line and assign each
             * line a position within that pool to improve data locality.
             *
             * A newly-constructed line is always flagged as dirty and all bytes
             * set to <initial>.
             */
            Line(line_idx_type idx,
                 offset_type offset,
                 offset_type size,
                 uint64_t initial,
                 uint32_t initial_val_size,
                 uint8_t* pool_ptr=0) :
                idx_(idx),
                offset_(offset),
                size_(size),
                is_pool_(pool_ptr != 0),
                dirty_(true)
            {
                sparta_assert(size > 0);
                sparta_assert(isPowerOf2(size));

                if(is_pool_){
                    data_ = pool_ptr;
                }else{
                    alloc_data_.reset(new uint8_t[size]);
                    data_ = alloc_data_.get();
                    sparta_assert(data_ != 0);
                }

                fillWithInitial(initial, initial_val_size);
            }

            //! Disallow copies, moves, and assignments
            Line(const Line &) = delete;
            Line(Line&&) = delete;
            Line & operator=(const Line &) = delete;

            void updateFrom(const Line& other)
            {
                sparta_assert(size_ == other.size_);
                memcpy(data_, other.data_, size_);
                dirty_ = true;
            }

            /*!
             * \brief Allows caller to explicitly flag this line as dirty.
             *
             * The only way to clear a dirty flag is to save this ArchData.
             */
            void flagDirty() {
                dirty_ = true;
            }

            /*!
             * \brief Fills the line's data with the initial value
             * \param initial Value to write to this Line's memory
             * \param initial_val_size Bytes to use of \a initial value
             */
            void fillWithInitial(uint64_t initial, uint32_t initial_val_size) {
                ArchData::fillValue(data_, size_, initial, initial_val_size, 0);
            }

            /*!
             * \brief Restore data from input buffer.
             * \param in Input buffer. Must contain bytes equal to the size of
             * the Line. All bytes will be read
             *
             * \post dirty flag cleared for this line
             * \throw SpartaException if \a in could not be read as
             * expected.
             */
            template <typename StorageT>
            void restore(StorageT& in) {
                in.copyLineBytes((char*)data_, size_);
                dirty_ = false;
            }

            /*!
             * \brief Store data to output buffer.
             * \param out Output buffer. Must allow writing a number of bytes
             * equal to the size of this Line. All bytes will be written
             *
             * \post dirty flag cleared for this line
             * \throw SpartaException if \a out could not be written as
             * expected.
             */
            template <typename StorageT>
            void save(StorageT& out) {
                out.writeLineBytes((char*)data_, size_);
                dirty_ = false;
            }

            /*!
             * \brief Index of this line (typically offset/line_size)
             */
            line_idx_type getIdx() const {
                return idx_;
            }

            /*!
             * \brief Offset into the owning ArchData
             */
            offset_type getOffset() const {
                return offset_;
            }

            /*!
             * \brief Size of this line's data including padding. Accessing
             * bytes from this line with an offset greater than or equal to this
             * value is invalid
             */
            offset_type getLayoutSize() const {
                return size_;
            }

            /*!
             * \brief Has this line been modified since the last save or restore.
             * Immediately after construction, this will be true.
             */
            bool isDirty() const {
                return dirty_;
            }

            //! \name Reading and Writing
            //! @{
            ////////////////////////////////////////////////////////////////////////

            /*!
             * \brief Read from this line, reordering bytes based on byte order
             * if required
             * \tparam T type of variable to read (e.g. uint32_t). This dictates
             * access size
             * \tparam BO Byte-order of access when read from memory
             * \param offset Offset in bytes into this line
             * \param idx Index into this line as a multiple of sizeof(T) bytes
             * \return The value read
             * \note offset+((idx+1)*sizeof(T)) must be <= the size of this line
             */
            template <typename T, ByteOrder BO>
            T read(offset_type offset, uint32_t idx=0) const {
                offset_type loc = offset + (idx*sizeof(T));
                sparta_assert(loc + sizeof(T) <= size_,
                              "Read at ArchData::line offset 0x" << std::hex
                              << loc << " with size " << std::dec << sizeof(T) << " B");

                uint8_t* d = data_ + loc;

                T val = *reinterpret_cast<T*>(d);
                return reorder<T,BO>(val);
            }

            /*!
             * \brief Read a number of contiguous bytes from this line
             * \param offset Offset in bytes into this line
             * \param size Size of the read in bytes
             * \param data Buffer to populate with \a size bytes copied from this ArchData
             * \note offset+size must be <= the size of this line
             */
            void read(offset_type offset, offset_type size, uint8_t* data) const {
                sparta_assert(offset + size <= size_,
                              "Read on ArchData::line offset 0x" << std::hex
                              << offset << " with size " << std::dec << size << " B");

                memcpy(data, data_ + offset, size);
            }

            /*!
             * \brief Write to this line, reordering bytes based on byte order
             * if required
             * \tparam T type of variable to write (e.g. uint32_t). This
             * dictates access size
             * \tparam BO Byte-order of access when written to memory
             * \param offset Offset in bytes into this line
             * \param idx Index into this line as a multiple of sizeof(T) bytes
             * \note offset+((idx+1)*sizeof(T)) must be <= the size of this line
             */
            template <typename T, ByteOrder BO>
            void write(offset_type offset, const T& t, uint32_t idx=0) {
                offset_type loc = offset + (idx*sizeof(T));
                sparta_assert(loc + sizeof(T) <= size_,
                              "Write on ArchData::line offset 0x" << std::hex
                              << loc << " with size " << std::dec << sizeof(T) << " B");

                uint8_t* d = data_ + loc;

                dirty_ = true;
                T& val = *reinterpret_cast<T*>(d);
                val = reorder<T,BO>(t);
            }

            /*!
             * \brief Write a number of contiguous bytes to this line
             * \param offset Offset in bytes into this line
             * \param size Size of the write in bytes
             * \param data Buffer containing \a size bytes to copy to this
             * object
             * \note offset+size must be <= the size of this line
             */
            void write(offset_type offset, offset_type size, const uint8_t* data) const {
                sparta_assert(offset + size <= size_,
                              "Read on ArchData::line offset 0x" << std::hex
                              << offset << " with size " << std::dec << size << " B");

                memcpy(data_ + offset, data, size);
                dirty_ = true;
            }

            ////////////////////////////////////////////////////////////////////////
            //! @}

            /*!
             * \brief Gets a pointer to data within this line for direct read
             * access
             * \param offset Offset into this line in bytes
             * \return Pointer to const memory. This method should be used for
             * reading data only since writes are required to flag this dataview
             * as dirty
             */
            const uint8_t* getDataPointer(offset_type offset) const {
                return data_ + offset;
            }

            /*!
             * \brief return the raw data pointer for this line for direct read and
             * write. No error checking is performed. Should be used with care.
             */
            uint8_t* getRawDataPtr(const offset_type offset) { return (data_ + offset); }

        private:

            line_idx_type idx_;  //!< Index of this line
            offset_type offset_; //!< Offset into owning ArchData
            offset_type size_;   //!< Size of this line
            bool is_pool_;       //!< Is this line's data part of a pool? If not, it is owned by this object
            mutable bool dirty_; //!< Is this line dirty. Mutable so that read methods can be const
            uint8_t * data_ = nullptr;   //!< Pointer to either the allocated memory or a pool
            std::unique_ptr<uint8_t[]> alloc_data_;      //!< Data held by this line. Always allocated

        }; // class Line


        //! \name Constuction
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Constructor
         * \param owner_node TreeNode which owns this ArchData. Is
         *                   allowed to be nullptr. This owner must
         *                   exist through the lifetime of this
         *                   ArchData.  This owner is part of a
         *                   diagnostic utility to determine whether
         *                   all relevant ArchDatas can be located by
         *                   checkpointing mechanisms.  See
         *                   sparta::RootTreeNode::checkArchDataAssociations.
         *
         * \param line_size Line size in bytes. All segments referring
         *                  to this ArchData must be aligned to one
         *                  line. segments referring to ranges
         *                  spanning lines will be allowed to
         *                  construct. Must be a power of 2.  A line
         *                  size of 0 indicates that everything is to
         *                  be laid out on one line
         *
         * \param initial Initial value of each byte allocated by the ArchData.
         *                 Bytes beyond \a initial_val_size must be 0
         *
         * \param initial_val_size Number of bytes from Value to use
         *                         repeating fill. This must be a
         *                         power of 2 between 1 and 8
         *                         inclusive.
         *
         * \param can_free_lines Can this ArchData free its lines when
         *                       reset.  This allows Memory objects to
         *                       reclaim memory when reset but may be
         *                       undesired when representing a set of
         *                       registers or counters where Register
         *                       or Counter classes have cached
         *                       pointers into the lines.  Any
         *                       ArchData which will allow objects to
         *                       cache pointers to its lines or lines'
         *                       memory must set this to false unless
         *                       it plans to update those cached
         *                       pointers each time it is reset.
         *
         * \post Adds self to static all_archdatas_-> Therefore ArchData
         *       construction is not thread safe.
         */
        ArchData(TreeNode* owner_node=nullptr,
                 offset_type line_size=DEFAULT_LINE_SIZE,
                 uint64_t initial=DEFAULT_INITIAL_FILL,
                 uint16_t initial_val_size=DEFAULT_INITIAL_FILL_SIZE,
                 bool can_free_lines=true) :
            owner_node_(owner_node),
            line_size_(line_size),
            initial_(initial),
            initial_val_size_(initial_val_size),
            line_lsb_(0),
            line_mask_(0),
            num_lines_laid_out_(0),
            size_(0),
            is_laid_out_(false),
            layout_padding_waste_(0),
            layout_line_waste_(0),
            can_free_lines_(can_free_lines)
        {
            sparta_assert(initial_val_size_ > 0 && initial_val_size_ <= 8 && isPowerOf2(initial_val_size_),
                          "ArchData initial_val_size type must be a power of 2 between 1 and 8 inclusive, is "
                          << initial_val_size_);
            sparta_assert((initial_val_size_ == 8) || (initial >> (uint64_t)(8*initial_val_size_) == 0),
                          "ArchData initial val has nonzero bits above initial_val_size. initial val: "
                          << std::hex << initial_ << " initial_val_size:" << std::dec << initial_val_size_);

            if(line_size >= 1){
                sparta_assert(line_size <= MAX_LINE_SIZE);
                double tmp = log2(line_size);
                if(tmp != floor(tmp)){
                    SpartaException ex("line_size must be a power of 2, is ");
                    ex << line_size;
                    throw ex;
                }
                line_lsb_ = (offset_type)tmp;
                line_mask_ = ~((1 << line_lsb_) - 1);

                sparta_assert((1ul << line_lsb_) == line_size);
            }else{
                line_lsb_ = sizeof(offset_type) * 8;
                line_mask_ = (offset_type)0;
            }

            if(owner_node_){
                owner_node_->associateArchData_(this);
            }

            all_archdatas_->push_back(this);
        }

        virtual ~ArchData() {

            // Deregister self. This exists in case of failed construction
            if(owner_node_){
                owner_node_->disassociateArchData_(this);
            }

            // Remove from all_archdatas_
            auto itr = std::find(all_archdatas_->begin(), all_archdatas_->end(), this);
            sparta_abort(itr != all_archdatas_->end());
            all_archdatas_->erase(itr);

            // Do not delete segments!
            // It should be save for subclasses to free their registered
            // segments in their destructors.

            // Free all lines which are allocated
            for(LineMap::iterator eitr = line_map_.begin(); eitr != line_map_.end(); ++eitr){
                delete *eitr;
            }
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Segments
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief All constructed segments must register themselves through this
         * method to be laid out within the ArchData object.
         * \pre Cannot already be laid out (see isLsizeaidOut).
         * \param seg Segment to add. Must outlive this ArchData.
         *
         * Segment-subset relationships are not checked here - only in layout.
         */
        void registerSegment(ArchDataSegment* seg) {
            if(is_laid_out_){
                throw SpartaException("This ArchData has already been laid out. New segments cannot be registered (segment id=")
                    << seg->getLayoutID() << ')';
            }

            checkDataSize(seg->getLayoutSize()); // Validate size of segment against size of a Line in this ArchData

            for(const ArchDataSegment* ls : seg_list_){
                if(ls == seg){
                    throw SpartaException("Segment @")
                        << std::hex << (void*)seg << " with id=0x" << seg->getLayoutID() << " already exists in ArchData @"
                        << (void*)this << std::dec;
                }
                if(seg->getLayoutID() != ArchDataSegment::INVALID_ID && ls->getLayoutID() == seg->getLayoutID()){
                    throw SpartaException("Segment id=")
                        << std::hex << "0x" << seg->getLayoutID() << " already exists in ArchData @"
                        << (void*)this << std::dec;
                }
            }

            seg_list_.push_back(seg);
        }

        /*!
         * \brief Gets the list of segments within this ArchData
         * \note Only const access to segments is allowed
         */
        const SegmentList getSegments() const { return seg_list_; }

        //! Gets number of segments in this ArchData
        uint32_t getNumSegments() const { return seg_list_.size(); }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Layout and ArchData Lines
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Organizes the Segments into overlapping regions as needed,
         * eventually calling ArchDataSegment::place on each segment with its
         * location inside this ArchData.
         * \pre Layout can not already be laid out
         * \note Layout can (and must) be performed exactly once. The ArchData
         * layout must remain constant for the save/restore mechanism.
         * \note It is entirely acceptable to layout an ArchData with 0 segments.
         * \post Initial values written for each segment
         *
         * Layout can also be performed through layoutRange(...)
         */
        void layout() {

            if(is_laid_out_){
                throw SpartaException("This ArchData has already been laid out");
            }

            LayoutHelperMap helper_map; // Map for lookup by ID
            LayoutHelperMap::const_iterator it;

            for(ArchDataSegment* seg : seg_list_){
                ArchDataSegment::ident_type lid = seg->getLayoutID();
                if(lid != ArchDataSegment::INVALID_ID){
                    // If ID is valid, add to helper map
                    it = helper_map.find(seg->getLayoutID());
                    if(it != helper_map.end()){
                        throw SpartaException("Found duplicate Segment id=")
                            << it->first << " in the same ArchData @" << (void*)this;
                    }

                    helper_map[seg->getLayoutID()] = seg; // Need to resolve IDs to names quickly
                }
            }

            for(ArchDataSegment*& seg : seg_list_){
                // Note: Segments being layed out are always less than the size
                // of a single line. This is enforced in registerSegment
                placeSegment_(seg, helper_map);
            }

            is_laid_out_ = true; // Disallow another layout of this ArchData

            // Write initial values for each segment
            for(ArchDataSegment* ls : seg_list_){
                ls->writeInitial();
            }
        }

        /*!
         * \brief Lays out the archdata to contain a range of addresses without
         * specifying any segments
         * \pre There must be no segments added to this ArchData.
         * \pre Layout can not already be laid out
         * \note Layout can (and must) be performed exactly once. The ArchData
         * layout must remain constant for the save/restore mechanism.
         * \note It is entirely acceptable to layout an ArchData with 0 segments.
         *
         * Layout can also be performed through layout()
         */
        void layoutRange(offset_type size) {

            if(is_laid_out_){
                throw SpartaException("This ArchData has already been laid out");
            }

            if(seg_list_.size() != 0){
                throw SpartaException("This ArchData has ")
                    << seg_list_.size() << " segments so it cannot be layed out using layoutRange";
            }

            size_ = size; // Simply set the size an allow internal structures to use it
            is_laid_out_ = true; // Disallow another layout of this ArchData
        }

        /*!
         * \brief Gets the line associated with this offset, allocating a new
         * line if necessary.
         * \param offset Offset within the line requested (lookup done by
         * getLineIndex)
         * \throw May assert if offset does not fall within this ArchData.
         *
         * If this line is not yet allocated, it will be allocated.
         * Checks whether the offset is valid (within ArchData) using
         * containsAddress.
         *
         * If this ArchData is used as a sparse memory representation,
         * then tryGetLine should be used to detect unrealized lines.
         */
        Line& getLine(offset_type offset) {
            sparta_assert(containsAddress(offset),
                          "Cannot access this ArchData at offset: 0x"
                          << std::hex << offset << " ArchData size= "
                          << size_ << " B.");

            line_idx_type ln_idx = getLineIndex(offset);
            Line* ln;
            //LineMap::iterator lnitr;
            //if((lnitr = line_map_.find(ln_idx)) == line_map_.end()){
            LineMap::pair_t* lnitr;
            if((lnitr = line_map_.find(ln_idx)) == nullptr){
                ln = allocateLine_(ln_idx); // Adds to line
            }else{
                ln = lnitr->second;
                if(ln->getOffset() > offset){
                    ln = line_map_.find(ln_idx)->second;
                }
            }
            return *ln;
        }

        /*!
         * \brief Gets the line associated with this offset only if it already
         * exists
         * \param offset Offset within the line requested (lookup done by
         * getLineIndex)
         * \return The line if it already exists (has been written). If line
         * does not exist, returns nullptr.
         * \throw May assert if offset does not fall within this ArchData.
         *
         * Checks whether the offset is valid (within ArchData) using
         * containsAddress.
         */
        const Line* tryGetLine(offset_type offset) const {
            sparta_assert(containsAddress(offset),
                          "Cannot access this ArchData at offset: 0x"
                          << std::hex << offset << " ArchData size= "
                          << size_ << " B.");

            line_idx_type ln_idx = getLineIndex(offset);
            //LineMap::const_iterator lnitr;
            //if((lnitr = line_map_.find(ln_idx)) == line_map_.end()){
            const LineMap::pair_t* lnitr;
            if((lnitr = line_map_.find(ln_idx)) == nullptr){
                return nullptr;
            }
            return lnitr->second;
        }

        //! \name Line Queries
        //! @{
        ////////////////////////////////////////////////////////////////////////

        //! Only const access is allowed to internal map
        const LineMap& getLineMap() const {
            if(false == is_laid_out_){
                throw SpartaException("Cannot get ArchData lines map until layout completes");
            }
            return line_map_;
        }

        /*!
         * \brief Deletes (if possible) data held within the ArchData, restoring
         * it to a 'clean-state'. This is not a 're-initialize' state, but
         * rather all data is cleared to the fill-value specified at ArchData
         * construction. Depending in whether canFreeLines returns true,
         * Lines may actually be deleted and reclaimed.
         * \pre ArchData must be laid out
         * \see reset
         * \note Does not change layout
         *
         * This method exists mainly for checkpointer usage and is faster than
         * reset. To reinitialize contained data to layout-time values, use
         * reset() instead.
         */
        void clean() {
            if(false == is_laid_out_){
                throw SpartaException("Cannot clear ArchData until layout completes");
            }

            if(canFreeLines()){
                // Delete all lines allocated first (map contains pointers to lines)
                for(LineMap::iterator itr = line_map_.begin(); itr != line_map_.end(); ++itr){
                    delete *itr;
                }

                // Delete all structures within the map
                line_map_.clear();
            }else{
                // Overwrite all lines with initial bytes
                for(LineMap::iterator itr = line_map_.begin(); itr != line_map_.end(); ++itr){
                    (*itr)->fillWithInitial(initial_, initial_val_size_);
                }

            }
        }

        /*!
         * \brief Cleans the ArchData (see clean) then applies all initial
         * values through ArchDataSegment::writeInitial().
         * \pre ArchData must be laid out
         * \see clean
         * \note Does not change layout
         */
        void reset() {
            clean(); // Checks for is_laid_out_

            // Write initial values for each segment
            for(ArchDataSegment* ls : seg_list_){
                ls->writeInitial();
            }
        }

        //! Gets the size of a line within this ArchData instance
        offset_type getLineSize() const {
            return line_size_;
        }

        /*!
         * \brief Gets the number of lines with allocated data;
         * \return Number of allocated lines
         */
        line_idx_type getNumAllocatedLines() const {
            return line_map_.size();
        }

        /*!
         * \brief Gets Index of a line containing the specified offset
         *
         * Does not require that offset be part of an actual line. This is just
         * a numeric computation.
         */
        line_idx_type getLineIndex(offset_type offset) const {
            return offset >> line_lsb_;
        }

        /*!
         * \brief Gets offset associated with a particular line that exists in
         * this ArchData.
         * \param idx Index of line whose offset should be returned
         *
         * This method can safely be called before layout is complete.
         * Does not require that idx be associated with an actual line. This is just a
         * numeric computation.
         */
        offset_type getLineOffset(line_idx_type idx) const {
            return line_size_ * idx;
        }

        /*!
         * \brief Indicates whether this ArchData can free its lines after
         * allocating them. Otherwise, any allocated line must exist for the
         * lifetime of this ArchData.
         *
         * Generally, Registers and Counters will not allow freeing lines since
         * they cache pointers to lines. Memory objects (without simple DMA
         * support) will allow this.
         */
        bool canFreeLines() const { return can_free_lines_; }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Access and layout validation
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Determines if an access of size 'bytes' can be performed at
         * the given offset based only on the size of the ArchData and
         * Line span. Validity checking matches that of ArchData::Line::read
         * \param offset Offset of access to check within this archdata (not
         * within a line)
         * \param bytes Number of bytes in access.
         * \return True if access fits within a single Line within the ArchData.
         * \throw SpartaException if access spans a line, is outside of the
         * ArchData, or is an illegal size. This works regardless of isLaidOut
         *
         * This test exists for sparse usage where a 'fake' read might be
         * performed and arguments must be validated without an ArchData::Line
         * instance
         */
        void checkCanAccess(offset_type offset, offset_type bytes) const {
            checkInSingleLine(offset, bytes);
            sparta_assert(offset + bytes <= size_,
                          "Generic access validity test on ArchData::line offset 0x" << std::hex
                          << offset << " with size " << std::dec << bytes << " B");
        }

        /*!
         * \brief Determines if this ArchData contains a byte for the specified
         * address
         * \param offset Offset into this ArchData
         * \return true if this offset is contained within this ArchData.
         *
         * This is a trivial bounds check to see if offset < getSize().
         * This method can be called at any time but is only meaningful once
         * ArchData is laid out (see isLaidOut)
         */
        bool containsAddress(offset_type offset) const noexcept {
            return offset < size_;
        }

        /*!
         * \brief Checks to see that the size of the data is a valid access
         * size.
         * \param size Size of access to check
         * \throw SpartaException if size is invalid (i.e. exceeds some ceiling, is
         * zero, or is not a power of 2)
         */
        void checkDataSize(offset_type size) const {
            static_assert(std::is_unsigned<offset_type>::value == true);
            if(size == 0){
                throw SpartaException("Segment size (")
                    << size << ") must be larger than 0 and less than line size ("
                    << line_size_ << ")";
            }
            if(line_size_ != 0 && size > line_size_){
                throw SpartaException("Segment size (")
                    << size << ") exceeds that of an ArchData line (" << line_size_ << ")";
            }
        }

        /*!
         * \brief Checks that a segment is valid within this archdata by its
         * given offset and size.
         * \param offset Offset of start of segment within this ArchData (not a
         * line)
         * \param size Size of segment to check
         * \throw SpartaException if the access is invalid (i.e. size is invalid
         * according to checkDataSize or endpoints of the segment spans a line).
         */
        void checkSegment(offset_type offset, offset_type size) const {

            checkDataSize(size);

            checkInSingleLine(offset, size);

            if(__builtin_expect((offset + size) > size_, 0)){
                throw SpartaException("Segment end (0x")
                    << std::hex << offset+size << ") extends pased end of ArchData (0x"
                    << size_ << ") by " << std::dec << (offset + size) - size_ << " B";
            }
        }

        /*!
         * \brief Determines whether the access defined by \a offset and \a size
         * spans multiple ArchData Lines
         * \param offset Offset of access into this ArchData
         * \param size Size of access in bytes
         * \throw SpartaException if access endpoints are in differnet Lines
         */
        void checkInSingleLine(offset_type offset, offset_type size) const {
            if(__builtin_expect((offset + size) - (offset & line_mask_) > line_size_, 0)){
                throw SpartaException("Segment spans multiple ArchData lines: from ")
                    << getLineIndex(offset) << " to " << getLineIndex(offset+size-1);
            }
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Selective Copy
        //! @{
        ////////////////////////////////////////////////////////////////////////

        virtual void updateFrom(const ArchData& other)
        {
            // Iterate through the other's line map...
            for (LineMap::const_iterator itr = other.line_map_.begin(); itr != other.line_map_.end(); ++itr) {
                const Line* other_ln = *itr;
                if (other_ln != nullptr) {
                    // Attempt to find a corresponding line in this
                    line_idx_type other_idx = other_ln->getIdx();
                    LineMap::pair_t* pln = line_map_.find(other_idx);
                    if (pln != nullptr) {
                        // Found corresponding line, copy over the data
                        pln->second->updateFrom(*other_ln);
                    } else {
                        // Allocate a new line in this
                        Line* ln = allocateLine_(other_idx);
                        ln->updateFrom(*other_ln);
                    }
                }
            }
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Checkpointing
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Writes checkpointing data from this ArchData to a stream
         * \post All lines flagged as not dirty
         */
        template <typename StorageT>
        void save(StorageT& out) {
            // Iterate lines and restore
            sparta_assert(out.good(),
                          "Saving delta checkpoint to bad ostream for " << getOwnerNode()->getLocation());

            for(LineMap::iterator itr = line_map_.begin(); itr != line_map_.end(); ++itr){
                Line* ln = *itr;
                if(ln != nullptr && ln->isDirty()){
                    out.beginLine(ln->getIdx());
                    ln->save(out);
                }
            }
            out.endArchData();
        }

        /*!
         * \brief Writes snapshot checkpointing data (all lines) from this
         * ArchData to a stream regardless of dirtiness.
         * \post All lines flagged as not dirty
         */
        template <typename StorageT>
        void saveAll(StorageT& out) {
            sparta_assert(out.good(),
                          "Saving delta checkpoint to bad ostream for " << getOwnerNode()->getLocation());

            for(LineMap::iterator itr = line_map_.begin(); itr != line_map_.end(); ++itr){
                Line* ln = *itr;
                if(ln != nullptr){
                    out.beginLine(ln->getIdx());
                    ln->save(out);
                }
            }
            out.endArchData();
        }

        /*!
         * \brief Restores a delta checkpoint (not a full snapshot).
         * This contains only additive changes.
         * \post All lines flagged as not dirty
         *
         * To restore full snapshots, use restoreAll
         */
        template <typename StorageT>
        void restore(StorageT& in) {
            // Iterate lines and restore
            sparta_assert(in.good(),
                          "Encountered bad checkpoint data (invalid stream) for " << getOwnerNode()->getLocation());

            while(1){
                line_idx_type ln_idx = in.getNextRestoreLine();
                if(ln_idx == INVALID_LINE_IDX){
                    break; // Done with this ArchData
                }
                Line& ln = getLine(ln_idx * line_size_);
                ln.restore(in);
            }
        }

        /*!
         * \brief Restores a full checkpoint snapshot (not a delta).
         * This removes all lines NOT found in the snapshot data
         * \post All lines flagged as not dirty
         *
         * To restore deltas, use restore instead
         */
        template <typename StorageT>
        void restoreAll(StorageT& in) {
            // Fresh, empty state, ready to be overwritten
            clean();

            restore(in);
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Layout Status and instrumentation
        //!
        //! Also includes layout and content-dump helper functions
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Returns the owner TreeNode
         * \note The result of this method is invariant. An ArchData has the
         * same owner for its entire lifetime.
         */
        const TreeNode* getOwnerNode() const { return owner_node_; }

        /*!
         * \brief Set the owner tree node
         */
        void setOwnerNode(TreeNode *node)
        {
            sparta_assert(owner_node_ == nullptr, "Owner already set");

            owner_node_ = node;
            if (owner_node_) {
                owner_node_->associateArchData_(this);
            }
        }

        /*!
         * \brief Has this ArchData been laid out yet.
         * \return true if layout ias completed.
         */
        bool isLaidOut() const { return is_laid_out_; }

        /*!
         * \brief Gets the current size of the layout for this ArchData
         * \return size used for layout by this ArchData.
         * \pre This method cannot be called until isLaidOut is true
         * \throw SpartaException if not yet laid out.
         */
        offset_type getSize() const {
            if(false == is_laid_out_){
                throw SpartaException("Cannot get layout size until layout completes");
            }
            return size_;
        }

        /*!
         * \brief Gets the value used to initialize unwritten memory
         */
        uint64_t getInitial() const {
            return initial_;
        }

        /*!
         * \brief Gets the size of the initial value
         */
        uint32_t getInitialValSize() const {
            return initial_val_size_;
        }

        //! Number of bytes wasted total during layout for any reason
        uint32_t getTotalWaste() const {
            return layout_padding_waste_ + layout_line_waste_;
        }

        //! Number of bytes wasted during layout because of optimal word-alignment
        uint32_t getPaddingWaste() const {
            return layout_padding_waste_;
        }

        //! Number of bytes wasted during layout because of line-ending alignment
        uint32_t getLineWaste() const {
            return layout_line_waste_;
        }

        //! Comparison functions for sorting by segment
        static bool compareSegmentOffsets(const ArchDataSegment* s1, const ArchDataSegment* s2){
            if(s1->getOffset() < s2->getOffset()){
                return true; // S1 < S2
            }else if(s1->getOffset() > s2->getOffset()){
                return false; // S1 > S2
            }

            if(s1->getLayoutSize() >= s2->getLayoutSize()){
                return true; // S1 larger than or equal in size to S2 so it should be considered before
            }
            return false; // S1 is smaller than S2, so it should be considered after
        }

        /*!
         * \brief Prints content of each ArchData Line in order
         * \param o ostream to which layout will be written.
         */
        void dumpLayout(std::ostream& o) const {
            if(false == is_laid_out_){
                throw SpartaException("Cannot dump ArchData layout until layout completes");
            }

            SegmentList sorted = seg_list_;
            std::sort(sorted.begin(), sorted.end(), compareSegmentOffsets);

            offset_type last_line_off = 0; // offset of last line
            offset_type last_end = 0; // end of last segment (expected start of next)

            dumpLayout_(o, sorted, last_line_off, last_end, true);
        }

        /*!
         * \brief Gets state information for each line in this ArchData.
         * \return Returns a vector of strings containing state information for
         * each line.
         *
         * This value is not mean to be parsed and is subject to change.
         *
         * For a 5 lines (getNumAllocatedLines) with various states:
         * \li d - dirty
         * \li c - clean
         * \li ! - unallocated (for sparse ArchData)
         * The result may be something like this:
         * \verbatim
         * {"    0:d", "    1:c", "    2:!", "    3:!", "    4:c"}
         * \endverbatim
         */
        std::vector<std::string> getLineStates() const {
            std::vector<std::string> result;
            for(LineMap::const_iterator itr = line_map_.begin(); itr != line_map_.end(); ++itr){
                const Line* ln = *itr;
                std::stringstream state;
                state << std::setw(5) << std::hex << ln->getIdx()
                      << ":";
                if(ln != 0){
                    if(ln->isDirty()){
                        state << 'd';
                    }else{
                        state << 'c';
                    }
                }else{
                    state << '!';
                }
                result.push_back(state.str());
            }
            return result;
        }

        uint64_t getNumTiers() const {
            return line_map_.getNumTiers();
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Diagnostics
        //!
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Static method to return a const vector of all ArchVectors that
         * currently exist.
         * \note This vector result will be modified whenever ArchDatas are
         * constructed or destructed
         */
        static const std::vector<const ArchData*> getAllArchDatas() {
            return *all_archdatas_;
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

    private:

        /*!
         * \brief Fill a buffer with the fill data of a templated type
         * \tparam FillT Fill type (indicates size)
         * \param buf Buffer to fill with repating fill pattern
         * \param size Number of bytes to write to buffer
         * \param fill Fill value. Uses N bytes of this value where N is  size
         * of \a FillT
         * \param fill_offset Offset buffer from fill (offset of \a buf within
         * its ArchData::Line or other container). This is necessary to line buf
         * the buffer with the fill pattern. For example, if fill size
         * were 4 and buf was a pointer to referred to 2 bytes into a block,
         * then buf_offset would be 2 % 4 = 2. Byte 0 of buf would be byte 2 of
         * the fill pattern.
         */
        template <typename FillT>
        static void fillWith_(uint8_t* buf, uint32_t size, FillT fill, uint16_t buf_offset=0) {
            sparta_assert(buf != nullptr,
                          "Null buf given");
            sparta_assert(buf_offset < sizeof(FillT),
                          "Cannot have a buf_offset larger than FillT size. Must be buffer offset % fill size");

            // Realign the pattern so it coincides with the misalignment of buf
            const FillT shifted_fill = (fill >> (buf_offset * 8)) | (fill << ((sizeof(FillT) - buf_offset) * 8));

            // Handle case where buf is smaller than fill pattern
            if(sizeof(FillT) >= size) {
                // Line size is 1,2,4 bytes and fill value is same size or
                // smaller. Just copy what will fit
                memcpy(buf, static_cast<const void*>(&shifted_fill), size);
                return;
            }

            uint8_t* ptr = buf;

            // Address of first byte followinf buf which is not written
            const uint8_t* const end = buf + size;
            sparta_assert(end > buf,
                          "buf (" << (void*)buf << ") was too large and adding size (" << size << ") rolled over to 0");

            // Stop point before writing last, partial value
            const uint8_t* const stop = end - sizeof(FillT);

            // Write entire fill pattern up until stop point to prevent overrun
            while(ptr < stop) {
                // Line size is > fill value size. Since line size is power
                // of 2, a line size greater than some power of two integer size is guaranteed
                // to fit a whole number of that integer
                memcpy(static_cast<void*>(ptr), static_cast<const void*>(&shifted_fill), sizeof(FillT));
                ptr += sizeof(FillT);
                sparta_assert(ptr >= buf, "ptr overflowed when adding fill size (" << sizeof(FillT) << ")");
            }

            // Write remainder of buffer size
            int32_t rem = end - ptr;
            sparta_assert(rem <= (int32_t)sizeof(FillT) && rem >= 0,
                          "fillWith_ remainder size was 0x" << std::hex << rem
                          << " ptr=" << (void*)ptr << " end=" << (const void*)end
                          << " sizeof(FillT)=" << sizeof(FillT));
            sparta_assert(ptr != nullptr,
                          "Somehow encountered a null pointer during arithmetic based on a pointer at " << (void*) buf);

            if(rem > 0) {
                // Remaining size is 1,2, or 4 bytes and rem fill value is same size or
                // smaller. Just copy what will fit

                memcpy(static_cast<void*>(ptr), static_cast<const void*>(&shifted_fill), rem);
            }
        }

        /*!
         * \brief Recursive method for duping an sequence of ArchDataSegments.
         * \param o ostream to which layout will be written
         * \param sorted List of some any segments to print with this
         * invokation. Sorted by compareSegmentOffsets.
         * \param last_line_off Offset of start of current line
         * \param last_end Last ending offset (last seg offset + last seg size)
         * \param show_line_nums Show line numbers at start of each line printed
         * to <o>.
         *
         * Used by public dumpLayout method.
         */
        void dumpLayout_(std::ostream& o,
                         SegmentList& sorted,
                         offset_type last_line_off,
                         offset_type last_end,
                         bool show_line_nums) const {

            // Print first line start
            if(show_line_nums){
                o << 'x' << std::hex << std::setw(5) << std::right << (last_end & line_mask_) << ": " << std::dec;
            }else{
                o << "     \": "; // no line num (implies same as above)
            }

            SegmentList::const_iterator it;
            std::vector<ArchDataSegment*> nestings;
            for(it = sorted.begin(); it != sorted.end(); ++it){
                offset_type off = (*it)->getOffset();

                // End line of offset moved to next
                if(last_line_off != (off & line_mask_)){
                    // Print skipped bytes at end of the line
                    dumpSkippedBytes_(o, (off & line_mask_) - last_end, true, true);
                    o << std::endl;

                    // Handle nestings
                    if(nestings.size() > 0){
                        dumpLayout_(o, nestings, last_line_off, last_line_off, false);
                    }
                    nestings.clear();

                    // Print new line start
                    if(show_line_nums){
                        o << 'x' << std::hex << std::setw(5) << std::right << (last_end & line_mask_) << ": " << std::dec;
                    }else{
                        o << "     \": "; // no line num (implies same as above)
                    }

                    last_line_off = (off & line_mask_);
                    last_end = last_line_off;
                }

                if(off < last_end){
                    nestings.push_back(*it);
                    continue; // Do not print, duplicate (nested)
                }else{
                    // Print skipped bytes betweeen last seg and this
                    offset_type jump = off - last_end;
                    dumpSkippedBytes_(o, jump, false, false);
                }

                offset_type size = (*it)->getLayoutSize();
                if(size == 1){
                    o << "|"; // Only 1B
                }else if(size == 2){
                    o << "|2"; // Only 2B
                }else{
                    o << "|";
                    for(offset_type i=1; i<size; ++i){
                        if(i == (offset_type)(size/2)){
                            o << std::left << std::dec;
                            if(size >= 1000){
                                o << std::setw(4) << size; // 4-char size
                            }else if(size >= 100){
                                o << std::setw(3) << size; // 3-char size
                            }else if(size >= 10){
                                o << std::setw(2) << size; // 2-char size
                            }else{
                                o << std::setw(1) << size; // 1-char size
                            }
                        }else{
                            if(size >= 1000 && (i > (size/2)+1 && i < (size/2)+3)){
                                // More than 4 digits printed for size. Skip
                            }else if(size >= 100 && (i > (size/2)+1 && i < (size/2)+2)){
                                // Print nothing. 3-digit size was just printed
                            }else if(size >= 10 && i == (size/2)+1){
                                // Print nothing. 2-digit size was just printed
                            }else{
                                o << '-'; // 1 char per byte.
                            }
                        }
                    } // for( ... size ... )
                } // else // if(size == 1)

                last_end = off + size;
            }

            // Complete final line
            if((last_end & ~line_mask_) == 0){
                o << '|' << std::endl;
            }else{
                // Print skipped bytes at end of the line
                offset_type leftover = line_size_ - (last_end & ~line_mask_);
                dumpSkippedBytes_(o, leftover, true, true);
                o << std::endl;
            }

            // Handle nestings on final line
            if(nestings.size() > 0){
                dumpLayout_(o, nestings, last_line_off, last_line_off, false);
            }
            nestings.clear();
        }

        /*!
         * \brief Dump a number of bytes in a single as spacing for the
         * dumpLayout_ method. Prints characters to represent the bytes or a
         * number of bytes if allowed to condense (e.g. at the end of a row)
         * \param o ostream to which the spacing will be written.
         * \param num Number of ignored bytes to dump
         * \param condense Can the bytes be condesend into a count instead of
         * writing a char for each byte.
         * \param end_row Should a '|' char br written after the bytes to
         * indicate the end of a layout row.
         */
        void dumpSkippedBytes_(std::ostream& o,
                               offset_type num,
                               bool condense,
                               bool end_row) const {
            if(num == 0){
                // Print nothing yet
            }else if(num == 1){
                o << '/';
            }else if(num <= 16 || !condense){
                o << '|';
                for(offset_type i=1; i < num; ++i){
                    //o << "+";
                    o << " ";
                }
            }else{
                //o << "|+++" << std::dec << num << " ";
                o << "|+  " << std::dec << num << " ";
            }

            if(end_row){
                o << '|';
            }
        }

        /*!
         * \brief Allocates a new line and inserts into the lines_ list and
         * line_map_ map at the appropriate location.
         * \param idx Line index (e.g. line number)
         * \throw exception if a line is already allocated at this index
         */
        Line* allocateLine_(line_idx_type idx) {
            if(idx * line_size_ > size_){
                throw SpartaException("Cannot allocate Line at idx ")
                    << idx
                    << " because idx*line_size is 0x"
                    << std::hex << (idx * line_size_)
                    << "and the current ArchData size is only 0x" << size_;
            }

            // This test introduces a bit of overhead and should be removed
#ifndef NDEBUG
            //if(line_map_.find(idx) != line_map_.end()){
            if(line_map_.find(idx) != nullptr){
                throw SpartaException("Line is already allocated at index ") << idx;
            }
#endif // NDEBUG

            // Allocate infinite-length lines
            if(0 == line_size_){
                if(idx != 0){
                    throw SpartaException("Cannot allocate a line at index other than 0 when ArchData line size is 0 (infinite)");
                }
                sparta_assert(line_map_.size() == 0); // Cannot yet have a line

                Line* ln = new Line(0, 0, size_, initial_, initial_val_size_);
                //lines_.push_back(ln);
                line_map_[0] = ln;
                return ln;
            }

            // Allocate finite-length lines
            offset_type ln_off = getLineOffset(idx);

            // Always use the full line size instead of trying to compute the
            // bytes leftover. When a line is being allocated, we may not know
            // the full size.
            Line* ln = new Line(idx, ln_off, line_size_, initial_, initial_val_size_);
            //LineList::iterator lnitr = lines_.begin();
            //for(; lnitr != lines_.end(); ++lnitr){
            //    if((*lnitr)->getIdx() > idx){
            //        break;
            //    }
            //}
            //lines_.insert(lnitr, ln);
            line_map_[idx] = ln;
            return ln;
        }


        /*!
         * \brief Places a segment seg into this ArchData at the next available
         * location that satisfies both ArchData::Line alignment and host
         * word-alignment conditions. If segment is a subset of other segments,
         * the appropriate parent segments are recursively placed first.
         * \param seg Segment to place
         * \param helper_map Map of Segment IDs to Segments for fas lookup by
         * ID.
         * \param depth Current depth of placement [for debugging]
         *
         * Uses sparta::ArchDataSegment::place() to indicate placement to the
         * segment once determined.
         */
        void placeSegment_(ArchDataSegment* seg,
                           LayoutHelperMap& helper_map,
                           uint32_t depth=0) {

            sparta_assert(seg != 0); // Segment should never be NULL

            if(seg->isPlaced()){
                /*
                  for(uint32_t i=0; i<depth; ++i){ std::cout << " "; }
                  std::cout << "seg " << std::dec << seg->getLayoutID() << " is already placed @ 0x"
                  << std::hex << seg->getOffset() << std::dec << std::endl;
                */
                return; // Done
            }


            offset_type placement = 0; // Where to place the segment in the whole ArchData (calculated below)
            const offset_type size = seg->getLayoutSize(); // Size of the segment
            LayoutHelperMap::iterator it;

            if(seg->getSubsetOf() != ArchDataSegment::INVALID_ID){
                // Place within another segment (subset)

                ArchDataSegment::ident_type sub_of = seg->getSubsetOf();

                //for(uint32_t i=0; i<depth; ++i){ std::cout << " "; }
                //std::cout << "seg " << std::dec << seg->getLayoutID() << " is subset @ +0x"
                //          << std::hex << seg->getSubsetOffset() << "; attempting to place base: "
                //          << std::dec << sub_of << std::endl;

                // Find parent
                it = helper_map.find(sub_of);
                if(it == helper_map.end()){
                    throw SpartaException("A Segment with identifier ")
                        << seg->getLayoutID() << " claimed to be a "
                        << "subset of Segment with identifier "
                        << sub_of << ", which does not exist in this ArchData";
                }
                ArchDataSegment* parent_seg = it->second;
                sparta_assert(parent_seg->getLayoutID() == sub_of); // Parent must match expected ID

                // Place parent first
                placeSegment_(it->second, helper_map, depth+1);
                sparta_assert(parent_seg->isPlaced()); // Parent must be placed at this point

                if(seg->getLayoutSize() + seg->getSubsetOffset() > parent_seg->getLayoutSize()){
                    throw SpartaException("Segment id=")
                        << seg->getLayoutID() << " had size 0x" << std::hex << seg->getLayoutSize()
                        << " and subset offset 0x" << std::hex << seg->getSubsetOffset()
                        << " which makes it larger than the parent id=" << std::dec
                        << parent_seg->getLayoutID() << " with size " << std::hex
                        << parent_seg->getLayoutSize() << " of which it is a child";
                }
                placement = parent_seg->getOffset() + seg->getSubsetOffset();
            }else{
                // Place at the root (not subset)

                // Word-alignment is probably important for reinterpret casting
                // Pad to the next word. size_ refers to ArchData size and offset of
                // next data.
                // Note that this only applies to segments placed at the root.
                if(size_ % HOST_INT_SIZE != 0){
                    offset_type delta = HOST_INT_SIZE - (size_ % HOST_INT_SIZE);
                    layout_padding_waste_ += delta;
                    size_ += delta;
                }

                // Ensure there is an ArchData line of appropriate size.
                // NOTE that segments CANNOT span lines.
                offset_type start_line_addr = size_ & line_mask_;
                offset_type end_line_addr = (size_ + size - 1) & line_mask_;
                if(start_line_addr != end_line_addr){
                    // Needs to be moved to the next line
                    sparta_assert(end_line_addr > start_line_addr); // Cannot allow overflow of offset_type. TODO: Test overflow case?

                    offset_type next = (size_ & line_mask_) + line_size_; // start of next line
                    layout_line_waste_ += next - size_; // Waste some bytes

                    // std::cout << "Exceeds lines at offset 0x" << std::hex << size_ << std::dec << " + " << size << " B" << std::endl;

                    size_ = next;
                    //lines_.push_back(0); // NULL pointer
                    ++num_lines_laid_out_;
                }else if(start_line_addr >= num_lines_laid_out_ * line_size_){//(lines_.size() * line_size_)){
                    // Starts naturally on a new line
                    sparta_assert((size_ & ~line_mask_) == 0); // Must start at beginning of line

                    // std::cout << "Added next line at offset 0x" << std::hex << size_ << std::dec << std::endl;

                    //lines_.push_back(0); // NULL pointer
                    ++num_lines_laid_out_;
                }

                placement = size_;
                size_ += size; // Increase size so that the segment can be placed.
                sparta_assert((placement % HOST_INT_SIZE) == 0); // Just to be sure
            }

            seg->place(placement);

            //! \todo pack larger values first and them smaller ones for space efficiency
        }

    private:

        /*!
         * \brief TreeNode which owns this ArchData. Invariant for the lifetime
         * of this object
         */
        TreeNode *owner_node_ = nullptr;

        /*!
         * \brief Size of a single ArchData Line. 0 implies unlimited.
         */
        offset_type   line_size_;

        /*!
         * \brief Initialization value of data allocated by this ArchData
         */
        const uint64_t initial_;

        /*!
         * \brief Size of the value contained in initial_
         */
        const uint32_t initial_val_size_;

        /*!
         * \brief Position of lsb in an offset which represents the Line.
         */
        offset_type   line_lsb_;

        /*!
         * \brief Mask for selecting lines
         */
        offset_type   line_mask_;

        /*!
         * \brief Number of lines laid out so far (used during layout only)
         */
        uint32_t      num_lines_laid_out_;

        /*!
         * \brief Map of line offsets to lines which are currently allocated
         */
        LineMap       line_map_;

        /*!
         * \brief List of all Segments registered
         */
        SegmentList   seg_list_;

        /*!
         * \brief Final size of ArchData after layout is complete.
         */
        offset_type   size_;

        /*!
         * \brief Has 'layout' been completed
         */
        bool          is_laid_out_;

        /*!
         * \brief Bytes wasted during layout because of padding
         */
        uint32_t      layout_padding_waste_;

        /*!
         * \brief Bytes wasted during layout because of line-alignment (if not
         * also padding)
         */
        uint32_t      layout_line_waste_;

        /*!
         * \brief Can this ArchData free its lines
         */
        bool          can_free_lines_;

        /*!
         * \brief All ArchDatas currently in existence. Used for checkpointing
         * debugging to ensure they are all part of a tree
         */
        static std::vector<const ArchData*> *all_archdatas_;

    }; // class ArchData

} // namespace sparta


//! \brief Required in simulator source to define some globals.
#define SPARTA_ARCHDATA_BODY
