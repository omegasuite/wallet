// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

#include "App.h"

#include <map>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <shobjidl.h>
#include <string.h>
#include <vector>
#include <process.h>
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <iostream>

#include "AppWindow.h"
#include "DpiUtil.h"

HINSTANCE g_hInstance;
int g_nCmdShow;
bool g_autoTabHandle = true;
Config cfg;
time_t locktime;
std::wstring walletpass;
time_t wptime;
std::wstring directory;

bool miningStarted = false;
static std::map<DWORD, HANDLE> s_threads;

static int RunMessagePump();
static DWORD WINAPI ThreadProc(void* pvParam);
static void WaitForOtherThreads();
char* tostr(const wchar_t* msg);

#define NEXT_PARAM_CONTAINS(command) \
    _wcsnicmp(nextParam.c_str(), command, ARRAYSIZE(command) - 1) == 0

std::wstring ExePath() {
    TCHAR buffer[MAX_PATH] = { 0 };

    GetModuleFileName(NULL, buffer, MAX_PATH);

    for (int i = 0; buffer[i] != 0; i++) {
        if (buffer[i] == '\\') buffer[i] = '/';
    }

    std::wstring::size_type pos = std::wstring(buffer).find_last_of(L"/");

    // debugging only. to be removed in release version
//    pos = std::wstring(std::wstring(buffer).substr(0, pos)).find_last_of(L"/");
//    pos = std::wstring(std::wstring(buffer).substr(0, pos)).find_last_of(L"/");

    return std::wstring(buffer).substr(0, pos);
}

std::wstring initialUri;
AppWindow* appwin;

int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      PWSTR lpCmdLine,
                      int nCmdShow)
{
    g_hInstance = hInstance;
    UNREFERENCED_PARAMETER(hPrevInstance);
    g_nCmdShow = nCmdShow;

    // Default DPI awareness to PerMonitorV2. The commandline parameters can
    // override this.
    DPI_AWARENESS_CONTEXT dpiAwarenessContext =
        DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2;
    std::wstring appId(L"OmegaWallet"); //  EBWebView.SampleApp");    //  OmegaWallet");
    
    DWORD creationModeId = IDM_CREATION_MODE_WINDOWED;

    directory = ExePath();
    initialUri = L"file:///" + directory + L"/pages/home.html";

    if (lpCmdLine && lpCmdLine[0])
    {
        int paramCount = 0;
        LPWSTR* params = CommandLineToArgvW(lpCmdLine, &paramCount);
        for (int i = 0; i < paramCount; ++i)
        {
            std::wstring nextParam;
            if (params[i][0] == L'-')
            {
                if (params[i][1] == L'-')
                {
                    nextParam.assign(params[i] + 2);
                }
                else
                {
                    nextParam.assign(params[i] + 1);
                }
            }
            if (NEXT_PARAM_CONTAINS(L"dpiunaware"))
            {
                dpiAwarenessContext = DPI_AWARENESS_CONTEXT_UNAWARE;
            }
            else if (NEXT_PARAM_CONTAINS(L"dpisystemaware"))
            {
                dpiAwarenessContext = DPI_AWARENESS_CONTEXT_SYSTEM_AWARE;
            }
            else if (NEXT_PARAM_CONTAINS(L"dpipermonitorawarev2"))
            {
                dpiAwarenessContext = DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2;
            }
            else if (NEXT_PARAM_CONTAINS(L"dpipermonitoraware"))
            {
                dpiAwarenessContext = DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE;
            }
            else if (NEXT_PARAM_CONTAINS(L"noinitialnavigation"))
            {
                initialUri = L"none";
            }
            else if (NEXT_PARAM_CONTAINS(L"appid="))
            {
                appId = nextParam.substr(nextParam.find(L'=') + 1);
            }
            else if (NEXT_PARAM_CONTAINS(L"initialUri="))
            {
                initialUri = nextParam.substr(nextParam.find(L'=') + 1);
            }
            else if (NEXT_PARAM_CONTAINS(L"creationmode="))
            {
                nextParam = nextParam.substr(nextParam.find(L'=') + 1);
                if (NEXT_PARAM_CONTAINS(L"windowed"))
                {
                    creationModeId = IDM_CREATION_MODE_WINDOWED;
                }
                else if (NEXT_PARAM_CONTAINS(L"visualdcomp"))
                {
                    creationModeId = IDM_CREATION_MODE_VISUAL_DCOMP;
                }
                else if (NEXT_PARAM_CONTAINS(L"targetdcomp"))
                {
                    creationModeId = IDM_CREATION_MODE_TARGET_DCOMP;
                }
#ifdef USE_WEBVIEW2_WIN10
/*
                else if (NEXT_PARAM_CONTAINS(L"visualwincomp"))
                {
                    creationModeId = IDM_CREATION_MODE_VISUAL_WINCOMP;
                }
*/
#endif
            }
        }
        LocalFree(params);
    }

    cfg.load();

    AllocConsole();

    SetCurrentProcessExplicitAppUserModelID(appId.c_str());

    DpiUtil::SetProcessDpiAwarenessContext(dpiAwarenessContext);

    appwin = new AppWindow(creationModeId, initialUri, true);

    appwin->OmegaSart();
    ShowWindow(GetConsoleWindow(), SW_HIDE);

//    backgroundProc(TEXT("omgd.exe /configfile btcd.conf"), "input.txt", &omgd);
    int retVal = RunMessagePump();

    WaitForOtherThreads();

    cfg.save();

    ShowWindow(GetConsoleWindow(), SW_SHOW);

    SetConsoleCtrlHandler(NULL, TRUE); // disable Control+C handling for our app
    GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);

    Sleep(2000);

    appwin->OmegaClose();

    return retVal;
}

