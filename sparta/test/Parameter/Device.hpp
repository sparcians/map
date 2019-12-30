
#ifndef __SAMPLE_DEVICE_H__
#define __SAMPLE_DEVICE_H__

#include <iostream>
#include <string>
#include <cassert>

#include "sparta/sparta.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ResourceFactory.hpp"

enum MyEnum {
    MY_ENUM_DEFAULT = 0,
    MY_ENUM_OTHER   = 1
};

inline std::istream& operator>>(std::istream& is, MyEnum& e){
    int x;
    is >> x;
    if(is.fail()){
        e = MY_ENUM_DEFAULT;
    }else if(x == MY_ENUM_DEFAULT){
        e = MY_ENUM_DEFAULT;
    }else if(x == MY_ENUM_OTHER){
        e = MY_ENUM_OTHER;
    }else{
        sparta_assert(false, "Bad value for enum");
    }
    return is;
}

// Parameter set which is not part of a class
class BaseParameterSetA : public sparta::ParameterSet
{
public:
    BaseParameterSetA(sparta::TreeNode* node) :
        sparta::ParameterSet(node)
    {
    }

    PARAMETER         (uint64_t,                 zpsA_var0,    0,                          "test from parameter set A")

    // Note lots of volatile params here for testing purposes. Params are read/written/read
    // just to test functionality without having to create new params. This does not happen
    // in real models where params are usually write once, read once.
    VOLATILE_PARAMETER(uint64_t,                 ypsA_var1,    1,                          "test from parameter set A")
    VOLATILE_PARAMETER(uint64_t,                 xpsA_var2,    2,                          "test from parameter set A")
};

// Parameter set which is not part of a class
class BaseParameterSetB : public virtual sparta::ParameterSet
{
public:
    BaseParameterSetB(sparta::TreeNode* node) :
        sparta::ParameterSet(node)
    {}

    PARAMETER(uint64_t,                   psB_var0,    0,                          "test from parameter set B")
    PARAMETER(uint64_t,                   psB_var1,    1,                          "test from parameter set B")
    PARAMETER(uint64_t,                   psB_var2,    2,                          "test from parameter set B")
};


extern bool validate_begin_global(uint32_t& val, const sparta::TreeNode*); // Defined in Device.cpp


class DeviceWithParams
{
public:
    virtual ~DeviceWithParams() {};
};

class SampleDevice : public DeviceWithParams
{
public:

    // Parameter set
    // Shows that there is no harm in re-inheriting from sparta::ParameterSet
    class SampleDeviceParameterSet : public BaseParameterSetA
    {
    public:

        bool was_validated = false; // To ensure validate_begin was called
        bool was_modified = false; // To ensure callback was made

        bool validate_begin(uint32_t& val, const sparta::TreeNode*){
            // One could look at other members in this class instance here
            std::cout << "Validating began (in member function) with value of " << val << std::endl;
            was_validated = true;
            return true;
        }

        static bool validate_begin_static(uint32_t& val, const sparta::TreeNode*){
            // This is a nice place to put a generic validator
            std::cout << "Validating began (statically) with value of " << val << std::endl;
            return true;
        }

        void modifyVarsCausePSAVar0WasWritten() {
            ypsA_var1 = 5;
            xpsA_var2 = 6;
        }

        void modifyVarsCausePSAVar1WasWritten() {
            bool ok = false;
            try {
                ypsA_var1 = 7;   // this is disallowed because var1 can not modify itself
            }
            catch(...) {
                ok = true;
            }
            sparta_assert(ok, "I shouldn't get here")
            xpsA_var2 = 8;
        }

