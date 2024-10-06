// <StateTimerUnit> -*- C++ -*-


#pragma once

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/statistics/InstrumentationNode.hpp"
#include "sparta/functional/DataView.hpp"
#include "sparta/utils/ByteOrder.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/statistics/Histogram.hpp"
#include "sparta/utils/Enum.hpp"
#include "sparta/statistics/EnumHistogram.hpp"
#include "sparta/utils/ValidValue.hpp"

namespace sparta
{

// type for the shared_ptr of a map of vectors. The map use state_set_hash_code,
// the vector contains the state_set_delta_ for each StateSet
typedef std::shared_ptr<std::unordered_map<uint32_t, std::vector<sparta::Clock::Cycle> *>>
        StateTimerDataContainerPtr;

/**
 * \class StateTimerUnit
 * \brief A high level wrapper contains the StateTimerPool and StateTimerHistogram
 */
class StateTimerUnit : public TreeNode
{
private:
    // forward declaration
    class StateTimerPool;

public:

    /**
     *  \class StateTimer
     *  \brief The timer that user can access to start/end tracking state.
     *         A StateTimer::Handle is returned from StateTimerUnit upon
     *         allocation.
     */
    class StateTimer
    {
        friend class StateTimerPool;

    public:

        typedef uint32_t TimerId;
        typedef std::shared_ptr<StateTimer> Handle;
        typedef std::shared_ptr<std::unordered_map<uint32_t, uint32_t>> StateSetInfo;

        ~StateTimer() {}

        /**
         * \brief Start timing state
         * \param state_enum The enum of the state
         */
        template<class EnumClassT>
        void startState(EnumClassT state_enum)
        {
            uint32_t state_set_hash_code;
            uint32_t state_index;
            uint32_t active_state_in_set;
            std::shared_ptr<StateSet> state_set;

            // using state_set_hash_code as state set identifier,
            // and state_index for state in the set.
            state_set_hash_code = typeid(EnumClassT).hash_code();
            state_index = static_cast<uint32_t>(state_enum);

            // find the state set in the timer.
            auto it = state_set_map_.find(state_set_hash_code);
            sparta_assert(it != state_set_map_.end(),
                    "Can not find state enum class in timer.");
            state_set = it->second;

            if (state_set->active_state_index_.isValid())
            {
                active_state_in_set = state_set->active_state_index_.getValue();
                sparta_assert(active_state_in_set != state_index ,
                        "State aleady started");
                sparta_assert(state_set->active_state_starting_time_ > 0,
                        "Wrong active state starting time.");

                // implicitly update the time delta of the active state in the set, if there is one.
                endTimerState_(state_set);
            }

            // set the new state to the active state in set.
            sparta_assert(state_index < state_set->state_set_delta_.size(),
                    "State enum out of range.")
            state_set->active_state_index_ = state_index;
            state_set->active_state_starting_time_ = clk_->currentCycle();
        }

        /**
         * \brief End timing state
         * \param state_enum The enum of the state
         */
        template<class EnumClassT>
        void endState(EnumClassT state_enum)
        {
            uint32_t state_set_hash_code;
            uint32_t state_index;
            int32_t active_state_in_set;
            std::shared_ptr<StateSet> state_set;

            // using state_set_hash_code as state set identifier,
            // and state_index for state in the set.
            state_set_hash_code = typeid(EnumClassT).hash_code();
            state_index = static_cast<uint32_t>(state_enum);

            // find the state set in the timer.
            auto it = state_set_map_.find(state_set_hash_code);
            sparta_assert(it != state_set_map_.end(),
                    "Can not find state enum class in timer.");
            state_set = it->second;

            // the end state needs to be the active one
            sparta_assert(state_set->active_state_index_.isValid() ,
                    "No active state in the set when endState.");
            active_state_in_set = state_set->active_state_index_.getValue();
            sparta_assert((uint32_t)active_state_in_set==state_index,
                    "State does not match active state in the set when endState.");

            // end the state
            endTimerState_(state_set);

            state_set->active_state_index_.clearValid();
            state_set->active_state_starting_time_ = 0;
        }


        /**
         * \brief Assignment for starting a state in the timer
         * \param The enum of the state need to be started
         */
        template<class EnumClassT>
        void operator=(EnumClassT state_enum)
        {
            startState(state_enum);
        }

