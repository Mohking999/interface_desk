#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <vector>
#include <string>

struct CommandItem {
    const wchar_t* title;
    const wchar_t* subtitle;
    const wchar_t* shortcut;
    const wchar_t* icon;
    const wchar_t* previewTitle;
    const wchar_t* previewPath;
    const wchar_t* kind;
    const wchar_t* contents;
    const wchar_t* lastOpened;
    const wchar_t* primaryAction;
};

static const std::vector<CommandItem> g_items = {
    {L"Open Downloads", L"Jump to your downloaded files", L"↵", L"▣", L"Downloads", L"/Users/you/Downloads", L"Folder", L"24 items", L"Today, 10:42", L"Open in Finder"},
    {L"Create a quick note", L"Capture a thought in a new document", L"⌘ N", L"✎", L"Quick Note", L"/Users/you/Documents/Notes", L"Command", L"Markdown Template", L"Today, 09:15", L"Create Note"},
    {L"Toggle Do Not Disturb", L"Silence notifications and stay focused", L"System", L"☾", L"Do Not Disturb", L"System Settings / Focus", L"System Toggle", L"Status: Off", L"Yesterday, 22:00", L"Toggle Focus"},
    {L"Weekly review.md", L"Documents / Notes", L"12m ago", L"≡", L"Weekly review.md", L"/Users/you/Documents/Notes/Weekly review.md", L"Markdown Document", L"4.2 KB", L"12m ago", L"Open Document"},
    {L"Interface exploration.fig", L"Documents / Design", L"38m ago", L"◌", L"Interface exploration.fig", L"/Users/you/Documents/Design/Interface exploration.fig", L"Figma Design File", L"18.7 MB", L"38m ago", L"Open in Figma"},
    {L"Terminal", L"Applications / Utilities", L"1h ago", L"⌘", L"Terminal.app", L"/System/Applications/Utilities/Terminal.app", L"Application", L"Version 2.14", L"1h ago", L"Launch Application"},
};

struct AppState {
    int selectedIndex = 0;
    int hoverIndex = -1;
    bool showPreview = true;
    bool isDragging = false;
};

static AppState g_state;

static HWND g_hwnd = nullptr;
static HFONT g_fontTitle = nullptr;
static HFONT g_fontBody = nullptr;
static HFONT g_fontSmall = nullptr;

