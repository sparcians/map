// <StatisticsArchives> -*- C++ -*-

#ifndef __SPARTA_STATISTICS_ARCHIVE_H__
#define __SPARTA_STATISTICS_ARCHIVE_H__

#include "sparta/statistics/dispatch/StatisticsHierRootNodes.hpp"
#include "sparta/statistics/dispatch/archives/RootArchiveNode.hpp"
#include "sparta/statistics/dispatch/archives/ArchiveController.hpp"

#include <boost/filesystem.hpp>
#include <boost/archive/binary_iarchive.hpp>

namespace sparta {
namespace statistics {

/*!
 * \brief This class owns and coordinates a group of named archives
 */
class StatisticsArchives : public StatisticsHierRootNodes<RootArchiveNode>
{
public:
    //! Default (empty) archives objects are created from
    //! simulations. The archive hierarchy is inferred
    //! from sparta::Report and sparta::StatisticInstance
    //! objects during simulation.
    StatisticsArchives() = default;

    //! This constructor overload is used when you want to
    //! attach an archives object to an existing database.
    //! This can occur offline; the archive hierarchy is
    //! inferred from a metadata file in this database
    //! directory which describes the archive tree.
    explicit StatisticsArchives(const std::string & db_dir)
    {
        namespace bfs = boost::filesystem;
        bfs::path p(db_dir);

        if (!bfs::is_directory(p)) {
            throw SpartaException(
                "The path given is not a directory: ") << db_dir;
        }

        //Archives are organized as follows:
        //
        //   db_directory
        //      db_subdirectory     <->    foo.csv
        //      db_subdirectory     <->    bar.json
        //      ...                        ...
        //
        //We need to loop over the subdirectories, and create
        //a RootArchiveNode object for each one.
        std::vector<bfs::directory_entry> subdirs;
        std::copy(bfs::directory_iterator(p),
                  bfs::directory_iterator(),
                  std::back_inserter(subdirs));

        auto dir_iter = subdirs.begin();
        while (dir_iter != subdirs.end()) {
            const std::string archive_fulldir = dir_iter->path().string();
            createArchivePlaceholderForExistingDatabase_(archive_fulldir);
            ++dir_iter;
        }
    }

    //! Save the database as-is to the given directory. This will
    //! save whatever is in the database when this method is called,
    //! even if it is in the middle of a simulation. In the case of
    //! active simulations, the file buffers will all be flushed to
    //! disk before the save is made so you won't be missing any data.
    void saveTo(const std::string & dir);

private:
    //! When an archives directory is loaded, we do not have to actually
    //! load archive handles for all the subdirectories up front. Just
    //! store a mapping from the archive name to the full archive directory
    //! such as:
    //!             { "foo.csv", "my/saved/archives/foo.csv" }
    //!             { "bar.txt", "my/saved/archives/bar.txt" }
    //!                                 ...
    void createArchivePlaceholderForExistingDatabase_(
        const std::string & archive_fulldir)
    {
        std::vector<std::string> split;
        boost::split(split, archive_fulldir, boost::is_any_of("/"));
        const std::string archive_name = split.back();
        archive_placeholder_dirs_[archive_name] = archive_fulldir;
    }

    //! Imported archives will be loaded on demand when asked. When a
    //! caller asks the base class for all the root names, they will
    //! get a combined list of *already* loaded roots and any *lazily*
    //! loaded roots. If they then request for the actual ArchiveNode
    //! called "lazyFoo", we will build an archive tree for that archive
    //! then, but not up front.
    std::vector<std::string> getLazyLoadedRootNames_() const override {
        std::vector<std::string> names;
        names.reserve(archive_placeholder_dirs_.size());
        for (const auto & dir : archive_placeholder_dirs_) {
            names.emplace_back(dir.first);
        }
        return names;
    }

    //! This callback will give us a chance to lazily load an archive
    //! when the user has asked for it by name, as opposed to recreating
    //! all archive trees by deserializing all of the metadata files up
    //! front.
    void onNamedRootRequest_(const std::string & root_name) override {
        auto iter = archive_placeholder_dirs_.find(root_name);
        if (iter != archive_placeholder_dirs_.end()) {
            loadArchiveFspartaxistingDatabase_(iter->second);
            archive_placeholder_dirs_.erase(iter);
        }
    }

    //! Recursively set all nodes' parents
    void recursSetParentForChildNodes_(ArchiveNode * parent) const {
        for (auto & child : parent->getChildren()) {
            child->setParent(parent);
            recursSetParentForChildNodes_(child.get());
        }
    }

    //! Deserialize archive metadata files to rebuild a RootArchiveNode,
    //! and store this root in our archives data structure. This is used
    //! to load/import offline archives that do not belong to any simulation.
    void loadArchiveFspartaxistingDatabase_(const std::string & archive_fulldir)
    {
        std::shared_ptr<RootArchiveNode> root(new RootArchiveNode);

        const std::string meta_filename = archive_fulldir + "/archive_tree.bin";
        std::ifstream fin(meta_filename, std::ios::binary);
        if (!fin) {
            throw SpartaException(
                "Unable to open archive file for read: ") << meta_filename;
        }

        boost::archive::binary_iarchive ia(fin);
        ia >> *root;

        //Give everyone in this archive tree easy access to their
        //raw values filename
        const std::string binary_filename = archive_fulldir + "/values.bin";
        root->setMetadataValue("output_filename", binary_filename);

        //Give the root archive node a controller it can use to
        //save the archive to another directory. Offline controllers
        //do not implement synchronization.
        std::shared_ptr<ArchiveController> controller(
            new OfflineArchiveController(archive_fulldir));
        root->setArchiveController(controller);

        //Make the connection from child nodes to their parent node
        recursSetParentForChildNodes_(root.get());

        //The archive directories given to us are in the form:
        //   "db_directory/db_subdirectory"
        //
        //The unique archive name is simply the db_subdirectory, which
        //will be something like 'foo.csv', 'bar.html', etc.
        std::vector<std::string> split;
        boost::split(split, archive_fulldir, boost::is_any_of("/"));
        const std::string archive_name = split.back();
        addHierarchyRoot(archive_name, root);
    }

    std::unordered_map<std::string, std::string> archive_placeholder_dirs_;
};

} // namespace statistics
} // namespace sparta

#endif
