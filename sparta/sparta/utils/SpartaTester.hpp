// <SpartaTester> -*- C++ -*-


#ifndef __SPARTA_TESTER_H__
#define __SPARTA_TESTER_H__

/**
 * \file   SpartaTester.hpp
 *
 * \brief File that defines the SpartaTester class and testing Macros
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <inttypes.h>
#include <stdexcept>
#include <string>
#include <set>
#include <boost/algorithm/string/predicate.hpp>
#include "sparta/utils/Colors.hpp"
#include "simdb/utils/MathUtils.hpp"

namespace sparta
{
    /**
     * \class SpartaTester
     * \brief A simple testing class
     *
     * The user of this framework should not have to instantiate this
     * class directly.  You can if you want to, but you should use the
     * TEST_INIT macro.
     *
     * Example usage:
     * \code
     * TEST_INIT;
     *
     * int main() {
     *     EXPECT_TRUE(true);
     *     EXPECT_FALSE(false);
     *     EXPECT_NOTHROW(int a = 3;);
     *     EXPECT_THROW(throw a;);
     *     EXPECT_EQUAL(2+2, 4);
     *     EXPECT_NOTEQUAL(2+2, 5)
     *     EXPECT_NOTEQUAL(p, nullptr) // Given: Thingy* p;
     *
     *     REPORT_ERROR;
     *     return ERROR_CODE;
     * }
     * \endcode
     */
    class SpartaTester
    {
    public:
        SpartaTester() : SpartaTester(0, std::cerr)
        {}

        bool expectAllReached(uint32_t expected_reached, const uint32_t line, const char * file) {
            bool ret = true;
            if(methods_reached_.size() != expected_reached)
            {
                cerr_ << SPARTA_CURRENT_COLOR_BRIGHT_RED << "Test failed to execute the "
                    << expected_reached << " expected methods at least once." << "\n"
                    "Instead, " << methods_reached_.size() << " were reached."
                    << std::endl;
                //list the methods that were in fact reached.
                cerr_ << "The test only reached the following: " << std::endl;
                cerr_ << SPARTA_CURRENT_COLOR_GREEN;
                for(auto s : methods_reached_)
                {
                    cerr_ << "-> " <<s << "\n";
                }
                 cerr_ << SPARTA_CURRENT_COLOR_BRIGHT_RED << "' FAILED on line "
                          << line << " in file " << file << SPARTA_CURRENT_COLOR_NORMAL << std::endl;
                ++num_errors_;
                ret = false;
                cerr_ << std::endl;
            }
            return ret;
        }
        bool expect(bool val, const char * test_type, const uint32_t line, const char * file) {
            bool ret = true;
            if(!val) {
                cerr_ << SPARTA_CURRENT_COLOR_BRIGHT_RED << "Test '" << test_type << "' FAILED on line "
                          << line << " in file " << file << SPARTA_CURRENT_COLOR_NORMAL << std::endl;
                ++num_errors_;
                ret = false;
            }
            return ret;
        }

        // Try and compare bytes and display values on failure instead of characters
        bool expectEqual(uint8_t v1, uint8_t v2, bool expected, const char * test_type, const uint32_t line, const char * file) {
            bool ret = true;
            if((v1 == v2) != expected) {
                cerr_ << SPARTA_CURRENT_COLOR_BRIGHT_RED << "Test '" << test_type << "' FAILED on line "
                          << line << " in file " << file
                          << ". Value: '" << (uint32_t)v1;
                if(expected){
                    cerr_ << "' should equal '";
                }else{
                    cerr_ << "' should NOT equal '";
                }
                cerr_ << (uint32_t)v2 << "'" << SPARTA_CURRENT_COLOR_NORMAL << std::endl;
                ++num_errors_;
                ret = false;
            }
            return ret;
        }

        // Try and compare different types
        template<typename T>
        bool expectEqual(const T& v1, const T& v2, bool expected, const char * test_type, const uint32_t line, const char * file) {
            bool ret = true;
            if((v1 == v2) != expected) {
                cerr_ << SPARTA_CURRENT_COLOR_BRIGHT_RED << "Test '" << test_type << "' FAILED on line "
                          << line << " in file " << file
                          << ". Value: '" << v1;
                if(expected){
                    cerr_ << "' should equal '";
                }else{
                    cerr_ << "' should NOT equal '";
                }
                cerr_ << v2 << "'" << SPARTA_CURRENT_COLOR_NORMAL << std::endl;
                ++num_errors_;
                ret = false;
            }
            return ret;
        }

        // Try and compare different types
        template<typename T, typename U=T>
        bool expectEqual(const T& v1, const U& v2, bool expected, const char * test_type, const uint32_t line, const char * file) {
            bool ret = true;
            if(compare<T,U>(v1,v2) != expected) {
                cerr_ << SPARTA_CURRENT_COLOR_BRIGHT_RED << "Test '" << test_type << "' FAILED on line "
                          << line << " in file " << file
                          << ". Value: '" << v1;
                if(expected){
                    cerr_ << "' should equal '";
                }else{
                    cerr_ << "' should NOT equal '";
                }
                cerr_ << v2 << "'" << SPARTA_CURRENT_COLOR_NORMAL << std::endl;
                ++num_errors_;
                ret = false;
            }
            return ret;
        }

        // Comparison operation for two integers having different signed-ness
        template<typename T, typename U>
        typename std::enable_if<std::is_integral<T>::value
                                && std::is_integral<U>::value
                                && (std::is_signed<T>::value != std::is_signed<U>::value), bool>::type
        compare(const T& t, const U& u) {
            return (t == (T)u);
        }

        // Comparison operation for everything else
        template<typename T, typename U>
        typename std::enable_if<!(std::is_integral<T>::value
                                  && std::is_integral<U>::value
                                  && (std::is_signed<T>::value != std::is_signed<U>::value)), bool>::type
        compare(const T& t, const U& u) {
            return (t == u);
        }


        // Overload for comparison with nullptr
        template<typename T>
        bool expectEqual(const T& v1, const std::nullptr_t, bool expected, const char * test_type, const uint32_t line, const char * file) {
            bool ret = true;
            if((v1 == nullptr) != expected) {
                cerr_ << SPARTA_CURRENT_COLOR_BRIGHT_RED << "Test '" << test_type << "' FAILED on line "
                          << line << " in file " << file
                          << ". Value: '" << v1;
                if(expected){
                    cerr_ << "' should equal '";
                }else{
                    cerr_ << "' should NOT equal '";
                }
                cerr_ << "null" << "'" << SPARTA_CURRENT_COLOR_NORMAL << std::endl;
                ++num_errors_;
                ret = false;
            }
            return ret;
        }

        // Overload for comparison of nullptr with a var
        template<typename T>
        bool expectEqual(const std::nullptr_t, const T& v1, bool expected, const char * test_type, const uint32_t line, const char * file) {
            bool ret = true;
            if((nullptr == v1) != expected) {
                cerr_ << SPARTA_CURRENT_COLOR_BRIGHT_RED << "Test '" << test_type << "' FAILED on line "
                          << line << " in file " << file
                          << ". Value: '" << v1;
                if(expected){
                    cerr_ << "' should equal '";
                }else{
                    cerr_ << "' should NOT equal '";
                }
                cerr_ << "null" << "'" << SPARTA_CURRENT_COLOR_NORMAL << std::endl;
                ++num_errors_;
                ret = false;
            }
            return ret;
        }

        template<typename T>
        typename std::enable_if<
            std::is_floating_point<T>::value,
        bool>::type
        expectEqualWithinTolerance(const T & v1, const T & v2, const T & tol,
                                   const char * test_type, const uint32_t line,
                                   const char * file) {
            bool ret = true;
            if (tol < 0) {
                cerr_ << SPARTA_CURRENT_COLOR_BRIGHT_RED << "Test '"
                      << test_type << "' FAILED on line "
                      << line << " in file " << file
                      << ". Negative tolerance supplied."
                      << SPARTA_CURRENT_COLOR_NORMAL << std::endl;
                ++num_errors_;
                ret = false;
            } else {
                ret = simdb::utils::approximatelyEqual(v1, v2, tol);
                if (!ret) {
                    cerr_ << SPARTA_CURRENT_COLOR_BRIGHT_RED << "Test '"
                          << test_type << "' FAILED on line "
                          << line << " in file " << file
                          << ". Value: '" << v1 << "' should be equal to '"
                          << v2 << "' within tolerance '" << tol << "'";
                    ++num_errors_;
                }
            }
            return ret;
        }

        void throwTestFailed(const char * test_type, const uint32_t line, const char * file, const char * exception_what="") {
            cerr_ << SPARTA_CURRENT_COLOR_BRIGHT_RED << "Throw Test Fail:'" << test_type << "' FAILED on line "
                      << line << " in file " << file << std::endl;
            if(exception_what != 0 && strlen(exception_what) != 0) {
                cerr_ << "  Exception: " << exception_what << std::endl;
            }
            cerr_ << SPARTA_CURRENT_COLOR_NORMAL << std::endl;
            ++num_errors_;
        }

        /*!
         * \brief Compares two files
         * \param a filename 1
         * \param b filename 2
         * \param expected Expected file equality. true expects files to match,
         * \param ignore_commented_lines Do not compare text for lines that begin with '#'
         * false expects differences.
         * \post Generates a test error if any of the following conditions are true
         * \li Either file cannot be opened
         * \li Files differ in length and exptected=true with the exception of
         * lines starting with '#'
         * \li Files differ at any position and expected=true with the exception
         * of lines starting with '#'
         * \li Files are same length and each char is identical and expected=false
         * \throw Does not throw
         *
         * Tracks line and col positions for error printouts. Newlines mode is
         * always '\n'.
         */
        void expectFilesEqual(const std::string& a, const std::string& b, bool expected, uint32_t line, const char * file, const bool ignore_commented_lines = true) {
            std::ifstream fa, fb;
            std::stringstream err;
            try{
                fa.open(a, std::ios_base::in);
            }catch(std::exception&){
            }
            if(fa.fail()){
                err.str("");
                err << "Could not open file \"" << a << "\"";
                fileComparisonFailed(a, b, line, file, err.str());
            }
            try{
                fb.open(b, std::ios_base::in);
            }catch(std::exception&){
            }
            if(fb.fail()){
                err.str("");
                err << "Could not open file \"" << b << "\"";
                fileComparisonFailed(a, b, line, file, err.str());
            }

            if(!fa.fail() && !fb.fail()){
                uint32_t line_num = 0;
                uint32_t last_line_pos = 0;
                uint64_t pos = 0;
                bool was_newline = true;
                while(1){
                    char cho, chn;
                    cho = fa.get();
                    chn = fb.get();

                    // Ignore lines starting with '#'
                    if(was_newline && ignore_commented_lines){
                        was_newline = false;
                        if(cho == '#'){
                            while(1){
                                // Read until after newline
                                cho = fa.get();
                                if(cho == '\n'){
                                    cho = fa.get();
                                    if(cho != '#'){
                                        break;
                                    }
                                }
                                ++pos; // Increment pos on this file, but not the other
                            }
                        }
                        if(chn == '#'){
                            // Read until after newline
                            while(1){
                                chn = fb.get();
                                if(chn == '\n'){
                                    chn = fb.get();
                                    if(chn != '#'){
                                        break;
                                    }
                                }
                            }
                        }
                    }

                    if(!fa.good() || !fb.good()){
                        if((fa.good() != fb.good()) && expected == true){
                            std::stringstream msg;
                            msg << "Files were different lengths: ";
                            if(!fa.good()){
                                msg << a << " was shorter than " << b << " at char '" << chn << "' #" << pos;
                            }else{
                                msg << b << " was shorter than " << a << " at char '" << cho << "' #" << pos;
                            }
                            fileComparisonFailed(a, b, line, file, msg.str());
                        }
                        break;
                    }

                    if(cho != chn){
                        err.str("");
                        err << "Files differed at pos " << pos << " (line "
                            << line_num << ", col " << pos - last_line_pos
                            <<  ") with chars: '" << cho << "' != '" << chn << "'";
                        if(expected == true){
                            fileComparisonFailed(a, b, line, file, err.str());
                        }
                        return;
                    }
                    ++pos;
                    if(cho == '\n'){ // prev char (pos-1)
                        ++line_num;
                        last_line_pos = pos; // Line starts here
                        was_newline = true;
                    }
                }

                if(expected == false){
                    fileComparisonFailed(a, b, line, file, "Files were the same");
                }
            }
        }

        void fileComparisonFailed(const std::string& a, const std::string& b, const uint32_t line, const char * file, const std::string& error) {
            cerr_ << SPARTA_CURRENT_COLOR_BRIGHT_RED << "File comparison test between \"" << a << "\" and \"" << b << "\" FAILED on line "
                      << line << " in file " << file << std::endl;
            cerr_ << "  Exception: " << error << std::endl;
            cerr_ << SPARTA_CURRENT_COLOR_NORMAL << std::endl;
            ++num_errors_;
        }

        void reachedMethod(const std::string& method_title)
        {
            methods_reached_.insert(method_title);
        }
        static SpartaTester * getInstance() {
            static SpartaTester inst;
            return & inst;
        }

        static std::unique_ptr<SpartaTester> makeTesterWithUserCError(std::ostream & cerr) {
            return std::unique_ptr<SpartaTester>(new SpartaTester(0, cerr));
        }

        static uint32_t getErrorCode(const SpartaTester * tester = getInstance()) {
            return tester->num_errors_;
        }

    private:
        SpartaTester(const uint32_t num_errors,
                   std::ostream & cerr) :
          num_errors_(num_errors),
          methods_reached_(),
          cerr_(cerr)
        {}

        uint32_t num_errors_;
        std::set<std::string> methods_reached_;
        std::ostream & cerr_;
    };


    /**
     * \def TEST_INIT
     * \brief Initialized the test.  Should be placed OUTSIDE of a
     * code block SOMEWHERE in the test source
     */
