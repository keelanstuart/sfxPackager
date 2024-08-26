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

	if (m_IndividualProgress.SubclassDlgItem(IDC_INDIVIDUALPROGRESS, this))
	{
		m_IndividualProgress.SetRange32(0, 100);

		m_IndividualProgress.GetWindowRect(r);
		r.left += wd;
		ScreenToClient(r);
		m_IndividualProgress.MoveWindow(r, FALSE);

		m_IndividualProgress.ShowWindow(SW_HIDE);
	}

	CWnd *pst = GetDlgItem(IDC_PROGRESSTEXT);
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
		m_pDynamicLayout->AddItem(GetDlgItem(IDC_INDIVIDUALPROGRESS)->GetSafeHwnd(), CMFCDynamicLayout::MoveVertical(100), CMFCDynamicLayout::SizeHorizontal(100));
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

void FixupJSPath(const TCHAR *path, tstring &newpath)
{
	if (!path)
	{
		newpath.clear();
		return;
	}

	tstring tmp = path;
	path = tmp.c_str();

	newpath.clear();

	while (*path)
	{
		if (*path == _T('\\'))
			newpath += *path;

		newpath += *(path++);
	}
}

const TCHAR *BuildFileScriptPreamble(tstring &preamble, const TCHAR *installpath, const TCHAR *filename, const TCHAR *relpath, const TCHAR *fullpath)
{
	tstring _installpath;
	FixupJSPath(installpath, _installpath);

	preamble = _T("var BASEPATH = \"");
	preamble += _installpath;
	preamble += _T("\";  /* the base install path */\n");

	preamble += _T("var FILENAME = \"");
	preamble += PathFindFileName(filename);
	preamble += _T("\";  /* the name of the file that was just extracted */\n");

	tstring _fpath;

	if (PathIsRelative(relpath))
	{
		_fpath = installpath;
		if (relpath && *relpath)
		{
			_fpath += _T("\\");
			_fpath += relpath;
		}
	}
	else
	{
		_fpath = relpath;
	}

	FixupJSPath(_fpath.c_str(), _fpath);

	preamble += _T("var PATH = \"");
	preamble += _fpath;
	preamble += _T("\";  /* the output path of that file */\n");

	tstring _ffull;
	if (fullpath && *fullpath)
	{
		FixupJSPath(fullpath, _ffull);
	}
	else
	{
		_ffull = _fpath;
		_ffull += _T("\\\\");
		_ffull += filename;
	}

	preamble += _T("var FILEPATH = \"");
	preamble += _ffull.c_str();
	preamble += _T("\";  /* the full filename (path + name) */\n\n");

	return preamble.c_str();
}

const TCHAR *BuildPreInstallScriptPreamble(tstring &preamble, const TCHAR *installpath)
{
	tstring _installpath;
	FixupJSPath(installpath, _installpath);

	preamble = _T("var BASEPATH = \"");
	preamble += _installpath;
	preamble += _T("\";  /* the base install path */\n\n");

	return preamble.c_str();
}


const TCHAR *BuildPostInstallScriptPreamble(tstring &preamble, const TCHAR *installpath, bool cancelled, bool extract_ok)
{
	tstring _installpath;
	FixupJSPath(installpath, _installpath);

	preamble = _T("var BASEPATH = \"");
	preamble += _installpath.c_str();
	preamble += _T("\";  /* the base install path */\n\n");

	preamble += _T("var CANCELLED = \"");
	preamble += cancelled ? _T("1") : _T("0");
	preamble += _T("\";  /* 1 if the installation was cancelled by the user, 0 if not */\n\n");

	preamble += _T("var INSTALLOK = \"");
	preamble += extract_ok ? _T("1") : _T("0");
	preamble += _T("\";  /* 1 if the file extraction / installation was ok, 0 if not */\n\n");

	return preamble.c_str();
}


inline bool BlankScript(const TCHAR *script)
{
	return (!script && !*script) ? true : false;
}

