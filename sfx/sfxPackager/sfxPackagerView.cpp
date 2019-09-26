/*
	Copyright ©2016. Authored by Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
	
	For inquiries, contact: keelanstuart@gmail.com
*/


// sfxPackagerView.cpp : implementation of the CSfxPackagerView class
//

#include "stdafx.h"
// SHARED_HANDLERS can be defined in an ATL project implementing preview, thumbnail
// and search filter handlers and allows sharing of document code with that project.
#ifndef SHARED_HANDLERS
#include "sfxPackager.h"
#endif

#include "sfxPackagerDoc.h"
#include "sfxPackagerView.h"
#include "MainFrm.h"
#include "PropertiesWnd.h"
#include "RepathDlg.h"
#include "ChildFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CSfxPackagerView

IMPLEMENT_DYNCREATE(CSfxPackagerView, CListView)

BEGIN_MESSAGE_MAP(CSfxPackagerView, CListView)
	ON_WM_CONTEXTMENU()
	ON_WM_RBUTTONUP()
	ON_NOTIFY_REFLECT(LVN_ENDLABELEDIT, &CSfxPackagerView::OnLvnEndlabeledit)
	ON_NOTIFY_REFLECT(LVN_COLUMNCLICK, &CSfxPackagerView::OnLvnColumnclick)
	ON_WM_DROPFILES()
	ON_NOTIFY_REFLECT(LVN_GETDISPINFO, &CSfxPackagerView::OnLvnGetdispinfo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_DELETE, &CSfxPackagerView::OnUpdateEditDelete)
	ON_COMMAND(ID_EDIT_DELETE, &CSfxPackagerView::OnEditDelete)
	ON_COMMAND(ID_EDIT_NEWFILE, &CSfxPackagerView::OnEditNewfile)
	ON_COMMAND(ID_SELECTALL, &CSfxPackagerView::OnSelectall)
	ON_NOTIFY_REFLECT(LVN_ITEMACTIVATE, &CSfxPackagerView::OnLvnItemActivate)
	ON_NOTIFY_REFLECT(NM_CLICK, &CSfxPackagerView::OnNMClick)
	ON_NOTIFY_REFLECT(LVN_KEYDOWN, &CSfxPackagerView::OnLvnKeydown)
	ON_UPDATE_COMMAND_UI(ID_APP_BUILDSFX, &CSfxPackagerView::OnUpdateAppBuildsfx)
	ON_COMMAND(ID_APP_BUILDSFX, &CSfxPackagerView::OnAppBuildsfx)
	ON_UPDATE_COMMAND_UI(ID_EDIT_NEWFILE, &CSfxPackagerView::OnUpdateEditNewfile)
	ON_COMMAND(ID_EDIT_REPATHSRC, &CSfxPackagerView::OnEditRepathSource)
	ON_WM_SETFOCUS()
	ON_UPDATE_COMMAND_UI(ID_APP_CANCELSFX, &CSfxPackagerView::OnUpdateAppCancelSfx)
	ON_COMMAND(ID_APP_CANCELSFX, &CSfxPackagerView::OnAppCancelSfx)
	ON_WM_SIZE()
END_MESSAGE_MAP()

// CSfxPackagerView construction/destruction

CSfxPackagerView::CSfxPackagerView()
{
	m_bFirstUpdate = true;
	m_hPackageThread = NULL;
	m_Splitter = nullptr;
	m_ScriptEditor = nullptr;
}

CSfxPackagerView::~CSfxPackagerView()
{
}

CSfxPackagerView *CSfxPackagerView::GetView()
{
	CMDIFrameWnd *pframe = (CMDIFrameWnd *)(AfxGetApp()->m_pMainWnd);
	CMDIChildWnd *pchild = pframe ? pframe->MDIGetActive() : NULL;
	if (pchild)
	{
		CView *pview = pchild->GetActiveView();

		if (pview && pview->IsKindOf(RUNTIME_CLASS(CSfxPackagerView)))
			return (CSfxPackagerView *)pview;
	}

	return NULL;
}

BOOL CSfxPackagerView::PreCreateWindow(CREATESTRUCT& cs)
{
	cs.style |= LVS_REPORT | LVS_SHOWSELALWAYS;
	cs.dwExStyle |= WS_EX_ACCEPTFILES;

	return CListView::PreCreateWindow(cs);
}

// CSfxPackagerView drawing

