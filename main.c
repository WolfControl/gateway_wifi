#include "shared_other.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_err.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "mqtt_client.h"
#include "serialbridge.h"

/* --------------------------- MQTT Related Globals & Stubs --------------------------- */

const static char* mqtt_uri = "mqtt://192.168.1.13";
const static uint32_t mqtt_port = 1883;
const static char* mqtt_username = "collector";
const static char* mqtt_password = "collectorpassword";

// Define FreeRTOS event group and bits
static EventGroupHandle_t s_mqtt_event_group;
#define MQTT_CONNECTED_BIT BIT0
#define MQTT_FAIL_BIT      BIT1

esp_mqtt_client_handle_t mqttClient;
#define             MQTT_MSG_SIZE 200
char                mqttTopic[MQTT_MSG_SIZE];
#define             MSG_BUFFER_SIZE (50)
char*               thingName;
const char*         willTopic = "homie/gateway1/$state";
const char*         willMessage = "lost";
bool                willRetain = false;
uint8_t             willQoS = 0;

/* --------------------------- WiFi Related Globals & Stubs --------------------------- */

const static uint8_t ssid[] = "SigmaNet";
const static uint8_t password[] = "Octagon1";

// Define FreeRTOS event group and bits
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// Wifi retry counter
static int s_retry_num = 0;

/* --------------------------- Callback Functions & Handlers --------------------------- */

