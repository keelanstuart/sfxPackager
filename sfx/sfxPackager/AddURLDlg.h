#pragma once


// CAddURLDlg dialog

class CAddURLDlg : public CDialog
{
	DECLARE_DYNAMIC(CAddURLDlg)

public:
	CAddURLDlg(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CAddURLDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ADDURL_DLG };
#endif

	CString m_URL;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	CEdit m_Edit;

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
};
