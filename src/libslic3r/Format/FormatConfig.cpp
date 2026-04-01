#include "FormatConfig.hpp"
#include "UFPWriter.hpp"
#include "MakerBotWriter.hpp"
#include "../Utils.hpp"
#include <boost/filesystem.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/log/trivial.hpp>

namespace Slic3r {

namespace fs = boost::filesystem;
namespace pt = boost::property_tree;

fs::path FormatConfig::get_formats_directory() {
    return fs::path(resources_dir()) / "formats";
}

std::string FormatConfig::load_template_file(const fs::path& template_path) {
    boost::nowide::ifstream file(template_path.string());
    if (!file.is_open()) {
        return "";
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
}

bool FormatConfig::load_format(const std::string& format_type,
                               std::vector<PrinterFormatConfig>& out_configs,
                               std::string& out_format_name) {
    fs::path manifest_path = get_formats_directory() / format_type / "manifest.json";
    
    if (!fs::exists(manifest_path)) {
        return false;
    }
    
    try {
        pt::ptree manifest;
        pt::read_json(manifest_path.string(), manifest);
        
        out_format_name = manifest.get<std::string>("format_name", "");
        std::string file_extension = manifest.get<std::string>("file_extension", "");
        std::string content_type = manifest.get<std::string>("content_type", "");
        
        for (const auto& printer : manifest.get_child("printers")) {
            PrinterFormatConfig config;
            config.id = printer.second.get<std::string>("id");
            config.printer_name = printer.second.get<std::string>("name");
            config.file_extension = file_extension;
            
            std::string config_file = printer.second.get<std::string>("config_file");
            fs::path config_path = get_formats_directory() / format_type / config_file;
            
            if (!load_printer_config(format_type, config.id, config)) {
                continue;
            }
            
            out_configs.push_back(config);
        }
        
        return !out_configs.empty();
    } catch (const std::exception&) {
        return false;
    }
}

bool FormatConfig::load_printer_config(const std::string& format_type,
                                       const std::string& printer_id,
                                       PrinterFormatConfig& out_config) {
    fs::path formats_dir = get_formats_directory();
    fs::path config_path = formats_dir / format_type / (printer_id + ".json");
    
    BOOST_LOG_TRIVIAL(info) << "FormatConfig: Looking for config at: " << config_path.string();
    BOOST_LOG_TRIVIAL(info) << "FormatConfig: Resources dir is: " << formats_dir.string();
    BOOST_LOG_TRIVIAL(info) << "FormatConfig: Format type: " << format_type << ", printer_id: " << printer_id;
    
    if (!fs::exists(config_path)) {
        BOOST_LOG_TRIVIAL(error) << "FormatConfig: Config file not found: " << config_path.string();
        return false;
    }
    
    try {
        pt::ptree config;
        pt::read_json(config_path.string(), config);
        
        out_config.id = printer_id;
        out_config.printer_name = config.get<std::string>("printer_name", "");
        out_config.target_machine = config.get<std::string>("target_machine", "");
        out_config.header_template = config.get<std::string>("header_template", "");
        out_config.opc_structure = config.get<bool>("opc_structure", false);
        
        // G-code metadata
        if (auto gcode_meta = config.get_child_optional("gcode_metadata")) {
            out_config.gcode_metadata.flavor = gcode_meta->get<std::string>("flavor", "");
            out_config.gcode_metadata.generator_name = gcode_meta->get<std::string>("generator_name", "");
            out_config.gcode_metadata.generator_version = gcode_meta->get<std::string>("generator_version", "");
            
            if (auto fields = gcode_meta->get_child_optional("required_fields")) {
                for (const auto& field : *fields) {
                    out_config.gcode_metadata.required_fields.push_back(field.second.get_value<std::string>());
                }
            }
        }
        
        // Content types (UFP)
        if (auto content_types = config.get_child_optional("content_types")) {
            for (const auto& ct : *content_types) {
                out_config.content_types[ct.first] = ct.second.get_value<std::string>();
            }
        }
        
        // Thumbnails
        if (auto thumbs = config.get_child_optional("thumbnails")) {
            for (const auto& thumb : *thumbs) {
                ThumbnailConfig tc;
                tc.size = thumb.second.get<std::string>("size");
                tc.filename = thumb.second.get<std::string>("name");
                out_config.thumbnails.push_back(tc);
            }
        }
        
        // ZIP files
        if (auto zip_files = config.get_child_optional("zip_files")) {
            for (const auto& file : *zip_files) {
                out_config.zip_files.push_back(file.second.get_value<std::string>());
            }
        }
        
        // MakerBot specific
        out_config.bot_type = config.get<std::string>("bot_type", "");
        out_config.tool_type = config.get<std::string>("tool_type", "");
        out_config.version = config.get<std::string>("version", "");
        out_config.build_plane_temperature = config.get<int>("build_plane_temperature", 28);
        
        if (auto bounds = config.get_child_optional("machine_bounds")) {
            for (const auto& bound : *bounds) {
                out_config.machine_bounds.push_back(bound.second.get_value<double>());
            }
        }
        
        // Miracle config
        if (auto mc = config.get_child_optional("miracle_config")) {
            out_config.miracle_config.curaengine_version = mc->get<std::string>("curaengine_version", "");
            out_config.miracle_config.curaengine_commit_hash = mc->get<std::string>("curaengine_commit_hash", "");
            out_config.miracle_config.dulcificum_version = mc->get<std::string>("dulcificum_version", "");
            out_config.miracle_config.dulcificum_commit_hash = mc->get<std::string>("dulcificum_commit_hash", "");
            out_config.miracle_config.makerbot_writer_version = mc->get<std::string>("makerbot_writer_version", "");
            out_config.miracle_config.pyDulcificum_version = mc->get<std::string>("pyDulcificum_version", "");
        }
        
        // Load header template content
        if (!out_config.header_template.empty()) {
            fs::path template_path = get_formats_directory() / format_type / "templates" / 
                                    ("header_" + out_config.header_template + ".txt");
            out_config.header_template_content = load_template_file(template_path);
        }
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<std::string> FormatConfig::get_available_formats() {
    std::vector<std::string> formats;
    fs::path formats_dir = get_formats_directory();
    
    if (!fs::exists(formats_dir)) {
        return formats;
    }
    
    for (const auto& entry : fs::directory_iterator(formats_dir)) {
        if (fs::is_directory(entry)) {
            fs::path manifest = entry.path() / "manifest.json";
            if (fs::exists(manifest)) {
                formats.push_back(entry.path().filename().string());
            }
        }
    }
    
    return formats;
}

bool FormatConfig::load_printer_config_with_fallback(const std::string& format_type,
                                                      const std::string& printer_id,
                                                      PrinterFormatConfig& out_config) {
    // First try to load the specific config
    if (load_printer_config(format_type, printer_id, out_config)) {
        return true;
    }
    
    // If that fails, load the manifest and use the first available printer config
    std::vector<PrinterFormatConfig> configs;
    std::string format_name;
    if (load_format(format_type, configs, format_name) && !configs.empty()) {
        out_config = configs[0];
        BOOST_LOG_TRIVIAL(warning) << "FormatConfig: Using fallback config '" << out_config.id 
                                   << "' for printer_id '" << printer_id << "'";
        return true;
    }
    
    return false;
}

std::string FormatConfig::parse_format_config_id(const std::string& printer_notes, const std::string& default_id) {
    // Look for FORMAT_CONFIG_ID:xxx tag in printer notes
    // This allows users to specify which format config to use even if they rename their printer preset
    const std::string tag = "FORMAT_CONFIG_ID:";
    size_t pos = printer_notes.find(tag);
    if (pos != std::string::npos) {
        // Extract the ID (everything after the tag until whitespace or end of line)
        size_t start = pos + tag.length();
        size_t end = printer_notes.find_first_of(" \t\n\r", start);
        if (end == std::string::npos) {
            end = printer_notes.length();
        }
        std::string config_id = printer_notes.substr(start, end - start);
        if (!config_id.empty()) {
            BOOST_LOG_TRIVIAL(info) << "FormatConfig: Found FORMAT_CONFIG_ID tag: " << config_id;
            return config_id;
        }
    }
    return default_id;
}

std::string FormatConfig::get_format_type_for_printer(const std::string& printer_notes) {
    // Parse the FORMAT_CONFIG_ID from printer notes
    std::string config_id = parse_format_config_id(printer_notes, "");
    
    if (config_id.empty()) {
        return "";
    }
    
    // UltiMaker formats (.ufp) - IDs match Cura's machine definition filenames
    if (config_id == "ultimaker_s3" || config_id == "ultimaker_s5" ||
        config_id == "ultimaker_s6" || config_id == "ultimaker_s7" ||
        config_id == "ultimaker_s8" || config_id == "ultimaker_factor4" ||
        config_id == "ultimaker2_plus_connect") {
        return "ufp";
    }
    
    // MakerBot formats (.makerbot)
    if (config_id == "sketch_small" || config_id == "sketch_sprint" ||
        config_id == "sketch_large" || config_id == "method_x" || config_id == "method_xl") {
        return "makerbot";
    }
    
    // Unknown format ID
    BOOST_LOG_TRIVIAL(warning) << "FormatConfig: Unknown FORMAT_CONFIG_ID: " << config_id;
    return "";
}

std::string FormatConfig::get_file_extension_for_format(const std::string& format_type) {
    if (format_type == "ufp") {
        return ".ufp";
    } else if (format_type == "makerbot") {
        return ".makerbot";
    }
    return ".gcode";  // Default
}

std::string FormatConfig::get_format_type_from_extension(const std::string& filepath) {
    boost::filesystem::path p(filepath);
    std::string ext = boost::to_lower_copy(p.extension().string());
    
    if (ext == ".ufp") {
        return "ufp";
    } else if (ext == ".makerbot") {
        return "makerbot";
    }
    return "";
}

bool FormatConfig::export_to_container(const std::string& format_type,
                                      const std::string& input_gcode_path,
                                      const std::string& output_path,
                                      const std::string& printer_notes,
                                      std::string& error_message) {
    // Call the overload with empty extruder variants (backward compatible)
    return export_to_container(format_type, input_gcode_path, output_path, printer_notes, {}, error_message);
}

bool FormatConfig::export_to_container(const std::string& format_type,
                                      const std::string& input_gcode_path,
                                      const std::string& output_path,
                                      const std::string& printer_notes,
                                      const std::vector<std::string>& extruder_variants,
                                      std::string& error_message) {
    // Call the overload with empty extruder data
    return export_to_container(format_type, input_gcode_path, output_path, printer_notes, 
                              extruder_variants, {}, error_message);
}

bool FormatConfig::export_to_container(const std::string& format_type,
                                      const std::string& input_gcode_path,
                                      const std::string& output_path,
                                      const std::string& printer_notes,
                                      const std::vector<std::string>& extruder_variants,
                                      const std::vector<ExtruderData>& extruder_data,
                                      std::string& error_message) {
    // Call overload with empty thumbnail data
    return export_to_container(format_type, input_gcode_path, output_path, printer_notes, 
                              extruder_variants, extruder_data, std::vector<uint8_t>(), error_message);
}

bool FormatConfig::export_to_container(const std::string& format_type,
                                      const std::string& input_gcode_path,
                                      const std::string& output_path,
                                      const std::string& printer_notes,
                                      const std::vector<std::string>& extruder_variants,
                                      const std::vector<ExtruderData>& extruder_data,
                                      const std::vector<uint8_t>& thumbnail_data,
                                      std::string& error_message) {
    // Parse the FORMAT_CONFIG_ID from printer notes
    std::string config_id = parse_format_config_id(printer_notes, "");
    
    if (config_id.empty()) {
        error_message = "No FORMAT_CONFIG_ID found in printer notes. "
                       "To export in container format, add FORMAT_CONFIG_ID:<id> to your printer notes. "
                       "Valid IDs for .ufp: ultimaker_s3, ultimaker_s5, ultimaker_s6, ultimaker_s7, ultimaker_s8, ultimaker_factor4, ultimaker2_plus_connect. "
                       "Valid IDs for .makerbot: sketch_small, sketch_sprint, sketch_large, method_x, method_xl.";
        BOOST_LOG_TRIVIAL(error) << "FormatConfig: " << error_message;
        return false;
    }
    
    // Load the printer-specific config (NO FALLBACK - fail if config doesn't exist)
    PrinterFormatConfig format_config;
    if (!load_printer_config(format_type, config_id, format_config)) {
        error_message = "Failed to load configuration for FORMAT_CONFIG_ID: " + config_id + ". "
                       "The configuration file '" + config_id + ".json' does not exist in the formats directory. "
                       "Please ensure the printer preset is properly configured.";
        BOOST_LOG_TRIVIAL(error) << "FormatConfig: " << error_message;
        return false;
    }
    
    BOOST_LOG_TRIVIAL(info) << "FormatConfig: Exporting to " << format_type 
                           << " format using config: " << config_id;
    
    // Create the appropriate writer
    bool success = false;
    if (format_type == "ufp") {
        UFPWriter writer(format_config);
        // Pass extruder variants for multi-extruder support (nozzle diameter/name)
        BOOST_LOG_TRIVIAL(warning) << "FormatConfig: extruder_variants.size()=" << extruder_variants.size();
        if (!extruder_variants.empty()) {
            writer.set_extruder_variants(extruder_variants);
            BOOST_LOG_TRIVIAL(info) << "FormatConfig: Setting " << extruder_variants.size() << " extruder variants for UFP export";
        }
        // Pass extruder data (GUIDs, temps, volumes) for multi-extruder metadata
        BOOST_LOG_TRIVIAL(warning) << "FormatConfig: Passing " << extruder_data.size() << " extruder data entries to UFPWriter";
        for (size_t i = 0; i < extruder_data.size() && i < 2; ++i) {
            BOOST_LOG_TRIVIAL(warning) << "FormatConfig: Extruder " << i << " - empty()=" << extruder_data[i].empty() 
                                   << ", GUID: '" << extruder_data[i].material_guid << "', temp: " 
                                   << extruder_data[i].extruder_temp << ", filament_mm: " 
                                   << extruder_data[i].filament_mm;
            writer.set_extruder_data(static_cast<int>(i), extruder_data[i]);
        }
        // Pass thumbnail data directly (NOT extracted from gcode - thumbnails should never be in gcode comments)
        if (!thumbnail_data.empty()) {
            writer.set_thumbnail_data(thumbnail_data);
            BOOST_LOG_TRIVIAL(info) << "FormatConfig: Setting thumbnail data for UFP export, size=" << thumbnail_data.size();
        }
        success = writer.write(input_gcode_path, output_path);
        if (!success) {
            error_message = "Failed to create UltiMaker Format Package (.ufp). "
                           "The UFP writer encountered an error while packaging the G-code file. "
                           "This may be caused by an invalid or corrupted G-code file, "
                           "or missing required metadata in the printer configuration.";
        }
    } else if (format_type == "makerbot") {
        MakerBotWriter writer(format_config);
        // Pass extruder variants for nozzle diameter/name
        if (!extruder_variants.empty()) {
            writer.set_extruder_variants(extruder_variants);
            BOOST_LOG_TRIVIAL(info) << "FormatConfig: Setting " << extruder_variants.size() << " extruder variants for MakerBot export";
        }
        // Pass thumbnail data directly (NOT extracted from gcode)
        if (!thumbnail_data.empty()) {
            writer.set_thumbnail_data(thumbnail_data);
            BOOST_LOG_TRIVIAL(info) << "FormatConfig: Setting thumbnail data for MakerBot export, size=" << thumbnail_data.size();
        }
        // Pass extruder data (GUIDs, temps, volumes)
        for (size_t i = 0; i < extruder_data.size() && i < 1; ++i) {
            ExtruderData mb_data;
            mb_data.material_guid = extruder_data[i].material_guid;
            mb_data.material_name = extruder_data[i].material_name;
            mb_data.extruder_temp = extruder_data[i].extruder_temp;
            mb_data.filament_mm = extruder_data[i].filament_mm;
            mb_data.filament_g = extruder_data[i].filament_g;
            writer.set_extruder_data(static_cast<int>(i), mb_data);
            BOOST_LOG_TRIVIAL(info) << "FormatConfig: Setting extruder data for MakerBot export - GUID: " << mb_data.material_guid;
        }
        success = writer.write(input_gcode_path, output_path);
        if (!success) {
            error_message = "Failed to create MakerBot file (.makerbot). "
                           "The MakerBot writer encountered an error while packaging the G-code file. "
                           "This may be caused by an invalid or corrupted G-code file, "
                           "or missing required metadata in the printer configuration.";
        }
    } else {
        error_message = "Unknown format type: " + format_type + ". "
                       "Supported formats are 'ufp' and 'makerbot'.";
        BOOST_LOG_TRIVIAL(error) << "FormatConfig: " << error_message;
        return false;
    }
    
    if (success) {
        BOOST_LOG_TRIVIAL(info) << "FormatConfig: Successfully exported to " << output_path;
    } else {
        BOOST_LOG_TRIVIAL(error) << "FormatConfig: Export failed: " << error_message;
    }
    
    return success;
}

bool FormatConfig::export_to_container(const std::string& format_type,
                                       const std::string& input_gcode_path,
                                       const std::string& output_path,
                                       const std::string& printer_notes,
                                       const std::vector<std::string>& extruder_variants,
                                       const std::vector<ExtruderData>& extruder_data,
                                       const std::vector<std::pair<std::vector<uint8_t>, std::string>>& thumbnails,
                                       std::string& error_message) {
    // For UFP format or when no thumbnails provided, delegate to single-thumbnail overload
    if (format_type != "makerbot" || thumbnails.empty()) {
        std::vector<uint8_t> single_thumbnail;
        if (!thumbnails.empty()) {
            single_thumbnail = thumbnails[0].first;
        }
        return export_to_container(format_type, input_gcode_path, output_path, printer_notes,
                                     extruder_variants, extruder_data, single_thumbnail, error_message);
    }
    
    // Parse the FORMAT_CONFIG_ID from printer notes
    std::string config_id = parse_format_config_id(printer_notes, "");
    
    if (config_id.empty()) {
        error_message = "No FORMAT_CONFIG_ID found in printer notes. "
                       "To export in container format, add FORMAT_CONFIG_ID:<id> to your printer notes. "
                       "Valid IDs for .ufp: ultimaker_s3, ultimaker_s5, ultimaker_s6, ultimaker_s7, ultimaker_s8, ultimaker_factor4, ultimaker2_plus_connect. "
                       "Valid IDs for .makerbot: sketch_small, sketch_sprint, sketch_large, method_x, method_xl.";
        BOOST_LOG_TRIVIAL(error) << "FormatConfig: " << error_message;
        return false;
    }
    
    // Load the printer-specific config (NO FALLBACK - fail if config doesn't exist)
    PrinterFormatConfig format_config;
    if (!load_printer_config(format_type, config_id, format_config)) {
        error_message = "Failed to load configuration for FORMAT_CONFIG_ID: " + config_id + ". "
                       "The configuration file '" + config_id + ".json' does not exist in the formats directory. "
                       "Please ensure the printer preset is properly configured.";
        BOOST_LOG_TRIVIAL(error) << "FormatConfig: " << error_message;
        return false;
    }
    
    BOOST_LOG_TRIVIAL(info) << "FormatConfig: Exporting to " << format_type 
                           << " format using config: " << config_id 
                           << " with " << thumbnails.size() << " thumbnails";
    
    // Create the MakerBot writer with multiple thumbnails
    MakerBotWriter writer(format_config);
    
    // Pass extruder variants for nozzle diameter/name
    if (!extruder_variants.empty()) {
        writer.set_extruder_variants(extruder_variants);
        BOOST_LOG_TRIVIAL(info) << "FormatConfig: Setting " << extruder_variants.size() << " extruder variants for MakerBot export";
    }
    
    // Pass multiple thumbnails with their filenames
    writer.set_thumbnails(thumbnails);
    BOOST_LOG_TRIVIAL(info) << "FormatConfig: Setting " << thumbnails.size() << " thumbnails for MakerBot export";
    
    // Pass extruder data (GUIDs, temps, volumes) - support up to 2 extruders for dual-extruder MakerBot models
    for (size_t i = 0; i < extruder_data.size() && i < 2; ++i) {
        ExtruderData mb_data;
        mb_data.material_guid = extruder_data[i].material_guid;
        mb_data.material_name = extruder_data[i].material_name;
        mb_data.extruder_temp = extruder_data[i].extruder_temp;
        mb_data.filament_mm = extruder_data[i].filament_mm;
        mb_data.filament_g = extruder_data[i].filament_g;
        writer.set_extruder_data(static_cast<int>(i), mb_data);
        BOOST_LOG_TRIVIAL(info) << "FormatConfig: Setting extruder data for MakerBot export - GUID: " << mb_data.material_guid;
    }
    
    bool success = writer.write(input_gcode_path, output_path);
    if (!success) {
        error_message = "Failed to create MakerBot file (.makerbot). "
                       "The MakerBot writer encountered an error while packaging the G-code file. "
                       "This may be caused by an invalid or corrupted G-code file, "
                       "or missing required metadata in the printer configuration.";
        BOOST_LOG_TRIVIAL(error) << "FormatConfig: Export failed: " << error_message;
    } else {
        BOOST_LOG_TRIVIAL(info) << "FormatConfig: Successfully exported to " << output_path;
    }
    
    return success;
}

} // namespace Slic3r
