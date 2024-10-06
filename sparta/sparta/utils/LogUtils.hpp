// <LogUtils.hpp> -*- C++ -*-

#pragma once

#include <iomanip>

/** @defgroup group1 Logger Output Macros
 *  These macros are meant to be used within a method within a class that
 *  derives from sparta::Unit to simplify sending messages to the debug,
 *  info, and warn loggers defined in the sparta::Unit base class.
 *
 *  Example usage:
 *
 *      class Fetch : public sparta::Unit
 *      {
 *          void FetchSomeInsts() const {
 *              const int val = 5;
 *              DLOG("Got " << val << " instructions");
 *          }
 *      }
 *
 *  Example debug logger output:
 *
 *      {0000000000 00000000 top.fetch debug} FetchSomeInsts: Got 5 instructions
 *
 *  @{
*/

/** The main motivation for decomposition into *LOG_CODE_BLOCK and *LOG_OUTPUT
 *  is to support something like the following:
 *
 *  DLOG_CODE_BLOCK(
 *      std::ostringstream oss;
 *      // Complex sequence of various outputs to the ostringstream
 *      // ...
 *      DLOG_OUTPUT(oss.str());
 *  )
 *
 *  The preprocessor can then still completely eliminate a complex logging message like
 *  this if SPARTA_DISABLE_MACRO_LOGGING is defined. It also allows a complex logging
 *  message to have consistent format with all other logging messages.
*/
#ifndef SPARTA_LOG_CODE_BLOCK
#ifdef SPARTA_DISABLE_MACRO_LOGGING
#define SPARTA_LOG_CODE_BLOCK(logger, code)
#else
#define SPARTA_LOG_CODE_BLOCK(logger, code) \
    if (SPARTA_EXPECT_FALSE(logger)) { \
        code \
    }
#endif
#endif

#ifndef DLOG_CODE_BLOCK
#define DLOG_CODE_BLOCK(code) SPARTA_LOG_CODE_BLOCK(debug_logger_, code)
#endif

#ifndef ILOG_CODE_BLOCK
#define ILOG_CODE_BLOCK(code) SPARTA_LOG_CODE_BLOCK( info_logger_, code)
#endif

#ifndef WLOG_CODE_BLOCK
#define WLOG_CODE_BLOCK(code) SPARTA_LOG_CODE_BLOCK( warn_logger_, code)
#endif

#ifndef SPARTA_LOG_OUTPUT
#define SPARTA_LOG_OUTPUT(logger, msg) logger << __func__ <<  ": " << msg;
#endif

#ifndef DLOG_OUTPUT
#define DLOG_OUTPUT(msg) SPARTA_LOG_OUTPUT(debug_logger_, msg)
#endif

#ifndef ILOG_OUTPUT
#define ILOG_OUTPUT(msg) SPARTA_LOG_OUTPUT( info_logger_, msg)
#endif

#ifndef WLOG_OUTPUT
#define WLOG_OUTPUT(msg) SPARTA_LOG_OUTPUT( warn_logger_, msg)
#endif

/** Macro to simplify sending messages to the Unit logger
 *  @param msg The message to send to the logger
 */
#ifndef SPARTA_LOG
#define SPARTA_LOG(logger, msg) SPARTA_LOG_CODE_BLOCK(logger, SPARTA_LOG_OUTPUT(logger, msg))
#endif

#ifndef DLOG
#define DLOG(msg) SPARTA_LOG(debug_logger_, msg)
#endif

#ifndef ILOG
#define ILOG(msg) SPARTA_LOG( info_logger_, msg)
#endif

#ifndef WLOG
#define WLOG(msg) SPARTA_LOG( warn_logger_, msg)
#endif

#ifndef SPARTA_LOG_IF
#define SPARTA_LOG_IF(logger, condition, msg) \
    SPARTA_LOG_CODE_BLOCK(logger, \
        if (condition) { \
            SPARTA_LOG_OUTPUT(logger, msg) \
        } \
    )
#endif

#ifndef DLOG_IF
#define DLOG_IF(condition, msg) SPARTA_LOG_IF(debug_logger_, condition, msg)
#endif

#ifndef ILOG_IF
#define ILOG_IF(condition, msg) SPARTA_LOG_IF( info_logger_, condition, msg)
#endif

#ifndef WLOG_IF
#define WLOG_IF(condition, msg) SPARTA_LOG_IF( warn_logger_, condition, msg)
#endif

/** @} */

/** @defgroup group2 Logger Hex Macros
 *  These macros are meant to be used within a Logger ouput macro to print
 *  hex values and then return the logger to the default state.
 *
 *  Example usage:
 *
 *      class Fetch : public sparta::Unit
 *      {
 *          void FetchSomeInsts() const {
 *              const int addr = 0xdeadbeef;
 *              DLOG("Got instruction at " << HEX8(addr));"
 *          }
 *      }
 *
 *  Example debug logger output:
 *
 *      {0000000000 00000000 top.fetch debug} FetchSomeInsts: Got instruction at 0xdeadbeef
 *  @{
*/

/** Macro to simplify printing of hex values
 *  @param val The value to print
 *  @param width The number of digits to print
 */
#ifndef HEX
#define HEX(val, width) \
    "0x" << std::setw(width) << std::setfill('0') << std::hex \
         << std::right << val << std::setfill(' ') << std::dec
#endif

/** Macro to simplify printing of 16 digit hex value
 *  @param val The value to print
 */
#ifndef HEX16
#define HEX16(val) HEX(val, 16)
#endif

/** Macro to simplify printing of 8 digit hex values
 *  @param val The value to print
 */
#ifndef HEX8
#define HEX8(val) HEX(val, 8)
#endif

/** @} */
