#include "UFPWriter.hpp"
#include "../miniz_extension.hpp"
#include "../Utils.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/filesystem.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <fstream>
#include <cmath>

namespace Slic3r {

namespace {

bool extruder_is_used(const ExtruderData& data)
{
    return data.filament_mm > 0.0 || data.filament_g > 0.0;
}

std::string to_lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string sanitize_material_filename_component(std::string value)
{
    value = to_lower_copy(std::move(value));
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return (std::isalnum(c) || c == '_' || c == '-') ? static_cast<char>(c) : '_';
    });

    std::string sanitized;
    sanitized.reserve(value.size());
    bool last_was_separator = false;
    for (char c : value) {
        if (c == '_') {
            if (!last_was_separator) {
                sanitized.push_back(c);
            }
            last_was_separator = true;
        } else {
            sanitized.push_back(c);
            last_was_separator = false;
        }
    }

    while (!sanitized.empty() && sanitized.front() == '_')
        sanitized.erase(sanitized.begin());
    while (!sanitized.empty() && sanitized.back() == '_')
        sanitized.pop_back();

    return sanitized.empty() ? "material" : sanitized;
}

struct MaterialAsset {
    std::string key;
    std::string filename;
    ExtruderData extruder;
};

ExtruderData resolve_extruder_data(size_t idx, const std::array<ExtruderData, 2>& extruders, const GCodeMetadata& meta)
{
    ExtruderData resolved = extruders[idx];
    if (!extruder_is_used(resolved))
        return resolved;

    if (resolved.extruder_temp <= 0) {
        if (idx > 0 && extruder_is_used(extruders[0]) && extruders[0].extruder_temp > 0)
            resolved.extruder_temp = extruders[0].extruder_temp;
        else
            resolved.extruder_temp = meta.extruder_temp;
    }

    if (resolved.material_guid.empty()) {
        if (idx > 0 && extruder_is_used(extruders[0]) && !extruders[0].material_guid.empty())
            resolved.material_guid = extruders[0].material_guid;
        else
            resolved.material_guid = meta.material_guid;
    }

    if (resolved.material_name.empty()) {
        if (idx > 0 && extruder_is_used(extruders[0]) && !extruders[0].material_name.empty())
            resolved.material_name = extruders[0].material_name;
        else if (!meta.material_type.empty())
            resolved.material_name = meta.material_type;
        else
            resolved.material_name = meta.material_name;
    }

    if (resolved.brand.empty() && idx > 0 && extruder_is_used(extruders[0]) && !extruders[0].brand.empty())
        resolved.brand = extruders[0].brand;

    return resolved;
}

} // namespace

