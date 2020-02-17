// <TreeNode> -*- C++ -*-


/*!
 * \file TreeNode.hpp
 * \brief Basic Node framework in sparta device tree composite pattern
 */

#ifndef __TREE_NODE_H__
#define __TREE_NODE_H__

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <string>
#include <ostream>
#include <vector>
#include <sstream>
#include <set>
#include <map>
#include <unordered_map>
#include <regex>
#include <functional>
#include <memory>
#include <type_traits>
#include <typeinfo>
#include <utility>

#include "sparta/utils/StaticInit.hpp"
#include "sparta/simulation/ResourceContainer.hpp"
#include "sparta/functional/ArchDataContainer.hpp"
#include "sparta/utils/Utils.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/Printing.hpp"
#include "sparta/utils/StringManager.hpp"
#include "sparta/kernel/PhasedObject.hpp"

namespace sparta {
class PostRunValidationInfo;
}  // namespace sparta

#ifndef TREENODE_LIFETIME_TRACE
/*!
 * \brief Enables tracing of TreeNode lifetimes in a set of output txt files.
 *
 * This can be used along with the tools/check_treenode_dtors.py script.
 * Generates construction.txt and destruction.txt files. Delete these files
 * before running the simulator again since they are always appended to and
 * never overwritten
 */
//#define TREENODE_LIFETIME_TRACE 1
#endif // #ifndef TREENODE_LIFETIME_TRACE


//! \brief Alphanumeric characters (valid for TreeNode names and groups)
#define ALPHANUM_CHARS \
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890_"

//! \brief Digit characters (for valid TreeNode groups)
#define DIGIT_CHARS "0123456789"

/*!
 * \brief Reserved words in Python language, Python builtins, and other
 * reservations. TreeNode names and groups are note allowed to exactly match
 * any of these (case sensitive)
 */
#define RESERVED_WORDS {"and",     "del",       "from",        "not",       \
                        "while",   "as",        "elif",        "global",    \
                        "or",      "with",      "assert",      "else",      \
                        "if",      "pass",      "yield",       "break",     \
                        "except",  "import",    "print",       "class",     \
                        "exec",    "in",        "raise",       "continue",  \
                        "finally", "is",        "return",      "def",       \
                        "for",     "lambda",    "try",         "__init__",  \
                        "__del__", "__cinit__", "__dealloc__",              \
                        /* End of Python reserved names */                  \
                        "name",    "author",    "report",      "content",   \
                        "include",                                          \
                        /* End of SPARTA Report definition reserved nodes */  \
                        "extension"                                         \
                        /* End of SPARTA extensions reserved words */         \
                        }

/*!
 * \brief Convenience macro for registering for a notification on a TreeNode
 * \param func Name of class member function that will be called when the
 * specified notification is posted. This function must have one of the
 * following signatures:
 * -# \code
 * ClassName::func( // TreeNode from which the notification originated
 *                  const TreeNode& origin_node,
 *                  // TreeNode at which the notifications was observed,
 *                  // causing this callback
 *                  const TreeNode& observation_node,
 *                  // Notification event data
 *                  const datat& data );
 * \endcode
 * -# \code
 * ClassName::func( // Notification event data
 *                  const datat& data );
 * \endcode
 * \param datat type of notification data for which this notification will
 * register. See sparta::TreeNode::registerForNotification for information on how
 * these types can be found in the tree.
 * \param name Name of the notification to register for. If "", will receive all
 * notifications from the subtree matching \a datat. See
 * sparta::TreeNode::registerForNotification for more details about the \a name
 * parameter.
 * \throw SpartaException if a registration is made on a datat/name combination
 * that cannot be posted by this the node's subtree (since there are no ancestor
 * NotificationSource nodes matching datat/name).
 * \note Deregister with DEREGISTER_FOR_NOTIFICATION. Failing to do so may cause
 * errors.
 *
 * This macro must be used within the class instance for which the callback
 * function is being registered. This macro uses the \a this pointer to
 * determine the observer class.
 *
 * This macro resolves to a function call to "registerForNotification" which is
 * should be used in a context such that it refers to
 * sparta::TreeNode::registerForNotification.
 *
 * Example
 * \code
 * MyClass::setup() {
 *   node->REGISTER_FOR_NOTIFICATION(handle_int1, int, "int_happened");
 *   node->REGISTER_FOR_NOTIFICATION(handle_int2, int, "int_happened");
 * }
 * MyClass::destroy() {
 *   node->DEREGISTER_FOR_NOTIFICATION(handle_int1, int, "int_happened");
 *   node->DEREGISTER_FOR_NOTIFICATION(handle_int2, int, "int_happened");
 * }
 * MyClass::handle_int1(const TreeNode& origin,
 *                      const TreeNode& observation_node,
 *                      const int& data) {
 *   (void) origin;
 *   (void) observation_node;
 *   (void) data;
 *   // ...
 * }
 * MyClass::handle_int2(const int& data) {
 *   (void) data;
 *   // ...
 * }
 * \endcode
 */
#define REGISTER_FOR_NOTIFICATION(func, datat, name)                    \
    registerForNotification<datat,                                      \
                            typename std::remove_reference<decltype(*this)>::type, \
                            &std::remove_reference<decltype(*this)>::type::func>(this, name);

/*!
 * \brief Convenience macro for deregistering for a notification on a TreeNode.
 *
 * \see REGISTER_FOR_NOTIFICATION
 */
#define DEREGISTER_FOR_NOTIFICATION(func, datat, name)                  \
    deregisterForNotification<datat,                                    \
                              typename std::remove_reference<decltype(*this)>::type, \
                              &std::remove_reference<decltype(*this)>::type::func>(this, name);

#define NOTIFY(func, datat, name) \


namespace sparta
{
    namespace app {
        class Simulation;
    }

    class Clock;
    class VirtualGlobalTreeNode;
    class ClockManager;
    class TreeNodePrivateAttorney;
    class ParameterSet;
    class Scheduler;
    class ExtensionDescriptor;

    typedef std::vector<std::unique_ptr<ExtensionDescriptor>> ExtensionDescriptorVec;

    //    /*!
    //     * \brief RAII base class for notification registeration
    //     */
    //    class NotificationRegistration
    //    {
    //        NotificationRegistration() = default;
    //
    //        /*!
    //         * \brief Copy construction disabled for base class
    //         * \note Prevents slicing
    //         */
    //        NotificationRegistration(const NotificationRegistration&) = delete;
    //
    //        /*!
    //         * \brief Move construction disabled for base class
    //         * \note Prevents slicing
    //         */
    //        NotificationRegistration(NotificationRegistration&&) = delete;
    //
    //        /*!
    //         * \brief Copy-assignment disabled for base class
    //         * \note Prevents slicing
    //         */
    //        NotificationRegistration& operator=(const NotificationRegistration&) = delete;
    //
    //        virtual ~NotificationRegistration() {;}
    //    };
    //
    //    /*!
    //     * \brief RAII notification registration with naked pointer as callback. Implements
    //     * NotificationRegistration
    //     */
    //    template <typename DataT, typename T, void (T::*TMethod)(const TreeNode&, const TreeNode&, const DataT&)>
    //    class PtrNotificationRegistration : private NotificationRegistration {
    //
    //
    //        PtrNotificationRegistration(TreeNode* n, T* obj, const std::string& name, bool ensure_possible) {
    //            n->registerForNotification<DataT, T, TMethod>
    //        }
    //
    //        PtrNotificationRegistration(TreeNode* n, T* obj, const std::string& name) :
    //            PtrNotificationRegistration(n, obj, name, true)
    //        {
    //
    //        }
    //
    //
    //
    //        ~PtrNotificationRegistration() {
    //
    //        }
    //    };

    /*!
     * \brief Node in a composite tree representing a sparta DeviceTree.
     * \warning TreeNode construction is NOT thread-safe
     * \note Children cannot be removed from their parents once attached
     * \note Not all TreeNodes in existance are required to be part of the same tree.
     * \note TreeNodes expect to be attached to a root node. See sparta::RootTreeNode
     * for details. This root is required to act on the tree and hold some tree-wide
     * properties. Multiple RootTreeNodes are expected to exist.
     *
     * Because this class expects to be used with a composite pattern,
     * several refinements exist elsewhere in sparta.
     *
     * <h4>Important</h4>
     * Searching and getting child nodes is done by getChild, findChildren, and
     * getChildAt only.
     *
     * <h4>Teardown</h4>
     * TreeNodes cannot be removed from the tree once added until the Tree
     * enters the TREE_TEARDOWN phase (see isTearingDown). Place the Tree's
     * RootTreeNode into the teardown phase using
     * sparta::RootTreeNode::enterTeardown.
     */
    class TreeNode : public ResourceContainer, public ArchDataContainer
    {
    public:
        /*!
         * \brief Allow this class access internals for handling notification
         * observation registration/deregistration broadcasts in a way that does
         * not fit the composite tree pattern.
         */
        friend class VirtualGlobalTreeNode;

        /*!
         * \brief Allow ClockManager to directly change phase on nodes
         */
        friend class ClockManager;

        /*!
         * \brief This class is responsible for instantiating the static
         * container held by this TreeNode
         */
        friend class SpartaStaticInitializer;

        /*!
         * \brief Friend an attorney pattern that can expose access to
         * getAllChildren_ or other methods that bypass privacy_level_.
         */
        friend class TreeNodePrivateAttorney;

        //! \name Types
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Type of unique-identifier assigned to every node
         */
        typedef uint64_t node_uid_type;

        /*!
         * \brief Vector of TreeNode children
         */
        typedef std::vector<TreeNode*> ChildrenVector;

        /*!
         * \brief Vector of aliases (other names for this node)
         */
        typedef std::vector<std::string> AliasVector;

        /*!
         * \brief Mapping of names, aliases, and groups to individual child
         * nodes within one node. This must be in a deterministic order, so
         * an ordered container (e.g. std::map) is required. A sorted contained
         * is probably desirable, but not required
         */
        typedef std::map<std::string, TreeNode*> ChildNameMapping;

        /*!
         * \brief Index within a group
         */
        typedef uint32_t group_idx_type;

        /*!
         * \brief Weak pointer to a TreeNode. Acquire with getWeakPtr
         */
        typedef std::weak_ptr<TreeNode> WeakPtr;

        /*!
         * \brief Weak pointer to a const TreeNode. Acquire with getWeakPtr
         */
        typedef std::weak_ptr<const TreeNode> ConstWeakPtr;

        /*!
         * \brief Shared pointer to TreeNode. Acquire with WeakPtr::lock().
         */
        typedef std::shared_ptr<TreeNode> SharedPtr;

        /*!
         * \brief Map of strings (interned in StringManager) tags to TreeNodes
         */
        typedef std::map<const std::string*, std::vector<TreeNode*>> TagsMap;

        /*!
         * \brief Type for indicating that ANY notification source type
         * should be included in a search performed by locateNotificationSources
         *
         * Note that this type cannot be used to observe multiple notification
         * types because no one callback signature could support this cleanly.
         */
        struct ANY_TYPE {};

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Constants
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief GroupIndex indicating that a node has no group index because
         * it belongs to no group
         */
        static const group_idx_type GROUP_IDX_NONE = (group_idx_type)-1;

        /*!
         * \brief Maximum value of node_uid_ before the framework throws an
         * exception.
         */
        static const node_uid_type MAX_NODE_UID;

        /*!
         * \brief Group name indicating that a node belongs to no group
         */
        static constexpr char GROUP_NAME_NONE[] = "";

        /*!
         * \brief Node name for anonymous node
         */
        static constexpr char NODE_NAME_NONE[] = "";

        /*!
         * \brief String dividing sections in a debug dump file
         */
        static const std::string DEBUG_DUMP_SECTION_DIVIDER;

        /*!
         * \brief Node name for the virtual glopbal node
         */
        static constexpr char NODE_NAME_VIRTUAL_GLOBAL[] = "_SPARTA_virtual_global_";

        /*!
         * \brief Separator character between node identifiers in a location
         * string when the child is attached to the parent
         * \warning Do not change these as builtin logic and documentation
         * depends on these constants.
         */
        static constexpr char LOCATION_NODE_SEPARATOR_ATTACHED = '.';

        /*!
         * \brief Separator character between node identifiers in a location
         * string when the child is being attached to the parent but has not
         * been entirely attached (i.g. during construction of the child node)
         * \warning Do not change these as builtin logic and documentation
         * depends on these constants.
         */
        static constexpr char LOCATION_NODE_SEPARATOR_EXPECTING = ',';

        /*!
         * \brief Separator character preceding a node identifiers in a location
         * string when that node has no parent and is not in the process of
         * being attached to one.
         * \warning Do not change these as builtin logic and documentation
         * depends on these constants.
         */
        static constexpr char LOCATION_NODE_SEPARATOR_UNATTACHED = '~';

        /*!
         * \brief Spaces to indent for each tree level in
         * sparta::TreeNode::renderSubtree
         * \note This is cosmetic only
         */
        static const uint32_t RENDER_SUBTREE_INDENT = 2;

        /*!
         * \brief Reserved name for built-in nodes.
         *
         * Nodes in this group are exempt from certain restrictions and
         * filtered from printing and searching operations.
         */
        static constexpr char GROUP_NAME_BUILTIN[] = "_SPARTA_builtin_group_";

        /*!
         * \brief Threshold for number of findChildren calls after finalization
         * before a warning message is printed about framework misuse.
         *
         * This value should be large enough that a few accesses by tools
         * or lookups for caching pointers to other objects are allowed, but
         * small enough that a findChildren call happening every N cycles
         * of execution in the simulator will be detected during runs that
         * take more than a few minutes. Consider that Parameters are also part
         * of a tree and ParameterSet or RegisterSet nodes may be the target of
         * some getChild calls after finalization, but generaly these nodes
         * should be cached in the requesting component.
         *
         * \note Reports applied after starting can search the tree, causing
         * many accesses. This constant requires a large value to not print
         * warnings in this situation
         * \todo getChild/findChildren should know the difference between
         * report-based accesses and model-based accesses so that this can be
         * conditionally incremented
         */
        static const uint64_t CHILD_FIND_THRESHOLD = 100000;

        /*!
         * \brief Threshold for number of getChild calls after finalization
         * before a warning message is printed about framework misuse.
         *
         * See CHILD_FIND_THRESHOLD for explanation of threshold value choice
         */
        static const uint64_t CHILD_GET_THRESHOLD = 100000;

        /*!
         * \brief Number of teardown-phase-related messages that can be printed
         * before the rest will be suppressed.
         */
        static const uint32_t TEARDOWN_ERROR_LIMIT = 5;

        /*!
         * \brief List of pattern susbtitutions when creating a search pattern
         * from a TreeNode name containing wildcards.
         */
        static const std::vector<std::pair<const char*, std::function<void (std::string&)>>> TREE_NODE_PATTERN_SUBS;

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Diagnostics
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Method to put the device tree in lockdown phase.
         * All LOCKED and HIDDEN parameters are frozen after this point.
         * Regular parameters are not affected by this phase.
         * This method requires a Simulation pointer and will assert at
         * compile time if anyone tries to lockdown the tree without the
         * context of a Simulation.
         */
        //! \brief Lockdown the tree node if Simulation is valid
        void lockdownParameters(){
            if(getRoot()->getPhase() != sparta::PhasedObject::TreePhase::TREE_BUILDING and
               getRoot()->getPhase() != sparta::PhasedObject::TreePhase::TREE_CONFIGURING){
                throw SpartaException("Device tree with root \"")
                    << getRoot()->getLocation() << "\" not currently in the TREE_BUILDING phase or "
                    << "TREE_CONFIGURING phase, so it cannot enter TREE_LOCKDOWN";
            }
            lockdownParametersUtil_();
        }

        static const std::map<const TreeNode*, WeakPtr>& getParentlessNodes();

        /*!
         * \brief Gets the vector of all TreeNodes currently known to be
         * constructed
         */
        static const std::map<const TreeNode*, WeakPtr>& getAllNodes();

        /*!
         * \brief Prints the list of all TreeNodes currently known to be
         * constructed.
         */
        static std::string formatAllNodes();

        /*!
         * \brief Is a given node constructed?
         * \note This is used for debugging double-frees
         */
        static bool isNodeConstructed(const TreeNode*);

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Construction
        //! @{
        ////////////////////////////////////////////////////////////////////////

        //! \brief Not default-constructable
        TreeNode() = delete;

        /*!
         * \brief Not copy-constructable
         */
        TreeNode(const TreeNode&) = delete;

        /*!
         * \brief Move constructor
         * \pre rhp must not be fully finalized
         * \pre rhp must not have any observers registered directly on it
         * \pre Avoid move-constructing from TreeNodes with children as
         * the children may fail to be re-added to this new TreeNode
         * if they attempt to dynamic_cast this TreeNode's pointer to a subclass
         * of TreeNode.
         */
        TreeNode(TreeNode&& rhp);

        //! \brief Not assign-constructable
        TreeNode& operator=(const TreeNode&) = delete;

        /*!
         * \brief TreeNode full constructor. Initializes node and adds it as a
         * child of an existing tree node (if parent is not null).
         * \warning Subclasses should generally not use this constructor (or
         * should pass a null parent instead) because this constructor can
         * invoke addChild, which notifies observers that a child or parent is
         * being added. Observers of these notifications tend to require fully
         * constructed clases because they dynamically cast the TreeNode pointer
         * to the intended tree type to find out its actual type. Then, it may
         * examine the fields of the TreeNode subclass which may not yet be
         * fully populated.
         *
         * \param parent TreeNode to which this will be added as a child. If
         * this constructor is being invoked by a subclass, parent must be
         * nullptr (or use a constructor with no parent argument). Subclass
         * constructor is then responsible for adding the node.
         * \param name Name of this node. Name must satisfy validateName. May be
         * NODE_NAME_NONE to indicate anonymous node. A group and group index is
         * required for anonymous nodes. name must not be the same as any alias
         * or name of another child of \a parent. It is HIGHLY recommended that
         * name end in a number if there are multiple instances of similar
         * objects at the same level in the tree (e.g. core0, core1). Even if
         * only one such object exists at some level in the tree, having a
         * numeric suffix now will ensure it is easy to adapt Python scripts to
         * handle muliple instanecs later.
         * \param group Group name of this node. Must satisfy validateGroup. May
         * be GROUP_NAME_NONE if a valid node name is given and group_idx ==
         * GROUP_IDX_NONE.
         * \param group_idx Group index within \a group. Must satisfy
         * validateGroup. Must be GROUP_IDX_NONE unless group !=
         * GROUP_NAME_NONE.
         * \param desc Description of this TreeNode. Must satisfy validateDesc
         * \param is_indexable Is this node indexable by its parent such that
         * it can be accessed by group and index. Set this to false if
         * children will have colliding group numbers and indexes (e.g. a banked
         * register set). In almost all cases, this should be true.
         * \note TreeNode construction is NOT thread safe
         *
         * <h4>Example 1: Using groups to identify a sequence of related
         * elements</h4>
         * \verbatim
         * // given TreeNode* parent with name "parent"
         * TreeNode foo2(&parent, "foo2", "foo", 2, "This is a Foo...")
         * TreeNode foo3(&parent, "foo3", "foo", 3, "This is a Foo...")
         * \endverbatim
         * In a Python shell atop sparta, \a might allow the following means of
         * accessing foo:
         * \verbatim
         * >>> parent.foo
         * <Group of 2 Nodes>
         * >>> parent.foo2 # Node name
         * <foo2 node>
         * >>> parent.foo3
         * <foo3 node>
         * >>> parent.foo[2] # Node group[index]
         * <foo2 node>
         * >>> parent.foo[3]
         * <foo3 node>
         * >>> for n in parent.foo: print n # Iterate Node group
         * <foo2 node>
         * <foo3 node>
         * \endvarbatim
         *
         * <h4>Example 2: Using groups for Nodes which have no siblings</h4>
         * Even if you will only have one node having a certain name prefix at
         * some point in the sparta tree (e.g. "soc"), it may make sense to make
         * it indexable. One day, a multi-soc/board simulation may need to be
         * created.
         * \verbatim
         * // given TreeNode* parent with name "parent"
         * TreeNode foo(&parent, "foo", "foo", 0, "This is a Foo...")
         * \endverbatim
         * In a Python shell atop sparta, \a might allow the following means of
         * accessing foo:
         * \verbatim
         * >>> parent.foo # Name
         * <foo node>
         * >>> parent.foo[0] # Group
         * <foo node>
         * >>> for f in parent.foo: print f
         * <foo node>
         * \endverbatim
         */
        TreeNode(TreeNode* parent,
                 const std::string& name,
                 const std::string& group,
                 group_idx_type group_idx,
                 const std::string& desc,
                 bool is_indexable);

