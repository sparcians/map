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
        using Triggers = uint64_t;
        static constexpr Triggers STOP = 0;
        static constexpr Triggers START = 1;
        static const std::string NOTIFICATION_SRC_NAME = "roi_start_stop";
    }
}
