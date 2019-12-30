// <Destination> -*- C++ -*-

#ifndef __DESTINATION_H__
#define __DESTINATION_H__

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <ostream>
#include <fstream>
#include <sstream>
#include <map>
#include <mutex>
#include <functional>
#include <memory>
#include <typeinfo>
#include <utility>
#include <vector>

#include "sparta/log/Message.hpp"
#include "sparta/app/SimulationInfo.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/SpartaException.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/Utils.hpp"
#include "sparta/log/MessageInfo.hpp"

namespace sparta
{
    namespace log
    {
        /*!
         * \brief Generic Logging destination stream interface which writes
         * sparta::log::Message structures to some output [file]stream. Subclasses
         * will implement stream I/O based on construction arguments.
         *
         * Destinations are uniquely identified by their construction argument
         * (i.e. string or ostream reference).
         *
         * Destinations are managed by sparta::log::DestinationManager to ensure
         * that there are usually no duplicates.
         *
         * This Interface constains some compare and compareX methods which
         * are used to compare the Destinations by string or ostream. Supporting
         * destinations identified by a different attribute will require
         * the appropriate comparison support to this class. In general, this
         * should be 1 compare template specialization and a virtual
         * compareTYPE function which returns false by default and is overloaded
         * in a DestinationInstance specialization which actually compares.
         *
         * Noncopyable and non-assignable
         */
        class Destination
        {
        public:

            Destination(const Destination&) = delete;
            Destination& operator=(const Destination&) = delete;

            //! Move constructor
            //! \note Mutex is not copied or moved.
            Destination(const Destination&& rhp) :
                num_msgs_received_(rhp.num_msgs_received_),
                num_msgs_written_(rhp.num_msgs_written_),
                num_msg_duplicates_(rhp.num_msg_duplicates_),
                last_seq_map_(rhp.last_seq_map_)
            { }

            //! Default constructor
            Destination() :
                num_msgs_received_(0),
                num_msgs_written_(0),
                num_msg_duplicates_(0)
            { }

            virtual ~Destination()
            { }

            /*!
             * \brief Comparison of destination for const char[].
             * Uses std::string comparison
             */
            template <std::size_t N>
            bool compare(const char (& arg)[N]) const {
                // Defer to subclass' compareStrings
                return compareStrings(std::string(arg));
            }

            /*!
             * \brief Comparison of destination for const char[].
             * Uses std::string comparison
             */
            bool compare(const char *& arg) const {
                // Defer to subclass' compareStrings
                return compareStrings(std::string(arg));
            }

            /*!
             * \brief Comparison of destination for std::string
             */
            bool compare(const std::string& arg) const {
                // Defer to subclass' compareStrings
                return compareStrings(arg);
            }

            /*!
             * \brief Handle Destination::operator== on ostreams
             */
            bool compare(const std::ostream& arg) const {
                return compareOstreams(arg);
            }

            /*!
             * \brief Operator= for for other unknown types.
             * \note Support for additional types of destinations will require
             * supporting operator== for those types
             * \note This is specialized for other destination types.
             */
            template <typename T>
            bool compare(const T&) const {
                throw SpartaException("Logging destination does not now how to compare a Destination "
                                    "instance with type: ") << demangle(typeid(T).name());
            }

            // Virtual Comparisons for matching destinations

            /*!
             * \brief Returns true if the destination behind this interface was
             * constructed with the string s.
             */
            virtual bool compareStrings(const std::string&) const {
                return false;
            }

            /*!
             * \brief Returns true if the destination behind this interface was
             * constructed with the ostream o.
             */
            virtual bool compareOstreams(const std::ostream&) const {
                return false;
            }

            /*!
             * \brief Create a string representation of this Destination
             * \param pretty Print a more verbose, multi-line representaiton (if
             * available).
             * \return string representation of this node "<" <location> ">"
             *
             * Subclasses should override this with representations appropriate for
             * their type.
             */
            virtual std::string stringize(bool pretty=false) const = 0;

            //! \note This method IS thread-safe
            void write(const sparta::log::Message& msg) {
                std::lock_guard<std::mutex> lock(write_mutex_);

                ++num_msgs_received_;

                // Filter by sequence. Get last sequence ID on this message's thread
                seq_num_type last_seq = getLastSequenceNum_(msg.info.thread_id);
                if(msg.info.seq_num <= last_seq){
                    // Duplicate (same msg from a different tap), do not write
                    ++num_msg_duplicates_;
                    return;
                }

                ++num_msgs_written_;
                write_(msg);

                last_seq_map_[msg.info.thread_id] = msg.info.seq_num; // Update latest sequence
            };

            /*!
             * \brief Get the total number of messages logged through this
             * destination.
             */
            uint64_t getNumMessagesReceived() const { return num_msgs_received_; }

