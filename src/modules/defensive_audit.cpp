#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "defensive_audit.hpp"

#include <windows.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <shlobj.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <vector>

namespace sixerc::audit {
namespace {

// ------------------------------------------------------------
// BROWSER PROFILE DISCOVERY
// ------------------------------------------------------------

void add_browser_profiles(
    const std::wstring& browser,
    const std::filesystem::path& root,
    std::vector<BrowserProfile>& output
) {
    std::error_code error;

    if (!std::filesystem::exists(root, error) || error) {
        return;
    }

    std::filesystem::directory_iterator iterator(
        root,
        std::filesystem::directory_options::skip_permission_denied,
        error
    );

    const std::filesystem::directory_iterator end;

    while (!error && iterator != end) {
        const auto entry = *iterator;

        std::error_code entry_error;

        if (entry.is_directory(entry_error) && !entry_error) {
            const std::wstring name =
                entry.path().filename().wstring();

            const bool is_profile =
                name == L"Default" ||
                name == L"Guest Profile" ||
                name.rfind(L"Profile ", 0) == 0;

            if (is_profile) {
                output.push_back({
                    browser,
                    name,
                    entry.path().wstring(),
                    true
                });
            }
        }

        iterator.increment(error);
    }
}

// ------------------------------------------------------------
// READ-ONLY STARTUP REGISTRY INSPECTION
// ------------------------------------------------------------

void read_run_key(
    HKEY root,
    const wchar_t* key_path,
    const wchar_t* source_label,
    std::vector<StartupEntry>& output
) {
    HKEY key = nullptr;

    const LONG open_result = RegOpenKeyExW(
        root,
        key_path,
        0,
        KEY_READ,
        &key
    );

    if (open_result != ERROR_SUCCESS) {
        return;
    }

    for (DWORD index = 0; index < 512; ++index) {
        wchar_t name[512]{};

        DWORD name_length =
            static_cast<DWORD>(std::size(name));

        DWORD type = 0;
        DWORD byte_count = 0;

        const LONG enum_result = RegEnumValueW(
            key,
            index,
            name,
            &name_length,
            nullptr,
            &type,
            nullptr,
            &byte_count
        );

        if (enum_result == ERROR_NO_MORE_ITEMS) {
            break;
        }

        if (enum_result != ERROR_SUCCESS) {
            continue;
        }

        if (type != REG_SZ && type != REG_EXPAND_SZ) {
            continue;
        }

        // Avoid unbounded allocations from malformed registry values.
        if (byte_count > 65536) {
            continue;
        }

        std::vector<BYTE> data(
            static_cast<std::size_t>(byte_count) +
            sizeof(wchar_t),
            0
        );

        DWORD actual_size =
            static_cast<DWORD>(data.size());

        const LONG read_result = RegQueryValueExW(
            key,
            name,
            nullptr,
            &type,
            data.data(),
            &actual_size
        );

        if (read_result != ERROR_SUCCESS) {
            continue;
        }

        const auto* value_data =
            reinterpret_cast<const wchar_t*>(data.data());

        std::wstring value(value_data);

        if (type == REG_EXPAND_SZ) {
            const DWORD needed =
                ExpandEnvironmentStringsW(
                    value.c_str(),
                    nullptr,
                    0
                );

            if (needed > 0 && needed < 32768) {
                std::vector<wchar_t> expanded(
                    static_cast<std::size_t>(needed) + 1,
                    L'\0'
                );

                if (ExpandEnvironmentStringsW(
                        value.c_str(),
                        expanded.data(),
                        needed
                    ) > 0) {
                    value.assign(expanded.data());
                }
            }
        }

        output.push_back({
            source_label,
            name,
            value
        });
    }

    RegCloseKey(key);
}

} // namespace

// ------------------------------------------------------------
// SYSTEM INTELLIGENCE
// ------------------------------------------------------------

SystemFacts collect_system_facts() {
    SystemFacts result;

    wchar_t computer_name[256]{};
    DWORD computer_name_length =
        static_cast<DWORD>(std::size(computer_name));

    if (GetComputerNameW(
            computer_name,
            &computer_name_length
        )) {
        result.computer_name = computer_name;
    }

    using GetVersionFunction =
        LONG(WINAPI*)(PRTL_OSVERSIONINFOW);

    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");

    auto get_version = ntdll
        ? reinterpret_cast<GetVersionFunction>(
            GetProcAddress(ntdll, "RtlGetVersion")
        )
        : nullptr;

    if (get_version != nullptr) {
        RTL_OSVERSIONINFOW version{};
        version.dwOSVersionInfoSize = sizeof(version);

        if (get_version(&version) == 0) {
            result.windows_version =
                L"Windows " +
                std::to_wstring(version.dwMajorVersion) +
                L"." +
                std::to_wstring(version.dwMinorVersion) +
                L" build " +
                std::to_wstring(version.dwBuildNumber);
        }
    }

    SYSTEM_INFO system_info{};
    GetNativeSystemInfo(&system_info);

    switch (system_info.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            result.architecture = L"x64";
            break;

        case PROCESSOR_ARCHITECTURE_ARM64:
            result.architecture = L"ARM64";
            break;

        case PROCESSOR_ARCHITECTURE_INTEL:
            result.architecture = L"x86";
            break;

        default:
            result.architecture = L"Unknown";
            break;
    }

    result.uptime_seconds =
        static_cast<std::uint64_t>(
            GetTickCount64() / 1000ULL
        );

    return result;
}

// ------------------------------------------------------------
// PROCESS INSPECTOR
// ------------------------------------------------------------

std::vector<ProcessRecord> list_processes() {
    std::vector<ProcessRecord> output;

    HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPPROCESS,
        0
    );

