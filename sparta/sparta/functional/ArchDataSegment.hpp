// <ArchDataSegment> -*- C++ -*-

#ifndef __ARCH_DATA_SEGMENT_H__
#define __ARCH_DATA_SEGMENT_H__

#include <iostream>
#include <math.h>

#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/Utils.hpp"


namespace sparta
{
    class ArchData;

    //! Layout interface used by ArchData class
    //! Noncopyable. ArchData will track these segments by pointer.
    class ArchDataSegment
    {
    public:

        typedef uint64_t offset_type; //<! Represents offset (address) into ArchData
        typedef uint32_t ident_type; //<! DataView identifiers (distinguishes items in the same ArchData)

        //! Indicates an invalid ID for an ArchDataSegment or any refinement
        static const ident_type INVALID_ID = ~(ident_type)0;

        ArchDataSegment(const ArchDataSegment&) = delete;
        ArchDataSegment(const ArchDataSegment&&) = delete;
        ArchDataSegment& operator=(const ArchDataSegment&) = delete;

        /*!
         * \brief Constructor
         * \param data Data which this view will access
         * \param size size of <data> accessed by this view. Must be a power of
         * 2 greater than 0 and less than or equal to the line size of <data>
         * (sparta::ArchData::getLineSize)
         * \param subset_of Segment of which this segment is a subset (refers
         * to only a subset of the same bytes in this other register).
         * \param subset_offset Offset into the register indicated by
         * subset_of if subset_of is not INVALID_ID. Otherwise, is ignored.
         */
        ArchDataSegment(ArchData* data,
                        offset_type size,
                        ident_type id,
                        ident_type subset_of=INVALID_ID,
                        offset_type subset_offset=0) :
            offset_(0),  is_placed_(false), size_(size),
            adata_(data),
            ident_(id),
            subset_of_(subset_of),
            subset_offset_(subset_offset)

        {
            // No reason not to allow this, it just can't have subsegments
            //if(id == INVALID_ID){
            //    throw SpartaException("Cannot construct ArchDataSegment with identifier=INVALID_ID");
            //}

            if(!isPowerOf2(size)){
                throw SpartaException("size must be a power of 2, is ")
                    << size;
            }

            sparta_assert(getOffset() == 0); // Ensure initialized to 0 because getOffset expects this
        }

        /*!
         * \brief Virtual destructor
         */
        virtual ~ArchDataSegment() {};

        /*!
         * \brief Sets the offset of this DataView within its ArchData then
         * invokes the place_ method for subclasses to handle.
         * \param offset Offset into ArchData (data_) where this segment
         * resides now.
         * \throw SpartaException if already placed. Re-placing is illegal.
         */
        void place(offset_type offset) {
            if(is_placed_){
                SpartaException ex("ArchDataSegment ");
                ex << ident_ << " was already placed. Cannot place again";
                throw ex;
            }
            offset_ = offset;
            is_placed_ = true;

            place_(offset);
        }

        /*!
         * \brief Invokes writeInitial_, giving subclasses a chance to write a
         * value to memory during initialization or reset of an ArchData.
         * \pre Must be laid out
         */
        void writeInitial() {
            sparta_assert(is_placed_, "Should not be invoking writeInitial when is_placed_ is false");
            writeInitial_();
        }

        // Layout State

        /*!
         * \brief Has this segment been placed yet
         * \return true if placed
         */
        bool isPlaced() const { return is_placed_; }

        /*!
         * \brief Gets the offset of this segment once placed
         * \return Offset if placed. Otherwise 0.
         */
        offset_type getOffset() const { return offset_; }


        // Const Attributes

        /*!
         * \brief Gets the layout size of this segment (number of bytes)
         * \return Number of bytes contained in this segment
         */
        offset_type getLayoutSize() const { return size_; }

        /*!
         * \brief Gets the layout Identifier of this segment
         * \return segment ID - unique within parent.
         */
        ident_type getLayoutID() const { return ident_; }

        /*!
         * \brief Gets the segment of which this segment is a subset
         * \return segment ID. Valid ID if this segment is a subset of some
         * other, INVALID_ID if not.
         */
        ident_type getSubsetOf() const { return subset_of_; }

        /*!
         * \brief Gets the offset into the segment of which this segment is a
         * subset
         * \return offset into containing segment. Valid ID if this segment is a
         * subset of some other, INVALID_ID if not.
         */
        offset_type getSubsetOffset() const { return subset_offset_; }

        /*!
         * \brief Gets the ArchData associated with this segment
         */
        ArchData* getArchData() { return adata_; }

        /*!
         * \brief Gets the ArchData associated with this segment
         */
        const ArchData* getArchData() const { return adata_; }

    protected:

        /*!
         * \brief Allows subclasses to observe placement in an ArchData.
         * Do not write an initial value from within this method. Use
         * writeInitial_ instead
         *
         * Subclasses may override and then indirectly invoke this method to
         * respond to the placement. Actual placement may not be modified,
         * however. I
         *
         * At this point, it is not yet safe to read and write from the
         * ArchData at the specified <offset> with the registered size (size_).
         * Wait until the ArchData completes its layout (adata_.isLaidOut()).
         */
        virtual void place_(offset_type offset) {
            (void) offset;
        }

        /*!
         * \brief Write initial value of this segment into ArchData. This
         * occurrs immediately after placement and may occur multiple times
         * later if the owning ArchData is reset.
         * \pre isPlaced() will return true
         *
         * Subclasses may overwrite to supply an initial value after placement.
         */
        virtual void writeInitial_() {
        }

    private:

        // Placement
        offset_type offset_;         //!< Offset of this view into the ArchData to which it refers
        bool is_placed_;             //!< Has this view been placed in an ArchData yet
        const offset_type size_;     //!< Size of this view's data

        // Backend data
        ArchData* const adata_;      //!< ArchData which holds this view

        //
        const ident_type ident_;     //!< Identifier for this DataView
        const ident_type subset_of_; //!< Identifier of the DataView of which this DataView is a subset
        const offset_type subset_offset_; //!< Offset into the DataView referenced in subset_of_
    };

} // namespace sparta

// __ARCH_DATA_SEGMENT_H__
#endif
