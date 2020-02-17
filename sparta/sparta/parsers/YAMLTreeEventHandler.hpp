// <YAMLTreeListener.hpp> -*- C++ -*-


/*!
 * \file YAMLTreeListener.hpp
 * \brief Tree event handler classs for yaml-cpp that maintains a stack of a tree
 */

#ifndef __YAML_TREE_EVENT_HANDLER_H__
#define __YAML_TREE_EVENT_HANDLER_H__

#include <yaml-cpp/anchor.h>
#include <yaml-cpp/emitterstyle.h>
#include <yaml-cpp/eventhandler.h>
#include <yaml-cpp/mark.h>
#include <yaml-cpp/node/type.h>
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stack>
#include <string>
#include <vector>

#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/Printing.hpp"


namespace YP = YAML; // Prevent collision with YAML class in ConfigParser namespace.


namespace sparta
{
/*!
 * \brief YAML parser event handler. Implements yaml-cpp's
 * YAMLTreeEventHandler interface to receive node events from the yaml
 * parser as it parses the input file.
 *
 * Internally, a stack is maintained based on traversal of a sparta
 * TreeNode-based device tree which is directed by the map keys, which
 * (mostly) contain node names. Each of these name keys are interpreted
 * of two ways. 1) As dot-separated path patterns pointing to 1 or more
 * nodes in the device tree relative to the current node(s). 2) As a
 * reserved "special" key as defined by a subclass of this class. This
 * determination is made by handleEnterMap_
 * When a YAML key is encountered that has a scalar or sequence of
 * scalars as its value, it is treated as a leaf and is given to a
 * handleLeafSequence_ or handleLeafScalar_ callbacks
 */
class YAMLTreeEventHandler : public YP::EventHandler
{
public:

    /*!
     * \brief Type for user (client) node IDs specified by subclass
     */
    typedef uint64_t node_uid_t;

    /*!
     * \brief Maximum valid value for a node_uid_t
     */
    static const node_uid_t MAX_NAV_NODE_UID = ~(uint64_t)0;

    /*!
     * \brief Node in a NavVector containin a node, a vector of nodes, and a
     * unique node ID that a subclass can use to identify this context in more
     * detail.
     */
    struct NavNode {
        const NavNode* parent; //!< Parent nav node (from which this node came)

        TreeNode* first; //!< Node reppresenting context

        std::vector<std::string> second; //!< Replacements

        /*!
         * \brief User ID. Node identifying this point in the tree as specified by a subclass through getNextNodeID_
         */
        uint64_t uid;

        //! \brief Dumps the content of this node to an ostream
        void dump(std::ostream& o) const {
            o << "<NavNode parent=" << (const void*)parent << " n=" << first << " subs=" << second << " uid=" << uid << ">";
        }
    };

    //! Vector representing the possible nodes during traversal
    typedef std::vector<TreeNode*> NodeVector;

    /*!
     * \brief Vector representing possible nodes during traversal and the
     * replacements made to any wildcards to reach that point
     */
    typedef std::vector<std::shared_ptr<NavNode>> NavVector;

    /*!
     * \brief Type for node filter callback.
     * \return True if node is acceptable and false if it should be excluded
     * from results
     */
    typedef bool (*NodeFilterFunc)(const TreeNode*);

    /*!
     * \brief Highest allowed width of subtree_ (or any level of
     * tree_stack_) in order to prevent excessive memory use or
     * unaccceptable performance.
     *
     * It is assumed that any pattern-matched traveral which match
     * more than this many nodes at a particular level is misusing
     * the framework, or has uncovered a bug.
     */
    static const size_t MAX_MATCHES_PER_LEVEL = 2000;

    /*!
     * \brief Constructor
     * \param filename Name of file being read from by the parser. This classe
     * uses the filename only for information and messages
     * \param device_trees vector of TreeNode* to use as the roots
     * for parsing the input file. All top-level items in the input
     * file will be resolved as descendents of this <device_tree>
     * node.
     * \param verbose Show verbose output
     * \param node_filter Filter function. Returns True if node should be
     * accepted. All nodes are accepted if nullptr.
     * \throws exception if filename cannot be opened for read
     */
    YAMLTreeEventHandler(const std::string& filename,
                         NavVector device_trees,
                         bool verbose,
                         NodeFilterFunc node_filter=nullptr) :
        filename_(filename),
        trees_(device_trees),
        nesting_(0),
        cur_(YP::NodeType::Null),
        in_sequence_(false),
        verbose_(verbose),
        node_filter_(node_filter)
    {
        sparta_assert(device_trees.size() != 0);
    }

