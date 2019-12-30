// <PreloadDumpableIF.h> -*- C++ -*-


/**
 * \file PreloadDumpableIF.h
 * \brief Provide an interface for
 *        caches to be able to dump their
 *        preload data.
 */

#ifndef __SPARTA_PRELOAD_DUMPABLE_H__
#define __SPARTA_PRELOAD_DUMPABLE_H__
#include "cache/preload/PreloadEmitter.hpp"
namespace sparta {
namespace cache {

    class PreloadDumpableIF
    {
    public:
        /**
         * \brief This is a method called by a Preloader
         *        that is expected to return a populated
         *        PreloadEmitter with the data for this
         *        cache.
         *
         * This is not virtual and calls the preloadDump_
         * method that is virtual. We could possibly add
         * pre dump steps in this function in the future.
         *
         * \param emitter is a reference to the PreloadEmitter
         *        of which the cache should populate with data.
         */
        void preloadDump(PreloadEmitter& emitter) const
        {
            return preloadDump_(emitter);
        }

    protected:
        /**
         * \brief An implemenation of this method should
         *        new up a PreloadEmitter and populate
         *        it with data of the cache.
         * \param emitter is a reference to the PreloadEmitter
         *        of which the cache should populate with data.
         */
        virtual void preloadDump_(PreloadEmitter& emitter) const = 0;

        /**
         * \brief destructor, to prevent deletion via the base pointer
         */
        virtual ~PreloadDumpableIF() = default;
    };
} //namespace cache
} //namespace sparta
#endif // __SPARTA_PRELOAD_DUMPABLE_H__
