// <ConcurrentQueue> -*- C++ -*-

#pragma once

#include <mutex>
#include <queue>

namespace simdb {

/*! 
 * \brief Thread-safe wrapper around std::queue (FIFO)
 */
template <class DataT>
class ConcurrentQueue
{
public:
    ConcurrentQueue() = default;

    //! Push an item to the back of the queue.
    void push(const DataT & item) {
        std::lock_guard<std::mutex> guard(mutex_);
        queue_.emplace(std::move(item));
    }

    //! Emplace an item to the back of the queue.
    template <class... Args>
    void emplace(Args&&... args) {
        std::lock_guard<std::mutex> guard(mutex_);
        queue_.emplace(std::forward<Args>(args)...);
    }

    //! Get the item at the front of the queue. This
    //! returns true if successful, or false if there
    //! was no data in the queue.
    bool try_pop(DataT & item) {
        std::lock_guard<std::mutex> guard(mutex_);
        if (queue_.empty()) {
            return false;
        }
        std::swap(item, queue_.front());
        queue_.pop();
        return true;
    }

    //! How many data points are in the queue?
    size_t size() const {
        std::lock_guard<std::mutex> guard(mutex_);
        return queue_.size();
    }

private:
    mutable std::mutex mutex_;
    std::queue<DataT> queue_;
};

} // namespace simdb