bool UFPWriter::write_container(const GCodeMetadata& meta, const std::string& gcode_content, const std::string& output_path) {
    // Check if output file already exists and can be written
    boost::filesystem::path out_path(output_path);
    boost::system::error_code ec;
    (void)boost::filesystem::exists(out_path, ec);
    
    mz_zip_archive archive;
    mz_zip_zero_struct(&archive);
    
    // Ensure output path uses forward slashes for Windows compatibility with miniz
    std::string normalized_path = output_path;
    std::replace(normalized_path.begin(), normalized_path.end(), '\\', '/');
    
    if (!open_zip_writer(&archive, normalized_path)) {
        BOOST_LOG_TRIVIAL(error) << "UFPWriter::write_container: Failed to open zip writer, last_error=" 
                                 << archive.m_last_error;
        return false;
    }
    
    // Debug: verify archive is ready
    if (archive.m_zip_mode != MZ_ZIP_MODE_WRITING) {
        BOOST_LOG_TRIVIAL(error) << "UFPWriter::write_container: Archive not in WRITE mode! mode=" << archive.m_zip_mode;
        close_zip_writer(&archive);
        return false;
    }
    
    auto cleanup = [&]() {
        close_zip_writer(&archive);
    };
    
    if (gcode_content.empty()) {
        BOOST_LOG_TRIVIAL(error) << "UFPWriter::write_container: gcode_content is EMPTY!";
        cleanup();
        return false;
    }
    
    // Debug: check if archive is still in writing mode before adding first file
    if (archive.m_zip_mode != MZ_ZIP_MODE_WRITING) {
        BOOST_LOG_TRIVIAL(error) << "UFPWriter::write_container: Archive mode changed before adding files! mode=" << archive.m_zip_mode;
        cleanup();
        return false;
    }
    
    // Write files in exact order matching valid UFP format:
    // 1. /3D/model.gcode (with leading slash)
    if (!mz_zip_writer_add_mem_ex(&archive, "/3D/model.gcode",
                              gcode_content.c_str(), gcode_content.length(),
                              nullptr, 0, MZ_NO_COMPRESSION, 0, 0)) {
        BOOST_LOG_TRIVIAL(error) << "UFPWriter::write_container: Failed to add model.gcode, m_last_error=" 
                                 << archive.m_last_error;
        cleanup();
        return false;
    }
    
    // 2. /Cura/slicemetadata.json - with leading slash (Cura puts this second)
    std::string slicemetadata = generate_slicemetadata_json(meta);
    if (!mz_zip_writer_add_mem_ex(&archive, "/Cura/slicemetadata.json",
                              slicemetadata.c_str(), slicemetadata.length(),
                              nullptr, 0, MZ_NO_COMPRESSION, 0, 0)) {
        BOOST_LOG_TRIVIAL(error) << "UFPWriter::write_container: Failed to add slicemetadata.json";
        cleanup();
        return false;
    }

    // 3. /Metadata/thumbnail.png (if available) - with leading slash
    // IMPORTANT: Thumbnail is passed directly via set_thumbnails(), NEVER extracted from gcode comments
    // Thumbnails in gcode comments would cause firmware to reject the file
    bool has_thumbnail = false;
    const auto& thumbnails = m_context.get_thumbnails();
    if (!thumbnails.empty()) {
        // Use the first thumbnail (UFP format expects a single thumbnail)
        const auto& thumbnail_data = thumbnails[0].first;
        if (!mz_zip_writer_add_mem_ex(&archive, "/Metadata/thumbnail.png",
                                      thumbnail_data.data(), thumbnail_data.size(),
                                      nullptr, 0, MZ_NO_COMPRESSION, 0, 0)) {
            BOOST_LOG_TRIVIAL(warning) << "UFPWriter::write_container: Failed to add thumbnail";
        } else {
            has_thumbnail = true;
        }
    }
    
    const auto& raw_extruders = m_context.get_extruder_data();
    const std::array<ExtruderData, 2> resolved_extruders = {
        resolve_extruder_data(0, raw_extruders, meta),
        resolve_extruder_data(1, raw_extruders, meta)
    };
    const bool has_per_extruder_usage = extruder_is_used(raw_extruders[0]) || extruder_is_used(raw_extruders[1]);
    std::vector<MaterialAsset> material_assets;
    material_assets.reserve(2);

    auto material_name_for_asset = [&](const ExtruderData& extruder) {
        if (!extruder.material_name.empty())
            return extruder.material_name;
        if (!meta.material_type.empty())
            return meta.material_type;
        if (!meta.material_name.empty())
            return meta.material_name;
        return std::string("material");
    };
    auto material_guid_for_asset = [&](const ExtruderData& extruder) {
        if (!extruder.material_guid.empty())
            return extruder.material_guid;
        return meta.material_guid;
    };
    auto append_material_asset = [&](const ExtruderData& extruder) {
        const std::string material_name = material_name_for_asset(extruder);
        const std::string material_guid = material_guid_for_asset(extruder);
        const std::string material_key = !material_guid.empty()
            ? "guid:" + to_lower_copy(material_guid)
            : "name:" + to_lower_copy(extruder.brand) + "|" + to_lower_copy(material_name);

        auto existing = std::find_if(material_assets.begin(), material_assets.end(), [&](const MaterialAsset& asset) {
            return asset.key == material_key;
        });
        if (existing != material_assets.end()) {
            if (existing->extruder.brand.empty() && !extruder.brand.empty())
                existing->extruder.brand = extruder.brand;
            if (existing->extruder.material_guid.empty() && !material_guid.empty())
                existing->extruder.material_guid = material_guid;
            if (existing->extruder.material_name.empty())
                existing->extruder.material_name = material_name;
            return;
        }

        std::string base_name = sanitize_material_filename_component(material_name);
        std::string filename = "ultimaker_" + base_name + ".xml.fdm_material";
        int suffix = 1;
        while (std::any_of(material_assets.begin(), material_assets.end(), [&](const MaterialAsset& asset) {
            return asset.filename == filename;
        })) {
            filename = "ultimaker_" + base_name + "_" + std::to_string(suffix++) + ".xml.fdm_material";
        }

        ExtruderData material_extruder = extruder;
        if (material_extruder.material_name.empty())
            material_extruder.material_name = material_name;
        if (material_extruder.material_guid.empty())
            material_extruder.material_guid = material_guid;
        material_assets.push_back({material_key, filename, material_extruder});
    };

    if (has_per_extruder_usage) {
        if (extruder_is_used(raw_extruders[0]))
            append_material_asset(resolved_extruders[0]);
        if (extruder_is_used(raw_extruders[1]))
            append_material_asset(resolved_extruders[1]);
    } else {
        append_material_asset(resolved_extruders[0]);
    }

    // 4. /Metadata/UFP_Global.json - with leading slash
    std::string ufp_global = generate_ufp_global_json(meta);
    if (!mz_zip_writer_add_mem_ex(&archive, "/Metadata/UFP_Global.json",
                              ufp_global.c_str(), ufp_global.length(),
                              nullptr, 0, MZ_NO_COMPRESSION, 0, 0)) {
        BOOST_LOG_TRIVIAL(error) << "UFPWriter::write_container: Failed to add UFP_Global.json";
        cleanup();
        return false;
    }
    
    // 5. /Materials/ultimaker_{material_type}.xml.fdm_material - one entry per used material
    std::vector<std::string> material_filenames;
    material_filenames.reserve(material_assets.size());
    for (const MaterialAsset& material_asset : material_assets) {
        std::string material_xml = generate_material_xml(material_asset.extruder, meta);
        if (!mz_zip_writer_add_mem_ex(&archive, ("/Materials/" + material_asset.filename).c_str(),
                                  material_xml.c_str(), material_xml.length(),
                                  nullptr, 0, MZ_NO_COMPRESSION, 0, 0)) {
            BOOST_LOG_TRIVIAL(error) << "UFPWriter::write_container: Failed to add " << material_asset.filename;
            cleanup();
            return false;
        }
        material_filenames.push_back(material_asset.filename);
    }
    
    // 6. /_rels/.rels - with leading slash
    std::string rels = generate_rels_xml();
    if (!mz_zip_writer_add_mem_ex(&archive, "/_rels/.rels",
                              rels.c_str(), rels.length(),
                              nullptr, 0, MZ_NO_COMPRESSION, 0, 0)) {
        BOOST_LOG_TRIVIAL(error) << "UFPWriter::write_container: Failed to add .rels";
        cleanup();
        return false;
    }

    // 7. /[Content_Types].xml - with leading slash
    std::string content_types = generate_content_types_xml();
    if (!mz_zip_writer_add_mem_ex(&archive, "/[Content_Types].xml",
                              content_types.c_str(), content_types.length(),
                              nullptr, 0, MZ_NO_COMPRESSION, 0, 0)) {
        BOOST_LOG_TRIVIAL(error) << "UFPWriter::write_container: Failed to add [Content_Types].xml";
        cleanup();
        return false;
    }

    // 8. /3D/_rels/model.gcode.rels - LAST entry, matching Cura order
    std::string gcode_rels = generate_gcode_rels_xml(has_thumbnail, material_filenames);
    if (!mz_zip_writer_add_mem_ex(&archive, "/3D/_rels/model.gcode.rels",
                              gcode_rels.c_str(), gcode_rels.length(),
                              nullptr, 0, MZ_NO_COMPRESSION, 0, 0)) {
        BOOST_LOG_TRIVIAL(error) << "UFPWriter::write_container: Failed to add model.gcode.rels";
        cleanup();
        return false;
    }
    
    // Finalize archive
    if (!mz_zip_writer_finalize_archive(&archive)) {
        BOOST_LOG_TRIVIAL(error) << "UFPWriter::write_container: Failed to finalize archive";
        cleanup();
        return false;
    }
    
    cleanup();
    return true;
}