void RunScript(const TCHAR *preamble, const TCHAR *script, const TCHAR *snippet, const TCHAR *scripttype)
{
	if (BlankScript(script) && BlankScript(snippet))
		return;

	tstring s;

	if (preamble)
		s += preamble;

	if (script)
		s += script;

	if (snippet)
		s += snippet;

	if (!IsScriptEmpty(s))
	{
		tstring err;
		try
		{
			theApp.m_js.execute(s);
		}
		catch (CScriptException *se)
		{
			err = se->text;
		}
		catch (...)
		{
			err = _T("UNKNOWN FATAL SCRIPT FAILURE!");
		}

		if (!err.empty())
		{
			TCHAR caption[64];
			_sntprintf(caption, 256, _T("[%s] Script Error"), scripttype);
			MessageBox(NULL, err.c_str(), caption, MB_OK | MB_ICONERROR);
			exit(-1);
		}
	}
}

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
		m.Format(_T("The installation could not continue because the file \"%s\" could not be found."), arcpath);
		if (!theApp.m_bRunAutomated)
		{
			m.Append(_T(" Click OK to exit."));
			MessageBox(m, _T("Archive Data Missing"), MB_OK);
		}
		else
		{
			theApp.Echo(m);
		}

		m_Thread = nullptr;
		PostQuitMessage(-1);
		AfxEndThread(-1);
		return 0;
	}

	CString msg;
	bool skip;

	m_Progress.SetPos(0);

	theApp.m_js.addNative(_T("function Skip()"), scSkip, (void *)&skip);

	tstring preinstallPreamble;
	RunScript(BuildPreInstallScriptPreamble(preinstallPreamble, theApp.m_InstallPath), theApp.m_Script[CSfxApp::EScriptType::PREINSTALL].c_str(), nullptr, _T("PREINSTALL"));

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
			theApp.Echo(msg);

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

				PARSEDURL urlinf = {0};
				urlinf.cbSize = sizeof(PARSEDURL);
				bool isurl = (ParseURL(fname.c_str(), &urlinf) == S_OK);
					
				tstring file_scripts_preamble;

				IExtractor::EXTRACT_RESULT er = IExtractor::EXTRACT_RESULT::ER_OK;

				// reset skip each iteration so that if the user calls the Skip() js function, we won't actually extract it
				skip = false;

				if (!isurl)
					RunScript(BuildFileScriptPreamble(file_scripts_preamble, theApp.m_InstallPath, fname.c_str(), fpath.c_str(), nullptr), theApp.m_Script[CSfxApp::EScriptType::PREFILE].c_str(), prefile_snippet.c_str(), _T("PREFILE"));

				if (!skip)
				{
					er = pie->ExtractFile(i, &ffull, nullptr, theApp.m_TestOnlyMode);

					if (er == IExtractor::ER_MUSTDOWNLOAD)
						RunScript(BuildFileScriptPreamble(file_scripts_preamble, theApp.m_InstallPath, PathFindFileName(ffull.c_str()), fpath.c_str(), ffull.c_str()), theApp.m_Script[CSfxApp::EScriptType::PREFILE].c_str(), prefile_snippet.c_str(), _T("PREFILE"));
				}

				if (skip)
					er = IExtractor::EXTRACT_RESULT::ER_SKIP;

				m_Progress.SetPos((int)i + 1);

				TCHAR relfull[MAX_PATH];
				if (!PathRelativePathTo(relfull, theApp.m_InstallPath, FILE_ATTRIBUTE_DIRECTORY, ffull.c_str(), 0))
					_tcscpy_s(relfull, MAX_PATH, ffull.c_str());
				if (!_tcslen(relfull))
					_tcscpy_s(relfull, MAX_PATH, fname.c_str());

				if (er == IExtractor::ER_OK)
				{
					msg.Format(_T("    %s "), relfull);
					theApp.Echo(msg);
				}

				switch (er)
				{
					case IExtractor::ER_MUSTDOWNLOAD:
					{
						TCHAR dir[MAX_PATH], *_dir = dir;
						_tcscpy_s(dir, ffull.c_str());
						while (_dir && *(_dir++)) { if (*_dir == _T('/')) *_dir = _T('\\'); }

						msg.Format(_T("    Downloading %s from %s ... "), relfull, fname.c_str());
						theApp.Echo(msg);

						if (!theApp.m_TestOnlyMode)
						{
							PathRemoveFileSpec(dir);
							FLZACreateDirectories(dir);

							m_IndividualProgress.ShowWindow(SW_SHOW);

							CHttpDownloader dl;
							bool dlok = dl.DownloadHttpFile(fname.c_str(), ffull.c_str(), m_CancelEvent, DownloadCallback, this);

							m_IndividualProgress.ShowWindow(SW_HIDE);

							if (!dlok)
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

				theApp.Echo(msg);

				if (er == IExtractor::ER_OK)
					RunScript(file_scripts_preamble.c_str(), theApp.m_Script[CSfxApp::EScriptType::POSTFILE].c_str(), postfile_snippet.c_str(), _T("POSTFILE"));
			}

			IExtractor::DestroyExtractor(&pie);

			if (pah)
				pah->Release();
		}

		CloseHandle(hfile);
	}

	tstring postinstallPreamble;
	RunScript(BuildPostInstallScriptPreamble(postinstallPreamble, theApp.m_InstallPath, cancelled, extract_ok), theApp.m_Script[CSfxApp::EScriptType::POSTINSTALL].c_str(), nullptr, _T("POSTINSTALL"));

	msg.Format(_T("Done.\r\n"));
	theApp.Echo(msg);

	CWnd *pok = GetDlgItem(IDOK);
	CWnd *pcancel = GetDlgItem(IDCANCEL);

	if (!extract_ok && !cancelled)
	{
		if (!theApp.m_bRunAutomated)
			MessageBox(_T("One or more files in your archive could not be properly extracted.\r\nThis may be due to your user or directory permissions or disk space."), _T("Extraction Failure"), MB_OK);
		else
			theApp.Echo(_T("One or more files in your archive could not be properly extracted.\r\nThis may be due to your user or directory permissions or disk space."));
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

	if (theApp.m_bRunAutomated)
		ExitProcess(extract_ok ? 0 : -1);

	return ret;
}


