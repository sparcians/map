
/**
 * \file   Addresstypes.hpp
 *
 * \brief  File that defines typedefs for addressing
 */

#ifndef __ADDRESS_TYPES_H__
#define __ADDRESS_TYPES_H__

#include <inttypes.h>

namespace sparta
{

    /*!
     * \brief Namespace containing memory interfaces, types, and storage objects
     */
    namespace memory
    {
        /**
         * \brief Type for generic address representation in
         * generic interfaces, errors and printouts within SPARTA.
         * */
        typedef uint64_t addr_t;
    }
}

#endif
