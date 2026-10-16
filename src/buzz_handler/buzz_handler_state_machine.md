# State Machine for the buzz handler

Near, Imminent and At buzz on entry, i.e., only when moving closer to the turn. "Turn changed" means
that the turn symbol changed, or that the distance suddenly jumped up (a reroute, or the next turn
after passing one). "Moving away" means that the distance grew by a margin from the closest seen in
the state.

```mermaid
stateDiagram-v2

[*] --> NoNavigation

NoNavigation --> NewTurn : navigation started

NewTurn --> Far : distance >= 100m
NewTurn --> Near : distance < 100m
NewTurn --> Imminent : distance < 20m
NewTurn --> At : distance <= 10m

Far --> NoNavigation : navigation ended
Far --> NewTurn : turn changed
Far --> Near : distance < 100m
Far --> Imminent : distance < 20m
Far --> At : distance <= 10m

Near --> NoNavigation : navigation ended
Near --> NewTurn : turn changed
Near --> Receding : moving away
Near --> Imminent : distance < 20m
Near --> At : distance <= 10m

Imminent --> NoNavigation : navigation ended
Imminent --> NewTurn : turn changed
Imminent --> Receding : moving away
Imminent --> At : distance <= 10m

At --> NoNavigation : navigation ended
At --> NewTurn : turn changed
At --> Receding : moving away

Receding --> NoNavigation : navigation ended
Receding --> NewTurn : turn changed
Receding --> Far : distance > 120m
Receding --> Near : moving closer, distance < 100m
Receding --> Imminent : moving closer, distance < 20m
Receding --> At : moving closer, distance <= 10m
```
