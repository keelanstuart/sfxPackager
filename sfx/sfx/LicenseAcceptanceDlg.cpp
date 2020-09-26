/*
	Copyright © 2013-2020, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
	Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
	provided that the above copyright notice appears in all copies, modifications, and distributions.
	Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
	any fitness of purpose of this software.
	All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.

	For inquiries, contact: keelanstuart@gmail.com
*/


#include "stdafx.h"
#include "sfx.h"
#include "LicenseAcceptanceDlg.h"
#include "afxdialogex.h"


// CLicenseAcceptanceDlg dialog

IMPLEMENT_DYNAMIC(CLicenseAcceptanceDlg, CDialog)

CLicenseAcceptanceDlg::CLicenseAcceptanceDlg(const TCHAR *description, CWnd *pParent)
	: CDialog(IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(_T("ICON"));

	m_Desc = description;
}

CLicenseAcceptanceDlg::~CLicenseAcceptanceDlg()
{
}

void CLicenseAcceptanceDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CLicenseAcceptanceDlg, CDialog)
	ON_BN_CLICKED(IDC_CHECK_ACCEPTLICENSE, &CLicenseAcceptanceDlg::OnBnClickedCheckAcceptlicense)
END_MESSAGE_MAP()


// CLicenseAcceptanceDlg message handlers


void CLicenseAcceptanceDlg::OnCancel()
{
	CDialog::OnCancel();
}


BOOL CLicenseAcceptanceDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	m_CtlOk.SubclassDlgItem(IDOK, this);

	if (m_CtlDesc.CreateFromStatic(IDC_BROWSER, this))
	{
		m_CtlDesc.LoadFromResource(_T("license"));
	}

	if (m_CtlAcceptCB.SubclassDlgItem(IDC_CHECK_ACCEPTLICENSE, this))
	{
		m_CtlOk.EnableWindow(false);
	}

	ShowWindow(SW_SHOWNORMAL);

	return FALSE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX Property Pages should return FALSE
}


void CLicenseAcceptanceDlg::OnBnClickedCheckAcceptlicense()
{
	m_CtlOk.EnableWindow((m_CtlAcceptCB.GetCheck() > 0) ? TRUE : FALSE);
}
