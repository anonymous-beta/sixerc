
#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace sixerc {

using byte = std::uint8_t;
using bytes = std::vector<byte>;

enum class AuditSeverity {
    Informational,
    Low,
    Medium,
    High,
    Critical
};

struct AuditFinding {
    std::string id;
    std::string title;
    std::string description;
    AuditSeverity severity = AuditSeverity::Informational;
    std::string recommendation;
    std::chrono::system_clock::time_point observed_at =
        std::chrono::system_clock::now();
};

struct SessionClaims {
    std::string issuer;
    std::string audience;
    std::string subject;
    std::string tenant_id;
    std::string object_id;
    std::string client_id;

    std::optional<std::int64_t> issued_at;
    std::optional<std::int64_t> not_before;
    std::optional<std::int64_t> expires_at;

    bool expired = false;
    bool not_yet_valid = false;
    bool signature_verified = false;
};

struct SessionInspection {
    bool structurally_valid = false;
    bool claims_read = false;
    bool signature_verified = false;

    std::string token_format;
    std::string message;

    SessionClaims claims;
    std::vector<AuditFinding> findings;
};

struct SystemFacts {
    std::wstring computer_name;
    std::wstring windows_version;
    std::wstring architecture;
    std::uint64_t uptime_seconds = 0;
};

struct ProcessRecord {
    std::uint32_t pid = 0;
    std::wstring image_name;
};

struct StartupEntry {
    std::wstring source;
    std::wstring name;
    std::wstring value;
};

struct FileIntegrityResult {
    bool success = false;
    std::wstring algorithm = L"SHA-256";
    std::wstring digest;
    std::wstring message;
};

struct AuditReport {
    std::string product = "SIXERC";
    std::string version = "2.0.0";
    std::string author = "Anonymous-beta";
    std::vector<AuditFinding> findings;
};

} // namespace sixerc
