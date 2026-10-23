#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#ifndef SIXERC_VERSION
#define SIXERC_VERSION "1.1.0"
#endif

#ifndef SIXERC_AUTHOR
#define SIXERC_AUTHOR "Anonymous-beta (Chinedu)"
#endif

#ifndef SIXERC_CODENAME
#define SIXERC_CODENAME "Ekwensu's Grasp"
#endif

namespace {

constexpr wchar_t APP_CLASS[] = L"SIXERCNativeDashboard";
constexpr wchar_t APP_TITLE[] = L"SIXERC";
constexpr int WINDOW_MIN_WIDTH = 900;
constexpr int WINDOW_MIN_HEIGHT = 620;

constexpr COLORREF COLOR_BACKGROUND = RGB(15, 17, 21);
constexpr COLORREF COLOR_PANEL = RGB(22, 25, 31);
constexpr COLORREF COLOR_PANEL_ALT = RGB(28, 32, 39);
constexpr COLORREF COLOR_BORDER = RGB(49, 54, 64);
constexpr COLORREF COLOR_TEXT = RGB(237, 239, 243);
constexpr COLORREF COLOR_MUTED = RGB(151, 159, 172);
constexpr COLORREF COLOR_ACCENT = RGB(194, 42, 54);
constexpr COLORREF COLOR_ACCENT_HOVER = RGB(221, 52, 65);
constexpr COLORREF COLOR_SUCCESS = RGB(91, 190, 135);
constexpr COLORREF COLOR_WARNING = RGB(235, 176, 77);
constexpr COLORREF COLOR_BLUE = RGB(112, 165, 230);

enum class Page {
    Dashboard,
    Modules,
    Activity,
    Configuration,
    Diagnostics,
    About
};

struct ActivityEntry {
    std::wstring time;
    std::wstring category;
    std::wstring message;
};

struct DiagnosticResult {
    std::wstring name;
    std::wstring detail;
    bool passed = false;
};

struct AppState {
    Page page = Page::Dashboard;

    std::vector<ActivityEntry> activity;
    std::vector<DiagnosticResult> diagnostics;

    std::wstring status = L"Ready";
    std::wstring lastAction = L"Application started";

    int sidebarWidth = 218;
    int topbarHeight = 74;

    bool diagnosticsRun = false;
    bool hoverExport = false;
    bool hoverRefresh = false;
};

AppState g_state;

HFONT g_fontRegular = nullptr;
HFONT g_fontMedium = nullptr;
HFONT g_fontBold = nullptr;
HFONT g_fontSmall = nullptr;

HWND g_window = nullptr;

struct Rect {
    int x;
    int y;
    int width;
    int height;

    RECT native() const {
        return RECT{x, y, x + width, y + height};
    }

    bool contains(int px, int py) const {
        return px >= x && py >= y &&
               px < x + width && py < y + height;
    }
};

std::wstring widen(const std::string& input) {
    if (input.empty()) {
        return {};
    }

    const int required = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        input.data(),
        static_cast<int>(input.size()),
        nullptr,
        0
    );

    if (required <= 0) {
        return L"";
    }

    std::wstring output(static_cast<size_t>(required), L'\0');

    MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        input.data(),
        static_cast<int>(input.size()),
        output.data(),
        required
    );

    return output;
}

std::wstring local_time_string() {
    SYSTEMTIME st{};
    GetLocalTime(&st);

    wchar_t buffer[64]{};

    swprintf_s(
        buffer,
        L"%02u:%02u:%02u",
        st.wHour,
        st.wMinute,
        st.wSecond
    );

    return buffer;
}

void log_activity(
    const std::wstring& category,
    const std::wstring& message
) {
    g_state.activity.insert(
        g_state.activity.begin(),
        ActivityEntry{
            local_time_string(),
            category,
            message
        }
    );

    if (g_state.activity.size() > 200) {
        g_state.activity.resize(200);
    }

    g_state.lastAction = message;
}

void set_status(const std::wstring& value) {
    g_state.status = value;

    if (g_window) {
        InvalidateRect(g_window, nullptr, FALSE);
    }
}

void fill_rect(
    HDC dc,
    const Rect& rect,
    COLORREF color
) {
    HBRUSH brush = CreateSolidBrush(color);
    RECT r = rect.native();

    FillRect(dc, &r, brush);

    DeleteObject(brush);
}

void draw_border(
    HDC dc,
    const Rect& rect,
    COLORREF color
) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));

    Rectangle(
        dc,
        rect.x,
        rect.y,
        rect.x + rect.width,
        rect.y + rect.height
    );

    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);

    DeleteObject(pen);
}

void draw_text(
    HDC dc,
    const std::wstring& text,
    int x,
    int y,
    int width,
    int height,
    COLORREF color,
    HFONT font,
    UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE
) {
    RECT rect{x, y, x + width, y + height};

    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);

    HGDIOBJ oldFont = SelectObject(dc, font);

    DrawTextW(
        dc,
        text.c_str(),
        static_cast<int>(text.size()),
        &rect,
        format
    );

    SelectObject(dc, oldFont);
}

void draw_line(
    HDC dc,
    int x1,
    int y1,
    int x2,
    int y2,
    COLORREF color
) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldPen = SelectObject(dc, pen);

    MoveToEx(dc, x1, y1, nullptr);
    LineTo(dc, x2, y2);

    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void draw_dot(
    HDC dc,
    int x,
    int y,
    COLORREF color,
    int radius = 4
) {
    HBRUSH brush = CreateSolidBrush(color);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));

    Ellipse(
        dc,
        x - radius,
        y - radius,
        x + radius,
        y + radius
    );

    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);

    DeleteObject(brush);
}

