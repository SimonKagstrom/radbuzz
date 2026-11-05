#!/usr/bin/env python3
"""Generate a 3D-printable fit mock-up of the board.

The result is a flat plate shaped like the PCB, with one block per component,
keepout windows cut through it, and J2's holes widened for printing. It is for
checking fit against the host board and the case, not an accurate model.

Heights come from KiCad's 3D models. Parts without a model use the values in
FALLBACK_HEIGHTS, which are estimates, or a low default height.

Setup:
    python3 -m venv venv
    venv/bin/pip install trimesh manifold3d numpy scipy matplotlib

Usage, from the schematics directory:
    venv/bin/python tools/make_mockup.py radbuzz.kicad_pcb radbuzz_mockup.stl

The project is copied to a temporary directory, so nothing in it is changed.
Requires kicad-cli on PATH.
"""

import argparse
import math
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

import manifold3d as m3d
import numpy as np
import trimesh
from matplotlib.font_manager import FontProperties
from matplotlib.textpath import TextPath

# Connector on the back side. It is not drawn as a block; its holes are widened.
HEADER_REF = "J2"
HEADER_DRILL_MM = 1.3
HEADER_FOOTPRINT = "PinHeader_2x20"

# Estimated heights in mm for parts with no 3D model in the KiCad library.
FALLBACK_HEIGHTS = {"J3": 13.0, "F1": 1.8}
DEFAULT_HEIGHT = 0.6
MIN_HEIGHT = 0.6

COURTYARD_INSET = 0.3
HEIGHT_SEARCH_MARGIN = 0.05
LABEL_SIZES = (3.0, 2.2)
LABEL_DEPTH = 0.4
LABEL_MARGIN = 0.8
MIN_LABEL_BLOCK_HEIGHT = 1.0
# A keepout closer to the board edge than this runs out through the edge,
# because the remaining wall would be too thin to print.
MIN_PRINTABLE_WALL = 1.0


def parse_sexpr(text):
    """Parse a KiCad S-expression file into nested lists."""
    pos = 0

    def parse_list():
        nonlocal pos
        result = []
        pos += 1
        while True:
            while text[pos] in " \t\r\n":
                pos += 1
            char = text[pos]
            if char == "(":
                result.append(parse_list())
            elif char == ")":
                pos += 1
                return result
            elif char == '"':
                end = pos + 1
                value = []
                while text[end] != '"':
                    if text[end] == "\\":
                        end += 1
                    value.append(text[end])
                    end += 1
                result.append("".join(value))
                pos = end + 1
            else:
                end = pos
                while text[end] not in " \t\r\n()":
                    end += 1
                result.append(text[pos:end])
                pos = end

    while text[pos] != "(":
        pos += 1
    return parse_list()


def children(node, key):
    return [c for c in node if isinstance(c, list) and c and c[0] == key]


def child(node, key):
    found = children(node, key)
    return found[0] if found else None


def rotate(x, y, origin_x, origin_y, degrees):
    angle = math.radians(-degrees)
    return (
        origin_x + x * math.cos(angle) - y * math.sin(angle),
        origin_y + x * math.sin(angle) + y * math.cos(angle),
    )


def graphic_points(graphic):
    points = []
    for key in ("start", "end"):
        point = child(graphic, key)
        if point:
            points.append((float(point[1]), float(point[2])))
    polygon = child(graphic, "pts")
    if polygon:
        points += [(float(p[1]), float(p[2])) for p in children(polygon, "xy")]
    return points


def read_courtyards(board):
    """Return {reference: (x0, y0, x1, y1)} in board coordinates."""
    result = {}
    for footprint in children(board, "footprint"):
        at = child(footprint, "at")
        x, y = float(at[1]), float(at[2])
        angle = float(at[3]) if len(at) > 3 else 0.0
        reference = next(
            p[2] for p in children(footprint, "property") if p[1] == "Reference"
        )
        points = []
        for kind in ("fp_line", "fp_rect", "fp_poly"):
            for graphic in children(footprint, kind):
                layer = child(graphic, "layer")
                if layer and "CrtYd" in layer[1]:
                    points += [
                        rotate(px, py, x, y, angle)
                        for px, py in graphic_points(graphic)
                    ]
        if points:
            xs, ys = zip(*points)
            result[reference] = (min(xs), min(ys), max(xs), max(ys))
    return result


