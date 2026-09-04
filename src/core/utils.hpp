// ============================================================================
// SIXERC — Utility Functions
// ============================================================================

#pragma once

#include "types.hpp"
#include <string>
#include <vector>
#include <windows.h>
#include <shlobj.h>

namespace sixerc::utils {

// String conversions
std::string wstring_to_utf8(const std::wstring& wstr);
std::wstring utf8_to_wstring(const std::string& str);
std::string wide_to_ansi(const std::wstring& wstr);

// Path utilities
std::wstring get_local_appdata_path();
std::wstring get_temp_path();
bool file_exists(const std::wstring& path);
bool directory_exists(const std::wstring& path);
std::vector<std::wstring> list_directories(const std::wstring& path);
bool delete_file_secure(const std::wstring& path);
bool wipe_file(const std::wstring& path);

// Process utilities
std::vector<DWORD> enumerate_processes();
std::wstring get_process_name(DWORD pid);
bool is_process_running(const std::wstring& name);

// Registry utilities
bool registry_write(HKEY root, const std::wstring& subkey, const std::wstring& value_name, 
                    const std::wstring& data);
bool registry_delete(HKEY root, const std::wstring& subkey);
bool registry_read(HKEY root, const std::wstring& subkey, const std::wstring& value_name,
                   std::wstring& out_data);

// Time utilities
uint64_t chrome_time_to_unix(uint64_t chrome_time);
void random_sleep(uint32_t min_ms, uint32_t max_ms);

// Memory utilities
void* allocate_encrypted_buffer(size_t size);
void free_encrypted_buffer(void* ptr, size_t size);

} // namespace sixerc::utils
