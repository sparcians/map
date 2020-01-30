// <State> -*- C++ -*-


#ifndef __STATE__H__
#define __STATE__H__

#include "sparta/simulation/Audience.hpp"
#include "sparta/events/Scheduleable.hpp"
#include "sparta/simulation/StateTracker.hpp"

namespace sparta
{
    namespace app{
        class Simulation;
        class SimulationConfiguration;
    }

    /**
     * \class BoolEnum
     * \brief The class responsible for handling State<bool>
     * instances and converts bool instances into actual EnumTValueType
     * instances to satisfy exisiting function signatures.
     */
    class BoolEnum {
    public:

        /**
         * \class BoolValue
         * \brief This is the internal enum class which holds the enum
         * values for bool instances. This enum has the same layout as
         * any other enum which is expected to be templatized on sparta::State.
         */
        enum class BoolValue : unsigned int {
            __FIRST,
            FALSE = __FIRST,
            TRUE,
            __LAST
        };

        // Default constructor.
        BoolEnum() = default;

        // Constructor to convert any bool instance into an
        // actual BoolEnum instance. This is important because
        // users will pass around bool instances for methods
        // related to sparta::State<bool> instances.
        BoolEnum(const bool val) :
            enum_val_(val ? BoolValue::TRUE : BoolValue::FALSE) {}

        // Constructor to convert any BoolValue enum instance
        // into an actual BoolEnum instance. This is important
        // because the State constructor can take any enum constant
        // and convert it into EnumTValueType instance.
        BoolEnum(const BoolValue& val) :
            enum_val_(val) {}

        // Copy Constructor.
        BoolEnum(const BoolEnum& other) :
            enum_val_(other.enum_val_) {}

        // Move Constructor.
        BoolEnum(BoolEnum&& other) :
            enum_val_(std::move(other.enum_val_)) {}

        // Copy Assignment operator.
        BoolEnum& operator=(const BoolEnum& other) {
            enum_val_ = other.enum_val_;
            return *this;
        }

        // Move Assignment operator.
        BoolEnum& operator=(BoolEnum&& other) {
            enum_val_ = std::move(other.enum_val_);
            return *this;
        }

        // BoolEnum instances should be implicitly converted
        // into integers.
        operator uint32_t() const {
            return static_cast<uint32_t>(enum_val_);
        }

        // BoolEnum instances should be implicitly converted
        // into its internal enum instance type.
        operator BoolValue() const {
            return enum_val_;
        }
    private:

        // Enum instance which holds the true or false state.
        BoolValue enum_val_;
    };

    /**
     * \class State
     * \brief The State class for watching transition between enum states
     *
     * The State classes are intended to encapsulate Object state
     * values (typically in an enum) such that state changes can be
     * observed by other objects -- i.e. via the GoF observer design
     * pattern.
     *
     * State which must be jointly updated by several objects before
     * changing is implemented via state markers. For instance, if a
     * State consists of a READY state in its enum declaration, and
     * there are two rules for a READY state transition, each rule is
     * represented by a State<EnumT>::Marker, which must be set before
     * the state can transition to READY.
     *
     * The State class supports observation of an enum type from one
     * value to another, but the State is exclusive -- it can ONLY be
     * only value at a time.
     *
     * State classes take two template parameters: the Enum type (enum
     * class or just standard enum) and the anticapted maximum number
     * of markers that will be requested -- default is 13.  Because
     * the State class is in the critical path for most performance
     * modeling applications, use of STL types is discouraged.
     */

    // FIAT-1489: Changing markers to accomodate the fixup related change
    template<class EnumT, class MetaDataT = void, uint32_t MAX_MARKERS = 15>
    class State
    {
        //! Forward declaration
        class MarkerSet;

    public:
        //! A peeter to the MetaData
        typedef MetaDataT * MetaDataTPtr;

        //! Convert the EnumT to a EnumT::Value or BoolEnum type.
        typedef typename std::conditional<
            !std::is_same<EnumT, bool>::value,
            typename sparta::utils::Enum<EnumT>::Value,
            BoolEnum>::type
        EnumTValueType;

        //! If EnumT is not an actual enum, we need to use
        //  BoolEnum::BoolValue because there are signatures
        //  which assert on std::is_enum.
        typedef typename std::conditional<
            !std::is_same<EnumT, bool>::value,
            EnumT, BoolEnum::BoolValue>::type
        EnumType;

        ////////////////////////////////////////////////////////////
        //! \class Marker
        //! \brief Class that will mark the state for transition
        class Marker
        {
        private:
            friend MarkerSet;

