// <PeventTrigger> -*- C++ -*-

/**
 * \file PeventTrigger
 *
 */
#pragma once
#include "sparta/trigger/Triggerable.hpp"
#include "sparta/pevents/PeventTreeNode.hpp"
#include "sparta/simulation/TreeNodePrivateAttorney.hpp"
namespace sparta
{
    using namespace pevents;
namespace trigger
{

/**
 * \class PeventTrigger
 * \brief A simple interface with 2 methods.
 *
 * Classes should inherit from this interface and override
 * the go() and stop() methods to be compatible with sparta::Triggers
 */
class PeventTrigger : public Triggerable
{

public:
    PeventTrigger(TreeNode* root) :
        Triggerable(),
        root_(root)
    {}
    virtual ~PeventTrigger(){}

    // Recursively go through and finaly start collectors.
    // turnOn's for all collectors should of already been done.
    virtual void go() override
    {
        go_(root_);
    }
    //!The method called when a trigger fires a turn off.
    virtual void stop() override {}
    //!The method to call on periodic repeats of the trigger.
    virtual void repeat() override {}
private:

    void go_(TreeNode* root)
    {
        PeventCollectorTreeNode* c_node = dynamic_cast<PeventCollectorTreeNode*>(root);
        if(c_node)
        {
            c_node->go();
        }
        for(TreeNode* node : TreeNodePrivateAttorney::getAllChildren(root))
        {
            go_(node);
        }
    }
    TreeNode* root_;
};

    }//namespace trigger
}//namespace sparta

