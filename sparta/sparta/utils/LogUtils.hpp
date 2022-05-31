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

/** Macro to simplify sending messages to the Unit debug logger
 *  @param msg The message to send to the debug logger
 */
#ifndef DLOG
#define DLOG(msg) \
    if (SPARTA_EXPECT_FALSE(debug_logger_)) { \
        debug_logger_ << __func__ <<  ": " << msg; \
    }
#endif

/** Macro to simplify sending messages to the Unit info logger
 *  @param msg The message to send to the info logger
 */
#ifndef ILOG
#define ILOG(msg) \
    if (SPARTA_EXPECT_FALSE(info_logger_)) { \
        info_logger_ << __func__ <<  ": " << msg; \
    }
#endif

/** Macro to simplify sending messages to the Unit warn logger
 *  @param msg The message to send to the warn logger
 */
#ifndef WLOG
#define WLOG(msg) \
    if (SPARTA_EXPECT_FALSE(warn_logger_)) { \
        warn_logger_ << __func__ <<  ": " << msg; \
    }
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
