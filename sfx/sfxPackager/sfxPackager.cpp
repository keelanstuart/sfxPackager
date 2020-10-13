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

#include <fcntl.h>
#include <io.h>


#ifdef _DEBUG
#define new DEBUG_NEW
#endif

extern bool ReplaceEnvironmentVariables(const tstring &src, tstring &dst);
extern bool ReplaceRegistryKeys(const tstring &src, tstring &dst);
extern bool FLZACreateDirectories(const TCHAR *dir);

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

	m_Props = props::IPropertySet::CreatePropertySet();

	// recommended format for string is CompanyName.ProductName.SubProduct.VersionInformation
	SetAppID(_T("sfxPackager.3.7.0.0"));
}

CSfxPackagerApp::~CSfxPackagerApp()
{
	m_Props->Release();
	m_Props = nullptr;
}

// The one and only CSfxPackagerApp object

CSfxPackagerApp theApp;


class CSfxPackagerCommandLineInfo : public CCommandLineInfo
{
public:
	CSfxPackagerCommandLineInfo()
	{
		m_bBuild = false;
	}

	// plain char* version on UNICODE for source-code backwards compatibility
	virtual void ParseParam(const TCHAR* pszParam, BOOL bFlag, BOOL bLast)
	{
		if (bFlag && !_tcsicmp(pszParam, _T("b")))
		{
			m_bBuild = true;
			m_bRunAutomated = true;
			m_bRunEmbedded = true;
		}
		else
			CCommandLineInfo::ParseParam(pszParam, bFlag, bLast);
	}
#ifdef _UNICODE
	virtual void ParseParam(const char* pszParam, BOOL bFlag, BOOL bLast)
	{
		if (bFlag && !_stricmp(pszParam, "b"))
		{
			m_bBuild = true;
			m_bRunAutomated = true;
			m_bRunEmbedded = true;
		}
		else
			CCommandLineInfo::ParseParam(pszParam, bFlag, bLast);
	}
#endif

	bool m_bBuild;
};

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

	m_bDeferShowOnFirstWindowPlacementLoad = true;

	// call DragAcceptFiles only if there's a suffix
	//  In an MDI app, this should occur immediately after setting m_pMainWnd

	// Parse command line for standard shell commands, DDE, file open
	CSfxPackagerCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	if (cmdInfo.m_nShellCommand == cmdInfo.FileNew)
		cmdInfo.m_nShellCommand = cmdInfo.FileNothing;

	if (cmdInfo.m_bBuild)
		m_AutomatedBuild = true;

	if (m_AutomatedBuild)
		m_nCmdShow = SW_HIDE;

	// create main MDI Frame window
	CMainFrame* pMainFrame = new CMainFrame;
	if (!pMainFrame || !pMainFrame->LoadFrame(IDR_MAINFRAME))
	{
		if (!pMainFrame)
			delete pMainFrame;

		return FALSE;
	}
	m_pMainWnd = pMainFrame;

	// Dispatch commands specified on the command line.  Will return FALSE if
	// app was launched with /RegServer, /Register, /Unregserver or /Unregister.
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;

	// fool the windows shell system into behaving like a console application...
	// write output to a console (preferrably to the parent process' console) and hide the main frame
	if (m_AutomatedBuild)
	{
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
		_tprintf(_T("\nsfxPackager, a light-weight install package creation utility for Windows.\nCopyright (c) 2013-2020, Keelan Stuart. All rights reserved.\n\n"));

		// start the package creation process for all 
		POSITION pdtp = m_pDocManager->GetFirstDocTemplatePosition();
		while (pdtp)
		{
			CDocTemplate *pdt = m_pDocManager->GetNextDocTemplate(pdtp);

			POSITION pdp = pdt->GetFirstDocPosition();
			while (pdp)
			{
				CDocument *pd = pdt->GetNextDoc(pdp);

				CSfxPackagerDoc *psfxdoc = dynamic_cast<CSfxPackagerDoc *>(pd);
				if (psfxdoc)
				{
					POSITION pvp = psfxdoc->GetFirstViewPosition();
					CSfxPackagerView *pv = (CSfxPackagerView *)psfxdoc->GetNextView(pvp);
					psfxdoc->RunCreateSFXPackage(pv);
				}
			}
		}

		return FALSE;
	}
	else
	{
		// The main window has been initialized, so show and update it
		pMainFrame->ShowWindow(m_nCmdShow);
		pMainFrame->UpdateWindow();
	}

	return TRUE;
}

int CSfxPackagerApp::ExitInstance()
{
	// Wait for all threads to exit before exiting the process.
	if (m_AutomatedBuild)
	{
		POSITION pdtp = m_pDocManager->GetFirstDocTemplatePosition();
		while (pdtp)
		{
			CDocTemplate *pdt = m_pDocManager->GetNextDocTemplate(pdtp);

			POSITION pdp = pdt->GetFirstDocPosition();
			while (pdp)
			{
				CDocument *pd = pdt->GetNextDoc(pdp);

				CSfxPackagerDoc *psfxdoc = dynamic_cast<CSfxPackagerDoc *>(pd);
				if (psfxdoc)
				{
					while (psfxdoc->m_hThread)
					{
						Sleep(20);
					}
				}
			}
		}
	}

	//TODO: handle additional resources you may have added
	AfxOleTerm(FALSE);

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



