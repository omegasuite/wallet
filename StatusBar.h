// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "stdafx.h"

#include "AppWindow.h"
#include "ComponentBase.h"

// This component handles commands from the File menu, except for Exit.
// It also handles the DocumentTitleChanged event.
class StatusBar : public ComponentBase
{
public:
    StatusBar(AppWindow* appWindow);

    bool HandleWindowMessage(
        HWND hWnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        LRESULT* result) override;

    ~StatusBar() override;

private:
    AppWindow* m_appWindow;
    wil::com_ptr<ICoreWebView2> m_webView;

    EventRegistrationToken m_webMessageReceivedToken = {};
    EventRegistrationToken m_contentLoadingToken = {};
};