    private:

        /**
         * \struct StateSet
         * \brief Contains the delta time info for a set of states (one enum class)
         */
        struct StateSet
        {
            //  The state index for the active state
            utils::ValidValue<uint32_t> active_state_index_;
            //  The starting time for the active state
            sparta::Clock::Cycle active_state_starting_time_;
            //  Delta time of all the states
            std::vector<sparta::Clock::Cycle> state_set_delta_;

            StateSet(uint32_t num_state):
                active_state_starting_time_(0),
                state_set_delta_(std::vector<sparta::Clock::Cycle>(num_state, 0))
            {
                // Initial active_state_index_ to invalid
                active_state_index_.clearValid();
            }
        };

        /*!
         * \brief StateTimer constructor
         *
         * \param clk The pointer to Clock for the timer to get timestamp
         * \param timer_id The id, also the vector index in the timer pool
         * \param state_set_info_map_ptr A map using the state set hashcode,
         *                               and contains the number of states in the set,
         *                               used to initialize the timers
         * \param state_timer_unit_ptr A pointer to the StateTimerUnit,
         *                             used when query or release
         */
        StateTimer(const sparta::Clock * clk, TimerId timer_id,
                StateSetInfo state_set_info_map_ptr,
                StateTimerUnit * state_timer_unit_ptr):
            clk_(clk),
            timer_id_(timer_id),
            state_timer_unit_ptr_(state_timer_unit_ptr),
            last_query_time_(0)
        {
            // use state_set_info_map_ptr to initial state_set_map_.
            for (auto it = state_set_info_map_ptr->begin(); it != state_set_info_map_ptr->end(); ++it)
            {
                state_set_map_.insert(std::map<uint32_t, std::shared_ptr<StateSet>>::value_type
                        (it->first, std::shared_ptr<StateSet>(new StateSet(it->second))));
            }
        }

        /**
         * \brief End the timer, used in start, end, release function
         *        the histogram is updated.
         */
        void endTimerState_(std::shared_ptr<StateSet> &state_set)
        {
            uint32_t active_state_in_set = state_set->active_state_index_.getValue();
            if (state_set->active_state_starting_time_ > last_query_time_)
            {
                state_set->state_set_delta_[active_state_in_set] +=
                        clk_->currentCycle() -  state_set->active_state_starting_time_;
            }
            else
            {
                state_set->state_set_delta_[active_state_in_set] +=
                        clk_->currentCycle() -  last_query_time_;
            }
        }

        /**
         * \brief Query the timer, used for dynamic query. The delta time of each state is checked and
         *        the histogram is updated.
         */
        void queryStateTimer_();

        /**
         * \brief Release the timer to StateTimerPool, can not be called by user.
         *        Only called when the StateTimer::Handle is deleted.
         */
        void releaseStateTimer_();

        /**
         * \struct CustomDeleter
         * \brief Custom deleter for StateTimer::Handle, checks it the timer outlives the pool
         */
        struct CustomDeleter
        {
        public:
            CustomDeleter(const std::weak_ptr<StateTimerPool> & pool_ptr):
                state_timer_pool_ptr_(pool_ptr)
            {}

            void operator()(StateTimer *ptr)
            {
                if(!state_timer_pool_ptr_.expired())
                {
                    ptr->releaseStateTimer_();
                }
            }
        private:
            std::weak_ptr<StateTimerPool> state_timer_pool_ptr_;
        };

        // sparta::Clock used to get current time
        const sparta::Clock * clk_ = nullptr;
        // The Id of the StateTimer, came from the index in the timer list vector in StateTimerPool
        TimerId timer_id_;
        // Pointer to StateTimerUnit, used when query or release
        StateTimerUnit * state_timer_unit_ptr_;
        // A map contains all the state sets
        std::unordered_map<uint32_t, std::shared_ptr<StateSet>> state_set_map_;
        // last query time used for dynamic query
        sparta::Clock::Cycle last_query_time_;

    }; // class StateTimer

