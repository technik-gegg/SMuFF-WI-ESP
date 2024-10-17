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

#define CHUNK_SIZE  250                           // max. number of bytes sent at one go over web-socket

StringStream        debugOut;
bool                debugToUART = true;
bool                logToUART = false;
bool                debugMemInfo = false;
bool                isPinging = false;
HardwareSerial      SerialSmuff(0);               // this one is mandatory!

#if defined(ESP32)
#if !defined(NOBT)
BluetoothSerial     SerialBT;
#endif
HardwareSerial      SerialUART(2);
#else
EspSoftwareSerial::UART SerialUART;
#endif

#if defined(DEBUG_TO_UART)
#define WM_DEBUG_PORT   (Stream&)SerialUART       // route WiFiManager console messages to UART output
#define DEBUG_ESP_PORT  (Stream&)SerialUART       // route ESP console messages to UART output
#else
#define WM_DEBUG_PORT   (Stream&)debugOut         // route WiFiManager console messages to String
#define DEBUG_ESP_PORT  (Stream&)debugOut         // route ESP console messages to String
#endif


String              fromSMuFF;
unsigned long       smuffSent = 0, wiSent = 0, btSent = 0;
RingBuf<byte, 2048> bufFromSMuFF;
uint32_t            millisCurrent;
uint32_t            millisLast;
uint32_t            millisNpxRefresh;
int                 btConnections = 0;
uint16_t            chunkSize = CHUNK_SIZE;

#if defined (ESP32)
void btStatus(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  if (event == ESP_SPP_SRV_OPEN_EVT) {
    __debugS(PSTR("Bluetooth serial connected"));
    btConnections++;
  }
  else if (event == ESP_SPP_CLOSE_EVT) {
    __debugS(PSTR("Bluetooth serial disconnected"));
    btConnections--;
  }
}
#endif

void toggleIntLED() {
  digitalWrite(INTLED_PIN, !digitalRead(INTLED_PIN));
}

void flashIntLED(int repeat, int _delay) {
  for(int i=0; i<(repeat*2); i++) {
    toggleIntLED();
    delay(_delay);
  }
  delay(_delay);
  digitalWrite(INTLED_PIN, HIGH);
}

void setup(){
  #if defined(ESP32)
    esp_log_set_vprintf(__debugESP);
    initDisplay();
    __debugS(PSTR("Display initialized..."));
    drawScreen();
  #endif

  fromSMuFF.reserve(1024);
  // initialize serial ports
  #if !defined(ESP32)
    SerialUART.begin(BAUDRATE2, SWSERIAL_8N1, RXD2_PIN, TXD2_PIN, false);
    __debugS(PSTR("Serial 2 (UART) initialized at %ld Baud"), BAUDRATE2);
    SerialSmuff.begin(BAUDRATE);       // RXD0, TXD0
    __debugS(PSTR("Serial 1 initialized at %ld Baud"), BAUDRATE);
  #else
    SerialUART.begin(BAUDRATE2, SERIAL_8N1, RXD2_PIN, TXD2_PIN);
    __debugS(PSTR("Serial 2 (UART) initialized at %ld Baud"), BAUDRATE2);
    SerialSmuff.begin (BAUDRATE, SERIAL_8N1, RXD0_PIN, TXD0_PIN);
    __debugS(PSTR("Serial 1 initialized at %ld Baud"), BAUDRATE);
    #if !defined(NOBT)
      // setup Bluetooth serial for SMuFF WebInterface connection (ESP32 only)
      SerialBT.begin(deviceName);
      SerialBT.register_callback(btStatus); 
      __debugS(PSTR("Bluetooth Serial initialized"));
    #endif
  #endif
  __debugS(PSTR("--------------------\nSMuFF-WI-ESP Version %s (%s)\n"), VERSION, MCUTYPE);
  __debugS(PSTR("Starting..."));

  #if defined(USE_FS)
    if(LittleFS.begin()) {
      __debugS(("FS init...  ok"));
    }
    else {
      __debugS(PSTR("FS init...  failed"));
    }  
  #endif
  pinMode(INTLED_PIN, OUTPUT);
  digitalWrite(INTLED_PIN, HIGH);
  
  // __debugS(PSTR("Heap before webserver: %zu B"), ESP.getFreeHeap());
  __debugS(PSTR("Webserver init..."));
  initWebserver();
  initWebsockets();

  #if defined(ESP32)
    #if defined(OLED_SSD1306) || defined(OLED_SH1106) || defined(OLED_SH1107)
      drawScreen();
    #else
      __debugS(PSTR("No display attached!"));
    #endif
  #endif

  millisLast = millis();
  millisNpxRefresh = millisLast;

  flashIntLED(3);
  // NeoPixels by default set to 4 LEDs
  numLeds = DEFAULT_NUMLEDS;
  initNeoPixels();
  // __debugS(PSTR("Heap after setup: %zu B"), ESP.getFreeHeap());
}

