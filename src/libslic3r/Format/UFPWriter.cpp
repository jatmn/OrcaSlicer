#include "UFPWriter.hpp"
#include "../miniz_extension.hpp"
#include "../Utils.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/filesystem.hpp>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <fstream>

namespace Slic3r {

bool UFPWriter::write_container(const GCodeMetadata& meta, const std::string& gcode_content, const std::string& output_path) {
    BOOST_LOG_TRIVIAL(info) << "UFPWriter::write_container: output=" << output_path;
    
    // Check if output file already exists and can be written
    boost::filesystem::path out_path(output_path);
    boost::system::error_code ec;
    if (boost::filesystem::exists(out_path, ec)) {
        BOOST_LOG_TRIVIAL(info) << "UFPWriter::write_container: Output file exists, will overwrite";
    }
    
    mz_zip_archive archive;
    mz_zip_zero_struct(&archive);
    
    // Ensure output path uses forward slashes for Windows compatibility with miniz
    std::string normalized_path = output_path;
    std::replace(normalized_path.begin(), normalized_path.end(), '\\', '/');
    
    BOOST_LOG_TRIVIAL(info) << "UFPWriter::write_container: Opening zip for writing, normalized_path=" << normalized_path;
    
    if (!open_zip_writer(&archive, normalized_path)) {
        BOOST_LOG_TRIVIAL(error) << "UFPWriter::write_container: Failed to open zip writer, last_error=" 
                                 << archive.m_last_error;
        return false;
    }
    
    BOOST_LOG_TRIVIAL(info) << "UFPWriter::write_container: Zip writer opened successfully, mode=" << archive.m_zip_mode 
                           << ", archive_size=" << archive.m_archive_size;
    
    // Debug: verify archive is ready
    if (archive.m_zip_mode != MZ_ZIP_MODE_WRITING) {
        BOOST_LOG_TRIVIAL(error) << "UFPWriter::write_container: Archive not in WRITE mode! mode=" << archive.m_zip_mode;
        close_zip_writer(&archive);
        return false;
    }
    
    BOOST_LOG_TRIVIAL(info) << "UFPWriter::write_container: Archive is ready for writing";
    
    auto cleanup = [&]() {
        close_zip_writer(&archive);
    };
    
    BOOST_LOG_TRIVIAL(info) << "UFPWriter::write_container: gcode_content size=" << gcode_content.length();
    
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
    BOOST_LOG_TRIVIAL(info) << "UFPWriter::write_container: Adding /3D/model.gcode, size=" << gcode_content.length();
    
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
        const auto& [thumbnail_data, filename] = thumbnails[0];
        BOOST_LOG_TRIVIAL(info) << "UFPWriter::write_container: Writing thumbnail from context, size=" << thumbnail_data.size();
        if (!mz_zip_writer_add_mem_ex(&archive, "/Metadata/thumbnail.png",
                                      thumbnail_data.data(), thumbnail_data.size(),
                                      nullptr, 0, MZ_NO_COMPRESSION, 0, 0)) {
            BOOST_LOG_TRIVIAL(warning) << "UFPWriter::write_container: Failed to add thumbnail";
        } else {
            has_thumbnail = true;
            BOOST_LOG_TRIVIAL(info) << "UFPWriter::write_container: Added thumbnail from context";
        }
    } else {
        BOOST_LOG_TRIVIAL(info) << "UFPWriter::write_container: No thumbnail data available";
    }
    
    // Prepare material filename (needed for UFP_Global, material, and gcode.rels)
    std::string material_type_lower = meta.material_type;
    std::transform(material_type_lower.begin(), material_type_lower.end(), material_type_lower.begin(), ::tolower);
    // Replace spaces with underscores for filename compatibility
    std::replace(material_type_lower.begin(), material_type_lower.end(), ' ', '_');
    std::string material_filename = "ultimaker_" + material_type_lower + ".xml.fdm_material";

    // 4. /Metadata/UFP_Global.json - with leading slash
    std::string ufp_global = generate_ufp_global_json(meta);
    if (!mz_zip_writer_add_mem_ex(&archive, "/Metadata/UFP_Global.json",
                              ufp_global.c_str(), ufp_global.length(),
                              nullptr, 0, MZ_NO_COMPRESSION, 0, 0)) {
        BOOST_LOG_TRIVIAL(error) << "UFPWriter::write_container: Failed to add UFP_Global.json";
        cleanup();
        return false;
    }
    
