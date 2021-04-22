// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"
#include "App.h"
#include "Assets.h"
#include "CheckFailure.h"

using namespace Microsoft::WRL;
std::wstring towstr(const char* s);

Assets::Assets(AppWindow* appWindow)
    : m_appWindow(appWindow), m_webView(appWindow->GetWebView())
{
    m_sampleUri = L"file:///" + directory + L"/pages/assets.html";

    CHECK_FAILURE(m_webView->add_WebMessageReceived(
        Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [this](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args)
            {
                wil::unique_cotaskmem_string uri;
                CHECK_FAILURE(args->get_Source(&uri));

                // Always validate that the origin of the message is what you expect.
                if (uri.get() != m_sampleUri)
                {
                    return S_OK;
                }
                wil::unique_cotaskmem_string messageRaw;
                CHECK_FAILURE(args->TryGetWebMessageAsString(&messageRaw));
                std::wstring message = messageRaw.get();

                if (message.compare(0, 5, L"Ready") == 0) {
                    auto cstr = L"{\"Type\":\"" + m_tosend + L"\",\"Passwd\":\"" + towstr(cfg.omgdpass()) + L"\"}";
                    CHECK_FAILURE(m_webView->PostWebMessageAsJson(cstr.c_str()));
                }
                else if (message.compare(0, 10, L"LockPrompt") == 0) {
                    m_appWindow->Authorize(false);
                }
        }).Get(), &m_webMessageReceivedToken));
}

bool Assets::HandleWindowMessage(
    HWND hWnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    LRESULT* result)
{
    if (message == WM_COMMAND)
    {
        switch (LOWORD(wParam))
        {
        case IDM_TOTAL_ASSET:
            if (!m_appWindow->Ready()) return true;

            m_tosend = L"false";
            CHECK_FAILURE(m_webView->Navigate(m_sampleUri.c_str()));

            return true;
        case IDM_ASSET_DETAIL:
            if (!m_appWindow->Ready()) return true;

            m_tosend = L"true";
            CHECK_FAILURE(m_webView->Navigate(m_sampleUri.c_str()));

            return true;
        }
    }
    return false;
}

Assets::~Assets()
{
}