void serialSmuffEvent() {
  while (SerialSmuff.available()) {
    int in = SerialSmuff.read();
    if(in == -1)
      break;
    
    #if defined(ESP32) && !defined(NOBT)
      if(btConnections > 0)
        SerialBT.write(in);
    #endif
    bufFromSMuFF.lockedPush(in);
  }
}

#if defined(ESP32) && !defined(NOBT)

void serialBTEvent() {
  while (SerialBT.available()) {
    int in = SerialBT.read();
    if(in == -1)
      break;
    SerialSmuff.write(in);
  }
}
#endif

void getStringFromBuffer(String& ref) {
    if(bufFromSMuFF.isEmpty()) {
      ref.clear();
      return;
    }
    bool stat = true;
    do {
      byte b;
      if((stat = (bool)bufFromSMuFF.lockedPop(b))) {
        if(b=='\n')
          break;
        ref += (char)b;
      }
      else
        break;
    } while (stat);
}

template<class T>
void dumpBuffer(const T& buffer, String& ref, const char* dbg, unsigned long* cntRef, bool sendWS = false) {
    if(buffer->isEmpty())
      return;
    bool stat = true;
    bool lineComplete = false;

    if(dbg != nullptr)
      __logS(PSTR("%s sent:"), dbg);

    do {
      byte b;
      if((stat = (bool)buffer->lockedPop(b))) {
        chunkSize--;
        if(b=='\n') {
          lineComplete = true;
          break;
        }
        ref += (char)b;
      }

      // send only data with the max. length of CHUNK_SIZE to not overwhelm the internal buffers
      if(chunkSize == 0) {
        if(sendWS)
          sendToWebsocket(ref);
        if(dbg != nullptr)
          __logS(PSTR("%s"), ref.c_str());
        chunkSize = CHUNK_SIZE;
        ref = ref.substring(CHUNK_SIZE);
      }
      
    } while (stat);

    if(lineComplete) {
      ref += "\n";
      if(ref.startsWith(cmdWI)) {
        // handle specific commands, like for the SerialUART or NeoPixels
        // see wi-control.md for details
        handleControlMessage(String(ref.substring(7)));
        // __debugS(PSTR("%s"), ref.c_str());
        ref.clear();
        return;
      }
      if(sendWS && chunkSize != 0) {
        sendToWebsocket(ref);
        chunkSize = CHUNK_SIZE;
      }
      if(dbg != nullptr)
        __logS(PSTR("%s"), ref.c_str());
      ref.clear();
      *cntRef += 1;
    }
}

void loop() { 

  __systick = millis();           // for Adafruit NeoPixel library

  if(SerialSmuff.available()) {
    serialSmuffEvent();
  }
  #if defined(ESP32) && !defined(NOBT)
    if(SerialBT.available()) {
      serialBTEvent();
    }
  #endif


  if(!bufFromSMuFF.isEmpty() && !isPinging)
    dumpBuffer(&bufFromSMuFF, fromSMuFF, PSTR("SMuFF"), &smuffSent, true);

  loopWebserver();
  
  if(millis()-millisLast > 5000) {
    millisLast = millis();
    if(debugMemInfo)
    #if defined(ESP32)
      __debugS(PSTR("Heap: %u B"), ESP.getFreeHeap());
    #else
      __debugS(PSTR("Heap: %u B Stack: %u B"), ESP.getFreeHeap(), ESP.getFreeContStack());
    #endif
  }
  
  if(neoPixels != nullptr && numLeds > 0 && millis()- millisNpxRefresh > 50) {
    if(isPulsing)
      setNeoPixelPulsing();
    neoPixels->show();
    millisNpxRefresh = millis();
  }

}

static char _dbg[2048];
static char _log[1024];

int __debugESP(const char* fmt, va_list arguments) {
  int res = vsnprintf(_dbg, ArraySize(_dbg) - 1, fmt, arguments);
  if (debugToUART) {
    SerialUART.println(_dbg);
  }
  return res;
}


void __debugS(const char* fmt, ...)
{
  va_list arguments;
  va_start(arguments, fmt);
  vsnprintf_P(_dbg, ArraySize(_dbg) - 1, fmt, arguments);
  va_end(arguments);
  if (debugToUART) {
    // SerialSmuff.printf("%cD%s%c",0x1B, _dbg, 0x1A);
    if(strlen(_dbg) > 1 && _dbg[strlen(_dbg)-1] == '\r')
      SerialUART.print(_dbg);
    else
      SerialUART.println(_dbg);
  }
}

void __logS(const char* fmt, ...)
{
  va_list arguments;
  va_start(arguments, fmt);
  vsnprintf_P(_log, ArraySize(_log) - 1, fmt, arguments);
  va_end(arguments);
  if (logToUART) {
    SerialUART.println(_log);
  }
}
