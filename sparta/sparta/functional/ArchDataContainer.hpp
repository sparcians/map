// <ArchDataContainer> -*- C++ -*-

/*!
 * \file ArchDataContainer.hpp
 * \brief Contains a number of ArchData pointers
 */

#ifndef __ARCHDATA_CONTAINER_H__
#define __ARCHDATA_CONTAINER_H__

#include <vector>

#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace sparta
{
    class ArchData;

    /*!
     * \brief Container class for any number of ArchData pointers owned
     * externally.
     *
     * Provides methods for adding and removing these ArchDatas
     */
    class ArchDataContainer
    {
    public:

        /*!
         * \brief ArchData must be able to invoke associateArchData_ and
         * disassociateArchData_ methods during construction and destruction
         */
        friend class ArchData;

        /*!
         * \brief Retrieves all ArchDatas associated with this TreeNode
         * so that children can use it to allocate their data.
         * \param target TreeNode from which to extract the ArchData.
         * \return ArchData* owned by this node. May be null if this node does
         * not provide an ArchData.
         *
         * Invokes sparta::TreeNode::getLocalArchData_ to actually get the
         * ArchData.
         *
         * This method exists as a non-virtual protected member so that
         * subclasses can call it but non-TreeNodes cannot. Subclasses cannot
         * call getLocalArchData_ directly because it is virtual.
         */
        std::vector<ArchData*> getAssociatedArchDatas() {
            return local_archdatas_;
        }

        /*!
         * \brief Const variant of getAssociatedArchDatas
         */
        const std::vector<ArchData*> getAssociatedArchDatas() const {
            return local_archdatas_;
        }

        /*!
         * \see sparta::PhasedObject::getLocation
         */
        virtual std::string getLocation() const = 0;

        /*!
         * \brief Default constructor
         */
        ArchDataContainer() = default;

        /*!
         * \brief Copy construction disabled
         */
        ArchDataContainer(ArchDataContainer&) = delete;

        /*!
         * \brief Move constructor
         */
        ArchDataContainer(ArchDataContainer&&) = default;

        /*!
         * \brief Virtual destructor
         */
        virtual ~ArchDataContainer() {}

    private:

        /*!
         * \brief Associates another ArchData for this Node.
         * \param ad ArchData to associate with this node. Must not be nullptr.
         * ad->getOwnerNode() must return this TreeNode. Has no effect if already
         * associated.
         * \note This can be called any number of times
         * \post The \a ad ArchData will be part of the result whenever
         * TreeNode::getAssociatedArchDatas is invoked (unless it is removed
         * with disassociateArchData)
         *
         * The purpose of this is to expose the ArchDatas in the tree for
         * consumption by checkpointers. Certain TreeNode subclasses
         * such as ResourceTreeNode, RegisterSet, StatisticSet, and possibly
         * more will automatically publish some internal ArchData object through
         * this method at construction.
         *
         * \todo Move to source so that we can verify that the ArchData owner
         * node is this node.
         */
        void associateArchData_(ArchData* ad) {
            if(!ad){
                throw SpartaException("associateArchData: ArchData pointer must not be nullptr. "
                                    "Error at node: ")
                    << getLocation();
            }
            // Commented currently because ArchData cannot guarantee this
            //if(ad->getOwnerNode() != this){
            //    throw SpartaException("associateArchData: ArchData owner node must be set as "
            //                        "the TreeNode with which it is being associated. Error at node: ")
            //        << getLocation();
            //}
            if(std::find(local_archdatas_.begin(), local_archdatas_.end(), ad)
               != local_archdatas_.end())
            {
                return; // Already in the list
            }

            local_archdatas_.push_back(ad);
        }

        /*!
         * \brief Disassociates the given ArchData
         * \param ad ArchData to disassociate. Has no effect if nullptr or not
         * currently associated.
         * \post ArchData ad will not be returned in getAssociatedArchDatas()
         * results.
         */
        void disassociateArchData_(ArchData* ad) {
            auto itr = std::find(local_archdatas_.begin(), local_archdatas_.end(), ad);
            if(itr != local_archdatas_.end()){
                local_archdatas_.erase(itr);
            }
        }

        /*!
         * \brief ArchDatas to be returned when getAssociatedArchDatas is
         * invoked on this
         */
        std::vector<ArchData*> local_archdatas_;
    };

} // namespace sparta

// __ARCHDATA_CONTAINER_H__
#endif