void UFPWriter::override_metadata(GCodeMetadata& meta) {
    // Override parsed metadata with injected values ONLY if they are non-zero
    // (zero means stats weren't captured; keep the parsed values from G-code comments)
    if (m_context.has_print_stats()) {
        if (m_context.get_duration_s() > 0)
            meta.duration_s = m_context.get_duration_s();
        if (m_context.get_filament_mm() > 0)
            meta.filament_mm = m_context.get_filament_mm();
        if (m_context.get_filament_g() > 0)
            meta.filament_g = m_context.get_filament_g();
    }
    
    // CRITICAL: Prioritize extruder GUID over metadata GUID
    // The extruder data comes from filament presets which should have the correct GUID
    // Metadata GUID is a default from G-code parsing (often wrong for specialized materials)
    const auto& extruders = m_context.get_extruder_data();
    const ExtruderData extruder0 = resolve_extruder_data(0, extruders, meta);
    
    // First handle extruder 0: always use extruder GUID if available
    if (!extruder0.material_guid.empty()) {
        // Extruder has GUID, use it to override metadata
        if (meta.material_guid != extruder0.material_guid) {
            meta.material_guid = extruder0.material_guid;
        }
    }
    
    // Also update material type from extruder data if available
    // This ensures "PLA Tough" is correctly identified instead of default "PLA"
    if (!extruder0.material_name.empty()) {
        if (meta.material_type != extruder0.material_name) {
            meta.material_type = extruder0.material_name;
        }
    }
    
    // Use machine bounds from config if available (6 values: min_x, min_y, min_z, max_x, max_y, max_z)
    if (m_config.machine_bounds.size() == 6) {
        meta.min_x = m_config.machine_bounds[0];
        meta.min_y = m_config.machine_bounds[1];
        meta.min_z = m_config.machine_bounds[2];
        meta.max_x = m_config.machine_bounds[3];
        meta.max_y = m_config.machine_bounds[4];
        meta.max_z = m_config.machine_bounds[5];
    }
}

