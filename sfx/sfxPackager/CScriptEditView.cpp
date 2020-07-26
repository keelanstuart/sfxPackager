// CScriptEditView.cpp : implementation file
//

#include "stdafx.h"
#include "sfxPackager.h"
#include "CScriptEditView.h"
#include "SfxPackagerDoc.h"


enum { CID_SELECTOR = 1, CID_HELPER, CID_HELPERFRAME, CID_EDITOR };

const TCHAR *helpers[CSfxPackagerDoc::EScriptType::NUMTYPES] =
{
	_T("var BASEPATH;\t// (string) the base install path"),
	_T("var BASEPATH;\t// (string) the base install path\r\nvar FILENAME;\t// (string) the name of the file that was just extracted\r\nvar PATH;\t// (string) the output path of that file\r\nvar FILEPATH;\t// (string) the full filename, PATH + FILENAME"),
	_T("var BASEPATH;\t// (string) the base install path\r\nvar CANCELLED;\t// (int) 1 if the installation was cancelled by the user, 0 if not\r\nvar INSTALLOK;\t// (int) 1 if the file extraction / installation was ok, 0 if not"),
};

// CScriptEditView

IMPLEMENT_DYNCREATE(CScriptEditView, CView)

CScriptEditView::CScriptEditView()
{

}

CScriptEditView::~CScriptEditView()
{
}

BEGIN_MESSAGE_MAP(CScriptEditView, CView)
	ON_WM_SIZE()
	ON_WM_CREATE()
	ON_CBN_SELCHANGE(CID_SELECTOR, OnScriptSelected)
END_MESSAGE_MAP()


// CScriptEditView drawing

// CScriptEditView diagnostics

#ifdef _DEBUG
void CScriptEditView::AssertValid() const
{
	CView::AssertValid();
}

#ifndef _WIN32_WCE
void CScriptEditView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}
#endif
#endif //_DEBUG


// CScriptEditView message handlers


void CScriptEditView::OnInitialUpdate()
{
	CView::OnInitialUpdate();

	CSfxPackagerDoc *doc = (CSfxPackagerDoc *)GetDocument();

	DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP;

	CRect r;
	r.SetRectEmpty();

	m_LastSel = -1;

	BOOL ret = TRUE;

	ret &= m_cbScriptSelect.Create(style | CBS_DROPDOWNLIST, r, this, CID_SELECTOR);
	if (ret)
	{
		int idx;

		idx = m_cbScriptSelect.AddString(_T("Initialization Script"));
		m_cbScriptSelect.SetItemData(idx, (DWORD_PTR)&(doc->m_Script[CSfxPackagerDoc::EScriptType::INIT]));

		idx = m_cbScriptSelect.AddString(_T("Per-File Script"));
		m_cbScriptSelect.SetItemData(idx, (DWORD_PTR)&(doc->m_Script[CSfxPackagerDoc::EScriptType::PERFILE]));

		idx = m_cbScriptSelect.AddString(_T("Finalization Script"));
		m_cbScriptSelect.SetItemData(idx, (DWORD_PTR)&(doc->m_Script[CSfxPackagerDoc::EScriptType::FINISH]));

		m_cbScriptSelect.SetCurSel(0);
		m_LastSel = 0;
	}

	ret &= m_stHelper.Create(_T("script help"), style | SS_LEFT, r, this, CID_HELPER);
	if (ret)
	{
		m_stHelper.SetWindowTextW(helpers[m_LastSel]);
	}

	ret &= m_stHelperFrame.Create(_T(""), style | SS_BLACKFRAME, r, this, CID_HELPERFRAME);
	if (ret)
	{
	}

	ret &= m_reScriptEditor.Create(style | ES_SELECTIONBAR | ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL, r, this, CID_EDITOR);
	if (ret)
	{
		// 1 MB of text... SHOULD be enough FFS!
		m_reScriptEditor.LimitText(1 << 20);

		m_reScriptEditor.SetSel(0, -1);
		int cbi = m_cbScriptSelect.GetCurSel();
		CString *s = (CString *)m_cbScriptSelect.GetItemData(cbi);
		m_reScriptEditor.ReplaceSel(s ? *s : _T(""));
	}

	if (doc && m_reScriptEditor.GetSafeHwnd() && m_cbScriptSelect.GetSafeHwnd())
	{
		int cbi = m_cbScriptSelect.GetCurSel();
		CSfxPackagerDoc::EScriptType t = (CSfxPackagerDoc::EScriptType)m_cbScriptSelect.GetItemData(cbi);
		//m_reScriptEditor.SetWindowText(doc->m_Script[t]);
	}
}


BOOL CScriptEditView::OnCreateAggregates()
{
	BOOL ret = true;

	CSfxPackagerDoc *doc = (CSfxPackagerDoc *)GetDocument();

	return ret;
}


BOOL CScriptEditView::OnChildNotify(UINT message, WPARAM wParam, LPARAM lParam, LRESULT *pLResult)
{
	// TODO: Add your specialized code here and/or call the base class

	return CView::OnChildNotify(message, wParam, lParam, pLResult);
}


void CScriptEditView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);

	CRect r;

	r.SetRectEmpty();

	r.right = cx;
	r.bottom = GetSystemMetrics(SM_CYMENU);
	if (m_cbScriptSelect.GetSafeHwnd())
	{
		m_cbScriptSelect.MoveWindow(r);
	}

	r.top = r.bottom + 4;
	r.bottom = (GetSystemMetrics(SM_CYMENU) * 6) + 2;
	if (m_stHelper.GetSafeHwnd())
	{
		CRect rh(r);
		rh.DeflateRect(3, 3, 3, 3);
		m_stHelper.MoveWindow(rh);
	}
	if (m_stHelperFrame.GetSafeHwnd())
	{
		m_stHelperFrame.MoveWindow(r);
	}

	r.top = r.bottom + 4;
	r.bottom = cy;
	if (m_reScriptEditor.GetSafeHwnd())
	{
		m_reScriptEditor.MoveWindow(r);
	}
}


void CScriptEditView::OnDraw(CDC *pDC)
{
	// TODO: Add your specialized code here and/or call the base class
}


int CScriptEditView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CView::OnCreate(lpCreateStruct) == -1)
		return -1;

	// TODO:  Add your specialized creation code here

	return 0;
}

void CScriptEditView::UpdateDocWithActiveScript()
{
	int cbi = m_cbScriptSelect.GetCurSel();
	CString *s = (CString *)m_cbScriptSelect.GetItemData(cbi);
	if (s)
	{
		CHARRANGE cr;
		m_reScriptEditor.GetSel(cr);

		m_reScriptEditor.SetSel(0, -1);

		*s = m_reScriptEditor.GetSelText();

		m_reScriptEditor.SetSel(cr);
	}
}


void CScriptEditView::OnScriptSelected()
{
	CString *s;

	// store the old script if we had one
	if (m_LastSel >= 0)
	{
		s = (CString *)m_cbScriptSelect.GetItemData(m_LastSel);
		if (s)
		{
			m_reScriptEditor.SetSel(0, -1);
			*s = m_reScriptEditor.GetSelText();
		}
	}

	m_reScriptEditor.SetSel(0, -1);

	int cbi = m_cbScriptSelect.GetCurSel();
	s = (CString *)m_cbScriptSelect.GetItemData(cbi);
	m_reScriptEditor.ReplaceSel(s ? *s : _T(""));

	m_LastSel = cbi;

	m_stHelper.SetWindowTextW(helpers[m_LastSel]);
}