def read_pad(board, reference, number):
    """Return the board position of one pad of a footprint."""
    for footprint in children(board, "footprint"):
        references = [
            p[2] for p in children(footprint, "property") if p[1] == "Reference"
        ]
        if references != [reference]:
            continue
        at = child(footprint, "at")
        x, y = float(at[1]), float(at[2])
        angle = float(at[3]) if len(at) > 3 else 0.0
        for pad in children(footprint, "pad"):
            if pad[1] == number:
                pad_at = child(pad, "at")
                return rotate(float(pad_at[1]), float(pad_at[2]), x, y, angle)
    raise ValueError(f"No pad {reference}.{number} on the board")


def read_keepouts(board):
    """Return the bounding box of every rule area on a copper layer."""
    result = []
    for zone in children(board, "zone"):
        if not child(zone, "keepout"):
            continue
        layers = child(zone, "layers") or child(zone, "layer")
        if not any(str(name).endswith(".Cu") for name in layers[1:]):
            continue
        outline = child(child(zone, "polygon"), "pts")
        points = [(float(p[1]), float(p[2])) for p in children(outline, "xy")]
        xs, ys = zip(*points)
        result.append((min(xs), min(ys), max(xs), max(ys)))
    return result


def widen_header_holes(path):
    """Enlarge the header drills in the temporary copy of the board."""
    text = path.read_text()
    match = re.search(r"\(footprint \"[^\"]*" + HEADER_FOOTPRINT, text)
    if not match:
        raise ValueError(f"No {HEADER_FOOTPRINT} footprint on the board")
    start = match.start()
    end = text.find("\n\t(footprint ", start + 10)
    section, count = re.subn(
        r"\(drill [\d.]+\)", f"(drill {HEADER_DRILL_MM})", text[start:end]
    )
    if not count:
        raise ValueError("Header footprint has no drills")
    path.write_text(text[:start] + section + text[end:])


def run_export(board_path, output, *flags):
    subprocess.run(
        ["kicad-cli", "pcb", "export", "stl", "--force", *flags,
         "-o", str(output), str(board_path)],
        check=True, capture_output=True, text=True,
    )
    return trimesh.load(output)


def to_manifold(mesh):
    return m3d.Manifold(
        m3d.Mesh(
            vert_properties=np.array(mesh.vertices, dtype=np.float32),
            tri_verts=np.array(mesh.faces, dtype=np.uint32),
        )
    )


def box(x0, y0, x1, y1, z0, z1):
    """Make a box from board coordinates; the STL has Y pointing the other way."""
    return m3d.Manifold.cube([x1 - x0, y1 - y0, z1 - z0]).translate([x0, -y1, z0])


def text_outline(text, center_x, center_y, size, rotated=False):
    path = TextPath((0, 0), text, size=size, prop=FontProperties(family="DejaVu Sans", weight="bold"))
    polygons = [
        np.array(p[:-1], dtype=float) for p in path.to_polygons() if len(p) > 3
    ]
    points = np.vstack(polygons)
    low, high = points.min(0), points.max(0)
    middle = (low + high) / 2
    contours = []
    for polygon in polygons:
        polygon = polygon - middle
        if rotated:
            polygon = np.stack([-polygon[:, 1], polygon[:, 0]], 1)
        contours.append(polygon + [center_x, -center_y])
    return m3d.CrossSection(contours, m3d.FillRule.EvenOdd), high - low


def component_height(vertices, courtyard, board_top, reference):
    if reference in FALLBACK_HEIGHTS:
        return FALLBACK_HEIGHTS[reference], "estimate"
    # A large flat body only has vertices at its corners, and an imported
    # footprint's courtyard can match the body exactly, so search the whole
    # courtyard. DRC guarantees that no other part overlaps it.
    x0, y0, x1, y1 = courtyard
    margin = HEIGHT_SEARCH_MARGIN
    inside = (
        (vertices[:, 0] > x0 - margin) & (vertices[:, 0] < x1 + margin)
        & (-vertices[:, 1] > y0 - margin) & (-vertices[:, 1] < y1 + margin)
        & (vertices[:, 2] > board_top + 0.05)
    )
    if not inside.any():
        return DEFAULT_HEIGHT, "default"
    return max(float(vertices[inside, 2].max() - board_top), MIN_HEIGHT), "model"