std::pair<std::string, std::string> UFPWriter::get_nozzle_info(const std::string& variant_name) {
    // Try to load nozzle variants from JSON
    static std::map<std::string, std::pair<std::string, std::string>> cached_variants;
    static bool loaded = false;
    
    if (!loaded) {
        cached_variants = load_nozzle_variants();
        loaded = true;
    }
    
    // Look up in cached variants
    auto it = cached_variants.find(variant_name);
    if (it != cached_variants.end()) {
        return it->second;
    }
    
    // Fallback: extract diameter from variant name (e.g., "AA 0.4" -> "0.4")
    std::string diameter = "0.4";  // default
    size_t space_pos = variant_name.rfind(' ');
    if (space_pos != std::string::npos && space_pos + 1 < variant_name.length()) {
        std::string potential_diameter = variant_name.substr(space_pos + 1);
        // Verify it looks like a diameter (contains only digits and .)
        bool looks_like_diameter = true;
        for (char c : potential_diameter) {
            if (!std::isdigit(c) && c != '.') {
                looks_like_diameter = false;
                break;
            }
        }
        if (looks_like_diameter) {
            diameter = potential_diameter;
        }
    }
    
    // Return diameter and the variant name as display name
    return std::make_pair(diameter, variant_name);
}

std::map<std::string, std::pair<std::string, std::string>> UFPWriter::load_nozzle_variants() {
    std::map<std::string, std::pair<std::string, std::string>> variants;
    
    namespace fs = boost::filesystem;
    std::vector<std::string> paths = {
        (fs::path(Slic3r::resources_dir()) / "formats" / "ufp" / "nozzle_variants.json").string(),
        (fs::path("resources") / "formats" / "ufp" / "nozzle_variants.json").string(),
        "nozzle_variants.json"
    };
    
    for (const auto& path : paths) {
        boost::nowide::ifstream file(path);
        if (file.is_open()) {
            try {
                nlohmann::json j = nlohmann::json::parse(file);
                if (j.contains("variants")) {
                    for (auto& [key, value] : j["variants"].items()) {
                        std::string diameter = value.value("diameter", "0.4");
                        std::string display_name = value.value("display_name", key);
                        variants[key] = std::make_pair(diameter, display_name);
                    }
                    return variants;
                }
            } catch (const std::exception& e) {
                BOOST_LOG_TRIVIAL(warning) << "UFPWriter: Failed to parse nozzle_variants.json: " << e.what();
            }
        }
    }

    return variants;
}

