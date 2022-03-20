// <Pipeline> -*- C++ -*-


/**
 * \file   Pipeline.hpp
 * \brief  Defines the Pipeline class
 */

#pragma once

#include <cinttypes>
#include <memory>
#include <array>
#include <list>
#include <type_traits>
#include <vector>

#include "sparta/simulation/Clock.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/MathUtils.hpp"
#include "sparta/collection/IterableCollector.hpp"

#include "sparta/resources/Pipe.hpp"
#include "sparta/events/Scheduleable.hpp"
#include "sparta/events/PhasedUniqueEvent.hpp"
#include "sparta/events/PhasedPayloadEvent.hpp"
#include "sparta/kernel/SpartaHandler.hpp"
#include "sparta/utils/IteratorTraits.hpp"

namespace sparta
{

    /*!
     * \class Pipeline
     * \brief A simple pipeline
     *
     * The sparta::Pipeline class is intended to provide an efficient and flexible event scheduling
     * framework for modeling generic pipeline concept.
     *
     * It contains a sparta::Pipe, and couples event-scheduling (i.e. control-flow path), with the
     * data-movement (i.e. data-flow path) provided by sparta::Pipe.
     *
     * Template parameter DataT specifies the type of data flowing through pipeline stages, and
     * EventT can be specified by 2 Event types: PhasedUniqueEvent (default) and PhasedPayloadEvent<DataT>.
     * The difference between both types is related to what kind of SpartaHandler modelers are going
     * to register at stages. With default PhasedUniqueEvent, you can register a SpartaHandler
     * with no data; or, with PhasedPayloadEvent<DataT>, you can register a SpartaHandler with
     * data of type DataT. Pipeline will prepare payload and pass data of the stage to every handler.
     *
     * The sparta::Pipeline is able to provide modelers with the following design capability:
     * -# Register event handler(s) for designated pipeline stage(s), sparse stage handler
     *    registration is supported.
     * -# Specify precedence between two pipeline stage(s).
     * -# Specify producer-consumer relationship between pipeline stage handling event(s) and
     *    other user-defined event(s).
     * -# Read and check validity of pipeline data at designated pipeline stage(s).
     * -# Append, write/modify, invalidate, or flush data at designated pipeline stage(s).
     * -# Perform manual or automatic pipeline update (i.e. forward progression). The registered
     *    pipeline stage handlers are called under-the-hood whenever valid pipeline data arrives.
     */
    template <typename DataT, typename EventT=PhasedUniqueEvent>
    class Pipeline
    {
    public:
        using size_type = uint32_t;
        using value_type = DataT;
        using EventHandle = std::unique_ptr<EventT>;
        using EventHandleList = std::list<EventHandle>;
        using EventList = std::list<EventT*>;
        using EventMatrix = std::array<EventList, NUM_SCHEDULING_PHASES>;

        /*!
         * \class Precedence
         * \brief Specify the default event scheduling precedence between pipeline stages
         */
        enum class Precedence {
            NONE,
            FORWARD,
            BACKWARD,
            NUM_OF_PRECEDENCE
        };

        template<typename DataT2, typename EventT2>
        friend class Pipeline;

        /*!
         * \class PipelineIterator
         *
         * \brief An iterator class for Pipeline class
         *
         * The PipelineIterator is a forward iterator. It could be instantiated as either a
         * const iterator or a non-const iterator.
         *
         * If the iterator is dereferenced with operator*(), a data-reference type is returned.
         * If the iterator is dereferenced with operator->(), a data-pointer type is returned.
         *
         * An assertion error will fire if the itertaor is dereferenced, but the pipeline stage
         * referred to by the iterator is not valid.
         */
        template<bool is_const_iterator = true>
        class PipelineIterator : public utils::IteratorTraits<std::forward_iterator_tag, DataT>
        {
            using DataReferenceType =
                typename std::conditional<is_const_iterator, const DataT &, DataT &>::type;

            using DataPointerType =
                typename std::conditional<is_const_iterator, const DataT *, DataT *>::type;

            using PipelinePointerType =
                typename std::conditional<is_const_iterator,
                                          const Pipeline<DataT, EventT> *, Pipeline<DataT, EventT> *>::type;

        public:
            friend class PipelineIterator<true>;
            friend class Pipeline;

            /*!
             * \brief Constructor
             *
             * \param ptr Pipeline pointer
             */
            PipelineIterator(PipelinePointerType ptr) : pipelinePtr_(ptr) {}

            /*!
             * \brief Constructor
             *
             * \param ptr Pipeline pointer
             * \param i Pipeline stage number
             */
            PipelineIterator(PipelinePointerType ptr, uint32_t i) : pipelinePtr_(ptr), index_(i) {}

            /*!
             * \brief Copy constructor
             *
             * \param rhs PipelineIterator(non-const)
             *
             * \note Allow for implicit conversion from non-const to const iterator
             */
            PipelineIterator(const PipelineIterator<false> & rhs) :
                pipelinePtr_(rhs.pipelinePtr_),
                index_(rhs.index_)
            {
            }

