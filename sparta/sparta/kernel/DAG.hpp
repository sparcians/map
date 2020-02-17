// <DAG.hpp> -*- C++ -*-


/**
 * \file DAG.hpp
 * \brief Used internally by the sparta::Scheduler to set event ordering
 */


#ifndef __DAG__H__
#define __DAG__H__

#include <cstdint>
#include <string>
#include <iostream>
#include <sstream>
#include <memory>
#include <list>
#include <map>
#include <vector>
#include <set>
#include <cinttypes>
#include <cassert>
#include <utility>

#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/events/Scheduleable.hpp"
#include "sparta/events/SchedulingPhases.hpp"
#include "sparta/kernel/Vertex.hpp"
#include "sparta/kernel/VertexFactory.hpp"
#include "sparta/kernel/EdgeFactory.hpp"

namespace sparta
{
    class Unit;
    class Scheduler;

    class DAG
    {

        friend std::ostream& operator<<(std::ostream& os, const Edge &e);
        friend std::ostream& operator<<(std::ostream& os, const Vertex &v);

        friend Unit;

    public:

        // For backward compatibility
        typedef Vertex GOPoint;
        typedef std::map<std::string, Vertex*>   VertexMap;

        class CycleException : public SpartaException
        {
        public:
            CycleException(const typename Vertex::VertexList & cycle_set) :
                SpartaException(),
                cycle_set_(cycle_set)
            {}

            CycleException(const std::string& reason) :
                SpartaException(reason)
            {}

            void writeCycleAsDOT (std::ostream & os) const;
            void writeCycleAsText (std::ostream & os) const;

        private:
            //void outPutIssue_(std::ostream & os, bool dot) const;
            typename Vertex::VertexList     cycle_set_;
        };

        ////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////

        // DAG Nabbit
        DAG(sparta::Scheduler * scheduler, const bool& check_cycles = false);

        DAG() =delete;

        //! Turn on early cycle detection -- as items are linked in
        //! the DAG, it will look for a cycle.
        void enableEarlyCycleDetect() {
            early_cycle_detect_ = true;
        }

        //! \brief Initialize the DAG
        //! Creates new vertices from the VertexFactory
        //! and links them according to precedence.
        void initializeDAG_();

        /**
         * \brief Finialize the DAG
         * \return The number of groups that were created
         */
        uint32_t finalize();

        //! Is the DAG finalized?
        bool isFinalized() const {
            return finalized_;
        }

        /**
         * \brief Get a new Vertex from the DAGs Vertex Factory
         * Called in a Scheduleable after the scheduler_ has
         * been set, before the Scheduleable is assigned/linked
         * to a DAG phase.
         * \return The new Vertex contained in a Scheduleable.
         */
        Vertex* newFactoryVertex(const std::string& label,
                                 sparta::Scheduler* const scheduler,
                                 const bool isgop=false);

        /**
         * \brief link(): Establish a precedence relationship between
         *        two entities. This method will wrap the Scheduleables
         *        with Vertices so that they can be manipulated by the
         *        DAG
         *
         * \param source The Vertex from
         * \param dest   The Vertex to
         * \param reason The reason for the link
         * \throw CycleException if the new link will cause a DAG cycle
         *
         * link(v,w) will introduce an edge from source to dest, so
         * that source precedes dest (and after sort, source's group
         * ID will be less than dest's)
         */

        void link(Vertex *v, Vertex *w, const std::string & reason = "");

        bool unlink(Vertex *v, Vertex *w)
        {
            sparta_assert(v != nullptr);
            sparta_assert(w != nullptr);

            return v->unlink(e_factory_, w);
        }

        uint32_t numGroups() {
            return num_groups_;
        }

        bool sort();

        /**
         * \brief Find a GOP point
         * \param label The GOP point to create
         * \return the GOP point; nullptr if not found
         */
        Vertex* findGOPVertex(const std::string& label) const
        {
            auto loc = gops_.find(label);
            return (loc != gops_.end()) ? loc->second : nullptr;
        }

        /**
         * \brief Create a new Vertex-GOP point
         * \throw Will assert if already exists
         * \param label The GOP point to create
         * \param scheduler The Scheduler associated with this Vertex
         * \return the new GOP point; assert if already exists
         */
        Vertex* newGOPVertex(const std::string& label, sparta::Scheduler* const scheduler)
        {
            sparta_assert(findGOPVertex(label) == nullptr);
            Vertex* gop = this->newFactoryVertex(label, scheduler, true);
            gops_[label] = gop;
            return gop;
        }

        /**
         * \brief Get the named GOP point; create it if not found.
         * \param label The GOP point to find or create
         * \return the GOP point, never nullptr
         */
        Vertex* getGOPoint(const std::string& label)
        {
            Vertex *gop = findGOPVertex(label);
            if (gop == nullptr) {
                return newGOPVertex(label, getScheduler());
            }
            return gop;
        }

        sparta::Scheduler * getScheduler() const{
            return my_scheduler_;
        }

        //! Look for cycles
        bool detectCycle() const;

        // Just print one cycle for now...
        void printCycles(std::ostream& os) const;

        //! Dump the DAG to a CSV vertices file and an edges file
        void dumpToCSV(std::ostream& os_vertices, std::ostream& os_edges) const;

        //! Print the DAG
        void print(std::ostream& os) const;

    private:
        // Just mark one cycle for now...
        typename Vertex::VertexList getCycles_();

        // Transfer Global Ordering Point GID's to associates
        void finalizeGOPs_()
        {
            for (auto& i : gops_) {
                i.second->transferGID();
            }
        }

        //! Vertex Factory to keep track of created vertices
        VertexFactory                           v_factory_;
        EdgeFactory                             e_factory_;

        std::vector<Vertex*>                    alloc_vertices_;
        uint32_t                                num_groups_ = 1;
        bool                                    early_cycle_detect_;
        VertexMap                               gops_;
        bool                                    finalized_ = false;
        sparta::Scheduler*                      my_scheduler_ = nullptr;
    };//End class DAG


    inline std::ostream& operator<<(std::ostream& os, const DAG &d)
    {
        d.print(os);
        return os;
    }

    inline std::ostream& operator<<(std::ostream& os, const DAG *d)
    {
        os << *d;
        return os;
    }

} // namespace sparta

#endif