std::string UFPWriter::generate_extruder_block(int idx, const ExtruderData& data) {
    // Only generate block if extruder was actually used (has filament data)
    // NOTE: We check filament_mm > 0, not data.empty() because even if GUID is empty,
    // we still need to generate the NOZZLE info and VOLUME_USED for the G-code header
    if (data.filament_mm <= 0.0) {
        return "";  // Return empty string only if no filament was used
    }
    
    std::ostringstream block;
    block << ";EXTRUDER_TRAIN." << idx << ".INITIAL_TEMPERATURE:" << data.extruder_temp << "\n";
    
    // Format filament length (not volume - the field name is misleading)
    std::ostringstream filament_stream;
    filament_stream << std::fixed << std::setprecision(2) << data.filament_mm;
    std::string filament_str = filament_stream.str();
    // Remove trailing zeros after decimal point
    filament_str.erase(filament_str.find_last_not_of('0') + 1, std::string::npos);
    if (!filament_str.empty() && filament_str.back() == '.') filament_str.pop_back();
    block << ";EXTRUDER_TRAIN." << idx << ".MATERIAL.VOLUME_USED:" << filament_str << "\n";
    
    // Use GUID from extruder data, or leave empty if not available
    // The caller should have set this via set_extruder_data()
    block << ";EXTRUDER_TRAIN." << idx << ".MATERIAL.GUID:" << data.material_guid;
    // No trailing \n - template continues with {{extruder_block}} which provides NOZZLE info
    
    return block.str();
}

