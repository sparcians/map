// <FastList.hpp> -*- C++ -*-


/**
 * \file   FastList.hpp
 *
 * \brief File that defines the FastList class -- an alternative to std::list
 */

#pragma once

#include <vector>
#include <iostream>
#include <exception>
#include <iterator>

#include "sparta/utils/SpartaAssert.hpp"

namespace sparta::utils
{
    /**
     * \class FastList
     * \brief An alternative to std::list, about 3x faster
     * \tparam T The object to maintain
     *
     */
    template <class T>
    class FastList
    {
        struct Node
        {
            // Stores the memory for an instance of 'T'.
            // Use placement new to construct the object and
            // manually invoke its dtor as necessary.
            typename std::aligned_storage<sizeof(T), alignof(T)>::type type_storage;

            // Where this node is in the vector
            int index = -1;

            // Points to the next element or the next free
            // element if this node has been removed.
            int next = -1;

            // Points to the previous element.
            int prev = -1;
        };

        Node * advanceNode_(const Node * node) {
            sparta_assert(node->index != -1);
            Node * ret_node = (node->next == -1 ? nullptr : &nodes_[node->next]);
            return ret_node;
        }

    public:

        using value_type = T;

        template<bool is_const = true>
        class NodeIterator // : public std::iterator<std::intput_iterator_tag, Node>
        {
            typedef std::conditional_t<is_const, const value_type &, value_type &> RefIteratorType;
            typedef std::conditional_t<is_const, const value_type *, value_type *> PtrIteratorType;
        public:

            NodeIterator(const NodeIterator<false> & iter) :
                node_(iter.node_)
            {}

            bool isValid() const { return (node_ != nullptr); }
            PtrIteratorType operator->()       {
                sparta_assert(isValid());
                return reinterpret_cast<value_type*>(&node_->type_storage);
            }
            PtrIteratorType operator->() const {
                sparta_assert(isValid());
                return reinterpret_cast<value_type*>(&node_->type_storage);
            }
            RefIteratorType operator* ()       {
                sparta_assert(isValid());
                return *reinterpret_cast<value_type*>(&node_->type_storage);
            }
            RefIteratorType operator* () const {
                sparta_assert(isValid());
                return *reinterpret_cast<value_type*>(&node_->type_storage);
            }

            int getIndex() const { return node_->index; }

            NodeIterator & operator++()
            {
                sparta_assert(isValid());
                node_ = flist_->advanceNode_(node_);
                return *this;
            }

            NodeIterator operator++(int)
            {
                NodeIterator orig = *this;
                sparta_assert(isValid());
                node_ = flist_->advanceNode_(node_);
                return orig;
            }

            bool operator!=(const NodeIterator &rhs)
            {
                return (rhs.flist_ != flist_) ||
                    (rhs.node_ != node_);
            }

            NodeIterator& operator=(const NodeIterator &rhs) = default;
            NodeIterator& operator=(NodeIterator &&rhs) = default;

        private:
            friend class FastList<T>;

            NodeIterator() = default;
            NodeIterator(FastList * flist, Node * node) :
                flist_(flist),
                node_(node)
            { }

            FastList * flist_ = nullptr;
            Node * node_ = nullptr;
        };

        FastList(size_t size) {
            nodes_.reserve(size);
            int node_idx = 0;
            for(size_t i = 0; i < size; ++i) {
                Node n;
                n.prev = node_idx - 1;
                n.next = node_idx + 1;
                n.index = node_idx;
                ++node_idx;
                nodes_.emplace_back(n);
            }
            nodes_.back().next = -1;
            free_head_ = 0;
        }

        using iterator       = NodeIterator<false>;
        using const_iterator = NodeIterator<true>;

        template<class ...ArgsT>
        iterator emplace_back(ArgsT&&...args)
        {
            if(free_head_ == -1) { throw std::bad_alloc(); }

            auto & n = nodes_[free_head_];
            new (&n.type_storage) T(args...);

            if(first_node_ != -1)
            {
                auto & old_first = nodes_[first_node_];
                const int old_first_idx = first_node_;
                first_node_ = free_head_;
                free_head_ = n.next;

                old_first.prev = first_node_;
                n.next = old_first_idx;
                n.prev = -1;
            }
            else {
                first_node_ = free_head_;
                free_head_ = n.next;
                n.next = -1;
            }
            ++size_;
            return iterator(this, &n);
        }

        void erase(const const_iterator & entry)
        {
            const auto node_idx = entry.getIndex();
            auto & curr_node = nodes_[node_idx];
            reinterpret_cast<T*>(&curr_node.type_storage)->~T();

            if(first_node_ == node_idx) {
                first_node_ = curr_node.next;
            }

            if(curr_node.next != -1)
            {
                auto & next_node = nodes_[curr_node.next];
                next_node.prev = curr_node.prev;
            }

            if(curr_node.prev != -1)
            {
                auto & prev_node = nodes_[curr_node.prev];
                prev_node.next = curr_node.next;
            }

            if(free_head_ != -1) {
                nodes_[free_head_].prev = node_idx;
                curr_node.next = free_head_;
            }
            free_head_ = node_idx;
            --size_;
        }

        iterator       begin()       { return iterator(this, (first_node_ == -1 ? nullptr : &nodes_[first_node_])); }
        const_iterator begin() const { return const_iterator(this, (first_node_ == -1 ? nullptr : &nodes_[first_node_])); }

        iterator       end()       { return iterator(this, nullptr); }
        const_iterator end() const { return const_iterator(this, nullptr); }

        size_t size()     const { return size_; };
        size_t capacity() const { return nodes_.capacity(); };
        bool   empty()    const { return size_ == 0; }

    private:

        friend std::ostream & operator<<(std::ostream & os, const FastList<T> & fl)
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
                    os << index << " elem="    << *reinterpret_cast<const T*>(&n.type_storage)
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

        // Points to the first free node or -1 if the free list
        // is empty. Initially this starts out as -1.
        int free_head_ = 0;

        int first_node_ = -1;

        size_t size_ = 0;
    };
}
