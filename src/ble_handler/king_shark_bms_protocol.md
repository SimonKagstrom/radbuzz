# Protocol for the em3ev King shark battery

The EM3ev Battery Analyst app is extremely helpful for disecting the protocol. Simply select
"Send debug log" from the settings page, and you'll get a dump of transmitted / received values.
Huge thanks to the developer for this!

## Ble characteristics
The 0000ffe1-0000-1000-8000-00805f9b34fb characteristic is used both for writes and notifications.

## The general protocol
All packets start with 0x3a 0x16 and end with 0x0d 0x0a. The general format is this:

B0:       0x3a
B1:       0x16
B2:       Command
B3:       Payload length, excluding the header

B4..BN-4: Payload

BN-3..2:  Checksum, from B1 through the payload. Little endian
BN-1:     0x0d
BN:       0x0a

The device answers with the same command, using the same 0x3a16, 0x0d0a. The notification is often
split in multiple 20 byte chunks, so reassembly is needed until the length and tail are received.

There seems to be no authentification, so the host can start sending any command directly.

## Commands, and their replies

P=offset into payload, i.e, starting at B4.

### 0x16: Information
Payload length: 0x01
Payload data:   0x00 (always, it seems)

Reply payload:

00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f 20 21 22 23 24 25 26 27 28 29
02 61 23 64 13 11 15 17 01 00 8E 2B 00 00 00 00 00 00 00 00 C0 C5 00 00 00 00 00 00 00 00 00 00 21 0E 1F 0E 01 06 53 31 39 32

P02:    Battery percentage, 0-100
P04:    Highest cell temperature, in °C
P05:    Lowest cell temperature, in °C
P06:    MOS temperature, in °C
P07:    Other temperature, in °C
P14-15: Battery voltage, in mV, little endian

### 0x0f: Remaining capacity
Payload length: 0x01
Payload data:   0x00 (always, it seems)

Reply payload
00 01 02 03
7D 1B 00 00

P00-01: Remaining capacity in mAh, little endian


### 0x18: Initial max capacity (?)
Payload length: 0x01
Payload data:   0x00 (always, it seems)

Reply payload
00 01 02 03
20 4E 00 00

P00-01: Initial max capacity in mAh, little endian

Read only once on startup by the app.


### 0x10: Remaining max capacity
Payload length: 0x01
Payload data:   0x00 (always, it seems)

Reply payload
00 01 02 03
20 4E 00 00

P00-01: Remaining max capacity in mAh, little endian
