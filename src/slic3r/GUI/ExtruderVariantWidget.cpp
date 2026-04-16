#include "ExtruderVariantWidget.hpp"
#include "GUI_App.hpp"
#include "Plater.hpp"
#include "Widgets/Label.hpp"
#include "MsgDialog.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/Preset.hpp"
#include "libslic3r/Config.hpp"
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>
#include <sstream>
#include <iomanip>
#include <wx/arrstr.h>
#include <wx/msgdlg.h>

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
    
    // Title - use Label class with Head_14 font (bold, larger) to match section titles
    auto* title = new Label(this, Label::Head_14, _L("Print Core Configuration"));
    sizer->Add(title, 0, wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, 5);

    // Add separator line to match optgroup visual style
    auto* line = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(200), 1));
    sizer->Add(line, 0, wxEXPAND | wxTOP | wxBOTTOM, FromDIP(5));

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
    std::vector<std::string> full_variant_list;
    if (preset_name.find("UltiMaker S3") != std::string::npos ||
        preset_name.find("UltiMaker S5") != std::string::npos ||
        preset_name.find("UltiMaker S7") != std::string::npos) {
        // S3/S5/S7 - basic cores only (AA, BB, CC)
        full_variant_list = {"AA 0.25", "AA 0.4", "AA 0.8", "BB 0.4", "BB 0.8", "CC 0.4", "CC 0.6"};
    } else if (preset_name.find("UltiMaker S6") != std::string::npos ||
               preset_name.find("UltiMaker S8") != std::string::npos) {
        // S6/S8 - all cores including HT and (+) variants
        full_variant_list = {"AA 0.25", "AA 0.4", "AA 0.8", "AA+ 0.4", "BB 0.4", "BB 0.8", 
                        "CC 0.4", "CC 0.6", "CC+ 0.4", "CC+ 0.6", "CC Red 0.6", "HT 0.6"};
    } else if (preset_name.find("UltiMaker Factor 4") != std::string::npos) {
        // Factor 4 - basic cores plus HT, but no (+) variants
        full_variant_list = {"AA 0.25", "AA 0.4", "AA 0.8", "BB 0.4", "BB 0.8", "CC 0.4", "CC 0.6", "HT 0.6"};
    } else {
        Hide();
        return;
    }
    
    // Filter variants by current nozzle diameter so only compatible cores are shown
    std::string active_nozzle;
    if (auto* nd = printer_preset.config.option<ConfigOptionFloats>("nozzle_diameter")) {
        if (!nd->values.empty()) {
            std::ostringstream oss;
            oss << nd->values[0];
            active_nozzle = oss.str();
            // Trim trailing zeros (0.40 -> 0.4)
            if (active_nozzle.find('.') != std::string::npos) {
                while (active_nozzle.back() == '0')
                    active_nozzle.pop_back();
                if (active_nozzle.back() == '.')
                    active_nozzle.push_back('0');
            }
        }
    }
    std::vector<std::string> variant_list;
    for (const auto& variant : full_variant_list) {
        if (active_nozzle.empty() || variant.size() >= active_nozzle.size() + 1) {
            // Variant ends with " <nozzle>" (e.g., " 0.4")
            if (variant.substr(variant.size() - active_nozzle.size()) == active_nozzle &&
                variant[variant.size() - active_nozzle.size() - 1] == ' ') {
                variant_list.push_back(variant);
            }
        }
    }
    if (variant_list.empty()) {
        // Fallback: if filtering fails, show all variants
        variant_list = full_variant_list;
    }
    
    // Clear existing rows and old sizers safely.
    // Using Clear(true) is critical: it destroys windows BEFORE deleting sizer items,
    // avoiding use-after-free when wxSizerItem destructor calls SetContainingSizer().
    auto* sizer = GetSizer();
    sizer->Clear(true);
    m_extruder_rows.clear();
    m_extruder_variants.clear();
    
    // Recreate static title and separator (also destroyed by Clear(true))
    auto* title = new Label(this, Label::Head_14, _L("Print Core Configuration"));
    sizer->Add(title, 0, wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, 5);
    auto* line = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(200), 1));
    sizer->Add(line, 0, wxEXPAND | wxTOP | wxBOTTOM, FromDIP(5));
    
    // Get nozzle diameter count from full_config (determines extruder count)
    const auto& full_config = preset_bundle->full_config();
    auto nozzle_diameters = full_config.option<ConfigOptionFloats>("nozzle_diameter");
    if (!nozzle_diameters) {
        Hide();
        return;
    }
    
    // Get current printer_extruder_variant from the PRESET (not full_config)
    // This ensures we get the default values from the preset file
    const auto* current_variants = printer_preset.config.option<ConfigOptionStrings>("printer_extruder_variant");
    if (!current_variants || current_variants->values.empty()) {
        // Fallback to full_config if not in preset directly
        current_variants = full_config.option<ConfigOptionStrings>("printer_extruder_variant");
    }
    
    size_t num_extruders = nozzle_diameters->values.size();
    
    // Ensure filament count matches extruder count BEFORE creating UI
    // This fixes the timing issue where filament dropdowns don't show
    size_t current_filaments = preset_bundle->filament_presets.size();
    bool filament_count_mismatch = (current_filaments != num_extruders);
    if (filament_count_mismatch) {
        preset_bundle->set_num_filaments((unsigned int)num_extruders);
    }
    
    // Create a single horizontal row with both print cores side by side
    auto* row_sizer = new wxBoxSizer(wxHORIZONTAL);
    
    // Create dropdown for each extruder
    for (size_t i = 0; i < num_extruders; i++) {
        // Print Core label - use Label class with Body_13 to match option label styling
        wxString label_text = wxString::Format(_L("Print Core %d"), (int)(i + 1));
        auto* label = new Label(this, Label::Body_13, label_text);
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
        
        // Also add to the variant choice vector for easier access
        m_extruder_variants.push_back(variant_choice);
        
        m_extruder_rows.push_back({label, variant_choice});
    }
    
    // Center the row sizer horizontally
    sizer->Add(row_sizer, 0, wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, 5);
    
    // Update filament count in sidebar if needed
    // This ensures filament combos match extruder count on first launch
    // Use CallAfter to ensure UI is fully initialized before updating
    if (filament_count_mismatch) {
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
    BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] on_variant_changed ENTER - extruder_idx=" << extruder_idx;
    
    // Guard against null preset_bundle
    if (!wxGetApp().preset_bundle) {
        BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] on_variant_changed EXIT - null preset_bundle";
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
    
    // Check for core size mismatch between extruders
    // Skip check if we're already handling a mismatch dialog (recursion prevention)
    wxString size1, size2;
    if (!m_in_mismatch_dialog && has_size_mismatch(size1, size2)) {
        // Show mismatch dialog - user may choose to change other core
        show_mismatch_dialog(size1, size2);
    }
    
    // Update process presets based on controlling print core
    // Get controlling core and extract type for process preset filtering
    int controlling_idx = get_controlling_core_index();
    if (controlling_idx >= 0 && controlling_idx < (int)m_extruder_variants.size()) {
        int sel = m_extruder_variants[controlling_idx]->GetSelection();
        if (sel != wxNOT_FOUND) {
            wxString selected_core = m_extruder_variants[controlling_idx]->GetString(sel);
            wxString core_type = extract_type_from_core(selected_core);
            update_process_presets(core_type);
        }
    }
    
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

