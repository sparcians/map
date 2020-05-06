#pragma once

#include "sparta/pairs/SpartaKeyPairs.hpp"
#include "sparta/pairs/PairCollectorTreeNode.hpp"
namespace sparta{
namespace pevents{

    class PeventCollectorTreeNode : public sparta::PairCollectorTreeNode
    {
        typedef sparta::PairCollectorTreeNode Base;
    public:
        PeventCollectorTreeNode(sparta::TreeNode* parent, const std::string& name) :
            PairCollectorTreeNode(parent, name)
        {}
        // Methods for turning on only specific pevent collector types, and to what file.
        virtual bool addTap(const std::string&, const std::string&, const bool) = 0;
        virtual void turnOff(const std::string&)= 0;
        // Method to officially start the collection process
        virtual void go() = 0;
        virtual const std::string& eventName() = 0;

    };



} // namespace pevents
} // namespace sparta


