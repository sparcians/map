// <StaticInit> -*- C++ -*-

/*!
 * \file StaticInit.hpp
 * \brief Helpers for enforcing StaticInitialization order
 * \see SpartaStaticInitializer
 */

#ifndef __STATIC_INIT_H__
#define __STATIC_INIT_H__

#include <iostream>

namespace sparta
{

    /*!
     * \brief Static-initialization order controller
     *
     * Files that include this header will have an instance of this class
     * in their compilation units, which will increment an internal refcount
     * which forces initialization of statics in TreeNode at that time and
     * delays deletion of TreeNode statics until the last instance of this class
     * is finally destroyed in the last complation unit
     *
     * For this to function correctly, any files attempting to use the
     * StringManager during static construction or destruction must [indirectly]
     * include this header.
     *
     * Each class that wants its static initialization order controlled must be
     * have this class as a friend and then add add new/delete statements in the
     * constructor/destructor of this class respectively (sparta.cpp). The header
     * of each class using this class MUST include this header so that they each
     * automatically instantiate a static instance of this class
     * (_SpartaStaticInitializer).
     */
    static class SpartaStaticInitializer {
    public:
        SpartaStaticInitializer();
        ~SpartaStaticInitializer();
    } _SpartaStaticInitializer;

} // namespace sparta

#endif // #ifdef __STATIC_INIT_H__
