// <Parameter> -*- C++ -*-


#ifndef __PARAMETER_H__
#define __PARAMETER_H__

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <ios>
#include <iomanip>
#include <string>
#include <ostream>
#include <stdexcept>
#include <vector>
#include <functional>
#include <type_traits>
#include <typeinfo>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/Utils.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/kernel/SpartaHandler.hpp"
#include "sparta/utils/KeyValue.hpp"
#include "sparta/utils/LexicalCast.hpp"
#include "sparta/utils/SmartLexicalCast.hpp"
#include "sparta/utils/Printing.hpp"
#include "sparta/kernel/PhasedObject.hpp"

/*!
 * \file Parameter.hpp
 * \brief Individual Parameter interface base class, container class, and
 * global helpers methods.
 */

namespace sparta
{
    class ParameterBase;
    class ParameterSet;

    /*!
     * \brief smartLexicalCast wrapper with parameter information added to exceptions
     */
    template <typename T>
    inline T smartLexicalCast(const ParameterBase* p,
                              const std::string& s,
                              size_t& end_pos,
                              bool allow_recursion=true,
                              bool allow_prefix=true);

    /*!
     * \brief Exception indicating a misconfigured Parameter or invalid
     * Parameter access.
     */
    class ParameterException : public SpartaException
    {
    public:

        /*!
         * \brief Construct with a default string.
         * \note Stream insertion operator can be used to add more text to the reason.
         */
        ParameterException(const std::string & reason) :
            SpartaException(reason)
        { }

        /*!
         * \brief Destructor
         */
        virtual ~ParameterException() noexcept {}

        /**
         * \brief Wrapper around SpartaException::operator<<
         */
        template<class T>
        ParameterException & operator<<(const T & msg) {
            SpartaException::operator<<(msg);
            return *this;
        }
    };

    /*!
     * \brief Non-templated base class for generic parameter access and
     * iteration.
     * \note This is a pure-virtual non-constructable class
     * \note Parameter type (getTypeName) is invariant for a given Parameter.
     * It is the responsibility of the subclass to enforce this
     * \see sparta::ParameterSet
     * \see sparta::Parameter
     *
     * Allows values to be set and read as strings. Default value and
     * description can also be read as a string. To access value by its
     * internal type, a subclass must be used. For a model to consume
     * parameters, the use of a sparta::ParameterSet subclass is expected.
     * Such as class would allow direct access to parameters in their native
     * types instead of through the generic string-based API provided by
     * ParameterBase.
     *
     * Subclasses are responsible for incremeneting read and write counts
     * when setting or reading the value as its actual type or as a string.
     * Serializing the Parameter does not count as a read. Refer to
     * incrementReadCount_ and incrementWriteCount_. Read count must always be
     * reset to 0 when the parameter is set (whether changed or not) and when
     * initialized.
     */
    class ParameterBase : public TreeNode
    {
    public:

        /*!
         * \brief Tag added to Parameter nodes
         */
        static constexpr char PARAMETER_NODE_TAG[] = "SPARTA_Parameter";

        /*!
         * \brief Generic value iterator for a SINGLE parameter which represents
         * values ONLY as std::string.
         * \note Can be used on any parameter.
         * \note Useful for displaying parameter contents without knowing
         * parameter type.
         * \see sparta::ParameterBase::isVector
         * \see sparta::ParameterBase::begin()
         *
         * For Scalar parameters (including std::string), this itererator will
         * cover exactly 1 element. For vector parameters, this iterator will
         * cover any number of elements.
         *
         * Iterates values in a ParameterBase by index.
         */
        typedef class ParameterValueIterator
        {
        public:

            /*!
             * \brief Full Constructor
             * \param pb ParameterBase whose values will be retrieved by index
             * during iteration
             * \param idx must be > 0. Typically, this should a value
             * representing the beginning or end of iteration such as 0 or the
             * size of the relevant ParameterBase <pb>.
             */
            ParameterValueIterator(const ParameterBase* pb, size_t idx) :
                p_(pb),
                idx_(idx)
            {
                sparta_assert(pb);
            }

            /*!
             * \brief Copy Constructor
             * \param rhp Iterator whose values will be copied
             */
            ParameterValueIterator(const ParameterValueIterator& rhp) :
                p_(rhp.p_),
                idx_(rhp.idx_)
            { }

            /*!
             * \brief Equality test
             * \param rhp Other iterator to compare
             * \return true if iterators are the equal
             *
             * Iterators are considered equal IFF referenced ParameterBases
             * match by address and index matches exactly.
             */
            bool operator==(const ParameterValueIterator& rhp) {
                return (idx_ == rhp.idx_) && (p_ == rhp.p_);
            }

            /*!
             * \brief Inequality test
             * \param rhp Other iterator to compare
             * \see operator==
             * \return true if iterators differ
             *
             * Iterators are compared using logical NOT on operator==
             */
            bool operator!=(const ParameterValueIterator& rhp) {
                return !(*this == rhp);
            }

            /*!
             * \brief Dereference operator
             * \return The value of the parameter currently referenced by the
             * iterator.
             * \throw ParameterException if this iterator is invalid (does not
             * refer to a value within the referenced ParameterBase).
             */
            const std::string operator*() const {
                if(idx_ >= p_->getNumValues()){
                    throw ParameterException("Tried to dereference invalid ParameterBase iterator");
                }
                return p_->getValueAsStringAt(idx_);
            }

            /*!
             * \brief Preincrement operator
             * \return An iterator which is equivalent this incremented by one.
             * \throw ParameterException if this iterator is invalid (does not
             * refer to a value within the referenced ParameterBase).
             */
            const ParameterValueIterator& operator++() {
                if(idx_ >= p_->getNumValues()){
                    throw ParameterException("Tried to increment past end of ParameterBase iterator");
                }
                ++idx_;
                return *this;
            }

            /*!
             * \brief Postincrement operator
             * \return An iterator which is equivalent this, but NOT incremented
             * \throw ParameterException if this iterator is invalid (does not
             * refer to a value within the referenced ParameterBase).
             */
            const ParameterValueIterator operator++(int i){
                (void) i;
                ParameterValueIterator current = *this;
                ++(*this);
                return current;
            }

        private:
            const ParameterBase* p_; //! Parameter referenced by this iterator
            size_t idx_; //! Index into p_. Constrained to [0, number of elements in p_]

        } const_iterator;


        /*!
         * \brief Constructor
         * \param name Name of this Parameter. Available through getName()
         * \param def Default value of this Parameter. Available through
         * getDefaultAsString()
         * \param desc Description of this parameter. Available through
         * getDesc()
         */
        ParameterBase(const std::string & name,
                      const std::string & desc) :
            TreeNode(name, desc),
            modifier_callback_(name.c_str()),
            ignored_(false),
            string_quote_(""),
            name_(name),
            desc_(desc),
            writes_(0),
            reads_(0),
            is_volatile_(false)
        {
            //! \todo Check that parent is a ParameterSet
            addTag(PARAMETER_NODE_TAG); // Tag for quick searching
        }

        /*!
         * \brief Destructor
         */
        virtual ~ParameterBase() {}

        /*!
         * \brief Set volatile flag (allows write after read)
         * \pre Must not be finalized and must not have been read yet
         */
        void setIsVolatile() {
            sparta_assert(getPhase() <= TREE_FINALIZED, "Cannot set volatile state on a Parameter after finalization");
            sparta_assert(getReadCount() == 0, "Cannot set volatile state on a Parameter after it has been read");
            is_volatile_ = true;
        }

        /*!
         * \brief Is this a volatile parmaeter?
         */
        bool isVolatile() const {return is_volatile_;}

        /*!
         * \brief Gets the compiler-independent readable type string of the
         * value currently held.
         */
        virtual const std::string getTypeName() const = 0;

        /*!
         * \brief Gets the default value of this Parameter as a string.
         * \note string value is rendered by the Parameter subclass, which
         * may contain its own sparta::utils::DisplayBase setting.
         * \return String representation of default value of this Parameter. If
         * a number, radix when displayed cannot be predicted.
         */
        virtual std::string getDefaultAsString() const = 0;

        /*!
         * \brief Is this parameter's current value the default value.
         * \return True if value is equal to default, false if not
         * \note Does not consider whether value has been written or not, only
         * current value.
         */
        virtual bool isDefault() const {
            if(getValueAsString() == getDefaultAsString()){
                return true;
            }
            return false;
        }

        /*!
         * \brief Gets the current value of this Parameter as a string.
         * \note string value is rendered by the Parameter subclass, which
         * may contain its own sparta::utils::DisplayBase setting.
         * \return String representation of current value of this Parameter. If
         * a number, radix when displayed cannot be predicted.
         */
        virtual std::string getValueAsString() const = 0;

        /*!
         * \brief Gets the current value of this Parameter as a string at a
         * particular index as if this Parameter were a vector.
         * \param idx Index into parameter of the value to get. If this
         * Parameter is a vector (See isVector()), this value can be in the
         * range [ 0, getNumValues() ). If this Parameter is a scalar, then
         * idx must be 0.
         * \param peek If true, does not increment read count
         * \throw ParameterException if idx does not refer to a valid element
         * for a Vector parameter or 0 for a scalar parameter
         *
         * \note string value is rendered by the Parameter subclass, which
         * may contain its own sparta::utils::DisplayBase setting.
         * \return String representation of an element within the current value
         * of this Parameter. If a number, radix when displayed cannot be
         * predicted.
         */
        virtual std::string getValueAsStringAt(size_t idx, bool peek=false) const = 0;


        /*!
         * \brief Gets the current value of a single element within this
         * parameter of the parameter as if this parameter were a N-dimensional
         * vector.
         * \param indices Index for each dimension of the vector. If this is a
         * 1-dimensional parameter, \a indices should have size 1. If this is a
         * scalar parameter, \a indices should be empty.
         * \param peek. If true, does not increment read count
         * \return String represenation of element located by /a indices
         */
        virtual std::string getItemValueFromString(const std::vector<uint32_t>& indices,
                                                   bool peek=false) const = 0;

