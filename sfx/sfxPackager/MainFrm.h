/*
	Copyright © 2013-2020, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
	
	For inquiries, contact: keelanstuart@gmail.com
*/


// MainFrm.h : interface of the CMainFrame class
//

#pragma once
#include "OutputWnd.h"
#include "PropertiesWnd.h"
#include "ProgressStatusBar.h"

class CMainFrame : public CMDIFrameWndEx
{
	DECLARE_DYNAMIC(CMainFrame)
public:
	CMainFrame();

// Attributes
public:

// Operations
public:

// Overrides
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL LoadFrame(UINT nIDResource, DWORD dwDefaultStyle = WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE, CWnd* pParentWnd = NULL, CCreateContext* pContext = NULL);

// Implementation
public:
	virtual ~CMainFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	COutputWnd &GetOutputWnd() { return m_wndOutput; }
	CPropertiesWnd &GetPropertiesWnd() { return m_wndProperties; }
	CProgressStatusBar &GetStatusBarWnd() { return m_wndStatusBar; }

protected:  // control bar embedded members
	CMFCMenuBar         m_wndMenuBar;
	CMFCToolBar		    m_wndToolBar;
	CProgressStatusBar  m_wndStatusBar;
	CMFCToolBarImages   m_UserImages;
	COutputWnd          m_wndOutput;
	CPropertiesWnd      m_wndProperties;

// Generated message map functions
protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnWindowManager();
	afx_msg void OnViewCustomize();
	afx_msg LRESULT OnToolbarCreateNew(WPARAM wp, LPARAM lp);
	afx_msg void OnApplicationLook(UINT id);
	afx_msg void OnUpdateApplicationLook(CCmdUI* pCmdUI);
	afx_msg void OnSettingChange(UINT uFlags, LPCTSTR lpszSection);
	afx_msg void OnUpdateAppBuildsfx(CCmdUI *pCmdUI);
	afx_msg void OnAppBuildsfx();
	afx_msg void OnUpdateAppCancelSfx(CCmdUI *pCmdUI);
	afx_msg void OnAppCancelSfx();
	afx_msg void OnUpdateTestSfx(CCmdUI *pCmdUI);
	afx_msg void OnTestSfx();
	afx_msg void OnActivate(UINT nCmdShow, CWnd *pw, BOOL b);

	DECLARE_MESSAGE_MAP()

	BOOL CreateDockingWindows();
	void SetDockingWindowIcons(BOOL bHiColorIcons);
public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);

	virtual void CMainFrame::ActivateFrame(int nCmdShow = -1);

};


