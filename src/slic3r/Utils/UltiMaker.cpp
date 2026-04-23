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
#include "slic3r/Utils/SecureStorage.hpp"
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

static bool write_oauth_metadata_file(const std::string& path, const nlohmann::json& metadata, const char* context)
{
    try {
        boost::nowide::ofstream ofs(path, std::ios::out | std::ios::trunc);
        ofs << std::setw(4) << metadata << std::endl;
        ofs.close();
        return true;
    } catch (const std::exception& err) {
        BOOST_LOG_TRIVIAL(error) << "UltiMaker: Failed to write OAuth metadata (" << context << "): " << err.what();
        return false;
    }
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
    
    BOOST_LOG_TRIVIAL(info) << "UltiMaker: Loading OAuth credentials";
    
    // Step 1: Try to load refresh token from secure keyring first
    if (SecureStorage::is_available()) {
        auto refresh_token = SecureStorage::retrieve(KEYRING_ACCOUNT);
        if (refresh_token.has_value()) {
            m_cred["refresh_token"] = refresh_token.value();
            BOOST_LOG_TRIVIAL(info) << "UltiMaker: Refresh token loaded from OS keyring";
        } else {
            BOOST_LOG_TRIVIAL(debug) << "UltiMaker: No refresh token in keyring";
        }
    } else {
        BOOST_LOG_TRIVIAL(warning) << "UltiMaker: Secure storage not available; refresh tokens will not persist across restarts";
    }
    
    // Step 2: Load access token and metadata from JSON
    if (boost::filesystem::exists(m_oauth_cred_file)) {
        nlohmann::json j;
        try {
            boost::nowide::ifstream ifs(m_oauth_cred_file);
            ifs >> j;
            ifs.close();

            if (j.contains("access_token")) {
                m_cred["access_token"] = j["access_token"];
            }
            
            bool scrub_legacy_refresh_token = false;
            if (j.contains("refresh_token")) {
                const std::string legacy_refresh_token = j["refresh_token"].get<std::string>();
                scrub_legacy_refresh_token             = true;

                if (!legacy_refresh_token.empty() && !m_cred.count("refresh_token")) {
                    m_cred["refresh_token"] = legacy_refresh_token;
                    BOOST_LOG_TRIVIAL(warning)
                        << "UltiMaker: Loaded legacy refresh token from JSON for this session only; it will be removed from disk";
                }

                if (!legacy_refresh_token.empty() && SecureStorage::is_available()) {
                    if (SecureStorage::store(KEYRING_ACCOUNT, legacy_refresh_token)) {
                        BOOST_LOG_TRIVIAL(info) << "UltiMaker: Migrated refresh token from JSON to OS keyring";
                    } else {
                        BOOST_LOG_TRIVIAL(warning)
                            << "UltiMaker: Failed to migrate refresh token to secure storage; re-authentication may be required after restart";
                    }
                }
            }

            if (scrub_legacy_refresh_token) {
                j.erase("refresh_token");
                if (write_oauth_metadata_file(m_oauth_cred_file, j, "scrub legacy refresh token")) {
                    BOOST_LOG_TRIVIAL(info) << "UltiMaker: Removed legacy refresh token from JSON metadata";
                }
            }
            
        } catch (std::exception& err) {
            BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << ": parse " << m_oauth_cred_file << " failed, reason = " << err.what();
            m_cred.clear();
        }
    }
}

