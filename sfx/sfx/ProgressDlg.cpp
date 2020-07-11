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
#include "../sfxPackager/GenParser.h"
#include "HttpDownload.h"
#include "../sfxFlags.h"

#include "../../Archiver/Include/Archiver.h"

extern bool ReplaceEnvironmentVariables(const tstring &src, tstring &dst);
extern bool ReplaceRegistryKeys(const tstring &src, tstring &dst);
extern bool FLZACreateDirectories(const TCHAR *dir);

static CLicenseKeyEntryDlg *licensedlg;

// CProgressDlg dialog

IMPLEMENT_DYNAMIC(CProgressDlg, CDialogEx)

CProgressDlg::CProgressDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CProgressDlg::IDD, pParent)
{
	licensedlg = new CLicenseKeyEntryDlg(_T(""));

	m_hIcon = AfxGetApp()->LoadIcon(_T("ICON"));

	m_CancelEvent = CreateEvent(NULL, true, false, NULL);
	m_Thread = NULL;

	m_mutexInstallStart = CreateMutex(NULL, TRUE, NULL);
}

CProgressDlg::~CProgressDlg()
{
	CloseHandle(m_mutexInstallStart);
	CloseHandle(m_CancelEvent);
	if (licensedlg)
	{
		delete licensedlg;
		licensedlg = nullptr;
	}
}

void CProgressDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CProgressDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_SYSCOMMAND()
END_MESSAGE_MAP()


// CProgressDlg message handlers


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

	CRect r;

	if (m_Progress.SubclassDlgItem(IDC_PROGRESS, this))
	{
		m_Progress.GetWindowRect(r);
		r.left += wd;
		ScreenToClient(r);
		m_Progress.MoveWindow(r, FALSE);
	}

	CEdit *pst = dynamic_cast<CEdit *>(GetDlgItem(IDC_PROGRESSTEXT));
	if (pst)
	{
		pst->GetWindowRect(r);
		r.left += wd;
		ScreenToClient(r);
		pst->MoveWindow(r, FALSE);
		pst->SetLimitText(USHORT_MAX);
	}

	if (m_Status.SubclassDlgItem(IDC_STATUS, this))
	{
#if 0
		int nFontSize = 8;
		int nHeight = -((GetDC()->GetDeviceCaps(LOGPIXELSY) * nFontSize) / 72);
		m_Font.CreateFont(nHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH | FF_DONTCARE, _T("Arial"));
		m_Status.SetFont(&m_Font);
#endif

		m_Status.GetWindowRect(r);
		r.left += wd;
		ScreenToClient(r);
		m_Status.MoveWindow(r, FALSE);
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

HRESULT CreateShortcut(const TCHAR *targetFile, const TCHAR *targetArgs, const TCHAR *linkFile, const TCHAR *description,
					   int showMode, const TCHAR *curDir, const TCHAR *iconFile, int iconIndex)
{
	HRESULT hr = E_INVALIDARG;

	if ((targetFile && *targetFile) && (linkFile && linkFile))
	{
		CoInitialize(NULL);

		IShellLink *pl;
		hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&pl);

		if (SUCCEEDED(hr))
		{
			hr = pl->SetPath(targetFile);
			hr = pl->SetArguments(targetArgs ? targetArgs : _T(""));

			if (description && *description)
				hr = pl->SetDescription(description);

			if (showMode > 0)
				hr = pl->SetShowCmd(showMode);

			if (curDir && curDir)
				hr = pl->SetWorkingDirectory(curDir);

			if (iconFile && *iconFile && (iconIndex >= 0))
				hr = pl->SetIconLocation(iconFile, iconIndex);

			IPersistFile* pf;
			hr = pl->QueryInterface(IID_IPersistFile, (LPVOID*)&pf);
			if (SUCCEEDED(hr))
			{
				wchar_t *fn;
				LOCAL_TCS2WCS(linkFile, fn);

				hr = pf->Save(fn, TRUE);
				pf->Release();
			}
			pl->Release();
		}

		CoUninitialize();
	}

	return (hr);
}


void CProgressDlg::Echo(const TCHAR *msg)
{
	m_Status.SetSel(-1, 0, TRUE);
	m_Status.ReplaceSel(msg);
}


// ******************************************************************************
// ******************************************************************************

