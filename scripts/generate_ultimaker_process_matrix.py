#!/usr/bin/env python3
"""Generate UltiMaker process profile matrix for dynamic core matching."""

import json
import os
import shutil
from pathlib import Path

RESOURCES = Path("resources/profiles/UltiMaker")
PROCESS_DIR = RESOURCES / "process"
MACHINE_DIR = RESOURCES / "machine"

# Family base settings
FAMILY_BASES = {
    "s35": {
        "name": "fdm_process_ultimaker_s35_common",
        "inherits": "fdm_process_common",
        "settings": {
            "bridge_acceleration": "75%",
            "default_acceleration": "10000",
            "default_jerk": "8",
            "elefant_foot_compensation": "0.15",
            "infill_jerk": "8",
            "initial_layer_acceleration": "1000",
            "initial_layer_infill_speed": "65",
            "initial_layer_jerk": "5",
            "initial_layer_speed": "55",
            "initial_layer_travel_speed": "150",
            "inner_wall_acceleration": "6000",
            "inner_wall_jerk": "8",
            "inner_wall_speed": "70",
            "outer_wall_acceleration": "3000",
            "outer_wall_jerk": "2",
            "outer_wall_speed": "55",
            "overhang_3_4_speed": "40",
            "overhang_4_4_speed": "35",
            "sparse_infill_pattern": "alignedrectilinear",
            "sparse_infill_speed": "200",
            "top_surface_acceleration": "3000",
            "top_surface_jerk": "4",
            "top_surface_speed": "60",
            "travel_acceleration": "15000",
            "travel_jerk": "8",
            "travel_speed": "200",
            "wall_loops": "2",
        },
    },
    "factor4": {
        "name": "fdm_process_ultimaker_factor4_common",
        "inherits": "fdm_process_common",
        "settings": {
            "bridge_acceleration": "75%",
            "default_acceleration": "10000",
            "default_jerk": "8",
            "elefant_foot_compensation": "0.15",
            "infill_jerk": "8",
            "initial_layer_acceleration": "1000",
            "initial_layer_infill_speed": "65",
            "initial_layer_jerk": "5",
            "initial_layer_speed": "55",
            "initial_layer_travel_speed": "150",
            "inner_wall_acceleration": "6000",
            "inner_wall_jerk": "8",
            "inner_wall_speed": "70",
            "outer_wall_acceleration": "3000",
            "outer_wall_jerk": "2",
            "outer_wall_speed": "55",
            "overhang_3_4_speed": "40",
            "overhang_4_4_speed": "35",
            "sparse_infill_pattern": "alignedrectilinear",
            "sparse_infill_speed": "200",
            "top_surface_acceleration": "3000",
            "top_surface_jerk": "4",
            "top_surface_speed": "60",
            "travel_acceleration": "15000",
            "travel_jerk": "8",
            "travel_speed": "200",
            "wall_loops": "2",
        },
    },
}

# Core variants per family (from ExtruderVariantWidget)
FAMILIES = {
    "s35": {
        "ids": ["ultimaker_s3", "ultimaker_s5", "ultimaker_s7"],
        "cores": [
            ("AA", "0.25"),
            ("AA", "0.4"),
            ("BB", "0.4"),
            ("CC", "0.4"),
            ("CC", "0.6"),
            ("BB", "0.8"),
            ("AA", "0.8"),
        ],
    },
    "s68": {
        "ids": ["ultimaker_s6", "ultimaker_s8"],
        "cores": [
            ("AA", "0.25"),
            ("AA", "0.4"),
            ("BB", "0.4"),
            ("CC", "0.4"),
            ("AA+", "0.4"),
            ("CC+", "0.4"),
            ("CC", "0.6"),
            ("CC+", "0.6"),
            ("CC Red", "0.6"),
            ("HT", "0.6"),
            ("BB", "0.8"),
            ("AA", "0.8"),
        ],
    },
    "factor4": {
        "ids": ["ultimaker_factor4"],
        "cores": [
            ("AA", "0.25"),
            ("AA", "0.4"),
            ("BB", "0.4"),
            ("CC", "0.4"),
            ("CC", "0.6"),
            ("HT", "0.6"),
            ("BB", "0.8"),
            ("AA", "0.8"),
        ],
    },
}

# Mapping of display family name -> machine family suffix for profile names
FAMILY_DISPLAY = {
    "s35": "S3-S5-S7",
    "s68": "S6-S8",
    "factor4": "Factor 4",
}

# Layer height per nozzle size
LAYER_HEIGHTS = {
    "0.25": "0.15",
    "0.4": "0.20",
    "0.6": "0.20",
    "0.8": "0.20",
}

