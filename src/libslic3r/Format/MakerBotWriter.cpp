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

// Helper to map material name to MakerBot material code
static std::string material_name_to_code(const std::string& name) {
    // Check for known material names (case-insensitive)
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower.find("tough") != std::string::npos) return "im-pla";
    if (lower.find("metallic") != std::string::npos) return "metallic-pla";
    if (lower.find("pla") != std::string::npos) return "pla";
    // fallback: lowercase and replace spaces with hyphens
    std::string code = name;
    std::transform(code.begin(), code.end(), code.begin(), ::tolower);
    std::replace(code.begin(), code.end(), ' ', '-');
    return code;
}

void MakerBotWriter::override_metadata(GCodeMetadata& meta) {
    // Override print stats if provided via set_print_stats()
    if (m_context.has_print_stats()) {
        meta.duration_s = m_context.get_duration_s();
        meta.filament_mm = m_context.get_filament_mm();
        meta.filament_g = m_context.get_filament_g();
        BOOST_LOG_TRIVIAL(info) << "MakerBotWriter: Override metadata with injected stats - "
                               << "duration=" << m_context.get_duration_s() << "s, "
                               << "filament=" << m_context.get_filament_mm() << "mm, "
                               << "weight=" << m_context.get_filament_g() << "g";
    }

    // Override material GUID from extruder data if available
    const auto& extruders = m_context.get_extruder_data();
    if (m_context.has_any_extruder_data() && !extruders[0].material_guid.empty()) {
        meta.material_guid = extruders[0].material_guid;
        BOOST_LOG_TRIVIAL(info) << "MakerBotWriter: Override metadata with GUID from extruder data: " << extruders[0].material_guid;
    }

    // Override material name from extruder data if available
    if (m_context.has_any_extruder_data() && !extruders[0].material_name.empty()) {
        meta.material_name = extruders[0].material_name;
    }

    // Override extruder temp from extruder data if available
    if (m_context.has_any_extruder_data() && extruders[0].extruder_temp > 0) {
        meta.extruder_temp = extruders[0].extruder_temp;
    }
}