void draw_card(
    HDC dc,
    const Rect& rect,
    const std::wstring& title,
    const std::wstring& value,
    const std::wstring& subtitle,
    COLORREF accent
) {
    fill_rect(dc, rect, COLOR_PANEL);
    draw_border(dc, rect, COLOR_BORDER);

    fill_rect(
        dc,
        Rect{rect.x, rect.y, 3, rect.height},
        accent
    );

    draw_text(
        dc,
        title,
        rect.x + 18,
        rect.y + 14,
        rect.width - 34,
        22,
        COLOR_MUTED,
        g_fontSmall
    );

    draw_text(
        dc,
        value,
        rect.x + 18,
        rect.y + 43,
        rect.width - 34,
        34,
        COLOR_TEXT,
        g_fontBold
    );

    draw_text(
        dc,
        subtitle,
        rect.x + 18,
        rect.y + 82,
        rect.width - 34,
        24,
        COLOR_MUTED,
        g_fontSmall
    );
}

void draw_section_heading(
    HDC dc,
    const std::wstring& title,
    const std::wstring& subtitle,
    int x,
    int y,
    int width
) {
    draw_text(
        dc,
        title,
        x,
        y,
        width,
        32,
        COLOR_TEXT,
        g_fontBold
    );

    draw_text(
        dc,
        subtitle,
        x,
        y + 35,
        width,
        24,
        COLOR_MUTED,
        g_fontSmall
    );
}

std::wstring page_title(Page page) {
    switch (page) {
        case Page::Dashboard:
            return L"Dashboard";

        case Page::Modules:
            return L"Module registry";

        case Page::Activity:
            return L"Activity log";

        case Page::Configuration:
            return L"Configuration";

        case Page::Diagnostics:
            return L"Diagnostics";

        case Page::About:
            return L"About SIXERC";
    }

    return L"SIXERC";
}

std::wstring executable_path() {
    std::vector<wchar_t> buffer(32768, L'\0');

    const DWORD length = GetModuleFileNameW(
        nullptr,
        buffer.data(),
        static_cast<DWORD>(buffer.size())
    );

    if (length == 0 || length >= buffer.size()) {
        return L"Unavailable";
    }

    return std::wstring(buffer.data(), length);
}

std::wstring windows_version_description() {
    OSVERSIONINFOEXW version{};
    version.dwOSVersionInfoSize = sizeof(version);

    /*
       Windows version reporting through the legacy version API can be
       affected by application manifests. The dashboard labels the result
       as the version reported by Windows rather than claiming that this
       is an authoritative build-identification mechanism.
    */

    using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);

    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");

    if (!ntdll) {
        return L"Windows version information unavailable";
    }

    auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(
        GetProcAddress(ntdll, "RtlGetVersion")
    );

    if (!rtlGetVersion) {
        return L"Windows version information unavailable";
    }

    RTL_OSVERSIONINFOW info{};
    info.dwOSVersionInfoSize = sizeof(info);

    if (rtlGetVersion(&info) != 0) {
        return L"Windows version information unavailable";
    }

    wchar_t buffer[128]{};

    swprintf_s(
        buffer,
        L"Windows %lu.%lu (build %lu)",
        info.dwMajorVersion,
        info.dwMinorVersion,
        info.dwBuildNumber
    );

    return buffer;
}

bool test_directory_writable(
    const std::filesystem::path& directory
) {
    try {
        if (!std::filesystem::exists(directory) ||
            !std::filesystem::is_directory(directory)) {
            return false;
        }

        const auto file = directory /
            L"sixerc-diagnostic-test.tmp";

        {
            std::ofstream output(
                file,
                std::ios::binary | std::ios::trunc
            );

            if (!output.is_open()) {
                return false;
            }

            output << "SIXERC diagnostic write test\n";

            if (!output.good()) {
                output.close();
                std::error_code ignored;
                std::filesystem::remove(file, ignored);
                return false;
            }
        }

        std::error_code error;
        std::filesystem::remove(file, error);

        return !error;
    } catch (...) {
        return false;
    }
}

std::wstring architecture_description() {
#if defined(_M_X64) || defined(__x86_64__)
    return L"x64";
#elif defined(_M_ARM64) || defined(__aarch64__)
    return L"ARM64";
#elif defined(_M_IX86) || defined(__i386__)
    return L"x86";
#else
    return L"Unknown";
#endif
}

