// <ParameterSet> -*- C++ -*-

#pragma once

#include <iostream>
#include <iomanip>
#include <string>
#include <ostream>
#include <vector>
#include <unordered_map>

#include "sparta/utils/Utils.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/Parameter.hpp"

/*!
 * \file ParameterSet.hpp
 * \brief A set of sparta::Parameters per sparta::ResourceTreeNode
 */

namespace sparta
{
    class ParameterTree;

    /*!
     * \brief Generic container of Parameters.
     * \todo Deduce set of fields using boost::fusion or other instrospection
     * fields instead of the current __collect method.
     * \note non-copyable
     *
     * Parameter names within a single ParameterSet cannot be duplicated.
     * Parameters are added to the device tree as TreeNodes.
     *
     * Typical use is to subclass this object for each simulator component/model
     * which has a set of parameters it must consume. sparta::ParameterSet
     * provides a generic interface through which config-file parsers or
     * command-line parsers can populate the parameters from strings. Models
     * that expose the ParameterSet full of sparta::Parameter instances should
     * dynamic_cast a sparta::ParameterSet supplied to them at runtime back to
     * a known type and operate through Parameters directly instead of through
     * this generic interface.
     *
     * Printing a ParameterSet and its Parameters
     * \code
     * // For some ParamterSet& ps
     * cout << ps << endl;
     * \endcode
     *
     * Locating a single parameter within a ParameterSet
     * \code
     * // For some ParamterSet& ps
     * cout << ps.getParameter("param1") << endl; // Exception if not found
     * \endcode
     *
     * Searching for parameters within a ParameterSet
     * \code
     * // For some ParamterSet& ps
     * cout << p.getParameter("param1") << endl; // No exception if not found
     * \endcode
     *
     * \todo Allow defaulting the numeric display base for entire set
     * (like sparta::Parameter::setNumericDisplayBase)
     */
    class ParameterSet : public TreeNode
    {
        friend class ParameterBase; //!< For invoking addParameter_

    public:
        /*!
         * \brief Pointer to this ParameterSet accessible in class declaration
         * scope. This is used By parameters to attach themselves to the parent
         * at construction
         */
        ParameterSet* __this_ps;

        ParameterSet() = delete; //!< Default constructor disabled
        ParameterSet(const ParameterSet& rhp) = delete; //!< Copying disabled. Do not override
        void operator=(const ParameterSet& rhp) = delete; //!< Copying disabled. Do not override

        /**
         * \brief Name of any ParameterSet TreeNode
         */
        static constexpr char NODE_NAME[] = "params";

        /*!
         * \brief Tag added to ParameterSet nodes
         */
        static constexpr char PARAMETER_SET_NODE_TAG[] = "SPARTA_ParameterSet";

        //! Vector of ParameterBase pointers
        typedef std::vector<sparta::ParameterBase*> ParameterVector;

        //! Mapping of parameter names to parameters (for fast lookup by name)
        typedef std::unordered_map<std::string, ParameterBase*> ParameterPairs;

        /*!
         * \brief Constructor
         * \param parent TreeNode parent of this ParameterSet node.
         * \note ParameterSets do not currently inherit construction phase from
         * the parent. They must be caught up later
         */
        ParameterSet(TreeNode* parent) :
            TreeNode(NODE_NAME,
                     TreeNode::GROUP_NAME_BUILTIN,
                     TreeNode::GROUP_IDX_NONE,
                     "Parameter set"),
            __this_ps(this)
        {
            addTag(PARAMETER_SET_NODE_TAG); // Tag for easier searching

            if(parent){
                // Do not inherit parent phase yet if finalizing
                const bool inherit_phase = parent->getPhase() < TREE_FINALIZING;
                parent->addChild(this, inherit_phase);
            }
        }

        /*!
         * \brief Destructor
         */
        virtual ~ParameterSet() = default;