void CSfxPackagerView::OnDraw(CDC* /*pDC*/)
{
	CSfxPackagerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc)
		return;

	// TODO: add draw code for native data here
}

void CSfxPackagerView::OnRButtonUp(UINT /* nFlags */, CPoint point)
{
	ClientToScreen(&point);
	OnContextMenu(this, point);
}

void CSfxPackagerView::OnContextMenu(CWnd* /* pWnd */, CPoint point)
{
#ifndef SHARED_HANDLERS
	theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_EDIT, point.x, point.y, this, TRUE);
#endif
}


// CSfxPackagerView diagnostics

#ifdef _DEBUG
void CSfxPackagerView::AssertValid() const
{
	CListView::AssertValid();
}

void CSfxPackagerView::Dump(CDumpContext& dc) const
{
	CListView::Dump(dc);
}

CSfxPackagerDoc* CSfxPackagerView::GetDocument() const // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CSfxPackagerDoc)));
	return (CSfxPackagerDoc*)m_pDocument;
}
#endif //_DEBUG


// CSfxPackagerView message handlers


void CSfxPackagerView::OnUpdate(CView* /*pSender*/, LPARAM /*lHint*/, CObject* /*pHint*/)
{
	CListCtrl &list = GetListCtrl();
	if (list.GetSafeHwnd())
	{
		if (m_bFirstUpdate)
		{
			CHeaderCtrl *phc = list.GetHeaderCtrl();
			HDITEM hdi;
			ZeroMemory(&hdi, sizeof(HDITEM));
			hdi.mask = HDI_TEXT | HDI_WIDTH;

			m_bFirstUpdate = false;

			CRect r;
			GetClientRect(r);
			int fifth = r.Width() / 5;

			hdi.pszText = _T("Filename");
			hdi.cxy = fifth;
			list.InsertColumn(0, hdi.pszText, LVCFMT_LEFT);
			list.SetColumnWidth(0, hdi.cxy);
			phc->SetItem(0, &hdi);

			hdi.pszText = _T("Source");
			hdi.cxy = fifth * 2;
			list.InsertColumn(1, hdi.pszText, LVCFMT_LEFT);
			list.SetColumnWidth(1, hdi.cxy);
			phc->SetItem(1, &hdi);

			hdi.pszText = _T("Destination");
			list.InsertColumn(2, hdi.pszText, LVCFMT_LEFT);
			list.SetColumnWidth(2, hdi.cxy);
			phc->SetItem(2, &hdi);

			list.SetExtendedStyle(LVS_EX_FULLROWSELECT);

			CSfxPackagerDoc *pDoc = GetDocument();

			LVITEM lvi;
			lvi.mask = LVIF_TEXT | LVIF_PARAM;
			lvi.pszText = LPSTR_TEXTCALLBACK;

			for (UINT i = 0; i < pDoc->GetNumFiles(); i++)
			{
				lvi.iItem = i;
				lvi.lParam = i;

				for (lvi.iSubItem = 0; lvi.iSubItem < 3; lvi.iSubItem++)
					list.InsertItem(&lvi);
			}

			m_ScriptEditor->SetWindowText(pDoc->m_scrInit);
		}
	}
}


void CSfxPackagerView::OnLvnEndlabeledit(NMHDR *pNMHDR, LRESULT *pResult)
{
	NMLVDISPINFO *pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
	// TODO: Add your control notification handler code here
	*pResult = 0;
}


void CSfxPackagerView::OnLvnColumnclick(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	// TODO: Add your control notification handler code here
	*pResult = 0;
}

void CSfxPackagerView::ImportLivingFolder(const TCHAR *dir, const TCHAR *include_ext, const TCHAR *exclude_ext)
{
	if (PathIsDirectory(dir))
	{
		if (!include_ext)
			include_ext = _T("*");

		if (!exclude_ext)
			exclude_ext = _T("");

		CString e;
#if 0
		e.Format(_T("+(%s) -(%s)"), include_ext, exclude_ext);
#else
		e.Format(_T("%s"), include_ext);
#endif

		CSfxPackagerDoc *pDoc = GetDocument();
		UINT handle = pDoc->AddFile(e, dir, _T("\\"));

		CListCtrl &list = GetListCtrl();

		LVINSERTMARK im;
		ZeroMemory(&im, sizeof(LVINSERTMARK));
		im.cbSize = sizeof(LVINSERTMARK);
		int idx = list.GetItemCount();

		if (list.GetSafeHwnd())
		{
			LVITEM lvi;
			lvi.mask = LVIF_TEXT | LVIF_PARAM;
			lvi.iItem = idx;
			lvi.lParam = handle;
			lvi.pszText = LPSTR_TEXTCALLBACK;

			for (lvi.iSubItem = 0; lvi.iSubItem < 3; lvi.iSubItem++)
				list.InsertItem(&lvi);
		}
	}
}

