#pragma once

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/collection/CollectionPoints.hpp"
#include "sparta/events/PayloadEvent.hpp"
#include "sparta/events/PhasedUniqueEvent.hpp"
#include "sparta/events/EventSet.hpp"

namespace sparta {
namespace collection {

class CollectionPoints;

// Base class for all collectable classes.
class CollectableTreeNode : public TreeNode
{
public:
    CollectableTreeNode(TreeNode* parent, const std::string& name, const std::string& desc = "CollectableTreeNode <no desc>")
        : TreeNode(parent, name, desc)
    {
        markHidden();
    }

    virtual void addCollectionPoint(CollectionPoints & collection_points) = 0;

    bool isCollected() const { return collected_; }

    void enable() { collected_ = true; }

    // Note there is no way to disable collection on a per-collectable basis. You can
    // only call PipelineCollector::stopCollecting() to stop all collection for all
    // collectables.

private:
    bool collected_ = false;
};

// Manually-collected means that the user must explicitly call collect() or collectWithDuration()
// and cannot give a backpointer to this class' constructor for automatic every-cycle collection.
template <typename CollectedT>
class ManualCollectable : public CollectableTreeNode
{
public:
    ManualCollectable(TreeNode* parent, const std::string& name, sparta::EventSet* ev_set, const std::string& desc = "ManualCollectable <no desc>")
        : CollectableTreeNode(parent, name, desc)
        , ev_end_duration_(getEventSet_(parent, ev_set), name + "_end_duration_event", SchedulingPhase::Collection, CREATE_SPARTA_HANDLER(ManualCollectable, endDuration_))
    {
        sparta_assert(getClock());
    }

    void addCollectionPoint(CollectionPoints & collection_points) override
    {
        collector_ = collection_points.addManualCollectable<CollectedT>(getLocation(), getClock());
    }

    using CollectableTreeNode::isCollected;

    void collect(const CollectedT & dat)
    {
        if (isCollected()) {
            collector_->collectOnce(dat);
        }
    }

    void collectWithDuration(const CollectedT & dat, const Clock::Cycle dur)
    {
        if (isCollected()) {
            collector_->beginZOH(dat);
            ev_end_duration_.schedule(getClock()->getTick(dur));
        }
    }

private:
    EventSet* getEventSet_(TreeNode* parent, EventSet* ev_set)
    {
        if (ev_set) {
            return ev_set;
        } else {
            ev_set_.reset(new EventSet(parent));
            return ev_set_.get();
        }
    }

    void endDuration_()
    {
        if (isCollected()) {
            collector_->endZOH();
        }
    }

    simdb::AnyCollection<CollectedT>* collector_ = nullptr;
    sparta::PhasedUniqueEvent ev_end_duration_;
    std::unique_ptr<EventSet> ev_set_;
};

// Similar to the ManualCollectable class, you cannot use a DelayedCollectable
// for any kind of automatic collection. You must explicitly call collect() or
// collectWithDuration() to collect data.
template <typename CollectedT>
class DelayedCollectable : public ManualCollectable<CollectedT>
{
    struct DurationData
    {
        DurationData() = default;

        DurationData(const CollectedT & dat,
                     sparta::Clock::Cycle duration)
            : data(dat)
            , duration(duration)
        {}

        CollectedT data;
        sparta::Clock::Cycle duration;
    };

public:
    DelayedCollectable(TreeNode* parent, const std::string& name, sparta::EventSet* ev_set, const std::string& desc = "DelayedCollectable <no desc>")
        : ManualCollectable<CollectedT>(parent, name, ev_set, desc)
        , ev_collect_(ev_set, name + "_event", CREATE_SPARTA_HANDLER_WITH_DATA(ManualCollectable<CollectedT>, collect, CollectedT))
        , ev_collect_duration_(ev_set, name + "_duration_event", CREATE_SPARTA_HANDLER_WITH_DATA(DelayedCollectable<CollectedT>, collectWithDuration_, DurationData))
    {
    }

    using ManualCollectable<CollectedT>::isCollected;

    void collect(const CollectedT & dat, uint64_t delay)
    {
        if (isCollected()) {
            if (delay == 0) {
                ManualCollectable<CollectedT>::collect(dat);
            } else {
                ev_collect_.schedule(dat, delay);
            }
        }
    }

    void collectWithDuration(const CollectedT & dat, uint64_t delay, const Clock::Cycle dur)
    {
        if (isCollected()) {
            if (delay == 0) {
                ManualCollectable<CollectedT>::collectWithDuration(dat, dur);
            } else {
                ev_collect_duration_.preparePayload({dat, dur})->schedule(delay);
            }
        }
    }

private:
    // Called from collectWithDuration where the data needs to be
    // delivered at a given delayed time, but only for a short
    // duration.
    void collectWithDuration_(const DurationData & dur_dat)
    {
        ManualCollectable<CollectedT>::collectWithDuration(dur_dat.data, dur_dat.duration);
    }

    // For those folks that want collection to appear in the future
    sparta::PayloadEvent<CollectedT, SchedulingPhase::Collection> ev_collect_;

    // For those folks that want collection to appear in the future with a duration
    sparta::PayloadEvent<DurationData, SchedulingPhase::Collection> ev_collect_duration_;
};

// Auto-collectable means that the user can give a backpointer to this class' constructor
// and we will collect your data every cycle automatically. There is no way to automatically
// collect at any other time than "every cycle".
//
// This class is to be used for non-iterable types like uint64_t, struct, bool, etc.
// You should use the IterableCollector class for iterable types like std::vector, sparta::Array, etc.
template <typename CollectedT>
class AutoCollectable : public CollectableTreeNode
{
public:
    AutoCollectable(TreeNode* parent, const std::string& name, const CollectedT* back_ptr, const std::string& desc = "AutoCollectable <no desc>")
        : CollectableTreeNode(parent, name, desc)
        , back_ptr_(back_ptr)
    {
        static_assert(std::is_integral<CollectedT>::value || std::is_floating_point<CollectedT>::value,
                      "AutoCollectable only supports integral and floating-point types!");
    }

    void addCollectionPoint(CollectionPoints & collection_points) override
    {
        if constexpr (std::is_same<CollectedT, bool>::value) {
            using value_type = int32_t;
            auto getter = std::function<value_type()>([this]() { return *back_ptr_ ? 1 : 0; });
            collection_points.addStat(getLocation(), getClock(), getter);
        } else {
            collection_points.addStat(getLocation(), getClock(), back_ptr_);
        }
    }

private:
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
