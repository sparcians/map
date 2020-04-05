// <Checkpoint> -*- C++ -*-

#pragma once

#include <iostream>
#include <sstream>

#include "sparta/functional/ArchData.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/kernel/Scheduler.hpp"

#include "sparta/serialization/checkpoint/CheckpointExceptions.hpp"


namespace sparta {
namespace serialization {
namespace checkpoint
{
    class FastCheckpointer;

    /*!
     * \brief Single checkpoint object interface with a tick number and an ID
     * unique to the owning Checkpointer instance
     *
     * A subclass of Checkpointer is expected to hold or refer to some
     * checkpoint data in memory or on disk at construction which can be
     * restored with load()
     */
    class Checkpoint
    {
    public:

        //! \name Local Types
        //! @{
        ////////////////////////////////////////////////////////////////////////

        //! \brief tick_t Tick type to which checkpoints will refer
        typedef sparta::Scheduler::Tick tick_t;

        //! \brief tick_t Tick type to which checkpoints will refer
        typedef uint64_t chkpt_id_t;

        ////////////////////////////////////////////////////////////////////////
        //! @}

        /*!
         * \brief Indicates the smallest valid checkpoint id
         */
        static const chkpt_id_t MIN_CHECKPOINT = 0;

        /*!
         * \brief Indicates unidentified checkpoint (could mean 'invalid' or
         * 'any') depending on context
         */
        static const chkpt_id_t UNIDENTIFIED_CHECKPOINT = ~(chkpt_id_t)0;


        //! \name Construction & Initialization
        //! @{
        ////////////////////////////////////////////////////////////////////////

        //! \brief Not default constructable
        Checkpoint() = delete;

        //! \brief Not copy constructable
        Checkpoint(const Checkpoint&) = delete;

        //! \brief Non-assignable
        const Checkpoint& operator=(const Checkpoint&) = delete;

    protected:

        /*!
         * \note Should only be constructed by subclasses
         */
        Checkpoint(chkpt_id_t id,
                   tick_t tick,
                   Checkpoint* prev) :
            tick_(tick),
            chkpt_id_(id),
            prev_(prev)
        { }

    public:


        /*!
         * \brief Destructor
         *
         * Removes this checkpoint from the chain and patches chain between prev
         * and each item in the nexts list
         */
        virtual ~Checkpoint() {
            if(getPrev()){
                getPrev()->removeNext(this);
            }

            // Reconnect deltas from this checkpoint to the prev (even if nullptr)
            for(auto& d : getNexts()){
                d->setPrev(getPrev());
                if(getPrev()){
                    getPrev()->addNext(d);
                }
            }

        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        /*!
         * \brief Returns a string describing this object
         */
        virtual std::string stringize() const {
            std::stringstream ss;
            ss << "<Checkpoint id=" << chkpt_id_ << " at t=" << tick_;
            ss << ' ' << getTotalMemoryUse()/1000.0f << "kB (" << getContentMemoryUse()/1000.0f << "kB Data)";
            ss << '>';
            return ss.str();
        }

        /*!
         * \brief Writes all checkpoint raw data to an ostream
         * \param o ostream to which raw data will be written
         * \note No newlines or other extra characters will be appended
         */
        virtual void dumpData(std::ostream& o) const = 0;

        /*!
         * \brief Returns memory usage by this checkpoint including any
         * framework data structures
         */
        virtual uint64_t getTotalMemoryUse() const noexcept = 0;

        /*!
         * \brief Returns memory usage by this checkpoint solely for the
         * checkpointed content.
         */
        virtual uint64_t getContentMemoryUse() const noexcept = 0;

        //! \name Checkpoint Actions
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Attempts to restore this checkpoint state to the simulation
         * state (ArchData) objects given to this Checkpoint at construction
         */
        virtual void load(const std::vector<ArchData*>& dats) = 0;

        /*!
         * \brief Returns the tick number at which this checkpoint was taken.
         */
        tick_t getTick() const noexcept { return tick_; }

        /*!
         * \brief Returns the ID of this checkpoint
         * \note Number has no sequential meaning - it is effectively a random
         * ID.
         */
        chkpt_id_t getID() const noexcept { return chkpt_id_; }

        /*!
         * \brief Gets the representation of this deleted checkpoint as part of
         * a checkpoint chain (if that checkpointer supports deletion)
         */
        virtual std::string getDeletedRepr() const {
            return "*";
        }

        /*!
         * \brief Returns the previous checkpoint. If this checkpoint is a
         * snapshot, it has no previous checkpoint.
         */
        Checkpoint* getPrev() const noexcept {
            return prev_;
        }

        /*!
         * \brief Sets the previous checkpoint of this checkpoint to \a prev
         * \param prev New previou checkpoint. Overwrites previous
         * This will often be accompanied by a call to addNext on the
         * \a prev argument
         */
        void setPrev(Checkpoint* prev) noexcept {
            prev_ = prev;
        }

        /*!
         * \brief Adds another next checkpoint following *this.
         * \param next Next checkpoint (later in simulator ticks) than
         * *this. Cannot be nullptr. Must contain (at a minimum) all state which
         * changed since *this checkpoint was taken. next->getPrev() must
         * be this checkpoint and next->getTick() must be >= this->getTick()
         * \throw CheckpointError if \a next argument is nullptr or already in
         * next checkpoints list or if next->getTick() < this->getTick()
         */
        void addNext(Checkpoint* next) {
            if(!next){
                throw CheckpointError("Cannot specify nullptr in addNext");
            }
            if(next->getPrev() != this){
                throw CheckpointError("Attempting to add a next checkpoint whose previous checkpoint pointer is not 'this'");
            }
            if(next->getTick() < getTick()){
                throw CheckpointError("Attempting to add a next checkpoint whose tick number (")
                    << next->getTick() << " is less than this checkpoint's tick: " << getTick();
            }
            if(std::find(nexts_.begin(), nexts_.end(), next) != nexts_.end()){
                throw CheckpointError("Next argument already present in this checkpoint's nexts_ list. Cannot re-add");
            }
            nexts_.push_back(next);
        }

        /*!
         * \brief Removes a checkpoint following *this because it was deleted.
         * \param next Next checkpoint to remove. Must be found in this
         * object's nexts list. next->getPrev() must be this. Must not be
         * nullptr.
         * \warning Do not call this within a loop of getNexts(). Iterators
         * on that object will be invalidated
         */
        void removeNext(Checkpoint* next) {
            if(!next){
                throw CheckpointError("Cannot specify nullptr in removeNext");
            }
            if(next->getPrev() != this){
                throw CheckpointError("Attempting to remove a next checkpoint whose previous pointer is not 'this'");
            }
            auto itr = std::find(nexts_.begin(), nexts_.end(), next);
            if(itr == nexts_.end()){
                throw CheckpointError("Next argument was not present in this checkpoint's nexts_ list. Cannot remove");
            }
            nexts_.erase(itr);
        }

        /*!
         * \brief Returns next checkpoint following *this. May be an empty
         * vector if there are no later checkpoints following this Checkpoint
         *
         * Returning an empty vector implies that there are no checkpoints
         * descended from this
         */
        const std::vector<Checkpoint*>& getNexts() const noexcept { return nexts_; }

        ////////////////////////////////////////////////////////////////////////
        //! @}

    protected:

        /*!
         * \brief Sets the checkpoint ID.
         */
        void setID_(chkpt_id_t id) {
            chkpt_id_ = id;
        }

    private:

        const tick_t tick_; //!< Tick number for this checkpoint.
        chkpt_id_t chkpt_id_; //!< This checkpoint's ID. Guaranteed to be unique from other checkpoints'

        /*!
         * \brief Next checkpoint (later tick numbers in same forward stream of
         * execution from this checkpoint). Next deltas contains changes
         * following this.
         */
        std::vector<Checkpoint*> nexts_;

        Checkpoint* prev_; //!< Previous checkpoint (earlier) than this. *this contains changes following prev.
    };

} // namespace checkpoint
} // namespace serialization
} // namespace sparta


//! ostream insertion operator for Checkpoint
inline std::ostream& operator<<(std::ostream& o, const sparta::serialization::checkpoint::Checkpoint& dcp){
    o << dcp.stringize();
    return o;
}

//! ostream insertion operator for Checkpoint
inline std::ostream& operator<<(std::ostream& o, const sparta::serialization::checkpoint::Checkpoint* dcp){
    if(dcp == 0){
        o << "null";
    }else{
        o << dcp->stringize();
    }
    return o;
}

//! \brief Required in simulator source to define some globals.
#define SPARTA_CHECKPOINT_BODY                                            \
    namespace sparta{ namespace serialization { namespace checkpoint {    \
        const Checkpoint::chkpt_id_t Checkpoint::MIN_CHECKPOINT;        \
        const Checkpoint::chkpt_id_t Checkpoint::UNIDENTIFIED_CHECKPOINT; \
    }}}

