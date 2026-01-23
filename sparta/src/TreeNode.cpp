// <TreeNode> -*- C++ -*-

/*!
 * \file TreeNode.cpp Implements TreeNode class
 * \brief Basic Node framework in sparta device tree composite pattern
 */

#include "sparta/simulation/TreeNode.hpp"

#include <cassert>
#include <exception>
#include <iostream>
#include <fstream>
#include <string>
#include <ostream>
#include <vector>
#include <sstream>
#include <stack>
#include <regex>
#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <iterator>

#include <boost/algorithm/string.hpp>

#include "sparta/simulation/TreeNodeExtensions.hpp"
#include "sparta/simulation/Clock.hpp"
#include "sparta/simulation/VirtualGlobalTreeNode.hpp"
#include "sparta/simulation/Resource.hpp"
#include "sparta/utils/Colors.hpp"
#include "sparta/kernel/Scheduler.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/ParameterTree.hpp"
#include "sparta/app/Simulation.hpp"
#include "sparta/app/SimulationConfiguration.hpp"
#include "sparta/simulation/RootTreeNode.hpp"
#include "sparta/log/MessageSource.hpp"
#include "sparta/utils/Printing.hpp"
#include "sparta/utils/Utils.hpp"

namespace sparta
{

VirtualGlobalTreeNode* VirtualGlobalTreeNode::getInstance() {
    static sparta::VirtualGlobalTreeNode vgtn;
    return &vgtn;
}

TreeNode* TreeNode::getVirtualGlobalNode() {
    return VirtualGlobalTreeNode::getInstance();
}

const std::map<const TreeNode*, TreeNode::WeakPtr>& TreeNode::getParentlessNodes()
{
    return statics_->parentless_map_;
}

const std::map<const TreeNode*, TreeNode::WeakPtr>& TreeNode::getAllNodes()
{
    return statics_->node_map_;
}

std::string TreeNode::formatAllNodes()
{
    std::stringstream ss;
    ss << statics_->node_map_.size() << " TreeNodes known:" << std::endl;
    for(auto& n : statics_->node_map_){
        if(n.second.expired() == true){
            ss << "expired!" << std::endl;
        }else{
            ss << n.second.lock()->getLocation() << std::endl;
        }
    }
    return ss.str();
}

bool TreeNode::isNodeConstructed(const TreeNode* n)
{
    auto itr = statics_->node_map_.find(n);
    if(itr != statics_->node_map_.end()){
        if(itr->second.expired() == false){
            sparta_assert(itr->second.lock().get() == n); // Mismatched entry. first != second.get()
            return true;
        }
    }
    return false;
}

TreeNode::TreeNode(TreeNode* parent,
                   const std::string& name,
                   const std::string& group,
                   group_idx_type group_idx,
                   const std::string& desc,
                   bool is_indexable) :
    node_uid_(getNextNodeUID_()),
    name_(StringManager::getStringManager().internString(name)),
    anon_(name == NODE_NAME_NONE),
    is_indexable_(is_indexable),
    group_ptr_(StringManager::getStringManager().internString(group)),
    group_idx_(group_idx),
    desc_ptr_(StringManager::getStringManager().internString(desc)),
    parent_loc_(nullptr),
    parent_(nullptr), // Initialized in body
    is_attached_(false),
    clock_(nullptr),
    working_clock_(nullptr),
    num_children_finds_(0),
    num_children_gets_(0),
    expected_parent_(nullptr),
    is_builtin_(GROUP_NAME_BUILTIN == group),
    is_hidden_(false),
    self_ptr_(this, [](TreeNode*){}), // no deleter
    is_expired_(false)
{
    // Try and catch a class of mis-matched compiler definition bugs between
    // clang and stdc++ built with gcc. This is just sanity checking
    sparta_assert(self_ptr_.use_count() == 1);
    sparta_assert(self_ptr_);

    // Store expected parent until construction args are validated
    // This is here so that error printouts can show the expected parent
    if(parent != nullptr){
        setExpectedParent_(parent);
    }

    // Validate inputs. This determines whether the
    validateName(name);
    validateGroup(name, group, group_idx);
    validateDesc(desc);

    // Finally add after name/group validation
    if(parent != nullptr){
        parent->addChild(this);
    }

    // For debugging unfreed nodes. Store name because location may
    // change
#ifdef TREENODE_LIFETIME_TRACE
    std::ofstream("construction.txt", std::ios::app) << *name_ << " @"
                                                     << (void*)this << std::endl;
#endif // #ifdef TREENODE_LIFETIME_TRACE

    // Track the node in the global list. This is a noexcept method
    // This must be done after all possible failure points in the
    // ctor because the TreeNode dtor is the only place this node
    // is removed from the node tracking list
    sparta::TreeNode::trackNode_(this);

    // Track this node in global parentless list immediately so that
    // logging works
    sparta::TreeNode::trackParentlessNode_(this);
}

TreeNode::TreeNode(TreeNode&& rhp) :
    node_uid_(rhp.getNodeUID()),
    name_(rhp.name_),
    anon_(rhp.anon_),
    is_indexable_(rhp.is_indexable_),
    group_ptr_(rhp.group_ptr_),
    group_idx_(rhp.group_idx_),
    desc_ptr_(rhp.desc_ptr_),
    parent_loc_(rhp.parent_loc_),
    parent_(nullptr), // Initialized in body
    is_attached_(rhp.is_attached_),
    clock_(rhp.clock_),
    working_clock_(rhp.working_clock_),
    num_children_finds_(0), // Reset
    num_children_gets_(0), // Reset
    expected_parent_(rhp.expected_parent_),
    is_builtin_(rhp.is_builtin_),
    is_hidden_(rhp.is_hidden_),
    self_ptr_(this, [](TreeNode*){}), // no deleter
    children_(), // Do not inherit
    names_(), // Do not inherit
    obs_local_(), // Do not inherit
    is_expired_(false)
{
    sparta_assert(rhp.isFinalized() == false,
                  "Cannot move-construct a TreeNode from a finalized right-hand operand");

    // Ensure no notifications
    sparta_assert(obs_local_.size() == 0,
                  "Cannot move-construct based on a TreeNode which has notifications being "
                  "observed directly on it");

    // Store expected parent until construction args are validated
    // This is here so that error printouts can show the expected parent
    TreeNode* parent = rhp.parent_;
    if(parent != nullptr){
        setExpectedParent_(parent);
    }

    // Note: asume name/group/desc have already been validated

    // Detach rhp from parent & children
    std::vector<TreeNode*> children = rhp.children_;
    rhp.detachFromParent_();
    rhp.detachFromChildren_();
    rhp.is_expired_ = true; // Set after detaching node
    // WARNING: do not clear rhp.parent_ here because subclass constructors need
    // access to rhp.parent_ to actually attach this new node to the tree

    //std::cout << "Move constructing " << (void*)this << " from " << rhp << " @ " << (void*)&rhp << std::endl;

    // WARNING: Do not attach self after name/group validation. Because this is
    // not constructed, it will not dynamic cast to the appropriate type.
    // Therefore, attaching this node as a child must be must be done outside of
    // this function
    //if(parent != nullptr){
    //    parent->addChild(this);
    //}

    // Add children. Assumes that this node's type is not relevant (i.e. it will
    // not be dynamically cast during child attachment)
    for(TreeNode* child : children){
        addChild(child);
    }

    // Track the node in the global list. This is a noexcept method
    // This must be done after all possible failure points in the
    // ctor because the TreeNode dtor is the only place this node
    // is removed from the node tracking list
    sparta::TreeNode::trackNode_(this);

    // Track this node in global parentless list immediately so that
    // logging works
    sparta::TreeNode::trackParentlessNode_(this);
}


TreeNode::TreeNode(TreeNode* parent,
                   const std::string& name,
                   const std::string& group,
                   group_idx_type group_idx,
                   const std::string& desc) :
    TreeNode(parent, name, group, group_idx, desc, true)
{
}

TreeNode::TreeNode(const std::string& name,
                   const std::string& group,
                   group_idx_type group_idx,
                   const std::string& desc) :
    TreeNode(nullptr, name, group, group_idx, desc)
{
}

TreeNode::TreeNode(TreeNode* parent,
                   const std::string& name,
                   const std::string& desc) :
    TreeNode(parent, name, GROUP_NAME_NONE, GROUP_IDX_NONE, desc)
{
}

TreeNode::TreeNode(TreeNode* parent,
                   const std::string& group,
                   group_idx_type group_idx,
                   const std::string& desc) :
    TreeNode(parent, NODE_NAME_NONE, group, group_idx, desc)
{
}

TreeNode::TreeNode(const std::string& name,
                   const std::string& desc) :
    TreeNode(name, GROUP_NAME_NONE, GROUP_IDX_NONE, desc)
{
}

TreeNode::~TreeNode()
{
    //if(isExpired()){
    //    std::cout << "Destructing expired " << *this << " @ " << (void*)this << std::endl;
    //}else{
    //    std::cout << "Destructing non-expired " << *this << " @ " << (void*)this << std::endl;
    //}

    // For debugging unfreed nodes
#ifdef TREENODE_LIFETIME_TRACE
    std::ofstream("destruction.txt", std::ios::app) << *name_ << " @"
                                                    << (void*)this << std::endl;
#endif // #ifdef TREENODE_LIFETIME_TRACE

    // Stop tracking on parentless list, even if never tracked
    sparta::TreeNode::untrackParentlessNode_(this);

    // Stop tracking this node. Expects this node to be in the list.
    // This must be done before doon in case
    sparta::TreeNode::untrackNode_(this);

    // Remove from tag map
    // Note that this includes a linear search for each tag removed.
    //! \todo Optimize so that the global tag map tag lists are linked
    //! lists and each node stores a pointer to it's entry in that list for quick removal
    for(const std::string* tag_id : tags_){
        std::vector<TreeNode*>& tag_vec = global_tags_map_[tag_id];
        auto itr = std::find(tag_vec.begin(), tag_vec.end(), this);
        if(itr != tag_vec.end()){
            tag_vec.erase(itr);
        }
    }

    // Do not remove descendant notification shortcuts here because they are
    // important for destruction-time logging

    // Only try and remove from parent if this TreeNode has not expired.
    // If expired, it might be wrong about its parent. The only way something
    // can become expired is by being the rhp of a move.
    if(false == isExpired()){
        detachFromParent_();
    }

    detachFromChildren_();

    // Destruction when not tearing down is illegal if "attached" to a tree with
    // a root
    if(false == isExpired() && isAttached() && false == isTearingDown()){
        if(teardown_errors_ < TEARDOWN_ERROR_LIMIT){
            std::cerr
                << "Destructing node \"" << getLocation()
                << "\" which has a parent while Tree is not in TREE_TEARDOWN phase. This \n"
                "tree must enter teardown through RootTreeNode::enterTeardown()' \n"
                "before any nodes within it are deleted. The explicit teardown phase \n"
                "is a protection against accidental deletion of nodes during \n"
                "simulation. I.e. you forgot to call rts.enterTeardown();" << std::endl;
            // FIXME should we call std::terminate here or are these really warnings about ugly teardown?
        }else if(teardown_errors_ == TEARDOWN_ERROR_LIMIT){
            std::cerr << "... More than " << teardown_errors_ << " warnings occurred where a "
                "TreeNode was destroyed without being in the teardown phase. This warning will be "
                "suppressed for the duration of this application instance." << std::endl;
        }
        ++teardown_errors_;
    }
}

void TreeNode::detachFromParent_()
{
    // Inform Parent
    if(parent_){
        removeFromParentForTeardown_(parent_);
    }
}

void TreeNode::detachFromChildren_()
{
    // Inform Children & set cached location string
    const std::string* my_loc = nullptr;
    if(children_.size() > 0){
        auto& strmgr = StringManager::getStringManager();
        my_loc = strmgr.internString(getLocation());
    }
    for(TreeNode* child : children_){
        child->onDestroyingParent_();

        // Unset this as children's parent, but store its location
        // string in case it is needed at destruction.
        child->parent_loc_ = my_loc;
        child->parent_ = nullptr;
    }
}


// Identification

void TreeNode::addAlias(const std::string& alias) {
    ensureNoParent_("set alias");
    validateName(alias);

    if(alias.size() == 0){
        throw SpartaException("Cannot add alias \"")
            << alias << "\" to TreeNode \""
            << getLocation() << "\" because aliases must not be empty strings";
    }

    // Must not be built becuse alias name collisions are resolved when
    // attaching to the tree. Can always create a TreeNode, add aliases, and
    // then attach to a tree that is in any state prior to TREE_FINALIZED
    if(isBuilt()){
        throw SpartaException("Cannot add alias \"")
            << alias << "\" to TreeNode \""
            << getLocation() << "\" because tree is already built";
    }

    // Check against existing
    for(const std::string& existing : aliases_){
        if(existing == alias){
            throw SpartaException("Cannot add alias \"")
                << alias << "\" to TreeNode \""
                << getLocation() << "\" because it is already present";
        }
    }

    aliases_.push_back(alias);
}

void TreeNode::addAliases(const AliasVector& v) {
    for(const std::string& alias : v){
        addAlias(alias);
    }
}

void TreeNode::addTag(const std::string& tag) {
    validateName(tag);

    if(tag.size() == 0){
        throw SpartaException("Cannot add tag \"")
            << tag << "\" to TreeNode \""
            << getLocation() << "\" because tags must not be empty strings";
    }

    // Allow adding tags to parameters and parameterSet while tree is not
    // finalizing (since parent state has not propogated to children yet)
    // Previously just allowed only nodes in finalizing state.
    if(getPhase() >= TREE_FINALIZED){
        throw SpartaException("Cannot add tag \"")
            << tag << "\" to TreeNode \""
            << getLocation() << "\" because tree is already finalized. Add tags before tree is finalized (immutable)";
    }

    const std::string* const tag_id = StringManager::getStringManager().internString(tag);

    // Check against existing
    for(const std::string* existing : tags_){
        if(existing == tag_id){
            throw SpartaException("Cannot add tag \"")
                << tag << "\" to TreeNode \""
                << getLocation() << "\" because it is already present";
        }
    }

    tags_.push_back(tag_id);
    std::vector<TreeNode*>& tag_vec = global_tags_map_[tag_id];
    tag_vec.push_back(this);
}

void TreeNode::addTags(const std::vector<std::string>& v){
    for(const std::string& tag : v){
        addTag(tag);
    }
}

TreeNode::node_uid_type TreeNode::getNodeUID() const {
    return node_uid_;
}

const std::string& TreeNode::getName() const {
    sparta_assert(name_ != nullptr);
    return *name_;
}

const std::string* TreeNode::getNamePtr() const {
    sparta_assert(name_ != nullptr);
    return name_;
}

bool TreeNode::isAnonymous() const {
    return anon_;
}

bool TreeNode::isExpired() const {
    return is_expired_;
}

bool TreeNode::isIndexableByGroup() const {
    return is_indexable_;
}

const std::string& TreeNode::getGroup() const {
    return *group_ptr_;
}

const std::string* TreeNode::getGroupNamePtr() const {
    return group_ptr_;
}

TreeNode::group_idx_type TreeNode::getGroupIdx() const {
    return group_idx_;
}

const std::vector<const std::string*>& TreeNode::getTags() const {
    return tags_;
}

bool TreeNode::hasTag(const std::string& tag) const {
    for(const std::string* t : tags_){
        if(*t == tag){
            return true;
        }
    }
    return false;
}

bool TreeNode::hasTag(const std::string* interned_tag_name) const {
    for(const std::string* t : tags_){
        if(t == interned_tag_name){ // pointer comparison
            return true;
        }
    }
    return false;
}

const std::string& TreeNode::getDesc() const {
    return *desc_ptr_;
}

bool TreeNode::isBuiltin() const {
    return is_builtin_;
}

void TreeNode::markHidden(bool hidden) {
    is_hidden_ = hidden;
}

bool TreeNode::isHidden() const {
    return is_hidden_;
}


// Validation

void TreeNode::validateName(const std::string& nm)
{
    for(const char* rsrv : RESERVED_WORDS){
        if(nm == rsrv){
            throw SpartaException("TreeNode name \"")
                << nm << "\" is a reserved word. Expected location was " << getLocation();
        }
    }

    std::string::size_type pos = nm.find("__");
    if(pos != std::string::npos && (pos == 0 || pos == (nm.size() - 3))) {
        throw SpartaException(" TreeNode name \"")
            << nm << "\" contains two adjacent underscores (at the beginning or end) which is not permitted. "
            "Expected location was " << getLocation();
    }

    if(nm.find_first_of(DIGIT_CHARS) == 0){
        throw SpartaException(" TreeNode name \"")
            << nm << "\" begins with a '" << nm[nm.size()-1]
            << "' character which is not permitted. "
            << "A TreeNode name must not begin with a decimal digit. Expected location was "
            << getLocation();;
    }

    pos = nm.find_first_not_of(ALPHANUM_CHARS);
    if(pos != std::string::npos){
        throw SpartaException("TreeNode name \"")
            << nm << "\" contains a '" << nm[pos] << "', which is not permitted. "
            << "A TreeNode name must contain only alphanumeric characters and underscores. "
            "Expected location was " << getLocation();
    }
}

void TreeNode::validateGroup(const std::string& name,
                             const std::string& group,
                             group_idx_type idx)
{
    size_t pos = group.find_first_not_of(ALPHANUM_CHARS);
    if(pos != std::string::npos){
        throw SpartaException("TreeNode group \"")
            << group << "\" contains a '" << group[pos]
            << "', which is not permitted. A TreeNode group must "
            << "contain only alphanumeric characters and underscores.";
    }

    for(const char* rsrv : RESERVED_WORDS){
        if(group == rsrv){
            throw SpartaException("TreeNode group \"")
                << group << "\" is a reserved word. ";
        }
    }

    if(group.find("__") != std::string::npos){
        throw SpartaException(" TreeNode group \"")
            << group << "\" contains two adjacent underscores which "
            " is not permitted";
    }

    // Because we should be able to have the content of a group (such as cpu)
    // be indexed in a Python shell both as cpu0 and cpu[0], any group ending in
    // a number can make this notation confusing. For example. group "v7" may
    // contain an object with index 1. Then v7[1] would be alised by v71.
    // We can relax this constraint if needed, since it isn't strictly necessary
    if(group.size() > 0 && group.find_last_of(DIGIT_CHARS) == group.size()-1){
        throw SpartaException(" TreeNode group \"")
            << group << "\" ends with a '" << group[group.size()-1]
            << "' character which is not permitted. "
            << "A TreeNode group must not end with a decimal digit.";
    }

    // Cannot end with decimal because Python cannot use the group as a variable
    if(group.find_first_of(DIGIT_CHARS) == 0){
        throw SpartaException(" TreeNode group \"")
            << group << "\" begins with a '" << group[group.size()-1]
            << "' character which is not permitted. "
            << "A TreeNode group must not begin with a decimal digit.";
    }

    if(name == NODE_NAME_NONE && group == GROUP_NAME_NONE){
        throw SpartaException(" TreeNode name is NODE_NAME_NONE, ")
            << "indicating an anonymous node but group is GROUP_NAME_NONE. "
            "Group cannot be GROUP_NAME_NONE in an anonymous node.";
    }

    if(group == GROUP_NAME_NONE && idx != GROUP_IDX_NONE){
        throw SpartaException(" TreeNode group is GROUP_NAME_NONE, but a ")
            << "group index was specified. A TreeNode group index cannot "
            "be set unless the group name is also set.";
    }

    if(group == GROUP_NAME_BUILTIN){
        if(idx != GROUP_IDX_NONE){
            throw SpartaException(" TreeNode group is GROUP_NAME_BUILTIN, ")
                << "but a group index was specified. A TreeNode group "
                "index cannot be set unless the group name is also "
                "set to something other than GROUP_NAME_BUILTIN or \"\"";
        }
    }else if(group != GROUP_NAME_NONE && idx == GROUP_IDX_NONE){
        throw SpartaException(" TreeNode group is not GROUP_NAME_NONE or ")
            << "GROUP_NAME_BUILTIN, but no group index was specified. "
            << "A TreeNode group index cannot be set unless the group "
            << "name is also set.";
    }
}

void TreeNode::validateDesc(const std::string& desc)
{
    // Stops laziness. Descriptions are usful for the end-user.
    // Please strongly reconsider before commenting this
    if(desc.size() == 0){
        throw SpartaException("TreeNode description string of node named '")
            << *name_<< "' cannot be empty";
    }
}


// Navigation and State

TreeNode* TreeNode::getRoot() {
    TreeNode* n;
    TreeNode* parent = this;
    do{
        n = parent;
        parent = n->getParent();
    }while(parent != nullptr);

    return n;
}

const TreeNode* TreeNode::getRoot() const {
    const TreeNode* n;
    const TreeNode* parent = this;
    do{
        n = parent;
        parent = n->getParent();
    }while(parent != nullptr);

    return n;
}

template <typename T>
T *TreeNode::getScopeRootImpl_(T *node) const
{
    auto parent = node;
    do {
        if (parent->isScopeRoot()) {
            return parent;
        }
        parent = parent->getParent();
    } while (parent != nullptr);

    sparta_assert(true,
                  "Couldn't find the scope root. The root node should defines "
                  "a default scope, so if no scopes have been explicitly defined, "
                  "getScopeRoot() should return the root tree node. Is this node not "
                  "an decendant of a RootTreeNode?");

    /* We'll never get here but this return statment is needed to make the
     * compiler happy. */
    return nullptr;
}

TreeNode *TreeNode::getScopeRoot()
{
    return getScopeRootImpl_(this);
}

const TreeNode *TreeNode::getScopeRoot() const
{
    return getScopeRootImpl_(this);
}

const TreeNode* TreeNode::getExpectedRoot() const {
    const TreeNode* n;
    const TreeNode* parent = this;
    do{
        n = parent;
        parent = n->getParent();
        if(parent == nullptr){
            parent = n->expected_parent_; // Check expected parent if there is no actual parent
        }
    }while(parent != nullptr);

    return n;
}

app::Simulation* TreeNode::getSimulation() const {
    const TreeNode* n = getRoot();
    if(!n){
        return nullptr;
    }
    const RootTreeNode* r = dynamic_cast<const RootTreeNode*>(n);
    if(!r){
        r = dynamic_cast<const RootTreeNode*>(getExpectedRoot());
        if(!r) {
            return nullptr;
        }
    }
    return r->getSimulator();
}

uint32_t TreeNode::getNumChildren() const {
    return getChildren().size();
}

TreeNode* TreeNode::getChildAt(uint32_t idx) const {
    if(idx >= getChildren().size()){
        SpartaException ex("Cannot access child ");
        ex << idx << " of TreeNode \"" << getLocation()
           << "\" because it has only " << getChildren().size()
           << " children";
        throw ex;
    }
    return getChildren()[idx];
}

const TreeNode::ChildrenVector TreeNode::getChildren() const {
    // The getChildren call should only return public children, i.e. the
    // children with the same privacy level as this tree node. To get all
    // children, public and private, call getAllChildren_().

    TreeNode::ChildrenVector p;
    auto privacy_level = privacy_level_; // Enable lambda capture

    std::copy_if(children_.begin(), children_.end(), std::back_inserter(p),
                 [privacy_level](TreeNode *child) {
                     return child->privacy_level_ == privacy_level;
                 }
        );

    return p;
}

uint32_t TreeNode::getChildren(std::vector<TreeNode*>& results,
                               bool ignore_builtin_group,
                               bool ignore_anonymous_nodes) {
    uint32_t found = 0;
    for(TreeNode* child : getChildren()){
        if(ignore_builtin_group && child->getGroup() == GROUP_NAME_BUILTIN){
            continue;
        }
        if(ignore_anonymous_nodes && child->isAnonymous()){
            continue;
        }
        results.push_back(child);
        ++found;
    }
    return found;
}

const TreeNode::AliasVector& TreeNode::getAliases() const {
    return aliases_;
}

uint32_t TreeNode::getChildrenIdentifiers(std::vector<std::string>& idents,
                                          bool ignore_builtin_group) const {
    uint32_t found = 0;
    idents.clear();
    for(const auto& nm : names_){
        if(ignore_builtin_group && nm.first == GROUP_NAME_BUILTIN){
            continue;
        }
        idents.push_back(nm.first);
        ++found;
    }
    return found;
}

std::vector<const std::string*> TreeNode::getIdentifiers() const {
    // Add name and aliases as entries in names map while checking for collisions
    std::vector<const std::string*> results;
    if(getName().size() > 0){
        results.emplace_back(&getName());
    }
    for(const std::string& alias : getAliases()){
        results.emplace_back(&alias);
    }
    return results;
}

uint32_t TreeNode::getGroupSize(const std::string& group) {
    uint32_t found = 0;
    for(TreeNode* child : getChildren()){
        if(child->getGroup() == group){
            ++found;
        }
    }

    return found;
}

TreeNode::group_idx_type TreeNode::getGroupIndexMax(const std::string& group) {
    if(group == GROUP_NAME_NONE){
        return GROUP_IDX_NONE;
    }

    group_idx_type largest=GROUP_IDX_NONE;
    for(TreeNode* child : getChildren()){
        if(child->getGroup() == group){
            if(largest == GROUP_IDX_NONE){
                largest = child->getGroupIdx();
            }else{
                largest = std::max(largest, child->getGroupIdx());
            }
        }
    }
    return largest;
}

uint32_t TreeNode::getGroup(const std::string& group,
                            std::vector<TreeNode*>& results) {
    const std::string* const id = StringManager::getStringManager().internString(group);
    uint32_t found = 0;
    for(TreeNode* child : getChildren()){
        if(child->getGroupNamePtr() == id){
            results.push_back(child);
            ++found;
        }
    }

    return found;
}

uint32_t TreeNode::findChildren_(const std::string& pattern,
                                 std::vector<TreeNode*>& results,
                                 std::vector<std::vector<std::string>>& replacements,
                                 bool allow_private)
{
    auto scheduler = getScheduler();
    if(isFinalized()
       && scheduler && scheduler->getNumFired() > 1
       && (++num_children_finds_ == CHILD_FIND_THRESHOLD)){
        //! \todo Warn via logger
        std::cerr << "Warning: there have been " << num_children_finds_
                  << " pattern searches for this node since finalization. "
                  << "This is very likely misuse of the framework!. "
                  << "This notice was printed from within \""
                  << getLocation() << "\" with the pattern \"" << pattern
                  << "\". Ensure that children are not being searched for "
                  << "regularly while the simulator is running" << std::endl;
    }

    size_t name_pos = 0;
    uint32_t num_matches = 0;
    if(pattern.size() == 0){
        results.push_back(this);
        replacements.push_back({}); // No replacements for empty pattern
        return 1;
    }

    // Parse next level of pattern betwee '.'s
    const std::string sub_pattern = getNextName(pattern, name_pos);
    std::string remaining_pattern;
    if(name_pos != std::string::npos){
        remaining_pattern = pattern.substr(name_pos);
    }

    // Ascend to parent if a '.' is followed by another dot immediately
    // before it or a '.' is encountered at the start/end of the string
    if(sub_pattern.size() == 0)
    {
        // Get parent instead of child
        log::MessageSource::getGlobalDebug()
            << "Empty pattern element, moving up to parent from: " << getLocation();

        TreeNode* tmp = getParent();
        if(nullptr == tmp){
            //! \todo show warning through logger instead
            log::MessageSource::getGlobalWarn()
                << "TreeNode::findChildren: Node \"" << getLocation()
                << "\" has no parent. Error trying to search with "
                << " pattern \"" << pattern << "\"";
        }else if(name_pos == std::string::npos){ // Reached end of pattern
            results.push_back(tmp);
            replacements.push_back({}); // No replacements for parent pattern
            ++num_matches;
        }else{
            // Recursively find children
            num_matches += tmp->findChildren_(remaining_pattern, results, replacements,
                                              allow_private);
        }
    }
    else
    {
        // Using regex here to find children because full regex pattern
        // should probably be supported eventually. Having some regex
        // code here already should encourage that.

        std::string patexp = createSearchRegexPattern(sub_pattern);

        //std::cout << "Local search pattern is \"" << patexp << "\" from " << getLocation()
        //          << " extracted from pattern \"" << pattern << "\"" << std::endl;
        std::regex expr(patexp);

        // Get the immediate children of this node matching the first part of
        // the pattern
        std::vector<TreeNode*> immediate_children;
        std::vector<std::vector<std::string>> immediate_replacements;
        findImmediateChildren_(expr, immediate_children, immediate_replacements,
                               allow_private);

        // This is logic for tracking all wildcard replacements while
        // recursively searching via findChildren until the pattern runs out.
        uint32_t idx = 0;
        for(TreeNode* child : immediate_children){
            if(name_pos == std::string::npos){ // Reached end of pattern
                results.push_back(child);
                replacements.push_back(immediate_replacements.at(idx)); // Add replacements for this child
                ++num_matches;
            }else{
                // Search in next level for all these children
                std::vector<std::vector<std::string>> descendent_replacements;
                std::vector<TreeNode*> descendent_children;
                num_matches += child->findChildren_(remaining_pattern, descendent_children,
                                                    descendent_replacements,
                                                    allow_private);

                // Append descendent replacements to immediate replacements for
                // this immediate child (at idx) and each descendent child in
                // descendent_children
                uint32_t subidx = 0;
                for(TreeNode* subchild : descendent_children){
                    results.push_back(subchild);
                    replacements.push_back(immediate_replacements.at(idx));
                    std::vector<std::string>& added = descendent_replacements.at(subidx);
                    replacements.back().resize(added.size() + replacements.back().size());
                    std::copy(added.begin(), added.end(), replacements.back().rbegin());
                    ++subidx;
                }
            }
            ++idx;
        }
    } // else // if(sub_pattern.size() == 0)

    return num_matches;
}

uint32_t TreeNode::findChildren_(const std::string& pattern,
                                 std::vector<TreeNode*>& results,
                                 bool allow_private) {
    std::vector<std::vector<std::string>> replacements;
    return findChildren_(pattern, results, replacements, allow_private);
}

uint32_t TreeNode::findImmediateChildren_(std::regex& expr,
                                          std::vector<TreeNode*>& found,
                                          std::vector<std::vector<std::string>>& replacements,
                                          bool allow_private) {
    uint32_t num_found = 0;
    for(ChildNameMapping::reference chp : names_)
    {
        std::vector<std::string> replaced; // Replacements per name
        if(identityMatchesPattern_(chp.first, expr, replaced)){
            TreeNode* child = chp.second;

            if(child){ // Ensure child is not null (e.g. grouping)
                const bool consider_child = (allow_private || canSeeChild_(child));
                if (consider_child)
                {
                    ++num_found;
                    // Can already be added/found if an alias matched
                    if(std::find(found.begin(), found.end(), child) == found.end()) {
                        found.push_back(child);
                        replacements.push_back(replaced);
                    }
                }

            }
        }
    }
    return num_found;
}

uint32_t TreeNode::findImmediateChildren_(std::regex& expr,
                                          std::vector<TreeNode*>& found,
                                          bool allow_private) {
    std::vector<std::vector<std::string>> replacements;
    return findImmediateChildren_(expr, found, replacements, allow_private);
}

// Const variant of findImmediateChildren_
uint32_t TreeNode::findImmediateChildren_(std::regex& expr,
                                          std::vector<const TreeNode*>& found,
                                          std::vector<std::vector<std::string>>& replacements,
                                          bool allow_private) const {
    uint32_t num_found = 0;
    for(ChildNameMapping::const_reference chp : names_){

        std::vector<std::string> replaced; // Replacements per name
        if(identityMatchesPattern_(chp.first, expr, replaced)){
            const TreeNode* child = chp.second;
            if (allow_private || canSeeChild_(child))
            {
                if(child){ // Ensure child is not null (e.g. grouping)
                    ++num_found;
                    if(std::find(found.begin(), found.end(), child) == found.end()) {
                        found.push_back(child);
                        replacements.push_back(replaced);
                    }
                }
            }
        }
    }
    return num_found;
}

uint32_t TreeNode::findImmediateChildren_(std::regex& expr,
                                          std::vector<const TreeNode*>& found,
                                          bool allow_private) const {
    std::vector<std::vector<std::string>> replacements;
    return findImmediateChildren_(expr, found, replacements, allow_private);
}

bool TreeNode::locationMatchesPattern(const std::string& pattern,
                                      const TreeNode* pat_loc) const {
    const TreeNode* const start = pat_loc; // TreeNode that we are working back to

    // Start with self, test a level (between two '.'s) in the search
    // string against the current node's identifiers, and then move up
    // if they match until parents or search pattern is exausted.
    // If the loop doesn't reach the start node exactly when it reaches
    // the beginning of the pattern string, there is no match.
    const TreeNode* node = this;
    size_t pos = pattern.size();

    if(pattern.size() == 0){
        return pat_loc == this; // If no pattern specified. Match if start was this
    }

    while(node != nullptr && pos != std::string::npos){
        std::string pat_tok = getPreviousName_(pattern, pos);
        if(pat_tok == ""){
            // This gets tricky here because the empty token
            // referred to a parent. Move down to the prior child in the
            // stack.
            throw SpartaException("locationMatchesPattern patterns cannot "
                                  "contain upward traversal. Problem with \"")
                << pattern << "\"";

            // If stack is exhausted
            // We could just pick any node, but if the input pat_loc
            // (start node) were below the *this node then we might
            // get to the wrong place. Similarly, if the pattern
            // went down then up to a parent, we'd have to look
            // ahead (earlier in the string).
            //
            // This is the reason why this function cannot be called
            // with any upwards traversal
        }
        std::string pat_expr = createSearchRegexPattern(pat_tok);
        std::regex expr(pat_expr);

        auto idents = node->getIdentifiers();
        bool matched = false;
        for(const std::string* ident : idents){
            std::cmatch what;
            // Test against this ident
            if(std::regex_match(ident->c_str(), what, expr)){

                // If parent is null, check that it might be the start
                // node because if the startnode is a GlobalTreeNode
                // then its child cannot reach it through getParent()
                if(node->getParent() == nullptr && start->hasImmediateChild(node) == true){
                    node = start;
                }else{
                    node = node->getParent();
                }

                // This node matches with the given token
                if(node == start && std::string::npos == pos){
                    return true; // Worked back to the start
                }

                matched = true;
                break; // Terminate loop for this child
            }
        }
        if(!matched){
            return false;
        }
    }

    return false;
}

TreeNode* TreeNode::getChild_(const std::string& name,
                              bool must_exist,
                              bool private_also)
{
    incrementGetChildCount_(name);

    size_t name_pos = 0;
    TreeNode* node = this;
    if(name.size() == 0){
        return this;
    }
    while(node != nullptr && name_pos != std::string::npos)
    {
        std::string immediate_child_name;
        immediate_child_name = getNextName(name, name_pos);

        if(immediate_child_name.size() == 0){
            // Get parent instead.
            TreeNode* tmp = node->getParent();
            if(nullptr == tmp){
                if(must_exist){
                    throw SpartaException("Node \"")
                        << node->getLocation()
                        << "\" has no parent. Error trying to evaluate \""
                        << name << "\" on Node \"" << getLocation() << "\"";
                }
                return nullptr;
            }
            node = tmp;
        }else{
            // Throws if not found
            node = node->getImmediateChildByIdentity_(immediate_child_name,
                                                      must_exist);
        }
    }

    // We cannot allow getChild to return a private child if we are not allowed
    // too.
    if (!private_also && node && !canSeeChild_(node))
    {
        if (!must_exist)
        {
            return nullptr;
        }
        else
        {
            throw SpartaException("Node \"")
                << getLocation() << " cannot access child node: \"" << node->getLocation()
                << " via getChild() because it is a private child of the parent";
        }
    }
    return node;
}

const TreeNode* TreeNode::getChild_(const std::string& name,
                                    bool must_exist,
                                    bool private_also) const {
    incrementGetChildCount_(name);

    size_t name_pos = 0;
    const TreeNode* node = this;
    if(name.size() == 0){
        return this;
    }
    while(node != nullptr && name_pos != std::string::npos){

        std::string immediate_child_name;
        immediate_child_name = getNextName(name, name_pos);

        if(immediate_child_name.size() == 0){
            // Get parent instead.
            const TreeNode* tmp = node->getParent();
            if(nullptr == tmp){
                if(must_exist){
                    throw SpartaException("Node \"")
                        << node->getLocation()
                        << "\" has no parent. Error trying to evaluate \""
                        << name << "\" on Node \"" << getLocation() << "\"";
                }
                return nullptr;
            }
            node = tmp;
        }else{
            // Throws if not found
            node = node->getImmediateChildByIdentity_(immediate_child_name,
                                                      must_exist);
        }
    }

    // We cannot allow getChild to return a private child if we are not allowed
    // too.
    if (!private_also && node && !canSeeChild_(node))
    {
        if (!must_exist)
        {
            return nullptr;
        }
        else
        {
            throw SpartaException("Node \"")
                << getLocation() << " cannot access child node: \"" << node->getLocation()
                << " via getChild() because it is a private child of the parent";
        }
    }
    return node;
}

std::string TreeNode::getDeepestMatchingPath(const std::string& path) const {
    auto result = recursGetDeepestMatchingPath_(path, 0);
    return result.second; // Path
}

std::pair<uint32_t, std::string> TreeNode::recursGetDeepestMatchingPath_(const std::string& path,
                                                                         size_t name_pos) const {
    size_t out_path_pos = name_pos;
    if(path.size() == 0 || name_pos == std::string::npos){
        return {1, *name_}; // 0-depth, empty path
    }

    uint32_t depth = 1; // Depth found, counting self
    std::string deepest;

    std::string immediate_child_name;
    immediate_child_name = getNextName(path, out_path_pos);

    if(immediate_child_name.size() == 0){
        // Get parent instead (no pattern, so no children-fanning needed)
        deepest += ".";
        deepest += getParent()->recursGetDeepestMatchingPath_(path, out_path_pos).second;
    }else{
        std::vector<const TreeNode*> children;
        std::string patexp = createSearchRegexPattern(immediate_child_name);
        std::regex expr(patexp);
        findImmediateChildren_(expr, children);
        uint32_t max_depth = 0;
        if(children.size() == 0){
            return {0, ""}; // No children found
        }
        for(auto child : children){
            auto pair = child->recursGetDeepestMatchingPath_(path, out_path_pos);
            if(pair.first >= max_depth){
                max_depth = pair.first;
                deepest = child->getName();
                if(pair.second.size() > 0){
                    deepest += ".";
                    deepest += pair.second;
                }
            }
        }
        depth += max_depth;
    }

    return {depth, deepest};
}

bool TreeNode::hasChild_(const std::string& name, const bool private_also) const noexcept {
    incrementGetChildCount_(name);

    size_t name_pos = 0;
    const TreeNode* node = this;
    if(name.size() == 0){
        return true;
    }
    while(node != nullptr && name_pos != std::string::npos){

        std::string immediate_child_name;
        immediate_child_name = getNextName(name, name_pos);

        if(immediate_child_name.size() == 0){
            // Get parent instead.
            node = node->getParent();
        }else{
            node = node->getImmediateChildByIdentity_(immediate_child_name,
                                                      false); // Must exist
        }
    }
    if(nullptr == node){
        return false;
    }

    if (!private_also && !canSeeChild_(node))
    {
        return false;
    }
    return true;
}

bool TreeNode::hasImmediateChild(const TreeNode* n) const noexcept {
    for(const TreeNode* child : children_){
        if(n == child){
            return true;
        }
    }
    return false;
}

uint32_t TreeNode::findChildrenByTag(const std::string& tag,
                                     std::vector<TreeNode*>& results,
                                     int32_t max_depth) {
    const std::string* const tag_id = StringManager::getStringManager().internString(tag);
    const std::vector<TreeNode*>& nodes = global_tags_map_[tag_id];
    uint32_t found = 0;
    for(TreeNode* node : nodes){
        const bool should_consider = (!isFinalized() || (node->privacy_level_ == privacy_level_));
        if(should_consider && node->isDescendantOf(this, max_depth)){
            ++found;
            results.push_back(node);
        }
    }
    return found;
}

bool TreeNode::isDescendantOf(const TreeNode* ancestor,
                              int32_t max_depth) const {
    const TreeNode* n = this;
    int32_t depth = max_depth;
    while(n){
        if(n == ancestor){
            return true;
        }
        if(depth == 0){
            break;
        }
        --depth;
        n = n->getParent();
    }
    return false;
}

std::string TreeNode::getLocation() const {
    std::stringstream ss;
    getLocation_(ss);
    return ss.str();
}

std::string TreeNode::getExpectedLocation() const {
    std::stringstream ss;
    getLocation_(ss, true); // anticipate parents when forming string (don't use comma node-separators)
    return ss.str();
}

std::string TreeNode::getDisplayLocation() const {
    std::stringstream ss;
    getDisplayLocation_(ss);
    return ss.str();
}

std::string TreeNode::renderSubtree(int32_t max_depth,
                                    bool show_builtins,
                                    bool names_only,
                                    bool hide_hidden,
                                    bool(*leaf_filt_fxn)(const TreeNode*)) const {
    std::stringstream ss;
    renderSubtree_(ss,
                   0,
                   max_depth,
                   show_builtins,
                   names_only,
                   hide_hidden,
                   leaf_filt_fxn);
    return ss.str();
}

uint32_t TreeNode::getLevel() const {
    uint32_t level = 0;
    for(const TreeNode* node = getParent();
        node != 0;
        ++level, node=node->getParent())
    {;}
    return level;
}


// Tree-Building

void TreeNode::addChild(TreeNode* child, bool inherit_phase) {
    addChild_(child, inherit_phase);
}

/*!
 * \brief AddChild by reference for convenience
 *
 * Overloads addChild(TreeNode* child)
 */
void TreeNode::addChild(TreeNode& child) {
    addChild_(&child, true);
}

// Miscellaneous

void TreeNode::setClock(const Clock* clk) {
    sparta_assert(clk != nullptr,
                  "Cannot assign null clock to any ResourceTreeNode "
                  "(error on " << getLocation() << ")");
    if(isFinalizing() || isFinalized()){
        throw SpartaException("Cannot set clock for node ")
            << getLocation() << " once in the TREE_FINALIZING phase";
    }
    if(clock_ != 0){
        throw SpartaException("A clock is already attached to TreeNode \"")
            << getName() << "\". Cannot change clocks once set";
    }
    clock_ = clk;
}

Scheduler * TreeNode::getScheduler(const bool must_exist)
{
    Scheduler * scheduler = nullptr;

    //If we are tied to a simulation object directly, return the
    //simulation's scheduler.
    auto sim = getSimulation();
    if (sim) {
        scheduler = sim->getScheduler();
    }

    if (scheduler) {
        return scheduler;
    }

    //If we are not tied to a simulation directly, try to get the
    //scheduler from our clock, if we have one.
    auto clk = getClock();
    if (clk) {
        scheduler = clk->getScheduler();
    }

    sparta_assert(scheduler || !must_exist);
    return scheduler;
}

const Scheduler * TreeNode::getScheduler(const bool must_exist) const
{
    Scheduler * scheduler = nullptr;

    //If we are tied to a simulation object directly, return the
    //simulation's scheduler.
    auto sim = getSimulation();
    if (sim) {
        scheduler = sim->getScheduler();
    }

    if (scheduler) {
        return scheduler;
    }

    //If we are not tied to a simulation directly, try to get the
    //scheduler from our clock, if we have one.
    auto clk = getClock();
    if (clk) {
        scheduler = clk->getScheduler();
    }

    sparta_assert(scheduler || !must_exist);
    return scheduler;
}

TreeNode::WeakPtr TreeNode::getWeakPtr() {
    sparta_assert(self_ptr_); // Check valid
    return WeakPtr(self_ptr_); // Weak pointer from self shared ptr
}

TreeNode::ConstWeakPtr TreeNode::getWeakPtr() const {
    sparta_assert(self_ptr_); // Check valid
    return ConstWeakPtr(self_ptr_); // Weak pointer from self shared ptr
}

void TreeNode::addLink(TreeNode *node, const std::string &label)
{
    sparta_assert(getPhase() == TREE_FINALIZED,
                  "Must be in finalized phase to link container tree nodes");

    if (getResource_() != nullptr) {
        getResource_()->addLink(node, label);
    }
}

void TreeNode::activateLink(const std::string &label)
{
    sparta_assert(getPhase() == TREE_FINALIZED,
                  "Must be in finalized phase to activate links");

    if (getResource_() != nullptr) {
        getResource_()->activateLink(label);
    }
}

std::string TreeNode::createSearchRegexPattern(const std::string& pat) {
    std::string patexp = pat;

    // Convert to regex with capture groups by applying a series of substring
    // replacement functions. These are functions so that they can be
    // automatically kept in sync with hasWildcardCharacters()
    for(auto& pair : TREE_NODE_PATTERN_SUBS) {
        pair.second(patexp);
    }

    return patexp;
}

bool TreeNode::hasWildcardCharacters(const std::string& name) {
    for(auto& pair : TREE_NODE_PATTERN_SUBS) {
        if(name.find(pair.first) != std::string::npos){
            return true;
        }
    }
    return false;
}

std::string TreeNode::getNextName(const std::string& name, size_t& pos) {
    size_t next_dot = name.find(LOCATION_NODE_SEPARATOR_ATTACHED, pos);
    std::string result;
    if(next_dot != std::string::npos){
        result = name.substr(pos, (next_dot - pos));
        pos = next_dot + 1;
        if(pos == name.size()){
            pos = std::string::npos; // No chars remaining in string
        }
    }else{
        result = name.substr(pos);
        pos = std::string::npos;
    }
    return result;
}

bool TreeNode::matchesGlobLike(const std::string& pattern, const std::string& other) {
    std::string patexpr = createSearchRegexPattern(pattern);
    std::regex expr(patexpr);
    std::smatch what;
    return std::regex_match(other, what, expr); // , boost::match_extra);
}

TreeNode::node_uid_type TreeNode::getNextNodeUID_() {
    if(next_node_uid_ >= MAX_NODE_UID){
        throw SpartaException("Maximum TreeNode unique identifier integers reached (")
            << MAX_NODE_UID
            << "). Try to reduce the number of nodes in the simulator or "
            "increase MAX_NODE_UID to prevent this in the future. "
            "Consider the effect on tools (such as pipe viewers) which "
            "may break if this value is increased";
    }
    return next_node_uid_++;
}

void TreeNode::trackParentlessNode_(TreeNode* node) {
    sparta_assert(node != nullptr);

    auto itr = statics_->parentless_map_.find(node);
    if(itr != statics_->parentless_map_.end()){
        const WeakPtr& wp = itr->second;
        if(wp.expired() == false){
            sparta_assert(wp.lock().get() == node); // Mismatched key and value
            throw SpartaException("Node ")
                << node->getLocation() << " is already known to be parentless, "
                << "so it cannot be re-added to the parentless list";
        }
    }

    // May overwrite an expired entry
    statics_->parentless_map_[node] = node->getWeakPtr();
}

void TreeNode::untrackParentlessNode_(TreeNode* node) {
    sparta_assert(node != nullptr);

    // Note: does not clean up expired nodes.
    statics_->parentless_map_.erase(node);
}

void TreeNode::trackNode_(TreeNode* node) {
    sparta_assert(node != nullptr);

    auto itr = statics_->node_map_.find(node);
    if(itr != statics_->node_map_.end()){
        const WeakPtr& wp = itr->second;
        if(wp.expired() == false){
            throw SpartaFatalError("TreeNode ")
                << node->getLocation() << " is already in the statics_->node_map_ list. Another "
                "node must have been constructed at the same address before the first was "
                "destructed. This is insanely unlikely but could indicate misuse of "
                "placement allocation. Otherwise, indicates corruption";
        }
        //statics_->node_map_.erase(itr);
    }

    // May overwrite an expired weak pointer entry
    statics_->node_map_[node] = node->getWeakPtr();
}

void TreeNode::untrackNode_(TreeNode* node) noexcept {
    sparta_abort(node != nullptr);

#ifdef TREENODE_LIFETIME_TRACE
    // Clean up other expired nodes. This is not stricly necessary and takes a lot of time
    std::vector<decltype(statics_->node_map_)::key_type> keys;
    keys.reserve(statics_->node_map_.size());
    for(auto& n : statics_->node_map_) {
        keys.push_back(n.first);
    }
    for(auto k : keys){
        auto& wp = statics_->node_map_.at(k);
        if(wp.expired() == true){
            //throw SpartaFatalError(
            std::cerr << "ERROR; Found an expired weak pointer to a TreeNode which was "
                "never destructed or somehow failed to fully destruct. This "
                "should not be possible unless a Destructor is throwing. "
                "Error occured while untracking (but was not caused by) "
                "node " << node->getName()
                      << std::endl;
            statics_->node_map_.erase(k);
        }
    }
#endif

    auto itr = statics_->node_map_.find(node);
    if(itr != statics_->node_map_.end()){
        if(itr->second.expired() == true){
            // Interesting, already expired. Node is deregistering itself now though
            // This could become a warning
        }
        statics_->node_map_.erase(itr);
    }else{
        std::stringstream msg("Unable to untrack TreeNode ");
        msg << node->getLocation() << " during destruction because it was not found in "
            "the statics_->node_map_ list. Somehow it was already destructed";
        sparta_abort(false, msg.str());
    }
}

void TreeNode::informAddedChildSubtree_() {
    onAddedAsChild_();

    TreeNode* p = this;
    while(p){
        if(p->isAttached()){
            is_attached_ = true; // Flag self as "Attached"
            break;
        }
        p = p->getParent();
    }
    for(TreeNode* child : children_){
        child->informAddedChildSubtree_();
    }
}

void TreeNode::informAddedChildAncestors_(TreeNode* des) {
    TreeNode* node = this;
    while(node){
        node->onDescendentSubtreeAdded_(des);
        node = node->getParent();
    }
}

void TreeNode::incrementGetChildCount_(const std::string& name) const {
    auto scheduler = getScheduler();
    if(isFinalized()
       && scheduler && scheduler->getNumFired() > 1
       && (++num_children_gets_ == CHILD_GET_THRESHOLD)){
        //! \todo Warn via logger
        std::cerr << "Warning: there have been " << num_children_gets_
                  << " child gets for this node since finalization. "
                  << "This is very likely misuse of the framework!. "
                  << "This notice was printed from within \""
                  << getLocation() << "\" with the name \"" << name
                  << "\". Ensure that children are not being searched for "
                  << "regularly while the simulator is running because this is "
                  << "slow" << std::endl;
    }
}

// Private Tree-Building Helpers

void TreeNode::setParent_(TreeNode* parent, bool inherit_phase) {
    if(getParent() != nullptr){
        throw SpartaException("A parent is already set for TreeNode \"")
            << getName() << "\". Cannot change parents";
    }
    if(nullptr == parent){
        throw SpartaException("Can never set a null parent for a TreeNode ")
            << "through setParent_. Error on node" << getLocation();
    }

    if(parent->getPhase() < getPhase()){
        throw SpartaException("A Node cannot be attached to a parent which ")
            << "has a phase less than the current phase of the node to attach. "
            << "Error when adding node \"" << getName() << "\", whose phase is "
            << getPhase() << " as a child of parent \"" << parent->getName()
            << "\" with phase " << parent->getPhase() << ". This is an issue regardless of "
            << "whether phase is being inherited";
    }
    if(inherit_phase){
        //recursSetPhase_(parent->getPhase()); // Act on subtree
        setPhase_(parent->getPhase());
    }

    // Update to new parent
    parent_ = parent;
    expected_parent_ = 0;

    sparta::TreeNode::untrackParentlessNode_(this); // This node is no longer parentless

    // Alert the subtree starting with this child that they have 1 or
    // more new ancestors
    informAddedChildSubtree_();
}

void TreeNode::addChild_(TreeNode* child, bool inherit_phase) {
    if(nullptr == child){
        SpartaException ex("Cannot add NULL child to device tree node \"");
        ex << getLocation() << "\". NULL Children are not allowed in the tree";
        throw ex;
    }

    if(isFinalized()){
        throw SpartaException("Cannot add device tree node \"")
            << child->getName() << "\" as child to device tree node \""
            << getLocation() << "\". This tree is in the TREE_FINALIZED phase";
    }

    if(this == child){
        throw SpartaException("Cannot add device tree node \"")
            << child->getName() << "\" as self-child at \""
            << getLocation() << "\". A TreeNode can never be a parent of itself.";
    }

    // Prevent cycles in the tree.
    TreeNode* parent = getParent();
    uint32_t levels = 0;
    while(parent){
        if(parent == child){
            SpartaException ex("Cannot add child \"");
            ex << child->getName() << "\" to parent \"" << *name_
               << "\" because it creates a parent-cycle over " << levels
               << " levels";
            throw ex;
            // It would be convenient to print the cycle, but this should be extrememly rare.
        }
        parent = parent->getParent();
        ++levels;
    }

    // Check for repeat objects and duplicates within the group.
    for(TreeNode* tn : children_){
        // Ensure no duplicate node instances (name collisions should be caught before this)
        if(tn == child){
            throw SpartaException("Child instance \"")
                << tn->getName() << " @" << (void*)tn
                << " is already present under TreeNode \"" << getName() << "\"";
        }

        // Ensure no group index collisions
        if(tn->getGroup() == child->getGroup()){
            if(tn->getGroupIdx() != GROUP_IDX_NONE \
               && tn->getGroupIdx() == child->getGroupIdx() \
               && child->isIndexableByGroup() \
               && tn->isIndexableByGroup()){
                throw SpartaException("Cannot add child named \"")
                    << child->getName() << "\" because a child named \""
                    << tn->getName() << "\" with the same group \""
                    << tn->getGroup() << "\" and group index " << tn->getGroupIdx()
                    << " is already present under TreeNode \"" << *name_ << "\"";
            }
        }
    }

    // Check for collisions in names, aliases, and groups BEFORE adding
    // a child. Then there will be no need for rollback if there is a
    // collision once the child and its identifiers are added.

    verifyUniqueChildIdentifier_(child->getName());
    for(const std::string& alias : child->getAliases()){
        verifyUniqueChildIdentifier_(alias);
    }
    if(child->getGroup() != ""){
        // Ignore gorup names for now
        //verifyUniqueChildIdentifier_(child->getGroup(), true);
    }
    verifyUniqueChildIdentifier_(child->getName());

    // Child has been fully validated.
    // Invoke hooks for performing additional add actions (or to reject
    // the child/parent). At this point it is safe to throw an exception
    // since nothing has been modified. No rollback would be required.
    child->onSettingParent_(this); // Call first because const
    onAddingChild_(child);

    // IMPORTANT: as per the contract with onAddingChild_, child must
    //            be actually attached and registered without any chance
    //            of failure. Any exceptions here are fatal errors

    try {

        // Add am mapping for all identifiers of this node
        std::vector<const std::string*> idents = child->getIdentifiers();
        for(const std::string* ident : idents){
            addChildNameMapping_(*ident, child);
        }

        if(child->getGroup().size() > 0){ // Group may be empty string
            addChildNameMapping_(child->getGroup() + std::to_string(child->getGroupIdx()), child);
        }

        // Connect child to parent.
        children_.push_back(child);
        child->incrementPrivacyLevel_(privacy_level_);
        child->setParent_(this, inherit_phase); // Child can inherits this node's phase

    }catch(std::exception& e){
        throw SpartaCriticalError()
            << "ERROR: Unable to register a TreeNode child \"" << child->getName()
            << "\" on " << getLocation() << " even after validation. This is a critical error and "
            "indicates an irrecoverable problem: " << e.what();
    }

    // Alert all ancestors that this child (and a possible subtree) has
    // been attached
    informAddedChildAncestors_(child);

}

void TreeNode::recursSetPhase_(TreePhase phase) {
    setPhase_(phase);
    for(TreeNode* child : children_){
        child->recursSetPhase_(phase);
    }
}


// Private Tree-Navigation and Rendering

void TreeNode::getLocation_(std::stringstream& ss, bool anticipate_parent) const {
    if(parent_ != nullptr){
        assert(parent_loc_ == nullptr); // parent_loc_ cannot be nullptr if parent_ is set.
        parent_->getLocation_(ss, anticipate_parent);
        ss << LOCATION_NODE_SEPARATOR_ATTACHED; // "."
    }else if(expected_parent_ != nullptr){
        expected_parent_->getLocation_(ss, anticipate_parent);
        if(anticipate_parent){
            // ".". Act as if the child is already a real child of the tree and
            // not just an expected child
            ss << LOCATION_NODE_SEPARATOR_ATTACHED; // Non-default behavior
        }else{
            // ",". Indicate that node is not yet a real child
            ss << LOCATION_NODE_SEPARATOR_EXPECTING;
        }
    }else if(parent_loc_ != nullptr){
        ss << *parent_loc_ << LOCATION_NODE_SEPARATOR_ATTACHED; // "."
    }else if(isAttached() == false){
        // "~". Indicate that node is completely unattached AND not a root node type
        ss << LOCATION_NODE_SEPARATOR_UNATTACHED;
    }

    ss << *name_;
}

void TreeNode::getDisplayLocation_(std::stringstream& ss) const {
    if(parent_ != nullptr){
        parent_->getLocation_(ss);
        ss << LOCATION_NODE_SEPARATOR_ATTACHED; // "."
    }else if(expected_parent_ != nullptr){
        expected_parent_->getLocation_(ss);
        // ",". Indicate that node is not yet a real child
        ss << LOCATION_NODE_SEPARATOR_EXPECTING;
    }else if(isAttached() == false){
        // "~'. Indicate that node is completely unattached AND not a root node type
        ss << LOCATION_NODE_SEPARATOR_UNATTACHED;
    }

    if(*name_ != ""){
        ss << *name_;
    }else if(group_ptr_ != StringManager::getStringManager().EMPTY){
        ss << *group_ptr_ << '[' << group_idx_ << ']';
    }else{
        throw SpartaCriticalError("Encountered a node: ")
            << this << " With no name and no group name. This should be impossible";
    }
}

uint32_t TreeNode::renderSubtree_(std::stringstream& ss,
                                  uint32_t indent,
                                  int32_t max_depth,
                                  bool show_builtins,
                                  bool names_only,
                                  bool hide_hidden,
                                  bool(*leaf_filt_fxn)(const TreeNode*)) const {
    if(isBuiltin() && !show_builtins){
        return 0; // Builtin node. Do not print or recurs into children.
    }

    if(isHidden() && hide_hidden){
        return 0; // Do not show hidden
    }

    uint32_t nodes_rendered = 0;

    // Render children first into a temporary stringstream.
    // If no children are rendered, then this node can be filtered as a leaf
    std::stringstream child_ss;
    if(max_depth != 0){
        for(const TreeNode* child : children_){
            nodes_rendered +=
                child->renderSubtree_(child_ss,
                                      indent + RENDER_SUBTREE_INDENT,
                                      max_depth-1,
                                      show_builtins,
                                      names_only,
                                      hide_hidden,
                                      leaf_filt_fxn);
        }
    }

    if(nodes_rendered == 0){
        // This is a leaf node OR it has no visible leaf children
        if(leaf_filt_fxn != nullptr
           && leaf_filt_fxn(this) == false){
            return 0; // Do not show this node
        }
    }

    // Incremental colorization
    const char* const * color = sparta::color::ColorScheme::getDefaultScheme().nextBasicColor();

    // Display indentation guides
    for(uint32_t i=0; i<indent; ++i){
        if(indent - i == RENDER_SUBTREE_INDENT){
            // Print a plus mark instead of a pipe char where children are to
            // be rendered
            ss << *color;
            color = sparta::color::ColorScheme::getDefaultScheme().nextBasicColor(color);
            ss << '+';
        }else if(indent - i == RENDER_SUBTREE_INDENT-1){
            // Draw a tick following the plus character
            ss << '-';
        }else if(i % RENDER_SUBTREE_INDENT == 0){
            ss << *color;
            color = sparta::color::ColorScheme::getDefaultScheme().nextBasicColor(color);
            ss << '|';
        }else{
            ss << ' ';
        }
    }

    // Color of item (deeper nesting than the latest indentation guide)
    //color = sparta::color::ColorScheme::getDefaultScheme()->nextBasicColor(color);
    ss << *color;

    if(*name_ != NODE_NAME_NONE){
        ss << *name_;
    }else{
        ss << '?'; // Render ? for anonymous nodes
    }

    // Restore to normal coloring for rest of line
    ss << SPARTA_CURRENT_COLOR_NORMAL;

    if(!names_only){
        ss << " : " << stringize();
    }

    if(isBuiltin()){
        ss << " {builtin}";
    }else if(group_ptr_ != StringManager::getStringManager().EMPTY){
        ss << " (" << *group_ptr_ << '[' << group_idx_ << "]) "; // Non-builtin
    }else{
        // No group
    }

    ss << " (privacy: " << privacy_level_ << ")";
    ss << std::endl;

    ++nodes_rendered;

    // Render children
    ss << child_ss.str();

    return nodes_rendered;
}

TreeNode*
TreeNode::getImmediateChildByIdentity_(const std::string& name,
                                       bool must_exist)
{
    // What's in a name?  The name could be an alias or the actual
    // TreeNode.  Look for the actual TreeNode name first, then return
    // the next alias.
    const auto it = names_.equal_range(name);

    // No match
    if(it.first == it.second)
    {
        if(false == must_exist){
            return nullptr;
        }

        std::vector<std::string> idents;
        getChildrenIdentifiers(idents, false);
        std::stringstream ss;
        for(std::string& id : idents){
            ss << "    " << id << '\n';
        }
        throw SpartaException("Could not get immediate child named \"")
            << name << "\" in node \"" << getLocation() << "\". Valid names are:\n"
            << ss.str();
    }
    else
    {
        for(auto item = it.first; item != it.second; ++item)
        {
            if(item->first == name) {
                if(nullptr != item->second) {
                    return item->second;
                }
            }
        }
    }

    if(false == must_exist){
        return nullptr;
    }

    throw SpartaException("name \"")
        << name << "\" resolved to a group (not a child) in node \""
        << getLocation() << "\"";

    return nullptr;
}

const TreeNode*
TreeNode::getImmediateChildByIdentity_(const std::string& name,
                                       bool must_exist) const
{
    // What's in a name?  The name could be an alias or the actual
    // TreeNode.  Look for the actual TreeNode name first, then return
    // the next alias.
    const auto it = names_.equal_range(name);

    // No match
    if(it.first == it.second)
    {
        if(false == must_exist){
            return nullptr;
        }

        std::vector<std::string> idents;
        getChildrenIdentifiers(idents, false);
        std::stringstream ss;
        for(std::string& id : idents){
            ss << "    " << id << '\n';
        }
        throw SpartaException("Could not get immediate child named \"")
            << name << "\" in node \"" << getLocation() << "\". Valid names are:\n"
            << ss.str();
    }
    else
    {
        for(auto item = it.first; item != it.second; ++item)
        {
            if(item->first == name) {
                if(nullptr != item->second) {
                    return item->second;
                }
            }
        }
    }

    if(false == must_exist){
        return nullptr;
    }

    throw SpartaException("name \"")
        << name << "\" resolved to a group (not a child) in node \""
        << getLocation() << "\"";

    return nullptr;
}

void TreeNode::ensureNoParent_(const char* action) {
    sparta_assert(action != 0);
    if(parent_ != 0){
        SpartaException ex("Cannot ");
        ex << action << " on TreeNode \""
           << getLocation() << "\" because it already has a parent. "
           << "TreeNode attributes can only be changed before it is added to a parent";
        throw ex;
    }
}


// Miscellaneous

bool TreeNode::identityMatchesPattern_(const std::string& ident,
                                       std::regex& expr,
                                       std::vector<std::string>& replacements) {
    std::smatch what;
    // Test against this name (could be alias, group, etc.)
    if(std::regex_match(ident, what, expr)) { // , boost::match_extra)){
        // Print out matches
        //for(unsigned i = 0; i < what.size(); ++i){
        //    std::cout << "      $" << i << " = \"" << what[i] << "\"\n";
        //}

        // Captures - Equivalent of perl regex $i.
        // Skip 0 because it is the whole expression and these replacements will
        // probably be concatenated together
        for(unsigned i = 1; i < what.size(); ++i){
            replacements.push_back(what[i].str());
        }
        return true;
    }
    return false;
}

bool TreeNode::identityMatchesPattern_(const std::string& ident,
                                       std::regex& expr) {
    std::vector<std::string> replacements;
    return identityMatchesPattern_(ident, expr, replacements);
}

std::string TreeNode::getPreviousName_(const std::string& name,
                                       size_t& pos) {
    if(name.size() == 0){
        return "";
    }
    if(pos == 0){
        return "";
    }
    if(pos == std::string::npos){
        pos = name.size();
    }
    size_t next_dot = name.rfind(LOCATION_NODE_SEPARATOR_ATTACHED, pos-1);
    std::string result;
    if(next_dot != std::string::npos){
        result = name.substr(next_dot+1, (pos - next_dot - 1));
        if(0 == next_dot){
            pos = std::string::npos; // No chars remaining in string
        }else{
            pos = next_dot;
        }
    }else{
        result = name.substr(0,pos);
        pos = std::string::npos;
    }
    return result;
}

void TreeNode::setExpectedParent_(const TreeNode* parent) {
    sparta_assert(nullptr == parent_);
    expected_parent_ = parent;
}

void TreeNode::enterFinalizing_() {
    sparta_assert(getPhase() < TREE_FINALIZING);

    setPhase_(TREE_FINALIZING);

    for(TreeNode* child : children_){
        child->enterFinalizing_();
    }
}

void TreeNode::finalizeTree_() {
    sparta_assert(getPhase() <= TREE_FINALIZING);
    if(getPhase() < TREE_FINALIZING){
        enterFinalizing_();
    }

    // Cache working clock since no clocks can be added
    working_clock_ = getClock();

    createResource_();

    // Tree node extensions parameter validation should occur here.
    // We do this here since parameter validation also occurs for
    // resources in createResource_() above.
    for (const auto& kvp : getAllExtensions()) {
        if (const auto* params = kvp.second->getParameters()) {
            std::string errs;
            if (!params->validateDependencies(this, errs)) {
                throw SpartaException("Parameter validation callbacks indicated invalid parameters: ")
                    << errs;
            }
        }
    }

    // Iterate by index just in case children may be added within
    // as a result of createResource_ on child nodes()
    // It is important that addChild always append new children to the
    // end of the childs_ list and that children cannot be removed
    for(size_t i = 0; i < children_.size(); ++i){
        children_[i]->finalizeTree_();
    }
}

void TreeNode::validateTree_() {
    sparta_assert(getPhase() == TREE_FINALIZED);

    // Check this node
    validateNode_();

    for(TreeNode* child : children_){
        child->validateTree_();
    }
}

void TreeNode::enterFinalized_() {
    sparta_assert(getPhase() < TREE_FINALIZED);

    setPhase_(TREE_FINALIZED);

    for(TreeNode* child : children_){
        child->enterFinalized_();
    }
}

void TreeNode::enterConfig_() noexcept {
    setPhase_(TREE_CONFIGURING);

    onConfiguring_();

    for(TreeNode* child : children_){
        child->enterConfig_();
    }
}

void TreeNode::bindTreeEarly_() {
    sparta_assert(getPhase() == TREE_FINALIZED);

    onBindTreeEarly_();

    Resource* const res = getResource_();
    if(res){
        res->onBindTreeEarly_();
    }

    for(TreeNode* child : children_){
        child->bindTreeEarly_();
    }
}

void TreeNode::bindTreeLate_() {
    sparta_assert(getPhase() == TREE_FINALIZED);

    onBindTreeLate_();

    Resource* const res = getResource_();
    if(res){
        res->onBindTreeLate_();
    }

    for(TreeNode* child : children_){
        child->bindTreeLate_();
    }
}

void TreeNode::simulationTerminating_()
{
    sparta_assert_context(getPhase() != TREE_TEARDOWN,
                          "Must not already be in teardown when terminating simulation. This "
                          "should occur before content");

    // Notify resource after the owning node (as noted in documentation)
    Resource* res = getResource_();
    if(res){
        try{
            res->simulationTerminating_(); // Notify resource that it should check sanity
        }catch(...){
            std::cerr << "Exception during simulationTerminating in " << getLocation()
                      << ":" << std::endl;
            throw; // Stop immediately and rethrow
        }
    }

    for(TreeNode* child : children_){
        child->simulationTerminating_();
    }
}

void TreeNode::validatePostRun_(const PostRunValidationInfo& info) const {
    sparta_assert_context(getPhase() != TREE_TEARDOWN,
                          "Must not already be in teardown when checking post-run sanity. This "
                          "should occur before content");

    // Notify resource after the owning node (as noted in documentation)
    const Resource* res = getResource_();
    if(res){
        try{
            res->validatePostRun_(info); // Notify resource that it should check sanity
        }catch(...){
            std::cerr << "Exception during post-run validation in " << getLocation()
                      << ":" << std::endl;
            throw; // Stop immediately and rethrow
        }
    }

    for(TreeNode* child : children_){
        child->validatePostRun_(info);
    }
}

void TreeNode::dumpDebugContent_(std::ostream& out) const noexcept  {
    // Notify resource after the owning node (as noted in documentation)
    const Resource* res = getResource_();
    if(res){
        bool error = false;
        // Create an ostream for each resource. This ensures each ostream is pristine and allows
        // a warning to be written BEFORE the output if there is an exception while generating the
        // output.
        std::stringstream tmp_out;
        try{
            res->dumpDebugContent_(tmp_out); // Notify resource that it should dump debug content)
        }catch(std::exception& e){
            std::cerr << "Warning: suppressed exception in dumpDebugContent_ at " << getLocation()
                      << ":\n"
                      << e.what() << std::endl;
            error = true;
        }catch(...){
            std::cerr << "Warning: suppressed unknown exception in dumpDebugContent_ at "
                      << getLocation() << std::endl;
            error = true;
        }

        if(tmp_out.str().size() > 0 || error){
            out << '\n' << getLocation() << std::endl;
            out << TreeNode::DEBUG_DUMP_SECTION_DIVIDER;
            out << tmp_out.str();
            if(error){
                out << "\n## ERROR: dumpDebugContent_ returned exception for this resource. Debug "
                    "output may be incomplete" << std::endl;
            }
            out << TreeNode::DEBUG_DUMP_SECTION_DIVIDER;
        }
    }

    for(TreeNode* child : children_){
        child->dumpDebugContent_(out);
    }
}

void TreeNode::enterTeardown_() noexcept {

    if(getPhase() != TREE_TEARDOWN){
        onEnteringTeardown_();

        // Notify resource after the owning node (as noted in documentation)
        Resource* res = getResource_();
        if(res){
            try{
                // Notify resource of last chance to access other resources
                res->onStartingTeardown_();
            }catch(std::exception& e){
                std::cerr << "Warning: suppressed exception in onStartingTeardown_ at "
                          << getLocation() << ":\n" << e.what() << std::endl;
            }catch(...){
                std::cerr << "Warning: suppressed unknown exception in onStartingTeardown_ at "
                          << getLocation() << std::endl;
            }
        }

        setPhase_(TREE_TEARDOWN);
    }

    for(TreeNode* child : children_){
        child->enterTeardown_();
    }
}

void TreeNode::verifyUniqueChildIdentifier_(const std::string& ident,
                                            bool is_group) {
    ChildNameMapping::const_iterator it = names_.find(ident);
    if(it != names_.end()){
        if(it->second != nullptr){ // Error if anything overwrites name or alias
            SpartaException ex("The ");
            if(is_group){
                ex << "group name";
            }else{
                ex << "name or alias";
            }
            ex << " \"" << ident << "\" is already taken by ";
            ex << "the name or alias of another child \"" << it->second->getName() << "\" of ";
            ex << "the same parent TreeNode \"" << getLocation() << "\" ";
            throw ex;
        }
    }
}


void TreeNode::removeChildForTeardown_(TreeNode* child) {
    sparta_assert(child != nullptr,
                  "Cannot removeChildForTeardown_ from " << getLocation()
                  << " with a null child pointer");

    onDestroyingChild_(this);

    auto erase_child = [this, child] (ChildrenVector& list, bool ignore_missing) {
                           // Remove child from children_ list
                           auto itr = std::find(list.begin(),
                                                list.end(),
                                                child);
                           if(itr == list.end() && !ignore_missing){
                               throw SpartaException("Cannot removeChildForTeardown_ with child node ")
                                   << child->getLocation() << " because it is not a child of parent: " << getLocation()
                                   << " whose children include: " << list;
                           }else if (itr != list.end()){
                               list.erase(itr);
                           }
                       };

    erase_child(children_, false);

    // Remove child from child identifier mapping
    for(const std::string* ident : child->getIdentifiers()){
        sparta_assert(ident != nullptr);
        names_.erase(*ident); // Does not fail if key not found
    }
}

void TreeNode::removeFromParentForTeardown_(TreeNode* parent) {
    sparta_assert(parent != nullptr,
                  "Cannot removeFromParentForTeardown_ with a null parent argument");
    parent->removeChildForTeardown_(this);
}


// Notifications

void
TreeNode::broadcastRegistrationForNotificationToChildren_(const std::type_info& tinfo,
                                                          const std::vector<const std::string*>& name_ids,
                                                          TreeNode* obs_node,
                                                          const delegate* del,
                                                          const bool allow_private) {
    for(auto name_id : name_ids){
        const std::string* noti_name = StringManager::getStringManager().EMPTY;
        if(canGenerateNotification_(tinfo, name_id, noti_name)){
            notificationObserverAdded_(tinfo, noti_name, obs_node, del);
            break; // Do not re-add the same delegate. We don't want it called twice
        }
    }

    const auto& children = allow_private ? getAllChildren_() : getChildren();
    for(TreeNode* child : children) {
        child->broadcastRegistrationForNotificationToChildren_(tinfo,
                                                               name_ids,
                                                               obs_node,
                                                               del,
                                                               allow_private);
    }
}

void
TreeNode::broadcastDeregistrationForNotificationToChildren_(const std::type_info& tinfo,
                                                            const std::vector<const std::string*>& name_ids,
                                                            TreeNode* obs_node,
                                                            const delegate* del,
                                                            const bool allow_private) {
    for(auto name_id : name_ids){
        const std::string* noti_name = StringManager::getStringManager().EMPTY;
        if(canGenerateNotification_(tinfo, name_id, noti_name)){
            notificationObserverRemoved_(tinfo, noti_name, obs_node, del);
            break; // Do not try and re-remove the same delegate. It will already be gone
        }
    }

    const auto& children = allow_private ? getAllChildren_() : getChildren();
    for(TreeNode* child : children) {
        child->broadcastDeregistrationForNotificationToChildren_(tinfo,
                                                                 name_ids,
                                                                 obs_node,
                                                                 del,
                                                                 allow_private);
    }
}

uint32_t TreeNode::getPossibleNotifications(std::vector<TreeNode::NotificationInfo>& infos) const {
    std::vector<NotificationInfo> added;
    getPossibleNotifications_(added);

    // Validate notifications returned by checking them against
    // canGenerateNotification. Subclass shall implement these correctly.
    // Skip this in production builds for performance
#ifndef NDEBUG
    for(NotificationInfo& ninf : added){
        if(ninf.origin != this){
            throw SpartaException("getPossibleNotifications_ call on ")
                << getLocation() << " added a notification ("
                << ninf.origin->getLocation() << ", " << ninf.tinfo->name()
                << ", \"" << *ninf.name << "\") whose origin did not match this node. "
                "getPossibleNotifications_ must respond with only nodes having this "
                "node as the origin";
        }
        if(canGenerateNotification(ninf) == false){
            throw SpartaException("getPossibleNotifications_ call on ")
                << getLocation() << " added a notification ("
                << ninf.origin->getLocation() << ", " << ninf.tinfo->name() << ", \""
                << *ninf.name << "\") which did not satisfy canGenerateNotification";
        }
    }
#endif

    // Append result of local getPossibleNotifications_
    uint32_t additions = added.size();
    if(additions > 0){
        infos.reserve(infos.size() + distance(added.begin(), added.end()));
        infos.insert(infos.end(), added.begin(), added.end());
    }
    return additions;
}

void TreeNode::dumpPossibleNotifications(std::ostream& o) const noexcept{
    std::vector<NotificationInfo> infos;
    getPossibleNotifications(infos);
    for(NotificationInfo& ninf : infos){
        o << "<" << ninf.origin->getLocation() << ", \""
          << demangle(ninf.tinfo->name()) << "\", \"" << *ninf.name << "\">" << std::endl;
    }
}

uint32_t TreeNode::getPossibleSubtreeNotifications(std::vector<TreeNode::NotificationInfo>& infos) const noexcept {
    uint32_t additions = getPossibleNotifications(infos);

    for(const TreeNode* child : children_){
        additions += child->getPossibleSubtreeNotifications(infos);
    }
    return additions;
}

void TreeNode::dumpPossibleSubtreeNotifications(std::ostream& o) const noexcept {
    std::vector<NotificationInfo> infos;
    getPossibleSubtreeNotifications(infos);
    for(NotificationInfo& ninf : infos){
        o << "<" << ninf.origin->getLocation() << ", \""
          << demangle(ninf.tinfo->name()) << "\", \"" << *ninf.name << "\">" << std::endl;
    }
}

bool TreeNode::canGenerateNotification(const std::type_info& tinfo,
                                       const std::string* name) const {
    const std::string* noti_name = StringManager::getStringManager().EMPTY;
    return canGenerateNotification_(tinfo, name, noti_name);
}

bool TreeNode::canGenerateNotification(const std::type_info& tinfo,
                                       const std::string& name) const {
    auto& strmgr = StringManager::getStringManager();
    return canGenerateNotification(tinfo, strmgr.internString(name));
}

bool TreeNode::canGenerateNotification(const TreeNode::NotificationInfo& info) const {
    if(info.origin != this){
        return false;
    }
    return canGenerateNotification(*info.tinfo, info.name);
}

bool TreeNode::canSubtreeGenerateNotification(const std::type_info& tinfo,
                                              const std::string* name) const {
    std::vector<const std::string*> names{name};
    return canSubtreeGenerateNotifications(tinfo, names);
}

bool TreeNode::canSubtreeGenerateNotification(const std::type_info& tinfo,
                                              const std::string& name) const {
    auto& strmgr = StringManager::getStringManager();
    return canSubtreeGenerateNotification(tinfo,
                                          strmgr.internString(name));
}

bool TreeNode::canSubtreeGenerateNotifications(const std::type_info& tinfo,
                                               const std::vector<const std::string*>& names) const {
    for(auto name : names){
        if(canGenerateNotification(tinfo, name)){
            return true;
        }
    }

    for(TreeNode* child : getChildren()) {
        if(child->canSubtreeGenerateNotifications(tinfo, names)){
            return true;
        }
    }

    return false;
}

std::vector<const std::string*> TreeNode::parseNotificationNameString(const std::string& csl) {
    std::vector<const std::string*> result;
    std::size_t end_pos = 0;
    do{
        // Grab next non-separator
        const std::size_t start_pos = csl.find_first_not_of(" \t,", end_pos);

        // Early-out case. String is empty or contains only whitespace or
        // encountered a tail comma followed by whitespace
        if(start_pos == std::string::npos){
            //std::cout << "parseNotificationNameString 1 \"" << csl << "\" => \"\"" << std::endl;
            result.push_back(StringManager::getStringManager().EMPTY);
            return result;
        }

        // Find ending of the current token (next separator)
        end_pos = csl.find_first_of(" \t,", start_pos);
        std::string substring;
        if(end_pos == std::string::npos){
            substring = csl.substr(start_pos);
        }else{
            substring = csl.substr(start_pos, end_pos-start_pos);
        }
        // Note that an empty substring is legitimate because it indicates "all" patterns.
        //std::cout << "parseNotificationNameString 2 \"" << csl << "\" => \"" << substring << "\"" << std::endl;
        result.push_back(StringManager::getStringManager().internString(substring));

        // Skip over trailing whitespace and reject garbage
        end_pos = csl.find_first_not_of(" \t", end_pos);
        if(end_pos != std::string::npos && csl[end_pos] != ','){
            throw SpartaException("Found non-comma character after parsing string \"")
                << substring << "\" from string \"" << csl << "\". Error at "
                "character " << end_pos << " Names must be separated "
                "by commas and spaces/tabls are allowed but must cannot be "
                "used to separate two name tokens";
        }
    }while(end_pos != std::string::npos);

    return result;
}

bool TreeNode::hasObserversRegisteredForNotification(const std::type_info& tinfo,
                                                     const std::string* name)
    const noexcept {
    auto itr = obs_local_.find(tinfo);
    if(itr == obs_local_.end()){
        return false; // No observers on DataT at all
    }

    // Check all observers for named match
    const DelegateVector& dvec = itr->second;
    const DelegateVector::const_iterator dend = dvec.end();
    DelegateVector::const_iterator d;
    for(d=dvec.begin(); d!=dend; ++d){
        if(d->observes(*this, name)){
            return true;
        }
    }
    return false; // No match
}

void TreeNode::getDelegatesRegisteredForNotification(const std::type_info& tinfo,
                                                     const std::string* name,
                                                     std::vector<TreeNode::delegate>& dels) noexcept {
    auto itr = obs_local_.find(tinfo);
    if(itr == obs_local_.end()){
        return; // No observers on DataT at all
    }

    // Check all observers for named match
    DelegateVector& dvec = itr->second;
    const DelegateVector::iterator dend = dvec.end();
    DelegateVector::iterator d;
    for(d=dvec.begin(); d!=dend; ++d){
        if(d->observes(*this, name)){
            dels.push_back(*d);
        }
    }
}

bool TreeNode::notificationCategoryMatch(const std::string* query_id,
                                         const std::string* node_id)
{
    sparta_assert(query_id != nullptr);
    sparta_assert(node_id != nullptr);

    // Check for exact match
    if(query_id == StringManager::getStringManager().EMPTY || query_id == node_id){
        return true;
    }

    // Check for pattern match
    if(matchesGlobLike(*query_id, *node_id)){
        return true;
    }

    return false;
}

TreeNode::ExtensionsBase * TreeNode::getExtension(const std::string & extension_name,
                                                  bool create_if_needed)
{
    auto it = cached_extensions_.find(extension_name);
    if (it != cached_extensions_.end()) {
        std::weak_ptr<ExtensionsBase> & weak_ext = it->second;
        if (auto ext = weak_ext.lock()) {
            return ext.get();
        } else {
            // Remove expired weak_ptr from cache
            cached_extensions_.erase(it);
        }
    }

    auto root = dynamic_cast<RootTreeNode*>(getRoot());
    if (!root) {
        return nullptr;
    }

    auto & ptree = root->getExtensionsUnboundParameterTree();
    constexpr bool must_be_leaf = false;
    auto ptree_node = ptree.tryGet(getLocation(), must_be_leaf);
    if (!ptree_node) {
        if (create_if_needed) {
            std::shared_ptr<ExtensionsBase> extension;
            auto factory = root->getExtensionFactory(extension_name);
            if (factory) {
                extension.reset(factory());
            } else {
                return nullptr;
            }

            extension->setParameters(std::make_unique<ParameterSet>(nullptr));
            extension->postCreate();

            constexpr bool required = false;
            ptree_node = ptree.create(getLocation(), required);
            ptree_node->setUserData(extension_name, std::move(extension));

            create_if_needed = false;
            return getExtension(extension_name, create_if_needed);
        }

        return nullptr;
    }

    auto extension = ptree_node->tryGetUserData<std::shared_ptr<ExtensionsBase>>(extension_name);
    if (!extension) {
        if (create_if_needed) {
            std::shared_ptr<ExtensionsBase> extension;
            auto factory = root->getExtensionFactory(extension_name);
            if (factory) {
                extension.reset(factory());
            } else {
                return nullptr;
            }

            extension->setParameters(std::make_unique<ParameterSet>(nullptr));
            extension->postCreate();
            ptree_node->setUserData(extension_name, std::move(extension));

            create_if_needed = false;
            return getExtension(extension_name, create_if_needed);
        }

        return nullptr;
    }

    cached_extensions_[extension_name] = *extension;
    return extension->get();
}

TreeNode::ExtensionsBase * TreeNode::getExtension()
{
    std::set<std::string> known_extension_names = getAllExtensionNames();

    //Don't have any extension names? No extensions.
    if (known_extension_names.empty()) {
        return nullptr;
    }

    //More than one unique extension name? Exception.
    else if (known_extension_names.size() > 1) {
        std::ostringstream oss;
        oss << "TreeNode::getExtension() overload called without any specific " << std::endl;
        oss << "named extension requested. However, more than one extension was " << std::endl;
        oss << "found. Applies to '" << getLocation() << "'" << std::endl;
        oss << "Here are the extension names found at this node:" << std::endl;
        for (const auto & ext : known_extension_names) {
            oss << "\t" << ext << std::endl;
        }
        throw SpartaException(oss.str());
    }

    //Get the one named extension
    return getExtension(*known_extension_names.begin());
}

std::set<std::string> TreeNode::getAllExtensionNames() const
{
    auto root = dynamic_cast<const RootTreeNode*>(getRoot());
    if (!root) {
        return {};
    }

    const auto & ptree = root->getExtensionsUnboundParameterTree();
    constexpr bool must_be_leaf = false;
    auto ptree_node = ptree.tryGet(getLocation(), must_be_leaf);
    if (!ptree_node) {
        return {};
    }

    const auto keys = ptree_node->getUserDataKeys();
    if (keys.empty()) {
        return {};
    }

    std::set<std::string> ext_names;
    for (const auto & key : keys) {
        const auto ext = ptree_node->tryGetUserData<std::shared_ptr<ExtensionsBase>>(key);
        if (ext) {
            ext_names.insert(key);
        }
    }

    return ext_names;
}

template <typename T>
typename std::enable_if<!is_vector<T>::value, T>::type
getParameterValueAsImpl(const std::string& param_val_str)
{
    size_t end_pos = 0;
    return utils::smartLexicalCast<T>(param_val_str, end_pos);
}

template <typename T>
typename std::enable_if<is_vector<T>::value, T>::type
getParameterValueAsImpl(const std::string& param_val_str)
{
    // Only difference for vectors is that they are enclosed in []
    if (param_val_str.size() < 2 ||
        param_val_str.front() != '[' ||
        param_val_str.back() != ']') {
        throw SpartaException("TreeNode extension parameter retrieval failed: Parameter value '")
            << param_val_str << "' is not a valid vector representation.";
    }

    auto loc_s = param_val_str.substr(1, param_val_str.size() - 2);

    // Split comma-delimited values
    std::vector<std::string> tokens;
    boost::split(tokens, loc_s, boost::is_any_of(","));

    T result;
    for (const auto& token : tokens) {
        size_t end_pos = 0;
        result.push_back(utils::smartLexicalCast<typename T::value_type>(token, end_pos));
    }

    return result;
}

template <typename T>
T TreeNode::ExtensionsBase::getParameterValueAs(const std::string& param_name)
{
    auto ps = getParameters();
    if (!ps) {
        throw SpartaException("TreeNode extension parameter retrieval failed: No ParameterSet ")
            << "is associated with this extension.";
    }

    auto p = ps->getParameter(param_name);
    if (!p) {
        throw SpartaException("TreeNode extension parameter retrieval failed: Parameter '")
            << param_name << "' does not exist.";
    }

    std::string param_val_str = p->getValueAsString();
    return getParameterValueAsImpl<T>(param_val_str);
}

// Supported types for tree node extensions (scalar):
template int8_t TreeNode::ExtensionsBase::getParameterValueAs<int8_t>(const std::string&);
template uint8_t TreeNode::ExtensionsBase::getParameterValueAs<uint8_t>(const std::string&);
template int16_t TreeNode::ExtensionsBase::getParameterValueAs<int16_t>(const std::string&);
template uint16_t TreeNode::ExtensionsBase::getParameterValueAs<uint16_t>(const std::string&);
template int32_t TreeNode::ExtensionsBase::getParameterValueAs<int32_t>(const std::string&);
template uint32_t TreeNode::ExtensionsBase::getParameterValueAs<uint32_t>(const std::string&);
template int64_t TreeNode::ExtensionsBase::getParameterValueAs<int64_t>(const std::string&);
template uint64_t TreeNode::ExtensionsBase::getParameterValueAs<uint64_t>(const std::string&);
template double TreeNode::ExtensionsBase::getParameterValueAs<double>(const std::string&);
template std::string TreeNode::ExtensionsBase::getParameterValueAs<std::string>(const std::string&);

// As well as vector extension parameters:
template std::vector<int8_t> TreeNode::ExtensionsBase::getParameterValueAs<std::vector<int8_t>>(const std::string&);
template std::vector<uint8_t> TreeNode::ExtensionsBase::getParameterValueAs<std::vector<uint8_t>>(const std::string&);
template std::vector<int16_t> TreeNode::ExtensionsBase::getParameterValueAs<std::vector<int16_t>>(const std::string&);
template std::vector<uint16_t> TreeNode::ExtensionsBase::getParameterValueAs<std::vector<uint16_t>>(const std::string&);
template std::vector<int32_t> TreeNode::ExtensionsBase::getParameterValueAs<std::vector<int32_t>>(const std::string&);
template std::vector<uint32_t> TreeNode::ExtensionsBase::getParameterValueAs<std::vector<uint32_t>>(const std::string&);
template std::vector<int64_t> TreeNode::ExtensionsBase::getParameterValueAs<std::vector<int64_t>>(const std::string&);
template std::vector<uint64_t> TreeNode::ExtensionsBase::getParameterValueAs<std::vector<uint64_t>>(const std::string&);
template std::vector<double> TreeNode::ExtensionsBase::getParameterValueAs<std::vector<double>>(const std::string&);
template std::vector<std::string> TreeNode::ExtensionsBase::getParameterValueAs<std::vector<std::string>>(const std::string&);

// Miscellaneous

void TreeNode::addChildNameMapping_(const std::string& name,
                                    TreeNode* child) {
    sparta_assert(name.size() != 0,
                  "Name of child identifier cannot be empty string. Parent is "
                  << getLocation());

    names_.emplace(name, child);
}

} // namespace sparta
