#pragma once

#include <esp_task_wdt.h>

// Feed the Task Watchdog Timer only when the calling task is actually
// subscribed to it. esp_task_wdt_reset() logs
//   "task_wdt: esp_task_wdt_reset(...): task not found"
// on every call when the current task was never registered via
// esp_task_wdt_add(). Whether the Arduino loopTask is auto-subscribed depends
// on the chip target's framework sdkconfig: the ESP32-C3 (X4) build subscribes
// it, the classic-ESP32 (m5paper) build does not, so the unguarded resets in
// the web server / WiFi paths spammed the log there.
//
// esp_task_wdt_status(nullptr) returns ESP_OK only when the current task is
// subscribed, so this guard makes the reset a no-op on builds where the loop
// task is not watchdog-monitored, while preserving the reset where it is.
static inline void feedTaskWatchdog() {
  if (esp_task_wdt_status(nullptr) == ESP_OK) {
    esp_task_wdt_reset();
  }
}
