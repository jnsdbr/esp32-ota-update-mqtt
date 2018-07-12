# Start ESP32 OTA update via MQTT

This source enables an ESP OTA update to be triggered via MQTT.
You simply supply the hostname/binfile and the ESP parses the URL. Afterwards the update is started automatically.
Currently this is only working on port 80!

Example MQTT message to trigger the update (**Don't** add any protocol to the hostname e.g. http://hostname/update.bin):

```
mosquitto_pub -h localhost -t "/update/url/" -m "hostname/update.bin"
```



