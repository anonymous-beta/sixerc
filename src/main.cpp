#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace {

constexpr wchar_t APP_CLASS[] = L"SIXERCNativeDashboard";
constexpr wchar_t APP_TITLE[] = L"SIXERC";

constexpr wchar_t APP_VERSION[] = L"1.1.0";
constexpr wchar_t APP_AUTHOR[] = L"Anonymous-beta (Chinedu)";
constexpr wchar_t APP_CODENAME[] = L"Ekwensu's Grasp";

constexpr int MIN_WINDOW_WIDTH = 900;
constexpr int MIN_WINDOW_HEIGHT = 620;
constexpr int SIDEBAR_WIDTH = 218;
constexpr int HEADER_HEIGHT = 76;

constexpr COLORREF C_BG = RGB(15, 17, 21);
constexpr COLORREF C_PANEL = RGB(22, 25, 31);
constexpr COLORREF C_PANEL_ALT = RGB(28, 32, 39);
constexpr COLORREF C_BORDER = RGB(49, 54, 64);
constexpr COLORREF C_TEXT = RGB(237, 239, 243);
constexpr COLORREF C_MUTED = RGB(151, 159, 172);
constexpr COLORREF C_ACCENT = RGB(194, 42, 54);
constexpr COLORREF C_ACCENT_HOVER = RGB(221, 52, 65);
constexpr COLORREF C_GREEN = RGB(91, 190, 135);
constexpr COLORREF C_YELLOW = RGB(235, 176, 77);
constexpr COLORREF C_BLUE = RGB(112, 165, 230);

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

    bool diagnosticsRun = false;
    bool exportHover = false;
    bool refreshHover = false;
};

AppState g_state;

HWND g_window = nullptr;

HFONT g_font_regular = nullptr;
HFONT g_font_medium = nullptr;
HFONT g_font_bold = nullptr;
HFONT g_font_small = nullptr;

struct Box {
    int x;
    int y;
    int w;
    int h;

    RECT rect() const {
        return RECT{x, y, x + w, y + h};
    }

    bool contains(int px, int py) const {
        return px >= x && py >= y &&
               px < x + w && py < y + h;
    }
};

std::wstring current_time() {
    SYSTEMTIME st{};
    GetLocalTime(&st);

    wchar_t buffer[32]{};

    swprintf_s(
        buffer,
        L"%02u:%02u:%02u",
        st.wHour,
        st.wMinute,
        st.wSecond
    );

    return buffer;
}

std::wstring current_date_time() {
    SYSTEMTIME st{};
    GetLocalTime(&st);

    wchar_t buffer[64]{};

    swprintf_s(
        buffer,
        L"%04u-%02u-%02u %02u:%02u:%02u",
        st.wYear,
        st.wMonth,
        st.wDay,
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
            current_time(),
            category,
            message
        }
    );

    if (g_state.activity.size() > 150) {
        g_state.activity.resize(150);
    }

    g_state.lastAction = message;
}

void set_status(const std::wstring& value) {
    g_state.status = value;

    if (g_window) {
        InvalidateRect(g_window, nullptr, FALSE);
    }
}

void select_page(Page page) {
    g_state.page = page;

    InvalidateRect(g_window, nullptr, FALSE);
}

void fill_rect(
    HDC dc,
    const RECT& rect,
    COLORREF color
) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void fill_box(
    HDC dc,
    const Box& box,
    COLORREF color
) {
    RECT r = box.rect();
    fill_rect(dc, r, color);
}

void draw_border(
    HDC dc,
    const RECT& rect,
    COLORREF color
) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));

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

void draw_box_border(
    HDC dc,
    const Box& box,
    COLORREF color
) {
    RECT r = box.rect();
    draw_border(dc, r, color);
}

void set_font(HDC dc, HFONT font) {
    SelectObject(dc, font);
}

void draw_text(
    HDC dc,
    const std::wstring& text,
    int x,
    int y,
    COLORREF color = C_TEXT,
    HFONT font = nullptr
) {
    if (font) {
        set_font(dc, font);
    }

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);

    TextOutW(
        dc,
        x,
        y,
        text.c_str(),
        static_cast<int>(text.size())
    );
}

void draw_text_rect(
    HDC dc,
    const std::wstring& text,
    RECT rect,
    COLORREF color = C_TEXT,
    HFONT font = nullptr,
    UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE
) {
    if (font) {
        set_font(dc, font);
    }

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);

    DrawTextW(
        dc,
        text.c_str(),
        static_cast<int>(text.size()),
        &rect,
        format
    );
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
    HGDIOBJ old_pen = SelectObject(dc, pen);

    MoveToEx(dc, x1, y1, nullptr);
    LineTo(dc, x2, y2);

    SelectObject(dc, old_pen);
    DeleteObject(pen);
}

