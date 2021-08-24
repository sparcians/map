// <SpartaSharedPointer.hpp> -*- C++ -*-

/**
 * \file   SpartaSharedPointer.hpp
 * \brief  Defines the SpartaSharedPointer class used for garbage collection
 *
 */

#pragma once

#include <cinttypes>
#include <cassert>
#include <type_traits>

#include "sparta/utils/Utils.hpp"
#include "sparta/utils/MetaStructs.hpp"
#include "sparta/utils/SpartaSharedPointerBaseAllocator.hpp"

namespace sparta
{
    // Forward declaration
    template<class PointerT>
    class SpartaSharedPointerAllocator;

    /**
     * \class SpartaSharedPointer
     * \brief Used for garbage collection, will delete the object it
     *        points to when all objects are finished using it
     *
     * Simple, thread-\a unsafe class that will delete memory it
     * points to when the last memory reference is deconstructed.
     *
     * This class was created from the use case of typical model
     * development: lots and lots of small, shared objects flowing
     * through the simulation.  Allocation/deallocation of
     * `std::shared_ptr` is a solution as well as Boost's
     * `shared_ptr`, but they serve a more general purpose and follow
     * the principles of allocation traits and thread safety.  This
     * class does not.  Because of the differentiated use case, this
     * class is *much* faster (up to 6x faster) than using
     * `std::shared_ptr`.
     *
     * In addition, the concept of a weak pointer is also supported
     * via the SpartaWeakPointer found nested in this class.
     *
     * Example usage:
     * \code
     * void foo()
     * {
     *    sparta::SpartaSharedPointer<int> ptr(new int);
     *    int * t = new int(0);   // Don't do this, BTW
     *    sparta::SpartaSharedPointer<int> ptr2(t);
     *
     *    ptr2 = ptr;
     *    *ptr = 5;
     *    sparta_assert(*ptr2 == 5);
     *
     *    sparta::SpartaSharedPointer<int> ptr3(new int(10));
     *    sparta_assert(*ptr3 == 10);
     *
     *    sparta::SpartaSharedPointer<int>::SpartaWeakPointer wp = ptr3;
     *    sparta_assert(false == wp.expired());
     *    sparta_assert(1 == wp.use_count());
     *    ptr3.reset();
     *    sparta_assert(true == wp.expired());
     *    sparta_assert(0 == wp.use_count());
     *
     *    // Don't delete!
     *}
     * \endcode
     *
     * This class can be used independently, or more efficiently with
     * sparta::allocate_sparta_shared_pointer<T>.  See
     * sparta::SpartaSharedPointerAllocator for more information.
     */
    template <class PointerT>
    class SpartaSharedPointer
    {
    public:
        class SpartaWeakPointer;

        template<class PointerT2>
        friend class SpartaSharedPointer;

    private:
        /// Internal structure to keep track of the reference
        ///
        /// If the RefCount contains a pointer to a mem_block, it does
        /// not own memory to PointerT nor is it responsible for
        /// deallocating it.
        struct RefCount
        {
            RefCount(PointerT * _p, void * mem_block) :
                p(_p), mem_block(mem_block)
            {}

            explicit RefCount(PointerT * _p) :
                RefCount(_p, nullptr)
            {}

            // Small cleanup -- set to nullptr
            ~RefCount() { p = nullptr; }

            int32_t count   {1};
            int32_t wp_count{0}; // For weakpointers
            PointerT * p = nullptr;
            void     * mem_block = nullptr;
        };

    public:
        //! Expected typedef for the resource type.
        using element_type = PointerT;

        /**
         * \brief Construct a Reference Pointer with the given memory object
         * \param p The memory that this reference pointer will take ownership of
         *
         * Construct a new Reference Pointer and take initial
         * ownership of the given object pointer.
         */
        explicit SpartaSharedPointer(PointerT * p = nullptr) noexcept :
            ref_count_(p == nullptr ? nullptr : new RefCount(p))
        {}

        /**
         * \brief Constructor for SpartaSharedPointer<T> ptr = nullptr;
         * \param nullptr_t
         */
        constexpr SpartaSharedPointer(std::nullptr_t) noexcept :
            ref_count_(nullptr)
        {}

