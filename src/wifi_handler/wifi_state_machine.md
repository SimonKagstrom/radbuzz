# Wifi state machine

The wifi handler connects to known networks (from `SSID.TXT` or the configuration) when the
moped is standing still, and turns off wifi when moving without a connection. An existing
connection is kept while moving, although it's expected to be lost soon.

```mermaid
stateDiagram-v2
    [*] --> On

    On --> Scanning : Scan

    Scanning --> Connect : Matching SSID found
    Scanning --> Idle : No known networks found
    Scanning --> Off : Moving (1 min)

    Idle --> Scanning : After 10 seconds

    Connect --> Connected : Connected
    Connect --> RetryConnect : Connection failed, retries left
    Connect --> Scanning : Connection failed, no retries left

    RetryConnect --> Connect : After 5 seconds

    Connected --> LostConnection : Lost connection

    LostConnection --> Scanning : Not moving (1 min)
    LostConnection --> Off : Moving (1 min)

    Off --> On : Not moving (1 min)

    classDef off fill:orange
    classDef connected fill:lightgreen
    class Off off
    class Connected connected
```

## States

- **On**: Turn on wifi
- **Off**: Turn off wifi
- **Scanning**: Scan for networks
- **Idle**: Wait for 10 seconds before scanning again
- **Connect**: Connect to the matching network
- **RetryConnect**: Wait 5 seconds before retrying the connection
- **Connected**: Connected to a network
- **LostConnection**: The connection was lost
