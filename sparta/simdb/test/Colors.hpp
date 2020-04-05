// <Colors> -*- C++ -*-

/*!
 * \brief Color codes / utilities for SimDB.
 */

#pragma once

#include "simdb/Errors.hpp"

#include <vector>
#include <type_traits>

//! Define some color code values that are used as the defaults
//! in the global default ColorScheme instance. These should never
//! actually be used manually. You should use the accessor methods of a
//! ColorScheme instance so you get support for easily disabling/enabling
//! the output of colors.
#define SIMDB_UNMANAGED_COLOR_NORMAL          "\033[0;0m"
#define SIMDB_UNMANAGED_COLOR_BOLD            "\033[0;1m"
#define SIMDB_UNMANAGED_COLOR_RED             "\033[0;31m"
#define SIMDB_UNMANAGED_COLOR_GREEN           "\033[0;32m"
#define SIMDB_UNMANAGED_COLOR_YELLOW          "\033[0;33m"
#define SIMDB_UNMANAGED_COLOR_BLUE            "\033[0;34m"
#define SIMDB_UNMANAGED_COLOR_MAGENTA         "\033[0;35m"
#define SIMDB_UNMANAGED_COLOR_CYAN            "\033[0;36m"
#define SIMDB_UNMANAGED_COLOR_BRIGHT_RED      "\033[1;31m"
#define SIMDB_UNMANAGED_COLOR_BRIGHT_GREEN    "\033[1;32m"
#define SIMDB_UNMANAGED_COLOR_BRIGHT_YELLOW   "\033[1;33m"
#define SIMDB_UNMANAGED_COLOR_BRIGHT_BLUE     "\033[1;34m"
#define SIMDB_UNMANAGED_COLOR_BRIGHT_MAGENTA  "\033[1;35m"
#define SIMDB_UNMANAGED_COLOR_BRIGHT_CYAN     "\033[1;36m"
#define SIMDB_UNMANAGED_COLOR_BG_RED          "\033[0;41m"
#define SIMDB_UNMANAGED_COLOR_BG_GREEN        "\033[0;42m"
#define SIMDB_UNMANAGED_COLOR_BG_YELLOW       "\033[0;43m"
#define SIMDB_UNMANAGED_COLOR_BG_BLUE         "\033[0;44m"
#define SIMDB_UNMANAGED_COLOR_BG_MAGENTA      "\033[0;45m"
#define SIMDB_UNMANAGED_COLOR_BG_CYAN         "\033[0;46m"

//! Macros for accessing the colors through the default scheme.
#define SIMDB_CURRENT_COLOR_NORMAL simdb::color::ColorScheme::getDefaultScheme().color(Color::Normal)
#define SIMDB_CURRENT_COLOR_BRIGHT_NORMAL simdb::color::ColorScheme::getDefaultScheme().color(Color::BrightNormal)
#define SIMDB_CURRENT_COLOR_BG_NORMAL simdb::color::ColorScheme::getDefaultScheme().color(Color::BgNormal)
#define SIMDB_CURRENT_COLOR_BOLD simdb::color::ColorScheme::getDefaultScheme().color(Color::Bold)
#define SIMDB_CURRENT_COLOR_BRIGHT_BOLD simdb::color::ColorScheme::getDefaultScheme().color(Color::BrightBold)
#define SIMDB_CURRENT_COLOR_BG_BOLD simdb::color::ColorScheme::getDefaultScheme().color(Color::BgBold)
#define SIMDB_CURRENT_COLOR_RED simdb::color::ColorScheme::getDefaultScheme().color(Color::Red)
#define SIMDB_CURRENT_COLOR_BRIGHT_RED simdb::color::ColorScheme::getDefaultScheme().color(Color::BrightRed)
#define SIMDB_CURRENT_COLOR_BG_RED simdb::color::ColorScheme::getDefaultScheme().color(Color::BgRed)
#define SIMDB_CURRENT_COLOR_GREEN simdb::color::ColorScheme::getDefaultScheme().color(Color::Green)
#define SIMDB_CURRENT_COLOR_BRIGHT_GREEN simdb::color::ColorScheme::getDefaultScheme().color(Color::BrightGreen)
#define SIMDB_CURRENT_COLOR_BG_GREEN simdb::color::ColorScheme::getDefaultScheme().color(Color::BgGreen)
#define SIMDB_CURRENT_COLOR_YELLOW simdb::color::ColorScheme::getDefaultScheme().color(Color::Yellow)
#define SIMDB_CURRENT_COLOR_BRIGHT_YELLOW simdb::color::ColorScheme::getDefaultScheme().color(Color::BrightYellow)
#define SIMDB_CURRENT_COLOR_BG_YELLOW simdb::color::ColorScheme::getDefaultScheme().color(Color::BgYellow)
#define SIMDB_CURRENT_COLOR_BLUE simdb::color::ColorScheme::getDefaultScheme().color(Color::Blue)
#define SIMDB_CURRENT_COLOR_BRIGHT_BLUE simdb::color::ColorScheme::getDefaultScheme().color(Color::BrightBlue)
#define SIMDB_CURRENT_COLOR_BG_BLUE simdb::color::ColorScheme::getDefaultScheme().color(Color::BgBlue)
#define SIMDB_CURRENT_COLOR_MAGENTA simdb::color::ColorScheme::getDefaultScheme().color(Color::Magenta)
#define SIMDB_CURRENT_COLOR_BRIGHT_MAGENTA simdb::color::ColorScheme::getDefaultScheme().color(Color::BrightMagenta)
#define SIMDB_CURRENT_COLOR_BG_MAGENTA simdb::color::ColorScheme::getDefaultScheme().color(Color::BgMagenta)
#define SIMDB_CURRENT_COLOR_CYAN simdb::color::ColorScheme::getDefaultScheme().color(Color::Cyan)
#define SIMDB_CURRENT_COLOR_BRIGHT_CYAN simdb::color::ColorScheme::getDefaultScheme().color(Color::BrightCyan)
#define SIMDB_CURRENT_COLOR_BG_CYAN simdb::color::ColorScheme::getDefaultScheme().color(Color::BgCyan)

