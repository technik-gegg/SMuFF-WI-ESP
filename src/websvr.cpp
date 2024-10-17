/**
 * SMuFF WI-ESP Firmware
 * Copyright (C) 2024 Technik Gegg
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "Config.h"
#if defined(ESP32)
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <HTTPUpdateServer.h>
#include <HTTPClient.h>
#include <NetBIOS.h>
#else
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266NetBIOS.h>
#endif
#include <WebSocketsServer.h>

WiFiManager             wifiMgr((Stream&)debugOut);
#if defined(ESP32)
WebServer               webServer(80);
HTTPUpdateServer        httpUpdateServer;
#else
ESP8266WebServer        webServer(80);
ESP8266HTTPUpdateServer httpUpdateServer(true);
#endif
WebSocketsServer        webSocketServer = WebSocketsServer(8080);
char                    deviceName[50];
int                     wsClientsConnected = 0;
int                     curClient = -1;

String                  updaterError;
uint32_t                uploadSize = 0;
uint8_t                 lastPercent;

#define WS_CHUNK_SIZE   256

static const char updateBinaryPage[] PROGMEM = {
     R"(<!DOCTYPE html>
     <html lang='en'>
     <head>
         <meta charset='utf-8'>
         <meta name='viewport' content='width=device-width,initial-scale=1'/>
         <meta http-equiv='refresh' content='10;URL=/'>
     </head>
     <body>
        <h1>{0}</h1>
        {1}
        <p>Please wait a few seconds for the device to reboot, then reload the page using <a href='/index.html'>this link</a>.</p>
     </body>
     </html>)"
};

const char* wsCliPrefix PROGMEM = "WS client #";


void sendResponse(int status, const char* mime, String value) {
    webServer.sendHeader("Access-Control-Allow-Origin", "*");
    webServer.sendHeader("Access-Control-Allow-Headers", "*");
    webServer.sendHeader("X-Content-Type-Options", "no-sniff");
    webServer.send(status, mime, value);
}

void sendOkResponse() {
    sendResponse(200, MIME_TEXT, String("ok"));
}

void handle404() {
    String message = "<h1>File Not Found</h1>";
    message += "<p>URI: " + webServer.uri() + "</p>";
    message += "<p>Method: " + String((webServer.method() == HTTP_GET) ? "GET" : "POST") + "</p>";
    if(webServer.args() > 0) {
        message += "<p>Arguments:</p><ul>";
        for (uint8_t i = 0; i < webServer.args(); i++) {
            message += "<li>" + webServer.argName(i) + ": " + webServer.arg(i) + "</li>";
        }
        message += "</ul>";
    }
    if(!webServer.uri().startsWith("/assets/img/")) {  // don't print any debug message for WI 404
        sendResponse(404, MIME_HTML, message);
        __debugS(PSTR("404: '%s' not found"), webServer.uri().c_str());
    }
}

void setUpdaterError() {
#if !defined(ESP32)
    updaterError = Update.getErrorString();
    __debugS(PSTR("Update error: [%d] %s\n"), Update.getError(), Update.getErrorString().c_str());
#else
    updaterError = String(Update.getError());
    __debugS(PSTR("Update error: [%d]\n"), Update.getError());
#endif
}

void handleFileUpload() {
    HTTPUpload& upload = webServer.upload();

    switch(upload.status) {

        case UPLOAD_FILE_START:
            updaterError.clear();
            #if !defined(ESP32)
            uploadSize = upload.contentLength;
            __debugS(PSTR("Upload started with file: \"%s\" (%zu B)"), upload.filename.c_str(), uploadSize);
            #endif
            if(neoPixels != nullptr)
                neoPixels->clear();
            if (upload.filename == "littlefs.bin") {
                #if !defined(ESP32)
                close_all_fs();
                size_t fsSize = ((size_t)FS_end - (size_t)FS_start);
                __debugS(PSTR("Updating file system... Filesystem size: %zu B"), fsSize);
                if (!Update.begin(fsSize, U_FS, INTLED_PIN, HIGH)){               //start with max available size
                    setUpdaterError();
                }
                #else
                uploadSize = webServer.clientContentLength();
                __debugS(PSTR("Updating file system... Image size: %zu"), uploadSize);
                if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)){               //start with max available size
                    setUpdaterError();
                }
                #endif
            } 
            else if (upload.filename == "firmware.bin") {
                uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
                __debugS(PSTR("Updating firmware... Max. sketch space: %zu B"), maxSketchSpace);
                if (!Update.begin(maxSketchSpace, U_FLASH, INTLED_PIN, HIGH)) {   //start with max available size
                    setUpdaterError();
                }
            }
            break;

        case UPLOAD_FILE_WRITE:
            if(Update.hasError()) {
                Update.end();
                #if !defined(ESP32)
                __debugS(PSTR("Update was aborted due to errors. %s"), Update.getErrorString().c_str());
                #else
                __debugS(PSTR("Update was aborted due to errors. Error=%d"), Update.getError());
                #endif
                break;
            }
            if(Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                setUpdaterError();
                Update.end();
            }
            else {
                #if !defined(ESP32)
                if(uploadSize >0) {
                    uint8_t percent = (uint8_t)(((float)upload.totalSize/uploadSize)*100);
                    if((percent % 10) == 0 && percent != lastPercent) {
                        __debugS(PSTR("Uploaded: %d%%\r"), percent);
                        lastPercent = percent;
                    }
                    if(neoPixels != nullptr) {
                        if((percent % 25) == 0) { // && percent != lastPercent) {
                            int num = percent / 25;
                            //__debugS(PSTR("Npx=%d"), num);
                            neoPixels->setPixelColor(num, 0xbf00bd);
                        }
                        neoPixels->show();
                    }

                }
                #else
                    __debugS(PSTR("Uploaded: %d%%\r"), upload.totalSize);
                #endif
            }
            break;

        case UPLOAD_FILE_END:
            if(neoPixels != nullptr)
                neoPixels->clear();
            if(Update.end(true)) {
                __debugS(PSTR("Update successful, bytes written %zu"), upload.totalSize);
            } 
            else {
                setUpdaterError();
            }
            break;

        case UPLOAD_FILE_ABORTED:
            Update.end();
            __debugS(PSTR("Update was aborted"));
            break;
    }
    #if !defined(ESP32)
        esp_yield();
    #endif
}

void initWebserver() {
    
    uint8_t mac[6];
    WiFi.macAddress(mac);
    #if !defined(ESP32)
        sprintf(deviceName, PSTR("SMuFF-WI-ESP_%02X%02X%02X"), mac[3], mac[4], mac[5]);
    #else
        sprintf(deviceName, PSTR("SMuFF-WI-ESP32_%02X%02X%02X"), mac[3], mac[4], mac[5]);
    #endif

    #if !defined(ESP32)
      wifi_set_sleep_type(NONE_SLEEP_T);
    #else
      esp_wifi_set_ps(WIFI_PS_NONE);
    #endif
    
    wifiMgr.setConfigPortalBlocking(false);
    wifiMgr.setClass("invert");
    
    if(wifiMgr.autoConnect(deviceName)){
      __debugS(PSTR("WiFi connected"));
      flashIntLED(2, 150);
    }
    else {
      __debugS(PSTR("WiFi not connected. Portal running."));
      flashIntLED(7, 150);
      return;
    }

    if(MDNS.begin(deviceName)) {
        MDNS.addService("SMuFF-WI", "tcp", 80);
        __debugS(PSTR("MDNS set to: %s"), deviceName);
    }
    if(NBNS.begin(deviceName)) {
        __debugS(PSTR("NetBios set to: %s"), deviceName);
    }

    httpUpdateServer.setup(&webServer);

    webServer.on("/info", HTTP_GET, []() {
        // __debugS(PSTR("/info requested; URI: %s"),webServer.uri().c_str());
        char info[60];
        String response, smuffVersion = "No SMuFF attached.";
        __debugS(PSTR("Pinging SMuFF..."));
        isPinging = true;
        SerialSmuff.write("M155S0\n");
        delay(250);
        if(SerialSmuff.available()) {
            serialSmuffEvent();
            bufFromSMuFF.clear();
            SerialSmuff.write("M115\n");
            delay(250);
            if(SerialSmuff.available()) {
                serialSmuffEvent();
                getStringFromBuffer(response);
                isPinging = false;
                if(response.length() > 0) {
                    __debugS(PSTR("SMuFF responded with: %s"), response.c_str());
                    int pos1 =  response.indexOf("FIRMWARE_VERSION:");
                    if(pos1 > -1) {
                        int pos2 = response.indexOf(" ", pos1+19);
                        if(pos2 > -1)
                            smuffVersion = "SMuFF "+ response.substring(pos1+18, pos2) + " attached.";
                    }
                    // __debugS(PSTR("%s"), smuffVersion.c_str());
                }
            }
        }
        snprintf_P(info, ArraySize(info)-1, "%s V%s\n%s", MCUTYPE, VERSION, smuffVersion.c_str());
        sendResponse(200, MIME_TEXT, String(info));
    });
    webServer.on("/debug", HTTP_GET, []() {
        // __debugS(PSTR("/debug requested; URI: %s"),webServer.uri().c_str());
        sendResponse(200, MIME_TEXT, String(debugOut.toString()));
        debugOut.clear();
    });
    webServer.on("/clear", HTTP_GET, []() {
        debugOut.clear();
        sendOkResponse();
    });
    webServer.on("/reset", HTTP_GET, []() {
        String value = webServer.arg("v");
        if(value == "wifi") {
            webServer.sendHeader(PSTR("Location"),PSTR("http://192.168.4.1"));
            webServer.send(303);
            wifiMgr.resetSettings();                // wipe credentials
        }
        if(value == "me") {
            ESP.restart();
        }
    });
    

    webServer.on("/uploadBinary", HTTP_POST, [&]() {
        __debugS("Update requested...");
        
        /* UPLOAD happens here */

        String updated = String(updateBinaryPage);
        if (Update.hasError()) {
            updated.replace(PSTR("{0}"), PSTR("Update failed!"));
            updated.replace(PSTR("{1}"), "<p>Failure reason:<br/>" + updaterError +"</p>");
            sendResponse(200, MIME_HTML, updated);
        } 
        else {
            // __debugS("Uploaded: %s", webServer.upload().name);
            webServer.client().setNoDelay(true);
            updated.replace(PSTR("{0}"),PSTR("Update successful!"));
            updated.replace(PSTR("{1}"), PSTR(""));
            sendResponse(200, MIME_HTML, updated);
            Update.clearError();
            if (webServer.upload().filename == "firmware.bin" || 
                webServer.upload().filename == "littlefs.bin") {
                __debugS(PSTR("\nFirmware/Filesystem was uploaded, restarting ESP...\n\n"));
                webSocketServer.close();
                webServer.client().stop();
                delay(100);
                ESP.restart();
            }
        }
    }, handleFileUpload); 

    #if defined(USE_FS)
        webServer.serveStatic("/", LittleFS, "/");
    #endif

    webServer.onNotFound(handle404);
    webServer.client().setTimeout(5000);    // bug in documentation, says seconds but means milliseconds
    __debugS(PSTR("Client timeout set to %d"), webServer.client().getTimeout());

    #if !defined(ESP32)
        webServer.keepAlive(true);
    #else
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
        // disconnected... seems to be a bug in the ESP library
        // try to reconnect using WiFiManager
        if(wifiMgr.autoConnect(deviceName)){
            __debugS(PSTR("WiFi reconnected"));
        }
    }, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    #endif
    webServer.begin();
    __debugS(PSTR("Webserver running"));
}

