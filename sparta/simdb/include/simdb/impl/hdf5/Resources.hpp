// <Resources> -*- C++ -*-

#pragma once

#include "hdf5.h"

/*!
 * \file Resources.h
 * \brief The HDF5 library returns opaque resource handles
 * as integer IDs from many of its APIs. Things like file
 * handles and dataspace handles are returned when you open
 * or create a file/dataspace in an HDF5 database, and they
 * must be closed/released using a separate HDF5 API. Failure
 * to do so can leak these resources, and eventually you may
 * see errors coming from HDF5 saying there are no more IDs
 * available. The classes in this file can be thought of as
 * smart pointers for all of the HDF5 resource types SimDB
 * uses.
 */

#define INVALID_HID_T -1

namespace simdb {

//! Default behavior is to take no action when an HDF5
//! resource goes out of scope.
struct H5DefaultDeleter {
    void operator()(const hid_t id) const {
        (void) id;
    }
};

/*!
 * \class H5Resource
 * \brief This class holds onto an HDF5 resource ID, calling
 * the appropriate resource deleter when H5Resource goes out
 * of scope.
 */
template <typename DeleterT = H5DefaultDeleter>
class H5Resource
{
public:
    //! Create an H5Resource without a resource handle.
    H5Resource() : H5Resource(INVALID_HID_T)
    {}

    //! Create an H5Resource associated with the given identifier.
    H5Resource(const hid_t id) : id_(id)
    {}

    //! No copies, no moves.
    H5Resource(const H5Resource &) = delete;
    H5Resource & operator=(const H5Resource &) = delete;

    //! Move constructor. The newly constructed resource
    //! will be solely responsible for closing the handle.
    H5Resource(H5Resource && rhs) : id_(rhs.id_) {
        rhs.id_ = INVALID_HID_T;
    }

    //! Move assignment operator. The newly assigned resource
    //! will be solely responsible for closing the handle. If
    //! the resource being assigned to was already holding an
    //! HDF5 resource identifier, it will be closed/released.
    H5Resource & operator=(H5Resource && rhs) {
        if (id_ != INVALID_HID_T && id_ != rhs.id_) {
            del_(id_);
        }
        id_ = rhs.id_;
        rhs.id_ = INVALID_HID_T;
        return *this;
    }

    //! Close the resource on destroy.
    ~H5Resource() {
        if (id_ != INVALID_HID_T) {
            del_(id_);
        }
    }

    //! Whether or not this is usable
    bool good() const {
        return id_ != INVALID_HID_T;
    }

    //! Assignment from a raw HDF5 identifier. If the resource
    //! being assigned to was already holding an HDF5 resource
    //! identifier, it will be closed/released.
    H5Resource & operator=(const hid_t id) {
        if (id_ != INVALID_HID_T && id_ != id) {
            del_(id_);
        }
        id_ = id;
        return *this;
    }

    //! Cast to the underlying HDF5 identifier value.
    operator hid_t() const {
        return id_;
    }

    bool operator==(const H5Resource & rhs) const {
        return id_ == rhs.id_;
    }

    bool operator!=(const H5Resource & rhs) const {
        return id_ != rhs.id_;
    }

    bool operator<(const H5Resource & rhs) const {
        return id_ < rhs.id_;
    }

    bool operator>(const H5Resource & rhs) const {
        return id_ > rhs.id_;
    }

    bool operator==(const hid_t rhs) const {
        return id_ == rhs;
    }

    bool operator!=(const hid_t rhs) const {
        return id_ != rhs;
    }

    bool operator<(const hid_t rhs) const {
        return id_ < rhs;
    }

    bool operator>(const hid_t rhs) const {
        return id_ > rhs;
    }

private:
    DeleterT del_;
    hid_t id_ = INVALID_HID_T;
};

//! H5Resource deleter for HDF5 file handles
struct H5FDeleter {
    void operator()(const hid_t id) const {
        H5Fclose(id);
    }
};
typedef H5Resource<H5FDeleter> H5FResource;

//! H5Resource deleter for HDF5 group handles
struct H5GDeleter {
    void operator()(const hid_t id) const {
        H5Gclose(id);
    }
};
typedef H5Resource<H5GDeleter> H5GResource;

//! H5Resource deleter for HDF5 dataset handles
struct H5DDeleter {
    void operator()(const hid_t id) const {
        H5Dclose(id);
    }
};
typedef H5Resource<H5DDeleter> H5DResource;

//! H5Resource deleter for HDF5 data type handles
struct H5TDeleter {
    void operator()(const hid_t id) const {
        H5Tclose(id);
    }
};
typedef H5Resource<H5TDeleter> H5TResource;

//! H5Resource deleter for HDF5 dataspace handles
struct H5SDeleter {
    void operator()(const hid_t id) const {
        H5Sclose(id);
    }
};
typedef H5Resource<H5SDeleter> H5SResource;

//! H5Resource deleter for HDF5 property list handles
struct H5PDeleter {
    void operator()(const hid_t id) const {
        H5Pclose(id);
    }
};
typedef H5Resource<H5PDeleter> H5PResource;

}

