#include "main.h"

/* --------------------------- Main Loop --------------------------- */

void app_main()
{
    static const char *TAG = "MAIN";
    esp_err_t status;

    status = setupWolfMQTTGateway();
    if (status != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize gateway: %s", esp_err_to_name(status));
    }

    ESP_LOGI(TAG, "%s ready.", deviceId);

}