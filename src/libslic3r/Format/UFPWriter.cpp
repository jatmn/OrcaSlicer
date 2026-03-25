#include "UFPWriter.hpp"
#include "../miniz_extension.hpp"
#include "../Utils.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/beast/core/detail/base64.hpp>
#include <boost/filesystem.hpp>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <fstream>

namespace Slic3r {

// Base64 decode using Boost.Beast
static std::vector<uint8_t> base64_decode(const std::string& encoded) {
    std::string cleaned = encoded;
    boost::algorithm::trim(cleaned);
    // Remove whitespace that might be in the base64
    cleaned.erase(std::remove_if(cleaned.begin(), cleaned.end(), ::isspace), cleaned.end());
    
    std::vector<uint8_t> result;
    result.resize(boost::beast::detail::base64::decoded_size(cleaned.size()));
    auto [len, ec] = boost::beast::detail::base64::decode(result.data(), cleaned.data(), cleaned.size());
    result.resize(len);
    return result;
}

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
    
    BOOST_LOG_TRIVIAL(info) << "UFPWriter::write_container: Zip writer opened successfully, mode=" << archive.m_zip_mode;
    
    BOOST_LOG_TRIVIAL(info) << "UFPWriter::write_container: Zip writer opened successfully";
    
    auto cleanup = [&]() {
        close_zip_writer(&archive);
    };
    
    BOOST_LOG_TRIVIAL(info) << "UFPWriter::write_container: gcode_content size=" << gcode_content.length();
    
    // Debug: check archive state
    BOOST_LOG_TRIVIAL(info) << "UFPWriter::write_container: archive m_zip_mode=" << archive.m_zip_mode 
                           << ", m_archive_size=" << archive.m_archive_size;
    
    if (gcode_content.empty()) {
        BOOST_LOG_TRIVIAL(error) << "UFPWriter::write_container: gcode_content is EMPTY!";
        cleanup();
        return false;
    }
    
    // Write files in exact order matching valid UFP format:
    // 1. /3D/model.gcode
    if (!mz_zip_writer_add_mem(&archive, "3D/model.gcode", 
                              gcode_content.c_str(), gcode_content.length(), 
                              MZ_NO_COMPRESSION)) {
        BOOST_LOG_TRIVIAL(error) << "UFPWriter::write_container: Failed to add model.gcode, m_last_error=" 
                                 << archive.m_last_error;
        cleanup();
        return false;
    }
    
    // 2. /Metadata/thumbnail.png (if available)
    bool has_thumbnail = false;
    if (!meta.thumbnails.empty()) {
        BOOST_LOG_TRIVIAL(info) << "UFPWriter::write_container: Has thumbnails, size=" << meta.thumbnails.size();
        auto it = meta.thumbnails.find("320x320");
        if (it == meta.thumbnails.end()) {
            it = meta.thumbnails.begin();
        }
        
        if (it != meta.thumbnails.end()) {
            // Decode base64 and write thumbnail
            try {
                std::vector<uint8_t> png_data = base64_decode(it->second);
                if (!png_data.empty()) {
                    if (!mz_zip_writer_add_mem(&archive, "Metadata/thumbnail.png",
                                              png_data.data(), png_data.size(),
                                              MZ_NO_COMPRESSION)) {
                        BOOST_LOG_TRIVIAL(warning) << "UFPWriter::write_container: Failed to add thumbnail";
                    } else {
                        has_thumbnail = true;
                        BOOST_LOG_TRIVIAL(info) << "UFPWriter::write_container: Added thumbnail, size=" << png_data.size();
                    }
                }
            } catch (const std::exception& e) {
                BOOST_LOG_TRIVIAL(warning) << "UFPWriter::write_container: Failed to decode thumbnail: " << e.what();
            }
        }
    }
    
    // 3. /3D/_rels/model.gcode.rels
    std::string gcode_rels = generate_gcode_rels_xml(has_thumbnail);
    if (!mz_zip_writer_add_mem(&archive, "3D/_rels/model.gcode.rels",
                              gcode_rels.c_str(), gcode_rels.length(),
                              MZ_NO_COMPRESSION)) {
        BOOST_LOG_TRIVIAL(error) << "UFPWriter::write_container: Failed to add model.gcode.rels";
        cleanup();
        return false;
    }
    
    // 4. /Cura/slicemetadata.json
    std::string slicemetadata = generate_slicemetadata_json(meta);
    if (!mz_zip_writer_add_mem(&archive, "Cura/slicemetadata.json",
                              slicemetadata.c_str(), slicemetadata.length(),
                              MZ_NO_COMPRESSION)) {
        BOOST_LOG_TRIVIAL(error) << "UFPWriter::write_container: Failed to add slicemetadata.json";
        cleanup();
        return false;
    }
    
    // 5. /Metadata/UFP_Global.json
    std::string ufp_global = generate_ufp_global_json(meta);
    if (!mz_zip_writer_add_mem(&archive, "Metadata/UFP_Global.json",
                              ufp_global.c_str(), ufp_global.length(),
                              MZ_NO_COMPRESSION)) {
        BOOST_LOG_TRIVIAL(error) << "UFPWriter::write_container: Failed to add UFP_Global.json";
        cleanup();
        return false;
    }
    
