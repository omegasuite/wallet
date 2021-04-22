// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

#include "KeyManager.h"
#include "AppWindow.h"

#include "App.h"

#include "CheckFailure.h"
#include <commdlg.h>
#include <shlwapi.h>
#include "TextInputDialog.h"
#include "TextInputDialog2.h"

using namespace Microsoft::WRL;

char* tostr(const wchar_t* msg);
std::wstring towstr(const char* s);


KeyManager::KeyManager(AppWindow* appWindow)
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

                if (message.compare(0, 5, L"Ready") == 0) {
                    auto cstr = L"{\"Passwd\":\"" + towstr(cfg.omgdpass()) + L"\"" + to_send + L"}";
                    CHECK_FAILURE(m_webView->PostWebMessageAsJson(cstr.c_str()));
                    to_send = L"";
                }
                else if (message.compare(0, 10, L"LockPrompt") == 0) {
                    m_appWindow->Authorize(false);
                }
                else if (message.compare(0, 10, L"PassPrompt") == 0) {
                    TextInputDialog dialog(m_appWindow->GetMainWindow());
                    if (dialog.confirmed) {
                        auto old = dialog.input;

                        TextInputDialog2 dialog2(m_appWindow->GetMainWindow());
                        if (dialog2.confirmed) {
                            walletpass = dialog2.input;
                            std::wstring reply = L"{\"Password\":\"" + old + L"\",\"NewPassword\":\"" + walletpass + L"\",\"Passwd\":\"" + towstr(cfg.omgdpass()) + L"\"}";
                            wptime = time(NULL);

                            CHECK_FAILURE(m_webView->PostWebMessageAsJson(reply.c_str()));
                        }
                    }
                }
                else if (message.compare(0, 8, L"SetKeys ") == 0) {
                    auto CStr = tostr(message.substr(8).c_str());

                    auto at = cfg.addresses();

                    if (cfg.insert(CStr)) {
                        cfg.save();
                        m_appWindow->AddKey(CStr, at);
                    }
                    free(CStr);
                }
                else if (message.compare(0, 9, L"ListKeys ") == 0) {
                    auto CStr = tostr(message.substr(9).c_str());

                    cJSON* lst = cJSON_Parse(CStr);
                    
                    cfg.merge(lst);
                    free(CStr);
                }
                else if (message.compare(0, 8, L"GetKeys ") == 0) {
                    char* s = cfg.stringify();

                    size_t len = strlen(s) + 1;
                    size_t converted = 0;
                    wchar_t* WStr;
                    WStr = (wchar_t*)malloc(len * sizeof(wchar_t));
                    mbstowcs_s(&converted, WStr, len, s, _TRUNCATE);

                    CHECK_FAILURE(sender->PostWebMessageAsJson(WStr));
                    free(s); free(WStr);
                } else if (message.compare(0, 14, L"KeyRegistered ") == 0) {
                    auto CStr = tostr(message.substr(14).c_str());

                    bool r = cfg.registered(CStr);
                    if (r) cfg.save();
                    free(CStr);
                } else if (message.compare(0, 8, L"KeyUsed ") == 0) {
                    auto CStr = tostr(message.substr(8).c_str());

                    bool t = cfg.activate(CStr);
                    if (t) cfg.save();
 
                    m_appWindow->SetMiner(cfg.find(CStr));
                    free(CStr);
                }
                else if (message.compare(L"GetWindowBounds") == 0) {
                    RECT bounds = m_appWindow->GetWindowBounds();
                    std::wstring reply =
                        L"{\"WindowBounds\":\"Left:" + std::to_wstring(bounds.left)
                        + L"\\nTop:" + std::to_wstring(bounds.top)
                        + L"\\nRight:" + std::to_wstring(bounds.right)
                        + L"\\nBottom:" + std::to_wstring(bounds.bottom)
                        + L"\"}";
                    CHECK_FAILURE(sender->PostWebMessageAsJson(reply.c_str()));
                }
                return S_OK;
            }).Get(), &m_webMessageReceivedToken));
    //! [WebMessageReceived]
/*
    // Turn off this scenario if we navigate away from the sample page
    CHECK_FAILURE(m_webView->add_ContentLoading(
        Callback<ICoreWebView2ContentLoadingEventHandler>(
            [this](
                ICoreWebView2* sender,
                ICoreWebView2ContentLoadingEventArgs* args) -> HRESULT {
                    wil::unique_cotaskmem_string uri;
                    sender->get_Source(&uri);
                    if (uri.get() != m_sampleUri)
                    {
                        m_appWindow->DeleteComponent(this);
                    }
                    return S_OK;
            })
        .Get(), &m_contentLoadingToken));
*/
}

