// <Colors> -*- C++ -*-


/*!
 * \file Colors.hpp
 * \brief Color code for SPARTA
 */

#pragma once
#include "sparta/utils/StaticInit.hpp"
#include <vector>
#include "sparta/utils/SpartaException.hpp"
//! Define some color code values that are used as the defaults
//! in the global default ColorScheme instance. These should never
//! actually be used manually. You should use the accessor methods of a
//! ColorScheme instance so you get support for easily disabling/enabling
//! the output of colors.
#define SPARTA_UNMANAGED_COLOR_NORMAL          "\033[0;0m"
#define SPARTA_UNMANAGED_COLOR_BOLD            "\033[0;1m"
#define SPARTA_UNMANAGED_COLOR_RED             "\033[0;31m"
#define SPARTA_UNMANAGED_COLOR_GREEN           "\033[0;32m"
#define SPARTA_UNMANAGED_COLOR_YELLOW          "\033[0;33m"
#define SPARTA_UNMANAGED_COLOR_BLUE            "\033[0;34m"
#define SPARTA_UNMANAGED_COLOR_MAGENTA         "\033[0;35m"
#define SPARTA_UNMANAGED_COLOR_CYAN            "\033[0;36m"
#define SPARTA_UNMANAGED_COLOR_BRIGHT_RED      "\033[1;31m"
#define SPARTA_UNMANAGED_COLOR_BRIGHT_GREEN    "\033[1;32m"
#define SPARTA_UNMANAGED_COLOR_BRIGHT_YELLOW   "\033[1;33m"
#define SPARTA_UNMANAGED_COLOR_BRIGHT_BLUE     "\033[1;34m"
#define SPARTA_UNMANAGED_COLOR_BRIGHT_MAGENTA  "\033[1;35m"
#define SPARTA_UNMANAGED_COLOR_BRIGHT_CYAN     "\033[1;36m"
#define SPARTA_UNMANAGED_COLOR_BG_RED          "\033[0;41m"
#define SPARTA_UNMANAGED_COLOR_BG_GREEN        "\033[0;42m"
#define SPARTA_UNMANAGED_COLOR_BG_YELLOW       "\033[0;43m"
#define SPARTA_UNMANAGED_COLOR_BG_BLUE         "\033[0;44m"
#define SPARTA_UNMANAGED_COLOR_BG_MAGENTA      "\033[0;45m"
#define SPARTA_UNMANAGED_COLOR_BG_CYAN         "\033[0;46m"

