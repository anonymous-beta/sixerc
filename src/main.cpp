
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>

#include "modules/defensive_audit.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr wchar_t WINDOW_CLASS[] = L"SIXERC_EKWENSU";
constexpr wchar_t WINDOW_TITLE[] = L"SIXERC | EKWENSU'S GRASP";

constexpr COLORREF BG = RGB(12, 10, 13);
constexpr COLORREF PANEL = RGB(25, 20, 26);
constexpr COLORREF PANEL2 = RGB(35, 25, 32);
constexpr COLORREF BORDER = RGB(74, 39, 48);
constexpr COLORREF RED = RGB(190, 35, 54);
constexpr COLORREF RED_HOT = RGB(235, 48, 65);
constexpr COLORREF GOLD = RGB(209, 155, 74);
constexpr COLORREF GREEN = RGB(95, 190, 125);
constexpr COLORREF WHITE = RGB(239, 231, 234);
constexpr COLORREF MUTED = RGB(153, 139, 148);

enum class Action {
    System,
    Processes,
    Startup,
    Hash,
    Export,
    Refresh
};

struct Button {
    RECT rect;
    const wchar_t* label;
    Action action;
};

HWND g_window = nullptr;
HFONT g_regular = nullptr;
HFONT g_bold = nullptr;
HFONT g_small = nullptr;

std::vector<std::wstring> g_output;
std::vector<std::wstring> g_report;
std::wstring g_status = L"AWAITING COMMAND";
std::wstring g_last_file;

std::array<Button, 6> g_buttons{{
    {{34, 205, 270, 254}, L"SYSTEM INTELLIGENCE", Action::System},
    {{34, 270, 270, 319}, L"PROCESS INVENTORY", Action::Processes},
    {{34, 335, 270, 384}, L"STARTUP INSPECTION", Action::Startup},
    {{34, 400, 270, 449}, L"FILE INTEGRITY", Action::Hash},
    {{34, 465, 270, 514}, L"EXPORT AUDIT REPORT", Action::Export},
    {{34, 530, 270, 579}, L"REFRESH CONSOLE", Action::Refresh}
}};

std::wstring timestamp() {
    SYSTEMTIME time{};
    GetLocalTime(&time);

    wchar_t value[32]{};

    swprintf_s(
        value,
        L"%02u:%02u:%02u",
        time.wHour,
        time.wMinute,
        time.wSecond
    );

    return value;
}

void log_line(const std::wstring& value) {
    const std::wstring line =
        L"[" + timestamp() + L"] " + value;

    g_output.insert(g_output.begin(), line);
    g_report.push_back(line);

    if (g_output.size() > 100) {
        g_output.resize(100);
    }

    g_status = value;

    if (g_window) {
        InvalidateRect(g_window, nullptr, FALSE);
    }
}

void fill(HDC dc, RECT rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void line(
    HDC dc,
    int x1,
    int y1,
    int x2,
    int y2,
    COLORREF color
) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ old = SelectObject(dc, pen);

    MoveToEx(dc, x1, y1, nullptr);
    LineTo(dc, x2, y2);

    SelectObject(dc, old);
    DeleteObject(pen);
}

void text(
    HDC dc,
    const std::wstring& value,
    int x,
    int y,
    COLORREF color,
    HFONT font
) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);

    HGDIOBJ old = SelectObject(dc, font);

    TextOutW(
        dc,
        x,
        y,
        value.c_str(),
        static_cast<int>(value.size())
    );

    SelectObject(dc, old);
}

void centered(
    HDC dc,
    const std::wstring& value,
    RECT rect,
    COLORREF color,
    HFONT font
) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);

    HGDIOBJ old = SelectObject(dc, font);

    DrawTextW(
        dc,
        value.c_str(),
        static_cast<int>(value.size()),
        &rect,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE
    );

    SelectObject(dc, old);
}

void panel(HDC dc, RECT rect) {
    fill(dc, rect, PANEL);

    HPEN pen = CreatePen(PS_SOLID, 1, BORDER);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush =
        SelectObject(dc, GetStockObject(HOLLOW_BRUSH));

    Rectangle(
        dc,
        rect.left,
        rect.top,
        rect.right,
        rect.bottom
    );

    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(pen);
}

void draw_button(HDC dc, const Button& button) {
    fill(dc, button.rect, PANEL2);

    HPEN pen = CreatePen(PS_SOLID, 1, RED);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush =
        SelectObject(dc, GetStockObject(HOLLOW_BRUSH));

    Rectangle(
        dc,
        button.rect.left,
        button.rect.top,
        button.rect.right,
        button.rect.bottom
    );

    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(pen);

    RECT label = button.rect;

    centered(
        dc,
        button.label,
        label,
        WHITE,
        g_bold
    );
}