void run_diagnostics() {
    g_state.diagnostics.clear();

    const std::wstring exe = executable_path();

    g_state.diagnostics.push_back({
        L"Application path",
        exe,
        exe != L"Unavailable"
    });

    g_state.diagnostics.push_back({
        L"Windows version",
        windows_version_description(),
        true
    });

    g_state.diagnostics.push_back({
        L"Build architecture",
        architecture_description(),
        true
    });

    wchar_t tempBuffer[MAX_PATH]{};

    const DWORD tempLength = GetTempPathW(
        MAX_PATH,
        tempBuffer
    );

    bool tempAvailable = false;
    std::wstring tempPath = L"Unavailable";

    if (tempLength > 0 && tempLength < MAX_PATH) {
        tempPath.assign(tempBuffer);
        tempAvailable = test_directory_writable(
            std::filesystem::path(tempBuffer)
        );
    }

    g_state.diagnostics.push_back({
        L"Temporary directory",
        tempPath,
        tempAvailable
    });

    wchar_t localAppData[MAX_PATH]{};

    const HRESULT folderResult = SHGetFolderPathW(
        nullptr,
        CSIDL_LOCAL_APPDATA,
        nullptr,
        SHGFP_TYPE_CURRENT,
        localAppData
    );

    bool appDataAvailable = false;
    std::wstring appDataPath = L"Unavailable";

    if (SUCCEEDED(folderResult)) {
        std::filesystem::path appDirectory =
            std::filesystem::path(localAppData) /
            L"Anonymous-beta" /
            L"SIXERC";

        std::error_code error;

        std::filesystem::create_directories(
            appDirectory,
            error
        );

        if (!error) {
            appDataAvailable = test_directory_writable(
                appDirectory
            );

            appDataPath = appDirectory.wstring();
        } else {
            appDataPath = L"Could not create application data directory";
        }
    }

    g_state.diagnostics.push_back({
        L"Application data directory",
        appDataPath,
        appDataAvailable
    });

    g_state.diagnostics.push_back({
        L"Legacy operational modules",
        L"Disconnected from the dashboard",
        true
    });

    g_state.diagnosticsRun = true;

    log_activity(
        L"DIAGNOSTICS",
        L"Local system diagnostics completed"
    );

    set_status(L"Diagnostics complete");
}

std::wstring diagnostic_report() {
    std::wstringstream report;

    report << L"SIXERC Diagnostic Report\n";
    report << L"========================\n\n";

    report << L"Product: SIXERC\n";
    report << L"Version: " << SIXERC_VERSION << L"\n";
    report << L"Codename: " << SIXERC_CODENAME << L"\n";
    report << L"Creator: " << SIXERC_AUTHOR << L"\n";
    report << L"Generated: " << local_time_string() << L"\n\n";

    report << L"Application status\n";
    report << L"------------------\n";
    report << L"Dashboard: running\n";
    report << L"Legacy operational modules: disconnected\n\n";

    report << L"Diagnostics\n";
    report << L"-----------\n";

    for (const auto& item : g_state.diagnostics) {
        report << L"[" << (item.passed ? L"PASS" : L"CHECK")
               << L"] " << item.name << L"\n";

        report << L"      " << item.detail << L"\n\n";
    }

    report << L"Recent activity\n";
    report << L"---------------\n";

    for (const auto& item : g_state.activity) {
        report << L"[" << item.time << L"] "
               << item.category << L": "
               << item.message << L"\n";
    }

    report << L"\nPrivacy note\n";
    report << L"------------\n";
    report << L"This report contains local diagnostic information "
              L"and activity messages.\n";

    report << L"No browser cookies, session tokens, or personal "
              L"documents are collected by this dashboard.\n";

    return report.str();
}

bool export_report(HWND owner) {
    if (!g_state.diagnosticsRun) {
        run_diagnostics();
    }

    wchar_t filename[MAX_PATH] = L"SIXERC-diagnostics.txt";

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter =
        L"Text report (*.txt)\0*.txt\0All files (*.*)\0*.*\0";
    dialog.lpstrFile = filename;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrDefExt = L"txt";
    dialog.Flags =
        OFN_OVERWRITEPROMPT |
        OFN_PATHMUSTEXIST |
        OFN_NOCHANGEDIR;

    if (!GetSaveFileNameW(&dialog)) {
        return false;
    }

    std::wofstream output(
        filename,
        std::ios::out | std::ios::trunc
    );

    if (!output.is_open()) {
        MessageBoxW(
            owner,
            L"SIXERC could not write the selected report file.",
            L"Export failed",
            MB_OK | MB_ICONERROR
        );

        log_activity(
            L"ERROR",
            L"Diagnostic report export failed"
        );

        return false;
    }

    output << diagnostic_report();
    output.close();

    if (!output.good()) {
        MessageBoxW(
            owner,
            L"The report could not be written completely.",
            L"Export failed",
            MB_OK | MB_ICONERROR
        );

        log_activity(
            L"ERROR",
            L"Diagnostic report write was incomplete"
        );

        return false;
    }

    log_activity(
        L"EXPORT",
        L"Diagnostic report exported successfully"
    );

    set_status(L"Report exported");

    MessageBoxW(
        owner,
        L"The diagnostic report was exported successfully.",
        L"SIXERC",
        MB_OK | MB_ICONINFORMATION
    );

    return true;
}

void create_fonts() {
    g_fontRegular = CreateFontW(
        16, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );

    g_fontMedium = CreateFontW(
        16, 0, 0, 0, FW_SEMIBOLD,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );

    g_fontBold = CreateFontW(
        23, 0, 0, 0, FW_BOLD,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );

    g_fontSmall = CreateFontW(
        13, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );
}

void destroy_fonts() {
    if (g_fontRegular) DeleteObject(g_fontRegular);
    if (g_fontMedium) DeleteObject(g_fontMedium);
    if (g_fontBold) DeleteObject(g_fontBold);
    if (g_fontSmall) DeleteObject(g_fontSmall);

    g_fontRegular = nullptr;
    g_fontMedium = nullptr;
    g_fontBold = nullptr;
    g_fontSmall = nullptr;
}