void CSfxPackagerView::ImportFile(const TCHAR *filename, UINT depth, bool refresh_props)
{
	if (m_hPackageThread != NULL)
		false;

	if (PathIsDirectory(filename) && !PathIsDirectoryEmpty(filename))
	{
		TCHAR filepath[MAX_PATH];
		PathCombine(filepath, filename, _T("*.*"));
		TCHAR *s = PathFindFileName(filepath);

		WIN32_FIND_DATA fd;
		HANDLE hf = FindFirstFile(filepath, &fd);
		if (hf == INVALID_HANDLE_VALUE)
			return;

		do
		{
			if (_tcscmp(fd.cFileName, _T(".")) && _tcscmp(fd.cFileName, _T("..")))
			{
				_tcscpy_s(s, MAX_PATH, fd.cFileName);
				ImportFile(filepath, depth + 1, refresh_props);
			}
		}
		while (FindNextFile(hf, &fd));

		FindClose(hf);

		return;
	}

	MSG msg;
	BOOL bRet; 
	if ((bRet = GetMessage(&msg, NULL, 0, 0)) != FALSE)
	{ 
		if (bRet != -1)
		{
			TranslateMessage(&msg); 
			DispatchMessage(&msg); 
		}
	} 

	CSfxPackagerDoc *pDoc = GetDocument();
	TCHAR srcdir[MAX_PATH];
	_tcscpy_s(srcdir, MAX_PATH, filename);

	TCHAR dstdir[MAX_PATH];
	_tcscpy_s(dstdir, MAX_PATH, srcdir);
	PathRemoveFileSpec(dstdir);

	_tcsrev(dstdir);
	TCHAR *tmp = depth ? dstdir : NULL;
	TCHAR *start = tmp;
	while (depth && tmp)
	{
		tmp = _tcschr(tmp, _T('\\'));
		if (tmp)
			tmp++;

		depth--;
	}

	if (tmp)
	{
		*tmp = _T('\0');
		_tcsrev(start);
	}

	CListCtrl &list = GetListCtrl();
	UINT handle = pDoc->AddFile(PathFindFileName(filename), srcdir, start ? start : _T("\\"));

	LVINSERTMARK im;
	ZeroMemory(&im, sizeof(LVINSERTMARK));
	im.cbSize = sizeof(LVINSERTMARK);
	int idx = list.GetItemCount();

	if (list.GetSafeHwnd())
	{
		LVITEM lvi;
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		lvi.iItem = idx;
		lvi.lParam = handle;
		lvi.pszText = LPSTR_TEXTCALLBACK;

		for (lvi.iSubItem = 0; lvi.iSubItem < 3; lvi.iSubItem++)
			list.InsertItem(&lvi);
	}

	if (refresh_props)
		RefreshProperties();
}

void CSfxPackagerView::OnDropFiles(HDROP hDropInfo)
{
	BeginWaitCursor();

	UINT numfiles = DragQueryFile(hDropInfo, 0xFFFFFFFF, NULL, 0);

	TCHAR dropfile[MAX_PATH];

	bool got_dir = false;
	bool living_folders = false;

	for (UINT i = 0; i < numfiles; i++)
	{
		DragQueryFile(hDropInfo, i, dropfile, MAX_PATH * sizeof(TCHAR));

		if (PathIsDirectory(dropfile))
		{
			if (!got_dir)
			{
				got_dir = true;
				if (MessageBox(_T("Dynamic folder(s)? This option allows the contents of the folders to be examined at build time and to use wildcards now."), _T("How should sfxPackager treat folders?"), MB_YESNO) == IDYES)
				{
					living_folders = true;
				}
			}

			if (living_folders)
			{
				ImportLivingFolder(dropfile);
				continue;
			}
		}

		ImportFile(dropfile, 0, false);
	}

	RefreshProperties();

	EndWaitCursor();

	CListView::OnDropFiles(hDropInfo);
}


