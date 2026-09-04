# Radbuzz
Radbuzz is an infotainment and trip computer for electric mopeds and motorcycles, targeting VESC-based
controllers. It's built for an ESP32P4 microcontroller, but can also be run on the desktop for testing.

<p align="center">
  <a href="doc/on_the_moped.jpg"><img src="doc/on_the_moped.jpg" alt="how it looks mounted on the moped" width="31%"></a>
  <a href="doc/default_zoom.png"><img src="doc/default_zoom.png" alt="the default map screen" width="31%"></a>
  <a href="doc/zoomed_out.png"><img src="doc/zoomed_out.png" alt="zoomed out" width="31%"></a>
</p>
<p align="center">
  <a href="doc/city_zoom.png"><img src="doc/city_zoom.png" alt="maximum zoom with range" width="31%"></a>
  <a href="doc/help_bubbles.png"><img src="doc/help_bubbles.png" alt="speech bubbles with help text" width="31%"></a>
  <a href="doc/speedometer_only.png"><img src="doc/speedometer_only.png" alt="speedometer only screen" width="31%"></a>
</p>
<p align="center">
  <a href="doc/trip_screen.png"><img src="doc/trip_screen.png" alt="trip meter screen" width="31%"></a>
  <a href="doc/menu.png"><img src="doc/menu.png" alt="menu screen" width="31%"></a>
  <a href="doc/menu_settings.png"><img src="doc/menu_settings.png" alt="settings menu screen" width="31%"></a>
</p>

Features:

* OpenStreetMap-based map (currently OpenCycleMap) with different zoom levels, via a GPS module
* Speedometer, both based on VESC data and GPS
* Trip data, with average consumption, distance etc
* Tesla-style power meter bar on the right
* Supports receiving navigation instructions from Google maps via an android app
* Trip log shown on the map, with power indicator in color
* Range estimation shown on the map
* A menu for configuration
* Written in clean and modern C++ (C++23)

Hardware:

* [Waveshare 4.3" ESP32P4 Touch](https://www.waveshare.com/product/arduino/boards-kits/esp32-p4/esp32-p4-wifi6-touch-lcd-4.3.htm)
* A rotary encoder
* UART-based GPS (UBLOX Neo-6M / Neo-7M tested)
* A CAN-bus adapter to communicate with the VESC (a Waveshare SN65HVD230 has been used)
* A DC/DC converter to power the ESP32P4 from the 12V outlet on the VESC (a [Waveshare DC-DC buck mini module](https://www.waveshare.com/dc5-36-to-dc3v3-5.htm) used here)

the pinout for the ESP32P4 can be seen in [waveshare_p4_touch_4_3/main.cc](esp32/waveshare_p4_touch_4_3/main/main.cc)

## Setup
First install the `eim` tool (the esp-idf installation manager).

```
eim install -i v6.1
source $HOME/.espressif/tools/activate_idf_v6.1.sh
npm i lv_font_conv -g
pip3 install jinja2 pyyaml
```

## Build setup (target)
```
cmake -GNinja -B radbuzz_esp32p4 -DCMAKE_BUILD_TYPE=Release <path>/radbuzz/esp32/waveshare_p4_touch_4_3
cmake --build radbuzz_esp32p4
```

## Build setup (qt/unittest)
Install the same lvgl/Python dependencies as on target:

```
npm i lv_font_conv -g
pip3 install jinja2 pyyaml
```

And then:

```
cmake -GNinja -B radbuzz_qt <path>/radbuzz/qt
cmake --build radbuzz_qt
```

or

```
cmake -GNinja -B radbuzz_unittest <path>/radbuzz/test/unittest/
cmake --build radbuzz_unittest
```

See [doc/build_instructions.md](doc/build_instructions.md) for more details on building and flashing the firmware.

## The OSM API key
Get an API key for thunderforest via https://www.thunderforest.com/docs/apikeys/

Put this key as a string in a `/APP_DATA/OSM_KEY.TXT` file in the root directory of this project.

A key can also be placed on the SD card, as osm_key.txt, which will be
preferred over the build time one if it exists.

## Wifi SSID
Store the SSIDs and passwords in a `/APP_DATA/SSID.TXT` file on the SD card, with
newlines. E.g.,

```
MySsid
PasswordForMySsid
OtherSsid
PasswordForOtherSsid
```
