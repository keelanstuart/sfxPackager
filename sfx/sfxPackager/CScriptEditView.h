#pragma once


// CScriptEditView view

class CScriptEditView : public CView
{
	DECLARE_DYNCREATE(CScriptEditView)

protected:
	CScriptEditView();           // protected constructor used by dynamic creation
	virtual ~CScriptEditView();


public:
	CComboBox m_cbScriptSelect;
	CStatic m_stHelper, m_stHelperFrame;
	CRichEditCtrl m_reScriptEditor;
	int m_LastSel;

	void UpdateDocWithActiveScript();

#ifdef _DEBUG
	virtual void AssertValid() const;
#ifndef _WIN32_WCE
	virtual void Dump(CDumpContext& dc) const;
#endif
#endif

protected:
	DECLARE_MESSAGE_MAP()
public:
	virtual void OnInitialUpdate();
	virtual BOOL OnCreateAggregates();
	virtual BOOL OnChildNotify(UINT message, WPARAM wParam, LPARAM lParam, LRESULT *pLResult);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual void OnDraw(CDC * /*pDC*/);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnScriptSelected();
};


