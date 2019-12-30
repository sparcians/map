// <Decoder> -*- C++ -*-

/**
 * \file   Decoder
 *
 * \brief  File that defines the Decoder class
 */


#ifndef __DECODER__H__
#define __DECODER__H__

#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/decode/DecoderBase.hpp"
#include "sparta/utils/Traits.hpp"
#include <cinttypes>
#include <type_traits>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cassert>
#include <string>
#include <vector>

// The ISA is descibed with a table of instructions.  Each instruction in the
// table is described by single inclusion mask/encoding and zero to many
// exclusion masks/encodings.  An opcode maps to an instruction if the opcode is
// "included" by the inclusion mask/encoding and not "excluded" by any of the
// exclusion masks/encodings.  An opcode is included if:
//
//     bool included = (inclusion_mask & opcode) == inclusion_encoding;
//
// An instruction is excluded if:
//
//     bool excluded = false;
//     for (int i = 0; i < num_exclusions; ++i) {
//         excluded = excluded || (exclusion_mask[i] & opcode) == exclusion_encoding[i];
//     }
//
// So, an opcode is mapped to an instruction if:
//
//     bool match = included && !excluded;
//
// Each opcode should map to one and only one instruction in the table.

// An opcode could be applied sequentially to each instruction in the table but
// that would be very slow.  There are potentially hundreds of instructions each
// with multiple masks and encodings.  So, instead, this decoder uses a tree to
// hopefully more quickly map an opcode to an instruction.  It is designed so
// looking up an opcode in the tree is fast.  Unfortunately, the initial
// construction of the tree is painful and confusing.

// The tree is constructed from Nodes.  These are the data members of a Node:
//
//     class Node {
//         const InstHandleT* inst; // The instruction referenced by this node
//         uint32_t op_mask;        // The mask to apply to the opcode to compute an
//                                  // index into the list.  This mask only has a single
//                                  // bit-field that is set to all ones.  If this field
//                                  // is 0 then this node is a leaf node.
//         uint8_t op_shift;        // The right shift to apply to the opcode to compute
//                                  // an index into the list.
//         Node** list;             // An array of pointers to the children nodes.  I
//                                  // don't know why this is named "list".  This
//                                  // probably should probably be an STL vector.
//         uint16_t list_size;      // The size of the array of pointers
//     }

// Looking up an opcode in the tree is simple.  Starting at the root of the tree
// (n) with opcode (op) this algorithm is applied:
//
//     Node* find_no_recurse_ (Node* n, uint32_t op) const {
//         while (!n->isLeaf()) {
//             uint32_t idx = (op & n->op_mask) >> n->op_shift;
//             n = n->list[idx];
//         }
//         return n;
//     }
//
// That't it.

namespace sparta
{
    namespace decode
    {

        static const uint32_t max_mask_bits = 8;

        /**
         * \brief Determine if a halfword is the first halfword of a Thumb 32-bit opcode
         * \param hw The halfword
         * \return True if the halfword is the first halfword of a Thumb 32-bit opcode
         */
        inline bool isThumb32 (uint16_t hw) {
            uint16_t val =  hw >> 8;
            bool t32 = ((val & 0xe0) == 0xe0) && ((val & 0x18) != 0x00);
            return t32;
        }

        /**
         * \brief Determine if a halfword is a Thumb 16-bit opcode
         * \param hw The halfword
         * \return True if the halfword is a Thumb 16-bit opcode
         */
        inline bool isThumb16 (uint16_t hw) {
            return !isThumb32(hw);
        }

        /**
         * \class Decoder
         *
         * Decoder class used to decode instructions to the user's
         * instruction type.
         */
        template<typename InstHandleT>
        class Decoder
        {
            class Node {
            public:
                const InstHandleT * inst;
                Node **list;
                uint32_t op_mask;
                uint16_t list_size;
                uint8_t op_shift;
                bool pruned;

                Node () :
                    inst (nullptr),
                    list (nullptr),
                    op_mask (0),
                    list_size (0),
                    op_shift (0),
                    pruned (false)
                {}

                inline bool isLeaf () const {
                    return (op_mask == 0);
                }

                inline bool operator== (const Node & node) const {
                    if (inst != node.inst) return false;
                    if (op_mask != node.op_mask) return false;
                    if (op_shift != node.op_shift) return false;
                    if (list_size != node.list_size) return false;
                    for (uint32_t i = 0; i < node.list_size; i++) {
                        if (list[i] != node.list[i]) return false;
                    }
                    return true;
                }
            };

        public:
            typedef Node * NodePtr;
            typedef std::vector<NodePtr> NodePtrVect;

            /* Define iterator to traverse tree in depth-first post-order sequence.
             *
             *                         F
             *                       /   \
             *                     B       G
             *                   /   \       \
             *                  A     D        I
             *                      /   \    /
             *                     C     E  H
             *
             * Post-order traversal sequence is A, C, E, D, B, H, I, G, F
             */
            class iterator
            {
                static const uint32_t max_depth = 32;