        /*!
         * \brief Wrapper for getItemValueFromString with peek=true
         */
        std::string peekItemValueFromString(const std::vector<uint32_t>& indices) const {
            return getItemValueFromString(indices, true);
        }

        /*!
         * \brief Gets the value of this ParameterBase as a templated type T
         * if this parameter actually contains a value of type T.
         * \tparam T Type of parameter to get.
         * \return Value of this parameter as type \a T if \a T is the
         * internally held type of this parameter.
         * \throw ParameterException if this parameter does not actually hold
         * a value of (exactly) type T.
         * \note Type \a t must match exactly. There is no intelligent casting.
         * If the variable stores its value as a uint32_t, the template type
         * \a T must be uint32_t, not uint64_t, int32_t, or string,
         * \warning Uses a dynamic cast. Do not use in performance critical code
         * \see getValueAsString
         * \see getTypeName
         */
        template <class T>
        const T getValueAs() const;

        /*!
         * \brief Determines whether this Parameter is a vector or a scalar
         * Parameter.
         * \return True if type of contained value is an std::vector of any type.
         * \see sparta::is_vector
         */
        virtual bool isVector() const = 0;


        /*!
         * \brief Determines the number of dimensions of this Parameter.
         * A scalar has 0 dimensions. A parameter of type vector<uint32_t> would
         * have a dimensionality of 1. vector<vector<uint32_t>> would have a
         * dimensionality of 2.
         * \note Does not increment read count
         */
        virtual uint32_t getDimensionality() const = 0;

        /*!
         * \brief Determines the size of a vector contained by this parameter
         * at the location specified by \a indices.
         * \param indices Indices into this parameter. The size of indices must
         * be less than the dimensionality of this parameter in order to refer
         * to a vector with a size. If the internal parameter value 'v' is of
         * type vector<vector<vector<int>>>, then indices = {0,1} effectively
         * refers to v[0][1].size(). indices = {0,1,1} would be illegal because
         * v[0][1][1] would refer to an integer, which is not a vector and thus
         * has no vector size.
         * If any index in indices
         * \param peek If true, do not increment the read count
         */
        virtual uint32_t getVectorSizeAt(const std::vector<uint32_t>& indices,
                                         bool peek=false) const = 0;

        /*!
         * \brief Wrapper of getVectorSizeAt with peek=true
         */
        uint32_t peekVectorSizeAt(const std::vector<uint32_t>& indices) const {
            return getVectorSizeAt(indices, true);
        }

        /*!
         * \brief Gets the number of elements contained in this Parameter as a Vector.
         * \param peek Should peek
         * \return Number of elements. For vector Parameters
         * (isVector() == true), this will be the size of the vector. For scalar
         * Parameters, always returns 1.
         * \note Iterating from begin() to end() will cover a number of elements
         * equal to the result of this method.
         * \see isVector
         * \see getValueAsStringAt
         * \see begin
         */
        virtual size_t getNumValues(bool peek=false) const = 0;

        /*!
         * \brief Wrapper for getNumValues with peek=true
         */
        size_t peekNumValues() const {
            return getNumValues(true);
        }

        /*!
         * \brief Gets the value of this parameter as a double.
         * \thwo Exception if underlying parameter type is not numeric.
         */
        virtual double getDoubleValue() const = 0;

        /*!
         * \brief Gets a beginning const_iterator for values of this Parameter.
         * \return An iterator referring to the first element in this Parameter.
         * \note Iterator returned refers to the same value as
         * getValueAsStringAt(0).
         */
        virtual const_iterator begin() const = 0;

        /*!
         * \brief Gets an ending const_iterator for values of this Parameter.
         * \return An iterator referring to the a point after the last element
         * in this Parameter. This iterator cannot be dereferenced without
         * generating an exception.
         */
        virtual const_iterator end() const = 0;


        /*!
         * \brief Has the default value (NOT the current value) for
         * parameter been overridden in any way (including partially changed).
         * \warning This does not provide information about the current value
         * held by the parameter, only the default value.
         *
         * In SPARTA, parameter defaults are supplied at construction-time through
         * constants or constructors, but these defaults can also be changed
         * during initialization in order to support custom architectural
         * configuration baselines. An overridden default value does not
         * necessarily imply a change in the current value held by this
         * parameter. To see if the current value (getValueAsString or
         * Parameter<t>::getValue) has changed, use isDefault (i.e. is this
         * parameter's value equal to the default?)
         */
        virtual bool isDefaultOverridden() const = 0;

        /*!
         * \brief Sets the default value of this non-vector parameter for
         * architecture baseline configuration purposes
         * \param[in] val Value to write to default
         * \pre This parameter is allowed to have been written but must not have
         * been read yet. This parameter must not be a vector
         * \note Does not attempt to resture current value from default.
         * \note Does not increment write count
         * \warning Do not assign new defaults to Parameters without
         * understanding how architecture configurations interact with defaults
         */
        virtual void overrideDefaultFromString(const std::string& val) = 0;

        /*!
         * \brief Sets the default value of this vector parameter for
         * architecture baseline configuration purposes
         * \param[in] val Value to write to default
         * \pre This parameter is allowed to have been written but must not have
         * been read yet. This parameter must not be a vector
         * \note Does not attempt to resture current value from default.
         * \note Does not increment write count
         * \warning Do not assign new defaults to Parameters without
         * understanding how architecture configurations interact with defaults
         */
        virtual void overrideDefaultFromStringVector(const std::vector<std::string>& val) = 0;

        /*!
         * \brief Partially override the default default value in some element
         * at an n-dimensional array specified
         * \pre This parameter is allowed to have been written but must not have
         * been read yet. This parameter must not be a vector
         * \note Does not attempt to resture current value from default.
         */
        virtual void overrideDefaultItemValueFromString(const std::vector<uint32_t>& indices,
                                                        const std::string& str) = 0;

        /*!
         * \brief Override the default value by clearing the possibly-nested
         * vector (if this parameter is a vector).
         * The nested vector contens can then be set by
         * overrideDefaultItemValueFromString
         */
        virtual void overrideDefaultResizeVectorsFromString(const std::vector<uint32_t>& indices) = 0;

        /*!
         * \brief If the parameter is a vector type, clears the default value so
         * that it becomes an empty vector (regardless of dimensionality)
         * \note Has no effect if parameter is a non-vector type
         */
        virtual void overrideDefaultClearVectorValue() = 0;

        /*!
         * \brief Attempts to restore the devalue value of this parameter
         * \pre This parameter must not have been read yet.
         * \post Increments write count
         */
        void restoreValueFromDefault();

        /*!
         * \brief Attempts to assign a value to this non-vector Parameter from
         * a string.
         * \param str String with which to populate this parameter.
         * \param poke If true, does not increment write count.
         * \note Interpretation of the string is handled by the subclass
         * \note Expected to use sparta::utils::smartLexicalCast to perform
         * conversion
         * \post Increments write count if poke is false
         * \throw ParameterException if the given string cannot be lexically
         * converted or if parameter is a vector type (isVector() == true).
         * cast to the internal parameter type.
         */
        void setValueFromString(const std::string& str, bool poke=false);

        /*!
         * \brief Attempts to assign a value to this vector Parameter from a
         * string.
         * \param str Vector of strings with which to populate this Parameter.
         * \param poke If true, does not increment write count.
         * \post Existing elements in the Parameter will be erased
         * \post Parameter will contain vector with elements and size
         * equivalent to str, in the appropriate internal type.
         * \note Interpretation of the string is handled by the subclass
         * \note Expected to use sparta::utils::smartLexicalCast to perform
         * conversions
         * \post Increments write count if poke is false
         * \throw ParameterException if the given string cannot be lexically
         * converted or if parameter is a non-vector type (isVector() == false).
         * cast to the internal parameter type.
         */
        void setValueFromStringVector(const std::vector<std::string>& str, bool poke=false);

        /*!
         * \brief Attempts to assign a value to this nested vector Parameter
         * from a string at a position within the vector indicated by indices.
         * \param indices Set of indices indicating positions in a nested vector
         * parameter type. For example, a parameter of type
         * vector<vector<vector<string>>> with an internal representation
         * referred to as 'p' may have indices [1,2,3] to effectively set \a str
         * at p[1][2][3]. Internal vector will be resized to contain a location
         * as described by \a indices if needed. Values in items created by a
         * resize are undefined. If indices.size() is 0,
         * treats this type as if it is a non-stl::vector parameter and
         * directly assigns the lexically cast value of \a str string to the
         * parameter's internal value
         * \param str String value to assign to the element identified by
         * \a indices. This string will be lexically cast to the approprate
         * internal type as in setValueFromString
         */
        void setItemValueFromString(const std::vector<uint32_t>& indices,
                                    const std::string& str);

        /**
         * \brief Returns true if the value of this equals \p other
         * \param other The parameter to compere with
         */
        virtual bool equals(const ParameterBase &other) = 0;

        /*!
         * \brief Attempt to resize a vector nested within this parameter to
         * contain the vector indicated by indices.
         * \param indices Set of indices referring to nested vectors that must
         * be accessible (at a minimum) by future reads. Nested vetors
         * containing non-vector types will not be resized by this method.
         * See resizeVectorsFromString_ for implementation details
         */
        virtual void resizeVectorsFromString(const std::vector<uint32_t>& indices) = 0;

        /*!
         * \brief If the parameter is a vector type, clears the value so that it
         * becomes an empty vector (regardless of dimensionality)
         * \note Has no effect if parameter is a non-vector type
         */
        virtual void clearVectorValue() = 0;

        /*!
         * \brief Render description of this parameter as a string
         * \note This is not only the value, but also a description of the
         * parameter itself
         * \note Does not increment the read counter for this parameter
         * \return String description of the form
         * \verbatim
         * (<type> "<name>" : <value>, def=<default> read: <read_count>)
         * \endverbatim
         */
        virtual std::string stringize(bool pretty=false) const override {
            (void) pretty;
            std::stringstream ss;
            ss << "[" << TreeNode::stringize(pretty) << "]<param " << getTypeName() << " " << getName() << "="
               << getValueAsString() << ", def=" << getDefaultAsString() << ", write=" << getWriteCount()
               << " read: " << getReadCount() << " ignored: " << isIgnored();
            if(isVolatile()){
                ss << " VOLATILE";
            }
            ss << ">";
            return ss.str();
        }

