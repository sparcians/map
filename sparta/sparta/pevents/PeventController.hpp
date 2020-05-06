#pragma once

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/pevents/PeventCollector.hpp"
#include "sparta/simulation/TreeNodePrivateAttorney.hpp"
namespace sparta{
namespace pevents{
    /**
     * \brief Simple class for recursively adding taps to pevents. There is probably a more
     * efficient way than each collector having it's own tap... But I don't think there
     * will be many taps at all, at max 10 for any run I imagine..
     *
     * This controller provides support for storing tap information during
     * command line parsing that
     * cannot be created until later when the sparta tree has been setup.
     */
    class PeventCollectorController
    {
        /**
         * \brief a struct that holds necessary information for
         * adding taps to pevents, but this information is cached before
         * a treenode is present.
         */
        struct CachedTapData
        {
            CachedTapData(const std::string& tree_dest,
                          const std::string& f,
                          const std::string& type,
                          bool is_verbose) :
                file(f),
                event_type(type),
                treenode_dest(tree_dest),
                verbose(is_verbose)
            {}
            std::string file;
            std::string event_type;
            std::string treenode_dest;
            bool verbose;
        };
    public:

        /**
         * \brief Add knowledge of a tap that will need to be created later, right
         * now we are only parsing the command line likely.
         */
        void cacheTap(const std::string& file, const std::string& type, const bool verbose,
                      const std::string& node="ROOT")
        {
            tap_info_.emplace_back(CachedTapData(node, file, type, verbose));
        }

        /**
         * \brief we are done parsing the command line, and simulation is setup, propagate the
         * tap information throug the tree.
         */
        void finalize(RootTreeNode* root)
        {
            for(auto& tap_data : tap_info_)
            {
                uint32_t count_added = 0;
                if(tap_data.treenode_dest != "ROOT")
                {
                    std::vector<TreeNode*> results;
                    root->getSearchScope()->findChildren(tap_data.treenode_dest, results);
                    for(TreeNode* node : results) {
                        addTap_(node, tap_data.event_type, tap_data.file, tap_data.verbose, count_added);
                    }
                }
                else {
                    addTap_(root, tap_data.event_type, tap_data.file, tap_data.verbose, count_added);
                }
                // assert that we at least turned on one pevent other wise the user may be bummed out
                // when their run actually finishes.
                if (count_added == 0)
                {
                    std::stringstream s;
                    s << "No pevents were actually enabled for the pevent " << tap_data.event_type << ". You likely supplied an invalid event type to the command line. ";
                    throw SpartaException(s.str());
                }



            }
        }

        //! Print out a nice listing of the pevents that we have in the model.
        void printEventNames(std::ostream& o, TreeNode* root)
        {
            o << "<TreeNode Path> : Event Name" << std::endl;
            printEventNames_(o, root);

        }

    private:
        //! Actually do the printing of event names.
        void printEventNames_(std::ostream& o, TreeNode* root)
        {
            for(TreeNode* node : TreeNodePrivateAttorney::getAllChildren(root))
            {
                PeventCollectorTreeNode* c_node = dynamic_cast<PeventCollectorTreeNode*>(node);
                if (c_node)
                {
                    o << c_node->stringize() << " : " << c_node->eventName() << std::endl;
                }
                printEventNames_(o, node);
            }
        }

        // Add a tap to every collector below root recursively, where the pevent type is the same.
        bool addTap_(TreeNode* root, const std::string& type, const std::string& file, const bool verbose,
                     uint32_t& count_added)
        {
            PeventCollectorTreeNode* c_node = dynamic_cast<PeventCollectorTreeNode*>(root);
            if(c_node)
            {
                if(c_node->addTap(type, file, verbose))
                {
                    count_added = count_added + 1; // We successfully added a tap.
                }
            }
            for(TreeNode* node : TreeNodePrivateAttorney::getAllChildren(root))
            {
                addTap_(node, type, file, verbose, count_added);
            }

            return true;
        }
        std::vector<CachedTapData> tap_info_;
    };

}// namespace pevents
}// namspace sparta

