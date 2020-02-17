// <LifeTracker.h> -*- C++ -*-


/**
 * \file   LifeTracker.hpp
 * \brief  File that defines the LifeTracker class
 */

#ifndef __LIFE_TRACKER_H__
#define __LIFE_TRACKER_H__

#include <memory>

namespace sparta
{
    namespace utils
    {
        /**
         * \class LifeTracker
         * \brief A class that will track the lifetime of the object it points to.
         *
         * This class, which derives from std::shared_ptr, allows a
         * developer to "wrap" a non-shared_ptr object with a lifetime
         * time bomb.  Receivers of this class can create a std::weak_ptr
         * to it and check for expiration. See example below and in the
         * Utils test.
         *
         * \code
         *
         * class MyVolatileTrackedClass
         * {
         * public:
         *     using MyVolatileTrackedClassTracker = sparta::utils::LifeTracker<MyVolatileTrackedClass>;
         *     const MyVolatileTrackedClassTracker & getLifeTracker() const {
         *         return life_tracker_;
         *     }
         *
         *     const uint32_t value = 10;
         *
         * private:
         *     // This object MUST live inside the class being tracked.
         *     MyVolatileTrackedClassTracker life_tracker_{this};
         * };
         *
         *
         * int main()
         * {
         *     std::weak_ptr<MyVolatileTrackedClass::MyVolatileTrackedClassTracker> wptr;
         *
         *     {
         *         MyVolatileTrackedClass my_object;
         *         wptr = my_object.getLifeTracker();
         *         if(false == wptr.expired()) {
         *             std::cout << "The class is still valid: " <<
         *                 wptr.lock()->tracked_object->value << std::endl;
         *         }
         *     }
         *
         *     if(wptr.expired()) {
         *         std::cout << "The class has expired" << std::endl;
         *     }
         *     return 0;
         * }
         *
         * \endcode
         */
        template<class TrackedObjectT>
        class LifeTracker : public std::shared_ptr<LifeTracker<TrackedObjectT> >
        {
        public:
            LifeTracker(TrackedObjectT * obj) :
                std::shared_ptr<LifeTracker<TrackedObjectT>>(this, [] (LifeTracker<TrackedObjectT> *) {}),
                tracked_object(obj)
            {}

            TrackedObjectT * tracked_object;
        };

    }
}

#endif
