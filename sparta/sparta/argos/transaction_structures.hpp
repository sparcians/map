/**
 * \file transaction_structures.hpp
 *
 */

#ifndef __TRANSACTION_STRUCTURES_H__
#define __TRANSACTION_STRUCTURES_H__

#include <inttypes.h>
#include <utility>
#include <vector>

#define is_Annotation 0x1
#define is_Instruction 0x2
#define is_MemoryOperation 0x3
#define is_Pair 0x4
#define TYPE_MASK 0x7//!< Mask used for extracting type ID portion from transaction flags
#define CONTINUE_FLAG 0x10 //!< Flag used to indicate if this transaction should be considered a continuation of the previous transaction

/*!
 * \brief Old version of the transaction structures namespace
 */
namespace version1 {
    struct transaction_t {
        uint64_t time_Start;  //! Event Start Time   8 Bytes
        uint64_t time_End;  //! Event End Time     8 Bytes
        uint64_t parent_ID;  //! Parent Transaction ID 8 Bytes
        uint64_t transaction_ID;  //! TRnasaction ID     8 Bytes
        uint16_t control_Process_ID;  //! Control Process ID 2 Bytes
        uint16_t location_ID;  //! Location           2 Bytes
        uint16_t flags;  //! Flags/Trans Type   2 Bytes
    };

    // All structures other than the Generic Transaction Event
    //will have the top varaible match the GTE variables in size
    //and in name

    // Instruction Event
    struct instruction_t : public transaction_t {
        uint32_t operation_Code;  //! Operation Code        4 Bytes
        uint64_t virtual_ADR;  //! Virtual Address       8 Bytes
        uint64_t real_ADR;  //! Real Address          8 Bytes
    };

    // Memory Operation Event
    struct memoryoperation_t : public transaction_t {
        uint64_t virtual_ADR;  //! 8 Bytes
        uint64_t real_ADR;  //! 8 Bytes
    };

    // Annotation Event (Catch-All)
    struct annotation_t : public transaction_t {
        uint16_t length;  //! Annotation Length 2 Bytes
        const char *annt; //! Pointer to Annotation Start
    };
}

/*!
 * \brief Generic transaction event, packed for density on disk
 * \warning Since this is written as a chunk, it is endian-dependent and
 * possibly compiler dependent
 * \todo Should consider removing packing attribute since it is mainly related
 * to serialization. Create custom packed serialization code instead.
 */
struct __attribute__ ((__packed__)) transaction_t  {
    uint64_t time_Start;  //! Event Start Time   8 Bytes
    uint64_t time_End;  //! Event End Time     8 Bytes
    uint64_t parent_ID;  //! Parent Transaction ID 8 Bytes
    uint64_t transaction_ID;  //! TRnasaction ID     8 Bytes
    uint32_t location_ID;  //! Location           4 Bytes
    uint16_t flags;  //! Flags/Trans Type   2 Bytes
    uint16_t control_Process_ID;  //! Control Process ID 2 Bytes

    transaction_t() = default;

    //! Parameterized Constructor
    transaction_t(uint64_t time_Start, uint64_t time_End, uint64_t parent_ID,
        uint64_t transaction_ID, uint32_t location_ID, uint16_t flags,
        uint16_t control_Process_ID) : 
        time_Start(time_Start), time_End(time_End), parent_ID(parent_ID),
        transaction_ID(transaction_ID), location_ID(location_ID), flags(flags),
        control_Process_ID(control_Process_ID) {}

    // Version conversion move constructors
    transaction_t(version1::transaction_t&& old_obj) :
        time_Start(old_obj.time_Start),
        time_End(old_obj.time_End),
        parent_ID(old_obj.parent_ID),
        transaction_ID(old_obj.transaction_ID),
        location_ID(old_obj.location_ID),
        flags(old_obj.flags),
        control_Process_ID(old_obj.control_Process_ID)
        {;}
};

// Instruction Event
struct instruction_t : public transaction_t {
    uint32_t operation_Code;  //! Operation Code        4 Bytes
    uint64_t virtual_ADR;  //! Virtual Address       8 Bytes
    uint64_t real_ADR;  //! Real Address          8 Bytes

    instruction_t() = default;

    // Version convertion move constructors
    instruction_t(version1::instruction_t&& old_obj) :
        transaction_t(std::move(old_obj)),
        operation_Code(old_obj.operation_Code),
        virtual_ADR(old_obj.virtual_ADR),
        real_ADR(old_obj.real_ADR)
    {;}
};

// Memory Operation Event
struct memoryoperation_t : public transaction_t {
    uint64_t virtual_ADR;  //! 8 Bytes
    uint64_t real_ADR;  //! 8 Bytes

    memoryoperation_t() = default;

    // Version convertion move constructors
    memoryoperation_t(version1::memoryoperation_t&& old_obj) :
        transaction_t(std::move(old_obj)),
        virtual_ADR(old_obj.virtual_ADR),
        real_ADR(old_obj.real_ADR)
    {;}
};

// Annotation Event (Catch-All)
struct annotation_t : public transaction_t {
    uint16_t length;  //! Annotation Length 2 Bytes
    const char *annt; //! Pointer to Annotation Start

    annotation_t() = default;

    // Version convertion move constructors
    annotation_t(version1::annotation_t&& old_obj) :
        transaction_t(std::move(old_obj)),
        length(old_obj.length),
        annt(old_obj.annt)
    {

        old_obj.length = 0;
        old_obj.annt = nullptr;
    }
};

// Name Value Pair Event
struct pair_t : public transaction_t {
    // Number of pairs contained in this record 2 Bytes.
    uint16_t length {0};

    // The Unique pair id for every Name-Value class collected.
    uint16_t pairId {0};

    // Vector of 2 Byte unsigned ints which contains the 
    // sizeofs of every different pair value in a record
    std::vector<uint16_t> sizeOfVector;

    // Vector of 8 Byte unsigned ints which contains the 
    // actual value or the Integral representation of the
    // actual values of every Name string in a record. 
    // We only store these values in the database.
    typedef std::pair<uint64_t, bool> ValidPair;
    std::vector<ValidPair> valueVector;

    // Vector of the different Name Strings in a record.
    std::vector<std::string> nameVector;

    // Vector of the actual String Values which we need to 
    // lookup while displaying in Argos.
    // If a field value has no string representation, 
    // the enum vector field is empty at that position.
    std::vector<std::string> stringVector;

    // Vector used for Argos Formatting.
    std::vector<std::string> delimVector;

    // The default constructor suffices for this structure.
    // No Move Constructor needed for this structure as there 
    // is no older version of such a structure.
    pair_t() = default;
      
    //! Parameterized Constructor
    pair_t(uint64_t time_Start,
           uint64_t time_End,
           uint64_t parent_ID,
           uint64_t transaction_ID,
           uint32_t location_ID,
           uint16_t flags,
           uint16_t control_Process_ID) :
        transaction_t(
            time_Start,
            time_End,
            parent_ID,
            transaction_ID,
            location_ID,
            flags,
            control_Process_ID) {}
}; // __TRANSACTION_STRUCTURES_H__
#endif
