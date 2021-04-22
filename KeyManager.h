// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "stdafx.h"

#include "AppWindow.h"
#include "ComponentBase.h"

// This component handles commands from the File menu, except for Exit.
// It also handles the DocumentTitleChanged event.
class KeyManager : public ComponentBase
{
public:
    KeyManager(AppWindow* appWindow);

    bool HandleWindowMessage(
        HWND hWnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        LRESULT* result) override;

    void SaveScreenshot();

    ~KeyManager() override;

private:
    AppWindow* m_appWindow;
    wil::com_ptr<ICoreWebView2> m_webView;

    EventRegistrationToken m_documentTitleChangedToken = {};

    std::wstring m_sampleUri;
    std::wstring to_send;
    EventRegistrationToken m_webMessageReceivedToken = {};
    EventRegistrationToken m_contentLoadingToken = {};
};

