// <Vertex> -*- C++ -*-


#ifndef __VERTEX__H__
#define __VERTEX__H__

#include <string>
#include <iostream>
#include <sstream>
#include <memory>
#include <list>
#include <map>

#include "sparta/events/Scheduleable.hpp"

namespace sparta
{
    class Vertex;
    class EdgeFactory;

    /**
     * \class Edge
     *
     * \brief A class used to connect 2 Vertices in the DAG. It indirectly
     *        connects 2 Scheduleables by connecting their internal vertex_.
     *
     * This class is used by the DAG when the DAG is created or anytime a
     * new Scheduleable needs to be placed in the DAG.
     *
     */
    class Edge
    {
    public:

        /**
         * \brief Default Constructor
         */
        Edge() = default;

        /**
         * \brief Construct an Edge object
         *
         * \param source The vertex this edge starts from
         * \param dest The Vertex this edge ends in
         * \param label The name for this edge
         */
        Edge(const Vertex * source, const Vertex * dest, const std::string& label);
        //! Allow copies
        Edge(const Edge &) = default;

        //! Get internal label/name
        const std::string& getLabel() const
        {
            return label_;
        }

        explicit operator std::string() const;

        void dumpToCSV(std::ostream& os, bool dump_header=false) const;

        void print(std::ostream& os) const
        {
            os << std::string(*this) << std::endl;
        }

    private:
        uint32_t        id_ = 0; // Unique global ID
        static uint32_t global_id_;
        const Vertex *  source_ = nullptr;
        const Vertex *  dest_ = nullptr;
        std::string     label_{"unitialized"};
    }; //END Edge

    /**
     * \class Vertex
     *
     * \brief A class used in DAG and Scheduleable
     *     A Scheduleable contains a Vertex to connect to other
     *     Vertices in a DAG (GOPs) or other Scheduleables to
     *     establish precedence.
     */
    class Vertex
    {

        typedef std::map<Vertex*, const Edge*>      EMap;
        typedef std::list<Scheduleable*>            AssociateList;

    public:
        enum class CycleMarker {
            WHITE,   // Not discovered
            GRAY,    // Just discovered
            BLACK    // Finished
        };

        typedef uint32_t PrecedenceGroup;
        static const PrecedenceGroup INVALID_GROUP;
        typedef std::list<Vertex *>          VList;
        typedef std::set<Vertex *>           VSet;


        /**
         * \brief Construct a Vertex object
         *
         * \param label The name for this Vertex
         * \param scheduler The Scheduler for this Vertex
         * \param isgop Determines whether this Vertex will
         *        transferGID()
         */
        Vertex(const std::string & label, sparta::Scheduler *scheduler,
                 bool isgop = false) :
            label_(label),
            my_scheduler_(scheduler)
        {
            id_ = global_id_++;
            is_gop_ = isgop;
            reset();
        }

        Vertex(const char * label, sparta::Scheduler *scheduler, bool isgop) :
            Vertex(std::string(label), scheduler, isgop)
        {}

        /**
         * \brief Virtual Destructor
         */
        virtual ~Vertex() =default;

        //! \return true if this is global ordering point
        bool isGOP() const {
            return is_gop_;
        }

        //! Place this Vertex as not yet discovered in the sort
        void resetMarker() {
            marker_ = CycleMarker::WHITE;
        }

        //! Completely reset this Vertex for discovery as well as
        //! group ID assignment
        void reset()
        {
            sorted_num_inbound_edges_ = num_inbound_edges_;
            sorting_edges_  = edges_;
            setGroupID(1);
            resetMarker();
        }

        //! Provide a new label for this Vertex
        void setLabel(const std::string & label) {
            label_ = label;
        }

        //! A unique global ID not associated with GroupID
        uint32_t getID() const { return id_; }

        //! Has this Vertex been visited yet?
        bool wasVisited() const {
            //return marker_ == CycleMarker::WHITE;
            return marker_ == CycleMarker::GRAY;
        }

        //! Has this Vertex NOT been visited yet?
        bool wasNotVisited() const {
            return marker_ == CycleMarker::WHITE;
        }

        //! Get Group ID of this Vertex
        Scheduleable::PrecedenceGroup getGroupID() const {
            return  pgid;
        }

        //! Get internal label/name Vertex
        const std::string& getLabel() const { return label_;}

        /**
         * \brief Set the group ID for this Vertex
         * \param gid The group ID assigned to this Vertex
         *
         * This method is virtual for derivers to be notified when
         * the group ID is set by the DAG.  This is useful by
         * classes like Port that need to pass that group ID on to
         * it's internal Events.
         */
        void setGroupID(const PrecedenceGroup& gid) {
            if(scheduleable_ != nullptr){
                scheduleable_->setGroupID(gid);
            }
            pgid = gid;
        }

