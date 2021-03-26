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

namespace sparta
{
    /**
     * \class SpartaSharedPointer
     * \brief Used for garbage collection, will delete the object it
     *        points to when all objects are finished using it
     *
     * Simple, thread-\a unsafe class that will delete memory it
     * points to when the last memory reference is deconstructed.
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
     *    // Don't delete!
     *}
     * \endcode
     *
     * This class can be used independently, or more efficiently with
     * sparta::allocate_sparta_shared_pointer<T>.  See
     * sparta::SpartaSharedPointer::SpartaSharedPointerAllocator for more information.
     */
    template <class PointerT>
    class SpartaSharedPointer
    {
    public:
        class SpartaWeakPointer;

    private:
        /// Internal structure to keep track of the reference
        struct RefCount
        {
            explicit RefCount(PointerT * _p,
                              bool perform_delete = true,
                              uint32_t initial_count = 1) :
                count(initial_count),
                p(_p),
                perform_delete(perform_delete)
            {}

            ~RefCount() { p = nullptr; }

            int32_t count   {0};
            int32_t wp_count{0}; // For weakpointers
            PointerT          * p = nullptr;
            void              * mem_block = nullptr;
            bool perform_delete = true;
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
            ref_count_(p == nullptr ? nullptr : new RefCount(p)) {}

        /**
         * \brief Constructor for SpartaSharedPointer<T> ptr = nullptr;
         * \param nullptr_t
         */
        constexpr SpartaSharedPointer(std::nullptr_t) noexcept :
            ref_count_(nullptr) {}

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
            orig.ref_count_    = nullptr;
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
            ref_count_    = orig.ref_count_;
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
            ref_count_     = p == nullptr ? nullptr : new RefCount(p);
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
                cnt_(sp.ref_count_)
            {
                if(SPARTA_EXPECT_TRUE(nullptr != cnt_)) {
                    ++(cnt_->wp_count);
                }
            }

            //! Destroy (and detach) from a SpartaSharedPointer
            ~SpartaWeakPointer() {
                if(SPARTA_EXPECT_TRUE(nullptr != cnt_)) {
                    --(cnt_->wp_count);
                    releaseRefCount_(cnt_);
                    cnt_ = nullptr;
                }
            }

            /**
             * \brief Create a copy of the SpartaWeakPointer
             * \param orig The original to copy.  Both are valid
             */
            SpartaWeakPointer(const SpartaWeakPointer & orig) :
                cnt_(orig.cnt_)
            {
                if(SPARTA_EXPECT_TRUE(nullptr != cnt_)) {
                    ++(cnt_->wp_count);
                }
            }

            /**
             * \brief Move a SpartaWeakPointer
             * \param orig The original to move.  The original is invalidated
             */
            SpartaWeakPointer(SpartaWeakPointer &&orig) :
                cnt_(orig.cnt_)
            {
                if(SPARTA_EXPECT_TRUE(nullptr != cnt_)) {
                    orig.cnt_ = nullptr;
                }
            }

            /**
             * \brief Assign a SpartaWeakPointer from another
             * \param orig The original to copy.  The original is valid
             */
            SpartaWeakPointer & operator=(const SpartaWeakPointer & orig) {
                cnt_ = orig.cnt_;
                if(SPARTA_EXPECT_TRUE(nullptr != cnt_)) {
                    cnt_ = orig.cnt_;
                    ++(cnt_->wp_count);
                }
                return *this;
            }

            /**
             * \brief Move assign a SpartaWeakPointer from another
             * \param orig The original to move.  The original is invalidated
             */
            SpartaWeakPointer & operator=(SpartaWeakPointer && orig)
            {
                cnt_ = orig.cnt_;
                if(SPARTA_EXPECT_TRUE(nullptr != cnt_)) {
                    cnt_ = orig.cnt_;
                    orig.cnt_ = nullptr;
                }
                return *this;
            }

            /**
             * \brief The use count of the SpartaSharedPointer.  Will be 0 if none left
             * \return The use count (the number of remaining SpartaSharedPointer)
             */
            long use_count() const noexcept {
                if(SPARTA_EXPECT_TRUE(nullptr != cnt_)) {
                    return (cnt_->count <= 0 ? 0 : cnt_->count);
                }
                return 0;
            }

            /**
             * \brief Has the SpartaSharedPointer that this weak pointer points to expired?
             * \return true if no SpartaSharedPointers are still alive
             */
            bool expired() const noexcept {
                if(SPARTA_EXPECT_TRUE(nullptr != cnt_)) {
                    return cnt_->count <= 0;
                }
                return true;
            }

