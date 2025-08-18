/*
	Copyright © 2013-2025, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
	
	For inquiries, contact: keelanstuart@gmail.com
*/

#pragma once


// CProgressStatusBar

class CProgressStatusBar : public CMFCStatusBar
{
	DECLARE_DYNAMIC(CProgressStatusBar)

public:
	enum { WM_UPDATE_STATUS = WM_USER + 1 };
	CProgressStatusBar();
	virtual ~CProgressStatusBar();

	void SetProgress(UINT pct);
	void SetMarquee(bool on);

protected:
	CProgressCtrl m_ProgressBar;

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg LRESULT OnUpdateStatus(WPARAM pct, LPARAM marquee);
};


