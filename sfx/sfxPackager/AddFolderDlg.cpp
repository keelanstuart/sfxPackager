// AddFolderDlg.cpp : implementation file
//

#include "stdafx.h"
#include "sfxPackager.h"
#include "AddFolderDlg.h"
#include "afxdialogex.h"


// CAddFolderDlg dialog

IMPLEMENT_DYNAMIC(CAddFolderDlg, CDialog)

CAddFolderDlg::CAddFolderDlg(CWnd* pParent /*=nullptr*/)
	: CDialog(IDD_ADDFOLDER_DLG, pParent)
{
	m_Folder.Empty();
}

CAddFolderDlg::~CAddFolderDlg()
{
}

void CAddFolderDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_MFCEDITBROWSE_FOLDER, m_EditBrowse);
}


BEGIN_MESSAGE_MAP(CAddFolderDlg, CDialog)
END_MESSAGE_MAP()


// CAddFolderDlg message handlers


BOOL CAddFolderDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_EditBrowse.SetWindowText(m_Folder);

	return TRUE;  // return TRUE unless you set the focus to a control
				  // EXCEPTION: OCX Property Pages should return FALSE
}


void CAddFolderDlg::OnOK()
{
	m_EditBrowse.GetWindowText(m_Folder);

	CDialog::OnOK();
}
