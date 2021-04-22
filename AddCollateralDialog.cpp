// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

#include "AddCollateralDialog.h"

#include "App.h"
#include "resource.h"

static INT_PTR CALLBACK DlgProcStatic(
    HWND hDlg,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    auto* self = (AddCollateralDialog*)GetWindowLongPtr(hDlg, GWLP_USERDATA);

    switch (message)
    {
    case WM_INITDIALOG:
        self = (AddCollateralDialog*)lParam;
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)self);

        SetDlgItemText(hDlg, IDC_COLLWARN, L"请输入质押币的UTXO哈希和序号，以:分开。");
        SetDlgItemText(hDlg, IDC_COLLWARN2, L"增、删在下次启动钱包后生效。");
        SetDlgItemText(hDlg, IDC_NEWCOLL, self->input.data());

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
            int length = GetWindowTextLength(GetDlgItem(hDlg, IDC_NEWCOLL));
            self->input.resize(length);
            PWSTR data = const_cast<PWSTR>(self->input.data());
            GetDlgItemText(hDlg, IDC_NEWCOLL, data, length + 1);
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

AddCollateralDialog::AddCollateralDialog(HWND parent)
{
    confirmed = false;
    DialogBoxParam(
        g_hInstance, MAKEINTRESOURCE(IDD_DIALOG_NEWCOLL),
        parent, DlgProcStatic, (LPARAM)this);
}
