// <StreamNode> -*- C++ -*-

#ifndef __SPARTA_STATISTICS_STREAM_NODE_H__
#define __SPARTA_STATISTICS_STREAM_NODE_H__

#include "sparta/report/Report.hpp"
#include "sparta/utils/SpartaAssert.hpp"

#include <queue>
#include <mutex>

#include <vector>
#include <string>
#include <sstream>
#include <algorithm>

namespace sparta {

class Report;
class StatisticInstance;

namespace statistics {

class StreamController;

/*!
 * \brief When a simulation is configured to stream its statistics
 * values for asynchronous processing, it will build a subset of its
 * device tree to organize the statistics. The resulting hierarchy is
 * made up of report/subreport nodes, and StatisticInstance leaves.
 * StreamNode's represent nodes in this hierarchy, and are designed
 * so that you can stream data out of any node, regardless of where
 * that node lives (could be the root, could be an SI leaf, could be
 * anywhere in the middle).
 */
class StreamNode
{
public:
    virtual ~StreamNode() {}
    StreamNode(const StreamNode &) = delete;
    StreamNode & operator=(const StreamNode &) = delete;

    //! StreamNode name - similar to a TreeNode's name
    const std::string & getName() const {
        return name_;
    }

    //! Full dot-delimited path from the root node to 'this' node,
    //! for example:
    //!
    //!     root                'foo_csv'
    //!       subreport         'top'
    //!         subreport       'core0'
    //!           subreport     'rob'
    //!             SI          'ipc'          <-- 'this' node
    //!
    //! In the above example tree, our full path would be:
    //!     'foo_csv.top.core0.rob.ipc'
    const std::string & getFullPath() {
        if (!full_path_.empty()) {
            return full_path_;
        }
        resolveFullPath_();
        return full_path_;
    }

    //! Direct descendants of this node, if any
    std::vector<std::shared_ptr<StreamNode>> & getChildren() {
        return children_;
    }

    //! Direct descendants of this node, if any
    const std::vector<std::shared_ptr<StreamNode>> & getChildren() const {
        return children_;
    }

    //! StreamNode's typically have their parent set only during
    //! the initial building of the stream hierarchy. Asserts
    //! if you try to call this method twice with a different
    //! parent node each time.
    void setParent(StreamNode * parent) {
        sparta_assert(parent_ == nullptr || parent_ == parent,
                    "Cannot reassign a StreamNode's parent node");
        parent_ = parent;
    }

    //! Return the root node at the top of the stream hierarchy
    //! this StreamNode lives in.
    StreamNode * getRoot() {
        if (cached_root_ != nullptr) {
            return cached_root_;
        }

        //Walk to the top node, and cache it for faster
        //access later on
        StreamNode * parent = this;
        while (parent != nullptr) {
            cached_root_ = parent;
            parent = cached_root_->parent_;
        }
        return cached_root_;
    }

    //! Streaming interface
    void initialize() {
        if (is_initialized_) {
            return;
        }

        //Subclasses now turn their vector/scalar SI(s) into a single
        //vector<double>, with all of their SI's connected to that
        //data vector via SnapshotLogger's. When anyone asks those
        //SI(s) what their current value is, the double value will
        //be written into our vector. It also goes into that vector
        //at the same index every time, so we can safely send the
        //entire vector to any sink for faster processing than if
        //we had to process just one point at a time.
        initialize_();

        //The reporting infrastructure will notify our root node
        //whenever a report write/update was made. We need the root
        //to forward that call to us so we can put the SI data vector
        //into a buffer queue for asynchronous processing.
        getRoot()->addStreamListener_(this);

        is_initialized_ = true;
    }

    //! Tell the nodes in this hierarchy that they should read
    //! their SI's current values, and buffer those values for
    //! processing later on (for example, on a consumer thread).
    //!
    //! This method is thread-safe.
    bool notifyListenersOfStreamUpdate();

    //! Call the 'notify listeners of new data' method, but also
    //! synchronously tell those listeners to push the data to its
    //! registered clients.
    //!
    //! ** This method is temporary while asynchronous C++/Python
    //! ** communication is developed. Python clients will just
    //! ** be fed their data packets from the main thread for now.
    void pushStreamUpdateToListeners();

