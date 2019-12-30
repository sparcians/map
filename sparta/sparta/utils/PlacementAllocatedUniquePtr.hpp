// <PlacementAllocatedUniquePtr> -*- C++ -*-

#ifndef __PLACEMENT_ALLOCATED_UNIQUE_PTR__
#define __PLACEMENT_ALLOCATED_UNIQUE_PTR__

#include <memory>

namespace sparta
{
    /*!
     * \brief Class which uses placement new to force the location of a
     * contained object. The main goal is to get the address of some object
     * that is about to be constructed
     */
    template <typename T>
    class PlacementAllocatedUniquePtr
    {
    public:

        typedef void (*dtor_func)(void);
        typedef T* ptr_type;
        typedef T const * const_ptr_type;
        typedef T& ref_type;
        typedef T const & const_ref_type;

        PlacementAllocatedUniquePtr(dtor_func dtor) :
            obj_(nullptr),
            mem_(new uint8_t[sizeof(T)]),
            dtor_(dtor)
        { }

        PlacementAllocatedUniquePtr() :
            PlacementAllocatedUniquePtr(nullptr)
        { }

        template<typename... _Args>
        PlacementAllocatedUniquePtr(_Args&&... __args, dtor_func dtor=nullptr) :
            PlacementAllocatedUniquePtr(dtor)
        {
            reallocate(__args...);
        }

        ~PlacementAllocatedUniquePtr() {
            deallocate();

            delete [] mem_;

            //! \todo Overwrite mem_ with zeroes in debug mode
        }

        /*!
         * \brief Returns the address of the memory held within this object.
         * This address is constant for this object's lifetime.
         * \note This is useful to get the address of some object that is
         * about to be allocated
         * \warning The fact that this returns a non-null value does not
         * imply that the object is allocated.
         * \see isAllocated
         * \see operator*
         */
        ptr_type getAddress() {
            return reinterpret_cast<ptr_type>(mem_);
        }

        ref_type operator*() {
            return *obj_;
        }

        const_ref_type operator*() const {
            return *obj_;
        }

        ptr_type operator->() {
            return obj_;
        }

        const_ptr_type operator->() const {
            return obj_;
        }

        ptr_type get() {
            return obj_;
        }

        const_ptr_type get() const {
            return obj_;
        }

        bool operator==(const_ptr_type rhp) const {
            return obj_ == rhp;
        }

        bool operator!=(const_ptr_type rhp) const {
            return obj_ != rhp;
        }

        bool operator>(const_ptr_type) = delete;
        bool operator>=(const_ptr_type) = delete;
        bool operator<(const_ptr_type) = delete;
        bool operator<=(const_ptr_type) = delete;

        template<typename... _Args>
        ptr_type reallocate(_Args&&... __args) {
            deallocate();
            obj_ = new (mem_) T(__args...);
            return obj_;
        }

        void deallocate() {
            if(obj_ != nullptr){
                obj_->~T();
            }
            if(dtor_ != nullptr){
                dtor_(); // Invoked destructor callback
            }
        }

    private:

        /*!
         * \brief T object contained (if allocated)
         */
        ptr_type obj_;

        /*!
         * \brief Memory in which allocated object will be placed
         */
        uint8_t * const mem_;

        /*!
         * \brief Destructor function pointer
         */
        dtor_func dtor_;
    };
} // namespace sparta

#endif // __PLACEMENT_ALLOCATED_UNIQUE_PTR__
