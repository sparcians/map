// <SpartaException> -*- C++ -*-

/**
 * \file   SpartaException.hpp
 *
 * \brief  Exception class for all of SPARTA.
 *
 *
 */

#ifndef __SPARTA_EXCEPTION_H__
#define __SPARTA_EXCEPTION_H__

#include <exception>
#include <sstream>
#include <string>
#include <memory>

/*!
 * \brief Utility for throwing an exception ONLY if there is not already an
 * uncaught exception causing the stack to unwind. This is useful in destructors
 * or other shutdown operations when testing for premature shutdown conditions.
 * Often, there are shutdown errors if an exception in the program causes
 * premature destruction and throwing a new exception during shutdown would mask
 * the root cause,
 * \warning this should typically ONLY be used for Fatal destructor errors
 * \deprecated this should not be used because std::uncaught_exception() can not
 *             fully prevent ending up in a double-exception case as explained in
 *             https://akrzemi1.wordpress.com/2011/09/21/destructors-that-throw/
 */
#define THROW_IF_NOT_UNWINDING(exclass, msg_construct) \
    if(std::uncaught_exceptions() > 0){                     \
        std::cerr << msg_construct << std::endl;       \
    }else{                                             \
        throw exclass() << msg_construct;              \
    }

#define DO_PRAGMA(x) _Pragma(#x)
#define TERMINATING_THROW(exclass, msg_construct)      \
    DO_PRAGMA(GCC diagnostic push)                     \
    DO_PRAGMA(GCC diagnostic ignored "-Wterminate")    \
    throw exclass() << msg_construct;                  \
    DO_PRAGMA(GCC diagnostic pop)

namespace sparta
{
    namespace app
    {
        class BacktraceData;
    }

    /**
     * \class SpartaException
     *
     * Used to construct and throw a standard C++ exception. Inherits from
     * std::exception.
     *
     * \warning Creates a backtrace, which introduces much overhead. Whenever
     * possible, avoid constructing the exception until the exact line of where
     * the error condition is detected or a neighboring line which is not
     * separated by any functional code from the place where the error condition
     * is detected.
     * \warning Do not construct this exception unless it is going to be thrown
     */
    class SpartaException : public std::exception
    {
    public:

        /**
         * \brief Construct a SpartaException object with empty reason
         * \note All other non-copy and non-move constructors must delegate to
         * this constructor so that breakpoints can easily be placed on one
         * symbol to catch sparta Exceptions
         *
         * Reason can be populated later with the insertion operator.
         */
        SpartaException();

        /**
         * \brief Construct a SpartaException object
         * \param reason The reason for the exception
         * \note Delegated to SpartaException so that debugger breakpoints may be
         * more easily be placed.
         */
        SpartaException(const std::string & reason);

        /**
         * \brief Copy construct a SpartaException object
         */
        SpartaException(const SpartaException & orig);

        /// Destroy!
        virtual ~SpartaException() noexcept;

        /**
         * \brief Overload from std::exception
         * \return Const char * of the exception reason
         */
        const char* what() const noexcept {
            reason_str_ = reason_.str();
            return reason_str_.c_str();
        }

        /**
         * \brief Returns the backtrace at the time this exception was generated
         */
        std::string backtrace() const noexcept;

        /**
         * \brief Return the raw reason without file, line information
         * \return The raw reason, no file information
         */
        std::string rawReason() const {
            return raw_reason_;
        }

        /**
         * \brief Append additional information to the message.
         * \param msg The addition info
         * \return This exception object
         *
         * Usage:
         * \code
         * int bad_company = 4;
         * sparta::SpartaException e("Oh uh");
         * e << ": this is bad: " << bad_company;
         * \endcode
         * or you can do this:
         * \code
         * int bad_company = 4;
         * throw sparta::SpartaException e("Oh uh") << ": this is bad: " << bad_company;
         * \endcode
         * but it's not as pretty.
         */
        template<class T>
        SpartaException & operator<<(const T & msg) {
            reason_ << msg;
            return *this;
        }

    private:
        // The raw reason without file/line information
        std::string raw_reason_;

        // The reason/explanation for the exception
        std::stringstream reason_;

        // Backtrace at the time of the exception
        std::unique_ptr<app::BacktraceData> bt_;

        // Need to keep a local copy of the string formed in the
        // string stream for the 'what' call
        mutable std::string reason_str_;
    };


    /**
     * \brief Indicates something went seriously wrong and likely indicates
     * corruption in simulator runtime state.
     *
     * This exception should be thrown only if a rule of the framework is
     * broken.
     *
     * This exception is slightly less severe than a fatal error, but in most
     * cases the simulator should attempt to enter a safe debugging mode instead
     * of exiting.
     */
    class SpartaCriticalError : public SpartaException
    {
    public:

        /**
         * \brief Construct a SpartaCriticalException object
         *
         * Reason can be populated later with the insertion operator.
         */
        SpartaCriticalError() :
            SpartaException()
        { }

        /**
         * \brief Construct a SpartaCriticalException object
         * \param reason The reason for the exception
         */
        SpartaCriticalError(const std::string & reason) :
            SpartaException(reason)
        { }
    };

    /**
     * \brief Indicates something went seriously wrong and likely indicates
     * unrecoverable corruption in simulator runtime state or misuse.
     * This is the only exception that should ever be generated by a destructor
     * since its semantics are that the simulation should terminate.
     *
     * This exception should generally only be thrown only if a fundamental rule
     * of the framework is broken related to framework object instantiation
     * (e.g. object destructed twice).
     *
     * Simulator should generally exit or allow this error to propogate, causing
     * an abort instead of entering a simulator-managed non-executing debug
     * state.
     */
    class SpartaFatalError : public SpartaException
    {
    public:

        /**
         * \brief Construct a SpartaFatalError object
         *
         * Reason can be populated later with the insertion operator.
         */
        SpartaFatalError() :
            SpartaException()
        { }

        /**
         * \brief Construct a SpartaFatalError object
         * \param reason The reason for the exception
         */
        SpartaFatalError(const std::string & reason) :
            SpartaException(reason)
        { }
    };

}

// __SPARTA_EXCEPTION_H__
#endif
