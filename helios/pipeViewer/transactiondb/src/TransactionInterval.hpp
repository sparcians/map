
/**
 * \file TransactionInterval.h
 *
 * \copyright
 */

#pragma once

#include <iostream>
#include <numeric>
#include <string>
#include <vector>
#include "sparta/pairs/PairFormatter.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace sparta::pipeViewer {
template <class Dat_t>
class transactionInterval {
private:
    const Dat_t time_Start_;    /*! Left Boundary of Interval */
    const Dat_t time_End_;    /*! Right Boundary of Interval */

    transactionInterval(const Dat_t lval,
                        const Dat_t rval,
                        const uint16_t cpid,
                        const uint64_t trid,
                        const uint64_t dispid,
                        const uint64_t lctn,
                        const uint16_t flgs,
                        const uint64_t ptid,
                        const uint32_t opcd,
                        const uint64_t vadr,
                        const uint64_t radr,
                        const uint16_t lngt,
                        std::string&& iannt,
                        const uint16_t pair_id,
                        const std::vector<uint16_t>& sz,
                        const std::vector<std::pair<uint64_t, bool>>& vals,
                        const std::vector<std::string>& nams,
                        const std::vector<std::string>& str,
                        const PairFormatterVector& del) :
        time_Start_(lval),
        time_End_(rval),
        control_ProcessID(cpid),
        transaction_ID(trid),
        display_ID(dispid),
        location_ID(lctn),
        flags(flgs),
        parent_ID(ptid),
        operation_Code(opcd),
        virtual_ADR(vadr),
        real_ADR(radr),
        length(lngt),
        annt(std::move(iannt)),
        pairId(pair_id),
        sizeOfVector(sz),
        valueVector(vals),
        nameVector(nams),
        stringVector(str),
        delimVector(del)
    {
        sparta_assert( time_Start_ <= time_End_);
    }

    transactionInterval(const Dat_t lval,
                        const Dat_t rval,
                        const uint16_t cpid,
                        const uint64_t trid,
                        const uint64_t dispid,
                        const uint64_t lctn,
                        const uint16_t flgs,
                        const uint64_t ptid,
                        const uint32_t opcd,
                        const uint64_t vadr,
                        const uint64_t radr,
                        const uint16_t lngt,
                        std::string&& iannt) :
        transactionInterval(lval,
                            rval,
                            cpid,
                            trid,
                            dispid,
                            lctn,
                            flgs,
                            ptid,
                            opcd,
                            vadr,
                            radr,
                            lngt,
                            std::move(iannt),
                            0,
                            {},
                            {},
                            {},
                            {},
                            {})
    {
    }

public:
    using IntervalDataT = Dat_t;
    const uint16_t control_ProcessID;    /*! Core ID*/
    const uint64_t transaction_ID;    /*! Transaction ID*/
    const uint64_t display_ID;      /*! Use to control display character and color */
    const uint16_t location_ID;    /*! Location ID*/
    const uint16_t flags;    /*! Assorted Transaction Flags*/
    const uint64_t parent_ID;    /*! Parent Transaction ID*/
    const uint32_t operation_Code;    /*! Operation Code*/
    const uint64_t virtual_ADR;    /*! Virtual Address*/
    const uint64_t real_ADR;    /*! Real Address*/
    const uint16_t length;   /*! Annotation Length or Name Value Pair count*/
    const std::string annt; /*! Annotation Pointer*/
    const uint16_t pairId; /*! Unique id required for pipeline collection*/
    const std::vector<uint16_t> sizeOfVector; /*! Vector of integers representing Sizeof of every field */
    const std::vector<std::pair<uint64_t, bool>> valueVector; /*! Vector of integers containing the actual data of every field */
    const std::vector<std::string> nameVector ; /*! Vector of strings containing the actual Names of every field */
    const std::vector<std::string> stringVector; /*! Vector of strings containing the actual string value of every field */
    const PairFormatterVector delimVector;

