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

#define PARAMOBJECT_MAX_STRING_LEN  256

 struct ParamObject {
    enum ParamType { None, Byte, Int, Hex, Bin, String } Type;
    union {
        byte        Byte;
        int         Int;
        unsigned    Hex;
        char        String[PARAMOBJECT_MAX_STRING_LEN];
    } Value;
};

void handleNeoPixel(String cmd);
void handleUART(String cmd);
void handleDebug(String cmd);
void handleLog(String cmd);
void handleESP(String cmd);
void handleSystem(String cmd);
void sendResponse(const char* fmt, ...);
void sendParamErrResponse(const char* cmd, int got, int expected);
void sendUnknownCmdResponse(const char* cmd, const char* got);
String translateParamType(ParamObject::ParamType type);

Adafruit_NeoPixel*  neoPixels;
int                 numLeds = 0;
uint16_t            hueMap[] = { HUE_WHITE, HUE_ORANGE, HUE_YELLOW, HUE_GREEN, HUE_CYAN, HUE_BLUE, HUE_PURPLE, HUE_PINK, HUE_RED };
const char*         colorNames[] PROGMEM = { "WHITE", "ORANGE", "YELLOW", "GREEN", "CYAN", "BLUE", "PURPLE", "PINK", "RED" };
static String       okString = String("ok\n");
ParamObject         firstParam;
ParamObject         secondParam;

#define HEAT_COLOR      hueMap[8]   // Red
#define HEATING_COLOR   hueMap[8]   // Red
#define COOL_COLOR      hueMap[5]   // Blue

const char cmdNPX[] PROGMEM     = { "NPX:" };
const char cmdUART[] PROGMEM    = { "UART:" };
const char cmdDBG[] PROGMEM     = { "DBG:" };
const char cmdLOG[] PROGMEM     = { "LOG:" };
const char cmdESP[] PROGMEM     = { "ESP:" };
const char cmdSYS[] PROGMEM     = { "SYS:" };

const char npxMode[] PROGMEM    = { "NeoPixels mode:" };

const char fncINIT[] PROGMEM    = { "INIT" };
const char fncCLEAR[] PROGMEM   = { "CLEAR" };
const char fncBRIGHT[] PROGMEM  = { "BRIGHT" };
const char fncFILL[] PROGMEM    = { "FILL" };
const char fncPULSE[] PROGMEM   = { "PULSE" };
const char fncBPM[] PROGMEM     = { "BPM" };
const char fncHEATING[] PROGMEM = { "HEATING" };
const char fncHEAT[] PROGMEM    = { "HEAT" };
const char fncCOOL[] PROGMEM    = { "COOL" };
const char fncON[] PROGMEM      = { "ON" };
const char fncOFF[] PROGMEM     = { "OFF" };
const char fncINFO[] PROGMEM    = { "INFO" };
const char fncBOOT[] PROGMEM    = { "BOOT" };
const char fncRESET[] PROGMEM   = { "RESET" };
const char fncSEND[] PROGMEM    = { "SEND" };
const char fncWIFI[] PROGMEM    = { "WIFI" };
const char fncMEM[] PROGMEM     = { "MEM" };

const char errNoColor[] PROGMEM         = { "No valid color value found!" };
const char errParamError[] PROGMEM      = { "'%s' parameter error (got %d, expected %d)" };
const char errParamTypeError[] PROGMEM  = { "'%s' parameter '%s' type error. Expected: %s Got: %s" };
const char errOutOfRange[] PROGMEM      = { "'%s' parameter '%s' out of range error. Min=%d, Max=%d, Got: %d" };
const char errUnknownCmd[] PROGMEM      = { "Unknown command received: \"%s\n" };
const char errUnknownFunc[] PROGMEM     = { "Unknown function \"%s\" for command \"%s\"" };

