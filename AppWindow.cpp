// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

#include "AppWindow.h"

#include <DispatcherQueue.h>
#include <functional>
#include <string>
#include <vector>
#include <ShObjIdl_core.h>
#include <Shellapi.h>
#include <ShlObj_core.h>
#include <winrt/windows.system.h>
#include "App.h"
#include "CheckFailure.h"
#include "DpiUtil.h"
#include "KeyManager.h"
#include "Assets.h"
#include "Resource.h"
#include "Mining.h"
#include "SettingsComponent.h"
#include "AddCollateralDialog.h"
#include "TextInputDialog.h"
#include "TextInputDialog2.h"
#include "Transaction.h"
#include "ViewComponent.h"
#include "StatusBar.h"
#include <process.h>
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <iostream>

using namespace Microsoft::WRL;
static constexpr size_t s_maxLoadString = 100;
static constexpr UINT s_runAsyncWindowMessage = WM_APP;

DWORD keyStartID = 500;
DWORD colKeyStartID = 700;
DWORD delcolKeyStartID = 850;
bool bDelColl = false;

static thread_local size_t s_appInstances = 0;
// The minimum height and width for Window Features.
// See https://developer.mozilla.org/en-US/docs/Web/API/Window/open#Size
static constexpr int s_minNewWindowSize = 100;

// Run Download and Install in another thread so we don't block the UI thread
DWORD WINAPI DownloadAndInstallWV2RT(_In_ LPVOID lpParameter)
{
    AppWindow* appWindow = (AppWindow*) lpParameter;

    int returnCode = 2; // Download failed
    // Use fwlink to download WebView2 Bootstrapper at runtime and invoke installation
    // Broken/Invalid Https Certificate will fail to download
    // Use of the download link below is governed by the below terms. You may acquire the link for your use at https://developer.microsoft.com/microsoft-edge/webview2/.
    // Microsoft owns all legal right, title, and interest in and to the WebView2 Runtime Bootstrapper ("Software") and related documentation, 
    // including any intellectual property in the Software. 
    // You must acquire all code, including any code obtained from a Microsoft URL, under a separate license directly from Microsoft, including a Microsoft download site 
    // (e.g., https://developer.microsoft.com/microsoft-edge/webview2/).
    HRESULT hr = URLDownloadToFile(NULL, L"https://go.microsoft.com/fwlink/p/?LinkId=2124703", L".\\MicrosoftEdgeWebview2Setup.exe", 0, 0);
    if (hr == S_OK)
    {
        // Either Package the WebView2 Bootstrapper with your app or download it using fwlink
        // Then invoke install at Runtime.
        SHELLEXECUTEINFO shExInfo = {0};
        shExInfo.cbSize = sizeof(shExInfo);
        shExInfo.fMask = SEE_MASK_NOASYNC;
        shExInfo.hwnd = 0;
        shExInfo.lpVerb = L"runas";
        shExInfo.lpFile = L"MicrosoftEdgeWebview2Setup.exe";
        shExInfo.lpParameters = L" /silent /install";
        shExInfo.lpDirectory = 0;
        shExInfo.nShow = 0;
        shExInfo.hInstApp = 0;

        if (ShellExecuteEx(&shExInfo))
        {
            returnCode = 0; // Install successfull
        }
        else
        {
            returnCode = 1; // Install failed
        }
    }

    appWindow->InstallComplete(returnCode);
    appWindow->Release();
    return returnCode;
}

// Creates a new window which is a copy of the entire app, but on the same thread.
AppWindow::AppWindow(
    UINT creationModeId,
    std::wstring initialUri,
    bool isMainWindow,
    std::function<void()> webviewCreatedCallback,
    bool customWindowRect,
    RECT windowRect,
    bool shouldHaveToolbar)
    : m_creationModeId(creationModeId),
    m_initialUri(initialUri),
    m_onWebViewFirstInitialized(webviewCreatedCallback)
{
    shouldHaveToolbar = false;

    // Initialize COM as STA.
    CHECK_FAILURE(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE));

    ++s_appInstances;

    WCHAR szTitle[s_maxLoadString]; // The title bar text
    LoadStringW(g_hInstance, IDS_APP_TITLE, szTitle, s_maxLoadString);

    if (customWindowRect)
    {
        m_mainWindow = CreateWindowExW(
            WS_EX_CONTROLPARENT, GetWindowClass(), szTitle, WS_OVERLAPPEDWINDOW, windowRect.left,
            windowRect.top, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, nullptr, nullptr, g_hInstance, nullptr);
    }
    else
    {
        auto wc = GetWindowClass();
        m_mainWindow = CreateWindowExW(
            WS_EX_CONTROLPARENT, wc, szTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
            0, CW_USEDEFAULT, 0, nullptr, nullptr, g_hInstance, nullptr);
    }

    SetWindowLongPtr(m_mainWindow, GWLP_USERDATA, (LONG_PTR)this);

#ifdef USE_WEBVIEW2_WIN10
    //! [TextScaleChanged1]
    if (winrt::try_get_activation_factory<winrt::Windows::UI::ViewManagement::UISettings>())
    {
        m_uiSettings = winrt::Windows::UI::ViewManagement::UISettings();
        m_uiSettings.TextScaleFactorChanged({ this, &AppWindow::OnTextScaleChanged });
    }
    //! [TextScaleChanged1]
#endif

    if (shouldHaveToolbar)
    {
        m_toolbar.Initialize(this);
    }

    UpdateCreationModeMenu();

    m_hMenu = ::GetMenu(m_mainWindow);
    // address menu
    m_Popup = ::GetSubMenu(m_hMenu, 0);
    // minging address list
    m_Popup = ::GetSubMenu(m_Popup, 4);

    // put mining addresses into menu
    for (int i = 0; i < cfg.addresses(); i++) {
        char* s = cfg.get(i)->address;
        WCHAR wszClassName[256];
        memset(wszClassName, 0, sizeof(wszClassName));
        MultiByteToWideChar(CP_ACP, 0, s, strlen(s) + 1, wszClassName,
            sizeof(wszClassName) / sizeof(wszClassName[0]));
        CHECK_FAILURE(::AppendMenu(m_Popup, MF_STRING, keyStartID + i, wszClassName));
        if (cfg.get(i)->active) UpdateKeyUI(i);
    }

    // setting menu
    m_Popup = ::GetSubMenu(m_hMenu, 4);
    // collateral list
    m_Popup = ::GetSubMenu(m_Popup, 3);
    // put collaterals into menu
    for (int i = 0; i < cfg.collaterals(); i++) {
        char* s = cfg.getColl(i);
        if (s == NULL) continue;
        WCHAR wszClassName[256];
        memset(wszClassName, 0, sizeof(wszClassName));
        MultiByteToWideChar(CP_ACP, 0, s, strlen(s) + 1, wszClassName,
            sizeof(wszClassName) / sizeof(wszClassName[0]));
        CHECK_FAILURE(::AppendMenu(m_Popup, MF_STRING, colKeyStartID + i, wszClassName));
    }

    m_Popup = ::GetSubMenu(m_Popup, 1);
    for (int i = 0; i < cfg.collaterals(); i++) {
        char* s = cfg.getColl(i);
        if (s == NULL) continue;
        WCHAR wszClassName[256];
        memset(wszClassName, 0, sizeof(wszClassName));
        MultiByteToWideChar(CP_ACP, 0, s, strlen(s) + 1, wszClassName,
            sizeof(wszClassName) / sizeof(wszClassName[0]));
        CHECK_FAILURE(::AppendMenu(m_Popup, MF_STRING, delcolKeyStartID + i, wszClassName));
    }

    ShowWindow(m_mainWindow, g_nCmdShow);
    UpdateWindow(m_mainWindow);

    // If no WebVieRuntime installed, create new thread to do install/download.
    // Otherwise just initialize webview.
    wil::unique_cotaskmem_string version_info;
    HRESULT hr = GetAvailableCoreWebView2BrowserVersionString(nullptr, &version_info);
    if (hr == S_OK && version_info != nullptr)
    {
        RunAsync([this] {
            InitializeWebView();
            });
    }
    else
    {
        if (isMainWindow) {
            AddRef();
            CreateThread(0, 0, DownloadAndInstallWV2RT, (void*)this, 0, 0);
        }
        else
        {
            MessageBox(m_mainWindow, L"WebView Runtime not installed", L"WebView Runtime Installation status", MB_OK);
        }
    }
}

