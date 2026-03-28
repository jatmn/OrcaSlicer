#include "GCodeContainerWriter.hpp"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <boost/filesystem.hpp>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>

namespace Slic3r {

bool GCodeContainerWriter::write(const std::string& input_gcode_path, const std::string& output_path) {
    BOOST_LOG_TRIVIAL(info) << "GCodeContainerWriter::write: input=" << input_gcode_path << ", output=" << output_path;
    try {
        boost::nowide::ifstream file(input_gcode_path);
        if (!file.is_open()) {
            BOOST_LOG_TRIVIAL(error) << "GCodeContainerWriter::write: Failed to open input file: " << input_gcode_path;
            return false;
        }
        
        // Check file size first
        boost::system::error_code ec;
        auto file_size = boost::filesystem::file_size(input_gcode_path, ec);
        if (ec) {
            BOOST_LOG_TRIVIAL(error) << "GCodeContainerWriter::write: Failed to get file size: " << ec.message();
            return false;
        }
        BOOST_LOG_TRIVIAL(info) << "GCodeContainerWriter::write: Input file size=" << file_size;
        
        std::vector<std::string> lines;
        lines.reserve(file_size / 80);  // Rough estimate of line count
        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(line + "\n");
        }
        BOOST_LOG_TRIVIAL(info) << "GCodeContainerWriter::write: Read " << lines.size() << " lines from G-code file";
        
        bool result = write_from_lines(lines, output_path);
        BOOST_LOG_TRIVIAL(info) << "GCodeContainerWriter::write: write_from_lines returned " << result;
        return result;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "GCodeContainerWriter::write: Exception: " << e.what();
        return false;
    }
}

bool GCodeContainerWriter::write_from_memory(const std::string& gcode_data, const std::string& output_path) {
    std::vector<std::string> lines;
    std::istringstream stream(gcode_data);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line + "\n");
    }
    return write_from_lines(lines, output_path);
}

bool GCodeContainerWriter::write_from_lines(const std::vector<std::string>& lines, const std::string& output_path) {
    GCodeMetadata meta = parse_gcode(lines);
    
    BOOST_LOG_TRIVIAL(error) << "GCodeContainerWriter::write_from_lines: parsed metadata - "
                          << "duration_s=" << meta.duration_s
                          << ", filament_mm=" << meta.filament_mm
                          << ", filament_g=" << meta.filament_g
                          << ", material_type=" << meta.material_type
                          << ", material_guid=" << meta.material_guid
                          << ", gcode_body lines=" << meta.gcode_body.size();
    
    // Allow subclasses to override metadata with injected values
    override_metadata(meta);
    
    BOOST_LOG_TRIVIAL(error) << "GCodeContainerWriter::write_from_lines: after override - "
                          << "duration_s=" << meta.duration_s
                          << ", filament_mm=" << meta.filament_mm
                          << ", filament_g=" << meta.filament_g;
    
    // Build G-code content with Python-script-style processing (pass original lines for proper body extraction, retraction fix, etc.)
    std::string gcode_content = build_gcode_content(meta, lines);
    
    return write_container(meta, gcode_content, output_path);
}

