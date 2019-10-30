/*
	Copyright ©2016. Authored by Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
	
	For inquiries, contact: keelanstuart@gmail.com
*/


#include "stdafx.h"

#include "PropertiesWnd.h"
#include "Resource.h"
#include "MainFrm.h"
#include "sfxPackager.h"
#include "sfxPackagerDoc.h"
#include "sfxPackagerView.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

enum { CID_SELECTOR = 1, CID_PROPGRID };


/////////////////////////////////////////////////////////////////////////////
// CResourceViewBar

CPropertiesWnd::CPropertiesWnd()
{
}

CPropertiesWnd::~CPropertiesWnd()
{
}

BEGIN_MESSAGE_MAP(CPropertiesWnd, CDockablePane)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_COMMAND(ID_EXPAND_ALL, OnExpandAllProperties)
	ON_UPDATE_COMMAND_UI(ID_EXPAND_ALL, OnUpdateExpandAllProperties)
	ON_COMMAND(ID_SORTPROPERTIES, OnSortProperties)
	ON_UPDATE_COMMAND_UI(ID_SORTPROPERTIES, OnUpdateSortProperties)
	ON_WM_SETFOCUS()
	ON_WM_SETTINGCHANGE()
	ON_CBN_SELCHANGE(CID_SELECTOR, OnObjectSelected)
	ON_UPDATE_COMMAND_UI(CID_PROPGRID, &CPropertiesWnd::OnUpdatePropertyGrid)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CResourceViewBar message handlers

void CPropertiesWnd::AdjustLayout()
{
	if (GetSafeHwnd() == NULL)
	{
		return;
	}

	CRect rectClient,rectCombo;
	GetClientRect(rectClient);

	m_wndObjectCombo.GetWindowRect(&rectCombo);

	int cyCmb = rectCombo.Size().cy;
	int cyTlb = m_wndToolBar.CalcFixedLayout(FALSE, TRUE).cy;

	m_wndObjectCombo.SetWindowPos(NULL, rectClient.left, rectClient.top, rectClient.Width(), 200, SWP_NOACTIVATE | SWP_NOZORDER);
	m_wndToolBar.SetWindowPos(NULL, rectClient.left, rectClient.top + cyCmb, rectClient.Width(), cyTlb, SWP_NOACTIVATE | SWP_NOZORDER);
	m_wndPropList.SetWindowPos(NULL, rectClient.left, rectClient.top + cyCmb + cyTlb, rectClient.Width(), rectClient.Height() -(cyCmb+cyTlb), SWP_NOACTIVATE | SWP_NOZORDER);
}

int CPropertiesWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;

	CRect rectDummy;
	rectDummy.SetRectEmpty();

	// Create combo:
	const DWORD dwViewStyle = WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_BORDER | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

	if (!m_wndObjectCombo.Create(dwViewStyle, rectDummy, this, CID_SELECTOR))
	{
		TRACE0("Failed to create Properties Combo \n");
		return -1;      // fail to create
	}

	m_wndObjectCombo.AddString(_T("Package"));
	m_wndObjectCombo.AddString(_T("File"));
	m_wndObjectCombo.AddString(_T("Settings"));
	m_wndObjectCombo.SetCurSel(0);

	if (!m_wndPropList.Create(WS_VISIBLE | WS_CHILD, rectDummy, this, CID_PROPGRID))
	{
		TRACE0("Failed to create Properties Grid \n");
		return -1;      // fail to create
	}

	InitPropList();

	m_wndToolBar.Create(this, AFX_DEFAULT_TOOLBAR_STYLE, IDR_PROPERTIES);
	m_wndToolBar.LoadToolBar(IDR_PROPERTIES, 0, 0, TRUE /* Is locked */);
	m_wndToolBar.CleanUpLockedImages();
	m_wndToolBar.LoadBitmap(theApp.m_bHiColorIcons ? IDB_PROPERTIES_HC : IDR_PROPERTIES, 0, 0, TRUE /* Locked */);

	m_wndToolBar.SetPaneStyle(m_wndToolBar.GetPaneStyle() | CBRS_TOOLTIPS | CBRS_FLYBY);
	m_wndToolBar.SetPaneStyle(m_wndToolBar.GetPaneStyle() & ~(CBRS_GRIPPER | CBRS_SIZE_DYNAMIC | CBRS_BORDER_TOP | CBRS_BORDER_BOTTOM | CBRS_BORDER_LEFT | CBRS_BORDER_RIGHT));
	m_wndToolBar.SetOwner(this);

	// All commands will be routed via this control , not via the parent frame:
	m_wndToolBar.SetRouteCommandsViaFrame(FALSE);

	AdjustLayout();
	return 0;
}