            /**
             * \brief Lock and return a SpartaSharedPointer this SpartaWeakPointer points to
             * \return nullptr if the SpartaSharedPointer has expired; otherwise a locked version
             */
            SpartaSharedPointer<PointerT> lock() const noexcept {
                return SpartaSharedPointer<PointerT>(cnt_);
            }

        private:
            //! Shared reference count between SpartaSharedPointer and SpartaWeakPointer
            RefCount * cnt_ = nullptr;
        };

        // Allocation helpers

        /*!
         * \class SpartaSharedPointerAllocator
         *
         * \brief A memory allocator complementing SpartaSharedPointer
         *        that reuses old memory
         *
         * First off, this allocator *DOES NOT* follow the semantics
         * of std::allocator_traits.  This is done on purpose to
         * prevent others from using this class with std types like
         * std::shared_ptr, std::vector, etc.  Also, this class *is
         * not* thread safe.  Do not expect it to work in a threaded
         * application where multiple threads are
         * allocating/deallocating with the same
         * sparta::SpartaSharedPointerAllocator instance.
         *
         * Also, the allocator *must outlive* any simulator
         * componentry that uses objects allocated by this allocator.
         * If not, random seg faults will plague the developer.
         * Suggested use is to make this allocator global, with
         * user-defined atomic operations if desired.
         *
         * A suggestion for Sparta developers using these allocators
         * is to create an `Allocator` class type that derives from
         * sparta::TreeNode and place that class somewhere in the
         * simulation under root:
         *
         * \code
         *
         * // Define a specific TreeNode that is just allocators
         * class OurAllocators : public sparta::TreeNode
         * {
         * public:
         *      // Constructors and what-not...
         *
         *      // Allocators
         *      using MyAllocator = sparta::SpartaSharedPointer<MyClassIUseALot>::SpartaSharedPointerAllocator;
         *      MyAllocator my_allocator(100, 200);  // whatever params you chose
         * };
         *
         * \endcode
         *
         * And in the modeler's block, they would retrive the
         * `OurAllocators` node and get the allocator:
         *
         * \code
         *
         * class MyDevice : public sparta::Unit
         * {
         * public:
         *     MyDevice(sparta::TreeNode * my_container, sparta::ParameterSet *) :
         *         my_allocator_(customUtilToFindTheAllocatorTreeNode(my_container)->my_allocator)
         *     {}
         *
         * private:
         *
         *     OurAllocators::MyAllocator & my_allocator_;
         * };
         *
         * \endcode
         *
         * The class is not intended to be used directly, but rather
         * in compliment with the allocator method
         * sparta::allocate_sparta_shared_pointer.
         *
         * A typical use of this class is when a modeler is
         * creating/destroying lots and lots and lots of little
         * objects throughout simulation.  Using the std::allocator or
         * boost::pool_allocator does help, but not as much as this
         * allocator, which is tuned/focused on the exact modeling use case.
         *
         * As an example of a healthy performance uplift, the
         * CoreExample saw a 20% boost in performance using this
         * allocation coupled with the SpartaSharedPointer over using
         * std::shared_ptr created via std::allocate_shared.  See a
         * performance example in the SpartaSharedPointer unit test.
         *
         *
         * To use this allocator, create a singleton of it in the
         * application for the given type:
         *
         * \code
         * // .cpp file
         *
         * // Instance of the allocator in source.  Set it up to
         * // initially allocate 100 blocks of memory.
         *
         * namespace mysimulator {
         *     const size_t max_blocks_allowed = 100;
         *     const size_t water_mark_complaining_point = 80;
         *     sparta::SpartaSharedPointer<MyClassIUseALot>::
         *         SpartaSharedPointerAllocator my_class_i_use_a_lot_allocator(max_blocks_allowed,
         *                                                                     water_mark_complaining_point);
         * }
         * \endcode
         *
         * \code
         * namespace mysimulator {
         *     // .hpp file
         *     class MyClassIUseALot
         *     {
         *     public:
         *         // Constructor with two args
         *         MyClassIUseALot(uint32_t, const std::string&);
         *     };
         *
         *     // The Allocator to use with the class
         *     extern sparta::SpartaSharedPointer<MyClassIUseALot>::
         *         SpartaSharedPointerAllocator my_class_i_use_a_lot_allocator;
         * }
         *
         * \endcode
         *
         * In runtime code, use allocate_sparta_shared_pointer
         * method to allocate instances of the SpartaSharedPointer:
         *
         * \code
         * void foo()
         * {
         *     sparta::SpartaSharedPointer<MyClassIUseALot> ptr =
         *         sparta::allocate_sparta_shared_pointer<MyClassIUseALot>(my_class_i_use_a_lot_allocator,
         *                                                                 30, "hello");
         *     ptr.reset(); // put back the allocated object in a pool
         * }
         * \endcode
         *
         * Use sparta::SpartaSharedPointer like a regular
         * sparta::SpartaSharedPointer.
         *
         * The allocator's constructor takes two arguments: a maximum
         * number of blocks to allocate and a water mark.  The maximum
         * number of blocks is the hard upper limit of this allocator.
         * Go beyond that and the allocator will throw an exception
         * that it's "out of memory."  If the allocator hits the
         * watermark, it will warn that memory is dangerously close to
         * being "used up."  Watermark much be less than or equal to
         * max_num_blocks.
         *
         * The Alocator also keeps track of those objects in flight
         * that have not returned to the allocator.  Using the call to
         * SpartaSharedPointerAllocator::getOutstandingAllocatedObjects,
         * a modeler can determine which objects are still
         * outstanding, where they might be, and help debug the
         * situation.
         *
         */
        class SpartaSharedPointerAllocator
        {
        public:

