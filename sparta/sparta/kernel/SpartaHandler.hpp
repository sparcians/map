// <SpartaHandler.hpp> -*- C++ -*-

/**
 * \file   SpartaHandler.hpp
 * \brief  File that contains the macro used to generate the class callbacks.
 */

#pragma once

#include <iostream>
#include <cinttypes>
#include <limits>

namespace sparta
{
#ifndef DO_NOT_DOCUMENT
    /**
     * \class SpartaHandler
     *
     * Based on the delegate concept document
     * http://www.codeproject.com/Articles/11015/The-Impossibly-Fast-C-Delegates
     * by Sergey Ryazanov
     *
     * This class creates a callback delegate for a class method in very
     * efficient and wildly awesome manner.
     *
     * Don't use this class directly: use the macros provided at the
     * end of this file.
     */
    class SpartaHandler
    {
    public:
        friend std::ostream & operator<<(std::ostream & os, const SpartaHandler & md);

        /*!
         * \brief Not default-constructable
         */
        SpartaHandler() = delete;

        /*!
         * \brief Named constructor
         */
        explicit SpartaHandler(const char * name) :
            object_ptr(nullptr),
            clear_ptr(&do_nothing_method_stub),
            stub_ptr(nullptr),
            stub_ptr_1(nullptr),
            stub_ptr_2(nullptr),
            arg_count_(std::numeric_limits<uint32_t>::max()),
            name_(name)
        {}

        /*!
         * \brief Copy Constructor
         */
        SpartaHandler(const SpartaHandler& rhp) = default;

        //! Assignment operations
        SpartaHandler& operator=(const SpartaHandler&) = default;

        template <class T, void (T::*TMethod)()>
        static SpartaHandler from_member(T* object_ptr, const char * name = "")
        {
            SpartaHandler d(name);
            d.object_ptr = object_ptr;
            d.stub_ptr = &method_stub<T, TMethod>;
            d.arg_count_ = 0;
            return d;
        }

        template <class T, void (T::*TMethod)() const>
        static SpartaHandler from_member(T* object_ptr, const char * name = "")
        {
            SpartaHandler d(name);
            d.object_ptr = object_ptr;
            d.stub_ptr = &method_stub<T, TMethod>;
            d.arg_count_ = 0;
            return d;
        }

        template <class T, void (T::*TMethod)(), void (T::*TMethodClear)()>
        static SpartaHandler from_member_clear(T* object_ptr, const char * name = "")
        {
            SpartaHandler d(name);
            d.object_ptr = object_ptr;
            d.stub_ptr = &method_stub<T, TMethod>;
            d.clear_ptr = &method_stub<T, TMethodClear>;
            d.arg_count_ = 0;
            return d;
        }

        template <class T, void (T::*TMethod)() const, void (T::*TMethodClear)()>
        static SpartaHandler from_member_clear(T* object_ptr, const char * name = "")
        {
            SpartaHandler d(name);
            d.object_ptr = object_ptr;
            d.stub_ptr = &method_stub<T, TMethod>;
            d.clear_ptr = &method_stub<T, TMethodClear>;
            d.arg_count_ = 0;
            return d;
        }

        template <class T, class DataT, void (T::*TMethod)(const DataT &)>
        static SpartaHandler from_member_1(T* object_ptr, const char * name = "")
        {
            SpartaHandler d(name);
            d.object_ptr = object_ptr;
            d.stub_ptr_1 = &method_stub_1<T, DataT, TMethod>;
            d.arg_count_ = 1;
            return d;
        }

        template <class T, class DataT, void (T::*TMethod)(const DataT &) const>
        static SpartaHandler from_member_1(T* object_ptr, const char * name = "")
        {
            SpartaHandler d(name);
            d.object_ptr = object_ptr;
            d.stub_ptr_1 = &method_stub_1<T, DataT, TMethod>;
            d.arg_count_ = 1;
            return d;
        }