void draw_circle(
    HDC dc,
    int x,
    int y,
    int diameter,
    COLORREF color
) {
    HBRUSH brush = CreateSolidBrush(color);
    HGDIOBJ old_brush = SelectObject(dc, brush);
    HGDIOBJ old_pen = SelectObject(dc, GetStockObject(NULL_PEN));

    Ellipse(dc, x, y, x + diameter, y + diameter);

    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(brush);
}

void draw_panel(
    HDC dc,
    const Box& box,
    COLORREF background = C_PANEL
) {
    fill_box(dc, box, background);
    draw_box_border(dc, box, C_BORDER);
}

void draw_button(
    HDC dc,
    const Box& box,
    const std::wstring& label,
    bool primary = false,
    bool hovered = false
) {
    COLORREF background = primary
        ? (hovered ? C_ACCENT_HOVER : C_ACCENT)
        : (hovered ? C_PANEL_ALT : C_PANEL);

    fill_box(dc, box, background);
    draw_box_border(
        dc,
        box,
        primary ? background : C_BORDER
    );

    RECT text_rect = box.rect();

    draw_text_rect(
        dc,
        label,
        text_rect,
        C_TEXT,
        g_font_medium,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE
    );
}

void draw_heading(
    HDC dc,
    const std::wstring& title,
    const std::wstring& subtitle
) {
    draw_text(dc, title, SIDEBAR_WIDTH + 30, 25, C_TEXT, g_font_bold);
    draw_text(dc, subtitle, SIDEBAR_WIDTH + 30, 49, C_MUTED, g_font_small);
}

std::wstring page_title(Page page) {
    switch (page) {
    case Page::Dashboard:
        return L"Dashboard";
    case Page::Modules:
        return L"Modules";
    case Page::Activity:
        return L"Activity";
    case Page::Configuration:
        return L"Configuration";
    case Page::Diagnostics:
        return L"Diagnostics";
    case Page::About:
        return L"About SIXERC";
    default:
        return L"Dashboard";
    }
}

std::wstring page_subtitle(Page page) {
    switch (page) {
    case Page::Dashboard:
        return L"Overview of the local application";
    case Page::Modules:
        return L"Components and their current availability";
    case Page::Activity:
        return L"Recent actions recorded in this session";
    case Page::Configuration:
        return L"Application metadata and local settings";
    case Page::Diagnostics:
        return L"Basic checks for this Windows installation";
    case Page::About:
        return L"Product information and build details";
    default:
        return L"";
    }
}

struct NavigationItem {
    Page page;
    const wchar_t* label;
    const wchar_t* symbol;
};

constexpr std::array<NavigationItem, 6> NAV_ITEMS{{
    {Page::Dashboard, L"Dashboard", L"D"},
    {Page::Modules, L"Modules", L"M"},
    {Page::Activity, L"Activity", L"A"},
    {Page::Configuration, L"Configuration", L"C"},
    {Page::Diagnostics, L"Diagnostics", L"!"},
    {Page::About, L"About", L"i"}
}};

void draw_sidebar(HDC dc, int height) {
    RECT sidebar{0, 0, SIDEBAR_WIDTH, height};
    fill_rect(dc, sidebar, C_PANEL);

    draw_line(dc, SIDEBAR_WIDTH - 1, 0, SIDEBAR_WIDTH - 1, height, C_BORDER);

    draw_box_border(dc, Box{24, 22, 38, 38}, C_ACCENT);

    draw_text(
        dc,
        L"S",
        37,
        28,
        C_ACCENT_HOVER,
        g_font_bold
    );

    draw_text(dc, L"SIXERC", 73, 24, C_TEXT, g_font_bold);
    draw_text(dc, L"LOCAL CONTROL CENTER", 73, 46, C_MUTED, g_font_small);

    draw_line(dc, 20, 80, SIDEBAR_WIDTH - 20, 80, C_BORDER);

    draw_text(dc, L"WORKSPACE", 24, 100, C_MUTED, g_font_small);

    int y = 126;

    for (const auto& item : NAV_ITEMS) {
        const bool selected = g_state.page == item.page;
        Box row{12, y, SIDEBAR_WIDTH - 24, 42};

        if (selected) {
            fill_box(dc, row, RGB(52, 29, 35));
            fill_box(dc, Box{12, y, 3, 42}, C_ACCENT);
        }

        draw_text(
            dc,
            item.symbol,
            28,
            y + 12,
            selected ? C_ACCENT_HOVER : C_MUTED,
            g_font_bold
        );

        draw_text(
            dc,
            item.label,
            55,
            y + 12,
            selected ? C_TEXT : C_MUTED,
            selected ? g_font_medium : g_font_regular
        );

        y += 48;
    }

    RECT footer{
        20,
        height - 82,
        SIDEBAR_WIDTH - 20,
        height - 81
    };

    draw_line(dc, footer.left, footer.top, footer.right, footer.top, C_BORDER);

    draw_circle(dc, 24, height - 61, 8, C_GREEN);
    draw_text(dc, L"LOCAL SESSION", 40, height - 62, C_TEXT, g_font_small);
    draw_text(dc, L"Windows desktop", 40, height - 42, C_MUTED, g_font_small);
}