            public:

                iterator (Node * node = nullptr) :
                    root_ (node),
                    node_ (node),
                    depth_ (0)
                {
                    for (uint32_t i = 0; i < max_depth; i++) {
                        parent_idx_[i] = 0;
                        parent_node_[i] = nullptr;
                    }
                }

                void up ()
                {
                    if (depth_ == -1) {
                        // Stay at the end node
                    } else if (depth_ == 0) {
                        // Go to the end node
                        node_ = nullptr;
                        parent_node_[0] = nullptr;
                        parent_idx_[0] = 0;
                        depth_ = -1;
                    } else {
                        node_ = parent_node_[depth_];
                        parent_node_[depth_] = nullptr;
                        parent_idx_[depth_] = 0;
                        depth_--;
                    }
                }

                void down (uint32_t idx)
                {
                    if (depth_ == -1) {
                        // Go to the root node
                        assert (idx == 0);
                        depth_ = 0;
                        parent_node_[0] = nullptr;
                        parent_idx_[0] = 0;
                        node_ = root_;
                    } else {
                        assert (idx < node_->list_size);
                        depth_++;
                        parent_node_[depth_] = node_;
                        parent_idx_[depth_] = idx;
                        node_ = node_->list[idx];
                    }
                }

                inline iterator & operator++ ()
                {
                    if (depth_ == -1) {
                        // Stay at the end node
                    } else if (depth_ == 0) {
                        // Go to the end node
                        depth_ = -1;
                        parent_node_[0] = nullptr;
                        parent_idx_[0] = 0;
                        node_ = nullptr;
                    } else {
                        // Go up one node.  If all children visited then return.
                        uint32_t next_idx = parent_idx_[depth_] + 1;
                        up();
                        if (next_idx == node_->list_size) {
                            return * this;
                        }

                        // Go down to leaf node on next child path
                        down (next_idx);
                        while (node_->isLeaf() == false) {
                            down (0);
                        }
                    }

                    return * this;
                }

                inline bool operator!= (iterator & itr) const
                {
                    if (node_ != itr.node_) return true;
                    if (depth_ != itr.depth_) return true;
                    if (root_ != itr.root_) return true;
                    for (int32_t i = 0; i <= depth_; i++) {
                        if (parent_idx_[i] != itr.parent_idx_[i]) return true;
                        if (parent_node_[i] != itr.parent_node_[i]) return true;
                    }
                    return false;
                }

                inline Node & operator* () const
                {
                    return *node_;
                }

                inline Node * operator-> () const
                {
                    return node_;
                }

                inline void replaceNode (Node * new_node)
                {
                    // Replace the node in the iterator
                    node_ = new_node;

                    // Replace the node in the tree
                    Node * parent_node = parent_node_[depth_];
                    uint32_t parent_idx = parent_idx_[depth_];
                    parent_node->list[parent_idx] = new_node;
                }

                inline std::string getPath () const
                {
                    std::ostringstream s;
                    if (depth_ == -1) {
                        s << "end";
                    } else {
                        s << "root";
                        for (uint32_t i = 1; i <= depth_; i++) {
                            s << "." << parent_idx_[i];
                        }
                    }
                    return s.str();
                }

            private:

                Node * root_;
                Node * node_;
                int32_t depth_;         // A depth of -1 is the end node
                uint32_t parent_idx_[max_depth];
                Node * parent_node_[max_depth];
            };

            iterator begin () {
                iterator itr (root_node_);
                while (itr->isLeaf() == false) {
                    itr.down (0);
                }
                return itr;
            }

            iterator end () {
                iterator itr (root_node_);
                itr.up ();
                return itr;
            }

            template<class InstVectT>
            Decoder (const InstVectT & inst_vect, bool use_arm32_opts = false,
                     const std::string & name = "unknown") :
                name_ (name),
                root_node_ (nullptr),
                undef_node_ (nullptr)
            {
                static_assert(sparta_traits<InstVectT>::stl_iterable,
                              "The Decoder requires the inst_vect to be an STL iterable type");

                if (use_arm32_opts == true) {
                    root_node_ = construct_and_populate_arm32_ (inst_vect);
                } else {
                    root_node_ = construct_and_populate_ (inst_vect);
                }
                // print_stats ();
                // std::cout << std::endl;
                // prune_ ();
                // std::cout << std::endl;
                // print_stats ();
                // std::cout << std::endl;
            }

            Decoder () {}

            ~Decoder () {
                deallocate_memory_ ();
            }

            const std::string & getName () const {
                return name_;
            }

            const InstHandleT * lookup (uint32_t op) const
            {
                Node * node = nullptr;

                if (debug_find_ == false) {
                    //node = find_ (root_node_, op);
                    node = find_no_recurse_ (root_node_, op);
                } else {
                    node = find_ ("root", root_node_, op);
                }
                return node->inst;
            }

