/*
This is the code for the AirGradient DIY Air Quality Sensor with an ESP8266 Microcontroller.

It is a high quality sensor showing PM2.5, CO2, Temperature and Humidity on a small display and can send data over Wifi.

For build instructions please visit https://www.airgradient.com/diy/

Compatible with the following sensors:
Plantower PMS5003 (Fine Particle Sensor)
SenseAir S8 (CO2 Sensor)
SHT30/31 (Temperature/Humidity Sensor)

Please install ESP8266 board manager (tested with version 3.0.0)

The codes needs the following libraries installed:
"WifiManager by tzapu, tablatronix" tested with Version 2.0.3-alpha
"ESP8266 and ESP32 OLED driver for SSD1306 displays by ThingPulse, Fabrice Weinberg" tested with Version 4.1.0

Configuration:
Please set in the code below which sensor you are using and if you want to connect it to WiFi.

If you are a school or university contact us for a free trial on the AirGradient platform.
https://www.airgradient.com/schools/

MIT License
*/

#include <AirGradient.h>
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "SSD1306Wire.h"

/*
 * In "secrets.h"
 *   - ssid
 *   - password
 *   - mqttBroker
 */
#include "secrets.h"

AirGradient ag = AirGradient();

SSD1306Wire display(0x3c, SDA, SCL);

// set sensors that you do not use to false
boolean hasPM = true;
boolean hasCO2 = true;
boolean hasSHT = false;

// set to true if you want to connect to wifi. The display will show values only when the sensor has wifi connection
boolean connectWIFI = true;

// 'C' for celcius; 'F' for farenheiht
char temp_display = 'F';

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long currentMillis = 0;

const int sixtySeconds = 60000;

const int sendToServerInterval = sixtySeconds;
unsigned long previoussendToServer = 0;

const int tempHumInterval = sixtySeconds;
unsigned long previousTempHum = 0;
int temp = 0;
int hum = 0;

const int co2Interval = sixtySeconds;
unsigned long previousCo2 = 0;
int Co2 = 0;

const int pm25Interval = sixtySeconds;
unsigned long previousPm25 = 0;
int pm25 = 0;

int counter = 0;
const int screenInterval = 5000;
unsigned long previousScreen  = 5000;

// DISPLAY
void showTextRectangle(String ln1, String ln2, boolean small) {
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    if (small) {
        display.setFont(ArialMT_Plain_16);
    } else {
        display.setFont(ArialMT_Plain_24);
    }
    display.drawString(32, 16, ln1);
    display.drawString(32, 36, ln2);
    display.display();
}

// Calculate PM2.5 US AQI
int PM_TO_AQI_US(int pm02) {
    if (pm02 <= 12.0) return ((50 - 0) / (12.0 - .0) * (pm02 - .0) + 0);
    else if (pm02 <= 35.4) return ((100 - 50) / (35.4 - 12.0) * (pm02 - 12.0) + 50);
    else if (pm02 <= 55.4) return ((150 - 100) / (55.4 - 35.4) * (pm02 - 35.4) + 100);
    else if (pm02 <= 150.4) return ((200 - 150) / (150.4 - 55.4) * (pm02 - 55.4) + 150);
    else if (pm02 <= 250.4) return ((300 - 200) / (250.4 - 150.4) * (pm02 - 150.4) + 200);
    else if (pm02 <= 350.4) return ((400 - 300) / (350.4 - 250.4) * (pm02 - 250.4) + 300);
    else if (pm02 <= 500.4) return ((500 - 400) / (500.4 - 350.4) * (pm02 - 350.4) + 400);
    else return 500;
};


void updateCo2() {
    if (currentMillis - previousCo2 >= co2Interval) {
        previousCo2 += co2Interval;
        Co2 = ag.getCO2_Raw();
        Serial.println(String(Co2));
    }
}

void updatePm25() {
    if (currentMillis - previousPm25 >= pm25Interval) {
        previousPm25 += pm25Interval;
        pm25 = ag.getPM2_Raw();
        Serial.println(String(pm25));
    }
}

void updateTempHum() {
    if (currentMillis - previousTempHum >= tempHumInterval) {
        previousTempHum += tempHumInterval;
        TMP_RH result = ag.periodicFetchData();
        temp = result.t;
        hum = result.rh;
        Serial.println(String(temp));
    }
}

