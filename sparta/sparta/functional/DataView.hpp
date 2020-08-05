// <DataView> -*- C++ -*-

#pragma once

#include <iostream>
#include <ios>
#include <iomanip>
#include <math.h>

#include "sparta/functional/ArchDataSegment.hpp"
#include "sparta/functional/ArchData.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"


namespace sparta
{

    //! Can be a subset of another DataView
    //! ArchDataSegment provides the layout inteface
    class DataView : public ArchDataSegment
    {
    public:

        typedef ArchDataSegment::offset_type offset_type; //!< Represents offset into ArchData
        typedef ArchDataSegment::ident_type ident_type; //!< DataView identifiers (distinguishes views in the same ArchData)
        typedef uint32_t index_type; //!< Type used for specifying index into this DataView during a read or write.

        //! Invalid Identifier constant for a DataView
        using ArchDataSegment::INVALID_ID;

        //! String to show instead of a value when representing an unplaced dataview.
        static const std::string DATAVIEW_UNPLACED_STR;

        /*!
         * \brief Construct a DataView
         * \param data Data which this view will access. Must not be nullptr
         * \param id ID of this DataView (as an ArchDataSegment
         * \param size size of <data> accessed by this view. Must be a power of
         * 2 greater than 0. This is validated by ArchDataSegment
         * \param subset_of ID of another DataView of which this is a subset
         * (occupies only a subset of the other dataview's space).
         * \param subset_offset Offset in bytes within the dataview indicated by
         * \a subset_of
         * \param initial_buf_le Buffer from which initial value will be copied
         * byte-by-byte from a little-endian byte array source. Therefore, bytes
         * could be swapped if SPARTA is run (if supported) on a BE architecture.
         * The result should be that hand-coded intitial_buf byte-arrays needn't
         * be updated during such changes. This pointer must be nullptr or
         * contain a number of bytes >= \a size. The pointer must be valid at
         * least until DataView::place is called during initialization
         * \note Subsets relationships might not be validated at construction.
         * Invalid subset relationships might only be tested at ArchData
         * layout-time. This means that a DataView which is larger than
         * another DataView of which it is declared a subset will not cause an
         * error until layout-time (see layout).
         */
        DataView(ArchData* data,
                 ident_type id,
                 offset_type size,
                 ident_type subset_of=INVALID_ID,
                 offset_type subset_offset=0,
                 const uint8_t* initial_buf_le=nullptr) :
            ArchDataSegment(data, size, id, subset_of, subset_offset),
            adata_(data), offset_(0),
            line_(0),
            initial_buf_le_(initial_buf_le)
        {
            sparta_assert(data, "ArchData (data) must not be null");
            static_assert(sizeof(uint64_t) > sizeof(index_type),
                          "Must have a type larger than index_type in order to "
                          "perform bounds checking on index values which must "
                          "support the entire range from 0 to the maximum value");

            // Allowed, but no other segments can be a subset
            //if(id == INVALID_ID){
            //    throw SpartaException("Cannot construct DataView with identifier=INVALID_ID");
            //}

            if(subset_of == INVALID_ID && subset_offset != 0){
                throw SpartaException("Cannot construct DataView with a valid identifier and nonzero offset: 0x")
                    << std::hex << subset_offset << ". Change subset_offset to 0 or set subset_of to INVALID_ID";
            }

            adata_->registerSegment(this); // Add self
        }


        // Attributes (non-virtual access)

        ArchData* getArchData() const { return adata_; }
        offset_type getSize() const { return getLayoutSize(); }
        offset_type getOffset() const { return offset_; }
        ArchData::Line* getLine() const { return line_; } //!< Get already-placed line
        ident_type getID() const { return getLayoutID(); }


        // I/O methods