            void print_tree () const {
                print_tree_ ("root", root_node_);
            }

            void print_tree_depth_first () {
                print_tree_depth_first_ ();
            }

            void print_stats () {
                num_undef_nodes_ = 0;
                num_pruned_undef_nodes_ = 0;
                num_inst_nodes_ = 0;
                num_pruned_inst_nodes_ = 0;
                num_interior_nodes_ = 0;
                num_pruned_interior_nodes_ = 0;

                hist_node_list_size_.clear ();
                hist_node_list_size_.resize ((1 << max_mask_bits) + 1, 0);

                hist_unpruned_node_list_size_.clear ();
                hist_unpruned_node_list_size_.resize ((1 << max_mask_bits) + 1, 0);

                hist_tree_depth_to_inst_.clear ();
                hist_tree_depth_to_inst_.resize (33, 0);

                set_pruned_ (false);
                print_stats_ ("root", root_node_);

                std::cout << "num_undef_nodes = " << num_undef_nodes_ << std::endl;
                std::cout << "num_pruned_undef_nodes = " << num_pruned_undef_nodes_ << std::endl;
                std::cout << "num_inst_nodes = " << num_inst_nodes_ << std::endl;
                std::cout << "num_pruned_inst_nodes = " << num_pruned_inst_nodes_ << std::endl;
                std::cout << "num_interior_nodes = " << num_interior_nodes_ << std::endl;
                std::cout << "num_pruned_interior_nodes = " << num_pruned_interior_nodes_ << std::endl;
                std::cout << std::endl;

                for (uint32_t i = 0; i < hist_node_list_size_.size(); i = (i == 0) ? 1 : i << 1) {
                    std::cout << "hist_node_list_size[" << i << "]=" << hist_node_list_size_[i] << std::endl;
                }
                std::cout << std::endl;

                for (uint32_t i = 0; i < hist_unpruned_node_list_size_.size(); i = (i == 0) ? 1 : i << 1) {
                    std::cout << "hist_unpruned_node_list_size[" << i << "]=" << hist_unpruned_node_list_size_[i] << std::endl;
                }
                std::cout << std::endl;

                for (uint32_t i = 0; i < hist_tree_depth_to_inst_.size(); i++) {
                    std::cout << "hist_tree_depth_to_inst[" << i << "]=" << hist_tree_depth_to_inst_[i] << std::endl;
                }
            }

        private:

            static const bool debug_construct_ = false;
            static const bool debug_populate_ = false;
            static const bool debug_find_ = false;
            static const bool debug_prune_ = false;

            std::string push_path_ (const std::string & path, uint32_t val) const
            {
                std::ostringstream s;
                s << path << "." << std::hex << val;
                return s.str();
            }

            const std::string print_name_ (const InstHandleT * inst) const
            {
                std::ostringstream s;
                s << inst->mnemonic << "-" << inst->instr_id;
                return s.str();
            }

            // *** Construction methods

            /**
             * \brief Count the leading zeros in n
             */
            uint32_t count_nlz_ (uint32_t n) const
            {
                return (n == 0 ? 0 : __builtin_clz(n));
            }

            /**
             * \brief Left shift x by n bits
             */
            uint32_t zl_shift_ (uint32_t x, uint32_t n) const
            {
                if (SPARTA_EXPECT_FALSE(n >= (sizeof(x) * 8))) {
                    return 0;
                }

                return x << n;
            }

            /**
             * \brief Make a mask with a single bit field of all ones
             * \param b Bit index to beginning of the bit field of all ones
             * \param e Bit index to one-past-the-end of the bit field of all ones
             * \return Mask with single bit field of all ones
             * \note This algorithm indexes the bits left to right. Bit 0 is the left-most bit.
             */
            uint32_t make_mask_ (uint32_t b, uint32_t e) const
            {
                if (SPARTA_EXPECT_FALSE(e == 0)) {
                    return 0;
                }

                if (SPARTA_EXPECT_FALSE(b == 0)) {
                    return zl_shift_ (-1U, (32 - e));
                }

                return zl_shift_ (-1U, (32 - b)) ^ zl_shift_ (-1U, (32 - e));
            }

