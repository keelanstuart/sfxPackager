/*
	Copyright ©2014-2019. Authored by Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
	
	For inquiries, contact: keelanstuart@gmail.com
*/


// sfxPackagerView.h : interface of the CSfxPackagerView class
//

#pragma once

#include "sfxPackagerDoc.h"
#include "CScriptEditView.h"


class CSfxPackagerView : public CListView
{
protected: // create from serialization only
	CSfxPackagerView();
	DECLARE_DYNCREATE(CSfxPackagerView)

// Attributes
public:
	virtual ~CSfxPackagerView();

	CSfxPackagerDoc* GetDocument() const;

	static CSfxPackagerView *GetView();
	void SetSplitterWnd(CSplitterWndEx *ps) { m_Splitter = ps; }
	CSplitterWndEx *GetSplitterWnd() { return m_Splitter; }

	void SetScriptEditView(CScriptEditView *pe) { m_ScriptEditor = pe; }
	CScriptEditView *GetScriptEditView() { return m_ScriptEditor; }

	void RefreshProperties(CSfxPackagerDoc *pd = NULL);

	virtual void OnDraw(CDC* pDC);  // overridden to draw this view
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	void DonePackaging();
	bool IsPackaging() { return (m_hPackageThread != NULL); }

	void TestSfx();

	void SetDestFolderForSelection(const TCHAR *dst, const TCHAR *rootdst);
	void SetSrcFolderForSelection(const TCHAR *src, const TCHAR *rootsrc);
	void SetFilenameForSelection(const TCHAR *name);
	void SetExclusionsForSelection(const TCHAR *exclude);
	void SetScriptSnippetForSelection(const TCHAR *snippet);

protected:
	void ImportLivingFolder(const TCHAR *dir, const TCHAR *include_ext = _T("*"), const TCHAR *exclude_ext = NULL);
	void ImportFile(const TCHAR *filename, UINT depth = 0, bool refresh_props = true);

	void AdjustSelectionPos(CSfxPackagerDoc::EMoveType mt);

	bool m_bFirstUpdate;
	HANDLE m_hPackageThread;
	CSplitterWndEx *m_Splitter;
	CScriptEditView *m_ScriptEditor;
	HACCEL m_hAccelTable;

// Generated message map functions
	afx_msg void OnFilePrintPreview();
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);

	DECLARE_MESSAGE_MAP()
public:
	virtual void OnUpdate(CView* /*pSender*/, LPARAM /*lHint*/, CObject* /*pHint*/);
	afx_msg void OnLvnEndlabeledit(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnColumnclick(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnDropFiles(HDROP hDropInfo);
	afx_msg void OnLvnGetdispinfo(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnUpdateEditDelete(CCmdUI *pCmdUI);
	afx_msg void OnEditDelete();
	afx_msg void OnEditNewfile();
	afx_msg void OnSelectall();
	virtual void OnActivateView(BOOL bActivate, CView* pActivateView, CView* pDeactiveView);
	afx_msg void OnLvnItemActivate(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnNMClick(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnKeydown(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnUpdateAppBuildsfx(CCmdUI *pCmdUI);
	afx_msg void OnAppBuildsfx();
	virtual void OnInitialUpdate();
	virtual void OnFinalRelease();
	virtual BOOL DestroyWindow();
	afx_msg void OnUpdateEditNewfile(CCmdUI *pCmdUI);
	afx_msg void OnEditRepathSource();
	virtual void OnActivateFrame(UINT nState, CFrameWnd* pDeactivateFrame);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnUpdateAppCancelSfx(CCmdUI *pCmdUI);
	afx_msg void OnAppCancelSfx();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnAdjustPosUp();
	afx_msg void OnAdjustPosDown();
	afx_msg void OnAdjustPosTop();
	afx_msg void OnAdjustPosBottom();
	afx_msg void OnUpdateAdjustPos(CCmdUI *pCmdUI);
	afx_msg void OnUpdateAppTestSfx(CCmdUI *pCmdUI);

	virtual BOOL PreTranslateMessage(MSG *pMsg);
};

#ifndef _DEBUG  // debug version in sfxPackagerView.cpp
inline CSfxPackagerDoc* CSfxPackagerView::GetDocument() const { return reinterpret_cast<CSfxPackagerDoc*>(m_pDocument); }
#endif