            /*!
             * \brief Copy constructor
             *
             * \param rhs PipelineIterator(const)
             *
             */
            PipelineIterator(const PipelineIterator<true> & rhs) :
                pipelinePtr_(rhs.pipelinePtr_),
                index_(rhs.index_)
            {
            }


            /*!
             * \brief Copy assignment operator
             *
             * \param rhs PipelineIterator(const or non-const)
             */
            PipelineIterator& operator=(const PipelineIterator & rhs) = default;

            //! Override the dereferencing operator*
            DataReferenceType operator*()
            {
                sparta_assert(isValid(), "Iterator is not valid for dereferencing!");
                return pipelinePtr_->operator[](index_);
            }

            //! Override the dereferencing operator->
            DataPointerType operator->()
            {
                sparta_assert(isValid(), "Iterator is not valid for dereferencing!");
                return &(this->operator*());
            }

            //! Override the pre-increment operator
            PipelineIterator operator++()
            {
                index_++;
                if (index_ > pipelinePtr_->capacity()) {
                    index_ = pipelinePtr_->capacity();
                }
                return *this;
            }

            //! Override the pre-increment operator
            PipelineIterator operator++(int)
            {
                PipelineIterator temp(*this);
                this->operator++();
                return temp;
            }

            //! Override the comparison(equal) operator
            bool operator==(const PipelineIterator & rhs) const
            {
                return ((pipelinePtr_ == rhs.pipelinePtr_) && (index_ == rhs.index_));
            }

            //! Override the comparison(not equal) operator
            bool operator!=(const PipelineIterator & rhs) const { return !this->operator==(rhs); }

            /*!
             * \brief Check the validity of the iterator
             *
             * \return Returns false if the iterator is not valid
             */
            bool isValid() const { return pipelinePtr_->isValid(index_); }

        private:
            PipelinePointerType pipelinePtr_;
            uint32_t index_ = 0;
        };

        using iterator = PipelineIterator<false>;
        using const_iterator = PipelineIterator<true>;

        iterator begin() { return iterator(this); }
        const_iterator begin() const { return const_iterator(this); }
        const_iterator cbegin() const { return const_iterator(this); }

        iterator end() { return iterator(this, capacity()); }
        const_iterator end() const { return const_iterator(this, capacity()); }
        const_iterator cend() const { return const_iterator(this, capacity()); }

        /*!
         * \brief Construct a Pipeline object by existed event set
         *
         * \param es The pointer to existed event set
         * \param name The name of Pipeline object
         * \param num_stages The number of pipeline stages
         * \param clk The clock this pipeline synchronized to
         */
        Pipeline(EventSet * es,
                 const std::string & name,
                 const uint32_t num_stages,
                 const Clock * clk) :
            name_(name),
            clock_(clk),
            pipe_(name, num_stages, clk),
            event_list_at_stage_(num_stages),
            event_matrix_at_stage_(num_stages),
            es_((es == nullptr) ? &dummy_es_ : es),
            ev_pipeline_update_
                (es_, name + "_update_event", CREATE_SPARTA_HANDLER(Pipeline, internalUpdate_), 1),
            num_stages_(num_stages)
        {
            sparta_assert(clk != nullptr, "Pipeline requires a clock");
            dummy_es_.setClock(clk);

            // Only support the following Event types:
            // 1. PhasedUniqueEvent
            // 2. PhasedPayloadEvent<DataT>
            static_assert(std::is_same_v<EventT, PhasedUniqueEvent> || std::is_same_v<EventT, PhasedPayloadEvent<DataT>>,
                          "Error: Pipeline is templated on a unsupported Event type. Supported Event types: "
                          "UniqueEvent, PayloaodEvent (where DataT == DataT of the Pipeline).");
            events_valid_at_stage_.resize(num_stages, false);
            advance_into_stage_.resize(num_stages, true);

            ev_pipeline_update_.setScheduleableClock(clk);
            ev_pipeline_update_.setScheduler(clk->getScheduler());
            ev_pipeline_update_.setContinuing(false);
        }

        /*!
         * \brief Construct a Pipeline object
         *
         * \param name The name of Pipeline object
         * \param num_stages The number of pipeline stages
         * \param clk The clock this pipeline synchronized to
         */
        Pipeline(const std::string & name,
                 const uint32_t num_stages,
                 const Clock * clk) :
            Pipeline(nullptr, name, num_stages, clk)
        {}

