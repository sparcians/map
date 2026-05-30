// <BitBucket.hpp> -*- C++ -*-

#pragma once

#include "sparta/utils/ValidValue.hpp"
#include "simdb/apps/argos/Collectables.hpp"

namespace sparta::collection {

//! \class BitBucket
//! \brief Base class for all collectable bit buckets (scalars, sparse/contig iterables).
//! We use this class so that we can gather data from e.g. Pair objects without requiring
//! that they hold onto their own collected bytes/values. When data values become available,
//! they go directly into the BitBucket and are organized into data structures that SimDB
//! expects (depends on the collected type).
class BitBucket
{
public:
    explicit BitBucket(simdb::argos::ArgosResources* resource_container) :
        argos_resources_(resource_container) {}

    virtual ~BitBucket() = default;
    virtual void clear() = 0;
    virtual void writeField(const void* data, uint32_t bytes, uint32_t field_id) = 0;
    virtual void writeTo(simdb::argos::CollectionEntryPoint* entry_point) = 0;

    template <typename T>
    bool writeField(const T& val, uint32_t field_id) {
        // Write bools as uint8_t
        if constexpr (std::is_same_v<T, bool>) {
            return writeField(val ? uint8_t(1) : uint8_t(0), field_id);
        }

        // Write strings as uint32_t via TinyStrings
        else if constexpr (std::is_same_v<T, std::string> || std::is_same_v<std::decay_t<T>, const char*>) {
            auto& tiny_strings = argos_resources_->getTinyStringsResource();
            return writeField(tiny_strings->getStringID(val), field_id);
        }

        // Write enums as int64_t or uint64_t based on enum signedness
        else if constexpr (std::is_enum_v<T>) {
            auto enum_maps = argos_resources_->getEnumMapResource();
            enum_maps->inspect(val);
            using underlying_t = std::underlying_type_t<T>;
            using int_t = std::conditional_t<std::is_signed_v<underlying_t>, int64_t, uint64_t>;
            auto enum_int = static_cast<int_t>(val);
            return writeField(enum_int, field_id);
        }

        // Write struct/class fields that provide exactly one cast-to-POD operator
        else if constexpr (simdb::type_traits::is_pod_convertible_v<T> && !std::is_enum_v<T> &&
                          (!std::is_trivial_v<T> || !std::is_standard_layout_v<T>)) {
            using converted_t = simdb::type_traits::pod_convertible_t<T>();
            static_assert(std::is_trivial_v<converted_t> && std::is_standard_layout_v<converted_t>);
            auto converted_val = static_cast<converted_t>(val);
            return writeField(converted_val, field_id);
        }

        // Write PODs (or TinyStrings uint32_t ID, or enums by their underlying type,
        // or bools as uint8_t)
        else if (std::is_trivial_v<T> && std::is_standard_layout_v<T>) {
            writeField(&val, sizeof(T), field_id);
            return true;
        }

        // Cannot collect this field. Let the caller decide what to do.
        else {
            return false;
        }
    }

    simdb::argos::ArgosResources* getArgosResources() const {
        return argos_resources_;
    }

private:
    simdb::argos::ArgosResources* argos_resources_ = nullptr;
};

//! BitBucket implementation for Collectable objects (whether "standalone"
//! or inside an IterableCollector).
class CollectableBitBucket : public BitBucket
{
public:
    using BitBucket::BitBucket;

    void clear() override final {
        buffer_.clear();
        if (!order_locked_) {
            // The first non-empty pass between clear() calls defines the
            // canonical field_id order for the lifetime of this bucket.
            if (!field_order_.empty()) {
                order_locked_ = true;
            }
        } else {
            // All-or-nothing: a pass must write every known field. If the
            // cursor isn't back at the start, a field cycle was incomplete.
            sparta_assert(expected_pos_ == 0,
                          "BitBucket cleared mid-pass: a field cycle was incomplete "
                          "(all-or-nothing violated)");
            expected_pos_ = 0;
        }
    }