void handleControlMessage(String msg) {
  if(msg.startsWith(cmdNPX)) {
    // __debugS("NeoPixel command received");
    handleNeoPixel(msg.substring(4));
  }
  else if(msg.startsWith(cmdUART)) {
    // __debugS("UART command received");
    handleUART(String(msg.substring(5)));
  }
  else if(msg.startsWith(cmdDBG)) {
    // __debugS("Debug command received");
    handleDebug(String(msg.substring(4)));
  }
  else if(msg.startsWith(cmdLOG)) {
    // __debugS("Log command received");
    handleLog(String(msg.substring(4)));
  }
  else if(msg.startsWith(cmdESP)) {
    handleESP(String(msg.substring(4)));
  }
  else if(msg.startsWith(cmdSYS)) {
    handleSystem(String(msg.substring(4)));
  }
  else {
    sendResponse(errUnknownCmd, msg.c_str());
  }

  sendToWebsocket(okString);
}

void sendUnknownCmdResponse(const char* cmd, const char* got) {
    sendResponse(errUnknownFunc, got, cmd);
}

void sendParamErrResponse(const char* cmd, int got, int expected) {
    sendResponse(errParamError, cmd, got, expected);
}

void sendRangeErrResponse(const char* cmd, const char* param, int min, int max, int got) {
    sendResponse(errOutOfRange, cmd, param, min, max, got);
}

void sendParamWrongTypeResponse(const char* cmd, const char* param, ParamObject::ParamType expected, ParamObject::ParamType got) {
    sendResponse(errParamTypeError, cmd, param, translateParamType(expected).c_str(), translateParamType(got).c_str());
}

void sendResponse(const char* fmt, ...) {
    char resp[2048];
    va_list arguments;
    va_start(arguments, fmt);
    vsnprintf_P(resp, ArraySize(resp) - 1, fmt, arguments);
    va_end(arguments);
    strcat(resp,"\n");

    String tmp = String(PSTR("echo: WI-ESP:\n")) + String(resp);
    sendToWebsocket(tmp);
    // __debugS(PSTR("%s"), tmp.c_str());
}

uint8_t getFunction(const char* cmd, char* fnc, uint8_t maxFncLen, char* params, uint8_t maxParamLen) {
    byte c;
    uint8_t ndx = 0;
    do {
        c = *cmd++;
        if(c == ':' || c == '\r' || c == '\n')
            break;
        if(ndx < maxFncLen)
            fnc[ndx++] = toUpperCase(c);
        else
            break;
    } while((c > 0));
    fnc[ndx] = 0;
    
    ndx = 0;
    uint8_t paramCnt = 0;
    do {
        c = *cmd++;
        if(c == '\n' || c == '\r')
            break;
        if(c == ' ') {
            if(ndx == 0)
                continue;
            paramCnt++;
        }
        if(ndx < maxParamLen)
            params[ndx++] = c;
        else
            break;
    } while((c > 0));
    params[ndx] = 0;
    paramCnt = strlen(params) > 0 ? paramCnt+1 : 0;

    // __debugS("getFunction: Func=%s, Params=%s, Count=%d", fnc, params, paramCnt);
    return paramCnt;
}