PROCESS_INFORMATION omgd;
PROCESS_INFORMATION wallet;
char* tostr(const wchar_t* msg);
std::wstring towstr(const char* s);

std::wstring AppWindow::GetPassword(bool quick) {
    if (quick || time(NULL) - wptime < locktime) {
        return walletpass;
    }
    else {
        TextInputDialog dialog(GetMainWindow());
        if (dialog.confirmed) {
            walletpass = dialog.input;
            wptime = time(NULL);
            return walletpass;
        }
    }
    return L"";
}

void AppWindow::Authorize(bool quick) {
    std::wstring reply = GetPassword(quick);
    if (reply != L"") 
        CHECK_FAILURE(m_webView->PostWebMessageAsJson((L"{\"Password\":\"" + reply + L"\"}").c_str()));
}

void AppWindow::OmegaClose() {
    DWORD res, accum;
    accum = 0;
    do {
        res = WaitForSingleObject(omgd.hProcess, 3000);
        accum++;
    } while (res != WAIT_OBJECT_0 && accum < 20);

    if (res != WAIT_OBJECT_0) {
        TerminateProcess(omgd.hProcess, 0);
    }
    CloseHandle(omgd.hProcess);
    CloseHandle(omgd.hThread);

    accum = 0;
    do {
        res = WaitForSingleObject(wallet.hProcess, 3000);
        accum++;
    } while (res != WAIT_OBJECT_0 && accum < 20);

    if (res != WAIT_OBJECT_0) {
        TerminateProcess(wallet.hProcess, 0);
    }
    CloseHandle(wallet.hProcess);
    CloseHandle(wallet.hThread);
}

std::wstring GenOmgdPass() {
    char s[8];
    char base[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    srand(time(NULL));
    for (int i = 0; i < 6; i++) {
        s[i] = base[rand() % strlen(base)];
    }
    s[6] = '\n'; s[7] = '\0';

    FILE* fp = fopen(tostr((directory + L"/btcd.conf").c_str()), "rb");
    FILE* fw = fopen(tostr((directory + L"/btcd.conf-tmp").c_str()), "wb");
    char buf[2048];
    int pos = 0, begin = 0;
    while (!feof(fp)) {
        auto sln = fgets(buf + pos, 1024, fp);
        if (sln == NULL) continue;
        pos += strlen(sln);
        int j;
        for (j = begin; j < pos && buf[j] != '\n'; j++);
        if (buf[j] != '\n') continue;

        if (strncmp("rpcpass=", buf + begin, 8))
            fwrite(buf + begin, 1, j - begin + 1, fw);

        if (j < pos - 1) begin = j + 1;
        else begin = pos = 0;
        if (begin >= 1024) {
            memcpy(buf, buf + begin, pos - begin);
            pos -= begin;
            begin = 0;
        }
    }
    while (pos > begin) {
        int j;
        for (j = begin; j < pos && buf[j] != '\n'; j++);

        if (strncmp("rpcpass=", buf + begin, 8))
            fwrite(buf + begin, 1, j - begin + 1, fw);
        begin = j + 1;
    }
    fwrite("\nrpcpass=", 1, 9, fw);
    fwrite(s, 1, 7, fw);
    fclose(fp); fclose(fw);

    unlink(tostr((directory + L"/btcd.conf").c_str()));
    rename(tostr((directory + L"/btcd.conf-tmp").c_str()), tostr((directory + L"/btcd.conf").c_str()));

    s[6] = '\0';
    cfg.setomgdpass(s);
    cfg.save();

    return towstr(s);
}

void AppWindow::OmegaSart() {
    SECURITY_ATTRIBUTES sa_p;
    sa_p.nLength = sizeof(sa_p);
    sa_p.lpSecurityDescriptor = NULL;
    sa_p.bInheritHandle = TRUE;

    SECURITY_ATTRIBUTES sa_t;
    sa_t.nLength = sizeof(sa_t);
    sa_t.lpSecurityDescriptor = NULL;
    sa_t.bInheritHandle = TRUE;

    STARTUPINFOW si;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    ZeroMemory(&omgd, sizeof(omgd));

    struct _stat buf;
    int result;
    char timebuf[26];
    char* filename = "omgw/mainnet/wallet.db";
    errno_t err;

    si.cb = sizeof(STARTUPINFOW);
    GetStartupInfo(&si);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_OUTPUT_HANDLE);        //意思是：子进程的stderr输出到hStdErrWrite  

    HANDLE  hStdInRead;         //子进程用的stdin的读入端  
    HANDLE  hStdInWrite;        //主程序用的stdin的读入端  
    DWORD nwrt, n;

    result = _stat(filename, &buf);
    if (result != 0) {        // create wallet
        auto omgdpass = GenOmgdPass();
        auto szExe = directory + L"/wallet.exe --create /configfile:" + directory +
            L"/omgw/omgw.conf /appdata:" + directory  + L"/omgw /logdir:" + directory +
            L"/omgw /noclienttls /noservertls /btcdusername:admin /btcdpassword:" + omgdpass + L" /username:admin /password:" + omgdpass;

        if (!CreatePipe(&hStdInRead, &hStdInWrite, &sa_t, 0))
            return;

        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdInput = hStdInRead;

        BOOL res1 = CreateProcess(NULL, (LPWSTR)(szExe.c_str()),
            &sa_p, &sa_t, TRUE, 0, NULL, NULL, &si, &wallet);   // CREATE_NEW_CONSOLE => 0
        if (!res1) exit(GetLastError());

        TextInputDialog2 dialog(m_mainWindow);
        if (dialog.confirmed) {
            walletpass = dialog.input;

            auto cs = tostr(walletpass.c_str());
            int nc = strlen(cs);
            WriteFile(hStdInWrite, cs, nc, &nwrt, NULL);
            WriteFile(hStdInWrite, "\n", 1, &nwrt, NULL);
            WriteFile(hStdInWrite, cs, nc, &nwrt, NULL);
            WriteFile(hStdInWrite, "\n", 1, &nwrt, NULL);
            WriteFile(hStdInWrite, "n\nn\nOK\n", 7, &nwrt, NULL);

            WaitForSingleObject(wallet.hProcess, INFINITE);
            CloseHandle(wallet.hProcess);
            CloseHandle(wallet.hThread);
        }
        else {
            TerminateProcess(wallet.hProcess, 0);/*终止进程*/
            exit(9);
        }
    }
    
    // open wallet
    auto wp = cfg.omgdpass();
    std::wstring pass;
    if (wp) pass = towstr(wp);
    else pass = L"";

    auto szExe = directory + L"/wallet.exe /configfile:" + directory +
            L"/omgw/omgw.conf /appdata:" + directory + L"/omgw /logdir:" + directory +
            L"/omgw /noclienttls /noservertls /btcdusername:admin /btcdpassword:" + pass + L" /username:admin /password:" + pass;

    BOOL res1 = CreateProcess(NULL, (LPWSTR)(szExe.c_str()),
        &sa_p, &sa_t, TRUE, 0, NULL, NULL, &si, &wallet);   // CREATE_NEW_CONSOLE => 0
    if (!res1) exit(GetLastError());

    if (!CreatePipe(&hStdInRead, &hStdInWrite, &sa_t, 0))
        return;

    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdInput = hStdInRead;

    TCHAR szExe2[] = TEXT("omgd.exe /configfile btcd.conf");

    res1 = CreateProcess(NULL, szExe2,
        &sa_p, &sa_t, TRUE, 0, NULL, NULL, &si, &omgd);   // CREATE_NEW_CONSOLE => 0
    if (!res1) {
        DWORD dwcode = GetLastError();
        printf("%x\n", dwcode);
    }
    WriteFile(hStdInWrite, "\n\n\n", 3, &nwrt, NULL);
}