        /*!
         * \brief Performs validation independently of all other Parameters.
         * \param err_names String which will be appended with names of any
         * failed validators.
         * \return false if any independent validators fail; true otherwise.
         * \post Appends to err_names a comma delimited list of any validators
         * that failed.
         */
        virtual bool validateIndependently(std::string& err_names) const = 0;

        /*!
         * \brief Performs validation based on other Parameters in the Device
         * Tree.
         * \param node TreeNode representing this Device Tree node which is
         * consuming these parameters.
         * \param err_names String which will be appended with names of any
         * failed validators.
         * \return false if any validators fail; true otherwise.
         * \post Appends to err_names a comma delimited list of any validators
         * that failed.
         */
        virtual bool validateDependencies(const TreeNode* node,
                                          std::string& err_names) const = 0;

        /*!
         * \brief Associate a parameter with this parameter for future modification
         * \param params List of parameters that this parameter will modify/reference
         * \param modifier_callback The callback called when this parameter is modifed
         *
         * Example:
         * \code
         * class MyParams : public sparta::ParameterSet
         * {
         * public:
         *    // Called when my_param is written (but not initialized)
         *    void myParamCallback() {
         *        my_other_param = 5;
         *    }
         *
         *    MyParams(...) {
         *        my_param->associateParametersForModification({my_other_param},
         *                                                     CREATE_SPARTA_HANDLER(MyParams, myParamCallback));
         *    }
         *
         * \endcode
         */
        void associateParametersForModification(std::vector<const ParameterBase *> params,
                                                const sparta::SpartaHandler& modifier_callback);

        //! \name Access Counting
        //! @{
        ////////////////////////////////////////////////////////////////////////

        /*!
         * \brief Number of times this Parameter has been written after
         * initialization.
         * \return Number of writes times written.
         *
         * The intent of this method is to help detect when Parameters are being
         * overwritten unexpectedly.
         */
        uint32_t getWriteCount() const {
            return writes_;
        }

        /*!
         * \brief Number of times this Parameter has been read after
         * initialization or after the last write (or explicit resetReadCount_()
         * call)
         * \return Number of writes times read since last write or
         * initialization
         *
         * The intent of this method is to ensure that Parameters are actually
         * read by the Components that defined them and that they are used AFTER
         * writing.
         */
        uint32_t getReadCount() const {
            return reads_;
        }

        /*!
         * \brief Is this parameter ignored or read at least once (barring any
         * reset of the read count or ignore flag)
         */
        bool isReadOrIgnored() const {
            return ignored_ || reads_ > 0;
        }

        /*!
         * \brief Has this parameter been ignored (without having read count
         * reset after)
         */
        bool isIgnored() const {
            return ignored_;
        }

        /*!
         * \brief Scalar parameters *may* compress well, but we cannot really
         * make a strong enough determination without knowing more about the
         * parameter. Let's disable compression for these by default.
         */
        virtual bool supportsCompression() const {
            return false;
        }

        /*!
         * \brief Query if this parameter is safe to be displayed via prints, dumps.
         * A parameter should not be displayed if it is HIDDEN and the subtree containing
         * this parameter is already locked.
         */
        virtual bool isVisibilityAllowed() const = 0;

        /*!
         * \brief Set the quote sequence for printing strings
         *
         * \param s String quote sequence (defaults to empty string)
         *
         * \returns The previous string quote sequence
         */
        std::string setStringQuote(const std::string& s) {
            auto old = string_quote_;
            string_quote_ = s;
            return old;
        }

    protected:

        /*!
         * \brief Implements restoreValueFromDefault
         */
        virtual void restoreValueFromDefaultImpl_() = 0;

        /*!
         * \brief Implements setValueFromString
         */
        virtual void setValueFromStringImpl_(const std::string& s, bool poke=false) = 0;

        /*!
         * \brief Implements setValueFromStringVectorImpl_
         */
        virtual void setValueFromStringVectorImpl_(const std::vector<std::string>& str, bool poke=false) = 0;

        /*!
         * \brief Implements setItemValueFromStringImpl_
         */
        virtual void setItemValueFromStringImpl_(const std::vector<uint32_t>& indices,
                                                 const std::string& str) = 0;

        /*!
         * \brief Mark this parameter as unread and unignored
         * This is used or preloading defaults to parameters and
         * then clearing the write count. It is intended to be used
         * by ParameterSet
         */
        void unread_() const {
            resetReadCount_();
            ignored_ = false;
        }

        /*!
         * \brief Invoke any register callbacks for this parameter.
         * These callbacks are used by Parameters to make
         * modifications to other parameters
         */
        void invokeModifierCB_();

        /*!
         * \brief Increment the number of reads that will be reported by
         * getReadCount.
         * \note Should be called only by a subclass
         */
        void incrementReadCount_() const {
            ++reads_;
        }

        /*!
         * \brief Resets the number of reads that wil be reported by
         * getReadCount.
         * \note Should be called only by a subclass or ParameterSet
         */
        void resetReadCount_() const {
            reads_ = 0;
        }

        /*!
         * \brief Resets the number of writes that wil be reported by
         * getWriteCount.
         * \note Should be called only by a subclass or ParameterSet
         * \warning This should not be necessary except in testing
         */
        void resetWriteCount_() {
            writes_ = 0;
        }

        /*!
         * \brief Flag as ignored. See Parameter<T>::ignore
         */
        void ignore_() const {
            ignored_ = true;
        }

        /*!
         * \brief Ask the simulator if we are using a final config.
         */
        bool usingFinalConfig_();

        /*!
         * \brief Increments the number of writes that will be reported by
         * getWriteCount.
         * \pre Must not have completed the sparta::TreeNode::TREE_CONFIGURING
         * phase.
         * \throw SpartaException if TreeNode base has already passed the
         * TREE_CONFIGURING phase (sparta::TreeNode::isConfigured).
         * \note Should be called only by a subclass
         * \post Write count is incremented
         * \post Read count is reset to 0. Parameters MUST be read after writing
         * or it is considered misuse
         */
        void incrementWriteCount_() {
            // Writing is illegal if the owning node already has a resource.
            // This is not how this SHOULD work - all configuration should be
            // done during configuration. However, dynamically created nodes
            // need to be configured
            //TreeNode* pset = getParent();
            //sparta_assert(//                pset != nullptr, "Cannot write a parameter that does not belong to a ParameterSet");
            //TreeNode* owner = pset->getParent();
            //if(owner){
            //    if(owner->getPhase() > TREE_CONFIGURING
            //       && owner->hasResource()){
            //        throw SpartaException("Cannot write to Parameter ")
            //            << getLocation() << " because the associated TreeNode ("
            //            << owner->getLocation() << ") has already created a resource";
            //    }
            //}

            // Write-after read is prohibited
            if(isVolatile() == false && reads_ > 0){
                throw SpartaException("Cannot write parameter ") << getLocation()
                      << " after reading it unless it is a volatile parameter";
            }

            // Writing is illegal once tree is configured
            if(getPhase() > TREE_FINALIZED){
                throw SpartaException("Cannot write to Parameter ")
                    << getLocation() << " because it is already finalized";
            }

            resetReadCount_();
            ++writes_;
        }

        /*!
         * \brief Log the default loaded to this parameter to the global
         * parameters logger for debugging
         */
        void logLoadedDefaultValue_() const;

        /*!
         * \brief Log the most recently assigned value given to this parameter
         * to the global parameters logger for debugging
         */
        void logAssignedValue_() const;

        /*!
         * \brief Log the current backtrace to the global parameters logger
         */
        static void logCurrentBackTrace_();

        /*!
         * \brief Add this parameter to a set - an action which is protected
         * and requires the friendship that the Parameter<> class has
         * \param[in] ps ParameterSet to add self to
         * \pre Must have be added to a ParameterSet already
         */
        void addToSet_(ParameterSet* ps);

        /*!
         * Allow access to protected access count methods by ParameterSet
         */
        friend class ParameterSet;

        /*!
         * \brief Modifier callback called when the parameter is written
         */
        sparta::SpartaHandler modifier_callback_;

        /*!
         * \brief Has this parameter been ignored. Resettable. Mutable so that
         * it can be accessed from a const Parmeter
         */
        mutable bool ignored_;

        /*!
         * \brief The quote sequence for printing strings.  Defaults to empty string.
         */
        std::string string_quote_;

    private:

        /*!
         * \brief Name of this parameter
         */
        std::string name_;

        /*!
         * \brief Description of this parameter
         */
        std::string desc_;

        /*!
         * \brief Number of times written. Resettable.
         */
        uint32_t writes_;

        /*!
         * \brief Number of times read. Resettable. Mutable so that it can be
         * accesed from a const Parameter
         */
        mutable uint32_t reads_;

        /*!
         * \brief Parameters to be modified/associated with this parameter
         */
        std::vector<const ParameterBase *> associated_params_;

        /*!
         * \brief Is this a volatile parameter (allows write after read)
         */
        bool is_volatile_;

        ////////////////////////////////////////////////////////////////////////
        //! @}

    }; // class ParameterBase

    /*!
     * \brief Delegate for Parameter validation
     *
     * Serves as a delegate to arbitrary class member, static member, or
     * global method to perform tests on a given
     *
     * Valid class member function and global/static functions must be of one
     * of the forms:
     * \li bool T::TMethod(ValueType, const TreeNode*) // member function
     * \li bool method(ValueType) // static/global
     *
     * Invoke the delegate using operator().
     *
     * This class is copyable through construction, but not assignable.
     *
     */
    template<class ValueType>
    class ValidationCheckCallback
    {
        //! Callback signature. All callback functions, whether member functions or must match this.
        std::function<bool(ValueType&, const sparta::TreeNode*)> callback_;

        std::string name_;

    public:

        template<class T, bool (T::*TMethod)(ValueType&, const sparta::TreeNode*)>
        static ValidationCheckCallback<ValueType> from_method(T* obj,
                                                              const std::string& name)
        {
            (void) obj;
            ValidationCheckCallback<ValueType> vcc(name);
            vcc.callback_ = std::bind(TMethod, obj, std::placeholders::_1, std::placeholders::_2);
            return vcc;

            // This is not valid because the template parameter TMethod cannot be deduced
            //return ValidationCheckCallback<ValueType>(obj, name);
        }

        /*!
         * \brief Construct delegate with a class member pointer
         * \tparam T class type containing a member to which this delegate will
         * point.
         * \tparam TMethod Member function of T which will be invoked by this
         * delegate.
         * \param obj Object of templated class T on which this delegate will
         * make its callback.
         * \param name Name of this validator. This should be an recognizable
         * name in the context of a Parameter so that failure of this dependency
         * are obvious. There are no name limitations except that it must not
         * contain a comma.
         */
        /*
        template<class T, bool (T::*TMethod)(ValueType)>
        ValidationCheckCallback(T* obj, const std::string& name) :
            callback_(std::bind(TMethod, obj, std::placeholders::_1)),
            name_(name)
        {
            validateName(name_);
        }*/

        //! Construct with a static method or normal function pointer
        ValidationCheckCallback(bool (*method)(ValueType&, const sparta::TreeNode*),
                                const std::string& name) :
            callback_(std::bind(method, std::placeholders::_1, std::placeholders::_2)),
            name_(name)
        {
            validateName(name_);
        }

        //! Construct with no functionality.
        ValidationCheckCallback() :
            callback_(std::bind(&ValidationCheckCallback<ValueType>::doNothing_, std::placeholders::_1, std::placeholders::_2)),
            name_("<uninitialized>")
        {
            validateName(name_);
        }

        //! Construct with no functionality and a name
        ValidationCheckCallback(const std::string& name) :
            callback_(std::bind(&ValidationCheckCallback<ValueType>::doNothing_, std::placeholders::_1, std::placeholders::_2)),
            name_(name)
        {
            validateName(name_);
        }

        //! Copy Constructor
        ValidationCheckCallback(const ValidationCheckCallback<ValueType>& rhp) :
            callback_(rhp.callback_),
            name_(rhp.name_)
        {
            validateName(name_);
        }

        void operator=(const ValidationCheckCallback<ValueType>& rhp)
        {
            callback_ = rhp.callback_;
            name_ = rhp.name_;
        }

        std::string getName() const
        {
            return name_;
        }

        //! Invoke callback to check given value at the indicated position in the device tree
        bool operator()(ValueType& val, const TreeNode* node) const
        {
            return callback_(val, node);
        }

        //! Validates the given name string for this ValidationCheckCallback
        //! \throw ParameterException if the name is invalid.
        static void validateName(const std::string& nm)
        {
            if(nm.find(',') != std::string::npos){
                ParameterException ex("ValidationCheckCallback name \"");
                ex << nm << "\" contains a comma, which is not permitted";
                throw ex;
            }
        }

    private:

        //! Returns True. Used internally to construct with default behavior.
        static bool doNothing_(ValueType& val, const TreeNode* node)
        {
            (void) val;
            (void) node;
            return true;
        }
    };

    /*!
     * \brief Parameter instance, templated to contain only a specific type.
     *
     * \tparam ValueType type of data held by this parameter
     * Supported types include are all bound types of
     * sparta::KeyValue::GBL_type_name_map contained within any number of
     * std::vector levels
     */
    template <typename ValueType>
    class Parameter : public ParameterBase
    {
    public:

        /*!
         * \brief ParameterAttribute enum class which describes special attributes
         * of this parameter.
         */
        enum class ParameterAttribute : std::uint8_t {
            DEFAULT = 0,
            __FIRST = DEFAULT,
            LOCKED = 1,
            HIDDEN = 2,
            __LAST
        };

    private:
        OneWayBool<false> default_override_; //!< Has this parameter's default been overridden
        ParameterAttribute param_attr_ {ParameterAttribute::DEFAULT}; //!< Attribute for special status of LOCKED, HIDDEN
        ValueType def_val_; //!< Default value
        ValueType val_; //!< Current value
        sparta::utils::DisplayBase disp_base_; //!< Display base used when rendering current or default value as a string
        std::vector<ValidationCheckCallback<ValueType> > bounds_; //!< Validation methods which operate only on the current value
        std::vector<ValidationCheckCallback<ValueType> > dependencies_; //!< Validations methods to call which require tree information

    public:
        //! Type held by this parameter. This cannot change at run-time.
        using type = ValueType;
        using value_type = ValueType; // only for passing regression

        /*!
         * \brief Construct a parameter
         * \note Within a ParameterSet subclass body, it's usually preferable to
         * use PARAMETER* macros instead of constructing these manually.
         * \param[in] name Param/Node name
         * \param[in] def Default value
         * \param[in] doc Docstring
         * \param[in] isvolatile Is this parameter volatile (are writes after
         * read allowed)? A param can never be written after finalization, but
         * some may be automatically calculated and changed multiple times based
         * on other values and their own.
         */
        Parameter(const std::string& name,
                  const ValueType& def,
                  const std::string& doc,
                  bool isvolatile = false) :
            ParameterBase(name, doc),
            def_val_(def),
            val_(def),
            disp_base_(sparta::utils::BASE_DEC)
        {
            //std::cout << name << " = " << typeid(ValueType).name() << std::endl;

            logLoadedDefaultValue_();

            if(isvolatile){
                setIsVolatile();
            }
        }

        /*!
         * \brief Constructor used by the PARAMETER macro.
         * \note This constructor delegates to the previous one defined above, and
         * adds to the right ParameterSet object.
         */
        Parameter(const std::string& name,
                  const ValueType& def,
                  const std::string& doc,
                  ParameterSet * ps,
                  bool isvolatile = false) :
            Parameter (name, def, doc, isvolatile)
        {
            sparta_assert(ps, "Must construct parameter " << name << " with valid ParameterSet");
            addToSet_(ps);
        }

        Parameter(const std::string& name,
                  const ValueType& def,
                  const std::string& doc,
                  const ParameterAttribute& attr,
                  ParameterSet* ps,
                  bool isvolatile = false) :
            Parameter(name, def, doc, isvolatile)
        {
            sparta_assert(ps, "Must construct parameter " << name << " with valid ParameterSet");
            param_attr_ = attr;
            addToSet_(ps);
        }

        //! Destructor
        virtual ~Parameter() = default;

        /*!
         * \brief Adds dependency callback to a class member function
         *
         * Usage:
         * \code
         * class MyClass {
         * public:
         *     bool Method(bool&, const sparta::TreeNode*) { return true; }
         * };
         * // ...
         * MyClass my_class;
         * // ...
         * // Given some Parameter<bool> parameter;
         * // ...
         * parameter->addDependentValidationCallback<MyClass, &MyClass::Method>(my_class, "constraint1");
         * \endcode
         */
        template<class T, bool (T::*TMethod)(ValueType&, const sparta::TreeNode*)>
        void addDependentValidationCallback(T* obj, const std::string& name)
        {
            dependencies_.push_back(ValidationCheckCallback<ValueType>::template from_method<T, TMethod>(obj, name));

            // Not allowed: Cannot resolve template arguments
            //dependencies_.push_back(ValidationCheckCallback<ValueType>(obj, name));
        }

        /*!
         * \brief Adds dependency callback via a global function or lambda
         *
         * Refer to other overload of addDependentValidationCallback for further
         * explanation
         *
         * Usage:
         * \code
         * bool global_function(bool&, const sparta::TreeNode*) { return true; }
         * // ...
         * MyClass my_class;
         * // ...
         * // Given some Parameter<bool> parameter_global_check;
         * // Given some Parameter<bool> parameter_lambda_check;
         * // ...
         * parameter_global_check->addDependentValidationCallback(&global_function, "constraint1")
         * parameter_lambda_check->addDependentValidationCallback([](std::vector<bool>& val, const sparta::TreeNode*){return val == true;},
         *                                                        "constraint2")
         * \endcode
         */
        void addDependentValidationCallback(bool (*method)(ValueType&, const sparta::TreeNode*),
                                            const std::string& name)
        {
            dependencies_.push_back(ValidationCheckCallback<ValueType>(method, name));
        }


        /*!
         * \brief Gets the human-readable name of this parameter's type.
         * \see sparta::ParameterBase::getTypeName
         */
        virtual const std::string getTypeName() const override final{
            return Parameter::template getTypeName_<ValueType>();
        }

        /*! \brief Returns the default value
         */
        ValueType getDefault() const {
            return def_val_;
        }

        /*!
         * \brief Returns the default value as a string, even if type is a
         * vector.
         *
         * Refer to 'sparta::stringize' for vector-to-string formatting.
         */
        virtual std::string getDefaultAsString() const override final {
            return sparta::utils::stringize_value(def_val_, disp_base_, string_quote_);
        }

        /*!
         * \brief Returns value as a string, even if type is a vector.
         * \return string representation of current value
         * \note does not increment read counter
         *
         * Refer to 'sparta::stringize' for vector-to-string formatting.
         */
        std::string getValueAsString() const override final {
            return sparta::utils::stringize_value(val_, disp_base_, string_quote_);
        }

        /*!
         * \brief Treats this parameter as a vector and gets the value as a
         * string at a specific index. If parameter is not a vector, it is
         * treated as a 1-element vector.
         * \throw SpartaException if idx is out of bounds
         * \warning peek is highly discouraged outside of framework use since it
         * undermines SPARTA's protection for parameter write-after-reads
         */
        std::string getValueAsStringAt(size_t idx, bool peek=false) const override final {
            return Parameter::template getValueAsStringAt_<ValueType, ValueType>(idx, peek);
        }

        /*!
         * \brief Override from ParameterBase
         * \warning peek is highly discouraged outside of framework use since it
         * undermines SPARTA's protection for parameter write-after-reads
         */
        std::string getItemValueFromString(const std::vector<uint32_t>& indices,
                                           bool peek=false) const override final {
            if(indices.size() == 0){
                // No indices given. This must be a non-vector type
                return getFinalVectorItemValueFromString_(val_, indices, 0, peek);
            }

            // Handle if this is a vector
            return getVectorItemValueFromString_(val_, indices, 0, peek);
        }

