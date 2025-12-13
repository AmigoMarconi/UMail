// OOP_Project.cpp
// WinAPI GUI -> calls your core: MailService::send(SmtpSettings, MailDraft, err)
/*
#include <windows.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <algorithm>

#pragma comment(lib, "Comdlg32.lib")

// ==== подключи своё ядро ====
#include "mail_service.h"   // должен содержать SmtpSettings, MailDraft, Attachment, MailService::send

// ---------- UI IDs ----------
static const int IDC_SERVER = 101;
static const int IDC_PORT = 102;
static const int IDC_USEAUTH = 103;
static const int IDC_USER = 104;
static const int IDC_PASS = 105;

static const int IDC_FROM = 106;
static const int IDC_TO = 107;
static const int IDC_SUBJECT = 108;
static const int IDC_BODY = 109;

static const int IDC_ATTACH_LIST = 110;
static const int IDC_BTN_ATTACH = 111;
static const int IDC_BTN_SEND = 112;

static const int IDC_LOG = 113;

// ---------- Globals ----------
static HWND g_hWnd = nullptr;

static HWND g_server = nullptr, g_port = nullptr, g_useAuth = nullptr, g_user = nullptr, g_pass = nullptr;
static HWND g_from = nullptr, g_to = nullptr, g_subject = nullptr, g_body = nullptr;
static HWND g_attachList = nullptr;
static HWND g_log = nullptr;

static std::vector<std::wstring> g_attachedFiles;

// ---------- Helpers ----------
static std::wstring GetTextW(HWND h) {
    int len = GetWindowTextLengthW(h);
    std::wstring s(len, L'\0');
    GetWindowTextW(h, s.data(), len + 1);
    return s;
}

static void SetTextW(HWND h, const std::wstring& s) {
    SetWindowTextW(h, s.c_str());
}

static void AppendLog(const std::wstring& line) {
    if (!g_log) return;
    int len = GetWindowTextLengthW(g_log);
    SendMessageW(g_log, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    std::wstring withNL = line + L"\r\n";
    SendMessageW(g_log, EM_REPLACESEL, FALSE, (LPARAM)withNL.c_str());
}

static std::string WStringToUtf8(const std::wstring& ws) {
    if (ws.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), out.data(), size, nullptr, nullptr);
    return out;
}

static std::wstring TrimW(std::wstring s) {
    auto notSpace = [](wchar_t c) { return c != L' ' && c != L'\t' && c != L'\r' && c != L'\n'; };
    while (!s.empty() && !notSpace(s.front())) s.erase(s.begin());
    while (!s.empty() && !notSpace(s.back())) s.pop_back();
    return s;
}

static std::vector<std::string> SplitRecipientsUtf8(const std::wstring& ws) {
    // split by ',' ';' whitespace-newlines
    std::vector<std::string> out;
    std::wstring s = ws;
    for (auto& ch : s) {
        if (ch == L';' || ch == L'\n' || ch == L'\r' || ch == L'\t') ch = L',';
    }
    std::wstringstream wss(s);
    std::wstring token;
    while (std::getline(wss, token, L',')) {
        token = TrimW(token);
        if (!token.empty())
            out.push_back(WStringToUtf8(token));
    }
    return out;
}

static std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

static std::string GuessMimeType(const std::filesystem::path& p) {
    std::string ext = ToLower(p.extension().string());
    if (ext == ".pdf") return "application/pdf";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".txt") return "text/plain; charset=utf-8";
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    return "application/octet-stream";
}

static bool ReadFileBytes(const std::filesystem::path& path, std::vector<unsigned char>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    f.seekg(0, std::ios::end);
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);

    if (size < 0) return false;
    out.resize((size_t)size);
    if (size > 0) f.read((char*)out.data(), size);
    return true;
}

static void RefreshAttachList() {
    SendMessageW(g_attachList, LB_RESETCONTENT, 0, 0);
    for (const auto& p : g_attachedFiles) {
        SendMessageW(g_attachList, LB_ADDSTRING, 0, (LPARAM)p.c_str());
    }
}

static void AttachFileDialog() {
    wchar_t fileName[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hWnd;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"All Files\0*.*\0PDF\0*.pdf\0Images\0*.png;*.jpg;*.jpeg;*.gif\0Text\0*.txt\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        g_attachedFiles.push_back(fileName);
        RefreshAttachList();
        AppendLog(L"Добавлено вложение: " + std::wstring(fileName));
    }
}

static int GetPortOrDefault(const std::wstring& ws, int defVal) {
    try {
        int p = std::stoi(ws);
        if (p <= 0 || p > 65535) return defVal;
        return p;
    }
    catch (...) {
        return defVal;
    }
}

static void SendEmail() {
    // Read GUI fields
    std::wstring wsServer = TrimW(GetTextW(g_server));
    std::wstring wsPort = TrimW(GetTextW(g_port));
    bool useAuth = (SendMessageW(g_useAuth, BM_GETCHECK, 0, 0) == BST_CHECKED);

    std::wstring wsUser = GetTextW(g_user);
    std::wstring wsPass = GetTextW(g_pass);

    std::wstring wsFrom = TrimW(GetTextW(g_from));
    std::wstring wsTo = TrimW(GetTextW(g_to));
    std::wstring wsSubject = GetTextW(g_subject);
    std::wstring wsBody = GetTextW(g_body);

    if (wsServer.empty()) wsServer = L"localhost";
    int port = GetPortOrDefault(wsPort, 25);

    if (wsFrom.empty() || wsTo.empty()) {
        AppendLog(L"Ошибка: поля From и To обязательны.");
        return;
    }

    // Build settings
    SmtpSettings smtp;
    smtp.server = WStringToUtf8(wsServer);
    smtp.port = (unsigned short)port;
    smtp.useAuth = useAuth;

    if (useAuth) {
        smtp.user = WStringToUtf8(wsUser);
        smtp.password = WStringToUtf8(wsPass);
    }

    // Build draft
    MailDraft d;
    d.from = WStringToUtf8(wsFrom);
    d.to = SplitRecipientsUtf8(wsTo);
    d.subject = WStringToUtf8(wsSubject);
    d.body = WStringToUtf8(wsBody);

    // Attachments
    d.attachments.clear();
    for (const auto& wpath : g_attachedFiles) {
        std::filesystem::path p(wpath);

        Attachment a;
        a.filename = p.filename().string();
        a.mimeType = GuessMimeType(p);

        if (!ReadFileBytes(p, a.data)) {
            AppendLog(L"Не удалось прочитать файл: " + std::wstring(wpath));
            return;
        }
        d.attachments.push_back(std::move(a));
    }

    AppendLog(L"Отправка письма...");
    std::string err;
    bool ok = MailService::send(smtp, d, err);

    if (ok) {
        AppendLog(L"OK: письмо отправлено.");
    }
    else {
        AppendLog(L"FAIL: " + std::wstring(err.begin(), err.end()));
    }
}

// ---------- Window proc ----------
static void ToggleAuthFields(bool enabled) {
    EnableWindow(g_user, enabled);
    EnableWindow(g_pass, enabled);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hWnd = hWnd;

        // basic font
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        int x = 12, y = 12;
        int wLabel = 70, wEdit = 260, h = 22, gap = 6;

        auto makeLabel = [&](const wchar_t* text, int lx, int ly, int lw, int lh) {
            HWND hL = CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE,
                lx, ly, lw, lh, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(hL, WM_SETFONT, (WPARAM)hFont, TRUE);
            return hL;
            };

        auto makeEdit = [&](int id, int ex, int ey, int ew, int eh, DWORD styleExtra = 0) {
            HWND hE = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | styleExtra,
                ex, ey, ew, eh, hWnd, (HMENU)(INT_PTR)id, nullptr, nullptr);
            SendMessageW(hE, WM_SETFONT, (WPARAM)hFont, TRUE);
            return hE;
            };

        auto makeBtn = [&](const wchar_t* text, int id, int bx, int by, int bw, int bh) {
            HWND hB = CreateWindowW(L"BUTTON", text, WS_CHILD | WS_VISIBLE,
                bx, by, bw, bh, hWnd, (HMENU)(INT_PTR)id, nullptr, nullptr);
            SendMessageW(hB, WM_SETFONT, (WPARAM)hFont, TRUE);
            return hB;
            };

        // Server / Port
        makeLabel(L"Server:", x, y + 3, wLabel, h);
        g_server = makeEdit(IDC_SERVER, x + wLabel, y, wEdit, h);
        SetTextW(g_server, L"localhost");

        makeLabel(L"Port:", x + wLabel + wEdit + 10, y + 3, 40, h);
        g_port = makeEdit(IDC_PORT, x + wLabel + wEdit + 55, y, 70, h);
        SetTextW(g_port, L"25");

        y += h + gap;

        // Auth checkbox + user/pass
        g_useAuth = CreateWindowW(L"BUTTON", L"Use AUTH", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            x, y, 100, h, hWnd, (HMENU)(INT_PTR)IDC_USEAUTH, nullptr, nullptr);
        SendMessageW(g_useAuth, WM_SETFONT, (WPARAM)hFont, TRUE);

        makeLabel(L"User:", x + 110, y + 3, 45, h);
        g_user = makeEdit(IDC_USER, x + 155, y, 190, h);

        makeLabel(L"Pass:", x + 350, y + 3, 45, h);
        g_pass = makeEdit(IDC_PASS, x + 395, y, 190, h, ES_PASSWORD);

        ToggleAuthFields(false);

        y += h + gap;

        // From / To
        makeLabel(L"From:", x, y + 3, wLabel, h);
        g_from = makeEdit(IDC_FROM, x + wLabel, y, 260, h);
        SetTextW(g_from, L"user@example.com");

        makeLabel(L"To:", x + 350, y + 3, 30, h);
        g_to = makeEdit(IDC_TO, x + 380, y, 260, h);
        SetTextW(g_to, L"recipient@example.com");

        y += h + gap;

        // Subject
        makeLabel(L"Subject:", x, y + 3, wLabel, h);
        g_subject = makeEdit(IDC_SUBJECT, x + wLabel, y, 568, h);
        SetTextW(g_subject, L"Тест тема");

        y += h + gap;

        // Body (multiline)
        makeLabel(L"Body:", x, y + 3, wLabel, h);
        g_body = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
            x + wLabel, y, 568, 140, hWnd, (HMENU)(INT_PTR)IDC_BODY, nullptr, nullptr);
        SendMessageW(g_body, WM_SETFONT, (WPARAM)hFont, TRUE);
        SetTextW(g_body, L"Привет!\r\nЭто письмо из GUI.\r\n");

        y += 140 + gap;

        // Attachments list + buttons
        makeLabel(L"Files:", x, y + 3, wLabel, h);
        g_attachList = CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL,
            x + wLabel, y, 420, 90, hWnd, (HMENU)(INT_PTR)IDC_ATTACH_LIST, nullptr, nullptr);
        SendMessageW(g_attachList, WM_SETFONT, (WPARAM)hFont, TRUE);

   //g_btnAttach:
        makeBtn(L"Прикрепить файл...", IDC_BTN_ATTACH, x + wLabel + 430, y, 100, 26);
        makeBtn(L"Отправить", IDC_BTN_SEND, x + wLabel + 430, y + 32, 100, 26);

        y += 90 + gap;

        // Log
        makeLabel(L"Log:", x, y + 3, wLabel, h);
        g_log = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY,
            x + wLabel, y, 568, 120, hWnd, (HMENU)(INT_PTR)IDC_LOG, nullptr, nullptr);
        SendMessageW(g_log, WM_SETFONT, (WPARAM)hFont, TRUE);

        AppendLog(L"GUI готов. Нажми Прикрепить файл... чтобы добавить файл, затем Отправить.");
        break;
    }
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hWnd, &rc);

            HBRUSH hBrush = GetSysColorBrush(COLOR_WINDOW);
            FillRect(hdc, &rc, hBrush);

            return 1;
        }

        case WM_SIZE: {
            InvalidateRect(hWnd, NULL, TRUE);
            UpdateWindow(hWnd);
            break;
        }

        case WM_ACTIVATE: {
            if (wParam != WA_INACTIVE) {
                InvalidateRect(hWnd, NULL, TRUE);
                UpdateWindow(hWnd);
            }
            break;
        }
    case WM_COMMAND: {
        int id = LOWORD(wParam);

        if (id == IDC_USEAUTH && HIWORD(wParam) == BN_CLICKED) {
            bool enabled = (SendMessageW(g_useAuth, BM_GETCHECK, 0, 0) == BST_CHECKED);
            ToggleAuthFields(enabled);
            AppendLog(enabled ? L"AUTH включён" : L"AUTH выключен");
            return 0;
        }

        if (id == IDC_BTN_ATTACH && HIWORD(wParam) == BN_CLICKED) {
            AttachFileDialog();
            return 0;
        }

        if (id == IDC_BTN_SEND && HIWORD(wParam) == BN_CLICKED) {
            SendEmail();
            return 0;
        }
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ---------- Entry point ----------
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    const wchar_t* CLASS_NAME = L"UMailGuiClass";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClassExW(&wc);

    HWND hWnd = CreateWindowExW(
        0, CLASS_NAME, L"UMail GUI (Core интеграция)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 720, 640,
        nullptr, nullptr, hInst, nullptr
    );

    if (!hWnd) return 0;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
*/

