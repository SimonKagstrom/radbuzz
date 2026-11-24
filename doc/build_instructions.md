
# C6 FW
(only needed if you want to update the wifi/bt fw)

## Build the fw
The radbuzz P4 project must have been configured first, since that downloads the esp_hosted component.

The component manager writes `dependencies.lock` and `managed_components` to the project directory
even with `idf.py -B`, so copy the project to keep the original unmodified. Run from the radbuzz
root directory:
```
source $HOME/.espressif/tools/activate_idf_v6.1.sh

rm -rf ~/build/radbuzz_c6_slave
cp -R esp32/waveshare_p4_touch_4_3/managed_components/espressif__esp_hosted/slave ~/build/radbuzz_c6_slave

cd ~/build/radbuzz_c6_slave
idf.py set-target esp32c6
idf.py build
```

Redo all steps when the esp_hosted version changes.

## Flash the esp32c6 fw
Writes the fw to the c6fw partition of the P4 (via the P4 USB port, adjust the port as needed):
```
esptool -p /dev/ttyACM0 write-flash --force 0x00ff0000 ~/build/radbuzz_c6_slave/build/network_adapter.bin
```

## Flash the esp32c6 fw if bricked
Connect to the RX/TX pins of the esp32c6 and run the following command (adjust the port as needed):

```
cd ~/build/radbuzz_c6_slave/build
esptool -b 1500000 --before no-reset --after no-reset -p /dev/ttyUSB0 write-flash @flash_args
```
