// <RegionOfInterest.hpp> -*- C++ -*-

/**
 * \file RegionOfInterest.hpp
 * \brief Types and variables defined for ROI usage
 */
#pragma once

namespace sparta
{
    namespace roi
    {
        enum Triggers : uint64_t {
            STOP = 0,
            START = 1
        };

        static const std::string NOTIFICATION_SRC_NAME = "roi_start_stop";
    }
}
