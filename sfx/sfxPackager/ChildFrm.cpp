/*
	Copyright © 2013-2025, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
	
	For inquiries, contact: keelanstuart@gmail.com
*/


// ChildFrm.cpp : implementation of the CChildFrame class
//

#include "stdafx.h"
#include "sfxPackager.h"
#include "sfxPackagerDoc.h"
#include "sfxPackagerView.h"
#include "MainFrm.h"

#include "ChildFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CChildFrame

IMPLEMENT_DYNCREATE(CChildFrame, CMDIChildWndEx)

BEGIN_MESSAGE_MAP(CChildFrame, CMDIChildWndEx)
	ON_WM_NCPAINT()
	ON_WM_SETFOCUS()
	ON_WM_MDIACTIVATE()
END_MESSAGE_MAP()

// CChildFrame construction/destruction

CChildFrame::CChildFrame()
{
	// TODO: add member initialization code here
}

CChildFrame::~CChildFrame()
{
}


BOOL CChildFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying the CREATESTRUCT cs
	if( !CMDIChildWndEx::PreCreateWindow(cs) )
		return FALSE;

	return TRUE;
}

// CChildFrame diagnostics

#ifdef _DEBUG
void CChildFrame::AssertValid() const
{
	CMDIChildWndEx::AssertValid();
}

void CChildFrame::Dump(CDumpContext& dc) const
{
	CMDIChildWndEx::Dump(dc);
}
#endif //_DEBUG

// CChildFrame message handlers


void CChildFrame::OnNcPaint()
{

}


void CChildFrame::ActivateFrame(int nCmdShow)
{
	CMDIChildWndEx::ActivateFrame(nCmdShow);
}


void CChildFrame::OnSetFocus(CWnd* pOldWnd)
{
	CMDIChildWndEx::OnSetFocus(pOldWnd);
}


BOOL CChildFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext *pContext)
{
	pContext->m_pNewViewClass = RUNTIME_CLASS(CSfxPackagerView);
	return CMDIChildWndEx::OnCreateClient(lpcs, pContext);
}

void CChildFrame::OnMDIActivate(BOOL activate, CWnd *activateWindow, CWnd *deactivateWindow)
{
	CMDIChildWndEx::OnMDIActivate(activate, activateWindow, deactivateWindow);
	CMainFrame *frame = DYNAMIC_DOWNCAST(CMainFrame, GetMDIFrame());
	if (!frame)
		return;

	if (activate)
		frame->GetScriptEditorWnd().SetDocument(DYNAMIC_DOWNCAST(CSfxPackagerDoc, GetActiveDocument()));
	else if (!activateWindow)
		frame->GetScriptEditorWnd().SetDocument(nullptr);
}
