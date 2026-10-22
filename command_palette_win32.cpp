#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>
#include <shellapi.h>
#include <commdlg.h>
#include <tchar.h>
#include <algorithm>
#include <cstring>
#include <cwchar>
#include <set>
#include <vector>
#include <string>
#include <cwctype>
#include "deskpilot_version.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comdlg32.lib")

#define HOTKEY_CTRL_K    1001
#define HOTKEY_ALT_SPACE 1002
#define TIMER_TOAST      2001
#define WM_TRAYICON      (WM_USER + 101)
#define ID_TRAY_SHOW     40001
#define ID_TRAY_EXIT     40002
#define ID_TRAY_ABOUT    40003

static constexpr wchar_t kAppName[] = L"DeskPilot";
static constexpr wchar_t kAppVersion[] = L"DeskPilot v" DESKPILOT_VERSION_W;

enum NoteCommand {
    NOTE_NEW = 41001,
    NOTE_OPEN,
    NOTE_SAVE
};

enum ActionType {
    ACT_OPEN_DOWNLOADS,
    ACT_TOGGLE_DND,
    ACT_OPEN_FILE,
    ACT_OPEN_TERMINAL,
    ACT_CREATE_NOTE,
    ACT_OPEN_NOTE,
    ACT_OPEN_APP
};

struct CommandItem {
    std::wstring id;
    std::wstring section;
    std::wstring category;
    std::wstring title;
    std::wstring subtitle;
    std::wstring shortcut;
    bool isBadge;
    std::wstring icon;
    std::wstring previewTitle;
    std::wstring previewPath;
    std::wstring kind;
    std::wstring contents;
    std::wstring lastOpened;
    std::wstring primaryAction;
    ActionType actionType;
};

struct RenderedRow {
    int itemIndex;
    RECT bounds;
};

struct AppState {
    int selectedIndex = 0;
    int hoverIndex = -1;
    int activeTab = 0;
    bool showPreview = true;
    bool dndEnabled = false;
    bool hoverPrimaryBtn = false;
    bool hoverCopyBtn = false;
    bool hoverPreviewToggle = false;
    bool hoverEscBtn = false;
    bool hoverFullscreenBtn = false;
    bool hoverQuitBtn = false;
    bool isFullscreen = false;
    int listScrollOffset = 0;

    std::wstring searchQuery;
    std::wstring toastMessage;
    RECT windowedBounds = {};

    std::vector<CommandItem> allItems;
    std::vector<CommandItem> filteredItems;
    std::vector<RenderedRow> rowHitBoxes;

    RECT tabHitBoxes[4] = {};
    RECT previewToggleBox = {};
    RECT primaryActionBox = {};
    RECT copyPathBox = {};
    RECT escBox = {};
    RECT fullscreenBox = {};
    RECT quitBox = {};
    RECT listBounds = {};
};

static AppState g_state;
static HWND g_hwnd = nullptr;
static HWND g_notesWindow = nullptr;
static HWND g_notesEdit = nullptr;
static std::wstring g_notePath;
static NOTIFYICONDATAW g_nid = {};
static const wchar_t* g_tabs[] = { L"All", L"Applications", L"Files", L"Commands" };

// Polices GDI
static HFONT g_fontSearch = nullptr;
static HFONT g_fontTitle = nullptr;
static HFONT g_fontBody = nullptr;
static HFONT g_fontSmall = nullptr;
static HFONT g_fontTinyBold = nullptr;
static HFONT g_fontPreviewIcon = nullptr;

static std::wstring GetKnownFolder(REFKNOWNFOLDERID rfid, const std::wstring& fallback) {
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(rfid, 0, nullptr, &path)) && path) {
        std::wstring result(path);
        CoTaskMemFree(path);
        return result;
    }
    return fallback;
}

static int CountFolderItems(const std::wstring& folderPath) {
    WIN32_FIND_DATAW fd;
    std::wstring searchMask = folderPath + L"\\*";
    HANDLE hFind = FindFirstFileW(searchMask.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return 24;

    int count = 0;
    do {
        if (wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0) {
            count++;
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
    return count;
}

static std::wstring ToLower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](wchar_t ch) { return static_cast<wchar_t>(towlower(ch)); });
    return value;
}

static void AddAppsFromFolder(const std::wstring& folder, int depth, std::set<std::wstring>& addedPaths) {
    if (folder.empty() || depth > 4) return;

    WIN32_FIND_DATAW data = {};
    HANDLE search = FindFirstFileW((folder + L"\\*").c_str(), &data);
    if (search == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) continue;

        const std::wstring path = folder + L"\\" + data.cFileName;
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            AddAppsFromFolder(path, depth + 1, addedPaths);
            continue;
        }

        const size_t extensionStart = path.find_last_of(L'.');
        const std::wstring extension = extensionStart == std::wstring::npos
            ? L"" : ToLower(path.substr(extensionStart));
        if (extension != L".lnk" && extension != L".url") continue;

        const std::wstring normalizedPath = ToLower(path);
        if (!addedPaths.insert(normalizedPath).second) continue;

        std::wstring title = data.cFileName;
        title.resize(title.find_last_of(L'.'));
        if (title.empty()) continue;

        g_state.allItems.push_back({
            L"app-" + normalizedPath, L"Recent", L"Applications",
            title, L"Installed application", L"App", false, L">",
            title, path, L"Application shortcut", L"Start menu app", L"Available",
            L"Launch " + title, ACT_OPEN_APP
        });
    } while (FindNextFileW(search, &data));

    FindClose(search);
}

static void ShowNotesWindow(bool openExisting, const std::wstring& initialPath = L"");

