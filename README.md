# :airplane: CYD Flight Radar :dart:
[![task](https://img.shields.io/badge/Task-Enabled-brightgreen?style=for-the-badge&logo=task&logoColor=white)](https://taskfile.dev/#/)

A real-time flight radar for the ESP32 Cheap Yellow Display (CYD) using the adsb.fi API.

> [!WARNING]
> This project is currently in a `v0.X.X` development stage. Features and configurations are subject to change, and breaking changes may be introduced at any time.

## :star: Features
* **Real-time ADS-B Data**: Fetches live flight data using the open [adsb.fi](https://adsb.fi/) API.
* **Animated Radar Sweep**: Features a smooth 20 FPS rotating radar sweep (configurable in `config/config.h`).
* **Local Time Clock**: Displays the current local time via background NTP synchronization using POSIX timezone formats.
* **Interactive Display**: Tap on any aircraft to view detailed information including its registration, model, altitude, speed, and distance from your location. Tapping the range scale label on the bottom right of the radar cycles through radar ranges.
* **Hardware Controls**: Use the physical **BOOT button** on the CYD:
  * **Short Press (Screen On)**: Cycle through radar distances.
  * **Long Press (Screen On)**: Turn the screen off and pause rendering to save power.
  * **Short Press (Screen Off)**: Wake the display back up.
* **On-Device Settings Menu**: Long-press the touch screen to bring up an LVGL-powered settings overlay. Configure preferences like radar sweep speed, radar radius, altitude limits, theme, and the maximum number of aircraft to track dynamically.
* **Smart Symbols**: Automatically detects rotorcraft and displays them with a dedicated helicopter symbol and distinct color.
* **Military Aircraft Detection**: Highlights military planes in a distinct peach color with a `[MIL]` tag.
* **Accurate Projections**: Uses latitude-corrected scaling so east-west distances aren't distorted on the radar.
* **Connection Monitoring**: Automatically detects stale data and displays a warning banner if the connection to the ADS-B API is lost.
* **Memory Optimized**: Employs zero-copy JSON parsing from local buffers to prevent `ArduinoJson` out-of-memory crashes. Smart distance-based aircraft eviction guarantees the closest planes are always shown, even in busy airspace.
* **Easy Setup**: Built-in Wi-Fi captive portal (broadcasts as `PlaneRadar-Setup-XXXX`) for configuring network credentials and radar coordinates.

## ⚙️ Configuration

There are two ways to configure the Wi-Fi credentials for the radar:

1. **Captive Portal**: On first boot, the device will host a Wi-Fi setup portal. Connect to the setup network to enter your network credentials and radar coordinates.
2. **Hardcoded Credentials**: If you prefer to bake the credentials into the firmware, copy `config/secrets.h.example` to `config/secrets.h` and update it with your `WIFI_SSID` and `WIFI_PASSWORD`.

## :hammer_and_wrench: Hardware Supported
Currently configured and tested on:
* `cyd_28r`: Standard 2.8" ESP32 CYD with resistive touch (XPT2046).
* `cyd_35c`: Standard 3.5" ESP32 CYD with capacitive touch (GT911).

## 🙌 Credits
* This project is heavily based on and ported from [MatixYo's ESP32-Plane-Radar](https://github.com/MatixYo/ESP32-Plane-Radar), which was originally designed for the ESP32-C3 Super Mini and round GC9A01 displays. It has been adapted here to run on various ESP32 Cheap Yellow Display (CYD) boards with touchscreen support.
* Additional inspiration and ideas drawn from Adam Conway's [XDA Developers article on building an ESP32 Plane Radar](https://www.xda-developers.com/built-own-plane-radar-esp32-display-havent-opened-flightradar24/).

## :balance_scale: License

[Apache License 2.0](LICENSE)

<details>
<summary>Third-Party Licenses</summary>

This project is heavily based on and ported from [MatixYo/ESP32-Plane-Radar](https://github.com/MatixYo/ESP32-Plane-Radar), which is licensed under the MIT License:

```text
MIT License

Copyright (c) 2026 MatixYo

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

</details>

## :writing_hand: Author

This project was started in 2026 by [Nicholas Wilde](https://github.com/nicholaswilde/).
