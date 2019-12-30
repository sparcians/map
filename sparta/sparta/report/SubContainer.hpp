
#ifndef __REPORT_SUB_CONTAINER_H__
#define __REPORT_SUB_CONTAINER_H__

#include "sparta/utils/SpartaException.hpp"

#include <memory>
#include <string>

#include <boost/any.hpp>

namespace sparta {

/*!
 * \brief Helper class used to hold named data of generic data type.
 */
class SubContainer
{
public:
    SubContainer() = default;
    ~SubContainer() = default;

    template <typename T>
    void setContentByName(const std::string & name, const T & data) {
        boost::any content(data);
        contents_[name][tid_<T>()] = content;
    }

    template <typename T>
    T & getContentByName(const std::string & name) {
        auto contents_iter = contents_.find(name);
        if (contents_iter == contents_.end()) {
            setContentByName(name, T());
            return getContentByName<T>(name);
        }
        auto content_iter = contents_iter->second.find(tid_<T>());
        if (content_iter == contents_iter->second.end()) {
            throw SpartaException("Invalid template type specified for "
                                "subcontainer content called ") << name;
        }
        return boost::any_cast<T&>(content_iter->second);
    }

    template <typename T>
    const T & getContentByName(const std::string & name) const {
        auto contents_iter = contents_.find(name);
        if (contents_iter == contents_.end()) {
            throw SpartaException("There is no content in this subcontainer "
                                "called ") << name;
        }
        auto content_iter = contents_iter->second.find(tid_<T>());
        if (content_iter == contents_iter->second.end()) {
            throw SpartaException("Invalid template type specified for "
                                "subcontainer content called ") << name;
        }
        return boost::any_cast<const T&>(content_iter->second);
    }

    bool hasContentNamed(const std::string & name) const {
        return contents_.find(name) != contents_.end();
    }

private:
    template <typename T>
    inline const char * tid_() const {
        return typeid(T).name();
    }

    std::map<
        std::string,
        std::map<std::string, boost::any>
    > contents_;
};

}

#endif
