# Radbuzz
Bicycle

## Setup
```
npm i lv_font_conv -g
```

## Build setup (target)
```
cmake -GNinja -B radbuzz_esp32p4 -DCMAKE_BUILD_TYPE=Release ~/projects/radbuzz/esp32/esp32p4/
```

## The OSM API key
Get an API key for thunderforest via https://www.thunderforest.com/docs/apikeys/

Put this key as a string in a `osm_api_key.txt` file in the root directory of this
project.

## Wifi SSID
Store the SSID and password in a `/APP_DATA/SSID.TXT` file on the SD card, with
newlines. E.g.,

```
MySsid
MyPasswordForMySsid
```
