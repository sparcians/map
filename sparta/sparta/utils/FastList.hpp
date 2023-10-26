// <FastList.hpp> -*- C++ -*-


/**
 * \file   FastList.hpp
 *
 * \brief File that defines the FastList class -- an alternative to
 * std::list when the user knows the size of the list ahead of time.
 */

#pragma once

#include <vector>
#include <iostream>
#include <exception>
#include <iterator>
#include <cinttypes>
#include <cassert>
#include <type_traits>

#include "sparta/utils/IteratorTraits.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace sparta::utils
{
    /**
     * \class FastList
     * \brief An alternative to std::list, about 70% faster
     * \tparam T The object to maintain
     *
     * This class is a container type that allows back emplacement and
     * random deletion.  'std::list' provides the same type of
     * functionality, but performs heap allocation of the internal
     * nodes.  Under the covers, this class does not perform
     * new/delete of the Nodes, but reuses existing ones, performing
     * an inplace-new of the user's object.
     *
     * Testing shows this class is 70% faster than using std::list.
     * Caveats:
     *
     *  - The size of FastList is fixed to allow for optimization
     *  - The API isn't as complete as typical STL container types
     *
     */
    template <class DataT>
    class FastList
    {
        struct Node
        {
            using NodeIdx = int;

            // Where this node is in the vector
            const NodeIdx index;

            // Points to the next element or the next free
            // element if this node has been removed.
            NodeIdx next = -1;

            // Points to the previous element.
            NodeIdx prev = -1;

            // Stores the memory for an instance of 'T'.
            // Use placement new to construct the object and
            // manually invoke its dtor as necessary.
            std::aligned_storage_t<sizeof(DataT), alignof(DataT)> type_storage;

            Node(NodeIdx _index) :
                index(_index)
            {}
        };

        typename Node::NodeIdx advanceNode_(typename Node::NodeIdx node_idx) const {
            typename Node::NodeIdx ret_idx = (node_idx == -1 ? -1 : nodes_[node_idx].next);
            return ret_idx;
        }

        auto getStorage(typename Node::NodeIdx node_idx) {
            return &nodes_[node_idx].type_storage;
        }

        auto getStorage(typename Node::NodeIdx node_idx) const {
            return &nodes_[node_idx].type_storage;
        }

    public:
        using value_type = DataT; //!< Handy using

        /**
         * \class NodeIterator
         * \brief The internal iterator type of FastList.  Use FastList<T>::[const_]iterator instead
         *
         */
        template<bool is_const = true>
        class NodeIterator : public sparta::utils::IteratorTraits<std::forward_iterator_tag, value_type>
        {
            typedef std::conditional_t<is_const, const value_type &, value_type &> RefIteratorType;
            typedef std::conditional_t<is_const, const value_type *, value_type *> PtrIteratorType;
            typedef std::conditional_t<is_const, const FastList *, FastList *>     FastListPtrType;
        public:

            NodeIterator() = default;

            NodeIterator(const NodeIterator<false> & iter) :
                flist_(iter.flist_),
                node_idx_(iter.node_idx_)
            {}

            /**
             * \brief Determine if the iteartor is valid
             * \return true if the iterator is valie
             */
            bool isValid() const { return (node_idx_ != -1); }

            //! Iterator dereference
            PtrIteratorType operator->()       {
                assert(isValid());
                return reinterpret_cast<PtrIteratorType>(flist_->getStorage(node_idx_));
            }

            //! Iterator dereference (const)
            PtrIteratorType operator->() const {
                assert(isValid());
                return reinterpret_cast<PtrIteratorType>(flist_->getStorage(node_idx_));
            }

            //! Iterator dereference
            RefIteratorType operator* ()       {
                assert(isValid());
                return *reinterpret_cast<PtrIteratorType>(flist_->getStorage(node_idx_));
            }

            //! Iterator dereference (const)
            RefIteratorType operator* () const {
                assert(isValid());
                return *reinterpret_cast<PtrIteratorType>(flist_->getStorage(node_idx_));
            }

            //! Get the index in the list where this iterator points
            int getIndex() const { return node_idx_; }

            //! Move to the next iterator (pre)
            NodeIterator & operator++()
            {
                assert(isValid());
                node_idx_ = flist_->advanceNode_(node_idx_);
                return *this;
            }

            //! Move to the next iterator (post)
            NodeIterator operator++(int)
            {
                NodeIterator orig = *this;
                assert(isValid());
                node_idx_ = flist_->advanceNode_(node_idx_);
                return orig;
            }

            //! Equality of iterator (not the underlying object)
            bool operator!=(const NodeIterator &rhs)
            {
                return (rhs.flist_ != flist_) ||
                    (rhs.node_idx_ != node_idx_);
            }

            //! Equality of iterator (not the underlying object)
            bool operator==(const NodeIterator & node) const noexcept {
                return (node.flist_ == flist_) && (node.node_idx_ == node_idx_);
            }

            //! Assignments
            NodeIterator& operator=(const NodeIterator &rhs) = default;
            NodeIterator& operator=(      NodeIterator &&rhs) = default;

        private:
            friend class FastList<DataT>;

            NodeIterator(FastListPtrType flist, typename Node::NodeIdx node_idx) :
                flist_(flist),
                node_idx_(node_idx)
            { }

            FastListPtrType flist_ = nullptr;
            typename Node::NodeIdx node_idx_ = -1;
        };

        /**
         * \brief Construct FastList of a given size
         * \param size Fixed size of the list
         */
        FastList(size_t size)
        {
            int node_idx = 0;
            nodes_.reserve(size);
            for(size_t i = 0; i < size; ++i) {
                Node n(node_idx);
                n.prev = node_idx - 1;
                n.next = node_idx + 1;
                ++node_idx;
                nodes_.emplace_back(n);
            }
            nodes_.back().next = -1;
        }

        //! Destroy (clear) the list
        ~FastList() { clear(); }

        using iterator       = NodeIterator<false>;  //!< Iterator type
        using const_iterator = NodeIterator<true>;   //!< Iterator type, const

        //! Obtain a beginning iterator
        iterator begin() {
            return iterator(this, first_node_);
        }

        //! Obtain a beginning const_iterator
        const_iterator begin() const {
            return const_iterator(this, first_node_);
        }

        //! Obtain an end iterator
        iterator       end()       { return iterator(this, -1); }

        //! Obtain an end const_iterator
        const_iterator end() const { return const_iterator(this, -1); }

        //! Get the front of the fast list non-const
        DataT & front() { return *begin(); }

        //! Get the front of the fast list, const
        const DataT & front() const { return *begin(); }

        //! \return Is this container empty?
        bool   empty()    const { return size_ == 0; }

        //! \return The current size of the container
        size_t size()     const { return size_; };

        //! \return The maximum size of this list
        size_t max_size() const { return nodes_.capacity(); };

        ////////////////////////////////////////////////////////////////////////////////
        // Modifiers
        void clear() noexcept {
            const auto my_end = end();
            for(auto it = begin(); it != my_end;) {
                erase(it++);
            }
        }

        /**
         * \brief Erase an element with the given iterator
         * \param entry Iterator to the entry being erased
         */
        iterator erase(const const_iterator & entry)
        {
            const auto node_idx = entry.getIndex();
            auto & node_to_erase = nodes_[node_idx];
            reinterpret_cast<DataT*>(&node_to_erase.type_storage)->~DataT();
            int next_elem = -1;

            if(first_node_ == node_idx) {
                first_node_ = node_to_erase.next;
            }
            if(last_node_ == node_idx) {
                last_node_ = node_to_erase.prev;
            }

            if(SPARTA_EXPECT_FALSE(node_to_erase.next != -1))
            {
                auto & next_node = nodes_[node_to_erase.next];
                next_node.prev = node_to_erase.prev;
                next_elem = node_to_erase.next;
            }

            if(SPARTA_EXPECT_FALSE(node_to_erase.prev != -1))
            {
                auto & prev_node = nodes_[node_to_erase.prev];
                prev_node.next = node_to_erase.next;
            }

            node_to_erase.prev = -1;
            node_to_erase.next = -1;
            if(SPARTA_EXPECT_TRUE(free_head_ != -1)) {
                nodes_[free_head_].prev = node_idx;
                node_to_erase.next = free_head_;
            }
            free_head_ = node_idx;
            --size_;
            return iterator(this, next_elem);
        }

        template<class ...ArgsT>
        iterator emplace(const const_iterator & pos, ArgsT&&...args)
        {
            sparta_assert(free_head_ != -1,
                          "FastList is out of element room");
            const auto index_pos = pos.getIndex();

            // If the index pos is -1, it's either end() or begin() on
            // an empty list.  Just emplace_back (or front, don't matter)
            if(index_pos == -1) {
                return emplace_back(std::forward<ArgsT>(args)...);
            }

            auto & new_node = nodes_[free_head_];
            free_head_ = new_node.next;
            new (&new_node.type_storage) DataT(args...);
            // Update pointers.  Start with a clean slate
            new_node.next = -1;
            new_node.prev = -1;

            // Insert before the given pt
            auto & insert_pt = nodes_[index_pos];
            new_node.next = insert_pt.index;
            new_node.prev = insert_pt.prev;
            insert_pt.prev = new_node.index;
            if(new_node.prev != -1) {
                // update the previous node's next
                nodes_[new_node.prev].next = new_node.index;
            }

            if((first_node_ == index_pos) || (first_node_ == -1))
            {
                first_node_ = new_node.index;
            }
            ++size_;

            return iterator(this, new_node.index);
        }

        /**
         * \brief Add an element to the front of the list
         * \tparam args Arguments to be passed to the user type for construction
         * \return iterator to the newly emplaced object
         */
        template<class ...ArgsT>
        iterator emplace_front(ArgsT&&...args)
        {
            sparta_assert(free_head_ != -1,
                          "FastList is out of element room");

            auto & new_node = nodes_[free_head_];
            free_head_ = new_node.next;
            new (&new_node.type_storage) DataT(args...);

            // Update pointers.  Start with a clean slate
            new_node.next = -1;
            new_node.prev = -1;
            if(SPARTA_EXPECT_TRUE(first_node_ != -1))
            {
                auto & old_first = nodes_[first_node_];
                old_first.prev = new_node.index;
                new_node.next = old_first.index;
            }
            first_node_ = new_node.index;
            if(SPARTA_EXPECT_FALSE(last_node_ == -1)) {
                last_node_ = first_node_;
            }

            ++size_;
            return iterator(this, new_node.index);
        }

        /**
         * \brief emplace and object at the back
         * \param args The arguments to the T constructor
         * \return iterator of the emplaced item
         */
        template<class ...ArgsT>
        iterator emplace_back(ArgsT&&...args) {
            sparta_assert(free_head_ != -1,
                          "FastList is out of element room");

            auto & new_node = nodes_[free_head_];
            free_head_ = new_node.next;
            new (&new_node.type_storage) DataT(args...);

            // Update pointers.  Start with a clean slate
            new_node.next = -1;
            new_node.prev = -1;
            if(SPARTA_EXPECT_TRUE(last_node_ != -1))
            {
                auto & old_last = nodes_[last_node_];
                old_last.next = new_node.index;
                new_node.prev = old_last.index;
            }
            last_node_ = new_node.index;
            if(SPARTA_EXPECT_FALSE(first_node_ == -1)) {
                first_node_ = last_node_;
            }

            ++size_;
            return iterator(this, new_node.index);
        }

        //! Insert an element at a specific place in the list.  Really
        //! just an alias for emplace
        template<class ...ArgsT>
        iterator insert(const const_iterator & pos, ArgsT&&...args) {
            return emplace(pos, args...);
        }

        //! Pop the last element off of the list
        void pop_back() {
            sparta_assert(last_node_ != -1,
                          "Can't pop_back on an empty list");
            erase(iterator(this, last_node_));
        }

        //! Pop the first element off of the list
        void pop_front() {
            sparta_assert(first_node_ != -1,
                          "Can't pop_front on an empty list");
            erase(iterator(this, first_node_));
        }

    private:

        // Friendly printer
        friend std::ostream & operator<<(std::ostream & os, const FastList<DataT> & fl)
        {
            int next_node = fl.first_node_;
            if(next_node == -1) {
                os << "<empty>" << std::endl;
            }
            else {
                int index = fl.size_ - 1;
                do
                {
                    const auto & n = fl.nodes_[next_node];
                    os << index << " elem="    << *reinterpret_cast<const DataT*>(&n.type_storage)
                       << " n.next=" << n.next
                       << " n.prev=" << n.prev << std::endl;
                    next_node = n.next;
                    --index;
                } while(next_node != -1);
            }
            return os;
        }

        // Stores all the nodes.
        std::vector<Node> nodes_;

        int free_head_  = 0;  //!< The free head
        int first_node_ = -1; //!< The first node in the list (-1 for empty)
        int last_node_  = -1; //!< The last node in the list (-1 for empty)
        size_t size_    = 0;  //!< The number of elements in the list
    };
}
