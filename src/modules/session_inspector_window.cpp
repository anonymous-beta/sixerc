
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "session_inspector_window.hpp"
#include "session_inspector.hpp"

#include <windows.h>

#include <cstdint>
#include <sstream>
#include <string>

namespace sixerc::ui {
namespace {

constexpr wchar_t CLASS_NAME[] =
    L"SIXERC_SESSION_INSPECTOR";

constexpr wchar_t WINDOW_NAME[] =
    L"SIXERC | EKWENSU SESSION INTELLIGENCE";

constexpr int ID_TOKEN_INPUT = 7101;
constexpr int ID_INSPECT = 7102;
constexpr int ID_RESULT = 7103;
constexpr int ID_CLEAR = 7104;

constexpr COLORREF BACKGROUND = RGB(15, 11, 15);
constexpr COLORREF PANEL = RGB(31, 23, 29);
constexpr COLORREF RED = RGB(205, 43, 61);
constexpr COLORREF GOLD = RGB(209, 164, 91);
constexpr COLORREF WHITE = RGB(239, 231, 234);
constexpr COLORREF MUTED = RGB(165, 150, 159);

HWND g_window = nullptr;
HWND g_input = nullptr;
HWND g_result = nullptr;
HFONT g_font = nullptr;
HFONT g_heading_font = nullptr;

std::wstring utf8_to_wide(const std::string& value) {
    if (value.empty()) {
        return {};
    }

    const int required = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0
    );

    if (required <= 0) {
        return L"[Unable to display this claim]";
    }

    std::wstring output(
        static_cast<std::size_t>(required),
        L'\0'
    );

    MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        output.data(),
        required
    );

    return output;
}

std::wstring optional_time(
    const std::optional<std::int64_t>& value
) {
    if (!value) {
        return L"Not supplied";
    }

    return std::to_wstring(*value) +
        L" (Unix timestamp)";
}

std::wstring make_result(
    const sixerc::SessionInspection& inspection
) {
    std::wstringstream output;

    output << L"SIXERC SESSION INTELLIGENCE\n";
    output << L"===========================\n\n";

    output << L"STRUCTURE\n";
    output << L"---------\n";

    output << L"Readable structure: "
           << (inspection.structurally_valid
                   ? L"YES"
                   : L"NO")
           << L"\n";

    output << L"Claims decoded: "
           << (inspection.claims_read
                   ? L"YES"
                   : L"NO")
           << L"\n";

    output << L"Signature verified: NO\n";
    output << L"Message: "
           << utf8_to_wide(inspection.message)
           << L"\n\n";

    if (!inspection.claims_read) {
        output << L"No claims are available to display.\n";
        output << L"\nIMPORTANT: This screen does not "
                  L"authenticate or validate a token.\n";

        return output.str();
    }

    const auto& claims = inspection.claims;

    output << L"IDENTITY CLAIMS\n";
    output << L"---------------\n";

    output << L"Issuer: "
           << utf8_to_wide(claims.issuer)
           << L"\n";

    output << L"Audience: "
           << utf8_to_wide(claims.audience)
           << L"\n";

    output << L"Subject: "
           << utf8_to_wide(claims.subject)
           << L"\n";

    output << L"Tenant ID: "
           << utf8_to_wide(claims.tenant_id)
           << L"\n";

    output << L"Object ID: "
           << utf8_to_wide(claims.object_id)
           << L"\n";

    output << L"Client ID: "
           << utf8_to_wide(claims.client_id)
           << L"\n\n";

    output << L"TIME CLAIMS\n";
    output << L"-----------\n";

    output << L"Issued at: "
           << optional_time(claims.issued_at)
           << L"\n";

    output << L"Not before: "
           << optional_time(claims.not_before)
           << L"\n";

    output << L"Expires at: "
           << optional_time(claims.expires_at)
           << L"\n";

    output << L"Expired: "
           << (claims.expired ? L"YES" : L"NO")
           << L"\n";

    output << L"Not yet valid: "
           << (claims.not_yet_valid ? L"YES" : L"NO")
           << L"\n\n";

    output << L"ASSESSMENT FINDINGS\n";
    output << L"-------------------\n";

    for (const auto& finding : inspection.findings) {
        output << L"\n[" << utf8_to_wide(finding.id)
               << L"] "
               << utf8_to_wide(finding.title)
               << L"\n";

        output << utf8_to_wide(finding.description)
               << L"\n";

        output << L"Recommendation: "
               << utf8_to_wide(finding.recommendation)
               << L"\n";
    }

    output << L"\nSECURITY LIMITATION\n";
    output << L"-------------------\n";

    output << L"This is a local, decode-only inspection. "
              L"It does not verify signatures, validate "
              L"issuer metadata, contact Microsoft, or "
              L"prove that a token is authentic.\n";

    output << L"\nNever treat decoded claims as proof "
              L"of identity or authorization.\n";

    return output.str();
}

