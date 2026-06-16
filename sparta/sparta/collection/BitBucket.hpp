// <BitBucket.hpp> -*- C++ -*-

#pragma once

#include "sparta/utils/ValidValue.hpp"
#include "simdb/apps/argos/Collectables.hpp"
#include <cstring>

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
        buffer_.reserve(bytes_per_pass_);
        bytes_per_pass_ = 0;
    }

    void writeField(const void* data, uint32_t bytes, uint32_t) override final {
        auto src = static_cast<const char*>(data);
        buffer_.resize(bytes_per_pass_ + bytes);
        auto dst = &buffer_[bytes_per_pass_];
        memcpy(dst, src, bytes);
        bytes_per_pass_ += bytes;
    }

    //! Called when using a standalone Collectable
    void writeTo(simdb::argos::CollectionEntryPoint* entry_point) override final {
        entry_point->setScalarValueBytes(std::move(buffer_));
        clear();
    }

    //! Called when inside an IterableCollector
    void writeTo(std::vector<char> & dest) {
        std::swap(dest, buffer_);
        clear();
    }

private:
    std::vector<char> buffer_;
    size_t bytes_per_pass_ = 0;
};

template <bool Sparse>
class IterableCollectorBitBucket;

template <>
class IterableCollectorBitBucket<true> : public BitBucket
{
public:
    IterableCollectorBitBucket(simdb::argos::ArgosResources* resource_container, size_t capacity)
        : BitBucket(resource_container)
        , capacity_(capacity)
    {
        while (capacity--)
        {
            bin_buckets_.emplace_back(std::make_unique<CollectableBitBucket>(getArgosResources()));
        }
    }

    void clear() override final {
        active_bin_idx_.clearValid();
        all_bin_idxs_.clear();
        all_bin_idxs_.reserve(capacity_);
    }

    void setActiveBinIdx(uint32_t bin_idx) {
        assert(bin_idx <= UINT16_MAX);
        assert(bin_idx < capacity_);
        active_bin_idx_ = static_cast<uint16_t>(bin_idx);
        all_bin_idxs_.emplace_back(bin_idx);
    }

    void writeField(const void* data, uint32_t bytes, uint32_t field_id) override final {
        auto& bin_bucket = bin_buckets_[active_bin_idx_.getValue()];
        assert(bin_bucket);
        bin_bucket->writeField(data, bytes, field_id);
    }

    void writeTo(simdb::argos::CollectionEntryPoint* entry_point) override final {
        for (auto bin_idx : all_bin_idxs_) {
            bin_buckets_[bin_idx]->writeTo(all_bin_bytes_[bin_idx]);
        }

        entry_point->setSparseContainerBinBytes(std::move(all_bin_bytes_));
        clear();
    }

private:
    std::vector<std::unique_ptr<CollectableBitBucket>> bin_buckets_;
    std::map<uint16_t, std::vector<char>> all_bin_bytes_;
    utils::ValidValue<uint16_t> active_bin_idx_;
    std::vector<uint16_t> all_bin_idxs_;
    size_t capacity_ = 0;
};

template <>
class IterableCollectorBitBucket<false> : public BitBucket
{
public:
    IterableCollectorBitBucket(simdb::argos::ArgosResources* resource_container, size_t capacity)
        : BitBucket(resource_container)
        , capacity_(capacity)
    {
        all_bin_bytes_.reserve(capacity_);
        while (capacity--)
        {
            bin_buckets_.emplace_back(std::make_unique<CollectableBitBucket>(getArgosResources()));
        }
    }

    void clear() override final {
        container_size_ = 0;
        all_bin_bytes_.reserve(capacity_);
    }

    void setActiveBinIdx(uint32_t bin_idx) {
        assert(bin_idx <= UINT16_MAX);
        assert(bin_idx == container_size_);
        ++container_size_;
    }

    void writeField(const void* data, uint32_t bytes, uint32_t field_id) override final {
        if (SPARTA_EXPECT_FALSE(container_size_ == 0)) {
            setActiveBinIdx(0);
        }
        auto& bin_bucket = bin_buckets_[container_size_ - 1];
        bin_bucket->writeField(data, bytes, field_id);
    }

    void writeTo(simdb::argos::CollectionEntryPoint* entry_point) override final {
        all_bin_bytes_.resize(container_size_);
        for (size_t i = 0; i < container_size_; ++i) {
            auto & bin_bucket = bin_buckets_[i];
            auto & bin_buffer = all_bin_bytes_[i];
            bin_bucket->writeTo(bin_buffer);
        }

        entry_point->setContigContainerBinBytes(std::move(all_bin_bytes_));
        clear();
    }

private:
    std::vector<std::unique_ptr<CollectableBitBucket>> bin_buckets_;
    std::vector<std::vector<char>> all_bin_bytes_;
    uint16_t container_size_ = 0;
    size_t capacity_ = 0;
};

} // sparta::collection
