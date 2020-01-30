// <Vertex> -*- C++ -*-


#include <cstdint>
#include <map>
#include <ostream>
#include <string>
#include <utility>

#include "sparta/kernel/DAG.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/kernel/Vertex.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/events/Scheduleable.hpp"

namespace sparta
{

    bool Vertex::link(Vertex * dest, const std::string& label)
    {
        if(dest == this) return false;

        if (edges_.find(dest) != edges_.end()) {
            // Edge already present -- not necessary to add it again
            return false;
        } else {

            edges_[dest] = Edge(this, dest, label);

            ++(dest->num_inbound_edges_);
        }
        return true;
    }

    bool Vertex::unlink(Vertex * w)
    {
        if(w == this) return false;

        auto ei = edges_.find(w);
        if (ei == edges_.end()) {
            // Edge not present -- just ignore
            return false;
        } else {
            edges_.erase(ei);
            sparta_assert(w->num_inbound_edges_ > 0);
            --(w->num_inbound_edges_);
        }
        return true;
    }

    void Vertex::assignConsumerGroupIDs(VList &zlist)
    {

        uint32_t gid = getGroupID();

        for(auto &ei : sorting_edges_)
        {
            Vertex * outbound = ei.first;
            // The outbound edge better have a count of edges by at
            // LEAST one -- it has to include this link!
            sparta_assert(outbound->sorted_num_inbound_edges_ > 0);
            --(outbound->sorted_num_inbound_edges_);

            // If the destination's group ID is at or less than this
            // source's ID, bump it -- there's a dependency
            if (outbound->getGroupID() <= gid) {
                outbound->setGroupID(gid + 1);
            }

            // If there are no other inputs to this Vertex, it's now
            // on the zlist to recursively set it's destination group
            // IDs.
            if (outbound->sorted_num_inbound_edges_ == 0) {
                zlist.push_back(outbound);
            }

        }
    }

    bool Vertex::detectCycle()
    {
        marker_ = CycleMarker::GRAY;
        for (auto& ei : edges_) {
            Vertex *w = ei.first;
            switch (w->marker_) {
            case CycleMarker::WHITE:
                if (w->detectCycle()) {
                    return true;
                }
                break;
            case CycleMarker::GRAY:
                return true;
            default:
                break; // Do nothing
            }
        }
        marker_ = CycleMarker::BLACK;
        return false;
    }

    bool Vertex::findCycle(VList& cycle_set)
    {
        marker_ = CycleMarker::GRAY;

        for (auto& ei : edges_) {
            Vertex *w = ei.first;
            switch (w->marker_) {
            case CycleMarker::WHITE:
                if (w->findCycle(cycle_set)) {
                    cycle_set.push_front(w);
                    return true;
                }
                break;
            case CycleMarker::GRAY:
                cycle_set.push_front(w);
                return true;
            default:
                break; // Do nothing
            }
        }
        marker_ = CycleMarker::BLACK;
        return false;
    }

    void Vertex::precedes(Scheduleable & w, const std::string & label) {
        sparta_assert(my_scheduler_);
        DAG * dag = my_scheduler_->getDAG();
        sparta_assert(dag->isFinalized() == false,
                    "You cannot set precedence during a running simulation (i.e., the DAG is finalized)");
        dag->link(this, w.getGOPoint(), label);
    }

    void Vertex::print(std::ostream& os) const
    {
        os << std::string(*this) << std::endl;
        for (const auto & ei : edges_) {
            os << "\t-> " << std::string(*(ei.first)) << ", " << std::string(ei.second) << std::endl;
        }
        os << std::endl;
    }

}