static void InitItems() {
    std::wstring downloadsPath = GetKnownFolder(FOLDERID_Downloads, L"C:\\Users\\you\\Downloads");
    std::wstring docsPath = GetKnownFolder(FOLDERID_Documents, L"C:\\Users\\you\\Documents");
    const std::wstring notesPath = docsPath + L"\\DeskPilot Notes";
    CreateDirectoryW(notesPath.c_str(), nullptr);
    int dlCount = CountFolderItems(downloadsPath);
    std::wstring dlCountStr = std::to_wstring(dlCount) + L" items";

    g_state.allItems = {
        {
            L"open-downloads", L"Suggested", L"Commands",
            L"Open Downloads", L"Jump to your downloaded files", L"↵", true, L"▣",
            L"Downloads", downloadsPath,
            L"Folder", dlCountStr, L"Today, 10:42", L"Open in Explorer",
            ACT_OPEN_DOWNLOADS
        },
        {
            L"new-note", L"Suggested", L"Commands",
            L"Create a note", L"Write and save a note in DeskPilot", L"Ctrl+N", false, L"+",
            L"New note", notesPath, L"Note editor", L"Markdown notes", L"Ready", L"Create note",
            ACT_CREATE_NOTE
        },
        {
            L"open-note", L"Suggested", L"Commands",
            L"Open a note", L"Read or continue an existing note", L"Ctrl+O", false, L"▤",
            L"Open a note", notesPath, L"Note library", L"Markdown and text files", L"Ready", L"Choose a note",
            ACT_OPEN_NOTE
        },
        {
            L"toggle-dnd", L"Suggested", L"Commands",
            L"Toggle Do Not Disturb", L"Silence notifications and stay focused", L"System", false, L"☾",
            L"Do Not Disturb", L"System Settings / Focus",
            L"System Toggle", L"Status: Off", L"Yesterday, 22:00", L"Toggle Focus",
            ACT_TOGGLE_DND
        },
        {
            L"weekly-review", L"Recent", L"Files",
            L"Weekly review.md", L"Documents / Notes", L"12m ago", false, L"≡",
            L"Weekly review.md", docsPath + L"\\Weekly review.md",
            L"Markdown Document", L"4.2 KB", L"12m ago", L"Open Document",
            ACT_OPEN_FILE
        },
        {
            L"interface-exploration", L"Recent", L"Files",
            L"Interface exploration.fig", L"Documents / Design", L"38m ago", false, L"❖",
            L"Interface exploration.fig", docsPath + L"\\Interface exploration.fig",
            L"Figma Design File", L"18.7 MB", L"38m ago", L"Open in Figma",
            ACT_OPEN_FILE
        },
        {
            L"terminal", L"Recent", L"Applications",
            L"Terminal", L"Applications / Utilities", L"1h ago", false, L">_",
            L"Terminal.exe", L"C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe",
            L"Application", L"Version 2.14", L"1h ago", L"Launch Application",
            ACT_OPEN_TERMINAL
        }
    };

    std::set<std::wstring> addedPaths;
    AddAppsFromFolder(GetKnownFolder(FOLDERID_Programs, L""), 0, addedPaths);
    AddAppsFromFolder(GetKnownFolder(FOLDERID_CommonPrograms, L""), 0, addedPaths);
}

static void ShowToast(const std::wstring& msg) {
    g_state.toastMessage = msg;
    if (g_hwnd) {
        SetTimer(g_hwnd, TIMER_TOAST, 2200, nullptr);
        InvalidateRect(g_hwnd, nullptr, FALSE);
    }
}

static std::wstring GetNotesDirectory() {
    const std::wstring path = GetKnownFolder(FOLDERID_Documents, L".") + L"\\DeskPilot Notes";
    if (!CreateDirectoryW(path.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
        MessageBoxW(g_hwnd, L"Could not create the DeskPilot Notes folder.", kAppName, MB_OK | MB_ICONERROR);
    }
    return path;
}

static void ShowCommonDialogError(HWND owner) {
    const DWORD error = CommDlgExtendedError();
    if (error != 0) {
        const std::wstring message = L"The Windows file dialog failed (error " + std::to_wstring(error) + L").";
        MessageBoxW(owner, message.c_str(), kAppName, MB_OK | MB_ICONERROR);
    }
}

static bool SaveCurrentNote() {
    if (!g_notesEdit) return false;

    if (g_notePath.empty()) {
        wchar_t path[MAX_PATH] = {};
        OPENFILENAMEW dialog = {};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = g_notesWindow;
        dialog.lpstrFilter = L"Markdown and text notes\0*.md;*.txt\0All files\0*.*\0";
        dialog.lpstrFile = path;
        dialog.nMaxFile = MAX_PATH;
        dialog.lpstrDefExt = L"md";
        const std::wstring initialDirectory = GetNotesDirectory();
        dialog.lpstrInitialDir = initialDirectory.c_str();
        dialog.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
        if (!GetSaveFileNameW(&dialog)) {
            ShowCommonDialogError(g_notesWindow);
            return false;
        }
        g_notePath = path;
    }

    const int charCount = GetWindowTextLengthW(g_notesEdit);
    std::wstring content(static_cast<size_t>(charCount) + 1, L'\0');
    GetWindowTextW(g_notesEdit, content.data(), charCount + 1);
    content.resize(static_cast<size_t>(charCount));

    const int byteCount = WideCharToMultiByte(CP_UTF8, 0, content.c_str(), charCount, nullptr, 0, nullptr, nullptr);
    if (charCount > 0 && byteCount == 0) {
        MessageBoxW(g_notesWindow, L"Could not encode this note as UTF-8.", kAppName, MB_OK | MB_ICONERROR);
        return false;
    }
    std::string utf8(static_cast<size_t>(byteCount), '\0');
    if (byteCount > 0) {
        WideCharToMultiByte(CP_UTF8, 0, content.c_str(), charCount, utf8.data(), byteCount, nullptr, nullptr);
    }

    HANDLE file = CreateFileW(g_notePath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        MessageBoxW(g_notesWindow, L"Could not save this note.", kAppName, MB_OK | MB_ICONERROR);
        return false;
    }

    DWORD written = 0;
    const bool saved = utf8.empty() || WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
    CloseHandle(file);
    if (!saved || written != static_cast<DWORD>(utf8.size())) {
        MessageBoxW(g_notesWindow, L"Could not write the complete note to disk.", kAppName, MB_OK | MB_ICONERROR);
        return false;
    }

    SendMessageW(g_notesEdit, EM_SETMODIFY, FALSE, 0);
    const std::wstring title = g_notePath.substr(g_notePath.find_last_of(L"\\/") + 1) + L" - DeskPilot";
    SetWindowTextW(g_notesWindow, title.c_str());
    ShowToast(L"Note saved");
    return true;
}

static bool LoadNoteFile(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        MessageBoxW(g_notesWindow ? g_notesWindow : g_hwnd, L"Could not open this note.", kAppName, MB_OK | MB_ICONERROR);
        return false;
    }

    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(file, &size)) {
        CloseHandle(file);
        MessageBoxW(g_notesWindow, L"Could not determine the note's size.", kAppName, MB_OK | MB_ICONERROR);
        return false;
    }
    if (size.QuadPart > 16 * 1024 * 1024) {
        CloseHandle(file);
        MessageBoxW(g_notesWindow, L"This note is too large to open (maximum 16 MB).", kAppName, MB_OK | MB_ICONERROR);
        return false;
    }

    std::string bytes(static_cast<size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    const bool readOk = bytes.empty() || ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
    CloseHandle(file);
    if (!readOk || read != static_cast<DWORD>(bytes.size())) {
        MessageBoxW(g_notesWindow, L"Could not read the complete note.", kAppName, MB_OK | MB_ICONERROR);
        return false;
    }

    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF &&
        static_cast<unsigned char>(bytes[1]) == 0xFE) {
        const size_t wideBytes = bytes.size() - 2;
        if (wideBytes % sizeof(wchar_t) != 0) {
            MessageBoxW(g_notesWindow, L"This UTF-16 note is incomplete.", kAppName, MB_OK | MB_ICONERROR);
            return false;
        }
        std::wstring content(wideBytes / sizeof(wchar_t), L'\0');
        if (!content.empty()) memcpy(content.data(), bytes.data() + 2, wideBytes);
        if (g_notesEdit) {
            SetWindowTextW(g_notesEdit, content.c_str());
            SendMessageW(g_notesEdit, EM_SETMODIFY, FALSE, 0);
        }
        g_notePath = path;
        if (g_notesWindow) {
            const std::wstring title = path.substr(path.find_last_of(L"\\/") + 1) + L" - DeskPilot";
            SetWindowTextW(g_notesWindow, title.c_str());
        }
        return true;
    }

    const int skip = bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB && static_cast<unsigned char>(bytes[2]) == 0xBF ? 3 : 0;
    const int wideCount = MultiByteToWideChar(CP_UTF8, 0, bytes.data() + skip,
        static_cast<int>(bytes.size()) - skip, nullptr, 0);
    if (wideCount == 0 && bytes.size() > static_cast<size_t>(skip)) {
        MessageBoxW(g_notesWindow, L"This note is not valid UTF-8 text.", kAppName, MB_OK | MB_ICONERROR);
        return false;
    }

    std::wstring content(static_cast<size_t>(wideCount), L'\0');
    if (wideCount > 0) {
        MultiByteToWideChar(CP_UTF8, 0, bytes.data() + skip,
            static_cast<int>(bytes.size()) - skip, content.data(), wideCount);
    }

    if (g_notesEdit) {
        SetWindowTextW(g_notesEdit, content.c_str());
        SendMessageW(g_notesEdit, EM_SETMODIFY, FALSE, 0);
    }
    g_notePath = path;
    if (g_notesWindow) {
        const std::wstring title = path.substr(path.find_last_of(L"\\/") + 1) + L" - DeskPilot";
        SetWindowTextW(g_notesWindow, title.c_str());
    }
    return true;
}