void draw_header(HDC dc, int width) {
    RECT header{SIDEBAR_WIDTH, 0, width, HEADER_HEIGHT};
    fill_rect(dc, header, C_BG);

    draw_line(
        dc,
        SIDEBAR_WIDTH,
        HEADER_HEIGHT - 1,
        width,
        HEADER_HEIGHT - 1,
        C_BORDER
    );

    const std::wstring title = page_title(g_state.page);
    const std::wstring subtitle = page_subtitle(g_state.page);

    draw_heading(dc, title, subtitle);

    Box refresh_button{width - 268, 20, 108, 36};
    Box export_button{width - 148, 20, 124, 36};

    draw_button(
        dc,
        refresh_button,
        L"Refresh",
        false,
        g_state.refreshHover
    );

    draw_button(
        dc,
        export_button,
        L"Export report",
        true,
        g_state.exportHover
    );
}

void draw_stat_card(
    HDC dc,
    const Box& box,
    const std::wstring& label,
    const std::wstring& value,
    const std::wstring& note,
    COLORREF indicator
) {
    draw_panel(dc, box);

    fill_box(dc, Box{box.x + 16, box.y + 18, 4, 38}, indicator);

    draw_text(dc, label, box.x + 31, box.y + 17, C_MUTED, g_font_small);
    draw_text(dc, value, box.x + 31, box.y + 38, C_TEXT, g_font_bold);
    draw_text(dc, note, box.x + 31, box.y + 72, C_MUTED, g_font_small);
}

void draw_section_title(
    HDC dc,
    const std::wstring& title,
    const std::wstring& detail,
    int x,
    int y
) {
    draw_text(dc, title, x, y, C_TEXT, g_font_medium);
    draw_text(dc, detail, x, y + 23, C_MUTED, g_font_small);
}

void draw_dashboard(HDC dc, int width, int height) {
    const int left = SIDEBAR_WIDTH + 26;
    const int right = width - 26;
    const int available = right - left;
    const int gap = 14;
    const int card_width = (available - gap * 3) / 4;
    const int top = HEADER_HEIGHT + 22;

    draw_stat_card(
        dc,
        Box{left, top, card_width, 108},
        L"APPLICATION",
        L"Running",
        L"Local desktop session",
        C_GREEN
    );

    draw_stat_card(
        dc,
        Box{left + card_width + gap, top, card_width, 108},
        L"DIAGNOSTICS",
        g_state.diagnosticsRun ? L"Checked" : L"Pending",
        g_state.diagnosticsRun ? L"Last check completed" : L"Run a local check",
        g_state.diagnosticsRun ? C_GREEN : C_YELLOW
    );

    draw_stat_card(
        dc,
        Box{left + (card_width + gap) * 2, top, card_width, 108},
        L"SESSION EVENTS",
        std::to_wstring(g_state.activity.size()),
        L"In-memory activity",
        C_BLUE
    );

    draw_stat_card(
        dc,
        Box{left + (card_width + gap) * 3, top, card_width, 108},
        L"LEGACY MODULES",
        L"Disabled",
        L"Not connected to UI actions",
        C_YELLOW
    );

    const int panel_y = top + 128;
    const int panel_h = std::max(210, height - panel_y - 28);
    const int left_panel_w = (available * 58) / 100;
    const int right_panel_x = left + left_panel_w + gap;
    const int right_panel_w = right - right_panel_x;

    draw_panel(dc, Box{left, panel_y, left_panel_w, panel_h});
    draw_section_title(
        dc,
        L"System overview",
        L"Current application state",
        left + 18,
        panel_y + 18
    );

    draw_line(
        dc,
        left + 18,
        panel_y + 62,
        left + left_panel_w - 18,
        panel_y + 62,
        C_BORDER
    );

    const std::array<std::pair<std::wstring, std::wstring>, 5> overview{{
        {L"Product", L"SIXERC Native Dashboard"},
        {L"Version", APP_VERSION},
        {L"Environment", L"Windows desktop"},
        {L"Processing", L"Local UI only"},
        {L"Current status", g_state.status}
    }};

    int row_y = panel_y + 82;

    for (const auto& row : overview) {
        draw_text(dc, row.first, left + 20, row_y, C_MUTED, g_font_small);
        draw_text(dc, row.second, left + 180, row_y, C_TEXT, g_font_regular);

        draw_line(
            dc,
            left + 18,
            row_y + 28,
            left + left_panel_w - 18,
            row_y + 28,
            C_BORDER
        );

        row_y += 42;
    }

    draw_panel(dc, Box{right_panel_x, panel_y, right_panel_w, panel_h});

    draw_section_title(
        dc,
        L"Recent activity",
        L"Latest entries from this session",
        right_panel_x + 18,
        panel_y + 18
    );

    draw_line(
        dc,
        right_panel_x + 18,
        panel_y + 62,
        right_panel_x + right_panel_w - 18,
        panel_y + 62,
        C_BORDER
    );

    int activity_y = panel_y + 82;
    const int visible = std::min<int>(
        5,
        static_cast<int>(g_state.activity.size())
    );

    for (int i = 0; i < visible; ++i) {
        const auto& entry = g_state.activity[static_cast<size_t>(i)];

        draw_circle(
            dc,
            right_panel_x + 19,
            activity_y + 3,
            7,
            i == 0 ? C_ACCENT : C_MUTED
        );

        draw_text(
            dc,
            entry.message,
            right_panel_x + 36,
            activity_y,
            C_TEXT,
            g_font_small
        );

        draw_text(
            dc,
            entry.time + L"  /  " + entry.category,
            right_panel_x + 36,
            activity_y + 19,
            C_MUTED,
            g_font_small
        );

        activity_y += 49;
    }

    if (visible == 0) {
        draw_text(
            dc,
            L"No activity recorded yet.",
            right_panel_x + 18,
            activity_y,
            C_MUTED,
            g_font_small
        );
    }
}