void CPropertiesWnd::OnSize(UINT nType, int cx, int cy)
{
	CDockablePane::OnSize(nType, cx, cy);
	AdjustLayout();
}

void CPropertiesWnd::OnExpandAllProperties()
{
	m_wndPropList.ExpandAll();
}

void CPropertiesWnd::OnUpdateExpandAllProperties(CCmdUI* /* pCmdUI */)
{
}

void CPropertiesWnd::OnSortProperties()
{
	m_wndPropList.SetAlphabeticMode(!m_wndPropList.IsAlphabeticMode());
}

void CPropertiesWnd::OnUpdateSortProperties(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_wndPropList.IsAlphabeticMode());
}

extern void PrunePathToCommonPart(TCHAR *path1, const TCHAR *path2);

void CPropertiesWnd::FillPropertyList(CSfxPackagerDoc *pd, EPropertySet s)
{
	if (!GetSafeHwnd() || m_wndPropList.IsLocked())
		return;

	m_wndPropList.RemoveAll();
	m_wndPropList.Invalidate();

	if (!pd)
		pd = CSfxPackagerDoc::GetDoc();
	if (!pd)
		return;

	if (s == PS_AUTODETECT)
	{
		UINT sel = m_wndObjectCombo.GetCurSel();

		switch (sel)
		{
			// the whole package
			case 0:
				s = PS_PACKAGE;
				break;

			// individual file(s)
			case 1:
				s = PS_FILE;
				break;

			// global settings
			case 2:
				s = PS_SETTINGS;
				break;
		}
	}

	CSfxPackagerView *pv = CSfxPackagerView::GetView();
	if ((s == PS_FILE) && !pv)
		return;

	static const TCHAR szIcoFilter[] = _T("Icon Files(*.ico)|*.ico|All Files(*.*)|*.*||");
	static const TCHAR szBmpFilter[] = _T("Bitmap Files(*.bmp)|*.bmp|All Files(*.*)|*.*||");
	static const TCHAR szExeFilter[] = _T("Executable Files(*.exe)|*.exe|All Files(*.*)|*.*||");

	switch (s)
	{
		case PS_PACKAGE:
		{
			CMFCPropertyGridProperty *pSettingsGroup = new CMFCPropertyGridProperty(_T("Settings"));

			CMFCPropertyGridProperty *pSfxNameProp = new CMFCPropertyGridFileProperty(_T("Output File"), FALSE, pd->m_SfxOutputFile, _T("exe"), 0, szExeFilter, _T("Specifies the output executable file that will be created when this project is built|*.exe"));
			CMFCPropertyGridProperty *pMaxSizeProp = new CMFCPropertyGridProperty(_T("Maximum Size (MB)"), pd->m_MaxSize, _T("The maximum size (in MB) constraint for generated sfx archives, beyond which, files will be split (-1 is no constraint)."));

			pSettingsGroup->AddSubItem(pSfxNameProp);
			pSettingsGroup->AddSubItem(pMaxSizeProp);

			m_wndPropList.AddProperty(pSettingsGroup);

			TCHAR *pext = PathFindExtension(pd->m_SfxOutputFile);

			if (pext && !_tcsicmp(pext, _T(".exe")))
			{
				{
					CMFCPropertyGridProperty *pDefaultPathProp = new CMFCPropertyGridFileProperty(_T("Default Path"), pd->m_DefaultPath, 0, _T("The default root path where the install data will go"));
					CMFCPropertyGridProperty *pRequireAdminProp = new CMFCPropertyGridProperty(_T("Require Admin"), (_variant_t)((bool)pd->m_bRequireAdmin), _T("If set, requires the user to have administrative privileges and will prompt the user to elevate if need be."));
					CMFCPropertyGridProperty *pShowDestDlgProp = new CMFCPropertyGridProperty(_T("Allow Destination Change"), (_variant_t)((bool)pd->m_bAllowDestChg), _T("If set, will allow the destination folder to be changed by the user. Should normally be set."));

					pSettingsGroup->AddSubItem(pDefaultPathProp);
					pSettingsGroup->AddSubItem(pRequireAdminProp);
					pSettingsGroup->AddSubItem(pShowDestDlgProp);
				}

				CMFCPropertyGridProperty *pAppearanceGroup = new CMFCPropertyGridProperty(_T("Appearance"));
				{
					CMFCPropertyGridProperty *pTitleProp = new CMFCPropertyGridProperty(_T("Caption"), pd->m_Caption, _T("Specifies the text that will be displayed in the window's title bar"));
					CMFCPropertyGridProperty *pDescriptionProp = new CMFCPropertyGridProperty(_T("Description"), pd->m_Description, _T("Specifies the text that will be displayed in the window's main area to tell the end-user what the package is"));
					CMFCPropertyGridProperty *pVersionProp = new CMFCPropertyGridProperty(_T("Version ID"), pd->m_VersionID, _T("Specifies the version number that will be displayed by the installer"));
					CMFCPropertyGridFileProperty *pIconProp = new CMFCPropertyGridFileProperty(_T("Icon"), TRUE, pd->m_IconFile, _T("ico"), 0, szIcoFilter, _T("Specifies the ICO-format window icon"));
					CMFCPropertyGridFileProperty *pImageProp = new CMFCPropertyGridFileProperty(_T("Image"), TRUE, pd->m_ImageFile, _T("bmp"), 0, szBmpFilter, _T("Specifies the BMP-format image that will be displayed on the window"));

					pAppearanceGroup->AddSubItem(pTitleProp);
					pAppearanceGroup->AddSubItem(pDescriptionProp);
					pAppearanceGroup->AddSubItem(pVersionProp);
					pAppearanceGroup->AddSubItem(pIconProp);
					pAppearanceGroup->AddSubItem(pImageProp);
				}
				m_wndPropList.AddProperty(pAppearanceGroup);

				CMFCPropertyGridProperty *pPostInstallGroup = new CMFCPropertyGridProperty(_T("Post-Install"));
				{
					CMFCPropertyGridProperty *pRequireRebootProp = new CMFCPropertyGridProperty(_T("Require Reboot"), (_variant_t)((bool)pd->m_bRequireReboot), _T("If set, will inform that user that a reboot of the system is recommended and prompt to do that."));
					CMFCPropertyGridProperty *pShowOpenProp = new CMFCPropertyGridProperty(_T("Explore"), (_variant_t)((bool)pd->m_bExploreOnComplete), _T("If set, will open a windows explorer window to the install directory upon completion"));
					CMFCPropertyGridProperty *pLaunchCmdProp = new CMFCPropertyGridProperty(_T("Launch"), pd->m_LaunchCmd, _T("A command that will be issued when installation is complete (see full docs for parameter options)"));

					pPostInstallGroup->AddSubItem(pRequireRebootProp);
					pPostInstallGroup->AddSubItem(pShowOpenProp);
					pPostInstallGroup->AddSubItem(pLaunchCmdProp);
				}
				m_wndPropList.AddProperty(pPostInstallGroup);
			}

			break;
		}
		case PS_FILE:
		{
			UINT selcount = 0;

			CString name;
			CString src;
			CString dst;
			CString exclude;
			CString snippet;

			TCHAR rawsrc[MAX_PATH];
			rawsrc[0] = _T('\0');

			TCHAR rawdst[MAX_PATH];
			rawdst[0] = _T('\0');

			bool got_name = false;
			bool got_src = false;
			bool got_dst = false;
			bool got_exclude = false;
			bool got_snippet = false;

			CListCtrl &list = pv->GetListCtrl();
			POSITION pos = list.GetFirstSelectedItemPosition();

			while (pos)
			{
				selcount++;

				int i = list.GetNextSelectedItem(pos);
				DWORD_PTR hi = list.GetItemData(i);

				if (!got_name)
				{
					name = pd->GetFileData((UINT)hi, CSfxPackagerDoc::FDT_NAME);
					got_name = true;
				}
				else
				{
					name = _T("");
				}

				if (!got_src)
				{
					_tcscpy_s(rawsrc, MAX_PATH, pd->GetFileData((UINT)hi, CSfxPackagerDoc::FDT_SRCPATH));
					got_src = true;
				}
				else
				{
					PrunePathToCommonPart(rawsrc, pd->GetFileData((UINT)hi, CSfxPackagerDoc::FDT_SRCPATH));
				}

				if (!got_dst)
				{
					_tcscpy_s(rawdst, MAX_PATH, pd->GetFileData((UINT)hi, CSfxPackagerDoc::FDT_DSTPATH));
					got_dst = true;
				}
				else
				{
					PrunePathToCommonPart(rawdst, pd->GetFileData((UINT)hi, CSfxPackagerDoc::FDT_DSTPATH));
				}

				if (!got_exclude)
				{
					exclude = pd->GetFileData((UINT)hi, CSfxPackagerDoc::FDT_EXCLUDE);
					got_exclude = true;
				}
				else
				{
					exclude = _T("");
				}

				if (!got_snippet)
				{
					snippet = pd->GetFileData((UINT)hi, CSfxPackagerDoc::FDT_SNIPPET);
					got_snippet = true;
				}
				else
				{
					snippet = _T("");
				}
			}

			src = rawsrc;
			dst = rawdst;

			//CString 
			CMFCPropertyGridProperty *pNameProp = new CMFCPropertyGridProperty(_T("Filename"), name, _T("The name of the file that will be installed (note: this can be different than the name of the source file)"));
			CMFCPropertyGridProperty *pSrcProp = new CMFCPropertyGridProperty(_T("Source"), src, _T("The source file"));
			CMFCPropertyGridProperty *pDstProp = new CMFCPropertyGridProperty(_T("Destination"), dst, _T("The destination directory, relative to the install folder chosen by the user"));
			CMFCPropertyGridProperty *pExcludeProp = new CMFCPropertyGridProperty(_T("Exclude"), exclude, _T("A semi-colon-delimited list of wildcard file descriptions of things that should be excluded (only applies to wildcard Filenames to begin with)"));
			CMFCPropertyGridProperty *pSnippetProp = new CMFCPropertyGridProperty(_T("Script Add-On"), snippet, _T("This snippet will be appended to the PER-FILE script that is executed after the file is installed. As an example, it could be used to call a function embedded within the global PER-FILE script"));

			pNameProp->Enable((selcount == 1) ? true : false);
			pExcludeProp->Enable(_tcschr(name, _T('*')) != NULL);

			m_wndPropList.AddProperty(pNameProp);
			m_wndPropList.AddProperty(pSrcProp);
			m_wndPropList.AddProperty(pDstProp);
			m_wndPropList.AddProperty(pExcludeProp);
			m_wndPropList.AddProperty(pSnippetProp);
			break;
		}

		case PS_SETTINGS:
		{
			CMFCPropertyGridFileProperty *pTempPathProp = new CMFCPropertyGridFileProperty(_T("Temporary Directory"), theApp.m_sTempPath, NULL, _T("The location where temporary working files will be stored - this should be an isolated location and contain no other files than those copied there by sfxPackager, since they will be deleted"));
			CMFCPropertyGridFileProperty *p7ZipPathProp = new CMFCPropertyGridFileProperty(_T("7-Zip Executable"), TRUE, theApp.m_s7ZipPath, _T("exe"), 0, szExeFilter, _T("The location of 7z.exe"));

			m_wndPropList.AddProperty(pTempPathProp);
			m_wndPropList.AddProperty(p7ZipPathProp);
			break;
		}
	}
}