            /*!
             * \brief Gets the total number of messages received by this
             * destination and then written to the actual output stream.
             */
            uint64_t getNumMessagesWritten() const { return num_msgs_written_; }

            /*!
             * \brief Gets the total number of times that a message has arrived
             * at this destination after already having been written to the
             * output stream.
             *
             * This is expected and can occur when multiple taps are on the
             * propagation path of a log message. The duplicates will not be
             * written to the same destination. Note that multiple destinations
             * could refer to the same output file if they were constructed with
             * different but equivalent paths or constructed with different
             * ofstream instances with handles to the same file.
             */
            uint64_t getNumMessageDuplicates() const { return num_msg_duplicates_; }

        private:

            //! Gets tthe last sequence number for this destintion for the given thread ID
            //! \note This method is not thread-safe
            seq_num_type getLastSequenceNum_(thread_id_type tid) const {

                auto itr = last_seq_map_.find(tid);
                if(itr != last_seq_map_.end()){
                    return itr->second;
                }
                return -1;
            };

            //! Write handler. Must be overridden by subclasses to serialize the log Message
            //! \pre Write mutex will be held on this destination
            //! Destinations will implement this method with a newline and flush (if applicable)
            //! Destinations are responsible for replacing any newlines within the content if
            //! desired.
            virtual void write_(const sparta::log::Message& msg) = 0;


            uint64_t num_msgs_received_;  //!< Total messages received
            uint64_t num_msgs_written_;   //!< Total messages written to the destination (received - duplicates)
            uint64_t num_msg_duplicates_; //!< Total number of messages which were already written

            std::map<thread_id_type, seq_num_type> last_seq_map_; //!< Mapping of thread IDs to latest sequence IDs

            std::mutex write_mutex_; //!< Mutex for writing and checking/setting sequence numbers within this destination only
        };

        /*!
         * \brief File writer formatting interface. Subclsases can format
         * for different file types (e.g. raw log, html, sql, etc.)
         */
        class Formatter {
        protected:
            std::ostream& stream_;

        public:

            /*!
             * \brief Formatter type description. Used in a table to describe
             * various formatter sublasses
             */
            struct Info {
                const char* const extension;
                const std::string extname;
                std::function<Formatter* (std::ostream&)> factory;
            };

            /*!
             * \brief Constructor
             * \param stream Output file to which formatter will write. This
             * stream must remain open for the lifetime of this Formatter
             */
            Formatter(std::ostream& stream) :
                stream_(stream)
            { }

            //! Destructor
            virtual ~Formatter() { }

            /*!
             * \brief Write a log message to a report
             */
            virtual void write(const sparta::log::Message& msg) = 0;

            /*!
             * \brief Write a header to a newly-opened report
             */
            virtual void writeHeader(const SimulationInfo& sim_info) = 0;

            /*!
             * \brief Formatter list terminated with a Info having an empty name
             * which is interpreted as the default formatter.
             *
             * DestinationInstance<std::string> will compare input filename
             * against the extensions in this table and find a matching
             * formatter.
             *
             * List must end with a valid (default) Info struct having a NULL
             * extension field
             */
            static const Info* FORMATTERS;
        };

        //! \brief Formatter that writes all message information
        class VerboseFormatter : public Formatter {
        public:
            VerboseFormatter(std::ostream& stream) :
                Formatter(stream)
            { }

            /*!
             * \brief Writes every piece of information associated with the log
             * message
             */
            void write(const sparta::log::Message& msg) override {
                // Replace any \n's with nothing
                stream_ << msg.info << copyWithReplace(msg.content, '\n', "") << std::endl;
                stream_.flush();
            }

            void writeHeader(const SimulationInfo& sim_info) override {
                sim_info.write(stream_, "#", "\n");
                stream_.flush();
            }
        };

        /*!
         * \brief Formatter that writes most message information, but excludes
         * thread/sequence
         */
        class DefaultFormatter : public Formatter {
        public:
            DefaultFormatter(std::ostream& stream) :
                Formatter(stream)
            { }

            /*!
             * \brief Writes a moderate amount of info to output stream
             */
            void write(const sparta::log::Message& msg) override;

            void writeHeader(const SimulationInfo& sim_info) override {
                sim_info.write(stream_, "#", "\n");
                stream_.flush();
            }
        };

        /*!
         * \brief Formatter including only a few bits of the most relevant
         * message information
         */
        class BasicFormatter : public Formatter {
        public:
            BasicFormatter(std::ostream& stream) :
                Formatter(stream)
            { }

            void write(const sparta::log::Message& msg) override {
                stream_ << msg.info.origin.getLocation() << ": "
                        << *msg.info.category << ": "
                        << copyWithReplace(msg.content, '\n', "") << std::endl;
                stream_.flush();
            }