bool KeyManager::HandleWindowMessage(
    HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* result)
{
    TextInputDialog* dialog;

    if (message == WM_COMMAND) {
        // 500 is a constant determined according to resource.h such that ids from
        // it and a long span after it are not used, therefore the space can be
        // used for addresses. 100 should be enough for the space.
        if (LOWORD(wParam) >= keyStartID && LOWORD(wParam) < keyStartID + cfg.addresses()) {
            // select this key for mining
            auto currentMiningKey = LOWORD(wParam) - keyStartID;
            auto key = cfg.get(currentMiningKey);

            std::string reply = std::string(",\"MiningAddress\":\"") + std::string(key->address) + std::string("\"");
            to_send = L"";

            if (!key->registered) {
                m_sampleUri = L"file:///" + directory + L"/pages/choosekey.html";
                to_send = towstr(reply.c_str());
                to_send += L",\"Password\":\"" + m_appWindow->GetPassword(walletpass.compare(L"") != 0) + L"\"";
                CHECK_FAILURE(m_webView->Navigate(m_sampleUri.c_str()));
            } else if (!key->active) {
                m_sampleUri = L"file:///" + directory + L"/pages/selkey.html";
                to_send = towstr(reply.c_str());
                CHECK_FAILURE(m_webView->Navigate(m_sampleUri.c_str()));
            }
            else {
                m_appWindow->SetMiner(currentMiningKey);
            }
            return true;
        }

        switch (LOWORD(wParam))
        {
        case IDM_PASSWORD:
            if (!m_appWindow->Ready()) return true;

            m_sampleUri = L"file:///" + directory + L"/pages/newpass.html";
            to_send = L"";

            CHECK_FAILURE(m_webView->Navigate(m_sampleUri.c_str()));
            return true;

        case IDM_NEW_KEY:
            if (!m_appWindow->Ready()) return true;

            m_sampleUri = L"file:///" + directory + L"/genaddress/bitaddress.org.html";
            to_send = L"";

            // Changes to ICoreWebView2Settings::IsWebMessageEnabled apply to the next document
            // to which we navigate.
            CHECK_FAILURE(m_webView->Navigate(m_sampleUri.c_str()));

            return true;

        case IDM_IMPORT_KEY:
            if (!m_appWindow->Ready()) return true;

            m_sampleUri = L"file:///" + directory + L"/pages/import.html";

            to_send = L",\"Password\":\"" + m_appWindow->GetPassword(false) + L"\"";

            CHECK_FAILURE(m_webView->Navigate(m_sampleUri.c_str()));

            return true;

        case IDM_EXPORT_KEY:
            if (!m_appWindow->Ready()) return true;

            m_sampleUri = L"file:///" + directory + L"/pages/export.html";
            to_send = L",\"Password\":\"" + m_appWindow->GetPassword(false) + L"\"";
            CHECK_FAILURE(m_webView->Navigate(m_sampleUri.c_str()));

            return true;

        case IDM_KEY_LIST:
            if (!m_appWindow->Ready()) return true;

            to_send = L"";
            m_sampleUri = L"file:///" + directory + L"/pages/keylist.html";
            CHECK_FAILURE(m_webView->Navigate(m_sampleUri.c_str()));

            return true;
/*
        case ID_NEW_ADDRESS:
            m_sampleUri = L"file:///F:/Gopath/src/github.com/omegasuite/omcsm/WebView2APISample/pages/newaddr.html";
            CHECK_FAILURE(m_webView->Navigate(m_sampleUri.c_str()));

            return true;
*/
/*
        case IDM_SAVE_SCREENSHOT:
            SaveScreenshot();
            return true;

        case IDM_GET_DOCUMENT_TITLE:
        {
            wil::unique_cotaskmem_string title;
            m_webView->get_DocumentTitle(&title);
            MessageBox(m_appWindow->GetMainWindow(), title.get(), L"Document Title", MB_OK);
            return true;
        }
*/
        }
    }
    return false;
}

//! [CapturePreview]
// Show the user a file selection dialog, then save a screenshot of the WebView
// to the selected file.
void KeyManager::SaveScreenshot()
{
    OPENFILENAME openFileName = {};
    openFileName.lStructSize = sizeof(openFileName);
    openFileName.hwndOwner = nullptr;
    openFileName.hInstance = nullptr;
    WCHAR fileName[MAX_PATH] = L"WebView2_Screenshot.png";
    openFileName.lpstrFile = fileName;
    openFileName.lpstrFilter = L"PNG File\0*.png\0";
    openFileName.nMaxFile = ARRAYSIZE(fileName);
    openFileName.Flags = OFN_OVERWRITEPROMPT;

    if (GetSaveFileName(&openFileName))
    {
        wil::com_ptr<IStream> stream;
        CHECK_FAILURE(SHCreateStreamOnFileEx(
            fileName, STGM_READWRITE | STGM_CREATE, FILE_ATTRIBUTE_NORMAL, TRUE, nullptr,
            &stream));

        HWND mainWindow = m_appWindow->GetMainWindow();

        CHECK_FAILURE(m_webView->CapturePreview(
            COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_PNG, stream.get(),
            Callback<ICoreWebView2CapturePreviewCompletedHandler>(
                [mainWindow](HRESULT error_code) -> HRESULT {
                    CHECK_FAILURE(error_code);

                    MessageBox(mainWindow, L"Preview Captured", L"Preview Captured", MB_OK);
                    return S_OK;
                })
                .Get()));
    }
}
//! [CapturePreview]

KeyManager::~KeyManager()
{
    m_webView->remove_DocumentTitleChanged(m_documentTitleChangedToken);
}