        /*!
         * \brief Reads a value from this DataView at the specific index.
         * \tparam T type of value to read. Should be [u]intNN_t. See
         * sparta::ArchData::Line::read specializations for supported types.
         * \tparam BO sparta::ByteOrder describing read byte-order.
         * \param idx Index offset within this DataView from low memory address
         * in the DataView. Index refers to multiples of sizeof(T).
         * This is used for reading the content of the DataView
         * as smaller values. For example, a 128b DataView can be read as two
         * uint64_t values by calling read<uint64_t, LE>(0), read<uint64_t, LE>(1).
         * The caller would then be responsible for performing 128b operations
         * using these two values.
         * \return The read value interpreted as the appropriate byte-order from
         * memory referenced by this DataView.
         * \note May generate assertion if read size is invalid. These
         * assertions should be disabled in optimized mode for performance.
         * \note sizeof(T)*index + sizeof(T) must be less than getSize().
         * \pre This view must have been placed (isPlaced())
         *
         * Example:
         * \code{.cpp}
         * assert(dv.getSize() == 64);
         * uint32_t val1 = dv.read<uint32_t, LE>();
         * uint32_t val2 = dv.read<uint32_t, LE>(1);
         * \endcode
         */
        template <typename T, ByteOrder BO=LE>
        T read(index_type idx=0) const {
            sparta_assert(idx < (getSize() / sizeof(T)), // Avoids overflow with large indexes
                              "read index " << idx << " and type " << demangle(typeid(T).name())
                              << " (size " << sizeof(T) << ") is invalid for this DataView of size " << getSize());
            return readUnsafe<T, BO>(idx);
        }

        /*!
         * \brief Same behavior as read but without checking access bounds
         * \note This exists to avoid redundancy if higher-level functions are
         * already checkint the access bounds
         */
        template <typename T, ByteOrder BO=LE>
        T readUnsafe(index_type idx=0) const {
            sparta_assert(line_, "There is no line pointer set for this DataView. ArchData likely has not been laid out yet. Tree probably needs to be finalized first.");
            return line_->read<T, BO>(offset_, idx);
        }

        /*!
         * \brief Reads a value from this DataView using a type T which might be
         * larger than the dataview
         * \see sparta::DataView::read
         * \param idx Index at which to read to this view in terms of multiples
         * of sizeof(T)
         * \see read
         * \pre This view must have been placed (isPlaced())
         *
         * A good example usage is T=uint64_t, getSize()=4, and idx_=0
         */
        template <typename T, ByteOrder BO=LE>
        T readPadded(index_type idx=0) const {
            sparta_assert(sizeof(T) * ((uint64_t)idx) <= getSize(),
                              "readPadded index " << idx << " and type " << demangle(typeid(T).name())
                              << " (size " << sizeof(T) << ") is invalid for this DataView of size " << getSize());
            return readPaddedUnsafe<T, BO>(idx);
        }

        /*!
         * \brief Same behavior as readPadded but without checking access bounds
         * \note This exists to avoid redundancy if higher-level functions are
         * already checkint the access bounds
         */

        template <typename T, ByteOrder BO=LE>
        T readPaddedUnsafe(index_type idx=0) const {
            sparta_assert(line_, "There is no line pointer set for this DataView. ArchData likely has not been laid out yet. Tree probably needs to be finalized first.");
            offset_type max_bytes = getSize() - (sizeof(T) * idx); // Must always be a power of 2
            sparta_assert(isPowerOf2(max_bytes));

            T result;
            if(max_bytes >= 8){
                result = line_->read<uint64_t, BO>(offset_, idx);
            }else if(max_bytes == 4){
                result = line_->read<uint32_t, BO>(offset_, idx);
            }else if(max_bytes == 2){
                result = line_->read<uint16_t, BO>(offset_, idx);
            }else if(max_bytes == 1){
                result = line_->read<uint8_t,  BO>(offset_, idx);
            }else{
                sparta_assert(max_bytes == 0);
                result = 0;
            }

            return result;
        }

        /*!
         * \brief Writes a value to this DataView at the specific index.
         * \tparam T type of value to write. Should be [u]intNN_t. See
         * sparta::ArchData::Line::read specializations for supported types.
         * \tparam BO sparta::ByteOrder describing read byte-order.
         * \param idx Index offset within this DataView from low memory address
         * in the DataView. Index refers to multiples of sizeof(T).
         * This is used for writing to the content of the DataView
         * as smaller values. For example, a 128b DataView can be written as two
         * uint64_t values by calling write<uint64_t, LE>(val1, 0),
         * write<uint64_t, LE>(val2, 1).
         * The caller would then be responsible for performing representing the
         * 128b value before writing to the register.
         * \return The write value stored into memory using the appropriate
         * byte-order from memory.
         * \note May generate assertion if read size is invalid. These
         * assertions should be disabled in optimized mode for performance.
         * \note sizeof(T)*index + sizeof(T) must be less than getSize().
         * \pre This view must have been placed (isPlaced())
         *
         * Example:
         * \code{.cpp}
         * assert(dv.getSize() == 64);
         * dv.write<uint32_t, LE>(val1);
         * dv.write<uint32_t, LE>(val2, 1);
         * \endcode
         */
        template <typename T, ByteOrder BO=LE>
        void write(T val, index_type idx=0) {
            sparta_assert(idx < (getSize() / sizeof(T)), // Avoids overflow with large indexes
                              "write index " << idx << " and type " << demangle(typeid(T).name())
                              << " (size " << sizeof(T) << ") is invalid for this DataView of size " << getSize());

            writeUnsafe<T, BO>(val, idx);
        }

