
/**
 * \file   CheckpointExceptions.hpp
 * \brief  File that contains checkpoint exception types
 */

#pragma once

#include "sparta/utils/SpartaException.hpp"
#include "sparta/memory/AddressTypes.hpp"

namespace sparta {
namespace serialization {
namespace checkpoint
{
    /*!
     * \brief Indicates that there was an issue operating on checkpoints
     * within the SPARTA framework.
     *
     * This is intended to communicate to a (typically external) client of
     * the framework that a problem ocurred with checkpointing.
     */
    class CheckpointError : public SpartaException
    {
    public:
        CheckpointError() :
            SpartaException()
        { }

        CheckpointError(const std::string& why) :
            SpartaException(why)
        { }
    };

} // namespace checkpoint
} // namespace memory
} // namespace sparta