        SampleDeviceParameterSet(sparta::TreeNode* node) :
            BaseParameterSetA(node),
            was_validated(false)
        {

            zpsA_var0.associateParametersForModification({&ypsA_var1, &xpsA_var2},
                                                        CREATE_SPARTA_HANDLER(SampleDeviceParameterSet,
                                                                            modifyVarsCausePSAVar0WasWritten));

            ypsA_var1.associateParametersForModification({&xpsA_var2},
                                                         CREATE_SPARTA_HANDLER(SampleDeviceParameterSet,
                                                                             modifyVarsCausePSAVar1WasWritten));

            bool ok = false;
            try {
                xpsA_var2.associateParametersForModification({&zpsA_var0},
                                                            CREATE_SPARTA_HANDLER(SampleDeviceParameterSet,
                                                                                modifyVarsCausePSAVar0WasWritten));
            }
            catch(sparta::SpartaException & e) {
                ok = true;
                std::cout << "error successfully caught: " << e.what() << std::endl;
            }
            sparta_assert(ok, "TEST ERROR: should not have gotten here, "
                                  "associateParameterForModification failed to catch error");

            // Initalize
            test_boolvec << true << false << true;
            test_int32vec << -1 << 0 << 1;
            test_uint32vec << 0 << 1 << 2;
            test_int64vec << -1 << 0 << 1;
            test_uint64vec << 0 << 1 << 2;
            test_doublevec << -1.1 << 0 << 1.1;
            test_stringvec << "A" << "Bee" << "C";

            // Add some dependency callbacks for validation
            begin.addDependentValidationCallback(&SampleDeviceParameterSet::validate_begin_static,
                                                "static begin constraint");

            begin.addDependentValidationCallback<SampleDeviceParameterSet, &SampleDeviceParameterSet::validate_begin>
                                                (this,
                                                 "begin constraint");

            begin.addDependentValidationCallback(&validate_begin_global,
                                                "global begin constraint");

            test_boolvec.addDependentValidationCallback([](std::vector<bool>& val, const sparta::TreeNode*){return val.size() == 6;},
                                                        "Vector length constraint");
        }

        // 'begin' hides name in ParamSet
        PARAMETER(uint32_t,                 begin,          100,                        "Docstring for begin")
        PARAMETER(uint32_t,                 length,         1,                          "Docstring for length")

        // Test every type of param
        VOLATILE_PARAMETER(bool,            test_bool,      true,                       "Docstring for test_bool")
        PARAMETER(int32_t,                  test_int32,     -1,                         "Docstring for test_int32")
        PARAMETER(uint32_t,                 test_uint32,    2,                          "Docstring for test_uint32")
        PARAMETER(int64_t,                  test_int64,     -3,                         "Docstring for test_int64")
        PARAMETER(uint64_t,                 test_uint64,    4,                          "Docstring for test_uint64")
        PARAMETER(double_t,                 test_double,    5.6,                        "Docstring for double")
        PARAMETER(std::string,              test_string,    "this is a test string",    "Docstring for test_string")
        //PARAMETER(const char*,              test_charptr,   "this is a test char*",     "Docstring for test_charptr")

        VOLATILE_PARAMETER(std::vector<bool>, test_boolvec, std::vector<bool>({false, false, true}), "Docstring for test_boolvec")
        PARAMETER(std::vector<int32_t>,     test_int32vec,  std::vector<int32_t>(),     "Docstring for test_int32vec")
        PARAMETER(std::vector<uint32_t>,    test_uint32vec, std::vector<uint32_t>(),    "Docstring for test_uint32vec")
        PARAMETER(std::vector<int64_t>,     test_int64vec,  std::vector<int64_t>(),     "Docstring for test_int64vec")
        PARAMETER(std::vector<uint64_t>,    test_uint64vec, std::vector<uint64_t>(),    "Docstring for test_uint64vec")
        PARAMETER(std::vector<double>,      test_doublevec, std::vector<double>(),      "Docstring for test_doublevec")
        PARAMETER(std::vector<std::string>, test_stringvec, std::vector<std::string>({"", ""}), "Docstring for test_stringvec")
        PARAMETER(std::vector<std::vector<std::string>>, test_stringvecvec,
                  std::vector<std::vector<std::string>>({{"1"}, {"2","3"}, {"4","5","6"}, {}}),
                  "Docstring for test_stringvecvec")