            /**
             * \brief Extract the largest bit field of all ones from a number
             * \param n The number from which to extract the bit field
             * \param max_len The maximum length allowed for the largest bit field
             * \return Mask with only the largest bit field of all ones set
             */
            uint32_t extract_consecutive_ones (uint32_t n, uint32_t max_len = max_mask_bits) const
            {
                // NOTE: This algorithm indexes the bits left to right.  Bit 0
                // is the left-most bit.  Why?  I don't know.  This algorithm
                // must have been cut-and-pasted from somewhere.

                uint32_t p = 0;  // Bit index to beginning of the field we are currently processing
                uint32_t m = 0;  // Max number of all ones found
                uint32_t b = 0;  // Bit index to beginning of the max field
                uint32_t e = 0;  // Bit index to one-past-the-end of the max field
                while (n != 0) {
                    // Skip past leading zeros
                    uint32_t k = count_nlz_ (n);
                    p += k;
                    n <<= k;

                    // Count leading ones
                    k = count_nlz_ (~n);
                    if (k > max_len) {
                        // If we found a field of ones larger than the max_len,
                        // then just use the first max_len bits from this field.
                        b = p;
                        e = p + max_len;
                        break;
                    } else if (k > m) {
                        // If we found a field of ones larger than any previous
                        // that we found, then save the info for the field.
                        m = k;
                        b = p;
                        e = p + m;
                    }
                    p += k;
                    n <<= k;
                }

                return make_mask_ (b, e);
            }

            template<class InstVectT>
            uint32_t find_intersect_mask_ (const InstVectT & table, uint32_t unscanned_bits) const
            {
                for (uint32_t i = 0; i < table.size(); i++) {
                    unscanned_bits &= getAsPointer<typename InstVectT::value_type>(table[i])->mask;
                }

                return extract_consecutive_ones(unscanned_bits);
            }

            /**
             * \param table The table of all instructions
             * \param op The encoding that got us to this node in the tree (not the encoding stored in this node)
             * \param mask The mask that got us to this node in the tree (not the mask stored in this node)
             * \param rem_mask Mask of the opcode bits that we still need to consider
             */
            template<class InstVectT>
            uint32_t find_common_mask_ (const InstVectT & table, uint32_t op,
                                               uint32_t mask, uint32_t rem_mask) const
            {

                if (debug_construct_) {
                    std::cout << "find_common_mask:  op=0x" << std::hex << std::setfill('0') << std::setw(8) << op
                              << ", mask=0x" << std::hex << std::setfill('0') << std::setw(8) << mask
                              << ", rem_mask=0x" << std::hex << std::setfill('0') << std::setw(8) << rem_mask
                              << std::endl;
                }

                if (rem_mask != 0)
                {
                    uint32_t common_mask = rem_mask;
                    bool found = false;
                    for (uint32_t i = 0; i < table.size(); i++) {
                        // Get the inst we are working on
                        const InstHandleT * inst = getAsPointer<typename InstVectT::value_type>(table[i]);

                        // The union of all the inclusion and exclusion masks
                        // for this inst
                        uint32_t union_mask = 0;

                        // Compute inclusion info.  Definitely included means
                        // this inst has passed the inclusion test.  Possibly
                        // included means this inst has passed the inclusion
                        // test, so far, but there may or may not be more bits
                        // to examine before we make the final determination.
                        uint32_t mask0 = mask & inst->mask;
                        bool possibly_included = (inst->encoding & mask0) == (op & mask0);
                        bool definitely_included = possibly_included && ((inst->mask & rem_mask) == 0);
                        union_mask = inst->mask;

                        // Compute exclusion info.  Definitely excluded means
                        // this inst has passed the exclusion test.  Possibly
                        // excluded means this inst has passed the exclusion
                        // test, so far, but there may or may not be more bits
                        // to examine before we make the final determination.
                        bool possibly_excluded = false;
                        bool definitely_excluded = false;
                        for (auto& e : inst->exclude) {
                            uint32_t mask1 = mask & e.mask;
                            uint32_t possibly_excluded1 = ((e.encoding & mask1) == (op & mask1));
                            uint32_t definitely_excluded1 = possibly_excluded1 && ((e.mask & rem_mask) == 0);

                            if (false) {
                                std::cout << print_name_(inst)
                                          << ", op=" << std::hex << std::setfill('0') << std::setw(8) << op
                                          << ", mask=" << std::hex << std::setfill('0') << std::setw(8) << mask
                                          << ", exclude.encoding=" << std::hex << std::setfill('0') << std::setw(8) << e.encoding
                                          << ", exclude.mask=" << std::hex << std::setfill('0') << std::setw(8) << e.mask
                                          << ", pe=" << possibly_excluded1
                                          << ", de=" << definitely_excluded1
                                          << std::endl;
                            }
                            possibly_excluded = possibly_excluded || possibly_excluded1;
                            definitely_excluded = definitely_excluded || definitely_excluded1;
                            union_mask |= e.mask;
                        }

                        // Compute the final status of this inst for this node.
                        // Hit means that this inst maps to this node.  Miss
                        // means that this inst does not map or does not pass
                        // through to this node.  Unknown means we just don't
                        // know yet and we have to examine more bits.
                        bool hit = definitely_included && !possibly_excluded;
                        bool miss = !possibly_included || definitely_excluded;
                        bool unknown = !miss && !hit;

                        if (false) {
                            std::cout << "find_common_mask:  " << print_name_(inst)
                                      << ", pi=" << possibly_included
                                      << ", di=" << definitely_included
                                      << ", pe=" << possibly_excluded
                                      << ", de=" << definitely_excluded
                                      << ", m=" << miss
                                      << ", h=" << hit
                                      << ", u=" << unknown
                                      << ", union_mask=" << std::hex << std::setfill('0') << std::setw(8) << union_mask
                                      << std::endl;
                        }

                        // The only insts that we want to include in further
                        // analysis are the ones with an unknown status.  There
                        // is no reason to analyze the hit and miss insts
                        // further.
                        if (unknown) {
                            // Save the intersection of the masks for all of the
                            // unknown insts.  We use the intersection to find
                            // the mask bits that all of the unkown insts have
                            // in common so we can use them to tweeze the insts
                            // apart.
                            common_mask &= union_mask;
                            found = true;

                            if (debug_construct_) {
                                std::cout << "find_common_mask:  include mask: encoding=0x"
                                          << std::hex << std::setfill('0') << std::setw(8) << inst->encoding
                                          << ", mask=0x" << std::hex << std::setfill('0') << std::setw(8) << inst->mask
                                          << ", common=0x" << std::hex << std::setfill('0') << std::setw(8) << common_mask
                                          << ", name=" << print_name_(inst)
                                          << std::endl;
                            }
                        }
                    }

                    if (debug_construct_) {
                        std::cout << "find_common_mask:  -> 0x" << std::hex
                                  << std::setfill('0') << std::setw(8) << extract_consecutive_ones(common_mask)
                                  << std::endl;
                    }

                    if (common_mask == 0) {
                        std::cout << "Decoder " << name_ << " encountered a common mask is 0 error." << std::endl;
                    }

                    // Finally, extract the largest field of consecutive ones so
                    // we can separate the insts as fast as possible.
                    if (found && (common_mask != 0)) {
                        return extract_consecutive_ones(common_mask);
                    }
                }
                return 0;
            }