void scSetGlobalInt(CScriptVar *c, void *userdata)
{
	tstring name = c->getParameter(_T("name"))->getString();
	int64_t val = c->getParameter(_T("val"))->getInt();

	CProgressDlg *_this = (CProgressDlg *)userdata;

	std::pair<CSfxApp::TIntMap::iterator, bool> insret = theApp.m_jsGlobalIntMap.insert(CSfxApp::TIntMap::value_type(name, val));
	if (!insret.second)
		insret.first->second = val;
}


void scGetGlobalInt(CScriptVar *c, void *userdata)
{
	tstring name = c->getParameter(_T("name"))->getString();

	CProgressDlg *_this = (CProgressDlg *)userdata;

	CScriptVar *ret = c->getReturnVar();
	if (ret)
	{
		CSfxApp::TIntMap::iterator it = theApp.m_jsGlobalIntMap.find(name);

		ret->setInt((it != theApp.m_jsGlobalIntMap.end()) ? it->second : 0);
	}
}


void scMessageBox(CScriptVar *c, void *userdata)
{
	tstring title = c->getParameter(_T("title"))->getString();
	tstring msg = c->getParameter(_T("msg"))->getString();

	CProgressDlg *_this = (CProgressDlg *)userdata;

	MessageBox(_this->GetSafeHwnd(), msg.c_str(), title.c_str(), MB_OK);
}


void scMessageBoxYesNo(CScriptVar *c, void *userdata)
{
	tstring title = c->getParameter(_T("title"))->getString();
	tstring msg = c->getParameter(_T("msg"))->getString();

	CProgressDlg *_this = (CProgressDlg *)userdata;

	bool bret = (MessageBox(_this->GetSafeHwnd(), msg.c_str(), title.c_str(), MB_YESNO) == IDYES) ? 1 : 0;
	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setInt(bret);
}


void scEcho(CScriptVar *c, void *userdata)
{
	tstring msg = c->getParameter(_T("msg"))->getString(), _msg;
	ReplaceEnvironmentVariables(msg, _msg);
	ReplaceRegistryKeys(_msg, msg);

	CProgressDlg *_this = (CProgressDlg *)userdata;
	_this->Echo(msg.c_str());
}


void scCreateDirectoryTree(CScriptVar *c, void *userdata)
{
	tstring path = c->getParameter(_T("path"))->getString(), _path;
	ReplaceEnvironmentVariables(path, _path);
	ReplaceRegistryKeys(_path, path);

	bool create_result = TRUE;
	if (!theApp.m_TestOnlyMode)
	{
		create_result = FLZACreateDirectories(path.c_str());
	}

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setInt(create_result ? 1 : 0);
}


void scCopyFile(CScriptVar *c, void *userdata)
{
	tstring src = c->getParameter(_T("src"))->getString(), _src;
	ReplaceEnvironmentVariables(src, _src);
	ReplaceRegistryKeys(_src, src);

	tstring dst = c->getParameter(_T("dst"))->getString(), _dst;
	ReplaceEnvironmentVariables(dst, _dst);
	ReplaceRegistryKeys(_dst, dst);

	BOOL copy_result;
	if (!theApp.m_TestOnlyMode)
	{
		copy_result = CopyFile(src.c_str(), dst.c_str(), false);
	}

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setInt(copy_result ? 1 : 0);
}


void scRenameFile(CScriptVar *c, void *userdata)
{
	tstring filename = c->getParameter(_T("filename"))->getString(), _filename;
	ReplaceEnvironmentVariables(filename, _filename);
	ReplaceRegistryKeys(_filename, filename);

	tstring newname = c->getParameter(_T("newname"))->getString(), _newname;
	ReplaceEnvironmentVariables(newname, _newname);
	ReplaceRegistryKeys(_newname, newname);

	int rename_result = TRUE;
	if (!theApp.m_TestOnlyMode)
	{
		rename_result = _trename(filename.c_str(), newname.c_str());
	}

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setInt((rename_result == 0) ? 1 : 0);
}


void scDeleteFile(CScriptVar *c, void *userdata)
{
	tstring path = c->getParameter(_T("path"))->getString(), _path;
	ReplaceEnvironmentVariables(path, _path);
	ReplaceRegistryKeys(_path, path);


	BOOL delete_result = TRUE;
	if (!theApp.m_TestOnlyMode)
	{
		delete_result = DeleteFile(path.c_str());
	}

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setInt(delete_result ? 1 : 0);
}


void scFileExists(CScriptVar *c, void *userdata)
{
	tstring path = c->getParameter(_T("path"))->getString(), _path;
	ReplaceEnvironmentVariables(path, _path);
	ReplaceRegistryKeys(_path, path);

	BOOL result = PathFileExists(path.c_str());
	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setInt(result ? 1 : 0);
}