# Nozzle-size-specific sparse infill line widths
NOZZLE_LINE_WIDTHS = {
    "0.25": "0.3",
    "0.4": "0.5",
    "0.6": "0.6",
    "0.8": "0.8",
}

# Default print profile per machine+nozzle (most common core)
DEFAULT_PRINT_PROFILES = {
    "s35": {
        "0.25": "0.15mm Standard @UltiMaker S3-S5-S7 AA 0.25",
        "0.4": "0.20mm Standard @UltiMaker S3-S5-S7 AA 0.4",
        "0.6": "0.20mm Standard @UltiMaker S3-S5-S7 CC 0.6",
        "0.8": "0.20mm Standard @UltiMaker S3-S5-S7 AA 0.8",
    },
    "s68": {
        "0.25": "0.15mm Standard @UltiMaker S6-S8 AA 0.25",
        "0.4": "0.20mm Standard @UltiMaker S6-S8 AA 0.4",
        "0.6": "0.20mm Standard @UltiMaker S6-S8 CC 0.6",
        "0.8": "0.20mm Standard @UltiMaker S6-S8 AA 0.8",
    },
    "factor4": {
        "0.25": "0.15mm Standard @UltiMaker Factor 4 AA 0.25",
        "0.4": "0.20mm Standard @UltiMaker Factor 4 AA 0.4",
        "0.6": "0.20mm Standard @UltiMaker Factor 4 CC 0.6",
        "0.8": "0.20mm Standard @UltiMaker Factor 4 AA 0.8",
    },
}

# Machine models per family
MACHINE_MODELS = {
    "s35": ["UltiMaker S3", "UltiMaker S5", "UltiMaker S7"],
    "s68": ["UltiMaker S6", "UltiMaker S8"],
    "factor4": ["UltiMaker Factor 4"],
}


def write_json(path, data):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=4, ensure_ascii=False)
        f.write("\n")


def create_family_bases():
    for key, base in FAMILY_BASES.items():
        path = PROCESS_DIR / f"{base['name']}.json"
        data = {
            "type": "process",
            "name": base["name"],
            "inherits": base["inherits"],
            "from": "system",
            "instantiation": "false",
        }
        data.update(base["settings"])
        write_json(path, data)
        print(f"Created base: {path}")


def create_standard_profiles():
    created = []
    for family, info in FAMILIES.items():
        display = FAMILY_DISPLAY[family]
        base_name = f"fdm_process_ultimaker_{family}_common"
        notes_conditions = " || ".join(f"printer_notes=~/.*{fid}.*/" for fid in info["ids"])

        for core, nozzle in info["cores"]:
            layer_h = LAYER_HEIGHTS[nozzle]
            profile_name = f"{layer_h}mm Standard @UltiMaker {display} {core} {nozzle}"
            variant_str = f"{core} {nozzle}"
            condition = (
                f'printer_variant=="{nozzle}" && printer_extruder_variant_0=="{variant_str}" && ({notes_conditions})'
            )

            data = {
                "type": "process",
                "name": profile_name,
                "inherits": base_name,
                "from": "system",
                "instantiation": "true",
                "print_settings_id": profile_name,
                "compatible_printers_condition": condition,
                "layer_height": layer_h,
            }

            # Add nozzle-specific line width if it's not the 0.4 default
            line_width = NOZZLE_LINE_WIDTHS[nozzle]
            if line_width != "0.5":
                data["sparse_infill_line_width"] = line_width

            path = PROCESS_DIR / f"{profile_name}.json"
            write_json(path, data)
            created.append(profile_name)
            print(f"Created profile: {path}")

    return created


def update_machine_profiles():
    for family, models in MACHINE_MODELS.items():
        defaults = DEFAULT_PRINT_PROFILES[family]
        for model in models:
            for nozzle in ["0.25", "0.4", "0.6", "0.8"]:
                filename = f"{model} {nozzle} nozzle.json"
                path = MACHINE_DIR / filename
                if not path.exists():
                    print(f"WARNING: Machine profile not found: {path}")
                    continue

                with open(path, "r", encoding="utf-8") as f:
                    data = json.load(f)

                profile_name = defaults[nozzle]
                data["default_print_profile"] = profile_name

                # Parse core+nozzle from profile name (e.g., "... AA 0.25" -> "AA 0.25")
                # Profile name format: "<layer>mm Standard @UltiMaker <Family> <Core> <nozzle>"
                parts = profile_name.rsplit(" ", 2)
                if len(parts) >= 3:
                    core_type = parts[-2]
                    nozzle_size = parts[-1]
                    variant = f"{core_type} {nozzle_size}"
                    data["printer_extruder_variant"] = [variant, variant]
                else:
                    print(f"WARNING: Could not parse variant from profile name: {profile_name}")

                write_json(path, data)
                print(f"Updated machine: {path} -> default_print={profile_name}, variant={data.get('printer_extruder_variant')}")