std::string UFPWriter::generate_header(const GCodeMetadata& meta) {
    auto format_bound = [](double value) {
        std::ostringstream oss;
        if (std::fabs(value - std::round(value)) < 1e-6)
            oss << static_cast<long long>(std::llround(value));
        else
            oss << std::fixed << std::setprecision(3) << value;
        return oss.str();
    };

    std::map<std::string, std::string> values;
    values["flavor"] = m_config.gcode_metadata.flavor;
    values["generator_name"] = m_config.gcode_metadata.generator_name;
    values["generator_version"] = m_config.gcode_metadata.generator_version;
    values["build_date"] = generate_build_date();
    values["target_machine"] = m_config.target_machine;
    values["bed_temp"] = std::to_string(meta.bed_temp);
    values["print_time"] = std::to_string(meta.duration_s);
    values["print_size_min_x"] = format_bound(meta.min_x);
    values["print_size_min_y"] = format_bound(meta.min_y);
    values["print_size_min_z"] = format_bound(meta.min_z);
    values["print_size_max_x"] = format_bound(meta.max_x);
    values["print_size_max_y"] = format_bound(meta.max_y);
    values["print_size_max_z"] = format_bound(meta.max_z);
    values["build_volume_temp"] = "28";
    values["print_groups"] = "1";
    values["slice_uuid"] = meta.slice_uuid;
    
    const auto& extruder_variants = m_context.get_extruder_variants();
    const auto& extruders = m_context.get_extruder_data();
    const std::array<ExtruderData, 2> resolved_extruders = {
        resolve_extruder_data(0, extruders, meta),
        resolve_extruder_data(1, extruders, meta)
    };

    // Generate nozzle metadata for the extruders that actually consumed filament.
    std::string extruder_block;
    bool has_nozzle_metadata = false;
    for (size_t i = 0; i < resolved_extruders.size(); ++i) {
        if (!extruder_is_used(resolved_extruders[i])) {
            continue;
        }

        std::string variant = (i < extruder_variants.size() && !extruder_variants[i].empty()) ? extruder_variants[i] : "AA 0.4";
        auto nozzle_info = get_nozzle_info(variant);
        extruder_block += ";EXTRUDER_TRAIN." + std::to_string(i) + ".NOZZLE.DIAMETER:" + nozzle_info.first + "\n";
        extruder_block += ";EXTRUDER_TRAIN." + std::to_string(i) + ".NOZZLE.NAME:" + nozzle_info.second + "\n";
        has_nozzle_metadata = true;
    }

    if (!has_nozzle_metadata) {
        std::string fallback_variant = (!extruder_variants.empty() && !extruder_variants[0].empty()) ? extruder_variants[0] : "AA 0.4";
        auto nozzle_info = get_nozzle_info(fallback_variant);
        extruder_block = ";EXTRUDER_TRAIN.0.NOZZLE.DIAMETER:" + nozzle_info.first + "\n";
        extruder_block += ";EXTRUDER_TRAIN.0.NOZZLE.NAME:" + nozzle_info.second + "\n";
    }
    values["extruder_block"] = extruder_block;

    // Generate per-extruder metadata blocks (temperature, material GUID, volume)
    // only for the extruders that actually consumed filament.
    values["extruder0_block"] = generate_extruder_block(0, resolved_extruders[0]);
    values["extruder1_block"] = generate_extruder_block(1, resolved_extruders[1]);
    
    return substitute_template(m_config.header_template_content, values);
}

