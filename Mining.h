// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "stdafx.h"

#include <map>
#include <string>

#include "AppWindow.h"
#include "ComponentBase.h"

// This component handles commands from the Script menu.
class Mining : public ComponentBase
{
public:
    Mining(AppWindow* appWindow);

    bool HandleWindowMessage(
        HWND hWnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        LRESULT* result) override;

    void InjectScript();
    void AddInitializeScript();
    void RemoveInitializeScript();
    void SendStringWebMessage();
    void SendJsonWebMessage();
    void SubscribeToCdpEvent();
    void CallCdpMethod();
    void AddComObject();

    ~Mining() override;

private:
    AppWindow* m_appWindow = nullptr;
    int m_currentMiningKey = -1;
    wil::com_ptr<ICoreWebView2> m_webView;
    std::wstring m_sampleUri;
    std::wstring m_tosend;
    EventRegistrationToken m_webMessageReceivedToken = {};
    EventRegistrationToken m_contentLoadingToken = {};

    std::wstring m_lastInitializeScriptId;
    std::map<std::wstring, EventRegistrationToken> m_devToolsProtocolEventReceivedTokenMap;
};