    // 5. /Materials/ultimaker_{material_type}.xml.fdm_material - Cura-style filename format
    std::string material_xml = generate_material_xml(meta);
    if (!mz_zip_writer_add_mem_ex(&archive, ("/Materials/" + material_filename).c_str(),
                              material_xml.c_str(), material_xml.length(),
                              nullptr, 0, MZ_NO_COMPRESSION, 0, 0)) {
        BOOST_LOG_TRIVIAL(error) << "UFPWriter::write_container: Failed to add " << material_filename;
        cleanup();
        return false;
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
    std::string gcode_rels = generate_gcode_rels_xml(has_thumbnail, material_filename);
    if (!mz_zip_writer_add_mem_ex(&archive, "/3D/_rels/model.gcode.rels",
                              gcode_rels.c_str(), gcode_rels.length(),
                              nullptr, 0, MZ_NO_COMPRESSION, 0, 0)) {
        BOOST_LOG_TRIVIAL(error) << "UFPWriter::write_container: Failed to add model.gcode.rels";
        cleanup();
        return false;
    }
    
    // Finalize archive
    BOOST_LOG_TRIVIAL(info) << "UFPWriter::write_container: Finalizing archive";
    if (!mz_zip_writer_finalize_archive(&archive)) {
        BOOST_LOG_TRIVIAL(error) << "UFPWriter::write_container: Failed to finalize archive";
        cleanup();
        return false;
    }
    
    cleanup();
    BOOST_LOG_TRIVIAL(info) << "UFPWriter::write_container: SUCCESS";
    return true;
}

void UFPWriter::override_metadata(GCodeMetadata& meta) {
    // Propagate extruder 0 data to extruder 1 if needed (for dual-extruder with same material)
    m_context.propagate_extruder_data_if_needed();

    // Override parsed metadata with injected values ONLY if they are non-zero
    // (zero means stats weren't captured; keep the parsed values from G-code comments)
    if (m_context.has_print_stats()) {
        BOOST_LOG_TRIVIAL(info) << "UFPWriter::override_metadata: Injected stats - "
                               << "duration_s=" << m_context.get_duration_s()
                               << ", filament_mm=" << m_context.get_filament_mm()
                               << ", filament_g=" << m_context.get_filament_g()
                               << " (parsed: duration_s=" << meta.duration_s
                               << ", filament_mm=" << meta.filament_mm << ")";
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
    
    // First handle extruder 0: always use extruder GUID if available
    if (!extruders[0].material_guid.empty()) {
        // Extruder has GUID, use it to override metadata
        if (meta.material_guid != extruders[0].material_guid) {
            BOOST_LOG_TRIVIAL(info) << "UFPWriter::override_metadata: Using extruder 0 GUID '" 
                                   << extruders[0].material_guid 
                                   << "' (overriding metadata GUID '" << meta.material_guid << "')";
            meta.material_guid = extruders[0].material_guid;
        }
    }
    
    // Also update material type from extruder data if available
    // This ensures "PLA Tough" is correctly identified instead of default "PLA"
    if (!extruders[0].material_name.empty()) {
        if (meta.material_type != extruders[0].material_name) {
            BOOST_LOG_TRIVIAL(info) << "UFPWriter::override_metadata: Using extruder 0 material name '" 
                                   << extruders[0].material_name 
                                   << "' (overriding metadata material type '" << meta.material_type << "')";
            meta.material_type = extruders[0].material_name;
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
        BOOST_LOG_TRIVIAL(info) << "UFPWriter::override_metadata: Using machine bounds - "
                               << "X[" << meta.min_x << "," << meta.max_x << "] "
                               << "Y[" << meta.min_y << "," << meta.max_y << "] "
                               << "Z[" << meta.min_z << "," << meta.max_z << "]";
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
                    BOOST_LOG_TRIVIAL(info) << "UFPWriter: Loaded " << variants.size() << " nozzle variants from " << path;
                    return variants;
                }
            } catch (const std::exception& e) {
                BOOST_LOG_TRIVIAL(warning) << "UFPWriter: Failed to parse nozzle_variants.json: " << e.what();
            }
        }
    }
    