    // Constructor for transaction_t
    transactionInterval( const Dat_t lval, const Dat_t rval,
                         const uint16_t cpid, const uint64_t trid, const uint64_t dispid,
                         const uint64_t lctn, const uint16_t flgs) :
        transactionInterval(lval, rval, cpid, trid, dispid, lctn, flgs, 0, 0, 0)
    {
    }

    // Constructor for annotation_t
    transactionInterval( const Dat_t lval, const Dat_t rval,
        const uint16_t cpid, const uint64_t trid, const uint64_t dispid,
        const uint64_t lctn, const uint16_t flgs,
        const uint64_t ptid, const uint16_t lngt, std::string iannt) :
        transactionInterval(lval,
                            rval,
                            cpid,
                            trid,
                            dispid,
                            lctn,
                            flgs,
                            ptid,
                            0,
                            0,
                            0,
                            lngt,
                            std::move(iannt))
    {
        sparta_assert(lngt!=0);
    }

    // Constructor for instrucation_t
    transactionInterval( const Dat_t lval, const Dat_t rval,
        const uint16_t cpid, const uint64_t trid, const uint64_t dispid,
        const uint64_t lctn, const uint16_t flgs,
        const uint64_t ptid, const uint32_t opcd,
        const uint64_t vadr, const uint64_t radr) :
        transactionInterval(lval,
                            rval,
                            cpid,
                            trid,
                            dispid,
                            lctn,
                            flgs,
                            ptid,
                            opcd,
                            vadr,
                            radr,
                            0,
                            "")
    {
    }

    // Constructor for memoryoperation_t
    transactionInterval( const Dat_t lval, const Dat_t rval,
        const uint16_t cpid, const uint64_t trid, const uint64_t dispid,
        const uint64_t lctn, const uint16_t flgs,
        const uint64_t ptid, const uint64_t vadr,
        const uint64_t radr) :
        transactionInterval(lval,
                            rval,
                            cpid,
                            trid,
                            dispid,
                            lctn,
                            flgs,
                            ptid,
                            0,
                            vadr,
                            radr)
    {
    }

    // Constructor for pair_t
    transactionInterval(const Dat_t lval, const Dat_t rval,
        const uint16_t cpid, const uint64_t trid, const uint64_t dispid,
        const uint64_t lctn, const uint16_t flgs,
        const uint64_t ptid, const uint16_t lngt,
        const uint16_t pair_id, const std::vector<uint16_t>& sz,
        const std::vector<std::pair<uint64_t, bool>>& vals,
        const std::vector<std::string>& nams,
        const std::vector<std::string>& str,
        const PairFormatterVector& del) :
        transactionInterval(lval,
                            rval,
                            cpid,
                            trid,
                            dispid,
                            lctn,
                            flgs,
                            ptid,
                            0,
                            0,
                            0,
                            lngt,
                            "",
                            pair_id,
                            sz,
                            vals,
                            nams,
                            str,
                            del)
    {
    }

    // Copy Constructor
    transactionInterval(const transactionInterval<Dat_t>& rhp) = default;

    // Move Constructor
    transactionInterval(transactionInterval<Dat_t>&& rhp) = default;

    /*!
     * \brief Compute size of this interval including memory allocated for the annotation or the memory allocated by all the values for the pair
     */
    uint64_t getSizeInBytes() const {
        uint64_t size = sizeof(*this);
        if(!annt.empty()) {
            size += length;
        }
        else {
            size = std::accumulate(std::next(sizeOfVector.begin()), sizeOfVector.end(), size);
        }
        return size;
    }

    /*! Function: Return the Left value of the event*/
    Dat_t getLeft() const noexcept{ return( time_Start_ ); }
    /*! Function: Return the Right value of the event*/
    Dat_t getRight() const noexcept{ return( time_End_ ); }

    /*! Function: Check if &V is within the event range*/
    bool contains(const Dat_t V) const noexcept {
        return( (V >= time_Start_) && ( V < time_End_) );
    }

    /*! Function: Check if &l,&r are within the event range*/
    bool containsInterval(const Dat_t l, const Dat_t r) const noexcept {
        return ( (time_Start_ <= l) && (time_End_ >= r) );
    }
}; // transactionInterval

}//NAMESPACE:sparta::pipeViewer

#pragma once