        /**
         * \brief Construct a reference pointer given another implicitly convertable reference pointer
         * \param r The other reference pointer
         *
         * The two reference pointers now share the common memory but the newly
         * constructed reference pointer will interpret it as a different class
         *
         */
        template<class PointerT2>
        SpartaSharedPointer(const SpartaSharedPointer<PointerT2>& orig) noexcept :
            ref_count_((SpartaSharedPointer<PointerT>::RefCount*)orig.ref_count_)
        {
            static_assert(std::is_base_of<PointerT, PointerT2>::value == true,
                "Only upcasting (derived class -> base class) of SpartaSharedPointer is supported!");
            static_assert(std::has_virtual_destructor<PointerT>::value == true,
                "Base class must have a virtual destructor defined to support upcasting!");
            if(SPARTA_EXPECT_TRUE(ref_count_ != nullptr)) {
                ++ref_count_->count;
            }
        }

        /**
         * \brief Construct a reference pointer given another reference pointer
         * \param orig The original pointer
         *
         * The two reference pointers now share the common memory
         */
        SpartaSharedPointer(const SpartaSharedPointer & orig) :
            ref_count_(orig.ref_count_)
        {
            if(SPARTA_EXPECT_TRUE(ref_count_ != nullptr)) {
                ++ref_count_->count;
            }
        }

        /**
         * \brief Take ownership of a reference pointer
         * \param orig The original pointer
         *
         * This SpartaSharedPointer now replaces orig.  Orig becomes a
         * nullptr with no references
         */
        SpartaSharedPointer(SpartaSharedPointer && orig) :
            ref_count_(orig.ref_count_)
        {
            // DO NOT unlink.
            orig.ref_count_ = nullptr;
        }

        //! \brief Detach this shared pointer; if last, delete underlying object
        ~SpartaSharedPointer() {
            unlink_();
        }

        /**
         * \brief Assignment operator
         * \param orig The original pointer
         * \return Reference to this
         *
         * The two reference pointers now share the common memory
         */
        SpartaSharedPointer & operator=(const SpartaSharedPointer & orig)
        {
            sparta_assert(&orig != this);
            unlink_();
            ref_count_ = orig.ref_count_;
            if(SPARTA_EXPECT_TRUE(ref_count_ != nullptr)) {
                ++ref_count_->count;
            }
            return *this;
        }

        /**
         * \brief Assignment move operator
         * \param orig The original pointer to be moved
         * \return Reference to this
         *
         * The rvalue is assigned into this class, with this class
         * unlinking itself from it's original pointer
         */
        SpartaSharedPointer & operator=(SpartaSharedPointer && orig)
        {
            sparta_assert(&orig != this);
            unlink_();
            ref_count_ = orig.ref_count_;
            orig.ref_count_ = nullptr;
            return *this;
        }

        /**
         * \brief The not ! operator
         * \return true if the pointer is null, false otherwise
         *
         * For operations like:
         * \code
         *     if(!refptr) { ... }
         * \endcode
         */
        bool operator!() const {
            return (ref_count_ == nullptr) || (ref_count_->p == nullptr);
        }

        /**
         * \brief Boolean cast
         * \return Will be true if the pointer is not null
         *
         * For operations like:
         * \code
         *     if(refptr) { ... }
         *     // const bool is_good = refptr; // Disallowed
         * \endcode
         */
        explicit operator bool() const {
            return (ref_count_ && (ref_count_->p == nullptr ? false : true));
        }

        /**
         * \brief Dereference the RefPointer (const)
         * \return The pointer pointed to by this RefPointer
         */
        PointerT * operator->() const {
            return (ref_count_ ? ref_count_->p : nullptr);
        }

        /**
         * \brief Dereference the RefPointer (const)
         * \return The pointer pointed to by this RefPointer
         */
        PointerT & operator*() const {
            sparta_assert(ref_count_ != nullptr, "This is a null SpartaSharedPointer");
            return *(ref_count_->p);
        }

        /**
         * \brief Get the underlying pointer
         * \return The underlying pointer
         */
        PointerT * get() const {
            return (ref_count_ ? ref_count_->p : nullptr);
        }

        /**
         * \brief Reset this shared pointer
         * \param p The new pointer
         */
        void reset(PointerT * p = nullptr) {
            unlink_();
            ref_count_ = p == nullptr ? nullptr : new RefCount(p);
        }

        /**
         * \brief Get the current reference count
         * \return The current reference count. 0 if this SpartaSharedPointer points to nullptr
         */
        uint32_t use_count() const {
            if(SPARTA_EXPECT_TRUE(ref_count_ != nullptr)) {
                return ref_count_->p ? ref_count_->count : 0;
            }
            return 0;
        }

        /**
         * \class SpartaWeakPointer
         * \brief Like in STL, create a weak pointer to a SpartaSharedPointer
         *
         *  Works like the original, just a little faster
         */
        class SpartaWeakPointer
        {
        public:
            //! Create an empty, expired weakpointer
            constexpr SpartaWeakPointer() noexcept = default;

