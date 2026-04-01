#include "JsonTemplatePatcher.hpp"
#include <boost/log/trivial.hpp>
#include <sstream>
#include <iomanip>

namespace Slic3r {

std::string JsonTemplatePatcher::patch(
    const std::string& template_json,
    const std::vector<std::pair<std::string, std::string>>& replacements)
{
    std::string result = template_json;
    
    for (const auto& [placeholder, value] : replacements) {
        // Simple string replacement for placeholder patterns like {{PLACEHOLDER}}
        std::string pattern = "{{" + placeholder + "}}";
        size_t pos = 0;
        while ((pos = result.find(pattern, pos)) != std::string::npos) {
            result.replace(pos, pattern.length(), value);
            pos += value.length();
        }
    }
    
    return result;
}

void JsonTemplatePatcher::patch_json(
    nlohmann::json& template_obj,
    const std::vector<std::pair<std::string, std::string>>& replacements)
{
    for (const auto& [path, value] : replacements) {
        set_nested_value(template_obj, path, nlohmann::json(value));
    }
}

void JsonTemplatePatcher::patch_json_typed(
    nlohmann::json& template_obj,
    const std::vector<std::pair<std::string, nlohmann::json>>& replacements)
{
    for (const auto& [path, value] : replacements) {
        set_nested_value(template_obj, path, value);
    }
}

std::string JsonTemplatePatcher::escape_json_string(const std::string& input)
{
    std::ostringstream oss;
    for (char c : input) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if (c >= 0x20 && c <= 0x7E) {
                    oss << c;
                } else {
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (static_cast<unsigned int>(c) & 0xFF);
                }
        }
    }
    return oss.str();
}

bool JsonTemplatePatcher::is_valid_json(const std::string& json_str)
{
    try {
        auto j = nlohmann::json::parse(json_str);
        return true;
    } catch (const nlohmann::json::exception&) {
        return false;
    }
}

const nlohmann::json* JsonTemplatePatcher::get_nested_value(
    const nlohmann::json& root,
    const std::string& path)
{
    const nlohmann::json* current = &root;
    
    size_t start = 0;
    size_t end = path.find('.');
    
    while (start < path.length()) {
        std::string key = (end == std::string::npos) 
            ? path.substr(start) 
            : path.substr(start, end - start);
        
        // Check if key is an array index
        if (key.find_first_not_of("0123456789") == std::string::npos && !key.empty()) {
            // It's a numeric index
            size_t index = std::stoul(key);
            if (!current->is_array() || index >= current->size()) {
                return nullptr;
            }
            current = &(*current)[index];
        } else {
            // It's an object key
            if (!current->is_object() || current->find(key) == current->end()) {
                return nullptr;
            }
            current = &(*current)[key];
        }
        
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
        end = path.find('.', start);
    }
    
    return current;
}

void JsonTemplatePatcher::set_nested_value(
    nlohmann::json& root,
    const std::string& path,
    const nlohmann::json& value)
{
    nlohmann::json* current = &root;
    
    size_t start = 0;
    size_t end = path.find('.');
    
    while (start < path.length()) {
        std::string key = (end == std::string::npos) 
            ? path.substr(start) 
            : path.substr(start, end - start);
        
        if (end == std::string::npos) {
            // Last component - set the value
            (*current)[key] = value;
            return;
        }
        
        // Check if key is an array index
        if (key.find_first_not_of("0123456789") == std::string::npos && !key.empty()) {
            // It's a numeric index
            size_t index = std::stoul(key);
            if (!current->is_array()) {
                // Convert to array
                *current = nlohmann::json::array();
            }
            // Ensure array is large enough
            while (current->size() <= index) {
                current->push_back(nlohmann::json::object());
            }
            current = &(*current)[index];
        } else {
            // It's an object key
            if (!current->is_object()) {
                // Convert to object
                *current = nlohmann::json::object();
            }
            if (current->find(key) == current->end()) {
                (*current)[key] = nlohmann::json::object();
            }
            current = &(*current)[key];
        }
        
        start = end + 1;
        end = path.find('.', start);
    }
}

} // namespace Slic3r
