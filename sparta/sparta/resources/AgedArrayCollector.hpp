// <ArrayCollector.hpp> -*- C++ -*-

/**
 * \file ArrayCollector.hpp
 * \brief Class used by the sparta::Array class
 */
#ifndef __SPARTA_AGED_ARRAY_COLLECTOR_H__
#define __SPARTA_AGED_ARRAY_COLLECTOR_H__

#include <inttypes.h>

#include "sparta/resources/VectorResourceCollectable.hpp"

namespace sparta
{
namespace collection
{

    /**
     * \class AgedArrayCollector
     * \brief An AgedArrayCollector is created by the Array class in
     *        sparta when Pipeline collection is required.
     *
     * This collector will always present the data of an Aged array
     * starting at index 0.
     */
    template <class ArrayType>
    class AgedArrayCollector : public VectorResourceCollectable<ArrayType>
    {
        using VectorResourceCollectable<ArrayType>::collected_resource_;
        using VectorResourceCollectable<ArrayType>::collectors_;
        using VectorResourceCollectable<ArrayType>::closeRecord;

    public:
        /**
         * \brief Construct an AgedArrayCollector
         * \param parent A pointer to the parent treenode for wich
         *               collectable objects will be created under.
         * \param array The array to be collected
         */
        AgedArrayCollector(sparta::TreeNode* parent, const ArrayType* array) :
            VectorResourceCollectable<ArrayType>(parent, array,
                                                 array->getName() + "_age_ordered",
                                                 array->getName() + " Age-Ordered")
        {}

        /**
         * \brief set up the Collector with the current state of the Array,
         * and begin collection going forward.
         */
        void collect() override final
        {
            // We need to step through the array and validate all valid positions.
            const typename ArrayType::AgedList & aged_list =
                collected_resource_->getInternalAgedList_();
            uint32_t collector_idx = aged_list.size() - 1;
            for(const auto & obj : aged_list)
            {
                collectors_[collector_idx]->collect(collected_resource_->read(obj));
                --collector_idx;
            }

            for(uint32_t i = aged_list.size(); i < collected_resource_->capacity(); ++i) {
                collectors_[i]->closeRecord();
            }

        }
    };
}//namespace collection
}//namespace sparta


//__SPARTA_AGED_ARRAY_COLLECTOR_H__
#endif