    virtual ~YAMLTreeEventHandler() {;}

    /*!
     * \brief Filter a node based on the given filter function.
     * Always passes if node_filter was not specified.
     * \return true if the node is accepted by the filter functor and false if
     * it is rejected.
     */
    bool acceptNode(const TreeNode* n) {
        return !node_filter_ || ((*node_filter_)(n) == true);
    }

    /*!
     * \brief Gets any errors generated. This is used to check for
     * successful yaml file consumption. This is never cleared after
     * construction
     * \return Reference to the internal vector of errors encountered
     * during events.
     */
    const std::vector<std::string>& getErrors() const {
        return errors_;
    }

    /*!
     * \brief Gets any warnings generated. This is used to check for
     *        successful and clean yaml file consumption. This is
     *        never cleared after construction
     *
     * \return Reference to the internal vector of warnings
     *         encountered during parsing/evaluation
     */
    const std::vector<std::string>& getWarnings() const {
        return warnings_;
    }

    /*!
     * \brief Returns the filename of the file being parsed by the parser
     * with which this handler is associated. The filename is not used except
     * for information
     */
    const std::string& getFilename() const {
        return filename_;
    }

    /*!
     * \brief Is this handler in verbose mode
     */
    bool isVerbose() const { return verbose_; }

    //! Dummy method that returns self in order to make log statements within code more readable.
    YAMLTreeEventHandler& verbose()
    {
        return *this;
    }

    //! Logging operator to cout.
    //! \tparam T type to insert in output stream
    template <class T>
    YAMLTreeEventHandler& operator<<(const T& r) {
        if(verbose_){
            std::cout << r;
        }
        return *this;
    }

    //! Help with some ostream modifiers. Template could not deduce parameters correctly.
    YAMLTreeEventHandler& operator<<(std::ostream&(*r)(std::ostream&)) {
        if(verbose_){
            std::cout << r;
        }
        return *this;
    }

    //! Help with some ios modifiers. Template could not deduce parameters correctly.
    YAMLTreeEventHandler& operator<<(std::ios&(*r)(std::ios&)) {
        if(verbose_){
            std::cout << r;
        }
        return *this;
    }

    //! Help with some ios modifiers. Template could not deduce parameters correctly.
    YAMLTreeEventHandler& operator<<(std::ios_base&(*r)(std::ios_base&)) {
        if(verbose_){
            std::cout << r;
        }
        return *this;
    }

    //! Handle document start YAML node from parser
    void OnDocumentStart(const YP::Mark& mark) override final
    {
        (void) mark;
        subtree_.insert(subtree_.end(), trees_.begin(), trees_.end());
        cur_ = YP::NodeType::Null;
        while(!tree_stack_.empty()){tree_stack_.pop();}
        in_sequence_ = false;
        seq_vec_.clear();
        last_val_ = "";

        verbose() << indent_() << "(" << subtree_.size() << ") + DocumentStart @"
                  << mark.line << std::endl;
        nesting_++;
    }

    //! Handle document end YAML node from parser
    void OnDocumentEnd() override final
    {
        if(subtree_.size() > 0){
            verbose() << indent_() << "(" << subtree_.size() << ") + DocumentEnd" << std::endl;
        }else{
            verbose() << indent_() << "(commented)" << " + DocumentEnd" << std::endl;
        }
        nesting_--;

        // Everything is re-cleared within OnDocumentStart
        // If tree_stack_ is not empty, something was incorrect
        sparta_assert(tree_stack_.empty());
    }

    //! Handle NULL YAML node from parser
    void OnNull(const YP::Mark& mark, YP::anchor_t anchor) override final
    {
        (void) anchor;

        if(subtree_.size() > 0){
            verbose() << indent_() << "(" << subtree_.size() << ") + NULL @" << mark.line
                      << std::endl;
        }else{
            verbose() << indent_() << "(commented)" << " + NULL @" << mark.line << std::endl;
        }

        // TODO: Determine if this is proper handling of NULLs

        last_val_ = "";
    }

    //! Handle Aliase YAML node from parser
    void OnAlias(const YP::Mark& mark, YP::anchor_t anchor) override final
    {
        (void) anchor;

        if(subtree_.size() > 0){
            verbose() << indent_() << "(" << subtree_.size() << ") + Alias @" << mark.line
                      << std::endl;
        }else{
            verbose() << indent_() << "(commented)" << " + Alias @" << mark.line << std::endl;
        }
        throw SpartaException("YAML Aliases are not yet supported in SPARTA");
    }

