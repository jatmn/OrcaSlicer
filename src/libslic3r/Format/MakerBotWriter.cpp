#include "MakerBotWriter.hpp"
#include "../miniz_extension.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/nowide/fstream.hpp>
#include <sstream>
#include <iomanip>

namespace Slic3r {

std::string MakerBotWriter::generate_header(const GCodeMetadata& meta) {
    std::map<std::string, std::string> values;
    values["generator_name"] = m_config.gcode_metadata.generator_name;
    values["generator_version"] = m_config.gcode_metadata.generator_version;
    values["build_date"] = "2026-03-20";
    values["target_machine"] = m_config.target_machine;
    values["extruder_temp"] = std::to_string(meta.extruder_temp);
    values["filament_volume"] = std::to_string(static_cast<int>(meta.filament_mm));
    values["material_guid"] = meta.material_guid;
    values["nozzle_diameter"] = "0.4";
    values["nozzle_name"] = "0.4mm";
    values["bed_temp"] = std::to_string(meta.bed_temp);
    values["print_time"] = std::to_string(meta.duration_s);
    values["print_groups"] = "1";
    values["min_x"] = std::to_string(meta.min_x);
    values["min_y"] = std::to_string(meta.min_y);
    values["min_z"] = std::to_string(meta.min_z);
    values["max_x"] = std::to_string(meta.max_x);
    values["max_y"] = std::to_string(meta.max_y);
    values["max_z"] = std::to_string(meta.max_z);
    values["slice_uuid"] = meta.slice_uuid;
    values["layer_height"] = std::to_string(meta.layer_height);
    values["filament_m"] = std::to_string(meta.filament_mm / 1000.0);
    
    return substitute_template(m_config.header_template_content, values);
}

bool MakerBotWriter::write_container(const GCodeMetadata& meta, const std::string& gcode_content, const std::string& output_path) {
    mz_zip_archive archive;
    mz_zip_zero_struct(&archive);
    
    if (!open_zip_writer(&archive, output_path)) {
        return false;
    }
    
    auto cleanup = [&]() {
        close_zip_writer(&archive);
    };
    if (!mz_zip_writer_add_mem(&archive, "print.gcode",
                              gcode_content.c_str(), gcode_content.length(),
                              MZ_DEFAULT_COMPRESSION)) {
        cleanup();
        return false;
    }
    
    // 2. meta.json
    std::string meta_json = generate_meta_json(meta);
    if (!mz_zip_writer_add_mem(&archive, "meta.json",
                              meta_json.c_str(), meta_json.length(),
                              MZ_DEFAULT_COMPRESSION)) {
        cleanup();
        return false;
    }
    
    // 3. slicemetadata.json
    std::string slicemetadata = generate_slicemetadata_json(meta);
    if (!mz_zip_writer_add_mem(&archive, "slicemetadata.json",
                              slicemetadata.c_str(), slicemetadata.length(),
                              MZ_DEFAULT_COMPRESSION)) {
        cleanup();
        return false;
    }
    
    // Finalize archive
    if (!mz_zip_writer_finalize_archive(&archive)) {
        cleanup();
        return false;
    }
    
    cleanup();
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
    std::ostringstream json;
    json << "{\n";
    json << "  \"material\": {\n";
    json << "    \"length\": [" << std::fixed << std::setprecision(1) << meta.filament_mm << "],\n";
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
