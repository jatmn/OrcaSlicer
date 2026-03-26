#include "ExtruderVariantWidget.hpp"
#include "GUI_App.hpp"
#include "Plater.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/Config.hpp"
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>
#include <sstream>
#include <iomanip>

namespace Slic3r { namespace GUI {

// Parse combined variant string like "AA 0.4" into core type and nozzle size
static std::pair<std::string, std::string> parse_variant(const std::string& variant)
{
    // Find the last space to separate core type from nozzle size
    auto pos = variant.find_last_of(' ');
    if (pos == std::string::npos) {
        return {variant, ""};
    }
    return {variant.substr(0, pos), variant.substr(pos + 1)};
}

// Build combined variant string from core type and nozzle size
static std::string make_variant(const std::string& core_type, const std::string& nozzle)
{
    if (nozzle.empty()) {
        return core_type;
    }
    return core_type + " " + nozzle;
}

ExtruderVariantWidget::ExtruderVariantWidget(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    
    // Title - use same font as rest of UI
    auto* title = new wxStaticText(this, wxID_ANY, _L("Print Core Configuration"));
    title->SetFont(wxGetApp().bold_font());
    sizer->Add(title, 0, wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, 5);
    
    SetSizer(sizer);
    Hide(); // Hidden by default, shown only for compatible printers
}

void ExtruderVariantWidget::update_from_config()
{
    // Guard against null preset_bundle
    if (!wxGetApp().preset_bundle) {
        Hide();
        return;
    }
    
    auto* preset_bundle = wxGetApp().preset_bundle;
    auto& printer_preset = preset_bundle->printers.get_edited_preset();
    std::string preset_name = printer_preset.name;
    
    // Determine which variant list to use based on printer model
    std::vector<std::string> variant_list;
    if (preset_name.find("UltiMaker S3") != std::string::npos ||
        preset_name.find("UltiMaker S5") != std::string::npos ||
        preset_name.find("UltiMaker S7") != std::string::npos ||
        preset_name.find("UltiMaker Factor 4") != std::string::npos) {
        // S3/S5/S7/Factor 4 - basic cores only (AA, BB, CC)
        variant_list = {"AA 0.25", "AA 0.4", "AA 0.8", "BB 0.4", "BB 0.8", "CC 0.4", "CC 0.6"};
    } else if (preset_name.find("UltiMaker S6") != std::string::npos ||
               preset_name.find("UltiMaker S8") != std::string::npos) {
        // S6/S8 - all cores including HT and (+) variants
        variant_list = {"AA 0.25", "AA 0.4", "AA 0.8", "AA+ 0.4", "BB 0.4", "BB 0.8", 
                        "CC 0.4", "CC 0.6", "CC+ 0.4", "CC+ 0.6", "CC Red 0.6", "HT 0.6"};
    } else {
        Hide();
        return;
    }
    
    // Clear existing rows
    auto* sizer = GetSizer();
    for (auto& row : m_extruder_rows) {
        if (row.label) row.label->Destroy();
        if (row.variant_choice) row.variant_choice->Destroy();
    }
    m_extruder_rows.clear();
    
    // Get nozzle diameter count from full_config (determines extruder count)
    auto& full_config = preset_bundle->full_config();
    auto nozzle_diameters = full_config.option<ConfigOptionFloats>("nozzle_diameter");
    if (!nozzle_diameters) {
        Hide();
        return;
    }
    
    // Get current printer_extruder_variant from the PRESET (not full_config)
    // This ensures we get the default values from the preset file
    auto current_variants = printer_preset.config.option<ConfigOptionStrings>("printer_extruder_variant");
    if (!current_variants || current_variants->values.empty()) {
        // Fallback to full_config if not in preset directly
        current_variants = full_config.option<ConfigOptionStrings>("printer_extruder_variant");
    }
    
    size_t num_extruders = nozzle_diameters->values.size();
    
    // Ensure filament count matches extruder count BEFORE creating UI
    // This fixes the timing issue where filament dropdowns don't show
    size_t current_filaments = preset_bundle->filament_presets.size();
    if (current_filaments != num_extruders) {
        preset_bundle->set_num_filaments((unsigned int)num_extruders);
    }
    
    // Create a single horizontal row with both print cores side by side
    auto* row_sizer = new wxBoxSizer(wxHORIZONTAL);
    
    // Create dropdown for each extruder
    for (size_t i = 0; i < num_extruders; i++) {
        // Print Core label - use same font as rest of UI
        wxString label_text = wxString::Format(_L("Print Core %d"), (int)(i + 1));
        auto* label = new wxStaticText(this, wxID_ANY, label_text);
        label->SetFont(wxGetApp().normal_font());
        row_sizer->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 3);
        
        // Combined variant choice (e.g., "AA 0.4", "BB 0.4", "CC 0.6")
        auto* variant_choice = new wxChoice(this, wxID_ANY);
        
        // Add variants to dropdown
        for (const auto& variant : variant_list) {
            variant_choice->Append(wxString::FromUTF8(variant));
        }
        
        // Set current selection based on current printer_extruder_variant (now in combined format)
        std::string expected_variant;
        if (current_variants && i < current_variants->values.size()) {
            std::string stored_variant = current_variants->values[i];
            // If already in combined format (contains space), use directly
            if (stored_variant.find(' ') != std::string::npos) {
                expected_variant = stored_variant;
            } else {
                // Legacy format - build combined string from core type and nozzle diameter
                double current_nozzle = (i < nozzle_diameters->values.size()) ? nozzle_diameters->values[i] : 0.4;
                // Format nozzle without trailing zeros
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(2) << current_nozzle;
                std::string nozzle_str = oss.str();
                // Remove trailing zeros
                nozzle_str.erase(nozzle_str.find_last_not_of('0') + 1, std::string::npos);
                if (!nozzle_str.empty() && nozzle_str.back() == '.') nozzle_str.pop_back();
                expected_variant = make_variant(stored_variant, nozzle_str);
            }
        }
        
        int sel = variant_choice->FindString(wxString::FromUTF8(expected_variant));
        if (sel != wxNOT_FOUND) {
            variant_choice->SetSelection(sel);
        }
        
        variant_choice->Bind(wxEVT_CHOICE, [this, i, variant_choice](wxCommandEvent&) {
            on_variant_changed(i, variant_choice->GetStringSelection());
        });
        
        row_sizer->Add(variant_choice, 0, wxALIGN_CENTER_VERTICAL, 0);
        
        // Add spacer between dropdowns (except after last one)
        if (i < num_extruders - 1) {
            row_sizer->AddSpacer(10);
        }
        
        m_extruder_rows.push_back({label, variant_choice});
    }
    
