// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

#include "TextInputDialog2.h"

#include "App.h"
#include "resource.h"

static INT_PTR CALLBACK DlgProcStatic(
    HWND hDlg,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    auto* self = (TextInputDialog2*)GetWindowLongPtr(hDlg, GWLP_USERDATA);

    switch (message)
    {
    case WM_INITDIALOG:
        self = (TextInputDialog2*)lParam;
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)self);

        SetDlgItemText(hDlg, IDC_EDIT_WARN, L"设置钱包密码。请输入钱包密码，\n然后再次输入确认。");
        SetDlgItemText(hDlg, IDC_EDIT_INPUT1, L"");
        SetDlgItemText(hDlg, IDC_EDIT_INPUT2, L"");

        RECT rect;
        GetWindowRect(hDlg, &rect);
        MoveWindow(hDlg, rect.left + 300, rect.top + 100,
            rect.right - rect.left, rect.bottom - rect.top, false);

        return (INT_PTR)TRUE;
    // TODO: don't close dialog if enter is pressed in edit control
    case WM_COMMAND:
        if (self) self->confirmed = false;

        if (self && LOWORD(wParam) == IDOK)
        {
            int length = GetWindowTextLength(GetDlgItem(hDlg, IDC_EDIT_INPUT1));
            self->input.resize(length);
            PWSTR data = const_cast<PWSTR>(self->input.data());
            GetDlgItemText(hDlg, IDC_EDIT_INPUT1, data, length + 1);

            length = GetWindowTextLength(GetDlgItem(hDlg, IDC_EDIT_INPUT2));
            self->input2.resize(length);
            data = const_cast<PWSTR>(self->input2.data());
            GetDlgItemText(hDlg, IDC_EDIT_INPUT2, data, length + 1);

            if (self->input.compare(self->input2)) {
                SetDlgItemTextW(hDlg, IDC_EDIT_WARN, L"二次输入的密码不一致，请重输。");
                return (INT_PTR)TRUE;
            }

            self->confirmed = true;
        }

        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            SetWindowLongPtr(hDlg, GWLP_USERDATA, NULL);
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    case WM_NCDESTROY:
        SetWindowLongPtr(hDlg, GWLP_USERDATA, NULL);
        return (INT_PTR)TRUE;
    }
    return (INT_PTR)FALSE;
}

TextInputDialog2::TextInputDialog2(HWND parent)
{
    confirmed = false;
    DialogBoxParam(
        g_hInstance, MAKEINTRESOURCE(IDD_DIALOG_INPUT2),
        parent, DlgProcStatic, (LPARAM)this);
}
