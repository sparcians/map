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
     * \brief Class to enable a modeler to register checkpointable objects in simulation
     *
     * Sparta's checkpointing mechanism use sparta::ArchData backend
     * (like memory, registers) to save/restore checkpoints, but some
     * classes store their data in member variables and not ArchData
     * backends.
     *
     * This class provides a simple interface that allows a user to
     * register those member variables with Sparta's checkpointer.
     *
     * The classes employs a lazy checkpointing mechanism meaning it
     * will collect the value of the object at the time of
     * checkpointing and not on every update to the object.
     *
     * When a restore is called this class will put back the data from
     * the checkpoint into the object.
     *
     * These objects _must be trival_ meaning they must be fully
     * contained as a data structure
     *
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
         * \brief Register a trivial, copyable type
         * \param obj Pointer to the trival, copyable item to capture
         * \tparam ObjT The object type -- must be a pointer
         *
         * A trival, copyable type can be POD, a simple structure
         * containing POD types, structures with embedded POD types,
         * array types of PODs, or event STL types (vector, map, list,
         * string as long as their elements types are
         * copyable/assignable).
         *
         * How to use:
         * \code
         *   class MyCheckpointable : public sparta::Checkpointable
         *   {
         *   public:
         *        MyCheckpointable(sparta:TreeNode * my_node) :
         *            sparta::Checkpointable(my_node),
         *            my_checkpointable_integer_(allocateCheckpointable<declype(my_checkpointable_integer_)>())
         *        {}
         *
         *   private:
         *        uint32_t & my_checkpointable_integer_;
         *
         *   };
         * \endcode
         */
        template<class CheckpointableT>
        CheckpointableT & allocateCheckpointable()
        {
            static_assert(false == std::is_pointer_v<CheckpointableT>, "Checkpointable object cannot be a pointer");
            static_assert(std::is_fundamental_v<CheckpointableT>,
                          "Checkpointable object is not trivally copyable meaning the class is not self-contained");
            constexpr size_t checkpointable_size = utils::next_power_of_2(sizeof(CheckpointableT));
            auto & cp_component = checkpoint_components_.emplace_back(new CheckpointComponent(cp_node_,
                                                                                              checkpointable_size));

            auto cp_mem = cp_component->getRawDataPtr();
            return *(new (cp_mem) CheckpointableT);
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