uint32_t getNextParam(const char* params, ParamObject* param) {
    int ndx = 0;
    char nextParam[80];
    int maxLen = ArraySize(nextParam);
    bool isHex = false, isString = false, isBin = false, isQuote = false;
    const char* pptr = params;

    while(*pptr == ' ') {
        pptr++;
        if(*pptr == 0)
            break;
    }
    if(*pptr == 0) {
        param->Type = ParamObject::ParamType::None;
        return 0;
    }
    // __debugS(PSTR("getNextParam: [%s]"), params);

    param->Type = ParamObject::ParamType::Int;     // parameter is Integer by default
    memset(nextParam, 0, ArraySize(nextParam)-1);
    do {
        // __debugS(PSTR("*param: [%c]"), *pptr);
        if(!isQuote && (*pptr == ' ' || *pptr == '\n')) {
            pptr++;
            break;
        }
        if(*pptr == '#') {
            pptr++;
            isHex = true;
            continue;
        }
        if(*pptr == '"') {
            pptr++;
            isString = true;
            isQuote = !isQuote;
            continue;
        }
        nextParam[ndx++] = *pptr;
        if(!isHex && isAlpha(*pptr))
            isString = true;
        pptr++;
        if(ndx == maxLen)
            break;
    } while(*pptr != 0);

    int res = 0;
    if(isHex) {
        param->Type = ParamObject::ParamType::Hex;
        param->Value.Hex = strtoul(nextParam, NULL, 16);
    }
    else if(isString) {
        param->Type = ParamObject::ParamType::String;
        strncpy(param->Value.String, nextParam, PARAMOBJECT_MAX_STRING_LEN-1);
    }
    else {
        param->Type = ParamObject::ParamType::Int;
        param->Value.Int = atoi(nextParam);
    }
    /*
    if(res) {
        __debugS(PSTR("Param is of type [%s]"), translateParamType(param->Type).c_str());
        switch(param->Type) {
            case ParamObject::ParamType::None:
                __debugS("No param found");
                break;
            case ParamObject::ParamType::Byte:
                __debugS(PSTR("Value = '%c'"), param->Value.Byte);
                break;
            case ParamObject::ParamType::Bin:
                __debugS(PSTR("Value = %d"), param->Value.Byte);
                break;
            case ParamObject::ParamType::Int:
                __debugS(PSTR("Value = %d"), param->Value.Int);
                break;
            case ParamObject::ParamType::Hex:
                __debugS(PSTR("Value = 0x%x"), param->Value.Hex);
                break;
            case ParamObject::ParamType::String:
                __debugS(PSTR("Value = \"%s\""), param->Value.String);
                break;
        }
    }
    */

    uint32_t nxtIndex = ((uint32_t)pptr-(uint32_t)params)-1;
    // __debugS(PSTR("NextParam: ndx=%u, next=[%s] remain=%d"), nxtIndex, (pptr == nullptr ? "(null)" : pptr), strlen(pptr));
    return nxtIndex;
}

String translateParamType(ParamObject::ParamType type) {
    switch(type) {
        case ParamObject::ParamType::None:
            return String("None");
        case ParamObject::ParamType::Byte:
            return String("Byte");
        case ParamObject::ParamType::Int:
            return String("Integer");
        case ParamObject::ParamType::Hex:
            return String("Hex");
        case ParamObject::ParamType::Bin:
            return String("Binary");
        case ParamObject::ParamType::String:
            return String("String");
    }
    return String("<Unknown>");
}

int translateColorString(char* colorByName, int32_t *color) {
    int res = 0;
    bool foundName = false;
    if(*colorByName != 0) {
        for(uint16_t i=0; i < ArraySize(colorNames); i++) {
            if(strncmp_P(strupr(colorByName), colorNames[i], 2) == 0){
                *color = i;
                foundName = true;
                // __debugS(PSTR("Color found: %s"), colorNames[i]);
                break;
            }
        }
        if(foundName)
            res = 1;
    }
    return res;
}

uint16_t getColorParam(const char* params, int32_t* gotColor, int* hasMoreParam) {
    int32_t color;
    int res = 0;
    ParamObject param;
    param.Type = ParamObject::ParamType::None;

    memset(param.Value.String, 0, PARAMOBJECT_MAX_STRING_LEN-1);
    *gotColor = -1;
    *hasMoreParam = 0;
    int len = getNextParam(params, &param);
    // __debugS(PSTR("getColorParam: returned type '%s'"), translateParamType(param.Type).c_str());

    switch(param.Type) {
        case ParamObject::ParamType::None:
        case ParamObject::ParamType::Byte:
        case ParamObject::ParamType::Bin:
            res = 0;
            break;
        case ParamObject::ParamType::Int:
        case ParamObject::ParamType::Hex:
            color = param.Value.Hex;
            res = 1;
            break;
        case ParamObject::ParamType::String:
            res = translateColorString(param.Value.String, &color);
            break;
        default:
            break;
    }
    if(res) {
        *gotColor = color;
        // __debugS(PSTR("Color set: %u %u"), *gotColor, *gotColor);
    }
    // else
    //     __debugS(PSTR("Color not set!"));
    if(len)
        *hasMoreParam = len;
    return res;
}


