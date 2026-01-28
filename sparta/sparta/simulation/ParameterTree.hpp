// <ParameterTree.hpp> -*- C++ -*-

#pragma once

#include <vector>
#include <sstream>
#include <regex>
#include <memory>
#include <string>
#include <map>
#include <any>

#include <boost/algorithm/string.hpp>

#include "sparta/utils/Utils.hpp"
#include "sparta/simulation/Parameter.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/LexicalCast.hpp"
#include "sparta/utils/MetaStructs.hpp"

namespace sparta
{
    /*!
     * \brief Virtual Parameter Tree. This represents a tree of parameters read
     * from some source but does not necessarily correspond to the parameters
     * in the simulation itself or even to the simulation tree. This is meant to
     * provide a hierarchical view into simulation parameters before they are
     * actually applied to a real TreeNode tree.
     *
     * Eventually, this structure will automatically share data with
     * sparta::ParameterSet instances as they are created. For now, it is limited.
     *
     * Typical use is to subclass this object for each simulator component/model
     * which has a set of parameters it must consume. sparta::ParameterSet
     * provides a generic interface through which config-file parsers or
     * command-line parsers can populate the parameters from strings. Models
     * that expose the ParameterSet full of sparta::Parameter instances should
     * dynamic_cast a sparta::ParameterSet supplied to them at runtime back to
     * a known type and operate through Parameters directly instead of through
     * this generic interface.
     */
    class ParameterTree
    {
    public:

        /*!
         * \brief Node containing a Parameter and value to apply. Can
         * be used to describes a value extracted from the tree when
         * using get or tryGet. This is a copy of the value and is not
         * in any way synchronized with the tree.
         *
         * It is illegal to try and read the value if there is no
         * value set (see hasValue) - An exception will be thrown.
         *
         * Can be written as well
         *
         * Contains a string that can be lexically cast using the
         * getAs<T> method. Supports all types supported by
         * sparta::lexicalCast
         */
        class Node
        {
        public:

            /*!
             * \brief Vector of children owned by this node
             */
            typedef std::vector<std::unique_ptr<Node>> ChildVector;

        private:

            Node* parent_ = nullptr; //!< Parent node
            ParameterTree * tree_ = nullptr;  //!< Tree owning this node. Applies to root only
            std::string name_;     //!< Name of this node relative to it's parent
            std::string value_;    //!< Value of this node (if set. See has_value_)
            std::string origin_;   //!< Origin of this node (e.g. which yaml file and line). Valid only if value is set.
            bool has_value_;       //!< Does this node have a value yet
            ChildVector children_; //!< Vector of children for this nodie
            uint32_t write_count_; //!< Number of times this node's value has been written.
            mutable uint32_t read_count_ = 0; //!< Number of times this node's value has been read
            std::map<std::string, std::any> user_data_; //!< Name-value pairs of any user data type

            //!< Base class for std::any printers
            class UserDataPrinterBase
            {
            public:
                virtual ~UserDataPrinterBase() = default;
                virtual void print(const std::string& name, std::any user_data, std::ostream& o, uint32_t indent) const = 0;
            };

            //!< Specific std::any printers
            template <typename T>
            class UserDataPrinter : public UserDataPrinterBase
            {
            public:
                void print(const std::string& name, std::any user_data, std::ostream& o, uint32_t indent) const override
                {
                    const T& ud = std::any_cast<const T&>(user_data);
                    for (uint32_t i = 0; i < indent; ++i) {
                        o << " ";
                    }
                    o << name << ": ";
                    print_(o, ud, indent);
                    o << "\n";
                }

            private:
                template <typename Value>
                typename std::enable_if<MetaStruct::is_any_pointer<Value>::value, void>::type
                print_(std::ostream& o, const Value& val, uint32_t indent) const {
                    if (!val) {
                        o << "nullptr";
                    } else {
                        o << val << " -> ";
                        print_(o, *val, indent);
                    }
                }

                template <typename Value>
                typename std::enable_if<!MetaStruct::is_any_pointer<Value>::value, void>::type
                print_(std::ostream& o, const Value& val, uint32_t indent) const {
                    if constexpr (std::is_same_v<Value, TreeNode::ExtensionsBase>) {
                        if (auto ps = val.getParameters()) {
                            o << val.getClassName() << " extension with parameters:\n";
                            const std::string s = ps->dumpList();
                            std::vector<std::string> lines;
                            boost::split(lines, s, boost::is_any_of("\n"));
                            for (auto & line : lines) {
                                for (uint32_t i = 0; i < indent+2; ++i) {
                                    o << " ";
                                }
                                o << line << "\n";
                            }
                        } else {
                            o << "extension without parameters";
                        }
                    } else {
                        o << val;
                    }
                }
            };

            //!< Map of user data names to their std::any printers
            std::unordered_map<std::string, std::unique_ptr<UserDataPrinterBase>> user_data_printers_;

            /*!
             * \brief How many "set"-ers require this virtual node to be a real
             * node in the tree? This is a deferred value that may be queried later by clients
             */
            uint32_t required_ = 0;

        public:

            /*!
             * \brief Not default-constructable
             */
            Node() = delete;

            Node(Node* parent, const std::string& name,
                 const std::string& value, const std::string& origin) :
                parent_(parent),
                name_(name),
                value_(value),
                origin_(origin),
                has_value_(true),
                write_count_(1)
            {;}

            /*!
             * \brief Value-less constructor
             */
            Node(Node* parent, const std::string& name) :
                parent_(parent),
                name_(name),
                value_(),
                has_value_(false),
                write_count_(0)
            {;}

            /*!
             * \brief Root node constructor. Constructs node pointing to a new
             * tree having no name. Normal nodes to not have tree pointers
             * (getOwner)
             */
            Node(Node* parent, ParameterTree* tree) :
                parent_(parent),
                tree_(tree),
                name_(""),
                value_(),
                has_value_(false),
                write_count_(0)
            {;}