    //! This method grabs any pending data that has been buffered
    //! during a simulation, and **transfers** it to the 'data_queue'
    //! output argument. The caller is fully responsible for getting
    //! the data to the requesting client.
    //!
    //! This method is thread-safe.
    void getBufferedStreamData(std::queue<std::vector<double>> & data_queue);

    //! Controller object shared between this node and the simulation's
    //! run controller. Used for things like synchronization between the
    //! simulation (main) thread and consumer thread(s).
    void setStreamController(const std::shared_ptr<StreamController> & controller) {
        sparta_assert(controller_ == nullptr,
                    "You cannot reset a StreamNode's stream controller");
        controller_ = controller;
    }

protected:
    explicit StreamNode(const std::string & name) :
        name_(name)
    {
        sparta_assert(!name_.empty(), "You may not create a StreamNode without a name");
    }

private:
    //When a report update occurs, the root stream node will be notified.
    //This root node contains all of the child nodes that have a streaming
    //client attached to them. We call these children "listeners".
    //
    //    Report update -> Root StreamNode
    //                              |
    //                    ---------------------
    //                    |                   |
    //                  ChildA             ChildB
    //                (no client)       (has clients)
    //
    //In this example, listeners_ = {ChildB*}
    void addStreamListener_(StreamNode * listener) {
        std::lock_guard<std::mutex> guard(listeners_mutex_);
        listeners_.emplace_back(listener);
    }

    //The simulation synchronously pushes packets of data into
    //the root StreamNode, and we keep that data organized in
    //a map of child StreamNode* -> queue<packet>
    //
    //This data can be consumed on a separate thread if desired.
    //
    // ** TEMPORARY: While asynchronous C++/Python communication
    // ** is developed, we will process this buffered data from
    // ** the main thread.
    void appendDataValuesForListener_(StreamNode * listener,
                                      const std::vector<double> & data)
    {
        std::lock_guard<std::mutex> guard(listeners_mutex_);
        listeners_data_[listener].push(data);
    }

    //The consumer thread (or the main thread during a forced
    //synchronous flush) is requesting all buffered data for
    //a particular client. We do not do any bookkeeping to
    //account for that released data. As far as the StreamNode
    //is concerned, the data is gone forever.
    void releaseDataBufferForListener_(
        StreamNode * listener,
        std::queue<std::vector<double>> & data_queue)
    {
        std::lock_guard<std::mutex> guard(listeners_mutex_);
        auto iter = listeners_data_.find(listener);
        if (iter != listeners_data_.end()) {
            std::swap(data_queue, iter->second);
        }
    }

    //Walk up to the topmost node in our hierarchy, and create
    //a string for the full node path.
    void resolveFullPath_() {
        std::vector<std::string> path;
        path.push_back(name_);

        const StreamNode * parent = parent_;
        while (parent) {
            path.emplace_back(parent->name_);
            parent = parent->parent_;
        }

        //We now have a vector that looks like:
        //    "ipc.rob.core0.top"
        //
        //So flip it around and dot-delimit it like TreeNode's do
        std::reverse(path.begin(), path.end());
        std::ostringstream oss;
        for (const auto & p : path) {
            oss << p << ".";
        }

        full_path_ = oss.str();
        if (!full_path_.empty() && full_path_.back() == '.') {
            full_path_.pop_back();
        }
    }

    //! One-time initialization. This is called from the public
    //! initialize() method and is guaranteed to only be called
    //! once.
    virtual void initialize_() = 0;

    //! Report/subreport and SI nodes have different ways of
    //! getting their SI data values. In the case of SI (leaf)
    //! nodes, this double vector would only have size 1.
    virtual const std::vector<double> & readFromStream_() = 0;

    //! Metadata and hierarchy
    const std::string name_;
    std::string full_path_;

    StreamNode * parent_ = nullptr;
    StreamNode * cached_root_ = nullptr;

    std::vector<std::shared_ptr<StreamNode>> children_;
    bool is_initialized_ = false;

