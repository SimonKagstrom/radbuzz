## Setup for the mock script
```
pip3 install manifold3d trimesh matplotlib
```

## Generate a step file for import into onshape

```
kicad-cli pcb export step --subst-models \
    --component-filter "J1,J2,J3,J4,J5,J6,U2,C3,C14,C15,C16,C17" \
    --user-origin 42.775x113.55mm \
    -f -o radbuzz.step radbuzz.kicad_pcb
```

The component filter has no way to exclude a part, so the parts to include are
listed. J9 is left out, since it is the GPS module's header.

For the encoder board:

```
kicad-cli pcb export step --subst-models \
    --user-origin 36.25x76mm \
    -f -o encoder_board.step encoder_board/encoder_board.kicad_pcb
```

KiCad has no 3D model for the EC11E encoder, so the step file has only the
board and its connector, and kicad-cli warns about the missing model.

The user origin is the lower left corner of each board.
