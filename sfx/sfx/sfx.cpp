/*
	Copyright ©2016. Authored by Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
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


// CSfxApp initialization

BOOL CSfxApp::InitInstance()
{
	CWinApp::InitInstance();

#if 0
	MessageBox(NULL, _T("Attach debugger now!"), _T("Debug"), MB_OK);
#endif

	m_TestOnlyMode = false;

	registerFunctions(&m_js);
	registerMathFunctions(&m_js);

	// Create the shell manager, in case the dialog contains
	// any shell tree view or shell list view controls.
	CShellManager *pShellManager = new CShellManager;

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

	TCHAR *script_res_name[EScriptType::NUMTYPES] = {_T("SFX_SCRIPT_INIT"), _T("SFX_SCRIPT_PERFILE"), _T("SFX_SCRIPT_FINISH")};
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

	if (_tcsstr(m_lpCmdLine, _T("-testonly")) != nullptr)
	{
		m_TestOnlyMode = true;
		m_Caption += _T(" (TEST ONLY)");
	}

	bool runnow = false;
	if (!m_TestOnlyMode && PathIsDirectory(m_lpCmdLine))
	{
		m_InstallPath = m_lpCmdLine;
		runnow = true;
	}

	if (m_InstallPath.IsEmpty())
		m_InstallPath = _T(".\\");
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

	UINT dt = runnow ? DT_PROGRESS : DT_FIRST;

	while (dt != DT_QUIT)
	{
		CDialog *dlg = CreateSfxDialog((ESfxDlgType)dt);
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
		dlg = NULL;
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