// Extract nozzle size from core string (e.g., "AA 0.4" -> "0.4")
wxString ExtruderVariantWidget::extract_size_from_core(const wxString& core)
{
    // Split the core string by space
    wxArrayString parts = wxSplit(core, ' ');
    if (parts.size() >= 2) {
        return parts[parts.size() - 1];  // Last part is the size
    }
    return wxString();  // Return empty if format is invalid
}

// Check if selected cores have different nozzle sizes
bool ExtruderVariantWidget::has_size_mismatch(wxString& size1, wxString& size2)
{
    BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] has_size_mismatch ENTER";
    
    if (m_extruder_variants.size() < 2) {
        BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] has_size_mismatch EXIT - single extruder";
        return false;  // Single extruder, no mismatch possible
    }
    
    // Guard against null pointers
    if (!m_extruder_variants[0] || !m_extruder_variants[1]) {
        BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] has_size_mismatch EXIT - null pointer";
        return false;
    }
    
    BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] has_size_mismatch - ptr0=" << m_extruder_variants[0] << " ptr1=" << m_extruder_variants[1];
    
    // Get current selections from combo boxes
    BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] has_size_mismatch - about to call GetSelection on ptr0";
    int sel0 = m_extruder_variants[0]->GetSelection();
    BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] has_size_mismatch - sel0=" << sel0;
    BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] has_size_mismatch - about to call GetSelection on ptr1";
    int sel1 = m_extruder_variants[1]->GetSelection();
    BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] has_size_mismatch - sel1=" << sel1;
    
    // Guard against no selection (GetSelection returns wxNOT_FOUND if none selected)
    if (sel0 == wxNOT_FOUND || sel1 == wxNOT_FOUND) {
        return false;
    }
    
    wxString core1 = m_extruder_variants[0]->GetString(sel0);
    wxString core2 = m_extruder_variants[1]->GetString(sel1);
    
    // Extract sizes
    size1 = extract_size_from_core(core1);
    size2 = extract_size_from_core(core2);
    
    // Compare sizes
    return !size1.IsEmpty() && !size2.IsEmpty() && size1 != size2;
}