            //! Reset the marker; called by MarkerSet
            void reset_() {
                marked_ = false;
            }

            // Default constructor only made by MarkerSet
            Marker() = default;

            // Initialize this Marker
            void initialize(MarkerSet * marker_set, const EnumTValueType & val) {
                marker_set_ = marker_set;
                val_ = val;
            }

        public:

            // No copies
            Marker(const Marker &) = delete;

            //! Set the marker
            void set(MetaDataTPtr ptr = nullptr) {
                //XXX Raj thinks this is a valid assert, but it
                //doesn't match the old State behavior
                //sparta_assert(marked_ == false);
                if(!marked_) {
                    marker_set_->jointSet_(ptr);
                    marked_ = true;
                }
            }

            //! Clear the marker
            void clear() {
                //XXX Raj thinks this is a valid assert, but it
                //doesn't match the old State behavior
                //sparta_assert(marked_ == true);
                if(marked_) {
                    // clear the marker set
                    marker_set_->clearMark_();
                    marked_ = false;
                }
            }

            //! Is the marker set? true if so
            bool isMarked() const {
                return marked_;
            }

            /**
             * \brief Return the marker value for this marker (debug use)
             * \return The marker value this marker represents
             */
            const EnumTValueType & getMarkerValue() const {
                return val_;
            }

        private:
            //! The marker set this Marker is part of
            MarkerSet * marker_set_ = nullptr;

            //! Is this marker marked?
            bool marked_ = false;

            //! For debug, the value this marker is assicated with
            EnumTValueType val_;

        }; // class Marker

        /**
         * \class Monitor
         * \brief Monitor a particular State value and allow the user
         * to determine a full state change
         *
         * This class supports the notion that a user wants to
         * intervene in the transition of a State from one enum value
         * to another.  Instead of the State transitioning from State
         * enum value 1 to enum value 2, the monitor will be signalled
         * instead, and it's up to the developer of the Monitor to
         * force the state change (via a call to State::setValue).
         */
        class Monitor
        {
        public:

            //! Expose the State's EnumTValueType
            typedef State::EnumTValueType EnumTValueType;

            //! Method called when the state enum is being changed
            //! \param val The value about to be set
            virtual void signalSet(const EnumTValueType & val, MetaDataTPtr) = 0;

            //! \brief prevent deletion via the base pointer
            virtual ~Monitor() {}
        }; // class Monitor

    private:

        ////////////////////////////////////////////////////////////
        //! \class MarkerSet
        //! \brief Class that will mark the state for transition once all
        //! markers have been set
        class MarkerSet
        {
        public:

            //! Construct MarkerSet
            MarkerSet() {}

            /**
             * \brief Make a new marker for the given enum type
             * \param state The state associated with the marker
             * \return Pointer to the new marker
             */
            Marker * makeMarker(State * state) {
                state_ = state;
                sparta_assert(marker_cnt_ != MAX_MARKERS);
                markers_[marker_cnt_].initialize(this, transition_val_);
                return &markers_[marker_cnt_++];
            }

            /**
             * \brief The number of markers created
             * \return The number of markers created
             */
            uint32_t numMarkers() const {
                return marker_cnt_;
            }

            //! Return the number of marks we have so far
            uint32_t numMarks() const {
                return marked_count_;
            }

            /**
             * \brief The number of markers that have been set
             * \return The number of markers that have been set
             */
            uint32_t numMarked() const {
                uint32_t cnt = 0;
                for(uint32_t i = 0; i < marker_cnt_; ++i) {
                    cnt += markers_[i].isMarked();
                }
                return cnt;
            }

            //! Reset the makerset
            void reset() {
                for(uint32_t i = 0; i < marker_cnt_; ++i) {
                    markers_[i].reset_();
                }
                marked_count_ = 0;
                is_set_ = false;
            }

            //! Enroll an event on this marker set
            void observe(const ScheduleableHandle & ev_hand) {
                audience_.enroll(ev_hand);
            }

            //! Withdraw an event on this marker set
            void withdraw(const ScheduleableHandle & ev_hand) {
                audience_.withdraw(ev_hand);
            }

            void release() {
                audience_.release();
            }

            //! Set the marker threshold that is the lower bound of
            //! markers necessary to fire an observation/change
            void setThreshold(uint32_t thresh) {
                marked_thresh_ = thresh;
            }

            //! Create, associate, and return a Monitor
            void attachMonitor(Monitor * mon) {
                monitors_.emplace_back(mon);
            }

