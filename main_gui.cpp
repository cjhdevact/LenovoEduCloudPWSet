#include "common.h"

#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <stdexcept>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

// ---------- 控件 ID ----------

#define IDC_STATIC_TITLE    1000
#define IDC_EDIT_PATH       1001
#define IDC_BTN_BROWSE      1002
#define IDC_BTN_ENCRYPT     1003
#define IDC_BTN_DECRYPT     1004
#define IDC_BTN_USERNAME    1005
#define IDC_EDIT_OUTPUT     1006

// ---------- 全局 ----------

static HWND g_hEditPath = NULL;
static HWND g_hEditOutput = NULL;
static HINSTANCE g_hInst = NULL;
static HFONT g_hFont = NULL;

// ---------- UI 辅助 ----------

static void append_output(const std::wstring& text) {
    int len = GetWindowTextLengthW(g_hEditOutput);
    SendMessageW(g_hEditOutput, EM_SETSEL, len, len);
    std::wstring with_nl = text + L"\r\n";
    SendMessageW(g_hEditOutput, EM_REPLACESEL, FALSE, (LPARAM)with_nl.c_str());
}

static void clear_output() {
    SetWindowTextW(g_hEditOutput, L"");
}

static std::wstring get_selected_path() {
    int len = GetWindowTextLengthW(g_hEditPath);
    if (len <= 0) return L"";
    std::vector<wchar_t> buf(len + 1);
    GetWindowTextW(g_hEditPath, &buf[0], len + 1);
    std::wstring path(&buf[0], len);

    size_t b = path.find_first_not_of(L" \t\r\n");
    if (b == std::wstring::npos) return L"";
    size_t e = path.find_last_not_of(L" \t\r\n");
    path = path.substr(b, e - b + 1);

    if (path.size() >= 2 &&
        ((path[0] == L'"' && path[path.size() - 1] == L'"') ||
         (path[0] == L'\'' && path[path.size() - 1] == L'\''))) {
        path = path.substr(1, path.size() - 2);
    }

    while (path.size() > 3 &&
           (path[path.size() - 1] == L'\\' || path[path.size() - 1] == L'/')) {
        path.erase(path.size() - 1);
    }

    return path;
}

static bool ensure_path_selected() {
    std::wstring path = get_selected_path();
    if (path.empty()) {
        MessageBoxW(NULL, L"请先选择或输入联想云教室控制台安装路径。", L"提示", MB_OK | MB_ICONWARNING);
        return false;
    }
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        MessageBoxW(NULL, L"路径不存在或不是目录，请检查后重试。", L"提示", MB_OK | MB_ICONWARNING);
        return false;
    }
    return true;
}

// ---------- 输入密码对话框 ----------

static bool prompt_password(HWND hWnd, std::wstring& out) {
    struct InputDlg {
        static LRESULT CALLBACK Proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
            static HWND hEdit = NULL;
            switch (msg) {
            case WM_CREATE: {
                HFONT hFont = CreateFontW(
                    -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                    L"微软雅黑");

                HWND hStatic = CreateWindowW(L"STATIC", L"请输入新密码:",
                              WS_CHILD | WS_VISIBLE,
                              15, 20, 100, 22, hWnd, NULL, NULL, NULL);
                hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                              WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                              120, 18, 230, 24, hWnd, (HMENU)100, NULL, NULL);
                HWND hOk = CreateWindowW(L"BUTTON", L"确定",
                              WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                              120, 60, 90, 28, hWnd, (HMENU)IDOK, NULL, NULL);
                HWND hCancel = CreateWindowW(L"BUTTON", L"取消",
                              WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                              220, 60, 90, 28, hWnd, (HMENU)IDCANCEL, NULL, NULL);

                SendMessageW(hStatic, WM_SETFONT, (WPARAM)hFont, TRUE);
                SendMessageW(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
                SendMessageW(hOk, WM_SETFONT, (WPARAM)hFont, TRUE);
                SendMessageW(hCancel, WM_SETFONT, (WPARAM)hFont, TRUE);

                SetFocus(hEdit);
                return 0;
            }
            case WM_COMMAND:
                if (LOWORD(wParam) == IDOK) {
                    wchar_t buf[256] = {0};
                    GetWindowTextW(hEdit, buf, 256);
                    SetWindowTextW(GetParent(hWnd), buf);
                    DestroyWindow(hWnd);
                    return 0;
                } else if (LOWORD(wParam) == IDCANCEL) {
                    SetWindowTextW(GetParent(hWnd), L"");
                    DestroyWindow(hWnd);
                    return 0;
                }
                return 0;
            case WM_CLOSE:
                SetWindowTextW(GetParent(hWnd), L"");
                DestroyWindow(hWnd);
                return 0;
            }
            return DefWindowProcW(hWnd, msg, wParam, lParam);
        }
    };

    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc;
        memset(&wc, 0, sizeof(wc));
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = InputDlg::Proc;
        wc.hInstance = g_hInst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"PWInputDlg";
        RegisterClassExW(&wc);
        registered = true;
    }

    HWND hParent = CreateWindowExW(0, L"STATIC", L"", WS_POPUP,
                                   0, 0, 0, 0, NULL, NULL, g_hInst, NULL);

    RECT rcParent;
    GetWindowRect(hWnd, &rcParent);
    int dlgW = 380;
    int dlgH = 140;
    int dlgX = rcParent.left + (rcParent.right - rcParent.left - dlgW) / 2;
    int dlgY = rcParent.top + (rcParent.bottom - rcParent.top - dlgH) / 2;

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        L"PWInputDlg", L"重置密码",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        dlgX, dlgY, dlgW, dlgH,
        hParent, NULL, g_hInst, NULL);

    EnableWindow(hWnd, FALSE);

    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);

    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    EnableWindow(hWnd, TRUE);
    SetActiveWindow(hWnd);

    wchar_t result[256] = {0};
    GetWindowTextW(hParent, result, 256);
    DestroyWindow(hParent);

    out = result;
    return !out.empty();
}