        /*!
         * \brief Register event handler for a designated pipeline stage
         *
         * \param id The stage number for registration
         * \param handler The SpartaHandler to be registered for the designated pipeline stage
         *
         * \note If user registers a pipeline stage handler whose scheduling phase is
         *       less than or equal to Flush (i.e. Update, PortUpdate, or Flush),
         *       the user should be aware that when flush event occurs at Flush phase,
         *       this handling event is already scheduled, and WILL NOT be cancelled.
         */
        template<SchedulingPhase sched_phase = SchedulingPhase::Tick>
        void registerHandlerAtStage(const uint32_t & id, const SpartaHandler & handler)
        {
            sparta_assert(static_cast<uint32_t>(default_precedence_) == static_cast<uint32_t>(Precedence::NONE),
                          "You have specified a default precedence (" << static_cast<uint32_t>(default_precedence_)
                          << ") between stages. No new handlers can be registered any more!");
            sparta_assert(id < event_list_at_stage_.size(),
                          "Attempt to register handler for invalid pipeline stage[" << id << "]!");

            // Create a new stage event handler, and add it to its event list
            auto & event_list = event_list_at_stage_[id];
            if constexpr (std::is_same_v<EventT, PhasedPayloadEvent<DataT>>) {
                sparta_assert(handler.argCount() == 1, "Expecting Sparta Handler with 1 data parameter!");
                event_list.emplace_back(new EventT(es_,
                                            "pev_" + name_ + "_stage_" + std::to_string(id) + "_" + std::to_string(event_list_at_stage_[id].size()),
                                            sched_phase,
                                            handler));
            } else {
                sparta_assert(handler.argCount() == 0, "Expecting Sparta Handler with no data parameter!");
                event_list.emplace_back(new EventT(es_,
                                            "uev_" + name_ + "_stage_" + std::to_string(id) + "_" + std::to_string(event_list_at_stage_[id].size()),
                                            sched_phase,
                                            handler));
            }
            auto new_event = event_list.back().get();

            // Set clock for this new event handler
            if constexpr (std::is_same_v<EventT, PhasedPayloadEvent<DataT>>) {
                new_event->getScheduleable().setScheduleableClock(clock_);
                new_event->getScheduleable().setScheduler(clock_->getScheduler());
            } else {
                new_event->setScheduleableClock(clock_);
                new_event->setScheduler(clock_->getScheduler());
            }

            // Update event matrix
            auto & event_list_per_phase = event_matrix_at_stage_[id][static_cast<uint32_t>(sched_phase)];
            if (event_list_per_phase.size() > 0) {
                auto & producer_event = event_list_per_phase.back();

                // Set the latest registered event (in this 'sched_phase') to precedes this new event
                if constexpr (std::is_same_v<EventT, PhasedPayloadEvent<DataT>>) {
                    producer_event->getScheduleable().precedes(new_event->getScheduleable());
                } else {
                    producer_event->precedes(*new_event);
                }
            } else {
                events_valid_at_stage_[id] = true;
            }

            // Add the raw pointer of newly added event to the stage event list on 'sched_phase'
            event_list_per_phase.push_back( new_event );
        }

        /*!
         * \brief Specify precedence between two different stages within the same pipeline instance
         *
         * \param pid The stage number of producer stage-handling event
         * \param cid The stage number of consumer stage-handling event
         *
         * \note This function can only be called when 'default_precedence_' is not specified
         */
        void setPrecedenceBetweenStage(const uint32_t & pid, const uint32_t & cid)
        {
            sparta_assert(static_cast<uint32_t>(default_precedence_) == static_cast<uint32_t>(Precedence::NONE),
                          "You have specified a default precedence (" << static_cast<uint32_t>(default_precedence_)
                          << "). No more precedence between stages can be set!");
            sparta_assert(pid != cid, "Cannot specify precedence with yourself!");
            sparta_assert((pid < event_list_at_stage_.size()) && (event_list_at_stage_[pid].size() > 0),
                          "Precedence setup fails: No handler for pipeline stage[" << pid << "]!");
            sparta_assert((cid < event_list_at_stage_.size()) && (event_list_at_stage_[cid].size() > 0),
                          "Precedence setup fails: No handler for pipeline stage[" << cid << "]!");

            for (uint32_t phase_id = 0; phase_id < NUM_SCHEDULING_PHASES; phase_id++) {
                auto & pstage_event_list = event_matrix_at_stage_[pid][phase_id];
                auto & cstage_event_list = event_matrix_at_stage_[cid][phase_id];

                if (pstage_event_list.empty() || cstage_event_list.empty()) {
                    continue;
                }

                // The last event in producer pipeline stage(#pid) is
                // scheduled earlier than the first event in consumer
                // pipeline stage(#cid)
                if constexpr (std::is_same_v<EventT, PhasedPayloadEvent<DataT>>) {
                    (pstage_event_list.back())->getScheduleable().precedes((cstage_event_list.front())->getScheduleable());
                } else {
                    (pstage_event_list.back())->precedes(*(cstage_event_list.front()));
                }
            }
        }

