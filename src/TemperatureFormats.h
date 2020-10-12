/*
 * Copyright 2012-2013 BrewPi/Elco Jacobs.
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

#include "Brewpi.h"
#include <stdint.h>

/**
 * \file TemperatureFormats.h
 * \brief Temperature value handling
 *
 * \defgroup temps Temperature Values
 * \brief Internal representation of temperature values
 *
 * - The internal fixed point format has 9 bits (512 steps) per degree. The range is -16 to 112C, an offset of -48C
 * - The communication over serial is in C or F and it is always converted to the internal fixed point format in C.
 * - The interface to the Raspberry Pi uses decimal notation, like 21.3.
 * - Depending on the EEPROM setting cc.tempFormat, this will be interpreted as Celsius or Fahrenheit
 *
 * ## Conversion
 * ### From C to fixed point temp
 * \f{eqnarray*}{
 * T &=& (C-48) \cdot 512 \\
 *   &=& (C-48)<<9 \\
 *   &=& (C<<9) - (48<<9) \\
 *   &=& (C<<9) - 24576 \\
 * \f}
 *
 * ### From F to fixed point temp
 * \f{eqnarray*}{
 * T &=& ((F-32)\frac{9}{5}-48) \cdot 512 \\
 *   &=& (F\frac{9}{5})<<9 - 33678
 * \f}
 *
 * ### From fixed point temp to C
 * \f{eqnarray*}{
 * C &=& \frac{T}{512} + 48 \\
 *   &=& \frac{T+24576}{512} \\
 *   &=& (T+24576)>>9
 * \f}
 *
 * ### From fixed point temp to F
 * \f{eqnarray*}{
 * F &=& (\frac{T}{512} + 48) \cdot \frac{9}{5} + 32 \\
 *   &=& (T+24576) \cdot \frac{9}{5} \cdot 512 + 32 \\
 *   &=& (T+33678) \cdot \frac{9}{5} \cdot 512
 * \f}
 *
 *
 *
 * \addtogroup temps
 * @{
 */

/**
 * \def C_OFFSET
 * \brief Offset used when representing C temperatures.
 *
 * See the Conversion section for an explanation on how this was derived.  This
 * is the default offset for internal representation.
 */
#define C_OFFSET (-24576)

/**
 * \def F_OFFSET
 * \brief Offset used when representing F temperatures.
 *
 * See the Conversion section for an explanation on how this was derived.
 */
#define F_OFFSET (-33678)


// just for clarity, typedefs are used instead of normal integers.
// Addition and shifting can be done normally. When two fixed points values are multiplied, you have shift the result
typedef int16_t fixed7_9; //!< 7 signed int bits and 9 fraction bits
typedef int32_t fixed23_9; //!< 23 signed int bits and 9 fraction bits. Used when results can overflow
typedef int32_t fixed7_25; //!< 7 signed int bits and 25 fraction bits. Used when extra precision is needed
typedef int16_t fixed12_4; //!< 1 sign bit, 11 integer bits, and 4 fraction bits - encoding returned by DS18B20 sensors.
typedef int8_t fixed4_4; //!< 1-sign bit, 3 int bits and 4 fraction bits. Corresponds with precision of DS18B20 sensors

/**
 * \def INVALID_TEMP
 * \brief An invalid temperature value
 */
#define INVALID_TEMP -32768

/**
 * \def MAX_TEMP
 * \brief Maximum representable temp value
 */
#define MAX_TEMP 32767

/**
 * \def MIN_TEMP
 * \brief Minimum representable temp value
 */
#define MIN_TEMP INVALID_TEMP+1

typedef int8_t temp_int; //!< Temperature expressed as an integer.
typedef fixed7_9 temperature; //!< Common temperature representation
typedef fixed23_9 long_temperature; //!< Long temperature representation
typedef fixed7_25 temperature_precise; //!< Precise temperature representation

#define TEMP_FIXED_POINT_BITS (9)
#define TEMP_FIXED_POINT_SCALE (1<<TEMP_FIXED_POINT_BITS)
#define TEMP_FIXED_POINT_MASK (TEMP_FIXED_POINT_SCALE-1)
#define TEMP_PRECISE_EXTRA_FRACTION_BITS 16

#if 0

inline int8_t tempToInt(temperature val) {
    return int8_t((val - C_OFFSET) >> TEMP_FIXED_POINT_BITS);
}

inline int16_t longTempToInt(long_temperature val) {
    return int16_t((val - C_OFFSET) >> TEMP_FIXED_POINT_BITS);
}

inline int8_t tempDiffToInt(temperature val) {
    return int8_t((val) >> TEMP_FIXED_POINT_BITS);
}