void hexDump(const void *mem, uint32_t len, uint8_t cols = 16) {
    char line[30+cols*7];
    char dump [7];

	const uint8_t* src = (const uint8_t*) mem;
	__debugS(PSTR("[HEXDUMP] Address: 0x%08X len: 0x%X (%d)"), (ptrdiff_t)src, len, len);
	for(uint32_t i = 0; i < len; i++) {
		if(i % cols == 0) {
            sprintf(line, PSTR("[0x%08X] 0x%08X: "), (ptrdiff_t)src, i);
		}
		sprintf(dump, PSTR("%02X "), *src);
        strcat(line, dump);
		src++;
	}
	__debugS(line);
}

void wsEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

    curClient = (int)num;
    switch(type) {
        case WStype_DISCONNECTED:
            __debugS(PSTR("%s%u has disconnected!"), wsCliPrefix, num);
            curClient = -1;
            wsClientsConnected--;
            if(wsClientsConnected < 0)
                wsClientsConnected = 0;
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocketServer.remoteIP(num);
                __debugS(PSTR("%s%u has connected from %d.%d.%d.%d url: %s"), wsCliPrefix, num, ip[0], ip[1], ip[2], ip[3], payload);
                wsClientsConnected++;
            }
            break;
        case WStype_TEXT: {
                String cmd = String((const char*)payload);
                __debugS(PSTR("%s%u sent: %s"), wsCliPrefix, num, cmd.c_str());
                if(cmd.startsWith(cmdWI)) {
                    if(cmd.length() > 7)
                        handleControlMessage(cmd.substring(7));
                    else
                        __debugS("Malformed WI-CMD!");
                }
                else {
                    SerialSmuff.write(cmd.c_str());
                    wiSent++;
                }
            }
            break;
        case WStype_BIN:
            __debugS(PSTR("%s%u sent: %u bytes"), wsCliPrefix, num, length);
            hexDump(payload, length);
            break;
		case WStype_ERROR:
            __debugS(PSTR("%s%u has caused an error"), wsCliPrefix, num);
            break;
		case WStype_PING:
            __debugS(PSTR("%s%u has pinged"), wsCliPrefix, num);
            break;
		case WStype_PONG:
            __debugS(PSTR("%s%u has ponged"), wsCliPrefix, num);
            break;
		case WStype_FRAGMENT_TEXT_START:
		case WStype_FRAGMENT_BIN_START:
		case WStype_FRAGMENT:
		case WStype_FRAGMENT_FIN:
            __debugS(PSTR("%s%u has sent an unsupported type (0x%04x / %d)"), wsCliPrefix, num, type, type);
			break;
    }
}

void initWebsockets() {
    webSocketServer.begin();
    webSocketServer.onEvent(wsEvent);
    __debugS(PSTR("WebSocketsServer running"));
}

void sendToWebsocket(String& data) {
    if(wsClientsConnected == 0)
        return;
    if(curClient != -1) {
        webSocketServer.sendTXT((uint8_t)curClient, data);
    }
    else {
        __debugS(PSTR("Invalid WS Client ID!"));
    }
}

void loopWebserver() {
    wifiMgr.process();
    webServer.handleClient();
    webSocketServer.loop();
    #if !defined(ESP32)
      MDNS.update();      // ESP32 doesn't have this method
    #endif
}