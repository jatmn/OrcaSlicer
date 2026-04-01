#include "ContainerWriterContext.hpp"

namespace Slic3r {

void ContainerWriterContext::set_print_stats(int duration_s, double filament_mm, double filament_g)
{
    m_duration_s = duration_s;
    m_filament_mm = filament_mm;
    m_filament_g = filament_g;
    m_has_stats = true;
}

void ContainerWriterContext::set_extruder_variants(const std::vector<std::string>& variants)
{
    m_extruder_variants = variants;
}

void ContainerWriterContext::set_extruder_data(int idx, const ExtruderData& data)
{
    if (idx >= 0 && idx < 2) {
        m_extruders[idx] = data;
    }
}

ExtruderData& ContainerWriterContext::get_extruder_data(int idx)
{
    // Return reference to requested extruder, or extruder 0 as fallback
    if (idx >= 0 && idx < 2) {
        return m_extruders[idx];
    }
    return m_extruders[0];
}

bool ContainerWriterContext::has_extruder_data(int idx) const
{
    if (idx >= 0 && idx < 2) {
        return !m_extruders[idx].empty();
    }
    return false;
}

bool ContainerWriterContext::has_any_extruder_data() const
{
    return !m_extruders[0].empty() || !m_extruders[1].empty();
}

void ContainerWriterContext::add_thumbnail(const std::vector<uint8_t>& data, const std::string& filename)
{
    m_thumbnails.emplace_back(data, filename);
}

void ContainerWriterContext::set_thumbnails(const std::vector<std::pair<std::vector<uint8_t>, std::string>>& thumbnails)
{
    m_thumbnails = thumbnails;
}

void ContainerWriterContext::clear()
{
    m_duration_s = 0;
    m_filament_mm = 0.0;
    m_filament_g = 0.0;
    m_has_stats = false;
    m_extruder_variants.clear();
    m_extruders[0] = ExtruderData();
    m_extruders[1] = ExtruderData();
    m_thumbnails.clear();
}

bool ContainerWriterContext::validate_for_export(std::string& error_out) const
{
    // Check that we have at least one extruder with data
    if (!has_any_extruder_data()) {
        error_out = "No extruder data set for export";
        return false;
    }
    
    // Check that extruder 0 has valid data
    if (m_extruders[0].filament_mm <= 0.0) {
        error_out = "Extruder 0 has no filament usage data";
        return false;
    }
    
    // All validations passed
    error_out.clear();
    return true;
}

void ContainerWriterContext::propagate_extruder_data_if_needed()
{
    // If extruder 1 is empty but extruder 0 has data, propagate it
    // This handles dual-extruder prints where both extruders use the same material
    if (m_extruders[1].empty() && !m_extruders[0].empty()) {
        m_extruders[1] = m_extruders[0];
    }
}

} // namespace Slic3r
