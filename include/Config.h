#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <WiFiManager.h>
#include "StringStream.h"
#if defined(ESP32)
#include <BluetoothSerial.h>
#else
#include "SoftwareSerial.h"
#endif
#include "Adafruit_NeoPixel.h"
#include "lib8tion/lib8tion.h"

#define VERSION         "1.0.0"
#if defined(ESP32)
#define MCUTYPE         "ESP32"
#else
#define MCUTYPE         "ESP8266"
#endif
#define USE_FS          1
#define DEBUG           1
#if defined (ESP32)
#define RXD0_PIN        3   // RX0
#define TXD0_PIN        1   // TX0
#define RXD2_PIN        16  // GPIO16
#define TXD2_PIN        17  // GPIO17
#define LED_PIN         12  // GPIO12
#define NPX_PIN         12  // GPIO12
#else
#define RXD2_PIN        13  // D7
#define TXD2_PIN        15  // D8
#define LED_PIN         14  // D5
#define NPX_PIN         14  // D5
#endif
#define INTLED_PIN      2   // GPIO2 - built in LED (D4)
#define BAUDRATE        115200
#define BAUDRATE2       19200

#define DEFAULT_NUMLEDS 4
#define PULSE_BPM       20

#define ArraySize(arr) (sizeof(arr) / sizeof(arr[0]))

#define MIME_JSON       "text/json"
#define MIME_HTML       "text/html"
#define MIME_TEXT       "text/plain"

extern WiFiManager      wifiMgr;
extern int              wifiBtn;
extern char             deviceName[];
extern HardwareSerial   SerialSmuff;
#if defined(ESP32)
#if !defined(NOBT)
extern BluetoothSerial  SerialBT;
#endif
extern HardwareSerial   SerialUART;
#else
extern EspSoftwareSerial::UART SerialUART;
#endif
extern StringStream     debugOut;
extern unsigned long    smuffSent, wiSent, btSent;
extern int              btConnections;
extern bool             debugToUART;
extern bool             logToUART;
extern bool             debugMemInfo;
extern int              numLeds;
extern Adafruit_NeoPixel*  neoPixels;
extern bool             isPulsing;
extern uint16_t         pulseBPM;
extern uint16_t         pulseBPMfast;
extern uint16_t         pulseBPMprev;
extern uint16_t         pulseColor;
extern uint16_t         hueMap[];

typedef enum {
  HUE_WHITE     = 0,
  HUE_ORANGE    =  7300,
  HUE_YELLOW    = 11000,
  HUE_GREEN     = 23000,
  HUE_CYAN      = 34000,
  HUE_BLUE      = 43200,
  HUE_PURPLE    = 52400,
  HUE_PINK      = 57000,
  HUE_RED       = 65300
} HSVHue;

extern void initWebserver();
extern void initWebsockets();
extern void sendToWebsocket(String& data);
extern void loopWebserver();
extern void initDisplay();
extern void resetDisplay();
extern void drawIPAddress(const char* buf);
extern void drawAP(const char* buf);
extern void drawScreen();
extern void flashIntLED(int repeat, int _delay = 300);
extern void __debugS(const char *fmt, ...);
extern void __logS(const char *fmt, ...);
extern int  __debugESP(const char* fmt, va_list arguments);
extern void handleControlMessage(String msg);
extern void initNeoPixels();
extern void setNeoPixelPulsing();
