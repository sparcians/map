// <ObjectProxy> -*- C++ -*-

#pragma once

#include <functional>
#include <string>

namespace simdb {

class ObjectProxy
{
public:
  

protected:
    ObjectProxy(const std::string & table_name,
                const std::vector<std::string> & column_names,
                ObjectManager & obj_mgr) :
        table_name_(table_name),
        column_names_(column_names),
        obj_mgr_(obj_mgr)
    {}

private:
    std::function<char(const std::string &)> getter_char_{
        std::bind(ObjectProxy::getPropertyChar_, this)};

    std::function<int8_t(const std::string &)> getter_int8_{
        std::bind(ObjectProxy::getPropertyInt8_, this)};

    std::function<uint8_t(const std::string &)> getter_uint8_{
        std::bind(ObjectProxy::getPropertyUInt8_, this)};

    std::function<int16_t(const std::string &)> getter_int16_{
        std::bind(ObjectProxy::getPropertyInt16_, this)};

    std::function<uint16_t(const std::string &)> getter_uint16_{
        std::bind(ObjectProxy::getPropertyUInt16_, this)};

    std::function<int32_t(const std::string &)> getter_int32_{
        std::bind(ObjectProxy::getPropertyInt32_, this)};

    std::function<uint32_t(const std::string &)> getter_uint32_{
        std::bind(ObjectProxy::getPropertyUInt32_, this)};

    std::function<int64_t(const std::string &)> getter_int64_{
        std::bind(ObjectProxy::getPropertyInt64_, this)};

    std::function<uint64_t(const std::string &)> getter_uint64_{
        std::bind(ObjectProxy::getPropertyUInt64_, this)};

    std::function<float(const std::string &)> getter_float_{
        std::bind(ObjectProxy::getPropertyFloat_, this)};

    std::function<double(const std::string &)> getter_double_{
        std::bind(ObjectProxy::getPropertyDouble_, this)};
        
    char getPropertyChar_(const std::string & col_name) const {
    }

    int8_t getPropertyInt8_(const std::string & col_name) const {
    }

    uint8_t getPropertyUInt8_(const std::string & col_name) const {
    }

    int16_t getPropertyInt16_(const std::string & col_name) const {
    }

    uint16_t getPropertyUInt16_(const std::string & col_name) const {
    }

    int32_t getPropertyInt32_(const std::string & col_name) const {
    }

    uint32_t getPropertyUInt32_(const std::string & col_name) const {
    }

    int64_t getPropertyInt64_(const std::string & col_name) const {
    }

    uint64_t getPropertyUInt64_(const std::string & col_name) const {
    }

    float getPropertyFloat_(const std::string & col_name) const {
    }

    double getPropertyDouble_(const std::string & col_name) const {
    }

    std::string getPropertyString_(const std::string & col_name) const {
    }

    std::string table_name_;
    std::vector<std::string> column_names_;
    ObjectManager & obj_mgr_;
    mutable std::shared_ptr<ObjectRef> realized_obj_;
};

