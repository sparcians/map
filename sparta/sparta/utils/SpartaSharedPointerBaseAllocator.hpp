#pragma once

namespace sparta
{

#ifndef DO_NOT_DOCUMENT
    ////////////////////////////////////////////////////////////////////////////////
    // Internals to support cross object allocation (derivative types)
    // using Allocators.  In a nutshell, if one SSP
    // (SpartaSharedPointer) of the base type is being reclaimed via
    // an allocator, we want to steer that deallocation to the correct
    // deallocator.
    template <class PointerT>
    class SpartaSharedPointer;

    //! Base class for the Allocator -- typeless and allows releasing
    //! of an object of derived types.
    class BaseAllocator {
    public:
        virtual ~BaseAllocator() {}

        struct MemBlockBase {
            MemBlockBase(BaseAllocator * alloc_in) : alloc(alloc_in) {}
            virtual ~MemBlockBase() {}
            BaseAllocator * const alloc = nullptr;
        };

        template<class PointerT>
        friend class SpartaSharedPointer;

    protected:
        // Release an object; release the block
        virtual void releaseObject_(void * block) const noexcept = 0;
        virtual void releaseBlock_(void * block) = 0;
    };
#endif

}
