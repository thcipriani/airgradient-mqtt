# AirGradient MQTT

A tweaked version of the [AirGradient
DIY](https://www.airgradient.com/open-airgradient/instructions/diy/) firmware that supports
sending values to MQTT.

This project is based on the [Arduino
examples](https://www.airgradient.com/open-airgradient/instructions/basic-setup-skills-and-equipment-needed-to-build-our-airgradient-diy-sensor/)
AirGradient provides along with a few tweaks.

## Setup

For MQTT to publish, it requires a host, username, and password. 

Add those credentials to `include/secrets.h`:

```
const char* mqttBroker = mqtt.example.com
const char* mqttUsername = fooUser
const char* mqttPassword = barPass
```

Wifi is set up using the awesome
[tzapu/WiFiManager](https://github.com/tzapu/WiFiManager) library.

## Flashing

I used [Platform.io](https://docs.platformio.org) with vscode to build this.

It's a pleasant alternative to the Ardunio IDE.