            uint32_t find_shift_ (uint32_t mask) const
            {
                // This uses a DeBrujin sequence to find the index
                // (0..31) of the least significant bit in iclass
                // Refer to Leiserson's bitscan algorithm:
                // http://chessprogramming.wikispaces.com/BitScan
                static const uint32_t index32[32] = {
                    0,   1, 28,  2, 29, 14, 24, 3,
                    30, 22, 20, 15, 25, 17,  4, 8,
                    31, 27, 13, 23, 21, 19, 16, 7,
                    26, 12, 18,  6, 11,  5, 10, 9
                };

                static const uint32_t debruijn32 = 0x077CB531U;

                return index32[((mask & -mask) * debruijn32) >> 27];
            }

            template<class InstVectT>
            Node * construct_ (const InstVectT & table, std::string path, uint32_t op, uint32_t op_mask, uint32_t rem_mask)
            {
                if (debug_construct_) {
                    std::cout << "construct:  [" << path << "] - enter"
                              << ",  op=0x" << std::hex << std::setfill('0') << std::setw(8) << op
                              << ", op_mask=0x" << std::hex << std::setfill('0') << std::setw(8) << op_mask
                              << ", rem=0x" << std::hex << std::setfill('0') << std::setw(8) << rem_mask
                              << std::endl;
                }

                if (rem_mask == 0) {
                    if (debug_construct_) {
                        std::cout << "construct:  [" << path << "] - no rem_mask so return null" << std::endl;
                    }
                    return nullptr;
                }

                uint32_t mask = find_common_mask_ (table, op, op_mask, rem_mask);
                if (mask == 0) {
                    if (debug_construct_) {
                        std::cout << "construct:  [" << path << "] - no common_mask so return null" << std::endl;
                    }
                    return nullptr;
                }

                uint32_t shift = find_shift_ (mask);

                Node *n = new Node;
                n->op_mask = mask;
                n->op_shift = shift;
                n->list_size = (mask >> shift) + 1;

                if (debug_construct_) {
                    std::cout << "construct:  [" << path << "] - add node - " << n << std::endl;
                }

                n->list = new NodePtr[n->list_size];

                for (uint32_t m = 0; m < n->list_size; ++m) {
                    n->list[m] = construct_ (table, push_path_(path, m), op | (m << shift), op_mask | mask, rem_mask & ~mask);
                }

                return n;
            }

