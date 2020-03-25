/*
	Copyright © 2013-2020, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
	
	For inquiries, contact: keelanstuart@gmail.com
*/


// sfxPackager.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "afxwinappex.h"
#include "afxdialogex.h"
#include "sfxPackager.h"
#include "MainFrm.h"

#include "ChildFrm.h"
#include "sfxPackagerDoc.h"
#include "sfxPackagerView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CSfxPackagerApp

BEGIN_MESSAGE_MAP(CSfxPackagerApp, CWinAppEx)
	ON_COMMAND(ID_APP_ABOUT, &CSfxPackagerApp::OnAppAbout)
	// Standard file based document commands
	ON_COMMAND(ID_FILE_NEW, &CWinAppEx::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, &CWinAppEx::OnFileOpen)
END_MESSAGE_MAP()


// CSfxPackagerApp construction

CSfxPackagerApp::CSfxPackagerApp()
{
	m_bHiColorIcons = TRUE;

	// recommended format for string is CompanyName.ProductName.SubProduct.VersionInformation
	SetAppID(_T("sfxPackager.sfxPackager.1.1.0.1"));
}

// The one and only CSfxPackagerApp object

CSfxPackagerApp theApp;


// CSfxPackagerApp initialization

BOOL CSfxPackagerApp::InitInstance()
{
	// InitCommonControlsEx() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// Set this to include all the common control classes you want to use
	// in your application.
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinAppEx::InitInstance();


	// Initialize OLE libraries
	if (!AfxOleInit())
	{
		AfxMessageBox(IDP_OLE_INIT_FAILED);
		return FALSE;
	}

	AfxEnableControlContainer();

	EnableTaskbarInteraction();

	// AfxInitRichEdit2() is required to use RichEdit control	
	AfxInitRichEdit2();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need
	// Change the registry key under which our settings are stored
	SetRegistryKey(_T("sfxPackager"));
	LoadStdProfileSettings(4);  // Load standard INI file options (including MRU)

	{
		TCHAR path7z[MAX_PATH] = {0};
		HKEY key;
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\7-Zip"), 0, KEY_READ, &key) == ERROR_SUCCESS)
		{
			DWORD rkt, rksz;
			rksz = sizeof(path7z);
			RegQueryValueEx(key, _T("Path"), 0, &rkt, (BYTE *)path7z, &rksz);

			RegCloseKey(key);
		}
		_tcscat_s(path7z, MAX_PATH, _T("7z.exe"));

		m_s7ZipPath = GetProfileString(_T("sfxPackager"), _T("7ZipPath"), path7z);
	}

	{
		TCHAR workpath[MAX_PATH] = {0};
		TCHAR *rootpath;
		HRESULT hr = SHGetKnownFolderPath(FOLDERID_ProgramData, 0, NULL, &rootpath);
		if (SUCCEEDED(hr))
		{
			_tcscat_s(workpath, MAX_PATH, rootpath);
			PathAddBackslash(workpath);
			CoTaskMemFree(rootpath);

			_tcscat_s(workpath, MAX_PATH, _T("sfxPackager"));
			if (!PathIsDirectory(workpath) && !PathIsDirectory(workpath))
				if (!CreateDirectory(workpath, nullptr))
					return false;

			PathAddBackslash(workpath);
			_tcscat_s(workpath, MAX_PATH, _T("WorkArea"));
			if (!PathIsDirectory(workpath) && !PathIsDirectory(workpath))
				if (!CreateDirectory(workpath, nullptr))
					return false;

			if (!PathIsDirectory(workpath) && !PathIsDirectory(workpath))
				return false;

		}

		m_sTempPath = GetProfileString(_T("sfxPackager"), _T("TempPath"), workpath);
	}

	InitContextMenuManager();

	InitKeyboardManager();

	InitTooltipManager();
	CMFCToolTipInfo ttParams;
	ttParams.m_bVislManagerTheme = TRUE;
	theApp.GetTooltipManager()->SetTooltipParams(AFX_TOOLTIP_TYPE_ALL,
		RUNTIME_CLASS(CMFCToolTipCtrl), &ttParams);

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views
	CMultiDocTemplate* pDocTemplate = new CMultiDocTemplate(IDR_sfxPackagerTYPE,
		RUNTIME_CLASS(CSfxPackagerDoc),
		RUNTIME_CLASS(CChildFrame), // custom MDI child frame
		RUNTIME_CLASS(CSfxPackagerView));

	if (!pDocTemplate)
		return FALSE;

	AddDocTemplate(pDocTemplate);

	// create main MDI Frame window
	CMainFrame* pMainFrame = new CMainFrame;
	if (!pMainFrame || !pMainFrame->LoadFrame(IDR_MAINFRAME))
	{
		if (!pMainFrame)
			delete pMainFrame;

		return FALSE;
	}
	m_pMainWnd = pMainFrame;
	// call DragAcceptFiles only if there's a suffix
	//  In an MDI app, this should occur immediately after setting m_pMainWnd

	// Parse command line for standard shell commands, DDE, file open
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	if (cmdInfo.m_nShellCommand == cmdInfo.FileNew)
		cmdInfo.m_nShellCommand = cmdInfo.FileNothing;

	// Dispatch commands specified on the command line.  Will return FALSE if
	// app was launched with /RegServer, /Register, /Unregserver or /Unregister.
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;
	// The main window has been initialized, so show and update it
	pMainFrame->ShowWindow(m_nCmdShow);
	pMainFrame->UpdateWindow();

	return TRUE;
}

int CSfxPackagerApp::ExitInstance()
{
	//TODO: handle additional resources you may have added
	AfxOleTerm(FALSE);

	WriteProfileString(_T("sfxPackager"), _T("7ZipPath"), m_s7ZipPath);
	WriteProfileString(_T("sfxPackager"), _T("TempPath"), m_sTempPath);

	return CWinAppEx::ExitInstance();
}

// CSfxPackagerApp message handlers


// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

// App command to run the dialog
void CSfxPackagerApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}

// CSfxPackagerApp customization load/save methods

void CSfxPackagerApp::PreLoadState()
{
	BOOL bNameValid;
	CString strName;
	bNameValid = strName.LoadString(IDS_EDIT_MENU);
	ASSERT(bNameValid);
	GetContextMenuManager()->AddMenu(strName, IDR_POPUP_EDIT);
	bNameValid = strName.LoadString(IDS_EXPLORER);
	ASSERT(bNameValid);
	GetContextMenuManager()->AddMenu(strName, IDR_POPUP_EXPLORER);
}

void CSfxPackagerApp::LoadCustomState()
{
}

void CSfxPackagerApp::SaveCustomState()
{
}

// CSfxPackagerApp message handlers