void draw_sidebar(
    HDC dc,
    int width,
    int height
) {
    const int sidebarWidth = g_state.sidebarWidth;

    fill_rect(
        dc,
        Rect{0, 0, sidebarWidth, height},
        COLOR_PANEL
    );

    draw_line(
        dc,
        sidebarWidth - 1,
        0,
        sidebarWidth - 1,
        height,
        COLOR_BORDER
    );

    fill_rect(
        dc,
        Rect{22, 23, 39, 39},
        COLOR_ACCENT
    );

    draw_text(
        dc,
        L"S",
        22,
        23,
        39,
        39,
        COLOR_TEXT,
        g_fontBold,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE
    );

    draw_text(
        dc,
        L"SIXERC",
        73,
        22,
        125,
        26,
        COLOR_TEXT,
        g_fontBold
    );

    draw_text(
        dc,
        L"EKWENSU'S GRASP",
        74,
        48,
        128,
        17,
        COLOR_MUTED,
        g_fontSmall
    );

    draw_line(
        dc,
        18,
        84,
        sidebarWidth - 18,
        84,
        COLOR_BORDER
    );

    draw_text(
        dc,
        L"WORKSPACE",
        24,
        103,
        160,
        20,
        COLOR_MUTED,
        g_fontSmall
    );

    const std::array<std::pair<Page, const wchar_t*>, 6> items{{
        {Page::Dashboard, L"Dashboard"},
        {Page::Modules, L"Module registry"},
        {Page::Activity, L"Activity log"},
        {Page::Configuration, L"Configuration"},
        {Page::Diagnostics, L"Diagnostics"},
        {Page::About, L"About SIXERC"}
    }};

    int y = 137;

    for (const auto& item : items) {
        const bool selected = g_state.page == item.first;

        if (selected) {
            fill_rect(
                dc,
                Rect{12, y, sidebarWidth - 24, 42},
                COLOR_PANEL_ALT
            );

            fill_rect(
                dc,
                Rect{12, y, 3, 42},
                COLOR_ACCENT
            );
        }

        draw_text(
            dc,
            item.second,
            30,
            y,
            sidebarWidth - 50,
            42,
            selected ? COLOR_TEXT : COLOR_MUTED,
            selected ? g_fontMedium : g_fontRegular
        );

        y += 49;
    }

    const int footerY = std::max(180, height - 104);

    draw_line(
        dc,
        18,
        footerY,
        sidebarWidth - 18,
        footerY,
        COLOR_BORDER
    );

    draw_dot(
        dc,
        28,
        footerY + 27,
        COLOR_SUCCESS,
        4
    );

    draw_text(
        dc,
        L"LOCAL APPLICATION",
        40,
        footerY + 15,
        sidebarWidth - 55,
        24,
        COLOR_TEXT,
        g_fontSmall
    );

    draw_text(
        dc,
        L"v" SIXERC_VERSION,
        24,
        height - 37,
        sidebarWidth - 40,
        22,
        COLOR_MUTED,
        g_fontSmall
    );
}

void draw_topbar(
    HDC dc,
    int width
) {
    const int sidebar = g_state.sidebarWidth;
    const int contentWidth = width - sidebar;

    fill_rect(
        dc,
        Rect{sidebar, 0, contentWidth, g_state.topbarHeight},
        COLOR_BACKGROUND
    );

    draw_text(
        dc,
        page_title(g_state.page),
        sidebar + 30,
        15,
        contentWidth - 60,
        28,
        COLOR_TEXT,
        g_fontBold
    );

    draw_text(
        dc,
        L"Native Windows workspace",
        sidebar + 31,
        43,
        contentWidth - 60,
        20,
        COLOR_MUTED,
        g_fontSmall
    );

    draw_line(
        dc,
        sidebar,
        g_state.topbarHeight - 1,
        width,
        g_state.topbarHeight - 1,
        COLOR_BORDER
    );
}