    // 6. /Materials/ultimaker_{material}.xml.fdm_material
    std::string material_filename = "ultimaker_" + meta.material_name + ".xml.fdm_material";
    std::string material_xml = generate_material_xml(meta);
    if (!mz_zip_writer_add_mem(&archive, ("Materials/" + material_filename).c_str(),
                              material_xml.c_str(), material_xml.length(),
                              MZ_NO_COMPRESSION)) {
        BOOST_LOG_TRIVIAL(error) << "UFPWriter::write_container: Failed to add " << material_filename;
        cleanup();
        return false;
    }
    
    // 7. /_rels/.rels
    std::string rels = generate_rels_xml();
    if (!mz_zip_writer_add_mem(&archive, "_rels/.rels",
                              rels.c_str(), rels.length(),
                              MZ_NO_COMPRESSION)) {
        BOOST_LOG_TRIVIAL(error) << "UFPWriter::write_container: Failed to add .rels";
        cleanup();
        return false;
    }
    
    // 8. /[Content_Types].xml
    std::string content_types = generate_content_types_xml();
    if (!mz_zip_writer_add_mem(&archive, "[Content_Types].xml",
                              content_types.c_str(), content_types.length(),
                              MZ_NO_COMPRESSION)) {
        BOOST_LOG_TRIVIAL(error) << "UFPWriter::write_container: Failed to add [Content_Types].xml";
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
    // Override parsed metadata with injected values ONLY if they are non-zero
    // (zero means stats weren't captured; keep the parsed values from G-code comments)
    if (m_has_stats) {
        BOOST_LOG_TRIVIAL(info) << "UFPWriter::override_metadata: Injected stats - "
                               << "duration_s=" << m_duration_s
                               << ", filament_mm=" << m_filament_mm
                               << ", filament_g=" << m_filament_g
                               << " (parsed: duration_s=" << meta.duration_s
                               << ", filament_mm=" << meta.filament_mm << ")";
        if (m_duration_s > 0)
            meta.duration_s = m_duration_s;
        if (m_filament_mm > 0)
            meta.filament_mm = m_filament_mm;
        if (m_filament_g > 0)
            meta.filament_g = m_filament_g;
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

std::string UFPWriter::generate_header(const GCodeMetadata& meta) {
    std::map<std::string, std::string> values;
    values["flavor"] = m_config.gcode_metadata.flavor;
    values["generator_name"] = m_config.gcode_metadata.generator_name;
    values["generator_version"] = m_config.gcode_metadata.generator_version;
    values["build_date"] = generate_build_date();
    values["target_machine"] = m_config.target_machine;
    values["extruder_temp"] = std::to_string(meta.extruder_temp);
    // Format filament with decimals if there are any
    std::ostringstream filament_stream;
    filament_stream << std::fixed << std::setprecision(2) << meta.filament_mm;
    std::string filament_str = filament_stream.str();
    // Remove trailing zeros after decimal point
    filament_str.erase(filament_str.find_last_not_of('0') + 1, std::string::npos);
    if (filament_str.back() == '.') filament_str.pop_back();
    values["filament_volume"] = filament_str;
    values["material_guid"] = meta.material_guid;
    values["nozzle_diameter"] = "0.4";
    values["nozzle_name"] = "AA+ 0.4";
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
    // Match native Cura format
    return "{\"metadata\": {\"objects\": [{\"name\": \"model.stl\"}]}}";
}

std::string UFPWriter::generate_material_xml(const GCodeMetadata& meta) {
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"utf-8\"?\u003e\n";
    xml << "<fdmmaterial version=\"1.3\" xmlns=\"http://www.ultimaker.com/material\"\u003e\n";
    xml << "  <metadata\u003e\n";
    xml << "    <name\u003e\n";
    xml << "      <brand\u003eGeneric</brand\u003e\n";
    xml << "      <material\u003e" << meta.material_type << "</material\u003e\n";
    xml << "      <color\u003eGeneric</color\u003e\n";
    xml << "    </name\u003e\n";
    xml << "    <GUID\u003e" << meta.material_guid << "</GUID\u003e\n";
    xml << "    <version\u003e1</version\u003e\n";
    xml << "  </metadata\u003e\n";
    xml << "</fdmmaterial\u003e";
    return xml.str();
}

std::string UFPWriter::generate_content_types_xml() {
    return R"(<?xml version="1.0" encoding="utf-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml" />
  <Default Extension="gcode" ContentType="text/x-gcode" />
  <Default Extension="png" ContentType="image/png" />
  <Default Extension="json" ContentType="application/json" />
  <Default Extension="fdm_material" ContentType="text/xml" />
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

std::string UFPWriter::generate_gcode_rels_xml(bool has_thumbnail) {
    // Match native Cura format: Target first, then Type, UTF-8 encoding
    std::string rels = R"(<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)";
    
    int rel_id = 0;
    if (has_thumbnail) {
        rels += "\n  <Relationship Target=\"/Metadata/thumbnail.png\" Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/thumbnail\" Id=\"rel" + std::to_string(rel_id) + "\" />";
        rel_id++;
    }
    
    // Add material relationship(s) - Cura native has specific material filenames
    // Use PLA as default, should be dynamic based on material_type
    rels += "\n  <Relationship Target=\"/Materials/ultimaker_pla.xml.fdm_material\" Type=\"http://schemas.ultimaker.org/package/2018/relationships/material\" Id=\"rel" + std::to_string(rel_id) + "\" />";
    
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
