// AddURLDlg.cpp : implementation file
//

#include "stdafx.h"
#include "sfxPackager.h"
#include "AddURLDlg.h"
#include "afxdialogex.h"


// CAddURLDlg dialog

IMPLEMENT_DYNAMIC(CAddURLDlg, CDialog)

CAddURLDlg::CAddURLDlg(CWnd* pParent /*=nullptr*/)
	: CDialog(IDD_ADDURL_DLG, pParent)
{
	m_URL.Empty();
}

CAddURLDlg::~CAddURLDlg()
{
}

void CAddURLDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_EDIT_URL, m_Edit);
}


BEGIN_MESSAGE_MAP(CAddURLDlg, CDialog)
END_MESSAGE_MAP()


// CAddURLDlg message handlers


BOOL CAddURLDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

	m_Edit.SetWindowText(m_URL);

    return TRUE;  // return TRUE unless you set the focus to a control
                  // EXCEPTION: OCX Property Pages should return FALSE
}


void CAddURLDlg::OnOK()
{
	m_Edit.GetWindowText(m_URL);

	CDialog::OnOK();
}