void CPropertiesWnd::InitPropList()
{
	SetPropListFont();

	m_wndPropList.EnableHeaderCtrl(FALSE);
	m_wndPropList.EnableDescriptionArea();
	m_wndPropList.SetVSDotNetLook();
	m_wndPropList.MarkModifiedProperties();

	FillPropertyList(NULL, PS_PACKAGE);
}

void CPropertiesWnd::OnSetFocus(CWnd* pOldWnd)
{
	CDockablePane::OnSetFocus(pOldWnd);
	m_wndPropList.SetFocus();
}

void CPropertiesWnd::OnSettingChange(UINT uFlags, LPCTSTR lpszSection)
{
	CDockablePane::OnSettingChange(uFlags, lpszSection);
	SetPropListFont();
}

void CPropertiesWnd::SetPropListFont()
{
	::DeleteObject(m_fntPropList.Detach());

	LOGFONT lf;
	afxGlobalData.fontRegular.GetLogFont(&lf);

	NONCLIENTMETRICS info;
	info.cbSize = sizeof(info);

	afxGlobalData.GetNonClientMetrics(info);

	lf.lfHeight = info.lfMenuFont.lfHeight;
	lf.lfWeight = info.lfMenuFont.lfWeight;
	lf.lfItalic = info.lfMenuFont.lfItalic;

	m_fntPropList.CreateFontIndirect(&lf);

	m_wndPropList.SetFont(&m_fntPropList);
	m_wndObjectCombo.SetFont(&m_fntPropList);
}

void CPropertiesWnd::OnObjectSelected()
{
	FillPropertyList(NULL, PS_AUTODETECT);
}


void CPropertiesWnd::OnUpdatePropertyGrid(CCmdUI *pCmdUI)
{
	CSfxPackagerView *pv = CSfxPackagerView::GetView();
	if (pv)
		pCmdUI->Enable(!pv->IsPackaging());
}