void draw_dashboard(
    HDC dc,
    int width,
    int height
) {
    const int x = g_state.sidebarWidth + 30;
    const int contentWidth = width - x - 30;
    const int y = g_state.topbarHeight + 26;

    draw_section_heading(
        dc,
        L"Operational overview",
        L"A clear view of the application, its health, and its components.",
        x,
        y,
        contentWidth
    );

    const int cardY = y + 78;
    const int gap = 14;
    const int cardWidth = std::max(
        150,
        (contentWidth - gap * 2) / 3
    );

    draw_card(
        dc,
        Rect{x, cardY, cardWidth, 116},
        L"APPLICATION",
        L"RUNNING",
        L"Native desktop interface",
        COLOR_SUCCESS
    );

    draw_card(
        dc,
        Rect{x + cardWidth + gap, cardY, cardWidth, 116},
        L"MODULE ACCESS",
        L"RESTRICTED",
        L"Legacy operations disconnected",
        COLOR_WARNING
    );

    draw_card(
        dc,
        Rect{x + (cardWidth + gap) * 2, cardY, cardWidth, 116},
        L"DIAGNOSTICS",
        g_state.diagnosticsRun ? L"COMPLETED" : L"NOT RUN",
        g_state.diagnosticsRun
            ? L"Local checks have been executed"
            : L"Run checks to inspect this system",
        g_state.diagnosticsRun ? COLOR_SUCCESS : COLOR_BLUE
    );

    const int lowerY = cardY + 143;
    const int lowerHeight = std::max(150, height - lowerY - 32);
    const int leftWidth = std::max(
        260,
        static_cast<int>(contentWidth * 0.61)
    );

    Rect activityRect{
        x,
        lowerY,
        leftWidth,
        lowerHeight
    };

    Rect statusRect{
        x + leftWidth + gap,
        lowerY,
        std::max(190, contentWidth - leftWidth - gap),
        lowerHeight
    };

    fill_rect(dc, activityRect, COLOR_PANEL);
    draw_border(dc, activityRect, COLOR_BORDER);

    draw_text(
        dc,
        L"Recent activity",
        x + 18,
        lowerY + 13,
        leftWidth - 36,
        27,
        COLOR_TEXT,
        g_fontMedium
    );

    draw_text(
        dc,
        L"Events generated by this application",
        x + 18,
        lowerY + 42,
        leftWidth - 36,
        20,
        COLOR_MUTED,
        g_fontSmall
    );

    draw_line(
        dc,
        x + 16,
        lowerY + 70,
        x + leftWidth - 16,
        lowerY + 70,
        COLOR_BORDER
    );

    int rowY = lowerY + 83;

    const int availableRows = std::max(1, (lowerHeight - 92) / 30);

    if (g_state.activity.empty()) {
        draw_text(
            dc,
            L"No events yet.",
            x + 18,
            rowY,
            leftWidth - 36,
            24,
            COLOR_MUTED,
            g_fontRegular
        );
    } else {
        for (int i = 0;
             i < static_cast<int>(g_state.activity.size()) &&
             i < availableRows;
             ++i) {
            const auto& entry = g_state.activity[i];

            draw_dot(
                dc,
                x + 24,
                rowY + 12,
                COLOR_ACCENT,
                3
            );

            draw_text(
                dc,
                L"[" + entry.time + L"] " + entry.message,
                x + 36,
                rowY,
                leftWidth - 54,
                24,
                COLOR_TEXT,
                g_fontSmall
            );

            rowY += 30;
        }
    }

    fill_rect(dc, statusRect, COLOR_PANEL);
    draw_border(dc, statusRect, COLOR_BORDER);

    draw_text(
        dc,
        L"System status",
        statusRect.x + 17,
        lowerY + 13,
        statusRect.width - 34,
        27,
        COLOR_TEXT,
        g_fontMedium
    );

    draw_line(
        dc,
        statusRect.x + 16,
        lowerY + 51,
        statusRect.x + statusRect.width - 16,
        lowerY + 51,
        COLOR_BORDER
    );

    draw_dot(
        dc,
        statusRect.x + 23,
        lowerY + 76,
        COLOR_SUCCESS,
        4
    );

    draw_text(
        dc,
        L"Dashboard",
        statusRect.x + 36,
        lowerY + 63,
        statusRect.width - 50,
        24,
        COLOR_TEXT,
        g_fontSmall
    );

    draw_dot(
        dc,
        statusRect.x + 23,
        lowerY + 108,
        COLOR_WARNING,
        4
    );

    draw_text(
        dc,
        L"Legacy modules",
        statusRect.x + 36,
        lowerY + 95,
        statusRect.width - 50,
        24,
        COLOR_TEXT,
        g_fontSmall
    );

    draw_text(
        dc,
        L"Not connected",
        statusRect.x + 36,
        lowerY + 116,
        statusRect.width - 50,
        22,
        COLOR_MUTED,
        g_fontSmall
    );

    draw_text(
        dc,
        L"Last action",
        statusRect.x + 17,
        lowerY + 153,
        statusRect.width - 34,
        20,
        COLOR_MUTED,
        g_fontSmall
    );

    draw_text(
        dc,
        g_state.lastAction,
        statusRect.x + 17,
        lowerY + 176,
        statusRect.width - 34,
        42,
        COLOR_TEXT,
        g_fontSmall,
        DT_LEFT | DT_TOP | DT_WORDBREAK
    );
}

void draw_modules(
    HDC dc,
    int width,
    int height
) {
    const int x = g_state.sidebarWidth + 30;
    const int contentWidth = width - x - 30;
    const int y = g_state.topbarHeight + 25;

    draw_section_heading(
        dc,
        L"Component registry",
        L"Inventory of the current application architecture.",
        x,
        y,
        contentWidth
    );

    const int startY = y + 80;

    struct ModuleRow {
        const wchar_t* name;
        const wchar_t* description;
        const wchar_t* state;
        COLORREF color;
    };

    const std::array<ModuleRow, 6> rows{{
        {
            L"Native dashboard",
            L"Windows interface, navigation, and event rendering",
            L"ACTIVE",
            COLOR_SUCCESS
        },
        {
            L"Local diagnostics",
            L"Windows version, architecture, and write-access checks",
            L"AVAILABLE",
            COLOR_SUCCESS
        },
        {
            L"Report exporter",
            L"User-selected local text report",
            L"AVAILABLE",
            COLOR_SUCCESS
        },
        {
            L"Browser session collection",
            L"Legacy credential and browser-session functionality",
            L"DISCONNECTED",
            COLOR_WARNING
        },
        {
            L"Remote data transfer",
            L"Legacy exfiltration and covert transport functionality",
            L"DISCONNECTED",
            COLOR_WARNING
        },
        {
            L"Persistence and evasion",
            L"Legacy persistence, anti-analysis, and destructive behavior",
            L"DISCONNECTED",
            COLOR_WARNING
        }
    }};

    const int rowHeight = 76;

    for (size_t i = 0; i < rows.size(); ++i) {
        const int rowY = startY + static_cast<int>(i) * rowHeight;

        Rect row{x, rowY, contentWidth, rowHeight - 8};

        fill_rect(dc, row, COLOR_PANEL);
        draw_border(dc, row, COLOR_BORDER);

        fill_rect(
            dc,
            Rect{x, rowY, 3, row.height},
            rows[i].color
        );

        draw_text(
            dc,
            rows[i].name,
            x + 17,
            rowY + 9,
            contentWidth - 180,
            24,
            COLOR_TEXT,
            g_fontMedium
        );

        draw_text(
            dc,
            rows[i].description,
            x + 17,
            rowY + 35,
            contentWidth - 180,
            20,
            COLOR_MUTED,
            g_fontSmall
        );

        draw_text(
            dc,
            rows[i].state,
            x + contentWidth - 145,
            rowY + 19,
            125,
            24,
            rows[i].color,
            g_fontSmall
        );
    }

    draw_text(
        dc,
        L"Registry entries describe the application; disconnected "
        L"legacy components are not executable from this interface.",
        x,
        height - 40,
        contentWidth,
        24,
        COLOR_MUTED,
        g_fontSmall
    );
}