    //! Handle Scalar (key or value) YAML node from parser
    void OnScalar(const YP::Mark& mark, const std::string& tag,
                  YP::anchor_t anchor, const std::string& value) override final;

    //! Handle SequenceStart YAML node from parser
    void OnSequenceStart(const YP::Mark& mark, const std::string& tag,
                         YP::anchor_t anchor, YP::EmitterStyle::value style) override final;

    //! Handle SequenceEnd YAML node from parser
    void OnSequenceEnd() override final;

    //! Handle MapStart YAML node from parser
    void OnMapStart(const YP::Mark& mark, const std::string& tag,
                    YP::anchor_t anchor, YP::EmitterStyle::value style) override final;

    //! Handle MapEnd YAML node from parser
    void OnMapEnd() override final;

protected:

    /*!
     * \brief Adds an error message the the list of errors to be printed
     * when parsing/event handling fails
     */
    void addError_(const std::string& msg) {
        errors_.push_back(msg);
    }

    /*!
     * \brief Adds an warning message the the list of errors to be printed
     *        when parsing/event handling fails
     */
    void addWarning_(const std::string& msg) {
        warnings_.push_back(msg);
    }

    /*!
     * \brief Found a scalar node as a leaf (i.e. item in a sequence or
     * value in a map) in a specific context node
     * \note This is invoked once per scope node in the current context where
     * the current node \a n passes the node filter (node_filter_)
     * \param n TreeNode context at which this leaf was encountered
     * \param value Value of the leaf scalar
     * \param assoc_key Key associated with this leaf scalar (it is expected
     * to be a pair in a map). This key identifies the \a n parameter
     * \param captures Ordered replacements of any wildcards used to get to the
     * node \a n in this tree yaml file.
     * \param uid user ID associated with the context in which this scalar was
     * encountered. This refines context in a way specific to the subclass.
     * \note This is called multiple times if the tree walking context
     * is more than one node (e.g. navigation with wildcards or starting
     * with multiple roots)
     */
    virtual void handleLeafScalar_(TreeNode* n,
                                   const std::string& value,
                                   const std::string& assoc_key,
                                   const std::vector<std::string>& captures,
                                   node_uid_t uid) {
        (void) n;
        (void) value;
        (void) assoc_key;
        (void) captures;
        (void) uid;
    }

    /*!
     * \brief Identical to handleLeafScalar_ but is called exactly once for all
     * contexts before invoking each handleLeafScalar_.
     */
    virtual void handleLeafScalarContexts_(const std::string& value,
                                           const std::string& assoc_key,
                                           const NavVector& context) {
        (void) value;
        (void) assoc_key;
        (void) context;
    }

    /*!
     * \brief Handle the unknown key if possible
     * \note This is invoked once per scope node in the current context
     * \param n TreeNode context at which this key was encountered
     * \param value String value associated with this key
     * \param assoc_key unkonwn key associated found within the context of \a n
     * \param scope Scope of this node being encountered
     * captures (from pattern-matching substitution). Also contains a UID for
     * each context node which can be updated based on the subclass' needs
     * \return true if the key could be handled, false if not.
     */
    virtual bool handleLeafScalarUnknownKey_(TreeNode* n,
                                             const std::string& value,
                                             const std::string& assoc_key,
                                             const NavNode& scope) {
        (void) n;
        (void) value;
        (void) assoc_key;
        (void) scope;

        return false;
    }

    /*!
     * \brief Found a sequence node as a (expected) leaf (i.e. item in a
     * sequence or value in a map). While this sequence could contain maps
     * or other sequences all usages of this class do not support this
     * structure
     * \note This is invoked once per scope node in the current context
     * \param n TreeNode context at which this "leaf" was encountered
     * \param value Vector of strings representing the elements of the
     * sequence.
     * \param assoc_key Key associated with this leaf sequence (it is
     * expected to be a pair in a map)
     * \param scope Scope of this node being encountered
     * \note This is called multiple times if the tree walking context
     * is more than one node (e.g. navigation with wildcards or starting
     * with multiple roots)
     */
    virtual void handleLeafSequence_(TreeNode* n,
                                     const std::vector<std::string>& value,
                                     const std::string& assoc_key,
                                     const NavNode& scope) {
        (void) n;
        (void) value;
        (void) assoc_key;
        (void) scope;
    }

    /*!
     * \brief Consumes another YAML file based on an include directives
     * destination.
     * \param pfilename YAML file to read
     * \param device_trees Vector of TreeNodes to act as roots of
     * the filename being read. This allows includes to be scoped to specific
     * nodes. The file will be parsed once an applied to all roots in
     * device_trees.
     */
    virtual void handleIncludeDirective_(const std::string& filename,
                                         NavVector& device_trees) {
        (void) filename;
        (void) device_trees;
    }

