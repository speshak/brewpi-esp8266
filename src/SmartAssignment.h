#pragma once

#include <type_traits>

/**
 * \brief Set target to value if value is set
 * \param value - Source value
 * \param target - Variable to set, if value is set
 */
template <typename T> void assignIfSet(const T value, T *target) {
  if (value >= (T)0)
    *target = (T)(value);
}