std::wstring make_report() {
    std::wstringstream out;

    out << L"SIXERC - EKWENSU'S GRASP\n";
    out << L"LOCAL SECURITY AUDIT REPORT\n";
    out << L"===========================\n\n";

    out << L"Generated: " << timestamp() << L"\n";
    out << L"Status: " << g_status << L"\n\n";

    out << L"ACTIVITY\n";
    out << L"--------\n";

    for (const auto& entry : g_report) {
        out << entry << L"\n";
    }

    out << L"\n";
    out << L"Privacy: This report was generated locally.\n";
    out << L"No browser credentials or authentication sessions "
           L"were collected.\n";

    return out.str();
}

void run_system_audit() {
    const auto facts =
        sixerc::audit::collect_system_facts();

    log_line(L"SYSTEM INTELLIGENCE");

    log_line(
        L"Computer: " + facts.computer_name
    );

    log_line(
        L"Operating system: " + facts.windows_version
    );

    log_line(
        L"Architecture: " + facts.architecture
    );

    log_line(
        L"Uptime: " +
        std::to_wstring(facts.uptime_seconds) +
        L" seconds"
    );
}

void run_process_audit() {
    const auto processes =
        sixerc::audit::list_processes();

    log_line(
        L"PROCESS INVENTORY: " +
        std::to_wstring(processes.size()) +
        L" processes found"
    );

    for (const auto& process : processes) {
        log_line(
            L"PID " +
            std::to_wstring(process.pid) +
            L" | " +
            process.image_name
        );
    }
}

void run_startup_audit() {
    const auto entries =
        sixerc::audit::audit_startup_entries();

    log_line(
        L"STARTUP INSPECTION: " +
        std::to_wstring(entries.size()) +
        L" entries found"
    );

    for (const auto& entry : entries) {
        log_line(
            entry.source +
            L" | " +
            entry.name +
            L" | " +
            entry.value
        );
    }
}

void run_file_hash(HWND owner) {
    wchar_t path[MAX_PATH]{};

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFile = path;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrFilter =
        L"All files (*.*)\0*.*\0";
    dialog.Flags =
        OFN_FILEMUSTEXIST |
        OFN_PATHMUSTEXIST |
        OFN_HIDEREADONLY;

    if (!GetOpenFileNameW(&dialog)) {
        return;
    }

    std::wstring digest;

    const auto result =
        sixerc::audit::sha256_file(path, digest);

    if (!result.ok) {
        log_line(L"FILE HASH FAILED: " + result.message);
        return;
    }

    g_last_file = path;

    log_line(L"SHA-256 FILE INTEGRITY CHECK");
    log_line(L"File: " + std::wstring(path));
    log_line(L"Digest: " + digest);
}

void export_report(HWND owner) {
    wchar_t path[MAX_PATH] = L"SIXERC-audit-report.txt";

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFile = path;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrFilter =
        L"Text report (*.txt)\0*.txt\0";
    dialog.lpstrDefExt = L"txt";
    dialog.Flags =
        OFN_OVERWRITEPROMPT |
        OFN_PATHMUSTEXIST;

    if (!GetSaveFileNameW(&dialog)) {
        return;
    }

    std::wofstream file(
        std::filesystem::path(path),
        std::ios::out | std::ios::trunc
    );

    if (!file) {
        MessageBoxW(
            owner,
            L"Could not write the report.",
            L"SIXERC",
            MB_OK | MB_ICONERROR
        );

        log_line(L"REPORT EXPORT FAILED");
        return;
    }

    file << make_report();
    file.close();

    if (!file) {
        log_line(L"REPORT EXPORT FAILED");
        return;
    }

    log_line(L"REPORT EXPORTED SUCCESSFULLY");

    MessageBoxW(
        owner,
        L"Audit report exported successfully.",
        L"SIXERC",
        MB_OK | MB_ICONINFORMATION
    );
}

void execute(Action action, HWND owner) {
    switch (action) {
    case Action::System:
        run_system_audit();
        break;

    case Action::Processes:
        run_process_audit();
        break;

    case Action::Startup:
        run_startup_audit();
        break;

    case Action::Hash:
        run_file_hash(owner);
        break;

    case Action::Export:
        export_report(owner);
        break;

    case Action::Refresh:
        log_line(L"CONSOLE REFRESHED");
        break;
    }

    InvalidateRect(owner, nullptr, FALSE);
}

