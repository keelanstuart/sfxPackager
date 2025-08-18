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
#include "LicenseEntryDlg.h"
#include "afxdialogex.h"


// CLicenseKeyEntryDlg dialog

IMPLEMENT_DYNAMIC(CLicenseKeyEntryDlg, CDialog)

CLicenseKeyEntryDlg::CLicenseKeyEntryDlg(const TCHAR *description, CWnd *pParent)
	: CDialog(IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(_T("ICON"));

	m_Key = _T("");
	m_User = _T("");
	m_Org = _T("");

	m_Desc = description;
}

CLicenseKeyEntryDlg::~CLicenseKeyEntryDlg()
{
}

void CLicenseKeyEntryDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CLicenseKeyEntryDlg, CDialog)
END_MESSAGE_MAP()


const TCHAR *CLicenseKeyEntryDlg::GetKey()
{
	return m_Key;
}


const TCHAR *CLicenseKeyEntryDlg::GetUser()
{
	return m_User;
}


const TCHAR *CLicenseKeyEntryDlg::GetOrg()
{
	return m_Org;
}

// CLicenseKeyEntryDlg message handlers


void CLicenseKeyEntryDlg::OnOK()
{
	m_CtlUser.GetWindowText(m_User);
	m_CtlOrg.GetWindowText(m_Org);
	m_CtlKey.GetWindowText(m_Key);
	if (!m_Key.IsEmpty())
		CDialog::OnOK();
}


void CLicenseKeyEntryDlg::OnCancel()
{
	CDialog::OnCancel();
}


BOOL CLicenseKeyEntryDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	if (m_CtlDesc.CreateFromStatic(IDC_BROWSER, this))
	{
		m_CtlDesc.LoadFromResource(_T("license"));
	}

	if (m_CtlUser.SubclassDlgItem(IDC_EDIT_USERNAME, this))
	{
		m_CtlUser.SetWindowText(m_User);
	}

	if (m_CtlOrg.SubclassDlgItem(IDC_EDIT_ORGANIZATION, this))
	{
		m_CtlOrg.SetWindowText(m_Org);
	}

	if (m_CtlKey.SubclassDlgItem(IDC_EDIT_KEY, this))
	{
		m_CtlKey.SetWindowText(_T(""));
		m_CtlKey.SetFocus();
	}

	if (m_pDynamicLayout)
	{
		delete m_pDynamicLayout;
	}

	m_pDynamicLayout = new CMFCDynamicLayout();
	if (m_pDynamicLayout)
	{
		m_pDynamicLayout->Create(this);

		m_pDynamicLayout->AddItem(GetDlgItem(IDOK)->GetSafeHwnd(), CMFCDynamicLayout::MoveHorizontalAndVertical(100, 100), CMFCDynamicLayout::SizeNone());
		m_pDynamicLayout->AddItem(GetDlgItem(IDCANCEL)->GetSafeHwnd(), CMFCDynamicLayout::MoveHorizontalAndVertical(100, 100), CMFCDynamicLayout::SizeNone());
		m_pDynamicLayout->AddItem(GetDlgItem(IDC_EDIT_USERNAME)->GetSafeHwnd(), CMFCDynamicLayout::MoveVertical(100), CMFCDynamicLayout::SizeHorizontal(100));
		m_pDynamicLayout->AddItem(GetDlgItem(IDC_EDIT_ORGANIZATION)->GetSafeHwnd(), CMFCDynamicLayout::MoveVertical(100), CMFCDynamicLayout::SizeHorizontal(100));
		m_pDynamicLayout->AddItem(GetDlgItem(IDC_EDIT_KEY)->GetSafeHwnd(), CMFCDynamicLayout::MoveVertical(100), CMFCDynamicLayout::SizeHorizontal(100));
		m_pDynamicLayout->AddItem(GetDlgItem(IDC_STATIC_NAME)->GetSafeHwnd(), CMFCDynamicLayout::MoveVertical(100), CMFCDynamicLayout::SizeNone());
		m_pDynamicLayout->AddItem(GetDlgItem(IDC_STATIC_ORG)->GetSafeHwnd(), CMFCDynamicLayout::MoveVertical(100), CMFCDynamicLayout::SizeNone());
		m_pDynamicLayout->AddItem(GetDlgItem(IDC_STATIC_KEY)->GetSafeHwnd(), CMFCDynamicLayout::MoveVertical(100), CMFCDynamicLayout::SizeNone());
		m_pDynamicLayout->AddItem(GetDlgItem(IDC_BROWSER)->GetSafeHwnd(), CMFCDynamicLayout::MoveNone(), CMFCDynamicLayout::SizeHorizontalAndVertical(100, 100));
	}

	ShowWindow(SW_SHOWNORMAL);

	return FALSE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX Property Pages should return FALSE
}