static void EnsureFonts() {
    if (g_fontTitle != nullptr) return;

    g_fontTitle = CreateFontW(
        20, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    g_fontBody = CreateFontW(
        14, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    g_fontSmall = CreateFontW(
        12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

static HBRUSH GetBrush(COLORREF c) {
    return CreateSolidBrush(c);
}

static void DrawRoundedRect(HDC hdc, RECT r, int radius, HBRUSH brush, bool stroke = false, COLORREF strokeColor = 0) {
    if (radius <= 0) {
        FillRect(hdc, &r, brush);
        if (stroke) {
            HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            HPEN pen = CreatePen(PS_SOLID, 1, strokeColor);
            HPEN oldPen = (HPEN)SelectObject(hdc, pen);
            Rectangle(hdc, r.left, r.top, r.right, r.bottom);
            SelectObject(hdc, oldPen);
            DeleteObject(pen);
            SelectObject(hdc, oldBrush);
        }
        return;
    }

    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    HPEN oldPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
    RoundRect(hdc, r.left, r.top, r.right, r.bottom, radius, radius);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);

    if (stroke) {
        HBRUSH nullBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        HPEN pen = CreatePen(PS_SOLID, 1, strokeColor);
        HPEN old = (HPEN)SelectObject(hdc, pen);
        RoundRect(hdc, r.left, r.top, r.right, r.bottom, radius, radius);
        SelectObject(hdc, old);
        DeleteObject(pen);
        SelectObject(hdc, nullBrush);
    }
}

static void DrawTextCentered(HDC hdc, const wchar_t* text, RECT rc, COLORREF color, HFONT font) {
    SetTextColor(hdc, color);
    SetBkMode(hdc, TRANSPARENT);
    HFONT old = (HFONT)SelectObject(hdc, font);
    DrawTextW(hdc, text, -1, &rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    SelectObject(hdc, old);
}

static void DrawTextLeft(HDC hdc, const wchar_t* text, RECT rc, COLORREF color, HFONT font) {
    SetTextColor(hdc, color);
    SetBkMode(hdc, TRANSPARENT);
    HFONT old = (HFONT)SelectObject(hdc, font);
    DrawTextW(hdc, text, -1, &rc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    SelectObject(hdc, old);
}

static int ItemHitTest(int x, int y) {
    RECT rc = { 44, 82, 640, 420 };
    if (x < rc.left || x >= rc.right || y < rc.top || y >= rc.bottom) return -1;

    int rowHeight = 58;
    int startY = rc.top;
    int visibleItems = 6;
    int index = (y - startY) / rowHeight;
    if (index < 0 || index >= visibleItems) return -1;
    return index;
}

static void PaintWindow(HDC hdc, RECT client) {
    // Background
    HBRUSH bgBrush = GetBrush(RGB(17, 17, 17));
    FillRect(hdc, &client, bgBrush);
    DeleteObject(bgBrush);

    // Main panel
    RECT panel = { 60, 30, client.right - 60, client.bottom - 60 };
    HBRUSH panelBrush = GetBrush(RGB(28, 28, 28));
    HBRUSH panelBrush2 = GetBrush(RGB(32, 32, 32));
    HBRUSH borderBrush = GetBrush(RGB(42, 42, 42));

    // panel shadow/padding
    RECT shadow = { panel.left + 4, panel.top + 4, panel.right + 4, panel.bottom + 4 };
    HBRUSH shadowBrush = GetBrush(RGB(0, 0, 0));
    FillRect(hdc, &shadow, shadowBrush);
    DeleteObject(shadowBrush);

    // border/rounded main split
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, panelBrush);
    HPEN oldPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
    RoundRect(hdc, panel.left, panel.top, panel.right, panel.bottom, 18, 18);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);

    // Search box
    RECT search = { 74, 42, 760, 76 };
    HBRUSH boxBrush = GetBrush(RGB(38, 38, 38));
    oldBrush = (HBRUSH)SelectObject(hdc, boxBrush);
    oldPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
    RoundRect(hdc, search.left, search.top, search.right, search.bottom, 12, 12);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(boxBrush);

    // Search icon
    SetTextColor(hdc, RGB(122, 122, 122));
    SetBkMode(hdc, TRANSPARENT);
    HFONT oldFont = (HFONT)SelectObject(hdc, g_fontBody);
    RECT searchIcon = { search.left + 16, search.top + 10, search.left + 40, search.bottom - 8 };
    DrawTextW(hdc, L"⌕", -1, &searchIcon, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    SelectObject(hdc, oldFont);

    // Search placeholder
    RECT searchText = { search.left + 48, search.top + 10, search.right - 64, search.bottom - 8 };
    oldFont = (HFONT)SelectObject(hdc, g_fontBody);
    SetTextColor(hdc, RGB(115, 115, 115));
    DrawTextW(hdc, L"Search apps, files, and commands...", -1, &searchText, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    SelectObject(hdc, oldFont);

    // esc key cap at right
    RECT escRect = { search.right - 52, search.top + 12, search.right - 18, search.top + 34 };
    HBRUSH keyBrush = GetBrush(RGB(38, 38, 38));
    oldBrush = (HBRUSH)SelectObject(hdc, keyBrush);
    oldPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
    RoundRect(hdc, escRect.left, escRect.top, escRect.right, escRect.bottom, 5, 5);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(keyBrush);
    RECT escText = { escRect.left + 2, escRect.top + 2, escRect.right - 2, escRect.bottom - 2 };
    SetTextColor(hdc, RGB(153, 153, 153));
    DrawTextW(hdc, L"esc", -1, &escText, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    // Tabs row
    RECT tabsRect = { 74, 80, 760, 110 };
    int tabStart = tabsRect.left + 6;
    const wchar_t* tabNames[] = { L"All", L"Applications", L"Files", L"Commands" };
    int widths[] = { 36, 100, 42, 72 };
    for (int i = 0; i < 4; ++i) {
        RECT tr = { tabStart, tabsRect.top, tabStart + widths[i], tabsRect.bottom };
        if (i == 0) {
            HBRUSH activeTab = GetBrush(RGB(44, 44, 44));
            oldBrush = (HBRUSH)SelectObject(hdc, activeTab);
            oldPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
            RoundRect(hdc, tr.left, tr.top + 2, tr.right, tr.bottom, 8, 8);
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBrush);
            DeleteObject(activeTab);
        }
        RECT txt = { tr.left + 4, tr.top + 4, tr.right - 4, tr.bottom };
        SetTextColor(hdc, i == 0 ? RGB(244, 244, 244) : RGB(128, 128, 128));
        oldFont = (HFONT)SelectObject(hdc, g_fontSmall);
        DrawTextW(hdc, tabNames[i], -1, &txt, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        SelectObject(hdc, oldFont);
        tabStart += widths[i] + 7;
    }

    // Switch hint right side
    RECT switchRect = { tabsRect.right - 110, 86, tabsRect.right - 12, 104 };
    SetTextColor(hdc, RGB(115, 115, 115));
    oldFont = (HFONT)SelectObject(hdc, g_fontSmall);
    DrawTextW(hdc, L"Switch", -1, &switchRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    SelectObject(hdc, oldFont);

    RECT keyBox = { tabsRect.right - 34, 88, tabsRect.right - 16, 105 };
    HBRUSH kBrush = GetBrush(RGB(38, 38, 38));
    oldBrush = (HBRUSH)SelectObject(hdc, kBrush);
    oldPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
    RoundRect(hdc, keyBox.left, keyBox.top, keyBox.right, keyBox.bottom, 5, 5);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(kBrush);
    RECT kText = { keyBox.left + 2, keyBox.top + 2, keyBox.right - 2, keyBox.bottom - 2 };
    SetTextColor(hdc, RGB(157, 157, 157));
    oldFont = (HFONT)SelectObject(hdc, g_fontSmall);
    DrawTextW(hdc, L"⇥", -1, &kText, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    SelectObject(hdc, oldFont);

    // Content area split
    RECT content = { 70, 110, 760, 462 };
    RECT listRect = { 70, 110, 510, 462 };
    HBRUSH leftArea = GetBrush(RGB(28, 28, 28));
    FillRect(hdc, &listRect, leftArea);
    DeleteObject(leftArea);

    if (g_state.showPreview) {
        RECT previewRect = { 510, 110, 760, 462 };
        HBRUSH prevBrush = GetBrush(RGB(26, 26, 26));
        FillRect(hdc, &previewRect, prevBrush);
        DeleteObject(prevBrush);
    }

    // List items
    const int startY = 128;
    const int rowHeight = 58;
    for (int i = 0; i < (int)g_items.size(); ++i) {
        RECT itemRect = { 86, startY + i * rowHeight, 494, startY + i * rowHeight + 50 };
        bool selected = (g_state.selectedIndex == i);
        bool hovered = (g_state.hoverIndex == i);
        if (selected || hovered) {
            HBRUSH listHover = GetBrush(selected ? RGB(237, 237, 237) : RGB(37, 37, 37));
            oldBrush = (HBRUSH)SelectObject(hdc, listHover);
            oldPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
            RoundRect(hdc, itemRect.left, itemRect.top, itemRect.right, itemRect.bottom, 12, 12);
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBrush);
            DeleteObject(listHover);
        }

        RECT iconRect = { itemRect.left + 10, itemRect.top + 15, itemRect.left + 32, itemRect.bottom - 15 };
        SetTextColor(hdc, selected ? RGB(29, 29, 29) : RGB(140, 140, 140));
        oldFont = (HFONT)SelectObject(hdc, g_fontBody);
        DrawTextW(hdc, g_items[i].icon, -1, &iconRect, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
        SelectObject(hdc, oldFont);

        RECT titleRect = { itemRect.left + 44, itemRect.top + 8, itemRect.right - 70, itemRect.top + 24 };
        SetTextColor(hdc, selected ? RGB(20, 20, 20) : RGB(237, 237, 237));
        oldFont = (HFONT)SelectObject(hdc, g_fontBody);
        DrawTextW(hdc, g_items[i].title, -1, &titleRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        SelectObject(hdc, oldFont);

        RECT subRect = { itemRect.left + 44, itemRect.top + 28, itemRect.right - 70, itemRect.bottom - 8 };
        SetTextColor(hdc, selected ? RGB(90, 90, 90) : RGB(120, 120, 120));
        oldFont = (HFONT)SelectObject(hdc, g_fontSmall);
        DrawTextW(hdc, g_items[i].subtitle, -1, &subRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        SelectObject(hdc, oldFont);

        RECT badgeRect = { itemRect.right - 62, itemRect.top + 13, itemRect.right - 12, itemRect.top + 35 };
        HBRUSH badgeBrush = GetBrush(selected ? RGB(217, 217, 217) : RGB(38, 38, 38));
        oldBrush = (HBRUSH)SelectObject(hdc, badgeBrush);
        oldPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
        RoundRect(hdc, badgeRect.left, badgeRect.top, badgeRect.right, badgeRect.bottom, 5, 5);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(badgeBrush);

        RECT badgeText = { badgeRect.left + 2, badgeRect.top + 2, badgeRect.right - 2, badgeRect.bottom - 2 };
        SetTextColor(hdc, selected ? RGB(51, 51, 51) : RGB(138, 138, 138));
        oldFont = (HFONT)SelectObject(hdc, g_fontSmall);
        DrawTextW(hdc, g_items[i].shortcut, -1, &badgeText, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        SelectObject(hdc, oldFont);
    }

    // Preview pane
    if (g_state.showPreview) {
        RECT pv = { 526, 124, 744, 418 };

        // Preview icon square
        RECT iconBox = { pv.left + 6, pv.top + 12, pv.left + 82, pv.top + 88 };
        HBRUSH iconBrush = GetBrush(RGB(36, 36, 36));
        oldBrush = (HBRUSH)SelectObject(hdc, iconBrush);
        oldPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
        RoundRect(hdc, iconBox.left, iconBox.top, iconBox.right, iconBox.bottom, 18, 18);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(iconBrush);

        RECT iconTxt = { iconBox.left, iconBox.top, iconBox.right, iconBox.bottom };
        SetTextColor(hdc, RGB(212, 212, 212));
        oldFont = (HFONT)SelectObject(hdc, g_fontTitle);
        DrawTextW(hdc, g_items[g_state.selectedIndex].icon, -1, &iconTxt, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        SelectObject(hdc, oldFont);

        RECT titleRect = { pv.left + 2, pv.top + 98, pv.right - 6, pv.top + 132 };
        SetTextColor(hdc, RGB(245, 245, 245));
        oldFont = (HFONT)SelectObject(hdc, g_fontTitle);
        DrawTextW(hdc, g_items[g_state.selectedIndex].previewTitle, -1, &titleRect, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
        SelectObject(hdc, oldFont);

        RECT pathRect = { pv.left + 2, pv.top + 130, pv.right - 6, pv.top + 156 };
        SetTextColor(hdc, RGB(115, 115, 115));
        oldFont = (HFONT)SelectObject(hdc, g_fontSmall);
        DrawTextW(hdc, g_items[g_state.selectedIndex].previewPath, -1, &pathRect, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
        SelectObject(hdc, oldFont);

        // Metadata rows
        RECT metabox = { pv.left + 4, pv.top + 170, pv.right - 4, pv.top + 278 };
        HBRUSH metaBrush = GetBrush(RGB(26, 26, 26));
        FillRect(hdc, &metabox, metaBrush);
        DeleteObject(metaBrush);

        const wchar_t* labels[] = { L"Kind", L"Contents", L"Last opened" };
        const wchar_t* values[] = {
            g_items[g_state.selectedIndex].kind,
            g_items[g_state.selectedIndex].contents,
            g_items[g_state.selectedIndex].lastOpened
        };
        for (int i = 0; i < 3; ++i) {
            RECT row = { pv.left + 6, pv.top + 180 + i * 26, pv.right - 8, pv.top + 203 + i * 26 };
            RECT lbl = { row.left, row.top, row.right - 90, row.bottom };
            RECT val = { row.right - 130, row.top, row.right - 4, row.bottom };
            SetTextColor(hdc, RGB(115, 115, 115));
            oldFont = (HFONT)SelectObject(hdc, g_fontSmall);
            DrawTextW(hdc, labels[i], -1, &lbl, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
            SetTextColor(hdc, RGB(212, 212, 212));
            DrawTextW(hdc, values[i], -1, &val, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
            SelectObject(hdc, oldFont);
        }

        // Buttons
        RECT mainBtn = { pv.left + 4, pv.top + 305, pv.right - 4, pv.top + 342 };
        HBRUSH actionBrush = GetBrush(RGB(42, 42, 42));
        oldBrush = (HBRUSH)SelectObject(hdc, actionBrush);
        oldPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
        RoundRect(hdc, mainBtn.left, mainBtn.top, mainBtn.right, mainBtn.bottom, 10, 10);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(actionBrush);

        RECT mainLabel = { mainBtn.left + 12, mainBtn.top + 8, mainBtn.right - 28, mainBtn.bottom - 8 };
        SetTextColor(hdc, RGB(255, 255, 255));
        oldFont = (HFONT)SelectObject(hdc, g_fontSmall);
        DrawTextW(hdc, g_items[g_state.selectedIndex].primaryAction, -1, &mainLabel, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
        SelectObject(hdc, oldFont);

        // action arrow
        RECT arrowRect = { mainBtn.right - 26, mainBtn.top + 12, mainBtn.right - 10, mainBtn.bottom - 10 };
        SetTextColor(hdc, RGB(153, 153, 153));
        oldFont = (HFONT)SelectObject(hdc, g_fontSmall);
        DrawTextW(hdc, L"↵", -1, &arrowRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        SelectObject(hdc, oldFont);

        RECT copyBtn = { pv.left + 4, pv.top + 348, pv.right - 4, pv.top + 378 };
        SetTextColor(hdc, RGB(138, 138, 138));
        oldFont = (HFONT)SelectObject(hdc, g_fontSmall);
        DrawTextW(hdc, L"Copy path", -1, &copyBtn, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
        SelectObject(hdc, oldFont);
        RECT copyKey = { pv.right - 72, pv.top + 352, pv.right - 10, pv.top + 374 };
        DrawTextW(hdc, L"⌘ ⇧ C", -1, &copyKey, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    }

    // Footer bar
    RECT footer = { 70, 462, 760, 496 };
    HBRUSH footerBrush = GetBrush(RGB(24, 24, 24));
    FillRect(hdc, &footer, footerBrush);
    DeleteObject(footerBrush);

    RECT footerLeft = { 84, 468, 250, 490 };
    SetTextColor(hdc, RGB(120, 120, 120));
    oldFont = (HFONT)SelectObject(hdc, g_fontSmall);
    DrawTextW(hdc, L"Navigate", -1, &footerLeft, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
    SelectObject(hdc, oldFont);

    RECT footerRight = { 610, 468, 760, 490 };
    SetTextColor(hdc, RGB(120, 120, 120));
    DrawTextW(hdc, L"Actions", -1, &footerRight, DT_SINGLELINE | DT_LEFT | DT_VCENTER);

    // Bottom caption
    RECT caption = { 70, 500, 760, 532 };
    SetTextColor(hdc, RGB(115, 115, 115));
    oldFont = (HFONT)SelectObject(hdc, g_fontSmall);
    DrawTextW(hdc, L"A little less clicking —", -1, &caption, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
    SelectObject(hdc, oldFont);

    RECT kcap = { 238, 502, 280, 530 };
    HBRUSH keycapBrush = GetBrush(RGB(34, 34, 34));
    oldBrush = (HBRUSH)SelectObject(hdc, keycapBrush);
    oldPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
    RoundRect(hdc, kcap.left, kcap.top, kcap.right, kcap.bottom, 5, 5);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(keycapBrush);
    RECT kcapText = { kcap.left, kcap.top, kcap.right, kcap.bottom };
    SetTextColor(hdc, RGB(176, 176, 176));
    oldFont = (HFONT)SelectObject(hdc, g_fontSmall);
    DrawTextW(hdc, L"⌘ K", -1, &kcapText, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    SelectObject(hdc, oldFont);

    RECT captionEnd = { 300, 500, 530, 532 };
    DrawTextW(hdc, L"from anywhere.", -1, &captionEnd, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
    RECT workspace = { 640, 500, 760, 532 };
    DrawTextW(hdc, L"Personal workspace", -1, &workspace, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        g_hwnd = hwnd;
        EnsureFonts();
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        PaintWindow(ps.hdc, ps.rcPaint);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        int idx = ItemHitTest(x, y);
        if (idx != -1) {
            g_state.hoverIndex = idx;
        } else {
            g_state.hoverIndex = -1;
        }
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        int idx = ItemHitTest(x, y);
        if (idx >= 0) {
            g_state.selectedIndex = idx;
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    }

    case WM_KEYDOWN:
        if (wParam == VK_UP) {
            g_state.selectedIndex = (g_state.selectedIndex - 1 + (int)g_items.size()) % (int)g_items.size();
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }
        if (wParam == VK_DOWN) {
            g_state.selectedIndex = (g_state.selectedIndex + 1) % (int)g_items.size();
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }
        if (wParam == VK_TAB) {
            g_state.selectedIndex = (g_state.selectedIndex + 1) % (int)g_items.size();
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"CommandPaletteWin32";

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Command Palette",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 920, 620,
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
