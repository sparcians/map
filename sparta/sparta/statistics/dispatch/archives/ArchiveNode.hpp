// <ArchiveNode> -*- C++ -*-

#pragma once

#include "sparta/app/ReportDescriptor.hpp"

#include <boost/serialization/vector.hpp>

namespace sparta {
namespace statistics {

class ArchiveNode;
class ArchiveDataSeries;
class RootArchiveNode;

/*!
 * \brief When a simulation is configured to archive its
 * statistics values, it will build a subset of its device
 * tree to organize the statistics. The analogy is that a
 * TreeNode is loosely the same as an ArchiveNode:
 *
 *     top.core0.rob.stats.total_number_retired  (device tree)
 *     top.core0.rob.total_number_retired (archive tree)
 *     ...
 *
 * The archive tree will strictly be a subset of the full
 * device tree.
 */
class ArchiveNode
{
public:
    //! Default constructor should only be used by Boost
    //! serialization code. Boost will build up the archive
    //! tree from a metadata file piecemeal. First it will
    //! create an ArchiveNode with no ctor arguments, then
    //! it will fill in the rest (name, children, ...)
    ArchiveNode() = default;

    //! Construct a named node
    ArchiveNode(const std::string & name) :
        name_(name)
    {}

    //! Archive trees are built with the help of the
    //! ReportStatisticsHierTree class. That class is
    //! templated on ArchiveNode, and needs a constructor
    //! that takes a name and an SI. For our case, we don't
    //! use the SI for anything, but we still have this
    //! constructor to make ReportStatisticsHierTree happy.
    ArchiveNode(const std::string & name,
                const StatisticInstance *) :
        ArchiveNode(name)
    {}

    //! See the comment above for the ArchiveNode(string, SI*)
    //! constructor. This constructor is here just to make the
    //! ArchiveNode class conform to the ReportStatisticsHierTree
    //! template code so we can reuse that tree builder class.
    ArchiveNode(const std::string & name,
                const Report *) :
        ArchiveNode(name)
    {}

    virtual ~ArchiveNode() {}

    //! Return the name that this node was originally created with
    const std::string & getName() const {
        return name_;
    }

    //! In every archive tree, all leaf nodes need to know their
    //! "leaf index", which is a 0-based index assigned in depth-
    //! first traversal of the tree. For example:
    //!
    //!               A
    //!         -------------
    //!          |         |
    //!          B         C
    //!                 -------
    //!                  |   |
    //!                  D   E
    //!
    //! Here, nodes A/C would not have a leaf index, because
    //! they are not leaves. But the others would have:
    //!
    //!      Node:        Leaf index:
    //!      ------       ------------
    //!      B            0
    //!      D            1
    //!      E            2
    //!
    //! Leaves use this index to figure out their byte offset
    //! in the underlying contiguous array of SI values.
    void setLeafIndex(const size_t index) {
        leaf_index_ = index;
    }

    std::vector<std::shared_ptr<ArchiveNode>> & getChildren() {
        return children_;
    }

    const std::vector<std::shared_ptr<ArchiveNode>> & getChildren() const {
        return children_;
    }

    void setParent(std::shared_ptr<ArchiveNode> & parent) {
        setParent(parent.get());
    }

    void setParent(ArchiveNode * parent) {
        sparta_assert(parent_ == nullptr, "Cannot reassign parent archive nodes");
        parent_ = parent;
    }

    //! All archive trees have a root node. This is different
    //! than a regular ArchiveNode in that the RootArchiveNode:
    //!
    //!   - Holds onto all shared metadata that is common to the tree nodes
    //!   - Holds an ArchiveController object which can be used to synchronize
    //!     the archive sources/sinks and save the archives to a specific directory
    //!
    //! This is lazily evaluated and then cached.
    RootArchiveNode * getRoot();

    //! Only leaf nodes have any data. Ask this node if it is a leaf.
    //! This is different from "do you have any data?" - leaves can
    //! have no data if the simulation hasn't even logged one report
    //! update yet.
    bool isLeaf() const {
        return children_.empty();
    }

    //! Only nodes that are *both* leaves, and that have had their
    //! leaf index assigned to them, are able to access SI data.
    bool canAccessDataSeries() const {
        return isLeaf() && leaf_index_.isValid();
    }

    //! Ask a leaf node to create an ArchiveDataSeries object. This
    //! object can be used to get individual SI values out of the
    //! archive. This method will throw if canAccessDataSeries()
    //! is FALSE.
    ArchiveDataSeries * createDataSeries();

