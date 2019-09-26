/*
	Copyright ©2016. Authored by Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
	
	For inquiries, contact: keelanstuart@gmail.com
*/


// sfxDlg.cpp : implementation file
//

#include "stdafx.h"
#include "sfx.h"
#include "sfxDlg.h"
#include "afxdialogex.h"
#include "../sfxFlags.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CSfxDlg dialog




CSfxDlg::CSfxDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CSfxDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(_T("ICON"));
}

void CSfxDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CSfxDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDOK, &CSfxDlg::OnBnClickedOk)
END_MESSAGE_MAP()


// CSfxDlg message handlers

BOOL CSfxDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	SetWindowText(theApp.m_Caption);

	CWnd *pe = GetDlgItem(IDC_EDIT_INSTALLPATH);
	if (pe)
	{
		pe->SetWindowText(theApp.m_InstallPath);
		pe->EnableWindow(theApp.m_Flags & SFX_FLAG_ALLOWDESTCHG);
	}

	ShowWindow(SW_SHOWNORMAL);

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CSfxDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CSfxDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



bool CreateDirectories(const TCHAR *dir)
{
	if (PathIsRoot(dir) || PathFileExists(dir))
		return false;

	TCHAR _dir[MAX_PATH];
	_tcscpy_s(_dir, MAX_PATH, dir);
	PathRemoveFileSpec(_dir);
	CreateDirectories(_dir);

	CreateDirectory(dir, NULL);

	return true;
}

void CSfxDlg::OnBnClickedOk()
{
	CWnd *pe = GetDlgItem(IDC_EDIT_INSTALLPATH);
	if (pe)
		pe->GetWindowText(theApp.m_InstallPath);

	if (!PathFileExists(theApp.m_InstallPath))
	{
		switch (::MessageBox(GetSafeHwnd(), _T("The specified directory does not exist.  Would you like to create it before proceeding?"), _T("Path Not Found - Create?"), MB_OKCANCEL | MB_ICONHAND))
		{
			case IDOK:
				CreateDirectories(theApp.m_InstallPath);
				break;

			case IDCANCEL:
				return;
				break;
		}
	}

	ULARGE_INTEGER freespace;
	GetDiskFreeSpaceEx(theApp.m_InstallPath, &freespace, NULL, NULL);
	if ((LONGLONG)freespace.QuadPart < theApp.m_SpaceRequired.QuadPart)
	{
		CString s;
		s.Format(_T("The location you have selected does not have enough free space to decompress the entire package (%" PRId64 "KB required)."), theApp.m_SpaceRequired.QuadPart >> 10);
		::MessageBox(GetSafeHwnd(), s, _T("Not Enough Space!"), MB_OK | MB_ICONERROR);
		return;
	}

	CDialogEx::OnOK();
}
