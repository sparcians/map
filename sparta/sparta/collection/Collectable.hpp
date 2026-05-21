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
#include "sparta/pipeViewer/transaction_structures.hpp"
#include "sparta/events/PayloadEvent.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/SchedulingPhases.hpp"
#include "sparta/pairs/SpartaKeyPairs.hpp"
#include "sparta/utils/Utils.hpp"
#include "sparta/utils/MetaStructs.hpp"

#include "simdb/apps/argos/ArgosCollector.hpp"

#include "boost/numeric/conversion/converter.hpp"

#define ENTER_COLLECTION \
    [[maybe_unused]] auto bp = sparta::collection::CollectionDebugger::instance()->enterCollection<decltype(this)>();

namespace sparta{
    namespace collection
    {
        inline void debugMe() {}

        /**
         * \class CollectionDebugger
         * \note TODO cnyce: Remove this class once all bugs are fixed.
         */
        class CollectionDebugger
        {
        public:
            class DebugPoint
            {
            public:
                DebugPoint(CollectionDebugger* debugger, const std::string& dtype)
                    : debugger_(debugger)
                    , dtype_(dtype)
                {
                    if (debugger_->shouldBreakOn(dtype))
                    {
                        std::cout << "Breakpoint hit for collected type: " << dtype << std::endl;
                        debugMe();
                    }
                }

                bool keep = false;

                ~DebugPoint()
                {
                    if (keep)
                    {
                        debugger_->activate(dtype_);
                    }
                    else
                    {
                        debugger_->deactivate(dtype_);
                    }
                }

            private:
                CollectionDebugger* debugger_ = nullptr;
                std::string dtype_;
            };

            static CollectionDebugger* instance()
            {
                static CollectionDebugger debugger;
                return &debugger;
            }

            template <typename T>
            DebugPoint enterCollection()
            {
                auto dtype = simdb::demangle_type<T>();
                return DebugPoint(this, dtype);
            }

            void activate(const std::string& dtype)
            {
                active_dtypes_.insert(dtype);
                all_dtypes_.insert(dtype);
            }

            void deactivate(const std::string& dtype)
            {
                active_dtypes_.erase(dtype);
                all_dtypes_.insert(dtype);
            }

            bool shouldBreakOn(const std::string& dtype) const
            {
                if (all_dtypes_.count(dtype) == 0)
                {
                    return true;
                }
                return active_dtypes_.count(dtype) > 0;
            }

        private:
            CollectionDebugger() = default;
            std::unordered_set<std::string> active_dtypes_;
            std::unordered_set<std::string> all_dtypes_;
        };

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
            static constexpr uint64_t BAD_DISPLAY_ID = 0x1000;
            using ValueType = MetaStruct::remove_any_pointer_t<DataT>;

            // TODO cnyce: legacy use is supported for enums too
            static constexpr bool simdb_support_ = std::is_trivial_v<ValueType> && std::is_standard_layout_v<ValueType>;

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
            {
                // Can't auto collect without setting collected_object_
                setManualCollection();
            }

            //! Virtual destructor -- does nothing
            virtual ~Collectable() {}

            /**
             * \brief Collectable classes must be able to register themselves with
             * the ArgosCollector and store a ScalarCollector/ContainerCollector.
             */
            void createSimDbEntryPoint(simdb::argos::ArgosCollector* argos_collector) override final
            {
                // Bin collectors under IterableCollectors do not need an entry point.
                if (dynamic_cast<CollectableTreeNode*>(getParent()))
                {
                    return;
                }

                // Legacy (non-PairCollector) usage will be turned off in map_v3 for any struct-like
                // collectables. The user will have to create the nested SpartaPairDefinitionType
                // in order to continue collecting this type.
                if constexpr (simdb_support_)
                {
                    auto loc = getLocation();
                    auto clk_name = notNull(getClock())->getName();
                    entry_point_ = argos_collector->createScalarCollector<ValueType>(loc, clk_name);
                    owned_bit_bucket_ = std::make_unique<CollectableBitBucket>(
                        argos_collector->getTinyStrings());
                    bit_bucket_ = owned_bit_bucket_.get();
                }
                else
                {
                    argos_collector->markUnsupported(getLocation());
                }
            }

            /**
             * \brief Forward the BitBucket from a parent IterableCollector.
             * \note Only used for simdb-supported (trivial POD) legacy collectables.
             */
            void setBitBucket(BitBucket* bit_bucket)
            {
                if constexpr (simdb_support_)
                {
                    sparta_assert(dynamic_cast<CollectableTreeNode*>(getParent()) != nullptr);
                    sparta_assert(bit_bucket_ == nullptr);
                    sparta_assert(entry_point_ == nullptr);
                    bit_bucket_ = bit_bucket;
                }
                else
                {
                    (void)bit_bucket;
                }
            }

