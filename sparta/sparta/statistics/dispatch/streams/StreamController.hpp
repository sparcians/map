// <StreamController> -*- C++ -*-

#pragma once

namespace sparta {
namespace statistics {

/*!
 * \brief Stream controller interface. One of these objects
 * will be shared between the simulation's run controller,
 * and the root-level StreamNode's sitting at the top of
 * each one of the simulation's report hierarchies.
 */
class StreamController
{
public:
    virtual ~StreamController() {}

    //! Begin streaming, start consumer threads, etc.
    //! Has no effect if called more than once.
    void startStreaming() {
        if (!is_streaming_) {
            startStreaming_();
            is_streaming_ = true;
        }
    }

    //! Notify the controller that it is time to get
    //! all buffered SI data and send it out to the
    //! registered client(s). This will call your
    //! startStreaming_() method if the controller
    //! hasn't been started yet.
    void processStreams() {
        if (!is_streaming_) {
            startStreaming_();
        }
        processStreams_();
    }

    //! Terminate streaming, stop consumer threads, etc.
    //! This will ask you to perform one last flush of
    //! any leftover data still in the buffer.
    void stopStreaming() {
        if (is_streaming_) {
            processStreams_();
            stopStreaming_();
            is_streaming_ = false;
        }
    }

private:
    //! Flag to guarantee one-time call to startStreaming_(),
    //! and to ignore calls to stopStreaming() if we never
    //! were told to start streaming in the first place.
    bool is_streaming_ = false;

    //! Controller-specific implementation
    virtual void startStreaming_() = 0;
    virtual void processStreams_() = 0;
    virtual void stopStreaming_() = 0;
};

} // namespace statistics
} // namespace sparta

