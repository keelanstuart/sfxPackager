/*
	Copyright ©2016. Authored by Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
	
	For inquiries, contact: keelanstuart@gmail.com
*/

// ProgressDlg.cpp : implementation file
//

#include "stdafx.h"
#include "sfx.h"
#include "ProgressDlg.h"
#include "afxdialogex.h"
#include <vector>

#include "../../Archiver/Include/Archiver.h"


// CProgressDlg dialog

IMPLEMENT_DYNAMIC(CProgressDlg, CDialogEx)

CProgressDlg::CProgressDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CProgressDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(_T("ICON"));

	m_CancelEvent = CreateEvent(NULL, true, false, NULL);
	m_Thread = NULL;
}

CProgressDlg::~CProgressDlg()
{
	CloseHandle(m_CancelEvent);
}

void CProgressDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CProgressDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
END_MESSAGE_MAP()


// CProgressDlg message handlers


BOOL CProgressDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	SetWindowText(theApp.m_Caption);

	m_Progress.SubclassDlgItem(IDC_PROGRESS, this);
	m_Status.SubclassDlgItem(IDC_STATUS, this);

	m_Status.SetLimitText(0);

	m_Thread = CreateThread(NULL, 0, InstallThreadProc, this, 0, NULL);

	ShowWindow(SW_SHOWNORMAL);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CProgressDlg::OnPaint()
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
HCURSOR CProgressDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CProgressDlg::OnCancel()
{
	if (m_Thread == NULL)
	{
		CDialogEx::OnCancel();
		return;
	}

	// raise the event
	SetEvent(m_CancelEvent);

	AfxGetApp()->DoWaitCursor(1);

	CWnd *pcancel = GetDlgItem(IDCANCEL);
	if (pcancel)
	{
		pcancel->EnableWindow(FALSE);
	}

	while (m_Thread)
	{
		MSG msg;
		BOOL bRet; 
		while ((bRet = GetMessage(&msg, NULL, 0, 0)) != FALSE)
		{ 
			if (bRet != -1)
			{
				TranslateMessage(&msg); 
				DispatchMessage(&msg); 
			}
		} 

		// then wait for the thread to exit
		Sleep(50);
	}

	if (pcancel)
	{
		pcancel->SetWindowText(_T("<<  Back"));
		pcancel->EnableWindow(TRUE);
	}

	AfxGetApp()->DoWaitCursor(-1);
}


void CProgressDlg::OnOK()
{
	CDialogEx::OnOK();
}

class CSfxHandle : public IArchiveHandle
{
public:
	CSfxHandle(HANDLE hf)
	{
		m_hFile = hf;
	}

	~CSfxHandle()
	{
	}

	virtual HANDLE GetHandle()
	{
		return m_hFile;
	}

	virtual bool Span()
	{
		return false;
	}

	virtual uint64_t GetLength()
	{
		LARGE_INTEGER p;
		GetFileSizeEx(m_hFile, &p);
		return p.QuadPart;
	}

	virtual uint64_t GetOffset()
	{
		LARGE_INTEGER p, z;
		z.QuadPart = 0;
		SetFilePointerEx(m_hFile, z, &p, FILE_CURRENT);
		return p.QuadPart;
	}

protected:
	HANDLE m_hFile;
};

DWORD CProgressDlg::RunInstall()
{
	DWORD ret = 0;

	TCHAR exepath[MAX_PATH];
	_tcscpy_s(exepath, MAX_PATH, theApp.m_pszHelpFilePath);
	PathRenameExtension(exepath, _T(".exe"));

	CString msg;

	m_Progress.SetPos(0);

	if (!theApp.m_Script[CSfxApp::EScriptType::INIT].empty())
	{
		theApp.m_js.execute(theApp.m_Script[CSfxApp::EScriptType::INIT]);
	}

	bool cancelled = false;
	bool extract_ok = true;

	HANDLE hfile = CreateFile(exepath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hfile != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER arcofs;
		DWORD br;
		SetFilePointer(hfile, -(LONG)(sizeof(LONGLONG)), NULL, FILE_END);
		ReadFile(hfile, &(arcofs.QuadPart), sizeof(LONGLONG), &br, NULL);

		SetFilePointerEx(hfile, arcofs, NULL, FILE_BEGIN);

		CSfxHandle sfxh(hfile);
		IExtractor *pie = NULL;
		if (IExtractor::CreateExtractor(&pie, &sfxh) == IExtractor::CR_OK)
		{
			size_t maxi = pie->GetFileCount();

			msg.Format(_T("Extracting %d files to %s...\r\n"), int(maxi), (LPCTSTR)(theApp.m_InstallPath));
			m_Status.SetSel(-1, 0, TRUE);
			m_Status.ReplaceSel(msg);

			m_Progress.SetRange32(0, (int)maxi);

			pie->SetBaseOutputPath((LPCTSTR)(theApp.m_InstallPath));

			for (size_t i = 0; (i < maxi) && !cancelled; i++)
			{
				if (WaitForSingleObject(m_CancelEvent, 0) != WAIT_TIMEOUT)
				{
					msg.Format(_T("Operation cancelled.\r\n"));
					cancelled = true;
					continue;
				}

				tstring fname;
				tstring fpath;
				uint64_t usize;
				pie->GetFileInfo(i, &fname, &fpath, NULL, &usize);

				m_Progress.SetPos((int)i + 1);

				IExtractor::EXTRACT_RESULT er = pie->ExtractFile(i);
				switch (er)
				{
					case IExtractor::ER_OK:
						msg.Format(_T("    %s%s%s (%" PRId64 "KB) [ok]\r\n"), fpath.c_str(), fpath.empty() ? _T("") : _T("\\"), fname.c_str(), usize / 1024);
						break;

					default:
						msg.Format(_T("    %s%s%s [failed]\r\n"), fpath.c_str(), fpath.empty() ? _T("") : _T("\\"), fname.c_str());
						extract_ok = false;
						break;
				}

				m_Status.SetSel(-1, 0, FALSE);
				m_Status.ReplaceSel(msg);
			}

			IExtractor::DestroyExtractor(&pie);
		}

		CloseHandle(hfile);
	}

	if (!theApp.m_Script[CSfxApp::EScriptType::FINISH].empty())
	{
		theApp.m_js.execute(theApp.m_Script[CSfxApp::EScriptType::FINISH]);
	}

	msg.Format(_T("Done.\r\n"));
	m_Status.SetSel(-1, 0, FALSE);
	m_Status.ReplaceSel(msg);

	if (!extract_ok)
		MessageBox(_T("One or more files in your archive could not be properly extracted.\r\nThis may be due to your user or directory permissions or disk space."), _T("Extraction Failure"), MB_OK);

	CWnd *pok = GetDlgItem(IDOK);
	if (pok)
		pok->EnableWindow(TRUE);
	CWnd *pcancel = GetDlgItem(IDCANCEL);
	if (pcancel)
		pcancel->EnableWindow(FALSE);

	m_Thread = NULL;

	return ret;
}

DWORD CProgressDlg::InstallThreadProc(LPVOID param)
{
	CProgressDlg *_this = (CProgressDlg *)param;

	return _this->RunInstall();
}