    if (snapshot == INVALID_HANDLE_VALUE) {
        return output;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snapshot, &entry)) {
        do {
            output.push_back({
                entry.th32ProcessID,
                entry.szExeFile
            });
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);

    std::sort(
        output.begin(),
        output.end(),
        [](const ProcessRecord& first,
           const ProcessRecord& second) {
            return first.image_name < second.image_name;
        }
    );

    return output;
}

// ------------------------------------------------------------
// BROWSER PROFILE INVENTORY
// ------------------------------------------------------------

std::vector<BrowserProfile> inventory_browser_profiles() {
    std::vector<BrowserProfile> output;

    PWSTR known_folder = nullptr;

    HRESULT result = SHGetKnownFolderPath(
        FOLDERID_LocalAppData,
        0,
        nullptr,
        &known_folder
    );

    if (SUCCEEDED(result) && known_folder != nullptr) {
        const std::filesystem::path local_root(
            known_folder
        );

        CoTaskMemFree(known_folder);

        add_browser_profiles(
            L"Chrome",
            local_root /
                L"Google" /
                L"Chrome" /
                L"User Data",
            output
        );

        add_browser_profiles(
            L"Edge",
            local_root /
                L"Microsoft" /
                L"Edge" /
                L"User Data",
            output
        );

        add_browser_profiles(
            L"Brave",
            local_root /
                L"BraveSoftware" /
                L"Brave-Browser" /
                L"User Data",
            output
        );
    }

    known_folder = nullptr;

    result = SHGetKnownFolderPath(
        FOLDERID_RoamingAppData,
        0,
        nullptr,
        &known_folder
    );

    if (SUCCEEDED(result) && known_folder != nullptr) {
        const std::filesystem::path firefox_root =
            std::filesystem::path(known_folder) /
            L"Mozilla" /
            L"Firefox" /
            L"Profiles";

        CoTaskMemFree(known_folder);

        std::error_code error;

        if (std::filesystem::exists(
                firefox_root,
                error
            ) && !error) {

            std::filesystem::directory_iterator iterator(
                firefox_root,
                std::filesystem::directory_options::
                    skip_permission_denied,
                error
            );

            const std::filesystem::directory_iterator end;

            while (!error && iterator != end) {
                const auto entry = *iterator;
                std::error_code entry_error;

                if (entry.is_directory(entry_error) &&
                    !entry_error) {
                    output.push_back({
                        L"Firefox",
                        entry.path().filename().wstring(),
                        entry.path().wstring(),
                        true
                    });
                }

                iterator.increment(error);
            }
        }
    }

    return output;
}

// ------------------------------------------------------------
// STARTUP AUDIT
// ------------------------------------------------------------

