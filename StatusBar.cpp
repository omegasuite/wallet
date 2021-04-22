// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

#include "StatusBar.h"
#include "AppWindow.h"

#include "App.h"

#include "CheckFailure.h"
#include <commdlg.h>
#include <shlwapi.h>
#include "TextInputDialog.h"
#include "TextInputDialog2.h"
#include <commctrl.h>

using namespace Microsoft::WRL;

char* tostr(const wchar_t* msg);
std::wstring towstr(const char* s);

#define IDC_STATUSBAR 900
extern HWND hStatusbar;

StatusBar::StatusBar(AppWindow* appWindow)
    : m_appWindow(appWindow), m_webView(appWindow->GetWebView())
{
    if (!hStatusbar) {
        int array[] = { 120,120 * 2,-1 };
        HINSTANCE hInst = GetModuleHandle(NULL);
        //创建Statusbar控件
        // hWnd
        hStatusbar = CreateWindowEx(0, STATUSCLASSNAME, TEXT(""), WS_CHILD | WS_BORDER | WS_VISIBLE,
            0, 0, 0, 0, m_appWindow->GetMainWindow(), (HMENU)IDC_STATUSBAR, hInst, NULL);
        if (hStatusbar)
        {
            SendMessage(hStatusbar, SB_SETPARTS, (WPARAM)3, (LPARAM)array); //设置面板个数
            SendMessage(hStatusbar, SB_SETTEXT, (LPARAM)0, (WPARAM)TEXT("Omega Wallet")); //设置第二个面板内容
            SendMessage(hStatusbar, SB_SETTEXT, (LPARAM)1, (WPARAM)TEXT("Status")); //设置第二个面板内容
            SendMessage(hStatusbar, SB_SETTEXT, (LPARAM)2, (WPARAM)TEXT("Sync Progress")); //设置第三个面板内容
        }
    }

    CHECK_FAILURE(m_webView->add_WebMessageReceived(
        Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [this](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args)
            {
                wil::unique_cotaskmem_string uri;
                CHECK_FAILURE(args->get_Source(&uri));

                wil::unique_cotaskmem_string messageRaw;
                CHECK_FAILURE(args->TryGetWebMessageAsString(&messageRaw));
                std::wstring message = messageRaw.get();

                if (message.compare(0, 13, L"StatusUpdate ") == 0) {
                    auto s = tostr(message.substr(13).c_str());
                    unsigned int end;
                    sscanf(s, "%d", &end);
                    time_t start = 1593812175;    // genesis time
                    time_t now = time(NULL);
                    float prog = (end - start) * 1.0 / (now - start);
                    char buf[128];
                    sprintf(buf, " %.4f %%", prog * 100.0);
                    SendMessage(hStatusbar, SB_SETTEXT, (LPARAM)2, (WPARAM)(L"网络同步进度：" + towstr(buf)).c_str()); //设置第三个面板内容
                }
                return S_OK;
            }).Get(), &m_webMessageReceivedToken));
}

bool StatusBar::HandleWindowMessage(
    HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* result)
{
    return false;
}

StatusBar::~StatusBar()
{
}
