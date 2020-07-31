/*
	Copyright © 2013-2020, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
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
#include "LicenseEntryDlg.h"
#include <vector>
#include <istream>
#include <chrono>
#include <ctime>
#include "HttpDownload.h"
#include "../sfxFlags.h"

#include "../../Archiver/Include/Archiver.h"

extern bool ReplaceEnvironmentVariables(const tstring &src, tstring &dst);
extern bool ReplaceRegistryKeys(const tstring &src, tstring &dst);
extern bool FLZACreateDirectories(const TCHAR *dir);

// CProgressDlg dialog

IMPLEMENT_DYNAMIC(CProgressDlg, CDialogEx)

CProgressDlg::CProgressDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CProgressDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(_T("ICON"));

	m_CancelEvent = CreateEvent(NULL, true, false, NULL);
	m_Thread = NULL;

	m_mutexInstallStart = CreateMutex(NULL, TRUE, NULL);
}

CProgressDlg::~CProgressDlg()
{
	CloseHandle(m_mutexInstallStart);
	CloseHandle(m_CancelEvent);
}

void CProgressDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CProgressDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_SYSCOMMAND()
	ON_WM_SIZE()
END_MESSAGE_MAP()


// CProgressDlg message handlers

void CProgressDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);
}


BOOL CProgressDlg::OnInitDialog()
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
		BITMAP b;
		if (bmp.LoadBitmap(_T("PACKAGE")))
		{
			bmp.GetBitmap(&b);
			m_ImgSize.x = b.bmWidth;
			m_ImgSize.y = b.bmHeight;
		}

		CRect ri;
		pImg->GetWindowRect(ri);
		wd = m_ImgSize.x - ri.Width();
		ri.right += wd;
		ScreenToClient(ri);
		pImg->MoveWindow(ri, FALSE);
	}

	CRect r;

	if (m_Progress.SubclassDlgItem(IDC_PROGRESS, this))
	{
		m_Progress.GetWindowRect(r);
		r.left += wd;
		ScreenToClient(r);
		m_Progress.MoveWindow(r, FALSE);
	}

	CStatic *pst = dynamic_cast<CStatic *>(GetDlgItem(IDC_PROGRESSTEXT));
	if (pst)
	{
		int rl = r.left;
		int rr = r.right;
		pst->GetWindowRect(r);
		ScreenToClient(r);
		r.left = rl;
		r.right = rr;
		pst->MoveWindow(r, FALSE);
	}

	if (m_Status.SubclassDlgItem(IDC_STATUS, this))
	{
		m_Status.GetWindowRect(r);
		r.left += wd;
		ScreenToClient(r);
		m_Status.MoveWindow(r, FALSE);
		m_Status.SetLimitText(USHORT_MAX);
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
		m_pDynamicLayout->AddItem(GetDlgItem(IDC_STATUS)->GetSafeHwnd(), CMFCDynamicLayout::MoveNone(), CMFCDynamicLayout::SizeHorizontalAndVertical(100, 100));
		m_pDynamicLayout->AddItem(GetDlgItem(IDC_PROGRESS)->GetSafeHwnd(), CMFCDynamicLayout::MoveVertical(100), CMFCDynamicLayout::SizeHorizontal(100));
		m_pDynamicLayout->AddItem(GetDlgItem(IDC_PROGRESSTEXT)->GetSafeHwnd(), CMFCDynamicLayout::MoveVertical(100), CMFCDynamicLayout::SizeHorizontal(100));
		m_pDynamicLayout->AddItem(GetDlgItem(IDC_IMAGE)->GetSafeHwnd(), CMFCDynamicLayout::MoveNone(), CMFCDynamicLayout::SizeVertical(100));
	}

	m_Status.SetLimitText(0);
	m_Status.SetReadOnly(TRUE);
	m_Status.EnableWindow(TRUE);

	m_Thread = AfxBeginThread((AFX_THREADPROC)InstallThreadProc, (LPVOID)this);

	SetWindowText(theApp.m_Caption);

	ShowWindow(SW_SHOWNORMAL);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CProgressDlg::OnPaint()
{
	static bool first_paint = true;
	if (first_paint)
	{
		ReleaseMutex(m_mutexInstallStart);

		first_paint = false;
	}

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

	if (MessageBox(_T("Do you really want to cancel this installation?"), _T("Confirm Cancel"), MB_YESNO) == IDNO)
		return;

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
		while (AfxPumpMessage())
		{

		}

		// then wait for the thread to exit
		Sleep(50);
	}

	AfxGetApp()->DoWaitCursor(-1);
}


void CProgressDlg::OnOK()
{
	CWnd *pok = GetDlgItem(IDOK);
	if (pok)
	{
		CString t;
		pok->GetWindowText(t);
		if (!t.Compare(_T("Close")))
			ExitProcess(0);
	}

	CDialogEx::OnOK();
}

class CUnpackArchiveHandle : public IArchiveHandle
{
protected:
	HANDLE m_hFile;

public:
	CUnpackArchiveHandle(HANDLE hf)
	{
		m_hFile = hf;
	}

	virtual ~CUnpackArchiveHandle()
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

};

class CSfxHandle : public CUnpackArchiveHandle
{
public:
	CSfxHandle(HANDLE hf) : CUnpackArchiveHandle(hf)
	{
		m_hFile = hf;
	}

	virtual ~CSfxHandle() { }

	virtual void Release()
	{
		delete this;
	}

};

class CExtArcHandle : public CUnpackArchiveHandle
{

protected:
	UINT m_spanIdx;
	TCHAR m_BaseFilename[MAX_PATH];
	TCHAR m_CurrentFilename[MAX_PATH];

public:
	CExtArcHandle(HANDLE hf, const TCHAR *base_filename) : CUnpackArchiveHandle(hf)
	{
		m_spanIdx = 0;
		_tcscpy_s(m_BaseFilename, base_filename);
		_tcscpy_s(m_CurrentFilename, base_filename);
	}

	~CExtArcHandle() { }

	virtual void Release()
	{
		delete this;
	}

	virtual bool Span()
	{
		m_spanIdx++;

		TCHAR local_filename[MAX_PATH];
		_tcscpy_s(local_filename, MAX_PATH, m_BaseFilename);

		TCHAR *plext = PathFindExtension(local_filename);
		if (plext)
			*plext = _T('\0');

		_stprintf_s(m_CurrentFilename, MAX_PATH, _T("%s_part%d.data"), local_filename, m_spanIdx + 1);

		// Finalize archive
		CloseHandle(m_hFile);
		m_hFile = INVALID_HANDLE_VALUE;

		m_hFile = CreateFile(m_CurrentFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		return (m_hFile != INVALID_HANDLE_VALUE);
	}

};

void CProgressDlg::Echo(const TCHAR *msg)
{
	m_Status.SetSel(-1, 0, TRUE);
	m_Status.ReplaceSel(msg);
}


void scSkip(CScriptVar *c, void *userdata)
{
	bool *pskip = (bool *)userdata;
	*pskip = true;
}


// ******************************************************************************
// ******************************************************************************

DWORD CProgressDlg::RunInstall()
{
	WaitForSingleObject(m_mutexInstallStart, INFINITE);

	DWORD ret = 0;

	TCHAR arcpath[MAX_PATH];
	_tcscpy_s(arcpath, MAX_PATH, theApp.m_pszHelpFilePath);
	// if this install uses an external archive file (i.e., not one built into the exe), then use that--
	// same base filename, but with a .data extension
	PathRenameExtension(arcpath, ((theApp.m_Flags & SFX_FLAG_EXTERNALARCHIVE) ? _T(".data") : _T(".exe")));

	if ((theApp.m_Flags & SFX_FLAG_EXTERNALARCHIVE) && !PathFileExists(arcpath))
	{
		CString m;
		m.Format(_T("The installation could not continue because the file \"%s\" could not be found. Click OK to exit."), arcpath);
		MessageBox(m, _T("Archive Data Missing"), MB_OK);
		m_Thread = nullptr;
		PostQuitMessage(-1);
		AfxEndThread(-1);
		return 0;
	}

	CString msg;
	bool skip;

	m_Progress.SetPos(0);

	theApp.m_js.addNative(_T("function Skip()"), scSkip, (void *)&skip);

	theApp.m_InstallPath.Replace(_T("\\"), _T("/"));

	if (!theApp.m_Script[CSfxApp::EScriptType::PREINSTALL].empty())
	{
		tstring iscr;

		iscr += _T("var BASEPATH = \"");
		iscr += (LPCTSTR)(theApp.m_InstallPath);
		iscr += _T("\";  /* the base install path */\n\n");

		iscr += theApp.m_Script[CSfxApp::EScriptType::PREINSTALL];

		if (!IsScriptEmpty(iscr))
			theApp.m_js.execute(iscr);
	}

	bool cancelled = false;
	bool extract_ok = true;

	HANDLE hfile = CreateFile(arcpath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hfile != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER arcofs = {0};
		DWORD br;

		CUnpackArchiveHandle *pah = nullptr;

		if (!(theApp.m_Flags & SFX_FLAG_EXTERNALARCHIVE))
		{
			SetFilePointer(hfile, -(LONG)(sizeof(LONGLONG)), NULL, FILE_END);
			ReadFile(hfile, &(arcofs.QuadPart), sizeof(LONGLONG), &br, NULL);

			pah = new CSfxHandle(hfile);
		}
		else
		{
			pah = new CExtArcHandle(hfile, arcpath);
		}

		SetFilePointerEx(hfile, arcofs, NULL, FILE_BEGIN);

		IExtractor *pie = NULL;
		if (pah && (IExtractor::CreateExtractor(&pie, pah) == IExtractor::CR_OK))
		{
			size_t maxi = pie->GetFileCount();

			msg.Format(_T("Installing %d files to %s  ...\r\n"), int(maxi), (LPCTSTR)(theApp.m_InstallPath));
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
					extract_ok = false;
					continue;
				}

				tstring fname, fpath, prefile_snippet, postfile_snippet, ffull;
				uint64_t usize;
				FILETIME created_time, modified_time;
				if (!pie->GetFileInfo(i, &fname, &fpath, NULL, &usize, &created_time, &modified_time, &prefile_snippet, &postfile_snippet))
					continue;

				tstring file_scripts_preamble;

				file_scripts_preamble += _T("var BASEPATH = \"");
				file_scripts_preamble += (LPCTSTR)(theApp.m_InstallPath);
				file_scripts_preamble += _T("\";  /* the base install path */\n\n");

				file_scripts_preamble += _T("var FILENAME = \"");
				file_scripts_preamble += fname.c_str();
				file_scripts_preamble += _T("\";  /* the name of the file that was just extracted */\n");

				file_scripts_preamble += _T("var PATH = \"");
				file_scripts_preamble += fpath.c_str();
				file_scripts_preamble += _T("\";  /* the output path of that file */\n");

				file_scripts_preamble += _T("var FILEPATH = \"");
				file_scripts_preamble += ffull.c_str();
				file_scripts_preamble += _T("\";  /* the full filename (path + name) */\n\n");

				IExtractor::EXTRACT_RESULT er = IExtractor::EXTRACT_RESULT::ER_SKIP;

				// reset skip each iteration so that if the user calls the Skip() js function, we won't actually extract it
				skip = false;

				if ((er == IExtractor::ER_OK) && (!theApp.m_Script[CSfxApp::EScriptType::PREFILE].empty() || !prefile_snippet.empty()))
				{
					tstring pfscr;

					pfscr += file_scripts_preamble;
					pfscr += theApp.m_Script[CSfxApp::EScriptType::PREFILE];
					pfscr += _T("\n\n");
					pfscr += prefile_snippet;

					if (!IsScriptEmpty(pfscr))
						theApp.m_js.execute(pfscr);
				}

				if (!skip)
					er = pie->ExtractFile(i, &ffull, nullptr, theApp.m_TestOnlyMode);

				m_Progress.SetPos((int)i + 1);

				TCHAR relfull[MAX_PATH];
				if (!PathRelativePathTo(relfull, theApp.m_InstallPath, FILE_ATTRIBUTE_DIRECTORY, ffull.c_str(), 0))
					_tcscpy_s(relfull, MAX_PATH, ffull.c_str());
				if (!_tcslen(relfull))
					_tcscpy_s(relfull, MAX_PATH, fname.c_str());

				if (er == IExtractor::ER_OK)
				{
					msg.Format(_T("    %s "), relfull);
					m_Status.SetSel(-1, 0, FALSE);
					m_Status.ReplaceSel(msg);
				}

				switch (er)
				{
					case IExtractor::ER_MUSTDOWNLOAD:
					{
						_tcscat_s(relfull, PathFindFileName(ffull.c_str()));

						TCHAR dir[MAX_PATH], *_dir = dir;
						_tcscpy_s(dir, ffull.c_str());
						while (_dir && *(_dir++)) { if (*_dir == _T('/')) *_dir = _T('\\'); }

						const TCHAR *dlfn = PathFindFileName(ffull.c_str());
						msg.Format(_T("    Downloading %s from %s ... "), relfull, fname.c_str());
						m_Status.SetSel(-1, 0, FALSE);
						m_Status.ReplaceSel(msg);

						if (!theApp.m_TestOnlyMode)
						{
							PathRemoveFileSpec(dir);
							FLZACreateDirectories(dir);

							CHttpDownloader dl;
							if (!dl.DownloadHttpFile(fname.c_str(), ffull.c_str(), _T("")))
							{
								msg.Format(_T("[download failed]\r\n"));
								break;
							}
						}

						HANDLE dlfh = CreateFile(ffull.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, NULL);
						if (dlfh == INVALID_HANDLE_VALUE)
						{
							msg.Format(_T("[file error]\r\n"));
							break;
						}

						LARGE_INTEGER fsz;
						GetFileSizeEx(dlfh, &fsz);
						usize = fsz.QuadPart;
						CloseHandle(dlfh);

						std::replace(ffull.begin(), ffull.end(), _T('\\'), _T('/'));

						fname = PathFindFileName(ffull.c_str());
						fpath = ffull;
						size_t sp = fpath.find_last_of(_T('/'));
						if (sp < fpath.length())
						{
							tstring::const_iterator pit = fpath.cbegin() + sp;
							fpath.erase(pit, fpath.cend());
						}
						else
							fpath = _T("./");

						er = IExtractor::ER_OK;
					}

					case IExtractor::ER_OK:
					{
						std::replace(ffull.begin(), ffull.end(), _T('\\'), _T('/'));
						std::replace(fpath.begin(), fpath.end(), _T('\\'), _T('/'));

						msg.Format(_T("(%" PRId64 "KB) [ok]\r\n"), std::max<uint64_t>(1, usize / 1024));

						break;
					}

					case IExtractor::ER_SKIP:
						msg.Format(_T("    %s [skipped]\r\n"), relfull);
						break;

					default:
						msg.Format(_T("    %s [failed]\r\n"), relfull);
						extract_ok = false;
						break;
				}

				m_Status.SetSel(-1, 0, FALSE);
				m_Status.ReplaceSel(msg);

				if ((er == IExtractor::ER_OK) && (!theApp.m_Script[CSfxApp::EScriptType::POSTFILE].empty() || !postfile_snippet.empty()))
				{
					tstring pfscr;

					pfscr += file_scripts_preamble;
					pfscr += theApp.m_Script[CSfxApp::EScriptType::POSTFILE];
					pfscr += _T("\n\n");
					pfscr += postfile_snippet;

					if (!IsScriptEmpty(pfscr))
						theApp.m_js.execute(pfscr);
				}
			}

			IExtractor::DestroyExtractor(&pie);

			if (pah)
				pah->Release();
		}

		CloseHandle(hfile);
	}

	if (!theApp.m_Script[CSfxApp::EScriptType::POSTINSTALL].empty())
	{
		tstring fscr;

		fscr += _T("var BASEPATH = \"");
		fscr += (LPCTSTR)(theApp.m_InstallPath);
		fscr += _T("\";  /* the base install path */\n\n");

		fscr += _T("var CANCELLED = \"");
		fscr += cancelled ? _T("1") : _T("0");
		fscr += _T("\";  /* 1 if the installation was cancelled by the user, 0 if not */\n\n");

		fscr += _T("var INSTALLOK = \"");
		fscr += extract_ok ? _T("1") : _T("0");
		fscr += _T("\";  /* 1 if the file extraction / installation was ok, 0 if not */\n\n");

		fscr += theApp.m_Script[CSfxApp::EScriptType::POSTINSTALL];

		if (!IsScriptEmpty(fscr))
			theApp.m_js.execute(fscr);
	}

	msg.Format(_T("Done.\r\n"));
	m_Status.SetSel(-1, 0, FALSE);
	m_Status.ReplaceSel(msg);

	CWnd *pok = GetDlgItem(IDOK);
	CWnd *pcancel = GetDlgItem(IDCANCEL);

	if (!extract_ok && !cancelled)
	{
		MessageBox(_T("One or more files in your archive could not be properly extracted.\r\nThis may be due to your user or directory permissions or disk space."), _T("Extraction Failure"), MB_OK);
	}

	if (pok)
	{
		if (!extract_ok)
			pok->SetWindowText(_T("Close"));

		pok->EnableWindow(!cancelled);
	}

	if (pcancel)
	{
		if (!extract_ok && !cancelled)
			pcancel->ShowWindow(SW_HIDE);
	
		if (cancelled)
			pcancel->SetWindowText(_T("<< Back"));

		pcancel->EnableWindow(cancelled);
	}

	m_Thread = nullptr;
	AfxEndThread(0);

	return ret;
}

UINT CProgressDlg::InstallThreadProc(LPVOID param)
{
	CProgressDlg *_this = (CProgressDlg *)param;

	return _this->RunInstall();
}


void CProgressDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if (nID == SC_CLOSE)
	{
		if (MessageBox(_T("Do you really want to cancel this installation process and exit?"), _T("Confirm Exit"), MB_YESNO) == IDYES)
			ExitProcess(0);

		return;
	}

	CDialogEx::OnSysCommand(nID, lParam);
}