std::string UFPWriter::generate_slicemetadata_json(const GCodeMetadata& meta) {
    // Load full Cura template and patch dynamic values using nlohmann/json
    namespace fs = boost::filesystem;
    using json = nlohmann::json;
    
    // Find template file - try multiple locations
    std::vector<std::string> template_paths = {
        (fs::path(Slic3r::resources_dir()) / "formats" / "ufp" / "slicemetadata_template.json").string(),
        (fs::path("resources") / "formats" / "ufp" / "slicemetadata_template.json").string(),
        "slicemetadata_template.json"
    };
    
    std::string template_content;
    bool found = false;
    for (const auto& path : template_paths) {
        boost::nowide::ifstream file(path);
        if (file.is_open()) {
            template_content = std::string((std::istreambuf_iterator<char>(file)),
                                           std::istreambuf_iterator<char>());
            found = true;
            break;
        }
    }
    
    if (!found) {
        BOOST_LOG_TRIVIAL(warning) << "UFPWriter: Could not find slicemetadata_template.json, using minimal fallback";
        return "{\"machine_id\": \"" + m_config.target_machine + "\", \"version\": 1}";
    }
    
    try {
        // Parse template
        json root = json::parse(template_content);
        const auto& raw_extruders = m_context.get_extruder_data();
        const std::array<ExtruderData, 2> extruders = {
            resolve_extruder_data(0, raw_extruders, meta),
            resolve_extruder_data(1, raw_extruders, meta)
        };
        const bool has_per_extruder_usage = extruder_is_used(raw_extruders[0]) || extruder_is_used(raw_extruders[1]);
        auto ensure_array_size = [](json& array, size_t size) {
            if (!array.is_array()) {
                array = json::array();
            }
            while (array.size() < size) {
                array.push_back(0.0);
            }
        };
        auto filament_length_for_index = [&](size_t idx) {
            if (raw_extruders[idx].filament_mm > 0.0) {
                return raw_extruders[idx].filament_mm;
            }
            return (idx == 0 && !has_per_extruder_usage) ? meta.filament_mm : 0.0;
        };
        auto filament_weight_for_index = [&](size_t idx) {
            if (raw_extruders[idx].filament_g > 0.0) {
                return raw_extruders[idx].filament_g;
            }
            return (idx == 0 && !has_per_extruder_usage) ? meta.filament_g : 0.0;
        };
        auto material_guid_for_index = [&](size_t idx) -> std::string {
            if (!extruders[idx].material_guid.empty()) {
                return extruders[idx].material_guid;
            }
            return idx == 0 ? meta.material_guid : std::string();
        };
        auto material_type_for_index = [&](size_t idx) -> std::string {
            if (!extruders[idx].material_name.empty()) {
                return extruders[idx].material_name;
            }
            return idx == 0 ? meta.material_type : std::string();
        };
        auto extruder_temp_for_index = [&](size_t idx) {
            if (extruders[idx].extruder_temp > 0) {
                return extruders[idx].extruder_temp;
            }
            return idx == 0 ? meta.extruder_temp : 0;
        };
        auto ensure_settings_object = [&](const char* section_name) -> json& {
            if (!root.contains(section_name) || !root[section_name].is_object()) {
                root[section_name] = json::object();
            }
            if (!root[section_name].contains("changes") || !root[section_name]["changes"].is_object()) {
                root[section_name]["changes"] = json::object();
            }
            if (!root[section_name].contains("all_settings") || !root[section_name]["all_settings"].is_object()) {
                if (root.contains("extruder_0") && root["extruder_0"].contains("all_settings") && root["extruder_0"]["all_settings"].is_object()) {
                    root[section_name]["all_settings"] = root["extruder_0"]["all_settings"];
                } else {
                    root[section_name]["all_settings"] = json::object();
                }
            }
            return root[section_name]["all_settings"];
        };
        
        // Patch material with proper types (numbers, not strings)
        if (root.contains("material")) {
            if (root["material"].contains("length")) {
                auto& lengths = root["material"]["length"];
                ensure_array_size(lengths, 2);
                lengths[0] = filament_length_for_index(0);
                lengths[1] = filament_length_for_index(1);
            }
            if (root["material"].contains("weight")) {
                auto& weights = root["material"]["weight"];
                ensure_array_size(weights, 2);
                weights[0] = filament_weight_for_index(0);
                weights[1] = filament_weight_for_index(1);
            }
        }
        
        // Patch quality
        if (root.contains("quality")) {
            root["quality"]["layer_height"] = meta.layer_height;
        }
        
        // Patch global settings with proper types
        if (root.contains("global") && root["global"].contains("all_settings")) {
            auto& settings = root["global"]["all_settings"];
            settings["material_guid"] = material_guid_for_index(0);
            settings["material_type"] = material_type_for_index(0);
            settings["layer_height"] = meta.layer_height;
            settings["infill_sparse_density"] = meta.infill_percent;
            settings["material_bed_temperature"] = meta.bed_temp;
            settings["material_bed_temperature_layer_0"] = meta.bed_temp;
            settings["material_print_temperature"] = extruder_temp_for_index(0);
            settings["material_print_temperature_layer_0"] = extruder_temp_for_index(0);
            settings["machine_name"] = m_config.target_machine;
        }
        
        auto patch_extruder_settings = [&](size_t idx, const char* section_name) {
            auto& settings = ensure_settings_object(section_name);
            settings["material_guid"] = material_guid_for_index(idx);
            settings["material_type"] = material_type_for_index(idx);
            settings["layer_height"] = meta.layer_height;
            settings["infill_sparse_density"] = meta.infill_percent;
            settings["material_bed_temperature"] = meta.bed_temp;
            settings["material_bed_temperature_layer_0"] = meta.bed_temp;
            settings["material_print_temperature"] = extruder_temp_for_index(idx);
            settings["material_print_temperature_layer_0"] = extruder_temp_for_index(idx);
            settings["machine_name"] = m_config.target_machine;
        };

        patch_extruder_settings(0, "extruder_0");
        patch_extruder_settings(1, "extruder_1");
        
        // Serialize back to JSON (compact format)
        return root.dump();
        
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "UFPWriter: Failed to patch template: " << e.what() << ", using fallback";
        return "{\"machine_id\": \"" + m_config.target_machine + "\", \"version\": 1}";
    }
}