        /*!
         * \brief Get the size of a nested vector within the parameter located
         * by indices
         * \warning peek is highly discouraged outside of framework use since it
         * undermines SPARTA's protection for parameter write-after-reads
         */
        uint32_t getVectorSizeAt(const std::vector<uint32_t>& indices,
                                 bool peek=false) const override final {
            if(indices.size() == 0){
                // No indices given. This must be a non-vector type
                return getFinalVectorSize_(val_, indices, 0, peek);
            }

            // Handle if this is a vector
            return getVectorSize_(val_, indices, 0, peek);
        }

        /*!
         * \brief Gets a the value currently held by this Parameter
         * \return Value currenty held by this Parameter
         */
        operator const ValueType&() const {
            return getValue();
        }

        /*!
         * \brief Gets the value currently held by this Parameter
         * \return Value currently held by this Parameter
         */
        const ValueType& operator()() const {
            return getValue();
        }

        /*!
         * \brief Marks this parameter as ignored
         * \note This method is intentionally omitted from ParameterBase so that
         * only components consuming parameters can choose to ignore.
         * \warning Read counts must may be reset by sparta infrastructure so this
         * method may have no effect if called within a ParameterSet
         * constructor. The point of this behavior is to prevent early reads of
         * the parameter value from appearing as if the Resource that owns
         * this parameter read it.
         * \see unread
         *
         * When instantiating a Resource, every parameter must be either read
         * (e.g. through getValue, operator ValueType, operator==), or ignored
         * with this method.
         */
        void ignore() const {
            ignore_();
        }

        /*!
         * \brief Mark this parameter as unread and unignored
         * \note This method is intentionally omitted from ParameterBase so that
         * only components consuming parameters can choose to ignore.
         * \see ignore
         *
         * This is useful within a resource constructor which first validates
         * parameters and then uses them. The validation would be considered
         * a read (unless peekValue) were used. Also clears ignored_flag
         */
        void unread() const {
            unread_();
        }

        /*!
         * \brief Gets the current value of this Parameter.
         * \return current value of parameter.
         * \post Increments reference count
         */
        const ValueType& getValue() const {
            incrementReadCount_();
            return getValue_();
        }

        /*!
         * \brief Gets the current value of this Parameter without incrementing
         * the read count. This should be used when validating parameters in a
         * Resource Constructor.
         * \return current value of parameter.
         * \warning peek is highly discouraged outside of framework use since it
         * undermines SPARTA's protection for parameter write-after-reads. If
         * using this command for a model without a thorough understanding tree
         * construction phases and parameter application, you may experience
         * unexpected values for your parameters or read parameters that differ
         * from the final assigned parameter.
         */
        const ValueType& peekValue() const {
            return getValue_();
        }

        /*!
         * \brief Cast value to double if possible. Throw if not
         */
        double getDoubleValue() const override final {
            return getDoubleValue_<ValueType>();
        }

        /*!
         * \brief Gets the number of values in this Parameter.
         * \return Number of values in this parameter if a vector. If not a
         * vector, returns 1.
         * \post Increments reference count if this parameter is a vector type
         * (see isVector). Otherwise, has no effect.
         */
        virtual size_t getNumValues(bool peek=false) const override final {
            return Parameter::template getNumValues_<ValueType, ValueType>(peek);
        }

        /*!
         * \brief Is this parameter a vector?
         * \note THis is an invariant and does not increment the read count
         */
        virtual bool isVector() const override final {
            return is_vector<ValueType>::value;
        }

        // Implements ParameterBase::getDimensionality
        virtual uint32_t getDimensionality() const override final {
            return Dimensionality<ValueType>::value;
        }

        /*!
         * \brief Get begin iterator
         * \note Increments read count
         */
        virtual ParameterBase::const_iterator begin() const override final{
            return ParameterBase::const_iterator(this, 0);
        }

        /*!
         * \brief Get begin iterator
         * \note Increments read count
         */
        virtual ParameterBase::const_iterator end() const override final{
            return ParameterBase::const_iterator(this, getNumValues());
        }

        /*!
         * \brief Compares two Parameter objects by value
         * \post Increments read count
         */
        template <class T>
        bool operator==(const Parameter<T>& rhp) const {
            return getValue() == rhp.getValue();
        }

        /*!
         * \brief Compares two Parameter objects by value
         * \post Increments read count
         */
        template <class T>
        bool operator!=(const Parameter<T>& rhp) const {
            return getValue() != rhp.getValue();
        }

        /*!
         * \brief Compares two Parameter objects by value
         * \post Increments read count
         */
        bool operator>(const Parameter<type>& rhp) const {
            return getValue() > rhp.getValue();
        }

        /*!
         * \brief Compares two Parameter objects by value
         * \post Increments read count
         */
        template <class T>
        bool operator>=(const Parameter<T>& rhp) const {
            return getValue() >= rhp.getValue();
        }

        /*!
         * \brief Compares two Parameter objects by value
         * \post Increments read count
         */
        bool operator<(const Parameter<type>& rhp) const {
            return getValue() < rhp.getValue();
        }

        /*!
         * \brief Compares two Parameter objects by value
         * \post Increments read count
         */
        bool operator<=(const Parameter<type>& rhp) const {
            return getValue() <= rhp.getValue();
        }

        //! Increments read count
        template <class T>
        typename std::enable_if<!std::is_base_of<ParameterBase, T>::value, bool>::type
        operator==(const T rhp) const {
            return getValue() == (ValueType)rhp;
        }

        //! Increments read count
        template <class T>
        typename std::enable_if<!std::is_base_of<ParameterBase, T>::value, bool>::type
        operator!=(const T rhp) const {
            return getValue() != (ValueType)rhp;
        }

        //! Increments read count
        template <class T>
        typename std::enable_if<!std::is_base_of<ParameterBase, T>::value, bool>::type
        operator>(const T rhp) const {
            return getValue() > (ValueType)rhp;
        }

        //! Increments read count
        template <class T>
        typename std::enable_if<!std::is_base_of<ParameterBase, T>::value, bool>::type
        operator>=(const T rhp) const {
            return getValue() >= (ValueType)rhp;
        }

        //! Increments read count
        template <class T>
        typename std::enable_if<!std::is_base_of<ParameterBase, T>::value, bool>::type
        operator<(const T rhp) const {
            return getValue() < (ValueType)rhp;
        }

        //! Increments read count
        template <class T>
        typename std::enable_if<!std::is_base_of<ParameterBase, T>::value, bool>::type
        operator<=(const T rhp) const {
            return getValue() <= (ValueType)rhp;
        }

        /*!
         * \brief Copy assignment - deleted explicitly.
         * \note This explicit delete of the copy assignment operator
         *       is to work around cases where we have one parameter A
         *       getting its value at run time from another parameter
         *       B, using the simple syntax A = B.  The issue with
         *       this is that it's rather ambiguous whether this
         *       syntax attempts to copy a parameter value into A from
         *       B, or copy the entire Parameter object. Hence,
         *       explicitly deleted. Use getValue() calls for value
         *       copy.
         */
        void operator=(const Parameter& p) = delete;

        /*!
         * \brief Assigns the specified value to this parameter
         * \todo Perform independent parameter validation and throw exception
         * if failed.
         * \post Write count is incremented and read count reset
         */
        void operator=(const ValueType& v) {
            checkModificationPermission_();
            // if the simulator was setup using --read-final-config
            // we do not allow the simulator to override values
            // so just return out and warn that we are skipping this.
            if (usingFinalConfig_())
            {
                static bool been_warned = false;
                if(!been_warned) {
                    std::cout << "WARNING: A simulator override for parameter " << getLocation()
                              << " (using operator=) was performed and ignored. This is because"
                              << " the simulator is using --read-final-config.  This is your"
                              << " first and last warning." << std::endl;
                    been_warned = true;
                }
                return;
            }

            incrementWriteCount_();
            ValueType old_val = val_;
            val_ = v;
            logAssignedValue_();
            try {
                invokeModifierCB_();
            }
            catch(sparta::SpartaException & e) {
                val_ = old_val;
                throw;
            }
        }

        virtual bool isDefaultOverridden() const override final {
            return default_override_;
        }

        virtual void overrideDefaultFromString(const std::string& str) override final {
            Parameter::template overrideDefaultFromString_<ValueType, ValueType>(str);
            sparta_assert(getReadCount() == 0,
                        "Cannot override default on parameter if read count is > 0. "
                        "Problem on parameter " << getLocation());
            default_override_ = true;
        }

        virtual void overrideDefaultFromStringVector(const std::vector<std::string>& vec) override final {
            Parameter::template overrideDefaultFromStringVector_<ValueType, ValueType>(vec);
            sparta_assert(getReadCount() == 0,
                        "Cannot override default on parameter if read count is > 0. "
                        "Problem on parameter " << getLocation());
            default_override_ = true;
        }

        virtual void overrideDefaultItemValueFromString(const std::vector<uint32_t>& indices,
                                                        const std::string& str) override final {
            if(indices.size() == 0){
                // No indices given. This must be a non-vector type
                Parameter::template overrideDefaultFromString_<ValueType, ValueType>(str);
            }else{
                // handle if this is a vector
                const bool incr_write_count = false;
                setVectorItemValueFromString_(def_val_, indices, 0, str, incr_write_count);
            }
            default_override_ = true;
        }

        virtual void overrideDefaultResizeVectorsFromString(const std::vector<uint32_t>& indices) override final {
            if(indices.size() == 0){
                return; // No effect
            }

            resizeVectorsFromString_(def_val_, indices, 0);
        }

        virtual void overrideDefaultClearVectorValue() override final {
            clearVectorValue_(def_val_);
        }


        virtual void restoreValueFromDefaultImpl_() override final {
            checkModificationPermission_();
            sparta_assert(getReadCount() == 0,
                        "Parameter " << getLocation() << " must not have been read when restoring "
                        "a value from the default. This is a write");

            incrementWriteCount_();

            val_ = def_val_;
        }

        virtual void setValueFromStringImpl_(const std::string& str, bool poke=false) override final {
            Parameter::template setValueFromString_<ValueType, ValueType>(str, poke);
        }

