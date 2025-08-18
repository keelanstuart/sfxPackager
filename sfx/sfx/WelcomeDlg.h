/*
	Copyright © 2013-2025, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
	
	For inquiries, contact: keelanstuart@gmail.com
*/

#include "HtmlCtrl.h"

#pragma once


// CWelcomeDlg dialog

class CWelcomeDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CWelcomeDlg)

public:
	CWelcomeDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CWelcomeDlg();

// Dialog Data
	enum { IDD = IDD_WELCOME_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	HICON m_hIcon;
	CHtmlCtrl m_Desc;

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
};