void AppWindow::AddKey(const char* s, int i) {
    int startID = 500;
    WCHAR wszClassName[256];
    memset(wszClassName, 0, sizeof(wszClassName));
    MultiByteToWideChar(CP_ACP, 0, s, strlen(s) + 1, wszClassName,
        sizeof(wszClassName) / sizeof(wszClassName[0]));
    CHECK_FAILURE(::AppendMenu(m_Popup, MF_STRING, startID + i, wszClassName));
}

// Register the Win32 window class for the app window.
PCWSTR AppWindow::GetWindowClass()
{
    // Only do this once
    static PCWSTR windowClass = [] {
        static WCHAR windowClass[s_maxLoadString];
        LoadStringW(g_hInstance, IDC_WEBVIEW2APISAMPLE, windowClass, s_maxLoadString);

        WNDCLASSEXW wcex;
        wcex.cbSize = sizeof(WNDCLASSEX);

        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WndProcStatic;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hInstance = g_hInstance;
        wcex.hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_WEBVIEW2APISAMPLE));
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_WEBVIEW2APISAMPLE);
        wcex.lpszClassName = windowClass;
        wcex.hIconSm = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_SMALL));

        RegisterClassExW(&wcex);
        return windowClass;
    }();
    return windowClass;
}

LRESULT CALLBACK AppWindow::WndProcStatic(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (auto app = (AppWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA))
    {
        LRESULT result = 0;
        if (app->HandleWindowMessage(hWnd, message, wParam, lParam, &result))
        {
            return result;
        }
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

#define IDC_STATUSBAR 900
HWND hStatusbar;

// Handle Win32 window messages sent to the main window
bool AppWindow::HandleWindowMessage(
    HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* result)
{
    // Give all components a chance to handle the message first.
    for (auto& component : m_components)
    {
        if (component->HandleWindowMessage(hWnd, message, wParam, lParam, result))
        {
            return true;
        }
    }

    switch (message)
    {
    case WM_CREATE:
    {
        int array[] = { 120,120 * 2,-1 };
        HINSTANCE hInst = GetModuleHandle(NULL);
        //创建Statusbar控件
        // hWnd
        hStatusbar = CreateWindowEx(0, STATUSCLASSNAME, TEXT(""), WS_CHILD | WS_BORDER | WS_VISIBLE,
            0, 0, 0, 0, GetMainWindow(), (HMENU)IDC_STATUSBAR, hInst, NULL);
        if (hStatusbar)
        {
            SendMessage(hStatusbar, SB_SETPARTS, (WPARAM)3, (LPARAM)array); //设置面板个数
            SendMessage(hStatusbar, SB_SETTEXT, (LPARAM)0, (WPARAM)TEXT("Omega Wallet")); //设置第二个面板内容
            SendMessage(hStatusbar, SB_SETTEXT, (LPARAM)1, (WPARAM)TEXT("Status")); //设置第二个面板内容
            SendMessage(hStatusbar, SB_SETTEXT, (LPARAM)2, (WPARAM)TEXT("Sync Progress")); //设置第三个面板内容
        }
        return true;
    }
    break;
    case WM_SIZE:
    {
        // Don't resize the app or webview when the app is minimized
        // let WM_SYSCOMMAND to handle it
        if (lParam != 0)
        {
            SendMessage(hStatusbar, WM_SIZE, 0, 0);
            ResizeEverything();
            return true;
        }
    }
    break;
    //! [DPIChanged]
    case WM_DPICHANGED:
    {
        m_toolbar.UpdateDpiAndTextScale();
        RECT* const newWindowSize = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(hWnd,
            nullptr,
            newWindowSize->left,
            newWindowSize->top,
            newWindowSize->right - newWindowSize->left,
            newWindowSize->bottom - newWindowSize->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        return true;
    }
    break;
    //! [DPIChanged]
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        return true;
    }
    break;
    case s_runAsyncWindowMessage:
    {
        auto* task = reinterpret_cast<std::function<void()>*>(wParam);
        (*task)();
        delete task;
        return true;
    }
    break;
    case WM_NCDESTROY:
    {
        int retValue = 0;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, NULL);
        NotifyClosed();
        if (--s_appInstances == 0)
        {
            PostQuitMessage(retValue);
        }
    }
    break;
    //! [RestartManager]
    case WM_QUERYENDSESSION:
    {
        // yes, we can shut down
        // Register how we might be restarted
        RegisterApplicationRestart(L"--restore", RESTART_NO_CRASH | RESTART_NO_HANG);
        *result = TRUE;
        return true;
    }
    break;
    case WM_ENDSESSION:
    {
        if (wParam == TRUE)
        {
            // save app state and exit.
            PostQuitMessage(0);
            return true;
        }
    }
    break;
    //! [RestartManager]
    case WM_KEYDOWN:
    {
        // If bit 30 is set, it means the WM_KEYDOWN message is autorepeated.
        // We want to ignore it in that case.
        if (!(lParam & (1 << 30)))
        {
            if (auto action = GetAcceleratorKeyFunction((UINT)wParam))
            {
                action();
                return true;
            }
        }
    }
    break;
    case WM_COMMAND:
    {
        // 800 is a constant determined according to resource.h such that ids from
        // it and a long span after it are not used, therefore the space can be
        // used for addresses. 100 should be enough for the space.
        if (LOWORD(wParam) >= delcolKeyStartID && LOWORD(wParam) < delcolKeyStartID + cfg.collaterals()) {
            // delete collateral
            m_hMenu = ::GetMenu(m_mainWindow);
            // address menu
            m_Popup = ::GetSubMenu(m_hMenu, 4);
            // minging address list
            m_Popup = ::GetSubMenu(m_Popup, 3);
            auto itemseq = cfg.collSeq(LOWORD(wParam) - delcolKeyStartID);
            ::RemoveMenu(m_Popup, itemseq + colKeyStartID, MF_BYCOMMAND);

            m_Popup = ::GetSubMenu(m_Popup, 1);
            ::RemoveMenu(m_Popup, itemseq + delcolKeyStartID, MF_BYCOMMAND);

            cfg.delColl(LOWORD(wParam) - delcolKeyStartID);
            
            return true;
        } else if (LOWORD(wParam) == ID_ADDCOLL) {
            // popup a dialog
            bDelColl = false;
            HMENU hMenu = ::GetMenu(m_mainWindow);
            m_Popup = ::GetSubMenu(m_hMenu, 4);
            m_Popup = ::GetSubMenu(m_Popup, 3);

            AddCollateralDialog dialog(m_mainWindow);
            if (dialog.confirmed) {
                auto coll = tostr(dialog.input.c_str());
                auto n = cfg.insertColl(coll);
                if (n < 0) return true;

                WCHAR wszClassName[256];
                memset(wszClassName, 0, sizeof(wszClassName));
                MultiByteToWideChar(CP_ACP, 0, coll, strlen(coll) + 1, wszClassName,
                    sizeof(wszClassName) / sizeof(wszClassName[0]));
                CHECK_FAILURE(::AppendMenu(m_Popup, MF_STRING, colKeyStartID + n, wszClassName));

                m_Popup = ::GetSubMenu(m_Popup, 1);
                CHECK_FAILURE(::AppendMenu(m_Popup, MF_STRING, delcolKeyStartID + n, wszClassName));

                return true;
            }
        }

        return ExecuteWebViewCommands(wParam, lParam) || ExecuteAppCommands(wParam, lParam);
    }
    break;
    }
    return false;
}

// Handle commands related to the WebView.
// This will do nothing if the WebView is not initialized.
bool AppWindow::ExecuteWebViewCommands(WPARAM wParam, LPARAM lParam)
{
    if (!m_webView)
        return false;
    switch (LOWORD(wParam))
    {
    case IDM_GET_BROWSER_VERSION_AFTER_CREATION:
    {
        //! [GetBrowserVersionString]
        wil::unique_cotaskmem_string version_info;
        m_webViewEnvironment->get_BrowserVersionString(&version_info);
        MessageBox(
            m_mainWindow, version_info.get(), L"Browser Version Info After WebView Creation",
            MB_OK);
        //! [GetBrowserVersionString]
        return true;
    }
    case IDM_CLOSE_WEBVIEW:
    {
        CloseWebView();
        return true;
    }
    case IDM_CLOSE_WEBVIEW_CLEANUP:
    {
        CloseWebView(true);
        return true;
    }
    case ID_OMEGA:
    {
        std::wstring m_scriptUri = L"http://omegasuite.org/omega/index.php";
        CHECK_FAILURE(m_webView->Navigate(m_scriptUri.c_str()));
        return true;
    }
    case IDM_SCENARIO_JAVA_SCRIPT:
    {
        WCHAR c_scriptPath[] = L"ScenarioJavaScriptDebugIndex.html";
        std::wstring m_scriptUri = GetLocalUri(c_scriptPath);
        CHECK_FAILURE(m_webView->Navigate(m_scriptUri.c_str()));
        return true;
    }
    case IDM_SCENARIO_TYPE_SCRIPT:
    {
        WCHAR c_scriptPath[] = L"ScenarioTypeScriptDebugIndex.html";
        std::wstring m_scriptUri = GetLocalUri(c_scriptPath);
        CHECK_FAILURE(m_webView->Navigate(m_scriptUri.c_str()));
    }
    }
    return false;
}

void AppWindow::updateLockTimeUI(int id) {
    HMENU hMenu = GetMenu(GetMainWindow());
    auto m_Popup = ::GetSubMenu(hMenu, 4);
    m_Popup = ::GetSubMenu(m_Popup, 1);

    CheckMenuRadioItem(
        m_Popup,
        IDM_1MIN,
        IDM_NOLOCK,
        id,
        MF_BYCOMMAND);
}

bool debugging = false;

bool AppWindow::Ready() {
    if (ready()) return true;
    ::MessageBox(m_mainWindow, L"服务器正在启动，请过几秒再试", L"服务器", 0);
    return false;
}

// Handle commands not related to the WebView, which will work even if the WebView
// is not currently initialized.
bool AppWindow::ExecuteAppCommands(WPARAM wParam, LPARAM lParam)
{
    switch (LOWORD(wParam))
    {
    case IDM_1MIN:
        locktime = time_t(60);
        updateLockTimeUI(IDM_1MIN);
        break;

    case IDM_5MIN:
        locktime = time_t(300);
        updateLockTimeUI(IDM_5MIN);
        break;

    case IDM_10MIN:
        locktime = time_t(600);
        updateLockTimeUI(IDM_10MIN);
        break;

    case IDM_30MIN:
        locktime = time_t(1800);
        updateLockTimeUI(IDM_30MIN);
        break;

    case IDM_NOLOCK:
        locktime = time_t(0);
        updateLockTimeUI(IDM_NOLOCK);
        break;

    case IDM_DEBUG:
        debugging = !debugging;
        ShowWindow(GetConsoleWindow(), debugging?SW_SHOW : SW_HIDE);
        break;

    case IDM_ABOUT:
        DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), m_mainWindow, About);
        return true;
    case IDM_GET_BROWSER_VERSION_BEFORE_CREATION:
    {
        wil::unique_cotaskmem_string version_info;
        GetAvailableCoreWebView2BrowserVersionString(nullptr, &version_info);
        MessageBox(
            m_mainWindow, version_info.get(), L"Browser Version Info Before WebView Creation",
            MB_OK);
        return true;
    }
    case IDM_EXIT:
        CloseAppWindow();
        return true;
    case IDM_CREATION_MODE_WINDOWED:
    case IDM_CREATION_MODE_VISUAL_DCOMP:
    case IDM_CREATION_MODE_TARGET_DCOMP:
#ifdef USE_WEBVIEW2_WIN10
//    case IDM_CREATION_MODE_VISUAL_WINCOMP:
#endif
        m_creationModeId = LOWORD(wParam);
        UpdateCreationModeMenu();
        return true;
    case IDM_REINIT:
        InitializeWebView();
        return true;
   
    case IDM_NEW_WINDOW:
        new AppWindow(m_creationModeId);
        return true;
    case IDM_NEW_THREAD:
        CreateNewThread(m_creationModeId);
        return true;
    case IDM_SET_LANGUAGE:
        ChangeLanguage();
        return true;
    case IDM_TOGGLE_AAD_SSO:
        ToggleAADSSO();
        return true;
    }
    return false;
}

