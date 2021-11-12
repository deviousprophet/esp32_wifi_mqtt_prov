#include <stdlib.h>
#include <string.h>
#include <cJSON.h>

#include <esp_wifi.h>
#include <esp_log.h>
#include <nvs.h>

#include "device.h"

static device_t g_device;

static const char* TAG = "device";

static void get_device_id(char* id) {
    uint8_t eth_mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    sprintf(
        id,
        "%02X%02X%02X%02X%02X%02X",
        eth_mac[0], eth_mac[1], eth_mac[2], eth_mac[3], eth_mac[4], eth_mac[5]
    );
}

static void print_list(void) {
    device_channel_t* temp = g_device.channels;

    printf("channels list\n");
    while(temp != NULL) {
        printf("%s %d %d %d\n", temp->name, temp->id, temp->type, temp->role);
        temp = temp->next;
    }
}

static void _add_channel(device_channel_t** channel_ref, const char* channel_name, int channel_id,
                            channel_data_type_t type, channel_data_role_t role) {

    device_channel_t* new_channel = (device_channel_t*) malloc(sizeof(device_channel_t));

    new_channel->name = malloc(sizeof(channel_name) + 1);
    strcpy(new_channel->name, channel_name);

    new_channel->id = channel_id;
    
    new_channel->type = type;
    
    new_channel->role = role;
    
    new_channel->next = *channel_ref;
    
    *channel_ref = new_channel;

}

static void _remove_channel(device_channel_t** channel_ref, int channel_id) {

    device_channel_t* temp = *channel_ref;
    device_channel_t* prev = NULL;
    
    if (temp != NULL && temp->id == channel_id) {
        *channel_ref = temp->next;
        free(temp);
        return;
    }

    
    while (temp != NULL && temp->id != channel_id) {
        prev = temp;
        temp = temp->next;
    }
 
    if (temp == NULL)
        return;
 
    prev->next = temp->next;
 
    free(temp);
}

void device_is_mqtt_provisioned(bool* provisioned) {

    nvs_handle_t mqtt_prov_handle;
    uint8_t prov_state;
    *provisioned = false;
    
    nvs_open("storage", NVS_READWRITE, &mqtt_prov_handle);
    nvs_get_u8(mqtt_prov_handle, "mqtt_prov", &prov_state);    
    if (!prov_state) *provisioned = true;
    nvs_close(mqtt_prov_handle);
}

void device_set_provisioned(void) {
    nvs_handle_t mqtt_prov_handle;
    nvs_open("storage", NVS_READWRITE, &mqtt_prov_handle);
    nvs_set_u8(mqtt_prov_handle, "mqtt_prov", 0x00);
    nvs_close(mqtt_prov_handle);
}

void device_init(const char* device_name) {

    /* Device name */
    g_device.name = malloc(sizeof(device_name) + 1);
    strcpy(g_device.name, device_name);

    /* Device ID - MAC address */
    char id[13];
    get_device_id(id);
    g_device.id = malloc(sizeof(id) + 1);
    strcpy(g_device.id, id);

    if(g_device.channels) {
        free(g_device.channels);
        g_device.channels = NULL;
    }

    ESP_LOGI(TAG, "Device created with name \"%s\" and id \"%s\"", g_device.name, g_device.id);
}

void device_add_channel(const char* channel_name, int channel_id, channel_data_type_t type, channel_data_role_t role) {
    _add_channel(&g_device.channels, channel_name, channel_id, type, role);
    print_list();
}

void device_remove_channel(int channel_id) {
    _remove_channel(&g_device.channels, channel_id);
    print_list();
}

char* device_get_mqtt_provision_json_data(void) {

    cJSON* device = cJSON_CreateObject();
    
    /* Device name */
    cJSON_AddStringToObject(device, "device_name", g_device.name);
    
    /* Device ID - MAC address */
    cJSON_AddStringToObject(device, "device_id", g_device.id);

    /* Device Channels */
    cJSON* channels = cJSON_AddArrayToObject(device, "channels");
    device_channel_t* temp = g_device.channels;
    while (temp != NULL) {
        cJSON* new_channel = cJSON_CreateObject();

        cJSON_AddStringToObject(new_channel, "channel_name", temp->name);

        cJSON_AddNumberToObject(new_channel, "channel_id", temp->id);

        cJSON_AddNumberToObject(new_channel, "type", temp->type);

        cJSON_AddNumberToObject(new_channel, "role", temp->role);

        cJSON_AddItemToArray(channels, new_channel);

        temp = temp->next;
    }

    char* output_buf = cJSON_PrintUnformatted(device);

    ESP_LOGI(TAG, "Device Provision JSON data:\n%s", cJSON_Print(device));
    
    cJSON_Delete(device);
    return output_buf;
}

char* device_get_mqtt_monitor_json_data(void) {
    
}