    BOOST_LOG_TRIVIAL(warning) << "UFPWriter: Could not find nozzle_variants.json, using fallback parsing";
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
    std::map<std::string, std::string> values;
    values["flavor"] = m_config.gcode_metadata.flavor;
    values["generator_name"] = m_config.gcode_metadata.generator_name;
    values["generator_version"] = m_config.gcode_metadata.generator_version;
    values["build_date"] = generate_build_date();
    values["target_machine"] = m_config.target_machine;
    values["bed_temp"] = std::to_string(meta.bed_temp);
    values["print_time"] = std::to_string(meta.duration_s);
    // Use fixed machine bounds for Ultimaker S6 instead of computed print bounds
    values["print_size_min_x"] = "0";
    values["print_size_min_y"] = "0";
    values["print_size_min_z"] = "0";
    values["print_size_max_x"] = "330";
    values["print_size_max_y"] = "240";
    values["print_size_max_z"] = "300";
    values["build_volume_temp"] = "28";
    values["print_groups"] = "1";
    values["slice_uuid"] = meta.slice_uuid;
    
    // Generate extruder block based on active extruders only (nozzle info)
    std::string extruder_block;
    const auto& extruder_variants = m_context.get_extruder_variants();
    const auto& extruders = m_context.get_extruder_data();
    BOOST_LOG_TRIVIAL(info) << "UFPWriter::generate_header: extruder_variants.size()=" << extruder_variants.size()
                           << ", extruder0 filament_mm=" << extruders[0].filament_mm
                           << ", extruder1 filament_mm=" << extruders[1].filament_mm;
    if (!extruder_variants.empty()) {
        for (size_t i = 0; i < extruder_variants.size() && i < 2; ++i) {
            BOOST_LOG_TRIVIAL(info) << "UFPWriter::generate_header: Checking extruder " << i
                                   << ", variant=" << extruder_variants[i]
                                   << ", filament_mm=" << extruders[i].filament_mm;
            // Only include nozzle info if extruder was actually used
            if (extruders[i].filament_mm > 0.0) {
                const std::string& variant = extruder_variants[i];
                auto nozzle_info = get_nozzle_info(variant);
                extruder_block += ";EXTRUDER_TRAIN." + std::to_string(i) + ".NOZZLE.DIAMETER:" + nozzle_info.first + "\n";
                extruder_block += ";EXTRUDER_TRAIN." + std::to_string(i) + ".NOZZLE.NAME:" + nozzle_info.second + "\n";
                BOOST_LOG_TRIVIAL(info) << "UFPWriter::generate_header: Added NOZZLE info for extruder " << i;
            } else {
                BOOST_LOG_TRIVIAL(info) << "UFPWriter::generate_header: Skipping NOZZLE info for inactive extruder " << i;
            }
        }
        BOOST_LOG_TRIVIAL(info) << "UFPWriter: Generated extruder block";
    } else {
        // Fallback for backward compatibility - use default values
        extruder_block = ";EXTRUDER_TRAIN.0.NOZZLE.DIAMETER:0.4\n;EXTRUDER_TRAIN.0.NOZZLE.NAME:AA 0.4\n";
        BOOST_LOG_TRIVIAL(info) << "UFPWriter: No extruder variants configured, using default extruder 0";
    }
    values["extruder_block"] = extruder_block;
    
