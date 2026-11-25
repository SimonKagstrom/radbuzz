# Wifi state machine

The wifi handler connects to known networks (from `SSID.TXT` or the configuration) when the
moped is standing still, and turns off wifi when moving without a connection. An existing
connection is kept while moving, although it's expected to be lost soon. Without known
networks, wifi is kept off, except for requests.

The UI can request a scan (`MSG::start_wifi_scan`) or a network to connect to
(`MSG::wifi_connect`). A requested network is stored first among the known networks, so the
scan after the request picks it if it's in range.

```mermaid
stateDiagram-v2
    [*] --> LoadKnownNetworks

    LoadKnownNetworks --> On : Any known networks
    LoadKnownNetworks --> Off : No known networks

    On --> Scanning : Scan

    Scanning --> Connecting : Matching SSID found
    Scanning --> Idle : No known networks found
    Scanning --> Off : Moving (1 min)

    Idle --> Scanning : After 10 seconds, or scan/connect requested
    Idle --> Off : After 10 seconds, if moving (1 min) or no known networks

    Connecting --> Connected : Succeeded
    Connecting --> ConnectFailed : Failed
    Connecting --> Scanning : Connect requested

    ConnectFailed --> Connecting : Other known networks in the scan
    ConnectFailed --> Idle : No more networks to try

    Connected --> LostConnection : Lost connection
    Connected --> Scanning : Connect requested

    LostConnection --> Scanning : Not moving (1 min), or scan/connect requested
    LostConnection --> Off : Moving (1 min), or no known networks

    Off --> On : Not moving (1 min) with known networks, or scan/connect requested

    classDef off fill:orange
    classDef connected fill:lightgreen
    class Off off
    class Connected connected
```

## States

- **LoadKnownNetworks**: Import `SSID.TXT` to the configuration, if it has changed
- **On**: Turn on wifi
- **Off**: Turn off wifi. The result of an interrupted scan is ignored
- **Scanning**: Scan for networks. Requests during the scan are covered by it
- **Idle**: Wait for 10 seconds before scanning again, or turning off
- **Connecting**: Connect to the first known network in the scan, with retries, using the
  [wifi connect state machine](wifi_connect_state_machine.md). An ongoing attempt is
  stopped when leaving for a connect request. A scan request waits until the connection has
  succeeded or failed, since the wifi driver can't scan while connecting
- **ConnectFailed**: Skip the network until the moped has moved, or the wifi networks in the
  configuration have changed
- **Connected**: Connected to a network. A scan request scans without disconnecting, while a
  connect request disconnects and scans for the requested network
- **LostConnection**: The connection was lost
