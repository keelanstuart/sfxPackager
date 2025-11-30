/*
	Copyright © 2013-2025, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
	
	For inquiries, contact: keelanstuart@gmail.com
*/


// sfx.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "sfx.h"

#include "sfxDlg.h"
#include "WelcomeDlg.h"
#include "ProgressDlg.h"
#include "FinishDlg.h"

#include "../sfxFlags.h"

#include "TinyJS_Functions.h"
#include "TinyJS_MathFunctions.h"
#include "TinyJS_SfxFunctions.h"

#include <fcntl.h>
#include <io.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


extern bool ReplaceEnvironmentVariables(const tstring &src, tstring &dst);
extern bool ReplaceRegistryKeys(const tstring &src, tstring &dst);


// CSfxApp

BEGIN_MESSAGE_MAP(CSfxApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CSfxApp construction

CSfxApp::CSfxApp()
{
	m_Flags = 0;
	m_LicenseDlg = nullptr;
	m_LicenseAcceptanceDlg = nullptr;
	m_TestOnlyMode = false;
	m_bRunAutomated = false;
	m_bEmbedded = false;
}


// The one and only CSfxApp object

CSfxApp theApp;

enum ESfxDlgType
{
	DT_QUIT = 0,

	DT_FIRST,
	DT_WELCOME = DT_FIRST,
	DT_SETTINGS,
	DT_PROGRESS,
	DT_FINISH,
	DT_LAST = DT_FINISH,

	DT_NUMTYPES
};

CDialog *CreateSfxDialog(ESfxDlgType dt)
{
	switch (dt)
	{
		case DT_WELCOME:
			return new CWelcomeDlg();
			break;

		case DT_SETTINGS:
			return new CSfxDlg();
			break;

		case DT_PROGRESS:
			return new CProgressDlg();
			break;

		case DT_FINISH:
			return new CFinishDlg();
			break;
	}

	return NULL;
}

void RemoveQuitMessage(HWND hwnd)
{
	MSG tmp;
	PeekMessage(&tmp, hwnd, 0, 0, PM_NOREMOVE);
	if (tmp.message == WM_QUIT)
		PeekMessage(&tmp, hwnd, 0, 0, PM_REMOVE);
}

BOOL CSfxApp::ProcessMessageFilter(int code, LPMSG lpMsg)
{
	// Check to see if the modal dialog box is up
	if (m_pActiveWnd != NULL)
	{
		if (lpMsg->hwnd == m_pActiveWnd->GetSafeHwnd() || ::IsChild(m_pActiveWnd->GetSafeHwnd(), lpMsg->hwnd))
		{
			// Use the global IsChild() function to get
			// messages destined for the dialog's controls
			// Perform customized message processing here
		}
	}

	return CWinApp::ProcessMessageFilter(code, lpMsg);
}

class CSfxPackageCommandLineInfo : public CCommandLineInfo
{
	bool path_last;
public:
	CSfxPackageCommandLineInfo()
	{
		path_last = false;
	}

	// plain char* version on UNICODE for source-code backwards compatibility
	virtual void ParseParam(const TCHAR* pszParam, BOOL bFlag, BOOL bLast)
	{
		bool handled = false;

		if (bFlag)
		{
			if (!_tcsicmp(pszParam, _T("auto")))
			{
				theApp.m_bRunAutomated = true;
				handled = true;
			}
			else if (!_tcsicmp(pszParam, _T("testonly")))
			{
				theApp.m_TestOnlyMode = true;
				handled = true;
			}
			else if (!_tcsicmp(pszParam, _T("embed")))
			{
				theApp.m_bEmbedded = true;
				handled = true;
			}
			else if (!_tcsicmp(pszParam, _T("path")))
			{
				path_last = true;
				handled = true;
			}
		}
		else
		{
			if (path_last)
			{
				path_last = false;
				theApp.m_InstallPath = pszParam;
			}
		}

		if (!handled)
			CCommandLineInfo::ParseParam(pszParam, bFlag, bLast);
	}

#ifdef _UNICODE
	// convert the string and pass it back into the standard handler
	virtual void ParseParam(const char* pszParam, BOOL bFlag, BOOL bLast)
	{
		TCHAR *param;
		LOCAL_MBCS2TCS(pszParam, param);

		ParseParam(param, bFlag, bLast);
	}
#endif

};

// CSfxApp initialization

BOOL CSfxApp::InitInstance()
{
	CWinApp::InitInstance();

#if 0
	MessageBox(NULL, _T("Attach debugger now!"), _T("Debug"), MB_OK);
#endif

	m_Props = props::IPropertySet::CreatePropertySet();

	registerFunctions(&m_js);
	registerMathFunctions(&m_js);

	theApp.m_js.addNative(_T("function AbortInstall()"), scAbortInstall, (void*)this);
	theApp.m_js.addNative(_T("function CompareStrings(str1, str2)"), scCompareStrings, (void*)this);
	theApp.m_js.addNative(_T("function CopyFile(src, dst)"), scCopyFile, (void *)this);
	theApp.m_js.addNative(_T("function CreateDirectoryTree(path)"), scCreateDirectoryTree, (void *)this);
	theApp.m_js.addNative(_T("function CreateShortcut(file, target, args, rundir, desc, showmode, icon, iconidx)"), scCreateShortcut, (void *)this);
	theApp.m_js.addNative(_T("function CreateSymbolicLink(linkname, targetname)"), scCreateSymbolicLink, (void *)this);
	theApp.m_js.addNative(_T("function DeleteFile(path)"), scDeleteFile, (void *)this);
	theApp.m_js.addNative(_T("function DeleteRegistryKey(root, key, subkey)"), scDeleteRegistryKey, (void *)this);
	theApp.m_js.addNative(_T("function DownloadFile(url, file)"), scDownloadFile, (void *)this);
	theApp.m_js.addNative(_T("function Echo(msg)"), scEcho, (void *)this);
	theApp.m_js.addNative(_T("function FileExists(path)"), scFileExists, (void *)this);
	theApp.m_js.addNative(_T("function FilenameHasExtension(filename, ext)"), scFilenameHasExtension, (void *)this);
	theApp.m_js.addNative(_T("function GetCurrentDateString()"), scGetCurrentDateStr, (void *)this);
	theApp.m_js.addNative(_T("function GetGlobalEnvironmentVariable(varname)"), scGetGlobalEnvironmentVariable, (void *)this);
	theApp.m_js.addNative(_T("function GetGlobalInt(name)"), scGetGlobalInt, (void *)this);
	theApp.m_js.addNative(_T("function GetExeVersion(file)"), scGetExeVersion, (void*)this);
	theApp.m_js.addNative(_T("function GetProperty(name)"), scGetProperty, (void *)this);
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
	theApp.m_js.addNative(_T("function SetProperty(name, type, aspect, value)"), scSetProperty, (void *)this);
	theApp.m_js.addNative(_T("function SetRegistryKeyValue(root, key, name, val)"), scSetRegistryKeyValue, (void *)this);
	theApp.m_js.addNative(_T("function ShowLicenseAcceptanceDlg()"), scShowLicenseAcceptanceDlg, (void *)this);
	theApp.m_js.addNative(_T("function ShowLicenseDlg()"), scShowLicenseDlg, (void *)this);
	theApp.m_js.addNative(_T("function SpawnProcess(cmd, params, rundir, block)"), scSpawnProcess, (void *)this);
	theApp.m_js.addNative(_T("function TextFileOpen(filename, mode)"), scTextFileOpen, (void *)this);
	theApp.m_js.addNative(_T("function TextFileClose(handle)"), scTextFileClose, (void *)this);
	theApp.m_js.addNative(_T("function TextFileReadLn(handle)"), scTextFileReadLn, (void *)this);
	theApp.m_js.addNative(_T("function TextFileWrite(handle, text)"), scTextFileWrite, (void *)this);
	theApp.m_js.addNative(_T("function TextFileReachedEOF(handle)"), scTextFileReachedEOF, (void *)this);
	theApp.m_js.addNative(_T("function ToLower(str)"), scToLower, (void *)this);
	theApp.m_js.addNative(_T("function ToUpper(str)"), scToUpper, (void *)this);

	TCHAR *pCaption = _T("Self-extracting Archive");
	HRSRC hfr = FindResource(NULL, _T("SFX_CAPTION"), _T("SFX"));
	if (hfr)
	{
		HGLOBAL hg = LoadResource(NULL, hfr);
		if (hg)
		{
			pCaption = (TCHAR *)LockResource(hg);
		}
	}
	m_Caption = pCaption;

	// Create the shell manager, in case the dialog contains
	// any shell tree view or shell list view controls.
	CShellManager *pShellManager = new CShellManager;

	CSfxPackageCommandLineInfo cmdinfo;
	ParseCommandLine(cmdinfo);

	if (m_bEmbedded)
	{
		m_bRunAutomated = true;

		// attach if possible, alloc if attach fails.
		if (AttachConsole( ATTACH_PARENT_PROCESS))
		{
			_tfreopen(_T("CONIN$"),  _T("r"), stdin);
			_tfreopen(_T("CONOUT$"), _T("w"), stdout);
			_tfreopen(_T("CONOUT$"), _T("w"), stderr);

#if defined(UNICODE)
			_setmode(_fileno(stdout), _O_U8TEXT);
			_setmode(_fileno(stderr), _O_U8TEXT);
#endif
			SetConsoleOutputCP(CP_UTF8);
			SetConsoleCP(CP_UTF8);
		}
		else
		{
			AllocConsole();
		}

		_tprintf(_T("\nInstalling \"%s\" ...\n\n"), (LPCTSTR)m_Caption);
	}

	TCHAR *pDefaultPath = _T("");
	hfr = FindResource(NULL, _T("SFX_DEFAULTPATH"), _T("SFX"));
	if (hfr)
	{
		HGLOBAL hg = LoadResource(NULL, hfr);
		if (hg)
		{
			pDefaultPath = (TCHAR *)LockResource(hg);
		}
	}
	m_InstallPath = pDefaultPath;

	TCHAR *script_res_name[EScriptType::NUMTYPES] = {_T("SFX_SCRIPT_INITIALIZE"), _T("SFX_SCRIPT_PREINSTALL"), _T("SFX_SCRIPT_PREFILE"), _T("SFX_SCRIPT_POSTFILE"), _T("SFX_SCRIPT_POSTINSTALL")};
	for (int si = 0, max_si = EScriptType::NUMTYPES; si < max_si; si++)
	{
		TCHAR *pscript = _T("");
		hfr = FindResource(NULL, script_res_name[si], _T("SFX"));
		if (hfr)
		{
			HGLOBAL hg = LoadResource(NULL, hfr);
			if (hg)
			{
				pscript = (TCHAR *)LockResource(hg);
			}
		}

		m_Script[si] = pscript;
	}

	if (m_TestOnlyMode)
	{
		m_Caption += _T(" (TEST ONLY)");
	}

	if (!theApp.m_Script[CSfxApp::EScriptType::INITIALIZE].empty())
	{
		if (!IsScriptEmpty(theApp.m_Script[CSfxApp::EScriptType::INITIALIZE].c_str()))
			theApp.m_js.execute(theApp.m_Script[CSfxApp::EScriptType::INITIALIZE].c_str());
	}

	if (m_InstallPath.IsEmpty())
	{
		m_InstallPath = _T(".\\");
	}
	else
	{
		tstring tmp = m_InstallPath, _tmp;
		ReplaceEnvironmentVariables(tmp, _tmp);
		ReplaceRegistryKeys(_tmp, tmp);
		m_InstallPath = tmp.c_str();
	}

	SFixupResourceData *furd = NULL;
	hfr = FindResource(NULL, _T("SFX_FIXUPDATA"), _T("SFX"));
	if (hfr)
	{
		HGLOBAL hg = LoadResource(NULL, hfr);
		if (hg)
		{
			furd = (SFixupResourceData *)LockResource(hg);
		}
	}

	m_RunCommand = furd ? furd-> m_LaunchCmd : _T("");
	m_Flags = furd ? furd->m_Flags : SFX_FLAG_ALLOWDESTCHG;
	m_VersionID = furd ? furd->m_VersionID : _T("");

	if (m_Flags & SFX_FLAG_ADMINONLY)
	{
		bool has_privs = false;

		HANDLE htoken;
		if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &htoken))
		{
			DWORD infolen;
			TOKEN_ELEVATION elevation;
			GetTokenInformation(htoken, TokenElevation, &elevation, sizeof(TOKEN_ELEVATION), &infolen);
			has_privs = (elevation.TokenIsElevated != 0);

			CloseHandle(htoken);
		}

		if (!has_privs)
		{
			if (MessageBox(NULL, _T("This installation requires administrative privileges.\r\nWould you like to elevate permissions?"), _T("UAC Override Required"), MB_YESNO) == IDYES)
			{
				SHELLEXECUTEINFO sei;
				ZeroMemory(&sei, sizeof(SHELLEXECUTEINFO));

				TCHAR exepath[MAX_PATH];
				_tcscpy_s(exepath, MAX_PATH, theApp.m_pszHelpFilePath);
				PathRenameExtension(exepath, _T(".exe"));

				HINSTANCE shellret = ShellExecute(NULL, _T("runas"), exepath, NULL, NULL, SW_SHOWNORMAL);

				if ((size_t)shellret <= 32)
				{
					MessageBox(NULL, _T("Permission elevation was not successful - Installation aborted."), _T("UAC Override Failure"), MB_OK);
				}

				ExitProcess(-1);
			}
		}
	}

	m_SpaceRequired.QuadPart = furd ? furd->m_SpaceRequired.QuadPart : 0;
	m_CompressedFileCount = furd ? furd->m_CompressedFileCount : 0;

	UINT dt = m_bRunAutomated ? DT_PROGRESS : DT_FIRST;

	CDialog *dlg = nullptr;
	while (dt != DT_QUIT)
	{
		dlg = CreateSfxDialog((ESfxDlgType)dt);
		if (!dlg)
			break;

		m_pMainWnd = dlg;

		HWND hwnd = dlg->GetSafeHwnd();

#ifdef _DEBUG
		// Crash if we don't reset this... not sure when / why this changed (update to VC?), but it definitely did!
		_AFX_THREAD_STATE* pState = AfxGetThreadState();
		pState->m_nDisablePumpCount = 0;
#endif

		INT_PTR nResponse = dlg->DoModal();

		if (nResponse == IDOK)
		{
			dt++;

			// if we don't allow the destination to be changed, don't show the settings dialog
			// this will likely change... but it's unknown how just yet. more settings are needed.
			if ((dt == DT_SETTINGS) && !(m_Flags & SFX_FLAG_ALLOWDESTCHG))
				dt++;

			if (dt > DT_LAST)
				break;
		}
		else if (nResponse == IDCANCEL)
		{
			dt--;

			// if we don't allow the destination to be changed, don't show the settings dialog
			// this will likely change... but it's unknown how just yet. more settings are needed.
			if ((dt == DT_SETTINGS) && !(m_Flags & SFX_FLAG_ALLOWDESTCHG))
				dt--;
		}

		RemoveQuitMessage(hwnd);

#if defined (DEBUG)
		pState->m_nDisablePumpCount = 0;
#endif

		m_pMainWnd = nullptr;

		delete dlg;
		dlg = nullptr;
	}

	if (dlg)
	{
		delete dlg;
		dlg = nullptr;
	}

	// Delete the shell manager created above.
	if (pShellManager != NULL)
	{
		delete pShellManager;
	}

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}

