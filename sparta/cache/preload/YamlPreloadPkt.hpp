// <YamlPreloadPkt.h> -*- C++ -*-


/**
 * \file YamlPreloadablePkt.h
 *
 * \brief Implement a PreloadPkt
 *        that just wraps a yaml-cpp tree.
 */

#pragma once
#include "cache/preload/PreloadPkt.hpp"
#include <yaml-cpp/yaml.h>
#include <memory>
#include <unordered_map>
#include <utility>
#include <memory>
#include <istream>
namespace sparta {
namespace cache {

    /**
     * \brief Implement a preload pkt that actually wraps the
     *        yaml-cpp tree.
     *
     * We wrap the yaml-cpp tree such that the user can access
     * values in the same consistent interface without copying
     * the tree.
     *
     * \note this cannot be copied and keeps a reference to the yaml-node.
     *       There for the yaml-cpp node should be kept in scope by the parser
     *       when YamlPreloadPkt depend on it.
     */
    class YamlPreloadPkt : public PreloadPkt
    {
    public:
        /**
         * \brief if you've already parsed the yaml
         *        tree you can construct with the node
         */
        YamlPreloadPkt(const YAML::Node & node) :
            PreloadPkt(),
            yaml_node_(node)
        {}

        /**
         * \brief virtual dtor since there are virtual functions
         */
        virtual ~YamlPreloadPkt() {;}

        /**
         * Construct the packet from a istream directly.
         */
        YamlPreloadPkt(std::istream& stream) :
            YamlPreloadPkt(YAML::Load(stream))
        { }

        /**
         * \brief try to print the yaml to key-value store
         *        to the ostream.
         */
        void print(std::ostream& ss) const override
        {
            YAML::Emitter out;
            out << yaml_node_;
            ss << "{" << out.c_str() << "}";
        }

        /**
         * \brief ask the yaml node if it has the key.
         */
        bool hasKey(const std::string& key) const override
        {
            return yaml_node_[key].IsDefined();
        }

    protected:
        /**
         * \brief access scalars at this level.
         */
        std::string getScalarValue_(const std::string& key) const override
        {
            return yaml_node_[key].as<std::string>();
        }

        /**
         * \brief Access a nested PreloadPkt at this level.
         */
        PreloadPkt::NodeHandle getNestedPkt_(const std::string& key) const override
        {
            return PreloadPkt::NodeHandle(new YamlPreloadPkt(yaml_node_[key]));
        }

        /**
         * \brief give the user a list of nodes grabbed using the yaml iterator.
         */
        uint32_t getList_(const std::string& key, NodeList& list) const override
        {
            return getList_(yaml_node_[key], list);

        }

        /**
         * \brief Get the current level node as a list of pkts.
         */
        uint32_t getList_(NodeList& list) const override
        {
            return getList_(yaml_node_, list);
        }

    private:
        /**
         * \brief A helper to build a list of nodes at the level of
         *        node.
         */
        uint32_t getList_(const YAML::Node& node, NodeList& list) const
        {
            for(const auto & n : node)
            {
                list.emplace_back(new YamlPreloadPkt(n));
            }
            return node.size();
        }

        const YAML::Node yaml_node_; //! The node in the yaml tree.
    };

} // namespace cache
} //namespace sparta