    void writeField(const void* data, uint32_t bytes, uint32_t field_id) override final {
        auto src = static_cast<const char*>(data);

        if (SPARTA_EXPECT_FALSE(!order_locked_)) {
            // Learning phase: record arrival order verbatim.
            field_order_.push_back(field_id);
        } else {
            // Verification phase (hot path): order must match exactly.
            sparta_assert(expected_pos_ < field_order_.size() &&
                          field_id == field_order_[expected_pos_],
                          "BitBucket field order changed: expected field_id "
                          << field_order_[expected_pos_] << " but got " << field_id);
            if (++expected_pos_ == field_order_.size()) {
                expected_pos_ = 0;
            }
        }

        buffer_.insert(buffer_.end(), src, src + bytes);
    }

    //! Called when using a standalone Collectable
    void writeTo(simdb::argos::CollectionEntryPoint* entry_point) override final {
        entry_point->setScalarValueBytes(buffer_);
        clear();
    }

    //! Called when inside an IterableCollector
    void writeTo(std::vector<char> & dest) {
        std::swap(dest, buffer_);
        clear();
    }

private:
    std::vector<char> buffer_;
    std::vector<uint32_t> field_order_;
    bool order_locked_ = false;
    size_t expected_pos_ = 0;
};

template <bool Sparse>
class IterableCollectorBitBucket;

template <>
class IterableCollectorBitBucket<true> : public BitBucket
{
public:
    using BitBucket::BitBucket;

    void clear() override final {
        bin_buckets_.clear();
        active_bin_idx_.clearValid();
    }

    void setActiveBinIdx(uint32_t bin_idx) {
        sparta_assert(bin_idx <= UINT16_MAX);
        active_bin_idx_ = static_cast<uint16_t>(bin_idx);
    }

    void writeField(const void* data, uint32_t bytes, uint32_t field_id) override final {
        auto& bin_bucket = bin_buckets_[active_bin_idx_.getValue()];
        if (!bin_bucket) {
            bin_bucket = std::make_unique<CollectableBitBucket>(getArgosResources());
        }
        bin_bucket->writeField(data, bytes, field_id);
    }

    void writeTo(simdb::argos::CollectionEntryPoint* entry_point) override final {
        std::map<uint16_t, std::vector<char>> all_bin_bytes;
        for (auto & [bin_idx, bin_bucket] : bin_buckets_) {
            bin_bucket->writeTo(all_bin_bytes[bin_idx]);
        }

        entry_point->setSparseContainerBinBytes(all_bin_bytes);
        clear();
    }

private:
    std::map<uint16_t, std::unique_ptr<CollectableBitBucket>> bin_buckets_;
    utils::ValidValue<uint16_t> active_bin_idx_;
};

template <>
class IterableCollectorBitBucket<false> : public BitBucket
{
public:
    using BitBucket::BitBucket;

    void clear() override final {
        bin_buckets_.clear();
    }

    void setActiveBinIdx(uint32_t bin_idx) {
        sparta_assert(bin_idx <= UINT16_MAX);
        sparta_assert(bin_idx == bin_buckets_.size());
        bin_buckets_.emplace_back(std::make_unique<CollectableBitBucket>(getArgosResources()));
    }

    void writeField(const void* data, uint32_t bytes, uint32_t field_id) override final {
        if (bin_buckets_.empty()) {
            setActiveBinIdx(0);
        }
        auto& bin_bucket = bin_buckets_.back();
        bin_bucket->writeField(data, bytes, field_id);
    }

    void writeTo(simdb::argos::CollectionEntryPoint* entry_point) override final {
        std::vector<std::vector<char>> all_bin_bytes(bin_buckets_.size());
        for (size_t i = 0; i < bin_buckets_.size(); ++i) {
            auto & bin_bucket = bin_buckets_[i];
            auto & bin_buffer = all_bin_bytes[i];
            bin_bucket->writeTo(bin_buffer);
        }

        entry_point->setContigContainerBinBytes(all_bin_bytes);
        clear();
    }

private:
    std::vector<std::unique_ptr<CollectableBitBucket>> bin_buckets_;
};

} // sparta::collection