GCodeMetadata GCodeContainerWriter::parse_gcode(const std::vector<std::string>& lines) {
    GCodeMetadata meta;
    
    // Defaults
    meta.duration_s = 0;
    meta.filament_mm = 0.0;
    meta.filament_g = 0.0;
    meta.layer_height = 0.2;
    meta.infill_percent = 15;
    meta.extruder_temp = 210;
    meta.bed_temp = 55;
    meta.material_name = "pla";
    meta.material_type = "PLA";
    meta.material_guid = "";
    meta.min_x = meta.min_y = meta.min_z = 0.0;
    meta.max_x = meta.max_y = meta.max_z = 100.0;
    meta.slice_uuid = generate_uuid();
    
    std::string content;
    for (const auto& line : lines) {
        content += line;
    }
    
    // Parse bracketed metadata
    boost::regex guid_regex(R"(\[GUID:\s*([^\]]+)\])", boost::regex::icase);
    boost::regex type_regex(R"(\[MATERIAL_TYPE:\s*([^\]]+)\])", boost::regex::icase);
    boost::regex name_regex(R"(\[MATERIAL_NAME:\s*([^\]]+)\])", boost::regex::icase);
    
    boost::smatch match;
    if (boost::regex_search(content, match, guid_regex)) {
        meta.material_guid = match[1];
        boost::algorithm::trim(meta.material_guid);
        boost::algorithm::to_lower(meta.material_guid);
    }
    
    if (boost::regex_search(content, match, type_regex)) {
        meta.material_type = match[1];
        boost::algorithm::trim(meta.material_type);
    }
    
    if (boost::regex_search(content, match, name_regex)) {
        meta.material_name = match[1];
        boost::algorithm::trim(meta.material_name);
        boost::algorithm::to_lower(meta.material_name);
        boost::algorithm::replace_all(meta.material_name, " ", "-");
    }
    
    // Parse temperatures
    boost::regex temp_regex(R"(M10[49] S([\d\.]+))");
    boost::regex bed_regex(R"(M1[49]0 S([\d\.]+))");
    boost::regex temp_comment_regex(R"(; first_layer_temperature = ([\d\.]+))");
    boost::regex bed_comment_regex(R"(; first_layer_bed_temperature = ([\d\.]+))");
    
    if (boost::regex_search(content, match, temp_regex) || 
        boost::regex_search(content, match, temp_comment_regex)) {
        meta.extruder_temp = static_cast<int>(std::stod(match[1]));
    }
    
    if (boost::regex_search(content, match, bed_regex) || 
        boost::regex_search(content, match, bed_comment_regex)) {
        meta.bed_temp = static_cast<int>(std::stod(match[1]));
    }
    
    // Parse time - use more specific regex to avoid matching other lines
    boost::regex time_regex(R"(; estimated printing time \([^)]+\)\s*=\s*([\dhms ]+))");
    if (boost::regex_search(content, match, time_regex)) {
        meta.duration_s = parse_time_string(match[1]);
        BOOST_LOG_TRIVIAL(error) << "GCodeContainerWriter::parse_gcode: Parsed duration_s=" << meta.duration_s << " from: " << match[1];
    } else {
        BOOST_LOG_TRIVIAL(error) << "GCodeContainerWriter::parse_gcode: Failed to parse estimated printing time";
    }
    
    // Parse filament
    boost::regex filament_regex(R"(; filament used \[mm\]\s*=\s*([\d\.]+))");
    boost::regex weight_regex(R"(; filament used \[g\]\s*=\s*([\d\.]+))");
    
    if (boost::regex_search(content, match, filament_regex)) {
        meta.filament_mm = std::stod(match[1]);
        BOOST_LOG_TRIVIAL(info) << "GCodeContainerWriter::parse_gcode: Parsed filament_mm=" << meta.filament_mm;
    } else {
        BOOST_LOG_TRIVIAL(warning) << "GCodeContainerWriter::parse_gcode: Failed to parse filament used [mm]";
    }
    
    if (boost::regex_search(content, match, weight_regex)) {
        meta.filament_g = std::stod(match[1]);
        BOOST_LOG_TRIVIAL(info) << "GCodeContainerWriter::parse_gcode: Parsed filament_g=" << meta.filament_g;
    } else {
        BOOST_LOG_TRIVIAL(warning) << "GCodeContainerWriter::parse_gcode: Failed to parse filament used [g]";
    }
    
    // Parse layer height
    boost::regex layer_regex(R"(; layer_height\s*=\s*([\d\.]+))");
    if (boost::regex_search(content, match, layer_regex)) {
        meta.layer_height = std::stod(match[1]);
    }
    
    // Parse infill
    boost::regex infill_regex(R"(; sparse_infill_density\s*=\s*([\d\.]+))");
    if (boost::regex_search(content, match, infill_regex)) {
        meta.infill_percent = static_cast<int>(std::stod(match[1]));
    }
    
    // Parse geometry bounds from G-code moves
    double min_x = 999999, min_y = 999999, min_z = 999999;
    double max_x = -999999, max_y = -999999, max_z = -999999;
    
    boost::regex move_regex(R"(G1 .*X([\d\.]+).*Y([\d\.]+))");
    boost::regex z_regex(R"(G1 .*Z([\d\.]+))");
    
    for (const auto& line : lines) {
        if (boost::regex_search(line, match, move_regex)) {
            double x = std::stod(match[1]);
            double y = std::stod(match[2]);
            min_x = std::min(min_x, x);
            max_x = std::max(max_x, x);
            min_y = std::min(min_y, y);
            max_y = std::max(max_y, y);
        }
        if (boost::regex_search(line, match, z_regex)) {
            double z = std::stod(match[1]);
            min_z = std::min(min_z, z);
            max_z = std::max(max_z, z);
        }
    }
    
    if (min_x != 999999) {
        meta.min_x = min_x; meta.max_x = max_x;
        meta.min_y = min_y; meta.max_y = max_y;
        meta.min_z = min_z; meta.max_z = max_z;
    }
    
    // Find G-code body start (purge sequence or printing object)
    size_t body_start = 0;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].find("; --- Purge Sequence ---") != std::string::npos ||
            lines[i].find("; printing object") != std::string::npos) {
            body_start = i;
            break;
        }
    }
    
    for (size_t i = body_start; i < lines.size(); ++i) {
        meta.gcode_body.push_back(lines[i]);
    }
    
    return meta;
}

