#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>

#include <driver/gpio.h>

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <esp_system.h>

#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>

#include <mqtt_client.h>

#include "device.h"

#define PROV_MAX_RETRY                  3

#define RESET_PROV_BUTTON_GPIO          21
#define RESET_PROV_BUTTON_GPIO_MASK     (1ULL << RESET_PROV_BUTTON_GPIO)

ESP_EVENT_DECLARE_BASE(MQTT_EVENTS);

// #define MQTT_SERVER_URL         "mqtt://tmt.smarthotel.io"
#define MQTT_SERVER_URL         "mqtt://broker.hivemq.com"
#define MQTT_SERVER_TOPIC       "test/device1"
#define DEVICE_PUBLISH_TOPIC    ""

static const char *TAG = "app";

const int MQTT_CONNECTED_EVENT = BIT0;
static EventGroupHandle_t mqtt_event_group;

/* MQTT client handle */
static esp_mqtt_client_handle_t mqtt_client;

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    if (gpio_num == RESET_PROV_BUTTON_GPIO) {

        /* Erase NVS Flash & reboot */
        ESP_ERROR_CHECK(nvs_flash_erase());
        esp_restart();
    }
}

/* Event handler for catching system events */
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {

    static int retries_prov;
    // static int retries_mqtt;

    /* WiFi Provision Event */
    if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {
            case WIFI_PROV_START:
                ESP_LOGI(TAG, "Provisioning started");
                break;
            case WIFI_PROV_CRED_RECV: {
                wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
                ESP_LOGI(TAG, "Received Wi-Fi credentials"
                         "\n\tSSID     : %s\n\tPassword : %s",
                         (const char *) wifi_sta_cfg->ssid,
                         (const char *) wifi_sta_cfg->password);
                break;
            }
            case WIFI_PROV_CRED_FAIL: {
                wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
                ESP_LOGE(TAG, "Provisioning failed!\n\tReason : %s"
                         "\n\tPlease reset to factory and retry provisioning",
                         (*reason == WIFI_PROV_STA_AUTH_ERROR) ?
                         "Wi-Fi station authentication failed" : "Wi-Fi access-point not found");

                retries_prov++;
                if (retries_prov >= PROV_MAX_RETRY) {
                    ESP_LOGI(TAG, "Failed to connect with provisioned AP, reseting provisioned credentials");
                    wifi_prov_mgr_reset_sm_state_on_failure();
                    retries_prov = 0;
                }
                break;
            }
            case WIFI_PROV_CRED_SUCCESS:
                ESP_LOGI(TAG, "Provisioning successful");
                retries_prov = 0;
                break;
            case WIFI_PROV_END:
                /* De-initialize manager once provisioning is finished */
                wifi_prov_mgr_deinit();
                break;
            default:
                break;
        }    
    }

    /* WiFi Event */
    else if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "Wifi STA Connected");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "Disconnected. Connecting to the AP again...");
            esp_wifi_connect();
            break;
        default:
            break;
        }
    }
    
    /* MQTT Event */
    else if (event_base == MQTT_EVENTS) {
        esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data;
        esp_mqtt_client_handle_t client = event->client;
        switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            esp_mqtt_client_subscribe(client, MQTT_SERVER_TOPIC, 0);
            // retries_mqtt = 0;
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            // retries_mqtt++;
            // if (retries_mqtt >= 3) {
            //     retries_mqtt = 0;
            //     esp_restart();
            // }
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED");
            xEventGroupSetBits(mqtt_event_group, MQTT_CONNECTED_EVENT);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED");
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED");
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
        }
    }
    
    /* IP Event */
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Connected with IP Address:" IPSTR, IP2STR(&event->ip_info.ip));

        /* Start MQTT Connection */
        esp_mqtt_client_start(mqtt_client);
    }
}

static void mqtt_client_init(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = MQTT_SERVER_URL,
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, event_handler, mqtt_client);
}

static void wifi_init_sta(void) {
    /* Start Wi-Fi in station mode */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void get_device_service_name(char *service_name, size_t max) {
    uint8_t eth_mac[6];
    const char *ssid_prefix = "PROV_";
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    snprintf(
        service_name,
        max,
        "%s%02X%02X%02X%02X%02X%02X",
        ssid_prefix,
        eth_mac[0], eth_mac[1], eth_mac[2], eth_mac[3], eth_mac[4], eth_mac[5]
    );
}

/* Handler for the optional provisioning endpoint registered by the application.
 * The data format can be chosen by applications. Here, we are using plain ascii text.
 * Applications can choose to use other formats like protobuf, JSON, XML, etc.
 */
esp_err_t custom_prov_data_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen, uint8_t **outbuf, ssize_t *outlen, void *priv_data) {
    if (inbuf) {
        ESP_LOGI(TAG, "Received data: %.*s", inlen, (char *)inbuf);
    }
    char response[] = "SUCCESS";
    *outbuf = (uint8_t *)strdup(response);
    if (*outbuf == NULL) {
        ESP_LOGE(TAG, "System out of memory");
        return ESP_ERR_NO_MEM;
    }
    *outlen = strlen(response) + 1; /* +1 for NULL terminating byte */

    return ESP_OK;
}