// OOP_Project.cpp
// WinAPI GUI -> calls your core: MailService::send(SmtpSettings, MailDraft, err)

// OOP_Project.cpp
// WinAPI GUI -> calls your core: MailService::send(SmtpSettings, MailDraft, err)

// OOP_Project.cpp
// WinAPI GUI -> calls your core: MailService::send(SmtpSettings, MailDraft, err)

// OOP_Project.cpp
// WinAPI GUI -> calls your core: MailService::send(SmtpSettings, MailDraft, err)

#include <windows.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <algorithm>

#pragma comment(lib, "Comdlg32.lib")

// ==== подключи своё ядро ====
#include "mail_service.h"   // должен содержать SmtpSettings, MailDraft, Attachment, MailService::send

// ---------- UI IDs ----------
static const int IDC_SERVER = 101;
static const int IDC_PORT = 102;
static const int IDC_USEAUTH = 103;
static const int IDC_USER = 104;
static const int IDC_PASS = 105;

static const int IDC_FROM = 106;
static const int IDC_TO = 107;
static const int IDC_SUBJECT = 108;
static const int IDC_BODY = 109;

static const int IDC_ATTACH_LIST = 110;
static const int IDC_BTN_ATTACH = 111;
static const int IDC_BTN_SEND = 112;
static const int IDC_BTN_REMOVE = 113;  // Новая кнопка для удаления