static bool ChooseAndOpenNote() {
    wchar_t path[MAX_PATH] = {};
    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = g_notesWindow ? g_notesWindow : g_hwnd;
    dialog.lpstrFilter = L"Text notes\0*.md;*.markdown;*.txt\0All files\0*.*\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = MAX_PATH;
    const std::wstring initialDirectory = GetNotesDirectory();
    dialog.lpstrInitialDir = initialDirectory.c_str();
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&dialog)) {
        ShowCommonDialogError(dialog.hwndOwner);
        return false;
    }
    return LoadNoteFile(path);
}

static LRESULT CALLBACK NotesWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        CreateWindowExW(0, L"BUTTON", L"New", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            12, 10, 76, 30, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(NOTE_NEW)), nullptr, nullptr);
        CreateWindowExW(0, L"BUTTON", L"Open", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            96, 10, 76, 30, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(NOTE_OPEN)), nullptr, nullptr);
        CreateWindowExW(0, L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            180, 10, 76, 30, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(NOTE_SAVE)), nullptr, nullptr);
        g_notesEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
            12, 50, 760, 500, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!g_notesEdit) return -1;
        SendMessageW(g_notesEdit, EM_SETLIMITTEXT, 16 * 1024 * 1024, 0);
        SendMessageW(g_notesEdit, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        return 0;
    }
    case WM_SIZE: {
        const int width = LOWORD(lParam);
        const int height = HIWORD(lParam);
        if (g_notesEdit) MoveWindow(g_notesEdit, 12, 50, width - 24, height - 62, TRUE);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case NOTE_NEW:
            if (g_notesEdit && SendMessageW(g_notesEdit, EM_GETMODIFY, 0, 0) &&
                MessageBoxW(hwnd, L"Discard the unsaved note?", kAppName, MB_YESNO | MB_ICONQUESTION) != IDYES) return 0;
            g_notePath.clear();
            SetWindowTextW(g_notesEdit, L"");
            SendMessageW(g_notesEdit, EM_SETMODIFY, FALSE, 0);
            SetWindowTextW(hwnd, L"New note - DeskPilot");
            SetFocus(g_notesEdit);
            return 0;
        case NOTE_OPEN:
            if (g_notesEdit && SendMessageW(g_notesEdit, EM_GETMODIFY, 0, 0) &&
                MessageBoxW(hwnd, L"Discard the unsaved note?", kAppName, MB_YESNO | MB_ICONQUESTION) != IDYES) return 0;
            ChooseAndOpenNote();
            return 0;
        case NOTE_SAVE:
            SaveCurrentNote();
            return 0;
        }
        break;
    case WM_CLOSE:
        if (g_notesEdit && SendMessageW(g_notesEdit, EM_GETMODIFY, 0, 0) &&
            MessageBoxW(hwnd, L"Close without saving your changes?", kAppName, MB_YESNO | MB_ICONQUESTION) != IDYES) return 0;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        g_notesEdit = nullptr;
        g_notesWindow = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

static void ShowNotesWindow(bool openExisting, const std::wstring& initialPath) {
    if (!g_notesWindow) {
        g_notesWindow = CreateWindowExW(WS_EX_APPWINDOW, L"DeskPilotNotes", L"New note - DeskPilot",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 820, 620,
            g_hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!g_notesWindow) {
            MessageBoxW(g_hwnd, L"Could not open the notes window.", kAppName, MB_OK | MB_ICONERROR);
            return;
        }
    }
    ShowWindow(g_notesWindow, SW_SHOW);
    SetForegroundWindow(g_notesWindow);
    if (g_notesEdit && !openExisting && initialPath.empty() &&
        SendMessageW(g_notesEdit, EM_GETMODIFY, 0, 0) &&
        MessageBoxW(g_notesWindow, L"Discard the unsaved note?", kAppName, MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    if (!initialPath.empty()) LoadNoteFile(initialPath);
    else if (openExisting) {
        if (g_notesEdit && SendMessageW(g_notesEdit, EM_GETMODIFY, 0, 0) &&
            MessageBoxW(g_notesWindow, L"Discard the unsaved note?", kAppName, MB_YESNO | MB_ICONQUESTION) != IDYES) return;
        ChooseAndOpenNote();
    } else {
        g_notePath.clear();
        SetWindowTextW(g_notesEdit, L"");
        SendMessageW(g_notesEdit, EM_SETMODIFY, FALSE, 0);
    }
    SetFocus(g_notesEdit);
}

static void ToggleFullscreen() {
    if (!g_hwnd) return;
    if (g_state.isFullscreen) {
        SetWindowPos(g_hwnd, nullptr, g_state.windowedBounds.left, g_state.windowedBounds.top,
            g_state.windowedBounds.right - g_state.windowedBounds.left,
            g_state.windowedBounds.bottom - g_state.windowedBounds.top, SWP_NOZORDER | SWP_NOACTIVATE);
        const int width = g_state.windowedBounds.right - g_state.windowedBounds.left;
        const int height = g_state.windowedBounds.bottom - g_state.windowedBounds.top;
        SetWindowRgn(g_hwnd, CreateRoundRectRgn(0, 0, width + 1, height + 1, 24, 24), TRUE);
        g_state.isFullscreen = false;
    } else {
        GetWindowRect(g_hwnd, &g_state.windowedBounds);
        MONITORINFO monitor = {};
        monitor.cbSize = sizeof(monitor);
        if (GetMonitorInfoW(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST), &monitor)) {
            SetWindowPos(g_hwnd, nullptr, monitor.rcMonitor.left, monitor.rcMonitor.top,
                monitor.rcMonitor.right - monitor.rcMonitor.left,
                monitor.rcMonitor.bottom - monitor.rcMonitor.top, SWP_NOZORDER | SWP_NOACTIVATE);
            SetWindowRgn(g_hwnd, nullptr, TRUE);
            g_state.isFullscreen = true;
        }
    }
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

static bool RegisterNotesWindowClass(HINSTANCE instance) {
    WNDCLASSEXW noteClass = {};
    noteClass.cbSize = sizeof(noteClass);
    noteClass.lpfnWndProc = NotesWindowProc;
    noteClass.hInstance = instance;
    noteClass.hCursor = LoadCursorW(nullptr, IDC_IBEAM);
    noteClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    noteClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_DESKPILOT));
    noteClass.lpszClassName = L"DeskPilotNotes";
    return RegisterClassExW(&noteClass) != 0;
}

