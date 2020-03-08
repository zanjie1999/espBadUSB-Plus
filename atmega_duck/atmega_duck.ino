#include "config.h"
#include "debug.h"

#include "keyboard.h"
#include "led.h"
#include "com.h"
#include "duckparser.h"
#include "serial_bridge.h"

#define ExternSerial Serial1

// ===== SETUP ====== //
void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    
    debug_init();

    serial_bridge::begin();
    keyboard::begin();
    led::begin();
    com::begin();

    // usb虚拟串口直通esp8266
    Serial.begin(115200);
    ExternSerial.begin(115200);

    debugs("Started! ");
    debugln(VERSION);

    digitalWrite(LED_BUILTIN, LOW);
}

// ===== LOOP ===== //
void loop() {
    com::update();
    if (com::hasData()) {
        const buffer_t& buffer = com::getBuffer();

        debugs("Interpreting: ");

        for (size_t i = 0; i<buffer.len; i++) debug(buffer.data[i]);

        duckparser::parse(buffer.data, buffer.len);

        com::sendDone();
    }

    // usb虚拟串口直通esp8266
    if (ExternSerial.available()) {
        digitalWrite(LED_BUILTIN, HIGH);
        Serial.write((uint8_t)ExternSerial.read());
        digitalWrite(LED_BUILTIN, LOW);
    }

    if (Serial.available()) {
        digitalWrite(LED_BUILTIN, HIGH);
        ExternSerial.write((uint8_t)Serial.read());
        digitalWrite(LED_BUILTIN, LOW);
    }
}