            /*!
             * \brief Deep-Copy constructor
             * \note Assigns new parent
             */
            Node(Node* parent, const Node& n) :
                parent_(parent),
                name_(n.name_),
                value_(n.value_),
                origin_(n.origin_),
                has_value_(n.has_value_),
                write_count_(n.write_count_),
                read_count_(n.read_count_)
            {
                for(auto& x : n.children_){
                    children_.emplace_back(new Node(this, *x.get()));
                }
            }

            /*!
             * \brief Parent-preserving deep-copy assignment operator
             * \post New node will have its parent preserved
             * \post write and read counts will be initialized to 0. If this node has a value after
             * the assignment, write_count_ will be incremented to 1
             */
            Node& operator= (const Node& n) {
                // preserve parent_
                name_ = n.name_;
                value_ = n.value_;
                origin_ = n.origin_;
                has_value_ = n.has_value_;
                write_count_ = static_cast<decltype(write_count_)>(has_value_);
                read_count_ = 0;
                for(auto& x : n.children_){
                    children_.emplace_back(new Node(this, *x.get()));
                }
                return *this;
            }

            /*!
             * \brief Dumps the content of this node to an ostream on a single
             * line. Does not recurs into children
             * \param o Ostream to write content to
             */
            void dump(std::ostream& o) const {
                o << "<VPT Node: \"" << name_ << "\" children:" << children_.size()
                  << " writes:" << write_count_ << " reads:" << read_count_ << " required:" << required_ << ">";
            }

            /*!
             * \brief Gets the name of this node
             */
            const std::string& getName() const { return name_; }

            /*!
             * \brief Gets the parent of this node
             */
            Node* getParent() { return parent_; }

            /*!
             * \brief Gets the parent of this node
             */
            Node const * getParent() const { return parent_; }

            /*!
             * \brief Gets the parent of this node
             */
            Node * getRoot() {
                Node* p = this;
                while(p->getParent() != nullptr){
                    p = p->getParent();
                }
                sparta_assert(p->getName().size() == 0);
                return p;
            }

            /*!
             * \brief Gets the parent of this node
             */
            Node const * getRoot() const {
                Node const * p = this;
                while(p->getParent() != nullptr){
                    p = p->getParent();
                }
                sparta_assert(p->getName().size() == 0);
                return p;
            }

            /*!
             * \brief Gets the ParameterTree object that owns this node
             */
            ParameterTree* getOwner() {
                return getRoot()->tree_;
            }

            /*!
             * \brief Gets the ParameterTree object that owns this node
             */
            const ParameterTree* getOwner() const {
                return getRoot()->tree_;
            }

            /*!
             * \brief Gets the path to this node including the root node.
             */
            std::string getPath() const {
                std::stack<const std::string*> names;
                Node const * n = this;
                while(n && n->getName().size() > 0){ // Stop at null parent, or root (which has no name)
                    names.push(&n->getName());
                    n = n->getParent();
                }

                std::stringstream ss;
                if(names.size() > 0){
                    ss << *names.top();
                    names.pop();
                    while(names.size() > 0){
                        ss << '.' << *names.top();
                        names.pop();
                    }
                }
                return ss.str();
            }

            /*!
             * \brief Is this a root node.
             */
            bool isRoot() const {
                return name_.size() == 0;
            }

            /*!
             * \brief Increment the read count of this node
             */
            void incrementReadCount() const {
                ++read_count_;
            }

            /*!
             * \brief Gets the number of times this node has been accessed to be read
             * (i.e. with get/tryGet)
             */
            uint32_t getReadCount() const {
                return read_count_;
            }

            /*!
             * \brief Gets the value of this object as a string
             * \pre Value must be set for this node. See hasValue
             * \post Increments read count
             */
            const std::string& getValue() const {
                sparta_assert(hasValue(), "Node \"" << name_ << "\" does not have a value associated with it");
                incrementReadCount();
                return value_;
            }

            /*!
             * \brief Gets the value of this object as a string
             * \pre Value must be set for this node. See hasValue
             * \post Does not increments read count
             */
            const std::string& peekValue() const {
                sparta_assert(hasValue(), "Node \"" << name_ << "\" does not have a value associated with it");
                return value_;
            }

            /*!
             * \brief Gets the origin associated with the value at this node
             * \pre Value must be set for this node. See hasValue
             */
            const std::string& getOrigin() const {
                sparta_assert(hasValue(), "Node \"" << name_ << "\" does not have a value associated with it");
                return origin_;
            }

            /*!
             * \brief Gets the value in this object.
             * \tparam Template parameter
             * \note This method handles non-string-convertible types. The other
             * getAs handles string-convertible types
             *
             * Example
             * \verbatim
             * val.getAs<std::string>()
             * val.getAs<uint32_t>()
             * \endverbatim
             */
            template <typename T, typename=typename std::enable_if<!std::is_convertible<T, std::string>::value>::type>
            T getAs() const {
                return lexicalCast<T>(getValue());
            }

            /*!
             * \brief getAs template instance for string types (e.g. char[], const char*, std::string)
             */
            template <typename T, typename=typename std::enable_if<std::is_convertible<T, std::string>::value>::type>
            const std::string& getAs() const {
                return getValue();
            }

            /*!
             * \brief Get value as a string
             */
            operator std::string () const {
                return value_;
            }

            /*!
             * \brief Get value as a specific type through getAs
             * \tparam T Type to retrieve content as
             */
            template <typename T>
            operator T () const {
                return getAs<T>();
            }

            /*!
             * \brief Equality test. Attempts to lexically cast underlying string to requested
             * data-type.
             */
            template <typename T>
            bool operator==(const T& rhp) const {
                return getAs<T>() == rhp;
            }

