// <PreloadableNode.h> -*- C++ -*-



/**
 * \file PreloadableNode.h
 *
 * \brief File that defines PreloadableNode.
 */
#pragma once
#include <functional>
#include "cache/preload/PreloadableIF.hpp"
#include "cache/preload/PreloadDumpableIF.hpp"

namespace sparta {
namespace cache {

    /**
     * \class PreloadableNode
     * \brief a PreloadableIF that is also a Treenode.
     *
     * The PreloadableNode forces that the TreeNode name is preloadable
     * to be consistent, and implements logging for every preloadPkt call.
     *
     * Calls are logged to the preload_logger log type.
     *     * You can register a callback function to be invoked by the
     * preloadPkt_ function using the appropriate constructor or
     * you can construct without a callback. This should only be
     * done if you are inheriting from PreloadableNode and have
     * reimplemented the preloadPkt_ method.
     */
    class PreloadableNode : public sparta::TreeNode,
                            public PreloadableIF,
                            public PreloadDumpableIF
    {
    public:
        //! The typedef for the functor type we expect to use
        //! as a callback for preloading.
        typedef std::function<bool(PreloadPkt&)> CallbackFunc;
        //! The typedef for the functor type we expect to use
        //! as a callback for dumping preload data.
        typedef std::function<void(PreloadEmitter&)> DumpFunc;

        /**
         * \brief Construct a PrelodableIFNode that calls a particular
         *        preload callback when receiving a PreloadPkt.
         * \param parent the parent treenode.
         * \param an std function that returns a bool and accepts a
         *        PreloadPkt& as the 1 argument. This will be called
         *        by this node's preloadPkt_ method.
         */        
        PreloadableNode(TreeNode* parent,
                        CallbackFunc preload_cb,
                        DumpFunc dump_cb) :
            sparta::TreeNode(parent, "preloadable", "A preloadable node"),
            PreloadableIF(this),
            PreloadDumpableIF(),
            preload_callback_(preload_cb),
            dump_callback_(dump_cb)
        {}

        virtual ~PreloadableNode() {}
    protected:
        /**
         * \brief Construct a PreloadableNode without a callback.
         *        This should only be used when inheriting from
         *        PreloadableNode and overriding the preloadPkt_
         *        method.
         * \param parent the parent treenode.
         */        
        PreloadableNode(TreeNode* parent) :
            PreloadableNode(parent, nullptr, nullptr)
        {}
        /**
         * \brief An implementation of this method should
         * actually load the data packet into a line in the cache.
         *
         * Should return false if the data was not preloaded
         * for some reason. 
         */ 
        virtual bool preloadPkt_(PreloadPkt& data) override
        {
            sparta_assert (preload_callback_ != nullptr);
            return preload_callback_(data);
        }

        /**
         * \brief an implementation of this method should
         *        dump the contents of the cache into the emitter
         */
        virtual void preloadDump_(PreloadEmitter& emitter) const override
        {
            sparta_assert(dump_callback_ != nullptr);
            dump_callback_(emitter);
        }
    private:
        CallbackFunc preload_callback_;
        DumpFunc dump_callback_;

    };
} // namespace cache
} //namespace sparta
