#include "ExtruderVariantWidget.hpp"
#include "GUI_App.hpp"
#include "Plater.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/Config.hpp"
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>

namespace Slic3r { namespace GUI {

ExtruderVariantWidget::ExtruderVariantWidget(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    
    // Title
    auto* title = new wxStaticText(this, wxID_ANY, _L("Print Core Setup"));
    title->SetFont(wxFont(wxFontInfo().Bold()));
    sizer->Add(title, 0, wxBOTTOM, 5);
    
    SetSizer(sizer);
    Hide(); // Hidden by default, shown only for compatible printers
}

void ExtruderVariantWidget::update_from_config()
{
    // Get current printer preset
    auto& printer_preset = wxGetApp().preset_bundle->printers.get_edited_preset();
    
    // Check if printer has variant list
    if (!printer_preset.config.has("extruder_variant_list")) {
        Hide();
        return;
    }
    
    auto variant_list_opt = printer_preset.config.option<ConfigOptionStrings>("extruder_variant_list");
    if (!variant_list_opt || variant_list_opt->values.empty()) {
        Hide();
        return;
    }
    
    // Check if this is a UltiMaker variant list (has AA, BB, CC)
    bool has_custom_variants = false;
    for (const auto& v : variant_list_opt->values) {
        if (v.find("AA") != std::string::npos || 
            v.find("BB") != std::string::npos ||
            v.find("CC") != std::string::npos) {
            has_custom_variants = true;
            break;
        }
    }
    
    if (!has_custom_variants) {
        Hide();
        return;
    }
    
    // Clear existing rows
    auto* sizer = GetSizer();
    for (auto& row : m_extruder_rows) {
        if (row.label) row.label->Destroy();
        if (row.variant_choice) row.variant_choice->Destroy();
        if (row.nozzle_choice) row.nozzle_choice->Destroy();
    }
    m_extruder_rows.clear();
    
    // Get nozzle diameter count (determines extruder count)
    auto nozzle_diameters = printer_preset.config.option<ConfigOptionFloats>("nozzle_diameter");
    if (!nozzle_diameters) {
        Hide();
        return;
    }
    
    size_t num_extruders = nozzle_diameters->values.size();
    
    // Create row for each extruder
    for (size_t i = 0; i < num_extruders; i++) {
        auto* row_sizer = new wxBoxSizer(wxHORIZONTAL);
        
        // Extruder label
        wxString label_text = wxString::Format(_L("Extruder %d:"), (int)(i + 1));
        auto* label = new wxStaticText(this, wxID_ANY, label_text);
        row_sizer->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
        
        // Core type choice
        auto* variant_choice = new wxChoice(this, wxID_ANY);
        
        // Parse variant list for this extruder
        std::string variant_str = variant_list_opt->get_at(i);
        std::vector<std::string> variants;
        boost::split(variants, variant_str, boost::is_any_of(","), boost::token_compress_on);
        
        for (const auto& variant : variants) {
            variant_choice->Append(wxString::FromUTF8(variant));
        }
        
        // Set current selection from printer_extruder_variant
        auto current_variants = printer_preset.config.option<ConfigOptionStrings>("printer_extruder_variant");
        if (current_variants && i < current_variants->values.size()) {
            int sel = variant_choice->FindString(wxString::FromUTF8(current_variants->values[i]));
            if (sel != wxNOT_FOUND) {
                variant_choice->SetSelection(sel);
            }
        }
        
        variant_choice->Bind(wxEVT_CHOICE, [this, i, variant_choice](wxCommandEvent&) {
            on_variant_changed(i, variant_choice->GetStringSelection());
        });
        
        row_sizer->Add(variant_choice, 1, wxRIGHT, 10);
        
        // Nozzle size choice (dynamic based on model)
        auto* nozzle_choice = new wxChoice(this, wxID_ANY);
        
        // Get available nozzle sizes from model
        auto* model = wxGetApp().preset_bundle->printers.get_preset(printer_preset.name).get_model();
        if (model && model->nozzle_diameter.size() > 0) {
            for (const auto& nd : model->nozzle_diameter) {
                nozzle_choice->Append(wxString::FromUTF8(nd));
            }
        } else {
            // Fallback to common sizes
            nozzle_choice->Append("0.25");
            nozzle_choice->Append("0.4");
            nozzle_choice->Append("0.6");
            nozzle_choice->Append("0.8");
        }
        
        // Set current nozzle size
        wxString current_nozzle = wxString::Format("%.2f", nozzle_diameters->values[i]);
        int nozzle_sel = nozzle_choice->FindString(current_nozzle);
        if (nozzle_sel != wxNOT_FOUND) {
            nozzle_choice->SetSelection(nozzle_sel);
        }
        
        nozzle_choice->Bind(wxEVT_CHOICE, [this, i, nozzle_choice](wxCommandEvent&) {
            on_nozzle_changed(i, nozzle_choice->GetStringSelection());
        });
        
        row_sizer->Add(nozzle_choice, 0);
        
        sizer->Add(row_sizer, 0, wxEXPAND | wxBOTTOM, 5);
        
        m_extruder_rows.push_back({label, variant_choice, nozzle_choice});
    }
    
    Show();
    sizer->Layout();
    GetParent()->Layout();
}

void ExtruderVariantWidget::on_variant_changed(int extruder_idx, const wxString& variant)
{
    // Update printer_extruder_variant in config
    auto& printer_preset = wxGetApp().preset_bundle->printers.get_edited_preset();
    auto opt = printer_preset.config.option<ConfigOptionStrings>("printer_extruder_variant");
    
    if (opt && extruder_idx < (int)opt->values.size()) {
        opt->values[extruder_idx] = variant.ToUTF8().data();
        
        // Mark preset as dirty
        wxGetApp().preset_bundle->printers.get_edited_preset().set_dirty();
        
        // Update UI
        wxGetApp().plater()->sidebar().update_presets(Preset::TYPE_PRINTER);
    }
}

void ExtruderVariantWidget::on_nozzle_changed(int extruder_idx, const wxString& nozzle)
{
    // Update nozzle_diameter in config
    auto& printer_preset = wxGetApp().preset_bundle->printers.get_edited_preset();
    auto opt = printer_preset.config.option<ConfigOptionFloats>("nozzle_diameter");
    
    if (opt && extruder_idx < (int)opt->values.size()) {
        double value;
        if (nozzle.ToDouble(&value)) {
            opt->values[extruder_idx] = value;
            wxGetApp().preset_bundle->printers.get_edited_preset().set_dirty();
            
            // Refresh the widget to update nozzle-dependent options
            update_from_config();
        }
    }
}

bool ExtruderVariantWidget::printer_has_variants()
{
    auto& printer_preset = wxGetApp().preset_bundle->printers.get_edited_preset();
    
    if (!printer_preset.config.has("extruder_variant_list")) {
        return false;
    }
    
    auto variant_list = printer_preset.config.option<ConfigOptionStrings>("extruder_variant_list");
    if (!variant_list || variant_list->values.empty()) {
        return false;
    }
    
    // Check for custom variants (AA, BB, CC)
    for (const auto& v : variant_list->values) {
        if (v.find("AA") != std::string::npos || 
            v.find("BB") != std::string::npos ||
            v.find("CC") != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

}} // namespace Slic3r::GUI