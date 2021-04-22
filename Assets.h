// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "stdafx.h"

#include "AppWindow.h"
#include "ComponentBase.h"

// This component handles commands from the Process menu, as well as some miscellaneous
// functions for managing the browser process.
class Assets : public ComponentBase
{
public:
    Assets(AppWindow* appWindow);

    bool HandleWindowMessage(
        HWND hWnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        LRESULT* result) override;

    void ShowBrowserProcessInfo();
    void CrashBrowserProcess();
    void CrashRenderProcess();

    ~Assets() override;

private:
    AppWindow* m_appWindow = nullptr;
    wil::com_ptr<ICoreWebView2> m_webView;

    std::wstring m_sampleUri;
    std::wstring m_tosend;
    EventRegistrationToken m_webMessageReceivedToken = {};
    EventRegistrationToken m_contentLoadingToken = {};
};