        /*!
         * \brief Same behavior as write but without checking access bounds
         * \note This exists to avoid redundancy if higher-level functions are
         * already checking the access bounds
         *
         */
        template <typename T, ByteOrder BO=LE>
        void writeUnsafe(T val, index_type idx=0) {
            sparta_assert(line_, "There is no line pointer set for this DataView. ArchData likely has not been laid out yet. Tree probably needs to be finalized first.");

            line_->write<T, BO>(offset_, val, idx);
        }

        /*!
         * \brief Writes value from this DataView using a type T which might be
         * larger than the size of the DataView.
         * \see sparta::DataView::read
         * \param val Value to write. This may contain more set significant
         * bytes than than the number of bytes available in this view following
         * the chosen idx
         * \param idx Index at which to write to this view in terms of multiples
         * of sizeof(T)
         * \pre This view must have been placed (isPlaced())
         *
         * Truncates most signficant bits from val if larger than the number of
         * bytes which can be written at idx.
         *
         * A good example usage is T=uint64_t, getSize()=4, and idx_=0
         */
        template <typename T, ByteOrder BO=LE>
        void writeTruncated(T val, uint32_t idx=0) {
            sparta_assert(sizeof(T) * ((uint64_t)idx) <= getSize(),
                              "writeTruncated index " << idx << " and type " << demangle(typeid(T).name())
                              << " (size " << sizeof(T) << ") is invalid for this DataView of size " << getSize());

            writeTruncatedUnsafe<T, BO>(val, idx);
        }

        /*!
         * \brief Same behavior as writeTruncated bout without checking access
         * bounds
         * \note This exists to avoid redundancy if higher-level functions are
         * already checking the access bounds
         * \note Still checks access size for sanity (i.e. positive integer
         * power of 2 and <= 8)
         */
        template <typename T, ByteOrder BO=LE>
        void writeTruncatedUnsafe(T val, uint32_t idx=0) {
            sparta_assert(line_, "There is no line pointer set for this DataView. ArchData likely has not been laid out yet. Tree probably needs to be finalized first.");
            offset_type max_bytes = getSize() - (sizeof(T) * idx); // Must always be a power of 2
            sparta_assert(isPowerOf2(max_bytes));

            if(max_bytes >= 8){
                line_->write<uint64_t, BO>(offset_, (uint64_t)val, idx);
            }else if(max_bytes == 4){
                line_->write<uint32_t, BO>(offset_, (uint32_t)val, idx);
            }else if(max_bytes == 2){
                line_->write<uint16_t, BO>(offset_, (uint16_t)val, idx);
            }else if(max_bytes == 1){
                line_->write<uint8_t,  BO>(offset_, (uint8_t)val, idx);
            }else{
                sparta_assert(max_bytes == 0);
            }
        }

        /*!
         * \brief Reads data from another dataview and writes that value to this
         * DataView.
         * \pre rhp and this must have same size
         * \pre Dataviews must be placed. See DataView::isPlaced
         * \throw SpartaException if dataviews have different size
         * \note This method performs no re-ordering since endianness is a
         * property of data-access and not data storage.
         */
        DataView& operator=(const DataView& rhp) {
            if(getSize() != rhp.getSize()){
                throw SpartaException("Cannot copy data between DataViews using operator= because the sizes differ");
            }
            if(rhp.line_ == nullptr){
                throw SpartaException("Cannot copy data between DataViews using operator= because the right-hand operand is not laid out");
            }
            if(line_ == nullptr){
                throw SpartaException("Cannot copy data between DataViews using operator= because the left-hand operand is not laid out");
            }

            const uint8_t* const src = rhp.line_->getDataPointer(rhp.offset_);
            line_->write(offset_, getSize(), src);

            return *this;
        }

