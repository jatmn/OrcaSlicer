#ifndef slic3r_UltiMaker_hpp_
#define slic3r_UltiMaker_hpp_

#include "PrintHost.hpp"
#include "slic3r/GUI/Jobs/OAuthJob.hpp"
#include <chrono>

namespace Slic3r {

class DynamicPrintConfig;
class Http;

class UltiMaker : public PrintHost
{
    std::string m_host{"https://api.ultimaker.com"};
    std::string m_client_id{"um----------------------------ultimaker_cura"};
    std::string m_oauth_cred_file;
    mutable std::map<std::string, std::string> m_cred;
    
    // Token expiration tracking
    mutable std::chrono::system_clock::time_point m_token_expires_at{};
    
    // Time before expiry to trigger proactive refresh (15 minutes)
    static constexpr std::chrono::seconds TOKEN_REFRESH_SKEW{900};

    void load_oauth_credential();
    bool refresh_token() const;
    bool ensure_token_fresh(const std::string& reason) const;

    bool do_api_call(std::function<Http(bool /*is_retry*/)>                                                           build_request,
                     std::function<bool(std::string /* body */, unsigned /* http_status */)>                          on_complete,
                     std::function<bool(std::string /* body */, std::string /* error */, unsigned /* http_status */)> on_error) const;

public:
    UltiMaker(DynamicPrintConfig* config);
    ~UltiMaker() override = default;

    const char* get_name() const override { return "UltiMaker"; }
    bool can_test() const override { return true; }
    bool has_auto_discovery() const override { return false; }
    bool is_cloud() const override { return true; }
    std::string get_host() const override { return m_host; }

    GUI::OAuthParams get_oauth_params() const;
    void             save_oauth_credential(const GUI::OAuthResult& cred) const;

    wxString                   get_test_ok_msg() const override;
    wxString                   get_test_failed_msg(wxString& msg) const override;
    bool                       test(wxString& curl_msg) const override;
    PrintHostPostUploadActions get_post_upload_actions() const override { return PrintHostPostUploadAction::None; }
    bool                       upload(PrintHostUpload upload_data, ProgressFn prorgess_fn, ErrorFn error_fn, InfoFn info_fn) const override;
    bool                       is_logged_in() const override { return !m_cred.empty(); }
    void                       log_out() const override;

    // Digital Factory project/folder methods
    struct ProjectInfo {
        std::string id;
        std::string display_name;
        std::string owner;
    };

    // Response from upload URL request (Step 1 of two-step upload)
    struct UploadResponse {
        std::string upload_url;
        std::string content_type;
        std::string job_id;
        bool success = false;
        std::string error_message;
    };

    bool get_projects(std::vector<ProjectInfo>& projects) const;
    bool get_projects(wxArrayString& project_names, wxArrayString& project_ids) const override;
    CreateProjectResult create_project(const std::string& name) const override;
};

} // namespace Slic3r

#endif