        /*!
         * \brief Specify precedence between two stages from different pipeline instances
         *
         * \param pid The stage number of producer stage-handling event
         * \param c_pipeline The consumer pipeline
         * \param cid The stage number of the consumer pipeline stage-handling event
         */
        template<class DataT2, class EventT2>
        void setPrecedenceBetweenPipeline(const uint32_t & pid, Pipeline<DataT2, EventT2> & c_pipeline, const uint32_t & cid)
        {
            sparta_assert(static_cast<void*>(&c_pipeline) != static_cast<void*>(this),
                          "Cannot use this function to set precedence between stages within the same pipeline instance!");
            sparta_assert((pid < event_list_at_stage_.size()) && (event_list_at_stage_[pid].size() > 0),
                          "Precedence setup fails: No handler for pipeline stage[" << pid << "]!");
            sparta_assert((cid < c_pipeline.event_list_at_stage_.size()) && (c_pipeline.event_list_at_stage_[cid].size() > 0),
                          "Precedence setup fails: No handler for pipeline stage[" << cid << "]!");

            for (uint32_t phase_id = 0; phase_id < NUM_SCHEDULING_PHASES; phase_id++) {
                auto & pstage_event_list = event_matrix_at_stage_[pid][phase_id];
                auto & cstage_event_list = c_pipeline.event_matrix_at_stage_[cid][phase_id];

                if (pstage_event_list.empty() || cstage_event_list.empty()) {
                    continue;
                }

                // The last event in producer pipeline stage(#pid) is
                // scheduled earlier than the first event in consumer
                // pipeline stage(#cid)
                if constexpr (std::is_same_v<EventT, PhasedPayloadEvent<DataT>>) {
                    (pstage_event_list.back())->getScheduleable().precedes((cstage_event_list.front())->getScheduleable());
                } else {
                    (pstage_event_list.back())->precedes(*(cstage_event_list.front()));
                }
            }
        }

        /*!
         * \brief Specify precedence of pipeline stage-handling events as forward/backward stage order
         * \param default_precedence default precedence (can be either forward or backward) between pipeline stages
         * \note This only set the precedence for stages that already have handler registered
         */
        void setDefaultStagePrecedence(const Precedence & default_precedence)
        {
            sparta_assert(static_cast<uint32_t>(default_precedence) < static_cast<uint32_t>(Precedence::NUM_OF_PRECEDENCE),
                          "Unknown default precedence is specified for sparta::Pipeline!");

            if (static_cast<uint32_t>(default_precedence) == static_cast<uint32_t>(Precedence::NONE)) {
                return;
            }

            bool forward = (static_cast<uint32_t>(default_precedence) == static_cast<uint32_t>(Precedence::FORWARD));

            uint32_t pid = 0;
            uint32_t cid = pid + 1;
            while (cid < event_list_at_stage_.size()) {
                if (event_list_at_stage_[pid].empty()) {
                    ++pid;
                } else if (event_list_at_stage_[cid].empty()) {
                    ++cid;
                } else {
                    if (forward) {
                        // This function can only be called when 'default_precedence_' is Precedence::NONE
                        setPrecedenceBetweenStage(pid, cid);
                    } else {
                        setPrecedenceBetweenStage(cid, pid);
                    }

                    // Skip the empty stages between producer and consumer to avoid duplicated checking
                    pid = cid;
                }

                if (pid == cid) {
                    ++cid;
                }
            }

            default_precedence_ = default_precedence;
        }

        /*!
         * \brief Specify producer event for the pipeline update event
         * \param ev_handler The producer event handler
         *
         * \note Since pipeline update event happens on the Update phase,
         * ev_handler is also expected to be on the same phase
         */
        template<typename EventType>
        void setProducerForPipelineUpdate(EventType & ev_handler)
        {
            auto phase = ev_handler.getScheduleable().getSchedulingPhase();
            sparta_assert(phase == SchedulingPhase::Update,
                          "Cannot set producer event for pipeline update event, it's not on the Update phase!");
            ev_handler.getScheduleable().precedes(ev_pipeline_update_.getScheduleable());
        }

        /*!
         * \brief Specify consumer event for the pipeline update event
         * \param ev_handler The consumer event handler
         *
         * \note Since pipeline update event happens on the Update phase,
         * ev_handler is also expected to be on the same phase
         */
        template<typename EventType>
        void setConsumerForPipelineUpdate(EventType & ev_handler)
        {
            auto phase = ev_handler.getScheduleable().getSchedulingPhase();
            sparta_assert(phase == SchedulingPhase::Update,
                          "Cannot set consumer event for pipeline update event, it's not on the Update phase!");
            ev_pipeline_update_.getScheduleable().precedes(ev_handler.getScheduleable());
        }

        /*!
         * \brief Specify producer event for a designated pipeline stage
         *
         * \param id The stage number
         * \param ev_handler The producer event handler
         */
        template<typename EventType>
        void setProducerForStage(const uint32_t & id, EventType & ev_handler)
        {
            sparta_assert((id < event_list_at_stage_.size()) && (event_list_at_stage_[id].size() > 0),
                          "Precedence setup fails: No handler for pipeline stage[" << id << "]!");

            auto phase_id = static_cast<uint32_t>(ev_handler.getScheduleable().getSchedulingPhase());
            auto & event_list = event_matrix_at_stage_[id][phase_id];

            sparta_assert(!event_list.empty(),
                          "Cannot set producer event for pipeline stage[" << id << "]. No registered stage event on the SAME phase!");
            if constexpr (std::is_same_v<EventT, PhasedPayloadEvent<DataT>>) {
                ev_handler.getScheduleable().precedes((event_list.front())->getScheduleable());
            } else {
                ev_handler.getScheduleable().precedes(*(event_list.front()));
            }
        }