//! Macros for accessing the colors through the default scheme.
#define SPARTA_CURRENT_COLOR_NORMAL sparta::color::ColorScheme::getDefaultScheme().color(Color::Normal)
#define SPARTA_CURRENT_COLOR_BRIGHT_NORMAL sparta::color::ColorScheme::getDefaultScheme().color(Color::BrightNormal)
#define SPARTA_CURRENT_COLOR_BG_NORMAL sparta::color::ColorScheme::getDefaultScheme().color(Color::BgNormal)
#define SPARTA_CURRENT_COLOR_BOLD sparta::color::ColorScheme::getDefaultScheme().color(Color::Bold)
#define SPARTA_CURRENT_COLOR_BRIGHT_BOLD sparta::color::ColorScheme::getDefaultScheme().color(Color::BrightBold)
#define SPARTA_CURRENT_COLOR_BG_BOLD sparta::color::ColorScheme::getDefaultScheme().color(Color::BgBold)
#define SPARTA_CURRENT_COLOR_RED sparta::color::ColorScheme::getDefaultScheme().color(Color::Red)
#define SPARTA_CURRENT_COLOR_BRIGHT_RED sparta::color::ColorScheme::getDefaultScheme().color(Color::BrightRed)
#define SPARTA_CURRENT_COLOR_BG_RED sparta::color::ColorScheme::getDefaultScheme().color(Color::BgRed)
#define SPARTA_CURRENT_COLOR_GREEN sparta::color::ColorScheme::getDefaultScheme().color(Color::Green)
#define SPARTA_CURRENT_COLOR_BRIGHT_GREEN sparta::color::ColorScheme::getDefaultScheme().color(Color::BrightGreen)
#define SPARTA_CURRENT_COLOR_BG_GREEN sparta::color::ColorScheme::getDefaultScheme().color(Color::BgGreen)
#define SPARTA_CURRENT_COLOR_YELLOW sparta::color::ColorScheme::getDefaultScheme().color(Color::Yellow)
#define SPARTA_CURRENT_COLOR_BRIGHT_YELLOW sparta::color::ColorScheme::getDefaultScheme().color(Color::BrightYellow)
#define SPARTA_CURRENT_COLOR_BG_YELLOW sparta::color::ColorScheme::getDefaultScheme().color(Color::BgYellow)
#define SPARTA_CURRENT_COLOR_BLUE sparta::color::ColorScheme::getDefaultScheme().color(Color::Blue)
#define SPARTA_CURRENT_COLOR_BRIGHT_BLUE sparta::color::ColorScheme::getDefaultScheme().color(Color::BrightBlue)
#define SPARTA_CURRENT_COLOR_BG_BLUE sparta::color::ColorScheme::getDefaultScheme().color(Color::BgBlue)
#define SPARTA_CURRENT_COLOR_MAGENTA sparta::color::ColorScheme::getDefaultScheme().color(Color::Magenta)
#define SPARTA_CURRENT_COLOR_BRIGHT_MAGENTA sparta::color::ColorScheme::getDefaultScheme().color(Color::BrightMagenta)
#define SPARTA_CURRENT_COLOR_BG_MAGENTA sparta::color::ColorScheme::getDefaultScheme().color(Color::BgMagenta)
#define SPARTA_CURRENT_COLOR_CYAN sparta::color::ColorScheme::getDefaultScheme().color(Color::Cyan)
#define SPARTA_CURRENT_COLOR_BRIGHT_CYAN sparta::color::ColorScheme::getDefaultScheme().color(Color::BrightCyan)
#define SPARTA_CURRENT_COLOR_BG_CYAN sparta::color::ColorScheme::getDefaultScheme().color(Color::BgCyan)


static constexpr char const * ALL_COLORS[] = {
    SPARTA_UNMANAGED_COLOR_NORMAL,
    SPARTA_UNMANAGED_COLOR_BOLD,
    SPARTA_UNMANAGED_COLOR_RED,
    SPARTA_UNMANAGED_COLOR_GREEN,
    SPARTA_UNMANAGED_COLOR_YELLOW,
    SPARTA_UNMANAGED_COLOR_BLUE,
    SPARTA_UNMANAGED_COLOR_MAGENTA,
    SPARTA_UNMANAGED_COLOR_CYAN,
    SPARTA_UNMANAGED_COLOR_BRIGHT_RED,
    SPARTA_UNMANAGED_COLOR_BRIGHT_GREEN,
    SPARTA_UNMANAGED_COLOR_BRIGHT_YELLOW,
    SPARTA_UNMANAGED_COLOR_BRIGHT_BLUE,
    SPARTA_UNMANAGED_COLOR_BRIGHT_MAGENTA,
    SPARTA_UNMANAGED_COLOR_BRIGHT_CYAN,
    SPARTA_UNMANAGED_COLOR_BG_RED,
    SPARTA_UNMANAGED_COLOR_BG_GREEN,
    SPARTA_UNMANAGED_COLOR_BG_YELLOW,
    SPARTA_UNMANAGED_COLOR_BG_BLUE,
    SPARTA_UNMANAGED_COLOR_BG_MAGENTA,
    SPARTA_UNMANAGED_COLOR_BG_CYAN

};

//! Define enums for accessing the different colors via a ColorScheme.
enum class Color {
    Normal, Bold, Red, Green, Yellow, Blue, Magenta, Cyan,
    BrightRed, BrightGreen, BrightYellow, BrightBlue,
    BrightMagenta, BrightCyan, BgRed, BgGreen, BgYellow, BgBlue,
    BgMagenta, BgCyan
};




