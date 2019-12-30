// <Unit.cpp> -*- C++ -*-


#include <cstdint>
#include <string>
#include <map>
#include <iostream>
#include <set>
#include <unordered_map>
#include <utility>

#include "sparta/simulation/Unit.hpp"
#include "sparta/kernel/Vertex.hpp"
#include "sparta/events/EventNode.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/Scheduleable.hpp"
#include "sparta/events/SchedulingPhases.hpp"
#include "sparta/ports/Port.hpp"
#include "sparta/ports/PortSet.hpp"

namespace
{

    void open_diagraph(std::ofstream & os, const std::string & name)
    {
        os << "digraph " << name <<
            "\n{\n"
            "\tedge [minlen=3]; // Don't crush everything together\n"
            "\tnode [shape=record, fontname=Helvetica, fontsize=8];\n"
            "\t{                                                    \n"
            "\t    node [shape=plaintext, fontsize=16];             \n"
            "\t    // Tier map                                      \n"
            "\t    edge [arrowhead=tee];                            \n"
            "\t    Update -> PortUpdate -> Tick -> PostTick;        \n"
            "\t}\n";
    }

    void add_section(std::ofstream & os, const std::string & section,
                     const std::set<std::string> & objs)
    {
        os << "\t{\n";
        os << "\t\trank=same;\n";
        os << "\t\t" << section << ";\n";
        for(auto & i : objs) {
            os << "\t\t\"" << i << "\";\n";
        }
        os << "\t}\n";
    }

    void add_links(std::ofstream & os,
                   const std::map<std::string, std::set<std::string>> & links)
    {
        for(auto & p : links) {
            for(auto & l : p.second) {
                os << "\t\"" << p.first << "\" -> \"" << l << "\";\n";
            }
        }
    }

    void close_diagraph(std::ofstream & os)
    {
        os<< "\n}\n";
    }
}


namespace sparta
{
    void Unit::onBindTreeLate_()
    {
        // Turn this off for now...
        return;

        std::ofstream dag_dot(getName() + ".dot");
        open_diagraph(dag_dot, getName());

        std::map<sparta::SchedulingPhase, std::set<std::string>> phase_to_name;
        std::map<std::string, std::set<std::string>> links;

        // Ports
        auto & ps_in_map = unit_port_set_.getPorts(Port::Direction::IN);
        for(auto & pp : ps_in_map) {
            InPort * inp = static_cast<sparta::InPort*>(pp.second);
            phase_to_name[inp->getDeliverySchedulingPhase()].insert(pp.first);
            for(auto & ptc : inp->getPortTickConsumers()) {
                links[pp.first].insert(ptc->getLabel());
            }
        }
        auto & ps_out_map = unit_port_set_.getPorts(Port::Direction::OUT);
        for(auto & pp : ps_out_map) {
            phase_to_name[sparta::SchedulingPhase::Tick].insert(pp.first);
        }

        // Events
        for(uint32_t i = 0; i < NUM_SCHEDULING_PHASES; ++i) {
            for(auto & e : unit_event_set_.getEvents(static_cast<sparta::SchedulingPhase>(i))) {
                const sparta::Scheduleable & sched = e->getScheduleable();
                phase_to_name[static_cast<sparta::SchedulingPhase>(i)].insert(sched.getLabel());
                for(auto & ed : sched.vertex_->edges())
                {
                    if(!ed.first->isGOP()) {
                        auto ext_dep = dynamic_cast<const Scheduleable*>(ed.first->getScheduleable());
                        if(ext_dep) {
                            phase_to_name[ext_dep->getSchedulingPhase()].
                                insert(ed.first->getLabel());
                        }
                        links[sched.getLabel()].insert(ed.first->getLabel());
                    }
                }
            }
        }

        add_section(dag_dot, "Update", phase_to_name[sparta::SchedulingPhase::Update]);
        add_section(dag_dot, "PortUpdate", phase_to_name[sparta::SchedulingPhase::PortUpdate]);
        add_section(dag_dot, "Tick", phase_to_name[sparta::SchedulingPhase::Tick]);
        add_section(dag_dot, "PostTick", phase_to_name[sparta::SchedulingPhase::PostTick]);

        add_links(dag_dot, links);

        close_diagraph(dag_dot);
    }
}