        bool validateIndependently(std::string& err_names) const
        {
            bool success = true;
            for(const ParameterPairs::value_type& p : keys_){
                std::string temp;
                if(!p.second->validateIndependently(temp)){
                    err_names += p.second->stringize();
                    err_names += ": ";
                    err_names += temp;
                    err_names += '\n';
                    success = false;
                }
            }
            return success;
        }

        bool validateDependencies(const TreeNode* node, std::string& err_names) const
        {
            bool success = true;
            for(const ParameterPairs::value_type& p : keys_){
                std::string temp;
                if(!p.second->validateDependencies(node, temp)){
                    err_names += p.second->stringize();
                    err_names += ": ";
                    err_names += temp;
                    err_names += '\n';
                    success = false;
                }
            }
            return success;
        }

        /*!
         * \brief Reset the read-count on all Parameters in this set
         */
        void resetReadCounts() {
            for(ParameterPairs::value_type & pair : keys_) {
                pair.second->resetReadCount_();
            }
        }

        /*!
         * \brief Reset the write-count on all Parameters in this set
         */
        void resetWriteCounts() {
            for(ParameterPairs::value_type & pair : keys_) {
                pair.second->resetWriteCount_();
            }
        }

        /*!
         * \brief Checks the read-count on all Parameters. If any have no been read
         * at least once since last write (since initialization if never written)
         * then throws ParameterException.
         * \see sparta::Parameter::getReadCount
         * \throw ParameterException if any parameter
         *
         * This method is to ensure that the parameters were actually consumed by
         * the model that defined them.
         */
        void verifyAllRead() const {
            std::stringstream errors;
            bool err = false;
            for(const ParameterPairs::value_type& p : keys_){
                if(not p.second->isReadOrIgnored()){
                    if(err){
                        errors << ", ";
                    }
                    errors << p.second->getName();
                    err = true;
                }
            }

            if(err){
                throw ParameterException("Some parameters in ParameterSet \"")
                    << getLocation() << "\" have not been read: " << errors.str()
                    << " . A sparta::Resource must read all of its parameters or explicitly "
                    ".ignore() them";
            }
        }

        /*!
         * \brief Increments the read count of each Parameter in this set.
         * \note This method is for <b>testing</b> purposes only. Normal
         * framework use should not require artificially incrementing read
         * counts
         * \see sparta::Parameter::ignore
         * \warning Read counts must may be reset by sparta infrastructure so this
         * method may have no effect if called within a ParameterSet
         * constructor. The point of this behavior is to prevent early reads of
         * any parameter in this ParameterSet from appearing as if the Resource
         * that owns this ParmeterSet read it.
         */
        void ignoreAll() const {
            for(const ParameterPairs::value_type& p : keys_){
                p.second->ignore_();
            }
        }

        //! Print out a friendly set of parameters
        virtual std::string dumpList() const {
            std::stringstream ss;
            ss << "Parameters for " << getLocation() << ":\n";

            // Determine column sizes
            //! \todo This should be optimized to store fields in a matrix
            //! such that each string isn't rendered twice
            uint32_t col_sizes[] = {0, 0, 0, 0, 0};
            for(const ParameterPairs::value_type& p : keys_){
                col_sizes[0] = std::max<uint32_t>(p.second->getTypeName().size(), col_sizes[0]);
                col_sizes[1] = std::max<uint32_t>(p.second->getName().size(), col_sizes[1]);
                col_sizes[2] = std::max<uint32_t>(p.second->getValueAsString().size(), col_sizes[2]);
                col_sizes[3] = std::max<uint32_t>(p.second->getDefaultAsString().size(), col_sizes[3]);
                col_sizes[4] = std::max<uint32_t>(numDecDigits(p.second->getReadCount()), col_sizes[4]);
            }

            // Render table with constant column size (type, key, value, default, read: num_reads)
            for(const ParameterPairs::value_type& p : keys_){
                const ParameterBase* pb = p.second;
                if(!pb->isVisibilityAllowed()){
                    continue;
                }
                ss << "  ";
                ss << '('
                   << std::setiosflags(std::ios::left) << std::setw(col_sizes[0]) << pb->getTypeName() << " "
                   << std::setiosflags(std::ios::left) << std::setw(col_sizes[1]) << pb->getName() << " : "
                   << std::setiosflags(std::ios::left) << std::setw(col_sizes[2]) << pb->getValueAsString() << ", def="
                   << std::setiosflags(std::ios::left) << std::setw(col_sizes[3]) << pb->getDefaultAsString()
                   << " read: " << std::setiosflags(std::ios::left) << std::setw(col_sizes[4]) << pb->getReadCount()
                   << ')';

                ss << '\n';
            }

            return ss.str();
        }

