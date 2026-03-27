#include "MakerBotWriter.hpp"
#include "../miniz_extension.hpp"
#include "../Utils.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace Slic3r {

std::string MakerBotWriter::generate_header(const GCodeMetadata& meta) {
    std::ostringstream header;
    
    if (m_config.header_template == "griffin") {
        // Griffin flavor header (Sketch Small style)
        header << ";START_OF_HEADER\n";
        header << ";HEADER_VERSION:0.1\n";
        header << ";FLAVOR:Griffin\n";
        header << ";PRINT.TIME:" << meta.duration_s << "\n";
        header << ";SLICE_UUID:" << meta.slice_uuid << "\n";
        header << ";END_OF_HEADER\n\n";
    } else if (m_config.header_template == "marlin") {
        // Marlin flavor header (Sketch Sprint style)
        double filament_m = meta.filament_mm / 1000.0;
        header << ";FLAVOR:Marlin\n";
        header << ";TIME:" << meta.duration_s << "\n";
        header << ";Filament used: " << std::fixed << std::setprecision(6) << filament_m << "m\n";
        header << ";Layer height: " << meta.layer_height << "\n";
        header << ";TARGET_MACHINE.NAME:" << m_config.target_machine << "\n\n";
    } else {
        // Fallback to Griffin
        header << ";START_OF_HEADER\n";
        header << ";HEADER_VERSION:0.1\n";
        header << ";FLAVOR:Griffin\n";
        header << ";PRINT.TIME:" << meta.duration_s << "\n";
        header << ";SLICE_UUID:" << meta.slice_uuid << "\n";
        header << ";END_OF_HEADER\n\n";
    }
    
    return header.str();
}

bool MakerBotWriter::write_container(const GCodeMetadata& meta, const std::string& gcode_content, const std::string& output_path) {
    mz_zip_archive archive;
    mz_zip_zero_struct(&archive);
    
    if (!open_zip_writer(&archive, output_path)) {
        BOOST_LOG_TRIVIAL(error) << "MakerBotWriter::write_container: Failed to open zip writer";
        return false;
    }
    
    auto cleanup = [&]() {
        close_zip_writer(&archive);
    };
    
    // 1. print.gcode
    if (!mz_zip_writer_add_mem(&archive, "print.gcode",
                              gcode_content.c_str(), gcode_content.length(),
                              MZ_DEFAULT_COMPRESSION)) {
        BOOST_LOG_TRIVIAL(error) << "MakerBotWriter::write_container: Failed to add print.gcode";
        cleanup();
        return false;
    }
    
    // 2. meta.json
    std::string meta_json = generate_meta_json(meta);
    if (!mz_zip_writer_add_mem(&archive, "meta.json",
                              meta_json.c_str(), meta_json.length(),
                              MZ_DEFAULT_COMPRESSION)) {
        BOOST_LOG_TRIVIAL(error) << "MakerBotWriter::write_container: Failed to add meta.json";
        cleanup();
        return false;
    }
    
    // 3. slicemetadata.json
    std::string slicemetadata = generate_slicemetadata_json(meta);
    if (!mz_zip_writer_add_mem(&archive, "slicemetadata.json",
                              slicemetadata.c_str(), slicemetadata.length(),
                              MZ_DEFAULT_COMPRESSION)) {
        BOOST_LOG_TRIVIAL(error) << "MakerBotWriter::write_container: Failed to add slicemetadata.json";
        cleanup();
        return false;
    }
    
    // 4. Thumbnails (passed directly via set_thumbnail_data(), never extracted from gcode)
    // IMPORTANT: Thumbnails should NEVER be embedded in gcode comments - they are passed separately
    if (has_thumbnail_data()) {
        BOOST_LOG_TRIVIAL(info) << "MakerBotWriter::write_container: Adding thumbnail, size=" << m_thumbnail_data.size();
        
        // Determine naming convention based on printer type
        // sketch_small (griffin): 120/320/640 -> isometric_thumbnail, others -> thumbnail
        // sketch_sprint (marlin): ALL -> isometric_thumbnail
        bool is_sprint = (m_config.header_template == "marlin");
        
        std::string filename;
        if (is_sprint) {
            // Sprint: use isometric naming
            filename = "isometric_thumbnail_320x320.png";
        } else {
            // Small: use regular thumbnail naming
            filename = "thumbnail_320x320.png";
        }
        
        if (!mz_zip_writer_add_mem(&archive, filename.c_str(),
                                   m_thumbnail_data.data(), m_thumbnail_data.size(),
                                   MZ_DEFAULT_COMPRESSION)) {
            BOOST_LOG_TRIVIAL(warning) << "MakerBotWriter::write_container: Failed to add thumbnail";
        } else {
            BOOST_LOG_TRIVIAL(info) << "MakerBotWriter::write_container: Added thumbnail as " << filename;
        }
    } else {
        BOOST_LOG_TRIVIAL(info) << "MakerBotWriter::write_container: No thumbnail data available";
    }
    
    // Finalize archive
    if (!mz_zip_writer_finalize_archive(&archive)) {
        BOOST_LOG_TRIVIAL(error) << "MakerBotWriter::write_container: Failed to finalize archive";
        cleanup();
        return false;
    }
    
    cleanup();
    BOOST_LOG_TRIVIAL(info) << "MakerBotWriter::write_container: SUCCESS";
    return true;
}