            /*!
             * \brief Does a string, \a name, interpreted as a sparta TreeNode pattern, match another
             * string interpreted as a concrete (no-wildcards) node name.
             * \param pattern String pattern to attempt to match with. This is a sparta::TreeNode
             * single location pattern (no '.' characters)
             * \param other String to attempt to match. Interpreted as a plain string, regardless
             * of content.
             * \erturn true if \a pattern matches \a other, false if not/
             */
            static bool matches(const std::string& pattern, const std::string& other) {
                std::regex expr(TreeNode::createSearchRegexPattern(pattern));
                std::smatch what;
                return std::regex_match(other, what, expr);
            }

            /*!
             * \brief Gets the most recently created child of this node by a concrete child name.
             * \param name Name to get a child with. Must not be a pattern. Pattern-based nodes must
             * be added and cannot be searched for. Searching based on a pattern would be too
             * complicated
             * \return First child node (newest-to-oldest order) whose patter matches on \a name
             */
            Node* getChild(const std::string& name) const {
                if(TreeNode::hasWildcardCharacters(name)){
                    throw SpartaException("Cannot call ParameterTree::Node::getChild with a name that is a search pattern: \"")
                        << name << "\". addChild must be used instead";
                }

                // Always search in reverse-applied order to match on most recent changes first
                auto itr = children_.rbegin();
                for(; itr != children_.rend(); ++itr){
                    if(matches((*itr)->getName(), name)){
                        return itr->get();
                    }
                }

                return nullptr;
            }

            /*!
             * \brief Get a child for setting a parameter, creating it if needed.
             * \param path Path of immediate child to get/create. If there is a child by this path
             * which was not created before any wildcard nodes matching \a name, then the existing
             * node will be returned. Otherwise, a new child will be created.
             * \param required Is this node required to exist in the tree. This is a deferred value
             * \note When not creating new children, it is probably better to invoke "create" from
             * the ParameterTree with a complete path rather than this node. Otherwise the intended
             * precedence of parameters might not match that represented by the tree.
             */
            Node* create(const std::string& path, bool required) {
                if(path.size() == 0){
                    return this;
                }

                size_t name_pos = 0;
                std::string immediate_child_name;
                immediate_child_name = TreeNode::getNextName(path, name_pos);
                if(immediate_child_name.size() == 0){
                    // Cannot get parent like this
                    return nullptr; // TEMPORARY behavior. See docstring
                    //throw SpartaException("Virtual parameter path \"") << path
                    //      << "\" is invalid because it contains an empty name (between two '.' "
                    //         "characters). Parents cannot currently be referenced in the virtual parameter tree";
                }

                // Get a child of node if one exists with an exact match before any nodes
                // with wilcards are encountered. If this node name includes a wildcard, a
                // child will be returned IFF it the pattern string itself matches with the
                // highest priority child in this node.
                Node* child = getPriorityChildMatch(immediate_child_name);
                if(!child){
                    child = addChild(immediate_child_name, required);
                }

                if(name_pos == std::string::npos){
                    return child;
                }

                const std::string remainder = path.substr(name_pos);
                return child->create(remainder, required);
            }

            /*!
             * \brief Attempts to get an immediate child with an exact match for a given name or
             * pattern string. There is no pattern matching in this method. Patterns are treated as
             * raw strings.
             *
             * This searches children in reverse order (priority order) for a match. This will
             * find the most recently created child which matches the pattern
             */
            Node* getPriorityChildMatch(const std::string& name) const {
                if(children_.size() == 0){
                    return nullptr;
                }

                // Becase the name is a pattern, it is impossible to math against other existing
                // children which are patterns. Therefore, if a pattern child is encountered
                // which does not exactly match the given name string, nullptr must be returned so
                // that the caller appends a new node, whih will have the highest priority
                const bool name_has_wildcard = TreeNode::hasWildcardCharacters(name);

                // Always search in reverse-applied order to match on most recent changes first.
                //
                // This is an optimization to prevent nodes from being created needlessly. It tries
                // to find a matching node while considering wildcards to avoid creating new nodes
                // in most cases where it is possible.
                //
                // Read the rules below to understand what happens. In general, if we don't get an
                // exact string match with an existing node before hitting another node which is
                // an exclusive superset or incomplete subset of the 'name' argument's pattern (it
                // may have no wildcards) then a new node will need to be added at the end of the
                // children list to specify a value. An existing node cannot be overridden.
                auto itr = children_.rbegin();
                for(; itr != children_.rend(); ++itr){
                    if((*itr)->getName() == name){
                        // Found a node with an exact name match (no pattern matching) before
                        // hitting a pattern node. This node can be used to apply a new parameter
                        return itr->get();
                    }

                    if(name_has_wildcard){
                        if(TreeNode::hasWildcardCharacters((*itr)->getName())){
                            // Encountered a wildcard node whch was not an exact string match.
                            // No way to tell if the name pattern exactly matches this node's, so
                            // assume it doesn't
                            return nullptr;
                        }else if(matches(name, (*itr)->getName())){
                            // Node has not wildcards but matches the pattern in 'name'. Therfore,
                            // A new parameter with this name would affect this node and more. A new
                            // node will have to be added to override it.
                            return nullptr;
                        }else{
                            // Node has no wildcards and the name agument is a pattern which
                            // does not match it. Therefore, it can be ignored because this 'name'
                            // will not apply
                            continue;
                        }
                    }else if(matches((*itr)->getName(), name)){
                        // Encountered a wildcard node which matches on this name before hitting an
                        // exact match. Therefore, a new node must be created by the caller so that
                        // the parameter being set will affect a subset of this node's pattern
                        return nullptr;
                    }
                }
                // No matches or important mismatches
                return nullptr;
            }

            /*!
             * \Create a child of this node at the end of the list (meaning it is the most recent)
             */
            Node* addChild(const std::string& name, bool required) {
                sparta_assert(hasValue() == false,
                            "Cannot add a child to a virtual parameter tree node \"" << name_
                            << "\" since it already has a value: \"" << value_ << "\"");
                children_.emplace_back(new Node(this, name));
                if(required){
                    children_.back()->incRequired();
                }
                return (children_.back().get());
            }

