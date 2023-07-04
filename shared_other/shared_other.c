#include "shared_other.h"

void setupOS()
{
    ESP_LOGI("WIFI", "Creating default event loop...");
    ESP_ERROR_CHECK(esp_event_loop_create_default());
}

void setupFlash()
{
  const char*TAG = "setupFlash";

    ESP_LOGI(TAG, "Initializing NVS flash...");
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        ESP_LOGW(TAG, "No free pages OR new version found. Erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());

        ESP_LOGI(TAG, "Initializing NVS flash again...");
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "NVS flash initialized.");
}