/*
	Copyright © 2013-2020, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
	
	For inquiries, contact: keelanstuart@gmail.com
*/

// WelcomeDlg.cpp : implementation file
//

#include "stdafx.h"
#include "sfx.h"
#include "HtmlCtrl.h"
#include "WelcomeDlg.h"
#include "afxdialogex.h"


// CWelcomeDlg dialog

IMPLEMENT_DYNAMIC(CWelcomeDlg, CDialogEx)

CWelcomeDlg::CWelcomeDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CWelcomeDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(_T("ICON"));
}

CWelcomeDlg::~CWelcomeDlg()
{
}

void CWelcomeDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CWelcomeDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
END_MESSAGE_MAP()


// CWelcomeDlg message handlers


BOOL CWelcomeDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

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

	CRect rw;
	m_Desc.CreateFromStatic(IDC_BROWSER, this);

	CHtmlCtrl *pDesc = &m_Desc;
	if (pDesc)
	{
		pDesc->GetWindowRect(rw);
		rw.left += wd;
		ScreenToClient(rw);
		pDesc->MoveWindow(rw, FALSE);

		pDesc->LoadFromResource(_T("welcome"));
	}

	CWnd *pVerStr = GetDlgItem(IDC_VERSIONID);
	if (pVerStr)
	{
		CRect rv;
		pVerStr->GetWindowRect(rv);
		rv.left += wd;
		ScreenToClient(rv);
		pVerStr->MoveWindow(rv, FALSE);

		pVerStr->SetWindowText(theApp.m_VersionID);
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
		m_pDynamicLayout->AddItem(GetDlgItem(IDC_VERSIONID)->GetSafeHwnd(), CMFCDynamicLayout::MoveVertical(100), CMFCDynamicLayout::SizeHorizontal(100));
		m_pDynamicLayout->AddItem(GetDlgItem(IDC_BROWSER)->GetSafeHwnd(), CMFCDynamicLayout::MoveNone(), CMFCDynamicLayout::SizeHorizontalAndVertical(100, 100));
		m_pDynamicLayout->AddItem(GetDlgItem(IDC_IMAGE)->GetSafeHwnd(), CMFCDynamicLayout::MoveNone(), CMFCDynamicLayout::SizeVertical(100));
	}

	SetWindowText(theApp.m_Caption);

	ShowWindow(SW_SHOWNORMAL);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CWelcomeDlg::OnPaint()
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
HCURSOR CWelcomeDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}