void CSfxPackagerView::OnLvnGetdispinfo(NMHDR *pNMHDR, LRESULT *pResult)
{
	NMLVDISPINFO *pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
	CSfxPackagerDoc *pdoc = GetDocument();

	pDispInfo->item.pszText = (TCHAR *)pdoc->GetFileData((UINT)pDispInfo->item.lParam, (CSfxPackagerDoc::EFileDataType)(pDispInfo->item.iSubItem));

	*pResult = 0;
}


void CSfxPackagerView::OnUpdateEditDelete(CCmdUI *pCmdUI)
{
	CListCtrl &list = GetListCtrl();
	pCmdUI->Enable((list.GetSelectedCount() > 0) && (m_hPackageThread == NULL));
}


void CSfxPackagerView::OnEditDelete()
{
	if (GetSafeHwnd() != ::GetFocus())
		return;

	CSfxPackagerDoc *pDoc = GetDocument();

	CListCtrl &list = GetListCtrl();
	UINT selcount = list.GetSelectedCount();
	int item = -1;

	// Update all of the selected items.
	if (selcount > 0)
	{
		UINT *pdi = (UINT *)_alloca(sizeof(UINT) * selcount);

		for (UINT i = 0; i < selcount; i++)
		{
			item = list.GetNextItem(item, LVNI_SELECTED);

			UINT handle = (UINT)list.GetItemData(item);

			pDoc->RemoveFile(handle);

			pdi[i] = item;
		}

		while (selcount)
		{
			selcount--;

			list.DeleteItem(pdi[selcount]);
		}
	}

	list.RedrawItems(list.GetTopIndex(), list.GetTopIndex() + list.GetCountPerPage());
	Invalidate(NULL);

	RefreshProperties();
}

void CSfxPackagerView::OnEditNewfile()
{
#if 1
	const int c_cMaxFiles = 100;
	const int c_cbBuffSize = (c_cMaxFiles * (MAX_PATH + 1)) + 1;

	CString filename;

	CFileDialog dlg(TRUE, NULL, NULL, OFN_ALLOWMULTISELECT | OFN_ENABLESIZING, NULL, AfxGetMainWnd(), sizeof(OPENFILENAME), TRUE);
	OPENFILENAME &ofn = dlg.GetOFN();
	ofn.lpstrFile = filename.GetBuffer(c_cbBuffSize);
	ofn.nMaxFile = c_cMaxFiles;
	ofn.lpstrTitle = _T("Choose file(s) to add to project...");

	if (dlg.DoModal() == IDOK)
	{
		POSITION pos = dlg.GetStartPosition();
		while (pos)
		{
			CString s = dlg.GetNextPathName(pos);
			ImportFile((LPCTSTR)s);
		}
	}

	filename.ReleaseBuffer();
#else
	BROWSEINFO bi;
	LPITEMIDLIST pidl;
	LPMALLOC pMalloc;
	if (SUCCEEDED(::SHGetMalloc(&pMalloc)))
	{
		ZeroMemory(&bi, sizeof(BROWSEINFO));

		bi.lpszTitle = _T("Choose files and/or folders to add to project...");
		bi.hwndOwner = AfxGetMainWnd();
		bi.pszDisplayName = 0;
		bi.pidlRoot = 0;
		bi.ulFlags = BIF_BROWSEINCLUDEFILES | BIF_STATUSTEXT | BIF_EDITBOX | BIF_NEWDIALOGSTYLE;
		bi.lpfn = NULL;
		bi.lParam = NULL;

		pidl = SHBrowseForFolder(&bi);
		if (pidl)
		{
			TCHAR szdir[MAX_PATH];
			if (SHGetPathFromIDList(pidl, szdir))
			{
				SetValue(CString(szdir));
			}

			pMalloc->Free(pidl);
			pMalloc->Release();
		}
	}
#endif
}


void CSfxPackagerView::OnSelectall()
{
	CListCtrl &list = GetListCtrl();
	for (UINT i = 0, maxi = list.GetItemCount(); i < maxi; i++)
	{
		list.SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
	}

	RefreshProperties();
}


void CSfxPackagerView::OnActivateView(BOOL bActivate, CView* pActivateView, CView* pDeactiveView)
{
	if (bActivate && (pActivateView != pDeactiveView))
	{
		RefreshProperties();
	}
		
	CListView::OnActivateView(bActivate, pActivateView, pDeactiveView);
}


void CSfxPackagerView::OnLvnItemActivate(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMIA = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);

	*pResult = 0;
}