void draw_modules(HDC dc, int width, int height) {
    const int left = SIDEBAR_WIDTH + 26;
    const int panel_width = width - left - 26;
    int y = HEADER_HEIGHT + 24;

    draw_text(
        dc,
        L"Module availability",
        left,
        y,
        C_TEXT,
        g_font_bold
    );

    draw_text(
        dc,
        L"This view describes UI components only. Legacy operations are not invoked.",
        left,
        y + 28,
        C_MUTED,
        g_font_small
    );

    y += 70;

    struct ModuleInfo {
        const wchar_t* name;
        const wchar_t* description;
        const wchar_t* status;
        COLORREF color;
    };

    const std::array<ModuleInfo, 5> modules{{
        {L"Dashboard", L"Local application status and overview", L"Available", C_GREEN},
        {L"Activity log", L"In-memory events generated by this UI", L"Available", C_GREEN},
        {L"Diagnostics", L"Basic Windows and filesystem checks", L"Available", C_GREEN},
        {L"Report export", L"Save a local text report", L"Available", C_GREEN},
        {L"Legacy operations", L"Disconnected from the dashboard", L"Disabled", C_YELLOW}
    }};

    for (const auto& module : modules) {
        draw_panel(dc, Box{left, y, panel_width, 76});

        draw_circle(dc, left + 18, y + 20, 9, module.color);
        draw_text(dc, module.name, left + 40, y + 15, C_TEXT, g_font_medium);
        draw_text(dc, module.description, left + 40, y + 39, C_MUTED, g_font_small);

        RECT status_rect{
            left + panel_width - 132,
            y + 22,
            left + panel_width - 18,
            y + 51
        };

        draw_text_rect(
            dc,
            module.status,
            status_rect,
            module.color,
            g_font_small,
            DT_RIGHT | DT_VCENTER | DT_SINGLELINE
        );

        y += 90;

        if (y > height - 70) {
            break;
        }
    }
}

void draw_activity_page(HDC dc, int width, int height) {
    const int left = SIDEBAR_WIDTH + 26;
    const int panel_width = width - left - 52;
    const int top = HEADER_HEIGHT + 24;

    draw_panel(dc, Box{left, top, panel_width, height - top - 28});

    draw_text(dc, L"Session activity", left + 20, top + 18, C_TEXT, g_font_bold);
    draw_text(
        dc,
        L"Entries are held in memory and are not transmitted.",
        left + 20,
        top + 43,
        C_MUTED,
        g_font_small
    );

    draw_line(
        dc,
        left + 18,
        top + 70,
        left + panel_width - 18,
        top + 70,
        C_BORDER
    );

    draw_text(dc, L"TIME", left + 20, top + 84, C_MUTED, g_font_small);
    draw_text(dc, L"CATEGORY", left + 130, top + 84, C_MUTED, g_font_small);
    draw_text(dc, L"EVENT", left + 260, top + 84, C_MUTED, g_font_small);

    int y = top + 112;
    const int max_rows = std::max(0, (height - y - 40) / 34);
    const int count = std::min<int>(
        max_rows,
        static_cast<int>(g_state.activity.size())
    );

    for (int i = 0; i < count; ++i) {
        const auto& entry = g_state.activity[static_cast<size_t>(i)];

        draw_text(dc, entry.time, left + 20, y, C_MUTED, g_font_small);
        draw_text(dc, entry.category, left + 130, y, C_BLUE, g_font_small);
        draw_text(dc, entry.message, left + 260, y, C_TEXT, g_font_small);

        draw_line(
            dc,
            left + 18,
            y + 24,
            left + panel_width - 18,
            y + 24,
            C_BORDER
        );

        y += 34;
    }

    if (count == 0) {
        draw_text(
            dc,
            L"No activity entries are available.",
            left + 20,
            y + 5,
            C_MUTED,
            g_font_regular
        );
    }
}