// Wifi Manager
void connectToWifi() {
    WiFiManager wifiManager;
    // WiFi.disconnect(); //to delete previous saved hotspot
    String HOTSPOT = "AG-" + String(ESP.getChipId(), HEX);
    showTextRectangle("Connect", HOTSPOT, true);
    delay(2000);
    wifiManager.setTimeout(240);
    if (!wifiManager.autoConnect((const char*)HOTSPOT.c_str())) {
        showTextRectangle("Offline", "Mode", true);
        delay(5000);
    }
}

void connectToMQTT() {
    client.setServer(mqttBroker, 1883);
    client.setBufferSize(255);
    Serial.print("Attempting MQTT connection...");

    char mqttClientId[20];
    sprintf(mqttClientId, "esp8266-%x", ESP.getChipId());

    // Loop until we're reconnected
    while (!client.connected()) {
        if (client.connect(mqttClientId, mqttUsername, mqttPassword)) {
            Serial.println("connected.");

            // Once connected, publish an announcement
            String topic = "sensors/airgradient/" + String(ESP.getChipId(), HEX) + "/meta";
            client.publish(topic.c_str(), "We're connected");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

void publishMQTT() {
    if (currentMillis - previoussendToServer >= sendToServerInterval) {
        previoussendToServer += sendToServerInterval;

        String payload = "{\"wifi\": " + String(WiFi.RSSI());

        if (hasPM) {
            payload += (pm25 < 0 ? "" : ", \"pm25\": " + String(pm25))
                    + (pm25 < 0 ? "" : ", \"aqi\": " + String(PM_TO_AQI_US(pm25)));
        }

        if (hasCO2) {
            payload += (Co2 < 0 ? "" : ", \"co2\": " + String(Co2));
        }

        if (hasSHT) {
            payload += ", \"temp\": " + String(temp)
                + (hum < 0 ? "" : ", \"hum\": " + String(hum));
        }

        payload += "}";

        if (WiFi.status()== WL_CONNECTED) {
            if (! client.connected()) {
                connectToMQTT();
            }
            Serial.print("ClientState: ");
            Serial.println(client.state());
            Serial.println("------------");
            Serial.print("Payload: ");
            Serial.println(payload);
            String topic = "sensors/airgradient/" + String(ESP.getChipId(), HEX) + "/measures";
            Serial.print("Topic:");
            Serial.println(topic);
            client.publish(topic.c_str(), payload.c_str());
            Serial.println("Published");
        } else {
            Serial.println("WiFi Disconnected");
        }
    }
}

void updateScreen() {
    int maxCounter = 0;
    if (hasPM) maxCounter++;
    if (hasCO2) maxCounter++;
    if (hasSHT) maxCounter += 2;
    if (currentMillis - previousScreen >= screenInterval) {
        previousScreen += screenInterval;
        switch (counter) {
            case 0:
                showTextRectangle("PM2", String(pm25),false);
                break;
            case 1:
                showTextRectangle("AQI", String(PM_TO_AQI_US(pm25)),false);
                break;
            case 2:
                showTextRectangle("CO2", String(Co2), false);
                break;
            case 3:
                if (temp_display == 'F' || temp_display == 'f') {
                    showTextRectangle("TMP", String((temp * 9 / 5) + 32, 1) + "F", false);
                } else {
                    showTextRectangle("TMP", String(temp, 1) + "C", false);
                }
                break;
            case 4:
                showTextRectangle("HUM", String(hum) + "%", false);
                break;
        }
        counter++;
        if (counter > maxCounter) counter = 0;
    }
}

void setup(){
    Serial.begin(9600);

    display.init();
    display.flipScreenVertically();
    showTextRectangle("Init", String(ESP.getChipId(),HEX), true);

    if (hasPM) ag.PMS_Init();
    if (hasCO2) ag.CO2_Init();
    if (hasSHT) ag.TMP_RH_Init(0x44);
    if (connectWIFI) {
        connectToWifi();
        connectToMQTT();
    }
    delay(2000);
}

void loop(){
    currentMillis = millis();

    if (hasPM) {
        updatePm25();
    }

    if (hasCO2) {
        updateCo2();
    }

    if (hasSHT) {
        updateTempHum();
    }

    if (connectWIFI) {
        publishMQTT();
    }

    updateScreen();
}
