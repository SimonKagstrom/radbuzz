
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