        // Overload of TreeNode::stringize
        virtual std::string stringize(bool pretty=false) const override {
            (void) pretty;
            std::stringstream ss;
            ss << '<' << getLocation() << ' ' << keys_.size() << " params>";
            return ss.str();
        }

        /*!
         * \brief Finds a parameter and gets its value as the templated type
         * \tparam T Type of parameter to get.
         * \param name Name of parameter whose value should be retrieved. Must
         * match parameter name exactly.
         * \return Value of parameter as type \a T if \a T is the internally
         * held type of this parameter.
         * \throw ParameterException if the parameter of \a name cannot be found
         * or the found parameter does not actually hold a value of (exactly)
         * type T.
         * \note Type \a t must match exactly. There is no intelligent casting.
         * If the variable stores its value as a uint32_t, the template type
         * \a T must be uint32_t, not uint64_t, int32_t, or string,
         * \warning Uses a dynamic cast. Do not use in performance critical code
         * \see ParameterBase::getValueAsString
         * \see ParameterBase::getTypeName
         */
        template <class T>
        const T getParameterValueAs(const std::string& name) const {
            ParameterPairs::const_iterator itr = keys_.find(name);
            if(keys_.end() == itr){
                throw ParameterException("Could not get parameter by name \"")
                    << name << "\". No parameter by that name exists.";
            }
            ParameterBase* pb = itr->second;
            sparta_assert(pb != nullptr);
            return pb->getValueAs<T>();
        }

        /*!
         * \brief Retrieves a sparta::Parameter<ContentT> reference from this
         * parameter set.
         * \tparam ContentT Type of Parameter to get
         * \param name Name of parameter to get. Parameter with this name must
         * be of the type specified by ContentT
         * \return Parameter as sparta::Parameter<ContentT>& if the parameter is
         * found by name and was of type \a ContentT
         * \throw SpartaExecption if parameter with given name does not exist or
         * was not a Parameter containing \a ContentT.
         * \note Returns a reference. Therefore, requested parameter must exist
         *
         * Example
         * \code
         * Parameter<uint32_t>& p = paramset->getParameterAs<uint32_t>("myint");
         * \endcode
         */
        template <class ContentT>
        const Parameter<ContentT>& getParameterAs(const std::string& name) const {
            const ParameterBase* pb = getParameter(name, true);
            Parameter<ContentT>* p = dynamic_cast<const Parameter<ContentT>*>(pb);
            if(!p){
                throw SpartaException("Found parameter \"") << name << "\" in ParameterSet "
                    << getLocation() << " but Parameter was not of requested type \""
                    << demangle(typeid(ContentT).name()) << "\". Was instead \""
                    << pb->getTypeName() << "\". ";
            }
            return *p;
        }

        // Overload of getParameterAs for non-const access with a pointer ContentT type
        template <class ContentT>
        Parameter<ContentT>& getParameterAs(const std::string& name) {
            ParameterBase* pb = getParameter(name, true);
            Parameter<ContentT>* p = dynamic_cast<Parameter<ContentT>*>(pb);
            if(!p){
                throw SpartaException("Found parameter \"") << name << "\" in ParameterSet "
                    << getLocation() << " but Parameter was not of requested type \""
                    << demangle(typeid(ContentT).name()) << "\". Was instead \""
                    << pb->getTypeName() << "\". ";
            }
            return *p;
        }

