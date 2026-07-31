<p align="center">
  <img src="/images/wled_logo_akemi.png">
  <a href="https://github.com/wled-dev/WLED/releases"><img src="https://img.shields.io/github/release/wled-dev/WLED.svg?style=flat-square"></a>
  <a href="https://raw.githubusercontent.com/wled-dev/WLED/main/LICENSE"><img src="https://img.shields.io/github/license/wled-dev/wled?color=blue&style=flat-square"></a>
  <a href="https://wled.discourse.group"><img src="https://img.shields.io/discourse/topics?colorB=blue&label=forum&server=https%3A%2F%2Fwled.discourse.group%2F&style=flat-square"></a>
  <a href="https://discord.gg/QAh7wJHrRM"><img src="https://img.shields.io/discord/473448917040758787.svg?colorB=blue&label=discord&style=flat-square"></a>
  <a href="https://kno.wled.ge"><img src="https://img.shields.io/badge/quick_start-wiki-blue.svg?style=flat-square"></a>
  <a href="https://github.com/Aircoookie/WLED-App"><img src="https://img.shields.io/badge/app-wled-blue.svg?style=flat-square"></a>
  <a href="https://gitpod.io/#https://github.com/wled-dev/WLED"><img src="https://img.shields.io/badge/Gitpod-ready--to--code-blue?style=flat-square&logo=gitpod"></a>

  </p>

# Welcome to WLED! ✨

A fast and feature-rich implementation of an ESP32 and ESP8266 webserver to control NeoPixel (WS2812B, WS2811, SK6812) LEDs or also SPI based chipsets like the WS2801 and APA102!

Originally created by [Aircoookie](https://github.com/Aircoookie)

## ⚙️ Features
- WS2812FX library with more than 100 special effects  
- FastLED noise effects and 50 palettes  
- Modern UI with color, effect and segment controls  
- Segments to set different effects and colors to user defined parts of the LED string  
- Settings page - configuration via the network  
- Access Point and station mode - automatic failsafe AP  
- [Up to 10 LED outputs](https://kno.wled.ge/features/multi-strip/#esp32) per instance
- Support for RGBW strips  
- Up to 250 user presets to save and load colors/effects easily, supports cycling through them.  
- Presets can be used to automatically execute API calls  
- Nightlight function (gradually dims down)  
- Full OTA software updateability (HTTP + ArduinoOTA), password protectable  
- Configurable analog clock (Cronixie, 7-segment and EleksTube IPS clock support via usermods) 
- Configurable Auto Brightness limit for safe operation  
- Filesystem-based config for easier backup of presets and settings  

## 💡 Supported light control interfaces
- WLED app for [Android](https://play.google.com/store/apps/details?id=ca.cgagnier.wlednativeandroid) and [iOS](https://apps.apple.com/gb/app/wled-native/id6446207239)
- JSON and HTTP request APIs  
- MQTT   
- E1.31, Art-Net, DDP and TPM2.net
- [diyHue](https://github.com/diyhue/diyHue) (Wled is supported by diyHue, including Hue Sync Entertainment under udp. Thanks to [Gregory Mallios](https://github.com/gmallios))
- [Hyperion](https://github.com/hyperion-project/hyperion.ng)
- UDP realtime  
- Alexa voice control (including dimming and color)  
- Sync to Philips hue lights  
- Adalight (PC ambilight via serial) and TPM2  
- Sync color of multiple WLED devices (UDP notifier)  
- Infrared remotes (24-key RGB, receiver required)  
- Simple timers/schedules (time from NTP, timezones/DST supported)  

## 📲 Quick start guide and documentation

See the [documentation on our official site](https://kno.wled.ge)!

[On this page](https://kno.wled.ge/basics/tutorials/) you can find excellent tutorials and tools to help you get your new project up and running!

## 🖼️ User interface
<img src="/images/macbook-pro-space-gray-on-the-wooden-table.jpg" width="50%"><img src="/images/walking-with-iphone-x.jpg" width="50%">

## 💾 Compatible hardware

See [here](https://kno.wled.ge/basics/compatible-hardware)!

## ✌️ Other

Licensed under the EUPL v1.2 license  
Credits [here](https://kno.wled.ge/about/contributors/)!
CORS proxy by [Corsfix](https://corsfix.com/)

Join the Discord server to discuss everything about WLED!

<a href="https://discord.gg/QAh7wJHrRM"><img src="https://discordapp.com/api/guilds/473448917040758787/widget.png?style=banner2" width="25%"></a>

Check out the WLED [Discourse forum](https://wled.discourse.group)!  

You can also send me mails to [dev.aircoookie@gmail.com](mailto:dev.aircoookie@gmail.com), but please, only do so if you want to talk to me privately.  

If WLED really brightens up your day, you can [![](https://img.shields.io/badge/send%20me%20a%20small%20gift-paypal-blue.svg?style=flat-square)](https://paypal.me/aircoookie)


*Disclaimer:*   

If you are prone to photosensitive epilepsy, we recommended you do **not** use this software.  
If you still want to try, don't use strobe, lighting or noise modes or high effect speed settings.

As per the EUPL license, I assume no liability for any damage to you or any other person or equipment.  


---

## WLED-Lite: Hardware Modules (Usermods)

This fork ships with a curated set of usermods. Two are active by default; the rest are available and can be enabled by adding them to `custom_usermods` in `platformio.ini`.

### Active
| Usermod | What it does |
|---|---|
| `audioreactive` | Drives LED effects from microphone/line-in audio input |
| `INA219` | Monitors LED strip power draw via I2C current sensor |

### Available (not currently enabled)
| Usermod | What it does |
|---|---|
| `usermod_v2_auto_save` | Saves current state to flash on power loss — LEDs resume where they left off after a reboot |
| `usermod_v2_brightness_follow_sun` | Auto-dims LEDs at sunset and restores at sunrise using location + time |
| `Temperature` | Reads a DS18B20 one-wire temperature sensor and exposes it via the WLED info page / API |
| `rotary_encoder_change_effect` | Physical rotary encoder to cycle through effects |
| `rgb-rotary-encoder` | Physical rotary encoder to adjust RGB color |
| `usermod_rotary_brightness_color` | Physical rotary encoder for brightness and color control |
| `usermod_v2_rotary_encoder_ui_ALT` | Alternative rotary encoder UI with mode switching |

### More usermods from upstream WLED
The full WLED project maintains 60+ additional usermods covering sensors, displays, integrations, and effects. When adding new hardware or functionality, check the upstream repo first — there may already be a module for it:

**https://github.com/wled-dev/WLED/tree/main/usermods**

Notable ones worth evaluating when the time comes:
- **Animated_Staircase** — motion-triggered step-by-step staircase lighting
- **PIR_sensor_switch** — motion-activated on/off
- **BME280_v2 / SHT** — temperature + humidity sensors
- **RTC** — real-time clock for time-based automations
- **Multi_relay** — control external relays from WLED
- **usermod_v2_four_line_display_ALT** — OLED/LCD display for status info
