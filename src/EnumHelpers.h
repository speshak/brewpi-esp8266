/*
 * Copyright 2012-2013 BrewPi/Elco Jacobs.
 * Copyright 2013 Matthew McGowan.
 * Copyright 2020 Scott Peshak
 *
 * This file is part of BrewPi.
 *
 * BrewPi is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * BrewPi is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with BrewPi.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#include <ArduinoJson.h>
#include <type_traits>

/**
 * \file EnumHelpers.h
 * \brief Helper functions for working with Enums
 *
 * ArduinoJson has decent support for tranditional Enums, but not C++11 enum
 * class.  These are helpers to bridge the gap so we can get the type safety of
 * enum class without a lot of pain at the interface points.
 *
 * \see https://github.com/bblanchon/ArduinoJson/issues/1360
 */

namespace EnumHelpers {

/**
 * \brief Read JsonPair value into a enum
 *
 * \param kv - Key Pair for value
 * \param value - Reference to value to read into
 */
template <class T> void readEnumValue(const JsonPair &kv, T &value) {
  static_assert(std::is_enum<T>::value, "Type T must be an enum");
  typename std::underlying_type<T>::type enumValue = 0;

  if (kv.value().is<typename std::underlying_type<T>::type>())
    // If we've already got the underlying type, just read it
    enumValue = kv.value().as<typename std::underlying_type<T>::type>();
  else if (kv.value().is<char *>())
    // If the data is a string, we're going to assume we want the binary value
    // of the first char.
    // This isn't that useful as a general application, but it is how the
    // pilink protocol has behaved.
    enumValue = kv.value().as<char *>()[0];

  value = static_cast<T>(enumValue);
}

/**
 * \brief Read JsonVariant value into a enum
 *
 * \param kv - Key Pair for value
 * \param value - Reference to value to read into
 */
template <class T> T readEnumValue(const JsonVariant &value) {
  static_assert(std::is_enum<T>::value, "Type T must be an enum");
  return static_cast<T>(value.as<typename std::underlying_type<T>::type>());
}

}; // namespace EnumHelpers
