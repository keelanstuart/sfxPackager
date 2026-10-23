#include "stdafx.h"
#include "sfxPackager.h"
#include "CScriptEditView.h"
#include "sfxPackagerDoc.h"

namespace
{
	enum
	{
		CID_STAGE = 1,
		CID_SELECTOR,
		CID_EDITOR
	};

	struct ScriptVariable
	{
		LPCTSTR name;
		LPCTSTR description;
		unsigned stages;
	};

	constexpr unsigned FileStages = (1U << CSfxPackagerDoc::PREFILE) |
		(1U << CSfxPackagerDoc::POSTFILE);
	constexpr unsigned InstallStages = FileStages |
		(1U << CSfxPackagerDoc::PREINSTALL) | (1U << CSfxPackagerDoc::POSTINSTALL);

	// Match the preambles in sfx/ProgressDlg.cpp. Insert references, not declarations.
	const ScriptVariable Variables[] =
	{
		{ _T("BASEPATH"), _T("Base install path"), InstallStages },
		{ _T("FILENAME"), _T("Current file name"), FileStages },
		{ _T("PATH"), _T("Current file's output directory"), FileStages },
		{ _T("FILEPATH"), _T("Current file's full output path"), FileStages },
		{ _T("CANCELLED"), _T("1 if cancelled; otherwise 0"), 1U << CSfxPackagerDoc::POSTINSTALL },
		{ _T("INSTALLOK"), _T("1 if installation succeeded; otherwise 0"), 1U << CSfxPackagerDoc::POSTINSTALL }
	};
}

BEGIN_MESSAGE_MAP(CScriptEditView, CDockablePane)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_SETFOCUS()
	ON_CBN_SELCHANGE(CID_SELECTOR, OnScriptSelected)
END_MESSAGE_MAP()

CScriptEditView::CScriptEditView()
{
	m_reScriptEditor.pane = this;
}

int CScriptEditView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;

	CRect rect(0, 0, 0, 0);
	const DWORD style = WS_CHILD | WS_VISIBLE;
	ModifyStyleEx(0, WS_EX_CONTROLPARENT);
	if (!m_cbScriptSelect.Create(style | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, rect, this, CID_SELECTOR) ||
		!m_reScriptEditor.Create(this, rect, CID_EDITOR))
		return -1;

	m_cbScriptSelect.SetFont(&afxGlobalData.fontRegular);
	m_cbScriptSelect.AddString(_T("Initialization Script"));
	m_cbScriptSelect.AddString(_T("Pre-Install Script"));
	m_cbScriptSelect.AddString(_T("Pre-File Script"));
	m_cbScriptSelect.AddString(_T("Post-File Script"));
	m_cbScriptSelect.AddString(_T("Post-Install Script"));
	m_reScriptEditor.GetEdit().LimitText(1 << 20);
	RegisterIntrinsics();
	LoadSelectedScript();
	Layout();
	return 0;
}

void CScriptEditView::SetDocument(CSfxPackagerDoc *document)
{
	if (m_Document == document)
		return;

	UpdateDocWithActiveScript();
	m_Document = document;
	m_LastSel = document ? document->m_SelectedScriptStage : -1;
	if (m_LastSel < 0 || m_LastSel >= CSfxPackagerDoc::NUMTYPES)
		m_LastSel = document ? CSfxPackagerDoc::INITIALIZE : -1;
	LoadSelectedScript();
}

void CScriptEditView::DetachDocument(CSfxPackagerDoc *document)
{
	if (m_Document == document)
		SetDocument(nullptr);
}

void CScriptEditView::LoadSelectedScript()
{
	if (!m_reScriptEditor.GetSafeHwnd())
		return;

	m_Loading = true;
	m_cbScriptSelect.SetCurSel(m_LastSel);
	m_cbScriptSelect.EnableWindow(m_Document != nullptr);
	m_reScriptEditor.SetText(m_Document && m_LastSel >= 0 ?
		m_Document->m_Script[m_LastSel].GetString() : _T(""));
	m_reScriptEditor.SetReadOnly(m_Document == nullptr);
	m_reScriptEditor.EnableWindow(m_Document != nullptr);
	m_Loading = false;
}

