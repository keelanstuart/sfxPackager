#pragma once

#include <afxwin.h>
#include <afxcmn.h>
#include <afxrich.h>
#include <vector>

// Resource-free MFC script editor. All public operations, including destruction,
// must take place on the window's owning UI thread. Requires C++17 or later.
// Add Pool.h to the include path; define POOL_STATIC when linking static pool.
class CJsRichEditCtrl : public CWnd
{
public:
    enum class ColorItem
    {
        Background,
        Text,
        Keyword,
        Number,
        String,
        Comment,
        Function,
        Intrinsic,
        Operator,
        GutterBackground,
        LineNumber,
        Breakpoint,
        Count
    };

    struct ColorConfig
    {
        COLORREF values[static_cast<int>(ColorItem::Count)];
        ColorConfig();
        COLORREF& operator[](ColorItem item);
        COLORREF operator[](ColorItem item) const;
    };

    struct IntrinsicFunction
    {
        CString name;          // Case-sensitive; qualified names are supported.
        CString parameters;    // For example: "hobj, name, recursive"; no parentheses.
        CString description;   // Optional help, shown in the functions window.
    };

    enum class BreakpointAction
    {
        Added,
        Removed,
        Moved
    };

    // Lines are 1-based. Zero means "not present" (Added.oldLine / Removed.newLine).
    struct BreakpointChange
    {
        BreakpointAction action;
        int oldLine;
        int newLine;
    };

    using BreakpointCallback = void (*)(CJsRichEditCtrl& editor,
        const BreakpointChange& change, void* userData);

    CJsRichEditCtrl();
    virtual ~CJsRichEditCtrl();

    BOOL Create(CWnd* parent, const RECT& rect, UINT id,
        DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP);

    CRichEditCtrl& GetEdit();
    const CRichEditCtrl& GetEdit() const;
    CStatusBarCtrl& GetStatusBar();

    CString GetText() const;
    // New document: clears undo history and breakpoints, notifies breakpoint removals.
    void SetText(LPCTSTR text);
    void SetReadOnly(bool readOnly);
    void GoTo(int line, int column = 1); // Both 1-based; clamps to document bounds.
    void GetCaretPosition(int& line, int& column) const;
    int GetSelectedLineCount() const;

    void SetBreakpointCallback(BreakpointCallback callback, void* userData = nullptr);
    bool SetBreakpoint(int line, bool enabled, bool notify = true);
    bool ToggleBreakpoint(int line);
    bool HasBreakpoint(int line) const;
    void ClearBreakpoints(bool notify = true);
    const std::vector<int>& GetBreakpoints() const;

    void RegisterIntrinsic(LPCTSTR name, LPCTSTR parameters, LPCTSTR description = nullptr);
    void ClearIntrinsics();
    const std::vector<IntrinsicFunction>& GetIntrinsics() const;
    void ShowIntrinsicFunctions();

    void SetConfigurationContext(LPCTSTR context);
    const CString& GetConfigurationContext() const;
    const ColorConfig& GetColors() const;
    void SetColors(const ColorConfig& colors);
    // Explicit calls: virtual hooks are never called by constructors/destructors.
    bool LoadColors();
    bool SaveColors();
    void ShowColorConfiguration();

    void SetEditorFont(LPCTSTR face, int pointSize = 11);
    void SetMinimumLineNumberDigits(unsigned digits); // Default 4; grows automatically.
    void SetColorizeDelay(UINT milliseconds);         // Default 150 ms after editing.
    void RequestColorization();
    const CString& GetLastError() const;

protected:
    // Return true on success. Base hooks deliberately do nothing and return false.
    virtual bool LoadColorConfiguration(LPCTSTR context, ColorConfig& colors);
    virtual bool SaveColorConfiguration(LPCTSTR context, const ColorConfig& colors);
    virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;

private:
    struct Impl;
    Impl* m_impl;
    CJsRichEditCtrl(const CJsRichEditCtrl&) = delete;
    CJsRichEditCtrl& operator=(const CJsRichEditCtrl&) = delete;
};