            /*!
             * \brief Gets a child of this node by its name
             * \note The subscript operator can be strung together for a multi-level lookup as in:
             * \code
             * uint32_t val = ptree["foo"]["bar"]["buz"].getAs<uint32_t>()
             * \endcode
             */
            Node& operator[] (const std::string& name) const {
                Node* child = getChild(name);
                sparta_assert(child != nullptr,
                                  "Node \"" << name_ << "\" has no child named \"" << name << "\"");
                return *child;
            }

            Node& operator[] (const char* name) const {
                return this->operator[](std::string(name));
            }

            /*!
             * \brief Does this node have a value written to it which can be accessed through:
             * \li operator std::string
             * \li getAs
             * \li operator T
             * \li getValue
             */
            bool hasValue() const {
                return has_value_;
            }

            /*!
             * \brief Set a value on this node directly
             * \param val Value to assign.
             * \param required Must the parameter identified by this node be consumed (may be overwritten later)?
             * \param origin Origin of the value (e.g. "foo.yaml line:2 col:35")
             * \warning This is direcly applied to this node, which may not have the intended effect
             */
            void setValue(const std::string& val, bool required=true, const std::string& origin="") {
                //sparta_assert(parent_,
                //                  "Cannot assign a value to the root node of a virtual parameter tree");
                //sparta_assert(read_count_ == 0,
                //                  "Cannot set(\"" << val << "\") on node \"" << name_ << "\" because it has been read already");
                value_ = val;
                origin_ = origin;
                has_value_ = true;
                write_count_++;
                if(required){
                    required_ += 1;
                }
            }

            /*!
             * \brief Increment the required count.
             */
            void incRequired() {
                required_ += 1;
            }

            /*!
             * \brief Clear the required count. This is necessary if a parameter is flagged as
             * deprecated or removed in a configuration file. This affects this node only.
             */
            void unrequire() {
                required_ = 0;
                for(auto & n : children_){
                    n->unrequire();
                }
            }

            /**
             * \brief Release this node and its children from the tree
             * \return The ChildVector including this node
             *
             */
            std::unique_ptr<Node> release() {
                return parent_->release_(this);
            }

            /*!
             * \brief Set the string value of a child of this node. Note that this may not affect
             * this node directly because of the way pattern nodes work
             * \param path Path to child. To set this node, use path=""
             * \param val Value to assign
             * \param required Is this parameter location required?
             * \param origin Origin of the value (e.g. "foo.yaml line:50 col:23")
             * \post Increments write_count_
             *
             * \return TEMPORARY: Returns true of parameter is set, false if not. May return false if
             * parameter path contains parent reference (any '.' character whicih is proceded by another
             * '.' or the beginnign of the string). This will be fixed eventually.
             */
            bool set(const std::string& path, const std::string& val, bool required, const std::string& origin="") {
                //sparta_assert(read_count_ == 0,
                //            "Cannot set(\"" << val << "\") on node \"" << name_
                //            << "\" because it has been read already");

                // Set through root of the tree.
                std::string full_path = getPath();
                if(full_path.size() > 0 && path.size() > 0){
                    full_path += ".";
                }
                full_path += path;
                return getOwner()->set(full_path, val, required, origin);
            }

            /*!
             * \brief String value assignment operator.
             * \note Alias for set
             * \return Value argument so that multiple = calls can be strung together.
             * \note Implies node will be required and with unkonwn ("") origin
             */
            const std::string& operator= (const std::string& val) {
                set("", val, true);
                return val;
            }

            /*!
             * \brief Return true if this parameter node is required to exist by
             * the client by 1 or more "set"-ers using this object
             * \note Invokes ParameterTree::isRequired with this node's path
             */
            bool isRequired() const {
                // Start at beginning. Another, later-written node may override
                // this node if it has the same patch or a matching pattern.
                if (getOwner()->isRequired(getPath()))
                {
                    return true;
                }
                return false;
            }

            /*!
             * \brief Returns the number of times this node has been flagged as
             * required
             */
            uint32_t getRequiredCount() const {
                return required_;
            }

            /*!
             * \brief Gets vector of pointers to children of this node
             */
            std::vector<Node*> getChildren() {
                std::vector<Node*> children;
                for(auto & n : children_){
                    children.push_back(n.get());
                }
                return children;
            }

            /*!
             * \brief Gets vector of pointers to children of this node
             */
            std::vector<const Node*> getChildren() const {
                std::vector<const Node*> children;
                for(auto & n : children_){
                    children.push_back(n.get());
                }
                return children;
            }

            /*!
             * \brief Recursively print
             */
            void recursePrint(std::ostream& o, uint32_t indent=0, bool print_user_data=true) const {
                for(uint32_t i=0; i<indent; ++i){
                    o << " ";
                }
                o << name_;
                if(has_value_){
                    o << " = \"" << value_ << "\" (read " << read_count_ << ", written " << write_count_
                      << ", required " << required_ << ", origin '" << getOrigin() << "')";
                }
                o << "\n";
                if(print_user_data){
                    printUserData(o, indent+2);
                }
                for(auto & n : children_){
                    n->recursePrint(o, indent+2, print_user_data);
                }
            }

            /*!
             * \brief Pretty-print all user data for this node, if any.
             */
            void printUserData(std::ostream& o, uint32_t indent=0) const {
                if (user_data_.empty()) {
                    return;
                }

                for(uint32_t i=0; i<indent; ++i){
                    o << " ";
                }

                o << "User data (" << getPath() << "):\n";
                for (const auto & [ud_name, ud_printer] : user_data_printers_) {
                    std::any ud = user_data_.at(ud_name);
                    ud_printer->print(ud_name, ud, o, indent+2);
                }
            }