    /*!
     * \brief Is this key a reserved word
     * \param key Key to test
     * \return true if key is a reserved word (cannot be a location)
     * and false if unreserved (may be a location pattern)
     */
    virtual bool isReservedKey_(const std::string& key) const {
        (void) key;
        return false;
    }

    /*!
     * \brief Is this key an indicator to this parser to ignore the block
     * \param key Key to test
     * \return true if key is an ignore key workd
     * and false if unreserved (may be a location pattern)
     */
    virtual bool isIgnoreKey_(const std::string& key) const {
        (void) key;
        return false;
    }

    /*!
     * \brief Should the parser continue traversing the sequence?
     * \return true (default) if the sequence is to be consumed
     * and false if its to be skipped
     */
    virtual bool traverseSequence_() const {
        return true;
    }

    /*!
     * \brief Handles state change of entering a map and can intercept cases
     * where the map is a value associated with a specially named key in a
     * parent map.
     * This is always called when entering a map. Return value determines
     * how the tree even handler will react
     * \param key Key string for which this map is an associated value
     * \param context Context of map entry: current nodes and associated
     * captures (from pattern-matching substitution). Also contains a UID for
     * each context node which can be updated based on the subclass' needs
     * \return true if the event handler should handle this as a typical
     * tree-traversal map entrance, and false if the event handler should
     * not attempt to change its subtree_ state based on this node
     */
    virtual bool handleEnterMap_(const std::string& key,
                                 NavVector& context) {
        (void) key;
        (void) context;
        return true; // Default behavior
    }

    /*!
     * \brief Handles state change of exiting a map and can intercept cases
     * where the map is a value associated with a specially named key in a
     * parent map.
     * \param key Key string for which this map is an associated value
     * \return true if the event handler should handle this as a typical
     * tree-traversal map exit, and false if the event handler should not
     * attempt to change its subtree_ state based on this node
     * \param context Context of map exit: current nodes and associated captures
     * (from pattern-matching substitution).
     * \note Returns values for this call must match the results of
     * handleEnterMap_ when acting on the same node
     */
    virtual bool handleExitMap_(const std::string& key,
                                const NavVector& context) {
        (void) key;
        (void) context;
        return true; // Default behavior
    }

    /*!
     * \brief Get the next node ID
     * \param parent The NavNode context that is a parent of this node. Often,
     * the context is inherited from this node and changed when the subclass
     * wants to distinguish scope more accurately than this class does. This will
     * be nullptr if the nav node has no parent context
     * \param node Node causing the new context.
     * \param substitution Substitutions for this new context
     */
    virtual node_uid_t getNextNodeID_(const NavNode* parent,
                                      const TreeNode* node,
                                      const std::vector<std::string> & substitutions) {
        (void) parent;
        (void) node;
        (void) substitutions;

        if(parent){
            return parent->uid;
        }
        return 0;
    }

    /*!
     * \brief Determine the next generation of nodes and substitutions based on
     * the current context.
     * \param current Input context
     * \param pattern Pattern used to find children (through
     * sparta::TreeNode::findChildren)
     * \param next Resulting context
     * \param mark Position in current YAML string
     */
    void findNextGeneration_(NavVector& current, const std::string& pattern,
                             NavVector& next, const YP::Mark& mark) {

        sparta_assert(next.size() == 0);
        sparta_assert(current.size() > 0);

        for(auto& nvp : current){
            // Get all children and all replacements
            NodeVector children;
            std::vector<std::vector<std::string>> replacements;
            nvp->first->findChildren(pattern, children, replacements);

            // Copy found children and replacement vectors to the next
            // generation NavVector
            size_t idx = 0;
            for(TreeNode* child : children){
                std::vector<std::string> all_replacements(nvp->second);
                std::vector<std::string>& added = replacements.at(idx);
                all_replacements.resize(all_replacements.size() + added.size());
                std::copy(added.begin(), added.end(), all_replacements.rbegin());
                auto next_id = getNextNodeID_(nvp.get(), child, all_replacements);
                next.emplace_back(new NavNode({nvp.get(), child, all_replacements, next_id}));
                verbose() << indent_() << "Got new ID (" << next.back()->uid
                          << ") parent id=" << nvp->uid << " for child in next gen: " << child
                          << " replacements " << all_replacements << std::endl;
                ++idx;
            }
        }
        if(next.size() == 0){
            SpartaException ex("Could not find any nodes matching the pattern \"");
            ex << pattern << "\" from nodes [";
            for(auto &x : current){
                ex << x->first->getLocation() << ",";
            }
            ex << "]. ";
            addMarkInfo_(ex, mark);
            throw ex;
        }
        if(next.size() > MAX_MATCHES_PER_LEVEL){
            SpartaException ex("Found more than ");
            ex << (size_t)MAX_MATCHES_PER_LEVEL << " nodes matching the pattern \""
               << pattern << "\" from " << subtree_.size() << " nodes. "
               << "This is likely a very deep and dangerous search pattern (or possibly a bug). "
               << "If there really should be this many matches, increase MAX_MATCHES_PER_LEVEL. ";
            addMarkInfo_(ex, mark);
            throw ex;
        }
    }

