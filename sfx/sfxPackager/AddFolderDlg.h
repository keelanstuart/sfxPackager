#pragma once


// CAddFolderDlg dialog

class CAddFolderDlg : public CDialog
{
	DECLARE_DYNAMIC(CAddFolderDlg)

public:
	CAddFolderDlg(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CAddFolderDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ADDFOLDER_DLG };
#endif

	CString m_Folder;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	CMFCEditBrowseCtrl m_EditBrowse;

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
};
