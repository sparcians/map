// <ConfigEmitter> -*- C++ -*-

#ifndef __CONFIG_EMITTER_H__
#define __CONFIG_EMITTER_H__

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"


namespace sparta
{
    /*!
     * \brief Namespace containing all config-file emitters
     */
    namespace ConfigEmitter
    {

        /*!
         * \brief Base class for configuration emitted objects
         */
        class ConfigEmitter
        {
        public:

            /*!
             * \brief Constructor
             * \param filename Config file to write
             */
            ConfigEmitter(const std::string& filename)
            {
                (void) filename;
            }
        };
    }
} // namespace sparta

// __CONFIG_EMITTER_H__
#endif