        virtual void setValueFromStringVectorImpl_(const std::vector<std::string>& vec, bool poke=false) override final {
            Parameter::template setValueFromStringVector_<ValueType, ValueType>(vec, poke);
        }

        virtual void setItemValueFromStringImpl_(const std::vector<uint32_t>& indices,
                                                 const std::string& str) override final {
            if(indices.size() == 0){
                // No indices given. This must be a non-vector type
                Parameter::template setValueFromString_<ValueType, ValueType>(str);
            }else{
                // handle if this is a vector
                const bool incr_write_count = true;
                setVectorItemValueFromString_(val_, indices, 0, str, incr_write_count);
            }
        }

        virtual bool equals(const ParameterBase &other) override final {
            return *this == dynamic_cast<const Parameter<ValueType> &>(other);
        }

        virtual void resizeVectorsFromString(const std::vector<uint32_t>& indices) override final {
            if(indices.size() == 0){
                return; // No effect
            }

            resizeVectorsFromString_(val_, indices, 0);
        }

        virtual void clearVectorValue() override final {
            clearVectorValue_(val_);
        }

        /*!
         * \brief Set the numeric base for displaying the value of this
         * parameter
         */
        sparta::utils::DisplayBase setNumericDisplayBase(sparta::utils::DisplayBase base) {
            checkModificationPermission_();
            sparta::utils::DisplayBase old = disp_base_;
            disp_base_ = base;
            return old;
        }

        /*!
         * \brief Gets the numeric base for displaying the value of this
         * parameter
         */
        sparta::utils::DisplayBase getNumericDisplayBase() const {
            return disp_base_;
        }

        bool validateIndependently(std::string& err_names) const override
        {
            (void) err_names;

            // \todo Implement independent validation
            //sparta_assert(0, "sparta::Parameter::validateIndependently is unimplemented");

            return true;
        }

        /*!
         * \brief Invokes all validation callbacks for a particular node in the
         * device tree and returns true if none of them fail.
         * \param node TreeNode in device tree associated with the object for
         * which these parameters are being validated.
         *
         * \param err_names string which will be appended
         * with the names of any ValidationCheckCallbacks that failed in a
         * comma-delimited list.
         */
        bool validateDependencies(const TreeNode* node, std::string& err_names) const override
        {
            bool success = true;
            ValueType val = getValue_(); // Does not count as a read
            for(const ValidationCheckCallback<ValueType>& vcb : dependencies_){
                if(!vcb(val, node)){
                    err_names += vcb.getName() + ",";
                    success = false;
                }
            }
            return success;
        }

        /*!
         * \brief Helper for constructing vectors
         * \note Does not impact getWriteCount.
         * \warning This should be used for initializing parameters only.
         * This method is currently public so that distant subclasses can access it.
         * \post Write count is incremented and read count reset
         *
         * Allows:
         * \code
         * param << 1 << 2 << 3;
         * \endcode
         */
        template <class U>
        Parameter<ValueType>& operator<< (U e) {
            return Parameter::template operator_insert_<U, ValueType>(e);
        }


        template <class U, class C1>
        typename std::enable_if<is_vector<C1>::value, Parameter<ValueType>&>::type
        operator_insert_(U e) {
            checkModificationPermission_();
            incrementWriteCount_();
            val_.push_back((typename ValueType::value_type) e);
            return *this;
        }

        /*!
         * \brief Query if this parameter is safe to be displayed via prints, dumps.
         * A parameter should not be displayed if it is HIDDEN and the subtree containing
         * this parameter is already locked.
         */
        virtual bool isVisibilityAllowed() const override{
            return ((param_attr_ == ParameterAttribute::HIDDEN) and
                    (this->areParametersLocked_())) ? false : true;
        }
    protected:

        //! Iternal getValue_ wrapper which does not increment the read counter
        const ValueType& getValue_() const {
            return val_;
        }

        template <typename T>
        typename std::enable_if<std::is_arithmetic<T>::value, double>::type
        getDoubleValue_() const {
            return val_;
        }

        template <typename T>
        typename std::enable_if<!std::is_arithmetic<T>::value, double>::type
        getDoubleValue_() const {
            throw SpartaException("Cannot get 'double' type value from parameter ")
                  << getLocation() << " which is of type " << getTypeName();
        }

    private:
        /*!
         * \brief Query if this parameter is LOCKED or HIDDEN and if the subtree
         * in which this node is rooted is already locked or not.
         */
        void checkModificationPermission_() const{
            if(this->areParametersLocked_() and
                param_attr_ != ParameterAttribute::DEFAULT){
                throw ParameterException("Modifying special parameters after Lockdown phase is disallowed.");
            }
        }

        // Multi-dimensional parameter indexed writes

        /*!
         * \brief Recursive lookup of an element within of a N-dimensional
         * vector type (e.g. vector<vector<vector<pod_type>>>) based on an input
         * \a indices vector. When the final element is found (or created if not
         * yet in existance) based on the given indices for each dimension,
         * it is assigned the lexically cast value of \a str
         * \param vec vector, possibly containing vectors or a pod type (or
         * string). An element in this vector will be selected based on
         * indices[index_level]. Then a value will be assigned to that element
         * if it is a pod type (or string), or this method will be recurisvely
         * called on a vector element of \a vec.
         * \param indices indexes for each dimension of data type being updated.
         * \param index_level index into \a indices representing the current
         * element of \a indices to use as an index into \a vec
         * \param str string to assign to element identified by \a indices
         * \param increment_write_count Should the write count be incremented
         * when finally assigning the value at the end of recursion
         * when found.
         */
        template <typename T>
        typename std::enable_if<is_vector<typename std::remove_reference<T>::type>::value>::type
        setVectorItemValueFromString_(T& vec,
                                      const std::vector<uint32_t>& indices,
                                      uint32_t index_level,
                                      const std::string& str,
                                      bool increment_write_count) {
            checkModificationPermission_();
            assert(indices.size() > 0);

            uint32_t idx = indices.at(index_level);
            if(idx >= vec.size()){
                vec.resize(idx+1);
            }

            if(indices.size() - 1 == index_level){
                // Final index should place a value in a vector
                // Note that vec is a vector
                typename T::reference to_set = vec.at(idx);
                setFinalVectorItemValueFromString_(to_set, indices, index_level, str);
                if(increment_write_count){
                    incrementWriteCount_();
                }
            }else{
                // Non-final index should select a vector and recurse until the
                // last level of nested vectors is reached
                typename T::reference next_vec = vec.at(idx);
                setVectorItemValueFromString_(next_vec, indices, index_level+1, str, increment_write_count);
            }
        }

        /*!
         * \brief Error case for setVectorItemValueFromString_. Selected by
         * template deduction when setVectorItemValueFromString_ or
         * setItemValueFromString is invoked with a type which is not a vector.
         */
        template <typename T>
        typename std::enable_if<!is_vector<typename std::remove_reference<T>::type>::value>::type
        setVectorItemValueFromString_(T& vec,
                                      const std::vector<uint32_t>& indices,
                                      uint32_t index_level,
                                      const std::string& str,
                                      bool increment_write_count) {
            checkModificationPermission_();
            (void) vec;
            (void) index_level;
            (void) str;
            (void) increment_write_count;

            throw ParameterException("Cannot set value from string on parameter \"")
                << getName() << "\" which is of type \""
                << getTypeName() << "\" with indices: " << indices << " (" << indices.size()
                << " levels)" << " because this type only has " << index_level << " dimensions";
        }

        /*!
         * \brief Error case for setFinalVectorItemValueFromString_. Selected by
         * template deduction when setVectorItemValueFromString_ attempts to
         * assign a string value to a vector (because too few indices were
         * given).
         */
        template <typename T>
        typename std::enable_if<is_vector<typename std::remove_reference<T>::type>::value>::type
        setFinalVectorItemValueFromString_(T& to_set,
                                           const std::vector<uint32_t>& indices,
                                           uint32_t index_level,
                                           const std::string& str) {
            checkModificationPermission_();
            (void) to_set;
            (void) str;

            throw ParameterException("Cannot set value from string on parameter \"")
                << getName() << "\" which is of type \""
                << getTypeName() << "\" with indices: " << indices << " (" << indices.size()
                << " levels)" << " because this type has more than " << index_level << " dimensions";
        }

        /*!
         * \brief Sets value of an item by reference to the lexically cast value
         * of an input string. This is the termination of any
         * setVectorItemValueFromString_ call chain that refers to a valid
         * non-vector item.
         * \tparam T type of value to set. Expected to be a non-vector type
         * supporting assignment and having an associated lexicalCast to covnert
         * a string to type \a T.
         * \param indices Indices from initial setValueFromStringVector call
         * for error printout if needed
         * \param index_level index into indices list for error printout if
         * needed
         */
        template <typename T>
        typename std::enable_if<!is_vector<typename std::remove_reference<T>::type>::value>::type
        setFinalVectorItemValueFromString_(T& to_set,
                                           const std::vector<uint32_t>& indices,
                                           uint32_t index_level,
                                           const std::string& str) {
            checkModificationPermission_();
            assert(indices.size() == 0 || indices.size() - 1 == index_level);

            size_t end_pos;
            to_set = smartLexicalCast<typename bit_reference_to_bool<T>::type>(this, str, end_pos);
        }


        // Multi-dimensional parameter indexed enlargement

        /*!
         * \brief Recursive elements within an N-dimensional
         * vector type (e.g. vector<vector<vector<pod_type>>>) based on an input
         * \a indices vector.
         * \post Parameter will contain vectors as indices described.
         * \param indices indexes for each dimension of data type being
         * enlarged. Each index can be treated as the new (minimum size-1) of an
         * element in the N-dimensional parameter. For example, indices={0,2,1}
         * would result in a 4-dimensional parameter containing at least
         * {{}, {}, { {}, {} }}. That is, the effective parameter 'p' has
         * p.size() = 1, p[0].size() = 3, and p[0][2].size() = 2.
         * \param index_level index into \a indices representing the current
         * element of \a indices to use as an index into \a vec
         * \param str string to assign to element identified by \a indices
         * when found.
         */
        template <typename T>
        typename std::enable_if<is_vector<typename std::remove_reference<T>::type>::value>::type
        resizeVectorsFromString_(T& vec,
                                 const std::vector<uint32_t>& indices,
                                 uint32_t index_level) {
            checkModificationPermission_();
            if(index_level == indices.size()){
                return; // Done. No further effect
            }

            uint32_t idx = indices.at(index_level);
            if(idx >= vec.size()){
                vec.resize(idx+1);
            }

            if(index_level < indices.size()){
                // Recurs to next index_level
                typename T::reference next_vec = vec.at(idx);
                resizeVectorsFromString_(next_vec, indices, index_level+1);
            }
        }

