/*
	Copyright © 2013-2025, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
	
	For inquiries, contact: keelanstuart@gmail.com
*/

#pragma once


// CFinishDlg dialog

class CFinishDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CFinishDlg)

public:
	CFinishDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CFinishDlg();

// Dialog Data
	enum { IDD = IDD_FINISH_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	HICON m_hIcon;

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
};