void set_result(const std::wstring& value) {
    if (g_result) {
        SetWindowTextW(g_result, value.c_str());
    }
}

void inspect_token() {
    if (!g_input || !g_result) {
        return;
    }

    const int length = GetWindowTextLengthW(g_input);

    if (length <= 0) {
        set_result(L"Paste a test JWT into the input field first.");
        return;
    }

    if (length > 16384) {
        set_result(L"Input is too large. Maximum size: 16 KB.");
        return;
    }

    std::wstring wide_token(
        static_cast<std::size_t>(length) + 1,
        L'\0'
    );

    GetWindowTextW(
        g_input,
        wide_token.data(),
        length + 1
    );

    wide_token.resize(static_cast<std::size_t>(length));

    const int required = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        wide_token.data(),
        length,
        nullptr,
        0,
        nullptr,
        nullptr
    );

    if (required <= 0) {
        set_result(L"Input could not be converted to UTF-8.");
        return;
    }

    std::string token(
        static_cast<std::size_t>(required),
        '\0'
    );

    WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        wide_token.data(),
        length,
        token.data(),
        required,
        nullptr,
        nullptr
    );

    const auto inspection =
        sixerc::session::inspect_jwt(token);

    // Do not retain the pasted token in the UI module.
    token.clear();
    token.shrink_to_fit();
    wide_token.clear();

    set_result(make_result(inspection));
}

void create_controls(HWND hwnd) {
    g_font = CreateFontW(
        15, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH,
        L"Segoe UI"
    );

    g_heading_font = CreateFontW(
        19, 0, 0, 0, FW_BOLD,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH,
        L"Segoe UI"
    );

    HWND heading = CreateWindowExW(
        0,
        L"STATIC",
        L"EKWENSU / SESSION INTELLIGENCE",
        WS_CHILD | WS_VISIBLE,
        24, 18, 600, 32,
        hwnd,
        nullptr,
        nullptr,
        nullptr
    );

    SendMessageW(
        heading,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(g_heading_font),
        TRUE
    );

    HWND warning = CreateWindowExW(
        0,
        L"STATIC",
        L"LOCAL JWT DECODER  |  SIGNATURE NOT VERIFIED",
        WS_CHILD | WS_VISIBLE,
        25, 53, 700, 24,
        hwnd,
        nullptr,
        nullptr,
        nullptr
    );

    SendMessageW(
        warning,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(g_font),
        TRUE
    );

    HWND input_label = CreateWindowExW(
        0,
        L"STATIC",
        L"TEST TOKEN INPUT",
        WS_CHILD | WS_VISIBLE,
        25, 91, 300, 22,
        hwnd,
        nullptr,
        nullptr,
        nullptr
    );

    SendMessageW(
        input_label,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(g_font),
        TRUE
    );

    g_input = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE |
        ES_MULTILINE | ES_AUTOVSCROLL |
        WS_VSCROLL,
        25, 118, 720, 105,
        hwnd,
        reinterpret_cast<HMENU>(ID_TOKEN_INPUT),
        nullptr,
        nullptr
    );

    SendMessageW(
        g_input,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(g_font),
        TRUE
    );

    SendMessageW(
        g_input,
        EM_LIMITTEXT,
        16384,
        0
    );

    HWND inspect = CreateWindowExW(
        0,
        L"BUTTON",
        L"INSPECT CLAIMS",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        25, 238, 170, 38,
        hwnd,
        reinterpret_cast<HMENU>(ID_INSPECT),
        nullptr,
        nullptr
    );

    SendMessageW(
        inspect,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(g_font),
        TRUE
    );

    HWND clear = CreateWindowExW(
        0,
        L"BUTTON",
        L"CLEAR",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        210, 238, 110, 38,
        hwnd,
        reinterpret_cast<HMENU>(ID_CLEAR),
        nullptr,
        nullptr
    );

    SendMessageW(
        clear,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(g_font),
        TRUE
    );

    HWND result_label = CreateWindowExW(
        0,
        L"STATIC",
        L"INSPECTION OUTPUT",
        WS_CHILD | WS_VISIBLE,
        25, 294, 300, 22,
        hwnd,
        nullptr,
        nullptr,
        nullptr
    );

    SendMessageW(
        result_label,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(g_font),
        TRUE
    );

    g_result = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"Paste a test JWT and select INSPECT CLAIMS.\r\n\r\n"
        L"Warning: decoded claims are unverified.",
        WS_CHILD | WS_VISIBLE |
        ES_MULTILINE | ES_AUTOVSCROLL |
        ES_READONLY | WS_VSCROLL,
        25, 321, 720, 290,
        hwnd,
        reinterpret_cast<HMENU>(ID_RESULT),
        nullptr,
        nullptr
    );

    SendMessageW(
        g_result,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(g_font),
        TRUE
    );
}