// Show dialog asking user to select which core size to use
// Returns true if user made a selection, false if cancelled
bool ExtruderVariantWidget::show_mismatch_dialog(const wxString& size1, const wxString& size2)
{
    BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] show_mismatch_dialog ENTER";
    
    // Guard against invalid state - ensure we have 2 valid extruder variant pointers
    if (m_extruder_variants.size() < 2 || !m_extruder_variants[0] || !m_extruder_variants[1]) {
        BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] show_mismatch_dialog EXIT - invalid state";
        return false;
    }
    
    // Get full core names for the dialog
    int sel0 = m_extruder_variants[0]->GetSelection();
    int sel1 = m_extruder_variants[1]->GetSelection();
    
    // Guard against no selection - use passed size strings as fallback
    wxString core1 = (sel0 != wxNOT_FOUND) ? m_extruder_variants[0]->GetString(sel0) : wxString::Format("Core 1 (%s)", size1);
    wxString core2 = (sel1 != wxNOT_FOUND) ? m_extruder_variants[1]->GetString(sel1) : wxString::Format("Core 2 (%s)", size2);
    
    // Set recursion prevention flag
    m_in_mismatch_dialog = true;
    
    // Create dialog similar to Bambu H2D pattern
    MessageDialog dlg(this,
                      _L("The selected print cores have different nozzle sizes. "
                         "UltiMaker printers require both print cores to have the same nozzle size "
                         "for dual extrusion printing.\n\n"
                         "Please select which nozzle size you would like to use for both cores."),
                      _L("Print Core Size Mismatch"),
                      wxYES_NO | wxNO_DEFAULT | wxICON_WARNING);
    
    // Set custom button labels showing core options
    dlg.SetButtonLabel(wxID_YES, wxString::Format(_L("Use %s (%smm)"), core1, size1));
    dlg.SetButtonLabel(wxID_NO, wxString::Format(_L("Use %s (%smm)"), core2, size2));
    
    // Show dialog and handle result
    int result = dlg.ShowModal();
    
    // Clear recursion prevention flag
    m_in_mismatch_dialog = false;
    
    if (result == wxID_YES) {
        // User chose to use size from core 1
        update_core_size(1, size1);  // Update core 2 to match size1
        return true;
    } else if (result == wxID_NO) {
        // User chose to use size from core 2
        update_core_size(0, size2);  // Update core 1 to match size2
        return true;
    }
    
    // User cancelled (shouldn't happen with YES_NO dialog, but handle anyway)
    return false;
}

