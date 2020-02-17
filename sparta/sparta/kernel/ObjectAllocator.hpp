// <ObjectAllocator.h> -*- C++ -*-


/**
 * \file   ObjectAllocator.hpp
 *
 * \brief  File that defines the ObjectAllocator class
 */

#ifndef __OBJECT_ALLOCATOR_H__
#define __OBJECT_ALLOCATOR_H__

#include <inttypes.h>
#include <queue>
#include <vector>
#include <limits>
#include <memory>
#include "sparta/utils/SpartaAssert.hpp"
#include <inttypes.h>


namespace sparta
{
    template<typename ObjT>
    class ObjectAllocator
    {
    private:
        typedef std::unique_ptr<ObjT> SmartPtr;
        std::queue<ObjT*> free_obj_list_;
        std::vector<SmartPtr> allocated_objs_;
    public:
        ~ObjectAllocator()
        {
        }
        //allow direct access to the allocator via create and Free
        template<typename... Args>
        ObjT * create(Args&&... args)
        {
            ObjT * obj = 0;
            if(free_obj_list_.empty()) {
                allocated_objs_.emplace_back(new ObjT(args...));
                obj = allocated_objs_.back().get();
            }
            else {
                obj = free_obj_list_.front();
                free_obj_list_.pop();
            }
            return obj;
        }

        //When the obj is finished, it puts itself back on the free
        //list
        void free(ObjT * obj) {
            free_obj_list_.push(obj);
        }

        void clear() {
            allocated_objs_.clear();
            free_obj_list_ = std::queue<ObjT *>();
        }

        //Make this allocator work as a stl compliant allocator
    // public:
    //     typedef ObjT value_type;
    //     typedef ObjT* pointer;
    //     typedef const value_type* const_pointer;
    //     typedef value_type& reference;
    //     typedef const value_type& const_reference;
    //     typedef std::size_t size_type;
    //     typedef std::ptrdiff_t difference_type;

    //     //convert to a different template allocator???
    //     template<typename U>
    //     struct rebind{
    //         typedef ObjectAllocator<U> other;
    //     };

    //     //Allow construction of the ObjectAllocator
    //     explicit ObjectAllocator() {}
    //     ///Destruct
    //     ~ObjectAllocator()
    //     {
    //         //nothing to do since UniquePtrs were used. :)
    //         allocated_objs_.clear();
    //     }
    //     ///Allow copy construction
    //     explicit ObjectAllocator(ObjectAllocator<ObjT> const&){}
    //     template<typename U>
    //     inline explicit ObjectAllocator(ObjectAllocator<U> const&) {}

    //     //allow support to get the address of objects.
    //     pointer address(reference r) { return &r; }
    //     const_pointer address(const_reference r) { return &r; }


    //     ///either new up some memory or use some of our already created memory.
    //     pointer allocate(size_type size, typename std::allocator<void>::const_pointer = 0)
    //     {
    //         std::cout << "allocating memory" << std::endl;
    //         pointer p;
    //         if(free_obj_list_.empty())
    //         {
    //             //i don't think size should ever be greater than 1, if it is...
    //             //then i don't know what's going on.
    //             sparta_assert(size == 1);
    //             allocated_objs_.emplace_back(SmartPtr(new char[sizeof(ObjT)]));
    //             p = reinterpret_cast<pointer>(allocated_objs_.back().get());
    //         }
    //         else
    //         {
    //             p = free_obj_list_.front();
    //             free_obj_list_.pop();
    //         }
    //         return p;
    //     }

    //     ///instead of deleting the memory, store a pointer to the memory
    //     ///in a list of free memory.
    //     void deallocate(pointer p, size_type)
    //     {
    //         std::cout << "deallocating by pushing to free list" << std::endl;
    //         free_obj_list_.push(p);
    //     }

    //     ///max size?
    //     size_type max_size() const
    //     {
    //         return std::numeric_limits<size_type>::max() / sizeof(ObjT);
    //     }

    //     ///construct and destruct objects.
    //     void construct(pointer p, const ObjT& t)
    //     {
    //         //construct the object implace at p's location?
    //         new (p) ObjT(t);
    //     }
    //     ///destruct an object.
    //     void destroy(pointer p)
    //     {
    //         (void)p;
    //         //there's nothing to do since we are going to keep the memory at p around.
    //         //we don't care to destruct the object even? sounds ugly.
    //     }

    //     bool operator==(ObjectAllocator<ObjT> const&) { return false;}
    //     bool operator!=(ObjectAllocator<ObjT> const& a) {return !operator==(a); }

    // public:
    };
}
#endif
