# WI-ESP Control Commands

WI-ESP comes with it's own set of commands, which can be used to control some of the functions built in. These commands are being sent through the WI Console, but instead of being passed to the SMuFF, they'll get evaluated locally on the ESP device. All such commands must start with the prefix **WI-CMD:** For example:
>WI-CMD:NPX:COUNT:8
WI-CMD:NPX:FILL: FF00FF
WI-CMD:DBG:OFF
WI-CMD:SYS:INFO
WI-CMD:ESP:INFO
WI-CMD:UART:SEND:Hello World\n

To keep parsing simple, those commands (and it's parameter) need to be all upper case. Parameter are either of type **Integer**, **Hex** or **String** and must be separated by blanks.

Responses from the WI-ESP (if available) will be displayed in the Console window, starting with the prefix "**echo: WI-CMD:**" and ending up with the string "**ok**".

Here's now a list of WI-ESP control commands the device will handle:

## NPX

This set of commands is addressing the NeoPixels connected at the NeoPixel  port.

|Command|Function|Parameter 1 |Parameter 2
|---|---|---|---
|INIT|Sets up the NeoPixels strip. Must be the very first command issued.|Amount of NeoPixels attached|-
|CLEAR|Clears the NeoPixel strip (turns all LEDs off).|-|-
|BRIGHT|Sets the max. brightness of the LEDs.|Brightness level 0..255|-
|FILL|Fills the NeoPixel strip with a given color.|The color value (see *Color Values* down below)|-
|ON|Turns a NeoPixel LED on with a given color.|LED index, starting at 0|The color value (see *Color Values* down below)
|OFF|Turns a NeoPixel LED off|Same as for **ON**|-
|BPM|Sets the *Beats Per Minute* value for *PULSE*, *HEAT*, *HEATING* and *COOL*. *HEATING* uses the 2nd BPM setting|Beats Per Minute (0..255; default = 12)|[Optional] Fast Beats Per Minute (0..255; default = 60)
|PULSE|Pulses the NeoPixel strip with a given color and BPM.|The color value (see *Color Values* down below)|Beats Per Minute (0..255; default = BMP setting)
|HEAT|Pulses the NeoPixel strip in bright RED|-|-
|HEATING|Pulses the NeoPixel strip **fast** in bright RED when the Heater is actually turned on (see **BPM** 2nd parameter)|-|-
|COOL|Pulses the NeoPixel strip in dark BLUE|-|-

### Color-Values

Beside the typical hex notation for a RGB color value (#FFAABB) and it's decimal equivalent.
Please keep in mind: for PULSE one **must** define an Hue value (a number between 0 and 8) or it's Name (whereas for the later only the first two letters are relevent).

|Color|Name| Hue Value
|:---:|---|:---:
|<span style='background-color: #ffffff; color: #ffffff;border: 1px solid #333;'>XXX</span>|WHITE| 0
|<span style='background-color: #ffa500; color: #ffa500;'>XXX</span>|ORANGE| 1
|<span style='background-color: #ffff00; color: #ffff00;'>XXX</span>|YELLOW| 2
|<span style='background-color: #7cfc00; color: #7cfc00;'>XXX</span>|GREEN| 3
|<span style='background-color: #00ffff; color: #00ffff;'>XXX</span>|CYAN| 4
|<span style='background-color: #4169f1; color: #4169f1;'>XXX</span>|BLUE| 5
|<span style='background-color: #ba55d3; color: #ba55d3;'>XXX</span>|PURPLE| 6
|<span style='background-color: #ff00ff; color: #ff00ff;'>XXX</span>|PINK| 7
|<span style='background-color: #ff0000; color: #ff0000;'>XXX</span>|RED| 8

## UART

This set of commands is addressing the spare UART port, which runs on Software Serial and is initialized to 19200 Baud.

|Command|Function|Parameter
|---|---|---
|SEND|Sends a string to the UART port.|String to be sent (add a **\n** for a newline in between).

## DBG

|Command|Function|Parameter
|---|---|---
|ON|Enables sending Debug messages to the UART port.|-
|OFF|Disables sending Debug messages to the UART port.|-
|MEM|En-/disables printing periodical heap memory info.| 1 = ON, 0 = OFF

## LOG

|Command|Function|Parameter
|---|---|---
|ON| Enables sending messages comming from SMuFF to the UART port.|-
|OFF| Disabled sending messages comming from SMuFF to the UART port.|-

## ESP

|Command|Function
|---|---
|INFO| Shows information about the ESP device.
|BOOT| Reboots the device.
|RESET| Resets the device.
|MEM| Shows free heap and stack space.

## SYS

|Command|Function
|---|---
|INFO| Shows information about the Wi-ESP firmware.
|WIFI| Shows information about the WiFi state.