void handleNeoPixel(String cmd) {
    char func[30];
    char params[60];
    uint8_t paramCnt = getFunction(cmd.c_str(), func, ArraySize(func)-1, params, ArraySize(params)-1);
    
    // __debugS("CMD: Func=[%s] Params=[%s] (%d param(s))", func, params, paramCnt);
    
    if(strcmp_P(func, fncINIT) != 0 && (numLeds == 0 || neoPixels == nullptr)) {
        __debugS(PSTR("NeoPixels not initialized yet!"));
        return;
    }
    const char* pptr = params;
    uint32_t len;

    if(strcmp_P(func, fncINIT) == 0) {
        firstParam.Value.Int = 0;
        len = getNextParam(pptr, &firstParam);
        // __debugS(PSTR("getNextParam returned: %d Type=%d"), len, firstParam.Type);
        if(firstParam.Type == ParamObject::ParamType::Int) {
            numLeds = firstParam.Value.Int;
            initNeoPixels();
            sendResponse(PSTR("NeoPixels initialized with %d leds."), numLeds);
        }
        else {
            sendParamWrongTypeResponse(cmdNPX, fncINIT, ParamObject::ParamType::Int, firstParam.Type);
            return;
        }

    }
    else if(strcmp_P(func, fncCLEAR) == 0) {
        isPulsing = false;
        neoPixels->clear();
        return;
    }
    else if(strcmp_P(func, fncBRIGHT) == 0) {
        len = getNextParam(pptr, &firstParam);
        if(firstParam.Type == ParamObject::ParamType::Int) {
            neoPixels->setBrightness((uint8_t)firstParam.Value.Int);
        }
        else {
            sendParamWrongTypeResponse(cmdNPX, fncBRIGHT, ParamObject::ParamType::Int, firstParam.Type);
            return;
        }
        return;
    }
    else if(strcmp_P(func, fncFILL) == 0) {
        int32_t color = -1;
        int hasMoreParam;
        if(!getColorParam(pptr, &color, &hasMoreParam)) {
            sendResponse(errNoColor);
            return;
        }
        if(color > 0) {
            isPulsing = false;
            neoPixels->fill(color);
        }
        return;
    }
    else if(strcmp_P(func, fncON) == 0) {
        int32_t color;
        firstParam.Value.Int = 0;
        len = getNextParam(pptr, &firstParam);
        if(firstParam.Type == ParamObject::ParamType::Int) {
            if(firstParam.Value.Int < 0 || firstParam.Value.Int >= numLeds) {
                sendRangeErrResponse(cmdNPX, fncON, 0, numLeds, firstParam.Value.Int);
                return;
            }
        }
        else {
            sendParamWrongTypeResponse(cmdNPX, fncON, ParamObject::ParamType::Int, firstParam.Type);
            return;
        }
        int hasMoreParam;
        int ret = getColorParam(pptr+len, &color, &hasMoreParam);
        if(ret) {
            neoPixels->setPixelColor(firstParam.Value.Int, color);
        }
        else {
            sendResponse(errNoColor);
            return;
        }
        return;
    }
    else if(strcmp_P(func, fncOFF) == 0) {
        firstParam.Value.Int = 0;
        getNextParam(pptr, &firstParam);
        if(firstParam.Type == ParamObject::ParamType::Int) {
            if(firstParam.Value.Int < 0 || firstParam.Value.Int >= numLeds) {
                sendRangeErrResponse(cmdNPX, fncOFF, 0, numLeds, firstParam.Value.Int);
                return;
            }
            neoPixels->setPixelColor(firstParam.Value.Int, 0);
        }
        else {
            sendParamWrongTypeResponse(cmdNPX, fncON, ParamObject::ParamType::Int, firstParam.Type);
            return;
        }
        return;
    }
    else if(strcmp_P(func, fncHEAT) == 0) {
        // neoPixels->clear();
        pulseColor = HEAT_COLOR;
        pulseBPM = pulseBPMprev;
        isPulsing = true;
        sendResponse(PSTR("%s %s"), npxMode, fncHEAT);
        return;
    }
    else if(strcmp_P(func, fncHEATING) == 0) {
        // neoPixels->clear();
        pulseBPMprev = pulseBPM;
        pulseBPM = pulseBPMfast;
        pulseColor = HEAT_COLOR;
        isPulsing = true;
        sendResponse(PSTR("%s %s"), npxMode, fncHEATING);
        return;
    }
    else if(strcmp_P(func, fncCOOL) == 0) {
        // neoPixels->clear();
        pulseColor = COOL_COLOR;
        pulseBPM = pulseBPMprev;
        isPulsing = true;
        sendResponse(PSTR("%s %s"), npxMode, fncCOOL);
        return;
    }
    else if(strcmp_P(func, fncPULSE) == 0) {
        isPulsing = false;
        neoPixels->clear();
        
        int32_t color = -1;
        int hasMoreParam;
        int res = getColorParam(pptr, &color, &hasMoreParam);
        if(res == 0) {
            sendResponse(errNoColor);
            return;
        }
        else {
            if(color < 0 || color >= (int32_t)ArraySize(hueMap)) {
                sendResponse(PSTR("Pulse function accepts only HUE color index (0..%d)."), ArraySize(hueMap)-1);
                return;
            }
            pulseColor = hueMap[(uint16_t)color];
            isPulsing = true;
            // __debugS(PSTR("hasMoreParam %d, ->%s"), hasMoreParam, pptr+hasMoreParam);
            if(hasMoreParam > 0) {
                pptr += hasMoreParam;
                getNextParam(pptr, &secondParam);
                // __debugS(PSTR("hasMoreParam %d, ->%d"), secondParam.Type, secondParam.Value.Int);
                if(secondParam.Type == ParamObject::ParamType::None)
                    return;
                if(secondParam.Type == ParamObject::ParamType::Int) {
                    if(secondParam.Value.Int < 0 || secondParam.Value.Int >= 255) {
                        sendRangeErrResponse(cmdNPX, func, 1, 255, secondParam.Value.Int);
                    }
                    pulseBPMprev = pulseBPM;
                    pulseBPM = (uint16_t)secondParam.Value.Int;
                }
                else {
                    sendParamWrongTypeResponse(cmdNPX, func, ParamObject::ParamType::Int, secondParam.Type);
                    return;
                }
            }
            sendResponse(PSTR("NeoPixels pulsing '%s' at %d BPM."), colorNames[color], pulseBPM);
        }
    }
    else if(strcmp_P(func, fncBPM) == 0) {
        firstParam.Value.Int = 0;
        int len = getNextParam(pptr, &firstParam);
        if(firstParam.Type == ParamObject::ParamType::Int) {
            if(firstParam.Value.Int < 0 || firstParam.Value.Int >= 255) {
                sendRangeErrResponse(cmdNPX, func, 1, 255, firstParam.Value.Int);
            }
            pulseBPM = (uint16_t)firstParam.Value.Int;
            pulseBPMprev = pulseBPM;
        }
        else {
            sendParamWrongTypeResponse(cmdNPX, func, ParamObject::ParamType::Int, firstParam.Type);
            return;
        }
        if(len > 0) {
            pptr += len;
            getNextParam(pptr, &secondParam);
            if(secondParam.Type == ParamObject::ParamType::None)
                return;
            if(secondParam.Type == ParamObject::ParamType::Int) {
                if(secondParam.Value.Int < 0 || secondParam.Value.Int >= 255) {
                    sendRangeErrResponse(cmdNPX, func, 1, 255, secondParam.Value.Int);
                }
                pulseBPMfast = (uint16_t)secondParam.Value.Int;
            }
            else {
                sendParamWrongTypeResponse(cmdNPX, func, ParamObject::ParamType::Int, secondParam.Type);
                return;
            }
        }
        return;
    }
    else {
        sendUnknownCmdResponse(cmdNPX, cmd.c_str());
    }
}

