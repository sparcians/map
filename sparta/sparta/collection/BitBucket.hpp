// <BitBucket.hpp> -*- C++ -*-

#pragma once

#include "sparta/utils/SpartaAssert.hpp"
#include "simdb/apps/argos/Collectables.hpp"

namespace sparta::collection {

class BitBucket
{
public:
    explicit BitBucket(simdb::argos::ArgosResources* resource_container) : argos_resources_(resource_container) {}
    virtual ~BitBucket() = default;

    virtual void clear() = 0;
    virtual void writeField(const void* data, uint32_t bytes, uint32_t bin_idx, uint32_t field_id) = 0;
    virtual void writeTo(simdb::argos::CollectionEntryPoint* entry_point) = 0;

    template <typename T>
    void writeField(const T& val, uint32_t bin_idx, uint32_t field_id) {
        if constexpr (std::is_same_v<T, bool>) {
            writeField(val ? uint8_t(1) : uint8_t(0), bin_idx, field_id);
        } else if constexpr (std::is_same_v<T, std::string> || std::is_same_v<std::decay_t<T>, const char*>) {
            auto& tiny_strings = argos_resources_->getTinyStringsResource();
            writeField(tiny_strings->getStringID(val), bin_idx, field_id);
        } else if constexpr (std::is_enum_v<T>) {
            auto enum_maps = argos_resources_->getEnumMapResource();
            enum_maps->inspect(val);
            writeField((std::underlying_type<T>)val, bin_idx, field_id);
        } else {
            writeField(&val, sizeof(T), bin_idx, field_id);
        }
    }

    simdb::argos::ArgosResources* getArgosResources() const {
        return argos_resources_;
    }

private:
    simdb::argos::ArgosResources* argos_resources_ = nullptr;
};

class CollectableBitBucket : public BitBucket
{
public:
    using BitBucket::BitBucket;

    void clear() override final {
        buffer_.clear();
        num_fields_written_ = 0;
    }

    void writeField(const void* data, uint32_t bytes, uint32_t bin_idx, uint32_t field_id) override final {
        sparta_assert(bin_idx == 0);
        sparta_assert(field_id == num_fields_written_);
        auto src = static_cast<const char*>(data);
        buffer_.insert(buffer_.end(), src, src + bytes);
        ++num_fields_written_;
    }

    void writeTo(simdb::argos::CollectionEntryPoint* entry_point) override final {
        entry_point->setScalarValueBytes(buffer_);
        clear();
    }

    void writeTo(std::vector<char> & dest) {
        std::swap(dest, buffer_);
        clear();
    }

    uint32_t numFieldsWritten() const {
        return num_fields_written_;
    }

private:
    std::vector<char> buffer_;
    uint32_t num_fields_written_ = 0;
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
    }

    void writeField(const void* data, uint32_t bytes, uint32_t bin_idx, uint32_t field_id) override final {
        sparta_assert(bin_idx <= UINT16_MAX);
        auto& bin_bucket = bin_buckets_[bin_idx];
        if (!bin_bucket) {
            bin_bucket = std::make_unique<CollectableBitBucket>(getArgosResources());
        }

        constexpr uint32_t dummy_bin_idx = 0;
        bin_bucket->writeField(data, bytes, dummy_bin_idx, field_id);
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
};

template <>
class IterableCollectorBitBucket<false> : public BitBucket
{
public:
    using BitBucket::BitBucket;

    void clear() override final {
        bin_buckets_.clear();
    }

    void writeField(const void* data, uint32_t bytes, uint32_t bin_idx, uint32_t field_id) override final {
        sparta_assert(bin_idx <= UINT16_MAX);
        if (bin_idx == bin_buckets_.size()) {
            bin_buckets_.emplace_back(std::make_unique<CollectableBitBucket>(getArgosResources()));
        }

        sparta_assert(bin_idx == bin_buckets_.size() - 1);
        auto& bin_bucket = bin_buckets_.back();
        constexpr uint32_t dummy_bin_idx = 0;
        bin_bucket->writeField(data, bytes, dummy_bin_idx, field_id);
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