static bool CopyTextToClipboard(HWND hwnd, const std::wstring& text) {
    if (!OpenClipboard(hwnd)) return false;
    EmptyClipboard();
    size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!hMem) {
        CloseClipboard();
        return false;
    }
    void* ptr = GlobalLock(hMem);
    if (ptr) {
        memcpy(ptr, text.c_str(), bytes);
        GlobalUnlock(hMem);
        SetClipboardData(CF_UNICODETEXT, hMem);
    }
    CloseClipboard();
    return true;
}

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

    for (const auto& item : g_state.allItems) {
        bool tabMatch = (g_state.activeTab == 0) || (item.category == activeTabName);
        bool searchMatch = ContainsCaseInsensitive(item.title, g_state.searchQuery) ||
                           ContainsCaseInsensitive(item.subtitle, g_state.searchQuery) ||
                           ContainsCaseInsensitive(item.kind, g_state.searchQuery);
        if (tabMatch && searchMatch) {
            g_state.filteredItems.push_back(item);
        }
    }

    if (g_state.selectedIndex >= (int)g_state.filteredItems.size()) {
        g_state.selectedIndex = g_state.filteredItems.empty() ? 0 : (int)g_state.filteredItems.size() - 1;
    }
    g_state.listScrollOffset = 0;
    g_state.hoverIndex = -1;
}

static int GetVisibleListHeight() {
    if (!g_hwnd) return 0;
    RECT client = {};
    GetClientRect(g_hwnd, &client);
    const int bodyTop = 20 + 92;
    const int bodyBottom = client.bottom - 52 - 40;
    return std::max(0, bodyBottom - (bodyTop + 12));
}

static int GetListContentHeight() {
    const wchar_t* sections[] = { L"Suggested", L"Recent" };
    int height = 0;
    for (const wchar_t* section : sections) {
        int itemCount = 0;
        for (const auto& item : g_state.filteredItems) {
            if (item.section == section) ++itemCount;
        }
        if (itemCount > 0) height += 24 + itemCount * 52 + 8;
    }
    return height;
}

static int GetMaxListScrollOffset() {
    return std::max(0, GetListContentHeight() - GetVisibleListHeight());
}

static void EnsureSelectionVisible() {
    if (g_state.filteredItems.empty()) {
        g_state.listScrollOffset = 0;
        return;
    }

    const wchar_t* sections[] = { L"Suggested", L"Recent" };
    int rowTop = 0;
    int selectedTop = 0;
    bool found = false;
    for (const wchar_t* section : sections) {
        bool hasItems = false;
        for (const auto& item : g_state.filteredItems) {
            if (item.section == section) {
                hasItems = true;
                break;
            }
        }
        if (!hasItems) continue;
        rowTop += 24;
        for (size_t i = 0; i < g_state.filteredItems.size(); ++i) {
            if (g_state.filteredItems[i].section != section) continue;
            if (static_cast<int>(i) == g_state.selectedIndex) {
                selectedTop = rowTop;
                found = true;
                break;
            }
            rowTop += 52;
        }
        if (found) break;
        rowTop += 8;
    }

    const int visibleHeight = GetVisibleListHeight();
    if (found && selectedTop < g_state.listScrollOffset) {
        g_state.listScrollOffset = selectedTop;
    } else if (found && selectedTop + 50 > g_state.listScrollOffset + visibleHeight) {
        g_state.listScrollOffset = selectedTop + 50 - visibleHeight;
    }
    g_state.listScrollOffset = std::min(g_state.listScrollOffset, GetMaxListScrollOffset());
}

