/* Channel data type */
typedef enum {
    CHANNEL_TYPE_BOOL,
    CHANNEL_TYPE_NUMBER,
    CHANNEL_TYPE_STRING,
    CHANNEL_TYPE_MULTI_OPTS,
} channel_type_t;

typedef struct device_channel_t {
    struct device_channel_t* next;
    char* name;
    bool cmd;
    channel_type_t type;

    float min;
    float max;
    float multipleof;

    union {
        bool bool_val;
        float num_val;
        char* str_val;
    } data_value;

} device_channel_t;

typedef struct {
    char* name;
    char* id;
    device_channel_t* channels;
} device_t;



/* Check for MQTT provision status */
void device_is_mqtt_provisioned(bool* provisioned);


/* Set MQTT provision status */
void device_set_provisioned(void);


/* Create device structure */
void device_init(const char* device_name);


/* Add channel to device */
void device_add_bool_channel(const char* name, bool cmd);

void device_add_nummber_channel(const char* name, bool cmd, const char* title,
                    const char* description, float min, float max, float multipleof);

void device_add_string_channel(const char* name, bool cmd, const char* title,
                    const char* description);

void device_add_multi_option_channel(const char* name, bool cmd, const char* title,
                    const char* description, uint8_t opt_count, char** opts);


/* Remove channel from device */
void device_remove_channel(const char* name);


/* Get the JSON provisioning data */
// char* device_get_mqtt_provision_json_data(void);