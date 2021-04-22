// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"
#include "App.h"
#include "Transaction.h"
#include "resource.h"
#include "CheckFailure.h"

using namespace Microsoft::WRL;
char* tostr(const wchar_t* msg);
std::wstring towstr(const char* s);

Transaction::Transaction(AppWindow* appWindow)
    : m_appWindow(appWindow), m_webView(appWindow->GetWebView())
{
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

                if (message.compare(0, 5, L"Ready") == 0 && m_tosend.compare(L"") != 0) {
                    auto c = m_tosend.c_str();
                    CHECK_FAILURE(m_webView->PostWebMessageAsJson(c));
                }
                else if (message.compare(0, 11, L"ClearInputs") == 0) {
                    m_inputs.clear();
                }
                else if (message.compare(0, 10, L"LockPrompt") == 0) {
                    m_appWindow->Authorize(false);
                }
                else if (message.compare(0, 10, L"AddInputs ") == 0) {
                    auto jsn = message.substr(10);
                    auto cj = cJSON_Parse(tostr(jsn.c_str()));

                    auto n = cJSON_GetArraySize(cj);
                    for (int i = 0; i < n; i++) {
                        cJSON* pl = cJSON_GetArrayItem(cj, i);
                        m_inputs.push_back(pl->valuestring);
                    }
                 }
                else if (message.compare(0, 10, L"AddFriend ") == 0) {
                    auto jsn = message.substr(10);
                    auto cj = cJSON_Parse(tostr(jsn.c_str()));
                    auto tt = cfg.friends();

                    cJSON_AddItemToArray(tt, cj);
                    cfg.save();
                }
                else if (message.compare(0, 11, L"AddOutputs ") == 0) {
                    auto jsn = message.substr(11);
                    auto pl = cJSON_Parse(tostr(jsn.c_str()));

                    Output p;
                    p.tokentype = cJSON_GetObjectItem(pl, "tokentype")->valueint;
                    p.receipient = cJSON_GetObjectItem(pl, "receipient")->valuestring;
                    if (p.tokentype & 1)
                        p.hash = cJSON_GetObjectItem(pl, "amount")->valuestring;
                    else p.amount = cJSON_GetObjectItem(pl, "amount")->valueint;
                    m_outputs.push_back(p);
                } else if (message.compare(0, 12, L"ClearOutputs") == 0) {
                    m_outputs.clear();
                }

           }).Get(), &m_webMessageReceivedToken));
}

bool Transaction::HandleWindowMessage(
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
        case IDM_SEND:
            if (!m_appWindow->Ready()) return true;

            m_sampleUri = L"file:///" + directory + L"/pages/send.html";
            m_tosend = L"{\"Myself\":" + towstr(cJSON_Print(cfg.myaddresses()))
                + L",\"Friends\":" + friendjstr() + L",\"Passwd\":\"" + towstr(cfg.omgdpass()) + L"\"}";
            CHECK_FAILURE(m_webView->Navigate(m_sampleUri.c_str()));

            return true;

        case IDM_INPUT:
            if (!m_appWindow->Ready()) return true;

            m_sampleUri = L"file:///" + directory + L"/pages/input.html";
            m_tosend = L"{\"Input\":" + inputjstr() + L",\"Passwd\":\"" + towstr(cfg.omgdpass()) + L"\"}";
            CHECK_FAILURE(m_webView->Navigate(m_sampleUri.c_str()));

            return true;

        case IDM_OUTPUT:
            m_sampleUri = L"file:///" + directory + L"/pages/output.html";
            m_tosend = L"{\"Outputs\":" + outputjstr() + L",\"Myself\":" + towstr(cJSON_Print(cfg.myaddresses()))
                + L",\"Friends\":" + friendjstr() + L",\"Passwd\":\"" + towstr(cfg.omgdpass()) + L"\"}";
            CHECK_FAILURE(m_webView->Navigate(m_sampleUri.c_str()));

            return true;

        case IDM_LAND:
            m_sampleUri = L"file:///" + directory + L"/pages/land.html";
            m_tosend = L"{\"Inputs\":" + inputjstr() + L",\"Friends\":" + friendjstr() + L",\"Passwd\":\"" + towstr(cfg.omgdpass()) + L"\"}";
            CHECK_FAILURE(m_webView->Navigate(m_sampleUri.c_str()));

            return true;

        case IDM_IMPORTRAWTX:
            m_sampleUri = L"file:///" + directory + L"/pages/txin.html";
            m_tosend = L"{\"Passwd\":\"" + towstr(cfg.omgdpass()) + L"\"}";
            CHECK_FAILURE(m_webView->Navigate(m_sampleUri.c_str()));

            return true;

        case IDM_EXPORTRAWTX:
            m_sampleUri = L"file:///" + directory + L"/pages/txout.html";
            m_tosend = L"{\"Input\":" + inputjstr() + L",\"Outputs\":" + outputjstr() + L",\"Passwd\":\"" + towstr(cfg.omgdpass()) + L"\"}";
            CHECK_FAILURE(m_webView->Navigate(m_sampleUri.c_str()));

            return true;

        case IDM_SIGNTX:
            if (!m_appWindow->Ready()) return true;

            m_sampleUri = L"file:///" + directory + L"/pages/signtx.html";
            m_tosend = L"{\"Input\":" + inputjstr() + L",\"Outputs\":" + outputjstr() + L",\"Passwd\":\"" + towstr(cfg.omgdpass()) + L"\"}";
            CHECK_FAILURE(m_webView->Navigate(m_sampleUri.c_str()));

            return true;
        }
    }
    return false;
}

std::wstring Transaction::inputjstr() {
    std::wstring s = L"[";
    std::wstring glue, ws;

    for (unsigned int i = 0; i < m_inputs.size(); i++) {
        s += glue + L"\"" + towstr(m_inputs[i]) + L"\"";
        glue = L",";
    }
    s += L"]";
    return s;
}

std::wstring Transaction::outputjstr() {
    std::wstring s = L"[";
    std::wstring glue;
    char tmp[20];
    for (int i = 0; i < m_outputs.size(); i++) {
        itoa(m_outputs[i].tokentype, tmp, 10);
        s += glue + L"{\"tokentype\":" + towstr(tmp) + L",\"receipient\":\"";
        s += towstr(m_outputs[i].receipient) + L"\",\"amount\":";
        if ((m_outputs[i].tokentype & 1) == 0) {
            itoa(m_outputs[i].amount, tmp, 10);
            s += towstr(tmp) + L"}";
        }
        else {
            s += L"\"" + towstr(m_outputs[i].hash) + L"\"}";
        }

        glue = L",";
    }
    s += L"]";
    return s;
}

std::wstring towstr(const char* s);

std::wstring Transaction::friendjstr() {
    return towstr(cJSON_Print(cfg.friends()));
}

Transaction::~Transaction()
{
}