static const int IDC_LOG = 114;

// ---------- Globals ----------
static HWND g_hWnd = nullptr;

static HWND g_server = nullptr, g_port = nullptr, g_useAuth = nullptr, g_user = nullptr, g_pass = nullptr;
static HWND g_from = nullptr, g_to = nullptr, g_subject = nullptr, g_body = nullptr;
static HWND g_attachList = nullptr;
static HWND g_log = nullptr;

// Метки для полей ввода
static HWND g_serverLabel = nullptr, g_portLabel = nullptr, g_fromLabel = nullptr, g_toLabel = nullptr;
static HWND g_subjectLabel = nullptr, g_bodyLabel = nullptr, g_attachLabel = nullptr, g_logLabel = nullptr;
static HWND g_userLabel = nullptr, g_passLabel = nullptr;

static std::vector<std::wstring> g_attachedFiles;

// ---------- Helpers ----------
static std::wstring GetTextW(HWND h) {
    int len = GetWindowTextLengthW(h);
    std::wstring s(len, L'\0');
    GetWindowTextW(h, s.data(), len + 1);
    return s;
}

static void SetTextW(HWND h, const std::wstring& s) {
    SetWindowTextW(h, s.c_str());
}

static void AppendLog(const std::wstring& line) {
    if (!g_log) return;
    int len = GetWindowTextLengthW(g_log);
    SendMessageW(g_log, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    std::wstring withNL = line + L"\r\n";
    SendMessageW(g_log, EM_REPLACESEL, FALSE, (LPARAM)withNL.c_str());
}

static std::string WStringToUtf8(const std::wstring& ws) {
    if (ws.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), out.data(), size, nullptr, nullptr);
    return out;
}