void UltiMaker::save_oauth_credential(const GUI::OAuthResult& cred) const
{
    BOOST_LOG_TRIVIAL(info) << "UltiMaker: Saving OAuth credentials";

    m_cred["access_token"] = cred.access_token;
    if (!cred.refresh_token.empty()) {
        m_cred["refresh_token"] = cred.refresh_token;
    } else {
        m_cred.erase("refresh_token");
    }

    // Step 1: Save refresh token to secure keyring only.
    bool refresh_token_secured = cred.refresh_token.empty();
    if (!cred.refresh_token.empty()) {
        if (SecureStorage::is_available()) {
            if (SecureStorage::store(KEYRING_ACCOUNT, cred.refresh_token)) {
                BOOST_LOG_TRIVIAL(info) << "UltiMaker: Refresh token stored in OS keyring";
                refresh_token_secured = true;
            } else {
                BOOST_LOG_TRIVIAL(warning)
                    << "UltiMaker: Failed to store refresh token in secure storage; it will not be persisted to disk";
            }
        } else {
            BOOST_LOG_TRIVIAL(warning)
                << "UltiMaker: Secure storage not available; refresh token will remain in memory only for this session";
        }
    }
    
    // Step 2: Save access token and metadata to JSON. Refresh tokens never go to disk.
    nlohmann::json j;
    j["access_token"] = cred.access_token;

    const bool metadata_saved = write_oauth_metadata_file(m_oauth_cred_file, j, "save oauth credential");

    if (!refresh_token_secured) {
        BOOST_LOG_TRIVIAL(warning)
            << "UltiMaker: Refresh token is not persisted and the user may need to log in again after restart or expiry";
    }

    if (metadata_saved) {
        BOOST_LOG_TRIVIAL(info) << "UltiMaker: OAuth credentials saved successfully";
    }
}

void UltiMaker::clear_oauth_credential() const
{
    m_cred.clear();

    if (SecureStorage::is_available()) {
        if (SecureStorage::remove(KEYRING_ACCOUNT)) {
            BOOST_LOG_TRIVIAL(info) << "UltiMaker: Refresh token removed from OS keyring";
        } else {
            BOOST_LOG_TRIVIAL(warning) << "UltiMaker: Failed to remove refresh token from keyring";
        }
    }

    if (boost::filesystem::exists(m_oauth_cred_file)) {
        boost::nowide::remove(m_oauth_cred_file.c_str());
    }
}

