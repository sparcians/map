// <Triggerable.hpp> -*- C++ -*-

/**
 * \file Triggerable.hpp
 * \brief Base class for Triggerable types
 *
 */
#pragma once

namespace sparta
{
    namespace trigger
    {

/**
 * \class Triggerable
 * \brief A simple interface with 2 methods.
 *
 * Classes should inherit from this interface and override the go()
 * and stop() methods to be compatible with sparta::Triggers
 */
class Triggerable
{

public:
    //! Virtually destroy
    virtual ~Triggerable(){}

    //! The method called when the trigger fires a turn on.
    virtual void go() {}

    //! The method called when a trigger fires a turn off.
    virtual void stop() {}

    //! The method to call on periodic repeats of the trigger.
    virtual void repeat() {}

    //! Has been triggered?
    bool isTriggered() const { return triggered_; }

protected:
    bool triggered_ = false;
};

    }// namespace trigger
}// namespace sparta