static std::wstring TrimW(std::wstring s) {
    auto notSpace = [](wchar_t c) { return c != L' ' && c != L'\t' && c != L'\r' && c != L'\n'; };
    while (!s.empty() && !notSpace(s.front())) s.erase(s.begin());
    while (!s.empty() && !notSpace(s.back())) s.pop_back();
    return s;
}

static std::vector<std::string> SplitRecipientsUtf8(const std::wstring& ws) {
    // split by ',' ';' whitespace-newlines
    std::vector<std::string> out;
    std::wstring s = ws;
    for (auto& ch : s) {
        if (ch == L';' || ch == L'\n' || ch == L'\r' || ch == L'\t') ch = L',';
    }
    std::wstringstream wss(s);
    std::wstring token;
    while (std::getline(wss, token, L',')) {
        token = TrimW(token);
        if (!token.empty())
            out.push_back(WStringToUtf8(token));
    }
    return out;
}

static std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

static std::string GuessMimeType(const std::filesystem::path& p) {
    std::string ext = ToLower(p.extension().string());
    if (ext == ".pdf") return "application/pdf";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".txt") return "text/plain; charset=utf-8";
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    return "application/octet-stream";
}

static bool ReadFileBytes(const std::filesystem::path& path, std::vector<unsigned char>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    f.seekg(0, std::ios::end);
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);

    if (size < 0) return false;
    out.resize((size_t)size);
    if (size > 0) f.read((char*)out.data(), size);
    return true;
}

static void RefreshAttachList() {
    SendMessageW(g_attachList, LB_RESETCONTENT, 0, 0);
    for (const auto& p : g_attachedFiles) {
        SendMessageW(g_attachList, LB_ADDSTRING, 0, (LPARAM)p.c_str());
    }
}

