#ifndef slic3r_MakerBotWriter_hpp_
#define slic3r_MakerBotWriter_hpp_

#include "GCodeContainerWriter.hpp"

namespace Slic3r {

class MakerBotWriter : public GCodeContainerWriter {
public:
    MakerBotWriter(const PrinterFormatConfig& config) : GCodeContainerWriter(config) {}
    
protected:
    std::string generate_header(const GCodeMetadata& meta) override;
    bool write_container(const GCodeMetadata& meta, const std::string& gcode_content, const std::string& output_path) override;
    
private:
    std::string generate_meta_json(const GCodeMetadata& meta);
    std::string generate_slicemetadata_json(const GCodeMetadata& meta);
};

} // namespace Slic3r

#endif