void draw_configuration(HDC dc, int width, int height) {
    const int left = SIDEBAR_WIDTH + 26;
    const int panel_width = width - left - 52;
    const int top = HEADER_HEIGHT + 24;

    draw_panel(dc, Box{left, top, panel_width, height - top - 28});

    draw_text(
        dc,
        L"Application information",
        left + 20,
        top + 20,
        C_TEXT,
        g_font_bold
    );

    draw_text(
        dc,
        L"Read-only metadata for this local dashboard.",
        left + 20,
        top + 46,
        C_MUTED,
        g_font_small
    );

    draw_line(
        dc,
        left + 18,
        top + 76,
        left + panel_width - 18,
        top + 76,
        C_BORDER
    );

    const std::array<std::pair<std::wstring, std::wstring>, 7> rows{{
        {L"Product", L"SIXERC"},
        {L"Version", APP_VERSION},
        {L"Codename", APP_CODENAME},
        {L"Creator", APP_AUTHOR},
        {L"Application type", L"Native Windows desktop"},
        {L"Processing model", L"Local UI and diagnostics"},
        {L"Legacy modules", L"Disconnected"}
    }};

    int y = top + 100;

    for (const auto& row : rows) {
        draw_text(dc, row.first, left + 22, y, C_MUTED, g_font_small);
        draw_text(dc, row.second, left + 220, y, C_TEXT, g_font_regular);

        draw_line(
            dc,
            left + 20,
            y + 27,
            left + panel_width - 20,
            y + 27,
            C_BORDER
        );

        y += 44;
    }
}

void draw_diagnostics(HDC dc, int width, int height) {
    const int left = SIDEBAR_WIDTH + 26;
    const int panel_width = width - left - 52;
    const int top = HEADER_HEIGHT + 24;

    draw_panel(dc, Box{left, top, panel_width, height - top - 28});

    draw_text(
        dc,
        L"Local diagnostics",
        left + 20,
        top + 20,
        C_TEXT,
        g_font_bold
    );

    draw_text(
        dc,
        L"These checks inspect basic local application conditions only.",
        left + 20,
        top + 46,
        C_MUTED,
        g_font_small
    );

    draw_button(
        dc,
        Box{left + panel_width - 168, top + 16, 146, 36},
        L"Run diagnostics",
        true
    );

    draw_line(
        dc,
        left + 18,
        top + 72,
        left + panel_width - 18,
        top + 72,
        C_BORDER
    );

    if (!g_state.diagnosticsRun) {
        draw_text(
            dc,
            L"Diagnostics have not been run during this session.",
            left + 22,
            top + 98,
            C_MUTED,
            g_font_regular
        );

        draw_text(
            dc,
            L"Select Run diagnostics to check the executable path, temporary directory,",
            left + 22,
            top + 130,
            C_MUTED,
            g_font_small
        );

        draw_text(
            dc,
            L"and basic Windows version information.",
            left + 22,
            top + 150,
            C_MUTED,
            g_font_small
        );

        return;
    }

    int y = top + 94;

    for (const auto& result : g_state.diagnostics) {
        draw_circle(
            dc,
            left + 22,
            y + 4,
            8,
            result.passed ? C_GREEN : C_YELLOW
        );

        draw_text(dc, result.name, left + 42, y, C_TEXT, g_font_medium);
        draw_text(dc, result.detail, left + 42, y + 23, C_MUTED, g_font_small);

        y += 58;
    }
}

