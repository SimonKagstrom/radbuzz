
# C6 FW
(only needed if you want to update the wifi/bt fw)

## Build the fw
Builds out-of-tree so the managed component stays unmodified:
```
cd esp32/waveshare_p4_touch_4_3/managed_components/espressif__esp_hosted/slave
idf.py -B ~/build/radbuzz_c6_slave -DSDKCONFIG=$HOME/build/radbuzz_c6_slave/sdkconfig set-target esp32c6
idf.py -B ~/build/radbuzz_c6_slave -DSDKCONFIG=$HOME/build/radbuzz_c6_slave/sdkconfig build
```

## Flash the esp32c6 fw
Writes the fw to the c6fw partition of the P4 (via the P4 USB port):
```
esptool.py -p /dev/tty.usbmodem5B5E0700331 write_flash 0x00ff0000 --force ~/build/radbuzz_c6_slave/network_adapter.bin
```

## Flash the esp32c6 fw if bricked
Connect to the RX/TX pins of the esp32c6 and run the following command:

```
cd ~/build/radbuzz_c6_slave
esptool -b 1500000 --before no_reset --after no_reset -p /dev/tty.SLAB_USBtoUART write_flash @flash_args
```
