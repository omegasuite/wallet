// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

#include "App.h"
#include "Mining.h"

#include "CheckFailure.h"
#include "TextInputDialog.h"

using namespace Microsoft::WRL;
std::wstring towstr(const char* s);

Mining::Mining(AppWindow* appWindow)
    : m_appWindow(appWindow), m_webView(appWindow->GetWebView())
{
    m_sampleUri = L"file:///" + directory + L"/pages/mining.html";

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
                    auto cstr = L"{\"Passwd\":\"" + towstr(cfg.omgdpass()) + L"\"" + m_tosend + L"}";
                    CHECK_FAILURE(m_webView->PostWebMessageAsJson(cstr.c_str()));
                }
            }).Get(), &m_webMessageReceivedToken));
}

bool Mining::HandleWindowMessage(
    HWND hWnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    LRESULT* result)
{
    if (message == WM_COMMAND)
    {
        HMENU hMenu = GetMenu(m_appWindow->GetMainWindow());
        auto mining = ::GetSubMenu(hMenu, 1);

        switch (LOWORD(wParam))
        {
         case IDM_START_MINING:
             if (!m_appWindow->Ready()) return true;

             if (m_appWindow->MiningKey() >= 0) {
                 m_tosend = L",\"Mining\":\"true\"";
                 m_webView->Navigate(m_sampleUri.c_str());
                 miningStarted = true;

                 CheckMenuRadioItem(
                     mining,
                     IDM_START_MINING,
                     IDM_STOP_MINING,
                     IDM_START_MINING,
                     MF_BYCOMMAND);
             }
            return true;

        case IDM_STOP_MINING:
            if (!m_appWindow->Ready()) return true;

            m_tosend = L",\"Mining\":\"false\"";
            m_webView->Navigate(m_sampleUri.c_str());
            miningStarted = false;

            CheckMenuRadioItem(
                mining,
                IDM_START_MINING,
                IDM_STOP_MINING,
                IDM_STOP_MINING,
                MF_BYCOMMAND);
            return true;
/*
        case IDM_INJECT_SCRIPT:
            InjectScript();
            return true;
        case ID_ADD_INITIALIZE_SCRIPT:
            AddInitializeScript();
            return true;
        case ID_REMOVE_INITIALIZE_SCRIPT:
            RemoveInitializeScript();
            return true;
        case IDM_POST_WEB_MESSAGE_STRING:
            SendStringWebMessage();
            return true;
        case IDM_POST_WEB_MESSAGE_JSON:
            SendJsonWebMessage();
            return true;
        case IDM_SUBSCRIBE_TO_CDP_EVENT:
            SubscribeToCdpEvent();
            return true;
        case IDM_CALL_CDP_METHOD:
            CallCdpMethod();
            return true;
        case IDM_ADD_HOST_OBJECT:
            AddComObject();
            return true;
        case IDM_OPEN_DEVTOOLS_WINDOW:
            m_webView->OpenDevToolsWindow();
            return true;
*/
        }
    }
    return false;
}

Mining::~Mining()
{
}
