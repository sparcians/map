// <PhasedObject> -*- C++ -*-

/*!
 * \file PhasedObject.hpp
 * \brief Basic Node framework in sparta device tree composite pattern
 */

#ifndef __PHASED_OBJECT_H__
#define __PHASED_OBJECT_H__

#include "sparta/utils/Utils.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/Printing.hpp"
#include "sparta/utils/StringManager.hpp"

namespace sparta
{

    /*!
     * \brief Object having a specific phase in the sparta construction paradigm
     * \see PhasedObject::TreePhase
     *
     * Contains methods for querying and setting phase with some transition
     * logic.
     */
    class PhasedObject
    {
    public:

        //! \name Types
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Current phase of tree construction (applies to node and
         * entire tree).
         * \note Phases listed here must be in sequence so that the can be
         * compared with > and < operators as well as ==.
         *
         * Tree construction steps through these discrete phases, which
         * restrict what operations can be done on the tree.
         */
        enum TreePhase {
            /*!
             * \brief Setting up tree hierarchy only (initial state)
             */
            TREE_BUILDING    = 0,

            /*!
             * \brief Setting parameters, ports, and other pre-instantiation
             * attributes. Hierarchy cannot be modified
             */
            TREE_CONFIGURING = 1,

            /*!
             * \brief Tree is being finalized, but has not completed (maybe
             * errors?)
             */
            TREE_FINALIZING  = 2,

            /*!
             * \brief Tree and all resources have been instantiated. No more
             * configuration/connection allowed
             */
            TREE_FINALIZED   = 3,

            /*!
             * \brief Simulation is complete. Tree and all resources are now
             * allowed to be deleted (phase does not imply that deletion will
             * occur)
             */
            TREE_TEARDOWN    = 4
        };

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Construction
        //! @{
        ////////////////////////////////////////////////////////////////////////


        PhasedObject() :
            phase_(TREE_BUILDING)
        { }

        PhasedObject(PhasedObject&&) = default;

        virtual ~PhasedObject()
        { }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Identification Attributes
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Gets the name of this node
         * \return string name of this node
         */
        virtual const std::string& getName() const = 0;

        /*!
         * \brief Returns the location of this node in device tree which can be
         * used to navigate the device tree in methods such as getChild if this
         * is fully attached to the device tree.
         * \return dotted absolute location string to (and including) this node
         * with some variations to indicate when the node is not yet part of a
         * tree.
         * \li If this node does not have a parent (getParent), then the
         * expected parent set through setExpectedParent_ will be used to
         * compute the location string and the expected-parent relationship will
         * be represented with a '.'.
         * \li If this node has no parent and no expected parent, then the
         * returned location will be the name of this node preceeded by a '~'
         * to indicate that the node is unattached.
         * \note If a node in the location has no name, it will not be
         * accessible using the string returned by this function and the
         * getChild/findChildren methods.
         *
         * The ',' is used to indicate expected, but unattached parents (set
         * through setExpectedParent_) which happens during construction to
         * avoid having to roll back any tree changes in the case of an
         * exception while validating a node about to be attached to the
         * tree.
         *
         * If this node 'c' is attached to parent 'b':
         * \verbatim
         * getLocation() => "top.a.b.c"
         * \endverbatim
         *
         * If this node 'c' is expecting parent 'b' and 'b' is has expected
         * parent 'a', which is already attached to parent 'top':
         * \verbatim
         * getLocation() => "top.a,b,c"
         * \endverbatim
         *
         * If this node 'c' has no parent, is not a RootTreeNode and has no
         * expected parent:
         * \verbatim
         * getLocation() => "~c"
         * \endverbatim
         */
        virtual std::string getLocation() const = 0;

        //! \name Phase Attributes
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Gets the trees current phase
         *
         * Initially TREE_BUILDING
         */
        TreePhase getPhase() const {
            return phase_;
        }

        /*!
         * \brief Is this node (and thus the entire tree above and below
         * it) currently in the TREE_BUILDING phase.
         * \return true if currently in the building phase
         *
         * Building phase allows new ResourceTreeNodes to be added to the tree.
         */
        virtual bool isBuilding() const {
            return phase_ == TREE_BUILDING;
        }

        /*!
         * \brief Is this node (and thus the entire tree above it) "built".
         * Checks that getPhase has passed TREE_BUILDING.
         * \return true if built.
         *
         * Being built prevents new nodes from being attached to the tree.
         */
        virtual bool isBuilt() const {
            return phase_ > TREE_BUILDING;
        }

        /*!
         * \brief Is this node (and thus the entire tree above it) "configured".
         * Checks that getPhase has \a passed TREE_CONFIGURING (i.e. FINALIZED,
         * FINALIZING, TEARDOWN, etc).
         * \return true if built.
         *
         * Being built prevents new nodes from being attached to the tree.
         */
        virtual bool isConfigured() const {
            return phase_ > TREE_CONFIGURING;
        }

        /*!
         * \brief Is this node (and thus the entire tree above it) currently in
         *  the TREE_CONFIGURING phase.
         * \return true if built.
         *
         * Being built prevents new nodes from being attached to the tree.
         */
        virtual bool isConfiguring() const {
            return phase_ == TREE_CONFIGURING;
        }

        /*!
         * \brief Is this node (and thus the entire tree above it) "finalized"
         * \return true if finalized.
         *
         * Being in TREE_FINALIZING prevents configuration but indicates that
         * the tree has not yet successfully finalized.
         */
        virtual bool isFinalizing() const {
            return phase_ == TREE_FINALIZING;
        }

        /*!
         * \brief Is this node (and thus the entire tree above it) "finalized"
         * \return true if finalized.
         * \note isFinalizing will not longer return true in this phase
         *
         * Being in TREE_FINALIZE prevents the tree from finalizing again.
         */
        virtual bool isFinalized() const {
            return phase_ == TREE_FINALIZED;
        }

        /*!
         * \brief Is this node (and thus the entire tree above it) in the
         * "teardown" phase
         * \return true if tearing down.
         * \note isFinalized will not longer return true in this phase
         *
         * Being in TREE_TEARDOWN supresses the exceptions normally thrown by
         * caused by deleting nodes.
         *
         * This phase exists in order to help catch the class of errors where
         * Nodes were allocated on the stack and deleted before the tree to
         * which they were attached is still being used. Tearing down indicates
         * that the tree should no longer be used.
         */
        virtual bool isTearingDown() const {
            return phase_ == TREE_TEARDOWN;
        }

    protected:

        /*!
         * \brief Sets the current phase
         * \pre Currently has no preconditions, but phase ording may be enforced
         * later
         * \todo Enforce phas ordering here
         */
        void setPhase_(TreePhase phase) {
            phase_ = phase;
        }

    private:

        /*!
         * \brief Phase of this node (always reflects entire tree)
         */
        TreePhase phase_;
    };

} // namespace sparta

// __PHASED_OBJECT_H__
#endif
