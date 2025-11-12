// <Checkpointable> -*- C++ -*-

#pragma once

#include <cassert>
#include <vector>
#include <list>
#include <memory>
#include <type_traits>
#include <cstring>

#include "sparta/utils/MathUtils.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/functional/ArchData.hpp"

namespace sparta
{
    /**
     * \class Checkpointable
     * \brief Class to enable a modeler to register checkpointable
     *        objects in simulation
     *
     * Sparta's checkpointing mechanism use sparta::ArchData backend
     * (for memory, registers) to save/restore checkpoints, but some
     * classes store their data in member variables and not ArchData
     * backends.
     *
     * This class provides a simple interface that allows a user to
     * register those member variables with Sparta's checkpointing
     * mechanism.
     *
     * Inside the Checkpointable, for each object registed/allocated,
     * a block of memory is created to completely contain that object
     * (for memory copy) and a reference to that memory is returned.
     * If the checkpointed object is _not_ self-contained, behavior
     * will be indeterministic on checkpoint restores.
     *
     * To guarantee the save/restore of a checkpointed object, the
     * checkpointed object's types must be serializabtle, meaning they
     * must be trivial/fundamental and direct.  For example, the
     * checkpointable object must not be nor contain any pointers to
     * untracked memory.  However, the checkpointed object is not
     * required to be trivially constructable.
     *
     * Examples of checkpointable types:
     *   - POD types (int, float, etc)
     *   - Static array types
     *   - Structual components comprised of POD types or nested
     *     structs, also of POD types
     *
     * This class will not ensure the checkpointable object falls
     * within the bound of the above contraints.  In other words, the
     * modeler is responsible for making sure the checkpointable
     * object is self-contained within singluar storage capacity.
     *
     * Example usecase:
     *
     * \code
     *   class MyCheckpointable
     *   {
     *   public:
     *        MyCheckpointable(sparta:TreeNode * my_node) :
     *            sparta::Checkpointable(my_node),
     *            my_checkpointable_integer_(allocateCheckpointable<uint64_t)>()),
     *            my_checkpointable_struct_(allocateCheckpointable<MyCheckpointStructure)>(0, "Hello World"))
     *        {}
     *
     *   private:
     *        // Checkpointable class
     *        sparta::Checkpointable my_checkpointables_;
     *
     *        // Objects to be checkpointed
     *        struct MyCheckpointStructure {
     *            MyCheckpointStructure(uint32_t initial_int_val,
     *                                  const char * initial_str_val) :
     *                my_struct_int_(initial_int_val)
     *            {
     *                ::strcpy(my_struct_ary_, initial_int_val);
     *            }
     *
     *            uint32_t my_struct_int_;
     *            char     my_struct_ary_[128];
     *        };
     *
     *        uint32_t & my_checkpointable_integer_;
     *        MyCheckpointStructure & my_checkpointable_struct_;
     *   };
     * \endcode
     */
    class Checkpointable
    {
    public:

        //! \brief Create a Checkpointable object used to allocate
        //!        components for checkpointing
        Checkpointable(sparta::TreeNode * cp_node) :
            cp_node_(cp_node)
        {
        }

        /**
         * \brief Allocate a checkpointable type
         * \param cp_args Arguments to pass to the object during construction
         * \tparam CheckpointableT The object type -- must not be a pointer
         * \tparam CpArgs The object construction args (if required)
         *
         * A trival, copyable type can be POD, a simple structure
         * containing POD types, structures with embedded POD types,
         * and array types of PODs.
         *
         */
        template<class CheckpointableT, typename... CpArgs>
        CheckpointableT & allocateCheckpointable(CpArgs... cp_args)
        {
            static_assert(false == std::is_pointer_v<CheckpointableT>,
                          "Checkpointable object cannot be a pointer");
            constexpr size_t checkpointable_size = utils::next_power_of_2(sizeof(CheckpointableT));
            auto & cp_component = checkpoint_components_.emplace_back(new CheckpointComponent(cp_node_,
                                                                                              checkpointable_size));

            auto cp_mem = cp_component->getRawDataPtr();
            return *(new (cp_mem) CheckpointableT(cp_args...));
        }

    private:
        sparta::TreeNode * cp_node_ = nullptr;

        struct CheckpointComponent
        {
            CheckpointComponent(sparta::TreeNode * cp_node,
                                ArchData::offset_type line_size) :
                adata_(cp_node,
                       line_size,
                       ArchData::DEFAULT_INITIAL_FILL,
                       ArchData::DEFAULT_INITIAL_FILL_SIZE,
                       false), // Cannot delete lines
                dview_(&adata_, 0, line_size,
                       ArchDataSegment::INVALID_ID, // subset of
                       0) // subset_offset
            {
                adata_.layout();
            }

            uint8_t * getRawDataPtr() {
                return dview_.getLine()->getRawDataPtr(0);
            }

            //! \brief ArchData that will hold snapshots of this Checkpointable
            ArchData adata_;

            //! \brief Objects registered
            DataView dview_;
        };

        std::vector<std::unique_ptr<CheckpointComponent>> checkpoint_components_;

    };
}