def build_manifest():
    manifest_path = RESOURCES.parent / "UltiMaker.json"
    with open(manifest_path, "r", encoding="utf-8") as f:
        manifest = json.load(f)

    # Keep entries that are NOT old UltiMaker S/Family process profiles
    keep_names = {
        # Bases
        "fdm_process_common",
        "fdm_process_ultimaker_s35_common",
        "fdm_process_ultimaker_s68_common",
        "fdm_process_ultimaker_factor4_common",
        # UltiMaker 2 profiles
        "0.12mm Fine @UltiMaker 2",
        "0.18mm Standard @UltiMaker 2",
        "0.25mm Draft @UltiMaker 2",
        # MakerBot profiles
        "0.20mm Standard @MakerBot Sketch 0.40",
        "0.20mm Standard @MakerBot Sketch Large 0.40",
        "0.20mm Standard @MakerBot Sketch Sprint 0.40",
        # 2+ Connect
        "0.20mm Standard @UltiMaker 2+ Connect",
    }

    # Also keep any new base files we've created or will create
    new_process_list = []

    # Add bases first
    for base in ["fdm_process_common", "fdm_process_ultimaker_s35_common", "fdm_process_ultimaker_s68_common", "fdm_process_ultimaker_factor4_common"]:
        new_process_list.append({
            "name": base,
            "sub_path": f"process/{base}.json"
        })

    # Add AA+ base for s68 (existing)
    new_process_list.append({
        "name": "fdm_process_ultimaker_s68_aa+04_common",
        "sub_path": "process/fdm_process_ultimaker_s68_aa+04_common.json"
    })

    # Add all Standard profiles in deterministic order
    for family in ["s35", "s68", "factor4"]:
        display = FAMILY_DISPLAY[family]
        for core, nozzle in FAMILIES[family]["cores"]:
            layer_h = LAYER_HEIGHTS[nozzle]
            name = f"{layer_h}mm Standard @UltiMaker {display} {core} {nozzle}"
            new_process_list.append({
                "name": name,
                "sub_path": f"process/{name}.json"
            })

    # Add kept legacy profiles
    legacy = [
        "0.12mm Fine @UltiMaker 2",
        "0.18mm Standard @UltiMaker 2",
        "0.25mm Draft @UltiMaker 2",
        "0.20mm Standard @UltiMaker 2+ Connect",
        "0.20mm Standard @MakerBot Sketch 0.40",
        "0.20mm Standard @MakerBot Sketch Large 0.40",
        "0.20mm Standard @MakerBot Sketch Sprint 0.40",
    ]
    for name in legacy:
        # Only add if not already in list
        if not any(p["name"] == name for p in new_process_list):
            new_process_list.append({
                "name": name,
                "sub_path": f"process/{name}.json"
            })

    manifest["process_list"] = new_process_list
    write_json(manifest_path, manifest)
    print(f"Updated manifest: {manifest_path}")


def delete_old_process_profiles():
    old_files = [
        # S3-S5-S7 old generics
        "0.15mm Standard @UltiMaker S3-S5-S7 0.25 nozzle.json",
        "0.20mm Standard @UltiMaker S3-S5-S7 0.4 nozzle.json",
        "0.20mm Standard @UltiMaker S3-S5-S7 0.6 nozzle.json",
        "0.20mm Standard @UltiMaker S3-S5-S7 0.8 nozzle.json",
        # S6-S8 old generics
        "0.15mm Standard @UltiMaker S6-S8 0.25 nozzle.json",
        "0.20mm Standard @UltiMaker S6-S8 0.6 nozzle.json",
        "0.20mm Standard @UltiMaker S6-S8 0.8 nozzle.json",
        # Factor 4 old generics
        "0.15mm Standard @UltiMaker Factor 4 0.25 nozzle.json",
        "0.20mm Standard @UltiMaker Factor 4 0.4 nozzle.json",
        "0.20mm Standard @UltiMaker Factor 4 0.6 nozzle.json",
        "0.20mm Standard @UltiMaker Factor 4 0.8 nozzle.json",
    ]
    for fname in old_files:
        path = PROCESS_DIR / fname
        if path.exists():
            path.unlink()
            print(f"Deleted old profile: {path}")
        else:
            print(f"Already deleted: {path}")


if __name__ == "__main__":
    print("Starting UltiMaker process profile matrix generation...")
    create_family_bases()
    create_standard_profiles()
    update_machine_profiles()
    delete_old_process_profiles()
    build_manifest()
    print("Done!")