void handleUART(String cmd) {
    char func[30];
    char params[60];
    uint8_t paramCnt = getFunction(cmd.c_str(), func, ArraySize(func)-1, params, ArraySize(params)-1);

    if(strcmp_P(func, fncSEND) == 0) {
        String data = params;
        data.replace("\\n", "\n");
        SerialUART.write(data.c_str());
    }
    else {
        sendUnknownCmdResponse(cmdUART, cmd.c_str());
    }
}

void handleDebug(String cmd) {
    char func[30];
    char params[60];
    uint8_t paramCnt = getFunction(cmd.c_str(), func, ArraySize(func)-1, params, ArraySize(params)-1);

    if(strcmp_P(func, fncON) == 0) {
        debugToUART = true;
    }
    else if(strcmp_P(func, fncOFF) == 0) {
        debugToUART = false;
    }
    else if(strcmp_P(func, fncMEM) == 0) {
        int state = atoi(params);
        debugMemInfo = state == 1;
    }
    else {
        sendUnknownCmdResponse(cmdDBG, cmd.c_str());
    }
}

void handleLog(String cmd) {
    char func[30];
    char params[60];
    uint8_t paramCnt = getFunction(cmd.c_str(), func, ArraySize(func)-1, params, ArraySize(params)-1);

    if(strcmp_P(func, fncON) == 0) {
        logToUART = true;
    }
    else if(strcmp_P(func, fncOFF) == 0) {
        logToUART = false;
    }
    else {
        sendUnknownCmdResponse(cmdLOG, cmd.c_str());
    }
}