            void writeHeader(const SimulationInfo& sim_info) override {
                sim_info.write(stream_, "#", "\n");
                stream_.flush();
            }
        };

        /*!
         * \brief Formatter including no meta-data from the message
         */
        class RawFormatter : public Formatter {
        public:
            RawFormatter(std::ostream& stream) :
                Formatter(stream)
            { }

            void write(const sparta::log::Message& msg) override {
                stream_ << copyWithReplace(msg.content, '\n', "") << std::endl;
                stream_.flush();
            }

            void writeHeader(const SimulationInfo& sim_info) override {
                sim_info.write(stream_, "#", "\n");
                stream_.flush();
            }
        };

        /*!
         * \brief A destination where log messages go to be written to a file
         * or other ostream. Attempts to prevent duplicate messages from being
         * written to the same output even if logged through mutliple taps.
         *
         * A destination is opened for write and file content cleared
         * where applicable. Because these destinations are persistent, it is
         * opened only once for a process. The output stream will never be
         * cleared by another reference to this destination, even if there are
         * 0 references to it at some point.
         */
        template <typename DestType>
        class DestinationInstance : public Destination
        {
        };

        /*!
         * \brief Logging Destination for an already-open ostream
         *
         * This is used for logging to cout and cerr
         */
        template <>
        class DestinationInstance<std::ostream> : public Destination
        {
            std::ostream& stream_;
            DefaultFormatter fmt_;

        public:

            DestinationInstance(std::ostream& stream) :
                stream_(stream),
                fmt_(stream_)
            {
                if(stream.good() == false){
                    throw SpartaException("stream must be a good() ostream");
                }
            }

            virtual bool compareOstreams(const std::ostream& o) const override {
                return &o == &stream_;
            }

            // From TreeNode
            virtual std::string stringize(bool pretty=false) const override {
                (void) pretty;
                std::stringstream ss;
                ss << "<" << "destination ostream=";
                if(&stream_ == &std::cout){
                    ss << "cout";
                }else if(&stream_ == &std::cerr){
                    ss << "cerr";
                }else{
                    ss << &stream_;
                }
                ss << " rcv="
                   << getNumMessagesReceived() << " wrote=" << getNumMessagesWritten()
                   << " dups=" << getNumMessageDuplicates() << ">";
                return ss.str();
            }

        private:

            virtual void write_(const sparta::log::Message& msg) override {
                fmt_.write(msg);
            };
        };

        /*!
         * \brief Destination that opens and writes to a file with a format
         * based on the file extension of the filename given during
         * construction.
         *
         * File extensions canot be explicitly chosen there can be only 1
         * destination per file and the file content must be agreed upon by all
         * taps writing to it. Restricting this by filename is failsafe.
         */
        template <>
        class DestinationInstance<std::string> : public Destination
        {
            std::ofstream stream_;
            const std::string filename_;
            std::unique_ptr<Formatter> formatter_;
            const Formatter::Info* fmtinfo_;

        public:

            /*!
             * \brief Constructs the destination with the given filename. Data
             * will be written in a format based on the extension.
             *
             * Chooses a formatter from FORMATTERS based on the file extension
             */
            DestinationInstance(const std::string& filename) :
                stream_(filename, std::ofstream::out), // write mode
                filename_(filename)
            {
                if(stream_.good() == false){
                    throw SpartaException("Failed to open logging destination file for append \"")
                        << filename << "\"";
                }

                // Throw on write errors
                stream_.exceptions(std::ostream::eofbit | std::ostream::badbit | std::ostream::failbit | std::ostream::goodbit);

                fmtinfo_ = Formatter::FORMATTERS;
                while(true){
                    if(nullptr == fmtinfo_->extension){
                        break; // End of the list. Use this formatter
                    }
                    std::string ext = fmtinfo_->extension;
                    size_t pos = filename.find(ext);
                    if(pos != std::string::npos && pos == filename.size() - ext.size()){
                        break; // Use this fmtinfo_->factory to construct
                    }
                    ++fmtinfo_;
                }
                if(nullptr == fmtinfo_){
                    throw SpartaException("No formatters defined in FORMATTERS ")
                        << "list for file-based destination instance. "
                        << "Cannot open file \"" << filename << "\"";
                }
                formatter_.reset(fmtinfo_->factory(stream_));
                sparta_assert(formatter_ != nullptr);

                formatter_->writeHeader(SimulationInfo::getInstance());
            }

            virtual bool compareStrings(const std::string& filename) const override {
                return filename == filename_;
            }

