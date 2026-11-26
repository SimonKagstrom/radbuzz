# Wifi connect state machine

Connects to one network, with retries. Used by the **Connecting** state of the
[wifi state machine](wifi_state_machine.md), which creates it with the network to connect to
on entry, and destroys it on exit. Connecting runs it on each evaluation, and acts on
**Succeeded** and **Failed**, which are final.

```mermaid
stateDiagram-v2
    [*] --> Connect

    Connect --> Succeeded : Connected
    Connect --> RetryConnect : Connection failed, retries left
    Connect --> Abort : Timeout (13 seconds), retries left
    Connect --> Failed : Connection failed or timeout, no retries left

    Abort --> RetryConnect

    RetryConnect --> Connect : After 5 seconds

    classDef failed fill:orange
    classDef succeeded fill:lightgreen
    class Failed failed
    class Succeeded succeeded
```

## States

- **Connect**: Connect to the network. Connected means having an IP address, so a timeout
  also covers e.g., DHCP not answering
- **Abort**: Stop the timed out attempt, which is still ongoing
- **RetryConnect**: Wait 5 seconds before retrying the connection (up to 3 retries)
- **Succeeded**: Connected to the network
- **Failed**: The connection failed, also after the retries. Stop the attempt, in case it
  timed out
