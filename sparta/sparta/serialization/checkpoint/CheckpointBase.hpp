// <CheckpointBase> -*- C++ -*-

#pragma once

#include <iostream>
#include <sstream>

#include "sparta/functional/ArchData.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/kernel/Scheduler.hpp"

#include "sparta/serialization/checkpoint/CheckpointExceptions.hpp"

namespace sparta::serialization::checkpoint
{
    /*!
     * \brief Single checkpoint object interface with a tick number and an ID
     * unique to the owning Checkpointer instance
     *
     * A subclass of Checkpointer is expected to hold or refer to some
     * checkpoint data in memory or on disk at construction which can be
     * restored with load()
     */
    class CheckpointBase
    {
    public:

        //! \name Local Types
        //! @{
        ////////////////////////////////////////////////////////////////////////

        //! \brief tick_t Tick type to which checkpoints will refer
        typedef sparta::Scheduler::Tick tick_t;

        //! \brief tick_t Checkpoint ID type to which checkpoints will refer
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

        //! \brief Not copy constructable
        CheckpointBase(const CheckpointBase&) = delete;

        //! \brief Non-assignable
        const CheckpointBase& operator=(const CheckpointBase&) = delete;

        //! \brief Default move construction
        CheckpointBase(CheckpointBase&&) = default;

        //! \brief Default move assignment
        CheckpointBase& operator=(CheckpointBase&&) = default;

    protected:

        /*!
         * \note Should only be constructed by subclasses
         */
        CheckpointBase(chkpt_id_t id, tick_t tick) :
            tick_(tick),
            chkpt_id_(id)
        { }

        CheckpointBase() = default;

    public:

        /*!
         * \brief Destructor
         */
        virtual ~CheckpointBase() = default;

        /*!
         * \brief boost::serialization support
         */
        template <typename Archive>
        void serialize(Archive& ar, const unsigned int /*version*/) {
            ar & tick_;
            ar & chkpt_id_;
        }

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
         * \brief Get the ID of our previous checkpoint. Returns UNIDENTIFIED_CHECKPOINT
         * only for the head checkpoint.
         */
        virtual chkpt_id_t getPrevID() const = 0;

        /*!
         * \brief Returns next checkpoint following *this. May be an empty
         * vector if there are no later checkpoints.
         */
        virtual std::vector<chkpt_id_t> getNextIDs() const = 0;

        /*!
         * \brief Gets the representation of this deleted checkpoint as part of
         * a checkpoint chain (if that checkpointer supports deletion)
         */
        virtual std::string getDeletedRepr() const {
            return "*";
        }

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
        tick_t tick_;         //!< Tick number for this checkpoint.
        chkpt_id_t chkpt_id_; //!< This checkpoint's ID. Guaranteed to be unique from other checkpoints'
    };

} // namespace sparta::serialization::checkpoint

//! ostream insertion operator for Checkpoint
inline std::ostream& operator<<(std::ostream& o, const sparta::serialization::checkpoint::CheckpointBase& dcp){
    o << dcp.stringize();
    return o;
}

//! ostream insertion operator for Checkpoint
inline std::ostream& operator<<(std::ostream& o, const sparta::serialization::checkpoint::CheckpointBase* dcp){
    if(dcp == 0){
        o << "null";
    }else{
        o << dcp->stringize();
    }
    return o;
}

//! \brief Required in simulator source to define some globals.
#define SPARTA_CHECKPOINT_BODY                                                    \
    namespace sparta{ namespace serialization { namespace checkpoint {            \
        const CheckpointBase::chkpt_id_t CheckpointBase::MIN_CHECKPOINT;          \
        const CheckpointBase::chkpt_id_t CheckpointBase::UNIDENTIFIED_CHECKPOINT; \
    }}}
