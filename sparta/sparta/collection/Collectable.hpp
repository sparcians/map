// <Collectable.hpp>  -*- C++ -*-

/**
 * \file Collectable.hpp
 *
 * \brief Implementation of the Collectable class that allows
 *        a user to collect an object into an pipeViewer pipeline file
 */

#pragma once

#include <algorithm>
#include <sstream>
#include <functional>
#include <type_traits>
#include <iomanip>

#include "sparta/collection/PipelineCollector.hpp"
#include "sparta/events/PayloadEvent.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/SchedulingPhases.hpp"
#include "sparta/utils/Utils.hpp"
#include "simdb/utils/TypeTraits.hpp"

#include "boost/numeric/conversion/converter.hpp"

namespace sparta{
    namespace collection
    {

        /**
         * \class Collectable
         * \brief Class used to either manually or auto-collect an Annotation String
         *        object in a pipeline database
         * \tparam DataT The DataT of the collectable being collected
         * \tparam collection_phase The phase collection will occur.  If
         *                          sparta::SchedulingPhase::Collection,
         *                          collection is done prior to Tick.
         *
         *  Auto-collection will occur only if a Collectable is
         *  constructed with a collected_object.  If no object is
         *  provided, it assumes a manual collection and the
         *  scheduling phase is ignored.
         */

        template<typename DataT, SchedulingPhase collection_phase = SchedulingPhase::Collection>
        class Collectable : public CollectableTreeNode
        {
            using ValueT = simdb::type_traits::remove_any_pointer_t<DataT>;

        public:
            static constexpr uint64_t BAD_DISPLAY_ID = 0x1000;

            /**
             * \brief Construct the Collectable, no data object associated, part of a group
             * \param parent A pointer to a parent treenode.  Must not be null
             * \param name The name for which to create this object as a child sparta::TreeNode
             * \param parentid The transaction id of a parent for this collectable; 0 for no parent
             * \param desc A description for the interface
             */
            Collectable(sparta::TreeNode* parent,
                        const std::string& name,
                        const std::string& group,
                        uint32_t index,
                        uint64_t parentid = 0,
                        const std::string & desc = "Collectable <manual, no desc>") :
                Collectable(parent, name, group, index, parentid, desc, true)
            {
            }

            /**
             * \brief Construct the Collectable
             * \param parent A pointer to a parent treenode.  Must not be null
             * \param name The name for which to create this object as a child sparta::TreeNode
             * \param collected_object Pointer to the object to collect during the "COLLECT" phase
             * \param parentid The transaction id of a parent for this collectable; 0 for no parent
             * \param desc A description for the interface
             */
            Collectable(sparta::TreeNode* parent,
                        const std::string& name,
                        const DataT * collected_object,
                        uint64_t parentid = 0,
                        const std::string & desc = "Collectable <no desc>") :
                Collectable(parent, name,
                            TreeNode::GROUP_NAME_NONE,
                            TreeNode::GROUP_IDX_NONE,
                            parentid,
                            desc, false)
            {
                collected_object_ = collected_object;

                if (auto collection = parent->getRoot()->getCollectionSystem(false)) {
                    auto path = getLocation();
                    auto clk_name = notNull(getClock())->getName();

                    impl_ = collection->collectScalarWithAutoCollection<DataT>(
                        path, clk_name, collected_object);

                    impl_->autoEnableOnCollect();
                }
            }

            /**
             * \brief Construct the Collectable, no data object associated
             * \param parent A pointer to a parent treenode.  Must not be null
             * \param name The name for which to create this object as a child sparta::TreeNode
             * \param parentid The transaction id of a parent for this collectable; 0 for no parent
             * \param desc A description for the interface
             */
            Collectable(sparta::TreeNode* parent,
                        const std::string& name,
                        uint64_t parentid = 0,
                        const std::string & desc = "Collectable <manual, no desc>") :
                Collectable(parent, name, nullptr, parentid, desc)
            {
                // Can't auto collect without setting collected_object_
                setManualCollection();
            }

            //! Virtual destructor -- does nothing
            virtual ~Collectable() {}

            /**
             * \brief For manual collection, provide an initial value
             * \param val The value to initial the record with
             */
            void initialize(const DataT & val) {
                sparta_assert(impl_);
                impl_->initializeValue(val);
            }

            //! Explicitly/manually collect a value for this collectable, ignoring
            //! what the Collectable is currently pointing to.
            void collect(const ValueT & val)
            {
                sparta_assert(impl_);
                impl_->collect(val);
            }

