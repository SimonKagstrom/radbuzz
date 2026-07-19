# Radbuzz
Bicycle

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

# Build and flash the esp32c6 fw if bricked

```
cd esp32/waveshare_p4_touch_4_3/managed_components/espressif__esp_hosted/slave
idf.py set-target esp32c6
idf.py build

esptool -b 1500000 --before no_reset --after no_reset -p /dev/tty.SLAB_USBtoUART write_flash \
  0x0000 ./bootloader/bootloader.bin \
  0x8000 ./partition_table/partition-table.bin \
  0x10000 ./network_adapter_esp32c6.bin
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