static constexpr const char * ALL_COLORS[] = {
    SIMDB_UNMANAGED_COLOR_NORMAL,
    SIMDB_UNMANAGED_COLOR_BOLD,
    SIMDB_UNMANAGED_COLOR_RED,
    SIMDB_UNMANAGED_COLOR_GREEN,
    SIMDB_UNMANAGED_COLOR_YELLOW,
    SIMDB_UNMANAGED_COLOR_BLUE,
    SIMDB_UNMANAGED_COLOR_MAGENTA,
    SIMDB_UNMANAGED_COLOR_CYAN,
    SIMDB_UNMANAGED_COLOR_BRIGHT_RED,
    SIMDB_UNMANAGED_COLOR_BRIGHT_GREEN,
    SIMDB_UNMANAGED_COLOR_BRIGHT_YELLOW,
    SIMDB_UNMANAGED_COLOR_BRIGHT_BLUE,
    SIMDB_UNMANAGED_COLOR_BRIGHT_MAGENTA,
    SIMDB_UNMANAGED_COLOR_BRIGHT_CYAN,
    SIMDB_UNMANAGED_COLOR_BG_RED,
    SIMDB_UNMANAGED_COLOR_BG_GREEN,
    SIMDB_UNMANAGED_COLOR_BG_YELLOW,
    SIMDB_UNMANAGED_COLOR_BG_BLUE,
    SIMDB_UNMANAGED_COLOR_BG_MAGENTA,
    SIMDB_UNMANAGED_COLOR_BG_CYAN
};

//! Define enums for accessing the different colors via a ColorScheme.
enum class Color {
    Normal, Bold, Red, Green, Yellow, Blue, Magenta, Cyan,
    BrightRed, BrightGreen, BrightYellow, BrightBlue,
    BrightMagenta, BrightCyan, BgRed, BgGreen, BgYellow, BgBlue,
    BgMagenta, BgCyan
};

#define SIMDB_CMDLINE_COLOR_NORMAL  "" // SIMDB_UNMANAGED_COLOR_NORMAL
#define SIMDB_CMDLINE_COLOR_ERROR   "" // SIMDB_UNMANAGED_COLOR_ERROR
#define SIMDB_CMDLINE_COLOR_WARNING "" // SIMDB_UNMANAGED_COLOR_YELLOW
#define SIMDB_CMDLINE_COLOR_GOOD    "" // SIMDB_UNMANAGED_COLOR_GOOD

namespace simdb {
namespace color {

    /**
     * \class ColorScheme
     * \brief Accessor methods for obtaining color code strings.
     * \details The idea behind ColorScheme is to have the ability
     * to disable terminal colors in the module with a simple flag.
     */
    class ColorScheme
    {
    public:
        static ColorScheme & getDefaultScheme() {
            static ColorScheme scheme;
            return scheme;
        }

        ~ColorScheme() = default;

        /**
         * \brief Enable or disable colors.
         * \param enabled Flag denoting whether colors are enabled for
         * error reporting in SimDB.
         */
        void setIsEnabled(const bool enabled) {
            enabled_ = enabled;
        }

        //! The accessors that should always be used for colors.
        const char * color(const Color c) const {
            if (enabled_) {
                using utype = typename std::underlying_type<Color>::type;
                return all_colors_.at(static_cast<utype>(c)).c_str();
            }

            static const char * empty = "";
            return empty;
        }

    private:
        ColorScheme()
        {
            // Load all the colors.
            for (const char * c : ALL_COLORS) {
                all_colors_.emplace_back(c);
            }
        }

        //! Whether or not we are returning real colors.
        bool enabled_ = true;

        //! A list of colors in order.
        std::vector<std::string> all_colors_;
    };

} // namespace color
} // namespace simdb

