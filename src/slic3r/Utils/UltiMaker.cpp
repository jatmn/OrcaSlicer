#include "UltiMaker.hpp"

#include <openssl/sha.h>
#include <openssl/rand.h>
#include <boost/beast/core/detail/base64.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio.hpp>
#include <thread>
#include <chrono>

#include "slic3r/Utils/Http.hpp"
#include "nlohmann/json.hpp"
#include "libslic3r/Utils.hpp"
#include "libslic3r/Format/FormatConfig.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/format.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/MainFrame.hpp"


namespace Slic3r {

#define URL_ACCOUNT "https://account.ultimaker.com"
#define URL_API "https://api.ultimaker.com"

// Default port - use a fixed port for consistency
static constexpr boost::asio::ip::port_type DEFAULT_CALLBACK_PORT = 32118;

static std::string get_callback_url()
{
    // Use a fixed port for the callback URL to ensure consistency between
    // authorization request and token exchange. The HTTP server will bind to this port.
    return "http://localhost:" + std::to_string(DEFAULT_CALLBACK_PORT) + "/callback";
}
static const std::string RESPONSE_TYPE = "code";
static const std::string CLIENT_ID = "um----------------------------ultimaker_cura";
static const std::string CLIENT_SECRET = "";
static const std::string SCOPES = "account.user.read drive.backup.read drive.backup.write packages.download packages.rating.read packages.rating.write connect.cluster.read connect.cluster.write connect.material.write library.project.read library.project.write cura.printjob.read cura.printjob.write cura.mesh.read cura.mesh.write";
static const std::string OAUTH_CREDENTIAL_PATH = "ultimaker_oauth.json";
static const std::string TOKEN_URL = URL_ACCOUNT "/token";
static const std::string LIBRARY_API_BASE = URL_API "/cura/v1";

static std::string generate_verification_code()
{
    constexpr int PKCE_VERIFIER_BYTES = 32;
    unsigned char random_bytes[PKCE_VERIFIER_BYTES];
    
    // Use cryptographically secure random generation from OpenSSL
    if (RAND_bytes(random_bytes, PKCE_VERIFIER_BYTES) != 1) {
        BOOST_LOG_TRIVIAL(error) << "UltiMaker OAuth: Failed to generate secure random bytes";
        // Fallback to simple generation if secure generation fails (should rarely happen)
        for (int i = 0; i < PKCE_VERIFIER_BYTES; i++) {
            random_bytes[i] = static_cast<unsigned char>(rand() % 0x100);
        }
    }
    
    std::string hex_str;
    hex_str.reserve(PKCE_VERIFIER_BYTES * 2);
    for (int i = 0; i < PKCE_VERIFIER_BYTES; i++) {
        static const char hex_chars[] = "0123456789abcdef";
        unsigned char byte = random_bytes[i];
        hex_str += hex_chars[(byte >> 4) & 0x0F];
        hex_str += hex_chars[byte & 0x0F];
    }
    return hex_str;
}

static std::string sha512_base64url(const std::string& inputStr)
{
    unsigned char hash[SHA512_DIGEST_LENGTH];
    SHA512(reinterpret_cast<const unsigned char*>(inputStr.data()), inputStr.size(), hash);
    std::string b64;
    b64.resize(boost::beast::detail::base64::encoded_size(SHA512_DIGEST_LENGTH));
    b64.resize(boost::beast::detail::base64::encode(&b64[0], hash, SHA512_DIGEST_LENGTH));
    // Match Cura's altchars = b"_-": b64encode(encoded, altchars = b"_-")
    // altchars[0] replaces '+', altchars[1] replaces '/'
    // So: '+' -> '_', '/' -> '-'
    std::replace(b64.begin(), b64.end(), '+', '_');
    std::replace(b64.begin(), b64.end(), '/', '-');
    // Keep the padding (=) - UltiMaker's server expects it unlike RFC strict base64url
    return b64;
}

static std::string url_encode(const std::vector<std::pair<std::string, std::string>> query)
{
    std::vector<std::string> q;
    q.reserve(query.size());

    std::transform(query.begin(), query.end(), std::back_inserter(q), [](const auto& kv) {
        if (kv.second.empty()) {
            return Http::url_encode(kv.first);
        }
        return Http::url_encode(kv.first) + "=" + Http::url_encode(kv.second);
    });

    return boost::algorithm::join(q, "&");
}

static void set_auth(Http& http, const std::string& access_token)
{
    http.header("Authorization", "Bearer " + access_token);
}

UltiMaker::UltiMaker(DynamicPrintConfig* config)
{
    m_oauth_cred_file = (boost::filesystem::path(data_dir()) / OAUTH_CREDENTIAL_PATH).make_preferred().string();
    load_oauth_credential();
}

GUI::OAuthParams UltiMaker::get_oauth_params() const
{
    const auto verification_code = generate_verification_code();
    const auto code_challenge = sha512_base64url(verification_code);
    const auto state = generate_verification_code();
    const auto callback_url = get_callback_url();
    const auto callback_port = static_cast<boost::asio::ip::port_type>(std::stoi(callback_url.substr(callback_url.rfind(':') + 1, callback_url.rfind('/') - callback_url.rfind(':') - 1)));

    BOOST_LOG_TRIVIAL(info) << "UltiMaker OAuth: Starting authorization with PKCE S512";
    BOOST_LOG_TRIVIAL(info) << "UltiMaker OAuth: verification_code length = " << verification_code.length();

    const std::vector<std::pair<std::string, std::string>> query_parameters{
        {"client_id", CLIENT_ID},
        {"redirect_uri", callback_url},
        {"scope", SCOPES},
        {"response_type", RESPONSE_TYPE},
        {"state", state},
        {"code_challenge", code_challenge},
        {"code_challenge_method", "S512"},
    };
    const auto login_url = (boost::format(URL_ACCOUNT "/authorize?%s") % url_encode(query_parameters)).str();

    BOOST_LOG_TRIVIAL(warning) << "===== UltiMaker OAuth: Dynamic callback URL: " << callback_url;
    BOOST_LOG_TRIVIAL(warning) << "===== UltiMaker OAuth: Generated login URL (truncated): " << login_url.substr(0, 150) << "...";

    // Use a separate success URL to avoid the browser re-requesting the callback URL
    // which would cause the code to be missing and trigger an error
    const std::string success_url = "http://localhost:" + std::to_string(DEFAULT_CALLBACK_PORT) + "/success";
    const std::string fail_url = "http://localhost:" + std::to_string(DEFAULT_CALLBACK_PORT) + "/fail";

    return GUI::OAuthParams{
        login_url,
        CLIENT_ID,
        CLIENT_SECRET,
        callback_port,
        callback_url,
        SCOPES,
        RESPONSE_TYPE,
        success_url,
        fail_url,
        TOKEN_URL,
        verification_code,
        state,
    };
}

void UltiMaker::load_oauth_credential()
{
    m_cred.clear();
    m_token_expires_at = std::chrono::system_clock::time_point{};
    BOOST_LOG_TRIVIAL(error) << "UltiMaker: Loading OAuth credentials from " << m_oauth_cred_file;
    if (boost::filesystem::exists(m_oauth_cred_file)) {
        nlohmann::json j;
        try {
            boost::nowide::ifstream ifs(m_oauth_cred_file);
            ifs >> j;
            ifs.close();

            m_cred["access_token"] = j["access_token"];
            m_cred["refresh_token"] = j["refresh_token"];
            
            // Load token expiration time if available
            if (j.contains("expires_at")) {
                int64_t expires_at_epoch = j["expires_at"];
                m_token_expires_at = std::chrono::system_clock::from_time_t(expires_at_epoch);
                auto now = std::chrono::system_clock::now();
                auto remaining = std::chrono::duration_cast<std::chrono::seconds>(m_token_expires_at - now).count();
                BOOST_LOG_TRIVIAL(info) << "UltiMaker: Token expires in " << remaining << " seconds";
            }
        } catch (std::exception& err) {
            BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << ": parse " << m_oauth_cred_file << " failed, reason = " << err.what();
            m_cred.clear();
        }
    }
}

void UltiMaker::save_oauth_credential(const GUI::OAuthResult& cred) const
{
    BOOST_LOG_TRIVIAL(error) << "UltiMaker: Saving OAuth credentials to " << m_oauth_cred_file;
    nlohmann::json j;
    j["access_token"] = cred.access_token;
    j["refresh_token"] = cred.refresh_token;
    
    // Store token expiration time
    if (cred.expires_in > 0) {
        auto expires_at = std::chrono::system_clock::now() + std::chrono::seconds(cred.expires_in);
        j["expires_at"] = std::chrono::system_clock::to_time_t(expires_at);
        BOOST_LOG_TRIVIAL(info) << "UltiMaker: Storing token expiration: " << cred.expires_in << " seconds";
    }

    boost::nowide::ofstream c;
    c.open(m_oauth_cred_file, std::ios::out | std::ios::trunc);
    c << std::setw(4) << j << std::endl;
    c.close();
    BOOST_LOG_TRIVIAL(error) << "UltiMaker: OAuth credentials saved successfully";
}

bool UltiMaker::refresh_token() const
{
    if (m_cred.find("refresh_token") == m_cred.end()) {
        BOOST_LOG_TRIVIAL(warning) << "UltiMaker: No refresh token available";
        return false;
    }

    BOOST_LOG_TRIVIAL(info) << "UltiMaker: Refreshing access token";

    // Build URL-encoded form body for refresh token request
    std::string post_body;
    post_body += "client_id=" + Http::url_encode(CLIENT_ID);
    post_body += "&client_secret=" + Http::url_encode(CLIENT_SECRET);
    post_body += "&redirect_uri=" + get_callback_url();
    post_body += "&grant_type=" + Http::url_encode("refresh_token");
    post_body += "&refresh_token=" + Http::url_encode(m_cred.at("refresh_token"));
    post_body += "&scope=" + Http::url_encode(SCOPES);

    bool success = false;
    
    auto http = Http::post(TOKEN_URL);
    http.timeout_connect(5)
        .timeout_max(10)
        .header("Content-Type", "application/x-www-form-urlencoded")
        .set_post_body(post_body)
        .on_complete([this, &success](std::string body, unsigned http_status) {
            GUI::OAuthResult r;
            GUI::OAuthJob::parse_token_response(body, false, r);
            if (r.success) {
                BOOST_LOG_TRIVIAL(info) << "UltiMaker: Successfully refreshed access token";
                this->save_oauth_credential(r);
                
                // Update in-memory credentials
                this->m_cred["access_token"] = r.access_token;
                this->m_cred["refresh_token"] = r.refresh_token;
                
                // Update expiration time
                if (r.expires_in > 0) {
                    this->m_token_expires_at = std::chrono::system_clock::now() + std::chrono::seconds(r.expires_in);
                }
                success = true;
            } else {
                BOOST_LOG_TRIVIAL(error) << "UltiMaker: Failed to parse refresh token response: " << r.error_message;
            }
        })
        .on_error([&success](std::string body, std::string error, unsigned http_status) {
            BOOST_LOG_TRIVIAL(error) << "UltiMaker: Failed to refresh token: " << error << ", HTTP " << http_status;
            success = false;
        })
        .perform_sync();

    return success;
}

bool UltiMaker::ensure_token_fresh(const std::string& reason) const
{
    // If no expiration time stored, assume token is fresh
    if (m_token_expires_at.time_since_epoch().count() == 0) {
        return true;
    }

    auto now = std::chrono::system_clock::now();
    auto time_until_expiry = m_token_expires_at - now;
    
    BOOST_LOG_TRIVIAL(info) << "UltiMaker: Token expires in " << std::chrono::duration_cast<std::chrono::seconds>(time_until_expiry).count() 
                            << " seconds (check reason: " << reason << ")";

    // Refresh if token expires within TOKEN_REFRESH_SKEW seconds
    if (time_until_expiry <= TOKEN_REFRESH_SKEW) {
        BOOST_LOG_TRIVIAL(info) << "UltiMaker: Token expiring soon, proactively refreshing (reason: " << reason << ")";
        return const_cast<UltiMaker*>(this)->refresh_token();
    }

    return true;
}

wxString UltiMaker::get_test_ok_msg() const
{
    return _(L("Connected to UltiMaker successfully!"));
}

wxString UltiMaker::get_test_failed_msg(wxString& msg) const
{
    return GUI::format_wxstr("%s: %s", _L("Could not connect to UltiMaker"), msg.Truncate(256));
}

void UltiMaker::log_out() const
{
    boost::nowide::remove(m_oauth_cred_file.c_str());
}

namespace {
// Helper function to check if response indicates token expiration
bool is_token_expired_error(const std::string& body, unsigned http_status)
{
    // Check for HTTP 401 (Unauthorized)
    if (http_status == 401) {
        return true;
    }
    
    // Check for HTTP 403 with tokenExpired in the response body
    // Response format: {"errors":[{"id":"...","code":"tokenExpired","http_status":"403",...}]}
    if (http_status == 403 || http_status == 401) {
        // Check if body contains "tokenExpired" string
        if (body.find("tokenExpired") != std::string::npos) {
            return true;
        }
    }
    
    return false;
}
} // namespace

bool UltiMaker::do_api_call(std::function<Http(bool)>                               build_request,
                            std::function<bool(std::string, unsigned)>              on_complete,
                            std::function<bool(std::string, std::string, unsigned)> on_error) const
{
    if (m_cred.find("access_token") == m_cred.end()) {
        return false;
    }

    // Proactively check if token needs refresh before making the call
    if (!ensure_token_fresh("do_api_call")) {
        BOOST_LOG_TRIVIAL(warning) << "UltiMaker: Token refresh failed, proceeding with existing token";
    }

    bool res = true;

    const auto create_request = [this, &build_request, &res, &on_complete](const std::string& access_token, bool is_retry) {
        auto http = build_request(is_retry);
        set_auth(http, access_token);
        http.header("User-Agent", "UltiMaker OrcaSlicer Plugin")
            .on_complete([&](std::string body, unsigned http_status) {
                res = on_complete(body, http_status);
            });

        return http;
    };

    create_request(m_cred.at("access_token"), false)
        .on_error([&res, &on_error, this, &create_request](std::string body, std::string error, unsigned http_status) {
            // Check if we need to refresh the token - handles both HTTP 401 and HTTP 403 with tokenExpired
            if (is_token_expired_error(body, http_status)) {
                BOOST_LOG_TRIVIAL(warning) << boost::format("UltiMaker: Access token expired or invalid: %1%, HTTP %2%, body: `%3%`") % error %
                                                  http_status % body;
                BOOST_LOG_TRIVIAL(info) << "UltiMaker: Attempting to refresh access token";

                // Try to refresh the token
                if (this->refresh_token()) {
                    // Retry the request with new token
                    create_request(this->m_cred.at("access_token"), true)
                        .on_error([&res, &on_error](std::string body, std::string error, unsigned http_status) {
                            res = on_error(body, error, http_status);
                        })
                        .perform_sync();
                } else {
                    // Refresh failed, call original error handler
                    res = on_error(body, error, http_status);
                }
            } else {
                res = on_error(body, error, http_status);
            }
        })
        .perform_sync();

    return res;
}

bool UltiMaker::test(wxString& curl_msg) const
{
    if (m_cred.find("access_token") == m_cred.end()) {
        return false;
    }

    return do_api_call(
        [](bool is_retry) {
            auto http = Http::get("https://api.ultimaker.com/connect/v1/clusters");
            http.header("Accept", "application/json");
            return http;
        },
        [](std::string body, unsigned) {
            BOOST_LOG_TRIVIAL(info) << boost::format("UltiMaker: Got clusters info: %1%") % body;
            return true;
        },
        [](std::string body, std::string error, unsigned status) {
            BOOST_LOG_TRIVIAL(error) << boost::format("UltiMaker: Error getting clusters info: %1%, HTTP %2%, body: `%3%`") % error %
                                            status % body;
            return false;
        });
}

bool UltiMaker::upload(PrintHostUpload upload_data, ProgressFn prorgess_fn, ErrorFn error_fn, InfoFn info_fn) const
{
    if (m_cred.find("access_token") == m_cred.end()) {
        error_fn(_L("UltiMaker account not linked. Go to Connect options to set it up."));
        return false;
    }

    // Get printer notes from extended_info to check for FORMAT_CONFIG_ID
    std::string printer_notes;
    auto notes_it = upload_data.extended_info.find("printer_notes");
    if (notes_it != upload_data.extended_info.end()) {
        printer_notes = notes_it->second;
    }

    // Check if printer requires container format (.ufp or .makerbot)
    std::string format_type = FormatConfig::get_format_type_for_printer(printer_notes);
    std::string container_path;
    
    if (!format_type.empty()) {
        // Printer requires container format - create container from G-code
        BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: Printer requires container format: " << format_type;
        
        // Create temp file for container
        fs::path temp_dir = boost::filesystem::temp_directory_path();
        std::string extension = FormatConfig::get_file_extension_for_format(format_type);
        std::string base_filename = upload_data.upload_path.filename().stem().string();
        container_path = (temp_dir / (base_filename + "_upload" + extension)).string();
        
        // Convert G-code to container format
        std::string error_message;
        if (!FormatConfig::export_to_container(format_type, 
                                               upload_data.source_path.string(), 
                                               container_path, 
                                               printer_notes, 
                                               error_message)) {
            // Hard stop - conversion failed, no fallback to raw G-code
            BOOST_LOG_TRIVIAL(error) << "UltiMaker::upload: Container conversion failed: " << error_message;
            error_fn(_L("Failed to create container file for upload: ") + error_message);
            return false;
        }
        
        BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: Container created at: " << container_path;
        
        // Update source_path to upload the container instead of raw G-code
        upload_data.source_path = container_path;
        // Update filename to reflect container extension
        upload_data.upload_path = upload_data.upload_path.parent_path() / 
                                  (base_filename + extension);
    }

    const auto filename = upload_data.upload_path.filename().string();

    // Check if a project folder is specified in extended_info
    std::string project_id;
    auto it = upload_data.extended_info.find("project_id");
    if (it != upload_data.extended_info.end()) {
        project_id = it->second;
    }

    // Capture container_path for cleanup scheduling
    std::string final_container_path = container_path;
    
    bool upload_success = do_api_call(
        [&upload_data, &filename, &prorgess_fn, &project_id](bool is_retry) {
            std::string url = LIBRARY_API_BASE + "/files";
            // If project_id is specified, upload to that project
            if (!project_id.empty()) {
                url = LIBRARY_API_BASE + "/projects/" + project_id + "/files";
            }
            auto http = Http::post(url);
            http.form_add("name", filename)
                .form_add_file("file", upload_data.source_path.string(), filename)
                .on_progress([&prorgess_fn](Http::Progress progress, bool& cancel) { prorgess_fn(std::move(progress), cancel); });
            return http;
        },
        [&info_fn, &filename, final_container_path](std::string body, unsigned status) {
            BOOST_LOG_TRIVIAL(info) << boost::format("UltiMaker: File uploaded: HTTP %1%: %2%") % status % body;
            info_fn("UltiMaker", _L("File uploaded successfully"));
            
            // Schedule cleanup of temp container file 1 minute after successful upload
            if (!final_container_path.empty()) {
                std::thread cleanup_thread([final_container_path]() {
                    std::this_thread::sleep_for(std::chrono::seconds(60));
                    BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: Cleaning up temp container file: " << final_container_path;
                    try {
                        boost::filesystem::remove(final_container_path);
                    } catch (const std::exception& e) {
                        BOOST_LOG_TRIVIAL(error) << "UltiMaker::upload: Failed to remove temp file: " << e.what();
                    }
                });
                cleanup_thread.detach();
            }
            return true;
        },
        [this, &error_fn](std::string body, std::string error, unsigned status) {
            BOOST_LOG_TRIVIAL(error) << boost::format("UltiMaker: Error uploading file: %1%, HTTP %2%, body: `%3%") % error % status % body;
            error_fn(format_error(body, error, status));
            return false;
        });
    
    return upload_success;
}

bool UltiMaker::get_projects(std::vector<ProjectInfo>& projects) const
{
    BOOST_LOG_TRIVIAL(error) << "UM_DEBUG: get_projects() called - checking for access token";
    
    if (m_cred.find("access_token") == m_cred.end()) {
        BOOST_LOG_TRIVIAL(error) << "UM_DEBUG: No access token found in m_cred for get_projects";
        // List all keys in m_cred for debugging
        std::string cred_keys;
        for (const auto& kv : m_cred) {
            cred_keys += kv.first + ", ";
        }
        BOOST_LOG_TRIVIAL(error) << "UM_DEBUG: Available cred keys: " << cred_keys;
        return false;
    }

    BOOST_LOG_TRIVIAL(error) << "UM_DEBUG: Access token found, making API call to get projects";
    
    bool result = false;
    projects.clear();

    return do_api_call(
        [](bool is_retry) {
            auto http = Http::get(LIBRARY_API_BASE + "/projects?shared=false&limit=100");
            http.header("Accept", "application/json");
            BOOST_LOG_TRIVIAL(error) << "UM_DEBUG: GET " << LIBRARY_API_BASE << "/projects?shared=false&limit=100";
            return http;
        },
        [&result, &projects](std::string body, unsigned http_status) {
            BOOST_LOG_TRIVIAL(error) << "UM_DEBUG: get_projects response HTTP " << http_status << ", body length: " << body.size();
            
            // Log response body for debugging
            std::string body_log = body.size() > 1000 ? body.substr(0, 1000) : body;
            BOOST_LOG_TRIVIAL(error) << "UM_DEBUG: Response body: " << body_log;
            
            try {
                auto j = nlohmann::json::parse(body);
                
                if (j.contains("data") && j["data"].is_array()) {
                    int count = 0;
                    for (const auto& proj : j["data"]) {
                        ProjectInfo info;
                        // Use library_project_id as the id - the API returns this field, not "id"
                        if (proj.contains("library_project_id")) {
                            info.id = proj["library_project_id"];
                        }
                        if (proj.contains("display_name")) {
                            info.display_name = proj["display_name"];
                        }
                        // Owner can be in "username" field directly, or in "owner" object
                        if (proj.contains("username")) {
                            info.owner = proj["username"];
                        } else if (proj.contains("owner")) {
                            if (proj["owner"].is_object() && proj["owner"].contains("username")) {
                                info.owner = proj["owner"]["username"];
                            }
                        }
                        BOOST_LOG_TRIVIAL(error) << "UM_DEBUG: Project #" << count << " - id: " << info.id << ", name: " << info.display_name << ", owner: " << info.owner;
                        if (!info.id.empty() && !info.display_name.empty()) {
                            projects.push_back(info);
                            count++;
                        }
                    }
                    
                    // Sort projects alphabetically by display_name (case-insensitive)
                    std::sort(projects.begin(), projects.end(), [](const ProjectInfo& a, const ProjectInfo& b) {
                        return a.display_name < b.display_name;
                    });
                    
                    result = true;
                    BOOST_LOG_TRIVIAL(error) << "UM_DEBUG: Total projects added: " << projects.size() << " out of " << j["data"].size() << " items in response";
                } else {
                    BOOST_LOG_TRIVIAL(error) << "UM_DEBUG: Projects response missing 'data' array or not an array";
                    // Log what keys ARE present
                    std::string keys;
                    for (auto& el : j.items()) {
                        keys += el.key() + ", ";
                    }
                    BOOST_LOG_TRIVIAL(error) << "UM_DEBUG: Available keys in response: " << keys;
                }
            } catch (const std::exception& e) {
                BOOST_LOG_TRIVIAL(error) << "UM_DEBUG: Failed to parse projects response: " << e.what();
            }
            return result;
        },
        [&result](std::string body, std::string error, unsigned http_status) {
            BOOST_LOG_TRIVIAL(error) << "UM_DEBUG: Error getting projects: " << error << ", HTTP " << http_status << ", body: " << body;
            result = false;
            return false;
        });
}

bool UltiMaker::get_projects(wxArrayString& project_names, wxArrayString& project_ids) const
{
    std::vector<ProjectInfo> projects;
    bool result = get_projects(projects);
    if (result) {
        for (const auto& proj : projects) {
            project_names.Add(wxString::FromUTF8(proj.display_name.c_str()));
            project_ids.Add(wxString::FromUTF8(proj.id.c_str()));
        }
    }
    return result;
}

PrintHost::CreateProjectResult UltiMaker::create_project(const std::string& name) const
{
    BOOST_LOG_TRIVIAL(error) << "UM_DEBUG: create_project() called with name: " << name;
    
    if (m_cred.find("access_token") == m_cred.end()) {
        BOOST_LOG_TRIVIAL(error) << "UM_DEBUG: create_project() - no access token";
        PrintHost::CreateProjectResult result;
        result.success = false;
        result.error_code = "no_access_token";
        result.error_title = "No access token available";
        return result;
    }

    PrintHost::CreateProjectResult result;

    // Use a lambda that calls do_api_call and captures the bool result
    bool api_call_result = do_api_call(
        [&name](bool is_retry) {
            auto http = Http::put2(LIBRARY_API_BASE + "/projects");
            http.header("Accept", "application/json");
            http.header("Content-Type", "application/json");
            
            // Wrap in "data" object like Cura does
            nlohmann::json body;
            body["data"]["display_name"] = name;
            http.set_post_body(body.dump());
            BOOST_LOG_TRIVIAL(error) << "UM_DEBUG: create_project request body: " << body.dump();
            return http;
        },
        [&result](std::string body, unsigned http_status) {
            BOOST_LOG_TRIVIAL(error) << "UM_DEBUG: create_project response HTTP " << http_status << ", body length: " << body.size();
            try {
                auto j = nlohmann::json::parse(body);
                if (j.contains("data")) {
                    const auto& proj = j["data"];
                    // Use library_project_id as the id
                    if (proj.contains("library_project_id")) {
                        result.project_id = proj["library_project_id"];
                    } else if (proj.contains("id")) {
                        result.project_id = proj["id"];
                    }
                    if (proj.contains("display_name")) {
                        result.project_name = proj["display_name"];
                    }
                    result.success = true;
                    BOOST_LOG_TRIVIAL(error) << "UM_DEBUG: Project created successfully - id: " << result.project_id << ", name: " << result.project_name;
                } else {
                    BOOST_LOG_TRIVIAL(error) << "UM_DEBUG: create_project response missing 'data' field";
                }
            } catch (const std::exception& e) {
                BOOST_LOG_TRIVIAL(error) << "UltiMaker: Failed to parse create project response: " << e.what();
            }
            return result.success;
        },
        [&result](std::string body, std::string error, unsigned http_status) {
            BOOST_LOG_TRIVIAL(error) << "UltiMaker: Error creating project: " << error << ", HTTP " << http_status << ", body: " << body;
            
            // Try to parse error JSON to extract error code and title
            try {
                auto j = nlohmann::json::parse(body);
                if (j.contains("errors") && j["errors"].is_array() && !j["errors"].empty()) {
                    const auto& err = j["errors"][0];
                    if (err.contains("code")) {
                        result.error_code = err["code"];
                    }
                    if (err.contains("title")) {
                        result.error_title = err["title"];
                    }
                    BOOST_LOG_TRIVIAL(error) << "UM_DEBUG: Parsed error code: " << result.error_code << ", title: " << result.error_title;
                }
            } catch (const std::exception& e) {
                BOOST_LOG_TRIVIAL(error) << "UltiMaker: Failed to parse error response: " << e.what();
            }
            
            result.success = false;
            return false;
        });
    
    (void)api_call_result;  // Suppress unused variable warning - result.success is already set by callbacks
    return result;
}

} // namespace Slic3r