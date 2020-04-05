// <PreloadableIF.h> -*- C++ -*-



/**
 * \file PreloadableIF.h
 *
 * \brief File that defines Preloadable.
 */

#pragma once

#include <string>
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/log/MessageSource.hpp"
#include "cache/preload/PreloadPkt.hpp"
namespace sparta {
namespace cache {

    /**
     * \class PreloadableIF
     * \brief Interface that provides an API for caches
     *        to preload their lines.
     */
    class PreloadableIF
    {
    public:
        /**
         * \brief PreloadableIF constructor.
         * No logging is enabled.
         */ 
        PreloadableIF()
        {}

        /**
         * \brief PreloadableIF constructor that will
         *        enable logging.
         * This constructor requires a treenode such that it
         * can create the appropriate logger.
         *
         * You should prefer to use this constructor so you get
         * free logging support of which packets were loaded.
         */
        PreloadableIF(sparta::TreeNode* node) :
            logger_(new sparta::log::MessageSource(node, "preload_logger",
                                                 "Log all preload pkts"))
        {}
                    
        virtual ~PreloadableIF() {}
        /**
         * \brief This method is called by a Preloader
         * to load lines into the cache.
         * This method will also log the preload.
         *
         * \return false if the data was not preloaded
         *         for some reason.
         */ 
        bool preloadPkt(PreloadPkt& data)
        {
            if (SPARTA_EXPECT_FALSE(logger_.get() != nullptr) &&
                SPARTA_EXPECT_FALSE(*logger_))
            {
                std::stringstream s;
                data.print(s);
                *logger_ << "Preloading data: " << s.str();
            }
            return preloadPkt_(data); 
        }


    protected:
        
        /**
         * \brief An implementation of this method should
         * actually load the data packet into a line in the cache.
         *
         * Should return false if the data was not preloaded
         * for some reason. 
         */ 
        virtual bool preloadPkt_(PreloadPkt& data) = 0;

    private:
        //! A possible logger to use.
        std::unique_ptr<sparta::log::MessageSource> logger_;
    };

} // namespace cache

} // namespace sparta