    //! Returns the total number of leaf nodes from this node
    //! on down. For example:
    //!
    //!               A
    //!         -------------
    //!          |         |
    //!          B         C
    //!                 -------
    //!                  |   |
    //!                  D   E
    //!
    //!      Node:        Num leaves:
    //!      ------       ------------
    //!      A            3
    //!      B            1
    //!      C            2
    //!      D            1
    //!      E            1
    size_t getTotalNumLeaves() const {
        size_t num_leaves = (children_.empty() ? 1 : 0);
        for (const auto & child : children_) {
            recursGetTotalNumLeaves_(*child, num_leaves);
        }
        return num_leaves;
    }

protected:
    //! This property is protected so that subclasses can
    //! write their Boost serialization routines as simply
    //! as "archive & name_" and not fuss with setter and
    //! getter methods.
    std::string name_;

    //! Same as 'name_', the children_ vector is protected
    //! to make the subclass Boost serialization routines
    //! easier to manage.
    std::vector<std::shared_ptr<ArchiveNode>> children_;

private:
    void recursGetTotalNumLeaves_(
        const ArchiveNode & node, size_t & num_leaves) const
    {
        if (node.children_.empty()) {
            ++num_leaves;
        } else {
            for (const auto & child : node.children_) {
                recursGetTotalNumLeaves_(*child, num_leaves);
            }
        }
    }

    //! The archive directories contain the following structure:
    //!    - ArchiveDir
    //!       - archive_tree.bin
    //!       - values.bin
    //!    - AnotherArchiveDir
    //!       - archive_tree.bin
    //!       - values.bin
    //!
    //! We let Boost call this same serialize() method whether
    //! we are writing to disk or reading from disk.
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive & ar, const unsigned int) {
        //Simple data types like strings and ints can
        //call the operator& method directly
        ar & name_;

        //Our children variable is a vector<shared_ptr<ArchiveNode>>
        //and Boost will call the serialize() method for each of those
        //shared_ptr's. That method is at the bottom of this file. Shortly
        //after calling serialize(Archive & ar, std::shared_ptr<ArchiveNode> & node)
        //Boost will call this method we are in right now.
        ar & children_;

        //There are only so many serialization routines that you
        //get for free (std::string, vectors/lists/maps of simple
        //types, PODs, etc.) but ValidValue<T> is not one of them.
        //This serialize() method is at the bottom of this file.
        ar & leaf_index_;
    }

    RootArchiveNode * cached_root_ = nullptr;
    ArchiveNode * parent_ = nullptr;
    utils::ValidValue<size_t> leaf_index_;
    std::unique_ptr<ArchiveDataSeries> ar_data_series_;
};

/*!
 * \brief Wrapper around a leaf ArchiveNode's data. Owns
 * a back pointer to the top RootArchiveNode in its tree,
 * which it uses to synchronize the data source and data
 * sink with every call to its data access APIs.
 */
class ArchiveDataSeries
{
public:
    ArchiveDataSeries(const size_t leaf_index,
                      RootArchiveNode * root) :
        leaf_index_(leaf_index),
        root_(root)
    {
        sparta_assert(root_ != nullptr);
    }

    //! Get just one SI value at the data series index.
    //! Throws if out of range.
    inline double getValueAt(const size_t idx) {
        synchronize_();
        return data_values_.at(idx);
    }

    //! Get the entire SI data array.
    const std::vector<double> & getDataReference() {
        synchronize_();
        return data_values_;
    }

    //! Get the size of the SI data array.
    size_t size() {
        synchronize_();
        return data_values_.size();
    }

    //! See if there are any SI data values at all
    bool empty() {
        synchronize_();
        return size() == 0;
    }

private:
    //Read in the data archive and flip the dirty flag
    //back to "not dirty"
    void synchronize_();
    void readAllDataFromArchive_();

    std::vector<double> data_values_;
    const size_t leaf_index_;
    RootArchiveNode * root_ = nullptr;
};

} // namespace statistics
} // namespace sparta

namespace boost {
namespace serialization {

//! Serialization of std::shared_ptr<ArchiveNode>
template <class Archive>
void serialize(Archive & ar,
               std::shared_ptr<sparta::statistics::ArchiveNode> & node,
               const unsigned int)
{
    if (node == nullptr) {
        node.reset(new sparta::statistics::ArchiveNode);
    }
    ar & *node;
}

//! Serialization of ValidValue<size_t>
template <class Archive>
void serialize(Archive & ar,
               sparta::utils::ValidValue<size_t> & vv,
               const unsigned int)
{
    if (vv.isValid()) {
        ar & vv.getValue();
    } else {
        size_t value = 0;
        ar & value;
        vv = value;
    }
}

} // namespace serialization
} // namespace boost