void __cdecl CProgressDlg::DownloadCallback(uint64_t bytes_received, uint64_t bytes_expected, void *userdata)
{
	CProgressDlg *_this = (CProgressDlg *)userdata;
	if (!_this)
		return;

	DWORD oldstyle = _this->m_IndividualProgress.GetStyle();
	DWORD newstyle = oldstyle;

	if (bytes_expected != CHttpDownloader::UNKNOWN_EXPECTED_SIZE)
	{
		newstyle &= ~(PBS_MARQUEE | PBS_SMOOTH);

		if (oldstyle != newstyle)
			SetWindowLong(_this->m_IndividualProgress.GetSafeHwnd(), GWL_STYLE, newstyle);

		double r = (double)bytes_received, e = (double)bytes_expected;
		int p = int(r / e * 100.0);

		_this->m_IndividualProgress.SetPos(p);
	}
	else
	{
		newstyle |= PBS_MARQUEE | PBS_SMOOTH;

		if (oldstyle != newstyle)
			SetWindowLong(_this->m_IndividualProgress.GetSafeHwnd(), GWL_STYLE, newstyle);

		_this->m_IndividualProgress.SetMarquee(true, 50);
	}
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
		if (!m_Thread || (MessageBox(_T("Do you really want to cancel this installation process and exit?"), _T("Confirm Exit"), MB_YESNO) == IDYES))
			ExitProcess(0);

		return;
	}

	CDialogEx::OnSysCommand(nID, lParam);
}
