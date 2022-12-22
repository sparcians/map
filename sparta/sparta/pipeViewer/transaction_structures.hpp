/**
 * \file transaction_structures.hpp
 *
 */

#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "sparta/pairs/PairFormatter.hpp"

#define is_Annotation 0x1
#define is_Instruction 0x2
#define is_MemoryOperation 0x3
#define is_Pair 0x4
#define TYPE_MASK 0x7//!< Mask used for extracting type ID portion from transaction flags
#define CONTINUE_FLAG 0x10 //!< Flag used to indicate if this transaction should be considered a continuation of the previous transaction

static constexpr uint64_t BAD_DISPLAY_ID = 0x1000;

static constexpr std::string_view HEADER_PREFIX = "sparta_pipeout_version:";
static constexpr int VERSION_LENGTH = 4;
static constexpr size_t HEADER_SIZE = HEADER_PREFIX.size() + VERSION_LENGTH + 1; // prefix + number + newline

/*!
 * \brief Generic transaction event, packed for density on disk
 * \warning Since this is written as a chunk, it is endian-dependent and
 * possibly compiler dependent
 * \todo Should consider removing packing attribute since it is mainly related
 * to serialization. Create custom packed serialization code instead.
 */
struct __attribute__ ((aligned(8))) transaction_t  {
    uint64_t time_Start = 0;  //! Event Start Time   8 Bytes
    uint64_t time_End = 0;  //! Event End Time     8 Bytes
    uint64_t parent_ID = 0;  //! Parent Transaction ID 8 Bytes
    uint64_t transaction_ID = 0;  //! Transaction ID     8 Bytes

    // Any value above 0x0fff is an invalid value for this field
    uint64_t display_ID = BAD_DISPLAY_ID;      //! Display ID         8 Bytes

    uint32_t location_ID = 0;  //! Location           4 Bytes
    uint16_t flags = 0;  //! Flags/Trans Type   2 Bytes
    uint16_t control_Process_ID = 0;  //! Control Process ID 2 Bytes

    transaction_t() = default;

    //! Parameterized Constructor
    transaction_t(uint64_t time_Start, uint64_t time_End, uint64_t parent_ID,
                  uint64_t transaction_ID, uint64_t display_ID, uint32_t location_ID, uint16_t flags,
                  uint16_t control_Process_ID) :
        time_Start(time_Start), time_End(time_End), parent_ID(parent_ID),
        transaction_ID(transaction_ID), display_ID(display_ID), location_ID(location_ID), flags(flags),
        control_Process_ID(control_Process_ID) {}

};

// Instruction Event
struct instruction_t : public transaction_t {
    uint32_t operation_Code = 0;  //! Operation Code        4 Bytes
    uint64_t virtual_ADR = 0;  //! Virtual Address       8 Bytes
    uint64_t real_ADR = 0;  //! Real Address          8 Bytes

    instruction_t() = default;
};

// Memory Operation Event
struct memoryoperation_t : public transaction_t {
    uint64_t virtual_ADR = 0;  //! 8 Bytes
    uint64_t real_ADR = 0;  //! 8 Bytes

    memoryoperation_t() = default;
};

// Annotation Event (Catch-All)
struct annotation_t : public transaction_t {
    uint16_t length = 0;  //! Annotation Length 2 Bytes
    std::string annt; //! Pointer to Annotation Start

    annotation_t() = default;

    explicit annotation_t(transaction_t&& rhs) :
        transaction_t(std::move(rhs))
    {
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
    using IntT = uint64_t;
    using ValidPair = std::pair<IntT, bool>;
    std::vector<ValidPair> valueVector;

    // Vector of the different Name Strings in a record.
    std::vector<std::string> nameVector;

    // Vector of the actual String Values which we need to
    // lookup while displaying in pipeViewer
    // If a field value has no string representation,
    // the enum vector field is empty at that position.
    std::vector<std::string> stringVector;

    sparta::PairFormatterVector delimVector;

    // The default constructor suffices for this structure.
    // No Move Constructor needed for this structure as there
    // is no older version of such a structure.
    pair_t() = default;

    explicit pair_t(transaction_t&& rhs) :
        transaction_t(std::move(rhs))
    {
    }

    //! Parameterized Constructor
    pair_t(const uint64_t time_Start,
           const uint64_t time_End,
           const uint64_t parent_ID,
           const uint64_t transaction_ID,
           const uint64_t display_ID,
           const uint32_t location_ID,
           const uint16_t flags,
           const uint16_t control_Process_ID) :
        transaction_t(
            time_Start,
            time_End,
            parent_ID,
            transaction_ID,
            display_ID,
            location_ID,
            flags,
            control_Process_ID) {}
};
