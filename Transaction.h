// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "stdafx.h"

#include "AppWindow.h"
#include "ComponentBase.h"
#include <vector>

typedef char * Input;
typedef struct {
    int tokentype;
    char* receipient;
    char* hash;
    int amount;
} Output;

// This component handles commands from the Process menu, as well as some miscellaneous
// functions for managing the browser process.
class Transaction : public ComponentBase
{
public:
    Transaction(AppWindow* appWindow);

    bool HandleWindowMessage(
        HWND hWnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        LRESULT* result) override;

    void ShowBrowserProcessInfo();
    void CrashBrowserProcess();
    void CrashRenderProcess();

    ~Transaction() override;

private:
    AppWindow* m_appWindow = nullptr;
    wil::com_ptr<ICoreWebView2> m_webView;
    std::wstring m_sampleUri;
    std::wstring m_tosend;
    std::vector<Input> m_inputs;
    std::vector<Output> m_outputs;
    EventRegistrationToken m_webMessageReceivedToken = {};
    EventRegistrationToken m_contentLoadingToken = {};

    std::wstring inputjstr();
    std::wstring outputjstr();
    std::wstring friendjstr();
};

