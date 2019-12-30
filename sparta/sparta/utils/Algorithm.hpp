/*
 */
#ifndef __SPARTA_UTILS_ALGORITHM_H__
#define __SPARTA_UTILS_ALGORITHM_H__
#include <iterator>

namespace sparta {
namespace utils {

template <class InputIterator,
          class OutputIterator,
          class T,
          class UnaryOperation>
static OutputIterator slide(InputIterator first,
                            InputIterator last,
                            OutputIterator result,
                            T init,
                            UnaryOperation operation)
{
    if (first == last) {
        return result;
    }

    for (; first + 1 != last; ++first, ++result) {
        *result = operation(*first, *(first + 1));
    }
    *result = operation(*first, init);

    return result;
}

} /* namespace utils */
} /* namespace sparta */
#endif /* __SPARTA_UTILS_ALGORITHM_H__ */
