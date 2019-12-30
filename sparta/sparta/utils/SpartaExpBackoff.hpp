// <SpartaExpBackoff> -*- C++ -*-

/**
 * \brief Macros for handling exponential backoff
 */

#ifndef __SPARTA_EXP_BACKOFF_H__
#define __SPARTA_EXP_BACKOFF_H__


namespace sparta {

    /*!
     * \brief Contains utilities and types of exponential backoff helpers
     */
    namespace exp_backoff {

/*!
 * \brief Type use for exponential backoff counters
 */
typedef uint32_t counter_t;

/*!
 * \brief Helper for SPARTA_EXPONENTIAL_BACKOFF. Concatenates some tokens
 */
#define SPARTA_EXPBO_CONCAT_ARG2(x,y,z) x##_##y##_##z

/*!
 * \brief Helper for SPARTA_EXPONENTIAL_BACKOFF. Expands some tokens (e.g.
 * __LINE__)
 * and forwards them to SPARTA_EXPBO_CONCAT_ARG2 for concatenation
 */
#define SPARTA_EXPBO_CONCAT_ARG(x,y,z) SPARTA_EXPBO_CONCAT_ARG2(x,y,z)

/*!
 * \brief Macro for applying exponential backoff to some action
 * \param mult Backoff multiplier. The first time this an instance of this macro
 * is encountered, it will perform \a action. Then it will wait wait \a mult
 * times (mult^1) before printing the message again, then mult^2, then mult^3
 * and so on until the local counter variable (sparta::exp_backoff::counter_t)
 * overflows .
 * \param action Action to perfom This is an arbitrary chunk of code. Note that
 * a local sparta::exp_backoff::counter_t reference will be available within
 * \a action caled SPARTA_EXPONENTIAL_BACKOFF_COUNT; This could be used go
 * generate a log message, for example, to show the occurrance count.
 * \warning Do not use this macro in a scope where there is a variable called
 * SPARTA_EXPONENTIAL_BACKOFF_COUNT (all caps).
 * \note This is implemented using local static variables whose names are
 * defined using the __LINE__ macro. Compilers not supporting this macro are
 * expected to be unable to use this macro multiple times within any scope, but
 * this is untested.
 */
#define SPARTA_EXPONENTIAL_BACKOFF(mult, action)                                            \
    static sparta::exp_backoff::counter_t SPARTA_EXPBO_CONCAT_ARG(_count,__LINE__,mult) = 0;  \
    static sparta::exp_backoff::counter_t SPARTA_EXPBO_CONCAT_ARG(_next,__LINE__,mult) = 1;   \
    SPARTA_EXPBO_CONCAT_ARG(_count,__LINE__,mult) ++;                                       \
    if(SPARTA_EXPBO_CONCAT_ARG(_count,__LINE__,mult) >= SPARTA_EXPBO_CONCAT_ARG(_next,__LINE__,mult)){ \
        const sparta::exp_backoff::counter_t& SPARTA_EXPONENTIAL_BACKOFF_COUNT = SPARTA_EXPBO_CONCAT_ARG(_count,__LINE__,mult); \
        (void) SPARTA_EXPONENTIAL_BACKOFF_COUNT;                                            \
        action;                                                                           \
        SPARTA_EXPBO_CONCAT_ARG(_next,__LINE__,mult) *= mult;                               \
    }

    } // namespace exp_backoff
} // namespace sparta

#endif // __SPARTA_EXP_BACKOFF_H__
