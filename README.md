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