    /*!
     * \brief StateTimerUnit constructor
     *
     * \param parent The parent of StateTimerUnit
     * \param state_timer_unit_name The name string of the state timer unit
     * \param description The description string of the state timer unit
     * \param num_timer_init The initial number of StateTimers in pool
     * \param lower Lower value of histogram
     * \param upper Upper value of histogram
     * \param bin_size Bin size of histogram
     * \param state_sets arbitrary number of state set as enum class
     */
    template<class... ArgsT>
    StateTimerUnit(TreeNode * parent,
                   std::string state_timer_unit_name,
                   std::string description,
                   uint32_t num_timer_init,
                   uint32_t lower,
                   uint32_t upper,
                   uint32_t bin_size,
                   ArgsT...state_sets);

    ~StateTimerUnit()
    {
        state_timer_pool_ptr_->releaseAllActiveTimer();
    }

    /**
    * \brief Allocate a StateTimer, used by user
    * \return StateTimer::Handle of the allocated StateTimer
    */
    StateTimer::Handle allocateStateTimer()
    {
        return state_timer_pool_ptr_->allocateTimer();
    }

    /**
    * \brief Dynamically query the timers, histograms will be updated, used by user
    */
    std::string dynamicQuery()
    {
        state_timer_pool_ptr_->queryAllActiveTimer();
        return state_timer_histogram_ptr_->getDisplayStringCumulativeAllState();
    }

    /**
    * \brief Dynamically query one state of all timers, histograms will be updated, used by user
    */
    template<class EnumClassT>
    std::string dynamicQuery(EnumClassT state_enum)
    {
        uint32_t state_set_hash_code;
        uint32_t state_index;

        state_set_hash_code = typeid(EnumClassT).hash_code();
        state_index = static_cast<uint32_t>(state_enum);

        state_timer_pool_ptr_->queryAllActiveTimer();
        return state_timer_histogram_ptr_->getDisplayStringCumulativeOneState(state_set_hash_code, state_index);
    }

private:

    /**
     *  \class StateTimerPool
     *  \brief A pool to maintain all the StateTimers, as well as active and available ones.
     */
    class StateTimerPool
    {
    public:

        /*!
         * \brief StateTimerPool constructor
         *
         * \param parent The parent of StateTimerUnit
         * \param state_set_info_map_ptr A map using the state set hashcode,
         *                               and contains the number of states in the set,
         *                               used to initialize the timers
         * \param state_timer_unit_ptr The pointer to StateTimerUnit, where the pool belongs to
         * \param num_state_timer_init Initial number of total StateTimers in pool, used as incremental interval
         */
        StateTimerPool(TreeNode * parent,
                StateTimer::StateSetInfo state_set_info_map_ptr,
                StateTimerUnit * state_timer_unit_ptr,
                uint32_t num_state_timer_init);

        /**
         * \brief Allocate a StateTimer from the pool
         * \return StateTimer::Handle of the allocated StateTimer
         */
        StateTimer::Handle allocateTimer()
        {
            StateTimer::TimerId timer_id;
            // check avalability in timer_list
            if(available_timer_vec_ptr_->size()==0)
            {
                // create more time if not exceed MAX_NUM_STATETIMER
                uint32_t current_num_timer = timer_list_ptr_->size();
                sparta_assert(current_num_timer < MAX_NUM_STATETIMER,
                        "No timer available, pool exceeds MAX capacity.")
                std::cout << "Warining: "<< current_num_timer <<
                        " StateTimers are inflight, creating more." << std::endl;
                for (uint32_t i = current_num_timer; i < current_num_timer + num_state_timer_init_; i++)
                {
                    timer_list_ptr_->push_back(StateTimer::Handle
                                (new StateTimer(clk_, i, state_set_info_map_ptr_, state_timer_unit_ptr_)));
                    available_timer_vec_ptr_->push_back(std::make_pair(i, timer_list_ptr_->at(i)));
                }
                sparta_assert(current_num_timer + num_state_timer_init_ == timer_list_ptr_->size(),
                        "Number of Timers created does not add up.")
            }

            timer_id = available_timer_vec_ptr_->back().first;
            active_timer_map_ptr_->insert(std::map<StateTimer::TimerId, StateTimer::Handle>::value_type
                            (timer_id, available_timer_vec_ptr_->back().second));
            available_timer_vec_ptr_->pop_back();
            sparta_assert(active_timer_map_ptr_->size() + available_timer_vec_ptr_->size()
                    == timer_list_ptr_->size(), "Number of Timers does not add up.")
            return StateTimer::Handle(timer_list_ptr_->at(timer_id).get(), StateTimer::CustomDeleter(my_life_));
        }