static void ExecuteSelectedItem() {
    if (g_state.filteredItems.empty()) return;
    const auto& sel = g_state.filteredItems[g_state.selectedIndex];

    switch (sel.actionType) {
    case ACT_OPEN_DOWNLOADS:
        ShellExecuteW(g_hwnd, L"open", sel.previewPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        ShowToast(L"Opened Downloads folder");
        break;

    case ACT_CREATE_NOTE:
        ShowNotesWindow(false);
        ShowToast(L"New note");
        break;

    case ACT_OPEN_NOTE:
        ShowNotesWindow(true);
        break;

    case ACT_TOGGLE_DND:
        g_state.dndEnabled = !g_state.dndEnabled;
        for (auto& item : g_state.allItems) {
            if (item.actionType == ACT_TOGGLE_DND) {
                item.contents = g_state.dndEnabled ? L"Status: On (Focused)" : L"Status: Off";
            }
        }
        RebuildFilteredList();
        ShowToast(g_state.dndEnabled ? L"Do Not Disturb: Enabled" : L"Do Not Disturb: Disabled");
        break;

    case ACT_OPEN_FILE: {
        HINSTANCE res = ShellExecuteW(g_hwnd, L"open", sel.previewPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if ((INT_PTR)res <= 32) {
            std::wstring docs = GetKnownFolder(FOLDERID_Documents, L"C:\\");
            ShellExecuteW(g_hwnd, L"open", docs.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            ShowToast(L"Opened Documents folder");
        } else {
            ShowToast(L"Opened " + sel.title);
        }
        break;
    }

    case ACT_OPEN_TERMINAL: {
        HINSTANCE res = ShellExecuteW(g_hwnd, L"open", L"wt.exe", nullptr, nullptr, SW_SHOWNORMAL);
        if ((INT_PTR)res <= 32) {
            ShellExecuteW(g_hwnd, L"open", L"powershell.exe", nullptr, nullptr, SW_SHOWNORMAL);
        }
        ShowToast(L"Launched Terminal");
        break;
    }

    case ACT_OPEN_APP: {
        HINSTANCE result = ShellExecuteW(g_hwnd, L"open", sel.previewPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if ((INT_PTR)result <= 32) {
            MessageBoxW(g_hwnd, (L"Could not launch " + sel.title + L".").c_str(), kAppName, MB_OK | MB_ICONERROR);
        } else {
            ShowToast(L"Launched " + sel.title);
        }
        break;
    }
    }
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

static COLORREF MixColor(COLORREF base, COLORREF accent, int amount) {
    const int inverse = 255 - amount;
    return RGB(
        (GetRValue(base) * inverse + GetRValue(accent) * amount) / 255,
        (GetGValue(base) * inverse + GetGValue(accent) * amount) / 255,
        (GetBValue(base) * inverse + GetBValue(accent) * amount) / 255);
}

static COLORREF GetItemAccent(const CommandItem& item) {
    if (item.id == L"open-downloads") return RGB(100, 166, 255);
    if (item.id == L"new-note" || item.id == L"open-note") return RGB(255, 184, 92);
    if (item.id == L"toggle-dnd") return RGB(182, 145, 255);
    if (item.id == L"weekly-review") return RGB(108, 210, 157);
    if (item.id == L"interface-exploration") return RGB(255, 126, 151);
    return RGB(100, 204, 226);
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
    HBRUSH bgBrush = CreateSolidBrush(RGB(17, 17, 17));
    FillRect(hdc, &client, bgBrush);
    DeleteObject(bgBrush);

    RECT panel = { 32, 20, client.right - 32, client.bottom - 52 };
    FillRoundBox(hdc, panel, 20, RGB(28, 28, 28), true, RGB(46, 46, 46));
    DrawLine(hdc, panel.left + 22, panel.top + 1, panel.right - 22, panel.top + 1, RGB(57, 73, 96));

    RECT searchBox = { panel.left + 14, panel.top + 10, panel.right - 104, panel.top + 52 };
    FillRoundBox(hdc, searchBox, 12, RGB(34, 36, 42), true, RGB(51, 55, 64));

    RECT searchIconRc = { panel.left + 20, panel.top + 16, panel.left + 44, panel.top + 48 };
    DrawLabel(hdc, L"⌕", searchIconRc, RGB(132, 169, 220), g_fontTitle, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    RECT searchTextRc = { panel.left + 50, panel.top + 16, panel.right - 160, panel.top + 48 };
    if (g_state.searchQuery.empty()) {
        DrawLabel(hdc, L"Search apps, files, and commands...", searchTextRc, RGB(115, 115, 115), g_fontSearch);
    } else {
        std::wstring displayQuery = g_state.searchQuery + L"|";
        DrawLabel(hdc, displayQuery.c_str(), searchTextRc, RGB(242, 242, 242), g_fontSearch);
    }

    RECT escRect = { panel.right - 148, panel.top + 20, panel.right - 108, panel.top + 42 };
    g_state.escBox = escRect;
    FillRoundBox(hdc, escRect, 6, g_state.hoverEscBtn ? RGB(52, 52, 52) : RGB(38, 38, 38), true, RGB(58, 58, 58));
    DrawLabel(hdc, L"esc", escRect, RGB(155, 155, 155), g_fontSmall, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    RECT fullscreenBox = { panel.right - 96, panel.top + 16, panel.right - 60, panel.top + 46 };
    g_state.fullscreenBox = fullscreenBox;
    FillRoundBox(hdc, fullscreenBox, 7, g_state.hoverFullscreenBtn ? RGB(48, 54, 66) : RGB(35, 37, 42),
        true, RGB(55, 58, 66));
    DrawLabel(hdc, g_state.isFullscreen ? L"❐" : L"□", fullscreenBox,
        g_state.hoverFullscreenBtn ? RGB(210, 224, 247) : RGB(147, 162, 184),
        g_fontBody, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    RECT quitBox = { panel.right - 50, panel.top + 16, panel.right - 14, panel.top + 46 };
    g_state.quitBox = quitBox;
    FillRoundBox(hdc, quitBox, 7, g_state.hoverQuitBtn ? RGB(94, 43, 49) : RGB(43, 35, 38),
        true, g_state.hoverQuitBtn ? RGB(139, 66, 73) : RGB(65, 48, 52));
    DrawLabel(hdc, L"×", quitBox, g_state.hoverQuitBtn ? RGB(255, 205, 207) : RGB(203, 146, 151),
        g_fontBody, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    int tabY = panel.top + 56;
    int tabX = panel.left + 20;
    int tabWidths[] = { 38, 92, 48, 84 };

    for (int i = 0; i < 4; ++i) {
        RECT tr = { tabX, tabY, tabX + tabWidths[i], tabY + 26 };
        g_state.tabHitBoxes[i] = tr;
        if (g_state.activeTab == i) {
            FillRoundBox(hdc, tr, 8, RGB(38, 50, 69), true, RGB(59, 79, 108));
        }
        DrawLabel(hdc, g_tabs[i], tr, g_state.activeTab == i ? RGB(245, 245, 245) : RGB(128, 128, 128),
            g_fontSmall, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        tabX += tabWidths[i] + 8;
    }

    RECT switchTxt = { panel.right - 92, tabY, panel.right - 46, tabY + 26 };
    DrawLabel(hdc, L"Switch", switchTxt, RGB(115, 115, 115), g_fontSmall, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    RECT tabKey = { panel.right - 40, tabY + 3, panel.right - 20, tabY + 23 };
    FillRoundBox(hdc, tabKey, 5, RGB(38, 38, 38), true, RGB(54, 54, 54));
    DrawLabel(hdc, L"⇥", tabKey, RGB(150, 150, 150), g_fontSmall, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    int bodyTop = panel.top + 92;
    int bodyBottom = panel.bottom - 40;
    DrawLine(hdc, panel.left, bodyTop, panel.right, bodyTop, RGB(42, 42, 42));

    int splitX = panel.left + 530;
    if (g_state.showPreview) {
        RECT previewSurface = { splitX + 1, bodyTop + 1, panel.right - 1, bodyBottom - 1 };
        FillRoundBox(hdc, previewSurface, 10, RGB(25, 26, 30));
        DrawLine(hdc, splitX, bodyTop, splitX, bodyBottom, RGB(42, 42, 42));
    }

    g_state.rowHitBoxes.clear();
    int listLeft = panel.left + 12;
    int listRight = (g_state.showPreview ? splitX : panel.right) - 12;
    const int listTop = bodyTop + 12;
    g_state.listBounds = { listLeft, bodyTop, listRight, bodyBottom };
    int curY = listTop - g_state.listScrollOffset;
    const int listDcState = SaveDC(hdc);
    if (listDcState != 0) IntersectClipRect(hdc, listLeft, bodyTop, listRight, bodyBottom);

    if (g_state.filteredItems.empty()) {
        RECT emptyRc = { listLeft, bodyTop + 80, listRight, bodyTop + 140 };
        DrawLabel(hdc, L"No matching commands or files found.", emptyRc, RGB(110, 110, 110),
            g_fontBody, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    } else {
        const wchar_t* sections[] = { L"Suggested", L"Recent" };
        const wchar_t* sectionRight[] = { L"COMMANDS", L"LAST OPENED" };

        for (int s = 0; s < 2; ++s) {
            bool hasSectionItems = false;
            for (const auto& item : g_state.filteredItems) {
                if (item.section == sections[s]) {
                    hasSectionItems = true;
                    break;
                }
            }
            if (!hasSectionItems) continue;

            RECT secLeftRc = { listLeft + 10, curY, listLeft + 200, curY + 20 };
            RECT secRightRc = { listRight - 140, curY, listRight - 10, curY + 20 };
            DrawLabel(hdc, sections[s], secLeftRc, RGB(117, 117, 117), g_fontSmall);
            DrawLabel(hdc, sectionRight[s], secRightRc, RGB(105, 105, 105), g_fontTinyBold, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
            curY += 24;

            for (int i = 0; i < (int)g_state.filteredItems.size(); ++i) {
                const auto& item = g_state.filteredItems[i];
                if (item.section != sections[s]) continue;

                RECT rowRc = { listLeft, curY, listRight, curY + 50 };
                if (rowRc.bottom > bodyTop && rowRc.top < bodyBottom) {
                    g_state.rowHitBoxes.push_back({ i, rowRc });
                }

                bool selected = (g_state.selectedIndex == i);
                bool hovered = (g_state.hoverIndex == i);

                if (selected) {
                    FillRoundBox(hdc, rowRc, 12, RGB(34, 47, 65), true, RGB(51, 71, 96));
                    RECT selectionMark = { rowRc.left + 2, rowRc.top + 10, rowRc.left + 5, rowRc.bottom - 10 };
                    FillRoundBox(hdc, selectionMark, 3, RGB(112, 170, 255));
                } else if (hovered) {
                    FillRoundBox(hdc, rowRc, 12, RGB(38, 38, 38));
                }

                RECT iconRc = { rowRc.left + 12, rowRc.top, rowRc.left + 38, rowRc.bottom };
                const COLORREF accent = GetItemAccent(item);
                RECT iconTile = { iconRc.left + 1, rowRc.top + 11, iconRc.right - 1, rowRc.bottom - 11 };
                FillRoundBox(hdc, iconTile, 7, MixColor(RGB(36, 36, 36), accent, 58));
                DrawLabel(hdc, item.icon.c_str(), iconRc, selected ? MixColor(RGB(225, 233, 245), accent, 75) : accent,
                    g_fontBody, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

                RECT titleRc = { rowRc.left + 46, rowRc.top + 7, rowRc.right - 80, rowRc.top + 26 };
                DrawLabel(hdc, item.title.c_str(), titleRc, selected ? RGB(242, 246, 252) : RGB(237, 237, 237), g_fontBody);

                RECT subRc = { rowRc.left + 46, rowRc.top + 26, rowRc.right - 80, rowRc.bottom - 6 };
                DrawLabel(hdc, item.subtitle.c_str(), subRc, selected ? RGB(177, 192, 211) : RGB(120, 120, 120), g_fontSmall);

                if (item.isBadge) {
                    RECT badgeRc = { rowRc.right - 36, rowRc.top + 14, rowRc.right - 12, rowRc.bottom - 14 };
                    FillRoundBox(hdc, badgeRc, 6, selected ? RGB(48, 65, 88) : RGB(38, 38, 38),
                        true, selected ? RGB(69, 91, 122) : RGB(54, 54, 54));
                    DrawLabel(hdc, item.shortcut.c_str(), badgeRc, selected ? RGB(200, 216, 237) : RGB(140, 140, 140),
                        g_fontSmall, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
                } else {
                    RECT rightLblRc = { rowRc.right - 90, rowRc.top, rowRc.right - 12, rowRc.bottom };
                    DrawLabel(hdc, item.shortcut.c_str(), rightLblRc, selected ? RGB(177, 192, 211) : RGB(115, 115, 115),
                        g_fontSmall, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
                }

                curY += 52;
            }
            curY += 8;
        }
    }

    const int contentHeight = GetListContentHeight();
    const int visibleHeight = bodyBottom - listTop;
    if (contentHeight > visibleHeight && visibleHeight > 0) {
        RECT track = { listRight - 5, bodyTop + 8, listRight - 2, bodyBottom - 8 };
        FillRoundBox(hdc, track, 3, RGB(34, 35, 39));
        const int trackHeight = track.bottom - track.top;
        const int thumbHeight = std::max(24, trackHeight * visibleHeight / contentHeight);
        const int thumbTravel = trackHeight - thumbHeight;
        const int maxScroll = contentHeight - visibleHeight;
        const int thumbTop = track.top + (thumbTravel * g_state.listScrollOffset / maxScroll);
        RECT thumb = { track.left, thumbTop, track.right, thumbTop + thumbHeight };
        FillRoundBox(hdc, thumb, 3, RGB(83, 96, 116));
    }
    if (listDcState != 0) RestoreDC(hdc, listDcState);

    int pvIconRight = panel.right - 18;
    RECT pvToggleRc = { pvIconRight - 24, bodyTop + 12, pvIconRight, bodyTop + 34 };
    g_state.previewToggleBox = pvToggleRc;
    if (g_state.hoverPreviewToggle) {
        FillRoundBox(hdc, pvToggleRc, 6, RGB(42, 42, 42));
    }
    DrawLabel(hdc, L"◧", pvToggleRc, g_state.hoverPreviewToggle ? RGB(210, 210, 210) : RGB(120, 120, 120),
        g_fontSmall, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    if (g_state.showPreview && !g_state.filteredItems.empty()) {
        const auto& sel = g_state.filteredItems[g_state.selectedIndex];
        int pvLeft = splitX + 20;
        int pvRight = panel.right - 20;
        int pvTop = bodyTop + 14;

        RECT pvHeaderRc = { pvLeft, pvTop, pvRight - 30, pvTop + 20 };
        DrawLabel(hdc, L"Preview", pvHeaderRc, RGB(120, 120, 120), g_fontSmall);

        RECT iconBox = { pvLeft, pvTop + 30, pvLeft + 72, pvTop + 102 };
        const COLORREF accent = GetItemAccent(sel);
        FillRoundBox(hdc, iconBox, 16, MixColor(RGB(31, 33, 39), accent, 64), true,
            MixColor(RGB(48, 50, 57), accent, 90));
        DrawLabel(hdc, sel.icon.c_str(), iconBox, MixColor(RGB(228, 234, 244), accent, 72),
            g_fontPreviewIcon, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

        RECT pvTitleRc = { pvLeft, pvTop + 114, pvRight, pvTop + 142 };
        DrawLabel(hdc, sel.previewTitle.c_str(), pvTitleRc, RGB(245, 245, 245), g_fontTitle);

        RECT pvPathRc = { pvLeft, pvTop + 142, pvRight, pvTop + 162 };
        DrawLabel(hdc, sel.previewPath.c_str(), pvPathRc, RGB(115, 115, 115), g_fontSmall, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_PATH_ELLIPSIS);

        const wchar_t* metaLabels[] = { L"Kind", L"Contents", L"Last opened" };
        const wchar_t* metaValues[] = { sel.kind.c_str(), sel.contents.c_str(), sel.lastOpened.c_str() };
        int metaY = pvTop + 178;

        for (int i = 0; i < 3; ++i) {
            RECT rowRc = { pvLeft, metaY + i * 24, pvRight, metaY + (i + 1) * 24 };
            DrawLabel(hdc, metaLabels[i], rowRc, RGB(115, 115, 115), g_fontSmall, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
            DrawLabel(hdc, metaValues[i], rowRc, RGB(212, 212, 212), g_fontSmall, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
        }

        int actionTop = pvTop + 266;
        DrawLine(hdc, pvLeft, actionTop, pvRight, actionTop, RGB(42, 42, 42));

        RECT mainBtn = { pvLeft, actionTop + 14, pvRight, actionTop + 50 };
        g_state.primaryActionBox = mainBtn;
        FillRoundBox(hdc, mainBtn, 10, g_state.hoverPrimaryBtn ? RGB(55, 91, 143) : RGB(43, 68, 105),
            true, g_state.hoverPrimaryBtn ? RGB(83, 126, 187) : RGB(61, 91, 135));

        RECT mainBtnText = { mainBtn.left + 14, mainBtn.top, mainBtn.right - 34, mainBtn.bottom };
        DrawLabel(hdc, sel.primaryAction.c_str(), mainBtnText, RGB(242, 247, 255), g_fontSmall);

        RECT mainBtnKey = { mainBtn.right - 30, mainBtn.top, mainBtn.right - 10, mainBtn.bottom };
        DrawLabel(hdc, L"↵", mainBtnKey, RGB(190, 210, 238), g_fontSmall, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

        RECT copyBtn = { pvLeft, actionTop + 54, pvRight, actionTop + 84 };
        g_state.copyPathBox = copyBtn;
        if (g_state.hoverCopyBtn) {
            FillRoundBox(hdc, copyBtn, 8, RGB(36, 36, 36));
        }
        RECT copyRow = { copyBtn.left + 14, copyBtn.top, copyBtn.right - 10, copyBtn.bottom };
        DrawLabel(hdc, L"Copy path", copyRow, g_state.hoverCopyBtn ? RGB(210, 210, 210) : RGB(138, 138, 138), g_fontSmall);
        DrawLabel(hdc, L"⌘ ⇧ C", copyRow, RGB(110, 110, 110), g_fontSmall, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    } else {
        g_state.primaryActionBox = { 0, 0, 0, 0 };
        g_state.copyPathBox = { 0, 0, 0, 0 };
    }

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

    if (!g_state.toastMessage.empty()) {
        RECT toastRc = { panel.left + 250, fY, panel.right - 140, fY + 22 };
        DrawLabel(hdc, g_state.toastMessage.c_str(), toastRc, RGB(180, 225, 190), g_fontSmall, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    }

    RECT actText = { panel.right - 120, fY, panel.right - 70, fY + 22 };
    DrawLabel(hdc, L"Actions", actText, RGB(120, 120, 120), g_fontSmall, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);

    RECT cmdKey = { panel.right - 64, fY, panel.right - 44, fY + 22 };
    RECT retKey = { panel.right - 40, fY, panel.right - 20, fY + 22 };
    FillRoundBox(hdc, cmdKey, 5, RGB(37, 37, 37), true, RGB(52, 52, 52));
    FillRoundBox(hdc, retKey, 5, RGB(37, 37, 37), true, RGB(52, 52, 52));
    DrawLabel(hdc, L"⌘", cmdKey, RGB(150, 150, 150), g_fontSmall, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    DrawLabel(hdc, L"↵", retKey, RGB(150, 150, 150), g_fontSmall, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    int capY = panel.bottom + 14;
    RECT capLeft = { panel.left + 4, capY, panel.left + 145, capY + 24 };
    DrawLabel(hdc, L"A little less clicking —", capLeft, RGB(115, 115, 115), g_fontSmall);

    RECT kCap = { panel.left + 148, capY + 1, panel.left + 184, capY + 23 };
    FillRoundBox(hdc, kCap, 5, RGB(34, 34, 34), true, RGB(51, 51, 51));
    DrawLabel(hdc, L"⌘ K", kCap, RGB(176, 176, 176), g_fontSmall, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    RECT capAfter = { panel.left + 192, capY, panel.left + 320, capY + 24 };
    DrawLabel(hdc, L"from anywhere.", capAfter, RGB(115, 115, 115), g_fontSmall);

    RECT capRight = { panel.right - 160, capY, panel.right - 4, capY + 24 };
    DrawLabel(hdc, kAppVersion, capRight, RGB(130, 148, 175), g_fontSmall, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
}

static int HitTestRow(POINT pt) {
    for (const auto& row : g_state.rowHitBoxes) {
        if (PtInRect(&row.bounds, pt)) {
            return row.itemIndex;
        }
    }
    return -1;
}

static int HitTestTab(POINT pt) {
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
        InitItems();
        RebuildFilteredList();

        g_nid.cbSize = sizeof(NOTIFYICONDATAW);
        g_nid.hWnd = hwnd;
        g_nid.uID = 1;
        g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        g_nid.uCallbackMessage = WM_TRAYICON;
        g_nid.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_DESKPILOT));
        wcscpy_s(g_nid.szTip, L"DeskPilot (Ctrl+K)");
        Shell_NotifyIconW(NIM_ADD, &g_nid);

        RegisterHotKey(hwnd, HOTKEY_CTRL_K, MOD_CONTROL, 'K');
        RegisterHotKey(hwnd, HOTKEY_ALT_SPACE, MOD_ALT, VK_SPACE);
        return 0;

    case WM_TRAYICON:
        if (LOWORD(lParam) == WM_LBUTTONUP || LOWORD(lParam) == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hwnd);

            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_SHOW, L"Show / Hide DeskPilot (Ctrl+K)");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_ABOUT, L"About DeskPilot");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Quit");

            TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, nullptr);
            DestroyMenu(hMenu);
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_TRAY_SHOW) {
            if (IsWindowVisible(hwnd)) {
                ShowWindow(hwnd, SW_HIDE);
            } else {
                ShowWindow(hwnd, SW_SHOW);
                SetForegroundWindow(hwnd);
                SetFocus(hwnd);
            }
        } else if (LOWORD(wParam) == ID_TRAY_EXIT) {
            DestroyWindow(hwnd);
        } else if (LOWORD(wParam) == ID_TRAY_ABOUT) {
            const std::wstring about = L"DeskPilot v" DESKPILOT_VERSION_W
                L"\nA desktop command palette and notes app.";
            MessageBoxW(hwnd, about.c_str(), L"About DeskPilot", MB_OK | MB_ICONINFORMATION);
        }
        return 0;

    case WM_HOTKEY:
        if (wParam == HOTKEY_CTRL_K || wParam == HOTKEY_ALT_SPACE) {
            if (IsWindowVisible(hwnd)) {
                ShowWindow(hwnd, SW_HIDE);
            } else {
                ShowWindow(hwnd, SW_SHOW);
                SetForegroundWindow(hwnd);
                SetFocus(hwnd);
            }
        }
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_TOAST) {
            KillTimer(hwnd, TIMER_TOAST);
            g_state.toastMessage.clear();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_SIZE:
        EnsureSelectionVisible();
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT client;
        GetClientRect(hwnd, &client);
        int width = client.right - client.left;
        int height = client.bottom - client.top;

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
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        int rowIdx = HitTestRow(pt);
        bool hPrimary = PtInRect(&g_state.primaryActionBox, pt) != 0;
        bool hCopy = PtInRect(&g_state.copyPathBox, pt) != 0;
        bool hToggle = PtInRect(&g_state.previewToggleBox, pt) != 0;
        bool hEsc = PtInRect(&g_state.escBox, pt) != 0;
        bool hFullscreen = PtInRect(&g_state.fullscreenBox, pt) != 0;
        bool hQuit = PtInRect(&g_state.quitBox, pt) != 0;

        if (rowIdx != g_state.hoverIndex ||
            hPrimary != g_state.hoverPrimaryBtn ||
            hCopy != g_state.hoverCopyBtn ||
            hToggle != g_state.hoverPreviewToggle ||
            hEsc != g_state.hoverEscBtn ||
            hFullscreen != g_state.hoverFullscreenBtn ||
            hQuit != g_state.hoverQuitBtn) {
            g_state.hoverIndex = rowIdx;
            g_state.hoverPrimaryBtn = hPrimary;
            g_state.hoverCopyBtn = hCopy;
            g_state.hoverPreviewToggle = hToggle;
            g_state.hoverEscBtn = hEsc;
            g_state.hoverFullscreenBtn = hFullscreen;
            g_state.hoverQuitBtn = hQuit;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_MOUSEWHEEL: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hwnd, &pt);
        if (PtInRect(&g_state.listBounds, pt)) {
            const int direction = GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? -1 : 1;
            const int nextOffset = std::clamp(g_state.listScrollOffset + direction * 52,
                0, GetMaxListScrollOffset());
            if (nextOffset != g_state.listScrollOffset) {
                g_state.listScrollOffset = nextOffset;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

        if (PtInRect(&g_state.quitBox, pt)) {
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
            return 0;
        }

        if (PtInRect(&g_state.fullscreenBox, pt)) {
            ToggleFullscreen();
            return 0;
        }

        if (PtInRect(&g_state.escBox, pt)) {
            if (!g_state.searchQuery.empty()) {
                g_state.searchQuery.clear();
                RebuildFilteredList();
                InvalidateRect(hwnd, nullptr, FALSE);
            } else {
                ShowWindow(hwnd, SW_HIDE);
            }
            return 0;
        }

        if (PtInRect(&g_state.previewToggleBox, pt)) {
            g_state.showPreview = !g_state.showPreview;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        int tabIdx = HitTestTab(pt);
        if (tabIdx != -1) {
            g_state.activeTab = tabIdx;
            RebuildFilteredList();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        if (g_state.showPreview && PtInRect(&g_state.primaryActionBox, pt)) {
            ExecuteSelectedItem();
            return 0;
        }

        if (g_state.showPreview && PtInRect(&g_state.copyPathBox, pt)) {
            if (!g_state.filteredItems.empty()) {
                const auto& sel = g_state.filteredItems[g_state.selectedIndex];
                if (CopyTextToClipboard(hwnd, sel.previewPath)) {
                    ShowToast(L"Copied path to clipboard!");
                }
            }
            return 0;
        }

        int rowIdx = HitTestRow(pt);
        if (rowIdx >= 0) {
            if (g_state.selectedIndex == rowIdx) {
                ExecuteSelectedItem();
            } else {
                g_state.selectedIndex = rowIdx;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        if (pt.y < 72) {
            ReleaseCapture();
            SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;
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
        } else if (wParam >= 32 && wParam != 127 && !(GetKeyState(VK_CONTROL) & 0x8000)) {
            g_state.searchQuery.push_back((wchar_t)wParam);
            RebuildFilteredList();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_KEYDOWN: {
        bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        bool shiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

        if (ctrlDown && shiftDown && wParam == 'C') {
            if (!g_state.filteredItems.empty()) {
                const auto& sel = g_state.filteredItems[g_state.selectedIndex];
                if (CopyTextToClipboard(hwnd, sel.previewPath)) {
                    ShowToast(L"Copied path to clipboard!");
                }
            }
            return 0;
        }

        if (ctrlDown && wParam == 'N') {
            ShowNotesWindow(false);
            return 0;
        }

        if (ctrlDown && wParam == 'O') {
            ShowNotesWindow(true);
            return 0;
        }

        if (wParam == VK_F11) {
            ToggleFullscreen();
            return 0;
        }

        if (ctrlDown && wParam == 'P') {
            g_state.showPreview = !g_state.showPreview;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        if (wParam == VK_UP && !g_state.filteredItems.empty()) {
            int n = (int)g_state.filteredItems.size();
            g_state.selectedIndex = (g_state.selectedIndex - 1 + n) % n;
            EnsureSelectionVisible();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (wParam == VK_DOWN && !g_state.filteredItems.empty()) {
            int n = (int)g_state.filteredItems.size();
            g_state.selectedIndex = (g_state.selectedIndex + 1) % n;
            EnsureSelectionVisible();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (wParam == VK_RETURN) {
            ExecuteSelectedItem();
            return 0;
        }
        if (wParam == VK_TAB) {
            if (shiftDown) {
                g_state.activeTab = (g_state.activeTab + 3) % 4;
            } else {
                g_state.activeTab = (g_state.activeTab + 1) % 4;
            }
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
                ShowWindow(hwnd, SW_HIDE);
            }
            return 0;
        }
        break;
    }

    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        UnregisterHotKey(hwnd, HOTKEY_CTRL_K);
        UnregisterHotKey(hwnd, HOTKEY_ALT_SPACE);
        CleanupFonts();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR, int nCmdShow) {
    SetProcessDPIAware();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    const wchar_t CLASS_NAME[] = L"DeskPilotMain";

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_DESKPILOT));
    wc.hIconSm = wc.hIcon;
    wc.hbrBackground = nullptr;
    wc.lpszClassName = CLASS_NAME;
    if (!RegisterClassExW(&wc) || !RegisterNotesWindowClass(hInstance)) {
        MessageBoxW(nullptr, L"Could not register the DeskPilot window classes.", kAppName, MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    int winWidth = 910;
    int winHeight = 576;

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenW - winWidth) / 2;
    int posY = (screenH - winHeight) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        CLASS_NAME,
        kAppVersion,
        WS_POPUP | WS_VISIBLE,
        posX, posY, winWidth, winHeight,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) {
        CoUninitialize();
        return 0;
    }

    HRGN hRgn = CreateRoundRectRgn(0, 0, winWidth + 1, winHeight + 1, 24, 24);
    SetWindowRgn(hwnd, hRgn, TRUE);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    return (int)msg.wParam;
}