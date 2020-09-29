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
 * 
 * You should have received a copy of the GNU General Public License
 * along with BrewPi.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "Brewpi.h"
#include "FilterFixed.h"
#include <stdlib.h>
#include <limits.h>
#include "TemperatureFormats.h"

/**
 * \brief Add a temperature value to the filter
 *
 * @param val - Temp value to add
 */
temperature FixedFilter::add(temperature val){
	temperature_precise returnVal = addDoublePrecision(tempRegularToPrecise(val));
	return tempPreciseToRegular(returnVal);
}


/**
 * \brief Add a temperature value to the filter
 *
 * @param val - Temp value to add
 */
temperature_precise FixedFilter::addDoublePrecision(temperature_precise val){
	xv[2] = xv[1];
	xv[1] = xv[0];
	xv[0] = val;

	yv[2] = yv[1];
	yv[1] = yv[0];

	/* Implementation that prevents overflow as much as possible by order of operations: */
	yv[0] = ((yv[1] - yv[2]) + yv[1]) // expected value + 1*
	- (yv[1]>>b) + (yv[2]>>b) + // expected value +0*
	+ (xv[0]>>a) + (xv[1]>>(a-1)) + (xv[2]>>a) // expected value +(1>>(a-2))
	- (yv[2]>>(a-2)); // expected value -(1>>(a-2))

	return yv[0];
}


/**
 * \brief Initialize the filter
 *
 * @param val - Initial temp value to seed the filter with
 */
void FixedFilter::init(temperature val){
		xv[0] = val;
		xv[0] = tempRegularToPrecise(xv[0]); // 16 extra bits are used in the filter for the fraction part

		xv[1] = xv[0];
		xv[2] = xv[0];

		yv[0] = xv[0];
		yv[1] = xv[0];
		yv[2] = xv[0];
}

/**
 * \brief Detect the positive peak
 * @return positive peak or INVALID_TEMP when no peak has been found
 */
temperature FixedFilter::detectPosPeak(){
	if(yv[0] < yv[1] && yv[1] >= yv[2]){
		return tempPreciseToRegular(yv[1]);
	}
	else{
		return INVALID_TEMP;
	}
}


/**
 * \brief Detect the negative peak
 * @return negative peak or INVALID_TEMP when no peak has been found
 */
temperature FixedFilter::detectNegPeak(){
	if(yv[0] > yv[1] && yv[1] <= yv[2]){
		return tempPreciseToRegular(yv[1]);
	}
	else{
		return INVALID_TEMP;
	}
}
