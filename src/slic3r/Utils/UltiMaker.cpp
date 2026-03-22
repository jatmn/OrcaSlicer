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
    std::replace(b64.begin(), b64.end(), '+', '-');
    std::replace(b64.begin(), b64.end(), '/', '_');
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

    return GUI::OAuthParams{
        login_url,
        CLIENT_ID,
        CLIENT_SECRET,
        callback_port,
        callback_url,
        SCOPES,
        RESPONSE_TYPE,
        callback_url,
        callback_url,
        TOKEN_URL,
        verification_code,
        state,
    };
}

void UltiMaker::load_oauth_credential()
{
    m_cred.clear();
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
    nlohmann::json j;
    j["access_token"] = cred.access_token;
    j["refresh_token"] = cred.refresh_token;

    boost::nowide::ofstream c;
    c.open(m_oauth_cred_file, std::ios::out | std::ios::trunc);
    c << std::setw(4) << j << std::endl;
    c.close();
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

                auto http = Http::post(TOKEN_URL);
                http.timeout_connect(5)
                    .timeout_max(5)
                    .form_add("grant_type", "refresh_token")
                    .form_add("client_id", CLIENT_ID)
                    .form_add("client_secret", CLIENT_SECRET)
                    .form_add("refresh_token", m_cred.at("refresh_token"))
                    .form_add("scope", SCOPES)
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

    return do_api_call(
        [&upload_data, &filename, &prorgess_fn](bool is_retry) {
            auto http = Http::post(LIBRARY_API_BASE + "/files");
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

} // namespace Slic3r
