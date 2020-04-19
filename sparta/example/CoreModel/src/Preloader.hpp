// <Preloader.hpp> -*- C++ -*-

#pragma once

#include "sparta/simulation/Resource.hpp"
#include "sparta/simulation/RootTreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"

#include "cache/preload/PreloaderIF.hpp"

namespace core_example
{
    /**
     * \class Preloader
     * \brief implement a PreloaderIF with appropriate
     *        knowledge of how to preload yaml files into
     *        the lsu's L1 cache.
     *
     * Some models will need more rigorous knowledge of the architecture
     * such as knowing that preloads to l1 must also preload into the l2
     * and notifying other relevant parts.
     *
     * This preloader is a resource such that it has the
     * ability to define parameter sets which may be useful
     * when implementing a preloader in your model.
     */
    class Preloader : public sparta::Resource,
                      public sparta::cache::PreloaderIF
    {
    public:
        static constexpr char name[] = "preloader";
        class PreloaderParameterSet : public sparta::ParameterSet
        {
        public:
            PreloaderParameterSet(sparta::TreeNode* n) :
                sparta::ParameterSet(n)
            {}
            PARAMETER(std::string, preload_file, "", "The path to the yaml file with preload data")
        };

        Preloader(sparta::TreeNode* node, const PreloaderParameterSet* params) :
            sparta::Resource(node),
            sparta::cache::PreloaderIF(),
            filepath_(params->preload_file)
        {}
        virtual ~Preloader() = default;
        /**
         * Start the preload process. Should be called in the simulators
         * bind setup.
         */
        void preload();
    private:
        /**
         * override the method which is called for each packet that is parsed
         * in from the parsers to be preloaded.
         */
        void preloadPacket_(const std::string& treenode,
                            sparta::cache::PreloadPkt& pkt) override;
        //! Keep track of the params.
        const std::string filepath_; //! The path to the yaml file.
    };

}

