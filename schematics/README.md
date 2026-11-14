## Setup for the mock script
```
pip3 install manifold3d trimesh matplotlib
```

## Generate a step file for import into onshape

```
kicad-cli pcb export step --subst-models \
    --component-filter "J*,U2,C3" \
    --user-origin 42.775x113.55mm \
    -f -o radbuzz.step radbuzz.kicad_pcb
```