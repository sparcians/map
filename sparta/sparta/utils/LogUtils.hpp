// <LogUtils.hpp> -*- C++ -*-

#pragma once

#include <iomanip>

/// Macro to simplify sending messages to info_logger_
#define ILOG(msg) \
    if (SPARTA_EXPECT_FALSE(info_logger_)) { \
        info_logger_ << __func__ <<  ": " << msg; \
    }

/// Macro to simplify sending messages to warn_logger_
#define WLOG(msg) \
    if (SPARTA_EXPECT_FALSE(warn_logger_)) { \
        warn_logger_ << __func__ <<  ": " << msg; \
    }

/// Macro to simplify sending messages to debug_logger_
#define DLOG(msg) \
    if (SPARTA_EXPECT_FALSE(debug_logger_)) { \
        debug_logger_ << __func__ <<  ": " << msg; \
    }

/// Macro to simplify printing of arbitrary digit hex values
#define HEX(val, width) \
    "0x" << std::setw(width) << std::setfill('0') << std::hex << std::right << val << std::setfill(' ') << std::dec

/// Macro to simplify printing of 8 digit hex values
#define HEX8(val) HEX(val, 8)

/// Macro to simplify printing of 16 digit hex values
#define HEX16(val) HEX(val, 16)