// Prompt the user for a new language string
void AppWindow::ChangeLanguage()
{
    TextInputDialog dialog(GetMainWindow());
    if (dialog.confirmed)
    {
        m_language = (dialog.input);
    }
}

// Toggle AAD SSO enabled
void AppWindow::ToggleAADSSO()
{
    m_AADSSOEnabled = !m_AADSSOEnabled;
    MessageBox(
        nullptr,
        m_AADSSOEnabled ? L"AAD single sign on will be enabled for new WebView "
                          L"created after all webviews are closed." :
                          L"AAD single sign on will be disabled for new WebView"
                          L" created after all webviews are closed.",
        L"AAD SSO change",
        MB_OK);
}

// Message handler for about dialog.
INT_PTR CALLBACK AppWindow::About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

// Decide what to do when an accelerator key is pressed. Instead of immediately performing
// the action, we hand it to the caller so they can decide whether to run it right away
// or running it asynchronously. Will return nullptr if there is no action for the key.
std::function<void()> AppWindow::GetAcceleratorKeyFunction(UINT key)
{
    if (GetKeyState(VK_CONTROL) < 0)
    {
        switch (key)
        {
        case 'N':
            return [this] { new AppWindow(m_creationModeId); };
        case 'Q':
            return [this] { CloseAppWindow(); };
        case 'S':
            return [this] {
                if (auto file = GetComponent<KeyManager>())
                {
                    file->SaveScreenshot();
                }
            };
        case 'T':
            return [this] { CreateNewThread(m_creationModeId); };
        case 'W':
            return [this] { CloseWebView(); };
        }
    }
    return nullptr;
}

