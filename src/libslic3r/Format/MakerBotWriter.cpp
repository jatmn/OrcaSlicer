#include "MakerBotWriter.hpp"

#include "../Utils.hpp"
#include "../miniz_extension.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/nowide/fstream.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <sstream>

namespace Slic3r {

namespace {

using json = nlohmann::json;

struct MethodMaterialInfo {
    const char *code;
    const char *name;
    const char *guid;
};

struct MethodBounds {
    bool   valid = false;
    double min_x = 0.0;
    double max_x = 0.0;
    double min_y = 0.0;
    double max_y = 0.0;
    double min_z = 0.0;
    double max_z = 0.0;
};

const std::vector<MethodMaterialInfo>& method_materials()
{
    static const std::vector<MethodMaterialInfo> materials = {
        {"abs", "ABS", "e0f1d581-cc6b-4e36-8f3c-3f5601ecba5f"},
        {"abs-cf10", "ABS-CF", "495a0ce5-9daf-4a16-b7b2-06856d82394d"},
        {"abs-wss1", "ABS-R", "88c8919c-6a09-471a-b7b6-e801263d862d"},
        {"asa", "ASA", "f79bc612-21eb-482e-ad6c-87d75bdde066"},
        {"nylon", "Nylon", "9475b03d-fd19-48a2-b7b5-be1fb46abb02"},
        {"nylon12-cf", "Nylon 12 CF", "3c6f2877-71cc-4760-84e6-4b89ab243e3b"},
        {"nylon-cf", "Nylon CF", "17abb865-ca73-4ccd-aeda-38e294c9c60b"},
        {"pet", "PETG", "2d004bbd-d1bb-47f8-beac-b066702d5273"},
        {"pla", "PLA", "abb9c58e-1f56-48d1-bd8f-055fde3a5b56"},
        {"pva", "PVA", "add51ef2-86eb-4c39-afd5-5586564f0715"},
        {"wss1", "RapidRinse", "a140ef8f-4f26-4e73-abe0-cfc29d6d1024"},
        {"sr30", "SR-30", "77873465-83a9-4283-bc44-4e542b8eb3eb"},
        {"im-pla", "Tough", "de031137-a8ca-4a72-bd1b-17bb964033ad"}
    };
    return materials;
}

std::string trim_copy(std::string value)
{
    boost::algorithm::trim(value);
    return value;
}

std::string lower_copy(std::string value)
{
    boost::algorithm::to_lower(value);
    return value;
}

std::string upper_copy(std::string value)
{
    boost::algorithm::to_upper(value);
    return value;
}

std::string normalize_material_name(std::string value)
{
    value = lower_copy(trim_copy(value));
    boost::algorithm::replace_all(value, "_", "-");
    boost::algorithm::replace_all(value, "makerbot method ", "");
    boost::algorithm::replace_all(value, "makerbot ", "");
    boost::algorithm::replace_all(value, "ultimaker ", "");
    boost::algorithm::replace_all(value, " pla tough", " tough pla");
    boost::algorithm::replace_all(value, " @system", "");
    boost::algorithm::replace_all(value, " @base", "");
    return trim_copy(value);
}

std::optional<MethodMaterialInfo> find_method_material_by_guid(const std::string& guid)
{
    if (guid.empty())
        return std::nullopt;

    const std::string guid_lower = lower_copy(guid);
    for (const MethodMaterialInfo& info : method_materials()) {
        if (guid_lower == lower_copy(info.guid))
            return info;
    }

    return std::nullopt;
}

std::optional<MethodMaterialInfo> find_method_material_by_name(const std::string& name)
{
    if (name.empty())
        return std::nullopt;

    const std::string normalized = normalize_material_name(name);
    for (const MethodMaterialInfo& info : method_materials()) {
        if (normalized == normalize_material_name(info.code) || normalized == normalize_material_name(info.name))
            return info;
    }

    if (normalized.find("abs-r") != std::string::npos) return MethodMaterialInfo{"abs-wss1", "ABS-R", "88c8919c-6a09-471a-b7b6-e801263d862d"};
    if (normalized.find("abs-cf") != std::string::npos) return MethodMaterialInfo{"abs-cf10", "ABS-CF", "495a0ce5-9daf-4a16-b7b2-06856d82394d"};
    if (normalized.find("rapidrinse") != std::string::npos || normalized.find("rapid-rinse") != std::string::npos) return MethodMaterialInfo{"wss1", "RapidRinse", "a140ef8f-4f26-4e73-abe0-cfc29d6d1024"};
    if (normalized.find("sr-30") != std::string::npos || normalized.find("sr30") != std::string::npos) return MethodMaterialInfo{"sr30", "SR-30", "77873465-83a9-4283-bc44-4e542b8eb3eb"};
    if (normalized.find("tough") != std::string::npos) return MethodMaterialInfo{"im-pla", "Tough", "de031137-a8ca-4a72-bd1b-17bb964033ad"};
    if (normalized.find("petg") != std::string::npos || normalized == "pet") return MethodMaterialInfo{"pet", "PETG", "2d004bbd-d1bb-47f8-beac-b066702d5273"};
    if (normalized.find("asa") != std::string::npos) return MethodMaterialInfo{"asa", "ASA", "f79bc612-21eb-482e-ad6c-87d75bdde066"};
    if (normalized.find("pva") != std::string::npos) return MethodMaterialInfo{"pva", "PVA", "add51ef2-86eb-4c39-afd5-5586564f0715"};
    if (normalized.find("pla") != std::string::npos) return MethodMaterialInfo{"pla", "PLA", "abb9c58e-1f56-48d1-bd8f-055fde3a5b56"};
    if (normalized.find("nylon 12 cf") != std::string::npos) return MethodMaterialInfo{"nylon12-cf", "Nylon 12 CF", "3c6f2877-71cc-4760-84e6-4b89ab243e3b"};
    if (normalized.find("nylon cf") != std::string::npos) return MethodMaterialInfo{"nylon-cf", "Nylon CF", "17abb865-ca73-4ccd-aeda-38e294c9c60b"};
    if (normalized.find("nylon") != std::string::npos) return MethodMaterialInfo{"nylon", "Nylon", "9475b03d-fd19-48a2-b7b5-be1fb46abb02"};
    if (normalized == "abs") return MethodMaterialInfo{"abs", "ABS", "e0f1d581-cc6b-4e36-8f3c-3f5601ecba5f"};

    return std::nullopt;
}

std::string material_name_to_code(const std::string& name, const std::string& guid = {})
{
    if (auto info = find_method_material_by_guid(guid); info.has_value())
        return info->code;
    if (auto info = find_method_material_by_name(name); info.has_value())
        return info->code;

    std::string code = normalize_material_name(name);
    std::replace(code.begin(), code.end(), ' ', '-');
    return code.empty() ? "pla" : code;
}

std::string material_name_to_guid(const std::string& name, const std::string& guid = {})
{
    if (!guid.empty())
        return guid;
    if (auto info = find_method_material_by_name(name); info.has_value())
        return info->guid;
    return {};
}

std::string tool_variant_to_method_tool(const std::string& config_id, std::string variant)
{
    variant = trim_copy(variant);
    if (variant.empty())
        return {};

    if (boost::algorithm::starts_with(lower_copy(variant), "mk14"))
        return lower_copy(variant);

    const auto space_pos = variant.find(' ');
    if (space_pos != std::string::npos)
        variant = variant.substr(0, space_pos);

    static const std::map<std::string, std::string> method_map = {
        {"1A", "mk14"},
        {"1C", "mk14_c"},
        {"2A", "mk14_s"},
        {"LABS", "mk14_e"}
    };
    static const std::map<std::string, std::string> methodx_map = {
        {"1A", "mk14"},
        {"1C", "mk14_c"},
        {"1XA", "mk14_hot"},
        {"2A", "mk14_s"},
        {"2XA", "mk14_hot_s"},
        {"LABS", "mk14_e"}
    };

    const auto& mapping = (config_id == "method") ? method_map : methodx_map;
    auto        it      = mapping.find(upper_copy(variant));
    if (it != mapping.end())
        return it->second;

    return {};
}

std::string comment_to_method_tag(const std::string& type_comment, int extruder_idx)
{
    const std::string suffix = "_" + std::to_string(std::max(0, extruder_idx));
    const std::string type   = upper_copy(trim_copy(type_comment));

    if (type == "SKIRT" || type == "RAFT") return "SKIRT" + suffix;
    if (type == "WALL-OUTER") return "WALL_OUTER" + suffix;
    if (type == "WALL-INNER") return "WALL_INNER" + suffix;
    if (type == "FILL") return "FILL" + suffix;
    if (type == "SUPPORT") return "SUPPORT" + suffix;
    if (type == "SUPPORT-INTERFACE") return "SUPPORT_INTERFACE" + suffix;
    if (type == "PRIME-TOWER") return "PRIME_TOWER" + suffix;
    if (type == "SKIN" || type == "SOLID-FILL" || type == "TOP-SURFACE") return "TOP_SURFACE" + suffix;

    return {};
}

std::optional<double> find_gcode_value(const std::string& line, char code)
{
    const char upper_code = static_cast<char>(std::toupper(static_cast<unsigned char>(code)));
    const char lower_code = static_cast<char>(std::tolower(static_cast<unsigned char>(code)));

    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] != upper_code && line[i] != lower_code)
            continue;

