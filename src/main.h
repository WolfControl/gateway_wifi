#include "esp_log.h"
#include "esp_err.h"
#include "messaging.h"
#include "provisioning.h"
#include "led.h"

// ---------- messaging.h ---------- //

esp_now_peer_info_t broadcastPeer;
TaskHandle_t receiveSerialTaskHandle, receiveESPNowTaskHandle, sendESPNowTaskHandle, sendSerialTaskHandle, serialDaemonTaskHandle;
QueueHandle_t incomingESPNowQueue, outgoingESPNowQueue, incomingSerialQueue, outgoingSerialQueue;

const int txPin = 7;
const int rxPin = 8;

// ---------- provisioning.h ---------- //

EventGroupHandle_t s_wifi_event_group;
EventGroupHandle_t s_mqtt_event_group;
EventGroupHandle_t adoption_event_group;
EventGroupHandle_t bridge_event_group;
EventGroupHandle_t ota_event_group;

SemaphoreHandle_t mutex;

TimerHandle_t heartbeatTimer;

TaskHandle_t otaTaskHandle;
char *deviceId;
char *adoptionStatus;

esp_mqtt_client_handle_t mqttClient;
uint8_t *ssid;
uint8_t *password;
char *mqtt_uri;
uint32_t mqtt_port = 1883;

int s_retry_num = 0;

char *firmwareVersionWifi = "SETDURINGCOMPILATION";
char *firmwareVersionEspNow;
char *espnowmac;
char *wifimac;

// ---------- led.h ---------- //

rgb_led_t bootLED = {
    LEDC_CHANNEL_0,
    4,
    LEDC_CHANNEL_1,
    5,
    LEDC_CHANNEL_2,
    6};
rgb_led_t connectivityLED = {
    LEDC_CHANNEL_3,
    12,
    LEDC_CHANNEL_4,
    11,
    LEDC_CHANNEL_5,
    10};
led_t activityLED1 = {LEDC_CHANNEL_6, 13};
led_t activityLED2 = {LEDC_CHANNEL_7, 14};

// ---------- main.h ---------- //

void app_main();