#include "UltiMaker.hpp"

#include <openssl/sha.h>
#include <openssl/rand.h>
#include <boost/beast/core/detail/base64.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio.hpp>

#include "slic3r/Utils/Http.hpp"
#include "nlohmann/json.hpp"
#include "libslic3r/Utils.hpp"
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
    BOOST_LOG_TRIVIAL(error) << "UltiMaker: Loading OAuth credentials from " << m_oauth_cred_file;
    if (boost::filesystem::exists(m_oauth_cred_file)) {
        nlohmann::json j;
        try {
            boost::nowide::ifstream ifs(m_oauth_cred_file);
            ifs >> j;
            ifs.close();

            m_cred["access_token"] = j["access_token"];
            m_cred["refresh_token"] = j["refresh_token"];
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

    boost::nowide::ofstream c;
    c.open(m_oauth_cred_file, std::ios::out | std::ios::trunc);
    c << std::setw(4) << j << std::endl;
    c.close();
    BOOST_LOG_TRIVIAL(error) << "UltiMaker: OAuth credentials saved successfully";
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
            .on_complete([&](std::string body, unsigned http_status) {
                res = on_complete(body, http_status);
            });

        return http;
    };

    create_request(m_cred.at("access_token"), false)
        .on_error([&res, &on_error, this, &create_request](std::string body, std::string error, unsigned http_status) {
            if (http_status == 401) {
                BOOST_LOG_TRIVIAL(warning) << boost::format("UltiMaker: Access token invalid: %1%, HTTP %2%, body: `%3%`") % error %
                                                  http_status % body;
                BOOST_LOG_TRIVIAL(info) << "UltiMaker: Attempt to refresh access token";

                // Build URL-encoded form body (matches Cura's getAccessTokenUsingRefreshToken)
                std::string post_body;
                post_body += "client_id=" + Http::url_encode(CLIENT_ID);
                post_body += "&client_secret=" + Http::url_encode(CLIENT_SECRET);
                post_body += "&redirect_uri=" + get_callback_url();
                post_body += "&grant_type=" + Http::url_encode("refresh_token");
                post_body += "&refresh_token=" + Http::url_encode(m_cred.at("refresh_token"));
                post_body += "&scope=" + Http::url_encode(SCOPES);

                auto http = Http::post(TOKEN_URL);
                http.timeout_connect(5)
                    .timeout_max(5)
                    .header("Content-Type", "application/x-www-form-urlencoded")
                    .set_post_body(post_body)
                    .on_complete([this, &res, &on_error, &create_request](std::string body, unsigned http_status) {
                        GUI::OAuthResult r;
                        GUI::OAuthJob::parse_token_response(body, false, r);
                        if (r.success) {
                            BOOST_LOG_TRIVIAL(info) << "UltiMaker: Successfully refreshed access token";
                            this->save_oauth_credential(r);

                            create_request(r.access_token, true)
                                .on_error([&res, &on_error](std::string body, std::string error, unsigned http_status) {
                                    res = on_error(body, error, http_status);
                                })
                                .perform_sync();
                        } else {
                            BOOST_LOG_TRIVIAL(error)
                                << boost::format("UltiMaker: Failed to refresh access token: %1%, body: `%2%`") % r.error_message % body;
                            res = on_error(body, r.error_message, http_status);
                        }
                    })
                    .on_error([&res, &on_error](std::string body, std::string error, unsigned http_status) {
                        BOOST_LOG_TRIVIAL(error)
                            << boost::format("UltiMaker: Failed to refresh access token: %1%, HTTP %2%, body: `%3%`") % error %
                                   http_status % body;
                        res = on_error(body, error, http_status);
                    })
                    .perform_sync();
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

    const auto filename = upload_data.upload_path.filename().string();

    // Check if a project folder is specified in extended_info
    std::string project_id;
    auto it = upload_data.extended_info.find("project_id");
    if (it != upload_data.extended_info.end()) {
        project_id = it->second;
    }

    return do_api_call(
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
        [&info_fn, &filename](std::string body, unsigned status) {
            BOOST_LOG_TRIVIAL(info) << boost::format("UltiMaker: File uploaded: HTTP %1%: %2%") % status % body;
            info_fn("UltiMaker", _L("File uploaded successfully"));
            return true;
        },
        [this, &error_fn](std::string body, std::string error, unsigned status) {
            BOOST_LOG_TRIVIAL(error) << boost::format("UltiMaker: Error uploading file: %1%, HTTP %2%, body: `%3%`") % error % status % body;
            error_fn(format_error(body, error, status));
            return false;
        });
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

bool UltiMaker::create_project(const std::string& name, std::string& project_id, std::string& project_name) const
{
    if (m_cred.find("access_token") == m_cred.end()) {
        return false;
    }

    bool result = false;

    const auto create_request = [this, &name](const std::string& access_token) {
        auto http = Http::put(LIBRARY_API_BASE + "/projects");
        http.header("Accept", "application/json");
        http.header("Authorization", "Bearer " + access_token);
        http.header("Content-Type", "application/json");
        http.header("User-Agent", "UltiMaker OrcaSlicer Plugin");
        
        nlohmann::json body;
        body["display_name"] = name;
        http.set_post_body(body.dump());
        return http;
    };

    create_request(m_cred.at("access_token"))
        .on_complete([&result, &project_id, &project_name](std::string body, unsigned http_status) {
            try {
                auto j = nlohmann::json::parse(body);
                if (j.contains("data")) {
                    const auto& proj = j["data"];
                    // Use library_project_id as the id
                    if (proj.contains("library_project_id")) {
                        project_id = proj["library_project_id"];
                    } else if (proj.contains("id")) {
                        project_id = proj["id"];
                    }
                    if (proj.contains("display_name")) {
                        project_name = proj["display_name"];
                    }
                    result = true;
                }
            } catch (const std::exception& e) {
                BOOST_LOG_TRIVIAL(error) << "UltiMaker: Failed to parse create project response: " << e.what();
            }
        })
        .on_error([](std::string body, std::string error, unsigned http_status) {
            BOOST_LOG_TRIVIAL(error) << boost::format("UltiMaker: Error creating project: %1%, HTTP %2%") % error % http_status;
        })
        .perform_sync();

    return result;
}

} // namespace Slic3r
