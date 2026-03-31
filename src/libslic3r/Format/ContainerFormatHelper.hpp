#ifndef slic3r_ContainerFormatHelper_hpp_
#define slic3r_ContainerFormatHelper_hpp_

#include <string>
#include <vector>
#include <utility>
#include <functional>
#include "../GCode/ThumbnailData.hpp"
#include "../GCode/Thumbnails.hpp"

namespace Slic3r {

// Thumbnail requirement from format config
struct ThumbnailRequirement {
    int width;
    int height;
    std::string filename;
};

// Helper class for container format operations (UFP, MakerBot, etc.)
class ContainerFormatHelper {
public:
    // Load thumbnail requirements from format config JSON file
    // format_type: "ufp" or "makerbot"
    // config_id: the FORMAT_CONFIG_ID from printer notes (e.g., "sketch_small", "ultimaker_s5")
    static std::vector<ThumbnailRequirement> load_thumbnail_requirements(
        const std::string& format_type,
        const std::string& config_id
    );
    
    // Generate thumbnails using the render callback
    // Returns vector of (PNG data, filename) pairs
    static std::vector<std::pair<std::vector<uint8_t>, std::string>> generate_thumbnails(
        const std::vector<ThumbnailRequirement>& requirements,
        std::function<ThumbnailsList(const ThumbnailsParams&)> render_callback
    );
    
    // Validate that generated thumbnails match requirements
    static bool validate_thumbnails(
        const std::vector<std::pair<std::vector<uint8_t>, std::string>>& thumbnails,
        const std::vector<ThumbnailRequirement>& requirements
    );
    
    // Get default thumbnail requirements for a format type
    // Used as fallback when config file doesn't exist
    static std::vector<ThumbnailRequirement> get_default_thumbnail_requirements(
        const std::string& format_type
    );
};

} // namespace Slic3r

#endif // slic3r_ContainerFormatHelper_hpp_