void reset_provision_button_init(void) {
    gpio_config_t btn_cfg = {
        .intr_type = GPIO_INTR_POSEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = RESET_PROV_BUTTON_GPIO_MASK,
        .pull_down_en = 0,
        .pull_up_en = 1
    };

    /* Configure GPIO with the given settings */
    gpio_config(&btn_cfg);

    /* Install gpio isr service */
    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);

    /* Hook isr handler for specific gpio pin */
    gpio_isr_handler_add(RESET_PROV_BUTTON_GPIO, gpio_isr_handler, (void*) RESET_PROV_BUTTON_GPIO);
}

void app_main(void) {

    ESP_ERROR_CHECK(nvs_flash_init());

    /* Initialize TCP/IP */
    ESP_ERROR_CHECK(esp_netif_init());

    /* Initialize the event loop */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    mqtt_event_group = xEventGroupCreate();

    /* Register our event handler for Wi-Fi, IP and Provisioning related events */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    /* Initialize Wi-Fi including netif with default config */
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* MQTT Client Initialize */
    mqtt_client_init();

    /* Enable reset button */
    reset_provision_button_init();

    /* Configuration for the provisioning manager */
    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
    };

    /* Initialize provisioning manager with the
     * configuration parameters set above */
    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

    bool provisioned = false;
    /* Let's find out if the device is provisioned */
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    /* If device is not yet provisioned start provisioning service */
    if (!provisioned) {
        ESP_LOGI(TAG, "Starting provisioning");

        /* What is the Device Service Name that we want
         * This translates to :
         *     - Wi-Fi SSID when scheme is wifi_prov_scheme_softap
         *     - device name when scheme is wifi_prov_scheme_ble
         */
        char service_name[18];
        get_device_service_name(service_name, sizeof(service_name));

        /* What is the security level that we want (0 or 1):
         *      - WIFI_PROV_SECURITY_0 is simply plain text communication.
         *      - WIFI_PROV_SECURITY_1 is secure communication which consists of secure handshake
         *          using X25519 key exchange and proof of possession (pop) and AES-CTR
         *          for encryption/decryption of messages.
         */
        wifi_prov_security_t security = WIFI_PROV_SECURITY_1;

        /* Do we want a proof-of-possession (ignored if Security 0 is selected):
         *      - this should be a string with length > 0
         *      - NULL if not used
         */
        const char *pop = "abcd1234";

        /* What is the service key (could be NULL)
         * This translates to :
         *     - Wi-Fi password when scheme is wifi_prov_scheme_softap
         *     - simply ignored when scheme is wifi_prov_scheme_ble
         */
        const char *service_key = NULL;

        /* This step is only useful when scheme is wifi_prov_scheme_ble. This will
         * set a custom 128 bit UUID which will be included in the BLE advertisement
         * and will correspond to the primary GATT service that provides provisioning
         * endpoints as GATT characteristics. Each GATT characteristic will be
         * formed using the primary service UUID as base, with different auto assigned
         * 12th and 13th bytes (assume counting starts from 0th byte). The client side
         * applications must identify the endpoints by reading the User Characteristic
         * Description descriptor (0x2901) for each characteristic, which contains the
         * endpoint name of the characteristic */
        uint8_t custom_service_uuid[] = {
            /* LSB <---------------------------------------
             * ---------------------------------------> MSB */
            0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
            0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02,
        };
        wifi_prov_scheme_ble_set_service_uuid(custom_service_uuid);

        /* An optional endpoint that applications can create if they expect to
         * get some additional custom data during provisioning workflow.
         * The endpoint name can be anything of your choice.
         * This call must be made before starting the provisioning.
         */
        wifi_prov_mgr_endpoint_create("custom-data");
        /* Start provisioning service */
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, pop, service_name, service_key));

        /* The handler for the optional endpoint created above.
         * This call must be made after starting the provisioning, and only if the endpoint
         * has already been created above.
         */
        wifi_prov_mgr_endpoint_register("custom-data", custom_prov_data_handler, NULL);

    } else {
        ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi STA");

        /* We don't need the manager as device is already provisioned,
         * so let's release it's resources */
        wifi_prov_mgr_deinit();

        /* Start Wi-Fi station */
        wifi_init_sta();
    }

    /* Wait for MQTT connection */
    xEventGroupWaitBits(mqtt_event_group, MQTT_CONNECTED_EVENT, false, true, portMAX_DELAY);

    // bool mqtt_provisioned;
    // device_is_mqtt_provisioned(&mqtt_provisioned);

    // if (mqtt_provisioned) {
    //     ESP_LOGI(TAG, "Already provisioned (MQTT)");
    // } else {
    //     // char* mqtt_prov_data = device_get_mqtt_provision_json_data();
    //     // esp_mqtt_client_publish(mqtt_client, MQTT_SERVER_TOPIC, mqtt_prov_data, strlen(mqtt_prov_data), 0, 0);
    //     // device_set_provisioned();

    // }

    /* Start application here */
    ESP_LOGI(TAG, "Start application here");

    device_init("test");
    device_add_channel("123", 1, CHANNEL_DATA_BOOL, CONTROL_ONLY);
    device_add_channel("test", 2, CHANNEL_DATA_INT, MONITOR_AND_CONTROL);
    device_add_channel("test2", 3, CHANNEL_DATA_INT, MONITOR_AND_CONTROL);
    device_add_channel("test3", 4, CHANNEL_DATA_INT, MONITOR_AND_CONTROL);
    device_add_channel("test4", 5, CHANNEL_DATA_INT, MONITOR_AND_CONTROL);

    device_remove_channel(2);
    device_remove_channel(1);

    device_get_mqtt_provision_json_data();
}