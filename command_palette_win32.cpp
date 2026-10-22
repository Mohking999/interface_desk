#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <vector>
#include <string>
#include <cwctype>

struct CommandItem {
    const wchar_t* section;      // "Suggested" or "Recent"
    const wchar_t* category;     // "Commands", "Files", "Applications"
    const wchar_t* title;
    const wchar_t* subtitle;
    const wchar_t* shortcut;
    bool isBadge;
    const wchar_t* icon;
    const wchar_t* previewTitle;
    const wchar_t* previewPath;
    const wchar_t* kind;
    const wchar_t* contents;
    const wchar_t* lastOpened;
    const wchar_t* primaryAction;
};

static const std::vector<CommandItem> g_allItems = {
    {L"Suggested", L"Commands", L"Open Downloads", L"Jump to your downloaded files", L"↵", true, L"▣", L"Downloads", L"/Users/you/Downloads", L"Folder", L"24 items", L"Today, 10:42", L"Open in Finder"},
    {L"Suggested", L"Commands", L"Create a quick note", L"Capture a thought in a new document", L"⌘ N", false, L"✎", L"Quick Note", L"/Users/you/Documents/Notes", L"Command", L"Markdown Template", L"Today, 09:15", L"Create Note"},
    {L"Suggested", L"Commands", L"Toggle Do Not Disturb", L"Silence notifications and stay focused", L"System", false, L"☾", L"Do Not Disturb", L"System Settings / Focus", L"System Toggle", L"Status: Off", L"Yesterday, 22:00", L"Toggle Focus"},
    {L"Recent", L"Files", L"Weekly review.md", L"Documents / Notes", L"12m ago", false, L"≡", L"Weekly review.md", L"/Users/you/Documents/Notes/Weekly review.md", L"Markdown Document", L"4.2 KB", L"12m ago", L"Open Document"},
    {L"Recent", L"Files", L"Interface exploration.fig", L"Documents / Design", L"38m ago", false, L"❖", L"Interface exploration.fig", L"/Users/you/Documents/Design/Interface exploration.fig", L"Figma Design File", L"18.7 MB", L"38m ago", L"Open in Figma"},
    {L"Recent", L"Applications", L"Terminal", L"Applications / Utilities", L"1h ago", false, L">_", L"Terminal.app", L"/System/Applications/Utilities/Terminal.app", L"Application", L"Version 2.14", L"1h ago", L"Launch Application"},
};

static const wchar_t* g_tabs[] = { L"All", L"Applications", L"Files", L"Commands" };

struct RenderedRow {
    int itemIndex; // Index into g_filteredItems
    RECT bounds;
};

struct AppState {
    int selectedIndex = 0;
    int hoverIndex = -1;
    int activeTab = 0;
    bool showPreview = true;
    std::wstring searchQuery;
    std::vector<CommandItem> filteredItems;
    std::vector<RenderedRow> rowHitBoxes;
    RECT tabHitBoxes[4] = {};
};

static AppState g_state;
static HWND g_hwnd = nullptr;

// Cached Fonts
static HFONT g_fontSearch = nullptr;
static HFONT g_fontTitle = nullptr;
static HFONT g_fontBody = nullptr;
static HFONT g_fontSmall = nullptr;
static HFONT g_fontTinyBold = nullptr;
static HFONT g_fontPreviewIcon = nullptr;