// Run the message pump for one thread.
static int RunMessagePump()
{
    HACCEL hAccelTable =
        LoadAccelerators(g_hInstance, MAKEINTRESOURCE(IDC_WEBVIEW2APISAMPLE));

    MSG msg;

    // Main message loop:
    //! [MoveFocus0]
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            // Calling IsDialogMessage handles Tab traversal automatically. If the
            // app wants the platform to auto handle tab, then call IsDialogMessage
            // before calling TranslateMessage/DispatchMessage. If the app wants to
            // handle tabbing itself, then skip calling IsDialogMessage and call
            // TranslateMessage/DispatchMessage directly.
            if (!g_autoTabHandle || !IsDialogMessage(GetAncestor(msg.hwnd, GA_ROOT), &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }
    //! [MoveFocus0]

    DWORD threadId = GetCurrentThreadId();
    auto it = s_threads.find(threadId);
    if (it != s_threads.end())
    {
        CloseHandle(it->second);
        s_threads.erase(threadId);
    }

    return (int)msg.wParam;
}

// Make a new thread.
void CreateNewThread(UINT creationModeId)
{
    DWORD threadId;
    HANDLE thread = CreateThread(
        nullptr, 0, ThreadProc, reinterpret_cast<LPVOID>(creationModeId),
        STACK_SIZE_PARAM_IS_A_RESERVATION, &threadId);
    s_threads.insert(std::pair<DWORD, HANDLE>(threadId, thread));
}

// This function is the starting point for new threads. It will open a new app window.
static DWORD WINAPI ThreadProc(void* pvParam)
{
    new AppWindow(reinterpret_cast<UINT>(pvParam));
    return RunMessagePump();
}

// Called on the main thread.  Wait for all other threads to complete before exiting.
static void WaitForOtherThreads()
{
    while (!s_threads.empty())
    {
        std::vector<HANDLE> threadHandles;
        for (auto it = s_threads.begin(); it != s_threads.end(); ++it)
        {
            threadHandles.push_back(it->second);
        }

        HANDLE* handleArray = threadHandles.data();
        DWORD dwIndex = MsgWaitForMultipleObjects(
            static_cast<DWORD>(threadHandles.size()), threadHandles.data(), FALSE,
            INFINITE, QS_ALLEVENTS);

        if (dwIndex == WAIT_OBJECT_0 + threadHandles.size())
        {
            MSG msg;
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }
}

void Config::save() {
    if (json == NULL) return;

    auto dir = directory + L"/omega.conf";
    FILE* fp = fopen(tostr(dir.c_str()), "wb");

    char* s = cJSON_Print(json);

    fputs(s, fp);
    fclose(fp);
    free(s);

    saveCollaterals();
}

bool Config::registered(const char* s) {
    for (int i = 0; i < nAddress; i++) {
        if (!strcmp(s, myaddress[i]->address)) {
            myaddress[i]->registered = true;
            auto tt = cJSON_GetObjectItem(cJSON_GetArrayItem(cJSON_GetObjectItem(json, "addresses"), i), "registered");
            tt->type = 1;
            return true;
        }
    }
    return false;
}

bool Config::activate(const char* s) {
    for (int i = 0; i < nAddress; i++) {
        if (!strcmp(s, myaddress[i]->address)) {
            if (myaddress[i]->active) return false;
            for (int j = 0; j < nAddress; j++) {
                if (myaddress[j]->active) {
                    myaddress[j]->active = false;
                }
            }
            myaddress[i]->active = true;
            return true;
        }
    }
    return false;
}

int Config::insertColl(const char * s) {
    for (int i = 0; i < nCollateral; i++) {
        if (!colllist[i]) {
            colllist[i] = (char*)malloc(strlen(s) + 1);
            strcpy(colllist[i], s);
            return i;
        }
        if (!strcmp(s, colllist[i])) return -1;
    }

    if (collallocated <= nCollateral) {
        char ** t;
        if (nCollateral != 0) {
            t = new char * [2 * nCollateral];
            collallocated = 2 * nCollateral;
            memcpy(t, colllist, nCollateral * sizeof(char*));
            delete[]colllist;
        }
        else {
            t = new char * [2];
            collallocated = 2;
            nCollateral = 0;
        }
        colllist = t;
    }
    colllist[nCollateral] = (char*)malloc(strlen(s) + 1);
    strcpy(colllist[nCollateral], s);

    nCollateral++;
    return nCollateral - 1;
}

bool Config::insert(const char* s) {
    cJSON* tt;
    if (json == NULL) {
        json = cJSON_CreateObject();
        tt = cJSON_CreateArray();
        cJSON_AddItemToObject(json, "addresses", tt);
    } else {
        tt = cJSON_GetObjectItem(json, "addresses");
    }

    for (int i = 0; i < nAddress; i++)
        if (!strcmp(s, myaddress[i]->address)) return false;

    if (allocated <= nAddress) {
        addressState** t;
        if (nAddress != 0) {
            t = new addressState * [2 * nAddress];
            allocated = 2 * nAddress;
            memcpy(t, myaddress, nAddress * sizeof(addressState*));
            delete[]myaddress;
        }
        else {
            t = new addressState * [2];
            allocated = 2;
        }
        myaddress = t;
    }
    myaddress[nAddress] = new addressState;
    myaddress[nAddress]->address = (char*) malloc(strlen(s) + 1);
    strcpy(myaddress[nAddress]->address, s);
    myaddress[nAddress]->active = false;
    myaddress[nAddress]->registered = false;
    appwin->AddKey(s, nAddress);

    cJSON* t = cJSON_CreateObject();
    cJSON_AddItemToObject(t, "address", cJSON_CreateString(s));
    cJSON_AddItemToObject(t, "registered", cJSON_CreateBool(0));
    cJSON_AddItemToArray(tt, t);

    nAddress++;
    return true;
}

bool Config::merge(cJSON* s) {
    auto n = cJSON_GetArraySize(s);
    for (int i = 0; i < n; i++) {
        cJSON* pl = cJSON_GetArrayItem(s, i);
        insert(pl->valuestring);
    }
    return true;
}

void Config::load() {
    // load config
    auto dir = directory + L"/omega.conf";
    FILE* fp = fopen(tostr(dir.c_str()), "rb");

    if (fp == NULL) return;

    struct stat stbuf;
    cJSON* cj;

    int fd = fileno(fp);
    fstat(fd, &stbuf);
    int size = stbuf.st_size; //get file size (byte)

    char* buf = (char*)malloc(size + 1);
    int loc = 0;

    while (!feof(fp)) {
        loc += strlen(fgets(buf + loc, size, fp));
        buf[loc] = '\0';
    }
    fclose(fp);

    cj = cJSON_Parse(buf);
    free(buf);

    if (cj == NULL) return;

    auto tt = cJSON_GetObjectItem(cj, "addresses");
    if (tt) {
        allocated = nAddress = cJSON_GetArraySize(tt);
        myaddress = new addressState*[nAddress];
        for (int i = 0; i < nAddress; i++) {
            cJSON* pl = cJSON_GetArrayItem(tt, i);
            myaddress[i] = new addressState;
            myaddress[i]->address = cJSON_GetObjectItem(pl, "address")->valuestring;
            myaddress[i]->registered = cJSON_GetObjectItem(pl, "registered")->valueint != 0;
            myaddress[i]->active = false;
        }
    }
    json = cj;

    loadCollaterals();
}

void Config::setomgdpass(const char* s) {
    if (json == NULL) {
        json = cJSON_CreateObject();
        auto tt = cJSON_CreateArray();
        cJSON_AddItemToObject(json, "addresses", tt);
    }

    auto tt = cJSON_GetObjectItem(json, "omgdpass");
    if (tt == NULL) {
        tt = cJSON_CreateString(s);
        cJSON_AddItemToObject(json, "omgdpass", tt);
    }
    else {
        tt = cJSON_CreateString(s);
        cJSON_ReplaceItemInObject(json, "omgdpass", tt);
    }
}

const char* Config::omgdpass() {
    if (json == NULL) {
        json = cJSON_CreateObject();
        auto tt = cJSON_CreateArray();
        cJSON_AddItemToObject(json, "addresses", tt);
    }
    auto tt = cJSON_GetObjectItem(json, "omgdpass");
    if (tt) return tt->valuestring;
    return NULL;
}

void Config::loadCollaterals() {
    FILE* fp = fopen(tostr((directory + L"/btcd.conf").c_str()), "rb");

    char buf[2048];
    int pos = 0, begin = 0;
    while (fp && !feof(fp)) {
        auto sln = fgets(buf + pos, 1024, fp);
        if (sln == NULL) continue;
        pos += strlen(sln);
        int j;
        for (j = begin; j < pos && buf[j] != '\n'; j++);
        if (buf[j] != '\n') continue;

        buf[j] = '\0';

        if (!strncmp("Collateral=", buf + begin, 11)) {
            insertColl(buf + begin + 11);
        }

        if (j < pos - 1) begin = j + 1;
        else begin = pos = 0;
        if (begin >= 1024) {
            memcpy(buf, buf + begin, pos - begin);
            pos -= begin;
            begin = 0;
        }
    }
    if (fp) fclose(fp);
}

void Config::saveCollaterals() {
    FILE* fp = fopen(tostr((directory + L"/btcd.conf").c_str()), "rb");
    FILE* fw = fopen(tostr((directory + L"/btcd.conf-tmp").c_str()), "wb");
    char buf[2048];
    int pos = 0, begin = 0;
    while (fp && !feof(fp)) {
        auto sln = fgets(buf + pos, 1024, fp);
        if (sln == NULL) continue;
        pos += strlen(sln);
        int j;
        for (j = begin; j < pos && buf[j] != '\n'; j++);
        if (buf[j] != '\n') continue;

        if (strncmp("Collateral=", buf + begin, 11)) {
            fwrite(buf + begin, 1, j - begin + 1, fw);
        }           

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

        if (strncmp("Collateral=", buf + begin, 11))
            fwrite(buf + begin, 1, j - begin + 1, fw);
        begin = j + 1;
    }

    for (int i = 0; i < nCollateral; i++) {
        if (!colllist[i]) continue;
        fwrite("Collateral=", 1, 11, fw);
        fwrite(colllist[i], 1, strlen(colllist[i]), fw);
        fwrite("\n", 1, 1, fw);
    }

    if (fp) fclose(fp);
    fclose(fw);

    unlink(tostr((directory + L"/btcd.conf").c_str()));
    rename(tostr((directory + L"/btcd.conf-tmp").c_str()), tostr((directory + L"/btcd.conf").c_str()));
}

char* tostr(const wchar_t* msg) {
    size_t len = wcslen(msg) + 1;
    size_t converted = 0;
    char* CStr;
    CStr = (char*)malloc(len * sizeof(char));
    wcstombs_s(&converted, CStr, len, msg, _TRUNCATE);
    return CStr;
}

std::wstring towstr(const char * s) {
    size_t len = strlen(s) + 1;
    size_t converted = 0;
    wchar_t* WStr;
    WStr = (wchar_t*)malloc(len * sizeof(wchar_t));
    mbstowcs_s(&converted, WStr, len, s, _TRUNCATE);

    return WStr;
}