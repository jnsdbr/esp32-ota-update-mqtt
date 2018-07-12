#include "Arduino.h"
#include <WiFi.h>
#include <Update.h>
#include <PubSubClient.h>
#include <Preferences.h>

Preferences preferences;

const char* WIFI_SSID = "";
const char* WIFI_SECRET = "";
const char* MQTT_SERVER = "";

WiFiClient client;
PubSubClient mqtt(client);

int contentLength = 0;
bool isValidContentType = false;

String getHeaderValue(String header, String headerName) {
    return header.substring(strlen(headerName.c_str()));
}

String getBinName(String url) {
    int index = 0;

    // Search for last /
    for (int i = 0; i < url.length(); i++) {
        if (url[i] == '/') {
            index = i;
        }
    }

    String binName = "";

    // Create binName
    for (int i = index; i < url.length(); i++) {
        binName += url[i];
    }

    return binName;
}

String getHostName(String url) {
     int index = 0;

    // Search for last /
    for (int i = 0; i < url.length(); i++) {
        if (url[i] == '/') {
            index = i;
        }
    }

    String hostName = "";

    // Create binName
    for (int i = 0; i < index; i++) {
        hostName += url[i];
    }

    return hostName;
}

void update(String url, int port) {
    String bin = getBinName(url);
    String host = getHostName(url);

    Serial.println("Connecting to: " + host);
    if (client.connect(host.c_str(), port)) {
        // Connection Succeed.
        // Fecthing the bin
        Serial.println("Fetching Bin: " + bin);

        // Get the contents of the bin file
        client.print(String("GET ") + bin + " HTTP/1.1\r\n" +
                        "Host: " + host + "\r\n" +
                        "Cache-Control: no-cache\r\n" +
                        "Connection: close\r\n\r\n");

        unsigned long timeout = millis();

        while (client.available() == 0) {
            if (millis() - timeout > 5000) {
                Serial.println("Client Timeout !");
                client.stop();
                return;
            }
        }
        while (client.available()) {
            // read line till /n
            String line = client.readStringUntil('\n');
            // remove space, to check if the line is end of headers
            line.trim();

            // if the the line is empty,
            // this is end of headers
            // break the while and feed the
            // remaining `client` to the
            // Update.writeStream();
            if (!line.length()) {
                //headers ended
                break; // and get the OTA started
            }

            // Check if the HTTP Response is 200
            // else break and Exit Update
            if (line.startsWith("HTTP/1.1")) {
                if (line.indexOf("200") < 0) {
                    Serial.println("Got a non 200 status code from server. Exiting OTA Update.");
                    break;
                }
            }

            // extract headers here
            // Start with content length
            if (line.startsWith("Content-Length: ")) {
                contentLength = atoi((getHeaderValue(line, "Content-Length: ")).c_str());
                Serial.println("Got " + String(contentLength) + " bytes from server");
            }

            // Next, the content type
            if (line.startsWith("Content-Type: ")) {
                String contentType = getHeaderValue(line, "Content-Type: ");
                Serial.println("Got " + contentType + " payload.");
                if (contentType == "application/octet-stream") {
                    isValidContentType = true;
                }
            }
        }
    }
    else {
        // Connect to S3 failed
        // May be try?
        // Probably a choppy network?
        Serial.println("Connection to " + host + " failed. Please check your setup");
        // retry??
    }

    // Check what is the contentLength and if content type is `application/octet-stream`
    Serial.println("contentLength : " + String(contentLength) + ", isValidContentType : " + String(isValidContentType));

    // check contentLength and content type
    if (contentLength && isValidContentType) {
        // Check if there is enough to OTA Update
        bool canBegin = Update.begin(contentLength);
        if (canBegin) {
            Serial.println("Begin OTA. This may take 2 - 5 mins to complete. Things might be quite for a while.. Patience!");
            size_t written = Update.writeStream(client);

            if (written == contentLength) {
                Serial.println("Written : " + String(written) + " successfully");
            }
            else {
                Serial.println("Written only : " + String(written) + "/" + String(contentLength) + ". Retry?" );
                // retry??
            }

            if (Update.end()) {
                Serial.println("OTA done!");
                if (Update.isFinished()) {
                    Serial.println("Update successfully completed. Rebooting.");
                    ESP.restart();
                }
                else {
                    Serial.println("Update not finished? Something went wrong!");
                }
            }
            else {
                Serial.println("Error Occurred. Error #: " + String(Update.getError()));
            }
        }
        else {
            // not enough space to begin OTA
            // Understand the partitions and
            // space availability
            Serial.println("Not enough space to begin OTA");
            client.flush();
        }
    }
    else {
        Serial.println("There was no content in the response");
        client.flush();
    }
}

void callback(char* topic, byte* payload, unsigned int length) {
    payload[length] = '\0';
    String _message = String((char*)payload);
    String _topic = String(topic);

    if (_topic.equals("/update/url/") == 1) {
        update(_message, 80);
    }
}

void connectToWiFi() {
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_SECRET);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void reconnect() {
    while (!mqtt.connected()) {
        Serial.print("Attempting MQTT connection...");
        if (mqtt.connect("ESP32Client")) {
            Serial.println("connected");
            mqtt.subscribe("/update/url/");
        }
        else {
            Serial.print("failed, rc=");
            Serial.print(mqtt.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

void setup() {
    // Load preferences
    preferences.begin("settings", false);
    String ssid = preferences.getString("WIFI_SSID", "");
    String secret = preferences.getString("WIFI_SECRET", "");
    String mqttServer = preferences.getString("MQTT_SERVER", "");
    preferences.end();

    if (!ssid.equals("")) {
        WIFI_SSID = ssid.c_str();
    }
    if (!secret.equals("")) {
        WIFI_SECRET = secret.c_str();
    }
    if (!mqttServer.equals("")) {
        MQTT_SERVER = mqttServer.c_str();
    }

    Serial.begin(9600);
    connectToWiFi();
    mqtt.setServer(MQTT_SERVER, 1883);
    mqtt.setCallback(callback);
}

void loop() {
    if (!mqtt.connected()) {
        reconnect();
    }
    mqtt.loop();
}