// <Collectable.hpp>  -*- C++ -*-

/**
 * \file Collectable.hpp
 *
 * \brief Implementation of the Collectable class that allows
 *        a user to collect an object into an Argos pipeline file
 */

#ifndef __PIPELINE_COLLECTABLE_H__
#define __PIPELINE_COLLECTABLE_H__

#include <algorithm>
#include <sstream>
#include <functional>
#include <type_traits>
#include <iomanip>

#include "sparta/collection/PipelineCollector.hpp"
#include "sparta/argos/transaction_structures.hpp"
#include "sparta/events/PayloadEvent.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/SchedulingPhases.hpp"
#include "sparta/pairs/SpartaKeyPairs.hpp"
#include "sparta/utils/Utils.hpp"
#include "sparta/utils/MetaStructs.hpp"

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

        template<typename DataT, SchedulingPhase collection_phase = SchedulingPhase::Collection, typename = void>
        class Collectable : public CollectableTreeNode
        {
        public:

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
                CollectableTreeNode(sparta::notNull(parent), name, group, index, desc),
                event_set_(this),
                ev_close_record_(&event_set_, name + "_pipeline_collectable_close_event",
                                 CREATE_SPARTA_HANDLER_WITH_DATA(Collectable, closeRecord, bool))
            {
                sparta_assert(getNodeUID() < ~(decltype(transaction_t::location_ID))0,
                            "Tree has at least " << getNodeUID() << " in it. Because pipeouts "
                            "use these locations and only have 32b location IDs, the pipeline "
                            "collection system imposes a limit of " << ~(decltype(transaction_t::location_ID))0
                            << " nodes in the tree. This is expected to be an unattainable number "
                            "without some kind of runaway allocation bug. If you are sure you "
                            << " nodes in the tree. This is expected to be an unattainable number "
                            "without some kind of runaway allocation bug. If you are sure you "
                            "need more nodes than this, contact the SPARTA team. In the meantime, "
                            "it is probably safe to comment out this assertion if you do not "
                            "plan to use the pipeline collection functionality");

                argos_record_.location_ID        = boost::numeric::converter<decltype(argos_record_.location_ID),
                                                                             decltype(getNodeUID())>::convert(getNodeUID());
                argos_record_.control_Process_ID = 0; // what is this for?
                argos_record_.parent_ID          = parentid;
                argos_record_.flags              = is_Annotation;
                argos_record_.time_Start         = 0;
                argos_record_.time_End           = 1;


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
                            desc)
            {
                collected_object_ = collected_object;

                // Get an initial value, if available
                if(collected_object) {
                    std::ostringstream ss;
                    ss << *collected_object;
                    prev_annot_ = ss.str();
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
            {}

            //! Virtual destructor -- does nothing
            virtual ~Collectable() {}

            /**
             * \brief For manual collection, provide an initial value
             * \param val The value to initial the record with
             */
            void initialize(const DataT & val) {
                std::ostringstream ss;
                ss << val;
                prev_annot_ = ss.str();
            }

            //! Explicitly/manually collect a value for this collectable, ignoring
            //! what the Collectable is currently pointing to.
            void collect(const DataT & val)
            {
                if(SPARTA_EXPECT_FALSE(isCollected()))
                {
                    std::ostringstream ss;
                    ss << val;
                    if((ss.str() != prev_annot_) && !record_closed_)
                    {
                        // Close the old record (if there is one)
                        closeRecord();
                    }

                    // Remember the new string for a new record and start
                    // a new record if not empty.
                    prev_annot_ = ss.str();
                    if(!prev_annot_.empty() && record_closed_) {
                        startNewRecord_();
                        record_closed_ = false;
                    }
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
            void collectWithDuration(const DataT & val, sparta::Clock::Cycle duration)
            {
                if(SPARTA_EXPECT_FALSE(isCollected()))
                {
                    if(duration != 0) {
                        ev_close_record_.preparePayload(false)->schedule(duration);
                    }
                    collect(val);
                }
            }

            //! Virtual method called by
            //! CollectableTreeNode/PipelineCollector when a user of the
            //! TreeNode requests this object to be collected.
            void collect() override final {
                collect(*collected_object_);
            }

            /*!
             * \brief Calls collectWithDuration using the internal collected_object_
             * specified at construction.
             * \pre Must have constructed wit ha non-null collected object
             */
            void collectWithDuration(sparta::Clock::Cycle duration) {
                collectWithDuration(*collected_object_, duration);
            }

            //! For heartbeat collections, existing records will need to
            //! be closed and reopened
            void restartRecord() override final {
                // Do not get a new ID when we're doing a heartbeat
                if(!record_closed_) {
                    argos_record_.flags |= CONTINUE_FLAG;
                    writeRecord_();
                    argos_record_.flags &= ~CONTINUE_FLAG;
                    startNewRecord_();
                }
            }

            //! Force close a record.  This will close the record
            //! immediately and clear the field for the next cycle
            void closeRecord(const bool & simulation_ending = false) override final
            {
                if(SPARTA_EXPECT_FALSE(isCollected()))
                {
                    if(!record_closed_ && writeRecord_(simulation_ending)) {
                        prev_annot_.clear();
                    }
                    record_closed_ = true;
                }
            }

            //! \brief Do not perform any automatic collection
            //! The SchedulingPhase is ignored
            void setManualCollection() {
                auto_collect_ = false;
            }

        protected:

            //! \brief Get a reference to the internal event set
            //! \return Reference to event set -- used by DelayedCollectable
            EventSet & getEventSet_() {
                return event_set_;
            }

        private:

            //! Return true if the annotation was written; false otherwise
            bool writeRecord_(bool simulation_ending = false)
            {
                sparta_assert(pipeline_col_,
                            "Must startCollecting_ on this Collectable "
                            << getLocation() << " before a record can be written");

                // This record hasn't changed, don't write it
                if(__builtin_expect(argos_record_.time_Start ==
                                    pipeline_col_->getScheduler()->getCurrentTick(), false)) {
                    return false;
                }

                // Set a new transaction ID since we're starting anew
                argos_record_.transaction_ID = pipeline_col_->getUniqueTransactionId();

                // Notice that we make the length +1 in order to null
                // terminate the data.
                argos_record_.length = (uint16_t)(prev_annot_.size() + 1);

                // We should just change the transaction structure to take
                // an std::string instead of char* in the future.
                argos_record_.annt = prev_annot_.c_str();

                // Capture the end time
                argos_record_.time_End =
                    pipeline_col_->getScheduler()->getCurrentTick() + (simulation_ending ? 1 : 0);;

                // If this assert fires, then a user is trying to collect
                // a record multiple times in their cycle window or
                // something else really bad has happened.
                sparta_assert(argos_record_.time_Start < argos_record_.time_End);

                // Write the old record to the file
                pipeline_col_->writeRecord(argos_record_);

                return true;
            }

            //! Start a new record
            void startNewRecord_() {
                // Set up the new start time for the new record
                argos_record_.time_Start = pipeline_col_->getScheduler()->getCurrentTick();
            }

            //! Virtual method called by CollectableTreeNode when
            //! collection is enabled on the TreeNode
            void setCollecting_(bool collect, Collector * collector) override final
            {
                pipeline_col_ = dynamic_cast<PipelineCollector *>(collector);
                sparta_assert(pipeline_col_ != nullptr,
                            "Collectables can only added to PipelineCollectors... for now");

                if(collect && !prev_annot_.empty()) {
                    // Set the start time for this transaction to be
                    // moment collection is enabled.
                    startNewRecord_();
                }

                // If the collected object is null, this Collectable
                // object is to be explicitly collected
                if(collected_object_ && auto_collect_) {
                    if(collect) {
                        // Add this Collectable to the PipelineCollector's
                        // list of objects requiring collection
                        pipeline_col_->addToAutoCollection(this, collection_phase);
                    }
                    else {
                        // Remove this Collectable from the
                        // PipelineCollector's list of objects requiring
                        // collection
                        pipeline_col_->removeFromAutoCollection(this);
                    }
                }

                if(!collect && !record_closed_) {
                    // Force the record to be written
                    closeRecord();
                }
            }

            // The annotation object to be collected
            const DataT * collected_object_ = nullptr;

            // The live transaction record
            annotation_t argos_record_;

            // Store the result of the previous annotation, the
            // annotation_t struct holds a pointer to this
            std::string prev_annot_;

            // Ze Collec-tor
            PipelineCollector * pipeline_col_ = nullptr;

            // For those folks that want a value to automatically
            // disappear in the future
            sparta::EventSet event_set_;
            sparta::PayloadEvent<bool, sparta::SchedulingPhase::Trigger> ev_close_record_;

            // Is this collectable currently closed?
            bool record_closed_ = true;

            // Should we auto-collect?
            bool auto_collect_ = true;
        };

        /**
        * \class UniquePairGenerator
        * \brief A class which provides every new instantiation of the Templated class Collectable
        * with a new unique value. This value is used by all the instantiations of a certain type
        * of a Collectable as its own unique PairId which differentiates itself from other Collectable
        * instatiations templated on some other type of class.
        */
        class UniquePairIDGenerator {
        public:

            // We need to make Collectable a friend of this class, as it will directly call
            // the static private function from within itself.
            template<typename DataT, SchedulingPhase collection_phase, typename>
            friend class Collectable;
        private:

            // Any point this funtion is invoked, it releases a new 64bit integer which is unique.
            // So, we need to limit the access of this function.
            static uint64_t getUniquePairID_() {
                static uint64_t id = 0;
                return ++id;
             }
        };

        /**
         * \class Collectable
         * \brief Class used to either manually or auto-collect a Name Value Pair
         *        object in a pipeline database
         * \tparam DataT The DataT of the Pair Definition of the collectable being collected
         * \tparam collection_phase The phase collection will occur.  If
         *                          sparta::SchedulingPhase::Collection,
         *                          collection is done prior to Tick.
         *
         *  Auto-collection will occur only if a Collectable is
         *  constructed with a collected_object.  If no object is
         *  provided, it assumes a manual collection and the
         *  scheduling phase is ignored.
         *
         * We are templatizing the Collectable Class on the actual collectable to be collected.
         * So, the modeler does not have to maintain two different ways to call Collectable.
         * The modeler will always templatize Collectable on
         * the actual DataType or on a pointer to it, exactly the way they do now.
         * To enable this template overload, we need to check if the DataT the modeler
         * has templatized on Collectable class,
         * is actually a sparta::Pair type or not.
         * The only way to check this is to find out if this DataT type has a type alias inside
         * its namespace which is derived from sparta::PairDefinition class which in turn must be templatized
         * on the actual Collectable class or the DataT type itself, that we want to collect.
         * That is why we have a type named "type" inside the Actual Collectable class which refers to its
         * PairDefinition type.
         * For the same reason, we have a type named "type" inside the Pair Definition class which refers
         * to its Actual Collectable type.
         * The std::enable_if template switching basically checks if the DataType has a type named "type",
         * which is actually a PairDefinition of itself, a Pair Collectable Entity, or not.
         * The only way to check if DataType has a type "type" which is Pair Definition of itself, a
         * Collectable Entity or not, is by using SFINAE(Substitution Failure Is Not An Error).
         * It checks if DataType has a type "type" in its namespace which derives from sparta::PairDefinition
         * which is templatized on the the DataType itself.
         * If this exact piece of code is substituted to form a well-formed code, this template overload is
         * selected by compiler. If this does not work, then the compiler creates an ill-formed code.
         * Any ill-formed code during Template Parameter Substitution  is not considered an error.
         * It is rather thrown away, and the compiler moves to find a more generic Template Overload.
         *
         * \note This Template Overload is switched on only for Pair-Collectable User Defined DataTypes.
         * \note The Modeler might also pass a Shared Pointer to the actual DataT object
         * instead of the actual Object.
         * \note So, we handle that case by using remove_shared_ptr templated struct.
         */
        template<typename DataT, SchedulingPhase collection_phase>
        class Collectable<DataT, collection_phase, MetaStruct::enable_if_t<
            std::is_base_of<sparta::PairDefinition<MetaStruct::remove_any_pointer_t<DataT>>,
                typename MetaStruct::remove_any_pointer_t<DataT>::type>::value>> :
                    public sparta::PairCollector<typename MetaStruct::remove_any_pointer_t<DataT>::type>,
                        public CollectableTreeNode {
            // Aliasing the actual Datatype of the collectable being collected as Data_t
            typedef typename MetaStruct::remove_any_pointer<DataT>::type Data_t;

            // Aliasing the Datatype of the Pair Definition of the collectable being collected as PairDef_t
            typedef typename Data_t::type PairDef_t;

            // Making a bunch of APIs of PairCollector class being available to us with the using directive
            using PairCollector<PairDef_t>::getNameStrings;
            using PairCollector<PairDef_t>::getDataVector;
            using PairCollector<PairDef_t>::getStringVector;
            using PairCollector<PairDef_t>::getSizeOfVector;
            using PairCollector<PairDef_t>::getPEventLogVector;
            using PairCollector<PairDef_t>::getArgosFormatGuide;
            using PairCollector<PairDef_t>::collect_;
            using PairCollector<PairDef_t>::isCollecting;

        public:

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
                sparta::PairCollector<PairDef_t>(),
                CollectableTreeNode(sparta::notNull(parent), name, group, index, desc),
                argos_record_(pair_t(0, 1, parentid, 0,
                    boost::numeric::converter<decltype(argos_record_.location_ID),
                        decltype(getNodeUID())>::convert(getNodeUID()),
                            is_Pair, 0)),
                event_set_(this),
                ev_close_record_(&event_set_, name + "_pipeline_collectable_close_event",
                                 CREATE_SPARTA_HANDLER_WITH_DATA(Collectable, closeRecord, bool)) {
                sparta_assert(getNodeUID() < ~(decltype(argos_record_.location_ID))0,
                            "Tree has at least " << getNodeUID() << " in it. Because pipeouts "
                            "use these locations and only have 32b location IDs, the pipeline "
                            "collection system imposes a limit of " << ~(decltype(argos_record_.location_ID))0
                            << " nodes in the tree. This is expected to be an unattainable number "
                            "without some kind of runaway allocation bug. If you are sure you "
                            "need more nodes than this, contact the SPARTA team. In the meantime, "
                            "it is probably safe to comment out this assertion if you do not "
                            "plan to use the pipeline collection functionality");
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
                            desc)
            {
                collected_object_ = collected_object;

                // Get an initial value, if available
                if(collected_object) {
                    initialize(*collected_object);
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
            {}

            //! Virtual destructor -- does nothing
            virtual ~Collectable() {}

            /**
             * \brief For manual collection, provide an initial value
             * \param val The value to initialze the record with
             */
            template<typename T>
            MetaStruct::enable_if_t<!MetaStruct::is_any_pointer<T>::value, void>
            initialize(const T & val) {
                if(SPARTA_EXPECT_FALSE(isCollected())) {
                    collect(val);
                }
            }

            /**
             * \brief For manual collection, provide an initial value
             * \param val A reference to a pointer pointing to the value to initialze the record with
             */
            template<typename T>
            MetaStruct::enable_if_t<MetaStruct::is_any_pointer<T>::value, void>
            initialize(const T & val){
                if(SPARTA_EXPECT_FALSE(isCollected())) {
                    collect(*val);
                }
            }

            //! Explicitly/manually collect a value for this collectable, ignoring
            //! what the Collectable is currently pointing to.
            //! Here we pass the actual object of the collectable type we are collecting.
            template<typename T>
            MetaStruct::enable_if_t<!MetaStruct::is_any_pointer<T>::value, void>
            collect(const T & val)
            {
                collect_(val);

                // if the value pairs are not the same as the
                // previous value pairs and the previous record is still open
                if(!isSameRecord() && !record_closed_)
                {

                    //Close the old record (if there is one)
                    closeRecord();
                }

                // Remember the new value pairs for a new record and start
                // a new record if not empty.
                updateLastRecord_();

                // If the new Value pairs are not empty and current record is closed, we start a new record.
                if(!last_record_values_.empty() && record_closed_){
                    startNewRecord_();
                    record_closed_ = false;
                }
            }

            //! Explicitly/manually collect a value for this collectable, ignoring
            //! what the Collectable is currently pointing to.
            //! Here we pass the shared pointer to the actual object of the collectable type we are collecting.
            template <typename T>
            MetaStruct::enable_if_t<MetaStruct::is_any_pointer<T>::value, void>
            collect(const T & val){
                collect_(*val);

                // if the value pairs are not the same as the
                // previous value pairs and the previous record is still open
                if(!isSameRecord() && !record_closed_)
                {

                    //Close the old record (if there is one)
                    closeRecord();
                }

                // Remember the new value pairs for a new record and start
                // a new record if not empty.
                updateLastRecord_();

                // If the new Value pairs are not empty and current record is closed, we start a new record.
                if(!last_record_values_.empty() && record_closed_){
                    startNewRecord_();
                    record_closed_ = false;
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
            template<typename T>
            MetaStruct::enable_if_t<!MetaStruct::is_any_pointer<T>::value, void>
            collectWithDuration(const T & val, sparta::Clock::Cycle duration){
                if(SPARTA_EXPECT_FALSE(isCollected()))
                {
                    if(duration != 0) {
                        ev_close_record_.preparePayload(false)->schedule(duration);
                    }
                    collect(val);
                }
            }

            /*!
             * \brief Explicitly collect a value from a shared pointer for the given duration
             * \param duration The amount of time in cycles the value is available
             *
             * Explicitly collect a value for this collectable passed as a shared pointer for the
             * given amount of time.
             *
             * \warn No checks are performed if a new value is collected
             *       within the previous duration!
             */
            template<typename T>
            MetaStruct::enable_if_t<MetaStruct::is_any_pointer<T>::value, void>
            collectWithDuration(const T & val, sparta::Clock::Cycle duration){
                if(SPARTA_EXPECT_FALSE(isCollected()))
                {
                    if(duration != 0) {
                        ev_close_record_.preparePayload(false)->schedule(duration);
                    }
                    collect(*val);
                }
            }

            //! Virtual method called by
            //! CollectableTreeNode/PipelineCollector when a user of the
            //! TreeNode requests this object to be collected.
            void collect() override final {
                collect(*collected_object_);
            }

            /*!
             * \brief Calls collectWithDuration using the internal collected_object_
             * specified at construction.
             * \pre Must have constructed wit ha non-null collected object
             */
            void collectWithDuration(sparta::Clock::Cycle duration) {
                collectWithDuration(*collected_object_, duration);
            }

            //! For heartbeat collections, existing records will need to
            //! be closed and reopened
            void restartRecord() override final {

                // Do not get a new ID when we're doing a heartbeat
               if(!record_closed_) {
                    argos_record_.flags |= CONTINUE_FLAG;
                    writeRecord_();
                    argos_record_.flags &= ~CONTINUE_FLAG;
                    startNewRecord_();
                }
            }

            //! Force close a record.  This will close the record
            //! immediately and clear the field for the next cycle
            void closeRecord(const bool & simulation_ending = false) override final
            {
                if(SPARTA_EXPECT_FALSE(isCollected()))
                {
                    if(!record_closed_ && writeRecord_(simulation_ending)) {

                        // Clear the previous vector containing the Name Value pairs.
                        last_record_values_.clear();
                    }
                    record_closed_ = true;
                }
            }

            //! \brief Do not perform any automatic collection
            //! The SchedulingPhase is ignored
            void setManualCollection() {
                auto_collect_ = false;
            }

            //! \brief Before writing a record to file, we need to check
            //  if any of the old Values have changed or not.
            bool isSameRecord() const {
                return last_record_values_ == getDataVector();
            }

            //! \brief Strictly a Debug/Testing API.
            //!  Never to be called in real modeler's code.
            const std::string & dumpNameValuePairs(const DataT & val) {
                collect_(val);
                log_string_.clear();
                std::ostringstream ss;
                for(const auto & pairs : getPEventLogVector()){
                    ss << pairs.first << "(" << pairs.second << ") ";
                }
                log_string_ = ss.str();
                return log_string_;
            }

        protected:

            //! \brief Get a reference to the internal event set
            //! \return Reference to event set -- used by DelayedCollectable
            inline EventSet & getEventSet_() {
                return event_set_;
            }

            //! \brief Collectable is inheriting from PairCollector Class
            //  and this is a pure virtual function in base
            //! \brief We need to redefine and override this function,
            //  else Collectable will also become Abstract.
            virtual void generateCollectionString_() override {}

        private:

            /**
            * \brief A static function which sets the Unique PairId of a certain instantiation of Collectable.
            * Function contains a static variable which tells us if this templated instantiation of Collectable
            * already has been instantiated anytime before, or this is the first time we are instantiating it.
            * Checks the value of the static variable to see if 0 or not. If it is 0, it means this is the very
            * first time we are creating an instance of this class, Set the variable id to the getuniquePairID
            * function of the UniquePairIDGenerator class generates, guranteed to be unique
            * and not shared by any other Pair class.
            */
            inline static uint64_t getUniquePairID_() {
                static uint64_t id = 0;
                if(id){ return id; }
                id = sparta::collection::UniquePairIDGenerator::getUniquePairID_();
                return id;
            }

            //! \brief Update the vector containing the Name Value Pairs
            //  we collected from the last collection
            //! \brief Fill it with the new Name Value pairs from
            //  this cycle of Collection.
            void updateLastRecord_(){
                last_record_values_ = getDataVector();
            }

            //! \brief Send the Pair Structure Record to Outputter
            //  for writing to the Transaction Database File.
            bool writeRecord_(bool simulation_ending = false)
            {
                sparta_assert(pipeline_col_,
                            "Must startCollecting_ on this Collectable "
                            << getLocation() << " before a record can be written");

                // This record hasn't changed, don't write it
                if(__builtin_expect(argos_record_.time_Start ==
                                    pipeline_col_->getScheduler()->getCurrentTick(), false)) {
                    return false;
                }

                // Set a new transaction ID since we're starting anew
                argos_record_.transaction_ID = pipeline_col_->getUniqueTransactionId();

                argos_record_.pairId = getUniquePairID_();
                argos_record_.nameVector = getNameStrings();
                argos_record_.sizeOfVector = getSizeOfVector();
                argos_record_.valueVector = getDataVector();
                argos_record_.stringVector = getStringVector();
                argos_record_.length = argos_record_.nameVector.size();
                argos_record_.delimVector.emplace_back(getArgosFormatGuide());

                // Capture the end time
                argos_record_.time_End =
                    pipeline_col_->getScheduler()->getCurrentTick() + (simulation_ending ? 1 : 0);

                // If this assert fires, then a user is trying to collect
                // a record multiple times in their cycle window or
                // something else really bad has happened.
                sparta_assert(argos_record_.time_Start < argos_record_.time_End);

                // Write the old record to the file
                pipeline_col_->writeRecord(argos_record_);
                return true;
            }

            //! Start a new record
            void startNewRecord_() {

                // Set up the new start time for the new record
                argos_record_.time_Start = pipeline_col_->getScheduler()->getCurrentTick();
            }

            //! collection is enabled on the TreeNode
            void setCollecting_(bool collect, Collector * collector) override final
            {
                pipeline_col_ = dynamic_cast<PipelineCollector *>(collector);
                sparta_assert(pipeline_col_ != nullptr,
                            "Collectables can only added to PipelineCollectors... for now");

                if(collect && !last_record_values_.empty()){

                    // Set the start time for this transaction to be
                    // moment collection is enabled.
                    startNewRecord_();
                }

                // If the collected object is null, this Collectable
                // object is to be explicitly collected
                if(collected_object_ && auto_collect_) {
                    if(collect) {

                        // Add this Collectable to the PipelineCollector's
                        // list of objects requiring collection
                        pipeline_col_->addToAutoCollection(this, collection_phase);
                    }
                    else {

                        // Remove this Collectable from the
                        // PipelineCollector's list of objects requiring
                        // collection
                        pipeline_col_->removeFromAutoCollection(this);
                    }
                }
            }
            // The pair collectable object to be collected
            const DataT * collected_object_ = nullptr;

            // The live transaction record
            pair_t argos_record_;

            // The Vector of pairs of string and unsigned ints to hold
            // a Name String and its corresponding Value.
            typedef std::pair<uint64_t, bool> ValidPair;
            std::vector<ValidPair> last_record_values_;

            // Ze Collec-tor
            PipelineCollector * pipeline_col_ = nullptr;

            // For those folks that want a value to automatically
            // disappear in the future
            sparta::EventSet event_set_;
            sparta::PayloadEvent<bool, sparta::SchedulingPhase::Trigger> ev_close_record_;

            // Is this collectable currently closed?
            bool record_closed_ = true;

            // Should we auto-collect?
            bool auto_collect_ = true;

            std::string log_string_;
        };
    }//namespace collection
}//namespace sparta
#endif