            //! Handy typedef
            using element_type = PointerT;

            //! Used for defining a custom watermark callback.
            //! Default is to print a warning
            using WaterMarkWarningCallback = std::function<void (const SpartaSharedPointerAllocator &)>;

            /**
             * \brief Construct this allocator with num_blocks of initial memory
             * \param max_num_blocks The maximum number of blocks this allocator is allowed to use
             * \param water_mark The point where this allocator will warn (once) if allocation passes this threshold
             *
             * Allocate a SpartaSharedPointerAllocator with a large
             * block of memory (up front) to use during simulation for
             * the PointerT type.  The water_mark is a warning spot to
             * help developers tune their allocations for their uses.
             *
             */
            SpartaSharedPointerAllocator(const size_t max_num_blocks,
                                         const size_t water_mark) :
                memory_blocks_(max_num_blocks),
                water_mark_(water_mark),
                watermark_warning_callback_(waterMarkWarningCallback_)
            {
                sparta_assert(water_mark <= max_num_blocks,
                              "The water_mark on SpartaSharedPointerAllocator should be less than or " <<
                              "equal to the maximum number of blocks. water_mark=" << water_mark <<
                              " max_num_blocks=" << max_num_blocks);
                free_blocks_.resize(max_num_blocks, nullptr);
            }

            //! Disallow copies/assignments/moves
            SpartaSharedPointerAllocator(const SpartaSharedPointerAllocator&) = delete;
            //! Disallow copies/assignments/moves
            SpartaSharedPointerAllocator(SpartaSharedPointerAllocator &&) = delete;
            //! Disallow copies/assignments/moves
            SpartaSharedPointerAllocator operator=(const SpartaSharedPointerAllocator&) = delete;

            //! Clean up the allocator -- will warn if there are objects not returned to the allocator
            ~SpartaSharedPointerAllocator() {
                if(hasOutstandingObjects())
                {
                    std::cerr << "WARNING: Seems that not all of the blocks made it back.  \n'" <<
                        __PRETTY_FUNCTION__ << "'\nAllocated: " << allocated_ <<
                        "\nReturned: " << free_idx_ << std::endl;
                }
            }

            /**
             * \brief Return the number of freed objects this allocator holds
             * \return Number of freed objects
             */
            size_t getNumFree() const {
                return free_idx_;
            }

            /**
             * \brief Return the number of allocated objects this allocator allocated over time
             * \return Number of allocated objects
             *
             * This count should always be <= getNumFree()
             */
            size_t getNumAllocated() const {
                return memory_blocks_.size();
            }

            /**
             * \brief Query to see if the allocator has any outstanding memory not yet returned
             * \return True if there are outstanding blocks not yet returned to the allocator
             */
            bool hasOutstandingObjects() const {
                return (allocated_ != free_idx_);
            }

            /**
             * \brief Return a vector of objects that have not been deleted/not yet returned to the allocator
             *
             * \return vector of PointerT objects that still have reference counts > 0
             */
            std::vector<const PointerT*> getOutstandingAllocatedObjects() const
            {
                std::vector<const PointerT*> allocated_objs;

                const size_t size = memory_blocks_.size();
                for(uint32_t i = 0; i < size; ++i) {
                    if(memory_blocks_[i]->ref_count->count > 0) {
                        allocated_objs.emplace_back(memory_blocks_[i]->object);
                    }
                }

                return allocated_objs;
            }

            /**
             * \brief Set a custom watermark callback
             *
             * \param callback The callback to use when the allocator hits the watermark
             *
             * The callback will be called only once after the watermark was hit.
             */
            void registerCustomWaterMarkCallback(const WaterMarkWarningCallback & callback) {
                watermark_warning_callback_ = callback;
            }

