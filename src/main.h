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
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_err.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "mqtt_client.h"
#include "messaging.h"
#include "provisioning.h"
#include "led.h"

// ---------- messaging.h ---------- //

esp_now_peer_info_t broadcastPeer;
TaskHandle_t receiveSerialTaskHandle, receiveESPNowTaskHandle, sendESPNowTaskHandle, sendSerialTaskHandle, serialDaemonTaskHandle;
QueueHandle_t incomingESPNowQueue, outgoingESPNowQueue, incomingSerialQueue, outgoingSerialQueue;

const int txPin = 16;
const int rxPin = 17;

// ---------- provisioning.h ---------- //

EventGroupHandle_t s_wifi_event_group;
EventGroupHandle_t s_mqtt_event_group;
EventGroupHandle_t adoption_event_group;
EventGroupHandle_t bridge_event_group;

SemaphoreHandle_t mutex;

TimerHandle_t heartbeatTimer;

char *deviceId;
char *adoptionStatus;

esp_mqtt_client_handle_t mqttClient;
uint8_t *ssid;
uint8_t *password;
char *mqtt_uri;
uint32_t mqtt_port = 1883;

int s_retry_num = 0;

const gpio_num_t powerLED = GPIO_NUM_NC;
const gpio_num_t adoptedLED = GPIO_NUM_NC;
const gpio_num_t bridgeConnectedLED = GPIO_NUM_NC;

const gpio_num_t wifiConnectedLED = GPIO_NUM_NC;
const gpio_num_t mqttConnectedLED = GPIO_NUM_NC;

const gpio_num_t espnowActivityLED = GPIO_NUM_NC;
const gpio_num_t serialActivityLED = GPIO_NUM_NC;
const gpio_num_t mqttActivityLED = GPIO_NUM_NC;

char *firmwareVersionWifi = "0.0.1";
char *firmwareVersionEspNow;
char *espnowmac;
char *espwifimac;

// ---------- led.h ---------- //

rgb_led_t bootLED = {
    LEDC_CHANNEL_0,
    5,
    LEDC_CHANNEL_1,
    6,
    LEDC_CHANNEL_2,
    7};
rgb_led_t connectivityLED = {
    LEDC_CHANNEL_3,
    36,
    LEDC_CHANNEL_4,
    37,
    LEDC_CHANNEL_5,
    38};
led_t activityLED1 = {LEDC_CHANNEL_6, 1};
led_t activityLED2 = {LEDC_CHANNEL_7, 2};

// ---------- main.h ---------- //

void app_main();