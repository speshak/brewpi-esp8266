#include <Arduino.h>
#include <unity.h>

#include "Logger.h"
#include "test_logger.h"

void test_errors() {
  // No asserts, just trusting that if we don't explode things must be mostly OK
  logError(ERROR_ONEWIRE_INIT_FAILED);
  logErrorInt(ERROR_INVALID_CHAMBER, 3);
  logErrorString(ERROR_SRAM_SENSOR, "deadbeef");
  // logErrorTemp();
  logErrorIntInt(ERROR_CANNOT_ASSIGN_TO_HARDWARE, 1, 2);
  logErrorIntIntInt(ERROR_INVALID_DEVICE_CONFIG_OWNER, 1, 1, 1);
}

void test_warnings() { logWarning(WARNING_START_IN_SAFE_MODE); }

void test_logger() {
  RUN_TEST(test_errors);
  RUN_TEST(test_warnings);
}
