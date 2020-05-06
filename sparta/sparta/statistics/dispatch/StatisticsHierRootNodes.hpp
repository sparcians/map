// <StatisticsHierRootNodes> -*- C++ -*-

#pragma once

#include <memory>
#include <string>
#include <unordered_map>

namespace sparta {
namespace statistics {

/*!
 * \brief Utility class that holds onto statistics node
 * hierarchies, accessible by a name that you choose.
 */

template <class StorageT>
class StatisticsHierRootNodes
{
public:
    virtual ~StatisticsHierRootNodes() {}

    //! Append a statistics hierarchy root node to this set
    void addHierarchyRoot(const std::string & storage_name,
                          std::shared_ptr<StorageT> & root)
    {
        sparta_assert(root != nullptr, "Unexpected null statistics hierarchy root encountered");
        if (roots_.count(storage_name) > 0) {
            throw SpartaException("Statistic hiearchy with root named '") << storage_name
                << "' already exists in this statistics set";
        }
        roots_[storage_name] = root;
    }

    //! Access the root node names in this set
    std::vector<std::string> getRootNames() const {
        std::set<std::string> names;
        for (const auto & root : roots_) {
            names.insert(root.first);
        }
        const std::vector<std::string> more_names = getLazyLoadedRootNames_();
        names.insert(more_names.begin(), more_names.end());
        return std::vector<std::string>(names.begin(), names.end());
    }

    //! Maintain a mapping from report filenames like 'out.csv' to the
    //! equivalent root name like 'out_csv'. This is to support tab
    //! completion for Python shell users (Python won't allow dots in
    //! node names).
    void mapRootNameToReportFilename(const std::string & root_name,
                                     const std::string & report_filename) const
    {
        root_names_to_report_filenames_[root_name] = report_filename;
    }

    //! Ask for a hierarchy root node by name. The name should be
    //! one that you originally gave to addHierarchyRoot(), or this
    //! method will return null.
    StorageT * getRootByName(const std::string & root_name)
    {
        onNamedRootRequest_(root_name);
        auto iter = roots_.find(root_name);
        if (iter == roots_.end()) {
            //Check if this root exists by a different name, for
            //example the name passed in was 'out_csv' but its name
            //in our hash is 'out.csv'
            utils::ValidValue<std::string> report_filename =
                getReportFilenameForRoot_(root_name);
            if (report_filename.isValid()) {
                return getRootByName(report_filename);
            } else {
                return nullptr;
            }
        }
        return iter->second.get();
    }

private:
    utils::ValidValue<std::string> getReportFilenameForRoot_(
        const std::string & root_name) const
    {
        utils::ValidValue<std::string> report_filename;
        auto iter = root_names_to_report_filenames_.find(root_name);
        if (iter != root_names_to_report_filenames_.end()) {
            report_filename = iter->second;
        }
        return report_filename;
    }

    virtual void onNamedRootRequest_(const std::string & root_name) {
        (void) root_name;
    }

    virtual std::vector<std::string> getLazyLoadedRootNames_() const {
        return {};
    }

    mutable std::unordered_map<
        std::string,
        std::string> root_names_to_report_filenames_;

    std::unordered_map<
        std::string,
        std::shared_ptr<StorageT>> roots_;
};

} // namespace sparta
} // namespace statistics

