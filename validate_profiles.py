#!/usr/bin/env python
import json
import os

def validate_json_files(directory):
    errors = []
    valid = 0
    
    for root, dirs, files in os.walk(directory):
        for file in files:
            if file.endswith('.json'):
                filepath = os.path.join(root, file)
                try:
                    with open(filepath, 'r', encoding='utf-8') as f:
                        data = json.load(f)
                    
                    # Check for instantiation field in machine/filament files
                    if 'machine' in root.lower() or 'filament' in root.lower():
                        if 'type' in data and data['type'] in ['machine', 'filament']:
                            if 'instantiation' not in data:
                                errors.append(f'MISSING: {filepath}')
                            elif data['instantiation'] not in ['true', 'false']:
                                errors.append(f'INVALID VALUE: {filepath} has instantiation={data["instantiation"]}')
                            else:
                                valid += 1
                        else:
                            valid += 1
                    else:
                        valid += 1
                        
                except json.JSONDecodeError as e:
                    errors.append(f'JSON ERROR: {filepath} - {str(e)}')
                except Exception as e:
                    errors.append(f'ERROR: {filepath} - {str(e)}')
    
    return valid, errors

# Validate UltiMaker files
print('=== VALIDATING ULTIMAKER FILES ===')
valid, errors = validate_json_files('resources/profiles/UltiMaker')
print(f'Valid files: {valid}')
if errors:
    print(f'ERRORS ({len(errors)}):')
    for err in errors[:20]:
        print(f'  {err}')
else:
    print('All files valid!')

# Validate MakerBot files
print('\n=== VALIDATING MAKERBOT FILES ===')
valid, errors = validate_json_files('resources/profiles/MakerBot')
print(f'Valid files: {valid}')
if errors:
    print(f'ERRORS ({len(errors)}):')
    for err in errors[:20]:
        print(f'  {err}')
else:
    print('All files valid!')

# Validate OrcaFilamentLibrary
print('\n=== VALIDATING ORCA FILAMENT LIBRARY FILES ===')
valid, errors = validate_json_files('resources/profiles/OrcaFilamentLibrary')
print(f'Valid files: {valid}')
if errors:
    print(f'ERRORS ({len(errors)}):')
    for err in errors[:20]:
        print(f'  {err}')
else:
    print('All files valid!')

print('\n=== VALIDATION COMPLETE ===')
