# Radbuzz
Radbuzz is an infotainment / trip computer for electric mopeds and motorcycles, for VESC-based controllers.

<p align="center">
  <a href="doc/default_zoom.png"><img src="doc/default_zoom.png" alt="the default map screen" width="31%"></a>
  <a href="doc/zoomed_out.png"><img src="doc/zoomed_out.png" alt="zoomed out" width="31%"></a>
  <a href="doc/city_zoom.png"><img src="doc/city_zoom.png" alt="maximum zoom with range" width="31%"></a>
</p>
<p align="center">
  <a href="doc/trip_screen.png"><img src="doc/trip_screen.png" alt="trip meter screen" width="31%"></a>
  <a href="doc/menu.png"><img src="doc/menu.png" alt="menu screen" width="31%"></a>
</p>

Features:

* OpenStreetMap-based map (currently open cycle map) with different zoom levels
* GPS location support
* Speedometer, both based on VESC data and GPS
* Tesla-style power meter bar on the right
* Supports receiving navigation instructions from Google maps via an android app
* Range estimation shown on the map
* A menu for configuration
* Written in clean and modern C++ (C++23)

Hardware:

* [Waveshare 4.3" ESP32P4 Touch](https://www.waveshare.com/product/arduino/boards-kits/esp32-p4/esp32-p4-wifi6-touch-lcd-4.3.htm)
* A rotary encoder
* UART-based GPS
* A CAN-bus adapter to communicate with the VESC

## Setup

```
source $HOME/.espressif/tools/activate_idf_v6.0.0.sh
npm i lv_font_conv -g
pip3 install jinja2 pyyaml
```

## Build setup (target)
```
cmake -GNinja -B radbuzz_esp32p4 -DCMAKE_BUILD_TYPE=Release ~/projects/radbuzz/esp32/esp32p4/
```

## Build setup (unittest/qt)
```
cmake -GNinja -B radbuzz_unittest ~/projects/radbuzz/test/unittest/
```

or

```
cmake -GNinja -B radbuzz_qt ~/projects/radbuzz/qt
```

## The OSM API key
Get an API key for thunderforest via https://www.thunderforest.com/docs/apikeys/

Put this key as a string in a `osm_api_key.txt` file in the root directory of this
project.

## Wifi SSID
Store the SSIDs and passwords in a `/APP_DATA/SSID.TXT` file on the SD card, with
newlines. E.g.,

```
MySsid
PasswordForMySsid
OtherSsid
PasswordForOtherSsid
```

# C6 FW
(only needed if you want to update the wifi/bt fw)

## Build the fw
```
cd esp32/waveshare_p4_touch_4_3/managed_components/espressif__esp_hosted/slave
idf.py set-target esp32c6
idf.py build
```

## Flash the esp32c6 fw
```
esptool.py -p /dev/tty.usbmodem5B5E0700331 write_flash 0x00ff0000 --force build/network_adapter.bin
```

## Flash the esp32c6 fw if bricked
Connect to the RX/TX pins of the esp32c6 and run the following command:

```
esptool -b 1500000 --before no_reset --after no_reset -p /dev/tty.SLAB_USBtoUART write_flash \
  0x0000 ./bootloader/bootloader.bin \
  0x8000 ./partition_table/partition-table.bin \
  0x10000 ./network_adapter_esp32c6.bin
```
