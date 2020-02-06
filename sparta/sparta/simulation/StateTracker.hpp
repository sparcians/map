// <StateTracker> -*- C++ -*-


#ifndef __STATE_TRACKER__H__
#define __STATE_TRACKER__H__

#include <inttypes.h>
#include <memory>
#include <vector>
#include <functional>
#include <deque>
#include <type_traits>
#include <algorithm>
#include <map>
#include <utility>

#include "sparta/utils/Enum.hpp"
#include "sparta/utils/ValidValue.hpp"

namespace sparta {

    namespace tracker {

        //! We need to have the capability to detect the presence of
        //  an overloaded operator for a type during compile time.
        //  This is needed because in order to annotate the Histograms,
        //  we need to label each enum constant with their string names.
        //  The process of converting enum constants into string is
        //  usually done by an overload global operator <<. But we cannot
        //  blindly assume that every enum class will have this overloaded.
        //  If we assume so, this will lead to compilation failure.
        //
        //  This can be detected using SFINAE.
        //  The idea of this technique is referred from here :
        //  en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Member_Detector/.
        namespace has_ostream_operator_impl {

            //!Typedef a char array of size one.
            typedef char no;

            //! Typedef a char array of size two.
            typedef char yes[2];

            //! A fallback struct which can create itself
            //  from any object of any type.
            struct fallback {
                template<typename T>
                fallback(const T &);
            };

            //! Declare a dummy << operator which operates on
            //  any fallback object.
            no operator << (std::ostream const &, fallback const &);

            //! If the class does have an << operator overloaded,
            //  then the return type of invoking it has to be a
            //  std::ostream &. In that case, this function will
            //  be invoked.
            yes & test(std::ostream &);

            //! If the class did not have an << operator overloaded
            //  to begin with, it will use the global << operator
            //  defined in this namespace. It will convert itself into
            //  fallback object and then invoke this function. In this
            //  case, the return type in no.
            no & test(no);

            template<typename T>
            struct has_ostream_operator {
                static std::ostream & s;
                static T const & t;
                static constexpr bool value {

                    //! Test to see what happens when we invoke
                    //  << operator on an object of type T. The return
                    //  value that we get will tell us if this class T
                    //  did have an << operator defined for it or not.
                    sizeof(test(s << t)) == sizeof(yes)};
            };
        }

        template<typename T>
        struct has_ostream_operator :
            has_ostream_operator_impl::has_ostream_operator<T> {};

        template<typename T> class StateTrackerUnit;
        template<typename T> class StatePool;
        template<typename T> struct StateSet;

        //! Custom Deleter of individual State Tracker Units.
        //  It contains a weak pointer pointing to the pool
        //  from where the state tracker unit was dispatched.
        //  If the weak pointer has not expired, the pool is still
        //  alive and the tracker unit gets recycled and enqueued
        //  back at the Dispatch Queue in the pool again. If the
        //  weak pointer is expired, then the tracker unit is freed.
        template<typename T>
        struct StateTrackerDeleter {
        public:

            //! Construct the internal weak_ptr from a shared_ptr
            //  reference.
            explicit StateTrackerDeleter<T>(
                const std::weak_ptr<StatePool<T>> & weak_pool) :
                weak_pool_ptr_(weak_pool) {}

            StateTrackerDeleter<T>() : valid_(false) {}

            inline void operator()(StateTrackerUnit<T> * ptr) const {
                if(!valid_ || !ptr) {
                    return;
                }
                if(!weak_pool_ptr_.expired()) {
                    weak_pool_ptr_.lock()->releaseToPool(ptr);
                }
                else {
                    delete ptr;
                    ptr = nullptr;
                }
            }

            std::weak_ptr<StatePool<T>> weak_pool_ptr_;
            bool valid_ {true};
        };

        //! Custom type for Unique Pointers to State Tracker Units.
        //  These unique pointers come with their own Deleters which
        //  is the StateTrackerDeleter Functor. Whenever a state tracker
        //  unit gets out of scope and is being destroyed, the Functor
        //  gets invoked. This is because, we need to know, whether we
        //  want to recycle the tracker unit or finally free it, if the
        //  simulation is over.
        template<typename T>
        using state_tracker_ptr =
            std::unique_ptr<StateTrackerUnit<T>, StateTrackerDeleter<T>>;

