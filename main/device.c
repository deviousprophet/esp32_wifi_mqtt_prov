#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <cJSON.h>

#include <esp_wifi.h>
#include <esp_log.h>
#include <nvs.h>

#include "device.h"

static device_t g_device;

static const char* TAG = "device";

void print_device_channels(void) {
    device_channel_t* temp = g_device.channels;
    ESP_LOGI(TAG, "Device channels:");

    while (temp != NULL) {
        printf("%10s:\n", temp->name);
        printf("        type: %d\n", temp->type);
        printf("         cmd: %d\n", temp->cmd);
        
        switch (temp->type) {
        case CHANNEL_TYPE_BOOL:
            printf("       value: %s\n", (temp->data_value.bool_val) ? "true" : "false");
            break;

        case CHANNEL_TYPE_NUMBER:
            printf("         min: %.2f\n", temp->prov_data.num_prov.min);
            printf("         max: %.2f\n", temp->prov_data.num_prov.max);
            printf("     multiof: %.2f\n", temp->prov_data.num_prov.multipleof);
            printf("       value: %.2f\n", temp->data_value.num_val);
            break;

        case CHANNEL_TYPE_CHOICE: {
            prov_opt_list_t* temp_opt = temp->prov_data.opts_prov;
            printf("        enum:\n");
            while(temp_opt != NULL) {
                printf("              %s\n", temp_opt->opt);
                temp_opt = temp_opt->next;
            }
            printf("       value: %s\n", (temp->data_value.str_val != NULL) ? temp->data_value.str_val : "null");
            break;
        }

        case CHANNEL_TYPE_STRING:
            printf("       value: %s\n", (temp->data_value.str_val != NULL) ? temp->data_value.str_val : "null");
            break;

        default:
            break;
        }

        printf("\n");
        temp = temp->next;
    }
}

const char* get_device_id(void) {
    char* id = malloc(13);
    uint8_t eth_mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    sprintf(
        id,
        "%02X%02X%02X%02X%02X%02X",
        eth_mac[0], eth_mac[1], eth_mac[2], eth_mac[3], eth_mac[4], eth_mac[5]
    );
    return id;
}

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

bool device_check_prov_resp(char* resp) {
    bool prov_status = false;
    
    cJSON* prov_resp = cJSON_Parse(resp);
    cJSON* status = cJSON_GetObjectItem(prov_resp, "status");
    if (cJSON_IsNumber(status)) {
        ESP_LOGI(TAG, "MQTT provisioning response status: %d", (int) status->valuedouble);
        if (status->valuedouble == 1)
            prov_status = true;
    }

    cJSON_Delete(prov_resp);
    return prov_status;
}

void device_init(const char* device_name) {

    /* Device name */
    g_device.name = malloc(strlen(device_name) + 1);
    strcpy(g_device.name, device_name);

    /* Device ID - MAC address */
    g_device.id = malloc(strlen(get_device_id()) + 1);
    strcpy(g_device.id, get_device_id());

    if(g_device.channels) {
        free(g_device.channels);
        g_device.channels = NULL;
    }

    ESP_LOGI(TAG, "Device structure is created with:\n            name: %s\n            id: %s", g_device.name, g_device.id);
}

void device_add_bool_channel(const char* name, bool cmd, const char* title,
                    const char* description) {
    
    device_channel_t* new_channel = (device_channel_t*) malloc(sizeof(device_channel_t));
    
    new_channel->name = malloc(strlen(name) + 1);
    strcpy(new_channel->name, name);

    new_channel->cmd = cmd;
    new_channel->type = CHANNEL_TYPE_BOOL;
    memset(&new_channel->data_value, 0, sizeof(new_channel->data_value));

    new_channel->next = g_device.channels;
    g_device.channels = new_channel;
}

void device_add_nummber_channel(const char* name, bool cmd, const char* title,
                    const char* description, float min, float max, float multipleof) {
    
    device_channel_t* new_channel = (device_channel_t*) malloc(sizeof(device_channel_t));
    
    new_channel->name = malloc(strlen(name) + 1);
    strcpy(new_channel->name, name);

    new_channel->cmd = cmd;
    new_channel->type = CHANNEL_TYPE_NUMBER;
    memset(&new_channel->data_value, 0, sizeof(new_channel->data_value));

    new_channel->prov_data.num_prov.min = min;
    new_channel->prov_data.num_prov.max = max;
    new_channel->prov_data.num_prov.multipleof = multipleof;

    new_channel->next = g_device.channels;
    g_device.channels = new_channel;
}