// Update a specific core to a new size while preserving its type
// Returns true if selection was actually changed, false otherwise
bool ExtruderVariantWidget::update_core_size(int extruder_idx, const wxString& new_size)
{
    BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] update_core_size ENTER - extruder_idx=" << extruder_idx;
    
    if (extruder_idx < 0 || extruder_idx >= (int)m_extruder_variants.size()) {
        BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] update_core_size EXIT - invalid index";
        return false;
    }
    
    wxChoice* combo = m_extruder_variants[extruder_idx];
    
    // Guard against null pointer
    if (!combo) {
        return false;
    }
    
    // Guard against invalid selection
    int current_sel = combo->GetSelection();
    if (current_sel == wxNOT_FOUND) {
        return false;
    }
    
    // First, try to find an exact match with same core type and new size
    wxString current_core = combo->GetString(current_sel);
    wxArrayString parts = wxSplit(current_core, ' ');
    if (parts.size() < 2) {
        return false;  // Invalid format
    }
    
    wxString core_type = parts[0];  // e.g., "AA", "BB", "CC"
    wxString exact_match = wxString::Format("%s %s", core_type, new_size);
    int new_sel = combo->FindString(exact_match);
    
    // If exact match not found, search for ANY core with the target size
    if (new_sel == wxNOT_FOUND) {
        // Iterate through all options to find one with the target size
        for (int i = 0; i < combo->GetCount(); i++) {
            wxString option = combo->GetString(i);
            wxString option_size = extract_size_from_core(option);
            if (option_size == new_size) {
                new_sel = i;
                break;
            }
        }
    }
    
    // If we found a valid option, select it
    if (new_sel != wxNOT_FOUND && new_sel != combo->GetSelection()) {
        combo->SetSelection(new_sel);
        
        // Trigger the change event to update config
        wxCommandEvent evt(wxEVT_CHOICE);
        evt.SetEventObject(combo);
        on_variant_changed(extruder_idx, combo->GetString(new_sel));
        return true;
    }
    
    return false;  // No valid option found or selection unchanged
}

// Determine which print core controls process selection based on printer type
// Returns: 0 for Print Core 1, 1 for Print Core 2
int ExtruderVariantWidget::get_controlling_core_index()
{
    // Get current printer preset
    const Preset& printer_preset = wxGetApp().preset_bundle->printers.get_edited_preset();
    const std::string& printer_name = printer_preset.name;
    
    // Factor 4 uses Print Core 2 for process selection
    if (printer_name.find("Factor 4") != std::string::npos) {
        return 1;  // Print Core 2
    }
    
    // S Series (S3, S5, S6, S7, S8) use Print Core 1 for process selection
    if (printer_name.find("S3") != std::string::npos ||
        printer_name.find("S5") != std::string::npos ||
        printer_name.find("S6") != std::string::npos ||
        printer_name.find("S7") != std::string::npos ||
        printer_name.find("S8") != std::string::npos) {
        return 0;  // Print Core 1
    }
    
    // Default to Print Core 1 for unknown UltiMaker printers
    return 0;
}

// Extract core type from core string (e.g., "AA 0.4" -> "AA")
wxString ExtruderVariantWidget::extract_type_from_core(const wxString& core)
{
    wxArrayString parts = wxSplit(core, ' ');
    if (parts.size() >= 1) {
        return parts[0];  // First part is the type
    }
    return wxString();
}

// Update process presets based on selected core type
void ExtruderVariantWidget::update_process_presets(const wxString& core_type)
{
    BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] update_process_presets ENTER - core_type=" << core_type.ToStdString();
    
    auto* preset_bundle = wxGetApp().preset_bundle;
    if (!preset_bundle) {
        BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] update_process_presets EXIT - null preset_bundle";
        return;
    }
    
    // Re-evaluate which process presets are compatible with the new printer_extruder_variant
    preset_bundle->update_compatible(PresetSelectCompatibleType::Always);
    
    // Explicitly select a process preset whose name matches the current extruder variant
    // (e.g., "AA+ 0.4") if the current preset is not already a matching one.
    // update_compatible() only switches when the current preset becomes INCOMPATIBLE;
    // it does not proactively upgrade from a generic/default preset to a core-specific one.
    auto* ev = preset_bundle->printers.get_edited_preset().config.option<ConfigOptionStrings>("printer_extruder_variant");
    if (ev && !ev->values.empty()) {
        const std::string& variant = ev->values[0];
        const auto& current_print = preset_bundle->prints.get_edited_preset();
        BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] update_process_presets - current_print=" << current_print.name << ", variant=" << variant;
        
        if (current_print.name.find(variant) == std::string::npos) {
            for (const auto& preset : preset_bundle->prints) {
                if (preset.is_visible && preset.is_compatible && preset.name.find(variant) != std::string::npos) {
                    BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] update_process_presets - selecting better match: " << preset.name;
                    preset_bundle->prints.select_preset_by_name(preset.name, true);
                    break;
                }
            }
        }
    }
    
    // Refresh the process preset dropdown in the sidebar
    wxGetApp().plater()->sidebar().update_presets(Preset::TYPE_PRINT);
    
    BOOST_LOG_TRIVIAL(warning) << "[ExtruderVariantWidget] update_process_presets EXIT";
}

}} // namespace Slic3r::GUI