void CSfxPackagerView::OnNMClick(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);

	RefreshProperties();

	*pResult = 0;
}

void CSfxPackagerView::RefreshProperties(CSfxPackagerDoc *pd)
{
	CMainFrame *pmf = (CMainFrame *)(AfxGetApp()->m_pMainWnd);
	if (pmf && pmf->GetSafeHwnd())
	{
		CPropertiesWnd &propwnd = pmf->GetPropertiesWnd();
		propwnd.FillPropertyList(pd);
	}
}


void CSfxPackagerView::OnLvnKeydown(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLVKEYDOWN pLVKeyDown = reinterpret_cast<LPNMLVKEYDOWN>(pNMHDR);

	if ((pLVKeyDown->wVKey == VK_UP) || (pLVKeyDown->wVKey == VK_DOWN) ||
		(pLVKeyDown->wVKey == VK_HOME) || (pLVKeyDown->wVKey == VK_END) ||
		(pLVKeyDown->wVKey == VK_PRIOR) || (pLVKeyDown->wVKey == VK_NEXT))
		RefreshProperties();

	*pResult = 0;
}


void CSfxPackagerView::OnUpdateAppBuildsfx(CCmdUI *pCmdUI)
{
	CListCtrl &list = GetListCtrl();
	pCmdUI->Enable(list.GetSafeHwnd() && list.GetItemCount() && (m_hPackageThread == NULL));
}


void CSfxPackagerView::OnAppBuildsfx()
{
	m_hPackageThread = CreateThread(NULL, 0, CSfxPackagerDoc::RunCreateSFXPackage, this, 0, NULL);
}


void CSfxPackagerView::OnInitialUpdate()
{
	CListView::OnInitialUpdate();

	RefreshProperties((CSfxPackagerDoc *)GetDocument());
}


void CSfxPackagerView::OnFinalRelease()
{
	CListView::OnFinalRelease();
}


BOOL CSfxPackagerView::DestroyWindow()
{
	return CListView::DestroyWindow();
}


void CSfxPackagerView::OnUpdateEditNewfile(CCmdUI *pCmdUI)
{
	CListCtrl &list = GetListCtrl();
	pCmdUI->Enable(list.GetSafeHwnd() && (m_hPackageThread == NULL));
}

void CSfxPackagerView::DonePackaging()
{
	m_hPackageThread = NULL;
}

void CSfxPackagerView::SetDestFolderForSelection(const TCHAR *dst, const TCHAR *rootdst)
{
	CSfxPackagerDoc *pDoc = GetDocument();

	CListCtrl &list = GetListCtrl();
	UINT selcount = list.GetSelectedCount();
	int item = -1;

	// Update all of the selected items.
	if (selcount > 0)
	{
		for (UINT i = 0; i < selcount; i++)
		{
			item = list.GetNextItem(item, LVNI_SELECTED);

			UINT handle = (UINT)list.GetItemData(item);

			CString val = pDoc->GetFileData(handle, CSfxPackagerDoc::FDT_DSTPATH);
			val.Replace(rootdst, dst);

			pDoc->SetFileData(handle, CSfxPackagerDoc::FDT_DSTPATH, val);
		}
	}

	list.RedrawItems(list.GetTopIndex(), list.GetTopIndex() + list.GetCountPerPage());
	Invalidate(NULL);

	RefreshProperties();
}

void CSfxPackagerView::SetSrcFolderForSelection(const TCHAR *src, const TCHAR *rootsrc)
{
	CSfxPackagerDoc *pDoc = GetDocument();

	CListCtrl &list = GetListCtrl();
	UINT selcount = list.GetSelectedCount();
	int item = -1;

	// Update all of the selected items.
	if (selcount > 0)
	{
		for (UINT i = 0; i < selcount; i++)
		{
			item = list.GetNextItem(item, LVNI_SELECTED);

			UINT handle = (UINT)list.GetItemData(item);

			CString val = pDoc->GetFileData(handle, CSfxPackagerDoc::FDT_SRCPATH);
			val.Replace(rootsrc, src);

			pDoc->SetFileData(handle, CSfxPackagerDoc::FDT_SRCPATH, val);
		}
	}

	list.RedrawItems(list.GetTopIndex(), list.GetTopIndex() + list.GetCountPerPage());
	Invalidate(NULL);

	RefreshProperties();
}

