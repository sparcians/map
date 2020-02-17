// <LocationFileWriter.hpp>  -*- C++ -*-

/**
 * \file LocationFileWriter.hpp
 *
 * \brief Contains LocationFileWriter class
 */

#ifndef __LOCATION_FILE_WRITER_H__
#define __LOCATION_FILE_WRITER_H__

#include <iostream>
#include <fstream>
#include <sstream>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/collection/CollectableTreeNode.hpp"
#include "sparta/simulation/TreeNodePrivateAttorney.hpp"
namespace sparta{
namespace argos
{
    /**
     * \brief Object capable of writing a file with Location entries for a given
     * device tree for consumption by the argos viewer
     *
     * File format is a version line:
     * \verbatim
     * <version>
     * \endverbatim
     * (where version is LocationFileWriter::VERSION)
     *
     * followed by any number of single-line entries (1 or more for each node
     * in the simualation) having the form:
     * \verbatim
     * <node_uid_int>,<node_location_string>,<clock_uid_int>\n
     * \endverbatim
     * Note that the newline (\n) will be present on every line
     *
     * For nodes with no clock association, NO_CLOCK_ID is used as \a clock_id
     *
     * Lines beginning with '#' as the first character are comments
     *
     * Multiple entries can be written for the same node if that node has
     * aliases or group information. This is intended to make that node
     * easier to identify in the viewer. However, this is only done for
     * the rightmost object in each entry location string. Ancestor
     * nodes of the node whose entry is being writtenwill be printed using the
     *
     * \todo Support all alias and group identifiers for each node in a
     * location, not just the last (rightmost) object in each location
     * printed
     * \todo Only write locations being collected to the file
     */
    class LocationFileWriter
    {
        /*!
         * \brief ID to write to disk when a location has no associated clock.
         */
        const int NO_CLOCK_ID = -1;

    public:

        /*!
         * \brief Default Constructor
         * \param prefix Prefix of file to which data will be written.
         */
        LocationFileWriter(const std::string& prefix,
                           const std::string& fn_extension = "location.dat",
                           const uint32_t fmt_version = 1) :
            filename_(prefix + fn_extension),
            file_(filename_, std::ios::out)
        {
            if(!file_.is_open()){
                throw sparta::SpartaException("Failed to open location file \"")
                    << filename_ << "\" for write";
            }

            // Throw on write failure
            file_.exceptions(std::ostream::eofbit | std::ostream::badbit | std::ostream::failbit | std::ostream::goodbit);

            file_ << fmt_version << " # Version Number" << std::endl;
        }

        /*!
         * \brief Writes content of an entire tree with the given root to this
         * location file.
         * \param root Root node of tree to write to the file
         * \warning There is no need to write each node individually.
         * \warnings Any nodes added more than once will cause repeat entries
         */
        LocationFileWriter& operator<<(const sparta::TreeNode& root) {
            recursWriteNode_(&root);
            file_.flush();
            return *this;
        }

    private:

        /*!
         * \brief Writes an 1-line entry for 1 node using the given location
         * string (which might not math node->getLocation)
         * \param node Node to write an entry for. UID and clock association are
         * extracted from this
         * \param location Location string to write to file. This should
         * identify the node but might be an alias, the actual name, or a
         * group[index] string. Must be fully qualified (e.g.
         * top.node0.node1.thisnode)
         */
        void writeNodeEntry_(const sparta::TreeNode* node, const std::string& location) {
            sparta_assert(node != nullptr);

            file_ << node->getNodeUID() << ',' << location << ',';

            const sparta::Clock* clk = node->getClock();
            if(clk){
                file_ << clk->getNodeUID();
            }else{
                file_ << NO_CLOCK_ID; // No clock
            }

            file_ << "\n";
        }

        /*!
         * \brief Recursively writes entries for all nodes in the subtree
         * including this \a node in pre-order
         * \param node Node for which entries should be written. Children will
         * then be iterated.
         * \post May write any number of lines to the output file depending on
         * number of nodes and each node's name, group info, and aliases.
         */
        void recursWriteNode_(const sparta::TreeNode* node) {
            sparta_assert(node != nullptr);

            // Write node only if it is a collectable
            if(dynamic_cast<const collection::CollectableTreeNode*>(node)){

                // Compute a valid location representing the parent
                std::string parent_loc;
                if(node->getParent() != nullptr){
                    parent_loc = node->getParent()->getDisplayLocation();
                }

                // Write Node with its given name string
                if(node->getName() != ""){
                    writeNodeEntry_(node, node->getLocation());
                }

                // Write node with its group info if appropriate: "group_name[idx]"
                if(node->getGroup() != "" && node->getGroup() != sparta::TreeNode::GROUP_NAME_BUILTIN) {
                    std::stringstream group_el_ident;
                    group_el_ident << parent_loc
                                   << sparta::TreeNode::LOCATION_NODE_SEPARATOR_ATTACHED
                                   << node->getGroup() << "[" << node->getGroupIdx() << "]";

                    writeNodeEntry_(node, group_el_ident.str());
                }

                // Write node with each alias this node has
                for(const std::string& alias : node->getAliases()){
                    std::stringstream alias_ident;
                    alias_ident << parent_loc
                                << sparta::TreeNode::LOCATION_NODE_SEPARATOR_ATTACHED
                                << alias;

                    writeNodeEntry_(node, alias_ident.str());
                }
            } // if(dynamic_cast<const CollectableTreeNode*>(node))

            // Recurse into children regardless of whether this node was collectable
            for(const sparta::TreeNode* child : sparta::TreeNodePrivateAttorney::getAllChildren(node)){
                recursWriteNode_(child);
            }
        }

        /*!
         * \brief Filename with which this file was actually opened (includes
         * prefix given at construction)
         */
        std::string filename_;

        /*!
         * \brief File to which location data will be written.
         */
        std::ofstream file_;

    }; // class LocationFileWriter

}//namespace argos
}//namespace sparta

// __LOCATION_FILE_WRITER_H__
#endif