static void wifi_event_handler(void* ctx, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
  const char*TAG = "WIFI_EVENT_HANDLER";

    switch (event_id) {
    case WIFI_EVENT_STA_START:
        if (event_base == WIFI_EVENT) {
            ESP_LOGI(TAG, "Making initial connection...");
            esp_wifi_connect();
        }
        break;

    case WIFI_EVENT_STA_DISCONNECTED:
        if (event_base == WIFI_EVENT) {
            if (s_retry_num < 5) {
                ESP_LOGI(TAG, "Lost connection to AP. Retrying...");
                esp_wifi_connect();
                s_retry_num++;
            } else {
                ESP_LOGW(TAG, "Max retries exceeded. Setting fail bit...");
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            }
        }
        break;

    case IP_EVENT_STA_GOT_IP:
        if (event_base == IP_EVENT) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;

            ESP_LOGI(TAG, "Connected! IP:" IPSTR, IP2STR(&event->ip_info.ip));

            ESP_LOGI(TAG, "Resetting internal counters...");
            s_retry_num = 0;
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
        break;

    default:
        break;
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
  const char*TAG = "MQTT_EVENT_HANDLER";

    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);

    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        xEventGroupSetBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        xEventGroupClearBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_ESP_TLS) {
            ESP_LOGE(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGE(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
        } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGE(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        } else {
            ESP_LOGE(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void receivedDeviceMessage(const char* topic, const char* payload)
{
  const char*TAG = "receivedDeviceMessage";
    ESP_LOGI(TAG, "Forwarding message to MQTT...");
    esp_mqtt_client_publish(mqttClient, topic, payload, 0, 1, 0);
}

static void receivedBridgeMessage(const char* payload)
{
  const char*TAG = "receivedBridgeMessage";
    if (strcmp(payload, "ESPNOWSetupSucceded"))
    {
        ESP_LOGI(TAG, "ESPNOW setup succeded!");
        xEventGroupSetBits(s_bridge_event_group, BRIDGE_CONNECTED_BIT);
    }
    else if (strcmp(payload, "ESPNOWSetupFailed"))
    {
        ESP_LOGE(TAG, "ESPNOW setup failed!");
        xEventGroupSetBits(s_bridge_event_group, BRIDGE_FAIL_BIT);
    }
    else
    {
        ESP_LOGW(TAG, "Received unhandled message from bridge!");
    }

}

// This is the callback function that will be called when a message is received over serial
// Determines if message is from device or bridge and calls appropriate subhandler
void onMessagereceiveHandler(const char* topic, const char* payload)
{
  const char*TAG = "messageHandler";
    ESP_LOGI(TAG, "Received message on topic %s: %s", topic, payload);

    if (strncmp(topic, "homie/", 6) == 0)
    {
        ESP_LOGI(TAG, "Received message from device. Forwarding to MQTT...");
        receivedDeviceMessage(topic, payload);
    }
    else if (strncmp(topic, "bridge", 6) == 0)
    {
        ESP_LOGI(TAG, "Received message from bridge. Handling internally...");
        receivedBridgeMessage(payload);
    }
    else
    {
        ESP_LOGE(TAG, "Received message on unknown topic!");
    }
}

/* --------------------------- Setup Functions --------------------------- */

void setupWIFI()
{
  const char*TAG = "setupWIFI";

    ESP_LOGI(TAG, "Creating wifi event group...");
    s_wifi_event_group = xEventGroupCreate();

    ESP_LOGI(TAG, "Initializing tcpip adapter...");
    ESP_ERROR_CHECK(esp_netif_init());
    
    ESP_LOGI(TAG, "Initializing default station...");
    esp_netif_create_default_wifi_sta();

    ESP_LOGI(TAG, "Copying default config...");
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_LOGI(TAG, "Initializing wifi...");
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_LOGI(TAG, "Registering event handlers...");
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_t instance_lost_ip;

    // In English: Set any events under WIFI_EVENT with any ID to call wifi_event_handler and register it to instance_any_id. Error check.
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_LOST_IP, &wifi_event_handler, NULL, &instance_lost_ip));


    wifi_config_t wifi_config = {
        .sta = {
            .ssid = {0},
            .password = {0},
        },
    };

    // This will let us put the ssid and password somewhere else and encrypt it + save to NVS in the future
    memcpy(wifi_config.sta.ssid, ssid, sizeof(ssid));
    memcpy(wifi_config.sta.password, password, sizeof(password));

    ESP_LOGI(TAG, "Setting WiFi config...");

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );

    ESP_LOGI(TAG, "Starting WiFi...");
    ESP_ERROR_CHECK(esp_wifi_start() );

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to AP SSID:%s password:%s",
                 ssid, password);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to SSID:%s, password:%s", ssid, password);
        exit(1);

    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

void setupMQTT()
{
  const char*TAG = "setupMQTT";

    ESP_LOGI(TAG, "Creating MQTT event group...");
    s_mqtt_event_group = xEventGroupCreate();

    ESP_LOGI(TAG, "Initializing MQTT client config...");
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = mqtt_uri,
        .port = mqtt_port,
        .client_id = "ESP32 Gateway",
        .username = mqtt_username,
        .password = mqtt_password,        

    };

    ESP_LOGI(TAG, "Initializing MQTT client...");
    mqttClient = esp_mqtt_client_init(&mqtt_cfg);

    ESP_LOGI(TAG, "Registering MQTT event handler...");
    esp_mqtt_client_register_event(mqttClient, MQTT_EVENT_ANY, mqtt_event_handler, NULL);

    ESP_LOGI(TAG, "Starting MQTT client...");
    esp_mqtt_client_start(mqttClient);

    ESP_LOGI(TAG, "Waiting for MQTT connection...");

    /* Wait until either the connection is established (MQTT_CONNECTED_BIT) or connection failed (MQTT_FAIL_BIT). */
    EventBits_t bits = xEventGroupWaitBits(s_mqtt_event_group, MQTT_CONNECTED_BIT | MQTT_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & MQTT_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to MQTT broker at %s:%d", mqtt_uri, mqtt_port);
    } else if (bits & MQTT_FAIL_BIT) {
        ESP_LOGW(TAG, "Failed to connect to MQTT broker at %s:%d", mqtt_uri, mqtt_port);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

}

/* --------------------------- Main Loop --------------------------- */

void app_main()
{
  const char*TAG = "MAIN";

    ESP_LOGI(TAG, "Call OS setup");
    setupOS();

    ESP_LOGI(TAG, "Call flash setup");
    setupFlash();

    ESP_LOGI(TAG, "Call bridge setup");
    setupBridge(&onMessagereceiveHandler, 27, 13);

    ESP_LOGI(TAG, "Call wifi setup");
    setupWIFI();

    ESP_LOGI(TAG, "Call MQTT setup");
    setupMQTT();

    ESP_LOGD(TAG, "All setup functions complete.");

    // Announce our presence
    esp_mqtt_client_publish(mqttClient, "homie/gateway1/$name", "ESP32 Gateway", 0, 1, 0);
    esp_mqtt_client_publish(mqttClient, "homie/gateway1/$nodes", "", 0, 1, 0);
    esp_mqtt_client_publish(mqttClient, "homie/gateway1/$state", "ready", 0, 1, 0);

}