        /*!
         * \brief TreeNode constructor with no is_indexable parameter [defaults
         * to true]
         * \see other sparta::TreeNode constructors
         */
        TreeNode(TreeNode* parent,
                 const std::string& name,
                 const std::string& group,
                 group_idx_type group_idx,
                 const std::string& desc);

        /*!
         * \brief TreeNode constructor with no parent Node
         * \see other sparta::TreeNode constructors
         */
        TreeNode(const std::string& name,
                 const std::string& group,
                 group_idx_type group_idx,
                 const std::string& desc);

        /*!
         * \brief TreeNode constructor with no group information
         * \see other sparta::TreeNode constructors
         * \warning Subclasses should generally not use this constructor (or
         * should pass a null parent instead) because this constructor can
         * invoke addChild, which requires a TreeNode subclass (at this) to be
         * fully constructed
         *
         * group defaults to GROUP_NAME_NONE. group_idx defaults to
         * GROUP_IDX_NONE.
         */
        TreeNode(TreeNode* parent,
                 const std::string& name,
                 const std::string& desc);

        /*!
         * \brief TreeNode constructor for anonymous node with group information
         * \see other sparta::TreeNode constructors
         * \note group and group_idx must be valid
         * \warning Subclasses should generally not use this constructor (or
         * should pass a null parent instead) because this constructor can
         * invoke addChild, which requires a TreeNode subclass (at this) to be
         * fully constructed
         *
         * Name defaults to NODE_NAME_NONE. group and group_idx should be
         * meaningful
         */
        TreeNode(TreeNode* parent,
                 const std::string& group,
                 group_idx_type group_idx,
                 const std::string& desc);

        /*!
         * \brief TreeNode constructor with no parent node or group information
         * \see other sparta::TreeNode constructors
         *
         * group defaults to GROUP_NAME_NONE. group_idx defaults to
         * GROUP_IDX_NONE.
         */
        TreeNode(const std::string& name,
                 const std::string& desc);

        /*!
         * \brief Virtual destructor
         * \throw SpartaException if node is attached to a root (via isAttached)
         * and tree is not in TREE_TEARDOWN phase
         */
        virtual ~TreeNode();

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Identification
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Add a single alias for this node.
         * \param alias Alias to assign to this node. alias is subject to the
         * same constraints as a node name and must satisfy validateName.
         * Additionally, aliases must not be empty strings
         * \pre Node must not yet have a parent. When adding a node as a child,
         * the parent checks for alias, name, group, and group+idx collisions
         * amongst all its children
         * \pre No alias with the same name may be present in this node
         * \pre alias cannot match any name, alias, or group of another child of
         * this node's parent.
         * \pre Tree cannot be built (Not having a parent should hide this
         * precondition)
         */
        void addAlias(const std::string& alias);

        /*!
         * \brief Adds each element of a vector of aliases to this node..
         * \param v Vector of alias strings. Each alias in the vector is subject
         * to the conditions of addAlias
         * \pre See addAlias
         */
        void addAliases(const AliasVector& v);

        /*!
         * \brief Adds single tag to this node
         * \param tag Tag to add to this node. Tag is subject to the same
         * constraints as a node name and must satisfy validateName.
         * Additionally, tags must not be empty strings
         * \note Tags cannot be removed once added
         * \pre Tree cannot be in or past TREE_FINALIZED phase
         * \pre No tag by this name may already be present. Since tag data may
         * come from multiple places in the simulator source code, tags of the
         * same name may have different semantics depending on who added them.
         * This test ensures that one node does not have tags (meta-data) with
         * conflicting semantics. Generally, the client should add each tag only
         * once without checking to see if it already present. This will
         * identify duplicate tags having different semantics
         * \post Adds tag to global_tags_map_. See findChildrenByTag
         */
        void addTag(const std::string& tag);

        /*!
         * \brief Adds each elements of a vector of tags to this node
         * \param v Vector of tag strings. Each tag in the vector is subject to
         * the conditions of addTag
         * \pre See addTag
         */
        void addTags(const std::vector<std::string>& v);

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Node-Representation
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Create a string representation of this node
         * \param pretty Print a more verbose, multi-line representaiton (if
         * available).
         * \return string representation of this node "<" <location> ">"
         * \note this representation contains some basic information and is not
         * meant for deserialization.
         *
         * Subclasses should override this with representations appropriate for
         * their type.
         */
        virtual std::string stringize(bool pretty=false) const {
            (void) pretty;
            std::stringstream ss;
            ss << "<" << getLocation();
            if(is_expired_){
                ss << " EXPIRED";
            }
            stringizeTags(ss);
            ss << ">";
            return ss.str();
        }

