#ifndef slic3r_UltiMakerLAN_hpp_
#define slic3r_UltiMakerLAN_hpp_

#include "PrintHost.hpp"
#include "libslic3r/PrintConfig.hpp"

namespace Slic3r {

class DynamicPrintConfig;
class Http;

/// UltiMaker LAN output device for local network printing.
/// Based on Cura's UM3NetworkPrinting plugin implementation.
/// 
/// Key characteristics:
/// - No authentication required for local network connections (printer's API is open on LAN)
/// - Uses /cluster-api/v1/ endpoints (with fallback to /api/v1/)
/// - Uploads via multipart form with 'owner' and 'file' fields
/// - Supports auto-discovery via Bonjour/mDNS
class UltiMakerLAN : public PrintHost
{
    std::string m_host;
    std::string m_username;  // Optional: only needed if printer requires auth
    std::string m_password;  // Optional: only needed if printer requires auth
    std::string m_cafile;
    bool m_ssl_revoke_best_effort;

    void set_auth(Http& http) const;
    std::string make_url(const std::string& path) const;

public:
    UltiMakerLAN(DynamicPrintConfig* config);
    ~UltiMakerLAN() override = default;

    const char* get_name() const override { return "UltiMaker LAN"; }
    bool test(wxString& curl_msg) const override;
    wxString get_test_ok_msg() const override;
    wxString get_test_failed_msg(wxString& msg) const override;
    bool upload(PrintHostUpload upload_data, ProgressFn progress_fn, ErrorFn error_fn, InfoFn info_fn) const override;
    bool has_auto_discovery() const override { return true; }
    bool can_test() const override { return true; }
    PrintHostPostUploadActions get_post_upload_actions() const override { return PrintHostPostUploadAction::StartPrint; }
    std::string get_host() const override { return m_host; }
    bool is_cloud() const override { return false; }
};

} // namespace Slic3r

#endif
