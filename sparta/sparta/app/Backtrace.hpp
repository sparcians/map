// <Backtrace.h> -*- C++ -*-


/**
 * \file Backtrace.hpp
 * \brief Handles writing backtraces on errors
 */

#ifndef __SPARTA_APP_BACKTRACE_H__
#define __SPARTA_APP_BACKTRACE_H__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef __USE_GNU
#define __USE_GNU
#endif

#include <csignal>
#include <iostream>
#include <map>
#include <vector>
#include <string>

namespace sparta
{
    namespace app
    {

/*!
 * \brief Backtrace without line information. Can be rendered out when
 * needed because rendering is slow
 */
class BacktraceData
{

    /*!
     * \brief Frames for this backtrace. frames_[0] is frame "1"
     */
    std::vector<std::pair<void*, std::string>> frames_;

public:

    /*!
     * \brief Construct the backtrace
     */
    BacktraceData() = default;

    /*!
     * \brief Copy constructable
     */
    BacktraceData(const BacktraceData&) = default;

    /*!
     * \brief Not copy-assignable
     */
    BacktraceData& operator=(const BacktraceData&) = delete;

    /*!
     * \brief Render the backtrace to a file
     * \param o Ostream to write to
     * \param line_info Display line info as part of back trace (in addition
     * to symbols and address)
     */
    void render(std::ostream& o, bool line_info=true) const;

    /*!
     * \brief Adds a new frame to the top of the backtrace (which is
     * progressively more shallow in the real stack). The first frame added
     * is called frame 1, the next: 2, and so on.
     * \param addr Address
     * \param message Message describing this stack frame. Will be copied
     */
    void addFrame(void* addr, const char* message);
};

/*!
 * \brief Backtrace printer. Registers a handler for certain fatal signals
 * and dumps the backtrace if they occur
 * \warning Simulators using this feature should not be distributed to
 * systems because backtrace printing code is highly platform-specific
 */
class Backtrace
{
    struct sigaction sigact_; //!< signal action

    std::map<int, struct sigaction> handled_; //!< All handled signals

public:

    /*!
     * \brief Default constructor
     */
    Backtrace();

    /*!
     * \brief Restore all handlers if they haven't been replaced a second time
     */
    ~Backtrace();

    /*!
     * \brief Set this as the handler for a signal
     * \note This can be multiply called with different signals
     * \param signum Signal to handle
     */
    void setAsHandler(int signum=SIGSEGV);

    /*!
     * \brief Write the current backtrace to a stream
     */
    static void dumpBacktrace(std::ostream& o);

    /*!
     * \brief Gets the current backtrace without rendering it
     */
    static BacktraceData getBacktrace();

}; // class Backtrace

    } // namespace app
} // namespace sparta
#endif // __SPARTA_APP_BACKTRACE_H__