void scIsDirectory(CScriptVar *c, void *userdata)
{
	tstring path = c->getParameter(_T("path"))->getString(), _path;
	ReplaceEnvironmentVariables(path, _path);
	ReplaceRegistryKeys(_path, path);

	BOOL result = PathIsDirectory(path.c_str());
	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setInt(result ? 1 : 0);
}


void scIsDirectoryEmpty(CScriptVar *c, void *userdata)
{
	tstring path = c->getParameter(_T("path"))->getString(), _path;
	ReplaceEnvironmentVariables(path, _path);
	ReplaceRegistryKeys(_path, path);

	BOOL result = PathIsDirectoryEmpty(path.c_str());
	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setInt(result ? 1 : 0);
}

void scCreateSymbolicLink(CScriptVar *c, void *userdata)
{
	tstring targetname = c->getParameter(_T("targetname"))->getString(), _targetname;
	ReplaceEnvironmentVariables(targetname, _targetname);
	ReplaceRegistryKeys(_targetname, targetname);

	tstring linkname = c->getParameter(_T("linkname"))->getString(), _linkname;
	ReplaceEnvironmentVariables(linkname, _linkname);
	ReplaceRegistryKeys(_linkname, linkname);

	DWORD flags = 0;
	if (PathIsDirectory(targetname.c_str()))
		flags |= SYMBOLIC_LINK_FLAG_DIRECTORY;

	BOOL result = CreateSymbolicLink(linkname.c_str(), targetname.c_str(), flags);
	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setInt(result ? 1 : 0);
}

void scCreateShortcut(CScriptVar *c, void *userdata)
{
	tstring file = c->getParameter(_T("file"))->getString(), _file;
	ReplaceEnvironmentVariables(file, _file);
	ReplaceRegistryKeys(_file, file);

	tstring targ = c->getParameter(_T("target"))->getString(), _targ;
	ReplaceEnvironmentVariables(targ, _targ);
	ReplaceRegistryKeys(_targ, targ);

	tstring args = c->getParameter(_T("args"))->getString(), _args;
	ReplaceEnvironmentVariables(args, _args);
	ReplaceRegistryKeys(_args, args);

	tstring rundir = c->getParameter(_T("rundir"))->getString(), _rundir;
	ReplaceEnvironmentVariables(rundir, _rundir);
	ReplaceRegistryKeys(_rundir, rundir);

	tstring desc = c->getParameter(_T("desc"))->getString(), _desc;
	ReplaceEnvironmentVariables(desc, _desc);
	ReplaceRegistryKeys(_desc, desc);

	int64_t showmode = c->getParameter(_T("showmode"))->getInt();

	tstring icon = c->getParameter(_T("icon"))->getString(), _icon;
	ReplaceEnvironmentVariables(icon, _icon);
	ReplaceRegistryKeys(_icon, icon);

	int64_t iconidx = c->getParameter(_T("iconidx"))->getInt();

	if (!theApp.m_TestOnlyMode)
	{
		CreateShortcut(targ.c_str(), args.c_str(), file.c_str(), desc.c_str(),
					   (int)showmode, rundir.c_str(), icon.c_str(), (int)iconidx);
	}
}


void scGetGlobalEnvironmentVariable(CScriptVar *c, void *userdata)
{
	CScriptVar *ret = c->getReturnVar();
	if (!ret)
		return;

	tstring var = c->getParameter(_T("varname"))->getString(), _var;
	ReplaceEnvironmentVariables(var, _var);
	ReplaceRegistryKeys(_var, var);

	tstring rs;
	DWORD sz = GetEnvironmentVariable(var.c_str(), nullptr, 0);
	if (sz > 0)
	{
		rs.resize(sz, _T('#'));
		GetEnvironmentVariable(var.c_str(), (TCHAR *)(rs.data()), sz);
	}

	ret->setString(rs);
}


