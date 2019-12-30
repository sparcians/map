// <RootTreeNode> -*- C++ -*-


/*!
 * \file RootTreeNode.cpp
 * \brief Implementation of RootTreeNode
 */

#include "sparta/simulation/RootTreeNode.hpp"

#include <algorithm>
#include <sstream>

#include "sparta/simulation/Resource.hpp"
#include "sparta/functional/ArchData.hpp"
#ifdef SPARTA_PYTHON_SUPPORT
#include "python/sparta_support/PythonInterpreter.hpp"
#endif

namespace sparta {

void RootTreeNode::validatePostRun() {
    PostRunValidationInfo info;
    validatePostRun_(info);
}

void RootTreeNode::getArchDataAssociations(std::vector<const ArchData*>& ad_this_tree,
                                     std::vector<const ArchData*>& ad_no_assoc,
                                     std::vector<const ArchData*>& ad_not_attached,
                                     std::vector<const ArchData*>& ad_other_tree) const noexcept {
    ad_this_tree.clear();
    ad_no_assoc.clear();
    ad_not_attached.clear();
    ad_other_tree.clear();

    // Sort into lists
    for(const ArchData* ad : ArchData::getAllArchDatas()){
        const TreeNode* on = ad->getOwnerNode();
        if(!on){
            ad_not_attached.push_back(ad);
        }else{
            const TreeNode* r = on->getRoot();
            if(r == this){
                ad_this_tree.push_back(ad);
            }else if(on->isAttached() == false){
                ad_not_attached.push_back(ad);
            }else{
                ad_other_tree.push_back(ad);
            }
        }
    }
}

void RootTreeNode::validateArchDataAssociations() const {
    std::vector<const ArchData*> ad_this_tree;
    std::vector<const ArchData*> ad_no_assoc;
    std::vector<const ArchData*> ad_not_attached;
    std::vector<const ArchData*> ad_other_tree;

    getArchDataAssociations(ad_this_tree,
                            ad_no_assoc,
                            ad_not_attached,
                            ad_other_tree);

    std::stringstream err;
    if(ad_no_assoc.size() > 0){
        err << " Found " << ad_no_assoc.size()
            << " ArchDatas not associated with any TreeNode at all. A checkpointer has no "
               "way of finding these ArchDatas. TreeNode::associateArchData() is required "
               "to associate an ArchData with a TreeNode in the tree that is to be "
               "checkpointed\n";
    }

    if(ad_not_attached.size() > 0){
        err << " Found " << ad_not_attached.size()
            << " ArchDatas associated with one or more TreeNodes which was not attached to "
               "any RootTreeNode\n";
    }

    if(err.str().size() > 0){
        std::stringstream ss;
        dumpArchDataAssociations(ss);
        throw SpartaException("Errors found while validating ArchData associations with tree "
                            "of RootTreeNode: ")
            << this->stringize() << ":\n" << err.str() << "\nAssociations:\n"
            << ss.str();
    }
}

void RootTreeNode::dumpArchDataAssociations(std::ostream& o) const noexcept {
    std::vector<const ArchData*> ad_this_tree;
    std::vector<const ArchData*> ad_no_assoc;
    std::vector<const ArchData*> ad_not_attached;
    std::vector<const ArchData*> ad_other_tree;

    getArchDataAssociations(ad_this_tree,
                            ad_no_assoc,
                            ad_not_attached,
                            ad_other_tree);

    // Write results
    o << "ArchDatas associated with nodes in this Tree: " << std::endl;
    for(auto ad : ad_this_tree){
        o << "  " << ad << " -> "
          << ad->getOwnerNode()->getLocation() << std::endl;
        auto adatas = ad->getOwnerNode()->getAssociatedArchDatas();
        if(std::find(adatas.begin(), adatas.end(), ad) == adatas.end()){
            o << "     Node does not know about this ArchData: " << ad
              << ", points to " << adatas.size() << " others";
        }
    }

    o << "\nArchDatas not associated with any TreeNode: " << std::endl;
    for(auto ad : ad_no_assoc){
        o << "  " << ad << " -> "
          << "nullptr" << std::endl;
    }

    o << "\nArchDatas associated with a node which is not attached to a tree: " << std::endl;
    for(auto ad : ad_not_attached){
        o << "  " << ad << " -> "
          << ad->getOwnerNode()->getLocation() << std::endl;
        auto adatas = ad->getOwnerNode()->getAssociatedArchDatas();
        if(std::find(adatas.begin(), adatas.end(), ad) == adatas.end()){
            o << "     Node does not know about this ArchData: " << ad
              << ", points to " << adatas.size() << " others";
        }
    }

    o << "\nArchDatas associated with a node in another tree: " << std::endl;
    for(auto ad : ad_other_tree){
        o << "  " << ad << " -> "
          << ad->getOwnerNode()->getLocation() << " -> ... -> "
          << ad->getOwnerNode()->getRoot() << std::endl;
        auto adatas = ad->getOwnerNode()->getAssociatedArchDatas();
        if(std::find(adatas.begin(), adatas.end(), ad) == adatas.end()){
            o << "     Node does not know about this ArchData: " << ad
              << ", points to " << adatas.size() << " others";
        }
    }
}

void RootTreeNode::dumpTypeMix(std::ostream& o) const {
    o << "UNIMPLEMENTED: " << __PRETTY_FUNCTION__ << std::endl;
}

void RootTreeNode::enterFinalized(sparta::python::PythonInterpreter* pyshell) {
    if(getPhase() != TREE_CONFIGURING){
        throw SpartaException("Device tree with root \"")
            << getLocation()
            << "\" not in the TREE_CONFIGURING phase, so it cannot be finalized yet";
    }

    enterFinalizing_(); // Enter the next phase (cannot throw)

    finalizeTree_(); // Do the finalization, which may throw

#ifdef SPARTA_PYTHON_SUPPORT
    if (pyshell) {
        //! Publish the partially built tree and the simulator so that resource nodes
        //  can be added or removed from the shell
        pyshell->interact();
        if (pyshell->getExitCode() != 0) {
            throw SpartaException("Python shell exited with non-zero exit code: ")
                << pyshell->getExitCode();
        }
    }
#else
    (void) pyshell;
#endif

    enterFinalized_(); // Enter the next phase (cannot throw)
}

} // namespace sparta
