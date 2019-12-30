// <DAG> -*- C++ -*-


#include "sparta/kernel/DAG.hpp"

#include "sparta/events/SchedulingPhases.hpp"


namespace sparta
{

    void DAG::CycleException::outPutIssue_(std::ostream & os, bool dot) const
    {
        if(dot) {
            os << "digraph dag_issue {\n";
            os << "\trankdir=TB;\n";
            os << "\tnode [shape=record, fontname=Helvetica, fontsize=10];\n\n";
        }
        else {
            os << "DAG CYCLE: " << std::endl;
        }

        Vertex * very_first = nullptr;
        Vertex * very_last = nullptr;

        typename Vertex::VList cycle_set = cycle_set_;

        while(!cycle_set.empty())
        {
            Vertex * first = cycle_set.front();
            cycle_set.pop_front();

            Vertex * second = nullptr;
            if(!cycle_set.empty()) {
                second = cycle_set.front();
            }
            if(first == second) {
                continue;
            }

            if(!very_first) { very_first = first; }
            if(second) {
                os << "\t\"" << first->getLabel() << "\"";
                os << " -> \"" << second->getLabel() << "\"";
                const Edge * e = first->getEdgeTo(second);
                if(e != nullptr && dot) {
                    os << " [fontsize=8 label=\"" << e->getLabel() << "\"]";
                }
                os << ";\n";
                very_last = second;
            }
        }
        if(very_first && very_last) {
            if(very_first != very_last) {
                const Edge * e = very_last->getEdgeTo(very_first);
                if(e) {
                    if(dot) {
                        os << "\t\"" << very_last->getLabel() << "\" -> \"" << very_first->getLabel()
                           << "\" [fontsize=8 label=\"" << e->getLabel() << "\"];\n";
                    }
                    else {
                        os << "\t\"" << very_last->getLabel() << "\" -> \""
                           << very_first->getLabel() << "\";" << std::endl;
                    }
                }
            }
        }
        if(dot) {
            os << "\n}\n";
        }
    }
    Vertex* DAG::newFactoryVertex(const std::string& label,
                                  sparta::Scheduler* const scheduler,
                                  const bool isgop)
    {
        return v_factory_->newFactoryVertex(label, scheduler, isgop);
    }

    /**
     * \brief Finialize the DAG
     * \return The number of groups that were created
     */
    uint32_t DAG::finalize()
    {
        sparta_assert(finalized_ == false);
        uint32_t group_count = 0;

        try {
            sort();
            group_count = numGroups();
        } catch (const DAG::CycleException & e){
            throw;
        }
        finalizeGOPs_();
        finalized_ = true;
        return group_count;
    }

    DAG::DAG(sparta::Scheduler * scheduler, const bool& check_cycles):
        v_factory_(std::unique_ptr<VertexFactory>(new VertexFactory())),
        num_groups_(1),
        early_cycle_detect_(check_cycles),
        gops_(),
        my_scheduler_(scheduler)
    {
        initializeDAG_();

    }

    void DAG::initializeDAG_()
    {

        sparta_assert(my_scheduler_);
        // Setup artificial phasing in the DAG
        //
        // Trigger -> Update -> PortUpdate -> Flush -> Collection -> Tick -> PostTick
        //
        // XXX Dave to make this go away.
        Vertex * trigger  = newVertex("Trigger", getScheduler(), true);
        Vertex * update   = newVertex("Update", getScheduler(),  true);
        Vertex * pu       = newVertex("PortUpdate", getScheduler(), true);
        Vertex * flush    = newVertex("Flush", getScheduler(), true);
        Vertex * collect  = newVertex("Collection", getScheduler(), true);
        Vertex * tick     = newVertex("Tick", getScheduler(), true);
        Vertex * posttick = newVertex("PostTick", getScheduler(), true);
        link(trigger, update);
        link(update, pu);
        link(pu, flush);
        link(flush, collect);
        link(collect, tick);
        link(tick, posttick);

        static_assert(sparta::NUM_SCHEDULING_PHASES == 7,
                      "You added a phase and didn't update the DAG");
        // XXX go away

    }

    void DAG::link(Vertex * source_vertex,
                   Vertex * dest_vertex, const std::string & reason)
    {
        if(!source_vertex->isInDAG()){
            alloc_vertices_.emplace_back(source_vertex);
            source_vertex->setInDAG(true);
        }

        if(!dest_vertex->isInDAG()){
            alloc_vertices_.emplace_back(dest_vertex);
            dest_vertex->setInDAG(true);
        }

        if (source_vertex->link(dest_vertex, reason)) {
            if (early_cycle_detect_ && detectCycle()) {
                throw CycleException(getCycles_());
            }
        }

    }


    bool DAG::sort()
    {
        uint32_t vcount = alloc_vertices_.size();
        num_groups_ = 1;
        typename Vertex::VList   zlist;

        // Initialize the queue of 0-vertices
        for (auto & vi : alloc_vertices_) {
            vi->reset();

            // If this vertex has no producers (sources, i.e. nothing
            // coming into it), add it to the zlist
            if (vi->degreeZero()) {
                zlist.push_back(vi);
            }
        }

        // As the graph assigns group IDs to the Vertexes, it chops
        // away at those Vertexes that start with 0 inbound edges.  As
        // it finds the next series of zero-inbound edged Vertexes, it
        // appends them to the zlist to keep this while loop going.
        // If list empties, but there are still vertexes not removed,
        // then we have a cycle
        while (!zlist.empty())
        {
            Vertex *v = zlist.front();
            sparta_assert(v != nullptr);
            zlist.pop_front();

            sparta_assert(vcount > 0);
            --vcount;

            v->assignConsumerGroupIDs(zlist);
            if (v->getGroupID() > num_groups_) {
                num_groups_ = v->getGroupID() + 1;
            }
        }

        //How many groups are there after finalization.
        sparta_assert(num_groups_ > 0);

        if (vcount != 0) {
            throw CycleException(getCycles_());
        }
        return (vcount == 0);
    }

    bool DAG::detectCycle() const
    {
        for (auto& vi : alloc_vertices_)
        {
            vi->resetMarker();
        }

        for (auto& vi : alloc_vertices_) {
            if (vi->wasVisited()) {
                if (vi->detectCycle()) {
                    return true;
                }
            }
        }
        return false;
    }

    // Just print one cycle for now...
    void DAG::printCycles(std::ostream& os) const
    {
        typename Vertex::VList     cycle_set;

        for (auto& vi : alloc_vertices_)
        {
            vi->resetMarker();
        }

        for (auto& vi : alloc_vertices_) {
            if (vi->wasVisited()) {
                if (vi->findCycle(cycle_set)) {
                    os << "CYCLE:" << std::endl;
                    for (auto ci : cycle_set) {
                        ci->print(os);
                    }
                    return;
                }
            }
        }
    }

    void DAG::print(std::ostream& os) const
    {
        if(finalized_) {
            for (const auto & vi : alloc_vertices_) {
                vi->print(os);
                os << std::endl;
            }
        }
        else {
            os << "DAG not finalized ... cannot print contents" << std::endl;
        }
    }

    // Just mark one cycle for now...
    typename Vertex::VList DAG::getCycles_()
    {
        typename Vertex::VList cycle_set;

        for (auto& vi : alloc_vertices_)
        {
            vi->resetMarker();
        }

        for (auto& vi : alloc_vertices_) {
            if (vi->wasVisited()) {
                vi->findCycle(cycle_set);
            }
        }
        return cycle_set;
    }
}
