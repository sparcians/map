// <Checkpointable> -*- C++ -*-

#pragma once

#include <cassert>
#include <vector>
#include <list>
#include <memory>
#include <type_traits>
#include <cstring>

#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/functional/ArchData.hpp"

namespace
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
     * These objects _must be trivally copyable_ and assignable
     * components.  While the checkpointer does not actually perform
     * any assignments, it will perform a blind memory copy.
     *
     */
    class Checkpointable
    {
        class ObjectBase
        {
        public:
            virtual ~ObjectBase() {}
        }

        template<class ObjT>
        class ObjectView : public ObjectBase
        {
            static constexpr size_t obj_size = sizeof(std::remove_pointer_t<ObjT>);
        public:
            ObjectView(ObjT * obj, ArchData * adata_, ArchDataSegment::ident_type obj_id):
                obj_(obj),
                dview_(adata_, obj_id, obj_size,
                       ArchDataSegment::INVALID_ID, // subset of
                       0, // subset_offset
                       *obj)
            {}
        private:
            ObjT * obj_;
            DataView dview_;
        };

    public:
        Checkpointable(sparta::TreeNode * cp_node = nullptr) :
            adata_(this,
                   ARCH_DATA_LINE_SIZE,
                   ArchData::DEFAULT_INITIAL_FILL,
                   ArchData::DEFAULT_INITIAL_FILL_SIZE,
                   false) // Cannot delete lines
        {
            adata_.layout();
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
         */
        template<class ObjT>
        void registerCheckpointableObject(ObjT * obj)
        {
            static_assert(std::is_trivially_copyable_v<ObjT> || std::is_assignable_v<ObjT, ObjT>,
                          "Object is not copyable nor assignable.  Cannot checkpoint it");
            objects_.emplace_back(new ObjectView<ObjT>(obj, &adata_, obj_id_++));
        }

    private:
        //! \brief ArchData that will hold snapshots of this Checkpointable
        ArchData adata_;

        //! \brief Object ID tracked by the DataView
        ArchDataSegment::ident_type obj_id_ = 0;

        //! \brief Objects registered
        std::vector<std::unique_ptr<ObjectBase>> objects_;

    };
}