void device_add_multi_option_channel(const char* name, bool cmd, const char* title,
                    const char* description, uint8_t opt_count, ...) {

    device_channel_t* new_channel = (device_channel_t*) malloc(sizeof(device_channel_t));
    
    new_channel->name = malloc(strlen(name) + 1);
    strcpy(new_channel->name, name);

    new_channel->cmd = cmd;
    new_channel->type = CHANNEL_TYPE_CHOICE;
    memset(&new_channel->data_value, 0, sizeof(new_channel->data_value));

    if (opt_count > 0) {
        if (new_channel->prov_data.opts_prov)
            new_channel->prov_data.opts_prov = NULL;

        va_list opts_list;
        va_start(opts_list, opt_count);
        for (int i = 0; i < opt_count; i++) {
            char* temp = va_arg(opts_list, char*);
            
            prov_opt_list_t* new_opt = (prov_opt_list_t*) malloc(sizeof(prov_opt_list_t));
            new_opt->opt = malloc(strlen(temp) + 1);
            strcpy(new_opt->opt, temp);

            new_opt->next = new_channel->prov_data.opts_prov;
            new_channel->prov_data.opts_prov = new_opt;
        }
        va_end(opts_list);
    }

    new_channel->next = g_device.channels;
    g_device.channels = new_channel;
}

void device_add_string_channel(const char* name, bool cmd, const char* title,
                    const char* description) {

    device_channel_t* new_channel = (device_channel_t*) malloc(sizeof(device_channel_t));
    
    new_channel->name = malloc(strlen(name) + 1);
    strcpy(new_channel->name, name);

    new_channel->cmd = cmd;
    new_channel->type = CHANNEL_TYPE_STRING;
    memset(&new_channel->data_value, 0, sizeof(new_channel->data_value));

    new_channel->next = g_device.channels;
    g_device.channels = new_channel;
}

void device_remove_channel(const char* name) {

    device_channel_t* temp = g_device.channels;
    device_channel_t* prev = NULL;
    
    if (temp != NULL && strcmp(temp->name, name) == 0) {
        g_device.channels = temp->next;
        free(temp->name);
        free(temp->data_value.str_val);

        if (temp->type == CHANNEL_TYPE_CHOICE) {
            while (temp->prov_data.opts_prov != NULL) {
                free(temp->prov_data.opts_prov->opt);
                temp->prov_data.opts_prov = temp->prov_data.opts_prov->next;
            }
        }

        free(temp);
        return;
    }
    
    while (temp != NULL && strcmp(temp->name, name) != 0) {
        prev = temp;
        temp = temp->next;
    }
 
    if (temp == NULL)
        return;
 
    prev->next = temp->next;
 
    free(temp);
}

void device_set_channel_value(const char* name, void* value) {

    device_channel_t* temp = g_device.channels;

    while (temp != NULL) {
        if (strcmp(temp->name, name) == 0) {
            switch (temp->type) {
            case CHANNEL_TYPE_BOOL:
                temp->data_value.bool_val = *((bool*) value);
                break;
            
            case CHANNEL_TYPE_NUMBER:
                temp->data_value.num_val = *((float*) value);
                break;

            case CHANNEL_TYPE_CHOICE:
            case CHANNEL_TYPE_STRING: {
                char* temp_str = *((char**) value);

                temp->data_value.str_val = realloc(temp->data_value.str_val, strlen(temp_str) + 1);

                strcpy(temp->data_value.str_val, temp_str);
                break;
            }
            
            default:
                break;
            }

            break;
        }

        temp = temp->next;
    }

    print_device_channels();
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

        cJSON* channel_temp = cJSON_AddObjectToObject(new_channel, temp->name);

        cJSON_AddBoolToObject(channel_temp, "command", temp->cmd);

        switch (temp->type) {
        case CHANNEL_TYPE_BOOL:
            cJSON_AddStringToObject(channel_temp, "type", "boolean");
            break;
        
        case CHANNEL_TYPE_NUMBER:
            cJSON_AddStringToObject(channel_temp, "type", "number");
            cJSON_AddNumberToObject(channel_temp, "min", temp->prov_data.num_prov.min);
            cJSON_AddNumberToObject(channel_temp, "max", temp->prov_data.num_prov.max);
            cJSON_AddNumberToObject(channel_temp, "multipleof", temp->prov_data.num_prov.multipleof);
            break;

        case CHANNEL_TYPE_CHOICE: {
            cJSON* temp_options = cJSON_AddArrayToObject(channel_temp, "enum");
            prov_opt_list_t* temp_opt = temp->prov_data.opts_prov;

            while (temp_opt != NULL) {
                cJSON_AddItemToArray(temp_options, cJSON_CreateString(temp_opt->opt));
                temp_opt = temp_opt->next;
            }
            break;
        }

        case CHANNEL_TYPE_STRING:
            cJSON_AddStringToObject(channel_temp, "type", "string");
            break;
        default:
            break;
        }

        cJSON_AddItemToArray(channels, new_channel);

        temp = temp->next;
    }

    char* output_buf = cJSON_PrintUnformatted(device);

    ESP_LOGI(TAG, "Device Provision JSON data:\n%s", cJSON_Print(device));
    
    cJSON_Delete(device);
    return output_buf;
}