        //! Custom type of Unique Pointers to Queues which hold these
        //  State Tracker Units. StatePool is a class template, templatized
        //  on the enum class type which we are tracking. Each StatePool class
        //  template contains its dedicated Tracker Queue, templatized on the
        //  same enum type. This Tracker Queue holds all the available,
        //  ready-to-go tracker units, ready for dispatching when the call comes.
        //  The State Tracker Units, upon deletion, come back to this queue during
        //  simulation and make themselves available again for tracking. Only when
        //  the Simulation class is torn down, this Tracker Queue gets destroyed and
        //  finally its custom deleter, which is TransferQueueData(), is called.
        template<typename T>
        using tracker_queue_ptr =
            std::unique_ptr<std::deque<state_tracker_ptr<T>>,
                std::function<void(std::deque<state_tracker_ptr<T>> *)>>;

        //! This is the polymorphic base class for the StatePool template class.
        //  This class has been designed to solve the problem of storing StatePool
        //  instances in a single homogenous container. This is difficult because
        //  StatePool is a template class and cannot be grouped together in a single
        //  container. Hence, the StatePoolBase class is designed. Instead of storing
        //  the actual StatePool instances, we store base class pointers to those
        //  instances in a map, with each different template type having a unique ID.
        class StatePoolBase {
        public:
            virtual ~StatePoolBase() {}

        protected:
            static size_t next() noexcept {
                static size_t counter {0};
                return counter++;
            }
        };

        //! StatePool class template is templatized on the enum type we are tracking.
        //  The basic functionality of this class is to maintain a Ready-To-Go Queue of
        //  State Tracker Units of the same enum type. It also contains the running
        //  number of all the State Tracker Units it has dispatched and a shared_ptr to
        //  itself. The Pool class also contains the state tracking filename, passed
        //  on from the StatePoolManager.
        template<typename T>
        class StatePool : public StatePoolBase {
        public:

            //! The Default Ctor is deleted because StatePool cannot be created
            //  without a valid state tracking filename.
            StatePool<T>() = delete;

            //! A file name is a must when constructing StatePool.
            explicit StatePool<T>(const std::string & tracking_filename) :

                //! We initialize instance count to 0 during construction.
                instance_count_(0),

                //! We set the pointer to refer to itself.
                //  This is used later on when deleting Tracker Units.
                pool_existence_reference_(this, [](void *){}),

                //! Store the output filename.
                tracking_filename_(tracking_filename),

                //! We create a new queue and pass in the custom deleter for this queue
                //  which is transferQueueData() API.
                available_tracker_queue_(new std::deque<state_tracker_ptr<T>>(),
                    [this](std::deque<state_tracker_ptr<T>> * raw_queue){
                        transferQueueData(raw_queue);}) {}

            //! Method which associates this StatePool template instantiation
            //  with a Unique ID. This is not a Thread Safe method.
            static size_t id() noexcept {
                static const size_t identifier {StatePoolBase::next()};
                return identifier;
            }

            //! Method which gets invoked whenever a demand for a new
            //  State Tracker Unit is raised.
            state_tracker_ptr<T> getNewStateTrackerUnit(Scheduler * scheduler) noexcept {

                //! Increment the instance count as number of tracker units
                //  increases by 1.
                ++instance_count_;

                //! If the Queue of tracker units is empty, we create a new
                //  Tracker Unit on the fly and dispatch it.
                if(available_tracker_queue_->empty()) {
                    state_tracker_ptr<T> state_tracker_unit(
                        new StateTrackerUnit<T>(scheduler),
                        StateTrackerDeleter<T>(pool_existence_reference_));
                    return state_tracker_unit;
                }

                //! If the Queue has available Tracker Units to go, we just pop
                //  one from the front and dispatch it. This is where the recycling
                //  state tracker units happen.
                state_tracker_ptr<T> state_tracker_unit {
                    std::move(available_tracker_queue_->front())};
                available_tracker_queue_->pop_front();
                return state_tracker_unit;
            }