#define TEST_INIT


/**
 * \def EXPECT_REACHED()
 * \brief Add this method to be checked against whether or not it was called at least once.
 * This macro can be placed anywhere inside the function expected to be called.
 */
#define EXPECT_REACHED() sparta::SpartaTester::getInstance()->reachedMethod(__FUNCTION__)


/**
 * \def ENSURE_ALL_REACHED(x)
 * \brief make sure that the same number of methods were reached as were expected by this test.
 * \param x the number of unique methods that you expect to be reached at least once.
 * x should equal the number of times EXPECT_REACHED() is placed throughout the test.
 */
#define ENSURE_ALL_REACHED(x) sparta::SpartaTester::getInstance()->expectAllReached(x, __LINE__, __FUNCTION__)


    /**
     * \def EXPECT_TRUE(x)
     * \brief Determine if the block \a x evaluates to true
     *
     * Example usage:
     * \code
     *      if(EXPECT_TRUE(true)) {
     *          std::cout << "Wadda know, it was true!" << std::endl;
     *      }
     *      else {
     *          std::cerr << "True isn't true anymore?!?  What's this world coming to?!" << std::endl;
     *      }
     * \endcode
     */
#define EXPECT_TRUE(x) sparta::SpartaTester::getInstance()->expect((x), #x, __LINE__, __FILE__)

    /**
     * \def EXPECT_EQUAL(x,y)
     * \brief Determine if the block \a x is equal (using operator= on x) to y
     * \note Types must match. Comparison is made at run-time.
     * \pre x must be printable to cout/cerr using the insertion operator
     *
     * Value of x and y, if not equal, is printed in error message.
     *
     * Example usage:
     * \code
     *      if(EXPECT_EQUAL((const char*)x, "12345")) {
     *          std::cout << "Wadda know, it they were the same!" << std::endl;
     *      }
     *      else {
     *          std::cerr << "Values differed" << std::endl;
     *      }
     * \endcode
     */