        /*!
         * \brief Specify consumer event for a designated pipeline stage
         *
         * \param id The stage number
         * \param ev_handler The consumer event handler
         */
        template<typename EventType>
        void setConsumerForStage(const uint32_t & id, EventType & ev_handler)
        {
            sparta_assert((id < event_list_at_stage_.size()) && (event_list_at_stage_[id].size() > 0),
                          "Precedence setup fails: No handler for pipeline stage[" << id << "]!");

            auto phase_id = static_cast<uint32_t>(ev_handler.getScheduleable().getSchedulingPhase());
            auto & event_list = event_matrix_at_stage_[id][phase_id];

            sparta_assert(!event_list.empty(),
                          "Cannot set consumer event for pipeline stage[" << id
                          << "]. No registered stage event on the SAME phase!");
            if constexpr (std::is_same_v<EventT, PhasedPayloadEvent<DataT>>) {
                (event_list.back())->getScheduleable().precedes(ev_handler.getScheduleable());
            } else {
                (event_list.back())->precedes(ev_handler.getScheduleable());
            }
        }

        /*!
         * \brief Get events at a stage for a particular scheduling phase
         * \param id The stage number
         * \note This function can ONLY be called when there is already a
         * registered event handler for the designated pipeline stage.
         */
        EventList & getEventsAtStage(const uint32_t & id,
                                     const SchedulingPhase phase = SchedulingPhase::Tick)
        {
            sparta_assert(id < event_matrix_at_stage_.size(),
                          "Attempt to get events at an invalid pipeline stage["
                          << id << "]!");
            using SchedUType = std::underlying_type<SchedulingPhase>::type;
            auto & event_list = event_matrix_at_stage_[id][static_cast<SchedUType>(phase)];
            sparta_assert(!event_list.empty(),
                          "No registered events at stage[" << id << "]!");
            return event_list;
        }

        /*!
         * \brief Check if any event is registered at a designated pipeline stage
         *
         * \param id The stage number
         *
         * \note The function needs to be called before
         *       activating/deactivating event for a designated stage
         *       if the user is not sure whether this stage has been
         *       registered with any event.
         */
        bool isEventRegisteredAtStage(const uint32_t & id) const {
            sparta_assert(id < event_list_at_stage_.size(),
                          "Attempt to check event handler for invalid pipeline stage[" << id << "]!");

            return (event_list_at_stage_[id].size() > 0);
        }

        /*!
         * \brief Activate event for a designated pipeline stage
         *
         * \param id The stage number
         *
         * \note This function can ONLY be called when there is
         *       already a registered event handler for the designated
         *       pipeline stage.
         */
        void activateEventAtStage(const uint32_t & id)
        {
            sparta_assert(id < event_list_at_stage_.size(),
                          "Attempt to activate event handler for invalid pipeline stage[" << id << "]!");
            sparta_assert((event_list_at_stage_[id].size() > 0),
                          "Activation fails: No registered event handler for stage[" << id << "]!");

            events_valid_at_stage_[id] = true;
        }

        /*!
         * \brief Deactivate event for a designated pipeline stage
         *
         * \param id The stage number
         *
         * \note This function can ONLY be called when there is already a registered event handler
         *       for the designated pipeline stage.
         */
        void deactivateEventAtStage(const uint32_t & id)
        {
            sparta_assert(id < event_list_at_stage_.size(),
                          "Attempt to deactivate event handler for invalid pipeline stage[" << id << "]!");
            sparta_assert((event_list_at_stage_[id].size() > 0),
                          "Deactivation fails: No registered event handler for stage[" << id << "]!");

            events_valid_at_stage_[id] = false;
        }

        /*!
         * \brief Append data to the beginning of the pipeline
         *
         * \param item The data to be appended to the front of pipeline
         */
        void append(const DataT & item)
        {
            appendImpl_(item);
        }

        /*!
         * \brief Append data to the beginning of the pipeline
         *
         * \param item The data to be appended to the front of pipeline
         */
        void append(DataT && item)
        {
            return appendImpl_(std::move(item));
        }

        /*!
         * \brief Is the pipe already appended data?
         */
        bool isAppended() const
        {
            return pipe_.isAppended();
        }

        /*!
         * \brief Get the data just appended; it will assert if
         *        no data appended
         */
        const DataT& readAppendedData() const
        {
            return pipe_.readAppendedData();
        }

        /*!
         * \brief Modify a specific stage of the pipeline
         *
         * \param stage_id The stage number
         * \param item The modified data for this designated stage
         */
        void writeStage(const uint32_t & stage_id, const DataT & item)
        {
            writeStageImpl_(stage_id, item);
        }