//! [CreateCoreWebView2Controller]
// Create or recreate the WebView and its environment.
void AppWindow::InitializeWebView()
{
    // To ensure browser switches get applied correctly, we need to close
    // the existing WebView. This will result in a new browser process
    // getting created which will apply the browser switches.
    CloseWebView();
    m_dcompDevice = nullptr;
#ifdef USE_WEBVIEW2_WIN10
    m_wincompCompositor = nullptr;
#endif
    LPCWSTR subFolder = nullptr;

    if (m_creationModeId == IDM_CREATION_MODE_VISUAL_DCOMP ||
        m_creationModeId == IDM_CREATION_MODE_TARGET_DCOMP)
    {
        HRESULT hr = DCompositionCreateDevice2(nullptr, IID_PPV_ARGS(&m_dcompDevice));
        if (!SUCCEEDED(hr))
        {
            MessageBox(
                m_mainWindow,
                L"Attempting to create WebView using DComp Visual is not supported.\r\n"
                "DComp device creation failed.\r\n"
                "Current OS may not support DComp.",
                L"Create with Windowless DComp Visual Failed", MB_OK);
            return;
        }
    }
#ifdef USE_WEBVIEW2_WIN10
/*
    else if (m_creationModeId == IDM_CREATION_MODE_VISUAL_WINCOMP)
    {
        HRESULT hr = TryCreateDispatcherQueue();
        if (!SUCCEEDED(hr))
        {
            MessageBox(
                m_mainWindow,
                L"Attempting to create WebView using WinComp Visual is not supported.\r\n"
                "WinComp compositor creation failed.\r\n"
                "Current OS may not support WinComp.",
                L"Create with Windowless WinComp Visual Failed", MB_OK);
            return;
        }
        m_wincompCompositor = winrtComp::Compositor();
    }
*/
#endif
    //! [CreateCoreWebView2EnvironmentWithOptions]
    auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    CHECK_FAILURE(options->put_AllowSingleSignOnUsingOSPrimaryAccount(
        m_AADSSOEnabled ? TRUE : FALSE));
    if (!m_language.empty())
        CHECK_FAILURE(options->put_Language(m_language.c_str()));
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        subFolder, nullptr, options.Get(),
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            this, &AppWindow::OnCreateEnvironmentCompleted)
            .Get());
    //! [CreateCoreWebView2EnvironmentWithOptions]
    if (!SUCCEEDED(hr))
    {
        if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
        {
            MessageBox(
                m_mainWindow,
                L"Couldn't find Edge installation. "
                "Do you have a version installed that's compatible with this "
                "WebView2 SDK version?",
                nullptr, MB_OK);
        }
        else
        {
            ShowFailure(hr, L"Failed to create webview environment");
        }
    }
}
// This is the callback passed to CreateWebViewEnvironmentWithOptions.
// Here we simply create the WebView.
HRESULT AppWindow::OnCreateEnvironmentCompleted(
    HRESULT result, ICoreWebView2Environment* environment)
{
    CHECK_FAILURE(result);
    m_webViewEnvironment = environment;

    auto webViewExperimentalEnvironment =
        m_webViewEnvironment.try_query<ICoreWebView2ExperimentalEnvironment>();
#ifdef USE_WEBVIEW2_WIN10
    if (webViewExperimentalEnvironment && (m_dcompDevice || m_wincompCompositor))
#else
    if (webViewExperimentalEnvironment && m_dcompDevice)
#endif
    {
        CHECK_FAILURE(webViewExperimentalEnvironment->CreateCoreWebView2CompositionController(
            m_mainWindow,
            Callback<
                ICoreWebView2ExperimentalCreateCoreWebView2CompositionControllerCompletedHandler>(
                [this](
                    HRESULT result,
                    ICoreWebView2ExperimentalCompositionController* compositionController) -> HRESULT {
                    auto controller =
                        wil::com_ptr<ICoreWebView2ExperimentalCompositionController>(compositionController)
                            .query<ICoreWebView2Controller>();
                    return OnCreateCoreWebView2ControllerCompleted(result, controller.get());
                })
                .Get()));
    }
    else
    {
        CHECK_FAILURE(m_webViewEnvironment->CreateCoreWebView2Controller(
            m_mainWindow, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                              this, &AppWindow::OnCreateCoreWebView2ControllerCompleted)
                              .Get()));
    }

    return S_OK;
}
//! [CreateCoreWebView2Controller]