    // Generate per-extruder metadata blocks (temperature, material GUID, volume) - only for active extruders
    values["extruder0_block"] = generate_extruder_block(0, extruders[0]);
    // Only include extruder 1 if it has filament data
    std::string extruder1_block = generate_extruder_block(1, extruders[1]);
    values["extruder1_block"] = extruder1_block;
    
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
            BOOST_LOG_TRIVIAL(info) << "UFPWriter: Loaded slicemetadata template from " << path;
            break;
        }
    }
    
    if (!found) {
        BOOST_LOG_TRIVIAL(error) << "UFPWriter: Could not find slicemetadata_template.json, using minimal fallback";
        return "{\"machine_id\": \"" + m_config.target_machine + "\", \"version\": 1}";
    }
    
    try {
        // Parse template
        json root = json::parse(template_content);
        
        // Patch material with proper types (numbers, not strings)
        if (root.contains("material")) {
            if (root["material"].contains("length") && root["material"]["length"].is_array() && root["material"]["length"].size() > 0) {
                root["material"]["length"][0] = meta.filament_mm;
            }
            if (root["material"].contains("weight") && root["material"]["weight"].is_array() && root["material"]["weight"].size() > 0) {
                root["material"]["weight"][0] = meta.filament_g;
            }
        }
        
        // Patch quality
        if (root.contains("quality")) {
            root["quality"]["layer_height"] = meta.layer_height;
        }
        
        // Patch global settings with proper types
        if (root.contains("global") && root["global"].contains("all_settings")) {
            auto& settings = root["global"]["all_settings"];
            settings["material_guid"] = meta.material_guid;
            settings["material_type"] = meta.material_type;
            settings["layer_height"] = meta.layer_height;
            settings["infill_sparse_density"] = meta.infill_percent;
            settings["material_bed_temperature"] = meta.bed_temp;
            settings["material_bed_temperature_layer_0"] = meta.bed_temp;
            settings["material_print_temperature"] = meta.extruder_temp;
            settings["material_print_temperature_layer_0"] = meta.extruder_temp;
            settings["machine_name"] = m_config.target_machine;
        }
        
        // Patch extruder_0 settings with proper types
        if (root.contains("extruder_0") && root["extruder_0"].contains("all_settings")) {
            auto& settings = root["extruder_0"]["all_settings"];
            settings["material_guid"] = meta.material_guid;
            settings["material_type"] = meta.material_type;
            settings["layer_height"] = meta.layer_height;
            settings["infill_sparse_density"] = meta.infill_percent;
            settings["material_bed_temperature"] = meta.bed_temp;
            settings["material_bed_temperature_layer_0"] = meta.bed_temp;
            settings["material_print_temperature"] = meta.extruder_temp;
            settings["material_print_temperature_layer_0"] = meta.extruder_temp;
            settings["machine_name"] = m_config.target_machine;
        }
        
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

std::string UFPWriter::generate_material_xml(const GCodeMetadata& meta) {
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml << "<fdmmaterial version=\"1.3\" xmlns=\"http://www.ultimaker.com/material\">\n";
    xml << "  <metadata>\n";
    xml << "    <name>\n";
    
    // Use brand from extruder data if available, otherwise default to "Generic"
    const auto& extruders = m_context.get_extruder_data();
    std::string brand = "Generic";
    if (!extruders[0].brand.empty()) {
        brand = extruders[0].brand;
        BOOST_LOG_TRIVIAL(info) << "UFPWriter::generate_material_xml: Using brand from extruder data: " << brand;
    }
    xml << "      <brand>" << brand << "</brand>\n";
    
    // Use material_type from extruder data if available, otherwise from metadata
    std::string material_type = meta.material_type;
    if (!extruders[0].material_name.empty()) {
        material_type = extruders[0].material_name;
    }
    xml << "      <material>" << material_type << "</material>\n";
    
    // Color is not available in extruder data, use "Generic" as default
    xml << "      <color>Generic</color>\n";
    xml << "    </name>\n";
    xml << "    <GUID>" << meta.material_guid << "</GUID>\n";
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

std::string UFPWriter::generate_gcode_rels_xml(bool has_thumbnail, const std::string& material_filename) {
    // Match native Cura format: Target first, then Type, UTF-8 encoding
    std::string rels = R"(<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)";
    
    int rel_id = 0;
    if (has_thumbnail) {
        rels += "\n  <Relationship Target=\"/Metadata/thumbnail.png\" Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/thumbnail\" Id=\"rel" + std::to_string(rel_id) + "\" />";
        rel_id++;
    }
    
    // Add material relationship(s) - match valid Cura format: ultimaker_{material}.xml.fdm_material
    rels += "\n  <Relationship Target=\"/Materials/" + material_filename + "\" Type=\"http://schemas.ultimaker.org/package/2018/relationships/material\" Id=\"rel" + std::to_string(rel_id) + "\" />";
    
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