void paint(HWND hwnd) {
    PAINTSTRUCT ps{};
    HDC dc = BeginPaint(hwnd, &ps);

    RECT client{};
    GetClientRect(hwnd, &client);

    fill(dc, client, BG);

    text(
        dc,
        L"SIXERC",
        34, 28,
        RED_HOT,
        g_bold
    );

    text(
        dc,
        L"E K W E N S U ' S   G R A S P",
        36, 60,
        GOLD,
        g_small
    );

    text(
        dc,
        L"NATIVE SECURITY OPERATIONS CONSOLE",
        36, 91,
        MUTED,
        g_small
    );

    line(dc, 34, 124, client.right - 34, 124, BORDER);

    RECT status_rect{
        34, 145, client.right - 34, 184
    };

    panel(dc, status_rect);

    text(dc, L"STATUS", 49, 157, GOLD, g_small);
    text(dc, g_status, 125, 157, GREEN, g_bold);

    RECT arsenal{
        24, 190, 290, client.bottom - 24
    };

    RECT activity{
        305, 190, client.right - 24, client.bottom - 24
    };

    panel(dc, arsenal);
    panel(dc, activity);

    text(dc, L"THE ARSENAL", 42, 174, RED_HOT, g_bold);
    text(dc, L"FIELD OPERATIONS", 325, 174, RED_HOT, g_bold);

    for (const auto& button : g_buttons) {
        draw_button(dc, button);
    }

    text(
        dc,
        L"LOCAL AUDIT OUTPUT",
        325, 210,
        GOLD,
        g_small
    );

    line(
        dc,
        325, 235,
        client.right - 44, 235,
        BORDER
    );

    int y = 252;

    const int max_lines =
        std::max(0, (client.bottom - y - 45) / 23);

    const int count = std::min<int>(
        max_lines,
        static_cast<int>(g_output.size())
    );

    for (int i = 0; i < count; ++i) {
        text(
            dc,
            g_output[static_cast<std::size_t>(i)],
            325,
            y,
            i == 0 ? WHITE : MUTED,
            g_small
        );

        y += 23;
    }

    if (count == 0) {
        text(
            dc,
            L"No operations performed. Select a module.",
            325, 260,
            MUTED,
            g_regular
        );
    }

    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK window_proc(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam
) {
    switch (message) {
    case WM_PAINT:
        paint(hwnd);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_SIZE:
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_LBUTTONUP: {
        const int x = GET_X_LPARAM(lparam);
        const int y = GET_Y_LPARAM(lparam);

        for (const auto& button : g_buttons) {
            if (x >= button.rect.left &&
                x <= button.rect.right &&
                y >= button.rect.top &&
                y <= button.rect.bottom) {
                execute(button.action, hwnd);
                break;
            }
        }

        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(
            hwnd,
            message,
            wparam,
            lparam
        );
    }
}

void create_fonts() {
    g_regular = CreateFontW(
        15, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH,
        L"Segoe UI"
    );

    g_bold = CreateFontW(
        18, 0, 0, 0, FW_BOLD,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH,
        L"Segoe UI"
    );

    g_small = CreateFontW(
        12, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH,
        L"Consolas"
    );
}

void destroy_fonts() {
    if (g_regular) DeleteObject(g_regular);
    if (g_bold) DeleteObject(g_bold);
    if (g_small) DeleteObject(g_small);
}

} // namespace

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int show_command
) {
    SetProcessDPIAware();
    create_fonts();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(BG);
    wc.lpszClassName = WINDOW_CLASS;

    if (!RegisterClassExW(&wc)) {
        destroy_fonts();
        return 1;
    }

    HWND hwnd = CreateWindowExW(
        0,
        WINDOW_CLASS,
        WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1120,
        760,
        nullptr,
        nullptr,
        instance,
        nullptr
    );

    if (!hwnd) {
        DeleteObject(wc.hbrBackground);
        UnregisterClassW(WINDOW_CLASS, instance);
        destroy_fonts();
        return 1;
    }

    g_window = hwnd;

    log_line(L"EKWENSU CONSOLE INITIALIZED");
    log_line(L"LOCAL AUDIT MODE ACTIVE");

    ShowWindow(hwnd, show_command);
    UpdateWindow(hwnd);

    MSG message{};

    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    destroy_fonts();
    DeleteObject(wc.hbrBackground);
    UnregisterClassW(WINDOW_CLASS, instance);

    return static_cast<int>(message.wParam);
}