std::vector<StartupEntry> audit_startup_entries() {
    std::vector<StartupEntry> output;

    read_run_key(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        L"HKCU Run",
        output
    );

    read_run_key(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce",
        L"HKCU RunOnce",
        output
    );

    read_run_key(
        HKEY_LOCAL_MACHINE,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        L"HKLM Run",
        output
    );

    read_run_key(
        HKEY_LOCAL_MACHINE,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce",
        L"HKLM RunOnce",
        output
    );

    PWSTR startup_folder = nullptr;

    const HRESULT folder_result =
        SHGetKnownFolderPath(
            FOLDERID_Startup,
            0,
            nullptr,
            &startup_folder
        );

    if (SUCCEEDED(folder_result) &&
        startup_folder != nullptr) {

        const std::filesystem::path root(
            startup_folder
        );

        CoTaskMemFree(startup_folder);

        std::error_code error;

        if (std::filesystem::exists(root, error) &&
            !error) {

            std::filesystem::directory_iterator iterator(
                root,
                std::filesystem::directory_options::
                    skip_permission_denied,
                error
            );

            const std::filesystem::directory_iterator end;

            while (!error && iterator != end) {
                const auto entry = *iterator;

                output.push_back({
                    L"User Startup folder",
                    entry.path().filename().wstring(),
                    entry.path().wstring()
                });

                iterator.increment(error);
            }
        }
    }

    return output;
}

// ------------------------------------------------------------
// FILE INTEGRITY — SHA-256
// ------------------------------------------------------------

AuditResult sha256_file(
    const std::wstring& path,
    std::wstring& hex_digest
) {
    hex_digest.clear();

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;

    DWORD object_length = 0;
    DWORD digest_length = 0;
    DWORD bytes_written = 0;

    NTSTATUS status = BCryptOpenAlgorithmProvider(
        &algorithm,
        BCRYPT_SHA256_ALGORITHM,
        nullptr,
        0
    );

    if (status < 0) {
        return {
            false,
            L"Could not initialize the SHA-256 provider."
        };
    }

    status = BCryptGetProperty(
        algorithm,
        BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&object_length),
        sizeof(object_length),
        &bytes_written,
        0
    );

    if (status < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);

        return {
            false,
            L"Could not determine the hash object size."
        };
    }

    status = BCryptGetProperty(
        algorithm,
        BCRYPT_HASH_LENGTH,
        reinterpret_cast<PUCHAR>(&digest_length),
        sizeof(digest_length),
        &bytes_written,
        0
    );

    if (status < 0 ||
        digest_length == 0 ||
        digest_length > 128) {

        BCryptCloseAlgorithmProvider(algorithm, 0);

        return {
            false,
            L"Could not determine the SHA-256 digest size."
        };
    }

    std::vector<BYTE> hash_object(object_length);
    std::vector<BYTE> digest(digest_length);

    status = BCryptCreateHash(
        algorithm,
        &hash,
        hash_object.data(),
        object_length,
        nullptr,
        0,
        0
    );

    if (status < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);

        return {
            false,
            L"Could not create the hash context."
        };
    }

    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ |
            FILE_SHARE_WRITE |
            FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL |
            FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr
    );

    if (file == INVALID_HANDLE_VALUE) {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);

        return {
            false,
            L"Could not open the file. Check its path and permissions."
        };
    }

    std::array<BYTE, 65536> buffer{};
    bool read_successfully = true;

    for (;;) {
        DWORD bytes_read = 0;

        if (!ReadFile(
                file,
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                &bytes_read,
                nullptr
            )) {
            read_successfully = false;
            break;
        }

        if (bytes_read == 0) {
            break;
        }

        status = BCryptHashData(
            hash,
            buffer.data(),
            bytes_read,
            0
        );

        if (status < 0) {
            read_successfully = false;
            break;
        }
    }

    CloseHandle(file);

    if (read_successfully) {
        status = BCryptFinishHash(
            hash,
            digest.data(),
            digest_length,
            0
        );
    }

    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);

    if (!read_successfully || status < 0) {
        return {
            false,
            L"Reading or hashing the file failed."
        };
    }

    constexpr wchar_t digits[] =
        L"0123456789abcdef";

    hex_digest.reserve(digest_length * 2);

    for (const BYTE value : digest) {
        hex_digest.push_back(
            digits[(value >> 4) & 0x0F]
        );

        hex_digest.push_back(
            digits[value & 0x0F]
        );
    }

    return {
        true,
        L"SHA-256 calculated successfully."
    };
}

} // namespace sixerc::audit