            template<class InstVectT>
            Node * construct_and_populate_ (const InstVectT & table)
            {
                for (const auto & inst : table) {
                    Node * node = new Node;
                    node->inst = getAsPointer<typename InstVectT::value_type>(inst);
                    inst_nodes_.push_back (node);
                }
                undef_node_ = new Node;

                // Create and populate the tree
                uint32_t acc_op = 0x00000000;
                uint32_t acc_mask = 0x00000000;
                uint32_t rem_mask = 0xffffffff;
                uint32_t op_mask = find_intersect_mask_ (table, rem_mask);
                if (op_mask == 0) return nullptr;
                uint32_t op_shift = find_shift_ (op_mask);
                uint32_t list_size = (op_mask >> op_shift) + 1;

                Node * root = new Node;
                root->op_mask = op_mask;
                root->op_shift = op_shift;
                root->list_size = list_size;
                root->list = new NodePtr[root->list_size];
                for (uint32_t m = 0; m < root->list_size; ++m) {
                    root->list[m] = construct_ (table, push_path_("root", m), acc_op | (m << op_shift), acc_mask | op_mask, rem_mask & ~op_mask);
                }
                populate_recurse_ (inst_nodes_, "root", root, acc_op, acc_mask, rem_mask);

                return root;
            }

            template<class InstVectT>
            Node * construct_and_populate_arm32_ (const InstVectT & table)
            {
                for(uint32_t i = 0; i < table.size(); ++i) {
                    Node * node = new Node;
                    node->inst = getAsPointer<typename InstVectT::value_type>(table[i]);
                    inst_nodes_.push_back (node);
                }
                undef_node_ = new Node;

                // Create and populate the tree for conditional instructions
                uint32_t acc_op = 0x00000000;
                uint32_t acc_mask = 0xf0000000;
                uint32_t rem_mask = 0x0fffffff;
                uint32_t op_mask = 0x0ff00000;
                uint32_t op_shift = 20;
                uint32_t list_size = 256;

                Node * cond_root = new Node;
                cond_root->op_mask = op_mask;
                cond_root->op_shift = op_shift;
                cond_root->list_size = list_size;
                cond_root->list = new NodePtr[cond_root->list_size];
                for (uint32_t m = 0; m < list_size; ++m) {
                    cond_root->list[m] = construct_ (table, push_path_("root.0", m), acc_op | (m << op_shift), acc_mask | op_mask, rem_mask & ~op_mask);
                }
                populate_recurse_ (inst_nodes_, "root.0", cond_root, acc_op, acc_mask, rem_mask);

                // Create and populate the tree for unconditional instructions
                acc_op = 0xf0000000;
                acc_mask = 0xf0000000;
                rem_mask = 0x0fffffff;
                op_mask = 0x0ff00000;
                op_shift = 20;
                list_size = 256;

                Node * uncond_root = new Node;
                uncond_root->op_mask = op_mask;
                uncond_root->op_shift = op_shift;
                uncond_root->list_size = list_size;
                uncond_root->list = new NodePtr[uncond_root->list_size];
                for (uint32_t m = 0; m < list_size; ++m) {
                    uncond_root->list[m] = construct_ (table, push_path_("root.1", m), acc_op | (m << op_shift), acc_mask | op_mask, rem_mask & ~op_mask);
                }
                populate_recurse_ (inst_nodes_, "root.1", uncond_root, acc_op, acc_mask, rem_mask);

                // Create the root tree node and populate it with conditional and unconditional trees
                Node * root = new Node;
                root->op_mask = 0xf0000000;
                root->op_shift = 28;
                root->list_size = 16;
                root->list = new NodePtr[root->list_size];
                for (uint32_t i = 0; i < 15; i++) {
                    root->list[i] = cond_root;
                }
                root->list[15] = uncond_root;

                return root;
            }

            // *** Populate methods

            void populate_hits_ (const NodePtrVect & in_nodes,
                                 const std::string & path,
                                 uint32_t acc_op,
                                 uint32_t acc_mask,
                                 uint32_t rem_mask,
                                 NodePtrVect & out_nodes)
            {
                if (debug_populate_) {
                    std::cout << "populate_hits: "
                              << " path=" << path
                              << " acc_op=0x" << std::hex << std::setfill('0') << std::setw(8) << acc_op
                              << " acc_mask=0x" << std::hex << std::setfill('0') << std::setw(8) << acc_mask
                              << " rem_mask=0x" << std::hex << std::setfill('0') << std::setw(8) << rem_mask
                              << std::endl;
                }

                for (auto & node : in_nodes)
                {
                    if (debug_populate_) {
                        std::cout << "populate_hits: "
                                  << " testing node=" << print_name_(node->inst) << std::endl;
                    }

                    // Decide whether the inst is possibly/definitely included
                    uint32_t mask0 = acc_mask & node->inst->mask;
                    bool possibly_included = (node->inst->encoding & mask0) == (acc_op & mask0);
                    bool definitely_included = possibly_included && ((node->inst->mask & rem_mask) == 0);

                    if (debug_populate_) {
                        std::cout << "populate_hits: "
                                  << " testing node=" << print_name_(node->inst)
                                  << " possibly_included=" << possibly_included
                                  << " definitely_included=" << definitely_included
                                  << std::endl;
                    }

                    // Decide whether the inst is possibly/definitely excluded
                    bool possibly_excluded = false;
                    bool definitely_excluded = false;
                    for (auto & exclude : node->inst->exclude)
                    {
                        uint32_t mask1 = acc_mask & exclude.mask;
                        uint32_t possibly_excluded1 = ((exclude.encoding & mask1) == (acc_op & mask1));
                        uint32_t definitely_excluded1 = possibly_excluded1 && ((exclude.mask & rem_mask) == 0);
                        possibly_excluded = possibly_excluded || possibly_excluded1;
                        definitely_excluded = definitely_excluded || definitely_excluded1;
                    }

                    if (debug_populate_) {
                        std::cout << "populate_hits: "
                                  << " testing node=" << print_name_(node->inst)
                                  << " possibly_excluded=" << possibly_excluded
                                  << " definitely_excluded=" << definitely_excluded
                                  << std::endl;
                    }

                    //bool hit = definitely_included && !possibly_excluded;
                    bool hit = possibly_included && !definitely_excluded;

                    if (hit)
                    {
                        if (debug_populate_) {
                            std::cout << "populate_hits:  pushing " << print_name_(node->inst) << std::endl;
                        }
                        out_nodes.push_back (node);
                    }
                }
            }