        /*!
         * \brief Gets a TreeNode by exact path relative to this node
         * \param name Search path from this node
         * \param must_exist Should this method generate an exception if the
         * parameter does not exist
         * \return A valid TreeNode*. If child cannot be found, method throws
         *
         * See sparta::TreeNode::getChild for more information on using get*
         * methods in a TreeNode.
         */
        ParameterBase* getParameter(const std::string& name, bool must_exist=true) {
            return getChildAs<ParameterBase>(name, must_exist);
        }

        /*!
         * \brief Const qualified variant of getParameter
         */
        const ParameterBase* getParameter(const std::string& name, bool must_exist=true) const {
            return getChildAs<ParameterBase>(name, must_exist);
        }

        //! \note This method should not be called in performance sensitive areas
        uint32_t findParameters(const std::string& name,
                                std::vector<ParameterBase*>& results) {
            std::vector<TreeNode*> nodes;
            TreeNode::findChildren(name, nodes);
            uint32_t num_found = 0;
            for(TreeNode* node : nodes){
                ParameterBase* p = dynamic_cast<ParameterBase*>(node);
                if(p != nullptr){
                    results.push_back(p);
                    ++num_found;
                }else{
                    std::cerr << "Could not cast node " << node->getLocation() << " to ParameterBase*" << std::endl;
                }
            }
            return num_found;
        }

        /*!
         * \brief Determines whether this ParameterSet has the parameter with the given name
         */
        bool hasParameter(const std::string& name) const {
            ParameterPairs::const_iterator itr = keys_.find(name);
            return itr != keys_.end();
        }

        /*!
         * \brief Gets the number of bound types
         * \return Number of bound type for this build of sparta.
         */
        static inline uint32_t getNumBoundTypes() {
            return (uint32_t)KeyValue::GBL_type_name_map.size();
        }

        /*!
         * \brief Determine if the given parameter can safely modify
         * the given list of parameters
         * \param modifying_param The modifying (or writing) parameter
         * \param params Vector of parameters to question
         * \throw SpartaException specifying which parameter cannot be modified
         * \return True if has permission; false in NDEBUG mode
         */
        bool hasModificationPermission(const ParameterBase * modifying_param,
                                       std::vector<const ParameterBase *> params) const
        {
            for(const auto & param : params)
            {
                sparta_assert(param != modifying_param);

                bool hit_modifier = false;
                for(const auto & it : params_) {
                    if(it == param) {
                        sparta_assert(hit_modifier == true, "Parameter: '"
                                          << it->getName() << "' cannot modifiy '" << param->getName()
                                          << "' because '" << it->getName()
                                          << "' does not come before '" << param->getName()
                                          << "' in the parameter set");
                        return (hit_modifier == true);
                    }
                    if(it == modifying_param) {
                        hit_modifier = true;
                    }
                }
            }
            return true;
        }

        /*!
         * \brief Determine if the given parameter can be modified by
         * a parameter currently writing to the parameter set
         * \param modify_e The modify-E
         * \throw SpartaException if the modify-E is the parameter at the top of the stack
         * \return true if can, false otherwise
         */
        bool canParameterBeModifiedByLockingParam(const ParameterBase * modify_e) {
            if(locking_params_stack_.empty()) {
                return true;
            }
            sparta_assert(locking_params_stack_.back() != modify_e,
                              "Parameter '" << locking_params_stack_.back()->getName()
                              << "' cannot modify itself in it's own callback!");
            return hasModificationPermission(locking_params_stack_.back(), {modify_e});
        }

        /*!
         * \brief Lock the parameter set for parameter to parameter updates
         * \param lock_param The parameter doing the locking
         */
        void lock(const ParameterBase * lock_param);

        /*!
         * \brief Is the parameter set currently locked by another parameter
         * \return true if locked; false otherwise
         */
        bool isLocked() const {
            return (locking_params_stack_.empty() == false);
        }

