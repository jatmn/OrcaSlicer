#ifndef EXTRUDER_VARIANT_WIDGET_HPP
#define EXTRUDER_VARIANT_WIDGET_HPP

#include <wx/panel.h>
#include <wx/choice.h>
#include <vector>
#include <string>

namespace Slic3r { namespace GUI {

class ExtruderVariantWidget : public wxPanel {
public:
    ExtruderVariantWidget(wxWindow* parent);
    
    // Update widget based on current printer config
    void update_from_config();
    
    // Check if current printer has variant options
    static bool printer_has_variants();
    
private:
    struct ExtruderRow {
        wxStaticText* label;
        wxChoice* variant_choice;
    };
    
    std::vector<ExtruderRow> m_extruder_rows;
    
    // Separate vector for direct access to variant choice controls (for mismatch checking)
    std::vector<wxChoice*> m_extruder_variants;
    
    // Flag to prevent recursion in mismatch dialog handling
    bool m_in_mismatch_dialog = false;
    
    void on_variant_changed(int extruder_idx, const wxString& variant);
    
    // Helper methods for size mismatch detection
    wxString extract_size_from_core(const wxString& core);
    bool has_size_mismatch(wxString& size1, wxString& size2);
    bool show_mismatch_dialog(const wxString& size1, const wxString& size2);
    bool update_core_size(int extruder_idx, const wxString& new_size);
    
    // Helper methods for process selection linkage
    int get_controlling_core_index();
    wxString extract_type_from_core(const wxString& core);
    void update_process_presets(const wxString& core_type);
};

}} // namespace Slic3r::GUI
#endif