void draw_about(HDC dc, int width, int height) {
    const int left = SIDEBAR_WIDTH + 26;
    const int panel_width = width - left - 52;
    const int top = HEADER_HEIGHT + 24;

    draw_panel(dc, Box{left, top, panel_width, height - top - 28});

    draw_box_border(dc, Box{left + 24, top + 24, 64, 64}, C_ACCENT);
    draw_text(dc, L"S", left + 46, top + 39, C_ACCENT_HOVER, g_font_bold);

    draw_text(dc, L"SIXERC", left + 108, top + 28, C_TEXT, g_font_bold);
    draw_text(dc, L"Native Windows dashboard", left + 108, top + 56, C_MUTED, g_font_regular);

    draw_line(
        dc,
        left + 22,
        top + 108,
        left + panel_width - 22,
        top + 108,
        C_BORDER
    );

    draw_text(dc, L"Version", left + 24, top + 132, C_MUTED, g_font_small);
    draw_text(dc, APP_VERSION, left + 190, top + 132, C_TEXT, g_font_regular);

    draw_text(dc, L"Codename", left + 24, top + 172, C_MUTED, g_font_small);
    draw_text(dc, APP_CODENAME, left + 190, top + 172, C_TEXT, g_font_regular);

    draw_text(dc, L"Creator", left + 24, top + 212, C_MUTED, g_font_small);
    draw_text(dc, APP_AUTHOR, left + 190, top + 212, C_TEXT, g_font_regular);

    draw_text(dc, L"Design", left + 24, top + 252, C_MUTED, g_font_small);
    draw_text(dc, L"Graphite interface with crimson accents", left + 190, top + 252, C_TEXT, g_font_regular);

    draw_text(dc, L"Privacy", left + 24, top + 292, C_MUTED, g_font_small);
    draw_text(dc, L"Diagnostics run locally; no telemetry is sent", left + 190, top + 292, C_TEXT, g_font_regular);

    draw_text(dc, L"Legacy operations", left + 24, top + 332, C_MUTED, g_font_small);
    draw_text(dc, L"Disconnected from this dashboard", left + 190, top + 332, C_YELLOW, g_font_regular);
}

void run_diagnostics() {
    g_state.diagnostics.clear();

    wchar_t exe_path[MAX_PATH]{};
    const DWORD exe_length = GetModuleFileNameW(
        nullptr,
        exe_path,
        static_cast<DWORD>(std::size(exe_path))
    );

    const bool exe_ok = exe_length > 0 &&
                        exe_length < std::size(exe_path);

    g_state.diagnostics.push_back({
        L"Application path",
        exe_ok ? std::wstring(exe_path) : L"Unable to resolve executable path",
        exe_ok
    });

    wchar_t temp_path[MAX_PATH]{};
    const DWORD temp_length = GetTempPathW(
        static_cast<DWORD>(std::size(temp_path)),
        temp_path
    );

    bool temp_ok = temp_length > 0 &&
                   temp_length < std::size(temp_path);

    std::wstring temp_detail = temp_ok
        ? std::wstring(temp_path)
        : L"Windows temporary directory is unavailable";

    if (temp_ok) {
        std::error_code ec;
        const std::filesystem::path test_file =
            std::filesystem::path(temp_path) /
            L"sixerc_diagnostic_test.tmp";

        {
            std::ofstream test(test_file, std::ios::binary | std::ios::trunc);
            temp_ok = test.good();

            if (temp_ok) {
                test << "SIXERC local diagnostic test";
                temp_ok = test.good();
            }
        }

        std::filesystem::remove(test_file, ec);

        if (!temp_ok) {
            temp_detail = L"Temporary directory is not writable";
        }
    }

    g_state.diagnostics.push_back({
        L"Temporary directory",
        temp_detail,
        temp_ok
    });

    OSVERSIONINFOEXW version_info{};
    version_info.dwOSVersionInfoSize = sizeof(version_info);

    using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);

    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    auto rtl_get_version = ntdll
        ? reinterpret_cast<RtlGetVersionFn>(
            GetProcAddress(ntdll, "RtlGetVersion")
        )
        : nullptr;

    bool version_ok = false;
    std::wstring version_detail = L"Windows version could not be queried";

    if (rtl_get_version) {
        RTL_OSVERSIONINFOW version{};
        version.dwOSVersionInfoSize = sizeof(version);

        if (rtl_get_version(&version) == 0) {
            version_ok = true;

            version_detail =
                L"Windows " +
                std::to_wstring(version.dwMajorVersion) +
                L"." +
                std::to_wstring(version.dwMinorVersion) +
                L" (build " +
                std::to_wstring(version.dwBuildNumber) +
                L")";
        }
    }

    g_state.diagnostics.push_back({
        L"Operating system",
        version_detail,
        version_ok
    });

    g_state.diagnostics.push_back({
        L"Legacy module isolation",
        L"Legacy operations are disconnected from UI actions",
        true
    });

    g_state.diagnosticsRun = true;
    set_status(L"Diagnostics completed");
    log_activity(L"Diagnostics", L"Local diagnostics completed");
}