        /*!
         * \brief Unlock the parameter set for parameter to parameter updates
         * \param unlock_param The parameter doing the unlocking --
         * must be the last parameter that locked the set
         */
        void unlock(const ParameterBase * unlock_param);

        /*!
         * \brief Non-const iterator for ParameterSet
         *
         * Units can access their parameters by name trough specific parameter
         * sets. Allowing units to iterate parameters undermines the ability
         * to ensure that all Parameters were actually consumed by a unit at
         * construction.
         */
        template<typename T>
        class ParameterSetIterator
        {
        public:
            ParameterSetIterator() :
                keys_(0), key_iter_(ParameterPairs::iterator())
            {
            }

            ParameterSetIterator(ParameterPairs const * keys, T const & itr) :
                keys_(keys), key_iter_(itr)
            {
                sparta_assert(keys != 0); // Cannot create without a valid keys map
            }

            ParameterSetIterator(const ParameterSetIterator& rhp) :
                keys_(rhp.keys_), key_iter_(rhp.key_iter_)
            {}

            const ParameterSetIterator& operator=(const ParameterSetIterator& rhp)
            {
                keys_ = rhp.keys_;
                key_iter_ = rhp.key_iter_;
                return *this;
            }

            // Prefix
            const ParameterSetIterator& operator++()
            {
                // There is a bug in boost that prevents gcc from compiling this
                // with -Werror=sequence-point on.
                ++key_iter_;
                return *this;
            }

            // Postfix
            const ParameterSetIterator operator++(int i)
            {
                (void) i;
                ParameterSetIterator current = *this;
                ++(*this);
                return current;
            }

            ParameterBase* operator*(){
                if(keys_ == 0){
                    throw ParameterException("Iterator has not been initialized");
                }
                if(key_iter_ == keys_->end()){
                    throw ParameterException("Iterator has reached end and is invalid");
                }
                return key_iter_->second;
            }

            const ParameterBase * operator*() const {
                if(keys_ == 0){
                    throw ParameterException("Iterator has not been initialized");
                }
                if(key_iter_ == keys_->end()){
                    throw ParameterException("Iterator has reached end and is invalid");
                }
                return key_iter_->second;
            }

            bool operator==(const ParameterSetIterator& rhp) const {
                return key_iter_ == rhp.key_iter_;
            }

            bool operator!=(const ParameterSetIterator& rhp) const {
                return key_iter_ != rhp.key_iter_;
            }

        private:
            ParameterPairs const * keys_;
            T key_iter_;

        };

        //! Non-const iterator
        typedef ParameterSetIterator<ParameterPairs::iterator> iterator;

        //! Const iterator
        typedef ParameterSetIterator<ParameterPairs::const_iterator> const_iterator;

        // Returns a vector of names
        std::vector<std::string> getNames() const {
            std::vector<std::string> result;
            for(ParameterPairs::value_type const & pair : keys_){
                result.push_back(pair.first);
            }
            return result;
        }

        iterator begin() {
            ParameterPairs::iterator itr = keys_.begin();
            return iterator((const ParameterPairs*)&keys_, itr);
        }

        iterator end() {
            ParameterPairs::iterator itr = keys_.end();
            return iterator((const ParameterPairs*)&keys_, itr);
        }

        const_iterator begin() const {
            ParameterPairs::const_iterator itr = keys_.begin();
            return const_iterator((const ParameterPairs*)&keys_, itr);
        }

        const_iterator end() const {
            ParameterPairs::const_iterator itr = keys_.end();
            return const_iterator((const ParameterPairs*)&keys_, itr);
        }

        size_t getNumParameters() const {
            return keys_.size();
        }

        /*!
         * \brief Read values for each volatile parameter from the virtual tree
         * and re-write them to the parameters the way SPARTA did before the
         * virtual parameter tree existed.
         *
         * \pre Must be in TREE_CONFIGURING phase
         * \return Number of volatile parameters which found falues in the
         * virtual tree
         * \note Ignores whether the parameters have been read or already
         * written from the virtual tree
         */
        uint32_t readVolatileParamValuesFromVirtualTree();