// Удалить выбранный файл
static void RemoveSelectedFile() {
    int selectedIndex = SendMessageW(g_attachList, LB_GETCURSEL, 0, 0);
    if (selectedIndex != LB_ERR && selectedIndex >= 0 && selectedIndex < (int)g_attachedFiles.size()) {
        std::wstring removedFile = g_attachedFiles[selectedIndex];
        std::filesystem::path path(removedFile);
        g_attachedFiles.erase(g_attachedFiles.begin() + selectedIndex);
        RefreshAttachList();
        AppendLog(L"Удалено вложение: " + path.filename().wstring());
    }
    else {
        AppendLog(L"Выберите файл для удаления");
    }
}

// Очистить все прикрепленные файлы (через контекстное меню или двойной клик)
static void ClearAllFiles() {
    if (!g_attachedFiles.empty()) {
        int count = g_attachedFiles.size();
        g_attachedFiles.clear();
        RefreshAttachList();
        AppendLog(L"Удалены все вложения (" + std::to_wstring(count) + L" файлов)");
    }
    else {
        AppendLog(L"Нет прикрепленных файлов для удаления");
    }
}

static void AttachFileDialog() {
    wchar_t fileName[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hWnd;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Все файлы\0*.*\0PDF\0*.pdf\0Изображения\0*.png;*.jpg;*.jpeg;*.gif\0Текст\0*.txt\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    // УБРАЛ OFN_ALLOWMULTISELECT - как было изначально

    if (GetOpenFileNameW(&ofn)) {
        g_attachedFiles.push_back(fileName);
        RefreshAttachList();
        std::filesystem::path path(fileName);
        AppendLog(L"Добавлено вложение: " + path.filename().wstring());
    }
}

static int GetPortOrDefault(const std::wstring& ws, int defVal) {
    try {
        int p = std::stoi(ws);
        if (p <= 0 || p > 65535) return defVal;
        return p;
    }
    catch (...) {
        return defVal;
    }
}

static void SendEmail() {
    // Read GUI fields
    std::wstring wsServer = TrimW(GetTextW(g_server));
    std::wstring wsPort = TrimW(GetTextW(g_port));
    bool useAuth = (SendMessageW(g_useAuth, BM_GETCHECK, 0, 0) == BST_CHECKED);

    std::wstring wsUser = GetTextW(g_user);
    std::wstring wsPass = GetTextW(g_pass);

    std::wstring wsFrom = TrimW(GetTextW(g_from));
    std::wstring wsTo = TrimW(GetTextW(g_to));
    std::wstring wsSubject = GetTextW(g_subject);
    std::wstring wsBody = GetTextW(g_body);

    if (wsServer.empty()) wsServer = L"localhost";
    int port = GetPortOrDefault(wsPort, 25);

    if (wsFrom.empty() || wsTo.empty()) {
        AppendLog(L"Ошибка: поля From и To обязательны.");
        return;
    }

    // Build settings
    SmtpSettings smtp;
    smtp.server = WStringToUtf8(wsServer);
    smtp.port = (unsigned short)port;
    smtp.useAuth = useAuth;

    if (useAuth) {
        smtp.user = WStringToUtf8(wsUser);
        smtp.password = WStringToUtf8(wsPass);
    }

    // Build draft
    MailDraft d;
    d.from = WStringToUtf8(wsFrom);
    d.to = SplitRecipientsUtf8(wsTo);
    d.subject = WStringToUtf8(wsSubject);
    d.body = WStringToUtf8(wsBody);

    // Attachments
    d.attachments.clear();
    for (const auto& wpath : g_attachedFiles) {
        std::filesystem::path p(wpath);

        Attachment a;

        // Получаем имя файла в UTF-8
        std::wstring wfilename = p.filename().wstring();
        a.filename = WStringToUtf8(wfilename);  // UTF-16 → UTF-8

        a.mimeType = GuessMimeType(p);

        if (!ReadFileBytes(p, a.data)) {
            AppendLog(L"Не удалось прочитать файл: " + std::wstring(wpath));
            return;
        }
        d.attachments.push_back(std::move(a));
    }

    AppendLog(L"Отправка письма...");
    std::string err;
    bool ok = MailService::send(smtp, d, err);

    if (ok) {
        AppendLog(L"OK: письмо отправлено.");
    }
    else {
        AppendLog(L"FAIL: " + std::wstring(err.begin(), err.end()));
    }
}

// ---------- Window proc ----------
static void ToggleAuthFields(bool enabled) {
    EnableWindow(g_user, enabled);
    EnableWindow(g_pass, enabled);
}

// Функция для обновления размеров элементов при изменении размера окна
static void UpdateLayout(int windowWidth, int windowHeight) {
    // Отступы
    const int padding = 12;
    const int controlHeight = 22;
    const int gap = 6;
    const int labelWidth = 70;

    // Вычисляем доступную ширину
    int availableWidth = windowWidth - 2 * padding;

    // Координаты Y для позиционирования
    int y = padding;

    // 1. Server и Port (с метками)
    int serverEditWidth = availableWidth - labelWidth - 10 - 70;
    int portEditWidth = 70;

    // Метка Server
    if (g_serverLabel) MoveWindow(g_serverLabel, padding, y + 3, labelWidth, controlHeight, TRUE);
    // Поле Server
    if (g_server) MoveWindow(g_server, padding + labelWidth, y, serverEditWidth, controlHeight, TRUE);

    // Метка Port
    if (g_portLabel) MoveWindow(g_portLabel, padding + labelWidth + serverEditWidth + 10, y + 3, 40, controlHeight, TRUE);
    // Поле Port
    if (g_port) MoveWindow(g_port, padding + labelWidth + serverEditWidth + 55, y, portEditWidth, controlHeight, TRUE);

    y += controlHeight + gap;

    // 2. Auth checkbox + User/Pass (с метками)
    int authWidth = 100;
    int userLabelWidth = 45;
    int userEditWidth = 190;
    int passLabelWidth = 45;
    int passEditWidth = 190;

    // Автоматически пересчитываем ширину полей User/Pass
    if (windowWidth > 600) {
        int remainingWidth = availableWidth - authWidth - userLabelWidth - passLabelWidth - 20;
        userEditWidth = remainingWidth / 2;
        passEditWidth = remainingWidth / 2;
    }

    // Checkbox Auth
    if (g_useAuth) MoveWindow(g_useAuth, padding, y, authWidth, controlHeight, TRUE);

    // Метка User
    if (g_userLabel) MoveWindow(g_userLabel, padding + authWidth + 5, y + 3, userLabelWidth, controlHeight, TRUE);
    // Поле User
    if (g_user) MoveWindow(g_user, padding + authWidth + 5 + userLabelWidth, y, userEditWidth, controlHeight, TRUE);

    // Метка Pass
    int passLabelX = padding + authWidth + 5 + userLabelWidth + userEditWidth + 5;
    if (g_passLabel) MoveWindow(g_passLabel, passLabelX, y + 3, passLabelWidth, controlHeight, TRUE);
    // Поле Pass
    if (g_pass) MoveWindow(g_pass, passLabelX + passLabelWidth, y, passEditWidth, controlHeight, TRUE);

    y += controlHeight + gap;

    // 3. From / To (с метками)
    int fromLabelWidth = labelWidth;
    int toLabelWidth = 30;
    int fieldWidth = (availableWidth - fromLabelWidth - toLabelWidth - 10) / 2;

    // Метка From
    if (g_fromLabel) MoveWindow(g_fromLabel, padding, y + 3, fromLabelWidth, controlHeight, TRUE);
    // Поле From
    if (g_from) MoveWindow(g_from, padding + fromLabelWidth, y, fieldWidth, controlHeight, TRUE);

    // Метка To
    int toLabelX = padding + fromLabelWidth + fieldWidth + 10;
    if (g_toLabel) MoveWindow(g_toLabel, toLabelX, y + 3, toLabelWidth, controlHeight, TRUE);
    // Поле To
    if (g_to) MoveWindow(g_to, toLabelX + toLabelWidth, y, fieldWidth, controlHeight, TRUE);

    y += controlHeight + gap;

    // 4. Subject (с меткой)
    if (g_subjectLabel) MoveWindow(g_subjectLabel, padding, y + 3, labelWidth, controlHeight, TRUE);
    if (g_subject) MoveWindow(g_subject, padding + labelWidth, y, availableWidth - labelWidth, controlHeight, TRUE);

    y += controlHeight + gap;

    // 5. Body (с меткой)
    int bodyHeight = (windowHeight - y - 90 - 120 - 3 * gap) / 2;
    if (g_bodyLabel) MoveWindow(g_bodyLabel, padding, y + 3, labelWidth, controlHeight, TRUE);
    if (g_body) MoveWindow(g_body, padding + labelWidth, y, availableWidth - labelWidth, bodyHeight, TRUE);

    y += bodyHeight + gap;

    // 6. Attachments list + buttons (с меткой)
    int listWidth = availableWidth - labelWidth - 110;

    // Проверяем, чтобы listWidth не был отрицательным
    if (listWidth < 100) listWidth = 100;

    // Проверяем, чтобы кнопки помещались
    int minRequiredWidth = labelWidth + listWidth + 110;
    if (availableWidth < minRequiredWidth) {
        listWidth = availableWidth - labelWidth - 110;
    }

    if (g_attachLabel) MoveWindow(g_attachLabel, padding, y + 3, labelWidth, controlHeight, TRUE);
    if (g_attachList) MoveWindow(g_attachList, padding + labelWidth, y, listWidth, 90, TRUE);

    // Кнопки - только 2 кнопки вертикально
    int btnX = padding + labelWidth + listWidth + 10;
    int btnWidth = 100;

    // Проверяем, чтобы кнопки не выходили за пределы окна
    if (btnX + btnWidth > windowWidth - padding) {
        btnX = windowWidth - padding - btnWidth;
    }

    // Прикрепление (выше)
    HWND btnAttach = GetDlgItem(g_hWnd, IDC_BTN_ATTACH);
    // Удаление (ниже)
    HWND btnRemove = GetDlgItem(g_hWnd, IDC_BTN_REMOVE);
    // Отправка (под кнопкой удаления)
    HWND btnSend = GetDlgItem(g_hWnd, IDC_BTN_SEND);

    int btnSpacing = 6;

    if (btnAttach) MoveWindow(btnAttach, btnX, y, btnWidth, 26, TRUE);
    if (btnRemove) MoveWindow(btnRemove, btnX, y + 26 + btnSpacing, btnWidth, 26, TRUE);
    if (btnSend) MoveWindow(btnSend, btnX, y + (26 + btnSpacing) * 2, btnWidth, 26, TRUE);

    y += 90 + gap;

    // 7. Log (с меткой)
    int logHeight = windowHeight - y - padding;
    if (logHeight < 50) logHeight = 50; // Минимальная высота для лога

    if (g_logLabel) MoveWindow(g_logLabel, padding, y + 3, labelWidth, controlHeight, TRUE);
    if (g_log) MoveWindow(g_log, padding + labelWidth, y, availableWidth - labelWidth, logHeight, TRUE);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hWnd = hWnd;

        // basic font
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        int x = 12, y = 12;
        int wLabel = 70, wEdit = 260, h = 22, gap = 6;

        auto makeLabel = [&](const wchar_t* text, int lx, int ly, int lw, int lh) {
            HWND hL = CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE,
                lx, ly, lw, lh, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(hL, WM_SETFONT, (WPARAM)hFont, TRUE);
            return hL;
            };

        auto makeEdit = [&](int id, int ex, int ey, int ew, int eh, DWORD styleExtra = 0) {
            HWND hE = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | styleExtra,
                ex, ey, ew, eh, hWnd, (HMENU)(INT_PTR)id, nullptr, nullptr);
            SendMessageW(hE, WM_SETFONT, (WPARAM)hFont, TRUE);
            return hE;
            };

        auto makeBtn = [&](const wchar_t* text, int id, int bx, int by, int bw, int bh) {
            HWND hB = CreateWindowW(L"BUTTON", text, WS_CHILD | WS_VISIBLE,
                bx, by, bw, bh, hWnd, (HMENU)(INT_PTR)id, nullptr, nullptr);
            SendMessageW(hB, WM_SETFONT, (WPARAM)hFont, TRUE);
            return hB;
            };

        // Server / Port
        g_serverLabel = makeLabel(L"Server:", x, y + 3, wLabel, h);
        g_server = makeEdit(IDC_SERVER, x + wLabel, y, wEdit, h);
        SetTextW(g_server, L"localhost");

        g_portLabel = makeLabel(L"Port:", x + wLabel + wEdit + 10, y + 3, 40, h);
        g_port = makeEdit(IDC_PORT, x + wLabel + wEdit + 55, y, 70, h);
        SetTextW(g_port, L"25");

        y += h + gap;

        // Auth checkbox + user/pass
        g_useAuth = CreateWindowW(L"BUTTON", L"Use AUTH", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            x, y, 100, h, hWnd, (HMENU)(INT_PTR)IDC_USEAUTH, nullptr, nullptr);
        SendMessageW(g_useAuth, WM_SETFONT, (WPARAM)hFont, TRUE);

        g_userLabel = makeLabel(L"User:", x + 110, y + 3, 45, h);
        g_user = makeEdit(IDC_USER, x + 155, y, 190, h);

        g_passLabel = makeLabel(L"Pass:", x + 350, y + 3, 45, h);
        g_pass = makeEdit(IDC_PASS, x + 395, y, 190, h, ES_PASSWORD);

        ToggleAuthFields(false);

        y += h + gap;

        // From / To
        g_fromLabel = makeLabel(L"From:", x, y + 3, wLabel, h);
        g_from = makeEdit(IDC_FROM, x + wLabel, y, 260, h);
        SetTextW(g_from, L"user@example.com");

        g_toLabel = makeLabel(L"To:", x + 350, y + 3, 30, h);
        g_to = makeEdit(IDC_TO, x + 380, y, 260, h);
        SetTextW(g_to, L"recipient@example.com");

        y += h + gap;

        // Subject
        g_subjectLabel = makeLabel(L"Subject:", x, y + 3, wLabel, h);
        g_subject = makeEdit(IDC_SUBJECT, x + wLabel, y, 568, h);
        SetTextW(g_subject, L"Тест тема");

        y += h + gap;

        // Body (multiline)
        g_bodyLabel = makeLabel(L"Body:", x, y + 3, wLabel, h);
        g_body = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
            x + wLabel, y, 568, 140, hWnd, (HMENU)(INT_PTR)IDC_BODY, nullptr, nullptr);
        SendMessageW(g_body, WM_SETFONT, (WPARAM)hFont, TRUE);
        SetTextW(g_body, L"Привет!\r\nЭто письмо из GUI.\r\n");

        y += 140 + gap;

        // Attachments list + buttons
        g_attachLabel = makeLabel(L"Files:", x, y + 3, wLabel, h);
        g_attachList = CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL,
            x + wLabel, y, 420, 90, hWnd, (HMENU)(INT_PTR)IDC_ATTACH_LIST, nullptr, nullptr);
        SendMessageW(g_attachList, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Кнопки на русском - ТОЛЬКО 3 КНОПКИ ВЕРТИКАЛЬНО
        int btnX = x + wLabel + 430;
        int btnWidth = 100;
        int btnHeight = 26;
        int btnSpacing = 6;

        // Первая кнопка - Прикрепить
        makeBtn(L"Прикрепить...", IDC_BTN_ATTACH, btnX, y, btnWidth, btnHeight);

        // Вторая кнопка - Удалить (ниже)
        makeBtn(L"Удалить", IDC_BTN_REMOVE, btnX, y + btnHeight + btnSpacing, btnWidth, btnHeight);

        // Третья кнопка - Отправить (еще ниже)
        makeBtn(L"Отправить", IDC_BTN_SEND, btnX, y + (btnHeight + btnSpacing) * 2, btnWidth, btnHeight);

        y += 90 + gap;

        // Log
        g_logLabel = makeLabel(L"Log:", x, y + 3, wLabel, h);
        g_log = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY,
            x + wLabel, y, 568, 120, hWnd, (HMENU)(INT_PTR)IDC_LOG, nullptr, nullptr);
        SendMessageW(g_log, WM_SETFONT, (WPARAM)hFont, TRUE);

        AppendLog(L"UMail готов. Нажмите << Прикрепить... >> чтобы добавить файл, затем << Отправить >>.");
        break;
    }

    case WM_SIZE: {
        // Получаем новый размер клиентской области
        int newWidth = LOWORD(lParam);
        int newHeight = HIWORD(lParam);

        // Обновляем layout
        UpdateLayout(newWidth, newHeight);

        // Перерисовываем окно
        InvalidateRect(hWnd, NULL, TRUE);
        UpdateWindow(hWnd);
        break;
    }

    case WM_GETMINMAXINFO: {
        // Устанавливаем минимальный размер окна
        MINMAXINFO* pMinMax = (MINMAXINFO*)lParam;
        pMinMax->ptMinTrackSize.x = 720;  // минимальная ширина как изначально
        pMinMax->ptMinTrackSize.y = 500;  // минимальная высота
        return 0;
    }

    case WM_ERASEBKGND: {
        // Обработка фона
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hWnd, &rc);

        // Заливаем фон цветом окна
        HBRUSH hBrush = GetSysColorBrush(COLOR_WINDOW);
        FillRect(hdc, &rc, hBrush);

        return 1; // Фон обработан
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);

        if (id == IDC_USEAUTH && HIWORD(wParam) == BN_CLICKED) {
            bool enabled = (SendMessageW(g_useAuth, BM_GETCHECK, 0, 0) == BST_CHECKED);
            ToggleAuthFields(enabled);
            AppendLog(enabled ? L"AUTH включён" : L"AUTH выключен");
            return 0;
        }

        if (id == IDC_BTN_ATTACH && HIWORD(wParam) == BN_CLICKED) {
            AttachFileDialog();
            return 0;
        }

        if (id == IDC_BTN_REMOVE && HIWORD(wParam) == BN_CLICKED) {
            RemoveSelectedFile();
            return 0;
        }

        if (id == IDC_BTN_SEND && HIWORD(wParam) == BN_CLICKED) {
            SendEmail();
            return 0;
        }

        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ---------- Entry point ----------
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    const wchar_t* CLASS_NAME = L"UMailGuiClass";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.style = CS_HREDRAW | CS_VREDRAW; // Перерисовывать при изменении размера

    RegisterClassExW(&wc);

    HWND hWnd = CreateWindowExW(
        0, CLASS_NAME, L"UMail GUI",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 720, 640,
        nullptr, nullptr, hInst, nullptr
    );

    if (!hWnd) return 0;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}