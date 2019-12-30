#ifndef __SPARTA_UTILS_TRACKING_HANDLE_H__
#define __SPARTA_UTILS_TRACKING_HANDLE_H__

#include <memory>
#include "sparta/utils/SpartaException.hpp"

namespace sparta {
namespace utils {

template<class Obj>
class TrackingHandle
{
    typedef std::shared_ptr<Obj>     Handle;
public:
    TrackingHandle()
    {}

    TrackingHandle(Obj* ptr):
        handle_(ptr)
    {}

    TrackingHandle(const Handle& hand):
        handle_(hand)
    {}

    TrackingHandle(const TrackingHandle& other):
        handle_(other.handle_)
    {}

    ~TrackingHandle()
    {}

    TrackingHandle& operator=(const TrackingHandle& other)
    {
        if (this != &other) {
            handle_ = other.handle_;
        }
        return *this;
    }

    TrackingHandle& operator=(const Handle& hand)
    {
        handle_ = hand;
        return *this;
    }

    void reset()
    {
        handle_.reset();
    }

    void reset(Obj* ptr)
    {
        handle_.reset(ptr);
    }

    Obj* get() const
    {
        return handle_.get();
    }

    Obj& operator*() const
    {
        return *handle_;
    }

    Obj* operator->() const
    {
        return handle_.get();
    }

    long use_count() const
    {
        return handle_.use_count();
    }

    operator bool() const
    {
        return bool(handle_);
    }

    bool operator==(const TrackingHandle& other) const
    {
        return (handle_ == other.handle_);
    }

    bool operator!=(const TrackingHandle& other) const
    {
        return (handle_ != other.handle_);
    }

    explicit operator std::shared_ptr<Obj>() const
    {
        return handle_;
    }

    operator std::weak_ptr<Obj>() const
    {
        return handle_;
    }

private:
    Handle  handle_;
};

template<class Obj>
bool operator==(const std::shared_ptr<Obj>& lhs, const TrackingHandle<Obj>& rhs)
{
    return (lhs == std::shared_ptr<Obj>(rhs));
}

} // namespace sparta::utils
} // namespace sparta

#endif // __SPARTA_UTILS_TRACKING_HANDLE_H__