        /*!
         * \brief Modify a specific stage of the pipeline
         *
         * \param stage_id The stage number
         * \param item The modified data for this designated stage
         */
        void writeStage(const uint32_t & stage_id, DataT && item)
        {
            writeStageImpl_(stage_id, std::move(item));
        }

        /*!
         * \brief Invalidate a specific stage of the pipeline
         *
         * \param stage_id The stage number
         */
        void invalidateStage(const uint32_t & stage_id)
        {
            pipe_.invalidatePS(stage_id);

            if (perform_own_update_) {
                ev_pipeline_update_.schedule();
            }
        }

        /*!
         * \brief Stall the pipeline up to designated stage for
         *        a specified number of cycles
         *
         * \param stall_stage_id The stage that causes the pipeline stall
         * \param stall_cycles The total number of stall cycles
         * \param crush_bubbles Allow stages before the stall point to move forward into empty slots
         * \param suppress_event Suppress events of stages before the stall point
         */
        void stall(const uint32_t & stall_stage_id,
                   const uint32_t & stall_cycles,
                   const bool crush_bubbles=false,
                   const bool suppress_events=true)
        {
            sparta_assert(pipe_.isValid(stall_stage_id), "Try to stall an empty pipeline stage!");
            sparta_assert(!isStalledOrStalling(), "Try to stall a pipeline that is stalling or already stalled!");

            if (stall_cycles == 0) {
                return;
            }

            stall_cycles_ = stall_cycles;
            stall_stage_id_ = stall_stage_id;
            deactivate_(stall_stage_id_, crush_bubbles, suppress_events);
        }

        /*!
         * \brief Check if the pipeline will be stalled the very next cycle
         * \note This implies either of the following two cases:
         *       - The pipeline is already stalled, and needs to stall (at least 1) more cycle(s).
         *       - The pipeline not stalled, but is about to stall next cycle.
         */
        bool isStalledOrStalling() const {
            return (stall_cycles_ > 0) || (stall_stage_id_ < std::numeric_limits<uint32_t>::max());
        }

        /*!
         * \brief Check if the designated pipeline stage will be stalled the very next cycle
         * \note This implies either of the following two cases:
         *       - The pipeline stage is already stalled, and needs to stall (at least 1) more cycle(s).
         *       - The pipeline stage is not stalled, but is about to stall next cycle.
         */
        bool isStalledOrStallingAtStage(const uint32_t & stage_id) const {
            return (isStalledOrStalling() && stage_id <= stall_stage_id_);
        }

        /*!
         * \brief Flush a specific stage of the pipeline using stage id
         * \param stage_id The stage number
         * \note All the pipeline stage handling events (after SchedulingPhase::Flush) will be cancelled.
         *       If the pipeline stage is stalled or about to stall, and the flushing stage happens
         *       to be the stall-causing stage, then the pipeline stall will be cancelled also.
         */
        void flushStage(const uint32_t & flush_stage_id) {
            cancelEventsAtStage_(flush_stage_id);

            if (flush_stage_id == stall_stage_id_) {
                sparta_assert(isStalledOrStalling());
                restart_(stall_stage_id_);

                stall_cycles_ = 0;
                stall_stage_id_ = std::numeric_limits<uint32_t>::max();
            }

            pipe_.flushPS(flush_stage_id);
        }

        /*!
         * \brief Flush a specific stage of the pipeline using const iterator
         * \param const_iter A const iterator pointing to a specific pipeline stage
         */
        void flushStage(const const_iterator & const_iter) {
            flushStage(const_iter.index_);
        }

        /*!
         * \brief Flush a specific stage of the pipeline using non-const iterator
         *
         * \param iter A non-const iterator pointing to a specific pipeline stage
         */
        void flushStage(const iterator & iter) {
            flushStage(iter.index_);
        }

        /*!
         * \brief Flush all stages of the pipeline
         */
        void flushAllStages() {
            for (uint32_t stage_id = 0; stage_id < num_stages_; stage_id++) {
                flushStage(stage_id);
            }
        }

        /*!
         * \brief Flush the data just appended
         */
        void flushAppend() {
            pipe_.flushAppend();
        }

        /*!
         * \brief set whether the update event is continuing or not
         *
         * \param value true for continuing, false for non-continuing
         * \details This will change the continuing property of ev_pipeline_update_. The purpose of this is to
         * determine whether data moving through this pipeline should prevent simulation from ending or not. If this
         * event is continuing, then the Pipeline will keep calling this event as long as there are items in the
         * Pipeline. These events will block the simulator from exiting.
         */
        void setContinuing(const bool value) {
            ev_pipeline_update_.setContinuing(value);
        }

        /*!
         * \brief Access (read-only) a specific stage of the pipeline
         *
         * \param stage_id The stage number
         */
        const DataT & operator[](const uint32_t & stage_id) const { return pipe_.read(stage_id); }

