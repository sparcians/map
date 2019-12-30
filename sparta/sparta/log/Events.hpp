// <Events> -*- C++ -*-

#ifndef __EVENTS_H__
#define __EVENTS_H__

#include <iostream>
#include <vector>

namespace sparta
{
    namespace log
    {
        class Tap;

        /*!
         */
        struct TapAddedEvent
        {
            Tap* tap;
        };

        /*!
         */
        struct TapRemovedEvent
        {
            Tap* tap;
        };

    } // namespace log
} // namespace sparta

// __EVENTS_H__
#endif