            //! Remove a monitor
            void detachMonitor(Monitor *mon)
            {
                auto i = std::find(monitors_.begin(), monitors_.end(), mon);
                if (i != monitors_.end()) {
                    monitors_.erase(i);
                }
            }

            //! Notify observers explicity for this marker set.  There
            //! are times when the State Value is set directly
            //! bypassing markers
            void notifyObservers() {
                audience_.notify();
                is_set_ = true;
            }

            //! Has this marker set been set?
            //! \return true if set, false otherwise
            bool isSet() const {
                return is_set_;
            }

        private:

            //! Make the marker a friend to set/clear the MarkerSet
            friend Marker;

            //! Make the State class a friend
            friend State;

            //! Join the set of markers
            void jointSet_(MetaDataTPtr ptr = nullptr) {
                ++marked_count_;
                if (SPARTA_EXPECT_FALSE(!monitors_.empty())) {
                    for (auto& i : monitors_) {
                        i->signalSet(transition_val_, ptr);
                    }
                    //} else if ((markers_.size() - marked_count_) <= marked_thresh_) {
                } else if ((marker_cnt_ - marked_count_) <= marked_thresh_) {
                    state_->setValue_(transition_val_);
                    notifyObservers();
                    is_set_ = true;
                }

            }

            //! Clear one marked count
            void clearMark_() {
                sparta_assert(marked_count_ != 0);
                --marked_count_;
            }

            // MarkerSet privates
            uint32_t                marked_count_  = 0;
            uint32_t                marked_thresh_ = 0;
            //std::list<Marker>       markers_;
            Marker                  markers_[MAX_MARKERS];
            uint32_t                marker_cnt_ = 0;
            State                  *state_ = nullptr;
            EnumTValueType          transition_val_;       //<! The value to transition to
            Audience                audience_;
            bool                    is_set_ = false;
            std::vector<Monitor *>  monitors_;
        }; // class MarkerSet

        //! Friend the MarkerSet to set the value on the State when
        //! all markers have been set
        friend class MarkerSet;

        //! Internal method to set the value -- called by State and MarkerSet
        void setValue_(const EnumTValueType & val) {
            current_state_ = val;
        }

        //! The behaviour of State<bool> requires it to be set to False after
        //  construction. There are tests which assert that. So, this method
        //  when called, checks if this current instance is a State<bool> or not,
        //  and depending on that, sets the value to FALSE.
        template<typename U = EnumT>
        typename std::enable_if<std::is_same<U, bool>::value, void>::type
        attemptSetValue_() {
            setValue(BoolEnum::BoolValue::FALSE);
        }

        //! The behaviour of State<bool> requires it to be set to False after
        //  construction. There are tests which assert that. So, this method
        //  when called, checks if this current instance is a State<bool> or not,
        //  and depending on that, sets the value to FALSE. If the current context
        //  is not tempaltized on bool, then do nothing.
        template<typename U = EnumT>
        typename std::enable_if<!std::is_same<U, bool>::value, void>::type
        attemptSetValue_() {}

    public:
        ////////////////////////////////////////////////////////////
        // State Class

        /**
         * \brief Construct a State class
         * \param initial_value The initial value; __FIRST is not given
         *
         * \note Ensures that the EnumTValueType is a true enum or
         *       class enum.  During construction of a State instance,
         *       we construct its internal state tracker unit. If
         *       tracking is disabled, a nullptr is returned, whereas,
         *       when it is enabled, a live state tracker pointer is
         *       given, ready to start tracking states.
         */
        State(const EnumTValueType & initial_value = EnumType::__FIRST) :
            initial_value_(initial_value),
            current_state_(initial_value),