        /**
         * \brief Release the StateTimer to the pool
         * \param timer_id the Id of released StateTimer
         */
        void releaseTimer(StateTimer::TimerId timer_id)
        {
            // check it should be in the active timer map
            auto it = active_timer_map_ptr_->find(timer_id);
            sparta_assert(it != active_timer_map_ptr_->end(),
                    "Timer not in active timer map when release. ");
            available_timer_vec_ptr_->push_back(std::make_pair(timer_id, it->second));
            active_timer_map_ptr_->erase(it);
            sparta_assert(active_timer_map_ptr_->size() + available_timer_vec_ptr_->size()
                    == timer_list_ptr_->size(), "Number of Timers does not add up.")
        }

        /**
         * \brief Query all the active StateTimers in the pool
         */
        void queryAllActiveTimer();

        /**
         * \brief Release all the active StateTimers in the pool, used when destruct StateTimerUnit
         */
        void releaseAllActiveTimer();

    private:

        // pointer to a vector of all StateTimer
        std::unique_ptr<std::vector<StateTimer::Handle>> timer_list_ptr_;
        // pointer to a map of active StateTimer: <timer_id, StateTimer pointer>
        std::unique_ptr<std::unordered_map<StateTimer::TimerId, StateTimer::Handle>>
            active_timer_map_ptr_;
        // pointer to a vector of available StateTimer: <timer_id, StateTimer pointer>
        std::unique_ptr<std::vector<std::pair<StateTimer::TimerId, StateTimer::Handle>>>
            available_timer_vec_ptr_;
        // A shared_ptr point to it self, used to track its life time
        std::shared_ptr<StateTimerPool> my_life_;
        // Initial number of total StateTimers in pool, used as incremental interval
        uint32_t num_state_timer_init_;
        // A map uses the state set hash_code, contains the number of state in the set
        StateTimer::StateSetInfo state_set_info_map_ptr_;
        // sparta::Clock used to get current time
        const sparta::Clock * clk_ = nullptr;
        // Pointer to StateTimerUnit, used when query or release
        StateTimerUnit * state_timer_unit_ptr_;
        // Max number of StateTimers in pool
        const uint32_t MAX_NUM_STATETIMER = 10000;

    };  // class StateTimerPool

    /**
     * \class StateTimerHistogram
     * \brief Maintains all the sparta::Histogram used. Each state has on sparta::Histogram
     */
    class StateTimerHistogram
    {
    public:

        /*!
         * \brief StateTimerHistogram constructor
         *
         * \param parent The parent of StateTimerUnit, passed to each histogram
         * \param state_timer_unit_name The name string of the state timer unit
         * \param state_set_name_map A map contains the name string of each state set
         * \param state_timer_data_container_ptr The pointer to the timer data container,
         *                                       which is used for passing the timer delta value
         * \param state_set_info_map_ptr A map using the state set hashcode,
         *                               and contains the number of states in the set,
         *                               used to initialize the histogram map
         * \param lower Lower value of histogram
         * \param upper Upper value of histogram
         * \param bin_size Bin size of histogram
         */
        StateTimerHistogram(TreeNode * parent,
                std::string state_timer_unit_name,
                std::unordered_map<uint32_t, std::string> state_set_name_map,
                StateTimerDataContainerPtr state_timer_data_container_ptr,
                StateTimer::StateSetInfo state_set_info_map_ptr,
                uint32_t lower, uint32_t upper, uint32_t bin_size):
            state_timer_data_container_ptr_(state_timer_data_container_ptr)
        {
            // use state_set_info_map_ptr  to initial timer_histogram_map_, they have the same states.
            for (auto it = state_set_info_map_ptr->begin(); it != state_set_info_map_ptr->end(); ++it)
            {
                std::shared_ptr<std::vector<std::shared_ptr<sparta::Histogram>>>
                        histogram_vector(new std::vector<std::shared_ptr<sparta::Histogram>>);
                for (uint32_t i = 0; i < it->second; i++)
                {
                    histogram_vector->push_back(std::shared_ptr<sparta::Histogram>
                            (new sparta::Histogram(parent,
                            state_timer_unit_name + "_histogram_set_" + state_set_name_map[it->first] +
                            "_state_" + std::to_string(i), "state timer histogram" ,
                            /*lower*/ lower, /*upper*/ upper , /*bin size*/ bin_size)));
                }
                timer_histogram_map_.insert(std::unordered_map<uint32_t,
                        std::shared_ptr<std::vector<std::shared_ptr<sparta::Histogram>>>>::value_type
                        (it->first, histogram_vector));
            }
        }

