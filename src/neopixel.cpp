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

bool                isPulsing = false;
uint16_t            pulseBPM = 12;
uint16_t            pulseBPMprev = 12;
uint16_t            pulseBPMfast = 60;
uint16_t            pulseColor;
volatile uint32_t   __systick;

void initNeoPixels() {
  neoPixels = new Adafruit_NeoPixel(numLeds, NPX_PIN, NEO_GRB + NEO_KHZ800);
  isPulsing = false;
  neoPixels->begin();
  neoPixels->fill(neoPixels->Color(255,0,255), 0, numLeds);
  neoPixels->show();
  neoPixels->clear();
}

void setNeoPixelPulsing() {
  uint8_t brightness = beatsin8(pulseBPM, 0, 255);
  if(neoPixels != nullptr)
    neoPixels->fill(neoPixels->gamma32(neoPixels->ColorHSV(pulseColor, (pulseColor == 0 ? 0 : 255), brightness)));
}

void setNeoPixelPulsing(int num) {
  uint8_t brightness = beatsin8(pulseBPM, 0, 255);
  if(neoPixels != nullptr)
    neoPixels->setPixelColor(num, neoPixels->gamma32(neoPixels->ColorHSV(pulseColor, (pulseColor == 0 ? 0 : 255), brightness)));
}