            //! Method which is invoked when individual State Tracker Units
            //  need to be returned back to the pool during recycling.
            void releaseToPool(StateTrackerUnit<T> *& raw_ptr){

                //! Wrap this raw pointer into a unique pointer.
                //  This makes the code safe from unexpected memory leaks.
                state_tracker_ptr<T> tracker_unique_ptr(raw_ptr,
                    StateTrackerDeleter<T>(pool_existence_reference_));

                //! This tracker unit cannot be a nullptr.
                sparta_assert(tracker_unique_ptr);

                //! If this tracker unit has some un-processed data in it
                //  from its last tracking run, we collect and process it
                //  before releasing it back in the pool.
                tracker_unique_ptr->updateLastDeltas();

                //! Finally, push the Tracker Unit at the back of the queue.
                available_tracker_queue_->push_back(std::move(tracker_unique_ptr));
            }

            //! This is the main method, the custom deleter of the Tracker Queues.
            //  This method gets called once and only once for each tracked enum type,
            //  when the StatePool instantiation is getting destroyed.
            void transferQueueData(std::deque<state_tracker_ptr<T>> * raw_queue) {

                //! Wrap the raw pointer into a unique_ptr, a good practise.
                std::unique_ptr<std::deque<state_tracker_ptr<T>>> queue_ptr(raw_queue);

                //! If we have reached this point, instance count cannot be 0.
                sparta_assert(instance_count_);

                //! Open the state tracking file.
                std::fstream data_file(tracking_filename_, std::ios_base::app);

                //! We need to reset this self-reference pointer so that the
                //  weak_pointer in StateTrackerDeleter pointing here, expires,
                //  and stops tracker units to come back to the queue.
                pool_existence_reference_.reset();

                //! Create a vector to hold the results.
                std::vector<sparta::Scheduler::Tick> stats_vector(
                    static_cast<uint64_t>(T::__LAST) + 1, 0);

                //! We process each and every State Tracker Unit in the queue.
                std::for_each(queue_ptr->begin(), queue_ptr->end(),
                    [&stats_vector](const state_tracker_ptr<T> & item) {

                    //! Grab the Calculation Engine from the tracker unit being processed.
                    const StateSet<T> & state_set {item->getStateSet()};
                    sparta_assert(state_set.state_delta_set.size() == stats_vector.size());

                    // Accumulate the data from this tracker unit into the result vector.
                    std::transform(stats_vector.begin(), stats_vector.end(),
                                   state_set.state_delta_set.begin(), stats_vector.begin(),
                                   std::plus<sparta::Scheduler::Tick>()); });

                queue_ptr->clear();

                //! Calculate average stats from the aggregate stats by using
                //  total instance count.
                std::vector<double> avg_stats_vector(
                    static_cast<uint64_t>(T::__LAST) + 1, 0);

                std::transform(stats_vector.begin(), stats_vector.end(),
                    avg_stats_vector.begin(),
                    [this](sparta::Scheduler::Tick & item) -> double {
                    return static_cast<double>(item) /
                           static_cast<double>(instance_count_); });


                std::string name_string;

                //! Store the enum class names by using gcc __PRETTY_FUNCTION__.
                extractEnumTypeAsString_(__PRETTY_FUNCTION__, "[", "]", name_string);

                //! Finally, we label the Histogram and write this data into file.
                data_file << "Enum Class Name : " << name_string << "\n"
                          << "Total State Tracker Units used : " << instance_count_ << "\n"
                          << "Aggregate Residency Stats: \n";

                std::vector<std::string> enum_name_strings;

                //! Store the individual enum constant names.
                fillHistogramLabels_<T>(enum_name_strings);

                //! This loop iterates one less than the size of this vector times.
                //  This is because the last enum state in every enum class is __LAST.
                //  This is not a real enum state but merely a placeholder or dummy state
                //  to calculate quickly the number of states in that particular state.
                for(size_t i = 0; i < stats_vector.size() - 1; ++i) {
                    data_file << enum_name_strings[i] << " : "  << stats_vector[i] << "\n";
                }

                data_file << "\n\nAverage Residency Stats: \n";

                //! This loop iterates one less than the size of this vector times.
                //  This is because the last enum state in every enum class is __LAST.
                //  This is not a real enum state but merely a placeholder or dummy state
                //  to calculate quickly the number of states in that particular state.
                for(size_t i = 0; i < avg_stats_vector.size() - 1; ++i) {
                    data_file << enum_name_strings[i] << " : " << avg_stats_vector[i] << "\n";
                }

                data_file << "\n\n";
            }

            private:

                //! The total number of State Tracker Units dispatched.
                uint64_t instance_count_;