    // Center the row sizer horizontally
    auto* center_sizer = new wxBoxSizer(wxHORIZONTAL);
    center_sizer->AddStretchSpacer(1);
    center_sizer->Add(row_sizer, 0, wxALIGN_CENTER_VERTICAL);
    center_sizer->AddStretchSpacer(1);
    sizer->Add(center_sizer, 0, wxEXPAND | wxBOTTOM, 5);
    
    // Update filament count in sidebar if needed
    // This ensures filament combos match extruder count on first launch
    if (current_filaments != num_extruders) {
        wxTheApp->CallAfter([num_extruders]() {
            auto* plater = wxGetApp().plater();
            if (plater) {
                plater->on_filament_count_change(num_extruders);
            }
        });
    }
    
    Show();
    sizer->Layout();
    GetParent()->Layout();
}

void ExtruderVariantWidget::on_variant_changed(int extruder_idx, const wxString& variant)
{
    // Guard against null preset_bundle
    if (!wxGetApp().preset_bundle) {
        return;
    }
    
    auto* preset_bundle = wxGetApp().preset_bundle;
    auto& printer_preset = preset_bundle->printers.get_edited_preset();
    
    // Parse combined variant string (e.g., "AA 0.4" -> core_type="AA", nozzle="0.4")
    auto [core_type, nozzle] = parse_variant(variant.ToStdString());
    
    // Update printer_extruder_variant with combined format (e.g., "AA 0.4")
    auto variant_opt = printer_preset.config.option<ConfigOptionStrings>("printer_extruder_variant");
    if (variant_opt && extruder_idx < (int)variant_opt->values.size()) {
        variant_opt->values[extruder_idx] = variant.ToStdString();  // Store combined format
    }
    
    // Update nozzle_diameter if nozzle size was specified
    if (!nozzle.empty()) {
        auto nozzle_opt = printer_preset.config.option<ConfigOptionFloats>("nozzle_diameter");
        if (nozzle_opt && extruder_idx < (int)nozzle_opt->values.size()) {
            double nozzle_value;
            if (std::istringstream(nozzle) >> nozzle_value) {
                nozzle_opt->values[extruder_idx] = nozzle_value;
            }
        }
    }
    
    // Mark preset as dirty
    preset_bundle->printers.get_edited_preset().set_dirty();
    
    // Update printer-related UI elements
    wxGetApp().plater()->sidebar().update_presets(Preset::TYPE_PRINTER);
    
    // Update printer thumbnail to reflect changes
    wxGetApp().plater()->sidebar().update_printer_thumbnail();
}

bool ExtruderVariantWidget::printer_has_variants()
{
    // Check if current printer is UltiMaker S-series or Factor 4
    if (!wxGetApp().preset_bundle) {
        return false;
    }
    
    auto& printer_preset = wxGetApp().preset_bundle->printers.get_edited_preset();
    std::string preset_name = printer_preset.name;
    
    // Check if it's an UltiMaker printer with swappable cores
    // S3, S5, S6, S7, S8, Factor 4 have swappable cores
    if (preset_name.find("UltiMaker S3") != std::string::npos ||
        preset_name.find("UltiMaker S5") != std::string::npos ||
        preset_name.find("UltiMaker S6") != std::string::npos ||
        preset_name.find("UltiMaker S7") != std::string::npos ||
        preset_name.find("UltiMaker S8") != std::string::npos ||
        preset_name.find("UltiMaker Factor 4") != std::string::npos) {
        return true;
    }
    
    return false;
}

}} // namespace Slic3r::GUI