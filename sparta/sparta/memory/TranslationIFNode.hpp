
/**
 * \file   TranslationIFNode.hpp
 *
 * \brief  Contains class for publishing a TranslationIF as a TreeNode
 */

#ifndef __TRANSLATION_INTERFACE_NODE_H__
#define __TRANSLATION_INTERFACE_NODE_H__

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/memory/TranslationIFNode.hpp"

namespace sparta
{
    namespace memory
    {
        /*!
         * \brief TranslationIF extension that builds on TranslastionIF, acting
         * as a TreeNode in the SPARTA device tree through which clients such as
         * tools and UIs can discover and interact with the interface.
         *
         * Clients can register SPARTA notifications on this interface's device
         * tree node
         *
         * \todo Add sparta Notification support
         */
        class TranslationIFNode : public sparta::TreeNode, public TranslationIF
        {
        public:

            //! \name Construction
            //! @{
            ////////////////////////////////////////////////////////////////////////

            TranslationIFNode() = delete;
            TranslationIFNode& operator=(const TranslationIFNode&) = delete;
            TranslationIFNode(const TranslationIFNode&) = delete;

            /*!
             * \brief Construct a blocking memory interface that is also a
             * sparta::TreeNode subclass
             * \param parent Parent TreeNode. Must not be null
             * \param group Group name. Must not be empty. See
             * sparta::TreeNode for rules
             * \param group_idx Group index. See sparta::TreeNode
             * \param desc Description of this interface. Must not be empty. See
             * sparta::TreeNode
             * \param input_type Name of input memory address type
             * \param output_type Name of output memory address type
             */
            TranslationIFNode(sparta::TreeNode* parent,
                              const std::string& name,
                              const std::string& group,
                              group_idx_type group_idx,
                              const std::string& desc,
                              const std::string& input_type,
                              const std::string& output_type) :
                TreeNode(notNull(parent), name, group, group_idx, desc),
                TranslationIF(input_type, output_type)
            { }

            /*!
             * \brief Constructor for TranslationIFNode without input_type and
             * output_type args
             *
             * This is the simplest constructor available for this class
             */
            TranslationIFNode(sparta::TreeNode* parent,
                              const std::string& name,
                              const std::string& group,
                              group_idx_type group_idx,
                              const std::string& desc) :
                TreeNode(notNull(parent),
                         name,
                         group,
                         group_idx,
                         desc),
                TranslationIF()
            { }

            /*!
             * \brief Constructor for TranslationIFNode without TreeNode group
             * information
             */
            TranslationIFNode(sparta::TreeNode* parent,
                              const std::string& name,
                              const std::string& desc) :
                TranslationIFNode(parent,
                                  name,
                                  sparta::TreeNode::GROUP_NAME_NONE,
                                  sparta::TreeNode::GROUP_IDX_NONE,
                                  desc)
            {
                // Delegated Constructor
            }

            /*!
             * \brief Constructor for TranslationIFNode without TreeNode group
             * information but still having input_type and output_type args
             */
            TranslationIFNode(sparta::TreeNode* parent,
                              const std::string& name,
                              const std::string& desc,
                              const std::string& input_type,
                              const std::string& output_type) :
                TranslationIFNode(parent,
                                  name,
                                  sparta::TreeNode::GROUP_NAME_NONE,
                                   sparta::TreeNode::GROUP_IDX_NONE,
                                  desc,
                                  input_type,
                                  output_type)
            {
                // Delegated Constructor
            }

            /*!
             * \brief Virtual Destructor
             */
            virtual ~TranslationIFNode() {}

            ////////////////////////////////////////////////////////////////////////
            //! @}
        };
    } // namespace memory
} // namespace sparta

#endif // __TRANSLATION_IF_NODE_H__
