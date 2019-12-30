// <DataTypeUtils> -*- C++ -*-

#ifndef __SIMDB_HDF5_DATA_TYPE_UTILS_H__
#define __SIMDB_HDF5_DATA_TYPE_UTILS_H__

#include "simdb/schema/ColumnMetaStructs.hpp"
#include "simdb/Errors.hpp"

#include <hdf5.h>
#include <numeric>
#include <string>
#include <sstream>
#include <vector>

namespace simdb {

//! \class HDF5ScopedDataType
//!
//! \brief Utility class which closes HDF5 data type
//! resources when it goes out of scope.
class HDF5ScopedDataType
{
public:
    //! \brief Construction
    //!
    //! \param dtype HDF5 data type ID
    //!
    //! \param close_on_destroy Flag telling this object
    //! if it should invoke H5Tclose() on destruction
    HDF5ScopedDataType(const hid_t dtype,
                       const bool close_on_destroy) :
        dtype_(dtype),
        close_on_destroy_(close_on_destroy)
    {}

    //! \brief Call H5Tclose() if we were instructed to
    //! do so on destruction
    ~HDF5ScopedDataType() {
        if (close_on_destroy_ && dtype_ > 0) {
            H5Tclose(dtype_);
        }
    }

    //! \return HDF5 data type ID
    hid_t getDataTypeId() const {
        return dtype_;
    }

private:
    hid_t dtype_ = 0;
    bool close_on_destroy_ = false;
};

//! \brief Get the equivalent H5T_NATIVE_* data type ID
//! for the given schema column
//!
//! \warning This method only supports built-in data types.
//! Types such as string will return -1, which is an invalid
//! identifier for all of the H5T*() APIs.
hid_t getNativeDTypeIdForHDF5(const Column & col)
{
    using dt = ColumnDataType;

    switch (col.getDataType()) {
        case dt::char_t: {
            return H5T_NATIVE_CHAR;
        }
        case dt::int8_t: {
            return H5T_NATIVE_CHAR;
        }
        case dt::uint8_t: {
            return H5T_NATIVE_UCHAR;
        }
        case dt::int16_t: {
            return H5T_NATIVE_SHORT;
        }
        case dt::uint16_t: {
            return H5T_NATIVE_USHORT;
        }
        case dt::int32_t: {
            return H5T_NATIVE_INT;
        }
        case dt::uint32_t: {
            return H5T_NATIVE_UINT;
        }
        case dt::int64_t: {
            return H5T_NATIVE_LONG;
        }
        case dt::uint64_t: {
            return H5T_NATIVE_ULONG;
        }
        case dt::float_t: {
            return H5T_NATIVE_FLOAT;
        }
        case dt::double_t: {
            return H5T_NATIVE_DOUBLE;
        }
        default: {
            return -1;
        }
    }
}

//! \brief Translate a SimDB column object into an equivalent
//! HDF5ScopedDataType object. This takes into account the base
//! data type of the column as well as its dimensionality.
//!
//! \param col Schema object to turn into the HDF5 equivalent
//!
//! \return Object holding the data type ID that matches the
//! data type of the column
HDF5ScopedDataType getScopedDTypeForHDF5(const Column & col)
{
    const std::vector<size_t> & dims = col.getDimensions();
    const hid_t hdtype = getNativeDTypeIdForHDF5(col);
    if (hdtype < 0) {
        throw DBException("Unsupported data type encountered");
    }

    std::vector<hsize_t> hdims;
    if (!dims.empty()) {
        hdims.reserve(dims.size());
        for (auto dimsval : dims) {
            hdims.emplace_back(dimsval);
        }
    } else {
        hdims.resize(1);
        hdims[0] = 1;
    }

    //The base data type is a POD type, and we do not have
    //to call H5Tclose() for these predefined data types
    //HDF5 provides out of the box. However, if the column
    //dimensions were specified as non-scalar, then we do
    //have to create/register our own custom data type, and
    //therefore we do have to tell the HDF5ScopedDataType
    //to take ownership of the data type handle, and close
    //it from its destructor.
    if (std::accumulate(hdims.begin(), hdims.end(), 1,
                        std::multiplies<hsize_t>()) == 1)
    {
        const bool take_ownership = false;
        return HDF5ScopedDataType(hdtype, take_ownership);
    }

    const auto hdims_sz = static_cast<unsigned int>(hdims.size());
    const auto hdims_ptr = hdims.data();
    const bool take_ownership = true;

    return HDF5ScopedDataType(
        H5Tarray_create(hdtype, hdims_sz, hdims_ptr),
        take_ownership);
}

//! \brief Utility method used in errors and warnings
//!
//! \param col Schema column to stringize
//!
//! \return Stringized column data type. Takes into
//! account the column base type as well as dimensions.
std::string getColumnDTypeStr(const Column & col)
{
    std::ostringstream oss;
    oss << col.getName();

    const auto & dims = col.getDimensions();
    if (dims.empty()) {
        return oss.str();
    }

    oss << "(";
    if (dims.size() == 1) {
        oss << dims[0];
    } else {
        for (size_t idx = 0; idx < dims.size() - 1; ++idx) {
            oss << dims[idx] << ",";
        }
        oss << dims.back();
    }
    oss << ")";
    return oss.str();
}

//! \brief Get the SimDB column data type enumeration that
//! is equivalent to the provided HDF5 identifier. HDF5 SimDB
//! currently only supports native data types, which includes
//! integers and floating-point types.
//!
//! \warning This method throws if the provided identifier is
//! none of the supported types, such as string or blob/opaque.
//!
//! \param tid HDF5 data type ID
//!
//! \return simdb::ColumnDataType equivalent
ColumnDataType getPODColumnDTypeFromHDF5(const hid_t tid)
{
    using dt = ColumnDataType;

    if (H5Tequal(tid, H5T_NATIVE_CHAR)) {
        return dt::char_t;
    } else if (H5Tequal(tid, H5T_NATIVE_SCHAR)) {
        return dt::int8_t;
    } else if (H5Tequal(tid, H5T_NATIVE_UCHAR)) {
        return dt::uint8_t;
    } else if (H5Tequal(tid, H5T_NATIVE_SHORT)) {
        return dt::int16_t;
    } else if (H5Tequal(tid, H5T_NATIVE_USHORT)) {
        return dt::uint16_t;
    } else if (H5Tequal(tid, H5T_NATIVE_INT)) {
        return dt::int32_t;
    } else if (H5Tequal(tid, H5T_NATIVE_UINT)) {
        return dt::uint32_t;
    } else if (H5Tequal(tid, H5T_NATIVE_LONG)) {
        return dt::int64_t;
    } else if (H5Tequal(tid, H5T_NATIVE_ULONG)) {
        return dt::uint64_t;
    } else if (H5Tequal(tid, H5T_NATIVE_FLOAT)) {
        return dt::float_t;
    } else if (H5Tequal(tid, H5T_NATIVE_DOUBLE)) {
        return dt::double_t;
    }

    //HDF5 SimDB currently only supports POD scalars
    //and POD arrays/matrices.
    throw DBException("Unrecognized data type encountered");
}

}

#endif