std::string UFPWriter::generate_ufp_global_json(const GCodeMetadata& meta) {
    // Match native Cura format: just {"version": 1}
    return "{\"version\": 1}";
}

std::string UFPWriter::generate_material_xml(const ExtruderData& extruder, const GCodeMetadata& meta) {
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml << "<fdmmaterial version=\"1.3\" xmlns=\"http://www.ultimaker.com/material\">\n";
    xml << "  <metadata>\n";
    xml << "    <name>\n";
    
    std::string brand = "Generic";
    if (!extruder.brand.empty())
        brand = extruder.brand;
    xml << "      <brand>" << brand << "</brand>\n";
    
    std::string material_type = extruder.material_name;
    if (material_type.empty())
        material_type = !meta.material_type.empty() ? meta.material_type : meta.material_name;
    xml << "      <material>" << material_type << "</material>\n";
    
    // Color is not available in extruder data, use "Generic" as default
    xml << "      <color>Generic</color>\n";
    xml << "    </name>\n";
    const std::string material_guid = !extruder.material_guid.empty() ? extruder.material_guid : meta.material_guid;
    xml << "    <GUID>" << material_guid << "</GUID>\n";
    xml << "    <version>1</version>\n";
    xml << "  </metadata>\n";
    xml << "</fdmmaterial>";
    return xml.str();
}

std::string UFPWriter::generate_content_types_xml() {
    return R"(<?xml version="1.0" encoding="UTF-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml" />
  <Default Extension="gcode" ContentType="text/x-gcode" />
  <Default Extension="png" ContentType="image/png" />
  <Default Extension="json" ContentType="application/json" />
  <Default Extension="xml.fdm_material" ContentType="application/x-ultimaker-material-profile" />
</Types>)";
}

std::string UFPWriter::generate_rels_xml() {
    // Match native Cura: only 2 relationships (gcode and opc_metadata)
    return R"(<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Target="/3D/model.gcode" Type="http://schemas.ultimaker.org/package/2018/relationships/gcode" Id="rel0" />
  <Relationship Target="/Metadata/UFP_Global.json" Type="http://schemas.ultimaker.org/package/2018/relationships/opc_metadata" Id="rel1" />
</Relationships>)";
}

std::string UFPWriter::generate_gcode_rels_xml(bool has_thumbnail, const std::vector<std::string>& material_filenames) {
    // Match native Cura format: Target first, then Type, UTF-8 encoding
    std::string rels = R"(<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)";
    
    int rel_id = 0;
    if (has_thumbnail) {
        rels += "\n  <Relationship Target=\"/Metadata/thumbnail.png\" Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/thumbnail\" Id=\"rel" + std::to_string(rel_id) + "\" />";
        rel_id++;
    }
    
    for (const std::string& material_filename : material_filenames) {
        rels += "\n  <Relationship Target=\"/Materials/" + material_filename + "\" Type=\"http://schemas.ultimaker.org/package/2018/relationships/material\" Id=\"rel" + std::to_string(rel_id) + "\" />";
        rel_id++;
    }
    
    rels += "\n</Relationships>";
    return rels;
}

std::string UFPWriter::generate_build_date() {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d");
    return oss.str();
}

} // namespace Slic3r