        /*!
         * \brief Access a specific stage of the pipeline
         *
         * \param stage_id The stage number
         */
        DataT & operator[](const uint32_t & stage_id) { return pipe_.access(stage_id); }

        /*!
         * \brief Access (read-only) a specific stage of the pipeline
         *
         * \param stage_id The stage number
         */
        const DataT & at(const uint32_t & stage_id) const { return pipe_.read(stage_id); }

        /*!
         * \brief Access a specific stage of the pipeline
         *
         * \param stage_id The stage number
         */
        DataT & at(const uint32_t & stage_id) { return pipe_.access(stage_id); }

        /*!
         * \brief Indicate the validity of a specific pipeline stage
         *
         * \param stage_id The stage number
         */
        bool isValid(const uint32_t & stage_id) const { return pipe_.isValid(stage_id); }

        //! Indicate the validity of the last pipeline stage
        bool isLastValid() const { return pipe_.isLastValid(); }

        //! Indicate the validity of the whole pipeline
        bool isAnyValid() const { return pipe_.isAnyValid(); }

        //! Indicate the number of valid pipeline stages
        uint32_t numValid() const { return pipe_.numValid(); }

        //! Indicate no valid pipeline stages
        bool empty() const { return (numValid() == 0); }

        //! Indicate the pipeline size
        uint32_t size() const { return pipe_.size(); }

        //! Indicate the pipeline capacity
        uint32_t capacity() const { return num_stages_; }

        /*!
         * \brief Ask pipeline to perform its own update
         *
         * \note The pipeline always performs its own update at SchedulingPhase::Update.
         *       This function has to be called at the beginning of the simulation ONLY IF
         *       the user doesn't want to manually perform the pipeline update.
         *
         * \sa update()
         */
        void performOwnUpdates()
        {
            if (!perform_own_update_) {
                perform_own_update_ = true;

                if (pipe_.isAnyValid()) {
                    ev_pipeline_update_.schedule();
                }
            }
        }

        /*!
         * \brief Manually update pipeline (i.e. data-movement and event-scheduling)
         *
         * \note The user is able to manually update pipeline in ANY SchedulingPhase.
         *       However, it is the user's responsibility to make sure that the SchedulingPhase
         *       of ALL the registered pipeline stage events are greater than or equal to the
         *       SchedulingPhase when this manual update function is called.
         *
         * \sa performOwnUpdates()
         */
        void update()
        {
            sparta_assert(perform_own_update_ == false, "You asked me to perform my own update!");

            internalUpdate_();
        }

        /*!
         * \brief Enable pipeline collection
         *
         * \param parent A pointer to the parent treenode for which to add
         *               Collectable objects under.
         *
         * \note This only sets the Pipeline up for collection.
         *       Collection must be started with an instatiation of the PipelineCollector.
         */
        template<SchedulingPhase phase = SchedulingPhase::Collection>
        void enableCollection(TreeNode * parent) { pipe_.template enableCollection<phase>(parent); }

    private:
        template<typename U>
        void appendImpl_(U && item)
        {
            pipe_.append(std::forward<U>(item));

            if (perform_own_update_) {
                ev_pipeline_update_.schedule();
            }
        }

        template<typename U>
        void writeStageImpl_(const uint32_t & stage_id, U && item)
        {
            pipe_.writePS(stage_id, std::forward<U>(item));

            if (perform_own_update_) {
                ev_pipeline_update_.schedule();
            }
        }

        //! Perform pipeline forward progression (i.e. data-movement and event-scheduling)
        void internalUpdate_()
        {
            if (!isStalledOrStalling()) {
                pipe_.update();
                scheduleEventForEachStage_();
            } else {
                drain_(stall_stage_id_ + 1);
                scheduleEventForEachStage_();

                --stall_cycles_;
                if (stall_cycles_ == 0) {
                    restart_(stall_stage_id_);
                    stall_stage_id_ = std::numeric_limits<uint32_t>::max();
                }
            }

            if (pipe_.isAnyValid() && perform_own_update_) {
                ev_pipeline_update_.schedule();
            }
        }

        //! Drain the rest of unstalled pipeline(stages after the stall causing stage).
        void drain_(const uint32_t start_id)
        {
            uint32_t stage_id = num_stages_ - 1;

            // remove last item from pipe
            if (start_id <= stage_id && pipe_.isValid(stage_id)) {
                pipe_.invalidatePS(stage_id);
            }

            // advance stages after stall point
            while (stage_id > start_id) {
                if (pipe_.isValid(stage_id - 1)) {
                    pipe_.writePS(stage_id, pipe_.access(stage_id - 1));
                    pipe_.invalidatePS(stage_id - 1);
                }

                --stage_id;
            }

            // check stages up to stall point for bubbles
            // NOTE: stage_id now points to 1 past the stall point
            // The stall point cannot advance because its events are disabled
            // The stage before the stall cannot advance because the stall stage is occupied
            // So, there are two useless loops here
            while (stage_id > 0) {
                if (advance_into_stage_[stage_id - 1] &&
                    !pipe_.isValid(stage_id) && pipe_.isValid(stage_id - 1)) {
                    pipe_.writePS(stage_id, pipe_.access(stage_id - 1));
                    pipe_.invalidatePS(stage_id - 1);
                }

                --stage_id;
            }

            // try advancement from insertion point
            pipe_.shiftAppend();
        }