            Node * populate_recurse_ (const NodePtrVect & in_nodes,
                                      const std::string & path,
                                      Node * node,
                                      uint32_t acc_op,
                                      uint32_t acc_mask,
                                      uint32_t rem_mask)
            {
                Node * retval = nullptr;

                // It's a leaf node
                if (node == nullptr)
                {
                    if (in_nodes.size() == 0) {
                        // No hits.  Return undef leaf node
                        retval = undef_node_;
                    } else if (in_nodes.size() == 1) {
                        // One hit.  Return instruction leaf node
                        retval = in_nodes[0];
                    } else {
                        // Multiple hits.  This is a bug.  Print error and abort.
                        std::cout << "Decoder " << name_ << " encountered a multihit error." << std::endl;
                        for (const auto & hit_node : in_nodes) {
                            std::cout << print_name_(hit_node->inst) << std::endl;
                        }
                        print_tree();
                        assert (false);
                    }
                }

                // It's an interior node
                else
                {
                    for (uint32_t i = 0; i < node->list_size; i++)
                    {
                        std::string new_path = push_path_ (path, i);

                        NodePtrVect out_nodes;
                        populate_hits_ (in_nodes, new_path, acc_op | (i << node->op_shift),
                                        acc_mask | node->op_mask, rem_mask & ~node->op_mask, out_nodes);

                        Node * leaf = populate_recurse_ (out_nodes, new_path, node->list[i], acc_op | (i << node->op_shift),
                                                             acc_mask | node->op_mask, rem_mask & ~node->op_mask);

                        if (leaf != nullptr) node->list[i] = leaf;
                    }
                }

                return retval;
            }

            // *** Prune methods

            void set_pruned_ (bool pruned)
            {
                iterator itr_end = end();
                iterator itr = begin();

                while (itr != itr_end)
                {
                    itr->pruned = pruned;
                    ++itr;
                }
            }

            void prune_ ()
            {
                iterator itr1 = begin();
                iterator itr_end = end();

                set_pruned_ (false);

                while (itr1 != itr_end)
                {
                    if (debug_prune_ == true) {
                        std::cout << "checking: " << "itr1=" << itr1.getPath() << " " << *itr1 << std::endl;
                    }

                    if (itr1->pruned == false)
                    {
                        iterator itr2 = itr1;
                        ++itr2;

                        while (itr2 != itr_end)
                        {
                            if (itr2->pruned == false)
                            {
                                if (*itr1 == *itr2)
                                {
                                    if (debug_prune_ == true) {
                                        std::cout << "pruning: " << "itr2=" << itr2.getPath() << " " << *itr2 << std::endl;
                                    }

                                    itr2.replaceNode (&*itr1);

                                    // This can be removed after I remove explicit leaf pruning
                                    itr1->pruned = true;
                                }
                            }
                            ++itr2;
                        }
                    }

                    itr1->pruned = true;
                    ++itr1;
                }
            }

            // *** Lookup methods

            Node * find_ (Node * n, uint32_t op) const
            {
                if (n->isLeaf() == true)
                {
                    return n;
                }

                else
                {
                    uint32_t m = (op & n->op_mask) >> n->op_shift;
                    assert (m < n->list_size);
                    return find_ (n->list[m], op);
                }
            }

            Node * find_no_recurse_ (Node * n, uint32_t op) const
            {
                while (n->isLeaf() == false)
                {
                    uint32_t m = (op & n->op_mask) >> n->op_shift;
                    assert (m < n->list_size);
                    n = n->list[m];
                }

                return n;
            }