            /**
             * \brief Whether a scalar (Collectable) or container (IterableCollector),
             * serialize struct-like data structure hierarchies (field name + dtype)
             * to the database.
             */
            void serializeStructSchema(simdb::DatabaseManager*, std::map<std::string, int>&) override final
            {
                // Legacy (non-PairCollector) usage is not supported for struct-like types.
            }

            template <typename T>
            static void serializeStructSchema(simdb::DatabaseManager*, std::map<std::string, int>&)
            {
                // Legacy (non-PairCollector) usage is not supported for struct-like types.
            }

            /**
             * \brief For manual collection, provide an initial value
             * \param val The value to initial the record with
             */
            void initialize(const DataT & val) {
                // TODO cnyce
                (void)val;
            }

            //! Explicitly/manually collect a value for this collectable, ignoring
            //! what the Collectable is currently pointing to.
            void collect(const DataT & val)
            {
                ENTER_COLLECTION
                if constexpr (simdb_support_)
                {
                    if (entry_point_) {
                        bit_bucket_->reset();
                        bit_bucket_->dump(val);
                        auto bytes = static_cast<CollectableBitBucket*>(bit_bucket_)->release();
                        entry_point_->setScalarValueBytes(bytes);
                    } else if (bit_bucket_) {
                        bit_bucket_->dump(val);
                    }
                }
                else
                {
                    (void)val;
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
                if constexpr (simdb_support_)
                {
                    if(SPARTA_EXPECT_FALSE(isCollected()))
                    {
                        if(duration != 0) {
                            ev_close_record_.preparePayload(false)->schedule(duration);
                        }
                        collect(val);
                    }
                }
                else
                {
                    (void)val;
                    (void)duration;
                }
            }

            //! Virtual method called by
            //! CollectableTreeNode/PipelineCollector when a user of the
            //! TreeNode requests this object to be collected.
            void collect() override final {
                if constexpr (simdb_support_)
                {
                    // If pointer has become nullified, close the record
                    if(nullptr == collected_object_) {
                        closeRecord();
                        return;
                    }
                    collect(*collected_object_);
                }
            }

            /*!
             * \brief Calls collectWithDuration using the internal collected_object_
             * specified at construction.
             * \pre Must have constructed wit ha non-null collected object
             */
            void collectWithDuration(sparta::Clock::Cycle duration) {
                if constexpr (simdb_support_)
                {
                    // If pointer has become nullified, close the record
                    if(nullptr == collected_object_) {
                        closeRecord();
                        return;
                    }
                    collectWithDuration(*collected_object_, duration);
                }
            }

            //! Force close a record.  This will close the record
            //! immediately and clear the field for the next cycle
            void closeRecord(const bool & = false) override final
            {
                if constexpr (simdb_support_)
                {
                    if(SPARTA_EXPECT_FALSE(isCollected()))
                    {
                        entry_point_->quiet();
                    }
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

            //! Virtual method called by CollectableTreeNode when
            //! collection is enabled on the TreeNode
            void setCollecting_(bool collect, Collector * collector) override final
            {
                if constexpr (!simdb_support_)
                {
                    return;
                }

                pipeline_col_ = dynamic_cast<PipelineCollector *>(collector);
                sparta_assert(pipeline_col_ != nullptr,
                            "Collectables can only added to PipelineCollectors... for now");

                if(collect && !initial_bytes_.empty()) {
                    //TODO cnyce
                    //entry_point_->setBytes(initial_bytes_);
                    initial_bytes_.clear();
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

                if(!collect) {
                    closeRecord();
                }
            }

            // The annotation object to be collected
            const DataT * collected_object_ = nullptr;

            // Ze Collec-tor
            PipelineCollector * pipeline_col_ = nullptr;

            // For those folks that want a value to automatically
            // disappear in the future
            sparta::EventSet event_set_;
            sparta::PayloadEvent<bool, sparta::SchedulingPhase::Trigger> ev_close_record_;

            // Should we auto-collect?
            bool auto_collect_ = true;

            // Extracted value bytes from initialize() to be applied when
            // collection is first enabled (setCollecting_)
            std::vector<char> initial_bytes_;

            std::unique_ptr<CollectableBitBucket> owned_bit_bucket_;
            BitBucket* bit_bucket_ = nullptr;
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
         * That is why we have a type named "SpartaPairDefinitionType" inside the Actual Collectable class which refers to its
         * PairDefinition type.
         * For the same reason, we have a type named "SpartaPairDefinitionType" inside the Pair Definition class which refers
         * to its Actual Collectable type.
         * The std::enable_if template switching basically checks if the DataType has a type named "SpartaPairDefinitionType",
         * which is actually a PairDefinition of itself, a Pair Collectable Entity, or not.
         * The only way to check if DataType has a type "SpartaPairDefinitionType" which is Pair Definition of itself, a
         * Collectable Entity or not, is by using SFINAE(Substitution Failure Is Not An Error).
         * It checks if DataType has a type "SpartaPairDefinitionType" in its namespace which derives from sparta::PairDefinition
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
        class Collectable<DataT, collection_phase,
                          MetaStruct::enable_if_t<
                              std::is_base_of<sparta::PairDefinition<MetaStruct::remove_any_pointer_t<DataT>>,
                                              typename MetaStruct::remove_any_pointer_t<DataT>::SpartaPairDefinitionType>::value>> :
            public sparta::PairCollector<typename MetaStruct::remove_any_pointer_t<DataT>::SpartaPairDefinitionType>, public CollectableTreeNode
        {
            // Aliasing the actual Datatype of the collectable being collected as Data_t
            typedef MetaStruct::remove_any_pointer_t<DataT> Data_t;

            // Aliasing the Datatype of the Pair Definition of the collectable being collected as PairDef_t
            typedef typename Data_t::SpartaPairDefinitionType PairDef_t;

            // Making a bunch of APIs of PairCollector class being available to us with the using directive
            using PairCollector<PairDef_t>::getNameStrings;
            using PairCollector<PairDef_t>::getDataVector;
            using PairCollector<PairDef_t>::getStringVector;
            using PairCollector<PairDef_t>::getFormatVector;
            using PairCollector<PairDef_t>::getSizeOfVector;
            using PairCollector<PairDef_t>::getPEventLogVector;
            using PairCollector<PairDef_t>::getArgosFormatGuide;
            using PairCollector<PairDef_t>::collect_;
            using PairCollector<PairDef_t>::setBitBucket_;
            using PairCollector<PairDef_t>::setTinyStrings_;
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
                event_set_(this),
                ev_close_record_(&event_set_, name + "_pipeline_collectable_close_event",
                                 CREATE_SPARTA_HANDLER_WITH_DATA(Collectable, closeRecord, bool))
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
            {
                // Can't auto collect without setting collected_object_
                setManualCollection();
            }

            //! Virtual destructor -- does nothing
            virtual ~Collectable() {}

            /**
             * \brief Collectable classes must be able to register themselves with
             * the ArgosCollector and store a ScalarCollector/ContainerCollector.
             */
            void createSimDbEntryPoint(simdb::argos::ArgosCollector* argos_collector) override final
            {
                // Bin collectors under IterableCollectors do not need an entry point.
                // But in order to serialize strings, all the PairDefinitions need
                // the TinyStrings regardless.
                setTinyStrings_(argos_collector->getTinyStrings());
                if (dynamic_cast<CollectableTreeNode*>(getParent()))
                {
                    return;
                }

                auto loc = getLocation();
                auto clk_name = notNull(getClock())->getName();
                entry_point_ = argos_collector->createScalarCollector<Data_t>(loc, clk_name);
                owned_bit_bucket_ = std::make_unique<CollectableBitBucket>(argos_collector->getTinyStrings());
                bit_bucket_ = owned_bit_bucket_.get();
                setBitBucket_(bit_bucket_);
            }

            /**
             * \brief Forward the BitBucket along to the Pair objects in this collector.
             * \note Only to be called from IterableCollector.
             */
            void setBitBucket(BitBucket* bit_bucket)
            {
                sparta_assert(dynamic_cast<CollectableTreeNode*>(getParent()) != nullptr);
                sparta_assert(bit_bucket_ == nullptr);
                sparta_assert(entry_point_ == nullptr);
                bit_bucket_ = bit_bucket;
                setBitBucket_(bit_bucket_);
            }

            /**
             * \brief Whether a scalar (Collectable) or container (IterableCollector),
             * serialize struct-like data structure hierarchies (field name + dtype)
             * to the database.
             */
            void serializeStructSchema(simdb::DatabaseManager* db_mgr, std::map<std::string, int>& schema_ids_by_dtype_name) override final
            {
                serializeStructSchema<DataT>(db_mgr, schema_ids_by_dtype_name);
            }

            template <typename T>
            static void serializeStructSchema(simdb::DatabaseManager* db_mgr, std::map<std::string, int>& schema_ids_by_dtype_name)
            {
                using value_type = MetaStruct::remove_any_pointer_t<T>;
                if constexpr (std::is_trivial_v<value_type> && std::is_standard_layout_v<value_type>)
                {
                    // No need to serialize built-in types
                    return;
                }
                else if constexpr (std::is_enum_v<value_type>)
                {
                    // No need to serialize enum types
                    return;
                }
                else if constexpr (std::is_same_v<value_type, std::string>)
                {
                    // No need to serialize string types
                    return;
                }
                else if constexpr (std::is_same_v<std::decay_t<value_type>, const char*>)
                {
                    // No need to serialize string types
                    return;
                }
                else
                {
                    const auto root_dtype = simdb::demangle_type<value_type>();
                    if(schema_ids_by_dtype_name.find(root_dtype) != schema_ids_by_dtype_name.end()) {
                        return;
                    }

                    const int32_t schema_id =
                        db_mgr->INSERT(SQL_TABLE("DataTypeSchemas"), SQL_VALUES(root_dtype))->getId();

                    using PairDef = typename value_type::SpartaPairDefinitionType;
                    PairDef pair_def;
                    sparta::PairCache pair_cache;
                    pair_def.finalizeKeys(&pair_cache);

                    const auto & names = pair_cache.getNameStrings();
                    const auto & dtypes = pair_def.getLeafArgosDtypeStrings();
                    const auto & formatters = pair_cache.getFormatVector();
                    sparta_assert(names.size() == dtypes.size());
                    sparta_assert(formatters.size() == names.size());

                    std::vector<std::string> format_strings;
                    for(std::size_t i = 0; i < formatters.size(); ++i) {
                        std::string fmt_str;
                        switch(formatters[i]) {
                            case PairFormatter::HEX:
                                fmt_str = "HEX"; break;
                            case PairFormatter::OCTAL:
                                fmt_str = "OCT"; break;
                            default: break;
                        }
                        format_strings.push_back(fmt_str);
                    }

                    constexpr int32_t parent_id_unset = 0;
                    const std::string kind_pod{"pod"};
                    const std::string empty;

                    for(std::size_t i = 0; i < names.size(); ++i) {
                        db_mgr->INSERT(SQL_TABLE("DataTypeNodes"),
                                       SQL_VALUES(schema_id,
                                                  parent_id_unset,
                                                  kind_pod,
                                                  names[i],
                                                  empty,
                                                  dtypes[i],
                                                  empty,
                                                  format_strings[i]));
                    }
                    schema_ids_by_dtype_name[root_dtype] = static_cast<int>(schema_id);
                }
            }
 
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
                ENTER_COLLECTION
                if (entry_point_) {
                    bit_bucket_->reset();
                    collect_(val);
                    auto bytes = static_cast<CollectableBitBucket*>(bit_bucket_)->release();
                    entry_point_->setScalarValueBytes(bytes);
                } else {
                    collect_(val);
                }
            }

            //! Explicitly/manually collect a value for this collectable, ignoring
            //! what the Collectable is currently pointing to.
            //! Here we pass the shared pointer to the actual object of the collectable type we are collecting.
            template <typename T>
            MetaStruct::enable_if_t<MetaStruct::is_any_pointer<T>::value, void>
            collect(const T & val){
                // If pointer has become nullified, close the record
                if(nullptr == val) {
                    closeRecord();
                    return;
                }
                collect(*val);
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
                // If pointer has become nullified, close the record
                if(nullptr == val) {
                    closeRecord();
                    return;
                }
                collectWithDuration(*val, duration);
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
                // If pointer has become nullified, close the record
                if(nullptr == collected_object_) {
                    closeRecord();
                    return;
                }
                collectWithDuration(*collected_object_, duration);
            }

            //! Force close a record.  This will close the record
            //! immediately and clear the field for the next cycle
            void closeRecord(const bool & = false) override final
            {
                if(SPARTA_EXPECT_FALSE(isCollected()))
                {
                    entry_point_->quiet();
                }
            }

            //! \brief Do not perform any automatic collection
            //! The SchedulingPhase is ignored
            void setManualCollection() {
                auto_collect_ = false;
            }

            //! \brief Strictly a Debug/Testing API.
            //!  Never to be called in real modeler's code.
            std::string dumpNameValuePairs(const DataT & val) {
                collect_(val);
                std::ostringstream ss;
                for(const auto & pairs : getPEventLogVector()){
                    ss << pairs.first << "(" << pairs.second << ") ";
                }
                return ss.str();
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

            //! collection is enabled on the TreeNode
            void setCollecting_(bool collect, Collector * collector) override final
            {
                pipeline_col_ = dynamic_cast<PipelineCollector *>(collector);
                sparta_assert(pipeline_col_ != nullptr,
                            "Collectables can only added to PipelineCollectors... for now");

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

            // Ze Collec-tor
            PipelineCollector * pipeline_col_ = nullptr;

            // For those folks that want a value to automatically
            // disappear in the future
            sparta::EventSet event_set_;
            sparta::PayloadEvent<bool, sparta::SchedulingPhase::Trigger> ev_close_record_;

            // Should we auto-collect?
            bool auto_collect_ = true;

            std::unique_ptr<CollectableBitBucket> owned_bit_bucket_;
            BitBucket* bit_bucket_ = nullptr;
        };
    }//namespace collection
}//namespace sparta