        /*!
         * \brief Dump data in this DataView as hex bytes in address order with a
         * space between each pair.
         * \pre This view must have been placed (isPlaced())
         *
         * Example for a 16bit DataView:
         * \code{.cpp}
         * std::cout << dv.getByteString();
         * de ad be ef
         * \endcode
         */
        std::string getByteString() const {
            sparta_assert(isPlaced(), "DataView has not been placed");

            std::stringstream o;
            o.fill('0');
            for(offset_type i=0; i<getSize(); ++i){
                o << std::setw(2) << std::hex << (uint16_t)read<uint8_t, LE>(i) << ' ';
            }
            return o.str();
        }

        /*!
         * \brief Reads the value of this DataView and renders as a string in
         * the specified Byte-Order as a prefixed hex number with zero-padding.
         * (e.g. 0x00c0ffee).
         * \pre This view must have been placed (isPlaced())
         * \return String of contained data read in the specified endianness
         * \tparam BO ByteOrder in which to read the value of this DataView
         */
        template <ByteOrder BO=LE>
        std::string getValueAsString() const {
            sparta_assert(isPlaced(), "DataView has not been placed");

            std::stringstream ss;
            ss << "0x" << std::hex << std::setw(getSize()*2) << std::setfill('0');
            switch(getSize()){
            case 1:
                ss << (uint32_t)read<uint8_t, BO>();
                break;
            case 2:
                ss << (uint32_t)read<uint16_t, BO>();
                break;
            case 4:
                ss << read<uint32_t, BO>();
                break;
            default: // 8 or more bytes
                assembleIndexedReadsToValue_<BO>(ss);
            }
            return ss.str();
        }

    protected:

        //! \name Inherited from ArchDataSegment
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Places this DataView within its ArchData.
         * \post Writes initial value if specified at construction
         * \param offset Offset into <data>
         * \todo Store pointer directly to value to save an add in offset
         */
        virtual void place_(offset_type offset) override {
            adata_->checkSegment(offset, getSize());
            line_ = &adata_->getLine(offset);
            offset_ = offset - line_->getOffset(); // Store locally for faster 'read' calls
        }

        /*!
         * \brief Writes the initial value of this DataView into memory.
         * This is guaranteed to be called after placement
         */
        virtual void writeInitial_() override {
            // Write initial value
            if(initial_buf_le_){
#if __BYTE_ORDER == __LITTLE_ENDIAN
                for(offset_type i=0; i<getSize(); ++i){
                    write<uint8_t>(initial_buf_le_[i], i);
                }
#else // #if __BYTE_ORDER == __LITTLE_ENDIAN
                for(offset_type i=getSize(); i>0; --i){
                    write<uint8_t>(initial_buf_le_[i], i);
                }
#endif // else // #if __BYTE_ORDER == __LITTLE_ENDIAN
            }
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

    private:

        /*!
         * \brief Performs indexed byte reads through this DataView
         * and orders them from MSB to LSB (normal readable representation)
         */
        template <ByteOrder BO=LE>
        void assembleIndexedReadsToValue_(std::stringstream& ss) const {
            ss << "<unknown byte-order: " << BO << ">";
        }

        ArchData* const adata_;   //!< ArchData which holds this view
        offset_type offset_;      //!< Offset of this view into the ArchData::Line to which it refers
        ArchData::Line* line_;    //!< Line within ArchData through which data will be accessed

        /*!
         * \brief Data which will be copied as if it were a little-endian source
         * into ArchData memory once this view is placed
         */
        const uint8_t* const initial_buf_le_;
    }; // class DataView

    template <>
    inline void DataView::assembleIndexedReadsToValue_<LE>(std::stringstream& ss) const {
        for(offset_type i=(getSize()/8); i>0; --i){
            ss << std::setw(16) << read<uint64_t, LE>(i-1);
        }
    }

    template <>
    inline void DataView::assembleIndexedReadsToValue_<BE>(std::stringstream& ss) const {
        for(offset_type i=0; i<getSize()/8; ++i){
            ss << std::setw(16) << read<uint64_t, BE>(i);
        }
    }

} // namespace sparta

//! \brief Required in simulator source to define some globals.
#define SPARTA_DATAVIEW_BODY                                              \
    const std::string sparta::DataView::DATAVIEW_UNPLACED_STR = "dataview-unplaced";