            /**
             * \brief Construct a SpartaWeakPointer with the given SpartaSharedPointer
             * \param sp Pointer to the SpartaSharedPointer to "watch"
             */
            SpartaWeakPointer(const sparta::SpartaSharedPointer<PointerT> & sp) noexcept :
                wp_ref_cnt_(sp.ref_count_)
            {
                if(SPARTA_EXPECT_TRUE(nullptr != wp_ref_cnt_)) {
                    ++(wp_ref_cnt_->wp_count);
                }
            }

            //! Destroy (and detach) from a SpartaSharedPointer
            ~SpartaWeakPointer() {
                if(SPARTA_EXPECT_TRUE(nullptr != wp_ref_cnt_)) {
                    --(wp_ref_cnt_->wp_count);
                    releaseRefCount_(wp_ref_cnt_);
                    wp_ref_cnt_ = nullptr;
                }
            }

            /**
             * \brief Create a copy of the SpartaWeakPointer
             * \param orig The original to copy.  Both are valid
             */
            SpartaWeakPointer(const SpartaWeakPointer & orig) :
                wp_ref_cnt_(orig.wp_ref_cnt_)
            {
                if(SPARTA_EXPECT_TRUE(nullptr != wp_ref_cnt_)) {
                    ++(wp_ref_cnt_->wp_count);
                }
            }

            /**
             * \brief Move a SpartaWeakPointer
             * \param orig The original to move.  The original is invalidated
             */
            SpartaWeakPointer(SpartaWeakPointer &&orig) :
                wp_ref_cnt_(orig.wp_ref_cnt_)
            {
                orig.wp_ref_cnt_ = nullptr;
            }

            /**
             * \brief Assign a SpartaWeakPointer from another
             * \param orig The original to copy.  The original is valid
             */
            SpartaWeakPointer & operator=(const SpartaWeakPointer & orig)
            {
                if(SPARTA_EXPECT_TRUE(nullptr != wp_ref_cnt_)) {
                    --(wp_ref_cnt_->wp_count);
                    releaseRefCount_(wp_ref_cnt_);
                }
                wp_ref_cnt_ = orig.wp_ref_cnt_;

                if(SPARTA_EXPECT_TRUE(nullptr != wp_ref_cnt_)) {
                    ++(wp_ref_cnt_->wp_count);
                }
                return *this;
            }

            /**
             * \brief Move assign a SpartaWeakPointer from another
             * \param orig The original to move.  The original is invalidated
             */
            SpartaWeakPointer & operator=(SpartaWeakPointer && orig)
            {
                if(SPARTA_EXPECT_TRUE(nullptr != wp_ref_cnt_)) {
                    --(wp_ref_cnt_->wp_count);
                    releaseRefCount_(wp_ref_cnt_);
                }

                wp_ref_cnt_ = orig.wp_ref_cnt_;
                orig.wp_ref_cnt_ = nullptr;
                return *this;
            }

            /**
             * \brief The use count of the SpartaSharedPointer.  Will be 0 if none left
             * \return The use count (the number of remaining SpartaSharedPointer)
             */
            long use_count() const noexcept {
                if(SPARTA_EXPECT_TRUE(nullptr != wp_ref_cnt_)) {
                    return (wp_ref_cnt_->count <= 0 ? 0 : wp_ref_cnt_->count);
                }
                return 0;
            }

            /**
             * \brief Has the SpartaSharedPointer that this weak pointer points to expired?
             * \return true if no SpartaSharedPointers are still alive
             */
            bool expired() const noexcept {
                if(SPARTA_EXPECT_TRUE(nullptr != wp_ref_cnt_)) {
                    return wp_ref_cnt_->count <= 0;
                }
                return true;
            }

            /**
             * \brief Lock and return a SpartaSharedPointer this SpartaWeakPointer points to
             * \return nullptr if the SpartaSharedPointer has expired; otherwise a locked version
             */
            SpartaSharedPointer<PointerT> lock() const noexcept {
                return SpartaSharedPointer(wp_ref_cnt_, true);
            }

        private:
            //! Shared reference count between SpartaSharedPointer and SpartaWeakPointer
            RefCount * wp_ref_cnt_ = nullptr;
        };


    private:

        /**
         * \brief Called by the SharedPointer and the WeakPointer to
         * release a reference count
         * \param ref_count The reference count to release
         *
         * If called by the SpartaSharedPointer and there are no weak
         * pointers pointing to this object, delete the user object
         * AND reclaim the reference count.
         *
         * If called by the SpartaWeakPointer and there are no other
         * pointers (of any smart type) pointing to this block, delete
         * the reference count.  The user object was already deleted
         * when the SpartaSharedPointer was destroyed.
         */
        static void releaseRefCount_(RefCount *& ref_count)
        {
            if(0 == ref_count->count) {
                BaseAllocator::MemBlockBase * memory_block =
                    static_cast<BaseAllocator::MemBlockBase *>(ref_count->mem_block);

                if(SPARTA_EXPECT_TRUE(nullptr != memory_block)) {
                    memory_block->alloc->releaseObject_(memory_block);
                }
                else {
                    delete ref_count->p;
                    ref_count->p = nullptr;
                }
                // Make it go negative to show there are no
                // SpartaSharedPointer objects using this ref_count.
                // We can't set it to nullptr because it might be
                // allocated on the heap and we need to ensure all
                // SpartaWeakPointer objects are done with it too.
                --(ref_count->count);
            }

            if((0 >= ref_count->count) && (0 == ref_count->wp_count))
            {
                BaseAllocator::MemBlockBase * memory_block =
                    static_cast<BaseAllocator::MemBlockBase *>(ref_count->mem_block);

                if(SPARTA_EXPECT_TRUE(nullptr != memory_block)) {
                    memory_block->alloc->releaseBlock_(memory_block);
                }
                else {
                    delete ref_count;
                }
                ref_count = nullptr;
            }
        }

        /// Unlink the reference and delete the memory if last to point to it
        void unlink_()
        {
            if(SPARTA_EXPECT_TRUE(ref_count_ != nullptr))
            {
                --ref_count_->count;
                releaseRefCount_(ref_count_);
            }
        }

        // Used by SpartaWeakPointer and Allocator.  weak_alloc is
        // true if from WP
        explicit SpartaSharedPointer(RefCount * cnt, const bool weak_ptr = false) :
            ref_count_(cnt)
        {
            if(weak_ptr && ref_count_) { ++ref_count_->count; }
        }

        RefCount * ref_count_ = nullptr;

        friend class SpartaSharedPointerAllocator<PointerT>;

        template<typename PtrT, typename... Args>
        friend SpartaSharedPointer<PtrT>
        allocate_sparta_shared_pointer(SpartaSharedPointerAllocator<PtrT> &, Args&&...args);
    };


    template<typename PtrT, typename Ptr2>
    bool operator==(const SpartaSharedPointer<PtrT>& ptr1, const SpartaSharedPointer<Ptr2>& ptr2) noexcept
    { return ptr1.get() == ptr2.get(); }

    template<typename PtrT>
    bool operator==(const SpartaSharedPointer<PtrT>& ptr1, std::nullptr_t) noexcept
    { return !ptr1; }

    template<typename PtrT>
    bool operator==(std::nullptr_t, const SpartaSharedPointer<PtrT>& ptr1) noexcept
    { return !ptr1; }

    template<typename PtrT, typename Ptr2>
    bool operator!=(const SpartaSharedPointer<PtrT>& ptr1, const SpartaSharedPointer<Ptr2>& ptr2) noexcept
    { return ptr1.get() != ptr2.get(); }

    template<typename PtrT>
    bool operator!=(const SpartaSharedPointer<PtrT>& ptr1, std::nullptr_t) noexcept
    { return (bool)ptr1; };

    template<typename PtrT>
    bool operator!=(std::nullptr_t, const SpartaSharedPointer<PtrT>& ptr1) noexcept
    { return (bool)ptr1; }

    template<typename PtrT>
    std::ostream& operator<<(std::ostream & os, const SpartaSharedPointer<PtrT> & p)
    {
        os << p.get();
        return os;
    }

} // sparta namespace

// Helper methods to determine pointer type and/or remove it
namespace MetaStruct {

    // Helper structs
    template<typename T>
    struct is_any_pointer<sparta::SpartaSharedPointer<T>> : public std::true_type {};

    template<typename T>
    struct is_any_pointer<sparta::SpartaSharedPointer<T> const> : public std::true_type {};

    template<typename T>
    struct is_any_pointer<sparta::SpartaSharedPointer<T> &> : public std::true_type {};

    template<typename T>
    struct is_any_pointer<sparta::SpartaSharedPointer<T> const &> : public std::true_type {};

    template<typename T>
    struct remove_any_pointer<sparta::SpartaSharedPointer<T>> { using type = T; };

    template<typename T>
    struct remove_any_pointer<sparta::SpartaSharedPointer<T> const> { using type = T; };

    template<typename T>
    struct remove_any_pointer<sparta::SpartaSharedPointer<T> &> { using type = T; };

    template<typename T>
    struct remove_any_pointer<sparta::SpartaSharedPointer<T> const &> { using type = T; };
}
