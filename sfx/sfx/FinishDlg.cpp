/*
	Copyright ©2016. Authored by Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
	
	For inquiries, contact: keelanstuart@gmail.com
*/

// FinishDlg.cpp : implementation file
//

#include "stdafx.h"
#include "sfx.h"
#include "FinishDlg.h"
#include "afxdialogex.h"

#include "../sfxFlags.h"

// CFinishDlg dialog

IMPLEMENT_DYNAMIC(CFinishDlg, CDialogEx)

CFinishDlg::CFinishDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CFinishDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(_T("ICON"));
}

CFinishDlg::~CFinishDlg()
{
}

void CFinishDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CFinishDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
END_MESSAGE_MAP()


// CFinishDlg message handlers


BOOL CFinishDlg::OnInitDialog()
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

	CWnd *pt = GetDlgItem(IDC_FINISHMSG);
	if (pt)
	{
		CRect rt;
		pt->GetWindowRect(rt);
		rt.left += wd;
		ScreenToClient(rt);
		pt->MoveWindow(rt, FALSE);
	}

	if (theApp.m_Flags & SFX_FLAG_SPAN)
	{
		if (pt)
			pt->SetWindowText(_T("Click Continue to install the remaining files in this package..."));

		CWnd *pb = GetDlgItem(IDOK);
		if (pb)
			pb->SetWindowText(_T("Continue"));
	}

	if ((theApp.m_Flags & SFX_FLAG_EXPLORE) && !(theApp.m_Flags & SFX_FLAG_SPAN))
	{
		CButton *pcb = (CButton *)GetDlgItem(IDC_CHECK_EXPLORE);
		if (pcb)
		{
			CRect rb;
			pcb->GetWindowRect(rb);
			rb.left += wd;
			ScreenToClient(rb);
			pcb->MoveWindow(rb, FALSE);

			pcb->ShowWindow(SW_SHOWNORMAL);
			pcb->SetCheck(BST_CHECKED);
		}
	}

	if (!theApp.m_RunCommand.IsEmpty())
	{
		CButton *pcb = (CButton *)GetDlgItem(IDC_CHECK_RUNCMD);
		if (pcb)
		{
			CRect rb;
			pcb->GetWindowRect(rb);
			rb.left += wd;
			ScreenToClient(rb);
			pcb->MoveWindow(rb, FALSE);

			CString s;
			s.Format(_T("Run command (\"%s\")"), theApp.m_RunCommand);
			pcb->SetWindowText(s);

			pcb->ShowWindow(SW_SHOWNORMAL);
			pcb->SetCheck(BST_CHECKED);
		}
	}

	SetWindowText(theApp.m_Caption);

	ShowWindow(SW_SHOWNORMAL);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CFinishDlg::OnPaint()
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
HCURSOR CFinishDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CFinishDlg::OnOK()
{
	CButton *pcb;
	
	pcb = (CButton *)GetDlgItem(IDC_CHECK_RUNCMD);
	if (pcb && (pcb->GetCheck() == BST_CHECKED))
	{
		if (!(theApp.m_Flags & SFX_FLAG_SPAN))
		{
			CString run = theApp.m_RunCommand;
			if (!PathFileExists(run))
			{
				TCHAR exe[MAX_PATH];
				_tcscpy_s(exe, MAX_PATH, theApp.m_InstallPath);
				PathAddBackslash(exe);
				run = exe;
				run += theApp.m_RunCommand;
			}

			if (PathFileExists(run))
				ShellExecute(NULL, _T("open"), run, NULL, theApp.m_InstallPath, SW_SHOWNORMAL);
		}
		else
		{
			TCHAR exepath[MAX_PATH];
			_tcscpy_s(exepath, MAX_PATH, theApp.m_pszHelpFilePath);
			PathRemoveFileSpec(exepath);
			PathRemoveBackslash(exepath);

			CString exe = exepath;
			exe += _T("\\");
			exe += theApp.m_RunCommand;

			while (!PathFileExists(exe))
			{
				CFileDialog fd(TRUE, NULL, exe, OFN_ENABLESIZING | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_DONTADDTORECENT, NULL, this, sizeof(OPENFILENAME), TRUE);
				fd.m_pOFN->lpstrInitialDir = exepath;
				fd.m_pOFN->lpstrTitle = _T("Browse to the next installer in the span...");
				if (fd.DoModal() != IDOK)
					return;

				exe = fd.GetPathName();
			}

			ShellExecute(NULL, _T("open"), exe, theApp.m_InstallPath, NULL, SW_SHOWNORMAL);
		}
	}

	pcb = (CButton *)GetDlgItem(IDC_CHECK_EXPLORE);
	if (pcb && (pcb->GetCheck() == BST_CHECKED))
	{
		ShellExecute(NULL, _T("explore"), theApp.m_InstallPath, NULL, NULL, SW_SHOWNORMAL);
	}

	if (theApp.m_Flags & SFX_FLAG_REBOOTNEEDED)
	{
		tstring msg;
		msg = _T("Following successful installation of ");
		msg += theApp.m_Caption;
		msg += _T(", a restart is recommended. Would you like to do that now?");

		if (MessageBox(msg.c_str(), _T("Confirmation of Restart"), MB_YESNO) == IDYES)
		{
			InitiateShutdown(NULL, (LPTSTR)((LPCTSTR)theApp.m_Caption), MAX_SHUTDOWN_TIMEOUT, SHUTDOWN_RESTART | SHUTDOWN_RESTARTAPPS, SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_MINOR_INSTALLATION | SHTDN_REASON_FLAG_PLANNED);
		}
	}

	CDialogEx::OnOK();
}


void CFinishDlg::OnCancel()
{
	CDialogEx::OnCancel();
}