std::pair<std::string, std::string> MakerBotWriter::get_bot_and_tool_type() const {
    // Use bot_type and tool_type directly from config (loaded from JSON config file)
    // The config_id (FORMAT_CONFIG_ID) is stored in m_config.id
    // The bot_type and tool_type are set in the JSON config files

    const std::string& bot_type = m_config.bot_type;
    const std::string& tool_type = m_config.tool_type;

    BOOST_LOG_TRIVIAL(info) << "MakerBotWriter: Using bot_type='" << bot_type << "', tool_type='" << tool_type << "' from config";

    // Return bot_type as-is and tool_type as-is
    // The config values are the authoritative source for MakerBot compatibility
    return std::make_pair(bot_type, tool_type);
}

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

    // 4. Thumbnails (passed directly via set_thumbnails() or set_thumbnail_data(), never extracted from gcode)
    // IMPORTANT: Thumbnails should NEVER be embedded in gcode comments - they are passed separately
    const auto& thumbnails = m_context.get_thumbnails();
    if (!thumbnails.empty()) {
        // Multiple thumbnails provided via set_thumbnails()
        BOOST_LOG_TRIVIAL(info) << "MakerBotWriter::write_container: Adding " << thumbnails.size() << " thumbnails";

        for (const auto& [thumbnail_data, filename] : thumbnails) {
            if (thumbnail_data.empty()) {
                BOOST_LOG_TRIVIAL(warning) << "MakerBotWriter: Skipping empty thumbnail: " << filename;
                continue;
            }

            BOOST_LOG_TRIVIAL(info) << "MakerBotWriter: Adding thumbnail '" << filename << "', size=" << thumbnail_data.size();

            if (!mz_zip_writer_add_mem(&archive, filename.c_str(),
                                       thumbnail_data.data(), thumbnail_data.size(),
                                       MZ_DEFAULT_COMPRESSION)) {
                BOOST_LOG_TRIVIAL(error) << "MakerBotWriter::write_container: FAILED to add thumbnail '" << filename << "' to archive";
            } else {
                BOOST_LOG_TRIVIAL(info) << "MakerBotWriter::write_container: SUCCESSFULLY added thumbnail '" << filename
                    << "' (" << thumbnail_data.size() << " bytes)";
            }
        }
    } else if (has_thumbnail_data()) {
        // Single thumbnail provided via set_thumbnail_data() (backward compatibility)
        BOOST_LOG_TRIVIAL(info) << "MakerBotWriter::write_container: Adding single thumbnail, size=" << m_thumbnail_data.size();

        // DEBUG: Log first few bytes of PNG to verify it's valid
        if (m_thumbnail_data.size() > 8) {
            BOOST_LOG_TRIVIAL(info) << "MakerBotWriter: PNG header bytes: "
                << std::hex << (int)m_thumbnail_data[0] << " " << (int)m_thumbnail_data[1] << " "
                << (int)m_thumbnail_data[2] << " " << (int)m_thumbnail_data[3] << " "
                << (int)m_thumbnail_data[4] << " " << (int)m_thumbnail_data[5] << " "
                << (int)m_thumbnail_data[6] << " " << (int)m_thumbnail_data[7] << std::dec;
        }

        // Determine naming convention based on printer type
        // IMPORTANT: Both sketch_small and sketch_sprint use isometric_thumbnail naming
        // This is required by the MakerBot firmware and Digital Factory
        std::string filename = "isometric_thumbnail_320x320.png";

        BOOST_LOG_TRIVIAL(info) << "MakerBotWriter: Using isometric thumbnail naming for " << m_config.printer_name;
        BOOST_LOG_TRIVIAL(info) << "MakerBotWriter: Adding thumbnail with filename: " << filename;

        if (!mz_zip_writer_add_mem(&archive, filename.c_str(),
                                   m_thumbnail_data.data(), m_thumbnail_data.size(),
                                   MZ_DEFAULT_COMPRESSION)) {
            BOOST_LOG_TRIVIAL(error) << "MakerBotWriter::write_container: FAILED to add thumbnail to archive";
        } else {
            BOOST_LOG_TRIVIAL(info) << "MakerBotWriter::write_container: SUCCESSFULLY added thumbnail as " << filename
                << " (" << m_thumbnail_data.size() << " bytes)";
        }
    } else {
        BOOST_LOG_TRIVIAL(warning) << "MakerBotWriter::write_container: No thumbnail data available - THUMBNAIL WILL BE MISSING";
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

    // Get dynamic bot_type and tool_type based on FORMAT_CONFIG_ID
    std::pair<std::string, std::string> bot_and_tool = get_bot_and_tool_type();
    const std::string& bot_type = bot_and_tool.first;
    const std::string& tool_type = bot_and_tool.second;

    // DEBUG: Log all metadata values
    BOOST_LOG_TRIVIAL(info) << "MakerBotWriter::generate_meta_json: BEGIN";
    BOOST_LOG_TRIVIAL(info) << "  bot_type=" << bot_type << ", tool_type=" << tool_type;
    BOOST_LOG_TRIVIAL(info) << "  duration_s=" << meta.duration_s;
    BOOST_LOG_TRIVIAL(info) << "  filament_mm=" << meta.filament_mm << ", filament_g=" << meta.filament_g;
    BOOST_LOG_TRIVIAL(info) << "  material_name=" << meta.material_name;
    BOOST_LOG_TRIVIAL(info) << "  material_guid=" << meta.material_guid;
    BOOST_LOG_TRIVIAL(info) << "  extruder_temp=" << meta.extruder_temp << ", bed_temp=" << meta.bed_temp;
    BOOST_LOG_TRIVIAL(info) << "  layer_height=" << meta.layer_height;

    json << "{\n";
    json << "  \"bot_type\": \"" << bot_type << "\",\n";
    json << "  \"platform_temperature\": " << meta.bed_temp << ",\n";
    json << "  \"build_plane_temperature\": " << m_config.build_plane_temperature << ",\n";
    json << "  \"commanded_duration_s\": " << meta.duration_s << ",\n";
    json << "  \"duration_s\": " << meta.duration_s << ",\n";
    json << "  \"extrusion_distance_mm\": " << std::fixed << std::setprecision(1) << meta.filament_mm << ",\n";
    json << "  \"extrusion_distances_mm\": [" << std::fixed << std::setprecision(1) << meta.filament_mm << "],\n";
    json << "  \"extrusion_mass_g\": " << std::fixed << std::setprecision(6) << meta.filament_g << ",\n";
    json << "  \"extrusion_masses_g\": [" << std::fixed << std::setprecision(6) << meta.filament_g << "],\n";
    // Generate a unique file UUID (not the material GUID)
    std::string file_uuid = generate_uuid();
    json << "  \"uuid\": \"" << file_uuid << "\",\n";
    std::string material_code = material_name_to_code(meta.material_name);
    BOOST_LOG_TRIVIAL(info) << "MakerBotWriter: material_name='" << meta.material_name << "' -> material_code='" << material_code << "'";
    json << "  \"material\": \"" << material_code << "\",\n";
    json << "  \"materials\": [\"" << material_code << "\"],\n";
    json << "  \"extruder_temperature\": " << meta.extruder_temp << ",\n";
    json << "  \"extruder_temperatures\": [" << meta.extruder_temp << "],\n";
    json << "  \"tool_type\": \"" << tool_type << "\",\n";
    json << "  \"tool_types\": [\"" << tool_type << "\"],\n";
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
        json << "  \"extruderProfiles\": [{ \"nozzle_diameter\": 0.4, \"tool_type\": \"" << tool_type << "\" }],\n";
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

    BOOST_LOG_TRIVIAL(info) << "MakerBotWriter::generate_slicemetadata_json: Looking for template " << template_filename;

    std::string template_content;
    bool found = false;
    std::string found_path;
    for (const auto& path : template_paths) {
        BOOST_LOG_TRIVIAL(info) << "  Trying path: " << path;
        boost::nowide::ifstream file(path);
        if (file.is_open()) {
            template_content = std::string((std::istreambuf_iterator<char>(file)),
                                           std::istreambuf_iterator<char>());
            found = true;
            found_path = path;
            BOOST_LOG_TRIVIAL(info) << "  FOUND template at: " << path << " (" << template_content.size() << " bytes)";
            break;
        }
    }

    // If template not found, fall back to minimal generation
    if (!found) {
        BOOST_LOG_TRIVIAL(warning) << "MakerBotWriter: Template " << template_filename << " NOT FOUND, using minimal fallback";
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
