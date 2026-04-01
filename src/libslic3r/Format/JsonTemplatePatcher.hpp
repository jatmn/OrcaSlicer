#ifndef slic3r_JsonTemplatePatcher_hpp_
#define slic3r_JsonTemplatePatcher_hpp_

#include <string>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp>

namespace Slic3r {

// Utility class for patching JSON template files with runtime values
// Used by container format writers (UFP, MakerBot) to customize JSON templates
class JsonTemplatePatcher {
public:
    // Patch a JSON string using a set of replacement rules
    // template_json: The JSON template as a string
    // replacements: Vector of (placeholder, value) pairs
    // Returns the patched JSON string
    static std::string patch(
        const std::string& template_json,
        const std::vector<std::pair<std::string, std::string>>& replacements
    );
    
    // Patch a JSON object using a set of replacement rules
    // template_obj: The JSON object to patch (modified in place)
    // replacements: Vector of (json_path, value) pairs
    // json_path uses dot notation (e.g., "metadata.duration", "extruders.0.material_guid")
    static void patch_json(
        nlohmann::json& template_obj,
        const std::vector<std::pair<std::string, std::string>>& replacements
    );
    
    // Patch a JSON object using a set of replacement rules with typed values
    // template_obj: The JSON object to patch (modified in place)
    // replacements: Vector of (json_path, value) pairs where value is a JSON value
    static void patch_json_typed(
        nlohmann::json& template_obj,
        const std::vector<std::pair<std::string, nlohmann::json>>& replacements
    );
    
    // Escape a string for safe insertion into JSON
    // Handles quotes, backslashes, and control characters
    static std::string escape_json_string(const std::string& input);
    
    // Validate that a JSON string is well-formed
    static bool is_valid_json(const std::string& json_str);
    
    // Get a nested value from a JSON object using dot notation
    // Returns nullptr if path doesn't exist
    static const nlohmann::json* get_nested_value(
        const nlohmann::json& root,
        const std::string& path
    );
    
    // Set a nested value in a JSON object using dot notation
    // Creates intermediate objects as needed
    static void set_nested_value(
        nlohmann::json& root,
        const std::string& path,
        const nlohmann::json& value
    );
};

} // namespace Slic3r

#endif // slic3r_JsonTemplatePatcher_hpp_