    /*!
     * \brief Inherit the next generation assigning each node a new ID.
     * \note This is a simple case of findNextGeneration_ where the pattern is
     * effectively "" (self). These have not been proven to be identical
     * functionalities though
     * \param current Input (current) context
     * \param next Next (output) context
     */
    void inheritNextGeneration_(const NavVector& current,
                                NavVector& next) {
        sparta_assert(next.size() == 0);
        sparta_assert(current.size() > 0);

        for(auto& nvp : current){
            auto next_id = getNextNodeID_(nvp.get(), nvp->first, nvp->second);
            next.emplace_back(new NavNode({nvp.get(), nvp->first, nvp->second, next_id}));
            verbose() << indent_() << "Direct subtrree inheritance: Got new ID ("
                      << next.back()->uid << ") parent = " << nvp << std::endl;
        }
    }

    /*!
     * \brief Return a stirng containing spaces as a multiple of the
     * nesting_ level.
     */
    std::string indent_()
    {
        std::stringstream ss;
        for(uint32_t i=0;i<nesting_;++i){
            ss << "  ";
        }
        return ss.str();
    }

    //! Adds the mark info from current node to a SpartaException.
    //! Includes filename, line, and column
    void addMarkInfo_(SpartaException& ex, const YP::Mark& mark){
        ex << "In file " << filename_ << ":" << mark.line
           << " col:" << mark.column;
    }

    //! Adds the mark info from current node to a SpartaException.
    //! Includes filename, line, and column
    std::string markToString_(const YP::Mark& mark){
        std::stringstream ss;
        ss << "In file " << filename_ << ":" << mark.line
           << " col:" << mark.column;
        return ss.str();
    }

private:

    // Config-file context information

    /*!
     * \brief Name of input file
     */
    const std::string filename_;

    /*!
     * \brief Device tree associated with the document head
     */
    NavVector const trees_;


    // Tree-State

    /*!
     * \brief Current possible nodes in the tree (children from 1 or more
     * generations of pattern matching)
     */
    NavVector subtree_;

    /*!
     * \brief For debugging - tree depth from top
     */
    uint32_t nesting_;

    /*!
     * \brief Current node type associated with subtree_
     */
    YP::NodeType::value cur_;

    /*!
     * \brief Stack of NavVector (copies of subtree_ at various depths)
     * representing possible paths through tree while traversing with patterns
     * \note All NavVectors must be properly freed at destruction
     */
    std::stack<NavVector> tree_stack_;

    /*!
     * \brief Nodes which will receive the values of the current sequence
     */
    std::vector<TreeNode*> seq_nodes_;

    /*!
     * \brief Currently in a sequence to assign to elements in seq_nodes_ *
     * (even if seq_nodes_ is empty)
     */
    bool in_sequence_;

    /*!
     * \brief Vector of values current sequence
     */
    std::vector<std::string> seq_vec_;

    /*!
     * \brief Prior scalar value
     */
    std::string last_val_;

    /*!
     * \brief Verbose parse mode
     */
    const bool verbose_;

    /*!
     * \brief Filter function to reject nodes
     */
    NodeFilterFunc node_filter_;

    /*!
     * \brief Errors encountered while running
     */
    std::vector<std::string> errors_;

    /*!
     * \brief Warnings encountered while running
     */
    std::vector<std::string> warnings_;

    /*!
     * \brief Stack of keys whose associated values are maps. These are the
     * effective "names" of all map nodes which contain the current parsing
     * position
     */
    std::stack<std::string> map_entry_key_stack_;

}; // class YAMLTreeEventHandler

    /*!
     * \brief ostream insertion operator for YAMLTreeEventHandler::NavNode
     */
    inline std::ostream& operator<<(std::ostream& o, const YAMLTreeEventHandler::NavNode& nn) {
        nn.dump(o);
        return o;
    };

}; // namespace sparta

// __YAML_TREE_EVENT_HANDLER_H__
#endif
