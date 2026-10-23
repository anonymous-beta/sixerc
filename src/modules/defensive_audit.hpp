#pragma once

#ifndef SIXERC_DEFENSIVE_AUDIT_HPP
#define SIXERC_DEFENSIVE_AUDIT_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace sixerc::audit {

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

struct BrowserProfile {
std::wstring browser;
std::wstring profile_name;
std::wstring path;
bool exists = false;
};

struct StartupEntry {
std::wstring source;
std::wstring name;
std::wstring value;
};

struct AuditResult {
bool ok = false;
std::wstring message;
};

SystemFacts collect_system_facts();

std::vector<ProcessRecord> list_processes();

std::vector<BrowserProfile> inventory_browser_profiles();

std::vector<StartupEntry> audit_startup_entries();

AuditResult sha256_file(
const std::wstring& path,
std::wstring& hex_digest
);

} // namespace sixerc::audit

#endif // SIXERC_DEFENSIVE_AUDIT_HPP