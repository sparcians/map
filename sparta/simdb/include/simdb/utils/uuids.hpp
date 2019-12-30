// <uuids> -*- C++ -*-

#ifndef __SIMDB_UUID_UTILS_H__
#define __SIMDB_UUID_UTILS_H__

#include <chrono>
#include <random>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace simdb {

//! \note There is a known issue with the Boost
//! UUID library that causes valgrind to fail.
//! Some Boost versions intentionally read from
//! uninitialized memory in order to increase
//! randomness of their UUID algorithms. This
//! occurs in the default constructor of:
//!
//! boost::uuids::basic_random_generator<T>::ctor()
//!
//! The Boost documentation notes related to
//! boost::uuid and valgrind suggest to get
//! around the valgrind failure by either
//! suppressing these valgrind errors, or
//! by using a non-default constructor for
//! the basic_random_generator<T> class.
inline std::string generateUUID()
{
    std::mt19937 ran;
    auto seed = std::chrono::high_resolution_clock::now().
        time_since_epoch().count();

    ran.seed(static_cast<uint64_t>(seed));

    boost::uuids::basic_random_generator<std::mt19937> gen(&ran);
    boost::uuids::uuid uuid = gen();

    std::ostringstream oss;
    oss << uuid;
    return oss.str();
}

} // namespace simdb

#endif