        /**
         * \brief Transfer GroupID of this GOP to all associated
         *        vertices
         *
         * This is how the DAG sets the height of the tree.
         */
        void transferGID() const
        {
            Scheduleable::PrecedenceGroup gid = getGroupID();
            for (auto i : associates_) {
                sparta_assert(i->isOrphan(),
                            "GOPoint::transferGID() -- Attempt to set GID " << gid
                            << "on non-orphan or assigned object '" << i->getLabel()
                            << "'");
                sparta_assert(i->getGroupID() == 0,
                            "GOPoint::transferGID() -- Attempt to set GID " << gid
                            << "on previously assigned object '" << i->getLabel()
                            << "', previous GID=" << i->getGroupID());
                i->setGroupID(gid);
            }
        }

        /**
         * \brief Have this Vertex precede another
         * \param consumer The Scheduleable to follow this Vertex
         * \param reason The reason for the precedence
         *
         *  \a this will preceed, or come before, the consumer
         *
         */
        void precedes(Scheduleable * consumer, const std::string & reason = "") {
            this->precedes(*consumer, reason);
        }

        void precedes(Scheduleable & s, const std::string & reason = "");

        bool degreeZero() const { return (sorted_num_inbound_edges_ == 0); }
        uint32_t numInboundEdges() const { return num_inbound_edges_; }
        uint32_t numSortedInboundEdges() const { return sorted_num_inbound_edges_; }

        const Edge* getEdgeTo(Vertex * w) const
        {
            assert(w != this);
            const auto & ei = edges_.find(w);
            if (ei == edges_.end()) {
                return nullptr;
            }
            return ei->second;
        }

        const EMap & edges() const { return edges_; }
        const Scheduleable * getScheduleable() const { return scheduleable_; }
        void setScheduleable(Scheduleable * s) { scheduleable_ = s; }
        EMap::size_type numOutboundEdges() const { return edges_.size(); }
        bool isOrphan() const { return ((num_inbound_edges_ == 0) && edges_.empty()); }
        bool isInDAG() { return in_dag_; }
        void setInDAG(bool v) { in_dag_ = v; }

        bool link(EdgeFactory& efact, Vertex * w, const std::string& label="");
        bool unlink(EdgeFactory& efact, Vertex * w);
        void assignConsumerGroupIDs(VList &zlist);
        bool detectCycle();
        bool findCycle(VList& cycle_set);

        // Printable string (called by DAG print routines)
        explicit operator std::string() const
        {
            std::stringstream ss;
            ss << (isGOP() ? "GOP" : "V")
               << "[" << getLabel() << "]:"
               << " id: " << id_
               << ", marker=" << (marker_ == CycleMarker::WHITE ? "white" : (marker_ == CycleMarker::GRAY ? "GRAY" : "black"))
               << ", edges(in=" << numInboundEdges()
               << ", out=" << numOutboundEdges() << ")"
               << ", group: " << getGroupID();
            return ss.str();
        }

        void dumpToCSV(std::ostream& os, bool dump_header=false) const;
        void print(std::ostream& os) const;
        void printFiltered(std::ostream& os, CycleMarker matchingMarker) const;

    protected:
        static uint32_t global_id_;
        PrecedenceGroup pgid = 0;
        bool is_gop_ = false;
        bool in_dag_ = false;

    private:
        Scheduleable *      scheduleable_ = nullptr; // The Scheduleable this Vertex is associated with
        std::string         label_;
        sparta::Scheduler*  my_scheduler_ = nullptr;
        uint32_t            id_ = 0;  // A unique global ID not associated with GroupID
        uint32_t            num_inbound_edges_ = 0;
        EMap                edges_;   // Outbound edges
        uint32_t            sorted_num_inbound_edges_ = num_inbound_edges_; // Number of inbound edges
        EMap                sorting_edges_ = edges_; // temporary copy needed for sorting algorithm
        CycleMarker         marker_ = CycleMarker::WHITE;
        AssociateList       associates_;
    };

    typedef Vertex GOPoint;

    inline Edge::Edge(const Vertex *source, const Vertex *dest,
                      const std::string& label):
        source_(source),
        dest_(dest),
        label_(label)
    {
        id_ = global_id_++;
        std::stringstream ss_id;
        if (label.empty()) {
            std::stringstream ss_lb;
            ss_lb << source->getLabel() << ":" << dest->getLabel();
            label_ = ss_lb.str();
        }
    }

    inline Edge::operator std::string() const
    {
        std::stringstream ss;
        ss << "Edge[" << getLabel() << "]: "
           << source_->getLabel()
           << " -> "
           << dest_->getLabel();
        return ss.str();
    }

    inline void Edge::dumpToCSV(std::ostream& os, bool dump_header) const {
        std::ios_base::fmtflags os_state(os.flags());

        if (dump_header) {
            os << "source_v,dest_v,label" << std::endl;
        }

        os << std::dec << source_->getID()
           << "," << dest_->getID()
           << ",\"" << getLabel() << "\""
           << std::endl;

        os.flags(os_state);
    }

    inline std::ostream& operator<<(std::ostream& os, const Vertex &v)
    {
        v.print(os);
        return os;
    }

} // namespace sparta

#endif