void CSfxPackagerView::SetFilenameForSelection(const TCHAR *name)
{
	CSfxPackagerDoc *pDoc = GetDocument();

	CListCtrl &list = GetListCtrl();
	UINT selcount = list.GetSelectedCount();
	int item = -1;

	// Update all of the selected items.
	if (selcount == 1)
	{
		item = list.GetNextItem(item, LVNI_SELECTED);

		UINT handle = (UINT)list.GetItemData(item);

		pDoc->SetFileData(handle, CSfxPackagerDoc::FDT_NAME, name);
	}

	list.RedrawItems(list.GetTopIndex(), list.GetTopIndex() + list.GetCountPerPage());
	Invalidate(NULL);

	RefreshProperties();
}

void PrunePathToCommonPart(TCHAR *path1, const TCHAR *path2)
{
	if (!path1)
		return;

	if (!path2)
	{
		*path1 = _T('\0');
		return;
	}

	TCHAR *p = path1;
	while (*p && *path2 && (*p == *path2))
	{
		p++;
		path2++;
	}

	*p = _T('\0');
	if (!PathIsDirectory(path1))
	{
		PathRemoveFileSpec(path1);
	}

	PathRemoveBackslash(path1);
}

void CSfxPackagerView::OnEditRepathSource()
{
	CSfxPackagerDoc *pDoc = GetDocument();

	CListCtrl &list = GetListCtrl();
	UINT selcount = list.GetSelectedCount();

	// Update all of the selected items.
	if (selcount > 0)
	{
		TCHAR rootpath[MAX_PATH];
		int item = -1;

		for (UINT i = 0; i < selcount; i++)
		{
			item = list.GetNextItem(item, LVNI_SELECTED);

			UINT handle = (UINT)list.GetItemData(item);
			if (i == 0)
				_tcscpy_s(rootpath, MAX_PATH, pDoc->GetFileData(handle, CSfxPackagerDoc::FDT_SRCPATH));
			else
				PrunePathToCommonPart(rootpath, pDoc->GetFileData(handle, CSfxPackagerDoc::FDT_SRCPATH));
		}

		if (!rootpath[0])
		{
			MessageBox(_T("A common path was not found for all items selected.  Choose only files that share a common root."), _T("No Common Root Path"), MB_OK);
			return;
		}

		CRepathDlg rpdlg;
		rpdlg.m_Path = rootpath;
		if (rpdlg.DoModal() == IDOK)
		{
			item = -1;
			for (UINT i = 0; i < selcount; i++)
			{
				item = list.GetNextItem(item, LVNI_SELECTED);

				UINT handle = (UINT)list.GetItemData(item);

				CString newpath = pDoc->GetFileData(handle, CSfxPackagerDoc::FDT_SRCPATH);
				newpath.Replace(rootpath, rpdlg.m_Path);

				pDoc->SetFileData(handle, CSfxPackagerDoc::FDT_SRCPATH, newpath);
			}

			list.RedrawItems(list.GetTopIndex(), list.GetTopIndex() + list.GetCountPerPage());
			Invalidate(NULL);

			RefreshProperties();
		}
	}
}


void CSfxPackagerView::OnActivateFrame(UINT nState, CFrameWnd* pDeactivateFrame)
{
	CListView::OnActivateFrame(nState, pDeactivateFrame);

	CChildFrame *pov = dynamic_cast<CChildFrame *>(pDeactivateFrame);
	if (pov)
		RefreshProperties(GetDocument());
}


void CSfxPackagerView::OnSetFocus(CWnd* pOldWnd)
{
	CListView::OnSetFocus(pOldWnd);

	CChildFrame *pov = dynamic_cast<CChildFrame *>(pOldWnd);
	if (pov)
		RefreshProperties(GetDocument());
}


void CSfxPackagerView::OnUpdateAppCancelSfx(CCmdUI *pCmdUI)
{
	CSfxPackagerDoc *pDoc = GetDocument();

	pCmdUI->Enable(pDoc->m_hThread != NULL);
}


void CSfxPackagerView::OnAppCancelSfx()
{
	CSfxPackagerDoc *pDoc = GetDocument();

	SetEvent(pDoc->m_hCancelEvent);
}


void CSfxPackagerView::OnSize(UINT nType, int cx, int cy)
{
#if 0
	if (m_Splitter)
	{
		CRect r;
		m_Splitter->GetClientRect(r);
		cx = r.Width();

		int dummy;
		m_Splitter->GetRowInfo(0, cy, dummy);
	}
#endif

	CListView::OnSize(nType, cx, cy);
}