std::string MakerBotWriter::generate_meta_json(const GCodeMetadata& meta) {
    std::ostringstream json;
    json << "{\n";
    json << "  \"bot_type\": \"" << m_config.bot_type << "\",\n";
    json << "  \"platform_temperature\": " << meta.bed_temp << ",\n";
    json << "  \"build_plane_temperature\": " << m_config.build_plane_temperature << ",\n";
    json << "  \"commanded_duration_s\": " << meta.duration_s << ",\n";
    json << "  \"duration_s\": " << meta.duration_s << ",\n";
    json << "  \"extrusion_distance_mm\": " << std::fixed << std::setprecision(1) << meta.filament_mm << ",\n";
    json << "  \"extrusion_distances_mm\": [" << std::fixed << std::setprecision(1) << meta.filament_mm << "],\n";
    json << "  \"extrusion_mass_g\": " << std::fixed << std::setprecision(6) << meta.filament_g << ",\n";
    json << "  \"extrusion_masses_g\": [" << std::fixed << std::setprecision(6) << meta.filament_g << "],\n";
    json << "  \"uuid\": \"" << meta.material_guid << "\",\n";
    json << "  \"material\": \"" << meta.material_name << "\",\n";
    json << "  \"materials\": [\"" << meta.material_name << "\"],\n";
    json << "  \"extruder_temperature\": " << meta.extruder_temp << ",\n";
    json << "  \"extruder_temperatures\": [" << meta.extruder_temp << "],\n";
    json << "  \"tool_type\": \"" << m_config.tool_type << "\",\n";
    json << "  \"tool_types\": [\"" << m_config.tool_type << "\"],\n";
    json << "  \"version\": \"" << m_config.version << "\",\n";
    json << "  \"model_counts\": [{\"count\": 1, \"name\": \"instance0\"}],\n";
    json << "  \"preferences\": {\n";
    json << "    \"instance0\": {\n";
    json << "      \"machineBounds\": [" << m_config.machine_bounds[0] << ", " << m_config.machine_bounds[1] << ", " << m_config.machine_bounds[2] << ", " << m_config.machine_bounds[3] << "],\n";
    json << "      \"printMode\": \"default\"\n";
    json << "    }\n";
    json << "  },\n";
    
    // Add extruderProfiles for Sprint variant
    if (m_config.header_template == "marlin") {
        json << "  \"extruderProfiles\": [{ \"nozzle_diameter\": 0.4, \"tool_type\": \"" << m_config.tool_type << "\" }],\n";
    }
    
    json << "  \"miracle_config\": {\n";
    json << "    \"gaggles\": {\"instance0\": {}},\n";
    json << "    \"curaengine_version\": \"" << m_config.miracle_config.curaengine_version << "\",\n";
    json << "    \"curaengine_commit_hash\": \"" << m_config.miracle_config.curaengine_commit_hash << "\",\n";
    json << "    \"dulcificum_version\": \"" << m_config.miracle_config.dulcificum_version << "\",\n";
    json << "    \"dulcificum_commit_hash\": \"" << m_config.miracle_config.dulcificum_commit_hash << "\",\n";
    json << "    \"makerbot_writer_version\": \"" << m_config.miracle_config.makerbot_writer_version << "\",\n";
    json << "    \"pyDulcificum_version\": \"" << m_config.miracle_config.pyDulcificum_version << "\"\n";
    json << "  },\n";
    json << "  \"curaengine_version\": \"" << m_config.miracle_config.curaengine_version << "\",\n";
    json << "  \"curaengine_commit_hash\": \"" << m_config.miracle_config.curaengine_commit_hash << "\",\n";
    json << "  \"dulcificum_version\": \"" << m_config.miracle_config.dulcificum_version << "\",\n";
    json << "  \"dulcificum_commit_hash\": \"" << m_config.miracle_config.dulcificum_commit_hash << "\",\n";
    json << "  \"makerbot_writer_version\": \"" << m_config.miracle_config.makerbot_writer_version << "\",\n";
    json << "  \"pyDulcificum_version\": \"" << m_config.miracle_config.pyDulcificum_version << "\"\n";
    json << "}\n";
    return json.str();
}

