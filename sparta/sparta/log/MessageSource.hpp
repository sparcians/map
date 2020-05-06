// <MessageSource> -*- C++ -*-

#pragma once

#include <cstdint>
#include <iostream>
#include <string>
#include <sstream>
#include <utility>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/log/NotificationSource.hpp"
#include "sparta/log/Message.hpp"
#include "sparta/log/Tap.hpp"
#include "sparta/log/Events.hpp"
#include "sparta/log/categories/CategoryManager.hpp"
#include "sparta/utils/StringManager.hpp"
#include "sparta/utils/Utils.hpp"
#include "sparta/log/MessageInfo.hpp"

namespace sparta
{
    /*!
     * \brief Diagnostic logging framework. This node generates logging messages
     * of a specific category.
     */
    namespace log
    {
        /*!
         * \brief Message source object associated with a sparta TreeNode through which messages can be sent.
         *
         * \todo Must understand current thread + sequence within that thread
         */
        class MessageSource : public NotificationSource<sparta::log::Message>
        {
        public:

            //! \brief Group name of logging message sources
            static const constexpr char* GROUP_NAME_MSG_SOURCE = "_sparta_log_msg_source_";

        public:

            MessageSource(const MessageSource&) = delete;
            MessageSource& operator=(const MessageSource&) = delete;
            MessageSource& operator=(const MessageSource&&) = delete;

            MessageSource(TreeNode* parent,
                          const std::string& category,
                          const std::string& desc) :
                MessageSource(parent, StringManager::getStringManager().internString(category), desc)
            { }

            // MessageSource with category_id interned in StringManager
            MessageSource(TreeNode* parent,
                          const std::string* category_id,
                          const std::string& desc) :
                NotificationSource(parent,
                                   GROUP_NAME_MSG_SOURCE,
                                   notNull(parent)->getGroupIndexMax(GROUP_NAME_MSG_SOURCE) + 1,
                                   desc,
                                   category_id)
            { }

            /*!
             * \note No action on destruction. Message source lifetime must
             * match the parent's.
             */
            virtual ~MessageSource() {
            }

            uint64_t getNumEmitted() const {
                return getNumPosts();
            }

            const std::string* getCategoryID() const {
                return getNotificationID();
            }

            const std::string& getCategoryName() const {
                return getNotificationName();
            }

            operator bool() const noexcept {
                return observed();
            }

            /*!
             * \brief Gets the global warning logger. These messages can be
             * picked up by placing a Tap on the
             * sparta::TreeNode::getVirtualGlobalNode (typically by placing an
             * empty string or "_global" as the location in a command-line
             * logging tap) specification)
             * \note Accessing this logger is a funcitonal call and extra
             * dereference, so avoid using global logging in the critical path
             * \warning Do not use from within static initialization or
             * destruction
             */
            static MessageSource& getGlobalWarn();

            /*!
             * \brief Gets the global warning logger. These are global-level
             * debugging messages from the SPARTA framework
             * \see getGlobalWarn
             */
            static MessageSource& getGlobalDebug();

            /*!
             * \brief Gets the global parameters/configuration logger. These are
             * global-level configuration messages from the SPARTA framework
             * \see getGlobalWarn
             */
            static MessageSource& getGlobalParameterTraceSource();


            //! \name Message Generation
            //! @{
            ////////////////////////////////////////////////////////////////////////

            /*!
             * \brief Temporary object for constructing a log message with a
             * ostream-like interface. Emits a message to the message source
             * upon destruction.
             * \note std::endl and some other function operations are not
             * supported in insertion operators.
             *
             * This allows MessageSource::operator<< to string multiple
             * insertions together just like cout waiting until the LogObject
             * is destroyed to actually send the message to its destination
             *
             */
            class LogObject
            {
                /*!
                 * \brief Message source to through which the message will be
                 * emitted
                 */
                const sparta::log::MessageSource* src_;

                /*!
                 * \brief Temporary string buffer
                 */
                std::ostringstream s_;

            public:

                //! \brief Not default-constructable
                LogObject() = delete;

                //! Move constructor
                LogObject(LogObject&& rhp) :
                    src_(rhp.src_),
                    s_(std::move(rhp.s_.str())) // May unfortunately involve a copy
                { }

                //! \brief Not Copy-constructable
                LogObject(const LogObject& rhp) = delete;

                /*!
                 * \brief Construct with message source
                 */
                LogObject(const MessageSource& src) :
                    src_(&src)
                { }

                /*!
                 * \brief Construct with an initial value
                 */
                template <class T>
                LogObject(const MessageSource& src, const T& init) :
                    src_(&src)
                {
                    s_ << init;
                }

                /*!
                 * \brief Construct with a function operating on ostreams (e.g.
                 * std::setw)
                 */
                LogObject(const MessageSource& src, std::ostream& (*f)(std::ostream&)) :
                    src_(&src)
                {
                    f(s_);
                }

                /*!
                 * \brief Destructor
                 *
                 * Sends the message constructed within this object through
                 * MessageSource::emit_
                 */
                ~LogObject() {
                    if(src_){
                        src_->emit_(s_.str());
                    }
                }

                /*!
                 * \brief Cancel the LogObject by
                 */
                void cancel() {
                    src_ = nullptr;
                }

                /*!
                 * \brief Insertion operator on this LogObject.
                 * \return This LogObject
                 *
                 * Appends object to internal ostringstream
                 */
                template <class T>
                LogObject& operator<<(const T& t) {
                    s_ << t;
                    return *this;
                }

                /*!
                 * \brief Handler for stream modifiers (e.g. endl)
                 */
                LogObject& operator<<(std::ostream& (*f)(std::ostream&)) {
                    f(s_);
                    return *this;
                }
            };

            template <class T>
            LogObject operator<<(const T& t) const {
                return LogObject(*this, t);
            }

            LogObject operator<<(std::ostream& (*f)(std::ostream&)) const {
                return LogObject(*this, f);
            }

            LogObject emit(const std::string& msg) const {
                return LogObject(*this, msg);
            }

            LogObject emit(std::string&& msg) const {
                return LogObject(*this, msg);
            }

            ////////////////////////////////////////////////////////////////////////
            //! @}


            // Override from TreeNode
            virtual std::string stringize(bool pretty=false) const override {
                (void) pretty;
                std::stringstream ss;
                ss << '<' << getParent()->getLocation() << ":log_msg_src cat:\""
                   << getCategoryName() << "\" observed:" << std::boolalpha << observed()
                   << " msgs:" << getNumEmitted() << '>';
                return ss.str();
            }

        private:

            /*!
             * \brief Sends a message to the destination immediately. Also adds
             * data from this message source and simulator thread.
             * \param msg Message string content.
             * \throw SpartaException if this TreeNode has no parent somehow
             * \post Increments seq_num_
             */
            void emit_(const std::string& content) const;

            //! \todo This needs to be thread-specific and come from threads
            static seq_num_type seq_num_;
        };

    } // namespace log
} // namespace sparta

