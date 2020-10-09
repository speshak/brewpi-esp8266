#pragma once

#include "EnumHelpers.h"
#include <unity.h>

enum class DummyEnum : char {
  one = '1',
  two = '2',
  red = 'r',
  blue = 'b',
};

void test_enum_helpers();
