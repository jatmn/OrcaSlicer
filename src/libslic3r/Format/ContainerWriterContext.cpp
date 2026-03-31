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

void ContainerWriterContext::set_thumbnail(const std::vector<uint8_t>& data, const std::string& filename)
{
    m_thumbnails.emplace_back(data, filename);
}

void ContainerWriterContext::set_thumbnails(const std::vector<std::pair<std::vector<uint8_t>, std::string>>& thumbnails)
{
    m_thumbnails = thumbnails;
}

} // namespace Slic3r