LRESULT CALLBACK inspector_proc(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam
) {
    switch (message) {
    case WM_CREATE:
        create_controls(hwnd);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case ID_INSPECT:
            inspect_token();
            return 0;

        case ID_CLEAR:
            SetWindowTextW(g_input, L"");
            set_result(L"Input and output cleared.");
            return 0;
        }
        break;

    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wparam);
        SetBkColor(dc, BACKGROUND);
        SetTextColor(dc, GOLD);

        static HBRUSH brush =
            CreateSolidBrush(BACKGROUND);

        return reinterpret_cast<LRESULT>(brush);
    }

    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wparam);
        SetBkColor(dc, PANEL);
        SetTextColor(dc, WHITE);

        static HBRUSH brush =
            CreateSolidBrush(PANEL);

        return reinterpret_cast<LRESULT>(brush);
    }

    case WM_CTLCOLORBTN: {
        HDC dc = reinterpret_cast<HDC>(wparam);
        SetBkColor(dc, RED);
        SetTextColor(dc, WHITE);

        static HBRUSH brush =
            CreateSolidBrush(RED);

        return reinterpret_cast<LRESULT>(brush);
    }

    case WM_DESTROY:
        if (g_font) {
            DeleteObject(g_font);
            g_font = nullptr;
        }

        if (g_heading_font) {
            DeleteObject(g_heading_font);
            g_heading_font = nullptr;
        }

        g_input = nullptr;
        g_result = nullptr;
        g_window = nullptr;
        return 0;

    default:
        return DefWindowProcW(
            hwnd,
            message,
            wparam,
            lparam
        );
    }

    return DefWindowProcW(
        hwnd,
        message,
        wparam,
        lparam
    );
}

} // namespace

void open_session_inspector(HWND owner) {
    if (g_window && IsWindow(g_window)) {
        ShowWindow(g_window, SW_SHOW);
        SetForegroundWindow(g_window);
        return;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = inspector_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(BACKGROUND);
    wc.lpszClassName = CLASS_NAME;

    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        CLASS_NAME,
        WINDOW_NAME,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        800,
        700,
        owner,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr
    );

    if (!hwnd) {
        DeleteObject(wc.hbrBackground);
        return;
    }

    g_window = hwnd;
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    DeleteObject(wc.hbrBackground);
}

} // namespace sixerc::ui