        /*!
         * \brief Error case for resizeVectorsFromString_. Selected by
         * template deduction when resizeVectorsFromString_ or
         * resizeVectorsFromString is invoked with a type which is not a vector.
         * This can happen when indices contains too many elements.
         */
        template <typename T>
        typename std::enable_if<!is_vector<typename std::remove_reference<T>::type>::value>::type
        resizeVectorsFromString_(T& vec,
                                  const std::vector<uint32_t>& indices,
                                  uint32_t index_level) {
            checkModificationPermission_();
            (void) vec;
            (void) index_level;

            throw ParameterException("Cannot resize a vector in parameter \"")
                << getName() << "\" which is of type \""
                << getTypeName() << "\" to contain indices: " << indices << " (" << indices.size()
                << " levels)" << " because this type only has " << getDimensionality()
                << " dimensions. Therefore this index "
                   "would be within a vector of scalars and this method has no idea with what "
                   "value to initialize the new elements of said vector. Ues an indices vector "
                   "with less than " << getDimensionality() << " elements";
        }

        // Vector Clearing

        /*!
         * \brief Clear the argument if it is a vector. When updating  multi-dim vectors, this helps
         * clear the old values when changing vector sizes
         * \param vec Vector argument
         * \tparam T Type of argument. THis method is enabled when T is a
         * std::vector of some type
         */
        template <typename T>
        static
        typename std::enable_if<is_vector<typename std::remove_reference<T>::type>::value>::type
        clearVectorValue_(T& vec) {
            vec.clear();
        }

        /*!
         * \brief ignored base for clearVectorValue_
         * \param vec dummy argument
         * \tparam T Type of argument. THis method is enabled when T is NOT a
         * std::vector of some type
         */
        template <typename T>
        static
        typename std::enable_if<!is_vector<typename std::remove_reference<T>::type>::value>::type
        clearVectorValue_(T& vec) {
            (void) vec;

            // No effect
        }


        // Multi-dimensional parameter indexed reads

        /*!
         * \brief Recursive lookup of an element within of a N-dimensional
         * vector type (e.g. vector<vector<vector<pod_type>>>) based on an input
         * \a indices vector. When the final element is found, a srting
         * representation of it is returned.
         * \param vec vector, possibly containing vectors or a pod type (or
         * string). An element in this vector will be selected based on
         * indices[index_level]. Then that value will be written to a string and
         * returned if it is a pod type (or string), or this method will be
         * recurisvely called on a vector element of \a vec.
         * \param indices indexes for each dimension of data type being indexed.
         * \param index_level index into \a indices representing the current
         * element of \a indices to use as an index into \a vec
         * \param peek If true, does not increment read count
         */
        template <typename T>
        typename std::enable_if<is_vector<typename std::remove_reference<T>::type>::value, std::string>::type
        getVectorItemValueFromString_(const T& vec,
                                      const std::vector<uint32_t>& indices,
                                      uint32_t index_level,
                                      bool peek) const {
            assert(indices.size() > 0);

            uint32_t idx = indices.at(index_level);
            if(idx >= vec.size()){
                throw ParameterException("Cannot get item from parameter \"")
                    << getName() << "\" as a vector which is of type \""
                    << getTypeName() << "\" with indices: " << indices << " (" << indices.size()
                    << " levels)" << " because this type has only " <<  vec.size()
                    << " elements at the vector located by indices[" << index_level << "]";
            }

            if(indices.size() - 1 == index_level){
                // Final index should place a value in a vector
                // Note that vec is a vector
                typename T::const_reference to_get = vec.at(idx);
                return getFinalVectorItemValueFromString_(to_get, indices, index_level, peek);
            }else{
                // Non-final index should select a vector and recurse
                typename T::const_reference next_vec = vec.at(idx);
                return getVectorItemValueFromString_(next_vec, indices, index_level+1, peek);
            }
        }

        /*!
         * \brief Error case for getVectorItemValueFromString_. Selected by
         * template deduction when getVectorItemValueFromString_ or
         * getItemValueFromString is invoked with a type which is not a vector.
         */
        template <typename T>
        typename std::enable_if<!is_vector<typename std::remove_reference<T>::type>::value, std::string>::type
        getVectorItemValueFromString_(const T& vec,
                                      const std::vector<uint32_t>& indices,
                                      uint32_t index_level,
                                      bool peek) const {
            (void) vec;
            (void) index_level;
            (void) peek;

            throw ParameterException("Cannot get item from parameter \"")
                << getName() << "\" which is of type \""
                << getTypeName() << "\" with indices: " << indices << " (" << indices.size()
                << " levels)" << " because this type only has " << index_level << " dimensions";
        }

        /*!
         * \brief Error case for getFinalVectorItemValueFromString_. Selected by
         * template deduction when getVectorItemValueFromString_ attempts to
         * get a string value of a whole vector (because too few indices were
         * given).
         */
        template <typename T>
        typename std::enable_if<is_vector<typename std::remove_reference<T>::type>::value, std::string>::type
        getFinalVectorItemValueFromString_(const T& to_get,
                                           const std::vector<uint32_t>& indices,
                                           uint32_t index_level,
                                           bool peek) const {
            (void) to_get;
            (void) peek;

            throw ParameterException("Cannot get value from string on parameter \"")
                << getName() << "\" which is of type \""
                << getTypeName() << "\" with indices: " << indices << " (" << indices.size()
                << " levels)" << " because this type has more than " << index_level << " dimensions";
        }

        /*!
         * \brief Gets value of an item given its reference as a string
         * represented as described by the display base for this parameter.
         * This is the termination of any getVectorItemValueFromString_ call
         * chain that refers to a valid non-vector item.
         * \tparam T type of value to get. Expected to be a non-vector type
         * having a sparta::utils::stringize_value capable of handling it.
         * \param indices Indices from intitial getItemValueFromString call for
         * error printout if needed
         * \param index_level index into indices list for error printout if
         * needed
         * \see getNumericDisplayBase
         * \see setNumericDisplayBase
         */
        template <typename T>
        typename std::enable_if<!is_vector<typename std::remove_reference<T>::type>::value, std::string>::type
        getFinalVectorItemValueFromString_(const T& to_get,
                                           const std::vector<uint32_t>& indices,
                                           uint32_t index_level,
                                           bool peek) const {
            assert(indices.size() == 0 || indices.size() - 1 == index_level);

            if(!peek){
                incrementReadCount_();
            }

            return sparta::utils::stringize_value(to_get, disp_base_, string_quote_);
        }


        // Nested Vector size queries

        /*!
         * \brief Recursive lookup the size of a vector within an N-dimensional
         * vector type (e.g. vector<vector<vector<pod_type>>>) based on an input
         * \a indices vector. When the vector specified is found, its size is
         * returned
         * \param vec vector, possibly containing vectors or a pod type (or
         * string). An element in this vector will be selected based on
         * indices[index_level]. Then the size of that vector will be returned
         * returned if index_level == indices.size()-1 has been reached, or this
         * method will be recurisvely called on that vector element of \a vec
         * until \a index_level == indices.size() - 1, at which point the size
         * of the element identified by \a indices will be returned
         * \param indices indexes for each dimension of data type being indexed.
         * For example, indices={1,2,3} refers to val_[1][2][3].size().
         * \param index_level index into \a indices representing the current
         * element of \a indices to use as an index into \a vec
         * \param peek If true, does not increment the read count
         */
        template <typename T>
        typename std::enable_if<is_vector<typename std::remove_reference<T>::type>::value, uint32_t>::type
        getVectorSize_(const T& vec,
                       const std::vector<uint32_t>& indices,
                       uint32_t index_level,
                       bool peek) const {
            assert(indices.size() > 0);

            uint32_t idx = indices.at(index_level);
            if(idx >= vec.size()){
                throw ParameterException("Cannot get size of vector from parameter \"")
                    << getName() << "\" as a vector which is of type \""
                    << getTypeName() << "\" with indices: " << indices << " (" << indices.size()
                    << " levels)" << " because this type has only " <<  vec.size()
                    << " elements at the vector located by indices[" << index_level << "]";
            }

            if(indices.size() - 1 == index_level){
                // Final index should place a value in a vector
                // Note that vec is a vector
                typename T::const_reference to_get = vec.at(idx);
                return getFinalVectorSize_(to_get, indices, index_level, peek);
            }else{
                // Non-final index should select a vector and recurse
                typename T::const_reference next_vec = vec.at(idx);
                return getVectorSize_(next_vec, indices, index_level+1, peek);
            }
        }

        /*!
         * \brief Error case for getVectorSize_. Selected by
         * template deduction when getVectorSize_ is invoked with a type which
         * is not a vector.
         */
        template <typename T>
        typename std::enable_if<!is_vector<typename std::remove_reference<T>::type>::value, uint32_t>::type
        getVectorSize_(const T& vec,
                       const std::vector<uint32_t>& indices,
                       uint32_t index_level,
                       bool peek) const {
            (void) vec;
            (void) index_level;
            (void) peek;

            throw ParameterException("Cannot get size of vector from parameter \"")
                << getName() << "\" which is of type \""
                << getTypeName() << "\" with indices: " << indices << " (" << indices.size()
                << " levels)" << " because this type only has " << index_level << " dimensions";
        }