        /*!
         * \brief Read values for all parameters from the virtual tree
         * and re-write them to the parameters the way SPARTA did before the
         * virtual parameter tree existed.
         *
         * \pre Must be in TREE_CONFIGURING phase
         * \pre All parameters found must either not have been read or must be
         * volatile parameters. Errors will occur if the virtual tree contains
         * values for non-volatile parameters which have already been read.
         * \return Number of parameters which found falues in the
         * virtual tree
         * \note Ignores whether the parameters have been read or already
         * written from the virtual tree
         */
        uint32_t readAllParamValuesFromVirtualTree();

   protected:

        /*!
         * \brief Add a parameter to the parameter set.
         * \temp This will be removed
         * \todo This must be done automatically by onAddingChild_ and not
         * explicitly by anyone except ParametersThemselves
         */
        void addParameter_(sparta::ParameterBase* p);

        /*!
         * \brief React to child registration
         * \param child TreeNode child that must be downcastable to a
         * sparta::ParameterBase. This is a borrowed reference - child is *not*
         * copied. Child lifetime must exceed that of this ParmeterSet instance.
         *
         * Overrides TreeNode::onAddingChild_
         */
        virtual void onAddingChild_(TreeNode* child) override;

        /*!
         * \brief When added to the tree after construction, try and pull values
         * for paraemters. Finalize the ParameterSet before attaching it to a
         * tree if this is not desired.
         */
        virtual void onAddedAsChild_() noexcept override;

        /*!
         * \brief Find the virtual parameter tree for this node's tree from its
         * root
         * \return nullptr if this node is not attached to a root or the
         * root has no simulator pointer or the simulator has no virutal
         * parmater tree.
         */
        const ParameterTree* getSimParameterTree_();

        /*!
         * \brief Find the virtual architectural parameter tree for this node's
         * tree from its root
         * \return nullptr if this node is not attached to a root or the
         * root has no simulator pointer or the simulator has no virutal
         * parmater tree.
         */
        const ParameterTree* getSimArchParameterTree_();

        /*!
         * \brief Read values for every parameter of possible from the virtual
         * parameter tree if possible.
         */
        void readValuesFromVirtualTree_();

        /*!
         * \brief Read a value for a single parameter from the virtual tree and
         * write it to the selected parameter. Also loads it's default from the
         * arch virtual tree.
         * \param[in] arch_pt Architectural Parameter Tree from which the
         * default value of the target parameter should be retrieved (if
         * available)
         * \param[in] pt Parameter Tree from which the value for the target
         * parameter should be retrieved
         * \param[in] p Parameter to which a value from the virtual parameter
         * tree shold be written. Should either be a volatile parameter
         * (sparta::Parameter::isVolatile) or have a 0 read count.
         * \todo Merge this into ParameterBase construction if possible
         * \return true if value was read from virtual tree, false if not (no
         * value available)
         */
        bool readValueFromVirtualTree_(const ParameterTree* arch_pt,
                                       const ParameterTree* pt,
                                       sparta::ParameterBase* p);

        /*!
         * \brief Map of name (key) to ParameterBase* instances
         */
        ParameterPairs keys_;

        /*!
         * \brief Straight vector of registered parameters
         */
        ParameterVector params_;

    private:

        /*!
         * \brief During parameter writes with callbacks, create a stack of
         * parameters modifying subsequent parameters
         * \note Using a vector instead of a stack to expose iterators for
         * diagnostic printing
         */
        std::vector<const ParameterBase *> locking_params_stack_;

        /*!
         * \brief Has this parameter set been populated from the virtual tree
         * already while it (or its children so far) were being constructed
         */
        OneWayBool<false> populated_from_virtual_;

    }; // class ParameterSet

} // namespace sparta

