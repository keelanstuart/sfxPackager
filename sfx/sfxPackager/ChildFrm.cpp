/*
	Copyright © 2013-2020, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
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
#include "CScriptEditView.h"
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
	ON_WM_SIZE()
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
	pContext->m_pNewViewClass = NULL;

	BOOL ret = CMDIChildWndEx::OnCreateClient(lpcs, pContext);

	if (ret)
	{
		// create splitter window
		ret &= m_wndSplitter.CreateStatic(this, 2, 1);

		if (ret)
		{
			CRect r;
			GetClientRect(r);

			if (!m_wndSplitter.CreateView(0, 0, RUNTIME_CLASS(CSfxPackagerView), CSize(r.Width(), r.Height() * 2 / 3), pContext) ||
				!m_wndSplitter.CreateView(1, 0, RUNTIME_CLASS(CScriptEditView), CSize(r.Width(), r.Height() / 3), pContext))
			{
				m_wndSplitter.DestroyWindow();
				ret = FALSE;
			}

			CSfxPackagerView *pv = dynamic_cast<CSfxPackagerView *>(m_wndSplitter.GetPane(0, 0));
			if (pv)
				pv->SetSplitterWnd(&m_wndSplitter);

			CScriptEditView *pe = dynamic_cast<CScriptEditView *>(m_wndSplitter.GetPane(1, 0));
			if (pe)
				pv->SetScriptEditView(pe);
		}
	}

	return ret;
}


void CChildFrame::OnSize(UINT nType, int cx, int cy)
{
	CMDIChildWndEx::OnSize(nType, cx, cy);

#if 0
	if (m_wndSplitter.GetSafeHwnd() && (nType != SIZE_MINIMIZED))
	{
		CRect cr;
		GetWindowRect(&cr);

		m_wndSplitter.SetRowInfo(0, cy * 2 / 3, cy / 10);
		m_wndSplitter.SetRowInfo(0, cy / 3, cy / 10);

		m_wndSplitter.RecalcLayout();
	}
#endif
}


void CChildFrame::RecalcLayout(BOOL bNotify)
{
	CMDIChildWndEx::RecalcLayout(bNotify);

#if 0
	if (m_wndSplitter.GetSafeHwnd())
	{
#if 0
		CRect r;
		GetClientRect(r);

		int cy, cymin;

		m_wndSplitter.GetRowInfo(0, cy, cymin);
		m_wndSplitter.GetPane(0, 0)->SetWindowPos(nullptr, 0, 0, r.Width(), cy, SWP_NOMOVE | SWP_NOZORDER);

		m_wndSplitter.GetRowInfo(1, cy, cymin);
		m_wndSplitter.GetPane(1, 0)->SetWindowPos(nullptr, 0, 0, r.Width(), cy, SWP_NOMOVE | SWP_NOZORDER);
#endif

		m_wndSplitter.RecalcLayout();
	}
#endif
}

CEditView *CChildFrame::GetScriptEditPane()
{
	CWnd* pWnd = m_wndSplitter.GetPane(1, 0);
	CEditView* pView = DYNAMIC_DOWNCAST(CEditView, pWnd);
	return pView;
}
