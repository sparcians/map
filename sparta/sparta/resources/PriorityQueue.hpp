// <PriorityQueue.hpp> -*- C++ -*-


/**
 * \file   PriorityQueue.hpp
 * \brief  Defines a Priority queue similar to STL's, but with more functionality
 */

#pragma once

#include <list>
#include <algorithm>

#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/FastList.hpp"

namespace sparta
{

    // A default sorting algorithm
    template<class DataT>
    struct DefaultSortingAlgorithm
    {
        bool operator()(const DataT & existing, const DataT & to_be_inserted) const {
            return existing < to_be_inserted;
        }
    };

    /**
     * \class PriorityQueue
     * \brief A data structure that allows pushing/emplacing into it
     *        with a defined sorter
     * \tparam DataT The data to be contained and sorted
     * \tparam SortingAlgorithmT The sorting algorithm to use
     * \tparam bounded_cnt The max number of elements in this PriorityQueue
     *
     * The PriorityQueue can be used by picking algorithms in a model
     * where more than one entry of a block is ready (for whatever
     * reason) and the model needs to know which one to "pick" for the
     * next operation.
     *
     * The queue defines a less-than type of sorter by default, but
     * allows the modeler to define an operator object that can
     * override the behavior.
     *
     * In addition, entries in the queue can be removed (even in the
     * middle).  This is handy for items in the queue that are no
     * longer participating in the priority.
     *
     * Finally, the queue supports a basic override to the order,
     * allowing a "high priority" DataT object to be pushed to the
     * front, even if that object doesn't conform to the ordering
     * rules.
     *
     * If the template parameter bounded_cnt is non-zero, the
     * PriorityQueue will be bounded to an upper limit of
     * bounded_cnt. This also improves the performance of the
     * PriorityQueue (uses sparta::utils::FastList).
     *
     */
    template <class DataT,
              class SortingAlgorithmT = DefaultSortingAlgorithm<DataT>,
              size_t bounded_cnt=0>
    class PriorityQueue
    {
    private:
        using PQueueType =
            typename std::conditional<bounded_cnt == 0,
                                      std::list<DataT>,
                                      utils::FastList<DataT>>::type;

    public:

        // For collection
        using size_type      = size_t;
        using iterator       = typename PQueueType::iterator;
        using const_iterator = typename PQueueType::const_iterator;

        /**
         * \brief Create a priority queue with a default instance of the
         *        sorting algorithm
         */
        PriorityQueue() :
            priority_items_(bounded_cnt)
        {}

        /**
         * \brief Create a priority queue with a specific instance of the
         *        sorting algorithm
         * \param sort_alg Reference to the sorting algorithm instance
         */
        PriorityQueue(const SortingAlgorithmT & sort_alg) :
            priority_items_(bounded_cnt),
            sort_alg_(sort_alg)
        {}

        /**
         * \brief Inserts the data item into the list using the
         *        sorting alg.  Stops at the first insertion.
         * \param data The data to insert
         */
        void insert(const DataT & data)
        {
            const auto eit = priority_items_.end();
            for(auto it = priority_items_.begin(); it != eit; ++it)
            {
                if(sort_alg_(data, *it)) {
                    priority_items_.insert(it, data);
                    return;
                }
            }
            priority_items_.emplace_back(data);
        }

        //! Get the number of items in the queue
        size_t size() const {
            return priority_items_.size();
        }

        //! Is the queue empty?
        bool empty() const {
            return priority_items_.empty();
        }

        //! Get the first element in the queue
        const DataT & top() const {
            sparta_assert(false == empty(), "Grabbing top from an empty queue");
            return priority_items_.front();
        }

        //! Get the last element (lowest priority) in the queue
        const DataT & back() const {
            sparta_assert(false == empty(), "Grabbing back from an empty queue");
            return priority_items_.back();
        }

        //! Pop the front of the queue (highest priority)
        void pop() {
            sparta_assert(false == empty(), "Popping on an empty priority queue");
            priority_items_.pop_front();
        }

        //! Clear the entire queue
        void clear() {
            priority_items_.clear();
        }

        //! Remove the item from the queue
        void remove(const DataT & data) {
            priority_items_.remove(data);
        }

        //! Erase the item from the queue (const_iterator/iterator)
        void erase(const const_iterator & it) {
            priority_items_.erase(it);
        }

        /**
         * \brief Force a data entry to the front of the queue
         * \param data Reference to the data object forced to the
         *             front of the queue
         *
         * Push the data item to the front of the queue, bypassing the
         * internal sorting algorithm.  This will mess up the sorting
         * -- be warned.
         */
        void forceFront(const DataT & data) {
            priority_items_.emplace_front(data);
        }

        /*! \defgroup iteration Iteration Support */
        /**@{*/
        //! Iterator to beginning of the queue -- highest priority
        iterator        begin()       { return priority_items_.begin(); }

        //! Const Iterator to beginning of the queue -- highest priority
        const_iterator  begin() const { return priority_items_.begin(); }

        //! Iterator to end of the queue -- end priority
        iterator        end()       { return priority_items_.end(); }

        //! Const Iterator to end of the queue -- end priority
        const_iterator  end() const { return priority_items_.end(); }
        /**@}*/

    private:

        //! The internal queue
        //PQueueType        priority_items_{bounded_cnt, DataT()};
        PQueueType        priority_items_;

        //! Copy of the sorting algorithm
        SortingAlgorithmT sort_alg_;
    };
}