std::wstring diagnostic_report() {
    std::wstringstream report;

    report << L"SIXERC Diagnostic Report\n";
    report << L"========================\n\n";
    report << L"Product: " << APP_TITLE << L"\n";
    report << L"Version: " << APP_VERSION << L"\n";
    report << L"Codename: " << APP_CODENAME << L"\n";
    report << L"Creator: " << APP_AUTHOR << L"\n";
    report << L"Generated: " << current_date_time() << L"\n\n";

    report << L"Application status\n";
    report << L"------------------\n";
    report << L"Status: " << g_state.status << L"\n";
    report << L"Last action: " << g_state.lastAction << L"\n";
    report << L"Activity entries: " << g_state.activity.size() << L"\n\n";

    report << L"Diagnostics\n";
    report << L"-----------\n";

    if (!g_state.diagnosticsRun) {
        report << L"Diagnostics have not been run.\n";
    } else {
        for (const auto& item : g_state.diagnostics) {
            report << L"[" << (item.passed ? L"PASS" : L"CHECK") << L"] "
                   << item.name << L": "
                   << item.detail << L"\n";
        }
    }

    report << L"\nActivity\n";
    report << L"--------\n";

    if (g_state.activity.empty()) {
        report << L"No activity recorded.\n";
    } else {
        for (const auto& entry : g_state.activity) {
            report << L"[" << entry.time << L"] "
                   << entry.category << L" - "
                   << entry.message << L"\n";
        }
    }

    report << L"\nPrivacy note: this report is generated locally by the UI.\n";
    report << L"Legacy operations are not invoked by this dashboard.\n";

    return report.str();
}

void export_report(HWND owner) {
    wchar_t path[MAX_PATH] = L"SIXERC-report.txt";

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter =
        L"Text reports (*.txt)\0*.txt\0All files (*.*)\0*.*\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = static_cast<DWORD>(std::size(path));
    dialog.lpstrDefExt = L"txt";
    dialog.Flags =
        OFN_OVERWRITEPROMPT |
        OFN_PATHMUSTEXIST |
        OFN_HIDEREADONLY;

    if (!GetSaveFileNameW(&dialog)) {
        return;
    }

    std::wofstream output(
        std::filesystem::path(path),
        std::ios::out | std::ios::trunc
    );

    if (!output) {
        MessageBoxW(
            owner,
            L"The report could not be written. Check the destination permissions.",
            L"SIXERC - Export failed",
            MB_OK | MB_ICONERROR
        );

        log_activity(L"Export", L"Report export failed");
        set_status(L"Report export failed");
        return;
    }

    output << diagnostic_report();
    output.close();

    if (!output) {
        MessageBoxW(
            owner,
            L"The report could not be completed.",
            L"SIXERC - Export failed",
            MB_OK | MB_ICONERROR
        );

        log_activity(L"Export", L"Report export failed");
        set_status(L"Report export failed");
        return;
    }

    log_activity(L"Export", L"Diagnostic report exported");
    set_status(L"Report exported");

    MessageBoxW(
        owner,
        L"The report was saved successfully.",
        L"SIXERC",
        MB_OK | MB_ICONINFORMATION
    );

    InvalidateRect(owner, nullptr, FALSE);
}

