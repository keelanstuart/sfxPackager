/*
	Copyright © 2013-2020, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.

	For inquiries, contact: keelanstuart@gmail.com
*/

#pragma once


#include "HtmlCtrl.h"

// CLicenseKeyEntryDlg dialog

class CLicenseKeyEntryDlg : public CDialog
{
	DECLARE_DYNAMIC(CLicenseKeyEntryDlg)

public:
	CLicenseKeyEntryDlg(const TCHAR *description, CWnd *pParent = nullptr);   // standard constructor
	virtual ~CLicenseKeyEntryDlg();

// Dialog Data
	enum { IDD = IDD_LICENSEKEY_DIALOG };

	const TCHAR *GetKey();

protected:
	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support

	HICON m_hIcon;
	CHtmlCtrl m_CtlDesc;
	CEdit m_CtlKey;

	CString m_Desc;
	CString m_Key;


	DECLARE_MESSAGE_MAP()
	virtual void OnOK();
	virtual void OnCancel();
public:
	virtual BOOL OnInitDialog();
};
