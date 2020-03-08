/*
   Copyright (c) 2019 Stefan Kremser
   This software is licensed under the MIT License. See the license file for details.
   Source: github.com/spacehuhn/WiFiDuck
 */

#include "config.h"
#include "debug.h"
#include <ESP8266WiFiMulti.h>

#include "com.h"
#include "duckscript.h"
#include "webserver.h"
#include "spiffs.h"
#include "settings.h"
#include "cli.h"

ESP8266WiFiMulti WiFiMulti;

void setup() {
    pinMode(2, OUTPUT);
    digitalWrite(2, LOW);

    debug_init();

    WiFi.mode(WIFI_AP_STA);
    WiFi.hostname(HOSTNAME);
    //  Connect AP here
    // WiFiMulti.addAP("name", "password");
    WiFiMulti.addAP("Derpy", "Sparkle:Yay");
    WiFiMulti.addAP("weslie", "zaj&1999");
    WiFiMulti.addAP("GZ_EBI", "GZepro1234");
    WiFiMulti.addAP("Sparkle", "password");
    int tryNum = 0;
    while (WiFiMulti.run() != WL_CONNECTED) {
        if (tryNum > 10) {
            break;
        }
        delay(500);
        tryNum++;
    }

    com::begin();

    spiffs::begin();
    settings::begin();
    cli::begin();
    webserver::begin();

    com::onDone(duckscript::nextLine);
    com::onError(duckscript::stopAll);
    com::onRepeat(duckscript::repeat);

    if (spiffs::freeBytes() > 0) com::send(MSG_STARTED);

    delay(10);
    com::update();

    duckscript::run(settings::getAutorun());
    
    digitalWrite(2, HIGH);
}

void loop() {
    com::update();
    webserver::update();

    debug_update();
}