bool UltiMaker::refresh_token() const
{
    if (m_cred.find("refresh_token") == m_cred.end()) {
        BOOST_LOG_TRIVIAL(warning) << "UltiMaker: No refresh token available";
        return false;
    }

    // Build URL-encoded form body for refresh token request
    std::string post_body;
    post_body += "client_id=" + Http::url_encode(CLIENT_ID);
    post_body += "&client_secret=" + Http::url_encode(CLIENT_SECRET);
    post_body += "&redirect_uri=" + get_callback_url();
    post_body += "&grant_type=" + Http::url_encode("refresh_token");
    post_body += "&refresh_token=" + Http::url_encode(m_cred.at("refresh_token"));
    post_body += "&scope=" + Http::url_encode(SCOPES);

    // Cura retries refresh asynchronously. Because Orca currently refreshes on the calling
    // thread, keep this to a single network attempt so interactive UI actions don't hang.
    BOOST_LOG_TRIVIAL(info) << "UltiMaker: Refreshing access token";

    bool success = false;

    auto http = Http::post(TOKEN_URL);
    http.timeout_connect(5)
        .timeout_max(10)
        .header("Content-Type", "application/x-www-form-urlencoded")
        .header("User-Agent", "UltiMaker OrcaSlicer Plugin")
        .set_post_body(post_body)
        .on_complete([this, &success](std::string body, unsigned http_status) {
            GUI::OAuthResult r;
            GUI::OAuthJob::parse_token_response(body, false, r);
            if (r.success) {
                if (r.refresh_token.empty() && this->m_cred.find("refresh_token") != this->m_cred.end()) {
                    r.refresh_token = this->m_cred.at("refresh_token");
                }

                BOOST_LOG_TRIVIAL(info) << "UltiMaker: Successfully refreshed access token";
                this->save_oauth_credential(r);

                // Update in-memory credentials
                this->m_cred["access_token"] = r.access_token;
                if (!r.refresh_token.empty()) {
                    this->m_cred["refresh_token"] = r.refresh_token;
                }
                success = true;
            } else {
                BOOST_LOG_TRIVIAL(error) << "UltiMaker: Failed to parse refresh token response: " << r.error_message;

                // Check if refresh token was revoked (specific error codes)
                if (r.error_message.find("invalid_grant") != std::string::npos ||
                    r.error_message.find("invalid_request") != std::string::npos) {
                    BOOST_LOG_TRIVIAL(error) << "UltiMaker: Refresh token appears to be revoked, clearing credentials";
                    this->clear_oauth_credential();
                }
            }
        })
        .on_error([&success](std::string body, std::string error, unsigned http_status) {
            BOOST_LOG_TRIVIAL(error) << "UltiMaker: Failed to refresh token: " << error << ", HTTP " << http_status;
            success = false;
        })
        .perform_sync();

    if (!success) {
        BOOST_LOG_TRIVIAL(error) << "UltiMaker: Token refresh request failed";
    }

    return success;
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
    clear_oauth_credential();
    BOOST_LOG_TRIVIAL(info) << "UltiMaker: OAuth credentials removed, user logged out";
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

    bool res = true;

    const auto create_request = [this, &build_request, &res, &on_complete](const std::string& access_token, bool is_retry) {
        auto http = build_request(is_retry);
        set_auth(http, access_token);
        http.header("User-Agent", "UltiMaker OrcaSlicer Plugin")
            .header("Cache-Control", "no-cache, no-store, must-revalidate")
            .header("Pragma", "no-cache")
            .header("Expires", "0")
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

namespace {
// Helper to get MIME type based on FORMAT_CONFIG_ID and file extension
// Valid types per API: text/plain, application/x-ufp, application/gzip, 
// application/x-makerbot, application/x-makerbot-sketch, text/x-gcode
std::string get_mime_type_for_upload(const std::string& format_config_id, const std::string& extension)
{
    // UltiMaker formats - use application/x-ufp
    if (format_config_id == "ultimaker_s6" || format_config_id == "ultimaker_s5" || 
        format_config_id == "ultimaker_2pc" || format_config_id == "ultimaker_f4") {
        return "application/x-ufp";
    }
    // MakerBot Sketch formats - use application/x-makerbot-sketch
    if (format_config_id == "sketch_small" || format_config_id == "sketch_sprint" || 
        format_config_id == "sketch_large") {
        return "application/x-makerbot-sketch";
    }
    // MakerBot Method formats - use application/x-makerbot
    if (format_config_id == "method" || format_config_id == "method_x" || format_config_id == "method_xl") {
        return "application/x-makerbot";
    }
    // Fallback based on extension
    if (extension == ".ufp") {
        return "application/x-ufp";
    }
    if (extension == ".makerbot") {
        return "application/x-makerbot";
    }
    return "application/x-ufp";  // Default to x-ufp for unknown formats
}
} // namespace

bool UltiMaker::upload(PrintHostUpload upload_data, ProgressFn prorgess_fn, ErrorFn error_fn, InfoFn info_fn) const
{
    BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: Starting upload";
    
    if (m_cred.find("access_token") == m_cred.end()) {
        BOOST_LOG_TRIVIAL(error) << "UltiMaker::upload: ERROR - Account not linked";
        error_fn(_L("UltiMaker account not linked. Go to Connect options to set it up."));
        return false;
    }

    // Get printer notes from extended_info to check for FORMAT_CONFIG_ID
    std::string printer_notes;
    auto notes_it = upload_data.extended_info.find("printer_notes");
    if (notes_it != upload_data.extended_info.end()) {
        printer_notes = notes_it->second;
    }

    BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: printer_notes=" << printer_notes;

    // Get format_config_id to determine correct Content-Type
    std::string format_config_id = FormatConfig::parse_format_config_id(printer_notes, "");
    std::string format_type = FormatConfig::get_format_type_for_printer(printer_notes);
    
    BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: format_config_id=" << format_config_id;
    BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: format_type=" << format_type;

    // UltiMaker Digital Factory ONLY accepts container formats - raw G-code will ALWAYS be rejected
    if (format_type.empty()) {
        BOOST_LOG_TRIVIAL(error) << "UltiMaker::upload: ERROR - Raw G-code upload not supported. Container format required.";
        error_fn(_L("UltiMaker Digital Factory requires container format (.ufp or .makerbot). Please ensure your printer profile has FORMAT_CONFIG_ID set in printer notes."));
        return false;
    }

    std::string container_path;
    std::string source_ext = upload_data.source_path.extension().string();
    boost::to_lower(source_ext);
    
    // Get extension for container format
    std::string extension = FormatConfig::get_file_extension_for_format(format_type);
    std::string base_filename = upload_data.upload_path.filename().stem().string();
    
    // Check if source is already a container format
    bool source_is_container = (source_ext == ".ufp" || source_ext == ".makerbot");
    
    if (source_is_container) {
        // Source is already a container - use it directly
        BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: Source is already container format: " << upload_data.source_path;
        container_path = upload_data.source_path.string();
    } else {
        // Printer requires container format - create container from G-code
        BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: Container format required: " << format_type;
        
        // Create temp file for container
        fs::path temp_dir = boost::filesystem::temp_directory_path();
        container_path = (temp_dir / (base_filename + "_upload" + extension)).string();
        
        BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: Creating container at: " << container_path;
        
        // Convert G-code to container format
        std::string error_message;
        if (!FormatConfig::export_to_container(format_type, 
                                               upload_data.source_path.string(), 
                                               container_path, 
                                               printer_notes, 
                                               error_message)) {
            // Hard stop - conversion failed, no fallback to raw G-code
            BOOST_LOG_TRIVIAL(error) << "UltiMaker::upload: ERROR - Container conversion FAILED: " << error_message;
            error_fn(_L("Failed to create container file for upload: ") + error_message);
            return false;
        }
    }
    BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: Container created successfully at: " << container_path;
    
    // Update source_path to upload the container instead of raw G-code
    upload_data.source_path = container_path;
    // Update filename to reflect container extension
    upload_data.upload_path = upload_data.upload_path.parent_path() / 
                              (base_filename + extension);

    const auto filename = upload_data.upload_path.filename().string();
    source_ext = upload_data.source_path.extension().string();
    
    BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: filename=" << filename;
    BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: source_ext=" << source_ext;

    // Get MIME type for the upload request
    std::string mime_type = get_mime_type_for_upload(format_config_id, source_ext);
    BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: MIME type=" << mime_type;

    // Check if a project folder is specified in extended_info
    std::string project_id;
    auto it = upload_data.extended_info.find("project_id");
    if (it != upload_data.extended_info.end()) {
        project_id = it->second;
    }
    BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: project_id=" << project_id;

    // Get file size for upload request
    boost::filesystem::ifstream file_check(upload_data.source_path, std::ios::binary | std::ios::ate);
    if (!file_check.good()) {
        BOOST_LOG_TRIVIAL(error) << "UltiMaker::upload: ERROR - Cannot open file for size check: " << upload_data.source_path;
        error_fn(_L("Cannot open file for upload"));
        return false;
    }
    size_t file_size = file_check.tellg();
    file_check.close();
    BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: file_size=" << file_size;

    // Capture container_path for cleanup
    std::string final_container_path = container_path;

    // =========================================================================
    // STEP 1: Request upload URL from Digital Factory API
    // =========================================================================
    BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: STEP 1 - Requesting upload URL from API";
    
    // Build JSON request body matching Cura's format
    nlohmann::json request_body = {
        {"data", {
            {"job_name", filename},
            {"file_size", file_size},
            {"content_type", mime_type},
            {"library_project_id", project_id}
        }}
    };
    // source_file_id is optional - only include if we have a valid value
    // API requires minimum 44 characters if provided
    
    std::string request_body_str = request_body.dump();

    UploadResponse upload_response;
    const bool step1_success = do_api_call(
        [&request_body_str](bool is_retry) {
            auto http = Http::put(LIBRARY_API_BASE + "/jobs/upload");
            http.header("Content-Type", "application/json")
                .header("Accept", "application/json")
                .set_put_body_raw(request_body_str);
            return http;
        },
        [&](std::string body, unsigned http_status) {
            BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: STEP 1 response status=" << http_status;
            BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: STEP 1 response body=" << body;

            if (http_status < 200 || http_status >= 300) {
                upload_response.error_message = format_error(body, "", http_status).utf8_string();
                return false;
            }

            try {
                const auto response_json = nlohmann::json::parse(body);
                if (!response_json.contains("data")) {
                    upload_response.error_message = "Missing 'data' in response";
                    return false;
                }

                const auto& data = response_json["data"];
                upload_response.upload_url = data.value("upload_url", "");
                upload_response.content_type = data.value("content_type", mime_type);
                upload_response.job_id = data.value("job_id", "");
                upload_response.success = !upload_response.upload_url.empty();

                if (!upload_response.success) {
                    upload_response.error_message = "Missing upload_url in response";
                }
            } catch (const std::exception& e) {
                upload_response.error_message = std::string("JSON parse error: ") + e.what();
                BOOST_LOG_TRIVIAL(error) << "UltiMaker::upload: JSON parse error: " << e.what();
                return false;
            }

            return upload_response.success;
        },
        [&](std::string body, std::string error, unsigned http_status) {
            BOOST_LOG_TRIVIAL(error) << "UltiMaker::upload: STEP 1 error: " << error << ", HTTP " << http_status;
            upload_response.error_message = format_error(body, error, http_status).utf8_string();
            return false;
        });
    
    if (!step1_success || !upload_response.success) {
        BOOST_LOG_TRIVIAL(error) << "UltiMaker::upload: STEP 1 FAILED: " << upload_response.error_message;
        error_fn(_("Failed to request upload URL: ") + upload_response.error_message);
        // Cleanup temp file on failure
        try { boost::filesystem::remove(final_container_path); } catch (...) {}
        return false;
    }
    
    BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: STEP 1 SUCCESS - upload_url=" << upload_response.upload_url;
    BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: STEP 1 SUCCESS - content_type=" << upload_response.content_type;

    // =========================================================================
    // STEP 2: Upload file to pre-signed URL
    // =========================================================================
    BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: STEP 2 - Uploading file to: " << upload_response.upload_url;
    BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: STEP 2 - File path: " << upload_data.source_path;
    
    // Verify file exists and is readable before uploading
    if (!boost::filesystem::exists(upload_data.source_path)) {
        BOOST_LOG_TRIVIAL(error) << "UltiMaker::upload: ERROR - File does not exist: " << upload_data.source_path;
        error_fn(_L("Upload failed: Container file not found"));
        // Cleanup temp file
        try { boost::filesystem::remove(final_container_path); } catch (...) {}
        return false;
    }
    
    // Test open the file to ensure it's readable
    {
        boost::filesystem::ifstream test_stream(upload_data.source_path, std::ios::binary);
        if (!test_stream.is_open()) {
            BOOST_LOG_TRIVIAL(error) << "UltiMaker::upload: ERROR - Cannot open file for reading: " << upload_data.source_path;
            error_fn(_L("Upload failed: Cannot read container file"));
            // Cleanup temp file
            try { boost::filesystem::remove(final_container_path); } catch (...) {}
            return false;
        }
        test_stream.close();
    }
    
    BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: STEP 2 - File verified, proceeding with upload";
    BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: STEP 2 - Content-Type: " << upload_response.content_type;
    
    // Read file into memory buffer to avoid Http::set_put_body callback lifetime issues
    std::string file_buffer;
    try {
        boost::filesystem::ifstream file_stream(upload_data.source_path, std::ios::binary | std::ios::ate);
        if (!file_stream.is_open()) {
            BOOST_LOG_TRIVIAL(error) << "UltiMaker::upload: ERROR - Cannot open file for reading: " << upload_data.source_path;
            error_fn(_L("Upload failed: Cannot read container file"));
            try { boost::filesystem::remove(final_container_path); } catch (...) {}
            return false;
        }
        
        auto file_size = file_stream.tellg();
        file_stream.seekg(0, std::ios::beg);
        
        BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: STEP 2 - File size: " << file_size;
        BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: STEP 2 - Reading file into memory buffer";
        
        file_buffer.resize(static_cast<size_t>(file_size));
        if (!file_stream.read(file_buffer.data(), file_size)) {
            BOOST_LOG_TRIVIAL(error) << "UltiMaker::upload: ERROR - Failed to read file into buffer";
            error_fn(_L("Upload failed: Cannot read container file"));
            try { boost::filesystem::remove(final_container_path); } catch (...) {}
            return false;
        }
        file_stream.close();
        
        BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: STEP 2 - File loaded into buffer, size=" << file_buffer.size();
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "UltiMaker::upload: ERROR - Exception reading file: " << e.what();
        error_fn(_L("Upload failed: Cannot read container file"));
        try { boost::filesystem::remove(final_container_path); } catch (...) {}
        return false;
    }
    
    bool upload_success = false;
    {
        BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: STEP 2 - Creating Http::put request";
        auto http = Http::put(upload_response.upload_url);
        http.header("Content-Type", upload_response.content_type);
        BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: STEP 2 - Calling set_put_body_raw with file buffer";
        http.set_put_body_raw(file_buffer);  // Use PUT with raw body (fixed for proper upload)
        BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: STEP 2 - set_post_body completed, adding progress callback";
        http.on_progress([&prorgess_fn](Http::Progress progress, bool& cancel) { 
            prorgess_fn(std::move(progress), cancel); 
        });
        BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: STEP 2 - All callbacks set, calling perform_sync";

        // No auth needed for pre-signed URL
        http.on_complete([&](std::string body, unsigned http_status) {
            BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: STEP 2 response status=" << http_status;
            BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: STEP 2 response body=" << body;
            upload_success = (http_status >= 200 && http_status < 300);
        });
        
        http.on_error([&](std::string body, std::string error, unsigned http_status) {
            BOOST_LOG_TRIVIAL(error) << "UltiMaker::upload: STEP 2 error: " << error << ", HTTP " << http_status << ", body: " << body;
            upload_success = false;
        });
        
        http.perform_sync();
    }
    
    // Schedule cleanup of temp container file 60 seconds after result
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
    
    if (upload_success) {
        BOOST_LOG_TRIVIAL(info) << "UltiMaker::upload: SUCCESS";
        info_fn("UltiMaker", _L("File uploaded successfully"));
    } else {
        BOOST_LOG_TRIVIAL(error) << "UltiMaker::upload: FAILED";
        error_fn(_L("Failed to upload file to Digital Factory"));
    }
    
    return upload_success;
}

bool UltiMaker::get_projects(std::vector<ProjectInfo>& projects) const
{
    if (m_cred.find("access_token") == m_cred.end())
        return false;
    
    bool result = false;
    projects.clear();

    return do_api_call(
        [](bool is_retry) {
            // Add cache-busting timestamp to prevent browser/proxy caching
            std::string url = LIBRARY_API_BASE + "/projects?shared=false&limit=100&_=" + std::to_string(std::time(nullptr));
            auto http = Http::get(url);
            http.header("Accept", "application/json");
            BOOST_LOG_TRIVIAL(info) << "UltiMaker: Fetching projects from " << url;
            return http;
        },
        [&result, &projects](std::string body, unsigned http_status) {
            try {
                auto j = nlohmann::json::parse(body);
                
                if (j.contains("data") && j["data"].is_array()) {
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
                        if (!info.id.empty() && !info.display_name.empty()) {
                            projects.push_back(info);
                        }
                    }
                    
                    // Sort projects alphabetically by display_name (case-insensitive)
                    std::sort(projects.begin(), projects.end(), [](const ProjectInfo& a, const ProjectInfo& b) {
                        return a.display_name < b.display_name;
                    });
                    
                    result = true;
                }
            } catch (const std::exception& e) {
                BOOST_LOG_TRIVIAL(error) << "UltiMaker: Failed to parse projects response: " << e.what();
            }
            return result;
        },
        [&result](std::string body, std::string error, unsigned http_status) {
            BOOST_LOG_TRIVIAL(error) << "UltiMaker: Error getting projects: " << error << ", HTTP " << http_status;
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
    if (m_cred.find("access_token") == m_cred.end()) {
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
            return http;
        },
        [&result](std::string body, unsigned http_status) {
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