void draw_activity_page(
    HDC dc,
    int width,
    int height
) {
    const int x = g_state.sidebarWidth + 30;
    const int contentWidth = width - x - 60;
    const int y = g_state.topbarHeight + 26;

    draw_section_heading(
        dc,
        L"Local event history",
        L"Events are recorded by the application during this session.",
        x,
        y,
        contentWidth
    );

    const int panelY = y + 78;
    const int panelHeight = height - panelY - 28;

    fill_rect(
        dc,
        Rect{x, panelY, contentWidth, panelHeight},
        COLOR_PANEL
    );

    draw_border(
        dc,
        Rect{x, panelY, contentWidth, panelHeight},
        COLOR_BORDER
    );

    draw_text(
        dc,
        L"TIME",
        x + 18,
        panelY + 13,
        105,
        24,
        COLOR_MUTED,
        g_fontSmall
    );

    draw_text(
        dc,
        L"CATEGORY",
        x + 135,
        panelY + 13,
        125,
        24,
        COLOR_MUTED,
        g_fontSmall
    );

    draw_text(
        dc,
        L"EVENT",
        x + 275,
        panelY + 13,
        contentWidth - 300,
        24,
        COLOR_MUTED,
        g_fontSmall
    );

    draw_line(
        dc,
        x + 16,
        panelY + 44,
        x + contentWidth - 16,
        panelY + 44,
        COLOR_BORDER
    );

    int rowY = panelY + 54;
    const int rowHeight = 32;

    const int maxRows = std::max(
        1,
        (panelHeight - 65) / rowHeight
    );

    for (int i = 0;
         i < static_cast<int>(g_state.activity.size()) &&
         i < maxRows;
         ++i) {
        const auto& entry = g_state.activity[i];

        draw_text(
            dc,
            entry.time,
            x + 18,
            rowY,
            105,
            25,
            COLOR_MUTED,
            g_fontSmall
        );

        draw_text(
            dc,
            entry.category,
            x + 135,
            rowY,
            125,
            25,
            COLOR_ACCENT_HOVER,
            g_fontSmall
        );

        draw_text(
            dc,
            entry.message,
            x + 275,
            rowY,
            contentWidth - 295,
            25,
            COLOR_TEXT,
            g_fontSmall
        );

        rowY += rowHeight;
    }

    if (g_state.activity.empty()) {
        draw_text(
            dc,
            L"No activity recorded yet.",
            x + 18,
            panelY + 62,
            contentWidth - 36,
            25,
            COLOR_MUTED,
            g_fontRegular
        );
    }
}

void draw_configuration(
    HDC dc,
    int width,
    int height
) {
    const int x = g_state.sidebarWidth + 30;
    const int contentWidth = width - x - 60;
    const int y = g_state.topbarHeight + 26;

    draw_section_heading(
        dc,
        L"Application configuration",
        L"Read-only build information and operational boundaries.",
        x,
        y,
        contentWidth
    );

    const int startY = y + 82;

    struct ConfigRow {
        const wchar_t* key;
        std::wstring value;
    };

    const std::array<ConfigRow, 6> config{{
        {L"Product", L"SIXERC"},
        {L"Version", widen(SIXERC_VERSION)},
        {L"Creator", widen(SIXERC_AUTHOR)},
        {L"Codename", widen(SIXERC_CODENAME)},
        {L"Execution mode", L"Local Windows desktop application"},
        {L"Legacy operations", L"Disconnected"}
    }};

    const int rowHeight = 55;

    for (size_t i = 0; i < config.size(); ++i) {
        const int rowY = startY + static_cast<int>(i) * rowHeight;

        fill_rect(
            dc,
            Rect{x, rowY, contentWidth, rowHeight - 5},
            COLOR_PANEL
        );

        draw_border(
            dc,
            Rect{x, rowY, contentWidth, rowHeight - 5},
            COLOR_BORDER
        );

        draw_text(
            dc,
            config[i].key,
            x + 17,
            rowY + 4,
            contentWidth / 3,
            36,
            COLOR_MUTED,
            g_fontRegular
        );

        draw_text(
            dc,
            config[i].value,
            x + contentWidth / 3,
            rowY + 4,
            contentWidth * 2 / 3 - 20,
            36,
            COLOR_TEXT,
            g_fontMedium
        );
    }

    draw_text(
        dc,
        L"This build does not expose controls for credential collection, "
        L"remote exfiltration, persistence, or destructive actions.",
        x,
        height - 54,
        contentWidth,
        40,
        COLOR_WARNING,
        g_fontSmall,
        DT_LEFT | DT_TOP | DT_WORDBREAK
    );
}