            template <typename T>
            std::enable_if_t<simdb::type_traits::is_any_pointer_v<T>, void>
            collect(const T& val)
            {
                if (val)
                {
                    collect(*val);
                }
                else
                {
                    closeRecord();
                }
            }

            /*!
             * \brief Explicitly collect a value for the given duration
             * \param duration The amount of time in cycles the value is available
             *
             * Explicitly collect a value for this collectable for the
             * given amount of time.
             *
             * \warn No checks are performed if a new value is collected
             *       within the previous duration!
             */
            void collectWithDuration(const ValueT & val, sparta::Clock::Cycle duration)
            {
                if(SPARTA_EXPECT_FALSE(isCollected()))
                {
                    if(duration != 0) {
                        ev_close_record_.preparePayload(false)->schedule(duration);
                    }
                    collect(val);
                }
            }

            template <typename T>
            std::enable_if_t<simdb::type_traits::is_any_pointer_v<T>, void>
            collectWithDuration(const T& val, sparta::Clock::Cycle duration)
            {
                if (val)
                {
                    collectWithDuration(*val, duration);
                }
                else
                {
                    closeRecord();
                }
            }

            //! Virtual method called by
            //! CollectableTreeNode/PipelineCollector when a user of the
            //! TreeNode requests this object to be collected.
            void collect() override final {
                // If pointer has become nullified, close the record
                if(nullptr == collected_object_) {
                    closeRecord();
                    return;
                }
                collect(*collected_object_);
            }

            /*!
             * \brief Calls collectWithDuration using the internal collected_object_
             * specified at construction.
             * \pre Must have constructed wit ha non-null collected object
             */
            void collectWithDuration(sparta::Clock::Cycle duration) {
                sparta_assert(collected_object_ != nullptr);
                collectWithDuration(*collected_object_, duration);
            }

            //! Force close a record.  This will close the record
            //! immediately and clear the field for the next cycle
            void closeRecord(const bool & = false) override final {
                if (impl_ && impl_->enabled())
                {
                    impl_->disable();
                }
            }

            //! \brief Do not perform any automatic collection
            //! The SchedulingPhase is ignored
            void setManualCollection() {
                if (impl_) {
                    impl_->enableAutoCollect(false);
                }
            }

        protected:

            //! \brief Get a reference to the internal event set
            //! \return Reference to event set -- used by DelayedCollectable
            EventSet & getEventSet_() {
                return event_set_;
            }

        private:
            Collectable(sparta::TreeNode* parent,
                        const std::string& name,
                        const std::string& group,
                        uint32_t index,
                        uint64_t parentid,
                        const std::string & desc,
                        const bool create_impl) :
                CollectableTreeNode(sparta::notNull(parent), name, group, index, desc),
                event_set_(this),
                ev_close_record_(&event_set_, name + "_pipeline_collectable_close_event",
                                CREATE_SPARTA_HANDLER_WITH_DATA(Collectable, closeRecord, bool))
            {
                if (create_impl) {
                    if (auto collection = parent->getRoot()->getCollectionSystem(false)) {
                        auto path = getLocation();
                        auto clk_name = notNull(getClock())->getName();
                        impl_ = collection->collectScalarManually<DataT>(path, clk_name);
                        impl_->autoEnableOnCollect();
                    }
                }
            }

            //! Virtual method called by CollectableTreeNode when
            //! collection is enabled on the TreeNode
            void setCollecting_(bool collect, Collector * collector) override final
            {
                sparta_assert(impl_);
                auto pc = static_cast<PipelineCollector*>(collector);

                // If the collected object is null, this Collectable
                // object is to be explicitly collected
                if(collected_object_ && impl_->autoCollecting()) {
                    if(collect) {
                        // Add this Collectable to the PipelineCollector's
                        // list of objects requiring collection
                        pc->addToAutoCollection(this, collection_phase);
                    }
                    else {
                        // Remove this Collectable from the
                        // PipelineCollector's list of objects requiring
                        // collection
                        pc->removeFromAutoCollection(this);
                    }
                }

                if(!collect) {
                    // Force the record to be written
                    closeRecord();
                }
            }

            // The annotation object to be collected
            const DataT * collected_object_ = nullptr;

            // For those folks that want a value to automatically
            // disappear in the future
            sparta::EventSet event_set_;
            sparta::PayloadEvent<bool, sparta::SchedulingPhase::Trigger> ev_close_record_;

            // SimDB collection point and entry into the DB pipeline (zlib/sqlite pipeline,
            // nothing to do with "pipeline" in "pipeline collector")
            std::shared_ptr<simdb::collection::ScalarCollector<DataT>> impl_;
        };
    }//namespace collection
}//namespace sparta