                //! Shared Pointer to itself which gets passed into StateTrackerDeleter.
                //  If this self-reference is reset, the StateTrackerDeleter knows the
                //  pool is going through destruction and that State Tracker Units
                //  should not be recycled anymore. If the reference is still valid,
                //  the State Tracker Units need to com back to the queue, recycled.
                std::shared_ptr<StatePool<T>> pool_existence_reference_;

                //! This is the State Tracking filename where all the histogram
                //  data will be written to.
                std::string tracking_filename_;

                //! This is the Queue which holds all the available State Tracker
                //  Units. It has a custom type with its own custom deleter.
                tracker_queue_ptr<T> available_tracker_queue_;

                //! This method extracts the name of the enum class type as a string.
                //  We put this string in our output text file to label the various
                //  histograms.
                bool extractEnumTypeAsString_(const std::string & source,
                    const std::string & start,
                    const std::string & end,
                    std::string & result) {

                    //! __PRETTY_FUNCTION__ generates a compile time constant string with
                    //  the enum class type that needs to be extracted as a std::string.
                    //  The first 9 chars of this string is "with T = " which needs to be
                    //  erased away before returning the rest of the string.
                    constexpr uint16_t redundant_char_length {9};

                    std::size_t start_index = source.find(start);
                    if(start_index == std::string::npos) {
                        return false;
                    }

                    start_index += start.length();

                    const std::string::size_type end_index =
                        source.find(end, start_index);

                    if(end_index == std::string::npos) {
                        return false;
                    }

                    result = source.substr(start_index, end_index - start_index);
                    result.erase(0, redundant_char_length);
                    return true;
                }

                //! Given an enum class type, this method figures out the string
                //  name equivalents of the different enum constants. For this,
                //  we need to invoke the << operator on individual enum constants.
                //  This template overload is SFINAEd out to enable if this enum
                //  type U has a << operator overloaded for it.
                template<typename U>
                typename std::enable_if<has_ostream_operator<U>::value, void>::type
                fillHistogramLabels_(std::vector<std::string> & enum_name_strings) {
                    typedef typename std::underlying_type<U>::type enumType;
                    constexpr enumType last_index = static_cast<enumType>(U::__LAST);
                    enum_name_strings.reserve(last_index);
                    std::stringstream ss;
                    for(enumType e = 0; e < last_index; ++e) {
                        auto val = static_cast<U>(e);
                        ss << val;
                        enum_name_strings.emplace_back(ss.str());
                        ss.str("");
                        ss.clear();
                    }
                }

                //! Given an enum class type, this method figures out the string
                //  name equivalents of the different enum constants. For this,
                //  we need to invoke the << operator on individual enum constants.
                //  This template overload is SFINAEd out to disable if this enum
                //  type U does not have a << operator overloaded for it. We fill
                //  the resulting vector with empty strings.
                template<typename U>
                typename std::enable_if<!has_ostream_operator<U>::value, void>::type
                fillHistogramLabels_(std::vector<std::string> & enum_name_strings) {
                    typedef typename std::underlying_type<U>::type enumType;
                    constexpr enumType last_index = static_cast<enumType>(U::__LAST);
                    enum_name_strings.resize(last_index);
                }
        };

        //! This is a Singleton class which manages all the different StatePool
        //  template instantiations. This class contains a map of unique integers
        //  mapped to StatePool template instantiations. Whenever a new State
        //  Tracker Unit is demanded, we first get a handle of the appropriate
        //  StatePool by using the Template type. Once we have the Pool, we query
        //  the internal Tracker Queue for an available Tracker Unit.
        class StatePoolManager
        {
        public:
            StatePoolManager(const StatePoolManager &) = delete;
            StatePoolManager & operator = (const StatePoolManager &) = delete;
            StatePoolManager(StatePoolManager &&) = delete;
            StatePoolManager & operator = (StatePoolManager &&) = delete;

            //! This is not a Thread Safe method.
            static StatePoolManager & getInstance() {
                static StatePoolManager state_pool_manager;
                return state_pool_manager;
            }

            //! This method needs to be public to
            //  make tracking enabled in stand-alone
            //  tester.
            inline void enableTracking() {
                is_tracking_enabled_ = true;
            }