void draw_diagnostics(
    HDC dc,
    int width,
    int height
) {
    const int x = g_state.sidebarWidth + 30;
    const int contentWidth = width - x - 60;
    const int y = g_state.topbarHeight + 26;

    draw_section_heading(
        dc,
        L"System diagnostics",
        L"Checks are performed locally when you run diagnostics.",
        x,
        y,
        contentWidth
    );

    const int startY = y + 82;

    if (!g_state.diagnosticsRun) {
        fill_rect(
            dc,
            Rect{x, startY, contentWidth, 105},
            COLOR_PANEL
        );

        draw_border(
            dc,
            Rect{x, startY, contentWidth, 105},
            COLOR_BORDER
        );

        draw_text(
            dc,
            L"Diagnostics have not been run.",
            x + 20,
            startY + 17,
            contentWidth - 40,
            28,
            COLOR_TEXT,
            g_fontMedium
        );

        draw_text(
            dc,
            L"Use the Run diagnostics action to inspect this system.",
            x + 20,
            startY + 49,
            contentWidth - 40,
            24,
            COLOR_MUTED,
            g_fontSmall
        );

        return;
    }

    const int rowHeight = 68;

    for (size_t i = 0; i < g_state.diagnostics.size(); ++i) {
        const auto& item = g_state.diagnostics[i];

        const int rowY = startY + static_cast<int>(i) * rowHeight;

        Rect row{x, rowY, contentWidth, rowHeight - 7};

        fill_rect(dc, row, COLOR_PANEL);
        draw_border(dc, row, COLOR_BORDER);

        draw_dot(
            dc,
            x + 20,
            rowY + 25,
            item.passed ? COLOR_SUCCESS : COLOR_WARNING,
            4
        );

        draw_text(
            dc,
            item.name,
            x + 35,
            rowY + 7,
            contentWidth - 190,
            23,
            COLOR_TEXT,
            g_fontMedium
        );

        draw_text(
            dc,
            item.detail,
            x + 35,
            rowY + 31,
            contentWidth - 190,
            22,
            COLOR_MUTED,
            g_fontSmall
        );

        draw_text(
            dc,
            item.passed ? L"PASS" : L"CHECK",
            x + contentWidth - 135,
            rowY + 17,
            110,
            24,
            item.passed ? COLOR_SUCCESS : COLOR_WARNING,
            g_fontSmall
        );
    }

    if (startY + static_cast<int>(g_state.diagnostics.size()) *
        rowHeight > height - 20) {
        draw_text(
            dc,
            L"Some diagnostic rows may be below the visible area.",
            x,
            height - 24,
            contentWidth,
            20,
            COLOR_MUTED,
            g_fontSmall
        );
    }
}

void draw_about(
    HDC dc,
    int width,
    int height
) {
    const int x = g_state.sidebarWidth + 30;
    const int contentWidth = width - x - 60;
    const int y = g_state.topbarHeight + 26;

    Rect hero{x, y, contentWidth, 165};

    fill_rect(dc, hero, COLOR_PANEL);
    draw_border(dc, hero, COLOR_BORDER);

    fill_rect(
        dc,
        Rect{x, y, 4, hero.height},
        COLOR_ACCENT
    );

    draw_text(
        dc,
        L"SIXERC",
        x + 24,
        y + 22,
        contentWidth - 48,
        42,
        COLOR_TEXT,
        g_fontBold
    );

    draw_text(
        dc,
        L"Ekwensu's Grasp",
        x + 24,
        y + 67,
        contentWidth - 48,
        26,
        COLOR_ACCENT_HOVER,
        g_fontMedium
    );

    draw_text(
        dc,
        L"Version " SIXERC_VERSION,
        x + 24,
        y + 101,
        contentWidth - 48,
        22,
        COLOR_MUTED,
        g_fontSmall
    );

    const int infoY = y + 190;

    draw_text(
        dc,
        L"CREATED BY",
        x,
        infoY,
        contentWidth,
        20,
        COLOR_MUTED,
        g_fontSmall
    );

    draw_text(
        dc,
        L"Anonymous-beta (Chinedu)",
        x,
        infoY + 26,
        contentWidth,
        32,
        COLOR_TEXT,
        g_fontBold
    );

    draw_text(
        dc,
        L"BUILD PHILOSOPHY",
        x,
        infoY + 82,
        contentWidth,
        20,
        COLOR_MUTED,
        g_fontSmall
    );

    draw_text(
        dc,
        L"Native Windows software. Clear status reporting. "
        L"Explicit local actions. Testable diagnostics.",
        x,
        infoY + 107,
        contentWidth,
        46,
        COLOR_TEXT,
        g_fontRegular,
        DT_LEFT | DT_TOP | DT_WORDBREAK
    );

    draw_text(
        dc,
        L"The dashboard does not claim that disconnected components "
        L"are operational.",
        x,
        height - 50,
        contentWidth,
        36,
        COLOR_MUTED,
        g_fontSmall,
        DT_LEFT | DT_TOP | DT_WORDBREAK
    );
}

