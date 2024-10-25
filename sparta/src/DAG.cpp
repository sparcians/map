// <DAG> -*- C++ -*-


#include "sparta/kernel/DAG.hpp"

#include "sparta/events/SchedulingPhases.hpp"


namespace sparta
{
    /**
     * Write a text version of the CycleException's cycle vertex list
     * @param os
     */
    void DAG::CycleException::writeCycleAsText(std::ostream& os) const {
        os << "DAG CYCLE: " << std::endl;

        const Vertex* prior_v = nullptr;
        for (const auto& v : cycle_set_) {
            if (prior_v != nullptr) {
                const Edge* e = prior_v->getEdgeTo(v);
                sparta_assert(e != nullptr);
                os << " -> " << v->getLabel()
                   // << "\t// edge: " << e->getLabel()
                   << std::endl;
            }
            os << "\t" << v->getLabel();
            prior_v = v;
        }

        // TODO: FOR NOW -- we relax the constraint that the final
        // vertex in the cycle_set_ needs to have an edge back to
        // the first vertex.
        //Vertex* first = cycle_set_.front();
        //const Edge* e = prior_v->getEdgeTo(first);
        //sparta_assert(e != nullptr);
        //os << " -> " << first->getLabel()
           //<< std::endl;

        // Find and print the cyclic edge from prior_v
        bool found_cycle_edge = false;
        for (const auto& w : cycle_set_) {
            const Edge* e = prior_v->getEdgeTo(w);
            if (e != nullptr) {
                os << " -> " << w->getLabel()
                   << std::endl;
                found_cycle_edge = true;
                break;
            }
        }
        sparta_assert(found_cycle_edge);
    }

    /**
     * Write a DOT graph version of the CycleException's cycle vertex list
     * @param os
     */
    void DAG::CycleException::writeCycleAsDOT(std::ostream& os) const {
        os << "digraph dag_cycle {" << std::endl;
        os << "\trankdir=TB;" << std::endl;
        os << "\tnode [shape=record, fontname=Helvetica, fontsize=10];" << std::endl;
        os << std::endl;

        bool first = true;
        for (const auto& v : cycle_set_) {
            if (!first) {
                os << " -> \"" << v->getLabel() << "\";" << std::endl;
            }
            os << "\t\"" << v->getLabel() << "\"";
            first = false;
        }

        // TODO: FOR NOW -- we relax the constraint that the final
        // vertex in the cycle_set_ needs to have an edge back to
        // the first vertex.
        // os << " -> \"" << cycle_set_.front()->getLabel() << "\";" << std::endl;

        // Find and print the cyclic edge from last_v
        const Vertex* last_v = cycle_set_.back();
        bool found_cycle_edge = false;
        for (const auto& w : cycle_set_) {
            const Edge* e = last_v->getEdgeTo(w);
            if (e != nullptr) {
                os << " -> \"" << w->getLabel() << "\";" << std::endl;
                found_cycle_edge = true;
                break;
            }
        }
        sparta_assert(found_cycle_edge);

        os << "}" << std::endl;
    }

    Vertex* DAG::newFactoryVertex(const std::string& label,
                                  sparta::Scheduler* const scheduler,
                                  const bool isgop)
    {
        return v_factory_.newFactoryVertex(label, scheduler, isgop);
    }

    /**
     * \brief Finalize the DAG
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
        Vertex * trigger  = newGOPVertex("Trigger", getScheduler());
        Vertex * update   = newGOPVertex("Update", getScheduler());
        Vertex * pu       = newGOPVertex("PortUpdate", getScheduler());
        Vertex * flush    = newGOPVertex("Flush", getScheduler());
        Vertex * collect  = newGOPVertex("Collection", getScheduler());
        Vertex * tick     = newGOPVertex("Tick", getScheduler());
        Vertex * posttick = newGOPVertex("PostTick", getScheduler());
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

    //! Only linked vertices will be known to the DAG
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

        // TODO: REMOVE DEBUGGING STATEMENTS
        //std::cout << "DAG::link()" << std::endl;
        //std::cout << "\t" << std::string(*source_vertex) << " -> " << std::string(*dest_vertex) << std::endl;

        if (source_vertex->link(e_factory_, dest_vertex, reason)) {
            if (early_cycle_detect_ && detectCycle()) {
                throw CycleException(getCycles_());
            }
        }

        // TODO: DEBUGGING - remove this
        //if (detectCycle()) {
            //throw CycleException(getCycles_());
        //}
    }


    bool DAG::sort()
    {
        uint32_t vcount = alloc_vertices_.size();
        num_groups_ = 1;
        typename Vertex::VertexList   zlist;

        if (detectCycle()) {
            printCycles(std::cout);
            //sparta_assert(false);
        }

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

            // v->assignConsumerGroupIDs(zlist);
            uint32_t gid = v->getGroupID();
            for(auto &w_out : v->edges())
            {
                // Vertex * outbound = ei.first;
                // The outbound edge better have a count of edges by at
                // LEAST one -- it has to include this link!
                uint32_t w_out_inbound_edges = w_out->getNumInboundEdgesForSorting();
                sparta_assert(w_out_inbound_edges > 0);
                --w_out_inbound_edges;

                // If the destination's group ID is at or less than this
                // source's ID, bump it -- there's a dependency
                if (w_out->getGroupID() <= gid) {
                    w_out->setGroupID(gid + 1);
                }

                // If there are no other inputs to this Vertex, it's now
                // on the zlist to recursively set it's destination group
                // IDs.
                if (w_out_inbound_edges == 0) {
                    zlist.push_back(w_out);
                }

                w_out->setNumInboundEdgesForSorting(w_out_inbound_edges);
            }

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

    //! Detect whether the DAG has at least one cycle
    bool DAG::detectCycle() const
    {
        for (auto& vi : alloc_vertices_)
        {
            vi->resetMarker();
        }

        for (auto& vi : alloc_vertices_) {
            if (vi->wasNotVisited()) {
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
        typename Vertex::VertexList     cycle_set;

        for (auto& vi : alloc_vertices_)
        {
            vi->resetMarker();
        }

        for (auto& vi : alloc_vertices_) {
            if (vi->wasNotVisited()) {
                if (vi->findCycle(cycle_set)) {
                    os << "CYCLE:" << std::endl;
                    for (auto ci : cycle_set) {
                        ci->printFiltered(os, Vertex::CycleMarker::GRAY);
                    }
                    return;
                }
            }
        }
    }

    void DAG::dumpToCSV(std::ostream &os_vertices, std::ostream &os_edges) const {
        v_factory_.dumpToCSV(os_vertices);
        e_factory_.dumpToCSV(os_edges);
    }

    void DAG::print(std::ostream& os) const
    {
        std::ios_base::fmtflags os_state(os.flags());
        if (!finalized_) {
            os << "=================" << std::endl;
            os << "WARNING: DAG IS NOT YET FINALIZED (unsorted, so group ID's are not yet fixed)" << std::endl;
            os << "=================" << std::endl;
        }
        for (const auto & vi : alloc_vertices_) {
            vi->print(os);
            os << std::endl;
        }
        os.flags(os_state);
    }

    // Just mark one cycle for now...
    typename Vertex::VertexList DAG::getCycles_()
    {
        typename Vertex::VertexList cycle_set;

        for (auto& vi : alloc_vertices_)
        {
            vi->resetMarker();
        }

        for (auto& vi : alloc_vertices_) {
            if (vi->wasNotVisited()) {
                if (vi->findCycle(cycle_set)) {
                    break;
                }
            }
        }
        return cycle_set;
    }
}
