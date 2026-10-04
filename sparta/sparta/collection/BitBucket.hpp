// <BitBucket.hpp> -*- C++ -*-

#pragma once

#include "sparta/utils/ValidValue.hpp"
#include "sparta/utils/Utils.hpp"
#include "simdb/apps/argos/EntryPoint.hpp"
#include "simdb/Exceptions.hpp"
#include <cstring>
#include <vector>

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
    BitBucket(simdb::TinyStrings<>* tiny_strings, simdb::argos::EnumInspector* enum_inspector) :
        tiny_strings_(tiny_strings),
        enum_inspector_(enum_inspector)
    {}

    virtual ~BitBucket() = default;
    virtual void clear() = 0;
    virtual void writeTo(simdb::argos::EntryPoint* entry_point) = 0;

    template <typename T>
    void writeField(const T& val, uint32_t field_id) {
        // Write bools as uint8_t
        if constexpr (std::is_same_v<T, bool>) {
            writeField(val ? uint8_t(1) : uint8_t(0), field_id);
        }

        // Write list-of-integer values as [count, elem0, elem1, ...].
        else if constexpr (sparta::is_vector<T>::value) {
            using value_type = typename T::value_type;
            sparta_assert(val.size() <= 32u);
            const uint8_t count = static_cast<uint8_t>(val.size());
            writeField_(static_cast<const void*>(&count), sizeof(count), field_id);
            if constexpr (std::is_integral_v<value_type> && !std::is_same_v<value_type, bool>) {
                for (const auto& elem : val) {
                    writeField(elem, field_id);
                }
            } else {
                using converted_t = simdb::type_traits::pod_convertible_t<value_type>;
                static_assert(simdb::type_traits::is_pod_convertible_v<value_type> &&
                              std::is_integral_v<converted_t> && !std::is_same_v<converted_t, bool>,
                              "Argos vector collection only supports integer vectors or integer-like values.");
                for (const auto& elem : val) {
                    writeField(static_cast<converted_t>(elem), field_id);
                }
            }
        }

        // Write strings as uint32_t via TinyStrings
        else if constexpr (std::is_same_v<T, std::string> || std::is_same_v<std::decay_t<T>, const char*>) {
            writeField(tiny_strings_->getStringID(val), field_id);
        }

        // Write enums as their underlying integer type.
        else if constexpr (std::is_enum_v<T>) {
            enum_inspector_->inspect(val);
            using underlying_t = std::underlying_type_t<T>;
            const underlying_t enum_int = static_cast<underlying_t>(val);
            writeField(enum_int, field_id);
        }

        // Write struct/class fields that provide exactly one cast-to-POD operator
        else if constexpr (simdb::type_traits::is_pod_convertible_v<T> && !std::is_enum_v<T> &&
                          (!std::is_trivial_v<T> || !std::is_standard_layout_v<T>)) {
            using converted_t = simdb::type_traits::pod_convertible_t<T>;
            static_assert(std::is_trivial_v<converted_t> && std::is_standard_layout_v<converted_t>);
            auto converted_val = static_cast<converted_t>(val);
            writeField(converted_val, field_id);
        }

        // Write PODs (or TinyStrings uint32_t ID, or enums by their underlying type,
        // or bools as uint8_t)
        else if constexpr (std::is_trivial_v<T> && std::is_standard_layout_v<T>) {
            writeField_(&val, sizeof(T), field_id);
        }

        // Invalid! We might not be able to get away with a static_assert here.
        // The PEvent system collects std::pair's which work for PEvents, but
        // are not valid for Argos. The switch is based on the presence of a
        // BitBucket (Argos) or not (PEvents) and not something we can switch
        // on with a constexpr. See the code in SpartaKeyPairs.hpp:
        //
        // bool finalizeCollection_(
        //     PairCache *& cache, const ValueType & tmp) {
        //
        //     if(auto bit_bucket = this->getBitBucket_(false)) {
        //         *** ARGOS ***
        //         bit_bucket->writeField(tmp, id_);
        //     } else {
        //         *** PEVENTS ***
        //         ...
        //     }
        //     return false;
        // }
        else {
            throw simdb::DBException("Invalid type! Must be a POD, enum, or string, not ")
                << simdb::demangle_type<T>();
        }
    }

    simdb::TinyStrings<>* getTinyStrings() const {
        return tiny_strings_;
    }

    simdb::argos::EnumInspector* getEnumInspector() const {
        return enum_inspector_;
    }

protected:
    virtual void writeField_(const void* data, uint32_t bytes, uint32_t field_id) = 0;

private:
    simdb::TinyStrings<>* tiny_strings_ = nullptr;
    simdb::argos::EnumInspector* enum_inspector_ = nullptr;
};

template <bool Sparse>
class IterableCollectorBitBucket;

