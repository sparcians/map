#include "Device.hpp"

// Declared in Device.h
bool validate_begin_global(uint32_t& val, const sparta::TreeNode*)
{
    std::cout << "Validating began (globally) with value of " << val << std::endl;
    return true;
}
