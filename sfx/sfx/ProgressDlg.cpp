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
		m_Status.GetWindowRect(r);
		r.left += wd;
		ScreenToClient(r);
		m_Status.MoveWindow(r, FALSE);
	}

	m_Status.SetLimitText(0);
	m_Status.SetReadOnly(TRUE);
	m_Status.EnableWindow(TRUE);

	m_Thread = CreateThread(NULL, 0, InstallThreadProc, this, 0, NULL);


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

#if 1
		while (AfxPumpMessage())
		{

		}
#else
		while ((bRet = GetMessage(&msg, NULL, 0, 0)) != FALSE)
		{
			if (bRet != -1)
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
#endif

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
	int val = c->getParameter(_T("val"))->getInt();

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

	int showmode = c->getParameter(_T("showmode"))->getInt();

	tstring icon = c->getParameter(_T("icon"))->getString(), _icon;
	ReplaceEnvironmentVariables(icon, _icon);
	ReplaceRegistryKeys(_icon, icon);

	int iconidx = c->getParameter(_T("iconidx"))->getInt();

	if (!theApp.m_TestOnlyMode)
	{
		CreateShortcut(targ.c_str(), args.c_str(), file.c_str(), desc.c_str(),
					   showmode, rundir.c_str(), icon.c_str(), iconidx);
	}
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

	STARTUPINFO si = { 0 };
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi;

	BOOL created = true;
	if (!theApp.m_TestOnlyMode)
	{
		created = CreateProcess(cmd.c_str(), (TCHAR *)(params.c_str()), NULL, NULL, FALSE, NULL, NULL, rundir.c_str(), &si, &pi);
		if (created && block)
			WaitForSingleObject(pi.hProcess, INFINITE);
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




// ******************************************************************************
// ******************************************************************************

DWORD CProgressDlg::RunInstall()
{
	WaitForSingleObject(m_mutexInstallStart, INFINITE);

	DWORD ret = 0;

	TCHAR exepath[MAX_PATH];
	_tcscpy_s(exepath, MAX_PATH, theApp.m_pszHelpFilePath);
	PathRenameExtension(exepath, _T(".exe"));

	CString msg;

	m_Progress.SetPos(0);

	theApp.m_js.addNative(_T("function AbortInstall()"), scAbortInstall, (void*)this);
	theApp.m_js.addNative(_T("function CompareStrings(str1, str2)"), scCompareStrings, (void*)this);
	theApp.m_js.addNative(_T("function CopyFile(src, dst)"), scCopyFile, (void *)this);
	theApp.m_js.addNative(_T("function CreateDirectoryTree(path)"), scCreateDirectoryTree, (void *)this);
	theApp.m_js.addNative(_T("function CreateShortcut(file, target, args, rundir, desc, showmode, icon, iconidx)"), scCreateShortcut, (void *)this);
	theApp.m_js.addNative(_T("function DeleteFile(path)"), scDeleteFile, (void *)this);
	theApp.m_js.addNative(_T("function Echo(msg)"), scEcho, (void *)this);
	theApp.m_js.addNative(_T("function FileExists(path)"), scFileExists, (void *)this);
	theApp.m_js.addNative(_T("function GetGlobalInt(name)"), scGetGlobalInt, (void *)this);
	theApp.m_js.addNative(_T("function GetExeVersion(file)"), scGetExeVersion, (void*)this);
	theApp.m_js.addNative(_T("function IsDirectory(path)"), scIsDirectory, (void *)this);
	theApp.m_js.addNative(_T("function IsDirectoryEmpty(path)"), scIsDirectoryEmpty, (void *)this);
	theApp.m_js.addNative(_T("function MessageBox(title, msg)"), scMessageBox, (void *)this);
	theApp.m_js.addNative(_T("function MessageBoxYesNo(title, msg)"), scMessageBoxYesNo, (void *)this);
	theApp.m_js.addNative(_T("function RegistryKeyValueExists(root, key, name)"), scRegistryKeyValueExists, (void *)this);
	theApp.m_js.addNative(_T("function RenameFile(filename, newname)"), scRenameFile, (void *)this);
	theApp.m_js.addNative(_T("function SetGlobalEnvironmentVariable(varname, val)"), scSetGlobalEnvironmentVariable, (void *)this);
	theApp.m_js.addNative(_T("function SetGlobalInt(name, val)"), scSetGlobalInt, (void *)this);
	theApp.m_js.addNative(_T("function SetRegistryKeyValue(root, key, name, val)"), scSetRegistryKeyValue, (void *)this);
	theApp.m_js.addNative(_T("function SpawnProcess(cmd, params, rundir, block)"), scSpawnProcess, (void *)this);

	theApp.m_InstallPath.Replace(_T("\\"), _T("/"));

	if (!theApp.m_Script[CSfxApp::EScriptType::INIT].empty())
	{
		tstring iscr;

		iscr += _T("var BASEPATH = \"");
		iscr += (LPCTSTR)(theApp.m_InstallPath);
		iscr += _T("\";  // the base install path\n\n");

		iscr += theApp.m_Script[CSfxApp::EScriptType::INIT];

		theApp.m_js.execute(iscr);
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

				tstring fname, fpath, snippet, ffull;
				uint64_t usize;
				FILETIME created_time, modified_time;
				pie->GetFileInfo(i, &fname, &fpath, NULL, &usize, &created_time, &modified_time, &snippet);

				m_Progress.SetPos((int)i + 1);

				IExtractor::EXTRACT_RESULT er = pie->ExtractFile(i, &ffull, nullptr, theApp.m_TestOnlyMode);

				for (tstring::iterator rit = ffull.begin(), last_rit = ffull.end(); rit != last_rit; rit++)
				{
					if (*rit == _T('\\'))
						*rit = _T('/');
				}

				for (tstring::iterator rit = fpath.begin(), last_rit = fpath.end(); rit != last_rit; rit++)
				{
					if (*rit == _T('\\'))
						*rit = _T('/');
				}

				switch (er)
				{
					case IExtractor::ER_OK:
					{
						if (!theApp.m_Script[CSfxApp::EScriptType::PERFILE].empty())
						{
							tstring pfscr;

							pfscr += _T("var BASEPATH = \"");
							pfscr += (LPCTSTR)(theApp.m_InstallPath);
							pfscr += _T("\";  // the base install path\n\n");

							pfscr += _T("var FILENAME = \"");
							pfscr += fname.c_str();
							pfscr += _T("\";  // the name of the file that was just extracted\n");

							pfscr += _T("var PATH = \"");
							pfscr += fpath.c_str();
							pfscr += _T("\";  // the output path of that file\n");

							pfscr += _T("var FILEPATH = \"");
							pfscr += ffull.c_str();
							pfscr += _T("\";  // the full filename (path + name)\n\n");

							pfscr += theApp.m_Script[CSfxApp::EScriptType::PERFILE];

							pfscr += _T("\n\n");
							pfscr += snippet;

							theApp.m_js.execute(pfscr);
						}

						msg.Format(_T("    %s (%" PRId64 "KB) [ok]\r\n"), ffull.c_str(), usize / 1024);
						break;
					}

					default:
						msg.Format(_T("    %s [failed]\r\n"), ffull.c_str());
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
		tstring fscr;

		fscr += _T("var BASEPATH = \"");
		fscr += (LPCTSTR)(theApp.m_InstallPath);
		fscr += _T("\";  // the base install path\n\n");

		fscr += theApp.m_Script[CSfxApp::EScriptType::FINISH];

		theApp.m_js.execute(fscr);
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
