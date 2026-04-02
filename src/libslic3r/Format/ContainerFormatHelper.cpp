#include "ContainerFormatHelper.hpp"
#include "../Utils.hpp"
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/log/trivial.hpp>
#include <boost/nowide/fstream.hpp>

namespace Slic3r {

namespace fs = boost::filesystem;
namespace pt = boost::property_tree;

// Helper to get formats directory (mirrors FormatConfig::get_formats_directory)
static fs::path get_formats_directory()
{
    return fs::path(resources_dir()) / "formats";
}

std::vector<ThumbnailRequirement> ContainerFormatHelper::load_thumbnail_requirements(
    const std::string& format_type,
    const std::string& config_id)
{
    std::vector<ThumbnailRequirement> requirements;
    
    fs::path config_path = get_formats_directory() / format_type / (config_id + ".json");
    
    if (!fs::exists(config_path)) {
        BOOST_LOG_TRIVIAL(warning) << "ContainerFormatHelper: Config file not found: " << config_path.string();
        return get_default_thumbnail_requirements(format_type);
    }
    
    try {
        pt::ptree config;
        pt::read_json(config_path.string(), config);
        
        // Try MakerBot format first: "thumbnails" array with "size" and "name" fields
        if (auto thumbnails = config.get_child_optional("thumbnails")) {
            for (const auto& thumb : *thumbnails) {
                ThumbnailRequirement req;
                std::string size_str = thumb.second.get<std::string>("size");
                req.filename = thumb.second.get<std::string>("name");
                
                // Parse size string (e.g., "120x120")
                size_t x_pos = size_str.find('x');
                if (x_pos != std::string::npos) {
                    req.width = std::stoi(size_str.substr(0, x_pos));
                    req.height = std::stoi(size_str.substr(x_pos + 1));
                } else {
                    // Fallback: assume square
                    req.width = std::stoi(size_str);
                    req.height = req.width;
                }
                
                requirements.push_back(req);
                BOOST_LOG_TRIVIAL(info) << "ContainerFormatHelper: Loaded thumbnail requirement: "
                                        << req.filename << " (" << req.width << "x" << req.height << ")";
            }
        }
        // Try UFP format: "thumbnail" object with "sizes" array and "filename" field
        else if (auto thumbnail = config.get_child_optional("thumbnail")) {
            std::string filename = thumbnail->get<std::string>("filename", "thumbnail.png");
            if (auto sizes = thumbnail->get_child_optional("sizes")) {
                for (const auto& size_entry : *sizes) {
                    ThumbnailRequirement req;
                    req.filename = filename;
                    std::string size_str = size_entry.second.data();
                    
                    // Parse size string (e.g., "320x320")
                    size_t x_pos = size_str.find('x');
                    if (x_pos != std::string::npos) {
                        req.width = std::stoi(size_str.substr(0, x_pos));
                        req.height = std::stoi(size_str.substr(x_pos + 1));
                    } else {
                        // Fallback: assume square
                        req.width = std::stoi(size_str);
                        req.height = req.width;
                    }
                    
                    requirements.push_back(req);
                    BOOST_LOG_TRIVIAL(info) << "ContainerFormatHelper: Loaded UFP thumbnail requirement: "
                                            << req.filename << " (" << req.width << "x" << req.height << ")";
                }
            }
        }
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "ContainerFormatHelper: Failed to load thumbnail requirements: " << e.what();
        return get_default_thumbnail_requirements(format_type);
    }
    
    return requirements;
}

std::vector<std::pair<std::vector<uint8_t>, std::string>> ContainerFormatHelper::generate_thumbnails(
    const std::vector<ThumbnailRequirement>& requirements,
    std::function<ThumbnailsList(const ThumbnailsParams&)> render_callback)
{
    std::vector<std::pair<std::vector<uint8_t>, std::string>> thumbnails;
    
    if (requirements.empty()) {
        BOOST_LOG_TRIVIAL(warning) << "ContainerFormatHelper: No thumbnail requirements provided";
        return thumbnails;
    }
    
    // Build thumbnail sizes for render callback
    std::vector<Vec2d> thumbnail_sizes;
    for (const auto& req : requirements) {
        thumbnail_sizes.emplace_back(req.width, req.height);
    }
    
    ThumbnailsParams params{thumbnail_sizes, true, true, true, true, 0};
    ThumbnailsList rendered_thumbs = render_callback(params);
    
    // Compress each rendered thumbnail to PNG
    for (size_t i = 0; i < rendered_thumbs.size() && i < requirements.size(); ++i) {
        if (rendered_thumbs[i].is_valid()) {
            auto compressed = GCodeThumbnails::compress_thumbnail(rendered_thumbs[i], GCodeThumbnailsFormat::PNG);
            if (compressed && compressed->data && compressed->size) {
                std::vector<uint8_t> png_data((uint8_t*)compressed->data,
                                              (uint8_t*)compressed->data + compressed->size);
                thumbnails.emplace_back(std::move(png_data), requirements[i].filename);
                BOOST_LOG_TRIVIAL(info) << "ContainerFormatHelper: Generated thumbnail "
                                        << requirements[i].filename << " (" << compressed->size << " bytes)";
            } else {
                BOOST_LOG_TRIVIAL(warning) << "ContainerFormatHelper: Failed to compress thumbnail "
                                           << requirements[i].filename;
            }
        } else {
            BOOST_LOG_TRIVIAL(warning) << "ContainerFormatHelper: Rendered thumbnail "
                                       << requirements[i].filename << " is not valid";
        }
    }
    
    // Clean up rendered thumbnail data
    for (auto& thumb : rendered_thumbs) {
        thumb.reset();
    }
    
    BOOST_LOG_TRIVIAL(info) << "ContainerFormatHelper: Generated " << thumbnails.size() << " of " 
                            << requirements.size() << " thumbnails";
    
    return thumbnails;
}

bool ContainerFormatHelper::validate_thumbnails(
    const std::vector<std::pair<std::vector<uint8_t>, std::string>>& thumbnails,
    const std::vector<ThumbnailRequirement>& requirements)
{
    if (thumbnails.size() != requirements.size()) {
        BOOST_LOG_TRIVIAL(warning) << "ContainerFormatHelper: Thumbnail count mismatch - got " 
                                   << thumbnails.size() << ", expected " << requirements.size();
        return false;
    }
    
    // Check that all required thumbnails are present
    for (size_t i = 0; i < requirements.size(); ++i) {
        bool found = false;
        for (const auto& thumb : thumbnails) {
            if (thumb.second == requirements[i].filename) {
                found = true;
                break;
            }
        }
        if (!found) {
            BOOST_LOG_TRIVIAL(warning) << "ContainerFormatHelper: Missing required thumbnail: " 
                                       << requirements[i].filename;
            return false;
        }
    }
    
    return true;
}

std::vector<ThumbnailRequirement> ContainerFormatHelper::get_default_thumbnail_requirements(
    const std::string& format_type)
{
    std::vector<ThumbnailRequirement> defaults;
    
    if (format_type == "makerbot") {
        // Default MakerBot thumbnails based on sketch_small.json
        defaults = {
            {120, 120, "isometric_thumbnail_120x120.png"},
            {320, 320, "isometric_thumbnail_320x320.png"},
            {640, 640, "isometric_thumbnail_640x640.png"},
            {90, 90, "thumbnail_90x90.png"},
            {140, 106, "thumbnail_140x106.png"},
            {212, 300, "thumbnail_212x300.png"},

        };
    } else if (format_type == "ufp") {
        // UFP only needs one thumbnail
        defaults = {
            {300, 300, "thumbnail.png"}
        };
    }
    
    BOOST_LOG_TRIVIAL(info) << "ContainerFormatHelper: Using default thumbnail requirements for " 
                            << format_type << " (" << defaults.size() << " thumbnails)";
    
    return defaults;
}

std::string ContainerFormatHelper::load_template_file(
    const std::string& format_type,
    const std::string& template_name)
{
    fs::path template_path = get_formats_directory() / format_type / "templates" / template_name;
    
    if (!fs::exists(template_path)) {
        BOOST_LOG_TRIVIAL(warning) << "ContainerFormatHelper: Template file not found: " << template_path.string();
        return "";
    }
    
    try {
        // Read the entire file into a string
        boost::nowide::ifstream file(template_path.string());
        if (!file.is_open()) {
            BOOST_LOG_TRIVIAL(error) << "ContainerFormatHelper: Failed to open template file: " << template_path.string();
            return "";
        }
        
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        
        BOOST_LOG_TRIVIAL(info) << "ContainerFormatHelper: Loaded template file: " << template_name 
                                << " (" << content.size() << " bytes)";
        return content;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "ContainerFormatHelper: Failed to load template file: " << e.what();
        return "";
    }
}

} // namespace Slic3r