        if (i > 0 && !std::isspace(static_cast<unsigned char>(line[i - 1])))
            continue;

        size_t start = i + 1;
        size_t end   = start;
        while (end < line.size()) {
            const char c = line[end];
            if (std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '+')
                ++end;
            else
                break;
        }

        if (end == start)
            return std::nullopt;

        try {
            return std::stod(line.substr(start, end - start));
        } catch (...) {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

json make_method_command(const std::string& function,
                         json               parameters,
                         json               metadata = json::object(),
                         json               tags     = json::array())
{
    json cmd;
    cmd["command"] = {
        {"function", function},
        {"metadata", std::move(metadata)},
        {"parameters", std::move(parameters)},
        {"tags", std::move(tags)}
    };
    return cmd;
}

bool starts_with_command(const std::string& line, const std::string& command)
{
    return boost::algorithm::starts_with(trim_copy(line), command);
}

int parse_layer_index(const std::string& comment)
{
    const auto colon_pos = comment.find(':');
    if (colon_pos == std::string::npos)
        return -1;

    std::string value = trim_copy(comment.substr(colon_pos + 1));
    const auto  slash = value.find('/');
    if (slash != std::string::npos)
        value = value.substr(0, slash);

    try {
        return std::stoi(value);
    } catch (...) {
        return -1;
    }
}

MethodBounds calculate_method_bounds(const std::vector<std::string>& lines)
{
    MethodBounds bounds;

    double min_x = std::numeric_limits<double>::max();
    double min_y = std::numeric_limits<double>::max();
    double min_z = std::numeric_limits<double>::max();
    double max_x = std::numeric_limits<double>::lowest();
    double max_y = std::numeric_limits<double>::lowest();
    double max_z = std::numeric_limits<double>::lowest();

    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    bool   relative_xyz = false;

    for (const std::string& raw_line : lines) {
        const std::string line = trim_copy(raw_line);
        if (line.empty() || line[0] == ';')
            continue;

        if (starts_with_command(line, "G90")) {
            relative_xyz = false;
            continue;
        }
        if (starts_with_command(line, "G91")) {
            relative_xyz = true;
            continue;
        }
        if (starts_with_command(line, "G92")) {
            if (auto value = find_gcode_value(line, 'X'); value.has_value()) x = *value;
            if (auto value = find_gcode_value(line, 'Y'); value.has_value()) y = *value;
            if (auto value = find_gcode_value(line, 'Z'); value.has_value()) z = *value;
            continue;
        }
        if (!starts_with_command(line, "G0") && !starts_with_command(line, "G1"))
            continue;

        bool updated = false;
        if (auto value = find_gcode_value(line, 'X'); value.has_value()) {
            x = relative_xyz ? (x + *value) : *value;
            updated = true;
        }
        if (auto value = find_gcode_value(line, 'Y'); value.has_value()) {
            y = relative_xyz ? (y + *value) : *value;
            updated = true;
        }
        if (auto value = find_gcode_value(line, 'Z'); value.has_value()) {
            z = relative_xyz ? (z + *value) : *value;
            updated = true;
        }

        if (!updated)
            continue;

        min_x = std::min(min_x, x);
        min_y = std::min(min_y, y);
        min_z = std::min(min_z, z);
        max_x = std::max(max_x, x);
        max_y = std::max(max_y, y);
        max_z = std::max(max_z, z);
        bounds.valid = true;
    }

    if (bounds.valid) {
        bounds.min_x = min_x;
        bounds.max_x = max_x;
        bounds.min_y = min_y;
        bounds.max_y = max_y;
        bounds.min_z = min_z;
        bounds.max_z = max_z;
    }

    return bounds;
}

json make_accel_entry(double rate_mm_per_s_sq)
{
    return {
        {"rate_mm_per_s_sq", {{"x", rate_mm_per_s_sq}, {"y", rate_mm_per_s_sq}}},
        {"max_speed_change_mm_per_s", {{"x", 12.5}, {"y", 12.5}}}
    };
}

json make_method_accel_overrides()
{
    json bead_mode;
    bead_mode["Travel Move"] = make_accel_entry(5000.0);

    static const std::array<const char*, 8> tag_prefixes = {
        "FILL",
        "PRIME_TOWER",
        "TOP_SURFACE",
        "SUPPORT",
        "SUPPORT_INTERFACE",
        "WALL_OUTER",
        "WALL_INNER",
        "SKIRT"
    };

    for (const char *prefix : tag_prefixes) {
        bead_mode[std::string(prefix) + "_0"] = make_accel_entry(800.0);
        bead_mode[std::string(prefix) + "_1"] = make_accel_entry(800.0);
    }

    return {
        {"bead_mode", bead_mode},
        {"rate_mm_per_s_sq", {{"x", 800.0}, {"y", 800.0}}},
        {"max_speed_change_mm_per_s", {{"x", 12.5}, {"y", 12.5}}}
    };
}

} // namespace

void MakerBotWriter::set_thumbnail_data(const std::vector<uint8_t>& png_data)
{
    if (png_data.empty())
        return;

    std::string filename = "thumbnail.png";
    if (!m_config.thumbnails.empty() && !m_config.thumbnails.front().filename.empty())
        filename = m_config.thumbnails.front().filename;

    m_context.set_thumbnails({{png_data, filename}});
}

GCodeMetadata MakerBotWriter::parse_gcode(const std::vector<std::string>& lines)
{
    m_original_lines = lines;
    return GCodeContainerWriter::parse_gcode(lines);
}

bool MakerBotWriter::is_method_archive() const
{
    return std::find(m_config.zip_files.begin(), m_config.zip_files.end(), "print.jsontoolpath") != m_config.zip_files.end();
}

void MakerBotWriter::override_metadata(GCodeMetadata& meta)
{
    m_context.propagate_extruder_data_if_needed();

    if (m_context.has_print_stats()) {
        meta.duration_s  = m_context.get_duration_s();
        meta.filament_mm = m_context.get_filament_mm();
        meta.filament_g  = m_context.get_filament_g();
    }

    const auto& extruders = m_context.get_extruder_data();
    if (m_context.has_any_extruder_data() && !extruders[0].material_guid.empty())
        meta.material_guid = extruders[0].material_guid;

    if (m_context.has_any_extruder_data() && !extruders[0].material_name.empty())
        meta.material_name = extruders[0].material_name;

    if (m_context.has_any_extruder_data() && extruders[0].extruder_temp > 0)
        meta.extruder_temp = extruders[0].extruder_temp;
}

std::pair<std::string, std::string> MakerBotWriter::get_bot_and_tool_type() const
{
    return std::make_pair(m_config.bot_type, m_config.tool_type);
}

std::string MakerBotWriter::generate_header(const GCodeMetadata& meta)
{
    std::ostringstream header;

    if (m_config.header_template == "griffin") {
        header << ";START_OF_HEADER\n";
        header << ";HEADER_VERSION:0.1\n";
        header << ";FLAVOR:Griffin\n";
        header << ";PRINT.TIME:" << meta.duration_s << "\n";
        header << ";SLICE_UUID:" << meta.slice_uuid << "\n";
        header << ";END_OF_HEADER\n\n";
    } else if (m_config.header_template == "marlin") {
        const double filament_m = meta.filament_mm / 1000.0;
        header << ";FLAVOR:Marlin\n";
        header << ";TIME:" << meta.duration_s << "\n";
        header << ";Filament used: " << std::fixed << std::setprecision(6) << filament_m << "m\n";
        header << ";Layer height: " << meta.layer_height << "\n";
        header << ";TARGET_MACHINE.NAME:" << m_config.target_machine << "\n\n";
    } else {
        header << ";START_OF_HEADER\n";
        header << ";HEADER_VERSION:0.1\n";
        header << ";FLAVOR:Griffin\n";
        header << ";PRINT.TIME:" << meta.duration_s << "\n";
        header << ";SLICE_UUID:" << meta.slice_uuid << "\n";
        header << ";END_OF_HEADER\n\n";
    }

    return header.str();
}

bool MakerBotWriter::write_container(const GCodeMetadata& meta, const std::string& gcode_content, const std::string& output_path)
{
    mz_zip_archive archive;
    mz_zip_zero_struct(&archive);

    if (!open_zip_writer(&archive, output_path)) {
        BOOST_LOG_TRIVIAL(error) << "MakerBotWriter::write_container: Failed to open zip writer";
        return false;
    }

    auto cleanup = [&]() { close_zip_writer(&archive); };

    const bool        method_archive   = is_method_archive();
    const std::string toolpath_name    = method_archive ? "print.jsontoolpath" : "print.gcode";
    const std::string toolpath_content = method_archive ? generate_method_toolpath_json() : gcode_content;

    if (!mz_zip_writer_add_mem(&archive,
                               toolpath_name.c_str(),
                               toolpath_content.c_str(),
                               toolpath_content.length(),
                               MZ_DEFAULT_COMPRESSION)) {
        BOOST_LOG_TRIVIAL(error) << "MakerBotWriter::write_container: Failed to add " << toolpath_name;
        cleanup();
        return false;
    }

    const std::string meta_json = method_archive ? generate_method_meta_json(meta) : generate_meta_json(meta);
    if (!mz_zip_writer_add_mem(&archive, "meta.json", meta_json.c_str(), meta_json.length(), MZ_DEFAULT_COMPRESSION)) {
        BOOST_LOG_TRIVIAL(error) << "MakerBotWriter::write_container: Failed to add meta.json";
        cleanup();
        return false;
    }

    const std::string slicemetadata = method_archive ? generate_method_slicemetadata_json(meta) : generate_slicemetadata_json(meta);
    if (!mz_zip_writer_add_mem(&archive, "slicemetadata.json", slicemetadata.c_str(), slicemetadata.length(), MZ_DEFAULT_COMPRESSION)) {
        BOOST_LOG_TRIVIAL(error) << "MakerBotWriter::write_container: Failed to add slicemetadata.json";
        cleanup();
        return false;
    }

    const auto& thumbnails = m_context.get_thumbnails();
    if (!thumbnails.empty()) {
        for (const auto& [thumbnail_data, filename] : thumbnails) {
            if (thumbnail_data.empty()) {
                BOOST_LOG_TRIVIAL(warning) << "MakerBotWriter: Skipping empty thumbnail: " << filename;
                continue;
            }

            if (!mz_zip_writer_add_mem(&archive,
                                       filename.c_str(),
                                       thumbnail_data.data(),
                                       thumbnail_data.size(),
                                       MZ_DEFAULT_COMPRESSION)) {
                BOOST_LOG_TRIVIAL(error) << "MakerBotWriter::write_container: Failed to add thumbnail '" << filename << "'";
            }
        }
    } else {
        BOOST_LOG_TRIVIAL(warning) << "MakerBotWriter::write_container: No thumbnail data available";
    }

    if (!mz_zip_writer_finalize_archive(&archive)) {
        BOOST_LOG_TRIVIAL(error) << "MakerBotWriter::write_container: Failed to finalize archive";
        cleanup();
        return false;
    }

    cleanup();
    return true;
}

std::string MakerBotWriter::generate_meta_json(const GCodeMetadata& meta)
{
    std::ostringstream json_stream;

    const auto [bot_type, tool_type] = get_bot_and_tool_type();
    const std::string material_code  = material_name_to_code(meta.material_name);
    const std::string file_uuid      = generate_uuid();

    json_stream << "{\n";
    json_stream << "  \"bot_type\": \"" << bot_type << "\",\n";
    json_stream << "  \"platform_temperature\": " << meta.bed_temp << ",\n";
    json_stream << "  \"build_plane_temperature\": " << m_config.build_plane_temperature << ",\n";
    json_stream << "  \"commanded_duration_s\": " << meta.duration_s << ",\n";
    json_stream << "  \"duration_s\": " << meta.duration_s << ",\n";
    json_stream << "  \"extrusion_distance_mm\": " << std::fixed << std::setprecision(1) << meta.filament_mm << ",\n";
    json_stream << "  \"extrusion_distances_mm\": [" << std::fixed << std::setprecision(1) << meta.filament_mm << "],\n";
    json_stream << "  \"extrusion_mass_g\": " << std::fixed << std::setprecision(6) << meta.filament_g << ",\n";
    json_stream << "  \"extrusion_masses_g\": [" << std::fixed << std::setprecision(6) << meta.filament_g << "],\n";
    json_stream << "  \"uuid\": \"" << file_uuid << "\",\n";
    json_stream << "  \"material\": \"" << material_code << "\",\n";
    json_stream << "  \"materials\": [\"" << material_code << "\"],\n";
    json_stream << "  \"extruder_temperature\": " << meta.extruder_temp << ",\n";
    json_stream << "  \"extruder_temperatures\": [" << meta.extruder_temp << "],\n";
    json_stream << "  \"tool_type\": \"" << tool_type << "\",\n";
    json_stream << "  \"tool_types\": [\"" << tool_type << "\"],\n";
    json_stream << "  \"version\": \"" << m_config.version << "\",\n";
    json_stream << "  \"model_counts\": [{\"count\": 1, \"name\": \"instance0\"}],\n";
    json_stream << "  \"preferences\": {\n";
    json_stream << "    \"instance0\": {\n";
    json_stream << "      \"machineBounds\": [" << m_config.machine_bounds[0] << ", " << m_config.machine_bounds[1] << ", " << m_config.machine_bounds[2] << ", " << m_config.machine_bounds[3] << "],\n";
    json_stream << "      \"printMode\": \"default\"\n";
    json_stream << "    }\n";
    json_stream << "  },\n";

    if (m_config.header_template == "marlin")
        json_stream << "  \"extruderProfiles\": [{\"nozzle_diameter\": 0.4, \"tool_type\": \"" << tool_type << "\"}],\n";

    json_stream << "  \"miracle_config\": {\n";
    json_stream << "    \"gaggles\": {\"instance0\": {}},\n";
    json_stream << "    \"curaengine_version\": \"" << m_config.miracle_config.curaengine_version << "\",\n";
    json_stream << "    \"curaengine_commit_hash\": \"" << m_config.miracle_config.curaengine_commit_hash << "\",\n";
    json_stream << "    \"dulcificum_version\": \"" << m_config.miracle_config.dulcificum_version << "\",\n";
    json_stream << "    \"dulcificum_commit_hash\": \"" << m_config.miracle_config.dulcificum_commit_hash << "\",\n";
    json_stream << "    \"makerbot_writer_version\": \"" << m_config.miracle_config.makerbot_writer_version << "\",\n";
    json_stream << "    \"pyDulcificum_version\": \"" << m_config.miracle_config.pyDulcificum_version << "\"\n";
    json_stream << "  },\n";
    json_stream << "  \"curaengine_version\": \"" << m_config.miracle_config.curaengine_version << "\",\n";
    json_stream << "  \"curaengine_commit_hash\": \"" << m_config.miracle_config.curaengine_commit_hash << "\",\n";
    json_stream << "  \"dulcificum_version\": \"" << m_config.miracle_config.dulcificum_version << "\",\n";
    json_stream << "  \"dulcificum_commit_hash\": \"" << m_config.miracle_config.dulcificum_commit_hash << "\",\n";
    json_stream << "  \"makerbot_writer_version\": \"" << m_config.miracle_config.makerbot_writer_version << "\",\n";
    json_stream << "  \"pyDulcificum_version\": \"" << m_config.miracle_config.pyDulcificum_version << "\"\n";
    json_stream << "}\n";
    return json_stream.str();
}

std::string MakerBotWriter::generate_slicemetadata_json(const GCodeMetadata& meta)
{
    namespace fs = boost::filesystem;

    const std::string template_filename = (m_config.header_template == "marlin")
        ? "slicemetadata_sprint_template.json"
        : "slicemetadata_small_template.json";

    const std::vector<std::string> template_paths = {
        (fs::path(Slic3r::resources_dir()) / "formats" / "makerbot" / template_filename).string(),
        (fs::path("resources") / "formats" / "makerbot" / template_filename).string(),
        template_filename
    };

    std::string template_content;
    bool        found = false;
    for (const auto& path : template_paths) {
        boost::nowide::ifstream file(path);
        if (file.is_open()) {
            template_content = std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            found = true;
            break;
        }
    }

    if (!found) {
        BOOST_LOG_TRIVIAL(warning) << "MakerBotWriter: Template " << template_filename << " not found, using minimal fallback";
        return generate_slicemetadata_json_minimal(meta);
    }

    try {
        json root = json::parse(template_content);

        if (root.contains("material")) {
            const bool   use_meters   = (m_config.header_template == "marlin");
            const double length_value = use_meters ? (meta.filament_mm / 1000.0) : meta.filament_mm;

            if (root["material"].contains("length") && root["material"]["length"].is_array() && !root["material"]["length"].empty())
                root["material"]["length"][0] = length_value;
            if (root["material"].contains("weight") && root["material"]["weight"].is_array() && !root["material"]["weight"].empty())
                root["material"]["weight"][0] = meta.filament_g;
        }

        if (root.contains("quality"))
            root["quality"]["layer_height"] = meta.layer_height;

        if (root.contains("global") && root["global"].contains("all_settings")) {
            auto& settings = root["global"]["all_settings"];
            settings["material_guid"]               = meta.material_guid;
            settings["material_type"]               = meta.material_type;
            settings["layer_height"]                = meta.layer_height;
            settings["infill_sparse_density"]       = meta.infill_percent;
            settings["material_bed_temperature"]    = meta.bed_temp;
            settings["material_print_temperature"]  = meta.extruder_temp;
            settings["machine_name"]                = m_config.target_machine;
        }

        if (root.contains("extruder_0") && root["extruder_0"].contains("all_settings")) {
            auto& settings = root["extruder_0"]["all_settings"];
            settings["material_guid"]         = meta.material_guid;
            settings["material_type"]         = meta.material_type;
            settings["layer_height"]          = meta.layer_height;
            settings["infill_sparse_density"] = meta.infill_percent;
            settings["machine_name"]          = m_config.target_machine;
        }

        return root.dump();
    } catch (const std::exception&) {
        return generate_slicemetadata_json_minimal(meta);
    }
}

std::string MakerBotWriter::generate_slicemetadata_json_minimal(const GCodeMetadata& meta)
{
    const bool   use_meters   = (m_config.header_template == "marlin");
    const double length_value = use_meters ? (meta.filament_mm / 1000.0) : meta.filament_mm;
    const int    precision    = use_meters ? 5 : 1;

    std::ostringstream json_stream;
    json_stream << "{\n";
    json_stream << "  \"material\": {\n";
    json_stream << "    \"length\": [" << std::fixed << std::setprecision(precision) << length_value << "],\n";
    json_stream << "    \"weight\": [" << std::fixed << std::setprecision(6) << meta.filament_g << "],\n";
    json_stream << "    \"cost\": [0.0]\n";
    json_stream << "  },\n";
    json_stream << "  \"quality\": {\n";
    json_stream << "    \"intent_category\": \"default\",\n";
    json_stream << "    \"intent_name\": \"Balanced\",\n";
    json_stream << "    \"profile\": \"Normal\",\n";
    json_stream << "    \"custom_profile\": null,\n";
    json_stream << "    \"layer_height\": " << meta.layer_height << ",\n";
    json_stream << "    \"is_experimental\": false\n";
    json_stream << "  },\n";
    json_stream << "  \"global\": {\n";
    json_stream << "    \"changes\": {},\n";
    json_stream << "    \"all_settings\": {\n";
    json_stream << "      \"material_guid\": \"" << meta.material_guid << "\",\n";
    json_stream << "      \"material_type\": \"" << meta.material_type << "\",\n";
    json_stream << "      \"layer_height\": " << meta.layer_height << ",\n";
    json_stream << "      \"infill_sparse_density\": " << meta.infill_percent << "\n";
    json_stream << "    }\n";
    json_stream << "  },\n";
    json_stream << "  \"extruder_0\": {\n";
    json_stream << "    \"changes\": {},\n";
    json_stream << "    \"all_settings\": {\n";
    json_stream << "      \"machine_name\": \"" << m_config.target_machine << "\",\n";
    json_stream << "      \"material_guid\": \"" << meta.material_guid << "\",\n";
    json_stream << "      \"material_type\": \"" << meta.material_type << "\",\n";
    json_stream << "      \"layer_height\": " << meta.layer_height << ",\n";
    json_stream << "      \"infill_sparse_density\": " << meta.infill_percent << "\n";
    json_stream << "    }\n";
    json_stream << "  }\n";
    json_stream << "}\n";
    return json_stream.str();
}

std::vector<std::string> MakerBotWriter::get_method_source_lines() const
{
    if (m_original_lines.empty())
        return {};

    size_t body_start       = 0;
    bool   found_end_header = false;

    for (size_t i = 0; i < m_original_lines.size(); ++i) {
        if (m_original_lines[i].find(";END_OF_HEADER") != std::string::npos) {
            body_start       = i + 1;
            found_end_header = true;
            break;
        }
    }

    if (!found_end_header) {
        for (size_t i = 0; i < m_original_lines.size(); ++i) {
            const std::string trimmed = trim_copy(m_original_lines[i]);
            if (boost::algorithm::starts_with(trimmed, "G") ||
                boost::algorithm::starts_with(trimmed, "M") ||
                boost::algorithm::starts_with(trimmed, "T")) {
                body_start = i;
                break;
            }
        }
    }

    return std::vector<std::string>(m_original_lines.begin() + body_start, m_original_lines.end());
}

std::string MakerBotWriter::generate_method_toolpath_json()
{
    const std::vector<std::string> lines = get_method_source_lines();
    json                           commands = json::array();

    double             x = 0.0;
    double             y = 0.0;
    double             z = 0.0;
    std::array<double, 2> e_absolute = {0.0, 0.0};
    bool               relative_xyz = false;
    bool               relative_e   = false;
    double             feedrate_mm_s = 0.0;
    int                active_tool = 0;
    std::string        current_type_comment;
    int                last_layer_comment = -1;
    bool               first_print_move_seen = false;

    auto emit_layer_comment = [&](int layer) {
        if (layer < 0 || layer == last_layer_comment)
            return;
        commands.push_back(make_method_command(
            "comment",
            {{"comment", "Layer Section " + std::to_string(layer) + " (0)"}}));
        last_layer_comment = layer;
    };

    for (const std::string& raw_line : lines) {
        const std::string trimmed = trim_copy(raw_line);
        if (trimmed.empty())
            continue;

        if (trimmed[0] == ';') {
            if (boost::algorithm::starts_with(trimmed, ";TYPE:")) {
                current_type_comment = trim_copy(trimmed.substr(6));
            } else if (boost::algorithm::starts_with(trimmed, ";LAYER:")) {
                emit_layer_comment(parse_layer_index(trimmed));
            } else if (boost::algorithm::starts_with(trimmed, "; layer num/total_layer_count:")) {
                emit_layer_comment(parse_layer_index(trimmed));
            } else if (boost::algorithm::starts_with(trimmed, "; CHANGE_LAYER") ||
                       boost::algorithm::starts_with(trimmed, ";LAYER_CHANGE")) {
                emit_layer_comment(last_layer_comment < 0 ? 0 : (last_layer_comment + 1));
            }
            continue;
        }

        if (starts_with_command(trimmed, "G90")) {
            relative_xyz = false;
            continue;
        }
        if (starts_with_command(trimmed, "G91")) {
            relative_xyz = true;
            continue;
        }
        if (starts_with_command(trimmed, "M82")) {
            relative_e = false;
            continue;
        }
        if (starts_with_command(trimmed, "M83")) {
            relative_e = true;
            continue;
        }
        if (starts_with_command(trimmed, "G92")) {
            if (auto value = find_gcode_value(trimmed, 'X'); value.has_value()) x = *value;
            if (auto value = find_gcode_value(trimmed, 'Y'); value.has_value()) y = *value;
            if (auto value = find_gcode_value(trimmed, 'Z'); value.has_value()) z = *value;
            if (auto value = find_gcode_value(trimmed, 'E'); value.has_value()) e_absolute[active_tool] = *value;
            continue;
        }
        if (starts_with_command(trimmed, "T0") || starts_with_command(trimmed, "T1")) {
            if (auto value = find_gcode_value(trimmed, 'T'); value.has_value()) {
                active_tool = std::clamp(static_cast<int>(*value), 0, 1);
                commands.push_back(make_method_command("change_toolhead", {{"index", active_tool}}));
            }
            continue;
        }
        if (starts_with_command(trimmed, "M104") || starts_with_command(trimmed, "M109")) {
            const auto temperature = find_gcode_value(trimmed, 'S');
            int        target_tool = active_tool;
            if (auto value = find_gcode_value(trimmed, 'T'); value.has_value())
                target_tool = std::clamp(static_cast<int>(*value), 0, 1);

            if (temperature.has_value()) {
                commands.push_back(make_method_command(
                    "set_toolhead_temperature",
                    {{"index", target_tool}, {"temperature", static_cast<int>(std::lround(*temperature))}}));
            }

            if (starts_with_command(trimmed, "M109")) {
                commands.push_back(make_method_command("wait_for_temperature", {{"index", target_tool}}));
            }
            continue;
        }
        if (starts_with_command(trimmed, "M106")) {
            int fan_index = active_tool;
            if (auto value = find_gcode_value(trimmed, 'P'); value.has_value())
                fan_index = std::clamp(static_cast<int>(*value), 0, 1);

            const double duty_raw = find_gcode_value(trimmed, 'S').value_or(255.0);
            const double duty     = std::clamp(duty_raw / 255.0, 0.0, 1.0);

            commands.push_back(make_method_command("toggle_fan", {{"index", fan_index}, {"value", duty > 0.0}}));
            if (duty > 0.0)
                commands.push_back(make_method_command("fan_duty", {{"index", fan_index}, {"value", duty}}));
            continue;
        }
        if (starts_with_command(trimmed, "M107")) {
            int fan_index = active_tool;
            if (auto value = find_gcode_value(trimmed, 'P'); value.has_value())
                fan_index = std::clamp(static_cast<int>(*value), 0, 1);
            commands.push_back(make_method_command("toggle_fan", {{"index", fan_index}, {"value", false}}));
            continue;
        }
        if (!starts_with_command(trimmed, "G0") && !starts_with_command(trimmed, "G1"))
            continue;

        const auto x_value = find_gcode_value(trimmed, 'X');
        const auto y_value = find_gcode_value(trimmed, 'Y');
        const auto z_value = find_gcode_value(trimmed, 'Z');
        const auto e_value = find_gcode_value(trimmed, 'E');
        const auto f_value = find_gcode_value(trimmed, 'F');

        if (!x_value.has_value() && !y_value.has_value() && !z_value.has_value() && !e_value.has_value())
            continue;

        if (last_layer_comment < 0 && e_value.has_value() && std::abs(*e_value) > 1e-9)
            emit_layer_comment(0);

        double move_x = 0.0;
        double move_y = 0.0;
        double move_z = relative_xyz ? 0.0 : z;
        bool   rel_x  = true;
        bool   rel_y  = true;
        bool   rel_z  = relative_xyz;

        bool has_motion = false;
        if (x_value.has_value()) {
            has_motion = true;
            if (relative_xyz) {
                move_x = *x_value;
                rel_x  = true;
                x += *x_value;
            } else {
                x      = *x_value;
                move_x = x;
                rel_x  = false;
            }
        }
        if (y_value.has_value()) {
            has_motion = true;
            if (relative_xyz) {
                move_y = *y_value;
                rel_y  = true;
                y += *y_value;
            } else {
                y      = *y_value;
                move_y = y;
                rel_y  = false;
            }
        }
        if (z_value.has_value()) {
            has_motion = true;
            if (relative_xyz) {
                move_z = *z_value;
                rel_z  = true;
                z += *z_value;
            } else {
                z      = *z_value;
                move_z = z;
                rel_z  = false;
            }
        }

        if (f_value.has_value())
            feedrate_mm_s = *f_value / 60.0;

        double extrusion_delta = 0.0;
        if (e_value.has_value()) {
            if (relative_e) {
                extrusion_delta = *e_value;
                e_absolute[active_tool] += *e_value;
            } else {
                extrusion_delta = *e_value - e_absolute[active_tool];
                e_absolute[active_tool] = *e_value;
            }
        }

        double move_a = 0.0;
        double move_b = 0.0;
        if (active_tool == 0)
            move_a = extrusion_delta;
        else
            move_b = extrusion_delta;

        json tags = json::array();
        if (extrusion_delta < -1e-6) {
            tags.push_back("Retract");
        } else if (extrusion_delta > 1e-6 && !has_motion) {
            tags.push_back("Restart");
        } else if (extrusion_delta > 1e-6) {
            const std::string method_tag = comment_to_method_tag(current_type_comment, active_tool);
            tags.push_back(method_tag.empty() ? "Travel Move" : method_tag);
        } else {
            tags.push_back("Travel Move");
        }

        commands.push_back(make_method_command(
            "move",
            {
                {"a", move_a},
                {"b", move_b},
                {"feedrate", feedrate_mm_s},
                {"x", move_x},
                {"y", move_y},
                {"z", move_z}
            },
            {
                {"relative", {
                    {"a", true},
                    {"b", true},
                    {"x", rel_x},
                    {"y", rel_y},
                    {"z", rel_z}
                }}
            },
            tags));

        first_print_move_seen = true;
    }

    if (!first_print_move_seen)
        BOOST_LOG_TRIVIAL(warning) << "MakerBotWriter::generate_method_toolpath_json: No Method moves were generated";

    return commands.dump();
}

std::string MakerBotWriter::generate_method_meta_json(const GCodeMetadata& meta)
{
    m_context.propagate_extruder_data_if_needed();

    const auto& extruders = m_context.get_extruder_data();
    const auto& variants  = m_context.get_extruder_variants();

    auto tool_for_index = [&](int idx) {
        if (idx < static_cast<int>(variants.size())) {
            const std::string mapped = tool_variant_to_method_tool(m_config.id, variants[idx]);
            if (!mapped.empty())
                return mapped;
        }
        return m_config.tool_type.empty() ? std::string("mk14") : m_config.tool_type;
    };

    auto material_name_for_index = [&](int idx) -> std::string {
        if (!extruders[idx].material_name.empty())
            return extruders[idx].material_name;
        return meta.material_name;
    };

    auto material_guid_for_index = [&](int idx) -> std::string {
        const std::string inferred = material_name_to_guid(material_name_for_index(idx), extruders[idx].material_guid);
        if (!inferred.empty())
            return inferred;
        return meta.material_guid;
    };

    auto material_code_for_index = [&](int idx) {
        return material_name_to_code(material_name_for_index(idx), material_guid_for_index(idx));
    };

    auto temperature_for_index = [&](int idx) {
        return extruders[idx].extruder_temp > 0 ? extruders[idx].extruder_temp : meta.extruder_temp;
    };

    const std::vector<std::string> source_lines = get_method_source_lines();
    const MethodBounds             bounds       = calculate_method_bounds(source_lines);

    json root;
    root["bot_type"]                = m_config.bot_type;
    root["bounding_box"]            = {
        {"x_min", bounds.valid ? bounds.min_x : meta.min_x},
        {"x_max", bounds.valid ? bounds.max_x : meta.max_x},
        {"y_min", bounds.valid ? bounds.min_y : meta.min_y},
        {"y_max", bounds.valid ? bounds.max_y : meta.max_y},
        {"z_min", bounds.valid ? bounds.min_z : meta.min_z},
        {"z_max", bounds.valid ? bounds.max_z : meta.max_z}
    };
    root["platform_temperature"]    = meta.bed_temp;
    root["build_plane_temperature"] = m_config.build_plane_temperature;
    root["commanded_duration_s"]    = meta.duration_s;
    root["duration_s"]              = meta.duration_s;
    root["extrusion_distance_mm"]   = meta.filament_mm;
    root["extrusion_distances_mm"]  = {extruders[0].filament_mm, extruders[1].filament_mm};
    root["extrusion_mass_g"]        = meta.filament_g;
    root["extrusion_masses_g"]      = {extruders[0].filament_g, extruders[1].filament_g};
    root["uuid"]                    = generate_uuid();
    root["material"]                = material_code_for_index(0);
    root["materials"]               = {material_code_for_index(0), material_code_for_index(1)};
    root["extruder_temperature"]    = temperature_for_index(0);
    root["extruder_temperatures"]   = {temperature_for_index(0), temperature_for_index(1)};
    root["model_counts"]            = json::array({{{"count", 1}, {"name", "instance0"}}});
    root["tool_type"]               = tool_for_index(0);
    root["tool_types"]              = {tool_for_index(0), tool_for_index(1)};
    root["version"]                 = m_config.version;
    root["preferences"]             = {
        {"instance0", {
            {"machineBounds", m_config.machine_bounds},
            {"printMode", "default"}
        }}
    };
    root["accel_overrides"]         = make_method_accel_overrides();
    root["miracle_config"]          = {
        {"gaggles", {{"instance0", json::object()}}},
        {"curaengine_version", m_config.miracle_config.curaengine_version},
        {"curaengine_commit_hash", m_config.miracle_config.curaengine_commit_hash},
        {"dulcificum_version", m_config.miracle_config.dulcificum_version},
        {"dulcificum_commit_hash", m_config.miracle_config.dulcificum_commit_hash},
        {"makerbot_writer_version", m_config.miracle_config.makerbot_writer_version},
        {"pyDulcificum_version", m_config.miracle_config.pyDulcificum_version}
    };
    root["curaengine_version"]      = m_config.miracle_config.curaengine_version;
    root["curaengine_commit_hash"]  = m_config.miracle_config.curaengine_commit_hash;
    root["dulcificum_version"]      = m_config.miracle_config.dulcificum_version;
    root["dulcificum_commit_hash"]  = m_config.miracle_config.dulcificum_commit_hash;
    root["makerbot_writer_version"] = m_config.miracle_config.makerbot_writer_version;
    root["pyDulcificum_version"]    = m_config.miracle_config.pyDulcificum_version;

    return root.dump(4);
}

std::string MakerBotWriter::generate_method_slicemetadata_json(const GCodeMetadata& meta)
{
    m_context.propagate_extruder_data_if_needed();

    const auto& extruders = m_context.get_extruder_data();
    const auto& variants  = m_context.get_extruder_variants();

    auto tool_for_index = [&](int idx) {
        if (idx < static_cast<int>(variants.size())) {
            const std::string mapped = tool_variant_to_method_tool(m_config.id, variants[idx]);
            if (!mapped.empty())
                return mapped;
        }
        return m_config.tool_type.empty() ? std::string("mk14") : m_config.tool_type;
    };

    auto material_name_for_index = [&](int idx) -> std::string {
        if (!extruders[idx].material_name.empty())
            return extruders[idx].material_name;
        return meta.material_name;
    };

    auto material_guid_for_index = [&](int idx) -> std::string {
        const std::string inferred = material_name_to_guid(material_name_for_index(idx), extruders[idx].material_guid);
        if (!inferred.empty())
            return inferred;
        return meta.material_guid;
    };

    auto material_code_for_index = [&](int idx) {
        return material_name_to_code(material_name_for_index(idx), material_guid_for_index(idx));
    };

    auto temperature_for_index = [&](int idx) {
        return extruders[idx].extruder_temp > 0 ? extruders[idx].extruder_temp : meta.extruder_temp;
    };

    auto make_extruder_settings = [&](int idx) {
        return json{
            {"machine_name", m_config.bot_type},
            {"machine_nozzle_id", tool_for_index(idx)},
            {"machine_nozzle_size", 0.4},
            {"machine_gcode_flavor", "Griffin"},
            {"machine_extruder_count", 2},
            {"extruders_enabled_count", 2},
            {"layer_height", meta.layer_height},
            {"infill_sparse_density", meta.infill_percent},
            {"material_guid", material_guid_for_index(idx)},
            {"material_type", material_code_for_index(idx)},
            {"material_bed_temperature", meta.bed_temp},
            {"material_bed_temperature_layer_0", meta.bed_temp},
            {"material_print_temperature", temperature_for_index(idx)},
            {"material_print_temperature_layer_0", temperature_for_index(idx)},
            {"default_material_bed_temperature", meta.bed_temp},
            {"default_material_print_temperature", temperature_for_index(idx)},
            {"build_volume_temperature", m_config.build_plane_temperature},
            {"machine_heated_build_volume", true},
            {"machine_width", std::abs(m_config.machine_bounds.size() > 0 ? m_config.machine_bounds[0] * 2.0 : 0.0)},
            {"machine_depth", std::abs(m_config.machine_bounds.size() > 1 ? m_config.machine_bounds[1] * 2.0 : 0.0)}
        };
    };

    json root;
    root["material"] = {
        {"length", {extruders[0].filament_mm / 1000.0, extruders[1].filament_mm / 1000.0}},
        {"weight", {extruders[0].filament_g, extruders[1].filament_g}},
        {"cost", {0.0, 0.0}}
    };
    root["global"] = {
        {"changes", json::object()},
        {"all_settings", {
            {"machine_name", m_config.bot_type},
            {"machine_nozzle_id", "unknown"},
            {"machine_nozzle_size", 0.4},
            {"machine_gcode_flavor", "Griffin"},
            {"machine_extruder_count", 2},
            {"extruders_enabled_count", 2},
            {"layer_height", meta.layer_height},
            {"infill_sparse_density", meta.infill_percent},
            {"material_guid", ""},
            {"material_type", ""},
            {"material_bed_temperature", meta.bed_temp},
            {"material_bed_temperature_layer_0", meta.bed_temp},
            {"material_print_temperature", temperature_for_index(0)},
            {"material_print_temperature_layer_0", temperature_for_index(0)},
            {"build_volume_temperature", m_config.build_plane_temperature},
            {"machine_heated_build_volume", true}
        }}
    };
    root["quality"] = {
        {"intent_category", "default"},
        {"intent_name", "Balanced"},
        {"profile", "Normal"},
        {"custom_profile", nullptr},
        {"layer_height", meta.layer_height},
        {"is_experimental", false}
    };
    root["extruder_0"] = {
        {"changes", json::object()},
        {"all_settings", make_extruder_settings(0)}
    };
    root["extruder_1"] = {
        {"changes", json::object()},
        {"all_settings", make_extruder_settings(1)}
    };

    return root.dump(4);
}

} // namespace Slic3r
