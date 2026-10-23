#pragma once

#include <afxdockablepane.h>
#include "JsRichEditCtrl.h"

class CSfxPackagerDoc;

// Retain the existing file/class name; this is now a main-frame docking pane.
class CScriptEditView : public CDockablePane
{
public:
	CScriptEditView();
	BOOL HandleEditorNavigation(MSG *message);
	void SetDocument(CSfxPackagerDoc *document);
	void DetachDocument(CSfxPackagerDoc *document);
	void UpdateDocWithActiveScript();

protected:
	class ScriptEditor : public CJsRichEditCtrl
	{
	public:
		CScriptEditView *pane = nullptr;

	protected:
		void BuildContextMenu(CMenu &menu) override;
		bool HandleContextMenuCommand(UINT command) override;
		void OnTextChanged() override;
	};

	CComboBox m_cbScriptSelect;
	ScriptEditor m_reScriptEditor;
	CSfxPackagerDoc *m_Document = nullptr; // Borrowed; detached before document destruction.
	int m_LastSel = -1;
	bool m_Loading = false;

	void LoadSelectedScript();
	void Layout();
	void RegisterIntrinsics();

	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSetFocus(CWnd *oldWindow);
	afx_msg void OnScriptSelected();
	DECLARE_MESSAGE_MAP()
};
