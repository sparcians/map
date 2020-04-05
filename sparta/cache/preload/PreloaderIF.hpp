// <PreloaderIF.h> -*- C++ -*-


/**
 * \file PreloadableIF.h
 *
 * \brief File that defines Preloadable.
 */

#pragma once
#include <fstream>
#include <yaml-cpp/yaml.h>
#include "sparta/simulation/TreeNode.hpp"
#include "cache/preload/PreloadPkt.hpp"
#include "cache/preload/YamlPreloadPkt.hpp"
#include "cache/preload/PreloadEmitter.hpp"
#include "cache/preload/PreloadableIF.hpp"
#include "cache/preload/PreloadDumpableIF.hpp"
#include "sparta/simulation/TreeNodePrivateAttorney.hpp"
namespace sparta {
namespace cache {

    /**
     * \class PreloaderIF
     * \brief An interface to read the preload data and pass the PreloadPkts
     *        to the appropriate cache.
     *
     *  Implementations of this interface are probably aware of the
     *  architecture and make the appropriate Preloadable::preloadLine calls to the
     *  caches (after finding the caches in the tree) with the data parsed in.
     *
     *  It is recommended that implmementations of the PreloaderIF live in
     *  the sparta tree, such that the preload files can be passed in via a sparta
     *  parameter. This likely means making the implementation a sparta::Resource
     *  or sparta::Unit. This way they can be created via resource factories. An
     *  example in the example/CoreModel shows this.
     *
     */
    class PreloaderIF
    {
    protected:
        /**
         * \brief A helper method that implementations can use to
         *        parse a yaml file and call acceptPkt on the preloader.
         *
         * This would likely be called from the constructor of your
         * Preloader implementation.
         */
        void parseYaml_(const std::string& filepath)
        {
            std::ifstream yamlstream;
            yamlstream.open(filepath, std::ios::in);
            if (!yamlstream)
            {
                sparta::SpartaException ex("Failed to open preload yaml: \"");
                ex << filepath << "\"";
                throw ex;
            }
            YAML::Node doc = YAML::Load(yamlstream);
            // Iterate the yaml file or files, and populate a PreloadPkt
            // per top level entry in the file. I.e. for each treenode
            // path there should be a dictionary to use the the PreloadPkt
            // data.
            // Traverse each treenode specified.
            for(const auto & n : doc)
            {
                std::string treenode_path(n.first.as<std::string>());
                YamlPreloadPkt pkt(n.second);
                preloadPacket(treenode_path, pkt);
            }
            yamlstream.close();
        }



        /**
         * \brief A helper utility that can be invoked by the Preloader
         *        implementation to dump any preloadables using the PreloadEmitter
         *        to the out stream.
         * \param the treenode for which to start searching under. This is likely
         *        the root.
         * \param an outstream to dump the yaml to.
         */
        void dumpPreloadTree_(sparta::TreeNode* node, std::ostream& out) const
        {
            sparta::cache::PreloadEmitter emitter;
            emitter << PreloadEmitter::BeginMap;
            dumpRecursor_(node, emitter);
            emitter << PreloadEmitter::EndMap;
            out << emitter;
        }

        /**
         * \brief Implement this method to pass the pkt to the appropriate
         * preloadable caches.
         * \param treenode the string path to the treenode following common
         *        treenode naming conventions. This could be for example
         *        top.core0.lsu.l1cache OR it could be something like
         *        top.core*.preload_helper. The idea is the pkt should go to
         *        this treenode though. But it does not necessarily need to be
         *        a cache depending on your model.
         * \param pkt the populated packet.
         *
         */
        virtual void preloadPacket_(const std::string& treenode, PreloadPkt& pkt) = 0;
    public:
        /**
         * \brief Construct the PreloaderIF with the preload data.
         *
         * Implementations would likely call one of the parse help functions
         * like parseYaml_() during construction or add their own preload
         * method to call during the bind stage.
         */
        PreloaderIF()
        {}

        virtual ~PreloaderIF() {}
        /**
         * \brief preload a packet and pass it to the required
         *        preloadable objects that should also share in the pkt's
         *        knowledge.
         *
         * \todo implement warnings such that all values of the pkt are
         *       read.
         */
        void preloadPacket(const std::string& treenode, PreloadPkt& pkt)
        {
            preloadPacket_(treenode, pkt);
        }
    private:
        /**
         * \brief A helper to the dumpPreloadTree_ function to actual
         *        handle the recursion.
         * \param node the treenode for which to search under.
         * \param emitter the PreloadEmitter to dump the yaml to.
         */
        void dumpRecursor_(sparta::TreeNode* node, sparta::cache::PreloadEmitter& emitter) const
        {
            PreloadDumpableIF* preloadable = dynamic_cast<PreloadDumpableIF*>(node);
            if (preloadable)
            {
                std::cout << " (" << *node << ") -> Dumping preload information" << std::endl;
                emitter << PreloadEmitter::Key << node->getLocation();
                emitter << PreloadEmitter::Value;
                preloadable->preloadDump(emitter);
                // Go ahead and make sure the node we just dumped
                // did not dump out invalid data.
                emitter.assertValid(node->getLocation());

            }
            else
            {
                for (sparta::TreeNode* c : TreeNodePrivateAttorney::getAllChildren(node))
                {
                    dumpRecursor_(c, emitter);
                }
            }
        }

    };

} // namespace cache

} // namespace sparta

