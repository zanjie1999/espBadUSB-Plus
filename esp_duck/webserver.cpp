#include "webserver.h"

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "config.h"
#include "debug.h"
#include "cli.h"
#include "spiffs.h"
#include "settings.h"
#include "com.h"

#include "webfiles.h"

void reply(AsyncWebServerRequest* request, int code, const char* type, const uint8_t* data, size_t len) {
    AsyncWebServerResponse* response =
        request->beginResponse_P(code, type, data, len);

    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

namespace webserver {
    // ===== PRIVATE ===== //
    AsyncWebServer   server(80);
    AsyncWebSocket   ws("/ws");
    AsyncEventSource events("/events");

    AsyncWebSocketClient* currentClient { nullptr };

    DNSServer dnsServer;

    bool reboot = false;

    void wsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
        if (type == WS_EVT_CONNECT) {
            debugf("WS Client connected %u\n", client->id());
        }

        else if (type == WS_EVT_DISCONNECT) {
            debugf("WS Client disconnected %u\n", client->id());
        }

        else if (type == WS_EVT_ERROR) {
            debugf("WS Client %u error(%u): %s\n", client->id(), *((uint16_t*)arg), (char*)data);
        }

        else if (type == WS_EVT_PONG) {
            debugf("PONG %u\n", client->id());
        }

        else if (type == WS_EVT_DATA) {
            AwsFrameInfo* info = (AwsFrameInfo*)arg;

            if (info->opcode == WS_TEXT) {
                char* msg = (char*)data;
                msg[len] = 0;

                debugf("Message from %u [%llu byte]=%s", client->id(), info->len, msg);

                currentClient = client;
                cli::parse(msg, [](const char* str) {
                    webserver::send(str);
                    debugf("%s\n", str);
                }, false);
                currentClient = nullptr;
            }
        }
    }

    String getContentType(String filename, AsyncWebServerRequest *request) {
        if (request->hasArg("download"))
            return "application/octet-stream";
        else if (filename.endsWith(".htm"))
            return "text/html";
        else if (filename.endsWith(".html"))
            return "text/html";
        else if (filename.endsWith(".css"))
            return "text/css";
        else if (filename.endsWith(".js"))
            return "application/javascript";
        else if (filename.endsWith(".png"))
            return "image/png";
        else if (filename.endsWith(".gif"))
            return "image/gif";
        else if (filename.endsWith(".jpg"))
            return "image/jpeg";
        else if (filename.endsWith(".ico"))
            return "image/x-icon";
        else if (filename.endsWith(".xml"))
            return "text/xml";
        else if (filename.endsWith(".pdf"))
            return "application/x-pdf";
        else if (filename.endsWith(".zip"))
            return "application/x-zip";
        else if (filename.endsWith(".gz"))
            return "application/x-gzip";
        return "text/plain";
    }

    bool handleFileRead(String path, AsyncWebServerRequest *request) {
        digitalWrite(2, LOW);
        if (path.endsWith("/")) {
            path += "index.html";
        }

        String contentType = getContentType(path, request);

        if (SPIFFS.exists(path + ".gz")) {
            path += ".gz";
            AsyncWebServerResponse *response = request->beginResponse(SPIFFS, path, contentType);
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);

            debugln("HTTP 200: " + path);
            digitalWrite(2, HIGH);
            return true;
        } else if (SPIFFS.exists(path)) {
            request->send(SPIFFS, path, contentType);

            debugln("HTTP 200: " + path);
            digitalWrite(2, HIGH);
            return true;
        }
        debugln("HTTP 404: " + path);
            digitalWrite(2, HIGH);
            return false;
    }

        void handleFileDelete(AsyncWebServerRequest *request) {
            if (request->args() == 0) {
                AsyncWebServerResponse *response = request->beginResponse_P(500, "text/plain", "BAD ARGS");
                request->send(response);
                return;
            }
            String path = request->arg(0u);
            debugln("handleFileDelete: " + path);
            if (path == "/") {
                AsyncWebServerResponse *response = request->beginResponse_P(500, "text/plain", "BAD PATH");
                request->send(response);
                return;
            }

            if (!SPIFFS.exists(path)) {
                AsyncWebServerResponse *response = request->beginResponse_P(404, "text/plain", "FileNotFound");
                request->send(response);
            }
            SPIFFS.remove(path);
            AsyncWebServerResponse *response = request->beginResponse_P(200, "text/plain", "");
            request->send(response);
            path = String();
        }

    void handleFileCreate(AsyncWebServerRequest *request) {
        if (request->args() == 0) {
            AsyncWebServerResponse *response = request->beginResponse_P(500, "text/plain", "BAD ARGS");
            request->send(response);
            return;
        }

        String path = request->arg(0u);
        debugln("handleFileCreate: " + path);
        if (path == "/") {
            AsyncWebServerResponse *response = request->beginResponse_P(500, "text/plain", "BAD PATH");
            request->send(response);
            return;
        }
        if (SPIFFS.exists(path)) {
            AsyncWebServerResponse *response = request->beginResponse_P(500, "text/plain", "FILE EXISTS");
            request->send(response);
            return;
        }
        File file = SPIFFS.open(path, "w");
        if (file)
            file.close();
        else {
            AsyncWebServerResponse *response = request->beginResponse_P(500, "text/plain", "BAD ARGS");
            request->send(response);
            return;
        }
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/plain", "");
        request->send(response);
        path = String();
        }

        void handleFileList(AsyncWebServerRequest *request) {
        if (!request->hasArg("dir")) {
            AsyncWebServerResponse *response = request->beginResponse_P(500, "text/plain", "BAD ARGS");
            request->send(response);
            return;
        }

        String path = request->arg("dir");
        debugln("handleFileList: " + path);
        Dir dir = SPIFFS.openDir(path);
        path = String();

        String output = "[";
        while (dir.next()) {
            File entry = dir.openFile("r");
            if (output != "[") output += ',';
            bool isDir = false;
            output += "{\"type\":\"";
            output += (isDir) ? "dir" : "file";
            output += "\",\"name\":\"";
            output += String(entry.name()).substring(1);
            output += "\"}";
            entry.close();
        }

        output += "]";
        request->send(200, "text/json", output);
    }


    void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        File f;

        if (!filename.startsWith("/")) filename = "/" + filename;

        if (!index) f = SPIFFS.open(filename, "w"); //create or trunicate file
        else f = SPIFFS.open(filename, "a"); //append to file (for chunked upload)

        f.write(data, len);

        if (final) { //upload finished
            debugf("UploadEnd: %s, %u B\n", filename.c_str(), index + len);
            f.close();
        }
    }

    // ===== PUBLIC ===== //
    void begin() {
        // Access Point
        WiFi.hostname(HOSTNAME);

        // WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(settings::getSSID(), settings::getPassword(), settings::getChannelNum());
        debugf("Started Access Point \"%s\":\"%s\"\n", settings::getSSID(), settings::getPassword());

        // Webserver
        server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
            request->redirect("/index.html");
        });

        server.onNotFound([](AsyncWebServerRequest* request) {
            if (!handleFileRead(request->url(), request)) {
                request->redirect("/error404.html");
            }
        });

        server.on("/run", [](AsyncWebServerRequest* request) {
            String message;

            if (request->hasParam("cmd")) {
                message = request->getParam("cmd")->value();
            }

            request->send(200, "text/plain", "Run: " + message);

            cli::parse(message.c_str(), [](const char* str) {
                debugf("%s\n", str);
            }, false);
        });

        server.on("/ip", HTTP_GET, [](AsyncWebServerRequest * request) {
            request->send(200, "text/json", String(WiFi.localIP()));
        });

        //list directory
        server.on("/list", HTTP_GET, handleFileList);
        //create file
        server.on("/edit", HTTP_PUT, handleFileCreate);
        //delete file
        server.on("/edit", HTTP_DELETE, handleFileDelete);
        //first callback is called after the request has ended with all parsed arguments
        //second callback handles file uploads at that location
        server.on("/edit", HTTP_POST, [](AsyncWebServerRequest * request) {
          AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", "");
          request->send(response);
        }, handleUpload);

        WEBSERVER_CALLBACK;

        // Arduino OTA Update
        ArduinoOTA.onStart([]() {
            events.send("Update Start", "ota");
        });
        ArduinoOTA.onEnd([]() {
            events.send("Update End", "ota");
        });
        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            char p[32];
            sprintf(p, "Progress: %u%%\n", (progress/(total/100)));
            events.send(p, "ota");
        });
        ArduinoOTA.onError([](ota_error_t error) {
            if (error == OTA_AUTH_ERROR) events.send("Auth Failed", "ota");
            else if (error == OTA_BEGIN_ERROR) events.send("Begin Failed", "ota");
            else if (error == OTA_CONNECT_ERROR) events.send("Connect Failed", "ota");
            else if (error == OTA_RECEIVE_ERROR) events.send("Recieve Failed", "ota");
            else if (error == OTA_END_ERROR) events.send("End Failed", "ota");
        });
        ArduinoOTA.setHostname(HOSTNAME);
        ArduinoOTA.begin();

        events.onConnect([](AsyncEventSourceClient* client) {
            client->send("hello!", NULL, millis(), 1000);
        });
        server.addHandler(&events);

        // Web OTA
        server.on("/update", HTTP_POST, [](AsyncWebServerRequest* request) {
            reboot = !Update.hasError();

            AsyncWebServerResponse* response;
            response = request->beginResponse(200, "text/plain", reboot ? "OK" : "FAIL");
            response->addHeader("Connection", "close");

            request->send(response);
        }, [](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
            if (!index) {
                debugf("Update Start: %s\n", filename.c_str());
                Update.runAsync(true);
                if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) {
                    Update.printError(Serial);
                }
            }
            if (!Update.hasError()) {
                if (Update.write(data, len) != len) {
                    Update.printError(Serial);
                }
            }
            if (final) {
                if (Update.end(true)) {
                    debugf("Update Success: %uB\n", index+len);
                } else {
                    Update.printError(Serial);
                }
            }
        });

        MDNS.addService("http", "tcp", 80);

        dnsServer.start(53, URL, IPAddress(192, 168, 4, 1));

        // Websocket
        ws.onEvent(wsEvent);
        server.addHandler(&ws);

        // Start Server
        server.begin();
        debugln("Started Webserver");
    }

    void update() {
        ArduinoOTA.handle();
        if (reboot) ESP.restart();
        dnsServer.processNextRequest();
    }

    void send(const char* str) {
        if (currentClient) currentClient->text(str);
    }
}