std::string GCodeContainerWriter::substitute_template(const std::string& templ, 
                                                       const std::map<std::string, std::string>& values) {
    std::string result = templ;
    for (const auto& kv : values) {
        std::string placeholder = "{{" + kv.first + "}}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), kv.second);
            pos += kv.second.length();
        }
    }
    return result;
}

std::string GCodeContainerWriter::generate_uuid() {
    boost::uuids::random_generator generator;
    boost::uuids::uuid uuid = generator();
    return boost::uuids::to_string(uuid);
}

int GCodeContainerWriter::parse_time_string(const std::string& time_str) {
    int total_seconds = 0;
    
    boost::regex d_regex(R"((\d+)d)");
    boost::regex h_regex(R"((\d+)h)");
    boost::regex m_regex(R"((\d+)m)");
    boost::regex s_regex(R"((\d+)s)");
    boost::smatch match;
    
    if (boost::regex_search(time_str, match, d_regex)) {
        total_seconds += std::stoi(match[1]) * 86400;
    }
    if (boost::regex_search(time_str, match, h_regex)) {
        total_seconds += std::stoi(match[1]) * 3600;
    }
    if (boost::regex_search(time_str, match, m_regex)) {
        total_seconds += std::stoi(match[1]) * 60;
    }
    if (boost::regex_search(time_str, match, s_regex)) {
        total_seconds += std::stoi(match[1]);
    }
    
    return total_seconds;
}

