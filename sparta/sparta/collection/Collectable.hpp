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
//#include "sparta/collection/DynamicDataType.hpp" TODO XXX
#include "sparta/collection/BitBucket.hpp"
#include "sparta/pipeViewer/transaction_structures.hpp"
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

        template <typename T, typename = void>
        struct use_raw_type : std::false_type {};

        template <typename T>
        struct use_raw_type<T,
            std::enable_if_t<std::is_trivial_v<T> &&
                             std::is_standard_layout_v<T>>> : std::true_type {};

        template <typename T>
        inline constexpr bool use_raw_type_v = use_raw_type<T>::value;

        template <typename T, typename = void>
        struct use_tiny_strings : std::false_type {};

        template <typename T>
        struct use_tiny_strings<T,
            std::enable_if_t<std::is_same_v<T, std::string> ||
                             std::is_same_v<std::decay_t<T>, const char*>>> : std::true_type {};

        template <typename T>
        inline constexpr bool use_tiny_strings_v = use_tiny_strings<T>::value;

        template <typename T, typename = void>
        struct use_pair_definition : std::false_type {};

        template <typename T>
        struct use_pair_definition<T,
            std::enable_if_t<std::is_base_of<sparta::PairDefinition<T>,
                             typename T::SpartaPairDefinitionType>::value>> : std::true_type {};

        template <typename T>
        inline constexpr bool use_pair_definition_v = use_pair_definition<T>::value;

        template <typename T, typename = void>
        struct use_cast_operator : std::false_type {};

        template <typename T>
        struct use_cast_operator<T,
            std::enable_if_t<simdb::type_traits::is_pod_convertible_v<T> &&
                             !use_raw_type_v<T> &&
                             !use_pair_definition_v<T> &&
                             !utils::has_ostream_operator<T>::value>> : std::true_type {};

        template <typename T>
        inline constexpr bool use_cast_operator_v = use_cast_operator<T>::value;

        template <typename T, typename = void>
        struct use_dynamic_fields : std::false_type {};

        template <typename T>
        struct use_dynamic_fields<T,
            std::enable_if_t<!use_raw_type_v<T> &&
                             !use_tiny_strings_v<T> &&
                             !use_pair_definition_v<T> &&
                             !use_cast_operator_v<T> &&
                             utils::has_ostream_operator<T>::value>> : std::true_type {};

        template <typename T>
        inline constexpr bool use_dynamic_fields_v = use_dynamic_fields<T>::value;

        //! Common code for all Collectable implementations below.
        template <typename DataT, SchedulingPhase collection_phase = SchedulingPhase::Collection>
        class CollectableCommon : public CollectableTreeNode
        {
        public:
            using ValueType = MetaStruct::remove_any_pointer_t<DataT>;

            /**
             * \brief Construct the Collectable, no data object associated, part of a group
             * \param parent A pointer to a parent treenode.  Must not be null
             * \param name The name for which to create this object as a child sparta::TreeNode
             * \param parentid The transaction id of a parent for this collectable; 0 for no parent
             * \param desc A description for the interface
             */
            CollectableCommon(sparta::TreeNode* parent,
                              const std::string& name,
                              const std::string& group,
                              uint32_t index,
                              uint64_t parentid = 0,
                              const std::string & desc = "Collectable <manual, no desc>") :
                CollectableTreeNode(sparta::notNull(parent), name, group, index, desc),
                event_set_(this),
                ev_close_record_(&event_set_, name + "_pipeline_collectable_close_event",
                                CREATE_SPARTA_HANDLER_WITH_DATA(CollectableCommon, closeRecord, bool))
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
            CollectableCommon(sparta::TreeNode* parent,
                              const std::string& name,
                              const DataT * collected_object,
                              uint64_t parentid = 0,
                              const std::string & desc = "Collectable <no desc>") :
                CollectableCommon(parent, name,
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
            CollectableCommon(sparta::TreeNode* parent,
                              const std::string& name,
                              uint64_t parentid = 0,
                              const std::string & desc = "Collectable <manual, no desc>") :
                CollectableCommon(parent, name, nullptr, parentid, desc)
            {
                // Can't auto collect without setting collected_object_
                setManualCollection();
            }

            //! Virtual destructor -- does nothing
            virtual ~CollectableCommon() {}

            /**
             * \brief For manual collection, provide an initial value
             * \param val The value to initial the record with
             */
            void initialize(const ValueType & val) {
                //TODO XXX
                (void)val;
            }

            template <typename T>
            std::enable_if_t<MetaStruct::is_any_pointer_v<T>, void>
            initialize(const T & val) {
                if (val) {
                    initialize(*val);
                }
            }

            //! Explicitly/manually collect a value for this collectable, ignoring
            //! what the Collectable is currently pointing to.
            void collect(const ValueType & val) {
                if(SPARTA_EXPECT_FALSE(isCollected()))
                {
                    performCollection_(val);
                }
            }

            template <typename T>
            std::enable_if_t<MetaStruct::is_any_pointer_v<T>, void>
            collect(const T & val) {
                if (val) {
                    collect(*val);
                } else {
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
            void collectWithDuration(const ValueType & val, sparta::Clock::Cycle duration) {
                if(SPARTA_EXPECT_FALSE(isCollected()))
                {
                    if(duration != 0) {
                        ev_close_record_.preparePayload(false)->schedule(duration);
                    }
                    collect(val);
                }
            }
 
            template <typename T>
            std::enable_if_t<MetaStruct::is_any_pointer_v<T>, void>
            collectWithDuration(const T & val, sparta::Clock::Cycle duration) {
                if (val) {
                    collectWithDuration(*val, duration);
                } else {
                    closeRecord();
                }
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

            //! Force close a record.  This will close the record
            //! immediately and clear the field for the next cycle
            void closeRecord(const bool & = false) override final {
                if (SPARTA_EXPECT_FALSE(isCollected() && entry_point_)) {
                    entry_point_->quiet();
                }
            }

            //! \brief Do not perform any automatic collection
            //! The SchedulingPhase is ignored
            void setManualCollection() {
                auto_collect_ = false;
            }

            //! Collectable classes must be able to register themselves with the ArgosCollector.
            void createSimDbEntryPoint(simdb::argos::ArgosCollector* argos_collector) override final {
                auto loc = getLocation();
                auto clk_name = notNull(getClock())->getName();
                auto type = encodeCollectedType();
                entry_point_ = argos_collector->createScalarCollector(loc, clk_name, type);

                auto bit_bucket = std::make_shared<CollectableBitBucket>(argos_collector->getResources());
                setBitBucket(bit_bucket);
            }

            //! Set the BitBucket (byte buffers for SimDB CollectionEntryPoint)
            virtual void setBitBucket(std::shared_ptr<BitBucket> bit_bucket) {
                bit_bucket_ = bit_bucket;
            }

        protected:

            //! \brief Get a reference to the internal event set
            //! \return Reference to event set -- used by DelayedCollectable
            EventSet & getEventSet_() {
                return event_set_;
            }

            //! Virtual method called by CollectableTreeNode when
            //! collection is enabled on the TreeNode
            void setCollecting_(bool collect, Collector * collector) override final {
                pipeline_col_ = dynamic_cast<PipelineCollector *>(collector);
                sparta_assert(pipeline_col_ != nullptr,
                              "Collectables can only added to PipelineCollectors... for now");

                if(collect && !initial_bytes_.empty()) {
                    //TODO XXX: handle initial value
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

            //! Subclasses are responsible for collecting values and writing the bytes
            virtual void performCollection_(const ValueType & val) = 0;

             //! The annotation object to be collected
            const DataT * collected_object_ = nullptr;

            //! Ze Collec-tor
            PipelineCollector * pipeline_col_ = nullptr;

            //! For those folks that want a value to automatically
            //! disappear in the future
            sparta::EventSet event_set_;
            sparta::PayloadEvent<bool, sparta::SchedulingPhase::Trigger> ev_close_record_;

            //! Should we auto-collect?
            bool auto_collect_ = true;

            //! Extracted value bytes from initialize() to be applied when
            //! collection is first enabled (setCollecting_)
            std::vector<char> initial_bytes_;

            //! Destination for all collected data, whether we are a standalone
            //! Collectable or one of the positions_ in the IterableCollector
            std::shared_ptr<BitBucket> bit_bucket_;
        };

        #define INHERIT_COMMON_INTERFACE                                                      \
            using ValueType = typename CollectableCommon<DataT, collection_phase>::ValueType; \
            using CollectableCommon<DataT, collection_phase>::CollectableCommon;              \
            using CollectableCommon<DataT, collection_phase>::initialize;                     \
            using CollectableCommon<DataT, collection_phase>::collect;                        \
            using CollectableCommon<DataT, collection_phase>::collectWithDuration;            \
            using CollectableCommon<DataT, collection_phase>::closeRecord;                    \
            using CollectableCommon<DataT, collection_phase>::setManualCollection;            \
            using CollectableCommon<DataT, collection_phase>::getEventSet_;                   \
            using CollectableCommon<DataT, collection_phase>::bit_bucket_;                    \
            using CollectableCommon<DataT, collection_phase>::entry_point_;

        //! Use case 0: This first Collectable class is the default no-op / unusable base implementation.
        //! It is really only here to keep all legacy code compiling.
        template<typename DataT, SchedulingPhase collection_phase = SchedulingPhase::Collection, typename = void>
        class Collectable : public CollectableCommon<DataT, collection_phase>
        {
        public:
            INHERIT_COMMON_INTERFACE

        private:
            std::string encodeCollectedType(bool = false) const override final {
                throw_();
                return "";
            }

            void performCollection_(const ValueType &) override final {
                throw_();
            }

            void throw_() const {
                throw SpartaException("Uncollectable type encountered at ") << this->getLocation();
            }
        };

        //! Use case 1: We are collecting a bool, int/float, or an enum. These collected values are written
        //! as their native type.
        template<typename DataT, SchedulingPhase collection_phase>
        class Collectable<DataT, collection_phase, std::enable_if_t<use_raw_type_v<MetaStruct::remove_any_pointer_t<DataT>>>>
            : public CollectableCommon<DataT, collection_phase>
        {
        public:
            INHERIT_COMMON_INTERFACE

            std::string encodeCollectedType(bool human_readable = false) const override final {
                auto type = simdb::demangle_type<ValueType>();
                if constexpr (std::is_enum_v<ValueType>) {
                    if (human_readable) {
                        using Underlying = std::underlying_type_t<ValueType>;
                        type += " (enum: " + simdb::demangle_type<Underlying>() + ")";
                    }
                }
                return type;
            }

        private:
            void performCollection_(const ValueType & val) override final {
                constexpr auto dummy_field_id = 0u;
                bit_bucket_->writeField(val, dummy_field_id);

                if (entry_point_) {
                    static_cast<CollectableBitBucket*>(bit_bucket_.get())->writeTo(entry_point_);
                }
            }
        };

        //! Use case 2: We are collecting a scalar string type (std::string, const char*, or enum with operator<<).
        //! These collected strings are written as uint32_t values after going through TinyStrings.
        template<typename DataT, SchedulingPhase collection_phase>
        class Collectable<DataT, collection_phase, std::enable_if_t<use_tiny_strings_v<MetaStruct::remove_any_pointer_t<DataT>>>>
            : public CollectableCommon<DataT, collection_phase>
        {
        public:
            INHERIT_COMMON_INTERFACE

            std::string encodeCollectedType(bool human_readable = false) const override final {
                auto type = std::string("string");
                if (human_readable && std::is_enum_v<ValueType>) {
                    type += " (enum with ostream operator)";
                }
                return type;
            }

        private:
            void performCollection_(const ValueType & val) override final {
                constexpr auto dummy_field_id = 0u;
                bit_bucket_->writeField(val, dummy_field_id);

                if (entry_point_) {
                    static_cast<CollectableBitBucket*>(bit_bucket_.get())->writeTo(entry_point_);
                }
            }
        };

        //! Use case 3: We are collecting a class/struct which does not provide SpartaPairDefinitionType,
        //! but provides a cast-to-POD operator.
        template<typename DataT, SchedulingPhase collection_phase>
        class Collectable<DataT, collection_phase, std::enable_if_t<use_cast_operator_v<MetaStruct::remove_any_pointer_t<DataT>>>>
            : public CollectableCommon<DataT, collection_phase>
        {
        public:
            INHERIT_COMMON_INTERFACE

            std::string encodeCollectedType(bool human_readable = false) const override final {
                using converted_t = simdb::type_traits::pod_convertible_t<ValueType>();
                auto type = simdb::demangle_type<converted_t>();
                if (human_readable) {
                    type += " (built-in type using cast operator)";
                }
                return type;
            }

        private:
            void performCollection_(const ValueType & val) override final {
                constexpr auto dummy_field_id = 0u;
                using converted_t = simdb::type_traits::pod_convertible_t<ValueType>();
                auto converted_val = static_cast<converted_t>(val);
                bit_bucket_->writeField(converted_val, dummy_field_id);

                if (entry_point_) {
                    static_cast<CollectableBitBucket*>(bit_bucket_.get())->writeTo(entry_point_);
                }
            }
        };
    
        //! Use case 4: We are collecting a class/struct that does not provide SpartaPairDefinitionType,
        //! has no cast-to-POD operator, and only provides operator<<. We will figure out the fields
        //! dynamically (at least field data type, as well as the field name if applicable).
        template<typename DataT, SchedulingPhase collection_phase>
        class Collectable<DataT, collection_phase, std::enable_if_t<use_dynamic_fields_v<MetaStruct::remove_any_pointer_t<DataT>>>>
            : public CollectableCommon<DataT, collection_phase>
        {
        public:
            INHERIT_COMMON_INTERFACE

            std::string encodeCollectedType(bool human_readable = false) const override final {
                auto type = std::string("dynamic");
                if (human_readable) {
                    type += " (" + simdb::demangle_type<ValueType>() + ")";
                }
                return type;
            }

        private:
            void performCollection_(const ValueType &) override final {
                // TODO XXX - fix up DynamicDataType.hpp
                if (warn_ && entry_point_) {
                    std::string warning = "Cannot collect data type '";
                    warning += simdb::demangle_type<ValueType>() + "'. ";
                    warning += "This must be moved to PairDefinition.";
                    entry_point_->postWarning(warning);
                }
                warn_ = false;
            }

            bool warn_ = true;
        };

        //! Use case 5: We are collecting a class/struct which provides SpartaPairDefinitionType.
        template<typename DataT, SchedulingPhase collection_phase>
        class Collectable<DataT, collection_phase, std::enable_if_t<use_pair_definition_v<MetaStruct::remove_any_pointer_t<DataT>>>>
            : public CollectableCommon<DataT, collection_phase>
            , public PairCollector<typename MetaStruct::remove_any_pointer_t<DataT>::SpartaPairDefinitionType>
        {
        public:
            INHERIT_COMMON_INTERFACE

            std::string encodeCollectedType(bool human_readable = false) const override final {
                auto type = simdb::demangle_type<ValueType>();
                if (human_readable) {
                    type += " (using PairDefinition)";
                }
                return type;
            }

            //! Whether a scalar (Collectable) or container (IterableCollector), serialize
            //! struct-like data structure hierarchies (field name + dtype) to the database.
            void serializeStructSchema(
                simdb::DatabaseManager* db_mgr,
                std::map<std::string, int>& schema_ids_by_dtype_name) override final
            {
                auto root_dtype = encodeCollectedType();
                if(schema_ids_by_dtype_name.find(root_dtype) != schema_ids_by_dtype_name.end()) {
                    return;
                }

                const int32_t schema_id =
                    db_mgr->INSERT(SQL_TABLE("DataTypeSchemas"), SQL_VALUES(root_dtype))->getId();

                using PairDef = typename ValueType::SpartaPairDefinitionType;
                PairDef pair_def;
                sparta::PairCache pair_cache;
                pair_def.finalizeKeys(&pair_cache);

                const auto & names = pair_cache.getNameStrings();
                const auto & dtypes = pair_def.getLeafArgosDtypeStrings();
                const auto & formatters = pair_cache.getFormatVector();
                sparta_assert(names.size() == dtypes.size());
                sparta_assert(formatters.size() == names.size());

                std::vector<std::string> format_strings;
                for(size_t i = 0; i < formatters.size(); ++i) {
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

                std::cout << "\nSerializing PairDefinition to database for '" << root_dtype << "'...\n";
                for(size_t i = 0; i < names.size(); ++i) {
                    std::cout << "\t" << names[i] << ", " << dtypes[i];
                    if (!format_strings[i].empty()) {
                        std::cout << " (" << format_strings[i] << ")";
                    }
                    std::cout << "\n";

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
                schema_ids_by_dtype_name[root_dtype] = schema_id;
            }

            void setBitBucket(std::shared_ptr<BitBucket> bit_bucket) override final {
                // Share the BitBucket with the Pairs
                setBitBucket_(bit_bucket);

                // Let the base class own the bit bucket
                CollectableCommon<DataT, collection_phase>::setBitBucket(bit_bucket);
            }

        private:
            typedef typename ValueType::SpartaPairDefinitionType PairDef_t;
            using PairCollector<PairDef_t>::collect_;
            using PairCollector<PairDef_t>::setBitBucket_;

            void performCollection_(const ValueType & val) override final {
                if (!this->isIterableCollectorBin()) {
                    bit_bucket_->clear();
                }
                collect_(val);

                if (entry_point_) {
                    static_cast<CollectableBitBucket*>(bit_bucket_.get())->writeTo(entry_point_);
                }
            }

            void generateCollectionString_() override {}
        };

    }//namespace collection
}//namespace sparta