            /*!
             * \brief Appends a tree as a child of this node
             * \param ot Other tree to append. If \a ot is a root node (isRoot),
             * it is merged over the current node, adding or replacing all its
             * children. Otherwise, \a ot is added as a child of this node. To
             * add all children of a particular node without adding the parent,
             * those children must currently be iterated and separately appended
             * to node through appendTree calls. However, since only leaf nodes
             * are expected to have actual parameter values, it is typically
             * safe to simply call appendTree with a node that is not a leaf
             * when its children (subtrees) are the only nodes of interest
             */
            void appendTree(const Node* ot) {
                sparta_assert(ot, "Cannot append a null virtual parameter tree");
                if(ot->getName().size() > 0){
                    // Attach 'ot' node argument as child of this
                    const bool required = false; // The starting node is not required to exist.. only its node children
                    Node* child = create(ot->getName(), required);
                    child->recursAppendTree_(ot);
                }else{
                    // 'ot' is a root node (no name). Merge it with this.
                    recursAppendTree_(ot);
                }
            }

            /*!
             * \brief Set any named user data (std::any)
             * \note User data type must be copy constructible
             */
            template <typename T>
            void setUserData(const std::string & name, const T & user_data) {
                static_assert(std::is_copy_constructible<T>::value, "std::any only works with copyable types");
                user_data_[name] = user_data;
                user_data_printers_[name] = std::make_unique<UserDataPrinter<T>>();
            }

            /*!
             * \brief Set any named user data (std::any)
             * \note User data type must be copy constructible
             */
            template <typename T>
            void setUserData(const std::string & name, T && user_data) {
                static_assert(std::is_copy_constructible<T>::value, "std::any only works with copyable types");
                user_data_[name] = std::move(user_data);
                user_data_printers_[name] = std::make_unique<UserDataPrinter<T>>();
            }

            /*!
             * \brief Get any named user data (std::any_cast)
             */
            template <typename T>
            const T & getUserData(const std::string & name) const {
                constexpr bool must_exist = true;
                return *tryGetUserData<T>(name, must_exist);
            }

            /*!
             * \brief Get any named user data (std::any_cast)
             */
            template <typename T>
            T & getUserData(const std::string & name) {
                constexpr bool must_exist = true;
                return *tryGetUserData<T>(name, must_exist);
            }

            /*!
             * \brief Try to get any named user data (std::any_cast)
             */
            template <typename T>
            const T * tryGetUserData(const std::string & name, bool must_exist = false) const {
                auto it = user_data_.find(name);
                if (it == user_data_.end()) {
                    if (must_exist) {
                        throw SpartaException("User data '") << name << "' does not exist for node '"
                            << getPath() << "'";
                    }
                    return nullptr;
                }
                return &std::any_cast<const T&>(it->second);
            }

            /*!
             * \brief Try to get any named user data (std::any_cast)
             */
            template <typename T>
            T * tryGetUserData(const std::string & name, bool must_exist = false) {
                auto it = user_data_.find(name);
                if (it == user_data_.end()) {
                    if (must_exist) {
                        throw SpartaException("User data '") << name << "' does not exist for node '"
                            << getPath() << "'";
                    }
                    return nullptr;
                }
                return &std::any_cast<T&>(it->second);
            }

            /*!
             * \brief Get a mapping from Nodes to their extensions recursively.
             */
            void recurseGetAllNodeExtensions(
                std::map<const Node*, std::map<std::string, const TreeNode::ExtensionsBase*>> & map) const
            {
                for (const auto & key : getUserDataKeys()) {
                    if (auto ext = tryGetUserData<std::shared_ptr<TreeNode::ExtensionsBase>>(key)) {
                        map[this][key] = ext->get();
                    }
                }

                for (auto child : getChildren()) {
                    child->recurseGetAllNodeExtensions(map);
                }
            }

            /*!
             * \brief Get all user data keys (names).
             */
            std::set<std::string> getUserDataKeys() const {
                std::set<std::string> keys;
                for (const auto & [key, _] : user_data_) {
                    keys.insert(key);
                }
                return keys;
            }

            /*!
             * \brief Clear named user data. Returns true if removed, false if not found.
             */
            bool clearUserData(const std::string & name) {
                user_data_printers_.erase(name);
                if (user_data_.count(name)) {
                    user_data_.erase(name);
                    return true;
                }
                return false;
            }

            /*!
             * \brief Clear all user data. Returns the number of elements removed.
             */
            size_t clearUserData() {
                user_data_printers_.clear();
                auto sz = user_data_.size();
                user_data_.clear();
                return sz;
            }

            /*!
             * Object for iterating over children to detect name/expression matches.
             * Iteration order is most recent to oldest.
             */
            class MatchIterator {
                ChildVector::const_reverse_iterator itr_;
            public:
                MatchIterator(ChildVector::const_reverse_iterator itr) :
                    itr_(itr)
                {;}

                MatchIterator(const MatchIterator&) = default;

                MatchIterator& operator=(const MatchIterator&) = default;

                bool operator==(const MatchIterator& rhp) const {
                    return itr_ == rhp.itr_;
                }

                bool operator!=(const MatchIterator& rhp) const {
                    return itr_ != rhp.itr_;
                }

                void operator++(int) {++itr_;}

                void operator++() {++itr_;}

                bool matches(const std::string& other) const {
                    return ParameterTree::Node::matches((*itr_)->getName(), other);
                }

                const Node* get() const {
                    return itr_->get();
                }

                Node* get() {
                    return itr_->get();
                }

                const Node* operator->() const {
                    return itr_->get();
                }
            };

            friend class MatchIterator;

            /*!
             * \brief Get most recent child added
             */
            MatchIterator getMatcherBegin() const { return MatchIterator(children_.rbegin()); }

            /*!
             * \brief Get end of child match iterator (past oldest child added)
             */
            MatchIterator getMatcherEnd() const { return MatchIterator(children_.rend()); }

        private:

            /*!
             * \brief Recursively append children of another node to this node while preserving order
             * \param ot "root" node of other tree to append to "this" tree
             */
            void recursAppendTree_(const Node* ot) {
                // Inherit value. Never invalidate
                if(ot->hasValue()){
                    setValue(ot->peekValue(), ot->getRequiredCount() > 0, ot->getOrigin());
                }

                // Inherit user data.
                for(const auto & [ud_name, ud_value] : ot->user_data_){
                    user_data_[ud_name] = ud_value;
                }

                for(auto & child : ot->getChildren()){
                    // TODO: copy required count instead of just boolean
                    Node* c = create(child->getName(), child->getRequiredCount() > 0); // Create if needed
                    c->recursAppendTree_(child);
                }
            }

            std::unique_ptr<Node> release_(Node *node) {
                std::unique_ptr<Node> rtn;
                auto it = std::find_if(children_.begin(), children_.end(),
                                       [node] (const auto & child) -> bool {
                                           return (child.get() == node);
                                       });
                if (it != children_.end()) {
                    rtn = std::move(*it);
                    children_.erase(it);
                }
                return rtn;
            }
        };

        /*!
         * \brief Default Constructor
         */
        ParameterTree() :
            root_(new Node(nullptr, this))
        {;}

        ParameterTree(const ParameterTree& rhp) :
            root_(new Node(nullptr, this))
        {
            root_->appendTree(rhp.getRoot());
        }

        ParameterTree& operator=(const ParameterTree& rhp) {
            clear();
            merge(rhp);
            return *this;
        }

        //bool operator==(const ParameterTree& rhp) const {
        //}

        /*!
         * \brief Destructor
         */
        virtual ~ParameterTree() {}

        /*!
         * \brief Clear all content from this tree
         */
        void clear() {
            root_.reset(new Node(nullptr, this)); // Clear all children
        }

        /*!
         * \brief Add a parameter to the tree, replacing any existing parameter
         * \param path Path (from root) to assign value
         * \param value Value to assign to a path in this tree
         * \param required Must this parameter exist?
         * \param origin Origin of the value (e.g. "foo.yaml line:50 col:23")
         * \return TEMPORARY: Returns true of parameter is set, false if not. May return false if
         * parameter path contains parent reference (any '.' character whicih is proceded by another
         * '.' or the beginnign of the string). This will be fixed eventually.
         *
         * Setting is fundamentally different than getting in a virtual parameter tree.
         * It cannot fail and (when setting with patterns [paths containing wildcards]),
         * new nodes are almost always created.
         */
        bool set(const std::string& path, const std::string& value, bool required, const std::string& origin="") {
            Node* n = create(path, false); // inc required after setting value
            if(!n){
                return false;
            }
            n->setValue(value, required, origin);

            return true;
        }

        /*!
         * \brief Add a node to the tree, with proper priority
         * \param path Path to create. TEMPORARY: Must not contain parent references (i.e. extra dots)
         * \param Are the nodes added by this call required to exist?
         *
         * \return TEMPORARY: Returns a Node if one is created or found, nullptr if not. May return
         * nullptr if parameter path contains parent reference (any '.' character whicih is proceded
         * by another '.' or the beginnign of the string). This will be fixed eventually. Nothing
         * else should cause this to return nullptr
         *
         * Setting is fundamentally different than getting in a virtual parameter tree.
         * It cannot fail and (when setting with patterns [paths containing wildcards]),
         * new nodes are almost always created.
         */
        Node* create(const std::string& path, bool required=false) {
            if(path.size() == 0){
                return getRoot();
            }

            return getRoot()->create(path, required);
        }

        /*!
         * \brief Gets a node from the parameter tree while respecting parameter application order.
         * In other words, getting through this method returns the latest value set for the
         * parameter at the location described by \a path
         * \param path Path of node to retrieve
         * \throw SpartaException if node with given path does not exist (see exists)
         * \return Node at the given location. This node is temporary and becomes stale on any
         * modification to this ParameterTree. Reading this node later may result in an incorrect
         * value
         * \warning The returned node should be considered stale when this ParameterTree is modified
         */
        const Node& get(const std::string& path) const {
            const Node* node = tryGet(path);
            if(!node){
                throw SpartaException("Unable to find parameter in tree: \"") << path << "\"";
            }
            return *node;
        }

        /*!
         * \brief Gets a node form the parameter tree
         * \param name Name of parameter to retrieve
         */
        const Node& operator[] (const std::string& name) const {
            return get(name);
        }

        /*!
         * \brief Try to check if a node has value
         * \param path Path of the node to check
         * \param must_be_leaf Check only if leaf node
         * \return Bool
         */
        bool hasValue(const std::string& path, const bool must_be_leaf = true) const {
            const Node* n = tryGet_(path, must_be_leaf);
            return n != nullptr && n->hasValue();
        }

        /*!
         * \brief Try to check if a node exists
         * \param path Path of the node to check
         * \param must_be_leaf Check only if leaf node
         * \return Bool
         */
        bool exists(const std::string& path, const bool must_be_leaf = true) const {
            return tryGet_(path, must_be_leaf) != nullptr;
        }

        /*!
         * \brief Counts the number of values attached to the parameter tree
         * which have values but have not been read. This is a smart system
         * which looks at historical parameter values as well as the latest to
         * be sure that all have been touched.
         * \param[out] nodes Nodes with unread values. This is not cleared
         *
         * The implementation of this method is highly coupled with the tryGet()
         * which marks read parameters (including historically set parameters)
         * as read to satify this method
         */
        uint32_t getUnreadValueNodes(std::vector<const Node*>* nodes) const {
            return recursCountUnreadValueNodes_<const Node>(root_.get(), nodes);
        }

        /*!
         * \brief Counts the number of values attached to the parameter tree
         * which have values but have not been read. This is a smart system
         * which looks at historical parameter values as well as the latest to
         * be sure that all have been touched.  non-const version
         * \param[out] nodes Nodes with unread values. This is not cleared
         *
         * The implementation of this method is highly coupled with the tryGet()
         * which marks read parameters (including historically set parameters)
         * as read to satify this method
         */
        uint32_t getUnreadValueNodes(std::vector<Node*>* nodes) {
            return recursCountUnreadValueNodes_<Node>(root_.get(), nodes);
        }