/*!
 * \brief Parameter declaration
 * \param type C++ type of the parameter. Must be an entry supported by
 * sparta::Parameter
 * \param name Name of the parameter. This will be a member variable of the
 * ParameterSet subclass in which this macro is used as well as a string in a
 * table of parameters held by that class.
 * \param def Default value. Must be a valid construction of \a type which can
 * be used to copy-construct an instance of \a type or assigned to an instance
 * of \a type.
 * \param doc Description of this Parameter. It should include the semantics
 * and allowed values.
 * \pre Invoke within public section of a class declaration which inherits from
 * sparta::ParameterSet
 * \note Validators may be attached to the parameter within the constructor of
 * the sparta::ParameterSet subclass in which this macro was invoked.
 * The PARAMETER macro was rewritten from a sub-class of sparta::Parameter to just
 * an instance of it.
 */
#define PARAMETER(type, name, def, doc)                                 \
    sparta::Parameter<type> name {#name, def, doc, __this_ps};

#define LOCKED_PARAMETER(type, name, def, doc)                          \
    sparta::Parameter<type> name {#name, def, doc, sparta::Parameter<type>::ParameterAttribute::LOCKED, __this_ps};

#define VOLATILE_LOCKED_PARAMETER(type, name, def, doc)                 \
    sparta::Parameter<type> name {#name, def, doc, sparta::Parameter<type>::ParameterAttribute::LOCKED, __this_ps, true};

#define HIDDEN_PARAMETER(type, name, def, doc)                          \
    sparta::Parameter<type> name {#name, def, doc, sparta::Parameter<type>::ParameterAttribute::HIDDEN,  __this_ps};

#define VOLATILE_HIDDEN_PARAMETER(type, name, def, doc)                 \
    sparta::Parameter<type> name {#name, def, doc, sparta::Parameter<type>::ParameterAttribute::HIDDEN, __this_ps, true};

/*!
 * \brief Special-case PARAMETER declaration
 * \note This macro was rewritten from a sub-class of sparta::Parameter to just
 * an instance of it.
 */
#define VOLATILE_PARAMETER(type, name, def, doc)                        \
    sparta::Parameter<type> name {#name, def, doc, __this_ps, true};

/*!
 * \brief Define a Parameter who will get its default from the constructor
 * \see PARAMETER
 *
 * Strictly speaking, this does not introduce any functionality that PARAMETER
 * couldn't support. However, explicitly identifying parameters who get their
 * default value from the ParameterSet subclass constructor may help reduce
 * surpise default values - which would happen if the default for PARAMETER were
 * allowed to be overridden in the ParameterSet initializer list
 */
#define PARAMETER_CTORDEFAULT(type, name, doc)                          \
    class _Parameter_##name : public sparta::Parameter<type>              \
    {                                                                   \
    public:                                                             \
        typedef type value_type;                                        \
                                                                        \
        /* construction with no default is disallowed */                \
        _Parameter_##name() = delete;                                   \
                                                                        \
        /* construction with new default value */                       \
        _Parameter_##name(ParameterSet* ps, type default_value) :       \
            sparta::Parameter<type>(#name, default_value, doc)            \
        {                                                               \
            sparta_assert(ps, "Must construct Parameter " #name " with valid ParameterSet"); \
            this->addToSet_(ps);                                        \
        }                                                               \
                                                                        \
        void operator=(const value_type& v) {                           \
            sparta::Parameter<value_type>::operator=(v);                  \
            /*return *this;*/                                           \
        }                                                               \
    };                                                                  \
    _Parameter_##name name; // No construction args suppled. Must be explicitly done by containing class

//! ParameterSet Pretty-Printing stream operator
inline std::ostream& operator<< (std::ostream& out, sparta::ParameterSet const & ps){
    out << ps.stringize(true); // pretty
    return out;
}

inline std::ostream& operator<< (std::ostream& out, sparta::ParameterSet const * ps){
    out << ps->stringize(true); // pretty
    return out;
}