// This is the callback passed to CreateCoreWebView2Controller. Here we initialize all WebView-related
// state and register most of our event handlers with the WebView.
HRESULT AppWindow::OnCreateCoreWebView2ControllerCompleted(HRESULT result, ICoreWebView2Controller* controller)
{
    if (result == S_OK)
    {
        m_controller = controller;
        wil::com_ptr<ICoreWebView2> coreWebView2;
        CHECK_FAILURE(m_controller->get_CoreWebView2(&coreWebView2));
        // We should check for failure here because if this app is using a newer
        // SDK version compared to the install of the Edge browser, the Edge
        // browser might not have support for the latest version of the
        // ICoreWebView2_N interface.
        coreWebView2.query_to(&m_webView);
        // Create components. These will be deleted when the WebView is closed.
        NewComponent<KeyManager>(this);
        NewComponent<Assets>(this);
        NewComponent<Mining>(this);
        NewComponent<Transaction>(this);
        NewComponent<StatusBar>(this);

//        NewComponent<SettingsComponent>(
//            this, m_webViewEnvironment.get(), m_oldSettingsComponent.get());
        m_oldSettingsComponent = nullptr;
        NewComponent<ViewComponent>(
            this, m_dcompDevice.get(),
#ifdef USE_WEBVIEW2_WIN10
            m_wincompCompositor,
#endif
            m_creationModeId == IDM_CREATION_MODE_TARGET_DCOMP);
//        NewComponent<ControlComponent>(this, &m_toolbar);

        // We have a few of our own event handlers to register here as well
        RegisterEventHandlers();

        // Set the initial size of the WebView
        ResizeEverything();

        if (m_onWebViewFirstInitialized)
        {
            m_onWebViewFirstInitialized();
            m_onWebViewFirstInitialized = nullptr;
        }
/*
        if (m_initialUri.empty())
        {
            // StartPage uses initialized values of the WebView and Environment
            // so we wait to call StartPage::GetUri until after the WebView is
            // created.
            m_initialUri = AppStartPage::GetUri(this);
        }
*/
        if (m_initialUri != L"none")
        {
            CHECK_FAILURE(m_webView->Navigate(m_initialUri.c_str()));
        }
    }
    else
    {
        ShowFailure(result, L"Failed to create webview");
    }
    return S_OK;
}
void AppWindow::ReinitializeWebView()
{
    // Save the settings component from being deleted when the WebView is closed, so we can
    // copy its properties to the next settings component.
    m_oldSettingsComponent = MoveComponent<SettingsComponent>();
    InitializeWebView();
}

void AppWindow::ReinitializeWebViewWithNewBrowser()
{
    // Save the settings component from being deleted when the WebView is closed, so we can
    // copy its properties to the next settings component.
    m_oldSettingsComponent = MoveComponent<SettingsComponent>();

    // Use the reference to the web view before we close it
    UINT webviewProcessId = 0;
    m_webView->get_BrowserProcessId(&webviewProcessId);

    // We need to close the current webviews and wait for the browser_process to exit
    // This is so the new webviews don't use the old browser exe
    CloseWebView();

    // Make sure the browser process inside webview is closed
//    Assets::EnsureProcessIsClosed(webviewProcessId, 2000);

    InitializeWebView();
}

void AppWindow::RestartApp()
{
    // Use the reference to the web view before we close the app window
    UINT webviewProcessId = 0;
    m_webView->get_BrowserProcessId(&webviewProcessId);

    // To restart the app completely, first we close the current App Window
    CloseAppWindow();

    // Make sure the browser process inside webview is closed
//    Assets::EnsureProcessIsClosed(webviewProcessId, 2000);

    // Get the command line arguments used to start this app
    // so we can re-create the process with them
    LPWSTR args = GetCommandLineW();

    STARTUPINFOW startup_info = {0};
    startup_info.cb = sizeof(startup_info);
    PROCESS_INFORMATION temp_process_info = {};
    // Start a new process
    if (!::CreateProcess(
            nullptr, args,
            nullptr, // default process attributes
            nullptr, // default thread attributes
            FALSE,   // do not inherit handles
            0,
            nullptr, // no environment
            nullptr, // default current directory
            &startup_info, &temp_process_info))
    {
        // Log some error information if desired
    }

    // Terminate this current process
    ::exit(0);
}

void AppWindow::RegisterEventHandlers()
{
    //! [ContainsFullScreenElementChanged]
    //! [NewWindowRequested]
    // Register a handler for the NewWindowRequested event.
    // This handler will defer the event, create a new app window, and then once the
    // new window is ready, it'll provide that new window's WebView as the response to
    // the request.
    CHECK_FAILURE(m_webView->add_NewWindowRequested(
        Callback<ICoreWebView2NewWindowRequestedEventHandler>(
            [this](ICoreWebView2* sender, ICoreWebView2NewWindowRequestedEventArgs* args) {
                wil::com_ptr<ICoreWebView2Deferral> deferral;
                CHECK_FAILURE(args->GetDeferral(&deferral));
                AppWindow* newAppWindow;

                wil::com_ptr<ICoreWebView2WindowFeatures> windowFeatures;
                CHECK_FAILURE(args->get_WindowFeatures(&windowFeatures));

                RECT windowRect = {0};
                UINT32 left = 0;
                UINT32 top = 0;
                UINT32 height = 0;
                UINT32 width = 0;
                BOOL shouldHaveToolbar = true;

                BOOL hasPosition = FALSE;
                BOOL hasSize = FALSE;
                CHECK_FAILURE(windowFeatures->get_HasPosition(&hasPosition));
                CHECK_FAILURE(windowFeatures->get_HasSize(&hasSize));

                bool useDefaultWindow = true;

                if (!!hasPosition && !!hasSize)
                {
                    CHECK_FAILURE(windowFeatures->get_Left(&left));
                    CHECK_FAILURE(windowFeatures->get_Top(&top));
                    CHECK_FAILURE(windowFeatures->get_Height(&height));
                    CHECK_FAILURE(windowFeatures->get_Width(&width));
                    useDefaultWindow = false;
                }
                CHECK_FAILURE(windowFeatures->get_ShouldDisplayToolbar(&shouldHaveToolbar));

                windowRect.left = left;
                windowRect.right = left + (width < s_minNewWindowSize ? s_minNewWindowSize : width);
                windowRect.top = top;
                windowRect.bottom = top + (height < s_minNewWindowSize ? s_minNewWindowSize : height);

                if (!useDefaultWindow)
                {
                  newAppWindow = new AppWindow(m_creationModeId, L"", false, nullptr, true, windowRect, !!shouldHaveToolbar);
                }
                else
                {
                  newAppWindow = new AppWindow(m_creationModeId, L"");
                }
                newAppWindow->m_isPopupWindow = true;
                newAppWindow->m_onWebViewFirstInitialized = [args, deferral, newAppWindow]() {
                    CHECK_FAILURE(args->put_NewWindow(newAppWindow->m_webView.get()));
                    CHECK_FAILURE(args->put_Handled(TRUE));
                    CHECK_FAILURE(deferral->Complete());
                };

                return S_OK;
            })
            .Get(),
        nullptr));
    //! [NewWindowRequested]

    //! [WindowCloseRequested]
    // Register a handler for the WindowCloseRequested event.
    // This handler will close the app window if it is not the main window.
    CHECK_FAILURE(m_webView->add_WindowCloseRequested(
        Callback<ICoreWebView2WindowCloseRequestedEventHandler>([this](
                                                                    ICoreWebView2* sender,
                                                                    IUnknown* args) {
            if (m_isPopupWindow)
            {
                CloseAppWindow();
            }
            return S_OK;
        }).Get(),
        nullptr));
    //! [WindowCloseRequested]

    //! [NewBrowserVersionAvailable]
    // After the environment is successfully created,
    // register a handler for the NewBrowserVersionAvailable event.
    // This handler tells when there is a new Edge version available on the machine.
    CHECK_FAILURE(m_webViewEnvironment->add_NewBrowserVersionAvailable(
        Callback<ICoreWebView2NewBrowserVersionAvailableEventHandler>(
            [this](ICoreWebView2Environment* sender, IUnknown* args) -> HRESULT {
                std::wstring message = L"We detected there is a new version for the browser.";
                if (m_webView)
                {
                    message += L"Do you want to restart the app? \n\n";
                    message += L"Click No if you only want to re-create the webviews. \n";
                    message += L"Click Cancel for no action. \n";
                }
                int response = MessageBox(
                    m_mainWindow, message.c_str(), L"New available version",
                    m_webView ? MB_YESNOCANCEL : MB_OK);

                if (response == IDYES)
                {
                    RestartApp();
                }
                else if (response == IDNO)
                {
                    ReinitializeWebViewWithNewBrowser();
                }
                else
                {
                    // do nothing
                }

                return S_OK;
            })
            .Get(),
        nullptr));
    //! [NewBrowserVersionAvailable]
    EventRegistrationToken m_webMessageReceivedToken = {};

    CHECK_FAILURE(m_webView->add_WebMessageReceived(
        Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [this](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args)
            {
                extern std::wstring initialUri;
                wil::unique_cotaskmem_string uri;
                CHECK_FAILURE(args->get_Source(&uri));

                wil::unique_cotaskmem_string messageRaw;
                CHECK_FAILURE(args->TryGetWebMessageAsString(&messageRaw));
                std::wstring message = messageRaw.get();

                if (message.compare(0, 13, L"ResetPassword") == 0) {
                    walletpass = L"";
                    return S_OK;
                }

                if (uri.get() != initialUri)
                {
                    return S_OK;
                }

                if (message.compare(0, 5, L"Ready") == 0) {
                    auto cstr = L"{\"Passwd\":\"" + towstr(cfg.omgdpass()) + L"\"}";
                    CHECK_FAILURE(m_webView->PostWebMessageAsJson(cstr.c_str()));
                }
                else if (message.compare(0, 11, L"ServerReady") == 0) {
                    serverReady = true;
                }
                return S_OK;
            }).Get(), &m_webMessageReceivedToken));
}

