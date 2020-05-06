// <Errors> -*- C++ -*-

#pragma once

#include "simdb/Errors.hpp"

#include <string>

namespace simdb {

//! General-purpose SQLite exception used for interrupts
class SqliteInterrupt : public DatabaseInterrupt
{
protected:
    virtual std::string getExceptionDetails_() const override {
        return "SQLite";
    }
};

} // namespace simdb

