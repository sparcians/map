#pragma once

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/collection/CollectionPoints.hpp"

namespace sparta {
namespace collection {

class CollectionPoints;

class CollectableTreeNode : public TreeNode
{
public:
    CollectableTreeNode(TreeNode* parent, const std::string& name, const std::string& desc = "CollectableTreeNode <no desc>")
        : TreeNode(parent, name, desc)
    {
        markHidden();
    }

    virtual void addCollectionPoint(CollectionPoints & collection_points) = 0;
};

template <typename CollectableT>
class Collectable : public CollectableTreeNode
{
public:
    using value_type = typename MetaStruct::remove_any_pointer<CollectableT>::type;

    Collectable(TreeNode* parent, const std::string& name, const CollectableT* collectable, const std::string& desc = "Collectable <no desc>")
        : CollectableTreeNode(parent, name, desc)
        , collectable_(collectable)
    {
    }

    void addCollectionPoint(CollectionPoints & collection_points) override
    {
        auto is_pod = std::integral_constant<bool, MetaStruct::is_pod<value_type>::value>();
        addCollectionPoint_(collection_points, is_pod);
    }

private:
    void addCollectionPoint_(CollectionPoints & collection_points, std::true_type)
    {
        collection_points.addStat(getLocation(), getClock(), collectable_);
    }

    void addCollectionPoint_(CollectionPoints & collection_points, std::false_type)
    {
        // TODO cnyce
        (void)collection_points;
    }

    const CollectableT* collectable_;
};

template <typename ContainerT, bool Sparse=false>
class IterableCollector : public CollectableTreeNode
{
public:
    IterableCollector(TreeNode* parent, const std::string& name, const ContainerT* container, const size_t capacity, const std::string& desc = "IterableCollector <no desc>")
        : CollectableTreeNode(parent, name, desc)
        , container_(container)
        , capacity_(capacity)
    {
        for (size_t i = 0; i < capacity_; ++i) {
            bins_.emplace_back(new IterableCollectorBin(this, name + std::to_string(i)));
        }
    }

    void addCollectionPoint(CollectionPoints & collection_points) override
    {
        collection_points.addContainer<ContainerT, Sparse>(getLocation(), getClock(), container_, capacity_);
    }

private:
    class IterableCollectorBin : public TreeNode
    {
    public:
        IterableCollectorBin(TreeNode* parent, const std::string& name, const std::string& desc = "IterableCollectorBin <no desc>")
            : TreeNode(parent, name, desc)
        {
            markHidden();
        }
    };

    const ContainerT* container_;
    const size_t capacity_;
    std::vector<std::unique_ptr<IterableCollectorBin>> bins_;
};

} // namespace collection
} // namespace sparta