        /**
        * \brief Update the histograms using the info in the container
        */
        void updateHistogram()
        {
            for (auto it = timer_histogram_map_.begin(); it != timer_histogram_map_.end(); ++it)
            {
                for (uint32_t i = 0; i < it->second->size(); i++)
                {
                    // add the delta of each state in the container to histogram, then reset the container.
                    // Since the container has the pointers to the timer, this will actually reset the timer.
                    (*it->second)[i]->addValue((*(*state_timer_data_container_ptr_)[it->first])[i]);
                    (*(*state_timer_data_container_ptr_)[it->first])[i] = 0;
                }
            }
        }

        /**
        * \brief Get the cumulative histogram string of all states
        */
        std::string getDisplayStringCumulativeAllState()
        {
            std::string histogram_string = "";
            for (auto it = timer_histogram_map_.begin(); it != timer_histogram_map_.end(); ++it)
            {
                for (uint32_t i = 0; i < it->second->size(); i++)
                {
                    histogram_string += (*it->second)[i]->getDisplayStringCumulative();
                }
            }
            return histogram_string;
        }

        /**
        * \brief Get the cumulative histogram string of one state
        */
        std::string getDisplayStringCumulativeOneState(uint32_t state_set_hash_code, uint32_t state_index)
        {
            std::string histogram_string = "";
            auto it = timer_histogram_map_.find(state_set_hash_code);
            sparta_assert(it != timer_histogram_map_.end(),
                    "Can not find state enum class in histogram map.");
            histogram_string = (*it->second)[state_index]->getDisplayStringCumulative();
            return histogram_string;
        }

    private:
        // A map of sparta::Histogram, using the hash_code of each state set (enum class), each set has
        // a vector of sparta::Histogram, each of them represent a state in the set.
        std::unordered_map<uint32_t, std::shared_ptr<std::vector<std::shared_ptr<sparta::Histogram>>>>
                timer_histogram_map_;
        // A pointer to the container used for updating the histograms
        StateTimerDataContainerPtr state_timer_data_container_ptr_;
    };  // class StateTimerHistogram

    /**
    * \brief Add arbitrary number of state set when construct the StateTimerUnit
    * \param state_sets_head, state_sets_tail... Arbitrary number of enum class
    *                                            each class is a state set
    */
    template<class ArgsHeadT, class... ArgsTailT>
    void addStateSet_(ArgsHeadT state_sets_head, ArgsTailT... state_sets_tail)
    {
        uint32_t state_set_hash_code = typeid(state_sets_head).hash_code();
        uint32_t num_state = static_cast<uint32_t>(state_sets_head);
        sparta_assert(state_set_info_map_ptr_->find(state_set_hash_code) == state_set_info_map_ptr_->end(),
                "Same enum class exists.");
        state_set_info_map_ptr_->insert(std::unordered_map<uint32_t,uint32_t>::value_type
                (state_set_hash_code, num_state));
        // using typeid().name() as state set name for now
        state_set_name_map_.insert(std::unordered_map<uint32_t, std::string>::value_type
                (state_set_hash_code, typeid(state_sets_head).name()));

        addStateSet_(state_sets_tail...);
    }

    void addStateSet_()
    {}

    /**
    * \brief The interface for StateTimer to update histograms, StateTimer only call this once
    *        to update all the states
    * \param timer_id The id of the StateTimer wants to update histograms
    * \param is_release_timer Whether release the StateTimer after updating the histograms
    */
    void updateStateHistogram_(StateTimer::TimerId timer_id, bool is_release_timer)
    {
        // update the histogram
        state_timer_histogram_ptr_->updateHistogram();
        // release timer if needed
        if (is_release_timer)
        {
            state_timer_pool_ptr_->releaseTimer(timer_id);
        }
    }

