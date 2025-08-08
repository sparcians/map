// <Checkpoint> -*- C++ -*-

#pragma once

#include "sparta/serialization/checkpoint/CheckpointBase.hpp"

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
    class Checkpoint : public CheckpointBase
    {
    public:

        //! \name Construction & Initialization
        //! @{
        ////////////////////////////////////////////////////////////////////////

        //! \brief Not default constructable
        Checkpoint() = delete;

        //! \brief Not copy constructable
        Checkpoint(const Checkpoint&) = delete;

        //! \brief Non-assignable
        Checkpoint& operator=(const Checkpoint&) = delete;

        //! \brief Not move constructable
        Checkpoint(Checkpoint&&) = delete;

        //! \brief Not move assignable
        Checkpoint& operator=(Checkpoint&&) = delete;

    protected:

        /*!
         * \note Should only be constructed by subclasses
         */
        Checkpoint(chkpt_id_t id,
                   tick_t tick,
                   Checkpoint* prev) :
            CheckpointBase(id, tick),
            prev_(prev)
        { }

    public:


        /*!
         * \brief Removes this checkpoint from the chain and patches chain between prev
         * and each item in the nexts list
         */
        virtual void disconnect() {
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
         * \brief Returns the previous checkpoint. If this checkpoint is a
         * snapshot, it has no previous checkpoint.
         */
        Checkpoint* getPrev() const noexcept {
            return prev_;
        }

        /*!
         * \brief Sets the previous checkpoint of this checkpoint to \a prev
         * \param prev New previous checkpoint. Overwrites previous
         * This will often be accompanied by a call to addNext on the
         * \a prev argument
         */
        void setPrev(Checkpoint* prev) noexcept {
            prev_ = prev;
        }

        /*!
         * \brief Get the ID of our previous checkpoint. Returns UNIDENTIFIED_CHECKPOINT
         * if we have no previous checkpoint, as is the case with the head checkpoint
         * and snapshots.
         */
        chkpt_id_t getPrevID() const override {
            return prev_ ? prev_->getID() : UNIDENTIFIED_CHECKPOINT;
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

        /*!
         * \brief Returns next checkpoint following *this. May be an empty
         * vector if there are no later checkpoints.
         */
        std::vector<chkpt_id_t> getNextIDs() const override {
            std::vector<chkpt_id_t> next_ids;
            for (const auto chkpt : getNexts()) {
                next_ids.push_back(chkpt->getID());
            }
            return next_ids;
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

    private:

        /*!
         * \brief Next checkpoint (later tick numbers in same forward stream of
         * execution from this checkpoint). Next deltas contains changes
         * following this.
         */
        std::vector<Checkpoint*> nexts_;

        Checkpoint* prev_; //!< Previous checkpoint (earlier) than this. *this contains changes following prev.
    };

} // namespace sparta::serialization::checkpoint