            //! StatePoolManager is the access point for the whole State Tracking
            // procedure. The sparta::State class asks the StatePoolManager to dispatch
            // a State Tracker Unit, templatized on the same enum type as itself, by
            // calling the dispatchNewTracker() API. Inside the dispatchNewTracker() API,
            // the pool manager looks at all the StatePool template instantiations that it
            // manages, to check if it has one instantiation which is templated on the same
            // type as requested right here. If it has, then we go to the second step of
            // taking that StatePool handle and demand a new StateTrackerUnit out of it.
            // The StatePool instance looks into its internal Tracker Queue to see if it
            // has any available, ready-to-go tracker unit for dispatching. If yes, then
            // it dispatches a std::unique_ptr to a new tracker unit back to the caller's
            // side, which is sparta::State and sparta::State moves that into it's internal
            // member variable. If the internal Tracker Queue does not have any tracker
            // unit to go, the StatePool creates one from heap right then and dispatches
            // it. If the Pool Manager does not have a StatePool instantiation of the
            // requested enum type (which happens only on the first call for an enum type),
            // the pool manager creates a new StatePool template instantiation and pushes it
            // in its internal map, along with a unique ID, which separates all the different
            // StatePool instantiations form each other.
            state_tracker_unit_(std::move(
                tracker::StatePoolManager::getInstance().
                    dispatchNewTracker<EnumType>()))
        {

            // If tracking is disabled, this internal
            // tracker unit will be nullptr. In that case,
            // we do not track anything.
            if(state_tracker_unit_) {
                state_tracker_unit_->startState(initial_value_);
            }

            for(uint32_t i = 0; i < EnumTValueType(EnumType::__LAST); ++i) {
                marker_set_[i].transition_val_ = static_cast<EnumType>(i);
            }
            static_assert(std::is_enum<EnumType>::value,
            "ERROR: State classes must be templatized on a valid enum or enum class type");
            attemptSetValue_();
        }

        //! No copies of State are allowed
        State(const State &) = delete;

        //! Deleting default assignment operator to prevent copies
        State & operator = (const State &) = delete;

        //! Virtual destructor
        virtual ~State() {}

        //! Get the current value of the state
        const EnumTValueType & getValue() const {
            return current_state_;
        }

        const EnumType & getEnumValue() const {
            return (current_state_.getValue()).getEnum();
        }

        //! This methods returns the amount of time in Scheduler
        //! Ticks, this sparta::State instance has been residing in
        //! its current state.  Requires StateTracking to be enabled.
        Scheduler::Tick getTimeInState() const {
            sparta_assert(state_tracker_unit_ != nullptr,
                          "This method can only be called on this State class with tracking enabled");
            return state_tracker_unit_->getActiveTime();
        }

        /**
         * \brief Set a new enum value explicit and fire observers
         * \param val The value to set
         */
        void setValue(const EnumTValueType & val) {

            //! If tracking is disabled, this internal
            // tracker unit will be nullptr. In that case,
            // we do not track anything.
            if(state_tracker_unit_) {
                state_tracker_unit_->startState(val);
            }
            setValue_(val);

            // Notify audience members of state change.  The observers
            // are registered with the Audience in the marker set for
            // that value type
            marker_set_[val].notifyObservers();
        }

        /**
         * \brief Reset this State class
         *
         * Clears all markers and puts the current_state_ to the
         * initial_value given in the constructor
         */
        void reset() {
            current_state_ = static_cast<EnumTValueType>(initial_value_);
            for(auto & ms : marker_set_) {
                ms.reset();
            }
            attemptSetValue_();
        }

        /**
         * \brief Reset this State class to the given value
         * \param val The value to reset the State to
         *
         * Clears all markers and puts the current_state_ to the given
         * value
         */
        void reset(const EnumTValueType & val) {
            current_state_ = val;
            for(auto & ms : marker_set_) {
                ms.reset();
            }
        }

        ////////////////////////////////////////////////////////////
        // \defgroup Assignment/comparison
        //@{
        /**
         * \brief Assign a new state
         * \param val The enum to assign to this state
         */
        void operator=(const EnumTValueType& val) {
            setValue(val);
        }

        /**
         * \param rhs_val The right hand side
         * \return True if equal
         */
        bool operator==(const EnumTValueType& rhs_val) const {
            return (getValue() == rhs_val);
        }

        /**
         * \param rhs_val The right hand side
         * \return True if equal
         */
        bool operator!=(const EnumTValueType& rhs_val) const {
            return (getValue() != rhs_val);
        }

        //@}

        ////////////////////////////////////////////////////////////
        // \defgroup Marker methods
        //@{

        /**
         * \brief Get a new marker for the enum type
         * \param val The enum type to get a marker for
         * \return A new marker
         */
        Marker* newMarker(const EnumTValueType & val) {
            return marker_set_[val].makeMarker(this);
        }

        /**
         * \brief Get the number of marks for the enum type
         * \param val The enum type to query
         * \return The number of marks set for the enum type
         */
        uint32_t numMarks(const EnumTValueType & val) const {
            return marker_set_[val].numMarks();
        }

        /**
         * \brief Get the number of markers for the enum type
         * \param val The enum type to query
         * \return The number of marks set for the enum type
         */
        uint32_t numMarkers(const EnumTValueType & val) const {
            return marker_set_[val].numMarkers();
        }

