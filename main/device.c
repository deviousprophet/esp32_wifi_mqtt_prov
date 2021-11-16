#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>

#include <esp_wifi.h>
#include <esp_log.h>
#include <nvs.h>

#include "device.h"

static device_t g_device;

static const char* TAG = "device";

void print_device(void) {
    device_channel_t* temp = g_device.channels;

    while (temp != NULL) {

        printf("channel name: %s\n", temp->name);
        printf("command: %d\n", temp->cmd);
        printf("type: %d\n", temp->type);
        
        if(temp->type == CHANNEL_TYPE_NUMBER) {
            printf(
                "max: %.2f min: %.2f multipleof: %.2f\n",
                temp->max,
                temp->min,
                temp->multipleof
            );
        } else if (temp->type == CHANNEL_TYPE_MULTI_OPTS) {
            printf("options:\n");
            // while (temp->options_list != NULL) {
            //     printf("    %s\n", temp->options_list->opt);
            //     temp->options_list = temp->options_list->next;
            // }
        }
        
        temp = temp->next;
        printf("\n");
    }

    printf("\n----------------------------------------\n");
}

static void get_device_id(char* id) {
    uint8_t eth_mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    sprintf(
        id,
        "%02X%02X%02X%02X%02X%02X",
        eth_mac[0], eth_mac[1], eth_mac[2], eth_mac[3], eth_mac[4], eth_mac[5]
    );
}

// static void _remove_channel(device_channel_t** channel_ref, int channel_id) {

//     device_channel_t* temp = *channel_ref;
//     device_channel_t* prev = NULL;
    
//     if (temp != NULL && temp->id == channel_id) {
//         *channel_ref = temp->next;
//         free(temp->name);
//         free(temp);
//         return;
//     }

    
//     while (temp != NULL && temp->id != channel_id) {
//         prev = temp;
//         temp = temp->next;
//     }
 
//     if (temp == NULL)
//         return;
 
//     prev->next = temp->next;
 
//     free(temp);
// }

void device_is_mqtt_provisioned(bool* provisioned) {

    nvs_handle_t mqtt_prov_handle;
    uint16_t prov_state;
    *provisioned = false;
    
    nvs_open("storage", NVS_READONLY, &mqtt_prov_handle);
    nvs_get_u16(mqtt_prov_handle, "mqtt_prov", &prov_state);    
    if (prov_state == 0xABCD) *provisioned = true;
    nvs_close(mqtt_prov_handle);
}

void device_set_provisioned(void) {
    nvs_handle_t mqtt_prov_handle;
    nvs_open("storage", NVS_READWRITE, &mqtt_prov_handle);
    nvs_set_u16(mqtt_prov_handle, "mqtt_prov", 0xABCD);
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

    ESP_LOGI(TAG, "Device structure is created with:\n            name: %s\n            id: %s", g_device.name, g_device.id);
}

void device_add_bool_channel(const char* name, bool cmd) {
    
    device_channel_t* new_channel = (device_channel_t*) malloc(sizeof(device_channel_t));
    
    new_channel->name = malloc(sizeof(name) + 1);
    strcpy(new_channel->name, name);

    new_channel->cmd = cmd;
    new_channel->type = CHANNEL_TYPE_BOOL;

    new_channel->next = g_device.channels;
    g_device.channels = new_channel;

    print_device();
}

void device_add_nummber_channel(const char* name, bool cmd, const char* title,
                    const char* description, float min, float max, float multipleof) {
    
    device_channel_t* new_channel = (device_channel_t*) malloc(sizeof(device_channel_t));
    
    new_channel->name = malloc(sizeof(name) + 1);
    strcpy(new_channel->name, name);

    new_channel->cmd = cmd;
    new_channel->type = CHANNEL_TYPE_NUMBER;
    new_channel->min = min;
    new_channel->max = max;
    new_channel->multipleof = multipleof;

    new_channel->next = g_device.channels;
    g_device.channels = new_channel;

    print_device();
}

void device_add_string_channel(const char* name, bool cmd, const char* title,
                    const char* description) {

}

void device_add_multi_option_channel(const char* name, bool cmd, const char* title,
                    const char* description, uint8_t opt_count, char** opts) {

    device_channel_t* new_channel = (device_channel_t*) malloc(sizeof(device_channel_t));
    
    new_channel->name = malloc(sizeof(name) + 1);
    strcpy(new_channel->name, name);

    new_channel->cmd = cmd;
    new_channel->type = CHANNEL_TYPE_MULTI_OPTS;

    for(int i = 0; i < opt_count; i++) {
        printf("%s\n", opts[i]);
    }

    new_channel->next = g_device.channels;
    g_device.channels = new_channel;

    print_device();
}

void device_remove_channel(const char* name) {

}

// char* device_get_mqtt_provision_json_data(void) {

//     // cJSON* device = cJSON_CreateObject();
    
//     // /* Device name */
//     // cJSON_AddStringToObject(device, "device_name", g_device.name);
    
//     // /* Device ID - MAC address */
//     // cJSON_AddStringToObject(device, "device_id", g_device.id);

//     // /* Device Channels */
//     // cJSON* channels = cJSON_AddArrayToObject(device, "channels");
//     // device_channel_t* temp = g_device.channels;
//     // while (temp != NULL) {
//     //     cJSON* new_channel = cJSON_CreateObject();

//     //     cJSON_AddStringToObject(new_channel, "channel_name", temp->name);

//     //     cJSON_AddNumberToObject(new_channel, "channel_id", temp->id);

//     //     cJSON_AddNumberToObject(new_channel, "type", temp->type);

//     //     cJSON_AddItemToArray(channels, new_channel);

//     //     temp = temp->next;
//     // }

//     // char* output_buf = cJSON_PrintUnformatted(device);

//     // ESP_LOGI(TAG, "Device Provision JSON data:\n%s", cJSON_Print(device));
    
//     // cJSON_Delete(device);
//     // return output_buf;
// }