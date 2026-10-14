# State Machine for the buzz handler

```mermaid
stateDiagram-v2

[*] --> NoNavigation

NoNavigation --> NewTurn : new icon
NewTurn --> Far : distance >= 100m

Far --> Near : distance < 100m
Far --> Imminent : distance < 20m
Far --> At : distance < 20m

Near --> Imminent : distance < 20m
Near --> At : distance < 20m

Imminent --> At : distance < 5m

At --> NewTurn : new icon
```
