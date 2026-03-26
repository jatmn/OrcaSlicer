#ifndef slic3r_FormatConfig_hpp_
#define slic3r_FormatConfig_hpp_

#include <string>
#include <vector>
#include <map>
#include <boost/filesystem/path.hpp>

namespace Slic3r {

// Forward declaration to avoid circular dependencies
struct ExtruderData;

struct ThumbnailConfig {
    std::string size;
    std::string filename;
};

struct PrinterFormatConfig {
    std::string id;
    std::string printer_name;
    std::string target_machine;
    std::string header_template;
    std::string file_extension;
    bool opc_structure;
    
    // G-code metadata
    struct {
        std::string flavor;
        std::string generator_name;
        std::string generator_version;
        std::vector<std::string> required_fields;
    } gcode_metadata;
    
    // Thumbnails
    std::vector<ThumbnailConfig> thumbnails;
    
    // Content types (for UFP)
    std::map<std::string, std::string> content_types;
    
    // ZIP structure
    std::vector<std::string> zip_files;
    
    // MakerBot specific
    std::string bot_type;
    std::string tool_type;
    std::string version;
    std::vector<double> machine_bounds;
    int build_plane_temperature;
    
    // Template values
    struct {
        std::string curaengine_version;
        std::string curaengine_commit_hash;
        std::string dulcificum_version;
        std::string dulcificum_commit_hash;
        std::string makerbot_writer_version;
        std::string pyDulcificum_version;
    } miracle_config;
    
    // Header template content (loaded from file)
    std::string header_template_content;
};

class FormatConfig {
public:
    // Load format manifest and all printer configs for a format type
    static bool load_format(const std::string& format_type, 
                           std::vector<PrinterFormatConfig>& out_configs,
                           std::string& out_format_name);
    
    // Load a specific printer config
    static bool load_printer_config(const std::string& format_type,
                                    const std::string& printer_id,
                                    PrinterFormatConfig& out_config);
    
    // Load printer config with fallback to first available if specific ID not found
    static bool load_printer_config_with_fallback(const std::string& format_type,
                                                  const std::string& printer_id,
                                                  PrinterFormatConfig& out_config);
    
    // Get list of available formats
    static std::vector<std::string> get_available_formats();
    
    // Parse format config ID from printer notes (looks for FORMAT_CONFIG_ID:xxx tag)
    // Returns the extracted ID if found, otherwise returns default_id
    static std::string parse_format_config_id(const std::string& printer_notes, const std::string& default_id);
    
    // Get the format type ("ufp" or "makerbot") based on FORMAT_CONFIG_ID in printer_notes
    // Returns empty string if no valid format config ID is found
    static std::string get_format_type_for_printer(const std::string& printer_notes);
    
    // Get the file extension for a given format type
    // Returns ".ufp", ".makerbot", or ".gcode" (default)
    static std::string get_file_extension_for_format(const std::string& format_type);
    
    // Get format type from file extension ("ufp" or "makerbot")
    // Returns empty string if extension doesn't match known container formats
    // Case-insensitive matching
    static std::string get_format_type_from_extension(const std::string& filepath);
    
    // Export G-code to container format (.ufp or .makerbot)
    // Returns true on success, false on failure with error_message populated
    // Note: No fallback - if config doesn't exist, export will fail
    static bool export_to_container(const std::string& format_type,
                                     const std::string& input_gcode_path,
                                     const std::string& output_path,
                                     const std::string& printer_notes,
                                     std::string& error_message);
    
    // Export G-code to container format with extruder variants
    // extruder_variants: List of printer extruder variant names (e.g., ["AA 0.4", "BB 0.4"])
    // Used for multi-extruder printers to set correct nozzle diameter/name in UFP header
    static bool export_to_container(const std::string& format_type,
                                     const std::string& input_gcode_path,
                                     const std::string& output_path,
                                     const std::string& printer_notes,
                                     const std::vector<std::string>& extruder_variants,
                                     std::string& error_message);
    
    // Export G-code to container format with per-extruder metadata (GUIDs, temps, volumes)
    // extruder_data: Vector of ExtruderData containing material GUID, temperature, and filament volume per extruder
    static bool export_to_container(const std::string& format_type,
                                     const std::string& input_gcode_path,
                                     const std::string& output_path,
                                     const std::string& printer_notes,
                                     const std::vector<std::string>& extruder_variants,
                                     const std::vector<ExtruderData>& extruder_data,
                                     std::string& error_message);
    
private:
    static boost::filesystem::path get_formats_directory();
    static std::string load_template_file(const boost::filesystem::path& template_path);
};

} // namespace Slic3r

#endif