#define EXPECT_EQUAL(x, y) sparta::SpartaTester::getInstance()->expectEqual((x), (y), true, #x, __LINE__, __FILE__)

    /**
     * \def EXPECT_NOTEQUAL(x,y)
     * \brief Determine if the block \a x is not equal (using operator= on x) to y
     * \note Types must match exactly. Comparison is made at run-time.
     * \pre x must be printable to cout/cerr using the insertion operator
     *
     * Value of x and y, if equal, is printed in error message.
     *
     * Example usage:
     * \code
     *      if(EXPECT_NOTEQUAL((const char*)x, "12345")) {
     *          std::cout << "Wadda know, they were different" << std::endl;
     *      }
     *      else {
     *          std::cerr << "Values were same" << std::endl;
     *      }
     * \endcode
     */
#define EXPECT_NOTEQUAL(x, y) sparta::SpartaTester::getInstance()->expectEqual((x), (y), false, #x, __LINE__, __FILE__)

   /**
     * \def EXPECT_WITHIN_TOLERANCE(x,y,tol)
     * \brief Determine if the block \a x is equal (using operator= on x) to y
     * within a specified tolerance
     * \note Types must match exactly. Comparison is made at run-time.
     * \pre x must be printable to cout/cerr using the insertion operator
     *
     * If x and y differ by more than (>) the specified tolerance, the x and y
     * values and the tolerance value will be printed in an error message.
     *
     * Example usage:
     * \code
     *      if(EXPECT_WITHIN_TOLERANCE(x, y, 0.02)) {
     *          std::cout << "The x and y values were within 0.02 of each other" << std::endl;
     *      }
     *      else {
     *          std::cerr << "The x and y values differed by more than 0.02" << std::endl;
     *      }
     * \endcode
     */
