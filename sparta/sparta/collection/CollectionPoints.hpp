// <CollectionPoints.hpp> -*- C++ -*-

#pragma once

#include "simdb/collection/Scalars.hpp"
#include "simdb/collection/Structs.hpp"
#include "simdb/collection/IterableStructs.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/utils/MetaStructs.hpp"

#include <type_traits>
#include <cxxabi.h>  // For __cxa_demangle
#include <fstream>

namespace sparta
{
    inline std::string demangle(const char* mangled_name) {
        int status = 0;
        char* demangled_name = abi::__cxa_demangle(mangled_name, nullptr, nullptr, &status);
        if (status == 0 && demangled_name) {
            std::string result(demangled_name);
            std::free(demangled_name);
            return result;
        }
        return mangled_name;
    }

    template <typename T>
    inline std::string demangled_type() {
        return demangle(typeid(T).name());
    }
}

namespace sparta {
namespace collection {

class CollectionPoints
{
public:
    template <typename StatT>
    typename std::enable_if<std::is_trivial<StatT>::value && std::is_standard_layout<StatT>::value, void>::type
    addStat(const std::string& location, const Clock* clk, const StatT* stat)
    {
        auto demangled = demangled_type<StatT>();
        auto& instantiator = instantiators_[demangled];
        if (!instantiator) {
            instantiator.reset(new StatInstantiator<StatT>());
        }

        dynamic_cast<StatInstantiator<StatT>*>(instantiator.get())->addStat(location, clk, stat);
    }

    template <typename StatT>
    typename std::enable_if<!std::is_trivial<StatT>::value || !std::is_standard_layout<StatT>::value, void>::type
    addStat(const std::string& location, const Clock* clk, const StatT* stat)
    {
        (void)location;
        (void)clk;
        (void)stat;
    }

    template <typename StatT>
    typename std::enable_if<std::is_pointer<StatT>::value, void>::type
    addStat(const std::string& location, const Clock* clk, const StatT stat)
    {
        (void)location;
        (void)clk;
        (void)stat;
    }

    template <typename ContainerT, bool Sparse>
    typename std::enable_if<MetaStruct::is_any_pointer<typename ContainerT::value_type>::value, void>::type
    addContainer(const std::string& location, const Clock* clk, const ContainerT* container, const size_t capacity)
    {
        auto demangled = demangled_type<ContainerT>() + (Sparse ? "Sparse" : "Contig");
        auto& instantiator = instantiators_[demangled];
        if (!instantiator) {
            instantiator.reset(new IterStructInstantiator<ContainerT, Sparse>());
        }

        dynamic_cast<IterStructInstantiator<ContainerT, Sparse>*>(instantiator.get())->addContainer(location, clk, container, capacity);
    }

    template <typename ContainerT, bool Sparse>
    typename std::enable_if<!MetaStruct::is_any_pointer<typename ContainerT::value_type>::value, void>::type
    addContainer(const std::string& location, const Clock* clk, const ContainerT* container, const size_t capacity)
    {
        (void)location;
        (void)clk;
        (void)container;
        (void)capacity;
    }

    void createCollections(simdb::Collections* collections)
    {
        std::unordered_map<std::string, uint32_t> clk_periods;
        for (const auto& kvp : instantiators_) {
            kvp.second->getClockPeriods(clk_periods);
        }

        std::unordered_map<std::string, std::string> clk_names_by_location;
        for (const auto& kvp : instantiators_) {
            kvp.second->getClockNamesByLocation(clk_names_by_location);
        }

        for (const auto& kvp : clk_periods) {
            collections->addClock(kvp.first, kvp.second);
        }

        for (const auto& kvp : clk_names_by_location) {
            collections->setClock(kvp.first, kvp.second);
        }

        size_t idx = 0;
        for (auto& kvp : instantiators_) {
            const auto collection_prefix = "Collection" + std::to_string(idx);
            kvp.second->createCollections(collections, collection_prefix);
            ++idx;
        }

        instantiators_.clear();
    }

private:
    class CollectableInstantiator
    {
    public:
        virtual ~CollectableInstantiator() = default;
        virtual void getClockPeriods(std::unordered_map<std::string, uint32_t>& clk_periods) const = 0;
        virtual void getClockNamesByLocation(std::unordered_map<std::string, std::string>& clk_names_by_location) const = 0;
        virtual void createCollections(simdb::Collections* collections, const std::string& collection_prefix) = 0;
    };