std::string GCodeContainerWriter::build_gcode_content(const GCodeMetadata& meta, const std::vector<std::string>& original_lines) {
    BOOST_LOG_TRIVIAL(info) << "GCodeContainerWriter::build_gcode_content: Starting with " << original_lines.size() << " original lines";
    
    // Find body start: everything after the original ;END_OF_HEADER line
    // The new header replaces START_OF_HEADER..END_OF_HEADER, body is everything after
    size_t body_start = 0;
    bool found_end_of_header = false;
    for (size_t i = 0; i < original_lines.size(); ++i) {
        if (original_lines[i].find(";END_OF_HEADER") != std::string::npos) {
            body_start = i + 1;  // Start AFTER the END_OF_HEADER line
            found_end_of_header = true;
            BOOST_LOG_TRIVIAL(info) << "GCodeContainerWriter::build_gcode_content: Found END_OF_HEADER at line " << i;
            break;
        }
    }
    
    // If no END_OF_HEADER found, try to detect header end by looking for first G-code command
    // Common first commands: G0, G1, G28 (home), G92 (set position), M82/M83 (extruder mode)
    if (!found_end_of_header) {
        for (size_t i = 0; i < original_lines.size(); ++i) {
            const auto& l = original_lines[i];
            // Skip empty lines and comment-only lines
            size_t first_non_ws = l.find_first_not_of(" \t\r\n;");
            if (first_non_ws == std::string::npos) continue;
            
            // Check if this line starts with a real G-code command
            char first_char = l[first_non_ws];
            if (first_char == 'G' || first_char == 'M' || first_char == 'T') {
                // Found first G-code command - everything before this is header
                body_start = i;
                BOOST_LOG_TRIVIAL(info) << "GCodeContainerWriter::build_gcode_content: No END_OF_HEADER found, using first G-code command at line " << i;
                break;
            }
        }
    }
    
    // Generate new Cura-style header from template
    std::string header = generate_header(meta);
    
    // Debug: Log the generated header (first 500 chars to avoid log spam)
    BOOST_LOG_TRIVIAL(info) << "GCodeContainerWriter::build_gcode_content: Generated header (first 500 chars):\n" 
                           << header.substr(0, std::min(header.length(), size_t(500)));
    
    // Build body from original lines starting at body_start
    std::string body;
    for (size_t i = body_start; i < original_lines.size(); ++i) {
        body += original_lines[i];
    }
    
    // Fix end retraction and truncate at ";End of Gcode", then add Cura footer
    // This replicates the Python script's build_gcode() function
    std::vector<std::string> final_lines;
    
    // Split body into lines for processing
    std::vector<std::string> lines;
    std::istringstream stream(body);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line + "\n");
    }
    
    // Add header (split into lines)
    std::istringstream header_stream(header);
    while (std::getline(header_stream, line)) {
        final_lines.push_back(line + "\n");
    }
    
    // Add body lines
    for (size_t i = 0; i < lines.size(); ++i) {
        final_lines.push_back(lines[i]);
    }
    
    // Fix end retraction - find "; stop printing object" and remove Orca's relative retraction block
    bool found_stop = false;
    for (size_t i = final_lines.size(); i > 0; --i) {
        if (final_lines[i-1].find("; stop printing object") != std::string::npos) {
            found_stop = true;
            // Find last G1 move with E before stop marker
            int last_move_idx = -1;
            for (size_t j = i; j > 0 && j > i - 20; --j) {
                if (final_lines[j-1].find("G1 ") != std::string::npos && 
                    final_lines[j-1].find("E") != std::string::npos &&
                    final_lines[j-1].find(";") == std::string::npos) {
                    last_move_idx = j - 1;
                    break;
                }
            }
            
            if (last_move_idx >= 0) {
                // Extract retraction E value and adjust
                std::string move_line = final_lines[last_move_idx];
                size_t e_pos = move_line.find("E");
                if (e_pos != std::string::npos) {
                    std::string e_str = move_line.substr(e_pos + 1);
                    double e_val = std::atof(e_str.c_str());
                    double retraction_e = e_val - 6.5;
                    
                    // Remove Orca's relative retraction block (lines after stop marker)
                    std::vector<std::string> cleaned;
                    for (size_t j = 0; j < final_lines.size(); ++j) {
                        if (j >= i - 1 && j <= i + 2) {
                            // Skip lines after stop marker that are G92 E0 or G1 E... 
                            if (final_lines[j].find("G92 E0") != std::string::npos || 
                                (final_lines[j].find("G1 ") != std::string::npos && final_lines[j].find("E") != std::string::npos)) {
                                continue;
                            }
                        }
                        cleaned.push_back(final_lines[j]);
                    }
                    final_lines = cleaned;
                    
                    // Find where we inserted retraction and add absolute retraction after stop marker
                    for (size_t j = 0; j < final_lines.size(); ++j) {
                        if (final_lines[j].find("; stop printing object") != std::string::npos) {
                            std::ostringstream retraction;
                            retraction << "G1 F2700 E" << std::fixed << std::setprecision(5) << retraction_e << " ;" << "\n";
                            final_lines.insert(final_lines.begin() + j + 1, retraction.str());
                            break;
                        }
                    }
                }
            }
            break;
        }
    }
    
    // Strip footer comments (lines starting with ;) from the BODY only.
    // The header (everything up to and including END_OF_HEADER plus the two
    // post-header lines generated by the template) must be preserved verbatim.
    // We identify the header/body boundary by counting how many lines came from
    // the generated header string.
    size_t header_line_count = 0;
    {
        std::istringstream hs(header);
        std::string hl;
        while (std::getline(hs, hl)) {
            ++header_line_count;
        }
    }

    std::vector<std::string> truncated;
    size_t line_idx = 0;
    for (const auto& l : final_lines) {
        bool in_header = (line_idx < header_line_count);
        ++line_idx;

        if (in_header) {
            // Never strip header lines — preserve the generated header exactly
            truncated.push_back(l);
            continue;
        }

        // Body: skip comment-only lines (Orca footer metadata comments)
        size_t first_non_ws = l.find_first_not_of(" \t\r\n");
        if (first_non_ws != std::string::npos && l[first_non_ws] == ';') {
            continue;  // Skip footer comment from original G-code body
        }
        truncated.push_back(l);
    }
    
    // Reconstruct final string
    std::ostringstream result;
    for (const auto& l : truncated) {
        result << l;
    }
    
    // Strip leading whitespace (like Python's .lstrip())
    std::string result_str = result.str();
    size_t first_non_ws = result_str.find_first_not_of(" \t\r\n");
    if (first_non_ws != std::string::npos && first_non_ws > 0) {
        result_str = result_str.substr(first_non_ws);
    }
    
    BOOST_LOG_TRIVIAL(info) << "GCodeContainerWriter::build_gcode_content: Result length=" << result_str.length();
    
    return result_str;
}

} // namespace Slic3r
