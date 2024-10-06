/*!
 * \file helpers.h
 * \brief Helpers for Cython wrappers
 */

#ifndef __HELPERS_H__
#define __HELPERS_H__

#include <string>
#include <stdexcept>
#include <Python.h>

#include "wxPython/sip.h"

#ifdef HAVE_SSIZE_T
    #undef HAVE_SSIZE_T
#endif
#include "wxPython/wxpy_api.h"

#include "wx/dcgraph.h"

#ifndef ARGOS_VERSION
#define ARGOS_VERSION unknown
#endif

class wxConversionException : public std::exception {
    private:
        std::string msg_;

    public:
        wxConversionException(const wxString& class_name) {
            msg_ = "Failed to convert object to type " + class_name.ToStdString();
        }

        const char* what() const noexcept final {
            return msg_.c_str();
        }
};

template<typename T>
inline T* getWrappedWXType(PyObject* py_type, const wxString& class_name) {
    T* c_type;
    if(!wxPyConvertWrappedPtr(py_type, reinterpret_cast<void**>(&c_type), class_name)) {
        throw wxConversionException(class_name);
    }
    return c_type;
}

#define UNWRAP_WX(type, obj) \
    static const wxString CLASS_TYPE(#type); \
    return getWrappedWXType<type>(obj, CLASS_TYPE);

inline wxGCDC* getDC_wrapped(PyObject* dc) {
    UNWRAP_WX(wxGCDC, dc)
}

inline wxFont* getFont_wrapped(PyObject* font) {
    UNWRAP_WX(wxFont, font)
}

inline wxBrush* getBrush_wrapped(PyObject* brush) {
    UNWRAP_WX(wxBrush, brush)
}

inline void getTextExtent(wxGCDC* dc, long* char_width, long* char_height) {
    static const wxString M("m");
    wxCoord width = 0;
    wxCoord height = 0;
    dc->GetTextExtent(M, &width, &height);
    *char_width = width;
    *char_height = height;
}

#endif // #ifndef __HELPERS_H__