        /*!
         * \brief Try to get a node if it exists. Returns nullptr it it does not
         * \param path Path of the node to retrieve
         * \return The node at the given path
         */
        const Node* tryGet(const std::string& path, const bool must_be_leaf = true) const {
            return tryGet_(path, must_be_leaf);
        }

        /*!
         * \brief tryGet non-const version
         * \param path Path of the node to retrieve
         * \return The node at the given path
         */
        Node* tryGet(const std::string& path, const bool must_be_leaf = true) {
            return tryGet_(path, must_be_leaf);
        }

        /*!
         * \brief Recursively find first leaf node matching this pattern and
         * decide if any node matching that node's pattern is required to
         * exist in the SPARTA tree.
         */
        bool isRequired(const std::string& path) const {

            if(path.size() == 0){
                return root_->getRequiredCount() > 0;
            }

            std::string immediate_child_name;
            size_t name_pos = 0;
            immediate_child_name = TreeNode::getNextName(path, name_pos);

            if(immediate_child_name.size() == 0){
                // Cannot get parent.
                throw SpartaException("Parameter ") << path
                                                  << " is invalid because it contains an empty name (between two '.' "
                    "characters). Parents cannot currently be refrenced in the parameter tree";
            }

            bool match_found = false;
            const bool required = recursGetIsRequired_(root_.get(), path, immediate_child_name, name_pos, match_found);
            sparta_assert(match_found,
                        "Asked ParameterTree if path \"" << path << "\" is required but no "
                        "matching node was found in the ParameterTree");
            return required;
        }

        /*!
         * \brief Unrequire a node in the tree
         * \param path The path to the node to unrequire (set ignore)
         * \return true if the node was found and set; false otherwise
         */
        bool unrequire(const std::string &path) {
            Node * node = tryGet_(path, false);
            if(nullptr != node) {
                node->unrequire();
                return true;
            }
            return false;
        }

        /*!
         * \brief Has a node with a given path been read
         * \param[in] path Path to check
         *
         * Resolves a path to any matching nodes (which may include any number
         * of wildcard nodes) and checks that at least one of those nodes have
         * been read 1 or more times.
         */
        bool isRead(const std::string& path) const {
            if(path.size() == 0){
                return root_->hasValue() && root_->getReadCount() > 0;
            }

            std::string immediate_child_name;
            size_t name_pos = 0;
            immediate_child_name = TreeNode::getNextName(path, name_pos);

            if(immediate_child_name.size() == 0){
                // Cannot get parent.
                throw SpartaException("Parameter ") << path
                                                  << " is invalid because it contains an empty name (between two '.' "
                    "characters). Parents cannot currently be refrenced in the parameter tree";
            }

            return recursIsRead_(root_.get(), path, immediate_child_name, name_pos);
        }

        Node const * getRoot() const { return root_.get(); }

        Node * getRoot() { return root_.get(); }

        /*!
         * \brief Merge this tree with another by applying all of its parameters
         * to this tree. Parameters in the right tree will override this tree's
         * parameters if there are duplicate paths or overlapping patterns
         * \note This also copies all user data from the source ParameterTree
         * into 'this'. User data will remain in the source tree (rhp).
         */
        void merge(const ParameterTree& rhp) {
            root_->appendTree(rhp.getRoot());
        }

        /*!
         * \brief Recursively print
         */
        void recursePrint(std::ostream& o, bool print_user_data=true) const {
            root_->recursePrint(o, 0, print_user_data); // Begin with 0 indent
        }

        /*!
         * \brief Get a mapping from Nodes to their extensions.
         */
        std::map<const Node*, std::map<std::string, const TreeNode::ExtensionsBase*>>
        getAllNodeExtensions() const
        {
            std::map<const Node*, std::map<std::string, const TreeNode::ExtensionsBase*>> all_ext_map;
            root_->recurseGetAllNodeExtensions(all_ext_map);
            return all_ext_map;
        }

    private:

        /*!
         * \brief Attempt to get a node with a given path while respecting parameter application
         * order.
         */
        const Node* tryGet_(const std::string& path, const bool must_be_leaf = true) const {
            if(path.size() == 0){
                return root_.get();
            }

            std::string immediate_child_name;
            size_t name_pos = 0;
            immediate_child_name = TreeNode::getNextName(path, name_pos);

            if(immediate_child_name.size() == 0){
                // Cannot get parent.
                throw SpartaException("Parameter ") << path
                                                  << " is invalid because it contains an empty name (between two '.' "
                    "characters). Parents cannot currently be refrenced in the parameter tree";
            }

            return recursTryGet_<const Node>(static_cast<const Node*>(root_.get()), path,
                                             immediate_child_name, name_pos, must_be_leaf);
        }

        /*!
         * \brief Attempt to get a node with a given path while
         * respecting parameter application order. Non-const version
         */
        Node* tryGet_(const std::string& path, const bool must_be_leaf = true) {
            if(path.size() == 0){
                return root_.get();
            }

            std::string immediate_child_name;
            size_t name_pos = 0;
            immediate_child_name = TreeNode::getNextName(path, name_pos);

            if(immediate_child_name.size() == 0){
                // Cannot get parent.
                throw SpartaException("Parameter ") << path
                                                  << " is invalid because it contains an empty name (between two '.' "
                    "characters). Parents cannot currently be refrenced in the parameter tree";
            }

            return recursTryGet_(root_.get(), path,
                                 immediate_child_name, name_pos, must_be_leaf);
        }

