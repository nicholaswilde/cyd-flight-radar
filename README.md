# ✈️ cCYD Fight- Rdar

A real-time flight radar for the ESP32 Cheap Yellow Display (CYD) using the adsb.fi API.

## ✨ Features
* **Real-time ADS-B Data**: Fetches live flight data using the open [adsb.fi](https://adsb.fi/) API.
* **Touchscreen Support**: Tap on any aircraft on the screen to view detailed information including its registration, model, altitude, speed, and distance from your location. Tapping an empty space zooms the radar range.
* **Aesthetics**: Fully themed using the gorgeous [Catppuccin Mocha](https://github.com/catppuccin/catppuccin) color palette.
* **Smart Symbols**: Automatically detects rotorcraft and displays them with a dedicated helicopter symbol and distinct color.

## 🚀 Hardware Supported
Currently configured and tested on:
* `cyd_28r`: Standard 2.8" ESP32 CYD with resistive touch (XPT2046).
* `cyd_35c`: Standard 3.5" ESP32 CYD with capacitive touch (GT911).

## 🙌 Credits
This project is heavily based on and ported from [MatixYo's ESP32-Plane-Radar](https://github.com/MatixYo/ESP32-Plane-Radar), which was originally designed for the ESP32-C3 Super Mini and round GC9A01 displays. It has been adapted here to run on various ESP32 Cheap Yellow Display (CYD) boards with touchscreen support.