        private:

            // Let's make friends
            friend class SpartaSharedPointer<PointerT>;

            // Make the allocate function a buddy
            template<typename PtrT, typename... Args>
            friend SpartaSharedPointer<PtrT>
            allocate_sparta_shared_pointer(typename SpartaSharedPointer<PtrT>::SpartaSharedPointerAllocator &,
                                           Args&&...args);

            // Internal MemoryBlock
            struct MemBlock
            {
                using RefCountType = SpartaSharedPointer<PointerT>::RefCount;

                using RefCountAlignedStorage =
                    typename std::aligned_storage<sizeof(RefCountType), alignof(RefCountType)>::type;

                using PointerTAlignedStorage =
                    typename std::aligned_storage<sizeof(PointerT), alignof(PointerT)>::type;

                template<typename ...ObjArgs>
                MemBlock(SpartaSharedPointerAllocator * alloc_in,
                         ObjArgs&&...obj_args) :
                    alloc(alloc_in)
                {
                    // Build the new object using the inplacement new.
                    object = new (&object_storage) PointerT(std::forward<ObjArgs>(obj_args)...);

                    // Build the reference count using that object
                    const bool perform_delete = false;
                    ref_count = new (&ref_count_storage) RefCountType(object, perform_delete);

                    // Have this reference count remember the block its in
                    ref_count->mem_block = (void*)this;
                }

                RefCountAlignedStorage         ref_count_storage;
                PointerTAlignedStorage         object_storage;
                SpartaSharedPointerAllocator * alloc = nullptr;
                PointerT                     * object = nullptr;
                RefCountType                 * ref_count = nullptr;

                MemBlock(const MemBlock &) = delete;
                MemBlock(MemBlock &&)      = delete;
                MemBlock & operator=(const MemBlock&) = delete;
                ~MemBlock() {} // Does nothing; implementation for cppcheck
            };

            // Class to manager the large block of memory allocated
            class MemBlockVector
            {
                // Align the storage for the MemBlock
                using MemBlockAlignedStorage =
                    typename std::aligned_storage<sizeof(MemBlock), alignof(MemBlock)>::type;

                std::vector<MemBlockAlignedStorage> data_;
                std::size_t size_ = 0;

            public:

                // Create num_blocks amount of MemBlocks to use
                explicit MemBlockVector(const size_t num_blocks) :
                    data_(num_blocks)
                {}

                // Allocate an object in aligned storage
                template<typename ...Args>
                MemBlock * allocate(SpartaSharedPointerAllocator * alloc, Args&&... args)
                {
                    sparta_assert(size_ < data_.size(), "Out of memory");

                    // construct value in memory of aligned storage
                    // using inplace operator new
                    auto block = new(&data_[size_]) MemBlock(alloc, std::forward<Args>(args)...);
                    ++size_;

                    return block;
                }

                // Access an object in aligned storage
                const MemBlock* operator[](std::size_t idx) const
                {
                    sparta_assert(idx < size_);
                    return reinterpret_cast<const MemBlock*>(&data_[idx]);
                }

                // The capacity or num_blocks
                size_t capacity() const {
                    return data_.size();
                }

                // The number of blocks requested so far
                size_t size() const {
                    return size_;
                }

                // Start over.
                void clear() {
                    size_ = 0;
                }
            };

            /**
             * \brief Allocate a memory block for the given object to be
             *        used by the SpartaSharedPointer
             *
             * \param args Arguments passed to the constructor of the internal object
             *
             * \return Pointer to the MemBlock used by the
             *         SpartaSharedPointer to release the memory
             */
            template<typename ...PointerTArgs>
            MemBlock * allocate_(PointerTArgs&&... args)
            {
                // Return memory allocated here.
                MemBlock * block = nullptr;

                // Check for previously freed blocks and reuse them.
                if(free_idx_ > 0) {
                    --free_idx_;
                    block = free_blocks_[free_idx_];
                    sparta_assert(block->ref_count->p != nullptr);
                    block->ref_count->mem_block = (void*)block;
                    block->ref_count->count = 1;
                    // perform an in place new on the reused pointer
                    new (block->ref_count->p) PointerT(std::forward<PointerTArgs>(args)...);
                }
                else {
                    if(SPARTA_EXPECT_FALSE(allocated_ > water_mark_)) {
                        if(SPARTA_EXPECT_FALSE(!water_mark_warning_)) {
                            watermark_warning_callback_(*this);
                            water_mark_warning_ = true;
                        }

                        // Only need to check for overallocation if we've passed the watermark
                        if(SPARTA_EXPECT_FALSE(allocated_ >= memory_blocks_.capacity())) {
                            SpartaException ex;
                            ex << "This allocator has run out of memory: \n\n\t"
                               << __PRETTY_FUNCTION__
                               << "\n\n"
                               << "\t\tNumber blocks preallocated: " << memory_blocks_.capacity()
                               << "\n\t\tWatermark                 : " << water_mark_;
                            throw ex;
                        }
                    }
                    block = memory_blocks_.allocate(this, std::forward<PointerTArgs>(args)...);
                    ++allocated_;
                }
                return block;
            }

