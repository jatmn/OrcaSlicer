# UltiMaker LAN Scanning Implementation Plan

## Problem Statement
The "UltiMaker - LAN" host connection type's browse button doesn't work for scanning UltiMaker printers on the local network. Currently, it uses the generic `BonjourDialog` which scans for "octoprint" service, not UltiMaker printers.

## Analysis Findings

### Current Implementation
1. **PhysicalPrinterDialog.cpp** line 212: Always creates `BonjourDialog` with service "octoprint"
2. **UltiMakerLAN.hpp**: Has `has_auto_discovery() const override { return true; }` but no actual discovery
3. **Bonjour infrastructure**: OrcaSlicer already has mDNS/Bonjour implementation in `src/slic3r/Utils/Bonjour.cpp`

### Cura Reference Implementation
1. **Service name**: `"_ultimaker._tcp.local."` (from Cura's ZeroConfClient.py line 22)
2. **Filtering**: Checks TXT record `type=b"printer"`
3. **Properties**: Includes model, version, and other printer metadata

## Solution Design

### 1. Create UltiMakerDialog Class
- Location: `src/slic3r/GUI/UltiMakerDialog.hpp/.cpp`
- Extends or reuses `BonjourDialog` functionality
- Key changes:
  - Service: `"_ultimaker._tcp.local."` instead of "octoprint"
  - TXT keys: `["type", "model", "version", "firmware"]`
  - Filtering: Only show devices with `type="printer"`
  - UI: Display UltiMaker-specific information

### 2. Modify PhysicalPrinterDialog
- Location: `src/slic3r/GUI/PhysicalPrinterDialog.cpp` lines 211-220
- Add host type check before creating dialog:
```cpp
if (m_printhost_port->GetValue() == "htUltiMakerLAN") {
    UltiMakerDialog dialog(this, Preset::printer_technology(*m_config));
    if (dialog.show_and_lookup()) {
        m_printhost_host->SetValue(dialog.get_selected());
    }
} else {
    BonjourDialog dialog(this, Preset::printer_technology(*m_config));
    if (dialog.show_and_lookup()) {
        m_printhost_host->SetValue(dialog.get_selected());
    }
}
```

### 3. Add Discovery Method to UltiMakerLAN
- Location: `src/slic3r/Utils/UltiMakerLAN.cpp`
- Add static method for scanning:
```cpp
static std::vector<std::string> discover_printers();
```
- Implementation reuses `Bonjour` class with UltiMaker service

### 4. Update Build System
- Add `UltiMakerDialog.cpp` to CMakeLists.txt
- Ensure proper includes and dependencies

## Implementation Steps

### Phase 1: Create UltiMakerDialog
1. Copy `BonjourDialog.cpp/.hpp` to `UltiMakerDialog.cpp/.hpp`
2. Modify service name to `"_ultimaker._tcp.local."`
3. Update TXT keys and filtering logic
4. Update UI strings for UltiMaker context

### Phase 2: Modify PhysicalPrinterDialog
1. Add include for `UltiMakerDialog.hpp`
2. Modify browse button handler to check host type
3. Test that other host types still use BonjourDialog

### Phase 3: Enhance UltiMakerLAN Discovery
1. Add static discovery method
2. Optionally integrate with dialog for direct API access

### Phase 4: Testing
1. Build and test with UltiMaker printer on network
2. Verify other host types unaffected
3. Test fallback behavior

## Files to Modify

### New Files
- `src/slic3r/GUI/UltiMakerDialog.hpp`
- `src/slic3r/GUI/UltiMakerDialog.cpp`

### Modified Files
- `src/slic3r/GUI/PhysicalPrinterDialog.cpp`
- `src/slic3r/Utils/UltiMakerLAN.cpp` (optional)
- `src/slic3r/Utils/UltiMakerLAN.hpp` (optional)
- `CMakeLists.txt` (add new source file)

## Success Criteria
1. Browse button for "UltiMaker - LAN" shows UltiMaker printers on network
2. Other host types (OctoPrint, SL1) continue to work as before
3. No regression in existing functionality
4. Proper error handling for network issues

## Notes
- The existing Bonjour infrastructure supports multiple service types
- Cura's implementation shows we need to filter by `type="printer"`
- UltiMaker printers use `/cluster-api/v1/` endpoints (newer) or `/api/v1/` (older)
- The dialog should display printer model and firmware version from TXT records