void draw_footer(
    HDC dc,
    int width,
    int height
) {
    const int y = height - 27;

    draw_line(
        dc,
        g_state.sidebarWidth,
        y - 7,
        width,
        y - 7,
        COLOR_BORDER
    );

    draw_text(
        dc,
        g_state.status,
        g_state.sidebarWidth + 30,
        y - 3,
        width - g_state.sidebarWidth - 60,
        22,
        COLOR_MUTED,
        g_fontSmall
    );
}

void paint_window(HWND hwnd) {
    PAINTSTRUCT ps{};
    HDC dc = BeginPaint(hwnd, &ps);

    RECT client{};
    GetClientRect(hwnd, &client);

    const int width = client.right;
    const int height = client.bottom;

    fill_rect(
        dc,
        Rect{0, 0, width, height},
        COLOR_BACKGROUND
    );

    draw_sidebar(dc, width, height);
    draw_topbar(dc, width);

    switch (g_state.page) {
        case Page::Dashboard:
            draw_dashboard(dc, width, height);
            break;

        case Page::Modules:
            draw_modules(dc, width, height);
            break;

        case Page::Activity:
            draw_activity_page(dc, width, height);
            break;

        case Page::Configuration:
            draw_configuration(dc, width, height);
            break;

        case Page::Diagnostics:
            draw_diagnostics(dc, width, height);
            break;

        case Page::About:
            draw_about(dc, width, height);
            break;
    }

    draw_footer(dc, width, height);

    EndPaint(hwnd, &ps);
}

void navigate_to(Page page) {
    g_state.page = page;

    log_activity(
        L"NAVIGATION",
        L"Opened " + page_title(page)
    );

    set_status(L"Ready");
}

void handle_click(HWND hwnd, int x, int y) {
    const int sidebarWidth = g_state.sidebarWidth;

    if (x < sidebarWidth) {
        if (y >= 137 && y < 179) {
            navigate_to(Page::Dashboard);
        } else if (y >= 186 && y < 228) {
            navigate_to(Page::Modules);
        } else if (y >= 235 && y < 277) {
            navigate_to(Page::Activity);
        } else if (y >= 284 && y < 326) {
            navigate_to(Page::Configuration);
        } else if (y >= 333 && y < 375) {
            navigate_to(Page::Diagnostics);
        } else if (y >= 382 && y < 424) {
            navigate_to(Page::About);
        }

        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    RECT client{};
    GetClientRect(hwnd, &client);

    const int width = client.right;
    const int height = client.bottom;

    if (g_state.page == Page::Dashboard) {
        const int contentX = sidebarWidth + 30;
        const int contentWidth = width - contentX - 30;
        const int cardY = g_state.topbarHeight + 26 + 78;

        if (y >= cardY && y < cardY + 116) {
            const int third = contentWidth / 3;

            if (x >= contentX + third * 2) {
                navigate_to(Page::Diagnostics);
                InvalidateRect(hwnd, nullptr, FALSE);
                return;
            }
        }
    }

    /*
       The dashboard deliberately keeps its controls simple:
       actions are explicit, visible, and do not run background work.
    */

    if (g_state.page == Page::Diagnostics) {
        const int contentX = sidebarWidth + 30;
        const int top = g_state.topbarHeight + 26 + 82;

        if (x >= contentX &&
            y >= top &&
            y <= top + 75) {
            run_diagnostics();
            navigate_to(Page::Diagnostics);
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }
    }

    InvalidateRect(hwnd, nullptr, FALSE);
}

LRESULT CALLBACK window_proc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
) {
    switch (message) {
        case WM_CREATE:
            g_window = hwnd;

            create_fonts();

            log_activity(
                L"SYSTEM",
                L"SIXERC dashboard initialized"
            );

            return 0;

        case WM_PAINT:
            paint_window(hwnd);
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_SIZE:
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_LBUTTONUP: {
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);

            handle_click(hwnd, x, y);
            return 0;
        }

        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);

            info->ptMinTrackSize.x = WINDOW_MIN_WIDTH;
            info->ptMinTrackSize.y = WINDOW_MIN_HEIGHT;

            return 0;
        }

        case WM_DESTROY:
            destroy_fonts();
            g_window = nullptr;
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int showCommand
) {
    SetProcessDPIAware();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(COLOR_BACKGROUND);
    wc.lpszClassName = APP_CLASS;

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(
            nullptr,
            L"SIXERC could not register its window class.",
            L"SIXERC startup error",
            MB_OK | MB_ICONERROR
        );

        return 1;
    }

    HWND hwnd = CreateWindowExW(
        0,
        APP_CLASS,
        APP_TITLE,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1280,
        820,
        nullptr,
        nullptr,
        instance,
        nullptr
    );

    if (!hwnd) {
        DeleteObject(wc.hbrBackground);

        MessageBoxW(
            nullptr,
            L"SIXERC could not create its main window.",
            L"SIXERC startup error",
            MB_OK | MB_ICONERROR
        );

        return 1;
    }

    ShowWindow(hwnd, showCommand);
    UpdateWindow(hwnd);

    MSG message{};

    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    DeleteObject(wc.hbrBackground);

    return static_cast<int>(message.wParam);
}