        /*!
         * \brief Error case for getFinalVectorSize_. Selected by
         * template deduction when getFinalVectorSize_ get the size of a
         * non-vector type (because too many indices were given)
         */
        template <typename T>
        typename std::enable_if<!is_vector<typename std::remove_reference<T>::type>::value, uint32_t>::type
        getFinalVectorSize_(const T& to_get,
                            const std::vector<uint32_t>& indices,
                            uint32_t index_level,
                            bool peek) const {
            (void) to_get;
            (void) peek;

            throw ParameterException("Cannot get size of vector from parameter \"")
                << getName() << "\" which is of type \""
                << getTypeName() << "\" with indices: " << indices << " (" << indices.size()
                << " levels)" << " because the in dimension " << index_level
                << " is a a scalar (not a vector)";
        }

        /*!
         * \brief Gets size of a vector given its reference
         * This is the termination of any getVectorSize_ call chain that refers
         * to a valid vector.
         * \tparam T type of value to get. Expected to be a std::vector type
         * \param indices Indices from intitial getVectorSizeAt call for
         * error printout if needed
         * \param index_level index into indices list for error printout if
         * needed
         * \param peek If true, do not increment read count
         */
        template <typename T>
        typename std::enable_if<is_vector<typename std::remove_reference<T>::type>::value, uint32_t>::type
        getFinalVectorSize_(const T& to_get,
                            const std::vector<uint32_t>& indices,
                            uint32_t index_level,
                            bool peek) const {
            assert(indices.size() == 0 || indices.size() - 1 == index_level);

            if(!peek){
                incrementReadCount_(); // This is considered a read because this information can be useful
            }

            return to_get.size();
        }


        // Type string computation

        template <class T>
        typename std::enable_if<is_vector<T>::value, std::string>::type
        getTypeName_() const {
            std::stringstream ss;
            ss << "std::vector<";
            ss << getTypeName_<typename T::value_type>();
            ss << ">";
            return ss.str();
        }

        template <class T>
        typename std::enable_if<!is_vector<T>::value, std::string>::type
        getTypeName_() const {
            if(KeyValue::hasTypeNameFor<T>()){
                return KeyValue::lookupTypeName<T>();
            }else{
                return demangle(typeid(T).name());
            }
        }


        // Vector value writes
        template <class T, class C1>
        typename std::enable_if<is_vector<C1>::value >::type
        setValueFromString_(const std::string& str, bool poke=false) {
            checkModificationPermission_();
            (void) str;
            (void) poke;
            throw ParameterException("Cannot set value from string on parameter \"")
                << getName() << "\" which is a vector type \""
                << getTypeName() << "\"";
        }

        // Scalar value writes
        template <class T, class C1>
        typename std::enable_if<!is_vector<C1>::value >::type
        setValueFromString_(const std::string& str, bool poke=false) {
            checkModificationPermission_();
            ValueType tmp;

            size_t end_pos;
            tmp = smartLexicalCast<ValueType>(this, str, end_pos);

            if (!poke) {
                incrementWriteCount_();
            }

            ValueType old_val = val_;
            val_ = tmp;
            try {
                invokeModifierCB_();
            }
            catch(sparta::SpartaException & e) {
                val_ = old_val;
                throw;
            }
        }


        // Vector value writes for DEFAULT value
        template <class T, class C1>
        typename std::enable_if<is_vector<C1>::value >::type
        overrideDefaultFromString_(const std::string& str) {
            checkModificationPermission_();
            (void) str;
            throw ParameterException("Cannot set default from string on parameter \"")
                << getName() << "\" which is a vector type \""
                << getTypeName() << "\"";
        }

        // Scalar value writes for DEFAULT value
        template <class T, class C1>
        typename std::enable_if<!is_vector<C1>::value >::type
        overrideDefaultFromString_(const std::string& str) {
            checkModificationPermission_();
            ValueType tmp;

            size_t end_pos;
            tmp = smartLexicalCast<ValueType>(this, str, end_pos);
            def_val_ = tmp;
        }


        // 1-d vector writes

        template <class T, class C1> // C1=ValueType
        typename std::enable_if<!is_vector<C1>::value >::type
        setValueFromStringVector_(const std::vector<std::string>& vec, bool poke=false) {
            checkModificationPermission_();
            (void) vec;
            (void) poke;
            throw ParameterException("Cannot directly set value from string vector on parameter \"")
                << getName() << "\" which is a scalar (or string) type \""
                << getTypeName() << "\"";
        }

        template <class T, class C1> // C1=ValueType
        typename std::enable_if<is_vector<C1>::value && !is_vector<typename C1::value_type>::value>::type
        setValueFromStringVector_(const std::vector<std::string>& vec, bool poke=false) {
            checkModificationPermission_();
            ValueType tmpvec;
            for(const std::string& s : vec){
                size_t end_pos;
                tmpvec.push_back(smartLexicalCast<typename ValueType::value_type>(this, s, end_pos));
            }
            if (!poke) {
                incrementWriteCount_();
            }
            ValueType old_val = val_;
            val_ = tmpvec;
            try {
                invokeModifierCB_();
            }
            catch(sparta::SpartaException & e) {
                val_ = old_val;
                throw;
            }
        }

        template <class T, class C1> // C1=ValueType
        typename std::enable_if<is_vector<C1>::value && is_vector<typename C1::value_type>::value>::type
        setValueFromStringVector_(const std::vector<std::string>& vec, bool poke=false) {
            checkModificationPermission_();
            (void) vec;
            (void) poke;
            throw ParameterException("Cannot directly set value from string vector on parameter \"")
                << getName() << "\" which has " << getDimensionality() << " dimensions. Type is \""
                << getTypeName() << "\". Only 1-dimensional parameters can be set using this method";
        }


        // 1-d vector writes for DEFAULT value

        template <class T, class C1> // C1=ValueType
        typename std::enable_if<!is_vector<C1>::value >::type
        overrideDefaultFromStringVector_(const std::vector<std::string>& vec) {
            checkModificationPermission_();
            (void) vec;

            throw ParameterException("Cannot directly override default value from string vector on parameter \"")
                << getName() << "\" which is a scalar (or string) type \""
                << getTypeName() << "\"";
        }

        template <class T, class C1> // C1=ValueType
        typename std::enable_if<is_vector<C1>::value && !is_vector<typename C1::value_type>::value>::type
        overrideDefaultFromStringVector_(const std::vector<std::string>& vec) {
            checkModificationPermission_();
            ValueType tmpvec;
            for(const std::string& s : vec){
                size_t end_pos;
                tmpvec.push_back(smartLexicalCast<typename ValueType::value_type>(this, s, end_pos));
            }
            def_val_ = tmpvec;
        }

        template <class T, class C1> // C1=ValueType
        typename std::enable_if<is_vector<C1>::value && is_vector<typename C1::value_type>::value>::type
        overrideDefaultFromStringVector_(const std::vector<std::string>& vec) {
            checkModificationPermission_();
            (void) vec;

            throw ParameterException("Cannot directly set value from string vector on parameter \"")
                << getName() << "\" which has " << getDimensionality() << " dimensions. Type is \""
                << getTypeName() << "\". Only 1-dimensional parameters can be set using this method";
        }


        // 1-d vector reads

        template <class T, class C1> // C1=ValueType
        typename std::enable_if<!is_vector<C1>::value, std::string>::type
        getValueAsStringAt_(size_t idx, bool peek) const {
            if(idx != 0){
                ParameterException ex("Cannot get value as string at index other than 0 on parameter \"");
                ex << getName() << "\" which is a scalar (or string) type \""
                   << getTypeName() << "\"";
                throw ex;
            }
            if(!peek){
                incrementReadCount_();
            }
            return getValueAsString();
        }

        template <class T, class C1> // C1=ValueType
        typename std::enable_if<is_vector<C1>::value, std::string>::type
        getValueAsStringAt_(size_t idx, bool peek) const {
            if(!peek){
                incrementReadCount_();
            }
            return sparta::utils::stringize_value(val_.at(idx), disp_base_, string_quote_);
        }


        // 1-d vector inspection

        template <class T, class C1> // C1=ValueType
        typename std::enable_if<!is_vector<C1>::value, size_t>::type
        getNumValues_(bool peek) const {
            (void) peek;
            // DO NOT count this as a read access. Nothing can be deduced about
            // the parameter from this information since this is a scalar
            return 1;
        }

        template <class T, class C1> // C1=ValueType
        typename std::enable_if<is_vector<C1>::value, size_t>::type
        getNumValues_(bool peek) const {
            // Getting number of values counts as reading since it *might* be
            // informative
            if(!peek){
                incrementReadCount_();
            }
            return val_.size();
        }

        // Parameter Dimensionality

        /*!
         * \brief Generic case
         */
        template<typename T, typename Enable = void>
        struct Dimensionality {
        };

        /*!
         * \brief Recursive vector dimensionality type. value is based on
         * number of recursions needed to remove all vector layers from the
         * scalar type contained.
         */
        template<typename T>
        struct Dimensionality<T, typename std::enable_if<is_vector<T>::value>::type> {
            enum { value = Dimensionality<typename T::value_type>::value + 1};
        };

        /*!
         * \brief Termination case. \a T is scalar and dimensionality value is 0
         */
        template<typename T>
        struct Dimensionality<T, typename std::enable_if<!is_vector<T>::value>::type> {
            enum { value = 0 }; // T is a scalar type, with 0 dimensions
        };

    }; // class Parameter


    // Implementation

    template <class T>
    inline const T ParameterBase::getValueAs() const {
        const Parameter<T>* p = dynamic_cast<const Parameter<T>*>(this);
        if(nullptr == p){
            throw ParameterException("Cannot get value from Parameter \"")
                << getName() << "\" as a " << demangle(typeid(T).name())
                << " because it is internally a " << getTypeName()
                << ". getValueAs must be exact";
        }
        return *p;
    }

    template <typename T>
    inline T smartLexicalCast(const ParameterBase* p,
                              const std::string& s,
                              size_t& end_pos,
                              bool allow_recursion,
                              bool allow_prefix)
    {
        try{
            return utils::smartLexicalCast<T>(s, end_pos, allow_recursion, allow_prefix);
        }catch(SpartaException& ex){
            ex << " in parameter " << p->getLocation();
            throw;
        }
    }

} // namespace sparta

// __PARAMETER_H__
#endif
