// <PeventCollector> -*- C++ -*-


/**
 * \file PeventCollector
 * \brief Contains A SpartaKeyPair collection implementation of a pevent
 * collector.
 */

#pragma once

#include "sparta/simulation/Clock.hpp"
#include "sparta/pairs/SpartaKeyPairs.hpp"
#include "sparta/pevents/PeventTreeNode.hpp"
#include "sparta/log/MessageSource.hpp"
#include <boost/algorithm/string.hpp>

#define PEVENT_COLLECTOR_NOTE "_pevent"
namespace sparta{
namespace pevents{



    /**
     * \class PeventCollector
     * \brief a class that is capable of recording pevents as key value pairs,
     * where a PairDefinition has been defined with the key values, and function
     * pointers to where to get the data for the pairs.
     * \tparam the Type of PairDefinition that we want to capture a pevent from.
     */
    template <class CollectedEntityType>
    class PeventCollector
        : public PairCollector<CollectedEntityType>, public PeventCollectorTreeNode
    {
    protected:
        using PairCollector<CollectedEntityType>::getPEventLogVector;
        using PairCollector<CollectedEntityType>::turnOn_;
        using PairCollector<CollectedEntityType>::turnOff_;
        using PairCollector<CollectedEntityType>::collect_;

    public:
        using PairCollector<CollectedEntityType>::isCollecting;
        PeventCollector(const std::string& event_name, sparta::TreeNode* parent, const Clock* clk, const bool verbosity=false) :
            PairCollector<CollectedEntityType>(),
            PeventCollectorTreeNode(parent, event_name+PEVENT_COLLECTOR_NOTE+(verbosity ? "_verbose" : "")),
            event_name_(event_name),
            message_src_(this, event_name+PEVENT_COLLECTOR_NOTE, "A collector used to collect pevent data."),
            clk_(clk),
            f_skew_(std::plus<uint64_t>()),
            skew_(0u),
            verbosity_(verbosity)
        {
            // There should never be a null clock
            sparta_assert (clk_ != nullptr);
        }

        virtual ~PeventCollector() {}

        /**
         * \brief Actually collect the object, and write out a pevent.
         * We ovverride the collect method to ensure the the generateCollectionString
         * is always called, even when there is not a change.
         */
        template<typename... Targs>
        void collect(const typename CollectedEntityType::TypeCollected& obj,
                     const Targs&... pos_args)
        {
            if(isCollecting())
            {
                collect_(obj, pos_args...);
                generateCollectionString_();
            }
        }


        //! Return the ev name.
        virtual const std::string& eventName() override
        {
            return event_name_;
        }

        void adjustSkew(const int32_t & skew_amount) {
            skew_ = std::abs(skew_amount);
            if(skew_amount >= 0) {
                f_skew_ = std::plus<uint64_t>();
            } else {
                f_skew_ = std::minus<uint64_t>();
            }
        }

        /**
         * \brief Mark this pevent with a tap to output the pevent's data to a file.
         * \param type the type of pevent being tapped. This is necessary since pevent's
         * are tapped via a traversal of the tree, we would like to only create the tap
         * if this pevent is of the same type.
         * \param file the output file path we'd like the tap to write too
         * \param verbose are we trying to tap a verbose pevent or normal pevent.
         */
        virtual bool addTap(const std::string& type, const std::string& file, const bool verbose) override final
        {
            // notice we only checkout on the lower case.
            std::string lowertype = type;
            boost::algorithm::to_lower(lowertype);
            std::string lower_ev_name = event_name_;
            boost::algorithm::to_lower(lower_ev_name);
            if(( lowertype == lower_ev_name || lowertype == "all") && verbosity_ == verbose)
            {
                // Make sure they cannot add taps after the trigger has fired.
                sparta_assert(running_ == false, "Cannot turnOn a pevent collector for which go() has already been called.");

                // only create a custom tap if we don't already have this one.
                // we could potentially end up with duplicates since the user can turn collection
                // on at treenodes that overlap
                bool is_unique = true;
                for(const auto& existing_tap : taps_)
                {
                    if(existing_tap->getDestination()->compareStrings(file) &&
                       (existing_tap->getCategoryName() == lowertype || lowertype == "all"))
                    {
                        is_unique = false;
                        break;
                    }
                }

                if(is_unique)
                {
                    // Setup a tap to capture the output.
                    taps_.emplace_back(new log::Tap(this, event_name_+PEVENT_COLLECTOR_NOTE, file));
                    // We detatch the tap since we need to wait till the trigger
                    // fires or is manually started to start pevent collection.
                    taps_[taps_.size() - 1]->detach();
                    // We did create a tap.
                    return true;

                }


            }
            // We never added a tap because it was not appropriate for our
            // event type.
            return false;

        }

        // Coming in a bit.
        virtual void turnOff(const std::string& type) override final
        {
            sparta_assert(false, "peventTurnOff not yet supported");
            if(type == event_name_)
            {
                //remove the taps, this is coming in the future.
                if(taps_.size() == 0)
                {
                    turnOff_();
                }
            }
        }

        /**
         * \brief This method should be called on all pevent collectors
         * when the trigger either manually fires to start
         * or the trigger is reached.
         */
        virtual void go() override final
        {

            if(taps_.size() > 0)
            {
                running_ = true;
                // Mark the pair collector running
                turnOn_();
            }
            // replace the taps here so that we are officially collecting the data to file.
            for(auto& tap : taps_)
            {
                tap->reset(this);
            }
        }
    protected:
        /**
         * \brief Override the generateCollectionString_() of the bases PairCollector,
         * In this method, we use the pair_cache owned by PairCache.
         */
        virtual void generateCollectionString_() override
        {
            // Write the pevent to the log.
            std::stringstream ss;
            // Write the event name.
            ss << "ev=" << "\"" << event_name_ << "\" ";

            // Now write the cached key values.
            for(const auto & pair : getPEventLogVector())
            {
                ss << pair.first << "=" << "\"" << pair.second << "\" ";
            }

            // Write the time
            ss << "cyc=" << f_skew_(clk_->currentCycle(), skew_);
            // Finish the line
            ss << ";";
            message_src_ << ss.str();
        }

        const std::string event_name_;
        // We are going to use sparta's logger to output our pevents for ease.
        log::MessageSource message_src_;
        // Log taps that this pevent is being outputted too.
        std::vector<std::unique_ptr<log::Tap > > taps_;
        // We do need a clock b/c each pevent records it's time.
        const Clock* clk_;
        std::function<uint64_t(const uint64_t &, const uint32_t &)> f_skew_;
        uint32_t skew_;
        bool verbosity_;
        bool running_ = false;
    };



} // namespace pevents
} // namespace sparta

