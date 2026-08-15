/*
	Copyright © 2013-2025, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.

	For inquiries, contact: keelanstuart@gmail.com
*/


#include "stdafx.h"
#include "sfx.h"
#include "PasswordDlg.h"
#include "afxdialogex.h"


// CPasswordDlg dialog

IMPLEMENT_DYNAMIC(CPasswordDlg, CDialog)

CPasswordDlg::CPasswordDlg(CWnd *pParent)
	: CDialog(IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(_T("ICON"));
}

CPasswordDlg::~CPasswordDlg()
{
}

void CPasswordDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Text(pDX, IDC_EDIT_PASSWORD, m_Password);
}


BEGIN_MESSAGE_MAP(CPasswordDlg, CDialog)
	ON_EN_CHANGE(IDC_EDIT_PASSWORD, &CPasswordDlg::OnChangeEditPassword)
	ON_BN_CLICKED(IDOK, &CPasswordDlg::OnBnClickedOk)
END_MESSAGE_MAP()


// CPasswordDlg message handlers


void CPasswordDlg::OnCancel()
{
	CDialog::OnCancel();
}


BOOL CPasswordDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	m_CtlOk.SubclassDlgItem(IDOK, this);

	ShowWindow(SW_SHOWNORMAL);

	return FALSE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

void CPasswordDlg::OnChangeEditPassword()
{
	CString password;
	GetDlgItemText(IDC_EDIT_PASSWORD, password);

	GetDlgItem(IDOK)->EnableWindow(!password.IsEmpty());
}

void CPasswordDlg::OnBnClickedOk()
{
	UpdateData(TRUE);
	theApp.m_Password = m_Password;

	CDialog::OnOK();
}