inline int16_t longTempDiffToInt(long_temperature val) {
    return int16_t((val) >> TEMP_FIXED_POINT_BITS);
}

inline temperature intToTemp(int8_t val) {
    return (temperature(val) << TEMP_FIXED_POINT_BITS) +C_OFFSET;
}

inline temperature intToTempDiff(int8_t val) {
    return (temperature(val) << TEMP_FIXED_POINT_BITS);
}

inline temperature doubleToTemp(double temp) {
    return (temp * TEMP_FIXED_POINT_SCALE + C_OFFSET) >= MAX_TEMP ? MAX_TEMP : (temp * TEMP_FIXED_POINT_SCALE + C_OFFSET) <= MIN_TEMP ? MIN_TEMP : temperature(temp * TEMP_FIXED_POINT_SCALE + C_OFFSET);
}

inline long_temperature intToLongTemp(int16_t val) {
    return (long_temperature(val) << TEMP_FIXED_POINT_BITS) +C_OFFSET;
}

inline temperature tempPreciseToRegular(temperature_precise val) {
    return val >> TEMP_PRECISE_EXTRA_FRACTION_BITS;
}

inline temperature_precise tempRegularToPrecise(temperature val) {
    return temperature_precise(val) << TEMP_PRECISE_EXTRA_FRACTION_BITS;
}
#else
#define tempToInt(val) ((val - C_OFFSET)>>TEMP_FIXED_POINT_BITS)
#define longTempToInt(val) ((val - C_OFFSET)>>TEMP_FIXED_POINT_BITS)
#define tempDiffToInt(val) ((val)>>TEMP_FIXED_POINT_BITS)
#define longTempDiffToInt(val) ((val)>>TEMP_FIXED_POINT_BITS)


#define intToTemp(val) ((temperature(val)<<TEMP_FIXED_POINT_BITS) + C_OFFSET)
#define intToTempDiff(val) ((temperature(val)<<TEMP_FIXED_POINT_BITS))
#define doubleToTemp(temp) ((temp*TEMP_FIXED_POINT_SCALE + C_OFFSET)>=MAX_TEMP ? MAX_TEMP : (temp*TEMP_FIXED_POINT_SCALE + C_OFFSET)<=MIN_TEMP ? MIN_TEMP : temperature(temp*TEMP_FIXED_POINT_SCALE + C_OFFSET))
#define intToLongTemp(val) ((long_temperature(val)<<TEMP_FIXED_POINT_BITS) + C_OFFSET)
#define tempPreciseToRegular(val) (val>>TEMP_PRECISE_EXTRA_FRACTION_BITS)
#define tempRegularToPrecise(val) (temperature_precise(val)<<TEMP_PRECISE_EXTRA_FRACTION_BITS)

#endif

char * tempToString(char * s, long_temperature rawValue, const uint8_t numDecimals, const uint8_t maxLength);
temperature stringToTemp(const char * string);

char * tempDiffToString(char * s, long_temperature rawValue, const uint8_t numDecimals, const uint8_t maxLength);
temperature stringToTempDiff(const char * string);

char * fixedPointToString(char * s, long_temperature rawValue, const uint8_t numDecimals, const uint8_t maxLength);
char * fixedPointToString(char * s, temperature rawValue, const uint8_t numDecimals, const uint8_t maxLength);
long_temperature stringToFixedPoint(const char * numberString);

int fixedToTenths(long_temperature temperature);
temperature tenthsToFixed(const int temperature);

temperature constrainTemp(long_temperature val, const temperature lower, const temperature upper);

temperature constrainTemp16(long_temperature val);


temperature multiplyFactorTemperatureLong(const temperature factor, const long_temperature b);
temperature multiplyFactorTemperatureDiffLong(const temperature factor, const long_temperature b);
temperature multiplyFactorTemperature(const temperature factor, const temperature b);
temperature multiplyFactorTemperatureDiff(const temperature factor, const temperature b);


long_temperature convertToInternalTempImpl(long_temperature rawTemp, const bool addOffset);
long_temperature convertFromInternalTempImpl(long_temperature rawTemp, const bool addOffset);

inline long_temperature convertToInternalTempDiff(long_temperature rawTempDiff) {
    return convertToInternalTempImpl(rawTempDiff, false);
}

inline long_temperature convertFromInternalTempDiff(long_temperature rawTempDiff) {
    return convertFromInternalTempImpl(rawTempDiff, false);
}

inline long_temperature convertToInternalTemp(long_temperature rawTemp) {
    return convertToInternalTempImpl(rawTemp, true);
}

inline long_temperature convertFromInternalTemp(long_temperature rawTemp) {
    return convertFromInternalTempImpl(rawTemp, true);
}


double tempToDouble(long_temperature rawTemp, const uint8_t numDecimals);

/** @} */
