/*
	Copyright © 2013-2025, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
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
	DDX_Control(pDX, IDC_OPTIONS, m_PropGrid);

	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CSfxDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDOK, &CSfxDlg::OnBnClickedOk)
	ON_WM_SYSCOMMAND()
END_MESSAGE_MAP()


// CSfxDlg message handlers

BOOL CSfxDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	int wd = 0;

	CWnd *pImg = GetDlgItem(IDC_IMAGE);
	if (pImg)
	{
		CBitmap bmp;
		bmp.LoadBitmap(_T("PACKAGE"));
		BITMAP b;
		bmp.GetBitmap(&b);

		CRect ri;
		pImg->GetWindowRect(ri);
		wd = b.bmWidth - ri.Width();
		ri.right += wd;
		ScreenToClient(ri);
		pImg->MoveWindow(ri, FALSE);
	}

	CWnd *pt = GetDlgItem(IDC_CHOOSETEXT);
	if (pt)
	{
		CRect rt;
		pt->GetWindowRect(rt);
		rt.left += wd;
		ScreenToClient(rt);
		pt->MoveWindow(rt, FALSE);
	}

	CWnd *pe = GetDlgItem(IDC_EDIT_INSTALLPATH);
	if (pe)
	{
		CRect re;
		pe->GetWindowRect(re);
		re.left += wd;
		ScreenToClient(re);
		pe->MoveWindow(re, FALSE);

		pe->SetWindowText(theApp.m_InstallPath);
		pe->EnableWindow(theApp.m_Flags & SFX_FLAG_ALLOWDESTCHG);
	}

	CWnd *pot = GetDlgItem(IDC_OPTIONSTEXT);
	if (pot)
	{
		CRect rt;
		pot->GetWindowRect(rt);
		rt.left += wd;
		ScreenToClient(rt);
		pot->MoveWindow(rt, FALSE);

		if (theApp.m_Props->GetPropertyCount() == 0)
			pot->ShowWindow(SW_HIDE);
	}

	if (m_PropGrid.GetSafeHwnd())
	{
		CRect rt;
		m_PropGrid.GetWindowRect(rt);
		rt.left += wd;
		ScreenToClient(rt);
		m_PropGrid.MoveWindow(rt, FALSE);

		if (theApp.m_Props->GetPropertyCount() == 0)
			m_PropGrid.ShowWindow(SW_HIDE);
		else
			m_PropGrid.SetActiveProperties(theApp.m_Props);
	}

	if (m_pDynamicLayout)
	{
		delete m_pDynamicLayout;
	}

	m_pDynamicLayout = new CMFCDynamicLayout();
	if (m_pDynamicLayout)
	{
		m_pDynamicLayout->Create(this);

		m_pDynamicLayout->AddItem(GetDlgItem(IDOK)->GetSafeHwnd(), CMFCDynamicLayout::MoveHorizontalAndVertical(100, 100), CMFCDynamicLayout::SizeNone());
		m_pDynamicLayout->AddItem(GetDlgItem(IDCANCEL)->GetSafeHwnd(), CMFCDynamicLayout::MoveHorizontalAndVertical(100, 100), CMFCDynamicLayout::SizeNone());
		m_pDynamicLayout->AddItem(GetDlgItem(IDC_CHOOSETEXT)->GetSafeHwnd(), CMFCDynamicLayout::MoveNone(), CMFCDynamicLayout::SizeHorizontal(100));
		m_pDynamicLayout->AddItem(GetDlgItem(IDC_EDIT_INSTALLPATH)->GetSafeHwnd(), CMFCDynamicLayout::MoveNone(), CMFCDynamicLayout::SizeHorizontal(100));
		m_pDynamicLayout->AddItem(GetDlgItem(IDC_OPTIONSTEXT)->GetSafeHwnd(), CMFCDynamicLayout::MoveNone(), CMFCDynamicLayout::SizeHorizontal(100));
		m_pDynamicLayout->AddItem(GetDlgItem(IDC_OPTIONS)->GetSafeHwnd(), CMFCDynamicLayout::MoveNone(), CMFCDynamicLayout::SizeHorizontalAndVertical(100, 100));
		m_pDynamicLayout->AddItem(GetDlgItem(IDC_IMAGE)->GetSafeHwnd(), CMFCDynamicLayout::MoveNone(), CMFCDynamicLayout::SizeVertical(100));
	}

	SetWindowText(theApp.m_Caption);

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


extern bool ReplaceEnvironmentVariables(const tstring &src, tstring &dst);
extern bool ReplaceRegistryKeys(const tstring &src, tstring &dst);
extern bool FLZACreateDirectories(const TCHAR *dir);

void CSfxDlg::OnBnClickedOk()
{
	CWnd *pe = GetDlgItem(IDC_EDIT_INSTALLPATH);
	if (pe)
	{
		CString tmppath;
		pe->GetWindowText(tmppath);

		tstring _expath, expath = (LPCTSTR)tmppath;
		ReplaceEnvironmentVariables(expath, _expath);
		ReplaceRegistryKeys(_expath, expath);

		theApp.m_InstallPath = expath.c_str();
	}

	if (!PathFileExists(theApp.m_InstallPath))
	{
		CString s;
		s.Format(_T("The specified directory\r\n\"%s\"\r\ndoes not exist.\r\n\r\nWould you like to create it before proceeding?"), theApp.m_InstallPath);
		switch (::MessageBox(GetSafeHwnd(), s, _T("Path Not Found - Create?"), MB_OKCANCEL | MB_ICONHAND))
		{
			case IDOK:
				FLZACreateDirectories(theApp.m_InstallPath);
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
		s.Format(_T("The location you have selected\r\n\"%s\"\r\ndoes not have enough free space to decompress the entire package (%" PRId64 "KB required)."), theApp.m_InstallPath, theApp.m_SpaceRequired.QuadPart >> 10);
		::MessageBox(GetSafeHwnd(), s, _T("Not Enough Space!"), MB_OK | MB_ICONERROR);
		return;
	}

	CDialogEx::OnOK();
}


void CSfxDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if (nID == SC_CLOSE)
	{
		ExitProcess(0);
		return;
	}

	CDialogEx::OnSysCommand(nID, lParam);
}