void handleSystem(String cmd) {
    char func[30];
    char params[60];
    uint8_t paramCnt = getFunction(cmd.c_str(), func, ArraySize(func)-1, params, ArraySize(params)-1);

    if(strcmp_P(func, fncINFO) == 0) {
        sendResponse(PSTR("SMuFF-WI-ESP\nVersion:\t\t%s\nMCU Type:\t\t%s\nDevice name:\t%s\n"), 
            VERSION, 
            MCUTYPE,
            deviceName);
    }
    else if(strcmp_P(func, fncWIFI) == 0) {
        sendResponse(PSTR("Device name:\t%s\nLocal IP:\t%s\nWiFi host:\t%s\nWiFi status:\t%s\n"), 
            deviceName,
            WiFi.localIP().toString().c_str(),
            wifiMgr.getWiFiHostname().c_str(), 
            wifiMgr.getWLStatusString().c_str());
    }
    else {
        sendUnknownCmdResponse(cmdSYS, cmd.c_str());
    }
}

void handleESP(String cmd) {
    char func[30];
    char params[60];
    uint8_t paramCnt = getFunction(cmd.c_str(), func, ArraySize(func)-1, params, ArraySize(params)-1);

    if(strcmp_P(func, fncBOOT) == 0) {
        ESP.restart();
    }
#if !defined(ESP32)
    else if(strcmp_P(func, fncRESET) == 0) {
        ESP.reset();
    }
#endif
    else if(strcmp_P(func, fncINFO) == 0) {
        #if !defined(ESP32)
        sendResponse(PSTR("Chip ID:\t\t0x%x\nCore Version:\t%s\nCPU Freq.:\t\t%d MHz\nFree Heap:\t\t%u B\nFree Stack:\t\t%u B\nReset Reason:\t%s\nReset Info:\t%s\n"),
            ESP.getChipId(),
            ESP.getCoreVersion().c_str(),
            ESP.getCpuFreqMHz(), 
            ESP.getFreeHeap(),
            ESP.getFreeContStack(),
            ESP.getResetReason().c_str(),
            ESP.getResetInfo().c_str());
        #else
        sendResponse(PSTR("Chip Model:\t\t%s\nCore Revision:\t%d\nCPU Freq.:\t\t%d MHz\nFree Heap:\t\t%u B\nFree PSRam:\t\t%u B\n"),
            ESP.getChipModel(),
            ESP.getChipRevision(),
            ESP.getCpuFreqMHz(), 
            ESP.getFreeHeap(),
            ESP.getFreePsram());
        #endif
    }
    else if(strcmp_P(func, fncMEM) == 0) {
        #if !defined(ESP32)
        sendResponse("FreeHeap: %u B, FreeStack: %u B", ESP.getFreeHeap(), ESP.getFreeContStack());
        #else
        sendResponse("FreeHeap: %u B, FreePSRam: %u B", ESP.getFreeHeap(), ESP.getFreePsram());
        #endif
    }
    else {
        sendUnknownCmdResponse(cmdESP, cmd.c_str());
    }
}
