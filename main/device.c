#include <stdlib.h>
#include <string.h>
#include <cJSON.h>

#include <esp_wifi.h>
#include <esp_log.h>

#include "device.h"

static const char* TAG = "device";

static device_t* g_device = NULL;
static uint8_t channel_id;

static void get_device_id(char* id) {
    uint8_t eth_mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    sprintf(
        id,
        "%02X%02X%02X%02X%02X%02X",
        eth_mac[0], eth_mac[1], eth_mac[2], eth_mac[3], eth_mac[4], eth_mac[5]
    );
}

static void add_channel(device_channel_t** channel_ref, const char* name, channel_data_type_t type) {
    device_channel_t* new_channel = (device_channel_t*) malloc(sizeof(device_channel_t));
    new_channel->name = malloc(sizeof(strlen(name) + 1));
    strcpy(new_channel->name, name);
    new_channel->data_type = type;
    new_channel->id = channel_id;
    new_channel->next = *channel_ref;
    *channel_ref = new_channel;

    ESP_LOGI(
        TAG, "Device added channel:\n    name: %s\n    id: %d\n    type: %s",
        name,
        channel_id,
        ((type == CHANNEL_DATA_BOOL) ? "bool" : ((type == CHANNEL_DATA_INT) ? "int" : ((type == CHANNEL_DATA_FLOAT) ? "float" : ((type == CHANNEL_DATA_STRING) ? "string" : "unknown"))))
    );

    channel_id++;
}

void device_init(const char* name) {

    channel_id = 0;

    g_device = (device_t*) malloc(sizeof(device_t));
    g_device->device_name = malloc(sizeof(strlen(name)) + 1);
    strcpy(g_device->device_name, name);
    ESP_LOGI(TAG, "Device init with name: %s", name);

    char id[14];
    get_device_id(id);
    g_device->device_id = malloc(sizeof(strlen(id)) + 1);
    strcpy(g_device->device_id, id);
    ESP_LOGI(TAG, "Device init with id: %s", id);
    
}

void device_add_channel(const char* name, channel_data_type_t type) {
    device_channel_t* g_channel = g_device->channels;
    add_channel(&g_channel, name, type);
}

void device_get_mqtt_provision_data(char* output_buf) {

}