void CScriptEditView::UpdateDocWithActiveScript()
{
	if (m_Loading || !m_Document || m_LastSel < 0 ||
		m_LastSel >= CSfxPackagerDoc::NUMTYPES || !m_reScriptEditor.GetSafeHwnd())
		return;

	CString text = m_reScriptEditor.GetText();
	// Rich Edit normalizes newlines; do not dirty an untouched project on a stage switch.
	CString stored = m_Document->m_Script[m_LastSel];
	stored.Replace(_T("\r\n"), _T("\n"));
	stored.Replace(_T("\r"), _T("\n"));
	stored.Replace(_T("\n"), _T("\r\n"));
	if (stored != text)
	{
		{
			std::lock_guard<std::mutex> lock(m_Document->m_ScriptMutex);
			m_Document->m_Script[m_LastSel] = text;
		}
		m_Document->SetModifiedFlag(TRUE);
	}
}

void CScriptEditView::OnScriptSelected()
{
	const int selected = m_cbScriptSelect.GetCurSel();
	if (!m_Document || selected < 0 || selected >= CSfxPackagerDoc::NUMTYPES || selected == m_LastSel)
		return;

	UpdateDocWithActiveScript();
	m_LastSel = selected;
	m_Document->m_SelectedScriptStage = selected;
	LoadSelectedScript();
	m_reScriptEditor.SetFocus();
}

void CScriptEditView::OnSize(UINT nType, int cx, int cy)
{
	CDockablePane::OnSize(nType, cx, cy);
	Layout();
}

void CScriptEditView::Layout()
{
	if (!m_cbScriptSelect.GetSafeHwnd() || !m_reScriptEditor.GetSafeHwnd())
		return;

	CRect rect;
	GetClientRect(rect);

	CRect cbr;
	m_cbScriptSelect.GetClientRect(cbr);

	CClientDC dc(this);

	// A combo's window height includes its drop-down list.
	m_cbScriptSelect.MoveWindow(0, 0, std::max<int>(0, rect.Width()), cbr.Height());
	m_reScriptEditor.MoveWindow(0, cbr.Height(), rect.Width(), std::max<int>(0, rect.Height() - cbr.Height()));
}

void CScriptEditView::OnSetFocus(CWnd *oldWindow)
{
	CDockablePane::OnSetFocus(oldWindow);
	if (m_Document && m_reScriptEditor.GetSafeHwnd())
		m_reScriptEditor.SetFocus();
}

BOOL CScriptEditView::HandleEditorNavigation(MSG *message)
{
	if (message->message == WM_KEYDOWN && message->wParam == VK_TAB)
	{
		CWnd *focus = GetFocus();
		if (focus == &m_cbScriptSelect)
		{
			m_reScriptEditor.SetFocus();
			return TRUE;
		}
		if ((GetKeyState(VK_SHIFT) & 0x8000) &&
			message->hwnd == m_reScriptEditor.GetEdit().GetSafeHwnd())
		{
			m_cbScriptSelect.SetFocus();
			return TRUE;
		}
	}

	return FALSE;
}

void CScriptEditView::ScriptEditor::OnTextChanged()
{
	pane->UpdateDocWithActiveScript();
}

void CScriptEditView::ScriptEditor::BuildContextMenu(CMenu &menu)
{
	CMenu variables;
	if (!variables.CreatePopupMenu())
		return;

	const unsigned stage = pane->m_Document && pane->m_LastSel >= 0 ? 1U << pane->m_LastSel : 0;
	const bool writable = pane->m_Document && !(GetEdit().GetStyle() & ES_READONLY);
	for (UINT i = 0; i < _countof(Variables); i++)
	{
		if (!(Variables[i].stages & stage))
			continue;

		CString label;
		label.Format(_T("%s - %s"), Variables[i].name, Variables[i].description);
		variables.AppendMenu(MF_STRING | (writable ? MF_ENABLED : MF_GRAYED), ContextMenuCommandFirst + i, label);
	}
	if (variables.GetMenuItemCount() == 0)
		variables.AppendMenu(MF_STRING | MF_GRAYED, ContextMenuCommandLast,
			_T("No implicit variables in this stage"));

	menu.AppendMenu(MF_SEPARATOR);
	if (menu.AppendMenu(MF_POPUP | MF_STRING, reinterpret_cast<UINT_PTR>(variables.GetSafeHmenu()),
		_T("Insert &variable")))
		variables.Detach(); // The parent popup now owns and destroys this submenu.
}

