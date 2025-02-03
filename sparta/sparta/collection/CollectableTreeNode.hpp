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

    bool isCollected() const { return false; }
};

template <typename CollectedT>
class ManualCollectable : public CollectableTreeNode
{
public:
    ManualCollectable(TreeNode* parent, const std::string& name, const std::string& desc = "ManualCollectable <no desc>")
        : CollectableTreeNode(parent, name, desc)
    {
    }

    void addCollectionPoint(CollectionPoints & collection_points) override
    {
        (void)collection_points;
    }

    void collect(const CollectedT & dat)
    {
        (void)dat;
    }

    void collectWithDuration(const CollectedT & dat, size_t dur)
    {
        (void)dat;
        (void)dur;
    }

private:
};

template <typename CollectedT>
class DelayedCollectable : public CollectableTreeNode
{
public:
    DelayedCollectable(TreeNode* parent, const std::string& name, const std::string& desc = "DelayedCollectable <no desc>")
        : CollectableTreeNode(parent, name, desc)
    {
    }

    void addCollectionPoint(CollectionPoints & collection_points) override
    {
        (void)collection_points;
    }

    void collect(const CollectedT & dat, uint64_t delay)
    {
        (void)dat;
        (void)delay;
    }

    void collectWithDuration(const CollectedT & dat, uint64_t delay, size_t dur)
    {
        (void)dat;
        (void)delay;
        (void)dur;
    }

private:
};

template <typename CollectedT>
class AutoCollectable : public CollectableTreeNode
{
public:
    AutoCollectable(TreeNode* parent, const std::string& name, const CollectedT* back_ptr, const std::string& desc = "AutoCollectable <no desc>")
        : CollectableTreeNode(parent, name, desc)
        , back_ptr_(back_ptr)
    {
    }

    void addCollectionPoint(CollectionPoints & collection_points) override
    {
        auto flag = std::integral_constant<bool, std::is_same<CollectedT, bool>::value>{};
        addCollectionPoint_(collection_points, flag);
    }

private:
    void addCollectionPoint_(CollectionPoints & collection_points, std::true_type)
    {
        using value_type = int32_t;
        auto getter = std::function<value_type()>([this]() { return *back_ptr_ ? 1 : 0; });
        collection_points.addStat(getLocation(), getClock(), getter);
    }

    void addCollectionPoint_(CollectionPoints & collection_points, std::false_type)
    {
        collection_points.addStat(getLocation(), getClock(), back_ptr_);
    }

    const CollectedT* back_ptr_;
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