//! \class CollectableBitBucket
//! \brief BitBucket implementation for Collectable objects (whether "standalone"
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

    //! Called when using a standalone Collectable
    void writeTo(simdb::argos::EntryPoint* entry_point) override final {
        entry_point->setScalarValueBytes(std::move(buffer_));
        clear();
    }

    //! Called when inside an IterableCollector
    void writeTo(std::vector<char> & dest) {
        std::swap(dest, buffer_);
        clear();
    }

private:
    void writeField_(const void* data, uint32_t bytes, uint32_t) override final {
        auto src = static_cast<const char*>(data);
        buffer_.resize(bytes_per_pass_ + bytes);
        auto dst = &buffer_[bytes_per_pass_];
        memcpy(dst, src, bytes);
        bytes_per_pass_ += bytes;
    }

    std::vector<char> buffer_;
    size_t bytes_per_pass_ = 0;

    template <bool Sparse>
    friend class IterableCollectorBitBucket;
};

//! \class IterableCollectorBitBucket
//! \brief BitBucket implementation for sparse IterableCollectors
template <>
class IterableCollectorBitBucket<true> : public BitBucket
{
public:
    IterableCollectorBitBucket(simdb::TinyStrings<>* tiny_strings, simdb::argos::EnumInspector* enum_inspector, size_t capacity)
        : BitBucket(tiny_strings, enum_inspector)
        , capacity_(capacity)
    {
        sparta_assert(capacity_ <= UINT16_MAX);
        all_bin_idxs_.reserve(capacity_);
        while (capacity--)
        {
            bin_buckets_.emplace_back(std::make_unique<CollectableBitBucket>(tiny_strings, enum_inspector));
        }
    }

    void clear() override final {
        active_bin_idx_.clearValid();
        all_bin_idxs_.clear();
        all_bin_idxs_.reserve(capacity_);
    }

    void setActiveBinIdx(uint32_t bin_idx) {
        sparta_assert(bin_idx < capacity_);
        active_bin_idx_ = static_cast<uint16_t>(bin_idx);
        all_bin_idxs_.emplace_back(bin_idx);
    }

    void writeTo(simdb::argos::EntryPoint* entry_point) override final {
        for (auto bin_idx : all_bin_idxs_) {
            bin_buckets_.at(bin_idx)->writeTo(all_bin_bytes_[bin_idx]);
        }

        entry_point->setSparseContainerBinBytes(std::move(all_bin_bytes_));
        clear();
    }

private:
    void writeField_(const void* data, uint32_t bytes, uint32_t field_id) override final {
        auto& bin_bucket = bin_buckets_.at(active_bin_idx_.getValue());
        assert(bin_bucket);
        bin_bucket->writeField_(data, bytes, field_id);
    }

    std::vector<std::unique_ptr<CollectableBitBucket>> bin_buckets_;
    std::map<uint16_t, std::vector<char>> all_bin_bytes_;
    utils::ValidValue<uint16_t> active_bin_idx_;
    std::vector<uint16_t> all_bin_idxs_;
    size_t capacity_ = 0;
};

//! \class IterableCollectorBitBucket
//! \brief BitBucket implementation for contiguous IterableCollectors
template <>
class IterableCollectorBitBucket<false> : public BitBucket
{
public:
    IterableCollectorBitBucket(simdb::TinyStrings<>* tiny_strings, simdb::argos::EnumInspector* enum_inspector, size_t capacity)
        : BitBucket(tiny_strings, enum_inspector)
        , capacity_(capacity)
    {
        all_bin_bytes_.reserve(capacity_);
        while (capacity--)
        {
            bin_buckets_.emplace_back(std::make_unique<CollectableBitBucket>(tiny_strings, enum_inspector));
        }
    }

    void clear() override final {
        container_size_ = 0;
        all_bin_bytes_.reserve(capacity_);
    }

    void setActiveBinIdx(uint32_t bin_idx) {
        sparta_assert(bin_idx <= UINT16_MAX);
        sparta_assert(bin_idx == container_size_);
        ++container_size_;
    }

    void writeTo(simdb::argos::EntryPoint* entry_point) override final {
        all_bin_bytes_.resize(container_size_);
        for (size_t i = 0; i < container_size_; ++i) {
            auto & bin_bucket = bin_buckets_.at(i);
            auto & bin_buffer = all_bin_bytes_.at(i);
            bin_bucket->writeTo(bin_buffer);
        }

        entry_point->setContigContainerBinBytes(std::move(all_bin_bytes_));
        clear();
    }

private:
    void writeField_(const void* data, uint32_t bytes, uint32_t field_id) override final {
        if (SPARTA_EXPECT_FALSE(container_size_ == 0)) {
            setActiveBinIdx(0);
        }
        auto& bin_bucket = bin_buckets_.at(container_size_ - 1);
        bin_bucket->writeField_(data, bytes, field_id);
    }

    std::vector<std::unique_ptr<CollectableBitBucket>> bin_buckets_;
    std::vector<std::vector<char>> all_bin_bytes_;
    uint16_t container_size_ = 0;
    size_t capacity_ = 0;
};

} // namespace sparta::collection
