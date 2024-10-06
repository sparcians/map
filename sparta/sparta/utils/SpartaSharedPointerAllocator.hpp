#pragma once

#include "sparta/utils/SpartaSharedPointer.hpp"

namespace sparta
{

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
     * sparta::SpartaSharedPointerAllocator<PointerT> instance.
     *
     * Also, the allocator *must outlive* any simulator
     * componentry that uses objects allocated by this allocator.
     * If not, random seg faults will plague the developer.
     * Suggested use is to make this allocator global, with
     * user-defined atomic operations if desired.
     *
     * Another suggestion for Sparta developers using these allocators
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
     *      using MyAllocator = sparta::SpartaSharedPointerAllocator<MyClassIUseALot>;
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
     *     sparta::SpartaSharedPointerAllocator<MyClassIUseALot>
                                            my_class_i_use_a_lot_allocator(max_blocks_allowed,
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
     *     extern sparta::SpartaSharedPointerAllocator<MyClassIUseALot>
     *                                       my_class_i_use_a_lot_allocator;
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
     * being "used up."  Watermark must be less than or equal to
     * max_num_blocks.
     *
     * The Allocator also keeps track of those objects in flight
     * that have not returned to the allocator.  Using the call to
     * SpartaSharedPointerAllocator<PointerT>::getOutstandingAllocatedObjects,
     * a modeler can determine which objects are still
     * outstanding, where they might be, and help debug the
     * situation.
     *
     */
    template<class PointerT>
    class SpartaSharedPointerAllocator : public BaseAllocator
    {
    public:

        //! Handy typedef
        using element_type = PointerT;

        //! Used for defining a custom watermark warning callback.
        //! Default is to print a warning
        using WaterMarkWarningCallback = std::function<void (const SpartaSharedPointerAllocator &)>;

        //! Used for defining a custom over allocation callback.
        //! Default is to throw an exception
        using OverAllocationCallback = std::function<void (const SpartaSharedPointerAllocator &)>;

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
            watermark_warning_callback_(waterMarkWarningCallback_),
            over_allocation_callback_(overAllocationCallback_)
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
         * \brief Set a custom watermark warning callback
         *
         * \param callback The callback to use when the allocator hits the watermark
         *
         * The callback will be called only once after the watermark was hit.
         */
        void registerCustomWaterMarkCallback(const WaterMarkWarningCallback & callback) {
            watermark_warning_callback_ = callback;
        }

        /**
         * \brief Set a custom over allocation callback
         *
         * \param callback The callback to use when the allocator exceeds max blocks
         *
         * The callback will be called only after the allocated exceeds the max blocks.
         */
        void registerCustomOverAllocationCallback(const OverAllocationCallback & callback) {
            over_allocation_callback_ = callback;
        }

    private:

        // Let's make friends
        friend class SpartaSharedPointer<PointerT>;

        // Make the allocate function a buddy
        template<typename PtrT, typename... Args>
        friend SpartaSharedPointer<PtrT>
        allocate_sparta_shared_pointer(SpartaSharedPointerAllocator<PtrT> &, Args&&...args);

        template<typename T>
        struct AlignedStorage
        {
            alignas(T) std::byte buf[sizeof(T)];
        };

        // Internal MemoryBlock
        struct MemBlock : public BaseAllocator::MemBlockBase
        {
            using RefCountType = typename SpartaSharedPointer<PointerT>::RefCount;

            using RefCountAlignedStorage = AlignedStorage<RefCountType>;

            using PointerTAlignedStorage = AlignedStorage<PointerT>;

            template<typename ...ObjArgs>
            MemBlock(SpartaSharedPointerAllocator * alloc_in, ObjArgs&&...obj_args) :
                MemBlockBase(alloc_in)
            {
                // Build the new object using the inplacement new.
                object = new (&object_storage) PointerT(std::forward<ObjArgs>(obj_args)...);

                // Build the reference count using that object
                ref_count = new (&ref_count_storage) RefCountType(object, (void*)this);
            }

            PointerT              * object = nullptr;
            RefCountType          * ref_count = nullptr;

            RefCountAlignedStorage  ref_count_storage;
            PointerTAlignedStorage  object_storage;

            MemBlock(const MemBlock &) = delete;
            MemBlock(MemBlock &&)      = delete;
            MemBlock & operator=(const MemBlock&) = delete;
            ~MemBlock() {} // Does nothing; implementation for cppcheck
        };

        // Class to manager the large block of memory allocated
        class MemBlockVector
        {
            // Align the storage for the MemBlock
            using MemBlockAlignedStorage = AlignedStorage<MemBlock>;

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
        typename SpartaSharedPointer<PointerT>::RefCount * allocate_(PointerTArgs&&... args)
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
                        over_allocation_callback_(*this);
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
            return block->ref_count;
        }

        // Default watermark warning callback
        static void waterMarkWarningCallback_(const SpartaSharedPointerAllocator & allocator) {
            std::cerr << "WARNING: The watermark for this allocator has been surpassed: \n\n\t"
                      << __PRETTY_FUNCTION__
                      << "\n\n"
                      << "\t\tNumber blocks preallocated: " << allocator.memory_blocks_.capacity()
                      << "\n\t\tWatermark                 : " << allocator.water_mark_ << std::endl;
        }

        // Default over allocation callback
        static void overAllocationCallback_(const SpartaSharedPointerAllocator & allocator) {
        }

        /**
         * \brief Release (calls the destructor) of the held object
         * \param block The block containing the ref count that contains the object
         */
        void releaseObject_(void * block) const noexcept override {
            static_cast<MemBlock *>(block)->ref_count->p->~PointerT();
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
        void releaseBlock_(void * block) override {
            sparta_assert(free_idx_ < free_blocks_.capacity());
            free_blocks_[free_idx_] = static_cast<MemBlock *>(block);
            ++free_idx_;
        }

        MemBlockVector           memory_blocks_;
        std::vector<MemBlock*>   free_blocks_;
        size_t                   free_idx_ = 0;
        size_t                   allocated_  = 0;
        const size_t             water_mark_;
        bool                     water_mark_warning_ = false;
        WaterMarkWarningCallback watermark_warning_callback_;
        OverAllocationCallback   over_allocation_callback_;
    };

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
    allocate_sparta_shared_pointer(SpartaSharedPointerAllocator<PointerT> & alloc,
                                   Args&&...args)
    {
        static_assert(std::is_constructible<PointerT, Args...>::value,
                      "Can't construct object in allocate_sparta_shared_pointer with the arguments given");

        SpartaSharedPointer<PointerT> ptr(alloc.allocate_(std::forward<Args>(args)...));
        return ptr;
    }

}
