/*
	Copyright © 2013-2020, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
	
	For inquiries, contact: keelanstuart@gmail.com
*/

// ProgressStatusBar.cpp : implementation file
//

#include "stdafx.h"
#include "sfxPackager.h"
#include "ProgressStatusBar.h"


// CProgressStatusBar

IMPLEMENT_DYNAMIC(CProgressStatusBar, CMFCStatusBar)

CProgressStatusBar::CProgressStatusBar()
{

}

CProgressStatusBar::~CProgressStatusBar()
{
}


BEGIN_MESSAGE_MAP(CProgressStatusBar, CMFCStatusBar)
	ON_WM_SIZE()
	ON_WM_CREATE()
	ON_MESSAGE(WM_UPDATE_STATUS, OnUpdateStatus)
END_MESSAGE_MAP()



// CProgressStatusBar message handlers




void CProgressStatusBar::OnSize(UINT nType, int cx, int cy)
{
	if (!GetSafeHwnd() || !m_ProgressBar.GetSafeHwnd())
		return;

	CMFCStatusBar::OnSize(nType, cx, cy);

	if (!GetCount())
		return;

	CRect rc;
	GetItemRect(0, &rc);

	m_ProgressBar.MoveWindow(&rc, FALSE);
}


int CProgressStatusBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	lpCreateStruct->style |= WS_CLIPCHILDREN | PBS_SMOOTH;

	if (CMFCStatusBar::OnCreate(lpCreateStruct) == -1)
		return -1;

	if (!m_ProgressBar.Create(WS_CHILD, CRect(), this, 1))
		return -1;

	m_ProgressBar.SetRange32(0, 100);

	return 0;
}

void CProgressStatusBar::SetProgress(UINT pct)
{
	DWORD oldstyle = m_ProgressBar.GetStyle();
	DWORD newstyle = oldstyle;

	UINT oldpct = m_ProgressBar.GetPos();

	if ((pct >= 0) && (pct <= 100))
	{
		newstyle |= WS_VISIBLE;
		m_ProgressBar.SetPos(max(1, pct));
	}
	else
	{
		newstyle &= ~WS_VISIBLE;
	}

	if ((oldstyle != newstyle) || (oldpct != pct))
	{
		SetWindowText(NULL);

		if (oldstyle != newstyle)
			SetWindowLong(m_ProgressBar.GetSafeHwnd(), GWL_STYLE, newstyle);
	}
}

void CProgressStatusBar::SetMarquee(bool on)
{
	DWORD oldstyle = m_ProgressBar.GetStyle();
	DWORD newstyle = oldstyle;

	if (on)
		newstyle |= PBS_MARQUEE | PBS_SMOOTH;
	else
		newstyle &= ~(PBS_MARQUEE | PBS_SMOOTH);

	if (oldstyle != newstyle)
		SetWindowLong(m_ProgressBar.GetSafeHwnd(), GWL_STYLE, newstyle);

	m_ProgressBar.SetMarquee(on, 50);
}

LRESULT CProgressStatusBar::OnUpdateStatus(WPARAM pct, LPARAM marquee)
{
	if ((INT32)pct < 0)
		ShowWindow(SW_HIDE);
	else if ((INT32)pct <= 100)
	{
		ShowWindow(SW_SHOW);
		SetProgress((UINT)pct);
	}

	SetMarquee(marquee != 0);

	return 0;
}