    template <typename StatT>
    class StatInstantiator : public CollectableInstantiator
    {
    public:
        void addStat(const std::string& location, const Clock* clk, const StatT* stat)
        {
            stats_.emplace_back(location, clk, stat);
        }

        void getClockPeriods(std::unordered_map<std::string, uint32_t>& clk_periods) const override
        {
            for (const auto& tup : stats_) {
                const auto clk = std::get<1>(tup);
                clk_periods[clk->getName()] = clk->getPeriod();
            }
        }

        void getClockNamesByLocation(std::unordered_map<std::string, std::string>& clk_names_by_location) const override
        {
            for (const auto& tup : stats_) {
                const auto location = std::get<0>(tup);
                const auto clk = std::get<1>(tup);
                clk_names_by_location[location] = clk->getName();
            }
        }

        void createCollections(simdb::Collections* collections, const std::string& collection_prefix) override
        {
            using CollectionT = simdb::StatCollection<StatT>;
            const auto collection_name = collection_prefix + "_" + demangled_type<StatT>();
            std::unique_ptr<CollectionT> collection(new CollectionT(collection_name));

            for (const auto& tup : stats_) {
                const auto& location = std::get<0>(tup);
                auto stat = std::get<2>(tup);
                collection->addStat(location, stat);
            }

            collections->addCollection(std::move(collection));
        }

    private:
        std::vector<std::tuple<std::string, const Clock*, const StatT*>> stats_;
    };

    template <typename ContainerT, bool Sparse>
    class IterStructInstantiator : public CollectableInstantiator
    {
    public:
        void addContainer(const std::string& location, const Clock* clk, const ContainerT* obj, const size_t capacity)
        {
            containers_.emplace_back(location, clk, obj, capacity);
        }

        void getClockPeriods(std::unordered_map<std::string, uint32_t>& clk_periods) const override
        {
            for (const auto& tup : containers_) {
                const auto clk = std::get<1>(tup);
                clk_periods[clk->getName()] = clk->getPeriod();
            }
        }

        void getClockNamesByLocation(std::unordered_map<std::string, std::string>& clk_names_by_location) const override
        {
            for (const auto& tup : containers_) {
                const auto location = std::get<0>(tup);
                const auto clk = std::get<1>(tup);
                clk_names_by_location[location] = clk->getName();
            }
        }

        void createCollections(simdb::Collections* collections, const std::string& collection_prefix) override
        {
            using CollectionT = simdb::IterableStructCollection<ContainerT, Sparse>;

            for (size_t idx = 0; idx < containers_.size(); ++idx) {
                const std::string &location = std::get<0>(containers_[idx]);
                const ContainerT *obj = std::get<2>(containers_[idx]);
                const size_t capacity = std::get<3>(containers_[idx]);

                const auto collection_name = collection_prefix + "_" + demangled_type<ContainerT>() + "_" + std::to_string(idx);
                std::unique_ptr<CollectionT> collection(new CollectionT(collection_name));

                collection->addContainer(location, obj, capacity);
                collections->addCollection(std::move(collection));
            }
        }

    private:
        std::vector<std::tuple<std::string, const Clock*, const ContainerT*, size_t>> containers_;
    };

    std::unordered_map<std::string, std::unique_ptr<CollectableInstantiator>> instantiators_;
};

} // namespace collection
} // namespace sparta
