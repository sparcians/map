// <SpartaException> -*- C++ -*-

/**
 * \file   SpartaException.cpp
 * \brief  Implements exception class for all of SPARTA.
 */

#include <sstream>
#include <string>
#include <memory>

#include "sparta/utils/SpartaException.hpp"
#include "sparta/app/Backtrace.hpp"

namespace sparta{

SpartaException::SpartaException() :
    bt_(new app::BacktraceData(app::Backtrace::getBacktrace()))
{
}

SpartaException::SpartaException(const SpartaException & orig) :
    bt_(new app::BacktraceData(*(orig.bt_)))
{
    raw_reason_ = orig.raw_reason_;
    reason_ << orig.reason_.str();
}

SpartaException::SpartaException(const std::string & reason) :
    SpartaException() // Delegate so that breakpoints can be set on SpartaException
{
    raw_reason_ = reason;
    reason_ << reason;
}

SpartaException::~SpartaException() noexcept {}

std::string SpartaException::backtrace() const noexcept {
    std::stringstream ss;
    bt_->render(ss,
                true); // show lines
    return ss.str();
}

} // namespace sparta