        template<typename NodeT>
        static NodeT* recursTryGet_(NodeT* node, const std::string& path,
                                    const std::string& match_name,
                                    size_t name_pos, const bool must_be_leaf)
        {
            sparta_assert(!TreeNode::hasWildcardCharacters(match_name),
                        "Cannot attempt to read a node with a path containing wildcard "
                        "characters. A specific node path must be used. Error in \""
                        << match_name << "\" from \"" << path << "\"");

            if(name_pos == std::string::npos){
                // End of the search.. No deeper
                NodeT* result = nullptr;
                NodeT* backup = nullptr; // First match (if is has no value)
                auto itr = node->getMatcherBegin();
                for(; itr != node->getMatcherEnd(); ++itr){
                    if(itr.matches(match_name)){
                        if(itr.get()->hasValue() || !must_be_leaf){
                            itr.get()->incrementReadCount();
                            if(!result){
                                result = itr.get(); // Found it
                            }
                        }else if(!backup){
                            backup = itr.get();
                        }
                    }
                }
                if(result){
                    return result;
                }
                return backup; // No match here
            }

            // Search deeper
            std::string immediate_child_name;
            immediate_child_name = TreeNode::getNextName(path, name_pos);

            if(immediate_child_name.size() == 0){
                // Cannot get parent.
                throw SpartaException("Parameter ") << path
                                                  << " is invalid because it contains an empty name (between two '.' "
                                                     "characters). Parents cannot currently be refrenced in the parameter tree";
            }

            NodeT* result = nullptr;
            auto itr = node->getMatcherBegin();
            for(; itr != node->getMatcherEnd(); ++itr){
                if(itr.matches(match_name)){
                    NodeT* match = recursTryGet_(itr.get(), path, immediate_child_name, name_pos, must_be_leaf);
                    if(match && (match->hasValue() || !must_be_leaf)) {
                        match->incrementReadCount();
                        if(result == nullptr){
                            result = match;
                        }
                    }
                    // Keep this match as result and continue iterating to mark all matching nodes as read
                }
            }
            return result;
        }

        /*!
         * \brief Implements ParameterTree::isRequired()
         */
        bool recursGetIsRequired_(const Node* node,
                                  const std::string& path,
                                  const std::string& match_name,
                                  size_t name_pos,
                                  bool& found_match) const
        {
            found_match = false;

            if(name_pos == std::string::npos){
                // End of the search.. No deeper
                auto itr = node->getMatcherBegin();
                for(; itr != node->getMatcherEnd(); ++itr){
                    if(TreeNode::hasWildcardCharacters(match_name)
                        ? itr->getName() == match_name // Exact name match if match_name has wildcards
                        : itr.matches(match_name))     // Pattern match if match_name has no wildcards
                    {
                        found_match = true;
                        return itr.get()->getRequiredCount() > 0;
                    }
                }

                return false; // dummy
            }

            // Search deeper
            std::string immediate_child_name;
            immediate_child_name = TreeNode::getNextName(path, name_pos);

            if(immediate_child_name.size() == 0){
                // Cannot get parent.
                throw SpartaException("Parameter ") << path
                                                  << " is invalid because it contains an empty name (between two '.' "
                                                     "characters). Parents cannot currently be refrenced in the parameter tree";
            }

            auto itr = node->getMatcherBegin();
            for(; itr != node->getMatcherEnd(); ++itr){
                if(itr.matches(match_name)){
                    const bool required = recursGetIsRequired_(itr.get(), path, immediate_child_name, name_pos, found_match);
                    if(found_match){
                        return required;
                    }
                    // No match found. Keep going
                }
            }

            sparta_assert(found_match == false); // Should not have been set
            return false; // dummy
        }

        /*!
         * \brief Implements getUnreadValueNodes for a specific node and it's children (recursively)
         */
        template<class NodeT>
        static uint32_t recursCountUnreadValueNodes_(NodeT* n, std::vector<NodeT*> * nodes) {
            uint32_t count = 0;
            if(n->hasValue() && (n->getReadCount() == 0)){
                count = 1;
                if(nodes){
                    nodes->push_back(n);
                }
            }
            auto itr = n->getMatcherBegin();
            for(; itr != n->getMatcherEnd(); ++itr){
                count += recursCountUnreadValueNodes_<NodeT>(itr.get(), nodes);
            }
            return count;
        }

        /*!
         * \brief Recurisvely check so see
         */
        bool recursIsRead_(const Node* node,
                           const std::string& path,
                           const std::string& match_name,
                           size_t name_pos) const
        {
            sparta_assert(!TreeNode::hasWildcardCharacters(match_name),
                              "Cannot attempt to read a node with a path containing wildcard "
                              "characters. A specific node path must be used. Error in \""
                              << match_name << "\" from \"" << path << "\"");

            if(name_pos == std::string::npos){
                auto itr = node->getMatcherBegin();
                for(; itr != node->getMatcherEnd(); ++itr){
                    if(itr.matches(match_name)){
                        if(itr.get()->hasValue() && itr.get()->getReadCount() > 0){
                            return true;
                        }
                    }
                }
                return false;
            }

            // Search deeper
            std::string immediate_child_name;
            immediate_child_name = TreeNode::getNextName(path, name_pos);
            if(immediate_child_name.size() == 0){
                // Cannot get parent.
                throw SpartaException("Parameter ") << path
                                                  << " is invalid because it contains an empty name (between two '.' "
                                                     "characters). Parents cannot currently be refrenced in the parameter tree";
            }

            auto itr = node->getMatcherBegin();
            for(; itr != node->getMatcherEnd(); ++itr){
                if(itr.matches(match_name)){
                    bool read = recursIsRead_(itr.get(), path, immediate_child_name, name_pos);
                    if(read){
                        return true;
                    }
                }
            }
            return false;
        }

        /*!
         * \brief Root of this ParameterTree
         */
        std::unique_ptr<Node> root_;

    }; // class ParameterSet

    inline std::ostream& operator<<(std::ostream& o, const ParameterTree::Node& n) {
        n.dump(o);
        return o;
    }

    inline std::ostream& operator<<(std::ostream& o, const ParameterTree::Node* n) {
        if(!n){
            o << "<null ParameterTree::Node>";
        }else{
            o << *n;
        }
        return o;
    }

} // namespace sparta
