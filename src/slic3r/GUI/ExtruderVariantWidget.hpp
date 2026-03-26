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
        wxChoice* nozzle_choice;
    };
    
    std::vector<ExtruderRow> m_extruder_rows;
    
    void on_variant_changed(int extruder_idx, const wxString& variant);
    void on_nozzle_changed(int extruder_idx, const wxString& nozzle);
};

}} // namespace Slic3r::GUI
#endif