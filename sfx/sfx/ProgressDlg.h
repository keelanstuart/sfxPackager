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


// CProgressDlg dialog

class CProgressDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CProgressDlg)

public:
	CProgressDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CProgressDlg();

	void Echo(const TCHAR *msg);

// Dialog Data
	enum { IDD = IDD_PROGRESS_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DWORD RunInstall();
	static UINT AFX_CDECL InstallThreadProc(LPVOID param);

	HANDLE m_CancelEvent;
	HANDLE m_Thread;

	CProgressCtrl m_Progress;
	CProgressCtrl m_IndividualProgress;
	CEdit m_Status;
	CFont m_Font;

	HICON m_hIcon;
	HANDLE m_mutexInstallStart;

	POINT m_ImgSize;

	static void __cdecl DownloadCallback(uint64_t bytes_received, uint64_t bytes_expected, void *userdata);

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	virtual void OnCancel();
	virtual void OnOK();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnSize(UINT nType, int cx, int cy);

};
