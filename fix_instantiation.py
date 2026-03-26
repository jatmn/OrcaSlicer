#!/usr/bin/env python3
import json
import os
from pathlib import Path

# Directories to process
dirs_to_process = [
    "resources/profiles/UltiMaker/machine",
    "resources/profiles/MakerBot/machine"
]

files_fixed = 0

for directory in dirs_to_process:
    machine_dir = Path(directory)
    
    for json_file in machine_dir.glob("*.json"):
        try:
            with open(json_file, 'r') as f:
                data = json.load(f)
            
            # Only process files with type "machine"
            if data.get("type") == "machine":
                # Check if instantiation field is missing
                if "instantiation" not in data:
                    # Determine the value based on whether the file inherits from a common base
                    # Base common files should have "false", variants should have "true"
                    if "fdm_" in json_file.name or "common" in json_file.name.lower():
                        data["instantiation"] = "false"
                    else:
                        data["instantiation"] = "true"
                    
                    # Write back
                    with open(json_file, 'w') as f:
                        json.dump(data, f, indent=2)
                    
                    files_fixed += 1
                    print(f"✓ Fixed: {json_file.name}")
        except json.JSONDecodeError as e:
            print(f"✗ JSON Error in {json_file.name}: {e}")
        except Exception as e:
            print(f"✗ Error processing {json_file.name}: {e}")

print(f"\nTotal files fixed: {files_fixed}")