        /**
         * \brief For a particular value, have all the marks been made
         * \param val The value to query
         * \return true if all markers have returned and marked
         */
        bool complete(const EnumTValueType & val) const {
            return (numMarks(val) == numMarkers(val));
        }

        /**
         * \brief Set the marker set threshold for the given state value
         * \param val The value to set the threshold on
         * \param thresh The threshold amount
         *
         * Sets the minimum number of markers that must "call in"
         * before the state transitions to that new value.
         */
        void setMarkedThreshold(const EnumTValueType & val, uint32_t thresh) {
            marker_set_[val].setThreshold(thresh);
        }

        //@}

        ////////////////////////////////////////////////////////////
        // \defgroup Condition methods, API
        //@{

        /**
         * \brief Determine if the State was ever set to value val
         * \param val The value to query
         * \return true if was set to that value
         */
        bool isSet(const EnumTValueType & val = BoolEnum::BoolValue::TRUE) const
        {
            return (marker_set_[val].isSet() == true);
        }

        /**
         * \brief Determine if the State value is clear (never set)
         * \param val The value to query
         * \return true if the state never reached this value
         */
        bool isClear(const EnumTValueType & val = BoolEnum::BoolValue::TRUE) const
        {
            return (marker_set_[val].isSet() == false);
        }

        //@}

        ////////////////////////////////////////////////////////////
        // \defgroup Observation methods
        //@{

        /**
         * \brief Observe this state, specifically, when it
         * transitions to the given enum val
         * \param val The value to observe
         * \param ev_hand The Scheduleable to schedule when the observation is made
         */
        void observe(const EnumTValueType & val,
                     const ScheduleableHandle & ev_hand)
        {
            marker_set_[val].observe(ev_hand);
        }

        /**
         * \brief withdraw event from this state
         * \param val The value to observe
         * \param ev_hand The Scheduleable that needs to be withdrawn
         */
        void withdraw(const EnumTValueType & val,
                     const ScheduleableHandle & ev_hand)
        {
            marker_set_[val].withdraw(ev_hand);
        }

        void release(const EnumTValueType & val)
        {
            marker_set_[val].release();
        }

        //@}

        ////////////////////////////////////////////////////////////
        // \defgroup Monitor methods
        //@{

        /**
         * \brief Attach a Monitor to a state value
         * \param val The value to associate with the Monitor
         * \param mon The monitor to attach
         *
         * Attach a Monitor that will be called when the state value
         * (val) is up for selection
         */
        void attachMonitor(const EnumTValueType & val, Monitor * mon) {
            marker_set_[val].attachMonitor(mon);
        }

        /**
         * \brief Decouple a monitor from a state value
         * \param val The Enum value to detach from
         * \param mon The monitor to remove
         */
        void detachMonitor(const EnumTValueType & val, Monitor *mon)
        {
            marker_set_[val].detachMonitor(mon);
        }

        //@}

#ifndef DO_NOT_DOCUMENT
        // Debug API for testing only.
        // This only tests the stats accumulated by a
        // single sparta::State instance.
        const std::vector<sparta::Scheduler::Tick> & getRawAccumulatedTime() const {
            sparta_assert(state_tracker_unit_ != nullptr);
            return state_tracker_unit_->getStateSet().state_delta_set;
        }
#endif

    private:
        ////////////////////////////////////////////////////////////
        // State Privates
        const EnumType initial_value_;
        sparta::utils::ValidValue<EnumTValueType> current_state_;
        MarkerSet marker_set_[static_cast<uint32_t>(EnumType::__LAST)];

        //! Personal State Tracker Unit for an instance of this class.
        // This tracker is a unique pointer with its own deleter which
        // knows what to do, when deleted.
        tracker::state_tracker_ptr<EnumType> state_tracker_unit_;
    }; // class State

    // SimulationConfiguration which holds the state tracker
    // name. The Simulation class contains an instance of this
    // state template. When Simulation is being torn down,
    // the destructor  of this class is invoked and then in turn
    // we start doing all the data collection and processing from
    // each and every state tracker unit ever used, which are all
    // queued up in their respective queues.
    template<typename T>
    class State<T,
        typename std::enable_if<
            std::is_same<T, sparta::PhasedObject::TreePhase>::value>::type> {
    public:
        State(sparta::app::Simulation * sim);
        void configure();
        ~State();

    private:
        sparta::app::Simulation * sim_ = nullptr;
    };
}
// __STATE__H__
#endif
