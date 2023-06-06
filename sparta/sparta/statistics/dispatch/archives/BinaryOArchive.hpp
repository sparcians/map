// <BinaryOArchive> -*- C++ -*-

#pragma once

#include "sparta/statistics/dispatch/archives/ArchiveSink.hpp"
#include "sparta/statistics/dispatch/archives/RootArchiveNode.hpp"
#include "sparta/statistics/dispatch/archives/ArchiveNode.hpp"

#include <fstream>
#include <filesystem>

#include <boost/archive/binary_oarchive.hpp>

namespace sparta {
namespace statistics {

/*!
 * \brief Use a binary archive file as a destination for
 * statistics values.
 */
class BinaryOArchive : public ArchiveSink
{
public:
    //One-time initialization. Open output files and serialize
    //the archive tree to a metadata file for future use.
    void initialize() override {
        const std::string & path = getPath();
        const std::string & subpath = getSubpath();
        createArchiveDirectory_(path, subpath);
        openBinaryArchiveFile_(path, subpath);

        //If this sink has a root archive node attached
        //to it, we should serialize it to disk now. This
        //will let us reconstruct the same archive tree later
        //on when we want to attach to an archive outside of
        //a simulation.
        RootArchiveNode * root = getRoot_();
        if (root) {
            serializeArchiveTree_(*root, path, subpath);
        }
    }

    //Copy metadata files from one archive to another. This may
    //occur during actions such as saving/re-saving an archive to
    //a different directory.
    void copyMetadataFrom(const ArchiveStream * stream) override
    {
        const std::string & source_path = stream->getPath();
        const std::string & source_subpath = stream->getSubpath();
        const std::string source_full_path = source_path + "/" + source_subpath;

        const std::string source_tree_filename = source_full_path + "/archive_tree.bin";
        if (!std::filesystem::exists(source_tree_filename)) {
            throw SpartaException(
                "Metadata file not available for read: ") << source_tree_filename;
        }

        const std::string & dest_path = getPath();
        const std::string & dest_subpath = getSubpath();
        const std::string dest_full_path = dest_path + "/" + dest_subpath;

        const std::string dest_tree_filename = dest_full_path + "/archive_tree.bin";
        if (std::filesystem::exists(dest_tree_filename)) {
            std::filesystem::remove(dest_tree_filename);
        }

        std::filesystem::copy_file(source_tree_filename, dest_tree_filename);
    }

    //Put one vector of statistics data values into the binary file
    void sendToSink(const std::vector<double> & values) override {
        if (!values.empty()) {
            binary_fout_.write(reinterpret_cast<const char*>(
                &values[0]), values.size() * sizeof(double));
        }
    }

    //Flush the file buffer. This action is performed whenever the
    //archive system needs to synchronize all data sources/sinks,
    //for example if a call is made to one of the "getData()" methods
    //during a live simulation.
    void flush() override {
        binary_fout_.flush();
    }

private:
    void createArchiveDirectory_(const std::string & path,
                                 const std::string & subpath) const
    {
        std::filesystem::create_directories(path + "/" + subpath);
    }

    void openBinaryArchiveFile_(const std::string & path,
                                const std::string & subpath)
    {
        const std::string binary_filename = path + "/" + subpath + "/values.bin";
        if (std::filesystem::exists(binary_filename)) {
            std::filesystem::remove(binary_filename);
        }

        binary_fout_.open(binary_filename, std::ios::binary);
        if (!binary_fout_) {
            throw SpartaException(
                "Unable to open archive file for write: ") << binary_filename;
        }

        auto root = getRoot_();
        if (root) {
            root->setMetadataValue("output_filename", binary_filename);
        }
    }

    //Serialize the root archive node (and all of its children
    //and metadata) to an auxiliary file in the archive directory.
    //We need this metadata later in order to rebuild this tree
    //when we connect to archives offline (no simulation, no
    //streaming statistics... just a Python shell, for instance)
    void serializeArchiveTree_(const RootArchiveNode & root,
                               const std::string & path,
                               const std::string & subpath) const
    {
        const std::string filename = path + "/" + subpath + "/archive_tree.bin";
        if (std::filesystem::exists(filename)) {
            std::filesystem::remove(filename);
        }

        std::ofstream fout(filename, std::ios::binary);
        if (!fout) {
            throw SpartaException(
                "Unable to open archive file for write: ") << filename;
        }

        boost::archive::binary_oarchive oa(fout);
        oa << root;
    }

    std::ofstream binary_fout_;
};

} // namespace statistics
} // namespace sparta