            // From TreeNode
            virtual std::string stringize(bool pretty=false) const override {
                (void) pretty;
                std::stringstream ss;
                ss << "<" << "destination file=\"" << filename_ << "\" format=\""
                   << fmtinfo_->extname << "\" ext=\"";
                if(nullptr == fmtinfo_->extension){
                    ss << "(default)";
                }else{
                    ss << fmtinfo_->extension;
                }
                ss << "\" ostream="
                   << &stream_ << " rcv=" << getNumMessagesReceived()
                   << " wrote=" << getNumMessagesWritten()
                   << " dups=" << getNumMessageDuplicates() << ">";
                return ss.str();
            }

        private:

            virtual void write_(const sparta::log::Message& msg) override {
                formatter_->write(msg);
            };
        };


        /*!
         * \brief Manages a set of destinations representing files or streams.
         * All log messages will be written to at least one destination in this
         * object
         */
        class DestinationManager
        {
        public:

            //! Vector of destination pointers.
            typedef std::vector<std::unique_ptr<Destination>> DestinationVector;

            /*!
             * \brief Requests an existing Destination* from the manager and
             * allocates a new one if it does not yet have a destination
             * matching the input argument.
             * \tparam DestT Destination type. An specialization of
             * DestinationInstance must exist for DestT. An overload of
             * Destination::compare must also exist for DestT.
             * \param arg Destination identifier. This parameter is interpreted
             * based on type and value. For example, const char*, std::string,
             * and const char[] are interpreted to be filenames.
             * \return Destination that will write incoming messages to the
             * destination described by arg.
             * \throw Exception if arg cannot be interpreted to describe a
             * Destination or a file cannot be opened.
             * \note Not thread-safe. Destinations modifications must be
             * protected outside of this method if thread-safety is needed
             * \node Once a destination is constructed with an open file handle
             * for "write", it will be appended to by this manager.
             * \warn Filename comparisons may be performed by string comparison.
             * Different paths referring to the same file can cause multiple
             * destinations to be created writing to the same file.
             *
             * Destinations can never be removed once constructed.
             */
            template <class DestT>
            static Destination* getDestination(DestT& arg) {
                for(std::unique_ptr<Destination>& d : dests_){
                    // Overload for Destination::compare(DestT&) must exist. c-string type is typically const char[N]
                    if(d->compare(arg)){
                        return d.get();
                    }
                }

                dests_.emplace_back(createDestination(arg));
                return dests_.back().get();
            }

            //! createDestination overload for handling const char[] strings
            template <std::size_t N>
            static Destination* createDestination(const char (&arg)[N]) {
                return new DestinationInstance<std::string>(arg);
            }

            //! createDestination overload for handling char* strings
            static Destination* createDestination(const char *& arg) {
                return new DestinationInstance<std::string>(arg);
            }

            template <class DestT>
            static Destination* createDestination(const DestT& arg) {
                return new DestinationInstance<DestT>(arg);
            }

            template <class DestT>
            static Destination* createDestination(DestT& arg) {
                return new DestinationInstance<DestT>(arg);
            }

            /*!
             * \brief Returns vector containing all destinations
             */
            static const DestinationVector& getDestinations() {
                return dests_;
            }

            /*!
             * \brief Returns number of destinations contained in the
             * DestinationManager
             */
            static uint32_t getNumDestinations() {
                return dests_.size();
            }

            /*!
             * \brief Dumps the destinations known by the destination manager to
             * an ostream with each destination on a separate line
             */
            static std::ostream& dumpDestinations(std::ostream& o, bool pretty=false) {
                (void) pretty;
                for(auto& d : dests_){
                    o << "  " << d->stringize() << std::endl;
                }

                return o;
            }

            /*!
             * \brief Dumps the supported file extensions for destinations
             * instantiated through the destination manager based on strings
             * (file paths) and describes the resulting file format used based
             * on that extension
             */
            static std::ostream& dumpFileExtensions(std::ostream& o, bool pretty=false) {
                (void) pretty;
                const Formatter::Info* finf = Formatter::FORMATTERS;
                while(true){
                    if(nullptr == finf->extension){
                        o << "  " << "(default) -> " << finf->extname << std::endl;
                        break; // End of the list
                    }
                    o << "  \"" << finf->extension << "\" -> " << finf->extname << std::endl;
                    ++finf;
                }

                return o;
            }

        private:

            //! Static vector of destinations. Automatically deleted upon destruction
            static DestinationVector dests_;
        };

    } // namespace log
} // namespace sparta

//! Destination stream operators
inline std::ostream& operator<< (std::ostream& out, sparta::log::Destination const & d){
    out << d.stringize();
    return out;
}

inline std::ostream& operator<< (std::ostream& out, sparta::log::Destination const * d){
    if(d == 0){
        out << "null";
    }else{
        out << d->stringize();
    }
    return out;
}

// __DESTINATION_H__
#endif