        // Test MANY parameters to ensure that the publishing mechanism works correctly
        PARAMETER(uint32_t,                 dummy00,        0x00,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy01,        0x01,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy02,        0x02,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy03,        0x03,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy04,        0x04,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy05,        0x05,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy06,        0x06,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy07,        0x07,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy08,        0x08,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy09,        0x09,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy0a,        0x0a,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy0b,        0x0b,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy0c,        0x0c,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy0d,        0x0d,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy0e,        0x0e,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy0f,        0x0f,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy10,        0x10,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy11,        0x11,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy12,        0x12,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy13,        0x13,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy14,        0x14,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy15,        0x15,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy16,        0x16,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy17,        0x17,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy18,        0x18,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy19,        0x19,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy1a,        0x1a,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy1b,        0x1b,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy1c,        0x1c,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy1d,        0x1d,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy1e,        0x1e,                       "dummy parameter")
        PARAMETER(uint32_t,                 dummy1f,        0x1f,                       "dummy parameter")

        // Can be written and read in dangerous orders (before finalization)
        VOLATILE_PARAMETER(MyEnum,          myenum,         MY_ENUM_DEFAULT,            "dummy parameter")

        // Test the new Locked Parameter which can be read and written as many times until
        // a parameter lockdown phase is called by the Simulation class.
        // Parameters can be specified as locked during construction in ParameterSets but behaves
        // just like a normal parameter would do until the parameter lockdown phase. After the
        // parameter lockdown phase, overwriting such a parameter is disallowed and is guaranteed
        // to throw under such a circumstance.
        LOCKED_PARAMETER(uint64_t, dummy_locked_var, 0x03, "locked param from specific parameter set A")
        VOLATILE_LOCKED_PARAMETER(uint32_t, dummy_locked_var_2, 0x00, "another locked param from specific parameter set A")

        // Test the new Hidden Parameter which can be read and written as many times until
        // a parameter lockdown phase is called by the Simulation class.
        // Parameters can be specified as hidden during construction in ParameterSets but behaves
        // just like a normal parameter would do until the parameter lockdown phase. After the
        // parameter lockdown phase, overwriting such a parameter is disallowed and is guaranteed
        // to throw under such a circumstance. Additionally, a hidden parameter would not participate
        // in dumpList or printAll methods of its ParameterSet.
        HIDDEN_PARAMETER(uint64_t, dummy_hidden_var, 0xA3, "hidden param from specific parameter set A")
        VOLATILE_HIDDEN_PARAMETER(uint32_t, dummy_hidden_var_2, 0xA4, "another hidden param from specific parameter set A")
    };

    // Constructor
    SampleDevice(const std::string& name, SampleDeviceParameterSet const * params)
    {
        (void) params;
        std::cout << "Constructing SampleDevice \"" << name << "\"" << std::endl;
    }

    // Subclass, 3x derived from boost noncopyable ParameterSet.
    // Provides copy construction and operator=, which noncopyable should prevent
    class SampleDeviceParameterSetWithCopyMethods : public SampleDeviceParameterSet
    {
    public:
        SampleDeviceParameterSetWithCopyMethods() :
            SampleDeviceParameterSet(0)
        {}

        void operator=(const SampleDeviceParameterSet& rhp) {
            (void) rhp;
        }
    };

}; // SampleDevice


inline DeviceWithParams* createDevice(const std::string& name,
                                      sparta::ParameterSet const * gps
                                      /*, ports*/)
{
    SampleDevice::SampleDeviceParameterSet const * sps; // specific parameter set
    sps = dynamic_cast<SampleDevice::SampleDeviceParameterSet const *>(gps);
    sparta_assert(sps); // Specific parameter set must not be NULL
    return new SampleDevice(name, sps);
}

class IllegalParameterSet : public sparta::ParameterSet
{
public:
    IllegalParameterSet() :
        sparta::ParameterSet(0)
    {}
};

// __SAMPLE_DEVICE_H__
#endif