// ---------- 按钮处理 ----------

static void on_browse(HWND hWnd) {
    BROWSEINFOW bi;
    memset(&bi, 0, sizeof(bi));
    bi.hwndOwner = hWnd;
    bi.lpszTitle = L"选择联想云教室控制台安装路径";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return;

    wchar_t path[MAX_PATH] = {0};
    if (SHGetPathFromIDListW(pidl, path)) {
        SetWindowTextW(g_hEditPath, path);
    }
    CoTaskMemFree(pidl);
}

static void on_reset_password(HWND hWnd) {
    if (!ensure_path_selected()) return;

    std::wstring dir = get_selected_path();
    std::wstring pub_path = join_path(dir, L"wfbspubkey.bin");
    std::wstring out_path = join_path(dir, L"wfpwd.bin");

    if (GetFileAttributesW(pub_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(hWnd,
                    L"未找到 wfbspubkey.bin，请确认选择的路径是联想云教室控制台安装目录。",
                    L"重置密码失败",
                    MB_OK | MB_ICONERROR);
        return;
    }

    std::wstring newpwd;
    if (!prompt_password(hWnd, newpwd)) return;

    clear_output();

    try {
        std::vector<unsigned char> pub_der = load_bin_file(pub_path);
        std::string password = wstring_to_utf8(newpwd);
        std::vector<unsigned char> cipher = cng_encrypt(pub_der, password);
        std::string hex = bytes_to_hex(cipher);
        write_file_w(out_path, hex + "\n", false);

        append_output(L"重置密码成功");
        append_output(L"公钥: wfbspubkey.bin");
        append_output(L"新密码: " + newpwd);
        append_output(L"已写入: wfpwd.bin");
    } catch (const std::exception& ex) {
        std::string msg = ex.what();
        MessageBoxA(hWnd, msg.c_str(), "重置密码失败", MB_OK | MB_ICONERROR);
        append_output(L"重置密码失败: " + std::wstring(msg.begin(), msg.end()));
    }
}

static void on_read_password(HWND hWnd) {
    if (!ensure_path_selected()) return;
    std::wstring dir = get_selected_path();
    std::wstring pri_path = join_path(dir, L"wfbsprikey.bin");
    std::wstring in_path = join_path(dir, L"wfpwd.bin");

    if (GetFileAttributesW(pri_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(hWnd, L"未找到 wfbsprikey.bin，请确认选择的路径正确。",
                    L"读取密码失败", MB_OK | MB_ICONERROR);
        return;
    }
    if (GetFileAttributesW(in_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(hWnd, L"未找到 wfpwd.bin，请先重置密码。",
                    L"读取密码失败", MB_OK | MB_ICONERROR);
        return;
    }

    clear_output();

    try {
        std::vector<unsigned char> pri_der = load_bin_file(pri_path);
        std::vector<unsigned char> cipher = load_bin_file(in_path);
        std::vector<unsigned char> plain = cng_decrypt(pri_der, cipher);
        std::string text(plain.begin(), plain.end());

        append_output(L"读取密码成功");
        append_output(L"当前密码: " + std::wstring(text.begin(), text.end()));
    } catch (const std::exception& ex) {
        std::string msg = ex.what();
        MessageBoxA(hWnd, msg.c_str(), "读取密码失败", MB_OK | MB_ICONERROR);
        append_output(L"读取密码失败: " + std::wstring(msg.begin(), msg.end()));
    }
}

static void on_username(HWND hWnd) {
    if (!ensure_path_selected()) return;
    std::wstring dir = get_selected_path();
    std::wstring ini_path = join_path(dir, L"BSSvrCfg.ini");

    if (GetFileAttributesW(ini_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(hWnd, L"未找到 BSSvrCfg.ini，请确认选择的路径正确。",
                    L"读取失败", MB_OK | MB_ICONERROR);
        return;
    }

    clear_output();

    try {
        std::string user = read_mag_username(ini_path);
        if (user.empty()) {
            append_output(L"未找到 MagUserName 或值为空");
            MessageBoxW(hWnd, L"未找到 MagUserName 或值为空。", L"提示", MB_OK | MB_ICONWARNING);
            return;
        }
        append_output(L"MagUserName: " + std::wstring(user.begin(), user.end()));
    } catch (const std::exception& ex) {
        std::string msg = ex.what();
        MessageBoxA(hWnd, msg.c_str(), "读取失败", MB_OK | MB_ICONERROR);
        append_output(L"读取失败: " + std::wstring(msg.begin(), msg.end()));
    }
}

// ---------- 窗口过程 ----------

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hFont = CreateFontW(
            -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            L"微软雅黑");

        HWND hTitle1 = CreateWindowW(L"STATIC", L"联想云教室控制台密码重置工具 Made by CJH.",
                      WS_CHILD | WS_VISIBLE,
                      15, 12, 580, 20, hWnd, (HMENU)IDC_STATIC_TITLE, g_hInst, NULL);

        HWND hTitle2 = CreateWindowW(L"STATIC", L"Github: https://github.com/cjhdevact/LenovoEduCloudPWSet",
                      WS_CHILD | WS_VISIBLE,
                      15, 34, 580, 20, hWnd, NULL, g_hInst, NULL);

        HWND hStatic = CreateWindowW(L"STATIC", L"联想云教室控制台安装路径:",
                      WS_CHILD | WS_VISIBLE,
                      15, 66, 220, 22, hWnd, NULL, g_hInst, NULL);

        g_hEditPath = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                      WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                      15, 93, 460, 24, hWnd, (HMENU)IDC_EDIT_PATH, g_hInst, NULL);

        HWND hBtnBrowse = CreateWindowW(L"BUTTON", L"选择路径...",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      485, 93, 100, 24, hWnd, (HMENU)IDC_BTN_BROWSE, g_hInst, NULL);

        HWND hBtnReset = CreateWindowW(L"BUTTON", L"重置密码",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      15, 145, 120, 32, hWnd, (HMENU)IDC_BTN_ENCRYPT, g_hInst, NULL);

        HWND hBtnRead = CreateWindowW(L"BUTTON", L"读取密码",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      15, 185, 120, 32, hWnd, (HMENU)IDC_BTN_DECRYPT, g_hInst, NULL);

        HWND hBtnUser = CreateWindowW(L"BUTTON", L"读取用户名",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      15, 225, 120, 32, hWnd, (HMENU)IDC_BTN_USERNAME, g_hInst, NULL);

        g_hEditOutput = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                      WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL |
                      ES_READONLY | WS_VSCROLL,
                      150, 145, 435, 195, hWnd, (HMENU)IDC_EDIT_OUTPUT, g_hInst, NULL);

        HWND controls[] = { hTitle1, hTitle2, hStatic, g_hEditPath, hBtnBrowse,
                            hBtnReset, hBtnRead, hBtnUser, g_hEditOutput };
        for (int i = 0; i < sizeof(controls) / sizeof(controls[0]); ++i) {
            SendMessageW(controls[i], WM_SETFONT, (WPARAM)g_hFont, TRUE);
        }

        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == IDC_BTN_BROWSE) on_browse(hWnd);
        else if (id == IDC_BTN_ENCRYPT) on_reset_password(hWnd);
        else if (id == IDC_BTN_DECRYPT) on_read_password(hWnd);
        else if (id == IDC_BTN_USERNAME) on_username(hWnd);
        return 0;
    }
    case WM_DESTROY:
        if (g_hFont) { DeleteObject(g_hFont); g_hFont = NULL; }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ---------- WinMain ----------

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    g_hInst = hInstance;

    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"LenovoEduCloudPWSetGUI";
    wc.hIcon = LoadIconW(hInstance, L"APPICON");
    wc.hIconSm = LoadIconW(hInstance, L"APPICON");
    RegisterClassExW(&wc);

    int winW = 620;
    int winH = 420;

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - winW) / 2;
    int y = (screenH - winH) / 2;

    HWND hWnd = CreateWindowExW(
        0, L"LenovoEduCloudPWSetGUI",
        L"LenovoEduCloudPWSet " APP_VERSION_W,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, winW, winH,
        NULL, NULL, hInstance, NULL);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}