            //! This method starts the teardown of all the different
            //  state pool template instantiations. This in turn,
            //  destroys all the individual state tracker units after
            //  processing them. This method must be called before the
            //  Simulation is done. This method is called from the
            //  destructor of the Special sparta::State.
            void flushPool() {
                unique_pool_type_map_.clear();
            }

            //! This is the public API to get new tracker units.
            //  This method is called during construction of individual
            //  State objects. This method encapsulates all the technicalities
            //  of State Pools, Tracker Queues and Deleter Functors.
            template<typename T>
            state_tracker_ptr<T> dispatchNewTracker() {

                //! We go to the process of issuing tracker units
                //  only if user have turned on state tracking.
                if(__builtin_expect(is_tracking_enabled_, 0)) {
                    return getStatePool_<T>()->getNewStateTrackerUnit(scheduler_);
                }
                return nullptr;
            }

            //! This filename is the name of the State Tracking File as
            //  provided by the user. This is passed down from the configure
            //  method of the Special sparta::State which is constructed during
            //  the Simulation construction phase. Pool Manager stores the
            //  filename and passes this name when constructing State Pools.
            //  State Pools use this filename to open file, write histogram
            //  data during the destruction of itself.
            void setTrackingFilename(const std::string & filename) {

                //! Clear all the data from previous Simulation runs.
                //  This is important if the modeler uses the same filename
                //  for consecutive simulation runs. Else, we would be getting
                //  data from multiple simulation runs, all clobbered together.
                is_tracking_enabled_ = true;
                std::fstream data_file(filename, std::ios::out | std::ios_base::trunc);
                data_file.close();
                tracking_filename_ = filename;
            }

            //! Set the scheduler used by the Simulation class
            void setScheduler(Scheduler * scheduler) {
                scheduler_ = scheduler;
            }

        private:
            StatePoolManager() = default;
            bool is_tracking_enabled_ {false};
            Scheduler * scheduler_ = nullptr;
            using CachedBase = std::map<size_t, std::unique_ptr<StatePoolBase>>;
            CachedBase unique_pool_type_map_;
            std::string tracking_filename_;

            //! Method which returns a State Pool Handle.
            //  The process of dispatching a State Tracker Unit is
            //  two-stepped. The first step is to get the correct
            //  State Pool template instatiation. This method does the
            //  first step. Once we have a valid pool handle, we query
            //  internal Tracker queues of that pool and start issuing
            //  new or recycled State Tracker Units.
            template<typename T>
            inline StatePool<T> * getStatePool_() {
                static StatePool<T> * state_pool {nullptr};

                //! If state_pool is not null, that means we
                //  have seen this enum type T before and hence,
                //  we do not need to create a new state pool to
                //  handle this enum type. We just return the
                //  state_pool pointer which is already pointing
                //  to the valid state pool for this enum type T.
                if(__builtin_expect(state_pool != nullptr, 1)) {
                    return state_pool;
                }

                //! If state_pool is null, that means we are seeing
                //  this enum type T for the first time. So, we must
                //  instantiate a new, valid State Pool to handle
                //  enums of this type. We then make state_pool
                //  point to this pool and return it.
                const size_t identifier = StatePool<T>::id();

                unique_pool_type_map_[identifier].reset(
                    new StatePool<T>(tracking_filename_));

                //! Note that static_cast has been used instead of
                //  dynamic_cast. This is done because we already know
                //  what the exact derived type of the resource being
                //  pointed by the base pointer is. So, it is much more
                //  efficient to not use the C++ runtime to perform the
                //  cast and let the compiler handle this.
                state_pool = static_cast<StatePool<T> *>(
                    unique_pool_type_map_[identifier].get());
                return state_pool;
            }
        };

        //! This is the Calculation Engine unit inside each State Tracker Unit.
        //  This struct tells us if there is a valid state which is being tracked
        //  right now or not, what is the timestamp when this state got active and
        //  a vector of accumulated ticks for all the different states of this
        //  particular enum type.
        template<typename EnumT>
        struct StateSet {
            sparta::utils::ValidValue<
                typename std::underlying_type<EnumT>::type> active_state_index;
            sparta::Scheduler::Tick active_state_starting_time;
            std::vector<sparta::Scheduler::Tick> state_delta_set;

            explicit StateSet(const uint64_t num_states) :
                active_state_starting_time(0),
                state_delta_set(std::vector<sparta::Scheduler::Tick>(num_states, 0)) {}

            StateSet(const StateSet & lval) = default;