        /*!
         * \brief Cancel pipeline stage handling events that are already scheduled for this cycle.
         * \note This typically happens on pipeline flush, which cancels all the pipeline stage handling events
         *       (after SchedulingPhase::Flush) that have been scheduled before flush.
         */
        void cancelEventsAtStage_(const uint32_t & stage_id)
        {
            sparta_assert(stage_id < num_stages_,
                          "Try to cancel events for invalid pipeline stage[" << stage_id << "]");
            if (pipe_.isValid(stage_id) && events_valid_at_stage_[stage_id]) {
                sparta_assert(event_list_at_stage_[stage_id].size());
                for (const auto & ev_ptr :  event_list_at_stage_[stage_id]) {
                    ev_ptr->cancel(sparta::Clock::Cycle(0));
                }
            }
        }

        //! Schedule events for active pipeline stages
        void scheduleEventForEachStage_()
        {
            sparta_assert(num_stages_ == event_list_at_stage_.size());

            for (uint32_t i = 0; i < num_stages_; i++) {
                if (pipe_.isValid(i) && events_valid_at_stage_[i]) {
                    sparta_assert(event_list_at_stage_[i].size());
                    for (const auto & ev_ptr :  event_list_at_stage_[i]) {
                        if constexpr (std::is_same_v<EventT, PhasedPayloadEvent<DataT>>) {
                            ev_ptr->preparePayload(at(i))->schedule(sparta::Clock::Cycle(0));
                        } else {
                            ev_ptr->schedule(sparta::Clock::Cycle(0));
                        }
                    }
                }
            }
        }

        //! Deactivate the pipeline stage handling events up to the stall causing stage
        void deactivate_(const uint32_t & stall_stage_id,
                         const bool crush_bubbles,
                         const bool suppress_events)
        {
            sparta_assert(stall_stage_id < num_stages_,
                          "Try to deactivate events for invalid pipeline stage[" << stall_stage_id << "]");

            for (int32_t stage_id = stall_stage_id; stage_id >= 0; stage_id--) {

                if (crush_bubbles && !pipe_.isValid(stage_id)) {
                    return; // bubble, crush it by allowing earlier stages to advance
                }

                if (suppress_events && event_list_at_stage_[stage_id].size() > 0) {
                    events_valid_at_stage_[stage_id] = false;
                }
                advance_into_stage_[stage_id] = false;
            }
        }

        //! Restart the pipeline stage handling events up to the stall causing stage
        void restart_(const uint32_t & stall_stage_id)
        {
            for (uint32_t stage_id = 0; stage_id <= stall_stage_id; stage_id++) {
                sparta_assert(stage_id < num_stages_,
                              "Try to restart invalid pipeline stage[" << stage_id << "]");
                if (event_list_at_stage_[stage_id].size() > 0) {
                    events_valid_at_stage_[stage_id] = true;
                }
                advance_into_stage_[stage_id] = true;
            }
        }

        //! Name of the pipeline
        const std::string name_;

        //! The clock this pipeline uses
        const Clock * clock_;

        //! Internal data movement pipe
        sparta::Pipe<DataT> pipe_;

        //! A vector of pipeline stage event handler pointers
        std::vector<EventHandleList> event_list_at_stage_;
        // NOTE:
        // (1) One advantage of using PhasedUniqueEvent instead of UnqiueEvent is:
        //     The pipeline stage events could potentially be scheduled in different phases.
        // (2) Scheduleable also works, but EventNode doesn't work (no scheduleable)
        //     std::vector<std::unique_ptr<Scheduleable>> event_list_at_stage_; // OK
        //     std::vector<std::unique_ptr<EventNode>> event_list_at_stage_; // oops

        //! A vector of valid/active bits for pipeline stage events
        std::vector<bool> events_valid_at_stage_;
        std::vector<bool> advance_into_stage_;

        //! A vector of event index matrix for every pipeline stage
        std::vector<EventMatrix> event_matrix_at_stage_;

        //! Pipeline event set
        sparta::EventSet dummy_es_{nullptr};
        sparta::EventSet * es_;

        //! Pipeline update event handler
        UniqueEvent<SchedulingPhase::Update> ev_pipeline_update_;

        //! Indicate automatic or manual pipeline update
        bool perform_own_update_ = false;

        //! Indicate number of pipeline stages
        const uint32_t num_stages_ = 0;

        //! Indicate a default precedence between stages has been set
        Precedence default_precedence_ = Precedence::NONE;

        //! Total number of stall cycles left
        uint32_t stall_cycles_ = 0;

        //! Stall causing stage id
        uint32_t stall_stage_id_ = std::numeric_limits<uint32_t>::max();
    };

}
