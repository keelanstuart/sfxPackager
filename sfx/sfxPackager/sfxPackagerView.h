/*
	Copyright ©2016. Authored by Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
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


class CSfxPackagerView : public CListView
{
protected: // create from serialization only
	CSfxPackagerView();
	DECLARE_DYNCREATE(CSfxPackagerView)

// Attributes
public:
	CSfxPackagerDoc* GetDocument() const;

	static CSfxPackagerView *GetView();

// Operations
public:
	void RefreshProperties(CSfxPackagerDoc *pd = NULL);

protected:
	void ImportLivingFolder(const TCHAR *dir, const TCHAR *include_ext = _T("*"), const TCHAR *exclude_ext = NULL);
	void ImportFile(const TCHAR *filename, UINT depth = 0, bool refresh_props = true);

// Overrides
public:
	virtual void OnDraw(CDC* pDC);  // overridden to draw this view
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:

// Implementation
public:
	virtual ~CSfxPackagerView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	void DonePackaging();
	bool IsPackaging() { return (m_hPackageThread != NULL); }

	void SetDestFolderForSelection(const TCHAR *dst, const TCHAR *rootdst);
	void SetSrcFolderForSelection(const TCHAR *src, const TCHAR *rootsrc);
	void SetFilenameForSelection(const TCHAR *name);

protected:
	bool m_bFirstUpdate;
	HANDLE m_hPackageThread;

// Generated message map functions
protected:
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
};

#ifndef _DEBUG  // debug version in sfxPackagerView.cpp
inline CSfxPackagerDoc* CSfxPackagerView::GetDocument() const
   { return reinterpret_cast<CSfxPackagerDoc*>(m_pDocument); }
#endif