            StateSet(StateSet && rval) :
                active_state_index(rval.active_state_index),
                active_state_starting_time(rval.active_state_starting_time),
                state_delta_set(std::move(rval.state_delta_set)) {}
        };

        //! This is the actual Lightweight Tracker class which does the
        //  starting and stopping of tracking states. Instances of this class
        //  are contained inside sparta::State class which uses these tracker units
        //  to perform arithmetical calculations.
        template<typename EnumT>
        class StateTrackerUnit {
        public:
            StateTrackerUnit(Scheduler * scheduler) :
                scheduler_instance_(scheduler),
                time_assigned_(scheduler_instance_->getCurrentTick()),
                    state_set_(StateSet<EnumT>(
                        static_cast<uint64_t>(EnumT::__LAST) + 1)) {}

            StateTrackerUnit(const StateTrackerUnit &) = default;

            StateTrackerUnit(StateTrackerUnit && rval) :
                scheduler_instance_(rval.scheduler_instance_),
                time_assigned_(rval.time_assigned_),
                state_set_(rval.state_set_) {
                rval.scheduler_instance_ = nullptr;
            }

            //! This method starts the timer on this particular value of
            //  the enum class type.
            template<typename EnumType>
            void startState(const EnumType& state_enum) {
                sparta::Scheduler::Tick current_time =
                    scheduler_instance_->getCurrentTick();
                typename std::underlying_type<EnumT>::type state_index =
                    static_cast<typename std::underlying_type<EnumT>::type>(state_enum);
                if(__builtin_expect(state_set_.active_state_index.isValid(), 1)){
                    typename std::underlying_type<EnumT>::type active_state_in_set =
                        state_set_.active_state_index.getValue();
                    if(active_state_in_set == state_index){
                        return;
                    }
                    endTimerState_();
                }
                sparta_assert(static_cast<size_t>(state_index) <
                    state_set_.state_delta_set.size());
                state_set_.active_state_index = state_index;
                state_set_.active_state_starting_time = current_time;
            }

            //! This method stops the timer on this particular value of
            //  the enum class type.
            template<typename EnumType>
            void endState(const EnumType& state_enum) {
                typename std::underlying_type<EnumT>::type state_index =
                    static_cast<typename std::underlying_type<EnumT>::type>(state_enum);
                sparta_assert(state_set_.active_state_index.isValid());
                typename std::underlying_type<EnumT>::type active_state_in_set =
                    state_set_.active_state_index.getValue();
                sparta_assert(active_state_in_set == state_index);
                endTimerState_();
                state_set_.active_state_index.clearValid();
                state_set_.active_state_starting_time = 0;
            }

            //! This method is called right before we send away a
            //  tracker unit for recycling. This method basically does
            //  one last calculation and processes stats before
            //  sending the tracker unit back to the queue.
            inline void updateLastDeltas() noexcept {
                if(__builtin_expect(state_set_.active_state_index.isValid(), 1)) {
                    sparta::Scheduler::Tick current_time =
                        scheduler_instance_->getCurrentTick();
                    typename std::underlying_type<EnumT>::type active_state_in_set =
                        state_set_.active_state_index.getValue();
                    state_set_.state_delta_set[active_state_in_set] +=
                        current_time - state_set_.active_state_starting_time;
                    state_set_.active_state_index.clearValid();
                    state_set_.active_state_starting_time = 0;
                }
            }

            //! Return the amount of time this state has been active
            //! in the current state
            sparta::Scheduler::Tick getActiveTime() const {
                return scheduler_instance_->getCurrentTick() -
                    state_set_.active_state_starting_time;
            }

            const StateSet<EnumT> & getStateSet() const {
                return state_set_;
            }

        private:
            inline void endTimerState_() noexcept {
                sparta::Scheduler::Tick current_time =
                    scheduler_instance_->getCurrentTick();
                typename std::underlying_type<EnumT>::type active_state_in_set =
                    state_set_.active_state_index.getValue();
                state_set_.state_delta_set[active_state_in_set] +=
                    current_time - state_set_.active_state_starting_time;
            }

            const sparta::Scheduler * scheduler_instance_ {nullptr};
            sparta::Scheduler::Tick time_assigned_ {0};
            StateSet<EnumT> state_set_ {};
        };
    }
}// __STATE_TRACKER__H__

#endif