std::string MakerBotWriter::generate_slicemetadata_json(const GCodeMetadata& meta) {
    namespace fs = boost::filesystem;
    using json = nlohmann::json;
    
    // Determine template based on printer type
    // Sprint (marlin) needs full Cura settings, Small (griffin) is minimal
    std::string template_filename = (m_config.header_template == "marlin") 
        ? "slicemetadata_sprint_template.json" 
        : "slicemetadata_small_template.json";
    
    // Find template file - try multiple locations
    std::vector<std::string> template_paths = {
        (fs::path(Slic3r::resources_dir()) / "formats" / "makerbot" / template_filename).string(),
        (fs::path("resources") / "formats" / "makerbot" / template_filename).string(),
        template_filename
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
    
    // If template not found, fall back to minimal generation
    if (!found) {
        return generate_slicemetadata_json_minimal(meta);
    }
    
    try {
        // Parse template
        json root = json::parse(template_content);
        
        // Patch material values
        if (root.contains("material")) {
            bool use_meters = (m_config.header_template == "marlin");
            double length_value = use_meters ? (meta.filament_mm / 1000.0) : meta.filament_mm;
            
            if (root["material"].contains("length") && root["material"]["length"].is_array() && root["material"]["length"].size() > 0) {
                root["material"]["length"][0] = length_value;
            }
            if (root["material"].contains("weight") && root["material"]["weight"].is_array() && root["material"]["weight"].size() > 0) {
                root["material"]["weight"][0] = meta.filament_g;
            }
        }
        
        // Patch quality settings
        if (root.contains("quality")) {
            root["quality"]["layer_height"] = meta.layer_height;
        }
        
        // Patch global settings
        if (root.contains("global") && root["global"].contains("all_settings")) {
            auto& settings = root["global"]["all_settings"];
            settings["material_guid"] = meta.material_guid;
            settings["material_type"] = meta.material_type;
            settings["layer_height"] = meta.layer_height;
            settings["infill_sparse_density"] = meta.infill_percent;
            settings["material_bed_temperature"] = meta.bed_temp;
            settings["material_print_temperature"] = meta.extruder_temp;
            settings["machine_name"] = m_config.target_machine;
        }
        
        // Patch extruder_0 settings
        if (root.contains("extruder_0") && root["extruder_0"].contains("all_settings")) {
            auto& settings = root["extruder_0"]["all_settings"];
            settings["material_guid"] = meta.material_guid;
            settings["material_type"] = meta.material_type;
            settings["layer_height"] = meta.layer_height;
            settings["infill_sparse_density"] = meta.infill_percent;
            settings["machine_name"] = m_config.target_machine;
        }
        
        return root.dump();
        
    } catch (const std::exception& e) {
        // Fall back to minimal if parsing fails
        return generate_slicemetadata_json_minimal(meta);
    }
}

// Minimal slicemetadata generation (fallback)
std::string MakerBotWriter::generate_slicemetadata_json_minimal(const GCodeMetadata& meta) {
    bool use_meters = (m_config.header_template == "marlin");
    double length_value = use_meters ? (meta.filament_mm / 1000.0) : meta.filament_mm;
    int precision = use_meters ? 5 : 1;
    
    std::ostringstream json;
    json << "{\n";
    json << "  \"material\": {\n";
    json << "    \"length\": [" << std::fixed << std::setprecision(precision) << length_value << "],\n";
    json << "    \"weight\": [" << std::fixed << std::setprecision(6) << meta.filament_g << "],\n";
    json << "    \"cost\": [0.0]\n";
    json << "  },\n";
    json << "  \"quality\": {\n";
    json << "    \"intent_category\": \"default\",\n";
    json << "    \"intent_name\": \"Balanced\",\n";
    json << "    \"profile\": \"Normal\",\n";
    json << "    \"custom_profile\": null,\n";
    json << "    \"layer_height\": " << meta.layer_height << ",\n";
    json << "    \"is_experimental\": false\n";
    json << "  },\n";
    json << "  \"global\": {\n";
    json << "    \"changes\": {},\n";
    json << "    \"all_settings\": {\n";
    json << "      \"material_guid\": \"" << meta.material_guid << "\",\n";
    json << "      \"material_type\": \"" << meta.material_type << "\",\n";
    json << "      \"layer_height\": " << meta.layer_height << ",\n";
    json << "      \"infill_sparse_density\": " << meta.infill_percent << "\n";
    json << "    }\n";
    json << "  },\n";
    json << "  \"extruder_0\": {\n";
    json << "    \"changes\": {},\n";
    json << "    \"all_settings\": {\n";
    json << "      \"machine_name\": \"" << m_config.target_machine << "\",\n";
    json << "      \"material_guid\": \"" << meta.material_guid << "\",\n";
    json << "      \"material_type\": \"" << meta.material_type << "\",\n";
    json << "      \"layer_height\": " << meta.layer_height << ",\n";
    json << "      \"infill_sparse_density\": " << meta.infill_percent << "\n";
    json << "    }\n";
    json << "  }\n";
    json << "}\n";
    return json.str();
}

} // namespace Slic3r
