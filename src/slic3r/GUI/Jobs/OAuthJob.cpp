#include "OAuthJob.hpp"

#include "Http.hpp"
#include "ThreadSafeQueue.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "nlohmann/json.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/log/trivial.hpp>
#include <thread>

namespace Slic3r {
namespace GUI {

wxDEFINE_EVENT(EVT_OAUTH_COMPLETE_MESSAGE, wxCommandEvent);

OAuthJob::OAuthJob(const OAuthData& input) : local_authorization_server(input.params.callback_port), _data(input) {
    local_authorization_server.set_port(input.params.callback_port);
}

void OAuthJob::parse_token_response(const std::string& body, bool error, OAuthResult& result)
{
    const auto j = nlohmann::json::parse(body, nullptr, false, true);
    if (j.is_discarded()) {
        BOOST_LOG_TRIVIAL(warning) << "UltiMaker OAuth: Invalid or empty JSON data in token response";
        result.error_message = _u8L("Unknown error");
    } else if (error) {
        if (j.contains("error_description")) {
            j.at("error_description").get_to(result.error_message);
        } else {
            result.error_message = _u8L("Unknown error");
        }
    } else {
        if (j.contains("error")) {
            std::string error_str;
            j.at("error").get_to(error_str);
            if (j.contains("error_description")) {
                j.at("error_description").get_to(result.error_message);
            } else {
                result.error_message = error_str;
            }
            BOOST_LOG_TRIVIAL(error) << "UltiMaker OAuth: Token request returned error: " << error_str << " - " << result.error_message;
        } else if (j.contains("access_token")) {
            j.at("access_token").get_to(result.access_token);
            j.at("refresh_token").get_to(result.refresh_token);
            // Capture token expiration time if provided
            if (j.contains("expires_in")) {
                j.at("expires_in").get_to(result.expires_in);
                BOOST_LOG_TRIVIAL(info) << "UltiMaker OAuth: Token expires in " << result.expires_in << " seconds";
            }
            result.success = true;
            BOOST_LOG_TRIVIAL(info) << "UltiMaker OAuth: Successfully obtained access token";
        } else {
            result.error_message = _u8L("No access token in response");
            BOOST_LOG_TRIVIAL(error) << "UltiMaker OAuth: No access_token in successful response";
        }
    }
}


void OAuthJob::process(Ctl& ctl)
{
    // Prepare auth process
    std::shared_ptr<ThreadSafeQueueSPSC<OAuthResult>> queue = std::make_shared<ThreadSafeQueueSPSC<OAuthResult>>();

    // Setup auth server to receive OAuth code from callback url
    local_authorization_server.set_request_handler([this, queue](const std::string& url) -> std::shared_ptr<HttpServer::Response> {
        // Handle /success endpoint - user successfully authenticated
        if (boost::contains(url, "/success")) {
            BOOST_LOG_TRIVIAL(info) << "UltiMaker OAuth: Authentication successful, showing success page";
            const std::string html =
                "<html><head><meta charset=\"utf-8\">"
                "<style>body{font-family:Arial,sans-serif;background:#f7f7f7;color:#222;margin:32px;}"
                "</style></head><body><div class=\"container\">"
                "<h2>Authentication Successful</h2>"
                "<p>You can return to OrcaSlicer. This window will close automatically.</p>"
                "<script>setTimeout(function(){try{window.close();}catch(e){}},1500);</script>"
                "</div></body></html>";
            return std::make_shared<HttpServer::ResponseHtml>(html);
        }

        // Handle /fail endpoint - authentication failed
        if (boost::contains(url, "/fail")) {
            BOOST_LOG_TRIVIAL(info) << "UltiMaker OAuth: Authentication failed, showing failure page";
            const std::string html =
                "<html><head><meta charset=\"utf-8\">"
                "<style>body{font-family:Arial,sans-serif;background:#f7f7f7;color:#222;margin:32px;}"
                "</style></head><body><div class=\"container\">"
                "<h2>Authentication Failed</h2>"
                "<p>Something went wrong. Please return to OrcaSlicer and try again.</p>"
                "<script>setTimeout(function(){try{window.close();}catch(e){}},3000);</script>"
                "</div></body></html>";
            return std::make_shared<HttpServer::ResponseHtml>(html);
        }

        if (boost::contains(url, "/callback")) {
            const auto code = url_get_param(url, "code");
            const auto state = url_get_param(url, "state");

            const auto handle_auth_fail = [this, queue](const std::string& message) -> std::shared_ptr<HttpServer::ResponseRedirect> {
                queue->push(OAuthResult{false, message});
                return std::make_shared<HttpServer::ResponseRedirect>(this->_data.params.auth_fail_redirect_url);
            };

            // Only validate state if it was returned (some OAuth providers like UltiMaker don't return it)
            if (!state.empty() && state != _data.params.state) {
                BOOST_LOG_TRIVIAL(warning) << "UltiMaker OAuth: State validation failed";
                return handle_auth_fail(_u8L("The provided state is not correct."));
            }

            if (code.empty()) {
                const auto error_code = url_get_param(url, "error_code");
                if (error_code == "user_denied") {
                    BOOST_LOG_TRIVIAL(debug) << "User did not give the required permission when authorizing this application";
                    return handle_auth_fail(_u8L("Please give the required permissions when authorizing this application."));
                }

                BOOST_LOG_TRIVIAL(warning) << "UltiMaker OAuth: Unexpected login error. Error_code: " << error_code;
                return handle_auth_fail(_u8L("Something unexpected happened when trying to log in, please try again."));
            }


            OAuthResult r;
            BOOST_LOG_TRIVIAL(info) << "UltiMaker OAuth: Exchanging authorization code for token";
            
            // Build application/x-www-form-urlencoded body exactly like Cura does:
            // urllib.parse.urlencode(data) with Content-type: application/x-www-form-urlencoded
            // NOTE: redirect_uri should NOT be double-encoded - it's already a simple localhost URL
            std::string post_body;
            post_body += "client_id=" + Http::url_encode(_data.params.client_id);
            post_body += "&client_secret=" + Http::url_encode(_data.params.client_secret);
            post_body += "&redirect_uri=" + _data.params.callback_url;
            post_body += "&grant_type=" + Http::url_encode("authorization_code");
            post_body += "&code=" + Http::url_encode(code);
            post_body += "&code_verifier=" + Http::url_encode(_data.params.verification_code);
            post_body += "&scope=" + Http::url_encode(_data.params.scope);

            auto http = Http::post(_data.params.token_url);
            http.timeout_connect(5)
                .timeout_max(5)
                .set_user_agent("Cura/5.7.0 (Windows x86_64)")
                .allow_tls_flexible(true)  // Allow TLSv1.3+ for UltiMaker OAuth servers
                .header("Content-Type", "application/x-www-form-urlencoded")
                .set_post_body(post_body)
                .on_complete([&](std::string body, unsigned status) { 
                    BOOST_LOG_TRIVIAL(info) << "UltiMaker OAuth: Token response status: " << status;
                    parse_token_response(body, false, r); 
                })
                .on_error([&](std::string body, std::string error, unsigned status) { 
                    BOOST_LOG_TRIVIAL(error) << "UltiMaker OAuth: Token request error: " << error << " HTTP " << status;
                    BOOST_LOG_TRIVIAL(error) << "UltiMaker OAuth: Token error body: " << body;
                    parse_token_response(body, true, r); 
                })
                .perform_sync();

            queue->push(r);
            return std::make_shared<HttpServer::ResponseRedirect>(r.success ? _data.params.auth_success_redirect_url :
                                                                              _data.params.auth_fail_redirect_url);
        } else {
            queue->push(OAuthResult{false});
            return std::make_shared<HttpServer::ResponseNotFound>();
        }
    });

    // Run the local server
    local_authorization_server.start();

    // Wait until we received the result
    bool received = false;
    while (!ctl.was_canceled() && !received ) {
        queue->consume_one(BlockingWait{1000}, [this, &received](const OAuthResult& result) {
            *_data.result = result;
            received      = true;
        });
    }

    // Handle timeout
    if (!received && ctl.was_canceled()) {
        _data.result->error_message = _u8L("User canceled.");
    } else {
        // Wait a while to ensure the response has sent
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
}

void OAuthJob::finalize(bool canceled, std::exception_ptr& e)
{
    // Make sure it's stopped
    local_authorization_server.stop();

    wxCommandEvent event(EVT_OAUTH_COMPLETE_MESSAGE);
    event.SetEventObject(m_event_handle);
    wxPostEvent(m_event_handle, event);
}
	
}} // namespace Slic3r::GUI
