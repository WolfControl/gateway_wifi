#ifndef SHARED_OTHER_H
#define SHARED_OTHER_H

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "driver/uart.h"
#include "esp_vfs_dev.h"
#include "cJSON.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "string.h"

void setupOS();
void setupFlash();

#endif