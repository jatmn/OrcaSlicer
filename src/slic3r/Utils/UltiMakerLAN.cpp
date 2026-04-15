#include "UltiMakerLAN.hpp"

#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <sstream>
#include <ctime>

#include "Http.hpp"
#include "libslic3r/Utils.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/format.hpp"
#include "slic3r/GUI/GUI_App.hpp"

namespace fs = boost::filesystem;

namespace Slic3r {

namespace {

std::string sanitize_host_input(std::string host)
{
    boost::trim(host);
    if (host.empty()) {
        return host;
    }

    const size_t scheme_pos      = host.find("://");
    const size_t authority_start = scheme_pos == std::string::npos ? 0 : scheme_pos + 3;
    const size_t suffix_pos      = host.find_first_of("/?#", authority_start);
    if (suffix_pos != std::string::npos) {
        host.erase(suffix_pos);
    }

    while (!host.empty() && host.back() == '/') {
        host.pop_back();
    }

    return host;
}

} // namespace

UltiMakerLAN::UltiMakerLAN(DynamicPrintConfig* config)
    : m_host(sanitize_host_input(config->opt_string("print_host")))
    , m_username(config->opt_string("printhost_user"))
    , m_password(config->opt_string("printhost_password"))
    , m_cafile(config->opt_string("printhost_cafile"))
    , m_ssl_revoke_best_effort(config->opt_bool("printhost_ssl_ignore_revoke"))
{
}

void UltiMakerLAN::set_auth(Http& http) const
{
    // UltiMaker LAN typically does not require authentication for local network connections
    // The printer's API is open on the local network when not using cloud features
    // However, if credentials are provided, use HTTP Digest authentication
    if (!m_username.empty() && !m_password.empty()) {
        http.auth_digest(m_username, m_password);
    }
    
    if (!m_cafile.empty()) {
        http.ca_file(m_cafile);
    }
}

std::string UltiMakerLAN::make_url(const std::string& path) const
{
    std::string base = m_host;
    if (!boost::algorithm::starts_with(base, "http://") && !boost::algorithm::starts_with(base, "https://")) {
        // Add http:// scheme if not present (UltiMaker local API uses HTTP)
        base = "http://" + base;
    }

    if (!base.empty() && base.back() == '/' && !path.empty() && path.front() == '/') {
        base.pop_back();
    } else if (!base.empty() && base.back() != '/' && !path.empty() && path.front() != '/') {
        base += '/';
    }

    return base + path;
}

wxString UltiMakerLAN::get_test_ok_msg() const
{
    return _(L("Connection to UltiMaker LAN works correctly."));
}

wxString UltiMakerLAN::get_test_failed_msg(wxString& msg) const
{
    return GUI::format_wxstr("%s: %s", _L("Could not connect to UltiMaker LAN"), msg);
}

bool UltiMakerLAN::test(wxString& curl_msg) const
{
    if (m_host.empty()) {
        curl_msg = _L("Host is empty");
        return false;
    }

    bool res = false;
    // Try the cluster API first (newer printers), fallback to printer API
    auto url = make_url("/cluster-api/v1/system");
    
    BOOST_LOG_TRIVIAL(info) << boost::format("UltiMaker LAN: Testing connection at: %1%") % url;
    
    auto http = Http::get(std::move(url));
    set_auth(http);
    http.on_error([&](std::string body, std::string error, unsigned status) {
        BOOST_LOG_TRIVIAL(warning) << boost::format("UltiMaker LAN: Error testing cluster-api: %1%, HTTP %2%") % error % status;
        // Try fallback to regular printer API
        auto fallback_url = make_url("/api/v1/system");
        BOOST_LOG_TRIVIAL(info) << boost::format("UltiMaker LAN: Trying fallback at: %1%") % fallback_url;
        
        auto http2 = Http::get(std::move(fallback_url));
        set_auth(http2);
        http2.on_error([&](std::string body2, std::string error2, unsigned status2) {
            BOOST_LOG_TRIVIAL(error) << boost::format("UltiMaker LAN: Error testing fallback: %1%, HTTP %2%, body: `%3%`") % error2 % status2 % body2;
            curl_msg = format_error(body2, error2, status2);
            res = false;
        })
        .on_complete([&](std::string body2, unsigned status2) {
            BOOST_LOG_TRIVIAL(info) << boost::format("UltiMaker LAN: Fallback test response: HTTP %1%, body: %2%") % status2 % body2;
            res = (status2 >= 200 && status2 < 300);
        })
        .ssl_revoke_best_effort(m_ssl_revoke_best_effort)
        .perform_sync();
    })
    .on_complete([&](std::string body, unsigned status) {
        BOOST_LOG_TRIVIAL(info) << boost::format("UltiMaker LAN: Test response: HTTP %1%, body: %2%") % status % body;
        res = (status >= 200 && status < 300);
    })
    .ssl_revoke_best_effort(m_ssl_revoke_best_effort)
    .perform_sync();
    
    return res;
}

bool UltiMakerLAN::upload(PrintHostUpload upload_data, ProgressFn progress_fn, ErrorFn error_fn, InfoFn info_fn) const
{
    if (m_host.empty()) {
        error_fn(_L("Host is empty"));
        return false;
    }

    // Get filename for upload
    const std::string filename = upload_data.upload_path.filename().string();
    const fs::path filepath = upload_data.source_path;
    
    BOOST_LOG_TRIVIAL(info) << boost::format("UltiMaker LAN: Uploading file %1% as %2%") % filepath % filename;
    
    // Verify file exists before uploading
    if (!fs::exists(filepath)) {
        BOOST_LOG_TRIVIAL(error) << "UltiMaker LAN: File does not exist: " << filepath;
        error_fn(_L("Upload failed: File not found"));
        return false;
    }
    
    // UltiMaker printers ONLY accept .ufp files - validate extension
    std::string extension = filepath.extension().string();
    boost::algorithm::to_lower(extension);
    if (extension != ".ufp") {
        BOOST_LOG_TRIVIAL(error) << "UltiMaker LAN: Invalid file type: " << extension << ", only .ufp is supported";
        error_fn(_L("UltiMaker printers only accept .ufp files. Please enable 'Export as UFP' in your printer profile."));
        return false;
    }
    
    // UltiMaker LAN uses /cluster-api/v1/print_jobs/ for upload (based on Cura implementation)
    auto url = make_url("/cluster-api/v1/print_jobs/");
    
    bool res = true;
    auto http = Http::post(std::move(url));
    set_auth(http);
    
    BOOST_LOG_TRIVIAL(info) << "UltiMaker LAN: File path: " << filepath;
    BOOST_LOG_TRIVIAL(info) << "UltiMaker LAN: Filename: " << filename;
    
    // Read file into memory and construct multipart/form-data manually
    // This avoids issues with curl's form API and Unicode paths on Windows
    std::string file_data;
    {
        fs::ifstream ifs(filepath, std::ios::in | std::ios::binary);
        if (!ifs.is_open()) {
            BOOST_LOG_TRIVIAL(error) << "UltiMaker LAN: Cannot open file for reading: " << filepath;
            error_fn(_L("Upload failed: Cannot open file for reading"));
            return false;
        }
        file_data.assign((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        BOOST_LOG_TRIVIAL(info) << "UltiMaker LAN: Read " << file_data.size() << " bytes from file";
    }
    
    // Generate a unique boundary string
    std::string boundary = "----OrcaSlicerBoundary" + std::to_string(std::time(nullptr));
    
    // Construct multipart/form-data body
    std::ostringstream body;
    
    // Add owner field
    body << "--" << boundary << "\r\n";
    body << "Content-Disposition: form-data; name=\"owner\"\r\n";
    body << "Content-Type: text/plain\r\n\r\n";
    body << "OrcaSlicer" << "\r\n";
    
    // Add file field
    body << "--" << boundary << "\r\n";
    body << "Content-Disposition: form-data; name=\"file\"; filename=\"" << filename << "\"\r\n";
    body << "Content-Type: application/x-ufp\r\n\r\n";
    body.write(file_data.data(), file_data.size());
    body << "\r\n";
    
    // End boundary
    body << "--" << boundary << "--\r\n";
    
    std::string body_str = body.str();
    BOOST_LOG_TRIVIAL(info) << "UltiMaker LAN: Constructed multipart body, total size: " << body_str.size();
    
    // Set up the HTTP request with raw body
    http.set_post_body(body_str)
        .header("Content-Type", "multipart/form-data; boundary=" + boundary)
        .on_progress([&](Http::Progress progress, bool& cancel) {
            progress_fn(std::move(progress), cancel);
            if (cancel) {
                BOOST_LOG_TRIVIAL(info) << "UltiMaker LAN: Upload canceled";
                res = false;
            }
        })
        .on_complete([&](std::string body, unsigned status) {
            BOOST_LOG_TRIVIAL(info) << boost::format("UltiMaker LAN: Upload response: HTTP %1%: %2%") % status % body;
            if (status >= 200 && status < 300) {
                info_fn("UltiMaker LAN", _L("File uploaded successfully"));
            } else {
                res = false;
                error_fn(format_error(body, "Upload failed", status));
            }
        })
        .on_error([&](std::string body, std::string error, unsigned status) {
            BOOST_LOG_TRIVIAL(error) << boost::format("UltiMaker LAN: Error uploading file: %1%, HTTP %2%, body: `%3%`") % error % status % body;
            error_fn(format_error(body, error, status));
            res = false;
        })
        .ssl_revoke_best_effort(m_ssl_revoke_best_effort)
        .perform_sync();
    
    return res;
}

} // namespace Slic3r