static bool ContainsCaseInsensitive(const std::wstring& haystack, const std::wstring& needle) {
    if (needle.empty()) return true;
    for (size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        bool match = true;
        for (size_t j = 0; j < needle.size(); ++j) {
            if (towlower(haystack[i + j]) != towlower(needle[j])) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

static void RebuildFilteredList() {
    g_state.filteredItems.clear();
    const wchar_t* activeTabName = g_tabs[g_state.activeTab];

    for (const auto& item : g_allItems) {
        bool tabMatch = (g_state.activeTab == 0) || (wcscmp(item.category, activeTabName) == 0);
        bool searchMatch = ContainsCaseInsensitive(item.title, g_state.searchQuery) ||
                           ContainsCaseInsensitive(item.subtitle, g_state.searchQuery);
        if (tabMatch && searchMatch) {
            g_state.filteredItems.push_back(item);
        }
    }

    if (g_state.selectedIndex >= (int)g_state.filteredItems.size()) {
        g_state.selectedIndex = g_state.filteredItems.empty() ? 0 : (int)g_state.filteredItems.size() - 1;
    }
    g_state.hoverIndex = -1;
}

static void EnsureFonts() {
    if (g_fontTitle != nullptr) return;

    g_fontSearch = CreateFontW(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    g_fontTitle = CreateFontW(22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    g_fontBody = CreateFontW(15, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    g_fontSmall = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    g_fontTinyBold = CreateFontW(11, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    g_fontPreviewIcon = CreateFontW(28, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Symbol");
}

static void CleanupFonts() {
    if (g_fontSearch) DeleteObject(g_fontSearch);
    if (g_fontTitle) DeleteObject(g_fontTitle);
    if (g_fontBody) DeleteObject(g_fontBody);
    if (g_fontSmall) DeleteObject(g_fontSmall);
    if (g_fontTinyBold) DeleteObject(g_fontTinyBold);
    if (g_fontPreviewIcon) DeleteObject(g_fontPreviewIcon);
}

static void FillRoundBox(HDC hdc, RECT r, int radius, COLORREF fill, bool hasBorder = false, COLORREF border = 0) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = hasBorder ? CreatePen(PS_SOLID, 1, border) : (HPEN)GetStockObject(NULL_PEN);

    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);

    RoundRect(hdc, r.left, r.top, r.right, r.bottom, radius, radius);

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);

    if (hasBorder) DeleteObject(pen);
    DeleteObject(brush);
}

static void DrawLine(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HPEN old = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, x1, y1, nullptr);
    LineTo(hdc, x2, y2);
    SelectObject(hdc, old);
    DeleteObject(pen);
}

static void DrawLabel(HDC hdc, const wchar_t* text, RECT rc, COLORREF color, HFONT font, UINT format = DT_SINGLELINE | DT_LEFT | DT_VCENTER) {
    SetTextColor(hdc, color);
    SetBkMode(hdc, TRANSPARENT);
    HFONT old = (HFONT)SelectObject(hdc, font);
    DrawTextW(hdc, text, -1, &rc, format);
    SelectObject(hdc, old);
}

static void PaintWindow(HDC hdc, RECT client) {
    // 1. Outer Dark Canvas Background
    HBRUSH bgBrush = CreateSolidBrush(RGB(17, 17, 17));
    FillRect(hdc, &client, bgBrush);
    DeleteObject(bgBrush);

    // 2. Main Command Palette Card
    RECT panel = { 40, 24, 864, 528 };
    FillRoundBox(hdc, panel, 20, RGB(28, 28, 28), true, RGB(46, 46, 46));

    // 3. Top Search Bar Row
    RECT searchIconRc = { panel.left + 20, panel.top + 16, panel.left + 44, panel.top + 48 };
    DrawLabel(hdc, L"⌕", searchIconRc, RGB(122, 122, 122), g_fontTitle, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    RECT searchTextRc = { panel.left + 50, panel.top + 16, panel.right - 70, panel.top + 48 };
    if (g_state.searchQuery.empty()) {
        DrawLabel(hdc, L"Search apps, files, and commands...", searchTextRc, RGB(115, 115, 115), g_fontSearch);
    } else {
        std::wstring displayQuery = g_state.searchQuery + L"|";
        DrawLabel(hdc, displayQuery.c_str(), searchTextRc, RGB(240, 240, 240), g_fontSearch);
    }

    // ESC Badge
    RECT escRect = { panel.right - 56, panel.top + 20, panel.right - 20, panel.top + 42 };
    FillRoundBox(hdc, escRect, 6, RGB(38, 38, 38), true, RGB(54, 54, 54));
    DrawLabel(hdc, L"esc", escRect, RGB(145, 145, 145), g_fontSmall, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    // 4. Filter Tabs Row
    int tabY = panel.top + 56;
    int tabX = panel.left + 20;
    int tabWidths[] = { 38, 92, 48, 84 };

    for (int i = 0; i < 4; ++i) {
        RECT tr = { tabX, tabY, tabX + tabWidths[i], tabY + 26 };
        g_state.tabHitBoxes[i] = tr;
        if (g_state.activeTab == i) {
            FillRoundBox(hdc, tr, 8, RGB(44, 44, 44), true, RGB(58, 58, 58));
        }
        DrawLabel(hdc, g_tabs[i], tr, g_state.activeTab == i ? RGB(245, 245, 245) : RGB(128, 128, 128),
            g_fontSmall, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        tabX += tabWidths[i] + 8;
    }

    // Switch ⇥ Hint
    RECT switchTxt = { panel.right - 92, tabY, panel.right - 46, tabY + 26 };
    DrawLabel(hdc, L"Switch", switchTxt, RGB(115, 115, 115), g_fontSmall, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    RECT tabKey = { panel.right - 40, tabY + 3, panel.right - 20, tabY + 23 };
    FillRoundBox(hdc, tabKey, 5, RGB(38, 38, 38), true, RGB(54, 54, 54));
    DrawLabel(hdc, L"⇥", tabKey, RGB(150, 150, 150), g_fontSmall, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    // Horizontal divider under tabs
    int bodyTop = panel.top + 92;
    int bodyBottom = panel.bottom - 40;
    DrawLine(hdc, panel.left, bodyTop, panel.right, bodyTop, RGB(42, 42, 42));

    // Vertical split divider between List and Preview
    int splitX = panel.left + 520;
    if (g_state.showPreview) {
        DrawLine(hdc, splitX, bodyTop, splitX, bodyBottom, RGB(42, 42, 42));
    }

    // 5. Left Column: Sectioned Command & File List
    g_state.rowHitBoxes.clear();
    int curY = bodyTop + 12;
    int listLeft = panel.left + 12;
    int listRight = (g_state.showPreview ? splitX : panel.right) - 12;

    const wchar_t* sections[] = { L"Suggested", L"Recent" };
    const wchar_t* sectionRight[] = { L"COMMANDS", L"LAST OPENED" };

    for (int s = 0; s < 2; ++s) {
        // Check if any filtered items belong to this section
        bool hasSectionItems = false;
        for (const auto& item : g_state.filteredItems) {
            if (wcscmp(item.section, sections[s]) == 0) {
                hasSectionItems = true;
                break;
            }
        }
        if (!hasSectionItems) continue;

        // Section Header
        RECT secLeftRc = { listLeft + 10, curY, listLeft + 200, curY + 20 };
        RECT secRightRc = { listRight - 140, curY, listRight - 10, curY + 20 };
        DrawLabel(hdc, sections[s], secLeftRc, RGB(117, 117, 117), g_fontSmall);
        DrawLabel(hdc, sectionRight[s], secRightRc, RGB(105, 105, 105), g_fontTinyBold, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
        curY += 24;

        // Render items in this section
        for (int i = 0; i < (int)g_state.filteredItems.size(); ++i) {
            const auto& item = g_state.filteredItems[i];
            if (wcscmp(item.section, sections[s]) != 0) continue;

            RECT rowRc = { listLeft, curY, listRight, curY + 50 };
            g_state.rowHitBoxes.push_back({ i, rowRc });

            bool selected = (g_state.selectedIndex == i);
            bool hovered = (g_state.hoverIndex == i);

            if (selected) {
                FillRoundBox(hdc, rowRc, 12, RGB(237, 237, 237));
            } else if (hovered) {
                FillRoundBox(hdc, rowRc, 12, RGB(38, 38, 38));
            }

            // Icon
            RECT iconRc = { rowRc.left + 12, rowRc.top, rowRc.left + 38, rowRc.bottom };
            DrawLabel(hdc, item.icon, iconRc, selected ? RGB(24, 24, 24) : RGB(145, 145, 145),
                g_fontBody, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

            // Title & Subtitle
            RECT titleRc = { rowRc.left + 46, rowRc.top + 7, rowRc.right - 80, rowRc.top + 26 };
            DrawLabel(hdc, item.title, titleRc, selected ? RGB(20, 20, 20) : RGB(237, 237, 237), g_fontBody);

            RECT subRc = { rowRc.left + 46, rowRc.top + 26, rowRc.right - 80, rowRc.bottom - 6 };
            DrawLabel(hdc, item.subtitle, subRc, selected ? RGB(90, 90, 90) : RGB(120, 120, 120), g_fontSmall);

            // Right label or keycap badge
            if (item.isBadge) {
                RECT badgeRc = { rowRc.right - 36, rowRc.top + 14, rowRc.right - 12, rowRc.bottom - 14 };
                FillRoundBox(hdc, badgeRc, 6, selected ? RGB(218, 218, 218) : RGB(38, 38, 38),
                    true, selected ? RGB(195, 195, 195) : RGB(54, 54, 54));
                DrawLabel(hdc, item.shortcut, badgeRc, selected ? RGB(45, 45, 45) : RGB(140, 140, 140),
                    g_fontSmall, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
            } else {
                RECT rightLblRc = { rowRc.right - 90, rowRc.top, rowRc.right - 12, rowRc.bottom };
                DrawLabel(hdc, item.shortcut, rightLblRc, selected ? RGB(85, 85, 85) : RGB(115, 115, 115),
                    g_fontSmall, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
            }

            curY += 52;
        }
        curY += 8; // Gap between sections
    }

    // 6. Right Column: Contextual Preview Pane
    if (g_state.showPreview && !g_state.filteredItems.empty()) {
        const auto& sel = g_state.filteredItems[g_state.selectedIndex];
        int pvLeft = splitX + 20;
        int pvRight = panel.right - 20;
        int pvTop = bodyTop + 14;

        // Preview header
        RECT pvHeaderRc = { pvLeft, pvTop, pvRight, pvTop + 20 };
        DrawLabel(hdc, L"Preview", pvHeaderRc, RGB(120, 120, 120), g_fontSmall);
        DrawLabel(hdc, L"◧", pvHeaderRc, RGB(120, 120, 120), g_fontSmall, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);

        // Large Rounded Icon Box
        RECT iconBox = { pvLeft, pvTop + 30, pvLeft + 72, pvTop + 102 };
        FillRoundBox(hdc, iconBox, 16, RGB(36, 36, 36), true, RGB(48, 48, 48));
        DrawLabel(hdc, sel.icon, iconBox, RGB(220, 220, 220), g_fontPreviewIcon, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

        // Preview Title & Path
        RECT pvTitleRc = { pvLeft, pvTop + 114, pvRight, pvTop + 142 };
        DrawLabel(hdc, sel.previewTitle, pvTitleRc, RGB(245, 245, 245), g_fontTitle);

        RECT pvPathRc = { pvLeft, pvTop + 142, pvRight, pvTop + 162 };
        DrawLabel(hdc, sel.previewPath, pvPathRc, RGB(115, 115, 115), g_fontSmall);

        // Key-Value Metadata Rows
        const wchar_t* metaLabels[] = { L"Kind", L"Contents", L"Last opened" };
        const wchar_t* metaValues[] = { sel.kind, sel.contents, sel.lastOpened };
        int metaY = pvTop + 178;

        for (int i = 0; i < 3; ++i) {
            RECT rowRc = { pvLeft, metaY + i * 24, pvRight, metaY + (i + 1) * 24 };
            DrawLabel(hdc, metaLabels[i], rowRc, RGB(115, 115, 115), g_fontSmall, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
            DrawLabel(hdc, metaValues[i], rowRc, RGB(212, 212, 212), g_fontSmall, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
        }

        // Divider above action buttons
        int actionTop = pvTop + 266;
        DrawLine(hdc, pvLeft, actionTop, pvRight, actionTop, RGB(42, 42, 42));

        // Primary Action Button
        RECT mainBtn = { pvLeft, actionTop + 14, pvRight, actionTop + 50 };
        FillRoundBox(hdc, mainBtn, 10, RGB(42, 42, 42), true, RGB(54, 54, 54));

        RECT mainBtnText = { mainBtn.left + 14, mainBtn.top, mainBtn.right - 34, mainBtn.bottom };
        DrawLabel(hdc, sel.primaryAction, mainBtnText, RGB(245, 245, 245), g_fontSmall);

        RECT mainBtnKey = { mainBtn.right - 30, mainBtn.top, mainBtn.right - 10, mainBtn.bottom };
        DrawLabel(hdc, L"↵", mainBtnKey, RGB(155, 155, 155), g_fontSmall, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

        // Secondary Action ("Copy path")
        RECT copyRow = { pvLeft + 14, actionTop + 56, pvRight - 10, actionTop + 82 };
        DrawLabel(hdc, L"Copy path", copyRow, RGB(138, 138, 138), g_fontSmall);
        DrawLabel(hdc, L"⌘ ⇧ C", copyRow, RGB(110, 110, 110), g_fontSmall, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    }

    // 7. Footer Bar inside Card
    DrawLine(hdc, panel.left, bodyBottom, panel.right, bodyBottom, RGB(42, 42, 42));

    int fY = bodyBottom + 9;
    RECT upKey = { panel.left + 20, fY, panel.left + 40, fY + 22 };
    RECT downKey = { panel.left + 44, fY, panel.left + 64, fY + 22 };
    FillRoundBox(hdc, upKey, 5, RGB(37, 37, 37), true, RGB(52, 52, 52));
    FillRoundBox(hdc, downKey, 5, RGB(37, 37, 37), true, RGB(52, 52, 52));
    DrawLabel(hdc, L"↑", upKey, RGB(150, 150, 150), g_fontSmall, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    DrawLabel(hdc, L"↓", downKey, RGB(150, 150, 150), g_fontSmall, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    RECT navText = { panel.left + 72, fY, panel.left + 135, fY + 22 };
    DrawLabel(hdc, L"Navigate", navText, RGB(120, 120, 120), g_fontSmall);

    RECT enterKey = { panel.left + 145, fY, panel.left + 167, fY + 22 };
    FillRoundBox(hdc, enterKey, 5, RGB(37, 37, 37), true, RGB(52, 52, 52));
    DrawLabel(hdc, L"↵", enterKey, RGB(150, 150, 150), g_fontSmall, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    RECT openText = { panel.left + 175, fY, panel.left + 230, fY + 22 };
    DrawLabel(hdc, L"Open", openText, RGB(120, 120, 120), g_fontSmall);

    // Right side of footer: Actions ⌘ ↵
    RECT actText = { panel.right - 120, fY, panel.right - 70, fY + 22 };
    DrawLabel(hdc, L"Actions", actText, RGB(120, 120, 120), g_fontSmall, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);

    RECT cmdKey = { panel.right - 64, fY, panel.right - 44, fY + 22 };
    RECT retKey = { panel.right - 40, fY, panel.right - 20, fY + 22 };
    FillRoundBox(hdc, cmdKey, 5, RGB(37, 37, 37), true, RGB(52, 52, 52));
    FillRoundBox(hdc, retKey, 5, RGB(37, 37, 37), true, RGB(52, 52, 52));
    DrawLabel(hdc, L"⌘", cmdKey, RGB(150, 150, 150), g_fontSmall, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    DrawLabel(hdc, L"↵", retKey, RGB(150, 150, 150), g_fontSmall, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    // 8. Bottom Presentation Caption (Outside Card)
    int capY = panel.bottom + 14;
    RECT capLeft = { panel.left + 4, capY, panel.left + 145, capY + 24 };
    DrawLabel(hdc, L"A little less clicking —", capLeft, RGB(115, 115, 115), g_fontSmall);

    RECT kCap = { panel.left + 148, capY + 1, panel.left + 184, capY + 23 };
    FillRoundBox(hdc, kCap, 5, RGB(34, 34, 34), true, RGB(51, 51, 51));
    DrawLabel(hdc, L"⌘ K", kCap, RGB(176, 176, 176), g_fontSmall, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    RECT capAfter = { panel.left + 192, capY, panel.left + 320, capY + 24 };
    DrawLabel(hdc, L"from anywhere.", capAfter, RGB(115, 115, 115), g_fontSmall);

    RECT capRight = { panel.right - 160, capY, panel.right - 4, capY + 24 };
    DrawLabel(hdc, L"Personal workspace", capRight, RGB(115, 115, 115), g_fontSmall, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
}

static int HitTestRow(int x, int y) {
    POINT pt = { x, y };
    for (const auto& row : g_state.rowHitBoxes) {
        if (PtInRect(&row.bounds, pt)) {
            return row.itemIndex;
        }
    }
    return -1;
}

static int HitTestTab(int x, int y) {
    POINT pt = { x, y };
    for (int i = 0; i < 4; ++i) {
        if (PtInRect(&g_state.tabHitBoxes[i], pt)) {
            return i;
        }
    }
    return -1;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        g_hwnd = hwnd;
        EnsureFonts();
        RebuildFilteredList();
        return 0;

    case WM_ERASEBKGND:
        // Prevent background erase to eliminate flickering
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT client;
        GetClientRect(hwnd, &client);
        int width = client.right - client.left;
        int height = client.bottom - client.top;

        // Off-screen double buffer
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

        PaintWindow(memDC, client);

        BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        int idx = HitTestRow(x, y);
        if (idx != g_state.hoverIndex) {
            g_state.hoverIndex = idx;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        int tabIdx = HitTestTab(x, y);
        if (tabIdx != -1 && tabIdx != g_state.activeTab) {
            g_state.activeTab = tabIdx;
            RebuildFilteredList();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        int rowIdx = HitTestRow(x, y);
        if (rowIdx >= 0 && rowIdx != g_state.selectedIndex) {
            g_state.selectedIndex = rowIdx;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_CHAR: {
        if (wParam == VK_BACK) {
            if (!g_state.searchQuery.empty()) {
                g_state.searchQuery.pop_back();
                RebuildFilteredList();
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        } else if (wParam >= 32 && wParam != 127) {
            g_state.searchQuery.push_back((wchar_t)wParam);
            RebuildFilteredList();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_KEYDOWN:
        if (wParam == VK_UP && !g_state.filteredItems.empty()) {
            int n = (int)g_state.filteredItems.size();
            g_state.selectedIndex = (g_state.selectedIndex - 1 + n) % n;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (wParam == VK_DOWN && !g_state.filteredItems.empty()) {
            int n = (int)g_state.filteredItems.size();
            g_state.selectedIndex = (g_state.selectedIndex + 1) % n;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (wParam == VK_TAB) {
            g_state.activeTab = (g_state.activeTab + 1) % 4;
            RebuildFilteredList();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            if (!g_state.searchQuery.empty()) {
                g_state.searchQuery.clear();
                RebuildFilteredList();
                InvalidateRect(hwnd, nullptr, FALSE);
            } else {
                PostQuitMessage(0);
            }
            return 0;
        }
        break;

    case WM_DESTROY:
        CleanupFonts();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"CommandPaletteWin32";

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr; // Handled via double-buffering in WM_PAINT
    wc.lpszClassName = CLASS_NAME;
    RegisterClassExW(&wc);

    // Calculate exact window size so client area is 904x580
    RECT desiredClient = { 0, 0, 904, 580 };
    AdjustWindowRectEx(&desiredClient, WS_OVERLAPPEDWINDOW, FALSE, 0);

    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Command Palette",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        desiredClient.right - desiredClient.left,
        desiredClient.bottom - desiredClient.top,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}