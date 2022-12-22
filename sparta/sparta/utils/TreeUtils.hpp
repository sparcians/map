// <TreeUtils.h> -*- C++ -*-


/**
 * \file   TreeUtils.hpp
 *
 * \brief  File that defines hand sparta::TreeNode functions
 */

#pragma once

#include <string>
#include <cinttypes>
#include <vector>

#include "sparta/simulation/TreeNode.hpp"

namespace sparta::utils
{
    /**
     * \brief Search a TreeNode for a given named node
     *
     * @param starting_node The TreeNode to start the search.  It's included in the search
     * @param name The TreeNode name to search for
     * @param RETURNED found_nodes Populated vector of nodes
     * @return Number of matches
     *
     * An expensive, but thorough search starting at the given
     * TreeNode for the nodes matching the given name.  This method
     * does _not_ do pattern matching.
     *
     * Note that the starting_node is _not_ `const` as the intention
     * of this call is to find those TreeNodes that might be modified
     */
    uint64_t recursiveTreeSearch(sparta::TreeNode * starting_node,
                                 const std::string & name,
                                 std::vector<sparta::TreeNode*> & found_nodes)
    {
        // XXXX check for dots
        if(starting_node->getName() == name) {
            found_nodes.emplace_back(starting_node);
        }
        else {
            for(auto child : starting_node->getChildren()) {
                recursiveTreeSearch(child, name, found_nodes);
            }
        }
        return found_nodes.size();
    }
}
