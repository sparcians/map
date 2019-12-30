// <RootArchiveNode> -*- C++ -*-

#ifndef __SPARTA_STATISTICS_ROOT_ARCHIVE_NODE_H__
#define __SPARTA_STATISTICS_ROOT_ARCHIVE_NODE_H__

#include "sparta/statistics/dispatch/archives/ArchiveNode.hpp"
#include "sparta/app/ReportDescriptor.hpp"

#include <boost/serialization/map.hpp>
#include <boost/serialization/vector.hpp>

namespace sparta {
namespace statistics {

class ArchiveNode;
class ArchiveController;

/*!
 * \brief There is one of these root node objects at the top
 * of each report's archive. The hierarchy looks like this:
 *
 *      archives        StatisticsArchives
 *        out_csv         RootArchiveNode
 *          top             ArchiveNode
 *            core0         ArchiveNode
 *        out_json        RootArchiveNode
 *          top             ArchiveNode
 *      ...             ...
 *
 * This object holds onto shared metadata common to all of
 * its child nodes, as well as a controller object used to
 * interact with the data source on this archive's behalf
 * (such as saving the entire archive to a new directory).
 */
class RootArchiveNode : public ArchiveNode
{
public:
    RootArchiveNode() = default;

    explicit RootArchiveNode(const std::string & name) :
        ArchiveNode(name)
    {}

    void initialize() {
        size_t num_leaves = 0;
        recursGetNumLeafChildren_(*this, num_leaves);
        total_num_leaves_ = num_leaves;
    }

    //! Metadata
    void setMetadata(const app::NamedExtensions & metadata) {
        metadata_ = metadata;
    }

    template <typename MetadataT>
    void setMetadataValue(const std::string & name, const MetadataT & value) {
        boost::any any_value(value);
        metadata_[name] = any_value;
    }

    template <typename MetadataT>
    const MetadataT & getMetadataValue(const std::string & name) const {
        auto iter = metadata_.find(name);
        if (iter == metadata_.end()) {
            throw SpartaException("Metadata does not exist: ") << name;
        }
        try {
            return boost::any_cast<const MetadataT &>(iter->second);
        } catch (const boost::bad_any_cast &) {
            throw SpartaException("Metadata named '")
                << name << "' does exist, but is not of type '"
                << typeid(MetadataT).name() << "'";
        }
    }

    template <typename MetadataT>
    const MetadataT * tryGetMetadataValue(const std::string & name) const {
        auto iter = metadata_.find(name);
        if (iter == metadata_.end()) {
            return nullptr;
        }
        try {
            return &boost::any_cast<const MetadataT &>(iter->second);
        } catch (const boost::bad_any_cast &) {
            return nullptr;
        }
    }

    size_t getTotalNumLeaves() const {
        return total_num_leaves_;
    }

    //! Saving and synchronizing archives are done through the given
    //! archive controller. How this controller performs these actions
    //! will depend on whether this archive is for a live simulation,
    //! or offline. But the archive node classes do not have to know
    //! about which mode we are in.
    void setArchiveController(const std::shared_ptr<ArchiveController> & controller) {
        archive_controller_ = controller;
    }
    void saveTo(const std::string & dir);
    bool synchronize();

private:
    void recursGetNumLeafChildren_(
        const RootArchiveNode & node, size_t & num_leaves) const
    {
        if (node.getChildren().empty()) {
            ++num_leaves;
        } else {
            for (const auto & child : node.getChildren()) {
                num_leaves += child->getTotalNumLeaves();
            }
        }
    }

    //! Serialization routine for writing/reading this
    //! node to/from an archive's metadata file
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive & ar, const unsigned int) {
        //Keep in mind that this one method is called for both
        //serialization to disk, and deserialization from disk.

        //Read/write node name
        ar & name_;

        //Read/write children vector
        ar & children_;

        //Read/write the total_num_leaves_ value. This serialize()
        //method is defined at the bottom of ArchiveNode.h
        ar & total_num_leaves_;

        //Serialize trigger metadata
        if (!metadata_.empty()) {
            //Boost has called this serialize() method, and since
            //our metadata map is not empty, that means we are now
            //*writing* to disk. This map is a bunch of boost::any's
            //which cannot be serialized as easily as a std::string
            //or size_t, etc. We will have to pick it apart into a
            //std::map<std::string, std::string> and just serialize
            //that data structure to get around it.
            //
            //Note that we are using a map and not an unordered_map
            //because older Boost versions do not support serialization
            //of unordered_map. This map is never larger than 4 or 5
            //items, so performance is not really different either way.
            auto iter = metadata_.find("trigger");
            sparta_assert(iter != metadata_.end());
            const app::TriggerKeyValues & source_kvs =
                boost::any_cast<app::TriggerKeyValues&>(iter->second);

            std::map<std::string, std::string> dest_kvs;
            for (const auto & kv : source_kvs) {
                dest_kvs[kv.first] = kv.second;
            }

            //Write the map<string,string> to disk
            ar & dest_kvs;
        } else {
            //Read the trigger information from disk, and store
            //it in our metadata. This map<string,string> is the
            //same data structure we used when we serialized this
            //node to disk.
            std::map<std::string, std::string> source_kvs;
            ar & source_kvs;

            //The metadata variable's data structure is an unordered_map,
            //however, so we have to convert the key-value pairs accordingly.
            //Note that if SPARTA and all downstream users/repos start using a
            //new enough Boost version, this could be switched to serialize
            //unordered_map<string,string> directly.
            //
            //           --> Boost v1.56 and above required <--
            //
            //--------------------------------------------------------------
            app::TriggerKeyValues dest_kvs;
            for (const auto & kv : source_kvs) {
                dest_kvs[kv.first] = kv.second;
            }
            setMetadataValue("trigger", dest_kvs);
        }
    }

    std::shared_ptr<ArchiveController> archive_controller_;
    utils::ValidValue<size_t> total_num_leaves_;
    app::NamedExtensions metadata_;
};

} // namespace statistics
} // namespace sparta

#endif
