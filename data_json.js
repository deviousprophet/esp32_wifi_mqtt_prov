{
    "device_name": "AC",
    "device_id": "(MAC address)",

    "channels": {
        "relay01": {
            "type": "bool",
            "title": "",
            "description": ""
        },

        "temp": {
            "type": "number",
            "minimum": 20,
            "maximum": 30,
            "multipleOf": 1,
            "title": "Temperature",
            "description": "The AC Temperature",

            "command": true
        },

        "mode": {
            "type": "choice"
            "enum": ["mode1", "mode2"],
            "title": "Mode",
            "description": "The AC Operation Mode"
        },

        "something_else": {
            "type": "string"
        }
    }
}
  
// Server send downstream command
{
    "channels": {
        "temp": 28
    }
}
  
// Node update telemetry data
{
    "channels": {
       "temp": 28
    }
}
  
  // PROVISION
  // Gateway
  // Listen: sub to up/provision/+device_id
  // Response: pub to down/provision/<device_id>
  
  // Esp
  // Request: pub to up/provision/<device_id>
  // Wait: sub to down/provision/<device_id>
  
  
  // COMMAND
  // Gateway
  // Send: pub to down/command/<device_id>
  
  // Esp
  // Listen: sub to down/command/<device_id>
  
  
  // TELEMETRY
  // Gateway
  // Listen: sub to up/telemetry/+device_id
  
  // Esp
  // Send: pub to up/telemetry/<device_id>
  
  
  
  // SUMMARY for Gateway
  // Listen: sub to up/+action/+device_id
  // action canbe provision|telemetry