    /**
    * \brief Pass the container to StateTimer
    * \return The pointer of the container
    */
    StateTimerDataContainerPtr getStateTimerInfoContainer_()
    {
        return state_timer_data_container_ptr_;
    }

    // The pointer to the container
    StateTimerDataContainerPtr state_timer_data_container_ptr_;
    // A map uses the state set hash_code, contains the number of state in the set
    StateTimer::StateSetInfo state_set_info_map_ptr_;
    // The pointer to StateTimerPool
    std::unique_ptr<StateTimerPool> state_timer_pool_ptr_;
    // The pointer to StateTimerHistogram
    std::unique_ptr<StateTimerHistogram> state_timer_histogram_ptr_;
    // A map uses the state set hash_code, contains the name string of each set
    std::unordered_map<uint32_t, std::string> state_set_name_map_;

}; // class StateTimerUnit

/**
 * \brief Query the timer, used for dynamic query. The delta time of each state is checked and
 *        the histogram is updated.
 */
inline void StateTimerUnit::StateTimer::queryStateTimer_()
{
    // already queried in the same cycle, should not update the timer and histogram again
    if (last_query_time_ == clk_->currentCycle())
        return;
    std::shared_ptr<StateTimerUnit::StateTimer::StateSet> state_set;
    // get the container pointer
    StateTimerDataContainerPtr
            timer_info_container = state_timer_unit_ptr_->getStateTimerInfoContainer_();
    // put the address of state_set_delta_ vector of each state set in the container
    for (auto it=state_set_map_.begin(); it!=state_set_map_.end(); ++it)
    {
        // update the time delta if there is active state
        state_set = it->second;
        if (state_set->active_state_index_.isValid())
        {
            sparta_assert(clk_->currentCycle() >= state_set->active_state_starting_time_,
                    "Wrong timing: current cycle less than state start time");

            // do not reset active state since this is query,
            // state_set_delta_ will be reset after the the histograms are updated
            endTimerState_(state_set);
        }
        (*timer_info_container)[it->first] = & state_set->state_set_delta_;
    }
    // let StateTimerUnit know when the container is ready,
    // "false" in updateStateHistogram_() means do not release timer
    state_timer_unit_ptr_->updateStateHistogram_(timer_id_, false);
    last_query_time_ = clk_->currentCycle();
}

/**
 * \brief Release the timer to StateTimerPool, can not be called by user.
 *        Only called when the StateTimer::Handle is deleted.
 */
inline void StateTimerUnit::StateTimer::releaseStateTimer_()
{
    std::shared_ptr<StateTimerUnit::StateTimer::StateSet> state_set;
    // get the container pointer
    StateTimerDataContainerPtr
            timer_info_container = state_timer_unit_ptr_->getStateTimerInfoContainer_();
    // put the address of state_set_delta_ vector of each state set in the container
    for (auto it=state_set_map_.begin(); it!=state_set_map_.end(); ++it)
    {
        // update the time delta if there is active state
        state_set = it->second;
        if (state_set->active_state_index_.isValid())
        {
            sparta_assert(clk_->currentCycle() >= state_set->active_state_starting_time_,
                    "Wrong timing: current cycle less than state start time");

            // end the active state,
            // state_set_delta_ will be reset after the the histograms are updated
            endTimerState_(state_set);

            state_set->active_state_index_.clearValid();
            state_set->active_state_starting_time_ = 0;
        }
        (*timer_info_container)[it->first] = & state_set->state_set_delta_;
    }
    // let StateTimerUnit know when the container is ready,
    // "true" in updateStateHistogram_() means release timer
    state_timer_unit_ptr_->updateStateHistogram_(timer_id_, true);
}

/**
 * \brief Query all the active StateTimers in the pool
 */
inline void StateTimerUnit::StateTimerPool::queryAllActiveTimer()
{
    // query all the active timer in pool
    for (auto it=active_timer_map_ptr_->begin(); it!=active_timer_map_ptr_->end(); ++it)
    {
        it->second->queryStateTimer_();
    }
}

/**
 * \brief Release all the active StateTimers in the pool, used when destruct StateTimerUnit
 */
inline void StateTimerUnit::StateTimerPool::releaseAllActiveTimer()
{
    // release all the active timer in pool
    while (active_timer_map_ptr_->size()>0)
    {
        auto it= active_timer_map_ptr_->begin();
        it->second->releaseStateTimer_();
    }
}

/*!
 * \brief StateTimerPool constructor
 *
 * \param parent The parent of StateTimerUnit
 * \param state_set_info_map_ptr A map using the state set hashcode,
 *                               and contains the number of states in the set,
 *                               used to initialize the timers
 * \param state_timer_unit_ptr The pointer to StateTimerUnit, where the pool belongs to
 * \param num_state_timer_init Initial number of total StateTimers in pool,
 *                             used as incremental interval
 */
inline StateTimerUnit::StateTimerPool::StateTimerPool(TreeNode * parent,
        StateTimerUnit::StateTimer::StateSetInfo state_set_info_map_ptr,
        StateTimerUnit * state_timer_unit_ptr, uint32_t num_state_timer_init):
    timer_list_ptr_(new std::vector<StateTimerUnit::StateTimer::Handle>),
    active_timer_map_ptr_(new std::unordered_map<StateTimerUnit::StateTimer::TimerId,
                          StateTimerUnit::StateTimer::Handle>),
    available_timer_vec_ptr_(new std::vector<std::pair<StateTimerUnit::StateTimer::TimerId,
                             StateTimerUnit::StateTimer::Handle>>),
    my_life_(this, [](void*){}),
    num_state_timer_init_(num_state_timer_init),
    state_set_info_map_ptr_(state_set_info_map_ptr),
    clk_(parent->getClock()),
    state_timer_unit_ptr_(state_timer_unit_ptr)
{
    for (uint32_t i = 0; i < num_state_timer_init_; i++)
    {
        timer_list_ptr_->push_back(StateTimerUnit::StateTimer::Handle
                    (new StateTimerUnit::StateTimer(clk_, i, state_set_info_map_ptr_,
                                                    state_timer_unit_ptr_)));
        available_timer_vec_ptr_->push_back(std::make_pair(i, timer_list_ptr_->at(i)));
    }
}

/*!
 * \brief StateTimerUnit constructor
 *
 * \param parent The parent of StateTimerUnit
 * \param state_timer_unit_name The name string of the state timer unit
 * \param description The description string of the state timer unit
 * \param num_timer_init The initial number of StateTimers in pool,
 *                       used as incremental interval
 * \param lower Lower value of histogram
 * \param upper Upper value of histogram
 * \param bin_size Bin size of histogram
 * \param state_sets arbitrary number of state set as enum class
 */
template<class... ArgsT>
inline StateTimerUnit::StateTimerUnit(TreeNode * parent,
                   std::string state_timer_unit_name,
                   std::string description,
                   uint32_t num_timer_init,
                   uint32_t lower,      /*lower value of histogram*/
                   uint32_t upper,      /*upper value of histogram*/
                   uint32_t bin_size,   /*bin size of histogram*/
                   ArgsT...state_sets):
    TreeNode(state_timer_unit_name, description)
{
    setExpectedParent_(parent);
    if(parent != nullptr)
    {
        parent->addChild(this);
    }
    state_timer_data_container_ptr_ = StateTimerDataContainerPtr
            (new std::unordered_map<uint32_t, std::vector<sparta::Clock::Cycle> *>);
    state_set_info_map_ptr_ = StateTimerUnit::StateTimer::StateSetInfo
            (new std::unordered_map<uint32_t, uint32_t>);
    static_assert(sizeof...(state_sets)>0,
            "At least one state enum set need to be provided.");
    addStateSet_(state_sets...);
    state_timer_pool_ptr_ = std::unique_ptr<StateTimerPool>
            (new StateTimerPool(parent, state_set_info_map_ptr_, this, num_timer_init));
    state_timer_histogram_ptr_ = std::unique_ptr<StateTimerHistogram>
        (new StateTimerHistogram(parent->getChild(state_timer_unit_name),
                                 state_timer_unit_name, state_set_name_map_,
                                 state_timer_data_container_ptr_, state_set_info_map_ptr_,
                                 lower, upper, bin_size));
}

} // namespace sparta

