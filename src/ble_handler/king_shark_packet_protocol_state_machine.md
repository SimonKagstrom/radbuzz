# King Shark packet protocol

Packets from the King Shark BMS look like

| 0x3a 0x16 | command | length | data (length bytes) | checksum (2 bytes, LE) | 0x0d 0x0a |
|-----------|---------|--------|---------------------|------------------------|-----------|

where the checksum is the 16-bit sum of the bytes from 0x16 to the end of the data.

The receive state machine is shown below. Each state waits for a byte: `Evaluate()` peeks at
it to choose the transition, and `Exit()` consumes it. On errors, the state machine resyncs
on the next header, without missing a header starting at the failing byte.

`Complete` holds the received packet until it has been returned from `Poll()`.

```mermaid
stateDiagram-v2
    [*] --> Header0

    Header0 --> Header1 : 0x3a
    Header0 --> Header0 : Other byte

    Header1 --> Command : 0x16
    Header1 --> Header1 : 0x3a
    Header1 --> Header0 : Other byte

    Command --> Length

    Length --> Data : Length > 0
    Length --> Checksum0 : Length == 0

    Data --> Data : More data
    Data --> Checksum0 : Last data byte

    Checksum0 --> Checksum1
    Checksum1 --> Footer0

    Footer0 --> Footer1 : 0x0d
    Footer0 --> Header1 : 0x3a
    Footer0 --> Header0 : Other byte

    Footer1 --> Complete : 0x0a, valid checksum
    Footer1 --> Header1 : 0x3a
    Footer1 --> Header0 : Other byte

    Complete --> Header0 : Packet returned
```
