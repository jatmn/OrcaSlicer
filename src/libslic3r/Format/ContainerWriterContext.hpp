#ifndef slic3r_ContainerWriterContext_hpp_
#define slic3r_ContainerWriterContext_hpp_

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "FormatConfig.hpp"

namespace Slic3r {

// Context class for container format writers (UFP and MakerBot)
// Encapsulates shared data and provides a unified interface for writer operations
class ContainerWriterContext {
public:
    ContainerWriterContext() = default;
    ~ContainerWriterContext() = default;

    // Print statistics
    void set_print_stats(int duration_s, double filament_mm, double filament_g);
    bool has_print_stats() const { return m_has_stats; }
    int get_duration_s() const { return m_duration_s; }
    double get_filament_mm() const { return m_filament_mm; }
    double get_filament_g() const { return m_filament_g; }

    // Extruder variants
    void set_extruder_variants(const std::vector<std::string>& variants);
    const std::vector<std::string>& get_extruder_variants() const { return m_extruder_variants; }

    // Extruder data
    void set_extruder_data(int idx, const ExtruderData& data);
    const std::array<ExtruderData, 2>& get_extruder_data() const { return m_extruders; }
    ExtruderData& get_extruder_data(int idx);
    bool has_extruder_data(int idx) const;
    bool has_any_extruder_data() const;

    // Thumbnails
    void add_thumbnail(const std::vector<uint8_t>& data, const std::string& filename);
    void set_thumbnails(const std::vector<std::pair<std::vector<uint8_t>, std::string>>& thumbnails);
    const std::vector<std::pair<std::vector<uint8_t>, std::string>>& get_thumbnails() const { return m_thumbnails; }
    bool has_thumbnails() const { return !m_thumbnails.empty(); }

    // State management
    void clear();
    bool validate_for_export(std::string& error_out) const;

    // Dual-extruder helper: propagate extruder 0 data to extruder 1 if empty
    void propagate_extruder_data_if_needed();

private:
    // Print statistics
    int m_duration_s = 0;
    double m_filament_mm = 0.0;
    double m_filament_g = 0.0;
    bool m_has_stats = false;

    // Extruder variants (e.g., ["AA 0.4", "BB 0.4"])
    std::vector<std::string> m_extruder_variants;

    // Per-extruder data for multi-extruder support (up to 2 extruders)
    std::array<ExtruderData, 2> m_extruders;

    // Thumbnails as (PNG data, filename) pairs
    std::vector<std::pair<std::vector<uint8_t>, std::string>> m_thumbnails;
};

} // namespace Slic3r

#endif // slic3r_ContainerWriterContext_hpp_
