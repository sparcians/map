
#include <cstddef>
#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include "sparta/statistics/dispatch/streams/StreamNode.hpp"
#include "sparta/statistics/dispatch/streams/StreamController.hpp"
#include "sparta/statistics/dispatch/ReportStatisticsHierTree.hpp"
#include "sparta/report/Report.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/statistics/StatisticInstance.hpp"
#include "sparta/statistics/dispatch/StatisticSnapshot.hpp"

namespace sparta {
namespace statistics {

/*!
 * \brief Go to the root StreamNode in a report hierarchy,
 * grab the current SI value(s) for all registered listeners
 * (stream clients - these are other StreamNode's down somewhere
 * in this hierarchy), and put those SI values into a thread-safe
 * buffer for later consumption.
 *
 * Returns TRUE if there was any new SI data available, FALSE
 * if not.
 */
bool StreamNode::notifyListenersOfStreamUpdate()
{
    //Root StreamNode's are the only ones with registered
    //listeners. This could possibly assert that parent_
    //is null (we are the root). But calls to getRoot()
    //cache the root node object under the hood, making
    //this recursive call the same performance cost of
    //asking the root node directly, so we can just do
    //that and ease up on the assertions.
    if (parent_ != nullptr) {
        return getRoot()->notifyListenersOfStreamUpdate();
    }

    //This is the root node. Get the available data and
    //push it into our thread-safe buffers.
    else {
        bool has_data = false;
        for (auto listener : listeners_) {
            const std::vector<double> & data = listener->readFromStream_();
            if (!data.empty()) {
                appendDataValuesForListener_(listener, data);
                has_data = true;
            }
        }
        return has_data;
    }
}

/*!
 * \brief TEMPORARY: While asynchronous C++/Python communication
 * is under development, we will push SI values to the registered
 * clients from the main thread during each report update.
 */
void StreamNode::pushStreamUpdateToListeners()
{
    //Same as with the 'notifyListenersOfStreamUpdate()' method
    //above, only root StreamNode's have any listeners. Call
    //recursively on the root node if needed.
    if (parent_ != nullptr) {
        getRoot()->pushStreamUpdateToListeners();
    }

    //This is the root node. Get the available data and
    //synchronously push it to our listeners.
    else {
        const bool has_data = notifyListenersOfStreamUpdate();

        //Async consumers will be greedily requesting data,
        //so the 'has_data' flag in those cases will be false
        //sometimes.
        if (has_data && controller_ != nullptr) {

            //We have some data and a shared controller object.
            //Push the data packets to their respective clients.
            controller_->processStreams();
        }
    }
}

/*!
 * \brief Go to our root node, and ask it for all available
 * data in the thread-safe SI values buffer. The root node
 * will release any buffered data that belongs to clients
 * registered on 'this' StreamNode.
 */
void StreamNode::getBufferedStreamData(std::queue<std::vector<double>> & data_queue)
{
    //There is no reason why calling code should already have
    //put something in the destination queue.
    sparta_assert(data_queue.empty());

    //Release the data from the buffer. The listener's final
    //destination (a Python object, a C++ object, whoever)
    //will be responsible for processing the data, i.e.
    //plotting the SI values, etc.
    //
    //If clients need SI data that occurred in the past that
    //they have already forgotten about (cleared from a plot,
    //for instance) they will have to go through the binary
    //archive APIs to get it again. All SI values are archived
    //behind the scenes to the temp directory.
    getRoot()->releaseDataBufferForListener_(this, data_queue);
}

/*!
 * \brief Create a 1-to-1 mapping between our underlying SI's and our
 * aggregate data vector.
 */
void ReportStreamNode::initialize_()
{
    using SRNode = ReportStreamNode;
    using SINode = StatisticInstStreamNode;
    using HierTree = ReportStatisticsHierTree<SRNode, SINode>;

    HierTree tree_builder(report_);

    //Let the ReportStatisticsHierTree class build our tree for us.
    //It will instantiate ReportStreamNode's at all report/subreport
    //nodes, and StatisticInstStreamNode's at all leaf SI's.
    std::shared_ptr<StreamNode> root(new SRNode(report_->getName(), report_));

    //Build the archive tree from the root ReportStreamNode down
    //through all subreports / SI's
    const std::vector<HierTree::LeafNodeSI> leaves =
        tree_builder.buildFrom(static_cast<SRNode*>(root.get()));

    //Flatten all SI's in this report into one vector
    stat_insts_.reserve(leaves.size());
    for (const auto & leaf : leaves) {
        stat_insts_.emplace_back(leaf.second);
    }

    //Connect each entry in our data vector<double> with its
    //corresponding StatisticInstance
    aggregated_si_values_.reserve(leaves.size());
    for (size_t leaf_idx = 0; leaf_idx < stat_insts_.size(); ++leaf_idx) {
        aggregated_si_values_.emplace_back(0);
        StatisticSnapshot snapshot(aggregated_si_values_.back());
        stat_insts_[leaf_idx]->addSnapshotLogger(snapshot);
    }

    //At this point, we don't technically need the SI vector
    //anymore. The ReportDescriptor that we belong to will
    //indirectly be asking all of the SI's for their current
    //values when reports triggers are hit, and those values
    //get written back to our data vector automatically via
    //SnapshotLogger's.
}

/*!
 * \brief The ReportStreamNode class ties its vector of double
 * values directly to its underlying SI's via SnapshotLogger's.
 * The reporting system will have already asked the SI's for
 * their values in order to write out the report update (in the
 * case of timeseries reports, this happens every time a report
 * update trigger fires). Just like the report archives, these
 * report stream SI vectors are already up to date by the time
 * we are asked for the data, and we just return a reference
 * to it.
 */
const std::vector<double> & ReportStreamNode::readFromStream_()
{
    return aggregated_si_values_;
}

/*!
 * \brief Create a 1-to-1 mapping between our one SI and
 * our data vector
 */
void StatisticInstStreamNode::initialize_()
{
    one_si_value_.resize(1);
    one_si_value_[0] = 0;

    StatisticSnapshot snapshot(one_si_value_[0]);
    stat_inst_->addSnapshotLogger(snapshot);

    //Similar to the ReportStreamNode class, at this point we do
    //not really need our SI member variable for anything, and
    //could set it to null if desired.
}

/*!
 * \brief The StatisticInstStreamNode class ties its vector of
 * double values (just 1 value) directly to its underlying SI
 * via a SnapshotLogger. Our data vector is already up to date
 * with the SI, so we can just return a reference to it.
 */
const std::vector<double> & StatisticInstStreamNode::readFromStream_()
{
    return one_si_value_;
}

} // namespace statistics
} // namespace sparta