def component_block(reference, courtyard, height, board_top):
    x0, y0, x1, y1 = courtyard
    inset = COURTYARD_INSET
    block = box(x0 + inset, y0 + inset, x1 - inset, y1 - inset,
                board_top - 0.2, board_top + height)
    width, depth = x1 - x0 - 2 * inset, y1 - y0 - 2 * inset
    if height < MIN_LABEL_BLOCK_HEIGHT:
        return block
    for size in LABEL_SIZES:
        outline, extent = text_outline(
            reference, (x0 + x1) / 2, (y0 + y1) / 2, size, rotated=depth > width
        )
        if (extent[0] < max(width, depth) - LABEL_MARGIN
                and extent[1] < min(width, depth) - LABEL_MARGIN):
            cutter = outline.extrude(LABEL_DEPTH * 2)
            return block - cutter.translate([0, 0, board_top + height - LABEL_DEPTH])
    return block


def build(board_path, output_path):
    board_path = board_path.resolve()
    source = board_path.parent
    board = parse_sexpr(board_path.read_text())

    with tempfile.TemporaryDirectory() as work_name:
        work = Path(work_name)
        for name in source.iterdir():
            if name.suffix == ".kicad_pro" or name.name == "fp-lib-table":
                shutil.copy(name, work)
        for folder in ("radbuzz.pretty", "3dmodels"):
            if (source / folder).exists():
                shutil.copytree(source / folder, work / folder)
        copy = work / board_path.name
        shutil.copy(board_path, copy)
        widen_header_holes(copy)

        courtyards = read_courtyards(board)
        references = [
            r for r in courtyards if r != HEADER_REF and not r.startswith("H")
        ]
        plate_mesh = run_export(copy, work / "plate.stl", "--board-only")
        parts_mesh = run_export(
            copy, work / "parts.stl", "--subst-models",
            "--component-filter", ",".join(references),
        )

    x_min, y_neg_min, _ = plate_mesh.bounds[0]
    x_max, y_neg_max, board_top = plate_mesh.bounds[1]
    y_min, y_max = -y_neg_max, -y_neg_min
    result = to_manifold(plate_mesh)

    for x0, y0, x1, y1 in read_keepouts(board):
        if x0 - x_min < MIN_PRINTABLE_WALL:
            x0 = x_min - 1
        if x_max - x1 < MIN_PRINTABLE_WALL:
            x1 = x_max + 1
        if y0 - y_min < MIN_PRINTABLE_WALL:
            y0 = y_min - 1
        if y_max - y1 < MIN_PRINTABLE_WALL:
            y1 = y_max + 1
        result = result - box(x0, y0, x1, y1, -1, board_top + 1)

    report = []
    for reference in references:
        height, origin = component_height(
            parts_mesh.vertices, courtyards[reference], board_top, reference
        )
        report.append((reference, height, origin))
        result = result + component_block(
            reference, courtyards[reference], height, board_top
        )

    pin1_x, pin1_y = read_pad(board, HEADER_REF, "1")
    outline, _ = text_outline("PIN 1 +5V", pin1_x - 7, pin1_y - 5.5, 2.6)
    result = result - outline.extrude(1.0).translate([0, 0, board_top - 0.4])
    result = result - m3d.Manifold.cylinder(2, 1.1, 1.1, 48).translate(
        [pin1_x + 2.9, -pin1_y, board_top - 0.5]
    )

    mesh = result.to_mesh()
    final = trimesh.Trimesh(mesh.vert_properties[:, :3], mesh.tri_verts, process=True)
    if not final.is_volume:
        raise RuntimeError("The mock-up is not a closed solid")
    final.export(output_path)

    print(f"Wrote {output_path}")
    print(f"Size: {final.extents.round(2).tolist()} mm")
    for reference, height, origin in sorted(report, key=lambda r: -r[1])[:8]:
        print(f"  {reference:5} {height:5.1f} mm  {origin}")
    guessed = sorted(r for r, _, origin in report if origin != "model")
    print(f"Not from a 3D model: {', '.join(guessed)}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("board", type=Path, help="Path to the .kicad_pcb file")
    parser.add_argument("output", type=Path, help="STL file to write")
    arguments = parser.parse_args()
    build(arguments.board, arguments.output)


if __name__ == "__main__":
    main()