            Node * find_ (std::string path, Node * n, uint32_t op) const
            {
                if (n->isLeaf() == true)
                {
                    return n;
                }

                else
                {
                    uint32_t m = (op & n->op_mask) >> n->op_shift;
                    assert (m < n->list_size);
                    return find_ (push_path_(path, m), n->list[m], op);
                }
            }

            // *** Print methods

            void print_tree_ (std::string path, Node * n) const
            {
                std::cout << "print_tree: "
                          << " path=" << path
                          << " node=[" << n << "]"
                          << std::endl;

                if (n->isLeaf() == true)
                {
                    return;
                }

                else
                {
                    for (uint32_t i = 0; i < n->list_size; i++) {
                        print_tree_ (push_path_(path, i), n->list[i]);
                    }
                }
            }

            void print_tree_depth_first_ ()
            {
                iterator itr_end = end();
                iterator itr = begin();

                while (itr != itr_end)
                {
                    std::cout << itr.get_path() << std::endl;
                    ++itr;
                }
            }

            void print_stats_ (std::string path, Node * n, uint32_t depth = 0)
            {
                hist_node_list_size_[n->list_size]++;
                if (n->pruned == false) hist_unpruned_node_list_size_[n->list_size]++;

                // We are at a leaf node, return
                if (n->isLeaf() == true)
                {
                    if (n->inst == nullptr) {
                        num_undef_nodes_++;
                        if (n->pruned == true) num_pruned_undef_nodes_++;
                    } else {
                        num_inst_nodes_++;
                        if (n->pruned == true) num_pruned_inst_nodes_++;
                        hist_tree_depth_to_inst_[depth]++;
                    }
                    n->pruned = true;
                    return;
                }

                // We are at an interior node, continue down the tree
                else
                {
                    num_interior_nodes_++;;
                    if (n->pruned == true) num_pruned_interior_nodes_++;
                    n->pruned = true;
                    for (uint32_t i = 0; i < n->list_size; i++) {
                        print_stats_ (push_path_(path, i), n->list[i], depth + 1);
                    }
                }
            }

            // *** Deallocate methods

            void deallocate_memory_ ()
            {
                // Go through the tree nodes, inst nodes, and undef
                // nodes setting pruned to false at each node
                set_pruned_ (false);
                for (auto & node : inst_nodes_) {
                    node->pruned = false;
                }
                undef_node_->pruned = false;

                // Go through the tree again and collect all the nodes
                // we plan to deallocate.  Use the pruned flag to make
                // sure we don't put nodes on the list twice.
                NodePtrVect delete_list;
                iterator itr_end = end();
                iterator itr = begin();
                while (itr != itr_end) {
                    if (itr->pruned == false) {
                        delete_list.push_back (&*itr);
                        itr->pruned = true;
                    }
                    ++itr;
                }

                // Add any missing inst nodes to the delete list
                for (auto & node : inst_nodes_) {
                    if (node->pruned == false) {
                        delete_list.push_back (node);
                        node->pruned = true;
                    }
                }

                // Add the undef node to the delete list
                if (undef_node_->pruned == false) {
                        delete_list.push_back (undef_node_);
                        undef_node_->pruned = true;
                }

                // Now delete the nodes on the list
                for (auto & node : delete_list) {
                    delete [] node->list;
                    delete node;
                }
            }

        private:

            std::string name_;

            uint64_t num_undef_nodes_;
            uint64_t num_pruned_undef_nodes_;
            uint64_t num_inst_nodes_;
            uint64_t num_pruned_inst_nodes_;
            uint64_t num_interior_nodes_;
            uint64_t num_pruned_interior_nodes_;
            std::vector<uint32_t> hist_node_list_size_;
            std::vector<uint32_t> hist_unpruned_node_list_size_;
            std::vector<uint32_t> hist_tree_depth_to_inst_;

            Node * root_node_;

            Node * undef_node_;

            // A table of leaf nodes for every instruction.  This is
            // so we can reuse the instruction leaf node many times
            // rather than allocating a new one.
            NodePtrVect inst_nodes_;
        };

        template<class InstT>
        inline std::ostream & operator<< (std::ostream & os, const typename Decoder<InstT>::Node & n)
        {
            os << "inst=";
            if (n.inst == nullptr) {
                os << "nullptr";
            } else {
                os << n.inst->mnemonic << "_" << n.inst;
            }
            os << " op_mask=0x" << std::hex << std::setfill('0') << std::setw(8) << n.op_mask
               << " op_shift=" << std::dec << static_cast<uint32_t>(n.op_shift)
               << " list_size=" << n.list_size
               << " list=" << n.list
               << " pruned=" << n.pruned;

            return os;
        };

        template<class InstT>
        inline std::ostream & operator<< (std::ostream & os, const typename Decoder<InstT>::Node * n) {
            assert (n != nullptr);
            os << *n;
            return os;
        }


    }
}

#endif