bool CScriptEditView::ScriptEditor::HandleContextMenuCommand(UINT command)
{
	if (command < ContextMenuCommandFirst || command >= ContextMenuCommandFirst + _countof(Variables))
		return false;

	const UINT index = command - ContextMenuCommandFirst;
	if (pane->m_Document && pane->m_LastSel >= 0 &&
		(Variables[index].stages & (1U << pane->m_LastSel)) && !(GetEdit().GetStyle() & ES_READONLY))
	{
		GetEdit().SetFocus();
		GetEdit().ReplaceSel(Variables[index].name, TRUE);
	}
	return true;
}

void CScriptEditView::RegisterIntrinsics()
{
	m_reScriptEditor.RegisterIntrinsic(_T("AbortInstall"), _T(""), _T("Immediately terminates the installer."));
	m_reScriptEditor.RegisterIntrinsic(_T("Array.contains"), _T("obj"), _T("Tests whether the array contains a matching value."));
	m_reScriptEditor.RegisterIntrinsic(_T("Array.join"), _T("separator"), _T("Joins array elements into a string with a separator."));
	m_reScriptEditor.RegisterIntrinsic(_T("Array.remove"), _T("obj"), _T("Removes matching array values and renumbers indices."));
	m_reScriptEditor.RegisterIntrinsic(_T("charToInt"), _T("ch"), _T("Returns the first character's numeric code, or 0 for an empty string."));
	m_reScriptEditor.RegisterIntrinsic(_T("CompareStrings"), _T("str1, str2"), _T("Compares strings case-sensitively; returns negative, zero, or positive."));
	m_reScriptEditor.RegisterIntrinsic(_T("CopyFile"), _T("src, dst"), _T("Copies a file, overwriting an existing destination."));
	m_reScriptEditor.RegisterIntrinsic(_T("CreateDirectoryTree"), _T("path"), _T("Creates a directory and any missing parent directories."));
	m_reScriptEditor.RegisterIntrinsic(_T("CreateShortcut"), _T("file, target, args, rundir, desc, showmode, icon, iconidx"), _T("Creates a Windows shortcut with the specified launch settings."));
	m_reScriptEditor.RegisterIntrinsic(_T("CreateSymbolicLink"), _T("linkname, targetname"), _T("Creates a symbolic link to a file or directory."));
	m_reScriptEditor.RegisterIntrinsic(_T("DeleteDirectory"), _T("path"), _T("Recursively deletes a directory and its contents."));
	m_reScriptEditor.RegisterIntrinsic(_T("DeleteFile"), _T("path"), _T("Deletes a file or wildcard-matched files without recursion."));
	m_reScriptEditor.RegisterIntrinsic(_T("DeleteRegistryKey"), _T("root, key, subkey"), _T("Deletes a registry subkey under the specified root and key."));
	m_reScriptEditor.RegisterIntrinsic(_T("DownloadFile"), _T("url, file"), _T("Downloads a URL to a local file."));
	m_reScriptEditor.RegisterIntrinsic(_T("Echo"), _T("msg"), _T("Writes a message to installer output, expanding environment and registry references."));
	m_reScriptEditor.RegisterIntrinsic(_T("eval"), _T("jsCode"), _T("Evaluates an expression and returns its result."));
	m_reScriptEditor.RegisterIntrinsic(_T("exec"), _T("jsCode"), _T("Executes script code."));
	m_reScriptEditor.RegisterIntrinsic(_T("FileExists"), _T("path"), _T("Tests whether a file or directory exists."));
	m_reScriptEditor.RegisterIntrinsic(_T("FilenameHasExtension"), _T("filename, ext"), _T("Tests an extension case-insensitively; ext excludes the leading dot."));
	m_reScriptEditor.RegisterIntrinsic(_T("GetCurrentDateString"), _T(""), _T("Returns the local date as YYYY/MM/DD."));
	m_reScriptEditor.RegisterIntrinsic(_T("GetFileNameFromPath"), _T("filepath"), _T("Returns the filename following the final path separator."));
	m_reScriptEditor.RegisterIntrinsic(_T("GetGlobalEnvironmentVariable"), _T("varname"), _T("Reads an environment variable from the current process."));
	m_reScriptEditor.RegisterIntrinsic(_T("GetGlobalInt"), _T("name"), _T("Returns a named installer integer, or 0 if absent."));
	m_reScriptEditor.RegisterIntrinsic(_T("GetExeVersion"), _T("file"), _T("Returns the file version as major.minor.build.revision."));
	m_reScriptEditor.RegisterIntrinsic(_T("GetLicenseKey"), _T(""), _T("Returns the license key entered in the license dialog."));
	m_reScriptEditor.RegisterIntrinsic(_T("GetLicenseOrg"), _T(""), _T("Returns the organization entered in the license dialog."));
	m_reScriptEditor.RegisterIntrinsic(_T("GetLicenseUser"), _T(""), _T("Returns the user name entered in the license dialog."));
	m_reScriptEditor.RegisterIntrinsic(_T("GetProperty"), _T("name"), _T("Returns a named installer property's value."));
	m_reScriptEditor.RegisterIntrinsic(_T("GetRegistryKeyValue"), _T("root, key, name"), _T("Reads a registry string, DWORD, or QWORD value."));
	m_reScriptEditor.RegisterIntrinsic(_T("Integer.parseInt"), _T("str"), _T("Parses an integer with automatic base detection."));
	m_reScriptEditor.RegisterIntrinsic(_T("Integer.valueOf"), _T("str"), _T("Returns a single character's numeric code, otherwise 0."));
	m_reScriptEditor.RegisterIntrinsic(_T("IsDirectory"), _T("path"), _T("Tests whether a path identifies a directory."));
	m_reScriptEditor.RegisterIntrinsic(_T("IsDirectoryEmpty"), _T("path"), _T("Tests whether a directory is empty."));
	m_reScriptEditor.RegisterIntrinsic(_T("JSON.stringify"), _T("obj, replacer"), _T("Serializes a value to JSON; replacer is ignored."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.abs"), _T("a"), _T("Returns the absolute value."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.acos"), _T("a"), _T("Returns the inverse cosine in radians."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.acosh"), _T("a"), _T("Returns the inverse hyperbolic cosine."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.asin"), _T("a"), _T("Returns the inverse sine in radians."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.asinh"), _T("a"), _T("Returns the inverse hyperbolic sine."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.atan"), _T("a"), _T("Returns the inverse tangent in radians."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.atanh"), _T("a"), _T("Intended as inverse hyperbolic tangent; currently computes atan."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.cos"), _T("a"), _T("Returns the cosine of an angle in radians."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.cosh"), _T("a"), _T("Returns the hyperbolic cosine."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.E"), _T(""), _T("Returns Euler's number e."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.exp"), _T("a"), _T("Returns e raised to the power a."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.log"), _T("a"), _T("Returns the natural logarithm."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.log10"), _T("a"), _T("Returns the base-10 logarithm."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.min"), _T("a, b"), _T("Returns the smaller value."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.max"), _T("a, b"), _T("Returns the larger value."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.PI"), _T(""), _T("Returns pi."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.pow"), _T("a, b"), _T("Returns a raised to the power b."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.range"), _T("x, a, b"), _T("Clamps x between a and b."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.rand"), _T(""), _T("Returns a random number from 0 to 1 inclusive."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.randInt"), _T("min, max"), _T("Returns a random integer between min and max inclusive."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.round"), _T("a"), _T("Rounds to the nearest integer, with ties away from zero."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.sign"), _T("a"), _T("Returns -1, 0, or 1 according to the value's sign."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.sin"), _T("a"), _T("Returns the sine of an angle in radians."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.sinh"), _T("a"), _T("Returns the hyperbolic sine."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.sqr"), _T("a"), _T("Returns the square of a."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.sqrt"), _T("a"), _T("Returns the square root of a."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.tan"), _T("a"), _T("Returns the tangent of an angle in radians."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.tanh"), _T("a"), _T("Returns the hyperbolic tangent."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.toDegrees"), _T("a"), _T("Converts radians to degrees."));
	m_reScriptEditor.RegisterIntrinsic(_T("Math.toRadians"), _T("a"), _T("Converts degrees to radians."));
	m_reScriptEditor.RegisterIntrinsic(_T("MessageBox"), _T("title, msg"), _T("Displays a message box with an OK button."));
	m_reScriptEditor.RegisterIntrinsic(_T("MessageBoxYesNo"), _T("title, msg"), _T("Displays a Yes/No prompt; returns 1 for Yes, otherwise 0."));
	m_reScriptEditor.RegisterIntrinsic(_T("Object.dump"), _T(""), _T("Prints the object's contents."));
	m_reScriptEditor.RegisterIntrinsic(_T("Object.clone"), _T(""), _T("Returns a copy of the object."));
	m_reScriptEditor.RegisterIntrinsic(_T("RegistryKeyValueExists"), _T("root, key, name"), _T("Tests whether a named registry value exists."));
	m_reScriptEditor.RegisterIntrinsic(_T("RenameFile"), _T("filename, newname"), _T("Renames or moves a file."));
	m_reScriptEditor.RegisterIntrinsic(_T("SetGlobalEnvironmentVariable"), _T("varname, val"), _T("Writes a machine-wide environment variable to the registry."));
	m_reScriptEditor.RegisterIntrinsic(_T("SetGlobalInt"), _T("name, val"), _T("Stores a named installer integer."));
	m_reScriptEditor.RegisterIntrinsic(_T("SetProperty"), _T("name, type, aspect, value"), _T("Creates or updates an installer property with a type and display aspect."));
	m_reScriptEditor.RegisterIntrinsic(_T("SetRegistryKeyValue"), _T("root, key, name, val"), _T("Writes a registry string value, creating the key if needed."));
	m_reScriptEditor.RegisterIntrinsic(_T("ShowLicenseAcceptanceDlg"), _T(""), _T("Displays license acceptance; cancelling terminates the installer."));
	m_reScriptEditor.RegisterIntrinsic(_T("ShowLicenseDlg"), _T(""), _T("Displays license entry; cancelling terminates the installer."));
	m_reScriptEditor.RegisterIntrinsic(_T("SpawnProcess"), _T("cmd, params, rundir, block"), _T("Launches a process with elevation, optionally waiting for it to exit."));
	m_reScriptEditor.RegisterIntrinsic(_T("String.charAt"), _T("pos"), _T("Returns the character at the zero-based position."));
	m_reScriptEditor.RegisterIntrinsic(_T("String.charCodeAt"), _T("pos"), _T("Returns the character code at the zero-based position."));
	m_reScriptEditor.RegisterIntrinsic(_T("String.fromCharCode"), _T("char"), _T("Creates a string from a character code."));
	m_reScriptEditor.RegisterIntrinsic(_T("String.includes"), _T("str, sensitive"), _T("Tests for a substring with selectable case sensitivity."));
	m_reScriptEditor.RegisterIntrinsic(_T("String.indexOf"), _T("search"), _T("Returns the first matching position, or -1 if absent."));
	m_reScriptEditor.RegisterIntrinsic(_T("String.split"), _T("separator"), _T("Splits the string into an array using a separator."));
	m_reScriptEditor.RegisterIntrinsic(_T("String.substring"), _T("lo, hi"), _T("Returns characters from lo through hi exclusive."));
	m_reScriptEditor.RegisterIntrinsic(_T("TextFileClose"), _T("handle"), _T("Closes an open text file."));
	m_reScriptEditor.RegisterIntrinsic(_T("TextFileOpen"), _T("filename, mode"), _T("Opens a text file using a C runtime mode string; returns a handle."));
	m_reScriptEditor.RegisterIntrinsic(_T("TextFileReachedEOF"), _T("handle"), _T("Tests whether end-of-file has been encountered."));
	m_reScriptEditor.RegisterIntrinsic(_T("TextFileReadLn"), _T("handle"), _T("Reads the next line from an open text file."));
	m_reScriptEditor.RegisterIntrinsic(_T("TextFileWrite"), _T("handle, text"), _T("Writes text, expanding environment and registry references."));
	m_reScriptEditor.RegisterIntrinsic(_T("ToLower"), _T("str"), _T("Returns the string converted to lowercase."));
	m_reScriptEditor.RegisterIntrinsic(_T("ToUpper"), _T("str"), _T("Returns the string converted to uppercase."));
	m_reScriptEditor.RegisterIntrinsic(_T("trace"), _T(""), _T("Prints the interpreter's root variables."));
}