void create_fonts() {
    g_font_regular = CreateFontW(
        15, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );

    g_font_medium = CreateFontW(
        15, 0, 0, 0, FW_SEMIBOLD,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );

    g_font_bold = CreateFontW(
        19, 0, 0, 0, FW_BOLD,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );

    g_font_small = CreateFontW(
        12, 0, 0, 0, FW_NORMAL,
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
    if (g_font_regular) {
        DeleteObject(g_font_regular);
        g_font_regular = nullptr;
    }

    if (g_font_medium) {
        DeleteObject(g_font_medium);
        g_font_medium = nullptr;
    }

    if (g_font_bold) {
        DeleteObject(g_font_bold);
        g_font_bold = nullptr;
    }

    if (g_font_small) {
        DeleteObject(g_font_small);
        g_font_small = nullptr;
    }
}

void paint_window(HWND hwnd) {
    PAINTSTRUCT ps{};
    HDC dc = BeginPaint(hwnd, &ps);

    RECT client{};
    GetClientRect(hwnd, &client);

    const int width = client.right - client.left;
    const int height = client.bottom - client.top;

    HDC buffer_dc = CreateCompatibleDC(dc);
    HBITMAP buffer_bitmap = CreateCompatibleBitmap(
        dc,
        std::max(1, width),
        std::max(1, height)
    );

    HGDIOBJ old_bitmap = SelectObject(buffer_dc, buffer_bitmap);

    fill_rect(buffer_dc, client, C_BG);

    draw_sidebar(buffer_dc, height);
    draw_header(buffer_dc, width);

    switch (g_state.page) {
    case Page::Dashboard:
        draw_dashboard(buffer_dc, width, height);
        break;

    case Page::Modules:
        draw_modules(buffer_dc, width, height);
        break;

    case Page::Activity:
        draw_activity_page(buffer_dc, width, height);
        break;

    case Page::Configuration:
        draw_configuration(buffer_dc, width, height);
        break;

    case Page::Diagnostics:
        draw_diagnostics(buffer_dc, width, height);
        break;

    case Page::About:
        draw_about(buffer_dc, width, height);
        break;
    }

    BitBlt(
        dc,
        0,
        0,
        width,
        height,
        buffer_dc,
        0,
        0,
        SRCCOPY
    );

    SelectObject(buffer_dc, old_bitmap);
    DeleteObject(buffer_bitmap);
    DeleteDC(buffer_dc);

    EndPaint(hwnd, &ps);
}

void handle_click(HWND hwnd, int x, int y, int width) {
    if (x < SIDEBAR_WIDTH && y >= 126) {
        const int index = (y - 126) / 48;

        if (index >= 0 &&
            index < static_cast<int>(NAV_ITEMS.size())) {
            select_page(NAV_ITEMS[static_cast<size_t>(index)].page);
            return;
        }
    }

    if (y >= 20 && y <= 56) {
        const Box refresh_button{width - 268, 20, 108, 36};
        const Box export_button{width - 148, 20, 124, 36};

        if (refresh_button.contains(x, y)) {
            log_activity(L"UI", L"Dashboard refreshed");
            set_status(L"Ready");
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }

        if (export_button.contains(x, y)) {
            export_report(hwnd);
            return;
        }
    }

    if (g_state.page == Page::Diagnostics) {
        const int panel_width = width - (SIDEBAR_WIDTH + 26) - 52;
        const int left = SIDEBAR_WIDTH + 26;
        const int top = HEADER_HEIGHT + 24;

        const Box run_button{
            left + panel_width - 168,
            top + 16,
            146,
            36
        };

        if (run_button.contains(x, y)) {
            run_diagnostics();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
    }
}

void update_hover(int x, int y, int width) {
    const Box refresh_button{width - 268, 20, 108, 36};
    const Box export_button{width - 148, 20, 124, 36};

    const bool old_refresh = g_state.refreshHover;
    const bool old_export = g_state.exportHover;

    g_state.refreshHover = refresh_button.contains(x, y);
    g_state.exportHover = export_button.contains(x, y);

    if (old_refresh != g_state.refreshHover ||
        old_export != g_state.exportHover) {
        InvalidateRect(g_window, nullptr, FALSE);
    }
}

LRESULT CALLBACK window_proc(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam
) {
    switch (message) {
    case WM_CREATE:
        return 0;

    case WM_PAINT:
        paint_window(hwnd);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_SIZE:
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_MOUSEMOVE: {
        RECT client{};
        GetClientRect(hwnd, &client);

        update_hover(
            GET_X_LPARAM(lparam),
            GET_Y_LPARAM(lparam),
            client.right - client.left
        );

        return 0;
    }

    case WM_LBUTTONUP: {
        RECT client{};
        GetClientRect(hwnd, &client);

        handle_click(
            hwnd,
            GET_X_LPARAM(lparam),
            GET_Y_LPARAM(lparam),
            client.right - client.left
        );

        return 0;
    }

    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lparam);

        info->ptMinTrackSize.x = MIN_WINDOW_WIDTH;
        info->ptMinTrackSize.y = MIN_WINDOW_HEIGHT;

        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
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

    log_activity(L"System", L"SIXERC dashboard initialized");

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(C_BG);
    wc.lpszClassName = APP_CLASS;

    if (!RegisterClassExW(&wc)) {
        destroy_fonts();

        MessageBoxW(
            nullptr,
            L"Windows could not register the SIXERC window class.",
            L"SIXERC startup error",
            MB_OK | MB_ICONERROR
        );

        return 1;
    }

    HWND hwnd = CreateWindowExW(
        0,
        APP_CLASS,
        APP_TITLE,
        WS_OVERLAPPEDWINDOW,
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
        UnregisterClassW(APP_CLASS, instance);
        destroy_fonts();

        MessageBoxW(
            nullptr,
            L"Windows could not create the SIXERC window.",
            L"SIXERC startup error",
            MB_OK | MB_ICONERROR
        );

        return 1;
    }

    g_window = hwnd;

    ShowWindow(hwnd, show_command);
    UpdateWindow(hwnd);

    MSG message{};

    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    HBRUSH background_brush = wc.hbrBackground;

    destroy_fonts();

    if (background_brush) {
        DeleteObject(background_brush);
    }

    UnregisterClassW(APP_CLASS, instance);

    return static_cast<int>(message.wParam);
}