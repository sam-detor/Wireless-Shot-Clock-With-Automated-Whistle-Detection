# ESP-32 Shot Clock Code 

This code for the ESP32 based wireless shot clock. The clock display is a 32 x 32 LED matrix controlled via RMT on data pin 12. The shot clock communicates with the mobile app and automated whistle detection app via an MQTT server.

This code was based on MQTT and RMT Espressif examples which can be found here:
* https://github.com/espressif/esp-idf/tree/master/examples/protocols/mqtt/ssl
* https://github.com/espressif/esp-idf/tree/master/examples/peripherals/rmt/led_strip