void scSetGlobalEnvironmentVariable(CScriptVar *c, void *userdata)
{
	tstring var = c->getParameter(_T("varname"))->getString(), _var;
	ReplaceEnvironmentVariables(var, _var);
	ReplaceRegistryKeys(_var, var);

	tstring val = c->getParameter(_T("val"))->getString(), _val;
	ReplaceEnvironmentVariables(val, _val);
	ReplaceRegistryKeys(_val, val);

	if (!theApp.m_TestOnlyMode)
	{
		CRegKey cKey;
		if (SUCCEEDED(cKey.Create(HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"))))
		{
			cKey.SetStringValue(var.c_str(), val.c_str());
		}
	}
}


void scRegistryKeyValueExists(CScriptVar *c, void *userdata)
{
	CScriptVar *ret = c->getReturnVar();
	if (!ret)
		return;

	ret->setInt(0);

	tstring root = c->getParameter(_T("root"))->getString();
	std::transform(root.begin(), root.end(), root.begin(), toupper);
	HKEY hr = HKEY_LOCAL_MACHINE;
	if (root == _T("HKEY_CURRENT_USER"))
		hr = HKEY_CURRENT_USER;
	else if (root == _T("HKEY_CURRENT_CONFIG"))
		hr = HKEY_CURRENT_CONFIG;
	else if (root != _T("HKEY_LOCAL_MACHINE"))
		return;

	tstring key = c->getParameter(_T("key"))->getString(), _key;
	ReplaceEnvironmentVariables(key, _key);
	ReplaceRegistryKeys(_key, key);
	for (tstring::iterator it = key.begin(), last_it = key.end(); it != last_it; it++)
	{
		if (*it == _T('/'))
			*it = _T('\\');
	}

	tstring name = c->getParameter(_T("name"))->getString(), _name;
	ReplaceEnvironmentVariables(name, _name);
	ReplaceRegistryKeys(_name, name);

	CRegKey cKey;
	if (SUCCEEDED(cKey.Open(hr, key.c_str())) && cKey.m_hKey)
	{
		if (!name.empty())
		{
			TCHAR tmp;
			ULONG tmps = 1;
			if (cKey.QueryStringValue(name.c_str(), &tmp, &tmps) != ERROR_FILE_NOT_FOUND)
				ret->setInt(1);
		}

		cKey.Close();
	}
}


void scGetRegistryKeyValue(CScriptVar *c, void *userdata)
{
	CScriptVar *ret = c->getReturnVar();
	if (!ret)
		return;

	tstring root = c->getParameter(_T("root"))->getString();
	std::transform(root.begin(), root.end(), root.begin(), toupper);
	HKEY hr = HKEY_LOCAL_MACHINE;
	if (root == _T("HKEY_CURRENT_USER"))
		hr = HKEY_CURRENT_USER;
	else if (root == _T("HKEY_CURRENT_CONFIG"))
		hr = HKEY_CURRENT_CONFIG;
	else if (root != _T("HKEY_LOCAL_MACHINE"))
		return;

	tstring key = c->getParameter(_T("key"))->getString(), _key;
	ReplaceEnvironmentVariables(key, _key);
	ReplaceRegistryKeys(_key, key);
	for (tstring::iterator it = key.begin(), last_it = key.end(); it != last_it; it++)
	{
		if (*it == _T('/'))
			*it = _T('\\');
	}

	tstring valname = c->getParameter(_T("name"))->getString(), _valname;
	ReplaceEnvironmentVariables(valname, _valname);
	ReplaceRegistryKeys(_valname, valname);

	if (!theApp.m_TestOnlyMode)
	{
		HKEY hkey;
		if (RegOpenKeyEx(hr, key.c_str(), 0, KEY_READ, &hkey) == ERROR_SUCCESS)
		{
			DWORD cb;
			DWORD type;
			BYTE val[(MAX_PATH + 1) * 2 * sizeof(TCHAR)];
			if (RegGetValue(hkey, nullptr, valname.c_str(), RRF_RT_DWORD | RRF_RT_QWORD | RRF_RT_REG_SZ, &type, val, &cb) == ERROR_SUCCESS)
			{
				switch (type)
				{
					case REG_SZ:
						ret->setString((const TCHAR *)val);
						break;

					case REG_DWORD:
						ret->setInt((int64_t)(DWORD((*(const DWORD *)val))));
						break;

					case REG_QWORD:
						ret->setInt((int64_t)(QWORD((*(const QWORD *)val))));
						break;

					default:
						break;
				}

			}

			RegCloseKey(hkey);
		}
	}
}


void scSetRegistryKeyValue(CScriptVar *c, void *userdata)
{
	tstring root = c->getParameter(_T("root"))->getString();
	std::transform(root.begin(), root.end(), root.begin(), toupper);
	HKEY hr = HKEY_LOCAL_MACHINE;
	if (root == _T("HKEY_CURRENT_USER"))
		hr = HKEY_CURRENT_USER;
	else if (root == _T("HKEY_CURRENT_CONFIG"))
		hr = HKEY_CURRENT_CONFIG;
	else if (root != _T("HKEY_LOCAL_MACHINE"))
		return;

	tstring key = c->getParameter(_T("key"))->getString(), _key;
	ReplaceEnvironmentVariables(key, _key);
	ReplaceRegistryKeys(_key, key);
	for (tstring::iterator it = key.begin(), last_it = key.end(); it != last_it; it++)
	{
		if (*it == _T('/'))
			*it = _T('\\');
	}

	tstring name = c->getParameter(_T("name"))->getString(), _name;
	ReplaceEnvironmentVariables(name, _name);
	ReplaceRegistryKeys(_name, name);

	tstring val = c->getParameter(_T("val"))->getString(), _val;
	ReplaceEnvironmentVariables(val, _val);
	ReplaceRegistryKeys(_val, val);

	if (!theApp.m_TestOnlyMode)
	{
		CRegKey cKey;
		if (SUCCEEDED(cKey.Create(hr, key.c_str())))
		{
			cKey.SetStringValue(name.c_str(), val.c_str());
		}
	}
}


void scSpawnProcess(CScriptVar *c, void *userdata)
{
	tstring cmd = c->getParameter(_T("cmd"))->getString(), _cmd;
	ReplaceEnvironmentVariables(cmd, _cmd);
	ReplaceRegistryKeys(_cmd, cmd);

	tstring params = c->getParameter(_T("params"))->getString(), _params;
	ReplaceEnvironmentVariables(params, _params);
	ReplaceRegistryKeys(_params, params);

	tstring rundir = c->getParameter(_T("rundir"))->getString(), _rundir;
	ReplaceEnvironmentVariables(rundir, _rundir);
	ReplaceRegistryKeys(_rundir, rundir);

	bool block = c->getParameter(_T("block"))->getBool();

	tstring arg;
	arg.reserve((cmd.length() + params.length()) * 2);

	if (!params.empty())
		arg += _T("\"");

	arg += cmd;

	if (!params.empty())
	{
		arg += _T("\" ");
		arg += params;
	}

	STARTUPINFO si = { 0 };
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi;

	BOOL created = true;
	if (!theApp.m_TestOnlyMode)
	{
		created = CreateProcess(nullptr, (TCHAR *)(arg.c_str()), NULL, NULL, FALSE, NULL, NULL, rundir.empty() ? NULL : rundir.c_str(), &si, &pi);
		if (created)
		{
			if (block)
				WaitForSingleObject(pi.hProcess, INFINITE);
		}
		else
		{
			LPVOID lpMsgBuf;
			DWORD dw = GetLastError();
			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
						  NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);

			// Display the error
			CProgressDlg *_this = (CProgressDlg *)userdata;
			_this->Echo(_T("SpawnProcess Failed:\n\t"));
			_this->Echo((const TCHAR *)lpMsgBuf);

			// Free resources created by the system
			LocalFree(lpMsgBuf);
		}
	}

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setInt(created ? 1 : 0);
}

void scGetExeVersion(CScriptVar* c, void* userdata)
{
	tstring fn = c->getParameter(_T("file"))->getString(), _fn;
	ReplaceEnvironmentVariables(fn, _fn);
	ReplaceRegistryKeys(_fn, fn);

	TCHAR v[128];
	_stprintf_s(v, 127, _T("%d.%d.%d.%d"), 0, 0, 0, 0);

	if (PathFileExists(fn.c_str()))
	{
		DWORD  verHandle = 0;
		UINT   size = 0;
		LPBYTE lpBuffer = NULL;
		DWORD  verSize = GetFileVersionInfoSize(fn.c_str(), &verHandle);

		if (verSize != NULL)
		{
			LPSTR verData = new char[verSize];

			if (GetFileVersionInfo(fn.c_str(), verHandle, verSize, verData))
			{
				if (VerQueryValue(verData, _T("\\"), (VOID FAR * FAR*) & lpBuffer, &size))
				{
					if (size)
					{
						VS_FIXEDFILEINFO* verInfo = (VS_FIXEDFILEINFO*)lpBuffer;
						if (verInfo->dwSignature == 0xfeef04bd)
						{
							_stprintf_s(v, 127, _T("%d.%d.%d.%d"),
								(verInfo->dwFileVersionMS >> 16) & 0xffff,
								(verInfo->dwFileVersionMS >> 0) & 0xffff,
								(verInfo->dwFileVersionLS >> 16) & 0xffff,
								(verInfo->dwFileVersionLS >> 0) & 0xffff);
						}
					}
				}
			}

			delete[] verData;
		}
	}

	CScriptVar* ret = c->getReturnVar();
	if (ret)
		ret->setString(tstring(v));
}


void scCompareStrings(CScriptVar* c, void* userdata)
{
	tstring str1 = c->getParameter(_T("str1"))->getString();
	tstring str2 = c->getParameter(_T("str2"))->getString();

	int cmp = _tcscmp(str1.c_str(), str2.c_str());

	CScriptVar* ret = c->getReturnVar();
	if (ret)
		ret->setInt(cmp);
}


void scAbortInstall(CScriptVar* c, void* userdata)
{
	exit(-1);
}


void scShowLicenseDlg(CScriptVar *c, void *userdata)
{
	if (!licensedlg)
		return;

	INT_PTR dlg_ret = licensedlg->DoModal();

	if (dlg_ret == IDCANCEL)
	{
		exit(-1);
	}
}


void scGetLicenseKey(CScriptVar *c, void *userdata)
{
	tstring key;
	if (licensedlg)
		key = licensedlg->GetKey();

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setString(key);
}


void scGetLicenseUser(CScriptVar *c, void *userdata)
{
	tstring user;
	if (licensedlg)
		user = licensedlg->GetUser();

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setString(user);
}


void scGetLicenseOrg(CScriptVar *c, void *userdata)
{
	tstring org;
	if (licensedlg)
		org = licensedlg->GetOrg();

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setString(org);
}


void scDownloadFile(CScriptVar *c, void *userdata)
{
	CScriptVar *purl = c->getParameter(_T("url"));
	CScriptVar *pfile = c->getParameter(_T("file"));

	BOOL result = FALSE;

	if (purl && pfile)
	{
		CHttpDownloader dl;
		result = dl.DownloadHttpFile(purl->getString().c_str(), pfile->getString().c_str(), _T(""));
	}

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setInt(result ? 1 : 0);
}


void scTextFileOpen(CScriptVar *c, void *userdata)
{
	CScriptVar *pfile = c->getParameter(_T("filename"));
	CScriptVar *pmode = c->getParameter(_T("mode"));

	FILE *f = nullptr;
	if (pfile)
		f = _tfopen(pfile->getString().c_str(), pmode ? pmode->getString().c_str() : _T("r, ccs=UTF-8"));

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setInt((int64_t)f);
}


void scTextFileClose(CScriptVar *c, void *userdata)
{
	CScriptVar *phandle = c->getParameter(_T("handle"));

	if (phandle)
	{
		FILE *f = (FILE *)phandle->getInt();
		if (f)
			fclose(f);
	}
}


void scTextFileReadLn(CScriptVar *c, void *userdata)
{
	CScriptVar *phandle = c->getParameter(_T("handle"));

	tstring s;

	if (phandle)
	{
		FILE *f = (FILE *)phandle->getInt();
		if (f)
		{
			TCHAR _s[4096];
			if (_fgetts(_s, 4096, f))
			{
				size_t n = _tcslen(_s);

				if ((n > 0) && (n <= 4096))
					_s[n - 1] = _T('\0');

				s = _s;
			}
		}
	}

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setString(s);
}


void scTextFileWrite(CScriptVar *c, void *userdata)
{
	CScriptVar *phandle = c->getParameter(_T("handle"));
	CScriptVar *ptext = c->getParameter(_T("text"));

	if (phandle && ptext)
	{
		FILE *f = (FILE *)phandle->getInt();
		if (f)
			_fputts(ptext->getString().c_str(), f);
	}
}


void scTextFileReachedEOF(CScriptVar *c, void *userdata)
{
	CScriptVar *phandle = c->getParameter(_T("handle"));

	int64_t b = 1;
	if (phandle)
	{
		FILE *f = (FILE *)phandle->getInt();
		if (f)
			b = feof(f);
	}

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setInt(b);
}


void scGetCurrentDateStr(CScriptVar *c, void *userdata)
{
	auto now = std::chrono::system_clock::now();
	std::time_t now_c = std::chrono::system_clock::to_time_t(now);
	struct tm *parts = std::localtime(&now_c);

	TCHAR dates[MAX_PATH];
	_stprintf(dates, _T("%04d/%02d/%02d"), 1900 + parts->tm_year, 1 + parts->tm_mon, parts->tm_mday);

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setString(tstring(dates));
}


void scGetFileNameFromPath(CScriptVar *c, void *userdata)
{
	CScriptVar *pfilename = c->getParameter(_T("filepath"));

	tstring n;
	if (pfilename)
	{
		tstring _n = pfilename->getString();
		size_t o = _n.find_last_of(_T("\\/"));
		tstring::const_iterator it = _n.cbegin();
		if (o <= _n.length())
		{
			it += (o + 1);
			while (it != _n.cend()) { n += *it; it++; }
		}
	}

	CScriptVar *ret = c->getReturnVar();
	if (ret)
		ret->setString(n);
}


// ******************************************************************************
// ******************************************************************************

bool IsScriptEmpty(const tstring &scr)
{
	CGenParser gp;

	gp.SetSourceData(scr.c_str(), scr.length());
	while (gp.NextToken())
	{
		tstring t = gp.GetCurrentTokenString();
		if ((t != _T("function")) && (t != _T("var")))
			return false;

		gp.NextLine();
	}

	return true;
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
		m.Format(_T("The installation could not continue because the file \"%s\" could not be found. Click OK to exit."), arcpath);
		MessageBox(m, _T("Archive Data Missing"), MB_OK);
		m_Thread = nullptr;
		PostQuitMessage(-1);
		AfxEndThread(-1);
		return 0;
	}

	CString msg;

	m_Progress.SetPos(0);

	theApp.m_js.addNative(_T("function AbortInstall()"), scAbortInstall, (void*)this);
	theApp.m_js.addNative(_T("function CompareStrings(str1, str2)"), scCompareStrings, (void*)this);
	theApp.m_js.addNative(_T("function CopyFile(src, dst)"), scCopyFile, (void *)this);
	theApp.m_js.addNative(_T("function CreateDirectoryTree(path)"), scCreateDirectoryTree, (void *)this);
	theApp.m_js.addNative(_T("function CreateShortcut(file, target, args, rundir, desc, showmode, icon, iconidx)"), scCreateShortcut, (void *)this);
	theApp.m_js.addNative(_T("function CreateSymbolicLink(linkname, targetname)"), scCreateSymbolicLink, (void *)this);
	theApp.m_js.addNative(_T("function DeleteFile(path)"), scDeleteFile, (void *)this);
	theApp.m_js.addNative(_T("function DownloadFile(url, file)"), scDownloadFile, (void *)this);
	theApp.m_js.addNative(_T("function Echo(msg)"), scEcho, (void *)this);
	theApp.m_js.addNative(_T("function FileExists(path)"), scFileExists, (void *)this);
	theApp.m_js.addNative(_T("function GetCurrentDateString()"), scGetCurrentDateStr, (void *)this);
	theApp.m_js.addNative(_T("function GetGlobalEnvironmentVariable(varname)"), scGetGlobalEnvironmentVariable, (void *)this);
	theApp.m_js.addNative(_T("function GetGlobalInt(name)"), scGetGlobalInt, (void *)this);
	theApp.m_js.addNative(_T("function GetExeVersion(file)"), scGetExeVersion, (void*)this);
	theApp.m_js.addNative(_T("function GetRegistryKeyValue(root, key, name)"), scGetRegistryKeyValue, (void *)this);
	theApp.m_js.addNative(_T("function GetFileNameFromPath(filepath)"), scGetFileNameFromPath, (void*)this);
	theApp.m_js.addNative(_T("function GetLicenseKey()"), scGetLicenseKey, (void *)this);
	theApp.m_js.addNative(_T("function GetLicenseOrg()"), scGetLicenseOrg, (void *)this);
	theApp.m_js.addNative(_T("function GetLicenseUser()"), scGetLicenseUser, (void *)this);
	theApp.m_js.addNative(_T("function IsDirectory(path)"), scIsDirectory, (void *)this);
	theApp.m_js.addNative(_T("function IsDirectoryEmpty(path)"), scIsDirectoryEmpty, (void *)this);
	theApp.m_js.addNative(_T("function MessageBox(title, msg)"), scMessageBox, (void *)this);
	theApp.m_js.addNative(_T("function MessageBoxYesNo(title, msg)"), scMessageBoxYesNo, (void *)this);
	theApp.m_js.addNative(_T("function RegistryKeyValueExists(root, key, name)"), scRegistryKeyValueExists, (void *)this);
	theApp.m_js.addNative(_T("function RenameFile(filename, newname)"), scRenameFile, (void *)this);
	theApp.m_js.addNative(_T("function SetGlobalEnvironmentVariable(varname, val)"), scSetGlobalEnvironmentVariable, (void *)this);
	theApp.m_js.addNative(_T("function SetGlobalInt(name, val)"), scSetGlobalInt, (void *)this);
	theApp.m_js.addNative(_T("function SetRegistryKeyValue(root, key, name, val)"), scSetRegistryKeyValue, (void *)this);
	theApp.m_js.addNative(_T("function ShowLicenseDlg()"), scShowLicenseDlg, (void *)this);
	theApp.m_js.addNative(_T("function SpawnProcess(cmd, params, rundir, block)"), scSpawnProcess, (void *)this);
	theApp.m_js.addNative(_T("function TextFileOpen(filename, mode)"), scTextFileOpen, (void *)this);
	theApp.m_js.addNative(_T("function TextFileClose(handle)"), scTextFileClose, (void *)this);
	theApp.m_js.addNative(_T("function TextFileReadLn(handle)"), scTextFileReadLn, (void *)this);
	theApp.m_js.addNative(_T("function TextFileWrite(handle, text)"), scTextFileWrite, (void *)this);
	theApp.m_js.addNative(_T("function TextFileReachedEOF(handle)"), scTextFileReachedEOF, (void *)this);

	theApp.m_InstallPath.Replace(_T("\\"), _T("/"));

	if (!theApp.m_Script[CSfxApp::EScriptType::INIT].empty())
	{
		tstring iscr;

		iscr += _T("var BASEPATH = \"");
		iscr += (LPCTSTR)(theApp.m_InstallPath);
		iscr += _T("\";  /* the base install path */\n\n");

		iscr += theApp.m_Script[CSfxApp::EScriptType::INIT];

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
					continue;
				}

				tstring fname, fpath, snippet, ffull;
				uint64_t usize;
				FILETIME created_time, modified_time;
				if (!pie->GetFileInfo(i, &fname, &fpath, NULL, &usize, &created_time, &modified_time, &snippet))
					continue;

				m_Progress.SetPos((int)i + 1);

				IExtractor::EXTRACT_RESULT er = pie->ExtractFile(i, &ffull, nullptr, theApp.m_TestOnlyMode);

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

					default:
						msg.Format(_T("    %s [failed]\r\n"), relfull);
						extract_ok = false;
						break;
				}

				m_Status.SetSel(-1, 0, FALSE);
				m_Status.ReplaceSel(msg);

				if ((er == IExtractor::ER_OK) && (theApp.m_Script[CSfxApp::EScriptType::PERFILE].empty() || snippet.empty()))
				{
					tstring pfscr;

					pfscr += _T("var BASEPATH = \"");
					pfscr += (LPCTSTR)(theApp.m_InstallPath);
					pfscr += _T("\";  /* the base install path */\n\n");

					pfscr += _T("var FILENAME = \"");
					pfscr += fname.c_str();
					pfscr += _T("\";  /* the name of the file that was just extracted */\n");

					pfscr += _T("var PATH = \"");
					pfscr += fpath.c_str();
					pfscr += _T("\";  /* the output path of that file */\n");

					pfscr += _T("var FILEPATH = \"");
					pfscr += ffull.c_str();
					pfscr += _T("\";  /* the full filename (path + name) */\n\n");

					pfscr += theApp.m_Script[CSfxApp::EScriptType::PERFILE];

					pfscr += _T("\n\n");
					pfscr += snippet;

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

	if (!theApp.m_Script[CSfxApp::EScriptType::FINISH].empty())
	{
		tstring fscr;

		fscr += _T("var BASEPATH = \"");
		fscr += (LPCTSTR)(theApp.m_InstallPath);
		fscr += _T("\";  /* the base install path */\n\n");

		fscr += theApp.m_Script[CSfxApp::EScriptType::FINISH];

		if (!IsScriptEmpty(fscr))
			theApp.m_js.execute(fscr);
	}

	msg.Format(_T("Done.\r\n"));
	m_Status.SetSel(-1, 0, FALSE);
	m_Status.ReplaceSel(msg);

	CWnd *pok = GetDlgItem(IDOK);
	CWnd *pcancel = GetDlgItem(IDCANCEL);

	if (!extract_ok)
	{
		MessageBox(_T("One or more files in your archive could not be properly extracted.\r\nThis may be due to your user or directory permissions or disk space."), _T("Extraction Failure"), MB_OK);
		m_Thread = nullptr;
	}

	if (pok)
	{
		if (!extract_ok)
			pok->SetWindowText(_T("Close"));

		pok->EnableWindow(extract_ok || !cancelled);
	}

	if (pcancel)
	{
		if (!extract_ok)
			pcancel->ShowWindow(SW_HIDE);
	
		pcancel->EnableWindow(cancelled);

		if (cancelled)
			pcancel->SetWindowText(_T("<< Back"));
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
