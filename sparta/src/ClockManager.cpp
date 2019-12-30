
#include "sparta/simulation/ClockManager.hpp"


std::ostream & sparta::operator<<(std::ostream &os, const sparta::ClockManager &m)
{
    m.print(os);
    return os;
}
