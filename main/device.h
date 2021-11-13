/* Channel data type */
typedef enum {
    CHANNEL_DATA_BOOL,
    CHANNEL_DATA_INT,
    CHANNEL_DATA_FLOAT,
    CHANNEL_DATA_STRING,
} channel_data_type_t;

typedef enum {
    MONITOR_ONLY,
    CONTROL_ONLY,
    MONITOR_AND_CONTROL
} channel_data_role_t;

typedef struct device_channel_t {
    struct device_channel_t* next;
    char* name;
    int id;
    channel_data_type_t type;
    channel_data_role_t role;

    union {
        bool bool_val;
        int int_val;
        float float_val;
        char* str_val;
    } data_value;

} device_channel_t;

typedef struct {
    char* name;
    char* id;
    device_channel_t* channels;
} device_t;

void device_is_mqtt_provisioned(bool* provisioned);

void device_set_provisioned(void);

void device_init(const char* device_name);

void device_add_channel(const char* channel_name, int channel_id, channel_data_type_t type, channel_data_role_t role);

void device_remove_channel(int channel_id);

char* device_get_mqtt_provision_json_data(void);