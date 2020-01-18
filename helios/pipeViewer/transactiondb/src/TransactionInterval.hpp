
/**
 * \file TransactionInterval.h
 *
 * \copyright
 */

#ifndef __TRANSACTION_INTERVAL_H__
#define __TRANSACTION_INTERVAL_H__

#include <iostream>
#include "sparta/utils/SpartaAssert.hpp"

namespace sparta {
namespace pipeViewer {
template <class Dat_t>
class transactionInterval {
private:
    Dat_t time_Start_;    /*! Left Boundary of Interval */
    Dat_t time_End_;    /*! Right Boundary of Interval */
public:
    typedef Dat_t IntervalDataT;
    uint16_t control_ProcessID;    /*! Core ID*/
    uint64_t transaction_ID;    /*! Transaction ID*/
    uint16_t location_ID;    /*! Location ID*/
    uint16_t flags;    /*! Assorted Transaction Flags*/
    uint64_t parent_ID;    /*! Parent Transaction ID*/
    uint32_t operation_Code;    /*! Operation Code*/
    uint64_t virtual_ADR;    /*! Virtual Address*/
    uint64_t real_ADR;    /*! Real Address*/
    uint16_t length;   /*! Annotation Length or Name Value Pair count*/
    char* annt; /*! Annotation Pointer*/
    uint16_t pairId; /*! Unique id required for pipeline collection*/
    std::vector<uint16_t> sizeOfVector; /*! Vector of integers representing Sizeof of every field */
    std::vector<std::pair<uint64_t, bool>> valueVector; /*! Vector of integers containing the actual data of every field */
    std::vector<std::string> nameVector ; /*! Vector of strings containing the actual Names of every field */
    std::vector<std::string> stringVector; /*! Vector of strings containing the actual string value of every field */
    std::vector<std::string> delimVector;

    // Constructor for transaction_t
    transactionInterval( const Dat_t &lval, const Dat_t &rval,
        const uint16_t &cpid, const uint64_t &trid,
        const uint64_t &lctn, const uint16_t &flgs) :
        time_Start_(lval), time_End_(rval), control_ProcessID(cpid),
        transaction_ID(trid), location_ID(lctn), flags(flgs) ,
        parent_ID(0), operation_Code(0), virtual_ADR(0), real_ADR(0),
        length(0), annt(nullptr), pairId(0), sizeOfVector(), valueVector(),
        nameVector(), stringVector(), delimVector(){
            sparta_assert( time_Start_ <= time_End_);
        }

    // Constructor for annotation_t
    transactionInterval( const Dat_t &lval, const Dat_t &rval,
        const uint16_t &cpid, const uint64_t &trid,
        const uint64_t &lctn, const uint16_t &flgs,
        const uint64_t &ptid, const uint16_t &lngt, const char *iannt) :
        transactionInterval(lval, rval, cpid, trid, lctn, flgs)
        {
            length = lngt;
            annt = nullptr;
            sparta_assert( time_Start_ <= time_End_);
            sparta_assert(lngt!=0);
            if(iannt != nullptr) {
                annt = new char[lngt];
                memcpy(annt,iannt,lngt);
            }
        }

    // Constructor for instrucation_t
    transactionInterval( const Dat_t &lval, const Dat_t &rval,
        const uint16_t &cpid, const uint64_t &trid,
        const uint64_t &lctn, const uint16_t &flgs,
        const uint64_t &ptid, const uint32_t &opcd,
        const uint64_t &vadr, const uint64_t &radr) :
        transactionInterval(lval, rval, cpid, trid, lctn, flgs)
        {
            parent_ID = ptid;
            operation_Code = opcd;
            virtual_ADR = vadr;
            real_ADR = radr;
            sparta_assert( time_Start_ <= time_End_);
        }

    // Constructor for memoryoperation_t
    transactionInterval( const Dat_t &lval, const Dat_t &rval,
        const uint16_t &cpid, const uint64_t &trid,
        const uint64_t &lctn, const uint16_t &flgs,
        const uint64_t &ptid, const uint64_t &vadr,
        const uint64_t &radr) :
        transactionInterval(lval, rval, cpid, trid, lctn, flgs)
        {
            parent_ID = ptid;
            virtual_ADR = vadr;
            real_ADR = radr;
            sparta_assert( time_Start_ <= time_End_);
        }

    // Constructor for pair_t
    transactionInterval(const Dat_t& lval, const Dat_t& rval,
        const uint16_t& cpid, const uint64_t& trid,
        const uint64_t& lctn, const uint16_t& flgs,
        const uint64_t& ptid, const uint16_t& lngt,
        const uint16_t pair_id, const std::vector<uint16_t>& sz,
        const std::vector<std::pair<uint64_t, bool>>& vals,
        const std::vector<std::string>& nams,
        const std::vector<std::string>& str,
        const std::vector<std::string>& del) :
        transactionInterval(lval, rval, cpid, trid, lctn, flgs) {
        sparta_assert(time_Start_ <= time_End_);
        parent_ID = ptid;
        length = lngt;
        pairId = pair_id;
        sizeOfVector = sz;
        valueVector = vals;
        nameVector = nams;
        stringVector = str;
        delimVector = del;
    }

    // Copy Constructor
    transactionInterval( const transactionInterval<Dat_t>& rhp ) :
        time_Start_(rhp.time_Start_),
        time_End_(rhp.time_End_),
        control_ProcessID(rhp.control_ProcessID),
        transaction_ID(rhp.transaction_ID),
        location_ID(rhp.location_ID),
        flags(rhp.flags),
        parent_ID(rhp.parent_ID),
        operation_Code(rhp.operation_Code),
        virtual_ADR(rhp.virtual_ADR),
        real_ADR(rhp.real_ADR),
        length(rhp.length),
        annt(rhp.annt),
        pairId(rhp.pairId),
        sizeOfVector(rhp.sizeOfVector),
        valueVector(rhp.valueVector),
        nameVector(rhp.nameVector),
        stringVector(rhp.stringVector),
        delimVector(rhp.delimVector)
        {
            if(rhp.annt){
                assert(length > 0);
                annt = new char[length];
                memcpy(annt, rhp.annt, length);
            }
        }

    /*!
     * \brief Not assignable
    */
    transactionInterval& operator=( const transactionInterval<Dat_t>& rhp ) = delete;

    /*!
     * \brief Destructor
     */
    ~transactionInterval() { delete [] annt; };

    /*!
     * \brief Compute size of this interval including memory allocated for the annotation or the memory allocated by all the values for the pair
     */
    uint64_t getSizeInBytes() const {
        uint64_t size = sizeof(*this);
        if(annt){
            size += length;
        }
        else{
            for(size_t i = 1; i != length; ++i){
                size += sizeOfVector[i];
            }
        }
        return size;
    }

    /*! Funcenumstion: Return the Left value of the event*/
    Dat_t getLeft() const noexcept{ return( time_Start_ ); }
    /*! Function: Return the Right value of the event*/
    Dat_t getRight() const noexcept{ return( time_End_ ); }

    /*! Function: Check if &V is within the event range*/
    bool contains(const Dat_t &V)const noexcept{
        return( (V >= time_Start_) && ( V < time_End_) );
    }

    /*! Function: Check if &l,&r are within the event range*/
    bool containsInterval( const Dat_t &l, const Dat_t &r ) const noexcept{
        return ( (time_Start_ <= l) && (time_End_ >= r) );
    }
}; // transactionInterval

}//NAMESPACE:pipeViewer
}//NAMESPACE:sparta

#endif // #ifndef __TRANSACTION_INTERVAL_H__