        /*!
         * \brief Render tags to a string in the form:
         * " tags:[tag0, tag1]"
         * If there are any tags. The leading space makes this a useful
         * sub-utility of stringize because if there are no tags, returns empty
         * string with no leading space
         * \param ss stringstream to write to
         */
        void stringizeTags(std::stringstream& ss) const {
            if(tags_.size() > 0){
                ss << " tags:[";
                uint32_t i = 0;
                for(auto & tag : tags_){
                    if(i != 0){
                        ss << ", ";
                    }
                    ss << *tag;
                    ++i;
                }
                ss << "]";
            }

        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Node Attributes
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Gets the unique ID of this node
         */
        node_uid_type getNodeUID() const;

        /*!
         * \brief Gets the name of this node
         * \return string name of this node
         */
        const std::string& getName() const override;

        /*!
         * \brief Gets the name of this node's string pointer which isinterned
         * in StringManager)
         * \return String pointer. Guaranteed not to be null
         */
        const std::string* getNamePtr() const;

        /*!
         * \brief Is this node anonymous
         * \return true if node is anonymous
         */
        bool isAnonymous() const;

        /*!
         * \brief Is this expired (i.e. has it been the rhp of a move
         * constructor)
         * \return true if node is expired
         */
        bool isExpired() const;

        /*!
         * \brief Returns whether this object is accessible through its parent's
         * interfaces for getting children by group and index
         */
        bool isIndexableByGroup() const;

        /*!
         * \brief Gets the group name of this node
         * \return string group name of this node
         * \see GROUP_NAME_NONE
         */
        const std::string& getGroup() const;

        /*!
         * \brief Gets the group name string pointer (interned in StringManager
         * singleton) of this node.
         * \return string pointer to group name of this node which can be
         * compared with other strings interned in StringManager
         * \see GROUP_NAME_NONE
         */
        const std::string* getGroupNamePtr() const;

        /*!
         * \brief Gets the group index of this node
         * \return string group index of this node
         * \see GROUP_IDX_NONE
         */
        group_idx_type getGroupIdx() const;

        /*!
         * \brief Gets the set of tags associated with this TreeNode.
         * \note This cannot change after finalization
         * \return const vector of const string pointers, which will never be
         * null. These tags are stored as pointers into strings held by
         * StringManager to save space and often improve comparison speed
         *
         * Tags are useful for associating meta-data with a TreeNode and forming
         * virtual groupings of nodes independent of hierarchy. For example,
         * all nodes (e.g. counters and notification sources) having to do with
         * power-modeling may have "power" tags indicating presence in a virtual
         * "power" group.
         */
        const std::vector<const std::string*>& getTags() const;

        /*!
         * \brief Does this node have a specific tag (by name)
         */
        bool hasTag(const std::string& tag) const;

        /*!
         * \brief Does this node have a specific tag (by string pointer interned
         * with StringManager). This is faster than the alternate hasTag method
         * because it relies only on pointer comparisons.
         * \param[in] interned_tag_name Tag pointer. Caller must get this
         * pointer argument from sparta StringManager or it may not match
         */
        bool hasTag(const std::string* interned_tag_name) const;

        /*!
         * \brief Gets the description of this node
         * \return string description of this node
         */
        const std::string& getDesc() const;

        /*!
         * \brief Is this node in the builtins group.
         * \return true if this node is in the GROUP_NAME_BUILTIN group.
         */
        bool isBuiltin() const;

        /*!
         * \brief Marks this TreeNode hidden for the purposes of printint out
         * nodes. This does not make the node inaccessible, but causes it (and
         * its subtree) to be hidden from typical recursive tree printouts
         */
        void markHidden(bool hidden=true);

        /*!
         * \brief Is this TreeNode supposed to be hidden during tree printouts
         * This value does not have to be respected by anything using TreeNode
         * and is mainly a UI/printout convenience
         *
         * Defaults to false at construction
         */
        bool isHidden() const;

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Node Validation
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Validates the given name string for this TreeNode. Does not
         * consider context  (e.g. name collisions)
         * \throw SpartaException if the name is invalid.
         *
         * Constraints:
         * \li Name must not exactly match any names in sparta::RESERVED_WORDS
         * \li Name must not begin with a decimal digit
         * \li Name must not contain two adjacent underscores
         * \li Name must contain only alphanumeric characters and underscores
         * (see sparta::ALPHANUM_CHARS).
         */
        void validateName(const std::string& nm);

        /*!
         * \brief Validates the given group name string and group index for
         * this TreeNode. Does not consider context  (e.g. name collisions)
         * \param name Node name
         * \param group Group name
         * \param idx Group index
         * \throw SpartaException if the group name and group index combination
         * is invalid (i.e. already used by another node)
         *
         * Constraints:
         * \li group must not exactly match any names in sparta::RESERVED_WORDS
         * \li group must not begin with a decimal digit
         * \li Name must not contain two adjacent underscores
         * \li group must contain only alphanumeric characters and underscores
         * (see sparta::ALPHANUM_CHARS).
         * \li group cannot end with a decimal digit
         * \li index must be sparta::TreeNode::GROUP_IDX_NONE if group is
         * sparta::TreeNode::GROUP_NAME_NONE
         * \li index must be sparta::TreeNode::GROUP_IDX_NONE if group is
         * sparta::TreeNode::GROUP_NAME_BUILTIN.
         * \li index must not be sparta::TreeNode::GROUP_IDX_NONE if group is not
         * empty and not sparta::TreeNode::GROUP_NAME_BUILTIN.
         * \li if node name is sparta::TreeNode::NODE_NAME_NONE, group must not be
         * GROUP_NAME_NONE.
         */
        void validateGroup(const std::string& name,
                           const std::string& group,
                           group_idx_type idx);

        /*!
         * \brief Validates the given description string for this TreeNode.
         * \pre name_ must be assigned for this instance. It will be included
         * in the exception message
         * \throw SpartaException if the string is invalid
         *
         * Constraints:
         * \li desc must not be empty
         */
        void validateDesc(const std::string& desc);

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Tree Navigation & State
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Is this node part of a device tree with a proper RootTreeNode
         * at the root
         * \return true if node is a child of a RootTreeNode regardless of how
         * many tree levels separate them
         */
        virtual bool isAttached() const {
            return is_attached_;
        }

        /*!
         * \brief Gets immediate parent of this node if one exists.
         * \return parent TreeNode if there is one. Otherwise returns 0.
         */
        virtual TreeNode* getParent() {
            return parent_;
        }

        /*!
         * \overload virtual TreeNode* getParent()
         */

        virtual const TreeNode* getParent() const {
            return parent_;
        }

        /*!
         * \brief Retrieves a parent casted to type T* if this node has a parent
         * \tparam T Type of parent to cast to
         * \param must_exist If true, Causes an exception to be thrown if
         * node has no parent or the parent cannot be dynamically cast to T*. If
         * false, a nullptr is returned when this node has no parent that can be
         * cast to T*.
         * \return Parent as a T* if the parent exists and was castable to T*
         * using dynamic_cast. If this node has no parent which can be cast to
         * T*, and must_exist == true, throws. Otherwise, returns nullptr
         * \throw SpartaException if must_exist==true and the parent was null or
         * the parent could not be cast to a T*.
         */
        template <class T, typename = typename std::enable_if<std::is_pointer<T>::value>::type>
        const T getParentAs(bool must_exist=true) const {
            const T result = dynamic_cast<const T>(getParent());
            if(result == nullptr){
                if(must_exist){
                    throw SpartaException("Could not get parent of ")
                        << getLocation() << " with type: const " << demangle(typeid(T*).name());
                }
                return nullptr;
            }
            return result;
        }

        // Overload of getParentAs for const access with a non-pointer T type
        template <class T, typename = typename std::enable_if<!std::is_pointer<T>::value>::type>
        const T* getParentAs(bool must_exist=true) const {
            return getParentAs<const T*>(must_exist);
        }

        // Overload of getParentAs for non-const access with a pointer T type
        template <class T, typename = typename std::enable_if<std::is_pointer<T>::value>::type>
        T getParentAs(bool must_exist=true) {
            T result = dynamic_cast<T>(getParent());
            if(result == nullptr){
                if(must_exist){
                    throw SpartaException("Could not get parent of ")
                        << getLocation() << " with type: const " << demangle(typeid(T*).name());
                }
                return nullptr;
            }
            return result;
        }

        // Overload of getParentAs for non-const access with a non-pointer T type
        template <class T, typename = typename std::enable_if<!std::is_pointer<T>::value>::type>
        T* getParentAs(bool must_exist=true) {
            return getParentAs<T*>(must_exist);
        }

        /*!
         * \brief Gets farthest ancestor of this node.
         * \return farthest ancestor TreeNode. May be this if node has no
         * parent. Return value will never be nullptr
         * \note returned TreeNode may not be an actual RootTreeNode
         */
        virtual TreeNode* getRoot();

        /*!
         * \overload virtual TreeNode* getRoot()
         */
        virtual const TreeNode* getRoot() const;

        /*!
         * Returns the root of the scope that this tree node is in.
         */
        TreeNode *getScopeRoot();

        /*!
         * Returns the root of the scope that this tree node is in.
         */
        const TreeNode *getScopeRoot() const;

        /*!
         * \brief build-time equivalent to getRoot before an object is actually
         * attached to a tree. This is a mainly a framework and debugging tool
         * for determining what the root of a node being constructed will be
         * when complete
         */
        const TreeNode* getExpectedRoot() const;

        /*!
         * \brief Gets the simulation (if any) associated with this tree.
         * \pre This node should be attached (isAttached) or this will return
         * nullptr
         * \note Function gets RootTreeNode of tree and asks it for Simulation
         * pointer (if any)
         */
        app::Simulation* getSimulation() const;

        /*!
         * \brief Gets the number of children that this node has including those
         * in the sparta builtins group.
         * \return Integer representing number of children in any group.
         *
         * \warning This will only return the public children. Private children
         *          must be accessed via the TreeNodePrivateAttorney available to
         *          the framework for traversing the entire tree, public and private.
         */
        uint32_t getNumChildren() const;

        /*!
         * \brief Gets a child at a position in the children vector in the
         * order in which they were added to this node.
         * \warning Do not use this method for iterating child nodes for the
         * purpose of searching. Use getChild* and findChildren* methods
         * instead.
         * \throw SpartaException if idx is out of range.
         *
         * This method is for iteration-purposes only (so that wrappers may
         * avoid STL). The index used here is NOT a consistent property with
         * any TreeNode or Resource.
         */
        TreeNode* getChildAt(uint32_t idx) const;

        /*!
         * \brief Gets a vector of all children of this node in any group in the
         * order in which they were added to this node.
         * \note This is a const vector matching the order and content of the
         * internal child list.
         * \warning Do not use this method for iterating child nodes for the
         * purpose of searching. Use getChild* and findChildren* methods
         * instead.
         * \return const vector of TreeNode* children
         *
         * \warning This will only return the public children. Private children
         *          must be accessed via the TreeNodePrivateAttorney available to
         *          the framework for traversing the entire tree, public and private.
         */
        const ChildrenVector getChildren() const;

        /*!
         * \brief Gets all children of this node in the order in which they were
         * added to this node.
         * \param results Vector to which all children will be appended. This
         * vector is not result.
         * \param ignore_builtin_group Ignore any children in group
         * GROUP_NAME_BUILTIN.
         * \param ignore_anonymous_nodes Ignore any children in group
         * GROUP_NAME_
         * \return Number of children found
         *
         * \warning This will only return the public children. Private children
         *          must be accessed via the TreeNodePrivateAttorney available to
         *          the framework for traversing the entire tree, public and private.
         */
        uint32_t getChildren(std::vector<TreeNode*>& results,
                             bool ignore_builtin_group=true,
                             bool ignore_anonymous_nodes=true);

        /*!
         * \brief return all the children matching a particular type using
         *        dynamic cast.
         * \warning This will only return the public children. Private children
         *          must be accessed via the TreeNodePrivateAttorney available to
         *          the framework for traversing the entire tree, public and private.
         */
        template <class T>
        uint32_t getChildrenOfType(std::vector<T*>& results) const
        {
            uint32_t orig_size = results.size();
            for (auto child : getChildren())
            {
                if (dynamic_cast<T*>(child))
                {
                    results.emplace_back(static_cast<T*>(child));
                }
            }
            return results.size() - orig_size;
        }

        /*!
         * \brief Find ancestor by name
         * \param name The name of the ancestor, can include wildcards understood by Sparta
         * \returns If found, ancestor's TreeNode, otherwise nullptr
         */
        sparta::TreeNode *findAncestorByName(const std::string &name) {
            sparta::TreeNode *parent = getParent();
            while (parent != getRoot()) {
                if (parent->locationMatchesPattern(name, parent->getParent())) {
                    return parent;
                }
                parent = parent->getParent();
            }
            return nullptr;
        }

        /*!
         * \brief Find ancestor by type
         * \tparam T The Resource type to check for
         * \returns If found, ancestor's TreeNode, otherwise nullptr
         *
         * This method will traverse up the tree until it finds a
         * TreeNode who's resource type matches the given type.  This
         * does not, by any means, guarantee that the resource is
         * available for obtaining.
         *
         * \warning If your resource inherits from some base class,
         *          passing T=your base class will not find the result.
         *          For this case, consider using findAncestorByTag, and
         *          add a tag to your base.
         */
        template <typename T>
        sparta::TreeNode *findAncestorByType() {
            sparta::TreeNode *parent = getParent();
            while (parent != getRoot()) {
                if (parent->getResourceTypeRaw() == typeid(T).name()) {
                    return parent;
                }
                parent = parent->getParent();
            }
            return nullptr;
        }

        /*!
         * \brief Find the first ancestor with a particular tag.
         * \param tag the tag we are searching for.
         *
         * This method will traverse up the tree until it finds a
         * TreeNode who's tag matches the searched tag. This does not,
         * by any means, guarantee that the resource is actually available
         * yet.
         *
         * \return a pointer to the treenode with tag or nullptr if not found.
         */
        sparta::TreeNode *findAncestorByTag(const std::string& tag)
        {
            sparta::TreeNode *parent = getParent();
            while (parent != getRoot()) {
                if (parent->hasTag(tag))
                {
                    return parent;
                }
                parent = parent->getParent();
            }
            return nullptr;
        }

        /*!
         * \brief Find an ancestor's resource with a certain type.
         * \tparam T the resource type you'd like to return.
         * \returns if found a pointer to the closest ancestor's resource
         *          of type T. nullptr otherwise.
         *
         * This method is similar to findAncestorByType but will return
         * the parent's resource instead.
         */
        template<typename T>
        T* findAncestorResourceByType()
        {
            static_assert(!std::is_pointer<T>::value && !std::is_reference<T>::value,
                          "Expected a non-pointer/non-reference T template argument");
            sparta::TreeNode* node = findAncestorByType<T>();
            if (node)
            {
                return node->getResourceAs<T*>();
            }
            return nullptr;
        }

        /*!
         * \brief Gets a vector of all aliases of this node.
         * \note This is a const vector matching the order and content of the
         * internal alias list
         * \return const vector of alias strings referring to this node
         */
        const AliasVector& getAliases() const;

        /*!
         * \brief Gets all child identifiers, aliases, group names, and
         * group-aliases which can be used to refer to a child of this node.
         * \param idents Vector to fill with new identities. Vector is not
         * cleared.
         * \param ignore_builtin_group=true If true, rejects the builtin group.
         * \return Number of identifiers added to idents
         * \post idents will be cleared and populated with names, aliases, and
         * group names within this node.
         * \note Order of names retrieved should be considered random.
         * Ordering is subject to change between calls due to use of a hash
         * map internally.
         * \warning Groups cannot currenly be retrieved by getChild. Do not
         * iterate these idents and invoke getChild.
         * \todo Allow groups to be retrieved witih getChild
         * \note This method copies many strings. It should not be considered
         * fast.
         * \note Anonymous children names will not appear in this list. Their
         * group and aliases will, however.
         *
         * This is useful for shell completion when browsing the children of a
         * node.
         */
        uint32_t getChildrenIdentifiers(std::vector<std::string>& idents,
                                        bool ignore_builtin_group=true) const;

        /*!
         * \brief Gets all the identifiers for this node (excluding groups)
         * \return A vector of string pointers. Elements are guaranteed to
         * be non-null
         * \warning These identifiers are pointers to strings in members and
         * vectors of this TreeNode instance. Altering the name or aliases or
         * group of this Node will invalidate the result vector of this call
         * \warning The returned vector is re-built each call
         */
        std::vector<const std::string*> getIdentifiers() const;

        /*!
         * \brief Gets the number of nodes in a group
         * \param group Name of group to get children from. If GROUP_NAME_NONE,
         * will retrieve only children with group=GROUP_NAME_NONE.
         * \return Number of children in this group, including anonymous nodes.
         * Aliases do not affect this result
         * \todo Optimize this count by storing group sizes (or group vectors)
         * in TreeNode.
         *
         * To count all children regardless of group, use getNumChildren
         */
        uint32_t getGroupSize(const std::string& group);

        /*!
         * \brief Gets the largest index of a node in the given group
         * \param group Name of group to get the index from.
         * \return Largest group index in the given group, including anonymous
         * nodes. If group is GROUP_NAME_NONE, searches for nodes with that
         * group name (not in a group). If there are no nodes in the chosen
         * group, returns GROUP_NAME_NONE.
         * \todo Optimize this count by storing group mappings
         */
        group_idx_type getGroupIndexMax(const std::string& group);

        /*!
         * \brief Gets all nodes in a child group
         * \param group Name of group to get children from. If GROUP_NAME_NONE,
         * will retrieve only children with group=GROUP_NAME_NONE.
         * \param results Vector of nodes to which results will be appended.
         * \return Number of nodes found in the group
         * \todo Optimize lookup by group using mappings
         *
         * To get all children regardless of group, use getChildren
         */
        uint32_t getGroup(const std::string& group,
                          std::vector<TreeNode*>& results);

        /*!
         * \brief Finds all children starting at this node with a
         * given pattern relative to this node by matching names an aliases.
         * Appends each found child to <results>.
         * \note this is not a full recursive search. All children found will be
         * N levels below this node whrere N Is dependent on the number of '.'
         * tokens in the search string.
         * \param pattern Search pattern in a restricted glob format.
         * \param results All children with identifiers matching the pattern
         * (alias or name) are appended to the back of this vector. Order of
         * results appended is implementation dependent and not guaranteed
         * consistent. All children found from this invocation of findChildren
         * (and any deeper recursion) will be adjacent in <results>.
         * \param replacements Vector of replacements made on wildcards in
         * path in order to find children. For each result added to \a results,
         * a vector element is added to \a replacements. That vector element
         * vector contains as many replacements as there are wildcards in the
         * \a pattern. To clarify results.size()=replacements.size() and
         * If replacements.size() > 1, replacements[0].size() is the number of
         * wildcards in \a pattern.
         * \post Appends all children that match pattern to results (does not
         * clear). Order of children appended is not defined or guaranteed to be
         * consistent.
         * \warning May return duplicates if any upward traversal is done after
         * multiple downward traversal (e.g. pattern is '*..a'). This occurs
         * since each of the N children found with "*" recognizes the upward
         * search pattern '.' (after another '.') and searches the parent for
         * "a".
         * \warning May return duplicates if multiple aliases refer to the same
         * TreeNode.
         * \return The number of children found and appended to results.
         * \note this is <b> not full path matching </b>. Patterns are extracted
         * between each '.' and the ends of the pattern string. Each of these
         * Extracted patterns is used to search in the current search context
         * and either find the new child/parent to search within OR (if the end
         * of the whole pattern has been reached), to find a node to add to the
         * results output vector
         * \throw Does not throw. May print a warning when a pattern attempts to
         * search up and the current node has no parent.
         * \see locationMatchesPattern
         *
         * The following glob wildcard patterns are supported
         * \li ? Match 0 or 1 characters
         * \li * Match 0 or more characters
         *
         * The following additional patterns are supported
         * \li + One or more characters
         *
         * The following glob patterns are <b>not</b> supported
         * <span style="color:#c00000;">
         * \li {}
         * \li []
         * \li \\ (since all names involved in search are alphanumeric and
         * underscore)
         * </span>
         *
         * For example:
         * \code
         * // For the following tree:
         * //
         * //       a
         * //     / | \________
         * //    /  |   \   \  \
         * // bee  ca  dee  ee  be
         * //            \
         * //             g
         * //
         * // With pointers to each node (e.g. TreeNode* a):
         * //
         * std::vector<sparta::TreeNode*> r;
         * std::vector<std::vector<std::string>> reps;
         *
         * r.clear();
         * assert(a->findChildren("bee", r) == 1);
         * assert(r[0] == bee);
         *
         * r.clear();
         * assert(a->findChildren("+ee", r, reps) == 2);
         * assert(find(r.begin(), r.end(), dee) != r.end()); // Order of results not guaranteed
         * assert(find(r.begin(), r.end(), bee) != r.end());
         * assert(reps.at(0).at(0) == "b");
         * assert(reps.at(1).at(0) == "d");
         *
         * r.clear();
         * assert(a->findChildren("*ee", r) == 3);
         * assert(find(r.begin(), r.end(), dee) != r.end()); // Order of results not guaranteed
         * assert(find(r.begin(), r.end(), bee) != r.end());
         * assert(find(r.begin(), r.end(), ee) != r.end());
         *
         * r.clear();
         * assert(a->findChildren("*e+", r) == 2);
         * assert(find(r.begin(), r.end(), dee) != r.end()); // Order of results not guaranteed
         * assert(find(r.begin(), r.end(), bee) != r.end());
         *
         * r.clear();
         * assert(ca->findChildren("..dee.g", r) == 1);
         * assert(r[0] == g);
         * \endcode
         */
        uint32_t findChildren(const std::string& pattern,
                              std::vector<TreeNode*>& results,
                              std::vector<std::vector<std::string>>& replacements)
        {
            return findChildren_(pattern, results, replacements, false /*allow_private*/);
        }

        /*!
         * \brief Version of findChildren with no replacements vector
         */
        uint32_t findChildren(const std::string& pattern,
                              std::vector<TreeNode*>& results)
        {
            return findChildren_(pattern, results, false /*allow_private*/);
        }

        /*!
         * \brief Finds immediate children with some identity (name or alias)
         * matching a regex.
         * \param expr Expression to match with child node identities
         * \param found All nodes with matching identities are appended to this
         * vector. This vector is not cleared
         * \return Number of children found in this call
         * \note Does not recurse
         */
        virtual uint32_t findImmediateChildren_(std::regex& expr,
                                                std::vector<TreeNode*>& found,
                                                std::vector<std::vector<std::string>>& replacements,
                                                bool allow_private=false);

        /*!
         * \brief Variant of findImmediateChildren_ with no replacements vector
         */
        uint32_t findImmediateChildren_(std::regex& expr,
                                        std::vector<TreeNode*>& found,
                                        bool allow_private=false);

        /*!
         * \brief Const-qualified variant of findImmediateChildren_
         */
        virtual uint32_t findImmediateChildren_(std::regex& expr,
                                                std::vector<const TreeNode*>& found,
                                                std::vector<std::vector<std::string>>& replacements,
                                                bool allow_private=false) const;

        /*!
         * \brief Variant of const-qualified findImmediateChildren_ with no
         * replacements vector
         */
        uint32_t findImmediateChildren_(std::regex& expr,
                                        std::vector<const TreeNode*>& found,
                                        bool allow_private=false) const;

        /*!
         * \brief Determines if the given pattern (which may contain wildcards)
         * can describe this node.
         * \param pattern Pattern to compare this node's location against.
         * This pattern CANNOT have any upward traversal (parent references).
         * \param pat_loc TreeNode representing the starting point of pattern.
         * This \a pat_loc node must be AT or ABOVE *this in the tree, otherwise
         * this method will always report false.
         * \note A surefire (but slower) way to implement this is to perform
         * findChildren with the inputs and search the result set for *this.
         * It is NOT trivial to write simple downward traversing comparison as
         * one might expect because patterns can go down and then up. All of the
         * paths downward must be tried even if they do not contain *this
         * because a down,down,up,up,down pattern could go into a subtree which
         * does not contain the target *this, but later come back up and then
         * back down to *this.
         * \note This is not able to cross virtual parent-child relationships
         * (e.g. in VirtualGlobalNode)
         * \throw SpartaException if \a pattern contains upward traversal
         *
         * Example:
         * \code
         * // RootTreeNode* r
         * // Created TreeNode* n
         * assert(n->getRoot() == r);
         * assert(n->getLocation() == "top.cpu0.regs");
         * assert(n->locationMatchesPattern("t?p.cpu*.regs", r) == true);
         * assert(n->locationMatchesPattern("", n) == true);
         * \endcode
         */
        bool locationMatchesPattern(const std::string& pattern,
                                    const TreeNode* pat_loc) const;

        /*!
         * \brief Retrieves a child with this dotted path name
         * \note this is not a full recursive search. The child, if found, will
         * be N levels below this node whrere N Is dependent on the number of
         * '.' tokens in the search string.
         * \param name path to child. This may be a single name or a dotted path
         * refering to a node several levels below this node.
         * \param must_exist If true, requires the child to exist by throwing an
         * exception if it does not. If false, allows the method to return
         * nullptr when no child is found.
         * \return A valid TreeNode* if child is found by name. If child cannot
         * be found and must_exist==true, throws SpartaException. Otherwise
         * returns nullptr.
         * \note no pattern matching supported in this method
         * \throw SpartaException if child is not found and must_exist==true
         *
         * Example:
         * \code
         * assert(node.getName() == "root");
         * assert(node.getChild("a.b.c").getName() == "c");
         * \endcode
         */
        TreeNode* getChild(const std::string& name,
                           bool must_exist=true)
        {
            return getChild_(name, must_exist, false /*private_also*/);
        }

        //! Overloaded const-qualified
        const TreeNode* getChild(const std::string& name,
                                 bool must_exist=true) const
        {
            return getChild_(name, must_exist, false /*private also */);
        }


        /*!
         * \brief Gets the deepest whole node location starting from \a this
         * node that matches some part of \a path starting at its beginning.
         * \param path Path to node relative this \a this node.
         * \return Dotted node path to the deepest ancestor of \a this node
         * whose location matches \a path from the beginning. If the node
         * described by \a path exists, returns the a path to the node
         * identified by \a path. Note that the returned string could differ in
         * this case if \a path contained aliases.
         * \warning This is an expensive operation as it is a recursive search.
         * It should not be called at run-time.
         *
         * Ths intent of this is to find out where a path given by a user (e.g.
         * configuration file) diverged from the actual tree for feedback
         * purposes
         */
        std::string getDeepestMatchingPath(const std::string& path) const;

        /*!
         * \brief Determines if a child can be found with the given dotted path.
         * \param name Dotted path to child
         * \note no pattern matching supported in this method. Child-finding
         * behavior is the same as getChild
         * \return true if the child can be found, false if not.
         * \see getChild
         */
        bool hasChild(const std::string& name) const noexcept
        {
            return hasChild_(name, false /*private also */);
        }

        /*!
         * \brief Determines if the node \a n is an immediate child of this node.
         * \param n Node to test for child-ness of \a this
         * \return true if \a n is an immediate child; false if not.
         */
        bool hasImmediateChild(const TreeNode* n) const noexcept;

        /*!
         * \brief Retrieves a child that is castable to T with the given dotted
         * path.
         * \tparam T Type of child expected
         * \param name Name of child. May be a dotted path indicating multiple
         * levels of device-tree traversal.
         * \param must_exist If true, Causes an exception to be thrown if no
         * child by this name could be found. If false, a nullptr is returned.
         * Finding a child and failing to cast to T always causes an exception
         * to be thrown
         * \return T* if a child of the correct name is found and it was
         * castable to T* using dynamic_cast.
         * If child cannot be found and must_exist==true, throws SpartaException.
         * Otherwise returns nullptr.
         * \note no pattern matching supported in this method, but dotted paths
         * are acceptable.
         * \throw SpartaException if must_exist==true and either the child by the
         * given name (path) could not be found or the found node was not
         * castable to T
         */
        template <class T,
                  typename = typename std::enable_if<std::is_pointer<T>::value>::type,
                  class ConstT = typename ptr_to_const_obj_ptr<T>::type>
        const ConstT getChildAs(const std::string& name, bool must_exist=true) const {
            static_assert(std::is_base_of<TreeNode, typename std::remove_pointer<T>::type>::value == true,
                          "Cannot use getChildAs with a type argument that is not a subclass of " \
                          "TreeNode. If the caller is looking for a resource, use " \
                          "getChild(name)->getResource instead.");
            const TreeNode* child = TreeNode::getChild(name, must_exist);
            const ConstT result = dynamic_cast<ConstT>(child);
            if(result == nullptr){
                if(must_exist){
                    throw SpartaException("Could not find child of \"")
                        << getLocation() << "\" with the relative path \""
                        << name << "\" that was of type: const "
                        << demangle(typeid(T*).name()) << ". Found node of type "
                        << demangle(typeid(*child).name());
                }
                return nullptr;
            }
            return result;
        }

        // Overload of getChildAs for const access with a non-pointer T typeid
        template <class T, typename = typename std::enable_if<!std::is_pointer<T>::value>::type>
        const T* getChildAs(const std::string& name, bool must_exist=true) const {
            return getChildAs<const T*>(name, must_exist);
        }

        // Overload of getChildAs for non-const access with a non-pointer T type
        template <class T, typename = typename std::enable_if<std::is_pointer<T>::value>::type>
        T getChildAs(const std::string& name, bool must_exist=true) {
            static_assert(std::is_base_of<TreeNode, typename std::remove_pointer<T>::type>::value == true,
                          "Cannot use getChildAs with a type argument that is not a subclass of " \
                          "TreeNode. If the caller is looking for a resource, use " \
                          "getChild(name)->getResource instead.");
            TreeNode* child = TreeNode::getChild(name, must_exist);
            T result = dynamic_cast<T>(child);
            if(result == nullptr){
                if(must_exist){
                    throw SpartaException("Could not find child of \"")
                        << getLocation() << "\" with the relative path \""
                        << name << "\" that was of type: " << demangle(typeid(T).name())
                        << ". Found node of type "<< demangle(typeid(*child).name());
                }
                return nullptr;
            }
            return result;
        }

        // Overload of getChildAs for non-const access with a non-pointer T type
        template <class T, typename = typename std::enable_if<!std::is_pointer<T>::value>::type>
        T* getChildAs(const std::string& name, bool must_exist=true) {
            return getChildAs<T*>(name, must_exist);
        }


        /*!
         * \brief Retrieves this node after casting to type T
         * \tparam T Type of child expected
         * \return T* if this node was castable to T* using dynamic_cast.
         * \throw SpartaException if this node could not be cast to T
         * \warning This method performs a dynamic cast (for now) and should not
         * be used in performance-critical code
         *
         * This could eventually be optimized or used to provide
         * meaningful errors by storing type information in TreeNode.
         */
        template <class T,
                  typename = typename std::enable_if<std::is_pointer<T>::value>::type,
                  class ConstT = typename ptr_to_const_obj_ptr<T>::type>
        ConstT getAs() const {
            static_assert(std::is_base_of<TreeNode, typename std::remove_pointer<T>::type>::value == true,
                          "Cannot use getAs with a type argument that is not a subclass of " \
                          "TreeNode.");
            ConstT result = dynamic_cast<typename ptr_to_const_obj_ptr<T>::type>(this);
            if(result == nullptr){
                throw SpartaException("Could not get TreeNode \"")
                    << getLocation() << "\" as type: const " << demangle(typeid(T).name());
            }
            return result;
        }

        // Overload of getAs for const access with a pointer T type
        template <class T, typename = typename std::enable_if<!std::is_pointer<T>::value>::type>
        const T* getAs() const {
            return getAs<const T*>();
        }

        // Overload of getAs for non-const access with pointer T type
        template <class T, typename = typename std::enable_if<std::is_pointer<T>::value>::type>
        T getAs() {
            static_assert(std::is_base_of<TreeNode, typename std::remove_pointer<T>::type>::value == true,
                          "Cannot use getAs with a type argument that is not a subclass of " \
                          "TreeNode.");
            T result = dynamic_cast<T>(this);
            if(result == nullptr){
                throw SpartaException("Could not get TreeNode \"")
                    << getLocation() << "\" as type: " << demangle(typeid(T).name());
            }
            return result;
        }

        // Overload of getAs for non-const access with non-pointer T type
        template <class T, typename = typename std::enable_if<!std::is_pointer<T>::value>::type>
        T* getAs() {
            return getAs<T*>();
        }

        /*!
         * \brief Finds a set of all children having the selected tag which
         * this node or descendants of this node within a distance of the given
         * max_depth.
         * \param tag Nodes having this tag will be placed into results vector
         * \param results Vector of to which all results found by this method
         * will be appended. This vector is not cleared
         * \param max_depth Maximum depth of a ancestor/descendant relationship
         * when filtring children. 0 means that any nodes returned must be the
         * node on which this method is invoked, 1 means that any nodes returned
         * can be this or immediate children, and so on. A max_depth of -1 means
         * that there is no limit on ancestor-descendant distance
         *
         * This search works by searching the global tag map for the specified
         * tag and then filtering this list for nodes which are a descendants of
         * the node on which this method is invoked.
         */
        uint32_t findChildrenByTag(const std::string& tag,
                                   std::vector<TreeNode*>& results,
                                   int32_t max_depth=-1);

        /*!
         * \brief Determines if this node is a descendant of the specified
         * ancestor node within some number of generations (depth).
         * \param ancestor TreeNode that will be tested for being a parent this
         * TreeNode
         * \param max_depth Maximum depth (distance) between ancestor and this
         * node that will allow this test to return true. A more distance
         * ancestor/descendant relationship will cause this method to
         * false. A max depth of 0 implies this node and ancestor must be the
         * same node, a max_depth of 1 means that the ancestor must be an
         * immediate parent or self, 2 means parent of parent or closer, and
         * so on. max_depth of -1 means no limit.
         */
        bool isDescendantOf(const TreeNode* ancestor,
                            int32_t max_depth=-1) const;

        /*!
         * \see sparta::PhasedObject::getLocation
         */
        std::string getLocation() const override final;

        /*!
         * \brief Returns the location of this node in the device tree
         * which \a might not be usable for navigating the device tree (it
         * cannot be used as an argument for getChild/findChildren).
         * \return Location string that is nice for presentation, even if not
         * progamatically useful in the getChild/findChildren methods.
         *
         * This differs from getLocation by representing nodes with groups
         * and no names as "group[group_index]" instead of "name". This string
         * cannot currently be used to find children by pattern or name because
         * it refers to a group.
         *
         * A motifivation for this method is to allow identification of
         * elements in anonymous buffers and queue resources to simulator tools.
         */
        std::string getDisplayLocation() const;

        /*!
         * \brief build-time equivalent to getLocation before an object is
         * actually attached to a tree. This is a mainly a framework and debugging
         * tool for determining what the location of a node currently being
         * constructed will be when complete.
         */
        std::string getExpectedLocation() const;

        /*!
         * \brief Renders the subtree starting at this node as a string
         * containing an indentation-based depth-first representation.
         * \return std::string representing the subtree.
         * \param max_depth Number of levels below this Node to traverse. 0
         * means this node only. Negative depth means no limit.
         * \param show_builtins When false, hides all TreeNodes in the
         * GROUP_NAME_BUILTIN group. If true, shows all nodes
         * \param names_only When true, renders names of nodes. Otherwise,
         * renders string representation of nodes
         * prints only this node.
         * \param hide_hidden Hides any nodes (and their subtrees) maked as
         * hidden
         * \param leaf_filt_fxn Filtering function. If nullptr, has no effect.
         * If not nullptr, this function is invoked on each LEAF node
         * encountered to determine whether it should be displayed. If this
         * function returns false, the leaf node is not rendered. If true, the
         * leaf node is rendered. Non-leaf nodes are not subject to filtering,
         * but will be omitted if they contain no leaves (at any depth) which
         * pass the leaf filter function. Note that \a max_depth prohibits
         * recursion past a certain depth, so nodes may not be shown even if
         * they have ancestor leaves that would be visible (according to the
         * filter) but are below the \a max_depth threshold.
         */
        std::string renderSubtree(int32_t max_depth=-1,
                                  bool show_builtins=false,
                                  bool names_only=false,
                                  bool hide_hidden=false,
                                  bool(*leaf_filt_fxn)(const TreeNode*) = nullptr) const;

        /*!
         * \brief Gets the level of this node. 0 is root.
         * \warning This can change when the node is connected to a tree.
         * Generally, this should only be queried once the Tree has exited the
         * TREE_FINALIZED phase because level cannot cahnge after finalization.
         * (see isFinalized)
         *
         * This is computed based on the number of parents iterated until a node
         * with no parents is reached.
         */
        uint32_t getLevel() const;

        /*!
         * \brief Recursively gets the count of a nodes that are a subclass of a
         * particular type (determined by dynamic_cast). Includes all
         * descendants of this node in the tree, but excludes this node
         */
        template <typename T>
        uint32_t getRecursiveNodeCount() const {
            uint32_t count = dynamic_cast<const typename std::remove_const<typename std::remove_pointer<T>::type>::type*>(this) != nullptr;
            for(auto& child : children_){
                count += child->getRecursiveNodeCount<T>();
            }
            return count;
        }

        /*!
         * \brief Gets the virtual global node singleton. This node
         * can have no parent and no children. It receives notifications from
         * all nodes in the simulator as if it were the parent of every node in
         * the simulation that has no parent.
         * \note Caller must not delete this node.
         *
         * This node is typically used to add logging taps that receive messages
         * before the entire device tree hierarchy is defined. This allows
         * warning messages from a newly-constructed node to be sent somewhere,
         * even before that node is part of any device tree.
         */
        static TreeNode* getVirtualGlobalNode();

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name TreeNode extensions
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Base class used to extend TreeNode parameter sets
         */
        class ExtensionsBase
        {
        public:
            ExtensionsBase();
            virtual ~ExtensionsBase();
            virtual void setParameters(std::unique_ptr<ParameterSet> params) = 0;
            virtual ParameterSet * getParameters() = 0;
            virtual ParameterSet * getYamlOnlyParameters() = 0;
            virtual void postCreate() {}
        };

        /*!
         * \brief Add a named parameter set to extend this tree node's metadata
         */
        void addExtensionParameters(const std::string & extension_name,
                                    std::unique_ptr<ParameterSet> extension_params);

        /*!
         * \brief Add an extension factory to this tree node by its type (name). This
         * method does not actually create any objects at this time. It will validate
         * and create the extension only if asked for later on during simulation.
         */
        void addExtensionFactory(const std::string & extension_name,
                                 std::function<ExtensionsBase*()> factory);

        /*!
         * \brief Get an extension object by type string. Returns nullptr if not
         * found (unrecognized).
         */
        ExtensionsBase * getExtension(const std::string & extension_name);

        /*!
         * \brief Get an extension without needing to specify any particular type
         * string. If no extensions exist, returns nullptr. If only one extension
         * exists, returns that extension. If more than one extension exists, throws
         * an exception.
         */
        ExtensionsBase * getExtension();

        /*!
         * \brief Extension names, if any. Tree node extensions are typically
         * instantiated on-demand for best performance (you have to explicitly
         * ask for an extension by its name, or it won't be created) - so note
         * that calling this method will trigger the creation of all this node's
         * extensions. The performance cost is proportional to the number of nodes
         * in the virtual parameter tree.
         */
        const std::set<std::string> & getAllExtensionNames();

        ////////////////////////////////////////////////////////////////////////
        //! @}
        //!

        //! \name Tree-Building
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Adds a TreeNode to this node as a child.
         * \pre All subblasses of this instance should be constructed before
         * calling this methods and none should be destructed. This ensures that
         * the onAddingChild_ and onSettingParent_ callback hooks can be invoked
         * with a child having valid RTTI data. See the C++11 standard: 12.7
         * Construction and destruction [class.cdtor] Paragraph 3.
         * \param child borrowed pointer to TreeNode to add as a child. child
         * must not already be a parent of *this. child cannot already be an
         * immediate child of *this. Child cannot have the same name or alias
         * as another name or alias of a child of *this (group names
         * may be repeated though). Child must not be nullptr.
         * \param inherit_phase Does the child inherit its phase from the parent
         * This should be true in almost all uses of the framework
         * \warning child is a borrowed reference - child is *not* copied.
         * child lifetime must exceed that of this instance.
         * \post Child must be added to this node and have this node set as its
         * parent.
         * \post New child will be added to the end of the list. This is
         * important for finalization because new nodes can be added while nodes
         * are being iterated.
         * \throw SpartaException if the child cannot be added for any reason.
         * \warning Do not call unless all subblasses of this instance have
         * begun construction. No subclasses have completed destruction. This
         * ensures that the onAddingChild_ and onSettingParent_ callback hooks
         * can be invoked with a child having valid RTTI data. See the C++11
         * standard: 12.7 Construction and destruction [class.cdtor] Paragraph 3
         *
         * Invokes onAddingChild_ before finally attaching the child. This gives
         * subclasses a chance to either reject the node or perform some local
         * actions like caching the child or determining its type.
         *
         * Invokes setParent_ on the child which handles passing information
         * about the new parent-child relationship to \a child's subtree.
         */
        void addChild(TreeNode* child, bool inherit_phase=true);

        /*!
         * \brief AddChild by reference for convenience
         *
         * Overloads addChild(TreeNode* child)
         */
        void addChild(TreeNode& child);

        /*!
         * \brief Make the entire subtree private
         *
         * This will add 1 to privacy_level_ for every node below, this
         * means it should work if B->C, where C is private on B, and B is
         * added as a private child of A, A.privacy_level_ = 0, B.privacy_level_ = 1,
         * C.privacy_level_ = 2 (C.privacy_level_ was previously == 1).
         *
         * See documentation for the privacy_level_ variable to really understand
         * what this is doing and why.
         */
        void makeSubtreePrivate()
        {
            incrementPrivacyLevel_(1);
        }

        /*!
         * Make this tree node the root of a scope
         */
        void setScopeRoot()
        {
            is_scope_root_ = true;
        }


        /*!
         * Returns true if this tree node is a scope root
         */
        bool isScopeRoot() const
        {
            return is_scope_root_;
        }

        ////////////////////////////////////////////////////////////////////////
        //! @}

        /*!
         * \brief Walks up parents (starting with self) until a parent with an
         * associated local clock is found, then returns that clock.
         * \return The clock of the first ancestor (including self) which has a
         * valid clock through getLocalClock. If no ancestor has a clock,
         * returns nullptr
         * \see getLocalClock
         */
        const Clock* getClock() override{
            if(isFinalized()){
                return working_clock_;
            }else{
                // Search for clock
                TreeNode* n = this;
                while(n){
                    const Clock* c = n->getLocalClock();
                    if(c){
                        return c;
                    }
                    n = n->getParent();
                }
                return nullptr;
            }
        }

        /*!
         * \overload Clock* getClock()
         */
        const Clock* getClock() const {
            if(isFinalized()){
                return working_clock_;
            }else{
                // Search for clock
                const TreeNode* n = this;
                while(n){
                    const Clock* c = n->getLocalClock();
                    if(c){
                        return c;
                    }
                    n = n->getParent();
                }
                return nullptr;
            }
        }

        /*!
         * \brief Gets the clock associated directly with this Node. This is
         * useful for configuration and simulator debug, but not at run-time.
         * \note In general, it is preferable to use getClock to find the
         * closest attached clock in this Node's ancestry.
         * \return Clock directly attached to this Node (if any). Returning
         * nullptr means no attached clock, however this Node is considered to
         * be within the nearest ancestor's clock. getClock will find nearest
         * ancestor's clock (or local clock, if present)
         *
         * By default TreeNodes are not associated (or required to be
         * associated) with a sparta::Clock. Subclasses may require this, however.
         */
        const Clock* getLocalClock() {
            return clock_;
        }

        /*!
         * \overload Clock* getLocalClock()
         */
        const Clock* getLocalClock() const {
            return clock_;
        }

        /*!
         * \brief Assigns a clock to this node. This clock will then be accessed
         * by any descendant which has no assigned clock or an ancestor with an
         * assigned clock between that descendant and this node. getClock
         * returns the Clock associated with the nearest ancestor.
         *
         * \param clk Clock to assign. Must not be null.
         * \pre Must be in the TREE_BUILDING phase
         * \pre Can only assign a non-NULL value once
         */
        virtual void setClock(const Clock * clk);

        /*!
         * \brief Get the scheduler associated with this tree node's root
         */
        Scheduler * getScheduler(const bool must_exist = false);
        const Scheduler * getScheduler(const bool must_exist = false) const;

        /*!
         * \brief Gets a weak pointer to this TreeNode. This weak pointer is
         * guaranteed to expire when this TreeNode is destructed unless locked.
         * \return weak_ptr to this TreeNode. Guaranteed not to be expired at
         * this time.
         * \warning Do not store the shared_ptr result of a call to
         * sparta::TreeNode::WeakPtr::lock because it will prevent weak
         * references to this shared pointer from expiring when the TreeNode is
         * actually deleted. Having a shared pointer to this node will NOT
         * prevent its destruction
         *
         * Example
         * \code
         * TreeNode::WeakPtr wp = node->getWeakPtr();
         * ...
         * if(!wp.expired()){ // Is weak pointer valid (node not yet deleted)
         *   wp.lock()->getName(); // lock converts to shared pointer for use.
         *                         // Shared pointer is nullptr if p is dead.
         * }
         * \endcode
         */
        WeakPtr getWeakPtr();

        /*!
         * \brief Const variant of getWeakPtr
         */
        ConstWeakPtr getWeakPtr() const;

        /*!
         * \brief Link this tree node with another tree node
         *
         * \param node The tree node to link with
         * \param label The label of the tree node
         *
         * This method is a generic interface for linking a tree node to other
         * tree nodes. Linking is not bi-directinal, linking a tree node A with
         * tree node be by calling this method on A, links A with B but not B
         * with A.
         *
         * In case this method is not overriden, we assume that it has a
         * resource and will let its resource know about what tree nodes are
         * being shadowed.
         */
        virtual void addLink(TreeNode *node, const std::string &label);

        /*!
         * \brief Activate one of the links added with addLink
         *
         * \param label The label of the tree node
         *
         * What it means to activate a link can differ from simulator to
         * simulator and each simulator that use the link notion is free to
         * define the semantics of the link and what it means to active a link.
         *
         * In case this method is not overriden, we assume that it has a
         * resource and will let its resource know about what tree nodes are
         * being shadowed.
         */
        virtual void activateLink(const std::string &label);

        /*!
         * \brief Compute a regex pattern for a node child path containing any
         * number of wildcard characters (not a dot-separated location) which
         * can be used to test against child node names.
         * \param pat Pattern for searching immediate children (e.g. "core*" or
         * "core0")
         * \note The caller is responsible for parsing a location string into
         * alphanumeric child strings.
         * \see hasWildcardCharacters
         */
        static std::string createSearchRegexPattern(const std::string& pat);

        /*!
         * \brief Determines if a given node name has any wildcard characters
         * which will be substituted in createSearchRegexPattern
         *
         * This is useful for identifying which nodes are patterns and which
         * are concrete node names. It is important to keep this in ssync with
         * createSearchRegexPattern
         */
        static bool hasWildcardCharacters(const std::string& name);

        /*!
         * \brief Gets the next name between two '.' chars in a string starting
         * at \a pos
         * \param name Name string to parse
         * \param pos Position in name to begin parsing. Will be updated to
         * location of the next '.' found plus 1.
         * \return String between pos and the next '.' found.
         *
         * Used for parsing tree paths:
         * \code
         * size_t pos = 4;
         * assert(getNextName("top.a.b.c", pos) == 'a');
         * assert(pos == 6);
         * \endcode
         */
        static std::string getNextName(const std::string& name, size_t& pos);

        /*!
         * \brief Determine if a glob-like search pattern matches some other
         * string
         * \param[in] pattern Glob-like pattern to search with. This is standard
         * SPARTA tree identifier pattern syntax
         * \param[in] other Other string with which \a pattern is being compared
         * \return true if pattern matches other, false if not.
         */
        static bool matchesGlobLike(const std::string& pattern, const std::string& other);


    private:
        /*!
         * \brief Private utility method to recursively lockdown all
         * special parameters in this subtree.
         */
        void lockdownParametersUtil_(){
            for(auto child : children_){
                child->lockdownParametersUtil_();
            }
            special_params_lockdown_ = true;
        }

        /*!
         * \brief Gets the next node unique ID from the static counter.
         *
         * This is used to assign a new ID to each node constructed
         */
         static node_uid_type getNextNodeUID_();

        /*!
         * \brief Adds a node to the static parentless list.
         * \param node Node to add. Must not already be on the list. Must not
         * be nullptr.
         *
         * This is typically called within a node's constructor
         */
        static void trackParentlessNode_(TreeNode* node);

        /*!
         * \brief Removes a node from the static parentless list and cleans up
         * expired node weak references
         * \note There is no harm in untracking a node that is not tracked.
         *
         * This is typically called within a node's setParent method and its
         * destructor
         */
        static void untrackParentlessNode_(TreeNode* node);

        /*!
         * \brief Adds a node to the static all_nodes_ list.
         * \param node Node to add. Must not already be on the list. Must not
         * be nullptr.
         * \throw SpartaFatalError if \a node is not in the list or any weak
         * pointers are is expired but not removed. Neither of these should be
         * possible. This exception cannot be caught.
         * \note Is a noexcept qualified function. Exception cannot and should
         * not be caught when thrown by this function
         *
         * This is always called within a TreeNode constructor
         */
        static void trackNode_(TreeNode* node);

        /*!
         * \brief Removes a node from the static parentless list and checks for
         * expired nodes.
         * \param node Node to remove. Must be in the all_nodes_ the list. Must
         * not be nullptr.
         * \throw SpartaFatalError if \a node is not in the list or any weak
         * pointers are is expired but not removed. Neither of these should be
         * possible. This exception cannot be caught.
         * \note Is a noexcept qualified function. Exception cannot and should
         * not be caught when thrown by this function
         * \note Even though this is called during destructors, this is a very
         * important test so throwing an exception WILL be allowed. Simulation
         * MUST die if this throws because
         * \note This is a low performance method because it requires iteration
         * of all nodes in the simulation. This should be disabled when the
         * framework proves itself stable in the wild (or atleast demoted to a
         * debug-only feature)
         * \todo Demote this to a debug-only test
         *
         * This is always called within the TreeNode destructor
         */
        static void untrackNode_(TreeNode* node) noexcept;

        /*!
         * \brief After a child is added, this recursively informs that child
         * and its descendants (subtree) that they have 1 or more new ancestors to
         * consider.
         *
         * Each child node informed via onAddedAsChild_ before its descendants.
         */
        void informAddedChildSubtree_();

        /*!
         * \brief After this node is added as a child, this recursively informs
         * the ancestors that they have 1 or more new nodes in the subtree to
         * consider.
         *
         * This node is not informed, but all ancestors are informed through
         * onDescendentSubtreeAdded_ starting with \a *this and traversing
         * upward to the root node
         */
        void informAddedChildAncestors_(TreeNode* des);

        /*!
         * \brief Increments child count if tree is finalized.
         *
         * Prints an error to cerr if number of invocations after isFinalized
         * exceeds CHILD_GET_THRESHOLD.
         */
        void incrementGetChildCount_(const std::string& name) const;

        //! \name Private Tree-Building Helpers
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Sets the parent node of this node and inherits the parent's
         * TreePhase (getPhase())
         * \param parent Parent to permanently set as this node's parent. Must
         * not be nullptr.
         * \param inherit_phase Does the child inherit its phase from the parent
         * \pre This node's current parent must not have been set.
         * \pre This node's phase must be <= that of the parent it is being
         * attached to
         * \throw SpartaException is parent_ is already set for this node, if
         * parent==nullptr, or if phase for this node is greater than the
         * parent's.
         * \post Stores new parent
         * \post Clears the expected parent (see setExpectedParent_)
         * \note Any changes to this node's parent \b must be performed through
         * this method
         * \note If parent phase is ahead of this node, phase transitions are
         * skipped. This node has no opportunity to act on those phases.
         *
         * Calls informAddedChildSubtree_ which gives all descendants of this
         * child (including the child) a chance to react to their new ancestor
         * and possibly patch their notification propagation shortcut list (if
         * they are a Notification source)
         *
         * This is expected only to be called by the parent node itself when
         * this node is added as a child.
         */
        virtual void setParent_(TreeNode* parent, bool inherit_phase);
        /*!
         * \brief Implements TreeNode::addChild with option to inherit phase
         * from parent or not.
         * \param child Node to add as a child of this
         * \param inherit_phase Does the child inherit its phase from the parent
         * \see addChild
         */
        void addChild_(TreeNode* child, bool inherit_phase);
        /*!
         * \brief Helper for setParent which assigns a phase to subtrees
         * \param phase Phase to assign. Must be >= getPhase() for each child.
         * \note To be invoked only by setParent_
         * \warning Node does not execute phase transitions. Phase is simply
         * written.
         *
         * Since roots are the only nodes which can set phase, this will not be
         * invoked unless a subtree is attached to a root late. Therefore, this
         * can safely set the phase without actually performing the phase
         * actions because any nodes created in the tree at later phases are
         * presumed to be except from earlier phases.
         */
        void recursSetPhase_(TreePhase phase);

        ////////////////////////////////////////////////////////////////////////
        //! @}

        //! \name Private Tree-Navigation and Rendering
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Recursive implementation of getLocation
         * \param ss StringStream to populate with location
         * \param anticipate_parent Should expected parent/child relationships
         * be treated as complete relationships? It is not useful to show this
         * to users but for some diagnostics or accessing virtual parameters at
         * construction-time, it is necessary.
         */
        void getLocation_(std::stringstream& ss, bool anticipate_parent=false) const;

        /*!
         * \brief Recursive implementation of getDisplayLocation
         * \param ss StringStream to populate with location
         */
        void getDisplayLocation_(std::stringstream& ss) const;

        /*!
         * \brief Recursive helper for renderSubtree.
         * \param ss std::stringstream to which nodes will be appended
         * \param indent Current indent level (number of spaces)
         * \param max_depth When 0, do not recurs into children. When < 0, no
         * limit to depth
         * \param show_builtins When false, hides all TreeNodes in the
         * GROUP_NAME_BUILTIN group. If true, shows all nodes
         * \param names_only When true, renders names of nodes. Otherwise,
         * renders string representation of nodes
         * \param hide_hidden Hides any nodes (and their subtrees) maked as
         * hidden
         * \param leaf_filt_fxn. See renderSubtree
         * \return Number of Nodes rendered (not filtered or hidden)
         */
        uint32_t renderSubtree_(std::stringstream& ss,
                                uint32_t indent,
                                int32_t max_depth,
                                bool show_builtins,
                                bool names_only,
                                bool hide_hidden,
                                bool(*leaf_filt_fxn)(const TreeNode*)) const;

        /*!
         * \brief Attempts to get a single child by its unique local identity
         * (name or alias)
         * \param name Identity of the node to get (name or alias)
         * \note Performs no pattern matching on name
         * \throw SpartaException if a node is not found for the given name or
         * alias. If name resolves to a group, still throws.
         * \note Intended to be overridden by special purpose nodes with virtual children
         */
        virtual TreeNode*
        getImmediateChildByIdentity_(const std::string& name,
                                     bool must_exist=true)
                                       ;

        // Overload of getImmediateChildByIdentity_
        //! \note Intended to be overridden by special purpose nodes with virtual children
        virtual const TreeNode*
        getImmediateChildByIdentity_(const std::string& name,
                                     bool must_exist=true)
                                        const;
        /*!
         * \brief Checks to see if this node has a parent.
         * \param action Name of action taking place when checking for no
         * parent. If an exception is thrown, this string will be included in
         * the exception message.
         * \throw SpartaException if there is a parent.
         */
        void ensureNoParent_(const char* action);

        /*!
         *\brief This boolean flag is true means LOCKED and HIDDEN parameters
         * cannot be overwritten or visible anymore.
         */
        OneWayBool<false> special_params_lockdown_; //!< Are LOCKED and HIDDEN params modifiable/visible?

        ////////////////////////////////////////////////////////////////////////
        //! @}

    protected:

        /*!
         * \brief Performs pattern matching on a identity string.
         * \param ident Regex identity. This could be generated from an input
         * glob pattern
         * \param expr Expression to compare against \a ident
         * \param replacements Returns each captured replacement of a in the
         * expression
         */
        static bool identityMatchesPattern_(const std::string& ident,
                                            std::regex& expr,
                                            std::vector<std::string>& replacements);


        /*!
         * \brief Variant of identityMatchesPattern_ with no replacements vector
         */
        static bool identityMatchesPattern_(const std::string& ident,
                                            std::regex& expr);

        /*!
         * \brief Gets the previous name between two '.' chars in a string starting
         * at \a pos
         * \param name Name string to parse
         * \param pos Position in name to begin parsing in reverse. Must
         * coincide with a '.' or end of string. Will be updated to location of
         * the next '.' found. Set to npos if the beginning of the string is
         * reached. To start parsing a string from the end, should be set to
         * length of string.
         * \return String between (excluding) pos and the '.' found earlier in
         * the string.
         *
         * Used for parsing tree paths:
         * \code
         * size_t pos = 5;
         * assert(getPreviousName_("top.a.b.c", pos) == 'a');
         * assert(pos == 3);
         * \endcode
         */
        static std::string getPreviousName_(const std::string& name,
                                            size_t& pos);

        /*!
         * \brief Finds the deepest node path mathing the input \a path.
         * Implements getDeepestMatchingPath.
         * \return pair containing <match depth, match path> relative to this
         * node
         */
        std::pair<uint32_t, std::string> recursGetDeepestMatchingPath_(const std::string& path,
                                                                       size_t name_pos) const;

        /*!
         * \brief Tracks a node as an expected parent without actually adding
         * this node as a child. This is used almost exclusively for printing
         * error messages which include a device-tree location BEFORE actually
         * inserting this node into the device tree since node construction can
         * fail.
         * \param parent Parent TreeNode that will act as the parent for the
         * purposes of printing device-tree location for this node.
         * \pre Tree cannot already have a parent. It makes no sense to set an
         * expected parent if the node alreay has a parent.
         * \post sparta::TreeNode::getLocation will reflect the location of this
         * node as if it is part of the tree under the specified parent
         * \note the expected parent node does not know about this instance.
         * \note An expected parent does not cause this node to behave as if it
         * is part of a tree except when printing a location.
         * \note The expected parent will be set to nullptr once an actual
         * parent is set through setParent_.
         *
         * Typical usage is to invoke this at the very beginning of the
         * constructor of a sparta::TreeNode subclass. This allows
         * sparta::TreeNode::getLocation to function as if this node were actually
         * part of a tree. After successful construction, addChild can be used
         * to connect this node to a parent in the device tree. At that time,
         * the expected parent is set back to nullptr.
         *
         * Using an expected parent for the purposes of printing a device-tree
         * location during construction is much cleaner and simpler than adding
         * a child node to some parent and then trying to remove it if something
         * goes wrong during construction.
         */
        void setExpectedParent_(const TreeNode* parent);

        /*!
         * \brief Recursively enter TREE_FINALIZING phase
         * \throw SpartaException already in TREE_FINALIZING phase or beyond
         * \post All nodes in tree are set to TREE_FINALIZED phase
         */
        void enterFinalizing_();

        /*!
         * \brief Recursively create resources based on tree configuration.
         * enter_finalize_ should be invoked after this method successfully
         * completes for an entire tree. Finalizes in the order of construction
         * \throw exception if there is an error anywhere in the finalization
         * procedure. No actions are rolled-back, but this method can be
         * called again.
         * \post Any TreeNode not in TREE_FINALIZING will be moved into
         * TREE_FINALIZING
         * \post res_ of this node contains allocated Resource unless there was
         * an exception or the node did not try to create a resource.
         * \post Clock is permanently associated with this node and cacheable
         */
        void finalizeTree_();

        /*!
         * \brief Iterates the finalized tree and validates each node
         * (e.g. ensures statistics can be evaluated)
         * \pre Tree must be in TREE_FINALIZED phase
         * \throw SpartaException if any node cannot be validated
         */
        void validateTree_();

        /*!
         * \brief Recursively enter TREE_FINALIZED phase
         * \throw SpartaException if already in TREE_FINALIZED phase or beyond
         * \note Can be called multiple times if tree did not fully finalize.
         * \post All nodes in three are set to TREE_FINALIZED phase
         */
        void enterFinalized_();

        /*!
         * \brief Recursively enter TREE_CONFIGURING phase
         * \throw Cannot throw
         * /post res_ of this node contains allocated Resource unless there was
         * an exception
         */
        void enterConfig_() noexcept;

        /*!
         * \brief Recursively invoke TreeNode::onBindTreeEarly_ and
         * Resource::onBindTreeEarly_ (in that order for each node)
         * \see sparta::RootTreeNode::bindTreeEarly
         */
        void bindTreeEarly_();

        /*!
         * \brief Recursively invoke TreeNode::onBindTreeEarly_ and
         * Resource::onBindTreeLate_ (in that order for each node)
         * \see sparta::RootTreeNode::bindTreeLate
         */
        void bindTreeLate_();

        /*!
         * \brief Hook to allow simulation resources to clean-up before simulation is ended.
         * \pre Simulator will be in a state earlier than TREE_TEARDOWN phase
         * \note The entire resource tree is accessible. Nothing has been
         * destructed yet
         * \note The simulator will not continue to run for this device tree at
         * this point.
         * \note This call may be followed by dumpDebugContent_ depending on the
         * configuration of the simulator and any errors encountered.
         * \note This call will be eventually followed by onEnteringTeardown_
         * unless the application is corrupted or hard-terminated (i.e.
         * std::terminate, exit call, or exception in a destructor when
         * unwinding)
         * \throw Any exceptions related to post-run state
         */
        void simulationTerminating_();

        /*!
         * \brief Hook to check the state of the simulator post-run and throw
         * exceptions if something is incorrect.
         * \pre Simulator post-run sanity checking will be enabled. If not
         * enabled, this will never be called
         * \pre Simulator will be in a state earlier than TREE_TEARDOWN phase
         * \note The entire resource tree is accessible. Nothing has been
         * destructed yet
         * \note The simulator will not continue to run for this device tree at
         * this point.
         * \note This can be invoked multiple times during a simulation with
         * different info content
         * \note This call may be followed by dumpDebugContent_ depending on the
         * configuration of the simulator and any errors encountered.
         * \note This call will be eventually followed by onEnteringTeardown_
         * unless the application is corrupted or hard-terminated (i.e.
         * std::terminate, exit call, or exception in a destructor when
         * unwinding)
         * \throw Any exceptions related to post-run state
         */
        void validatePostRun_(const PostRunValidationInfo& info) const;

        /*!
         * \brief Allows resources to write out detailed textual debugging
         * information about the node. This is typically called by a simulator
         * when shutting down due to an exception (or depending on simulator
         * config). However, it could also be called at other times.
         * \param out Output osteam to which this node should write all of
         * its debug state. Note that this is not necessarily the same ostream
         * for all nodes and resources, so this should not be shared.
         * \pre Simulator can be in any phase
         * \note The entire resource tree is accessible. Nothing has been
         * destructed yet
         * \note If you need to print a message
         * \note Do not throw here. If simulation state is invalid, that should
         * be detected in validatePostRun_.
         * \note The simulator will not continue to run for this device tree at
         * this point.
         * \note Conventionally, this method should write to \a out only.
         * Writing entirely new files from this method is unexpected by the user
         * and violates one of the design principals of this library where
         * the only simulator input and output files and named explicitly by the
         * user. If you must create a new file, please name it clearly.
         * \throw Must not throw!
         */
        void dumpDebugContent_(std::ostream& out) const noexcept;


        /*!
         * \brief Recursively enter TREE_TEARDOWN phase while alerting nodes
         * through onEnteringTeardown_ and alterting Resources through
         * Resource::onStartingTeardown_. Nodes already in TREE_TEARDOWN phase
         * will not be alerted (neither will their associated Resources).
         * All nodes are visited regardless of their parent's phase.
         * \throw Cannot throw
         */
        void enterTeardown_() noexcept;

        /*!
         * \brief Verifies that the given identifier is unique for all children
         * of this node by comparing against names, groups, and aliases. Throws
         * SpartaException if not unique.
         * \param ident Identifier of child to check for uniqueness (e.g. a name
         * or alias)
         * \param ignore_group_collision If true, collisions of keys where the
         * existing value in the names_ map is a Grouping are ignored. (default
         * false: throw exception on collision).
         * \throw SpartaException if a ident is not unique.
         */
        void verifyUniqueChildIdentifier_(const std::string& ident,
                                          bool ignore_group_collision=false);

        /*!
         * \brief "Removes" the given child by invoking onDestroyingChild_
         * then removing this child from the children_ list
         * \pre Must be in teardown phase if this node is attached (isAttached)
         * \param child Child to remove from this node. Must actually be a
         * current child of SpartaException is thrown
         * \warning It is not currently safe to continue accessing a tree from
         * which a child was removed. This is a teardown utility for breaking
         * parent-child references which can confuse the notification system
         * during random-order desstruction of the tree. This method DOES NOT
         * allow removal of nodes at runtime because it does not erase all
         * traces of the child. For example, the name map will still exist with
         * dangling pointers. A more thorough removal method is required to
         * actually support removal
         * \note This method does not care about phase. Presuably it will only
         * be called from TreeNode destructors, which will detect any phase
         * problems.
         * \throw SpartaException if \a child is not an actual child of this node
         */
        void removeChildForTeardown_(TreeNode* child);

        /*!
         * \brief Protected Wrapper for getParent()->removeChildForTeardown_
         * which allows subclases of TreeNode to indirectly invoke
         * removeChildForTeardown_ with themselves as the argument
         * \param parent Parent to remove this child from. Must be this
         * TreeNode's parent. Must not be nullptr
         */
        void removeFromParentForTeardown_(TreeNode* parent);

        /*!
         * \brief Removes a node from its parent with the expectation this node
         *        will be immediately destroyed (i.e. is an xvalue)
         */
        void detachFromParent_();

        /*!
         * \brief Removes a node from its children with the expectation this
         * node will be immediately destroyed (i.e. is an xvalue)
         */
        void detachFromChildren_();

        /*!
         * \brief This method informs whether the tree is past the lockdown phase
         * for all LOCKED and HIDDEN parameters.
         * Modifying LOCKED and HIDDEN parameters after this phase is disallowed.
         * Tree can be locked down during TREE_BUILDING phase or TREE_CONFIGURING
         * phase. During TREE_FINALIZING phase, all parameters are locked down as is.
         */
        bool areParametersLocked_() const{
            return special_params_lockdown_;
        }

        //! \name Internal Notification System
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Temporary delegate for notificaiton invokation until the
         * implementation is working.
         * This will then be merged with sparta::SpartaHandler
         * \note This class must be mostly inlined (constructors, queries,
         * comparisons, and calls) for performance
         */
        class delegate
        {
        public:

            typedef void (*stub_type)(void* obj,
                                      const TreeNode& origin,
                                      const TreeNode* obs_pt,
                                      const void* data); //, const std::type_info& tinfo);

            delegate() :
                object_ptr(0),
                obs_point(0),
                name_ptr(0),
                stub_ptr(0),
                reveals_origin(true)
            { }

            delegate(const delegate&& d) :
                object_ptr(d.object_ptr),
                obs_point(d.obs_point),
                name_ptr(d.name_ptr),
                stub_ptr(d.stub_ptr),
                reveals_origin(d.reveals_origin)
            { }

            //! \note This is required for placing delegates in a vector (for
            //! fast iteration during broadcast) despite also having a move
            //! constructor.
            delegate& operator=(const delegate& d) {
                object_ptr = d.object_ptr;
                obs_point = d.obs_point;
                name_ptr = d.name_ptr;
                stub_ptr = d.stub_ptr;
                reveals_origin = d.reveals_origin;

                return *this;
            }

            delegate(const delegate& d) :
                object_ptr(d.object_ptr),
                obs_point(d.obs_point),
                name_ptr(d.name_ptr),
                stub_ptr(d.stub_ptr),
                reveals_origin(d.reveals_origin)
            { }


            /*!
             * \brief Compares against another delegate's information based on
             * DataT, T, TMethod, observer pointer, and the observation point in
             * the tree.
             * \note It is critical to compare observer point to ensure that
             * observers of a particular event type can be registered on
             * multiple nodes.
             */
            template <class DataT, class T, void (T::*TMethod)(const TreeNode&,
                                                               const TreeNode&,
                                                               const DataT&)>
            constexpr bool equals(T* obj,
                                  const TreeNode& obs_pt,
                                  const std::string& name) const
            {
                return (stub_ptr == &method_stub<DataT, T, TMethod>)
                    && (obj == object_ptr)
                    && (&obs_pt == obs_point)
                    && (name == *name_ptr);
            }

            template <class DataT, class T, void (T::*TMethod)(const DataT&)>
            constexpr bool equals(T* obj,
                                  const TreeNode& obs_pt,
                                  const std::string& name) const
            {
                return (stub_ptr == &method_stub<DataT, T, TMethod>)
                    && (obj == object_ptr)
                    && (&obs_pt == obs_point)
                    && (name == *name_ptr);
            }


            /*!
             * \brief Compares all fields in this delegate against another
             * delegate
             * \return true if all fields are equal (e.g. same stub pointer,
             * same observation point, observer object, and observation name)
             * \see equals
             */
            bool operator==(const delegate& d) const {
                return (stub_ptr == d.stub_ptr)
                    && (object_ptr == d.object_ptr)
                    && (obs_point == d.obs_point)
                    && (name_ptr == d.name_ptr);
            }

            /*!
             * \brief Compares against another delegate based on the observation
             * point and name ignoring delegate callback signature (assumes it
             * alreay matches).
             * \param obs_pt Observation point to compare against this delegate.
             * Just checks to see if they match.
             * \param name String name of notification in question. This string
             * must be interned in StringManager. This delegate's name will be
             * compared to the name argument by address unless the delegate
             * has a name pointer of StringManager::EMPTY (implying it observes
             * all notification names of a given data type)
             * \return true if both:
             * \li This delegate has the same observation point as obs_pt
             * \li This delegate has either a name matching the name argument or
             * the delegate name StringManager::EMPTY (empty string) (this
             * implies that the delegate observes all notification names for the
             * given data type)
             */
            bool observes(const TreeNode& obs_pt, const std::string* name) const
            {
                return (&obs_pt == obs_point)
                       && TreeNode::notificationCategoryMatch(name_ptr, name);
            }

            /*!
             * \brief Creates delegate with given properties
             *
             * \param name Name of notifications to observe. Passing empty
             * string observes all notifications with payload type DataT
             */
            template <class DataT, class T, void (T::*TMethod)(const TreeNode&,
                                                               const TreeNode&,
                                                               const DataT&)>
            static delegate from_method(T* obj,
                                        const TreeNode& obs_pt,
                                        const std::string& name)
            {
                delegate d;
                d.object_ptr = obj;
                d.obs_point = &obs_pt;
                d.name_ptr = StringManager::getStringManager().internString(name);
                d.stub_ptr = &method_stub<DataT, T, TMethod>;
                return d;
            }

            template <class DataT, class T, void (T::*TMethod)(const DataT&)>
            static delegate from_method(T* obj,
                                        const TreeNode& obs_pt,
                                        const std::string& name)
            {
                delegate d;
                d.object_ptr = obj;
                d.obs_point = &obs_pt;
                d.name_ptr = StringManager::getStringManager().internString(name);
                d.stub_ptr = &method_stub<DataT, T, TMethod>;
                d.reveals_origin = false;
                return d;
            }

            /*!
             * \brief Invokes the delegate
             */
            template <typename DataT>
            void operator()(const TreeNode& origin,
                            const DataT& data) const
            {
                (*stub_ptr)(object_ptr, origin, obs_point, &data); //, typeid(DataT));
            }

            const std::string* getNameID() const {
                return name_ptr;
            }

            const TreeNode* getObservationPoint() const {
                return obs_point;
            }

            bool revealsOrigin() const { return reveals_origin; }

        private:

            /*!
             * \brief Stub method that handles an type of delegate invokation
             * and calls the callback specified at construction with the
             * appropriate arguments
             * \todo Remove typeid argument and lookup when in production mode
             */
            template <class DataT, class T, void (T::*TMethod)(const TreeNode&,
                                                               const TreeNode&,
                                                               const DataT&)>
            static void method_stub(void* obj,
                                    const TreeNode& origin,
                                    const TreeNode* obs_pt,
                                    const void* data)
            {
                // Check type
                //sparta_assert(tinfo == typeid(DataT),
                //                  "Expected to invoke method delegate for data type \"" << typeid(DataT).name()
                //                  << "\" with incorrect data type \"" << typeid(tinfo).name() << "\"")

                T* p = static_cast<T*>(obj);
                const DataT& d = *static_cast<const DataT*>(data);
                return (p->*TMethod)(origin, *obs_pt, d);
            }

            template <class DataT, class T, void (T::*TMethod)(const DataT&)>
            static void method_stub(void* obj,
                                    const TreeNode& origin,
                                    const TreeNode* obs_pt,
                                    const void* data)
            {
                (void) origin;
                (void) obs_pt;
                T* p = static_cast<T*>(obj);
                const DataT& d = *static_cast<const DataT*>(data);
                return (p->*TMethod)(d);
            }

            template <class DataT, void (*TMethod)(const TreeNode&,
                                                   const TreeNode&,
                                                   const DataT&)>
            static void method_stub(void* obj,
                                    const TreeNode& origin,
                                    const TreeNode* obs_pt,
                                    const void* data)
            {
                (void) obj;
                const DataT& d = *static_cast<const DataT*>(data);
                return (*TMethod)(origin, *obs_pt, d);
            }

            template <class DataT, void (*TMethod)(const DataT&)>
            static void method_stub(void* obj,
                                    const TreeNode& origin,
                                    const TreeNode* obs_pt,
                                    const void* data)
            {
                (void) obj;
                (void) origin;
                (void) obs_pt;
                const DataT& d = *static_cast<const DataT*>(data);
                return (*TMethod)(d);
            }

            /*!
             * \brief Notification callback object on which MethodT of a
             * method_stub instantiation is invoked.
             */
            void* object_ptr;

            /*!
             * \brief Point at which observation was registered
             */
            const TreeNode* obs_point;

            /*!
             * \brief Pointer to string interned in StringManager representing
             * ID. Do not deallocate
             */
            const std::string* name_ptr;

            /*!
             * \brief Pointer to the templated callback stub
             */
            stub_type stub_ptr;

            /*!
             * \brief the delegates callback reveal the origin of the
             *        source. i.e. it posts the source as one of the callback
             *        parameters.
             */
            bool reveals_origin = true;
        };

        /*!
         * \brief Container for std::type_info
         *
         * Allows copy-construction, comparison, and provides operator< based on
         * type_info hashes
         */
        class type_info_container {

            const std::type_info* tinfo_;
            size_t hash_code_;

        public:

            type_info_container() = delete;

            type_info_container(const type_info_container& rhp) :
                tinfo_(rhp.tinfo_),
                hash_code_(rhp.hash_code_)
            { }

            type_info_container(const std::type_info& tinfo) :
                tinfo_(&tinfo),
                hash_code_(tinfo.hash_code())
            { }

            bool operator==(const type_info_container& rhp) const {
                return hash_code_ == rhp.hash_code_;
            }

            bool operator<(const type_info_container& rhp) const {
                return hash_code_ < rhp.hash_code_;
            }

            const std::type_info& operator*() const {
                return *tinfo_;
            }

            const std::type_info* get() const {
                return tinfo_;
            }
        };

        /*!
         * \brief Vector of delegates representing a list of observers to notify
         */
        typedef std::vector<delegate> DelegateVector;

        /*!
         * \brief Map of delegate vectors containing all observers
         */
        typedef std::map<type_info_container, DelegateVector> NotificationObserverMap;


        /*!
         * \brief Starts a notification propagating up the tree
         * \param name_id Pointer to interned name in StringManager. Compared
         * against delegate name. The pointer is treated like a numeric ID for
         * comparison.
         * \note Invokes propagateNotification_ on this node after validating,
         * then invokes propagateNotification_ on the global virtual node.
         */
        template <typename DataT>
        void postPropagatingNotification_(const TreeNode* origin,
                                          const DataT& data,
                                          const std::string* name_id) {
            // Assure that this notification passes canGenerateNotification. It
            // is of no use to us if nodes are generating Notifications that no
            // observer can possible expect by examining the tree.
            // Skip this in production builds for performance
        #ifndef NDEBUG
            sparta_assert(origin);
            sparta_assert(name_id);
            NotificationInfo info(origin, &typeid(DataT), name_id);
            if(!canGenerateNotification(info)){
                throw SpartaException("TreeNode ")
                    << getLocation() << " posted a notification <"
                    << origin->getLocation() << ", \"" << demangle(typeid(DataT).name())
                    << "\", \"" << *name_id
                    << "\"> which it did not properly announce through canGenerateNotification";
            }
        #endif
            propagateNotification_(origin, data, name_id);

            // Post to the global virtual node
            getVirtualGlobalNode()->propagateNotification_(origin, data, name_id);
        }

        /*!
         * \brief Finds a delegate associated with the given type T, object
         * pointer, DataT, and TMethod within a DelevateVector. The intent of
         * this function is to help see if a delegate is already registered with
         * a calback by checking all known information associated with that
         * callback against the input arguments
         * \param[in] dvec Vector of delegates to search
         * \param[in] obj Object which owns the delegate
         * \param[in] target_name Name or pattern of notification type.
         * that this is used as-is.
         * \return Iterator within delegate vector parameter dvec. Will be a
         * valid iterator if no matching delegate is found. Otherwise, will be
         * dvec.end().
         * \note Performs string comparison for delegate target name
         */
        template <typename DataT, typename T, void (T::*TMethod)(const TreeNode&,
                                                                 const TreeNode&,
                                                                 const DataT&)>
        DelegateVector::iterator findDelegate_(DelegateVector& dvec,
                                               T* obj,
                                               const std::string& target_name) {
            const DelegateVector::const_iterator dend = dvec.end();
            DelegateVector::iterator d;
            for(d=dvec.begin(); d!=dend; ++d){
                if(d->equals<DataT, T, TMethod>(obj, *this, target_name)){
                    break;
                }
            }
            return d;
        }

        // Overload of findDelegate_ with one TMethod signature having a sole DataT parameter
        template <typename DataT, typename T, void (T::*TMethod)(const DataT&)>
        DelegateVector::iterator findDelegate_(DelegateVector& dvec,
                                               T* obj,
                                               const std::string& target_name) {
            const DelegateVector::const_iterator dend = dvec.end();
            DelegateVector::iterator d;
            for(d=dvec.begin(); d!=dend; ++d){
                if(d->equals<DataT, T, TMethod>(obj, *this, target_name)){
                    break;
                }
            }
            return d;
        }

        /*!
         * \brief Recursively notifies children that the notification described
         * is now (or still is) being observed at the observation point TreeNode
         * \a obs_node with the newly registered delegate \a del
         *
         * \param[in] tinfo Type info of the notification
         * \param[in] name_ids vector of pointers to names or glob-like treenode
         * identifier patterns.
         * that may or may not be interned with sparta::StringManager.
         * \param[in] obs_node Node at which observer was installed
         * \param[in] del Delegate to invoke when the notification posts
         *
         * This method allows Notification nodes to keep a map of which
         * TreeNodes are observing observing a notification and what delegates
         * are registered to observe that notification. The delegates can then
         * be invoked directly by notification sources when appropriate
         * notifications are posted.
         *
         * Override to use a non-standard child-list. By default, this method
         * simply iterates this node's children.
         */
        virtual void
        broadcastRegistrationForNotificationToChildren_(const std::type_info& tinfo,
                                                        const std::vector<const std::string*>& name_ids,
                                                        TreeNode* obs_node,
                                                        const delegate* del,
                                                        const bool allow_private);

        /*!
         * \brief Recursively notifies children that the notification described
         * has lost one particular observer (\a del) which was observing at the
         * observation point TreeNode \a obs_node with the delegate \a del
         *
         * This is the symmetric opposite of
         * broadcastRegistrationForNotificationToChildren_
         *
         * Override to use a non-standard child-list. By default, this method
         * simply iterates this node's children and call itself recursively for
         * each
         */
        virtual void
        broadcastDeregistrationForNotificationToChildren_(const std::type_info& tinfo,
                                                          const std::vector<const std::string*>& name_ids,
                                                          TreeNode* obs_node,
                                                          const delegate* del,
                                                          const bool allow_private);

        /*!
         * \brief Entry point to broadcastRegistrationForNotificationToChildren_
         * recursion. Breaks a name string
         */
        void
        broadcastRegistrationForNotificationListStringToChildren_(const std::type_info& tinfo,
                                                                  const std::string& name,
                                                                  TreeNode* obs_node,
                                                                  const delegate* del,
                                                                  const bool private_only)
        {
            auto names = parseNotificationNameString(name);
            broadcastRegistrationForNotificationToChildren_(tinfo, names, obs_node, del,
                                                            private_only);
        }

        /*!
         * \brief Symmetric oppostie of
         * broadcastRegistrationForNotificationListStringToChildren_
         */
        void
        broadcastDeregistrationForNotificationListStringToChildren_(const std::type_info& tinfo,
                                                                    const std::string& name,
                                                                    TreeNode* obs_node,
                                                                    const delegate* del,
                                                                    const bool private_only)
        {
            auto names = parseNotificationNameString(name);
            broadcastDeregistrationForNotificationToChildren_(tinfo, names, obs_node, del,
                                                              private_only);
        }

        /*!
         * \brief Protected wrapper for invokeDelegates_ which allows a TreeNode
         * to invoke delegates on another TreeNode using itself as the origin
         * \param to_invoke TreeNodes whose delegates will be invoked (those
         * which are appropriate for the specified DataT and name_id)
         *
         * See invokeDelegates_ for other argument semantics
         */
        template <typename DataT>
        void invokeDelegatesOn_(TreeNode* to_invoke,
                                const DataT& data,
                                const std::string* name_id) {
            to_invoke->invokeDelegates_(this, data, name_id);
        }

    private:

        /*!
         * \brief Invokes delegates on this node for the given data object and
         * notificaiton name. Does not propagate
         * \tparam DataT type of data to pass to delegates (only delegates
         * which accept this data type will be invoked)
         * \param origin TreeNode from which the notification originated
         * \param data Data to pass to appropriate delegates
         * \param name_id Pointer to interned name in StringManager. Compared
         * against delegate name (by pointer) to determine which delegates to
         * invoke.
         */
        template <typename DataT>
        void invokeDelegates_(const TreeNode* origin,
                              const DataT& data,
                              const std::string* name_id) {
            auto itr = obs_local_.find(typeid(DataT));
            if(itr != obs_local_.end()){
                DelegateVector& observers = itr->second;
                for(delegate& d : observers) {
                    // Invoke delegate if matched
                    if(d.getNameID() == name_id
                       || d.getNameID() == StringManager::getStringManager().EMPTY){
                        d(*origin, data); // invoke
                    }
                }
            }
        }

        /*!
         * \brief Invokes delegates on this node for the given data and category
         * then traverses up to parent to do the same
         * \tparam DataT type of data to pass to delegates (only delegates
         * which accept this data type will be invoked)
         * \param origin TreeNode from which the notification originated
         * \param data Data to pass to appropriate delegates
         * \param name_id Pointer to interned name in StringManager. Compared
         * against delegate name (by pointer) to determine which delegates to
         * invoke.
         * \note this must not be called directly by subclasses so that TreeNode
         * can guarantee that only expected notification types/name scan enter
         * propagation.
         * \note Does not propagate to the global virtual node. This must be
         * done externally to this method.
         * Uses invokeDelegates_ to actually invoke the appropriate local
         * delegates
         */
        template <typename DataT>
        void propagateNotification_(const TreeNode* origin,
                                    const DataT& data,
                                    const std::string* name_id) {

            invokeDelegates_<DataT>(origin, data, name_id);

            if(getParent() != nullptr){
                getParent()->propagateNotification_(origin, data, name_id);
            }
        }

        // Internal Notification System
        ////////////////////////////////////////////////////////////////////////
        //! @}

    public:

        //! \name Public Notification System
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Notification type/name information.
         *
         * Used to satisfy queries on whether a node or its subtree can generate
         * a paticular notification.
         */
        struct NotificationInfo {

            //! \brief Basic constructor
            NotificationInfo(const TreeNode* _origin,
                             const std::type_info* _tinfo,
                             const std::string* _name) :
                origin(_origin),
                tinfo(_tinfo),
                name(_name)
            {
                checkValid();
            }

            //! \brief Move constructor
            NotificationInfo(const NotificationInfo&& rhp) :
                origin(rhp.origin),
                tinfo(rhp.tinfo),
                name(rhp.name)
            {
                checkValid();
            }

            //! \brief Copy constructor
            NotificationInfo(const NotificationInfo& rhp) :
                origin(rhp.origin),
                tinfo(rhp.tinfo),
                name(rhp.name)
            {
                checkValid();
            }

            //! \brief Assignment operator
            NotificationInfo& operator=(const NotificationInfo& rhp) {
                rhp.checkValid();
                origin = rhp.origin;
                tinfo = rhp.tinfo;
                name = rhp.name;
                return *this;
            }

            /*!
             * \brief Ensures the node contains valid data
             */
            void checkValid() const {
                sparta_assert(origin);
                sparta_assert(tinfo);
                sparta_assert(name);
            }

            /*!
             * \brief TreeNode from which this notification can be generated
             */
            const TreeNode* origin;

            /*!
             * \brief Type from typeid() on notification DataT
             */
            const std::type_info* tinfo;

            /*!
             * \brief String interned by StringManager. Must not be nullptr
             */
            const std::string* name;
        };

        /*!
         * \brief Gets information on the possible notifications generated by
         * this node (excludes children).
         * \param infos Vector of notification inormation to which this node's
         * information will be added
         * \return Number of infos appended to the infos vector.
         * \post infos has NotificationInfo instances appended. Original content
         * remains in place. Vector can only grow.
         */
        uint32_t getPossibleNotifications(std::vector<NotificationInfo>& infos) const;

        /*!
         * \brief Dumps a listing of the notifications which this node can
         * generate to the ostream o.
         * \param o ostream to which the notification info list will be dumped
         */
        void dumpPossibleNotifications(std::ostream& o) const noexcept;

        /*!
         * \brief Gets all possible notification info from NotificationSources
         * within this node's subtree
         * \tparam DataT type of data for which to search. This must be
         * non-const, non-volatile, non-pointer, and non-reference.
         * \param nodes Result set of nodes to which all found nodes will be
         * appended. This vector is \b not cleared.
         * \param name Name of the NotificationSource to search for. Specifying
         * "" locates any sourcee matching on DataT.
         *
         * Example:
         * \code
         * // Find any nodes that may post the sparta::Register::PostWriteAccess
         * // type regardless of the notification name.
         * std::vector<sparta::TreeNode*> results;
         * node->locateNotificationSources<sparta::Register::PostWriteAccess>(nodes);
         *
         * // Find any nodes that may post any data structure with the notification
         * // name "post_write"
         * std::vector<sparta::TreeNode*> results;
         * node->locateNotificationSources(nodes, "post_write");
         *
         * \code
         * // Find all notification sources at node or below
         * std::vector<sparta::TreeNode*> results;
         * node->locateNotificationSources(nodes);
         * \endcode
         */
        template <typename DataT=ANY_TYPE>
        uint32_t locateNotificationSources(std::vector<TreeNode*>& nodes,
                                           const std::string& name="") {
            static_assert(std::is_same<
                              typename std::remove_cv<
                                  typename std::remove_reference<
                                      typename std::remove_pointer<DataT>::type
                                  >::type
                              >::type,
                              DataT
                          >::value,
                          "DataT must NOT be a const, volatile, pointer, or reference"
                          "type. It violates at least one of these restrictions");

            auto& strmgr = StringManager::getStringManager();
            const std::string* name_id = strmgr.internString(name);
            return locateNotificationSources_<DataT>(nodes, name_id);
        }

        /*!
         * \brief Retrieves the relevant NotificationSources from
         * locateNotificationSources and prints them to the output \a o
         * \tparam DataT type of notification to search for or ANY_TYPE. See
         * locateNotificationSource.
         * \param o ostream to write found nodes to.
         */
        template <typename DataT=ANY_TYPE>
        void dumpLocatedNotificationSources(std::ostream& o, const std::string& name=""){
            std::vector<TreeNode*> nodes;
            locateNotificationSources<DataT>(nodes, name);
            for(const TreeNode* n : nodes){
                o << n->stringize() << std::endl;
            }
        }

        /*!
         * \brief Gets information on the possible notifications generated by
         * this node and all its descendants.
         * \param infos Vector of notification information to which this node's
         * information will be added
         * \return Number of infos appended to the infos vector
         * \post infos has NotificationInfo instances appended from this node
         * and all descendants. Vector can only grow.
         */
        uint32_t getPossibleSubtreeNotifications(std::vector<NotificationInfo>& infos) const noexcept;
        /*!
         * \brief Dumps a listing of the notifications which this node and its
         * descendants can generate to the ostream o
         * \param o ostream to which the noitification info list will be dumped
         */
        void dumpPossibleSubtreeNotifications(std::ostream& o) const noexcept;

        /*!
         * \brief Can this TreeNode generate a notification of the given type
         * having the given name or pattern.
         * \param tinfo type_info from typeid on the type of notification being
         * queried.
         * \param name Pointer to interned name or pattern of notifications to
         * query (from StringManager::internString). If This value is
         * StringManager::EMPTY, looks for notifications having any name.
         * glob-like patterns are supported (see matchesGlobLike).
         * \return true if this node can post a notification having the given
         * type T where tinfo=typeid(T) and having a notification name matching
         * the parameter 'name'. If the parameter 'name' is
         * StringManager::EMPTY, matches on any notification name.
         * \note This method is effectively a const expression - It will
         * invariably return the same result for the same inputs.
         *
         * Example
         * \code
         * bool bCanGenLogMsg =
         *   n->canGenerateNotification(typeid(sparta::log::Message), StringManager::getStringManager().EMPTY);
         * bool bCanGenWarningLogMsg =
         *   n->canGenerateNotification(typeid(sparta::log::Message), sparta::log::categories::WARN);
         * \endcode
         */
        bool canGenerateNotification(const std::type_info& tinfo,
                                     const std::string* name) const;

        /*!
         * \brief Can this TreeNode generate a notification of the given type
         * having the given name (does not require interned string - will intern
         * automatically with StringManager).
         *
         * This is a version of the other canGenerateNotification function which
         * does not require name to be a string interned with StringManager.
         * Though the other variant is faster, this can be more convenient.
         */
        bool canGenerateNotification(const std::type_info& tinfo,
                                     const std::string& name) const;

        /*!
         * \brief Can this TreeNode generate a notification of the given
         * NotificationInfo.
         * \note Also checks origin to ensure that it points to this node.
         */
        bool canGenerateNotification(const NotificationInfo& info) const;

        /*!
         * \brief Can this TreeNode or its descendants (of any distance)
         * generate a notification of the given type having the given name.
         * \param tinfo type_info from typeid on the type of notification being
         * queried.
         * \param name Pointer to interned name of notifications to query about
         * (from StringManager::internString). If This value is
         * StringManager::EMPTY, looks for notifications having any name.
         * \return true if this node can post a notification having the given
         * type T where tinfo=typeid(T) and having a notification name matching
         * the parameter 'name'. If the parameter 'name' is
         * StringManager::EMPTY, matches on any notification name.
         * \warning This searches the entire subtree and can be slow.
         *
         * Uses canGenerateNotification at each node to check. Returns on first
         * match.
         *
         * Example
         * \code
         * bool bCanGenLogMsg =
         *   n->canSubtreeGenerateNotification(typeid(sparta::log::Message), StringManager::getStringManager().EMPTY);
         * bool bCanGenWarningLogMsg =
         *   n->canSubtreeGenerateNotification(typeid(sparta::log::Message), sparta::log::categories::WARN);
         * \endcode
         */
        bool canSubtreeGenerateNotification(const std::type_info& tinfo,
                                            const std::string* name) const;

        /*!
         * \brief Can this TreeNode or its descendants (of any distance)
         * generate a notification of the given type having the given name (does
         * not require interned string)
         *
         * This is a version of the other canGenerateNotification function which
         * does not require name to be a string interned with StringManager.
         * Though the other variant is faster, this can be more convenient.
         */
        bool canSubtreeGenerateNotification(const std::type_info& tinfo,
                                            const std::string& name) const;

        /*! \brief Checks to see if any a subtree can generate any of several
         *  notification names which may be patterns
         *  \see canSubtreeGenerateNotifications
         */
        bool canSubtreeGenerateNotifications(const std::type_info& tinfo,
                                            const std::vector<const std::string*>& names) const;

        /*!
         * \brief Parses a comma-separated list of notification names (or patterns)
         * separated by commas and ignoring whitespace around commas
         * \param[in] csl Comma-separated list (e.g. "a,b,foo*"). Most common
         * case will be a single name with no commas
         * \return List of interned string pointers (through sparta::StringManager)
         */
        static std::vector<const std::string*> parseNotificationNameString(const std::string& csl);

        /*!
         * \brief Registers a callback method to listen for all notifications
         * having the specified data type DataT and name (or any name if name is
         * "") occuring on this node or any descendant (subtree).
         * \note Some parameters of this method can be automatically filled-in
         * using the REGISTER_FOR_NOTIFICATION macro.
         * \tparam DataT Type of data object given to the notification callback
         * function registered. This type must be a C++ typename of a type which
         * is copyable, non-const, non-reference, and non-pointer.
         * Useful values for this type can be found by locating
         * sparta::NotificationSource nodes throughout the device tree and
         * then by looking at sparta::NotificationSource::getNotificationTypeName.
         * Note that ANY_TYPE is not a valid argument for DataT.
         * \tparam T Class of object on which callback member function will be
         * invoked.
         * \tparam TMethod member function pointer of class \a T that will be
         * invoked when the specified notification is posted. This member
         * function signature must be:
         * \code
         * ClassName::func(// TreeNode from which the notification originated
         *                 const TreeNode& origin_node,
         *                 // TreeNode at which the notifications was observed, causing this callback
         *                 const TreeNode& observation_node,
         *                 // Notification event data
         *                 const datat& data)
         * \endcode
         * \param name Name of notification being observed. If \a name is "",
         * all notifications having a data type of \a DataT will cause this
         * callback to be notified. Locating a notificationSource in the device
         * tree and and using sparta::NotificationSource::getNotificationName
         * can help to determine useful values for \a name.
         * \param ensure_possible If true, this method will check that one or
         * more nodes in this node's subtree can generate the notification
         * described by DataT and name. If false, this test is not performed.
         * \throw SpartaException if a ensure_possible=true and a registration is
         * made on a DataT/name combination that cannot be posted by this the
         * node's subtree (since there are no ancestor NotificationSource
         * nodes).
         * \note Cannot re-register with same arguments.
         * \todo Faster attachment of delegates to children. There is notable
         * performance impact today, however.
         * \note This method is potentially slow because all children may be
         * walked to build a notification shortcut list, which is an
         * optimization for the performance-critical notification-posting code
         * \see REGISTER_FOR_NOTIFICATION
         *
         * NotificationSources can be located throughout the simulation using
         * the sparta::TreeNode::locateNotificationSources method.
         *
         * The methods
         * getPossibleSubtreeNotifications and dumpPossibleSubtreeNotifications
         * can assist in finding NotificationSource nodes in the device tree.
         *
         * Example:
         * \code
         * struct MyClass {
         *     void func(// TreeNode from which the notification originated
         *               const TreeNode& origin_node,
         *               // TreeNode at which the notifications was observed, causing this callback
         *               const TreeNode& observation_node,
         *               // Notification event data
         *               const datat& data)
         *     void func(const TreeNode&, const TreeNode&, const SomeData&) {}
         * }
         * // ...
         * MyClass my_class;
         * // ...
         * // Given NotificationSource<struct SomeData> node;
         * node->registerForNotification<MyClass, &MyClass::func>(my_class);
         * // ...
         * node->deregisterForNotification<MyClass, &MyClass::func>(my_class);
         * \endcode
         */
        template <typename DataT, typename T, void (T::*TMethod)(const TreeNode&, const TreeNode&, const DataT&)>
        void registerForNotification(T* obj, const std::string& name, bool ensure_possible=true)
        {
            registerForNotification_<DataT, T, TMethod>(obj, name, ensure_possible, false /*allow_private*/);
        }

        // Overload which allows registration with a class member function accepting only a data argument
        template <typename DataT, typename T, void (T::*TMethod)(const DataT&)>
        void registerForNotification(T* obj, const std::string& name, bool ensure_possible=true)
        {
            registerForNotification_<DataT, T, TMethod>(obj, name, ensure_possible, false /*allow_private*/);
        }

        /*!
         * \brief Removes at most one registration made with
         * registerForNotification
         * \see REGISTER_FOR_NOTIFICATION
         *
         * Refer to registerForNotification for more information about
         * parameters. To deregister an observer delegate registered through
         * registerForNotification, pass the same arguments.
         */
        template <typename DataT, typename T, void (T::*TMethod)(const TreeNode&, const TreeNode&, const DataT&)>
        void deregisterForNotification(T* obj, const std::string& name)
        {
            deregisterForNotification_<DataT, T, TMethod>(obj, name, false /* allow_private */);
        }

        // Overload which allows deregistration with a class member function accepting only a data argument
        template <typename DataT, typename T, void (T::*TMethod)(const DataT&)>
        void deregisterForNotification(T* obj, const std::string& name)
        {
            deregisterForNotification_<DataT, T, TMethod>(obj, name, false /* allow_private */);
        }

        /*!
         * \brief Determines if this TreeNode (not descendants) has any
         * observers for this type of event and name
         * \param tinfo typeid of the notification to be queried
         * \param name Name of the notification to be queried (Can be "" to
         * ignore name)
         */
        bool hasObserversRegisteredForNotification(const std::type_info& tinfo,
                                                   const std::string* name) const noexcept;

        /*!
         * \brief Gets the vector of delegates registered for a notification on
         * this TreeNode
         * \param tinfo typeid of the notification to be queried
         * \param name Name of the notification to be queried (Can be "" to
         * ignore name)
         * \param dels Vector to which all found delegates matching search
         * parameters will be appended. This vector is not cleared
         */
        void getDelegatesRegisteredForNotification(const std::type_info& tinfo,
                                                   const std::string* name,
                                                   std::vector<delegate>& dels) noexcept;

        /*!
         * \brief Checks if two notification categories match where one is an actual category
         * \param[in] query_id pointer to StringManager interned string which may be a single
         * category or a glob-like pattern. If StringManager.EMPTY, matches all patterns.
         * \param[in] node_id concrete category. This is a name, not a pattern.
         */
        static bool notificationCategoryMatch(const std::string* query_id,
                                              const std::string* node_id);

        // Public Notification System
        ////////////////////////////////////////////////////////////////////////
        //! @}

    private:

        /*!
         * \brief Increment the privacy level of all nodes in subtree
         */
        void incrementPrivacyLevel_(uint32_t privacy_increment)
        {
            privacy_level_ += privacy_increment;
            for (auto n : children_)
            {
                n->incrementPrivacyLevel_(privacy_increment);
            }
        }

        /*!
         * \brief Hook for reacting to or rejecting new children.
         * Invoked immediately before adding a child to this node (but after
         * onSettingParent_)
         * \param child Child to add. This child has been validated by
         * TreeNode::addChild and it ready to be finally attached as a
         * child.
         *
         * At this point, child is not yet a child, but will become an attached
         * child with absolute certainty immediately after successful return
         * from this function. This  method method is not required to do
         * anything at all.
         *
         * This method may be overridden to throw a SpartaException if the child
         * should not be added. The exception should explain why the child was
         * rejected. Only SpartaException can propogate from within this method.
         * If this function throws, the child will not be added and there will
         * be no signs that any attempt was made to add it except changes made
         * by onSettingParent_.
         */
        virtual void onAddingChild_(TreeNode* child) {
            (void) child;
            // No actions by default
            // Could throw to reject child
        }

        /*!
         * \brief Hook for reacting to or rejecting new children.
         * Invoked immediately before adding a child to this node (before
         * onAddingChild_)
         * \param parent Parent that will be set on this node. This node and the
         * parent have been validated by TreeNode::addChild and are ready to be
         * finally attached.
         * \node Because this occurs before onAddingChild during child-addition,
         * this test can have no side-effects
         *
         * At this point, this node is not yet a child of \a parent, but will
         * become an attached child immediately after successful return
         * from this function AND the passing of onAddingChild_ for this node.
         * This method method is not required to do anything at all.
         *
         * This method may be overridden to throw a SpartaException if the parent
         * should not be allowed for this child. The exception should explain
         * why the parent was rejected. Only SpartaException can propogate from
         * within this method.If this function throws, this node will not be
         * added as a child and there will be no signs that any attempt was made
         * to add it.
         */
        virtual void onSettingParent_(const TreeNode* parent) const {
            (void) parent;
            // No actions by default
            // Could throw to reject parent
        }

        /*!
         * \brief Hook for reacting to being part of an independent subtree that
         * has just been connected to an existing (ancestor) parent node. This
         * method is invoked on all nodes in the added subtree regardless of
         * depth.
         * \throw none
         *
         * The node on which this method is called is already a child of the new
         * subtree and cannot reject the connection. Throwing an exception
         * not valid.
         *
         * Typically, this method will be called recursively on a subtree that
         * gained one or more new ancestors so that all children can wak their
         * parents and examine them for observers on the notifications that
         * those children may generate
         */
        virtual void onAddedAsChild_() noexcept {
        }

        /*!
         * \brief Hook for reacting to having some descendant get a new child
         * node, which may have its own subtree attached already.
         * \param des Descendant that was just attached to this node or another
         * descendant.
         *
         * Children of the newly attached descendant \a des will not cause this
         * method to be invoked again. Any class which overrides this method
         * should walk the entire subtree of \a des.
         *
         * \todo Correct spelling of descendent in function name to descendant.
         */
        virtual void onDescendentSubtreeAdded_(TreeNode* des) noexcept {
            (void) des;
        }

        /*!
         * \brief Hook for reacting to having an immediate parent node destroyed
         * The parent can be accessed during this function, but not after.
         * Parent must be accessed as a TreeNode or base class. Subclasses of
         * TreeNode may have already been destructed.
         */
        virtual void onDestroyingParent_() noexcept {
        }

        /*!
         * \brief Hook for reacting to having an immediate child node destroyed
         * \param child Child that is being destroyed. This child can be
         * accessed during this function, but not after. child must be accessed
         * as a TreeNode or base class. Subclasses of TreeNode may have already
         * been destructed.
         */
        virtual void onDestroyingChild_(TreeNode* child) noexcept {
            (void) child;
        }

        /*!
         * \brief Called for each node during bindTreeEarly_ recursion
         */
        virtual void onBindTreeEarly_() {;}

        /*!
         * \brief Called for each node during bindTreeLate_ recursion
         */
        virtual void onBindTreeLate_() {;}

        /*!
         * \brief Hook for reacting to the entire device tree entering the
         * TREE_TEARDOWN phase. This is the last time at which it is safe to
         * access other resources or parents and children in the device tree
         * before they are destroyed and/or removed from the tree.
         * Soon after this call, other resources and nodes will begin begin
         * destructed. Immediately following this call, this Node's owned
         * resource (if any) will be utomatically alerted that teardown is
         * starting. Then, this node will be placed
         * into TREE_TEARDOWN phase followed by its children (in order of
         * construction) and then its siblings (in order of construction)
         * \note This is not a good place to validate simulator sanity at
         * shutdown. See validatePostRun_
         * \note This is not a good place to dump out debugging information or
         * counters. The former should be done with reports and athe latter with
         * the debug dumping framework. See dumpDebugContent_
         * \note The simulator will not continue to run for this device tree at
         * this point.
         * \note Regardless of whether this is overridden, the resource owned by
         * this node will be alerted of teardown through
         * Resource::onStartingTeardown_
         * \note This method is named named distinctly from
         * Resource::onStartingTeardown_ so that objects so that some class
         * could potentially inherit from both Resource and TreeNode.
         * \warning Do not attempt to delete any resources or children in this
         * call. The destructor for this TreeNode and its contained resource
         * will still be invoked later
         */
        virtual void onEnteringTeardown_() noexcept {
        }

        /*!
         * \brief Track another mapping between a name and a child TreeNode*.
         * \param name Name that refers to <child>
         * \param child Node that <name> maps to. For now, child should point to
         * a valid TreeNode or, if referring to a group, 0.
         * \warning Does not check name for uniqueness among other children.
         * Always check <name> with verifyUniqueChildIdentifier_ before calling
         * this method.
         * \node This cannot be used to add empty string identifiers (anonymous
         * nodes).
         *
         * The name mapping object exists for fast lookup of child nodes or
         * groups by name or alias.
         */
        void addChildNameMapping_(const std::string& name,
                                  TreeNode* child);

        /*!
         * \brief Implements locateNotificationSources using a name string
         * interned with StringManager.
         */
        template <typename DataT>
        uint32_t locateNotificationSources_(std::vector<TreeNode*>& nodes, const std::string* name_id){
            uint32_t additions = 0;

            const std::string* dummy;
            if(canGenerateNotification_(typeid(DataT), name_id, dummy)){
                nodes.push_back(this);
                ++additions;
            }

            for(TreeNode* child : children_){
                additions += child->template locateNotificationSources_<DataT>(nodes, name_id);
            }
            return additions;
        }

        /*!
         * \brief Notifies a single TreeNode that at some TreeNode obs_node,
         * the notification described by tinfo and name is now being observed
         * with delegate \a del. Other observers at \a obs_node and elsewhere
         * may already exist.
         * \param tinfo type info of the Notififcation payload
         * \param name_id Name of the notification being observed (or "" to mean
         * all of type tinfo) as string interned with Manager
         * \param obs_node TreeNode at which the notification is being observed
         * (not necessarily the only TreeNode where the notification is observed
         * and does not imply that there is only 1 observer at that node)
         * \param del Delegate that is now observing this node's notifications.
         * This delegate can be stored (copied or referenced) and invoked when
         * this node generates a notification matching \a tinfo and \a name_id
         * \pre This node replied true to canGenerateNotification(tinfo, name)
         * \note This may occur multiple times for the same obs_node if
         * observers are added. This does not change the meaning of
         * notificationObserverRemoved_ so no reference counting per observation
         * point (obs_node) is required.
         * \see notificationObserverRemoved_
         *
         * Implementer should track obs_node internally and notify it directly
         * when posting a notification rather than generating an actual
         * propagating notification
         */
        virtual void notificationObserverAdded_(const std::type_info& tinfo,
                                                const std::string* name_id,
                                                TreeNode* obs_node,
                                                const delegate* del) {
            (void) tinfo;
            (void) name_id;
            (void) obs_node;
            (void) del;
        }


        /*!
         * \brief Notifies a single TreeNode that at some TreeNode obs_node,
         * the notification described by tinfo and name is no longer being
         * observed by the delegate \a del. There may still be other observers
         * at \a obs_node or elsewhere.
         * \param tinfo type info of the Notififcation payload
         * \param name_id Name of the notification being observed (or "" to mean
         * all of type tinfo) as string interned with Manager.
         * \param obs_node TreeNode that had observers for this notification but
         * now has none. There may be observers at other TreeNodes, however.
         * \param del Delegate which is being removed. If this node stored the
         * delegate or a copy locally, it should be discarded at this point
         * \pre This node replied true to canGenerateNotification(tinfo, name)
         * \see notificationObserverAdded_
         *
         * Implementer should remove obs_node from an internally tracked list
         * and NOT post the notification directly to that node anymore.
         */
        virtual void notificationObserverRemoved_(const std::type_info& tinfo,
                                                  const std::string* name_id,
                                                  TreeNode* obs_node,
                                                  const delegate* del) {
            (void) tinfo;
            (void) name_id;
            (void) obs_node;
            (void) del;
        }

        /*!
         * \brief Implements TreeNode::canGenerateNotification. Node
         * subclasses which will post notifications must implement this method
         * and only post notifications with types and names that would cause
         * this function to return true.
         * \param[in] tinfo type_info from typeid on the type of
         * being queried.
         * \param[in] name Pointer to interned name of notifications to query
         * about (from StringManager::internString). If This value is
         * StringManager::EMPTY, looks for notifications having any name.
         * glob-like patterns must be supported by overrides of this method (see
         * matchesGlobLike). Name will refer to at most one name. If lists of
         * notification names are searched-for, they must be split up at a
         * higher level
         * \param[out] match Populated with the name of the notification which
         * can be generated in case \a name is a glob-like pattern. Only set
         * when returning true. Otherwise unmodified.
         * \return true if this node can post a notification having the given
         * type T where tinfo=typeid(T) and having a notification name matching
         * the parameter 'name'. If the parameter 'name' is
         * StringManager::EMPTY, matches on any notification name.
         * The returned value must be the same between calls for a given input.
         * The return value shall not change based on whether there are any
         * observers for this type of notification.
         * \note Potential notifications are not allowed to change with time
         * \note Any set of info for which this method returns true shall be
         * included in the result set of getPossibleNotifications_.
         */
        virtual bool canGenerateNotification_(const std::type_info& tinfo,
                                              const std::string* name,
                                              const std::string*& match) const {
            (void) tinfo;
            (void) name;
            (void) match;
            return false;
        }

        /*!
         * \brief Implements TreeNode::getPossibleNotifications. Node subclasses
         * which will post notifications must implement this method and post opnly
         * notifications with information retrieved by calls to this function.
         * \param infos Vector of infos to which new notifications shall be
         * appended. Do not clear this vector or modify its current content. Each
         * NotificationInfo added must contain this node as the origin.
         * \note infos will typically be empty
         * \node All elements added to the infos vector will be checked against
         * canGenerateNotification_ to ensure consistency.
         */
        virtual void getPossibleNotifications_(std::vector<NotificationInfo>& infos) const {
            (void) infos;
        }

        /*!
         * \brief Create any resources from this finalized device tree
         * \pre Device tree guaranteed be in the TREE_FINALIZING phase.
         * \pre All descendants in the tree guaranteed to be finalizing (and
         * laid out) before this point.
         * \note must be allowed to be called on a TreeNode that already loaded
         * a resource without error. Subclasses must support multiple
         * invokations without error so that tree-building is fault tolerant for
         * use in interactive simulator sessions.
         */
        virtual void createResource_() {
            sparta_assert(isFinalizing()); // Must be in the finalizing phase
        }

        /*!
         * \brief Allows validation to be performed on a single node in a
         * finalized Tree (e.g. making sure a statistical definition can be
         * evaluated).
         * \pre Device tree guaranteed be in the TREE_FINALIZED phase.
         * \pre All descendants in the tree guaranteed to be finalized (and
         * fully laid out) before this point.
         * \note Subclass implementations should support multiple invocations
         * of this node.
         */
        virtual void validateNode_() const {
        }

        /*!
         * \brief Return whether or not this node can see its child node
         * when doing getChild methods with respect to private nodes.
         * \param node the node of the child in question.
         * \return true if it can see the node.
         * see documentation for the privacy_level_ variable for more details.
         */
        bool canSeeChild_(const TreeNode* node) const
        {
            sparta_assert(node != nullptr);
            // If the node is not on the same privacy level then we can't see it.
            return (privacy_level_ == node->privacy_level_);
        }
        uint32_t findChildren_(const std::string& pattern,
                               std::vector<TreeNode*>& results,
                               std::vector<std::vector<std::string>>& replacements,
                               bool allow_private);

        /*!
         * \brief Version of findChildren with no replacements vector
         */
        uint32_t findChildren_(const std::string& pattern,
                               std::vector<TreeNode*>& results,
                               bool allow_private);
        /**
         * Implementation of registerForNotification that can decide whether or not
         * to register with private subtress as well.
         */
        template <typename DataT, typename T, void (T::*TMethod)(const TreeNode&, const TreeNode&, const DataT&)>
        void registerForNotification_(T* obj, const std::string& name, bool ensure_possible=true, bool allow_private=false)
        {
            (void)allow_private;
            const std::type_info& data_type = typeid(DataT);
            if(true == ensure_possible && false == canSubtreeGenerateNotification(data_type, name)){
                throw SpartaException("Cannot registerForNotification for data type \"")
                    << demangle(typeid(DataT).name()) << "\" and name=\"" << name << "\" on node " << getLocation()
                    << " with callback on \"" << demangle(typeid(T).name()) << "\""
                    << "\" because this notification cannot possibly be generated by any descendant of this "
                    << "node. Set ensure_possible=false to prevent this check if additional notification "
                    << "source descendants are expected to be added. "
                    << "It is possible the node generating the desired notification is in a private sub tree.";
            }

            DelegateVector& observers = obs_local_[data_type]; // Create notification map
            if(findDelegate_<DataT, T, TMethod>(observers, obj, name) != observers.end()){
                throw SpartaException("Already observing a notification for data type \"")
                    << demangle(typeid(DataT).name()) << "\" Name \"" << name << "\" on node " << getLocation()
                    << " with callback on \"" << demangle(typeid(T).name()) << "\""
                    << "\". Cannot register";
            }

            delegate d = delegate::from_method<DataT, T, TMethod>(obj, *this, name);
            observers.push_back(std::move(d));

            // Let children know
            broadcastRegistrationForNotificationListStringToChildren_(typeid(DataT), name, this, &observers.back(), allow_private);
        }

        // Overload which allows registration with a class member function accepting only a data argument
        template <typename DataT, typename T, void (T::*TMethod)(const DataT&)>
        void registerForNotification_(T* obj, const std::string& name, bool ensure_possible=true, const bool allow_private=false)
        {
            (void)allow_private;
            const std::type_info& data_type = typeid(DataT);
            if(true == ensure_possible && false == canSubtreeGenerateNotification(data_type, name)){
                throw SpartaException("Cannot registerForNotification for data type \"")
                    << demangle(typeid(DataT).name()) << "\" and name=\"" << name << "\" on node " << getLocation()
                    << " with callback on \"" << demangle(typeid(T).name()) << "\""
                    << "\" because this notification cannot possibly be generated by any descendant of this "
                    << "node. Set ensure_possible=false to prevent this check if additional notification "
                    << "source descendants are expected to be added. "
                    << "It is possible the node generating the desired notification is in a private sub tree.";
            }

            DelegateVector& observers = obs_local_[data_type]; // Create notification map
            if(findDelegate_<DataT, T, TMethod>(observers, obj, name) != observers.end()){
                throw SpartaException("Already observing a notification for data type \"")
                    << demangle(typeid(DataT).name()) << "\" Name \"" << name << "\" on node " << getLocation()
                    << " with callback on \"" << demangle(typeid(T).name()) << "\""
                    << "\". Cannot register";
            }

            delegate d = delegate::from_method<DataT, T, TMethod>(obj, *this, name);
            observers.push_back(std::move(d));

            // Let children know
            broadcastRegistrationForNotificationListStringToChildren_(typeid(DataT), name, this, &observers.back(), allow_private);
        }

        /*!
         * implment deregistration for notification with boolean for whether or not the
         * private subtrees should be considered.
         */
        template <typename DataT, typename T, void (T::*TMethod)(const TreeNode&, const TreeNode&, const DataT&)>
        void deregisterForNotification_(T* obj, const std::string& name, const bool allow_private)
        {
            (void)allow_private;
            const std::type_info& data_type = typeid(DataT);
            auto itr = obs_local_.find(data_type);
            if(itr == obs_local_.end()){
                throw SpartaException("Not currently observing any notification for data type \"")
                    << demangle(typeid(DataT).name()) << "\" Name \"" << name << "\" on node " << getLocation()
                    << " with callback on \"" << demangle(typeid(T).name()) << "\" function \"" << TMethod
                    << "\". Cannot deregister";
            }
            DelegateVector& observers = itr->second;
            DelegateVector::iterator d = findDelegate_<DataT, T, TMethod>(observers, obj, name);
            if(observers.end() == d){
                throw SpartaException("Not currently observing a notification for data type \"")
                    << demangle(typeid(DataT).name()) << "\" Name \"" << name << "\" on node " << getLocation()
                    << " . Attempted to deregister \"" << demangle(typeid(T).name()) << "\" function \"" << TMethod
                    << "\". Cannot deregister";
            }

            // Let children know that a delegate has been deregistered
            broadcastDeregistrationForNotificationListStringToChildren_(data_type, name, this, &(*d), allow_private);

            observers.erase(d);
        }
        // Overload which allows deregistration with a class member function accepting only a data argument
        template <typename DataT, typename T, void (T::*TMethod)(const DataT&)>
        void deregisterForNotification_(T* obj, const std::string& name, const bool allow_private)
        {
            (void)allow_private;
            const std::type_info& data_type = typeid(DataT);
            auto itr = obs_local_.find(data_type);
            if(itr == obs_local_.end()){
                throw SpartaException("Not currently observing any notification for data type \"")
                    << demangle(typeid(DataT).name()) << "\" Name \"" << name << "\" on node " << getLocation()
                    << " with callback on \"" << demangle(typeid(T).name()) << "\" function \"" << TMethod
                    << "\". Cannot deregister";
            }
            DelegateVector& observers = itr->second;
            DelegateVector::iterator d = findDelegate_<DataT, T, TMethod>(observers, obj, name);
            if(observers.end() == d){
                throw SpartaException("Not currently observing a notification for data type \"")
                    << demangle(typeid(DataT).name()) << "\" Name \"" << name << "\" on node " << getLocation()
                    << " . Attempted to deregister \"" << demangle(typeid(T).name()) << "\" function \"" << TMethod
                    << "\". Cannot deregister";
            }

            // Let children know that a delegate has been deregistered
            broadcastDeregistrationForNotificationListStringToChildren_(data_type, name, this, &(*d), allow_private);

            observers.erase(d);
        }
        /*!
         * Return access to all children public and private.
         * See TreeNodePrivateAttorney.
         */
        const TreeNode::ChildrenVector& getAllChildren_() const
        {
            return children_;
        }

        /*!
         * Helper method to implement getScopeRoot(), see getScopeRoot()
         */
        template <typename T> T *getScopeRootImpl_(T *node) const;

        /*!
         * Implement getChild, but has ability to return private children also when asked.
         */
        TreeNode* getChild_(const std::string& name,
                            bool must_exist,
                            bool private_also);

        //! Overloaded const-qualified getChild_
        const TreeNode* getChild_(const std::string& name,
                                 bool must_exist,
                                 bool private_also) const;

        /*!
         * Implement hasChild_ but also take a boolean for whether we should consider private
         * children as well.
         */
        bool hasChild_(const std::string& name, bool private_also) const noexcept;

    private:

        /*!
         * \brief Unique ID of this node
         */
        const node_uid_type node_uid_;

        /*!
         * \brief Name of this node (must not be nullptr)
         */
        const std::string* name_;

        /*!
         * \brief Is this node anonymous (name == NODE_NAME_ANONYMOUS)
         */
        const bool anon_;

        /*!
         * \brief Is this object is accessible through its parent's interfaces
         * for getting children by group and index. This is set in a special
         * constructor
         */
        const bool is_indexable_;

        /*!
         * \brief Group name pointer (interned in StringManager) (may be "" to
         * indicate no group)
         */
        const std::string * const group_ptr_;

        /*!
         * \brief Group index (may be GROUP_IDX_NONE to indicate no group)
         */
        const group_idx_type group_idx_;

        /*!
         * \brief Tags associated with this node.
         * \note Tags are stored as pointers to interned strings in
         * StringManager
         */
        std::vector<const std::string*> tags_;

        /*!
         * \brief Description of this TreeNode (interned in StringManager)
         */
        const std::string * const desc_ptr_;

        /*!
         * \brief Interned string containing parent's full location string.
         * Begins as nullptr and may be set during sparta TREE_TEARDOWN if this
         * node's parent is being removed.
         *
         * Is non-null pointer if this should be used to satisfy getLocation
         * calls instead of looking at the parent.
         *
         * Should only ever be set if parent_ is nullptr.
         *
         * This value is set by this node's parent in its destructor.
         */
        const std::string* parent_loc_;

        /*!
         * \brief Parent TreeNode (may be 0).
         */
        TreeNode* parent_;

        /*!
         * \brief Is this node attached. This is stored locally so that it is
         * available at teardown even if parent has been deleted
         */
        bool is_attached_;

        /*!
         * \brief This node's assigned clock (if any). Note that getClock walks
         * parents in an attempt to find the clock
         */
        const Clock* clock_;

        /*!
         * \brief Actual clock obtained from finalized device tree
         *
         * Nullptr until node is Finalized. When finalizing, if caches value of
         * getClock to ascertain nearest ancestor clock.
         */
        const Clock* working_clock_;

        /*!
         * \brief Set of extensions and their factories. Will only be turned into actual extension
         * objects when (if) accessed during simulation. Validation will occur at that time as well.
         */
        std::unordered_map<std::string, std::unique_ptr<ExtensionsBase>> extensions_;
        std::unordered_map<std::string, std::unique_ptr<ParameterSet>> extension_parameters_;
        std::unordered_map<std::string, std::function<ExtensionsBase*()>> extension_factories_;
        std::set<std::string> extension_names_;
        ExtensionDescriptorVec extension_descs_;

        //! \name Internal class mis-use metrics
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Number of times a pattern-based search has been performed on
         * this node since finalization
         */
        mutable uint32_t num_children_finds_;

        /*!
         * \brief Number of times a name-based get has been performed on this
         * node since finalization. Mutable so that const accesses may be
         * counted
         */
        mutable uint32_t num_children_gets_;

        ////////////////////////////////////////////////////////////////////////
        //! @}

        /*!
         * \brief Temporary expected parent during initialization for printing
         * location in errors without adding self to the device tree.
         */

        const TreeNode* expected_parent_;

        /*!
         * \brief Is this node in the GROUP_NAME_BUILTIN group
         */
        bool is_builtin_;

        /*!
         * \brief Has this node been hidden with markHidden
         * Should default to false
         */
        bool is_hidden_;

        /*!
         * \brief Vector of aliases for this node
         */
        AliasVector aliases_;

        /*!
         * \brief Shared pointer to self from which weak_refs can be made for
         * observers via getWeakPtr
         */
        SharedPtr self_ptr_;

        /*!
         * \brief Vector of TreeNode* children in order of addition as children
         * of this node. This must be used as an append-only vector and cannot
         * be resorted in order to guarantee that child creation occurs in
         * added-order.
         *
         * Contains BOTH public and private children pointers, for public only
         * see public_children_.
         */
        ChildrenVector children_;

        /*!
         * \brief Mapping of names (aliases, child names) to objects for fast
         * lookup by string. Groups names map to NULL pointer.
         */
        ChildNameMapping names_;

        /*!
         * \brief Map of observers registered on this node.
         *
         * These observers will always be notified of the appropriate events on
         * this node or children.
         */
        NotificationObserverMap obs_local_;

        /*!
         * \brief Is this TreeNode expired (e.g. has it been destructed or used
         * as the rhp in a move constructor)
         */
        bool is_expired_;

        /*!
          privacy_level_ is used to implement a tighter scoping of which parts of the tree are accessible to others.
          This can be used to prevent sub-components leaking implementation to other components that should only know
          about the parent component, and not necessarily their children.

          The privacy_level_ ONLY affects calls to getChildren/findChildren/etc on parent nodes.
          The relationship that matters is whether or not the privacy_level_ of the child is the same as the parent.
          The value of privacy_level_ only has any meaning up and down the subtree,
          side to side on the tree (siblings) privacy levels can be completely different values etc.
          The idea is when recursing down the tree, you cant recurse to a child node that
          is more private than the current node.

          Doing it this way allows for subtrees that are more and more private.
               A(0)
            /        \
         B(1)       C(0)
           |          |
         D(1)       E(1)
           |          |
         F(2)       G(2)

         In this example "A" can be used to get to C,
         but NOT B. A handle on B can be used to get to D,
         but not F. The fact that F and G both have a privacy level of 2
         has no meaning other than their privacy levels are greater than their respective parents.

         A notification from F cannot be been from B in this since registration for callbacks relies
         on getChildren to find the notification source. This is the main desire.

         This value is set when using addChildPrivate.
         */
        uint32_t privacy_level_ = 0;

        /*!
         * True if this tree node is the root of a search scope.
         *
         * The main use case of search scopes is to limit the set of
         * notification sources a resource can register for without having to
         * change the resource. Such resources have to be well behaved and call
         * getScopeRoot() and use the returned tree node to register for
         * notifications. Since getScopeRoot() returns the tree node's first
         * ancestor that is a scope root, only notification sources in the
         * sub-tree rooted at the scope root will be registered for.
         */
        bool is_scope_root_ = false;

        /*!
         * \brief Container for static members which allocate things on the heap
         * These are destructed during static destruction which has an undefined
         * order as per the C++ spec. This container class has controlled
         * construction and destruction.
         */
        class TreeNodeStatics {
        public:
            /*!
             * \brief Vector of nodes having no parent so that the virtual global
             * node can notify them of observation if needed
             */
            std::map<const TreeNode*, WeakPtr> parentless_map_;

            /*!
             * \brief Map of node pointers (which may be deleted) to weak
             * pointers.  This helps debug which nodes were constructed but not
             * deallocated
             *
             * There are 3 things that this does:
             * \li Detects memory leaks in the form of unfreed nodes
             * \li Detects Nodes which were allocated with placement new, but never
             * explicitly destructed
             * \li Can be used to detect whether a node causing an error/segfault
             * has already been deallocated or not.
             */
            std::map<const TreeNode*, WeakPtr> node_map_;
        };
        static TreeNodeStatics *statics_;

        /*!
         * \brief Unique ID assigned to each SPARTA TreeNode
         *
         * This value is constrained by MAX_NODE_UID
         */
        static node_uid_type next_node_uid_;

        /*!
         * \brief Static map of tags to a vector of all nodes having that tag
         */
        static TagsMap global_tags_map_;

        /*!
         * \brief Number of errors encountered during teardown. Errors
         * suppressed after /a TEARDOWN_ERROR_LIMIT errors.
         */
        static uint32_t teardown_errors_;
    };

    //! \brief TreeNode stream operator
    template<class Ch,class Tr>
    inline std::basic_ostream<Ch,Tr>&
    operator<< (std::basic_ostream<Ch,Tr>& out, sparta::TreeNode const & tn) {
        out << tn.stringize();
        return out;
    }

    //! \brief TreeNode stream operator
    inline std::ostream& operator<< (std::ostream& out, sparta::TreeNode const * tn) {
        if(nullptr == tn){
            out << "null";
        }else{
            out << tn->stringize();
        }
        return out;
    }
} // namespace sparta

// __TREE_NODE_H__
#endif