        template <class T, class DataT, class DataTwo, void (T::*TMethod)(const DataT &, const DataTwo&)>
        static SpartaHandler from_member_2(T* object_ptr, const char * name = "")
        {
            SpartaHandler d(name);
            d.object_ptr = object_ptr;
            d.stub_ptr_2 = &method_stub_2<T, DataT, DataTwo, TMethod>;
            d.arg_count_ = 1;
            return d;
        }

        template <class T, class DataT, class DataTwo, void (T::*TMethod)(const DataT &, const DataTwo&) const>
        static SpartaHandler from_member_2(T* object_ptr, const char * name = "")
        {
            SpartaHandler d(name);
            d.object_ptr = object_ptr;
            d.stub_ptr_2 = &method_stub_2<T, DataT, DataTwo, TMethod>;
            d.arg_count_ = 1;
            return d;
        }

        void operator()() const
        {
            (*stub_ptr)(object_ptr);
        }

        // Can this be changed to a template DataT and the void cast
        // be hidden in the function?
        void operator()(const void * dat) const
        {
            (*stub_ptr_1)(object_ptr, dat);
        }

        void operator()(const void* dat, const void* dat_two) const
        {
            (*stub_ptr_2)(object_ptr, dat, dat_two);
        }

        template<class ObjT>
        void setObjectPtr(ObjT * obj) {
            object_ptr = (void *)obj;
        }

        template<class ObjT>
        ObjT* getObjectPtr() {
            return static_cast<ObjT*>(object_ptr);
        }

        void clear() {
            (*clear_ptr)(object_ptr);
        }

        uint32_t argCount() const {
            return arg_count_;
        }

        const char * getName() const {
            return name_;
        }

        void setName(const char * name) {
            name_ = name;
        }

        bool operator==(const SpartaHandler & rhs) const {
            return (std::string(name_) == rhs.name_);
        }

        bool operator!=(const SpartaHandler & rhs) const {
            return !operator==(rhs);
        }

        template<class ObjT>
        bool operator==(ObjT* obj) const {
            return object_ptr == static_cast<void*>(obj);
        }

        template<class ObjT>
        bool operator!=(ObjT* obj) const {
            return !operator==(obj);
        }

        operator bool() const {
            return (object_ptr != nullptr);
        }

    private:

        typedef void (*stub_type)(void* object_ptr);
        typedef void (*stub_type_1)(void* object_ptr, const void*);
        typedef void (*stub_type_2)(void* object_ptr, const void*, const void*);

        void* object_ptr;
        stub_type clear_ptr;
        stub_type stub_ptr;
        stub_type_1 stub_ptr_1;
        stub_type_2 stub_ptr_2;

        uint32_t arg_count_;

        const char * name_;

        static void do_nothing_method_stub(void*)
        { }

        template <class T, void (T::*TMethod)()>
        static void method_stub(void* object_ptr)
        {
            T* p = static_cast<T*>(object_ptr);
            (p->*TMethod)();
        }

        template <class T, void (T::*TMethod)() const>
        static void method_stub(void* object_ptr)
        {
            T* p = static_cast<T*>(object_ptr);
            (p->*TMethod)();
        }

        template <class T, class DataT, void (T::*TMethod)(const DataT &)>
        static void method_stub_1(void* object_ptr, const void * dat)
        {
            T* p = static_cast<T*>(object_ptr);
            const DataT & data = *(static_cast<const DataT *>(dat));
            (p->*TMethod)(data);
        }

        template <class T, class DataT, void (T::*TMethod)(const DataT &) const>
        static void method_stub_1(void* object_ptr, const void * dat)
        {
            T* p = static_cast<T*>(object_ptr);
            const DataT & data = *(static_cast<const DataT *>(dat));
            (p->*TMethod)(data);
        }

        template <class T, class DataT, class DataTwo, void (T::*TMethod)(const DataT &, const DataTwo&)>
        static void method_stub_2(void* object_ptr, const void * dat, const void * dat_two)
        {
            T* p = static_cast<T*>(object_ptr);
            const DataT & data = *(static_cast<const DataT *>(dat));
            const DataTwo & data_two = *(static_cast<const DataTwo *>(dat_two));
            (p->*TMethod)(data, data_two);
        }