#define SPARTA_CMDLINE_COLOR_NORMAL  "" // SPARTA_UNMANAGED_COLOR_NORMAL
#define SPARTA_CMDLINE_COLOR_ERROR   "" // SPARTA_UNMANAGED_COLOR_ERROR
#define SPARTA_CMDLINE_COLOR_WARNING "" // SPARTA_UNMANAGED_COLOR_YELLOW
#define SPARTA_CMDLINE_COLOR_GOOD    "" // SPARTA_UNMANAGED_COLOR_GOOD

namespace sparta {

namespace color {


    /**
     * \class ColorScheme
     * \brief Accessor methods for obtaining color code strings.
     *
     * \details The idea behind ColorScheme is to have the ability to disable terminal colors in
     * the model with a simple flag. There will be a single ColorScheme instance that is controlled
     * by a sparta command line option disables color for that instance. The majority of output can
     * use that instance unless explicit control over colorization enable/disabled is required.
     */
    class ColorScheme {
    public:
        //! The default color scheme singleton.
        static ColorScheme* _GBL_COLOR_SCHEME;
    private:
        /**
         * \brief a nice wrap to get the color or a blank if
         * coloring is disabled.
         * \param color the reference to the color string to
         * return if we are enabled.
         */
        virtual const char* value_(Color color)

        {
            if(enabled_)
            {
                return all_colors_.at((uint32_t)color);
            }
            else{
                return "";
            }
        }

    public:
        virtual ~ColorScheme() {}
        //! Return the default instance to the global
        // color scheme.
        static ColorScheme& getDefaultScheme()
        {
            return *_GBL_COLOR_SCHEME;
        }


        ColorScheme()
        {
            // load all the colors.
            for (char const * c : ALL_COLORS)
            {
                all_colors_.emplace_back(c);
            }
            basic_colors_.emplace_back(SPARTA_UNMANAGED_COLOR_RED);
            basic_colors_.emplace_back(SPARTA_UNMANAGED_COLOR_GREEN);
            basic_colors_.emplace_back(SPARTA_UNMANAGED_COLOR_YELLOW);
            basic_colors_.emplace_back(SPARTA_UNMANAGED_COLOR_BLUE);
            basic_colors_.emplace_back(SPARTA_UNMANAGED_COLOR_MAGENTA);
            basic_colors_.emplace_back(SPARTA_UNMANAGED_COLOR_CYAN);
            basic_colors_.emplace_back(SPARTA_UNMANAGED_COLOR_BRIGHT_RED);
            basic_colors_.emplace_back(SPARTA_UNMANAGED_COLOR_BRIGHT_GREEN);
            basic_colors_.emplace_back(SPARTA_UNMANAGED_COLOR_BRIGHT_YELLOW);
            basic_colors_.emplace_back(SPARTA_UNMANAGED_COLOR_BRIGHT_BLUE);
            basic_colors_.emplace_back(SPARTA_UNMANAGED_COLOR_BRIGHT_MAGENTA);
            basic_colors_.emplace_back(SPARTA_UNMANAGED_COLOR_BRIGHT_CYAN);
            basic_colors_.emplace_back(static_cast<char const*>(nullptr));
        }

        /**
         * \brief would we like to enable or disable colors.
         * By invoking this function with false, all color accessor
         * functions return the empty string
         * \param enabled a boolean for whether or not we should
         * return true color codes or empty strings.
         */
        virtual void enabled(bool enabled)
        {
            enabled_ = enabled;
        }

        /**
         * \brief Find the next basic color after the color
         * inputted.
         */
        const char* const *  nextBasicColor(const char* const * color=nullptr)
        {
            if(enabled_)
            {
                if (color == nullptr || color == empty_){
                    return basic_colors_.data();
                }

                const char * const * next = color + 1;
                if (*next == nullptr){
                    return basic_colors_.data();
                }
                return next;
            }
            else
            {
                return empty_;
            }
        }

        //! The accessors that should always be used for colors.
        const char* color(Color c)
        {
            return value_(c);
        }
    protected:
        bool enabled_ = true; //! Whether or not we are returning real colors.
        char const * empty_[1]={""}; //! A dummy empty string array to make nextBasicColor work.
        std::vector<const char*> all_colors_; //! A list of colors in order.
        std::vector<const char*> basic_colors_; //! A list of colors in order.
    };




} // namespace color
} // namespace sparta
