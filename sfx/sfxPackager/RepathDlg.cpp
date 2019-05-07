/*
	Copyright ©2016. Authored by Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
	
	For inquiries, contact: keelanstuart@gmail.com
*/

// RepathDlg.cpp : implementation file
//

#include "stdafx.h"
#include "sfxPackager.h"
#include "RepathDlg.h"
#include "afxdialogex.h"


// CRepathDlg dialog

IMPLEMENT_DYNAMIC(CRepathDlg, CDialog)

CRepathDlg::CRepathDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CRepathDlg::IDD, pParent)
{
	b_LockReplace = false;
}

CRepathDlg::~CRepathDlg()
{
}

void CRepathDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_EDIT_ORIGPATH, m_OrigPath);
	DDX_Control(pDX, IDC_SPIN_ORIGSUBDIR, m_OrigSubDirSpin);
	DDX_Control(pDX, IDC_SHELLTREE_REPLACEWITH, m_ShellTree);
	DDX_Control(pDX, IDC_EDIT_REPLACEWITH, m_ReplaceWithPath);
	DDX_Control(pDX, IDC_EDIT_RESULT, m_ResultPath);
}


BEGIN_MESSAGE_MAP(CRepathDlg, CDialog)
	ON_NOTIFY(TVN_SELCHANGED, IDC_SHELLTREE_REPLACEWITH, &CRepathDlg::OnSelchangedShelltreeReplace)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_ORIGSUBDIR, &CRepathDlg::OnDeltaposSpinOrigsubdir)
	ON_EN_CHANGE(IDC_EDIT_REPLACEWITH, &CRepathDlg::OnChangeEditReplacewith)
END_MESSAGE_MAP()


// CRepathDlg message handlers


BOOL CRepathDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	CComboBox *pcb = (CComboBox *)GetDlgItem(IDC_COMBO_PATHTYPE);
	if (pcb)
		pcb->SetCurSel(0);

	m_OrigPath.SetWindowText(m_Path);
	m_SlashPositions.reserve(m_Path.GetLength());
	const TCHAR *slashes = (LPCTSTR)m_Path;
	const TCHAR *s = slashes;
	while (slashes && (*slashes == _T('\\')) && *slashes)
	{
		slashes++;
	}

	while (slashes && *slashes)
	{
		if (*slashes == _T('\\'))
		{
			m_SlashPositions.push_back(slashes - s);
		}

		slashes++;
	}

	m_OrigSubDirSpin.SetRange32(0, m_SlashPositions.size() + 1);
	m_OrigSubDirSpin.SetPos(0);

	m_SubDir = m_SlashPositions.size();
	m_OrigPath.SetSel(0, -1, TRUE);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}


void CRepathDlg::OnOK()
{
	CString s;
	m_ResultPath.GetWindowText(s);

	BOOL network_path = PathIsNetworkPath(s);
	BOOL directory = PathIsDirectory(s);
	BOOL exists = PathFileExists(s);
	if (!(network_path || directory))
	{
		CString msg;
		msg.Format(_T("The path you have chosen (\"%s\") does not exist.\r\nPlease select another or cancel."), s);
		MessageBox(msg, _T("Invalid Path"), MB_OK);

		return;
	}

	m_Path = s;

	CDialog::OnOK();
}

void CRepathDlg::UpdateResultPath()
{
	int ssel, esel;
	m_OrigPath.GetSel(ssel, esel);

	CString origpath;
	m_OrigPath.GetWindowText(origpath);

	CString resultsubdir = origpath.Right(origpath.GetLength() - esel);

	CString result;
	m_ReplaceWithPath.GetWindowText(result);

	result += resultsubdir;

	m_ResultPath.SetWindowText(result);
}

void CRepathDlg::OnSelchangedShelltreeReplace(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);

	b_LockReplace = true;

	HTREEITEM hsel = m_ShellTree.GetNextItem(TVI_ROOT, TVGN_CARET);
	CString result;
	m_ShellTree.GetItemPath(result, hsel);
	m_ReplaceWithPath.SetWindowText(result);

	UpdateResultPath();

	b_LockReplace = false;

	*pResult = 0;
}


void CRepathDlg::OnDeltaposSpinOrigsubdir(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);

	m_SubDir = min(max(0, (m_SubDir + pNMUpDown->iDelta)), m_SlashPositions.size());

	int endsel = (m_SubDir < m_SlashPositions.size()) ? m_SlashPositions[m_SubDir] : -1;

	m_OrigPath.SetSel(0, endsel, TRUE);

	UpdateResultPath();

	*pResult = 0;
}


void CRepathDlg::OnChangeEditReplacewith()
{
	if (!b_LockReplace)
	{
		CString s;
		m_ReplaceWithPath.GetWindowText(s);

		if (PathIsDirectory(s))
			m_ShellTree.SelectPath(s);

		UpdateResultPath();
	}
}