#define EXPECT_WITHIN_TOLERANCE(x, y, tol) sparta::SpartaTester::getInstance()->  \
    expectEqualWithinTolerance((x), (y), (tol), #x, __LINE__, __FILE__)

   /**
     * \def EXPECT_WITHIN_EPSILON(x,y)
     * \brief Determine if the block \a x is equal (using operator= on x) to y
     * within 2x the machine epsilon, as determined by std::numeric_limits<T>::epsilon()
     * \note Types must match exactly. Comparison is made at run-time.
     * \note Types compared cannot be integral.
     * \pre x must be printable to cout/cerr using the insertion operator
     *
     * If x and y differ by strictly more than (>) the specified tolerance, the x and y
     * values and the tolerance value will be printed in an error message.
     *
     * Example usage:
     * \code
     *      if(EXPECT_WITHIN_EPSILON(x, y)) {
     *          std::cout << "The x and y values were within machine epsilon of each other" << std::endl;
     *      }
     *      else {
     *          std::cerr << "The x and y values differed by more than machine epsilon" << std::endl;
     *      }
     * \endcode
     */
#define EXPECT_WITHIN_EPSILON(x, y) sparta::SpartaTester::getInstance()->  \
    expectEqualWithinTolerance((x), (y), \
        (std::numeric_limits<decltype(x)>::epsilon()), #x, __LINE__, __FILE__)

    /**
     * \def EXPECT_FALSE(x)
     * \brief Determine if the block \a x evaluates to false
     *
     * Example usage:
     * \code
     *      if(EXPECT_FALSE(false) == false) {
     *          std::cout << "Wadda know, false IS false!" << std::endl;
     *      }
     *      else {
     *          std::cerr << "False isn't false anymore?!?  What's this world coming to?!" << std::endl;
     *      }
     * \endcode
     */
#define EXPECT_FALSE(x) sparta::SpartaTester::getInstance()->expect(!(x), #x, __LINE__, __FILE__)

    /**
     * \def EXPECT_THROW(x)
     * \brief Determine if the block \a x correctly throws an exception
     *
     * Example usage:
     * \code
     *      EXPECT_THROW(throw 10);
     * \endcode
     */
#define EXPECT_THROW(x) {                                               \
        bool did_it_throw = false;                                      \
        try {x;}                                                        \
        catch(...)                                                      \
        { did_it_throw = true; }                                        \
        if(did_it_throw == false) {                                     \
            sparta::SpartaTester::getInstance()->throwTestFailed(#x, __LINE__, __FILE__); \
        }                                                               \
   }

    /**
     * \def EXPECT_THROW_MSG_SHORT(x, expected_msg)
     * \brief Determine if the block \a x correctly throws an
     *        exception with the given message (no line number/file)
     *
     * Example usage:
     * \code
     *      // The thrower:
     *      void foo() {
     *          throw sparta::SpartaException("Hello");
     *      }
     *
     *      // The message:
     *      // ex.what() -> "Hello: in file: 'blah.cpp', on line: 123"
     *
     *      EXPECT_THROW_MSG_SHORT(ex.what(), "Hello");
     * \endcode
     */
#define EXPECT_THROW_MSG_SHORT(x, expected_msg) { \
        bool did_it_throw = false;              \
        try { x; }                              \
        catch(sparta::SpartaException& ex)          \
        { did_it_throw = true;                  \
            if (strcmp(expected_msg, ex.rawReason().c_str()) != 0) {    \
                std::cerr << "Expected msg: " << expected_msg << std::endl; \
                std::cerr << "Actual msg:   " << ex.what() << std::endl; \
                sparta::SpartaTester::getInstance()->throwTestFailed(#x, __LINE__, __FILE__, ex.what()); \
            }                                   \
        }                                       \
        if(did_it_throw == false) {             \
            sparta::SpartaTester::getInstance()->throwTestFailed(#x, __LINE__, __FILE__, "did not throw"); \
        }                                       \
   }

    /**
     * \def EXPECT_THROW_MSG_LONG(x, expected_msg)
     * \brief Determine if the block \a x correctly throws an
     *        exception with the given message on the file and line number
     *
     * Example usage:
     * \code
     *      // The thrower:
     *      void foo() {
     *          throw sparta::SpartaException("Hello");
     *      }
     *
     *      // The message:
     *      // ex.what() -> "Hello: in file: 'blah.cpp', on line: 123"
     *
     *      EXPECT_THROW_MSG_LONG(ex.what(), "Hello: in file: 'blah.cpp', on line: 123");
     * \endcode
     */
#define EXPECT_THROW_MSG_LONG(x, expected_msg) { \
        bool did_it_throw = false;              \
        try { x; }                              \
        catch(sparta::SpartaException& ex)          \
        { did_it_throw = true;                  \
            if (strcmp(expected_msg, ex.what()) != 0) {    \
                std::cerr << "Expected msg: " << expected_msg << std::endl; \
                std::cerr << "Actual msg:   " << ex.what() << std::endl; \
                sparta::SpartaTester::getInstance()->throwTestFailed(#x, __LINE__, __FILE__, ex.what()); \
            }                                   \
        }                                       \
        if(did_it_throw == false) {             \
            sparta::SpartaTester::getInstance()->throwTestFailed(#x, __LINE__, __FILE__, "did not throw"); \
        }                                       \
   }

#define EXPECT_THROW_MSG_CONTAINS(x, expected_msg) {    \
        bool did_it_throw = false;              \
        try { x; }                              \
        catch(sparta::SpartaException& ex)          \
        { did_it_throw = true;                  \
            if (boost::algorithm::contains(ex.what(), expected_msg) != true) { \
                std::cerr << "Expected msg: " << expected_msg << std::endl; \
                std::cerr << "Actual msg:   " << ex.what() << std::endl; \
                sparta::SpartaTester::getInstance()->throwTestFailed(#x, __LINE__, __FILE__, ex.what()); \
            }                                   \
        }                                       \
        if(did_it_throw == false) {             \
            sparta::SpartaTester::getInstance()->throwTestFailed(#x, __LINE__, __FILE__, "did not throw"); \
        }                                       \
   }




    /**
     * \def EXPECT_NOTHROW(x)
     * \brief Determine if the block \a x throws an exception incorrectly
     *
     * Example usage:
     * \code
     *      EXPECT_NOTHROW(throw 10);
     * \endcode
     */
#define EXPECT_NOTHROW(x) {                                             \
        bool did_it_throw = false;                                      \
        std::string exception_what;                                     \
        try { x; }                                                      \
        catch(std::exception& e)                                        \
        { did_it_throw = true;                                          \
            exception_what = e.what();                                  \
        }                                                               \
        catch(...)                                                      \
        { did_it_throw = true; }                                        \
        if(did_it_throw == true) {                                      \
            sparta::SpartaTester::getInstance()->throwTestFailed(#x, __LINE__, __FILE__, exception_what.c_str()); \
        }                                                               \
    }

    /**
     * \def EXPECT_FILES_EQUAL(a, b)
     * \brief Determine if the block \a a and \a b contain the same exact data
     * with the exception of lines beginning with '#'
     *
     * Example usage:
     * \code
     *      EXPECT_FILES_EQUAL("a.out.golden", "b.out");
     * \endcode
     */
#define EXPECT_FILES_EQUAL(a, b) {                                      \
        sparta::SpartaTester::getInstance()->expectFilesEqual(a, b, true, __LINE__, __FILE__); \
    }

    /**
     * \def EXPECT_FILES_NOTEQUAL(a, b)
     * \brief Determine if the block \a a and \a b contain different data
     * with the exception of lines beginning with '#'
     *
     * Example usage:
     * \code
     *      EXPECT_FILES_NOTEQUAL("a.out.golden", "b.out");
     * \endcode
     */
#define EXPECT_FILES_NOTEQUAL(a, b) {                                   \
        sparta::SpartaTester::getInstance()->expectFilesEqual(a, b, false, __LINE__, __FILE__); \
    }

    /**
     * \def ERROR_CODE
     * \brief The number of errors found in the testing
     */
#define ERROR_CODE sparta::SpartaTester::getErrorCode()

    /**
     * \def REPORT_ERROR
     * \brief Prints the error code with a nice pretty message
     * \note This is separate from returning ERROR_CODE, which must be done
     * after this macro. See the example for sparta::SpartaTester . The reason for
     * this separation is so that errors can be reported before teardown, which
     * could fail uncatchably (i.e. segfault) when there are caught errors
     * earlier in the test (e.g. dangling pointers).
     *
     * Example:
     * \code
     * int main() }
     *    // ...
     *    REPORT_ERROR;
     *
     *    performDangerousCleanup();
     *
     *    return ERROR_CODE;
     * }
     * \endcode
     */
#define REPORT_ERROR                                                    \
    if(ERROR_CODE != 0) {                                               \
        std::cout << std::dec << "\n" << SPARTA_UNMANAGED_COLOR_BRIGHT_RED << ERROR_CODE \
                  << " ERROR(S) found during test.\n" << SPARTA_UNMANAGED_COLOR_NORMAL << std::endl; \
    } else {                                                            \
        std::cout << std::dec << "\n"                                   \
                  << "TESTS PASSED -- No errors found during test.\n" << std::endl; \
    }
}

// __SPARTA_TESTER_H__
#endif
