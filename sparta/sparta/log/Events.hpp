// <Events> -*- C++ -*-

#pragma once

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

