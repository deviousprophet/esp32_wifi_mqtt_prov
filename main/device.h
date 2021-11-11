
/* Channel data type */
typedef enum {
    CHANNEL_DATA_BOOL,
    CHANNEL_DATA_INT,
    CHANNEL_DATA_FLOAT,
    CHANNEL_DATA_STRING,
} channel_data_type_t;

/* Deivce channel structure */
typedef struct device_channel_t {
    
    struct device_channel_t* next;
    
    char* name;
    
    uint8_t id;

    channel_data_type_t data_type;

    union {
        bool bool_val;
        int int_val;
        float float_val;
        char str_val[20];
    } data_value;

    char* unit;
} device_channel_t;

/* Device structure */
typedef struct {
    
    char* device_name;
    
    char* device_id;
    
    device_channel_t* channels;

} device_t;

void device_init(const char* name);

void device_add_channel(const char* name, channel_data_type_t type);

void device_get_mqtt_provision_data(char* output_buf);