            // Default watermark warning callback
            static void waterMarkWarningCallback_(const SpartaSharedPointerAllocator & allocator) {
                std::cerr << "WARNING: The watermark for this allocator has been surpassed: \n\n\t"
                          << __PRETTY_FUNCTION__
                          << "\n\n"
                          << "\t\tNumber blocks preallocated: " << allocator.memory_blocks_.capacity()
                          << "\n\t\tWatermark                 : " << allocator.water_mark_ << std::endl;
            }

            /**
             * \brief Release (calls the destructor) of the held object
             * \param block The block containing the ref count that contains the object
             */
            void releaseObject_(MemBlock * block) {
                block->ref_count->p->~PointerT();
            }

            /**
             * \brief Return the block of memory back to the "pool"
             * \param block The block to return
             *
             * Note that this *does not* delete the memory but
             * reclaims the memory block.  The object should be
             * considered "dead" and will be used in the placement new
             * in the future.
             */
            void releaseBlock_(MemBlock * block) {
                sparta_assert(free_idx_ < free_blocks_.capacity());
                free_blocks_[free_idx_] = block;
                ++free_idx_;
            }

            MemBlockVector           memory_blocks_;
            std::vector<MemBlock*>   free_blocks_;
            size_t                   free_idx_ = 0;
            size_t                   allocated_  = 0;
            const size_t             water_mark_;
            bool                     water_mark_warning_ = false;
            WaterMarkWarningCallback watermark_warning_callback_;
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
                typename SpartaSharedPointerAllocator::MemBlock * memory_block =
                    static_cast<typename SpartaSharedPointerAllocator::MemBlock *>(ref_count->mem_block);

                if(SPARTA_EXPECT_TRUE(nullptr != memory_block)) {
                    memory_block->alloc->releaseObject_(memory_block);
                }
                else {
                    ref_count->p->~PointerT();
                }
                --(ref_count->count);
            }

            if((0 >= ref_count->count) && (0 == ref_count->wp_count))
            {
                typename SpartaSharedPointerAllocator::MemBlock * memory_block =
                    static_cast<typename SpartaSharedPointerAllocator::MemBlock *>(ref_count->mem_block);

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

        // Used my SpartaWeakPointer only
        explicit SpartaSharedPointer(RefCount * cnt) :
            ref_count_(cnt)
        {
            if(SPARTA_EXPECT_TRUE(ref_count_ != nullptr)) {
                ++ref_count_->count;
            }
        }

        // Used explicitly by the allocator only
        explicit SpartaSharedPointer(typename SpartaSharedPointerAllocator::MemBlock * block) :
            ref_count_(block->ref_count)
        {
            assert((void*)block == ref_count_->mem_block);
        }

        RefCount * ref_count_ = nullptr;

        template<typename PtrT, typename... Args>
        friend SpartaSharedPointer<PtrT>
        allocate_sparta_shared_pointer(typename SpartaSharedPointer<PtrT>::SpartaSharedPointerAllocator &,
                                       Args&&...args);

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

    /**
     * \brief Allocate a SpartaSharedPointer
     * \tparam PointerT The pointer type to allocate
     * \param args Arguments to the class being instantiated
     * \return sparta::SpartaSharedPointer type
     *
     * Allocate a SpartaSharedPointer using the
     * SpartaSharedPointerAllocator instance found in the PointerT.
     *
     * See SpartaSharedPointerAllocator for example usage
     */
    template<typename PointerT, typename... Args>
    SpartaSharedPointer<PointerT>
    allocate_sparta_shared_pointer(typename SpartaSharedPointer<PointerT>::SpartaSharedPointerAllocator & alloc,
                                   Args&&...args)
    {
        static_assert(std::is_constructible<PointerT, Args...>::value,
                      "Can't construct object in allocate_sparta_shared_pointer with the arguments given");

        SpartaSharedPointer<PointerT> ptr(alloc.allocate_(std::forward<Args>(args)...));
        return ptr;
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