        template <class T, class DataT, class DataTwo, void (T::*TMethod)(const DataT &, const DataTwo&) const>
        static void method_stub_2(void* object_ptr, const void * dat, const void * dat_two)
        {
            T* p = static_cast<T*>(object_ptr);
            const DataT & data = *(static_cast<const DataT *>(dat));
            const DataTwo & data_two = *(static_cast<const DataTwo *>(dat_two));
            (p->*TMethod)(data, data_two);
        }
    };

    inline std::ostream & operator<<(std::ostream & os, const SpartaHandler & md) {
        os << md.name_;
        return os;
    }
#endif

    /*!
     * \def CREATE_SPARTA_HANDLER(clname, meth)
     *
     * Creates a \c SpartaHandler type given the class name (\c
     * clname) and a class method (\c meth).
     *
     * This method requires a class instance (expects \c this to be
     * defined and of type \c clname).  The class method given must
     * match the following signature:
     *
     * \code
     * void clname::meth();
     * \endcode
     *
     * Example:
     *
     * \code
     * class MyClass
     * {
     * public:
     *
     *     MyClass() {
     *         sparta::SpartaHandler handler =
     *             CREATE_SPARTA_HANDLER(MyClass, myMethod);
     *         // ... do something with the handle
     *         handler();
     *     }
     *
     *     void myMethod() {
     *         std::cout << "myMethod called" << std::endl;
     *     }
     *
     * };
     * \endcode
     */
#define CREATE_SPARTA_HANDLER(clname, meth)                            \
    sparta::SpartaHandler::from_member<clname, &clname::meth>            \
    (this, #clname"::"#meth"()")

    /*!
     * \def CREATE_SPARTA_HANDLER_WITH_CLEAR(clname, meth, clear)
     *
     * Creates a \c SpartaHandler type given the class name (\c
     * clname), a class method (\c meth), and a clear method (\c clear).
     *
     * This method requires a class instance (expects \c this to be
     * defined and of type \c clname).  The class method and clear
     * method given must match the following signature:
     *
     * \code
     * void clname::meth();
     * void clname::clear();
     * \endcode
     *
     * This handler type allows the sparta::Scheduler to "clear" the
     * handle that was previously set.  This is useful for nullifying
     * a handler on the scheduler.
     *
     * Example:
     *
     * \code
     * class MyClass
     * {
     * public:
     *
     *     MyClass() {
     *         sparta::SpartaHandler handler =
     *             CREATE_SPARTA_HANDLER_WITH_CLEAR(MyClass, myMethod, myClear);
     *         // ... do something with the handle
     *         handler();
     *     }
     *
     *     void myMethod() {
     *         std::cout << "myMethod called" << std::endl;
     *     }
     *
     *     void myClear() {
     *         std::cout << "myClear called" << std::endl;
     *     }
     * };
     * \endcode
     */
#define CREATE_SPARTA_HANDLER_WITH_CLEAR(clname, meth, clear)                    \
    sparta::SpartaHandler::from_member_clear<clname, &clname::meth, &clname::clear>\
    (this, #clname"::"#meth"()")

    /*!
     * \def CREATE_SPARTA_HANDLER_WITH_OBJ(clname, obj, meth)
     *
     * Creates a \c SpartaHandler type given the class name (\c
     * clname), a class instance (\c obj) and a class method (\c meth).
     *
     * This method requires a class instance to be given.  The class
     * method given must match the following signature:
     *
     * \code
     * void clname::meth();
     * \endcode
     *
     * Example:
     *
     * \code
     * class MyClass
     * {
     * public:
     *
     *     MyClass() {
     *         sparta::SpartaHandler handler =
     *             CREATE_SPARTA_HANDLER_WITH_OBJ(MyClass, this, myMethod);
     *         // ... do something with the handle
     *         handler();
     *     }
     *
     *     void myMethod() {
     *         std::cout << "myMethod called" << std::endl;
     *     }
     * };
     * \endcode
     */
#define CREATE_SPARTA_HANDLER_WITH_OBJ(clname, obj, meth)       \
    sparta::SpartaHandler::from_member<clname, &clname::meth>     \
    (obj, #clname"::"#meth"()")

    /*!
     * \def CREATE_SPARTA_HANDLER_WITH_DATA(clname, meth, dataT)
     *
     * Creates a \c SpartaHandler type given the class name (\c clname),
     * a class method (\c meth), and the data type that method expects
     * (\c dataT).
     *
     * This method requires a class instance (expects \c this to be
     * defined and of type \c clname).  The class method given must
     * match the following signature:
     *
     * \code
     * void clname::meth(const dataT &);
     * \endcode
     *
     * Example:
     *
     * \code
     * class MyClass
     * {
     * public:
     *
     *     MyClass() {
     *         sparta::SpartaHandler handler =
     *             CREATE_SPARTA_HANDLER_WITH_DATA(MyClass, myMethod, uint32_t);
     *         // ... do something with the handle
     *         handler(10);
     *     }
     *
     *     void myMethod(const uint32_t & dat) {
     *         std::cout << "myMethod called: " << dat << std::endl;
     *     }
     *
     * };
     * \endcode
     */
#define CREATE_SPARTA_HANDLER_WITH_DATA(clname, meth, dataT)           \
    sparta::SpartaHandler::from_member_1<clname, dataT, &clname::meth>   \
    (this, #clname"::"#meth"("#dataT")")

    /*!
     * \def CREATE_SPARTA_HANDLER_WITH_TWO_DATA(clname, meth, dataOne, dataTwo)
     *
     * Creates a \c SpartaHandler type given the class name (\c clname),
     * a class method (\c meth), and two arguments (\c dataOne and \c
     * dataTwo).
     *
     * This method requires a class instance (expects \c this to be
     * defined and of type \c clname).  The class method given must
     * match the following signature:
     *
     * \code
     * void clname::meth(const dataOne&, const dataTwo &);
     * \endcode
     *
     * Example:
     *
     * \code
     * class MyClass
     * {
     * public:
     *
     *     MyClass() {
     *         sparta::SpartaHandler handler =
     *             CREATE_SPARTA_HANDLER_WITH_TWO_DATA(MyClass, myMethod, uint32_t, std::string);
     *         // ... do something with the handle
     *         handler(10, "hello");
     *     }
     *
     *     void myMethod(const uint32_t & dat, const std::string & msg) {
     *         std::cout << "myMethod called " << dat << msg << std::endl;
     *     }
     * };
     * \endcode
     */
#define CREATE_SPARTA_HANDLER_WITH_TWO_DATA(clname, meth, dataOne, dataTwo) \
        sparta::SpartaHandler::from_member_2<clname, dataOne, dataTwo, &clname::meth> \
        (this, #clname"::"#meth"("#dataOne","#dataTwo")")

    /*!
     * \def CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(clname, obj, meth, dataT)
     *
     * Creates a \c SpartaHandler type given the class name (\c clname),
     * class instance (\c obj), a class method (\c meth), and the
     * parameter data (\c dataT).
     *
     * This method requires a class instance to be given.  The class
     * method given must match the following signature:
     *
     * \code
     * void clname::meth(const dataT &);
     * \endcode
     *
     * Example:
     *
     * \code
     * class MyClass
     * {
     * public:
     *
     *     MyClass() {
     *         sparta::SpartaHandler handler =
     *             CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(MyClass, this, myMethod, uint32_t);
     *         // ... do something with the handle
     *         handler();
     *     }
     *
     *     void myMethod(const uint32_t & dat) {
     *         std::cout << "myMethod called: " << dat << std::endl;
     *     }
     * };
     * \endcode
     */
#define CREATE_SPARTA_HANDLER_WITH_DATA_WITH_OBJ(clname, obj, meth, dataT) \
    sparta::SpartaHandler::from_member_1<clname, dataT, &clname::meth>   \
    (obj, #clname"::"#meth"("#dataT")")
}
