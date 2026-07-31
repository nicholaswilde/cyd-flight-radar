# :airplane: CYD Fight Radar :dart:
[![task](https://img.shields.io/badge/Task-Enabled-brightgreen?style=for-the-badge&logo=task&logoColor=white)](https://taskfile.dev/#/)

A real-time flight radar for the ESP32 Cheap Yellow Display (CYD) using the adsb.fi API.

> [!WARNING]
> This project is currently in a `v0.X.X` development stage. Features and configurations are subject to change, and breaking changes may be introduced at any time.

## :star: Features
* **Real-time ADS-B Data**: Fetches live flight data using the open [adsb.fi](https://adsb.fi/) API.
* **Touchscreen Support**: Tap on any aircraft on the screen to view detailed information including its registration, model, altitude, speed, and distance from your location. Tapping an empty space zooms the radar range.
* **Aesthetics**: Fully themed using the gorgeous [Catppuccin Mocha](https://github.com/catppuccin/catppuccin) color palette.
* **Smart Symbols**: Automatically detects rotorcraft and displays them with a dedicated helicopter symbol and distinct color.
* **Easy Setup**: Built-in Wi-Fi captive portal for configuring network credentials, radar coordinates, and display options—all matching the Catppuccin Mocha visual aesthetic.

## ⚙️ Configuration

There are two ways to configure the Wi-Fi credentials for the radar:

1. **Captive Portal**: On first boot, the device will host a Wi-Fi setup portal. Connect to the setup network to enter your network credentials and radar coordinates.
2. **Hardcoded Credentials**: If you prefer to bake the credentials into the firmware, copy `include/secrets.h.example` to `include/secrets.h` and update it with your `WIFI_SSID` and `WIFI_PASSWORD`.

## :hammer_and_wrench: Hardware Supported
Currently configured and tested on:
* `cyd_28r`: Standard 2.8" ESP32 CYD with resistive touch (XPT2046).
* `cyd_35c`: Standard 3.5" ESP32 CYD with capacitive touch (GT911).

## 🙌 Credits
This project is heavily based on and ported from [MatixYo's ESP32-Plane-Radar](https://github.com/MatixYo/ESP32-Plane-Radar), which was originally designed for the ESP32-C3 Super Mini and round GC9A01 displays. It has been adapted here to run on various ESP32 Cheap Yellow Display (CYD) boards with touchscreen support.

## :balance_scale: License

[Apache License 2.0](LICENSE)

## :writing_hand: Author

This project was started in 2026 by [Nicholas Wilde](https://github.com/nicholaswilde/).