// Updates the sizing and positioning of everything in the window.
void AppWindow::ResizeEverything()
{
    RECT availableBounds = {0};
    GetClientRect(m_mainWindow, &availableBounds);

   
    if (m_toolbar.enabled()) availableBounds = m_toolbar.Resize(availableBounds);

//    const float dpiScale = GetDpiScale();
//    availableBounds.bottom -= 32 * dpiScale;

    availableBounds.bottom -= 32;       // room for status bar

    if (auto view = GetComponent<ViewComponent>())
    {
        view->SetBounds(availableBounds);
    }
}

//! [Close]
// Close the WebView and deinitialize related state. This doesn't close the app window.
void AppWindow::CloseWebView(bool cleanupUserDataFolder)
{
    DeleteAllComponents();
    if (m_controller)
    {
        m_controller->Close();
        m_controller = nullptr;
        m_webView = nullptr;
    }
    m_webViewEnvironment = nullptr;
    if (cleanupUserDataFolder)
    {
        // For non-UWP apps, the default user data folder {Executable File Name}.WebView2
        // is in the same directory next to the app executable. If end
        // developers specify userDataFolder during WebView environment
        // creation, they would need to pass in that explicit value here.
        // For more information about userDataFolder:
        // https://docs.microsoft.com/microsoft-edge/webview2/reference/win32/webview2-idl#createcorewebview2environmentwithoptions
        WCHAR userDataFolder[MAX_PATH] = L"";
        // Obtain the absolute path for relative paths that include "./" or "../"
        _wfullpath(
            userDataFolder, GetLocalPath(L".WebView2", true).c_str(), MAX_PATH);
        std::wstring userDataFolderPath(userDataFolder);

        std::wstring message = L"Are you sure you want to clean up the user data folder at\n";
        message += userDataFolderPath;
        message += L"\n?\nWarning: This action is not reversible.\n\n";
        message += L"Click No if there are other open WebView instances.\n";

        if (MessageBox(m_mainWindow, message.c_str(), L"Cleanup User Data Folder", MB_YESNO) ==
            IDYES)
        {
            CHECK_FAILURE(DeleteFileRecursive(userDataFolderPath));
        }
    }
}
//! [Close]

HRESULT AppWindow::DeleteFileRecursive(std::wstring path)
{
    wil::com_ptr<IFileOperation> fileOperation;
    CHECK_FAILURE(
        CoCreateInstance(CLSID_FileOperation, NULL, CLSCTX_ALL, IID_PPV_ARGS(&fileOperation)));

    // Turn off all UI from being shown to the user during the operation.
    CHECK_FAILURE(fileOperation->SetOperationFlags(FOF_NO_UI));

    wil::com_ptr<IShellItem> userDataFolder;
    CHECK_FAILURE(
        SHCreateItemFromParsingName(path.c_str(), NULL, IID_PPV_ARGS(&userDataFolder)));

    // Add the operation
    CHECK_FAILURE(fileOperation->DeleteItem(userDataFolder.get(), NULL));
    CHECK_FAILURE(userDataFolder->Release());

    // Perform the operation to delete the directory
    CHECK_FAILURE(fileOperation->PerformOperations());

    CHECK_FAILURE(fileOperation->Release());
    CoUninitialize();
    return S_OK;
}

void AppWindow::CloseAppWindow(){
    CloseWebView();
    DestroyWindow(m_mainWindow);
}

void AppWindow::DeleteComponent(ComponentBase* component)
{
    for (auto iter = m_components.begin(); iter != m_components.end(); iter++)
    {
        if (iter->get() == component)
        {
            m_components.erase(iter);
            return;
        }
    }
}

void AppWindow::DeleteAllComponents()
{
    // Delete components in reverse order of initialization.
    while (!m_components.empty())
    {
        m_components.pop_back();
    }
}

template <class ComponentType> std::unique_ptr<ComponentType> AppWindow::MoveComponent()
{
    for (auto iter = m_components.begin(); iter != m_components.end(); iter++)
    {
        if (dynamic_cast<ComponentType*>(iter->get()))
        {
            auto wanted = reinterpret_cast<std::unique_ptr<ComponentType>&&>(std::move(*iter));
            m_components.erase(iter);
            return std::move(wanted);
        }
    }
    return nullptr;
}

void AppWindow::SetTitleText(PCWSTR titleText)
{
    SetWindowText(m_mainWindow, titleText);
}

RECT AppWindow::GetWindowBounds()
{
    RECT hwndBounds = {0};
    GetClientRect(m_mainWindow, &hwndBounds);
    return hwndBounds;
}