    //! Listeners and data buffers (thread-safe). Here is what
    //! is meant by a "listener" node...
    //!
    //! Say we are the root node 'foo_csv', and this is the stream tree:
    //!
    //!   foo_csv
    //!     top
    //!       core0
    //!         fpu   - streaming all SI's out to a client
    //!       core1
    //!         rob
    //!           ipc - streaming just this one SI out to a client
    //!
    //! Then the listeners are the 'fpu' StreamNode* and the 'ipc'
    //! StreamNode*, and 'this' root node is responsible for getting
    //! its own listeners' data into the thread-safe buffers.
    //!
    //! Listeners are always attached to root StreamNode's, and never
    //! to leaves (SI nodes). This is done for performance reasons -
    //! we want the main thread (reporting system) to only have to
    //! tell one object to grab the current SI values, or as few
    //! objects as possible at least. There is a 1-to-1 mapping
    //! between a report and its root StreamNode, regardless of
    //! the number of listeners and clients out there getting the
    //! data.
    std::vector<StreamNode*> listeners_;

    std::unordered_map<
        StreamNode*,
        std::queue<std::vector<double>>> listeners_data_;

    std::mutex listeners_mutex_;

    //! Stream controller used to coordinate asynchronous
    //! producer / consumer systems.
    //!
    //! Note that our stream controller is shared with
    //! the simulation's run controller too.
    //!
    //!    Simulation
    //!      RunController
    //!            ^ shares a StreamController     <--|
    //!      ReportRepository                         |   (these are
    //!        Report (root)                          |  shared ctrl's)
    //!            ^ shares a StreamController     <--|
    //!          Subreport
    //!            ...
    //!              SI
    //!              SI
    //!              ...
    //!
    //! Controllers only exist at root StreamNode's, *never* at
    //! intermediate or leaf nodes.
    std::shared_ptr<StreamController> controller_;
};

/*!
 * \brief In the stream node hierarchy, this class is used wherever
 * we encounter a sparta::Report node, which includes all subreports.
 * This class can turn a Report node into a flattened vector of SI's,
 * which in turn feed their data into our contiguous vector of double
 * values, connected via SnapshotLogger's.
 */
class ReportStreamNode : public StreamNode
{
public:
    ReportStreamNode(const std::string & name,
                     const Report * r) :
        StreamNode(name),
        report_(r)
    {
        sparta_assert(report_, "Null Report given to a ReportStreamNode");
    }

    ReportStreamNode(const ReportStreamNode &) = delete;
    ReportStreamNode & operator=(const ReportStreamNode &) = delete;

private:
    //! Creates a 1-to-1 mapping from the SI's and their spot
    //! in our double vector
    void initialize_() override;

    //! No-op, just return a reference to the aggregate vector.
    //! It is always up to date with the SI values taken during
    //! report updates.
    const std::vector<double> & readFromStream_() override;

    //! Report node we belong to, and flattened SI's / data values
    //! for all the statistics in that report (and its subreports,
    //! all the way down)
    const Report * report_ = nullptr;
    std::vector<const StatisticInstance*> stat_insts_;
    std::vector<double> aggregated_si_values_;
};

/*!
 * \brief In the stream node hierarchy, this class is used wherever
 * we encounter a sparta::StatisticInstance leaf node. We make a data
 * vector of size 1, just for our single SI, and connect them together
 * via a SnapshotLogger.
 */
class StatisticInstStreamNode : public StreamNode
{
public:
    StatisticInstStreamNode(const std::string & name,
                            const StatisticInstance * si) :
        StreamNode(name),
        stat_inst_(si)
    {
        sparta_assert(stat_inst_, "Null StatisticInstance given to a "
                                "StatisticInstStreamNode");
    }

    StatisticInstStreamNode(const StatisticInstStreamNode &) = delete;
    StatisticInstStreamNode & operator=(const StatisticInstStreamNode &) = delete;

private:
    //! Creates a 1-to-1 mapping from our only SI and its spot
    //! in our size-1 double vector
    void initialize_() override;

    //! No-op, just return a reference to the single-value vector.
    //! It is always up to date with the SI value taken during
    //! report updates.
    const std::vector<double> & readFromStream_() override;

    //! A single statistic instance and one data value to go with it
    const StatisticInstance * stat_inst_ = nullptr;
    std::vector<double> one_si_value_;
};

} // namespace statistics
} // namespace sparta

#endif