void CSfxApp::CaptureWindowPos(CWnd *pw)
{
	if (!pw || !pw->GetSafeHwnd())
		return;

	CRect r;
	pw->GetWindowRect(r);

	if (!m_Rect.has_value())
		m_Rect = std::make_optional(r);
	else
		*m_Rect = r;
}

void CSfxApp::ApplyWindowPos(CWnd *pw)
{
	if (!pw || !pw->GetSafeHwnd() || !m_Rect.has_value())
		return;

	pw->MoveWindow(*m_Rect);
}

BOOL CSfxApp::ExitInstance()
{
	if (m_Props)
	{
		m_Props->Release();
		m_Props = nullptr;
	}

	if (m_LicenseDlg)
	{
		delete m_LicenseDlg;
		m_LicenseDlg = nullptr;
	}

	if (m_LicenseAcceptanceDlg)
	{
		delete m_LicenseAcceptanceDlg;
		m_LicenseAcceptanceDlg = nullptr;
	}

	return TRUE;
}

void CSfxApp::Echo(const TCHAR *msg)
{
	if (m_pMainWnd)
	{
		CProgressDlg *ppd = dynamic_cast<CProgressDlg *>(m_pMainWnd);
		if (ppd)
			ppd->Echo(msg);
	}

	if (theApp.m_bEmbedded)
		_tprintf(msg);
}

bool IsScriptEmpty(const tstring &scr)
{
	genio::IParserT *gp = (genio::IParserT *)genio::IParser::Create(genio::IParser::CHAR_MODE::CM_TCHAR);

	bool ret = true;

	gp->SetSourceData(scr.c_str(), scr.length());
	while (gp->NextToken())
	{
		// not a function or variable declaration but still an identifier (like a function call or assignment, but not a comment)
		if (!gp->IsToken(_T("function")) && !gp->IsToken(_T("var")) && (gp->GetCurrentTokenType() == genio::IParser::TOKEN_TYPE::TT_IDENT))
		{
			ret = false;
			break;
		}

		gp->NextLine();
	}

	gp->Release();

	return ret;
}

