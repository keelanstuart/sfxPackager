/*
	Copyright © 2013-2020, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
	
	For inquiries, contact: keelanstuart@gmail.com
*/

#pragma once

#include <vector>
#include "ShellTree.h"

typedef std::vector<UINT> TUintVector;

// CRepathDlg dialog

class CRepathDlg : public CDialog
{
	DECLARE_DYNAMIC(CRepathDlg)

public:
	CRepathDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CRepathDlg();

	CString m_Path;

// Dialog Data
	enum { IDD = IDD_REPATH_DLG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	CEdit m_OrigPath;
	CSpinButtonCtrl m_OrigSubDirSpin;
	CShellTree m_ShellTree;
	CEdit m_ReplaceWithPath;
	CEdit m_ResultPath;

	TUintVector m_SlashPositions;
	int m_SubDir;

	bool b_LockReplace;

	void UpdateResultPath();

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnSelchangedShelltreeReplace(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnDeltaposSpinOrigsubdir(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnChangeEditReplacewith();
};