std::wstring AppWindow::GetLocalPath(std::wstring relativePath, bool keep_exe_path)
{
    WCHAR rawPath[MAX_PATH];
    GetModuleFileNameW(g_hInstance, rawPath, MAX_PATH);
    std::wstring path(rawPath);
    if (keep_exe_path)
    {
        path.append(relativePath);
    }
    else
    {
        std::size_t index = path.find_last_of(L"\\") + 1;
        path.replace(index, path.length(), relativePath);
    }
    return path;
}
std::wstring AppWindow::GetLocalUri(std::wstring relativePath)
{
#if 0 // To be enabled after AddHostMappingForLocalFolder fully works.
    //! [LocalUrlUsage]
    const std::wstring localFileRootUrl = L"https://app-file.invalid/";
    return localFileRootUrl + regex_replace(relativePath, std::wregex(L"\\"), L"/");
    //! [LocalUrlUsage]
#else
    std::wstring path = GetLocalPath(relativePath, false);

    wil::com_ptr<IUri> uri;
    CHECK_FAILURE(CreateUri(path.c_str(), Uri_CREATE_ALLOW_IMPLICIT_FILE_SCHEME, 0, &uri));

    wil::unique_bstr uriBstr;
    CHECK_FAILURE(uri->GetAbsoluteUri(&uriBstr));
    return std::wstring(uriBstr.get());
#endif
}

void AppWindow::RunAsync(std::function<void()> callback)
{
    auto* task = new std::function<void()>(callback);
    PostMessage(m_mainWindow, s_runAsyncWindowMessage, reinterpret_cast<WPARAM>(task), 0);
}

// We have our own implementation of DCompositionCreateDevice2 that dynamically
// loads dcomp.dll to create the device. Not having a static dependency on dcomp.dll
// enables the sample app to run on versions of Windows that don't support dcomp.
HRESULT AppWindow::DCompositionCreateDevice2(IUnknown* renderingDevice, REFIID riid, void** ppv)
{
    HRESULT hr = E_FAIL;
    static decltype(::DCompositionCreateDevice2)* fnCreateDCompDevice2 = nullptr;
    if (fnCreateDCompDevice2 == nullptr)
    {
        HMODULE hmod = ::LoadLibraryEx(L"dcomp.dll", nullptr, 0);
        if (hmod != nullptr)
        {
            fnCreateDCompDevice2 = reinterpret_cast<decltype(::DCompositionCreateDevice2)*>(
                ::GetProcAddress(hmod, "DCompositionCreateDevice2"));
        }
    }
    if (fnCreateDCompDevice2 != nullptr)
    {
        hr = fnCreateDCompDevice2(renderingDevice, riid, ppv);
    }
    return hr;
}

// WinRT APIs cannot run without a DispatcherQueue. This helper function creates a
// DispatcherQueueController (which instantiates a DispatcherQueue under the covers) that will
// manage tasks for the WinRT APIs. The DispatcherQueue implementation lives in
// CoreMessaging.dll Similar to dcomp.dll, we load CoreMessaging.dll dynamically so the sample
// app can run on versions of windows that don't have CoreMessaging.
HRESULT AppWindow::TryCreateDispatcherQueue()
{
    namespace winSystem = winrt::Windows::System;

    HRESULT hr = S_OK;
    thread_local winSystem::DispatcherQueueController dispatcherQueueController{ nullptr };

    if (dispatcherQueueController == nullptr)
    {
        hr = E_FAIL;
        static decltype(::CreateDispatcherQueueController)* fnCreateDispatcherQueueController =
            nullptr;
        if (fnCreateDispatcherQueueController == nullptr)
        {
            HMODULE hmod = ::LoadLibraryEx(L"CoreMessaging.dll", nullptr, 0);
            if (hmod != nullptr)
            {
                fnCreateDispatcherQueueController =
                    reinterpret_cast<decltype(::CreateDispatcherQueueController)*>(
                        ::GetProcAddress(hmod, "CreateDispatcherQueueController"));
            }
        }
        if (fnCreateDispatcherQueueController != nullptr)
        {
            winSystem::DispatcherQueueController controller{ nullptr };
            DispatcherQueueOptions options
            {
                sizeof(DispatcherQueueOptions),
                DQTYPE_THREAD_CURRENT,
                DQTAT_COM_STA
            };
            hr = fnCreateDispatcherQueueController(
                options, reinterpret_cast<ABI::Windows::System::IDispatcherQueueController**>(
                             winrt::put_abi(controller)));
            dispatcherQueueController = controller;
        }
    }

    return hr;
}

#ifdef USE_WEBVIEW2_WIN10
//! [TextScaleChanged2]
void AppWindow::OnTextScaleChanged(
    winrt::Windows::UI::ViewManagement::UISettings const& settings,
    winrt::Windows::Foundation::IInspectable const& args)
{
    RunAsync([this] {
        m_toolbar.UpdateDpiAndTextScale();
    });
}
//! [TextScaleChanged2]
#endif
void AppWindow::UpdateCreationModeMenu()
{
    HMENU hMenu = GetMenu(m_mainWindow);
    CheckMenuRadioItem(
        hMenu,
        IDM_CREATION_MODE_WINDOWED,
//#ifdef USE_WEBVIEW2_WIN10
//        IDM_CREATION_MODE_VISUAL_WINCOMP,
//#else
        IDM_CREATION_MODE_TARGET_DCOMP,
//#endif
        m_creationModeId,
        MF_BYCOMMAND);
}

void AppWindow::UpdateKeyUI(int menuitem) {
     CheckMenuRadioItem(
        m_Popup,
        keyStartID,
        keyStartID + cfg.addresses() - 1,
        keyStartID + menuitem,
        MF_BYCOMMAND);
}

double AppWindow::GetDpiScale()
{
    return DpiUtil::GetDpiForWindow(m_mainWindow) * 1.0f / USER_DEFAULT_SCREEN_DPI;
}

#ifdef USE_WEBVIEW2_WIN10
double AppWindow::GetTextScale()
{
    return m_uiSettings ? m_uiSettings.TextScaleFactor() : 1.0f;
}
#endif

void AppWindow::AddRef()
{
    InterlockedIncrement((LONG *)&m_refCount);
}

void AppWindow::Release()
{
    uint32_t refCount = InterlockedDecrement((LONG *)&m_refCount);
    if (refCount == 0)
    {
        delete this;
    }
}

void AppWindow::NotifyClosed()
{
    m_isClosed = true;
}

void AppWindow::InstallComplete(int return_code)
{
    if (!m_isClosed)
    {
        if (return_code == 0)
        {
            RunAsync([this] {
                InitializeWebView();
                });
        }
        else if (return_code == 1)
        {
            MessageBox(m_mainWindow, L"WebView Runtime failed to Install", L"WebView Runtime Installation status", MB_OK);
        }
        else if (return_code == 2)
        {
            MessageBox(m_mainWindow, L"WebView Bootstrapper failled to download", L"WebView Bootstrapper Download status", MB_OK);
        }
    }
}
