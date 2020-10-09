#include <Arduino.h>
#include <unity.h>

#include "DeviceNameManager.h"

void test_get_invalid_name() {
  // Get an invalid name
  String name = DeviceNameManager::getDeviceName("blammo");
  TEST_ASSERT_EQUAL_STRING("", name.c_str());
}

void test_set_name() {
  DeviceNameManager::setDeviceName("blammo", "myname");
  String name = DeviceNameManager::getDeviceName("blammo");
  TEST_ASSERT_EQUAL_STRING("myname", name.c_str());
}

void test_devicenamemanager() {
  RUN_TEST(test_get_invalid_name);
  RUN_TEST(test_set_name);
}
