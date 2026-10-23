#include "stdafx.h"

#include "JsRichEditCtrl.h"
#include <Pool.h>
#include <afxdlgs.h>
#include <richedit.h>
#include <richole.h>
#include <tom.h>
#include <windowsx.h>
#include <algorithm>
#include <atomic>
#include <cwctype>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <limits>

#pragma comment(lib, "ole32.lib")

#ifdef SubclassWindow
#undef SubclassWindow
#endif

namespace JsEditorDetail
{
    using Item = CJsRichEditCtrl::ColorItem;
    struct Span
    {
        long begin;
        long end;
        Item item;
    };


    bool Identifier(TCHAR ch)
    {
        // Matches TinyJS's alphabetic/underscore identifier convention.
        return ::_istalpha(ch) != 0 || ch == _T('_');
    }

    bool Keyword(tstring_view word)
    {
        static constexpr tstring_view words[] =
        {
            _T("if"), _T("else"), _T("do"), _T("while"), _T("for"), _T("break"), _T("continue"),
            _T("function"), _T("return"), _T("var"), _T("true"), _T("false"), _T("null"),
            _T("undefined"), _T("new")
        };

		for (auto candidate : words)
        {
            if (word == candidate)
                return true;
        }

		return false;
    }

    // Portable core: cancellation checked inside long tokens as well as between them.
    bool Lex(const tstring &text, const std::unordered_set<tstring> &intrinsics,
        std::vector<Span> &spans, const std::atomic<unsigned long long> &revision,
        unsigned long long expected)
    {
        spans.clear();
        const size_t size = text.size();
        size_t p = 0;

		auto alive = [&]()
        {
            return revision.load(std::memory_order_relaxed) == expected;
        };

		auto advance = [&]()
        {
            p++;
            return (p & 1023) != 0 || alive();
        };

		while (p < size)
        {
            if (!alive())
                return false;

			if (_istspace(text[p]))
            {
                p++;
                continue;
            }

			const size_t begin = p;
            Item item = Item::Operator;

			if (text[p] == _T('/') && p + 1 < size && text[p + 1] == _T('/'))
            {
                item = Item::Comment;
                while (p < size && text[p] != _T('\r') && text[p] != _T('\n'))
                {
                    if (!advance())
                        return false;
                }
            }
            else if (text[p] == _T('/') && p + 1 < size && text[p + 1] == _T('*'))
            {
                item = Item::Comment;
                p += 2;
                while (p < size)
                {
                    if (text[p] == _T('*') && p + 1 < size && text[p + 1] == _T('/'))
                    {
                        p += 2;
                        break;
                    }

					if (!advance())
                        return false;
                }
            }
            else if (text[p] == _T('\'') || text[p] == _T('\"'))
            {
                item = Item::String;
                TCHAR quote = text[p++];
                while (p < size)
                {
                    TCHAR ch = text[p];

					if (!advance())
                        return false;

					if (ch == quote)
                        break;

					if (ch == _T('\\') && p < size && !advance())
                        return false;
                }
            }
            else if (_istdigit(text[p]))
            {
                item = Item::Number;
                if (text[p] == _T('0') && p + 1 < size && text[p + 1] == _T('x'))
                {
                    p += 2;
                    while (p < size && _istxdigit(text[p]))
                    {
                        if (!advance())
                            return false;
                    }
                }
                else
                {
                    while (p < size && _istdigit(text[p]))
                    {
                        if (!advance())
                            return false;
                    }
                    if (p < size && text[p] == _T('.'))
                    {
                        p++;
                        while (p < size && _istdigit(text[p]))
                        {
                            if (!advance())
                                return false;
                        }
                    }
                    if (p < size && (text[p] == _T('e') || text[p] == _T('E')))
                    {
                        p++;
                        if (p < size && text[p] == _T('-'))
                            p++;

						while (p < size && _istdigit(text[p]))
                        {
                            if (!advance())
                                return false;
                        }
                    }
                }
            }
            else if (Identifier(text[p]))
            {
                item = Item::Text;
                while (p < size && (Identifier(text[p]) || _istdigit(text[p])))
                {
                    if (!advance())
                        return false;
                }

				if (Keyword(tstring_view(text.data() + begin, p - begin)))
                    item = Item::Keyword;
            }
			else
			{
				p++;
			}

			spans.push_back({ static_cast<long>(begin), static_cast<long>(p), item });
        }

        // Second pass ignores comments between a name and '('. This also avoids
        // treating member calls obj.Log() as the global intrinsic Log().
        tstring qualified;
        for (size_t i = 0; i < spans.size(); i++)
        {
            if (!alive())
                return false;

			Span &span = spans[i];
            if (span.item != Item::Text)
                continue;

			size_t next = i + 1;
            while (next < spans.size() && spans[next].item == Item::Comment)
                next++;

			if (next == spans.size() || text[spans[next].begin] != _T('('))
                continue;

			span.item = Item::Function;
            qualified.assign(text, span.begin, span.end - span.begin);
            size_t first = i;
            bool completeName = true;

			while (first > 0)
            {
                size_t dot = first;
                do
                {
                    dot--;
                }
				while (dot > 0 && spans[dot].item == Item::Comment);

				if (text[spans[dot].begin] != _T('.'))
                    break;

				if (dot == 0)
                {
                    completeName = false;
                    break;
                }

				size_t name = dot;
                do
                {
                    name--;
                }
				while (name > 0 && spans[name].item == Item::Comment);

				if (spans[name].item != Item::Text)
                {
                    completeName = false;
                    break;
                }

				qualified.insert(0, _T("."));
                qualified.insert(0, text, spans[name].begin, spans[name].end - spans[name].begin);
                first = name;
            }

			// Declarations look like functions but are not calls to a native function.
            size_t prev = i;
            while (prev > 0 && spans[prev - 1].item == Item::Comment)
                prev--;

			const bool declaration = prev > 0 &&
                tstring_view(text.data() + spans[prev - 1].begin,
                    spans[prev - 1].end - spans[prev - 1].begin) == _T("function");

			if (completeName && !declaration && intrinsics.find(qualified) != intrinsics.end())
                span.item = Item::Intrinsic;
        }

		return alive();
    }

    void BuildLines(const tstring &text, std::vector<long> &lines)
    {
        lines.clear();
        lines.push_back(0);

		for (size_t i = 0; i < text.size(); i++)
        {
            if (text[i] == _T('\r') || text[i] == _T('\n'))
            {
                if (text[i] == _T('\r') && i + 1 < text.size() && text[i + 1] == _T('\n'))
                    i++;

				lines.push_back((long)(i + 1));
            }
        }
    }

    int LineAt(const std::vector<long>& lines, long position)
    {
        return static_cast<int>(std::upper_bound(lines.begin(), lines.end(), position) - lines.begin());
    }

    struct EditRange
    {
        long begin;
        long oldEnd;
        long newEnd;
    };

    EditRange Difference(const tstring &oldText, const tstring &newText, long prefixLimit = (std::numeric_limits<long>::max)())
    {
        size_t begin = 0;
        const size_t limit = (std::min)(static_cast<size_t>((std::max)(0L, prefixLimit)),
            (std::min)(oldText.size(), newText.size()));

		while (begin < limit && oldText[begin] == newText[begin])
            begin++;

		size_t oldEnd = oldText.size();
        size_t newEnd = newText.size();

		while (oldEnd > begin && newEnd > begin && oldText[oldEnd - 1] == newText[newEnd - 1])
        {
            oldEnd--;
            newEnd--;
        }

		return { static_cast<long>(begin), static_cast<long>(oldEnd), static_cast<long>(newEnd) };
    }

    int MoveBreakpoint(int line, const std::vector<long> &oldLines,
        const std::vector<long> &newLines, long oldLength, const EditRange &edit)
    {
        const long start = oldLines[static_cast<size_t>(line - 1)];
        const bool last = static_cast<size_t>(line) == oldLines.size();
        const long end = last ? oldLength : oldLines[static_cast<size_t>(line)];

		// Whole terminated line removed, or final nonempty line removed in full.
        if (edit.oldEnd > edit.begin && edit.begin <= start && edit.oldEnd >= end &&
            (!last || end > start || edit.begin < start))
            return 0;

		long anchor = start;
        if (start >= edit.oldEnd)
            anchor += edit.newEnd - edit.oldEnd;
        else if (start > edit.begin)
            anchor = edit.newEnd;

		return LineAt(newLines, (std::max)(0L, anchor));
    }
}

using namespace JsEditorDetail;

namespace
{
    constexpr uint64_t EditId = 101;
    constexpr uint64_t StatusId = 102;
    constexpr uint64_t GutterId = 103;
    constexpr UINT_PTR PollTimer = 1;
    constexpr UINT CmdUndo = 1001;
    constexpr UINT CmdRedo = 1002;
    constexpr UINT CmdCut = 1003;
    constexpr UINT CmdCopy = 1004;
    constexpr UINT CmdPaste = 1005;
    constexpr UINT CmdDelete = 1006;
    constexpr UINT CmdSelectAll = 1007;
    constexpr UINT CmdBreakpoint = 1008;
    constexpr UINT CmdColors = 1009;
    constexpr UINT CmdFunctions = 1010;

    const TCHAR* const ColorNames[] =
    {
        _T("Background"), _T("Plain text"), _T("Keywords"), _T("Numbers"),
        _T("Strings"), _T("Comments"), _T("Function names"), _T("Intrinsic functions"),
        _T("Operators / punctuation"), _T("Gutter background"), _T("Line numbers"),
        _T("Breakpoints")
    };

    HMODULE RichEditModule()
    {
        // Intentionally process-lifetime: another editor may still use this class.
        static HMODULE module = LoadLibrary(_T("Msftedit.dll"));
        return module;
    }
}

CJsRichEditCtrl::ColorConfig::ColorConfig()
{
    values[(size_t)ColorItem::Background]		= RGB(255, 255, 255);
    values[(size_t)ColorItem::Text]				= RGB(32, 32, 32);
    values[(size_t)ColorItem::Keyword]			= RGB(0, 0, 0);
    values[(size_t)ColorItem::Number]			= RGB(61, 166, 18);
    values[(size_t)ColorItem::String]			= RGB(128, 64, 0);
    values[(size_t)ColorItem::Comment]			= RGB(64, 128, 64);
    values[(size_t)ColorItem::Function]			= RGB(64, 64, 192);
    values[(size_t)ColorItem::Intrinsic]		= RGB(64, 128, 255);
    values[(size_t)ColorItem::Operator]			= RGB(64, 0, 0);
    values[(size_t)ColorItem::GutterBackground] = RGB(192, 192, 192);
    values[(size_t)ColorItem::LineNumber]		= RGB(64, 64, 64);
    values[(size_t)ColorItem::Breakpoint]		= RGB(240, 35, 35);
}

COLORREF &CJsRichEditCtrl::ColorConfig::operator[](ColorItem item)
{
    return values[static_cast<int>(item)];
}

COLORREF CJsRichEditCtrl::ColorConfig::operator[](ColorItem item) const
{
    return values[static_cast<int>(item)];
}

struct CJsRichEditCtrl::Impl
{
    struct Edit : CRichEditCtrl
    {
        Impl* host = nullptr;
        LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;

		BOOL PreTranslateMessage(MSG* message) override
		{
			if (message->message == WM_KEYDOWN)
			{
				bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
				bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
				bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;

				if (control && !alt && message->wParam == 'Z')
				{
					SendMessage(shift ? EM_REDO : EM_UNDO);
					return TRUE;
				}

				if (control && !alt && message->wParam == 'Y')
				{
					SendMessage(EM_REDO);
					return TRUE;
				}

				if (!alt && message->wParam == VK_DELETE)
				{
					// deliver to our WindowProc and then the native Rich Edit before the host consumes the key
					SendMessage(message->message, message->wParam, message->lParam);
					return TRUE;
				}
			}

			return CRichEditCtrl::PreTranslateMessage(message);
		}
    };

    struct Gutter : CWnd
    {
        Impl* host = nullptr;
        LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;
    };

    struct Popup : CWnd
    {
        Impl* host = nullptr;
        bool colors = false;

		struct List : CListBox
        {
            Popup* popup = nullptr;
            LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override
            {
                const LRESULT result = CListBox::WindowProc(message, wParam, lParam);
                if (popup && popup->colors && message == WM_LBUTTONUP)
                {
                    const CPoint point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));

					CRect rect;
                    GetClientRect(&rect);

					BOOL outside = FALSE;
                    const UINT item = ItemFromPoint(point, outside);

					if (!outside && item < static_cast<UINT>(GetCount()) && point.x >= rect.right - popup->host->Scale(56))
                        popup->SendMessage(WM_COMMAND, MAKEWPARAM(201, LBN_DBLCLK), reinterpret_cast<LPARAM>(GetSafeHwnd()));
                }
                return result;
            }
        };

		List list;
        CEdit search;
        CStatic help;
        CButton primary;
        CButton secondary;
        CButton defaults;
        CButton close;
        std::vector<size_t> indices;
        void Open(bool colorMode);
        void Layout();
        void Populate();
        void UpdateHelp();
        void InsertFunction();
        BOOL PreTranslateMessage(MSG* message) override;
        LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;
    };

    CJsRichEditCtrl &owner;
    Edit edit;
    Gutter gutter;
    CStatusBarCtrl status;
    Popup colorPopup;
    Popup functionPopup;
    CFont editorFont;
    CFont uiFont;
    CFont glyphFont;
    CString fontFace = _T("Consolas");
    int pointSize = 11;
    unsigned minDigits = 4;
    int gutterWidth = 70;
    int layoutWidth = -1;
    int layoutHeight = -1;
    int layoutGutter = -1;
    int glyphWidth = 24;
    int lineHeight = 18;
    int dpi = 96;
    ColorConfig colors;
    CString context;
    CString lastError;
    std::vector<IntrinsicFunction> intrinsics;
    std::vector<int> breakpoints;
    std::vector<int> movedBreakpoints;
    std::vector<BreakpointChange> breakpointNotifications;
    BreakpointCallback callback = nullptr;
    void* callbackData = nullptr;
    tstring text;
    tstring readBuffer;
    std::vector<long> lines = { 0 };
    std::vector<long> nextLines;
    bool formatting = false;
    bool resetting = false;
    bool destroyed = false;
    bool wasVisible = false;
    bool dirty = true;

	struct TextEdit
    {
        EditRange range;
        tstring removed;
        tstring inserted;
        size_t beforeLength;
        size_t afterLength;
    };

	std::vector<TextEdit> editHistory;
    int historyDirection = 0; // -1 undo, +1 redo, 0 ordinary editing.
    long prefixLimit = (std::numeric_limits<long>::max)();
    UINT delay = 150;
    ULONGLONG changedAt = 0;
    pool::IThreadPool* pool = nullptr;
    ITextDocument* document = nullptr;
    std::atomic<unsigned long long> revision{ 1 };
    std::atomic<bool> busy{ false };
    std::mutex resultMutex;
    bool resultReady = false;
    bool resultFailed = false;
    unsigned long long resultRevision = 0;
    tstring jobText;
    std::unordered_set<tstring> jobIntrinsics;
    std::vector<Span> workerSpans;
    std::vector<Span> resultSpans;
    std::vector<Span> uiSpans;

    explicit Impl(CJsRichEditCtrl &control) : owner(control)
    {
        edit.host = this;
        gutter.host = this;
        colorPopup.host = this;
        functionPopup.host = this;
    }

    ~Impl()
    {
        Stop();
    }

    int Scale(int value) const
    {
        return MulDiv(value, dpi, 96);
    }

    void MarkDirty()
    {
        revision.fetch_add(1, std::memory_order_relaxed);
        dirty = true;
        changedAt = GetTickCount64();
    }

    void Notify(const BreakpointChange &change)
    {
        // Host callbacks may inspect the control, but must not destroy it or edit
        // its text/breakpoints reentrantly. Post such operations to the UI thread.
        if (callback)
            callback(owner, change, callbackData);
    }

    bool Initialize();
    void Stop();
    void MakeFonts();
    void Layout();
    void ReadText(tstring &target) const;
    void Changed();
    void UpdateStatus();
    long Caret() const;
    int SelectedLines() const;
    void Tick();
    void ApplyColors();
    void PaintGutter();
    int HitLine(CPoint point) const;
    void ContextMenu(CPoint screenPoint, CWnd* source);
};

bool CJsRichEditCtrl::Impl::Initialize()
{
    if (!RichEditModule())
    {
        lastError = _T("Unable to load Msftedit.dll.");
        return false;
    }

	INITCOMMONCONTROLSEX common = { sizeof(common), ICC_BAR_CLASSES };
    InitCommonControlsEx(&common);

	const CString windowClass = AfxRegisterWndClass(CS_DBLCLKS, LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr);

	if (!gutter.CreateEx(0, windowClass, _T("Line numbers and breakpoints"),
        WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), &owner, GutterId))
        return false;

	HWND rich = CreateWindowEx(0, _T("RICHEDIT50W"), _T(""),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL |
        ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL | ES_WANTRETURN,
        0, 0, 0, 0, owner.GetSafeHwnd(), (HMENU)EditId,
        AfxGetInstanceHandle(), nullptr);

	if (!rich)
        return false;

	if (!edit.SubclassWindow(rich))
    {
        ::DestroyWindow(rich);
        return false;
    }

	if (!status.Create(WS_CHILD | WS_VISIBLE | CCS_NOPARENTALIGN | CCS_NORESIZE, CRect(0, 0, 0, 0), &owner, StatusId))
        return false;

	int part = -1;
    status.SetParts(1, &part);
    edit.SendMessage(EM_EXLIMITTEXT, 0, 0x7FFFFFFE);
    edit.SendMessage(EM_SETTARGETDEVICE, 0, 1); // Disable wrapping.
    edit.SendMessage(EM_AUTOURLDETECT, FALSE, 0);
    edit.SetEventMask(edit.GetEventMask() | ENM_CHANGE | ENM_SELCHANGE | ENM_SCROLL);
    IRichEditOle* richOle = nullptr;
    edit.SendMessage(EM_GETOLEINTERFACE, 0, reinterpret_cast<LPARAM>(&richOle));
    if (richOle)
    {
        richOle->QueryInterface(__uuidof(ITextDocument), reinterpret_cast<void**>(&document));
        richOle->Release();
    }
    if (!document)
    {
        lastError = _T("Rich Edit's Text Object Model is unavailable.");
        return false;
    }

	MakeFonts();
    edit.SetBackgroundColor(FALSE, colors[ColorItem::Background]);

	Layout();
    ReadText(text);
    BuildLines(text, lines);
    UpdateStatus();

	if (!owner.SetTimer(PollTimer, 40, nullptr))
    {
        lastError = _T("Unable to create the editor timer.");
        return false;
    }

	return true;
}

void CJsRichEditCtrl::Impl::Stop()
{
    if (destroyed)
        return;

	destroyed = true;
    revision.fetch_add(1, std::memory_order_relaxed);

	if (owner.GetSafeHwnd())
        owner.KillTimer(PollTimer);

	if (colorPopup.GetSafeHwnd())
        colorPopup.DestroyWindow();

	if (functionPopup.GetSafeHwnd())
        functionPopup.DestroyWindow();

	if (pool)
    {
        // Workers never SendMessage or wait for the UI. Release joins workers.
        // Do not rely on this pool revision's queue-only WaitForAllTasks check.
        pool->Release();
        pool = nullptr;
    }

	if (document)
    {
        document->Release();
        document = nullptr;
    }
}

void CJsRichEditCtrl::Impl::MakeFonts()
{
    const BOOL modified = edit.GetModify();
    const DWORD mask = edit.GetEventMask();
    POINT scroll = {};
    edit.SendMessage(EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll));

	CHARRANGE selection;
    edit.GetSel(selection);
    long unused = 0;
    const bool suspended = document && SUCCEEDED(document->Undo(tomSuspend, &unused));
    formatting = true;
    edit.SetEventMask(0);
    CClientDC dc(&owner);
    dpi = dc.GetDeviceCaps(LOGPIXELSY);

	// GetDpiForWindow if available, without requiring a particular SDK target.
    using GetDpi = UINT (WINAPI*)(HWND);
    const auto getDpi = reinterpret_cast<GetDpi>(GetProcAddress(GetModuleHandle(_T("user32.dll")), "GetDpiForWindow"));
	if (getDpi)
        dpi = static_cast<int>(getDpi(owner.GetSafeHwnd()));

	LOGFONT font = {};
    font.lfHeight = -MulDiv(pointSize, dpi, 72);
    font.lfCharSet = DEFAULT_CHARSET;
    font.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    _tcsncpy_s(font.lfFaceName, fontFace, _TRUNCATE);

	// Detach the font from HWNDs before deleting/replacing it.
    edit.SendMessage(WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(SYSTEM_FONT)), FALSE);
    editorFont.DeleteObject();
    editorFont.CreateFontIndirect(&font);
    edit.SetFont(&editorFont, FALSE);
    CFont* old = dc.SelectObject(&editorFont);
    TEXTMETRIC metrics = {};
    dc.GetTextMetrics(&metrics);
    lineHeight = metrics.tmHeight;
    dc.SelectObject(old);

	// UI and symbol fonts also owned by the control for its complete lifetime.
    if (!uiFont.GetSafeHandle())
    {
        font.lfHeight = -MulDiv(9, dpi, 72);
        font.lfPitchAndFamily = DEFAULT_PITCH;
        _tcscpy_s(font.lfFaceName, _T("Segoe UI"));
        uiFont.CreateFontIndirect(&font);
    }

	status.SetFont(&uiFont, FALSE);
    glyphFont.DeleteObject();
    font.lfHeight = -MulDiv(pointSize + 2, dpi, 72);
    _tcscpy_s(font.lfFaceName, _T("Segoe UI Symbol"));
	glyphFont.CreateFontIndirect(&font);

	PARAFORMAT para = {};
    para.cbSize = sizeof(para);
    para.dwMask = PFM_TABSTOPS;
    para.cTabCount = 1;

	// A single tab stop sets the repeating interval, in twips.
    para.rgxTabs[0] = MulDiv(metrics.tmAveCharWidth * 4, 1440, dpi);
    edit.SetSel(0, -1);
    edit.SetParaFormat(para);
    edit.SetSel(selection);

	if (suspended)
        document->Undo(tomResume, &unused);

	edit.SetModify(modified);
    edit.SendMessage(EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll));
    formatting = false;

	edit.SetEventMask(mask);
}

void CJsRichEditCtrl::Impl::Layout()
{
    if (!edit.GetSafeHwnd() || !status.GetSafeHwnd())
        return;

	unsigned digits = minDigits;
    size_t count = lines.size();
    unsigned needed = 1;

	while (count >= 10)
    {
        needed++;
        count /= 10;
    }

	digits = (std::max)(digits, needed);
    CClientDC dc(&owner);
    CFont* old = dc.SelectObject(&editorFont);
    const int digitWidth = dc.GetTextExtent(_T("0")).cx;
    dc.SelectObject(old);
    glyphWidth = (std::max)(Scale(24), lineHeight + Scale(4));
    gutterWidth = glyphWidth + digitWidth * static_cast<int>(digits) + Scale(12);
    CRect rect;
    owner.GetClientRect(&rect);
    const int statusHeight = (std::min)(rect.Height(), Scale(24));
    const int width = (std::min)(gutterWidth, rect.Width());
    const int height = (std::max)(0, rect.Height() - statusHeight);

	if (layoutWidth != rect.Width() || layoutHeight != rect.Height() || layoutGutter != width)
    {
        layoutWidth = rect.Width();
        layoutHeight = rect.Height();
        layoutGutter = width;
        gutter.MoveWindow(0, 0, width, height);
        edit.MoveWindow(width, 0, (std::max)(0, rect.Width() - width), height);
        status.MoveWindow(0, height, rect.Width(), statusHeight);
    }

	gutter.Invalidate(FALSE);
}

void CJsRichEditCtrl::Impl::ReadText(tstring &target) const
{
#ifdef _UNICODE
	GETTEXTLENGTHEX length = { GTL_NUMCHARS | GTL_PRECISE, 1200 };
#else
	GETTEXTLENGTHEX length = { GTL_NUMBYTES | GTL_PRECISE, CP_ACP };
#endif

	const LRESULT count = edit.SendMessage(EM_GETTEXTLENGTHEX,
        reinterpret_cast<WPARAM>(&length), 0);

	target.resize(static_cast<size_t>((std::max)(LRESULT(0), count)) + 1);

	GETTEXTEX request = {};
    request.cb = static_cast<DWORD>(target.size() * sizeof(TCHAR));
    request.flags = GT_DEFAULT; // CR, not expanded CRLF: preserves Rich Edit offsets.

#ifdef _UNICODE
	request.codepage = 1200;
#else
	request.codepage = CP_ACP;
#endif

	const LRESULT actual = edit.SendMessage(EM_GETTEXTEX,
        reinterpret_cast<WPARAM>(&request), reinterpret_cast<LPARAM>(target.data()));

	target.resize(static_cast<size_t>((std::max)(LRESULT(0), actual)));
}

void CJsRichEditCtrl::Impl::Changed()
{
    if (formatting || resetting || destroyed)
        return;

	ReadText(readBuffer);

	if (readBuffer == text)
        return;

	EditRange change = Difference(text, readBuffer, prefixLimit);

	if (historyDirection != 0)
    {
        // Most recent matching transition disambiguates identical adjacent lines.
        // Rich Edit may coalesce typing into a larger undo; fallback to the diff.
        for (auto it = editHistory.rbegin(); it != editHistory.rend(); it++)
        {
            const bool undo = historyDirection < 0;
            const auto &from = undo ? it->inserted : it->removed;
            const auto &to = undo ? it->removed : it->inserted;
            const size_t fromLength = undo ? it->afterLength : it->beforeLength;
            const size_t toLength = undo ? it->beforeLength : it->afterLength;
            const size_t begin = static_cast<size_t>(it->range.begin);
            if (text.size() != fromLength || readBuffer.size() != toLength ||
                begin + from.size() > text.size() || begin + to.size() > readBuffer.size())
                continue;

			if (text.compare(begin, from.size(), from) != 0 ||
                readBuffer.compare(begin, to.size(), to) != 0 ||
                text.compare(0, begin, readBuffer, 0, begin) != 0 ||
                text.compare(begin + from.size(), tstring::npos,
                    readBuffer, begin + to.size(), tstring::npos) != 0)
                continue;

			change = { static_cast<long>(begin), static_cast<long>(begin + from.size()),
                static_cast<long>(begin + to.size()) };

			break;
        }
    }
    else
    {
        // Bounded metadata cache, not a competing text undo implementation.
        if (editHistory.size() == 128)
            editHistory.erase(editHistory.begin());

		editHistory.push_back({ change,
            text.substr(change.begin, change.oldEnd - change.begin),
            readBuffer.substr(change.begin, change.newEnd - change.begin),
            text.size(), readBuffer.size() });
    }
    BuildLines(readBuffer, nextLines);
    auto &notifications = breakpointNotifications;
    auto &moved = movedBreakpoints;
    notifications.clear();
    moved.clear();
    moved.reserve(breakpoints.size());

	for (int oldLine : breakpoints)
    {
        const int newLine = MoveBreakpoint(oldLine, lines, nextLines,
            static_cast<long>(text.size()), change);

		if (newLine == 0 || std::find(moved.begin(), moved.end(), newLine) != moved.end())
		{
			notifications.push_back({BreakpointAction::Removed, oldLine, 0});
		}
        else
        {
            moved.push_back(newLine);
            if (newLine != oldLine)
                notifications.push_back({ BreakpointAction::Moved, oldLine, newLine });
        }
    }

	breakpoints.swap(moved);
    text.swap(readBuffer);
    lines.swap(nextLines);
    MarkDirty();
    Layout();
    UpdateStatus();

	for (const auto &notification : notifications)
        Notify(notification);
}

long CJsRichEditCtrl::Impl::Caret() const
{
    CHARRANGE range;
    edit.GetSel(range);
    long flags = 0;
    ITextSelection* selection = nullptr;

	if (document && SUCCEEDED(document->GetSelection(&selection)) && selection)
    {
        selection->GetFlags(&flags);
        selection->Release();
    }

	return (flags & tomSelStartActive) ? range.cpMin : range.cpMax;
}

int CJsRichEditCtrl::Impl::SelectedLines() const
{
    CHARRANGE range;
    edit.GetSel(range);

	if (range.cpMin == range.cpMax)
        return 0;

	return LineAt(lines, range.cpMax - 1) - LineAt(lines, range.cpMin) + 1;
}

void CJsRichEditCtrl::Impl::UpdateStatus()
{
    if (!status.GetSafeHwnd())
        return;

	const long position = Caret();
    const int line = LineAt(lines, position);
    const long column = position - lines[static_cast<size_t>(line - 1)] + 1;
    CString value;
    value.Format(_T("  Line %d, Character %ld"), line, column);

	const int selected = SelectedLines();
	if (selected > 0)
    {
        CString suffix;
        suffix.Format(_T("   |   %d line%s selected"), selected, selected == 1 ? _T("") : _T("s"));
        value += suffix;
    }

	status.SetText(value, 0, 0);
    gutter.Invalidate(FALSE);
}

void CJsRichEditCtrl::Impl::Tick()
{
    if (destroyed)
        return;

	// IsWindowVisible includes the ancestor chain, which catches hidden tab panes.
    const bool visible = owner.IsWindowVisible() && edit.IsWindowVisible();
    if (visible != wasVisible)
    {
        wasVisible = visible;
        MarkDirty();

		if (!visible)
        {
            if (colorPopup.GetSafeHwnd())
                colorPopup.ShowWindow(SW_HIDE);
            if (functionPopup.GetSafeHwnd())
                functionPopup.ShowWindow(SW_HIDE);
        }
    }

	if (!visible)
        return;

	if (busy.load(std::memory_order_acquire))
        return;

	{
        std::lock_guard<std::mutex> lock(resultMutex);
        if (resultReady)
        {
            resultReady = false;
            if (resultRevision == revision.load(std::memory_order_relaxed))
            {
                if (resultFailed)
                {
                    lastError = _T("Background colorization failed (allocation or lexer exception).");
                    dirty = false; // Retry on the next edit or explicit request.
                }
                else
                {
                    uiSpans.swap(resultSpans);
                    ApplyColors();
                    dirty = false;
                }
            }
        }
    }

	if (!dirty || GetTickCount64() - changedAt < delay)
        return;

	if (!pool)
    {
        try
        {
            pool = ::pool::IThreadPool::Create(size_t(1));
        }
        catch (...)
        {
            pool = nullptr;
        }
        if (!pool)
        {
            lastError = _T("Unable to create the colorization worker pool.");
            dirty = false;
            return;
        }
    }

	const auto expected = revision.load(std::memory_order_relaxed);
    // One outstanding job; these buffers are never mutated while busy is true.
    jobText.assign(text);

	jobIntrinsics.clear();
	for (const auto &intrinsic : intrinsics)
	{
		jobIntrinsics.insert((LPCTSTR)intrinsic.name);
	}

	busy.store(true, std::memory_order_release);
    const HWND window = owner.GetSafeHwnd();
    bool queued = false;

	try
    {
        queued = pool->RunTask([this, expected, window](size_t)
        {
            bool complete = false;
            bool failed = false;
            try
            {
                // Read-only Win32 visibility check; lifetime is protected by Release/join.
                if (::IsWindowVisible(window))
                    complete = Lex(jobText, jobIntrinsics, workerSpans, revision, expected);
            }
            catch (...)
            {
                failed = true;
            }
            if (complete || failed)
            {
                std::lock_guard<std::mutex> lock(resultMutex);
                resultSpans.swap(workerSpans);
                resultRevision = expected;
                resultReady = true;
                resultFailed = failed;
            }
            busy.store(false, std::memory_order_release);
            return ::pool::IThreadPool::OK;
        });
    }
    catch (...)
    {
        queued = false;
    }

	if (!queued)
    {
        busy.store(false, std::memory_order_release);
        dirty = false;
        lastError = _T("Unable to queue the colorization task.");
    }
}

void CJsRichEditCtrl::Impl::ApplyColors()
{
    long unused = 0;
    if (!document || FAILED(document->Undo(tomSuspend, &unused)))
    {
        lastError = _T("Unable to suspend undo recording for syntax formatting.");
        return;
    }

	const BOOL modified = edit.GetModify();
    const DWORD events = edit.GetEventMask();
    POINT scroll = {};
    edit.SendMessage(EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll));
    formatting = true;
    edit.SetEventMask(0);
    const bool frozen = SUCCEEDED(document->Freeze(&unused));
    bool success = true;
    ITextRange* range = nullptr;
    HRESULT hr = document->Range(0, static_cast<long>(text.size()), &range);

	if (SUCCEEDED(hr) && range)
    {
        auto setColor = [&](long begin, long end, COLORREF color)
        {
            ITextFont* font = nullptr;
            if (FAILED(range->SetRange(begin, end)) || FAILED(range->GetFont(&font)) || !font)
            {
                success = false;
                return;
            }

			if (FAILED(font->SetForeColor(static_cast<long>(color))))
                success = false;
            font->Release();
        };

		setColor(0, static_cast<long>(text.size()), colors[ColorItem::Text]);
        for (const auto &span : uiSpans)
        {
            if (span.item != ColorItem::Text)
                setColor(span.begin, span.end, colors[span.item]);
        }

		range->Release();
    }
	else
	{
		success = false;
	}

	edit.SetBackgroundColor(FALSE, colors[ColorItem::Background]);

	if (frozen)
        document->Unfreeze(&unused);

	document->Undo(tomResume, &unused);
    edit.SetModify(modified);
    edit.SendMessage(EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll));
    edit.SetEventMask(events);
    formatting = false;
    lastError = success ? _T("") : _T("Unable to apply one or more syntax colors.");
    gutter.Invalidate(FALSE);
}

void CJsRichEditCtrl::Impl::PaintGutter()
{
    CPaintDC dc(&gutter);
    CRect rect;
    gutter.GetClientRect(&rect);

	// Small double-buffered gutter; avoids flashing during scroll and selection.
    CDC memory;
    memory.CreateCompatibleDC(&dc);
    CBitmap bitmap;
    bitmap.CreateCompatibleBitmap(&dc, (std::max)(1, rect.Width()), (std::max)(1, rect.Height()));

	CBitmap* oldBitmap = memory.SelectObject(&bitmap);
    memory.FillSolidRect(rect, colors[ColorItem::GutterBackground]);
    memory.SetBkMode(TRANSPARENT);

	CFont* oldFont = memory.SelectObject(&editorFont);
    memory.SetTextColor(colors[ColorItem::LineNumber]);

	CRect format;
    edit.GetRect(format);

	long first = static_cast<long>(edit.GetFirstVisibleLine());
    first = (std::max)(0L, first);
    for (size_t index = static_cast<size_t>(first); index < lines.size(); index++)
    {
        POINTL position = {};
        edit.SendMessage(EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&position), lines[index]);

		if (position.y >= rect.bottom)
            break;

		if (position.y + lineHeight <= format.top)
            continue;

		CString number;
        number.Format(_T("%d"), static_cast<int>(index + 1));
        CRect lineRect(glyphWidth, position.y, rect.right - Scale(6), position.y + lineHeight);
        memory.DrawText(number, lineRect, DT_RIGHT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

		if (owner.HasBreakpoint(static_cast<int>(index + 1)))
        {
            memory.SelectObject(&glyphFont);
            memory.SetTextColor(colors[ColorItem::Breakpoint]);
            RECT marker = { 0, position.y, glyphWidth, position.y + lineHeight };
            // U+2BC3 BLACK OCTAGON: monochrome stop-sign glyph, no bitmap/emoji.
            ::DrawTextW(memory.GetSafeHdc(), L"\x2BC3", 1, &marker,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            memory.SelectObject(&editorFont);
            memory.SetTextColor(colors[ColorItem::LineNumber]);
        }
    }

	memory.SelectObject(oldFont);
    dc.BitBlt(0, 0, rect.Width(), rect.Height(), &memory, 0, 0, SRCCOPY);
    memory.SelectObject(oldBitmap);
}

int CJsRichEditCtrl::Impl::HitLine(CPoint point) const
{
    CRect format;
    edit.GetRect(format);

	if (point.y < format.top)
        return 0;

	POINTL hit = { format.left, point.y };
    const long position = static_cast<long>(edit.SendMessage(EM_CHARFROMPOS, 0,
        reinterpret_cast<LPARAM>(&hit)));

	const int line = LineAt(lines, (std::max)(0L, position));
    POINTL top = {};
    edit.SendMessage(EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&top), lines[line - 1]);

	return point.y >= top.y && point.y < top.y + lineHeight ? line : 0;
}

LRESULT CJsRichEditCtrl::Impl::Gutter::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_PAINT)
    {
        host->PaintGutter();
        return 0;
    }

	if (message == WM_ERASEBKGND)
        return 1;

	if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK)
    {
        const int line = host->HitLine(CPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
        if (line > 0)
            host->owner.ToggleBreakpoint(line);

		host->edit.SetFocus();
        return 0;
    }

	if (message == WM_MOUSEWHEEL)
        return host->edit.SendMessage(message, wParam, lParam);

	if (message == WM_CONTEXTMENU)
    {
        host->ContextMenu(CPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)), this);
        return 0;
    }

	return CWnd::WindowProc(message, wParam, lParam);
}

LRESULT CJsRichEditCtrl::Impl::Edit::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_CONTEXTMENU)
    {
        host->ContextMenu(CPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)), this);
        return 0;
    }

	if (message == WM_GETDLGCODE)
        return CRichEditCtrl::WindowProc(message, wParam, lParam) | DLGC_WANTTAB | DLGC_WANTCHARS;

	if (message == WM_KEYDOWN && wParam == VK_F9)
    {
        host->owner.ToggleBreakpoint(LineAt(host->lines, host->Caret()));
        return 0;
    }

	if (message == WM_KEYDOWN && (GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'A')
    {
        SetSel(0, -1);
        return 0;
    }

	if (message == WM_PASTE)
    {
        // Re-enter this subclass so the replacement location is captured too.
        return SendMessage(EM_PASTESPECIAL, CF_UNICODETEXT, 0);
    }

	const int previousDirection = host->historyDirection;

	if (message == WM_UNDO || message == EM_UNDO)
        host->historyDirection = -1;
    else if (message == EM_REDO)
        host->historyDirection = 1;
    else if (message == WM_KEYDOWN && (GetKeyState(VK_CONTROL) & 0x8000) && (wParam == 'Z' || wParam == 'Y'))
        host->historyDirection = (wParam == 'Y' || (GetKeyState(VK_SHIFT) & 0x8000)) ? 1 : -1;

	const long previousLimit = host->prefixLimit;
    if (message == WM_CHAR || message == WM_CUT || message == WM_CLEAR ||
        message == EM_REPLACESEL || message == EM_PASTESPECIAL ||
        (message == WM_KEYDOWN && wParam == VK_DELETE))
    {
        CHARRANGE range;
        GetSel(range);
        host->prefixLimit = range.cpMin;
        if (message == WM_CHAR && wParam == VK_BACK && range.cpMin == range.cpMax)
            host->prefixLimit = (std::max)(0L, range.cpMin - 1);
    }

	const LRESULT result = CRichEditCtrl::WindowProc(message, wParam, lParam);
    host->prefixLimit = previousLimit;
    host->historyDirection = previousDirection;

	if (message == WM_VSCROLL || message == WM_HSCROLL || message == WM_MOUSEWHEEL ||
        message == WM_KEYDOWN || message == WM_LBUTTONUP || message == EM_SCROLLCARET ||
        message == EM_SETSCROLLPOS || message == EM_LINESCROLL)
    {
        if (!host->formatting && !host->destroyed)
            host->UpdateStatus();
    }

	return result;
}

void CJsRichEditCtrl::Impl::ContextMenu(CPoint screenPoint, CWnd* source)
{
    int targetLine = LineAt(lines, Caret());

	if (screenPoint == CPoint(-1, -1))
    {
        POINTL position = {};
        edit.SendMessage(EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&position), Caret());
        screenPoint = CPoint(position.x, position.y + lineHeight);
        edit.ClientToScreen(&screenPoint);
    }
    else
    {
        CPoint local = screenPoint;
        edit.ScreenToClient(&local);
        const int hit = HitLine(local);
        if (hit > 0)
            targetLine = hit;
    }

	UNREFERENCED_PARAMETER(source);
    CHARRANGE selection;
    edit.GetSel(selection);
    const bool selected = selection.cpMin != selection.cpMax;
    const bool writable = (edit.GetStyle() & ES_READONLY) == 0;
    CMenu menu;
    menu.CreatePopupMenu();

	auto add = [&](UINT id, LPCTSTR label, bool enabled)
    {
        menu.AppendMenu(MF_STRING | (enabled ? MF_ENABLED : MF_GRAYED), id, label);
    };

	add(CmdUndo, _T("&Undo\tCtrl+Z"), writable && edit.CanUndo());
    add(CmdRedo, _T("&Redo\tCtrl+Y"), writable && edit.SendMessage(EM_CANREDO));
    menu.AppendMenu(MF_SEPARATOR);
    add(CmdCut, _T("Cu&t\tCtrl+X"), writable && selected);
    add(CmdCopy, _T("&Copy\tCtrl+C"), selected);
    add(CmdPaste, _T("&Paste\tCtrl+V"), writable && IsClipboardFormatAvailable(CF_UNICODETEXT));
    add(CmdDelete, _T("&Delete\tDel"), writable && selected);
    add(CmdSelectAll, _T("Select &all\tCtrl+A"), !text.empty());
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING | (owner.HasBreakpoint(targetLine) ? MF_CHECKED : 0),
        CmdBreakpoint, _T("Toggle &breakpoint\tF9"));
    menu.AppendMenu(MF_SEPARATOR);
    add(CmdColors, _T("&Colors..."), true);
    add(CmdFunctions, _T("Intrinsic &functions..."), true);
    const UINT command = static_cast<UINT>(menu.TrackPopupMenu(
        TPM_RETURNCMD | TPM_RIGHTBUTTON, screenPoint.x, screenPoint.y, &owner));

	switch (command)
    {
		case CmdUndo:
			edit.Undo();
			break;

		case CmdRedo:
			edit.SendMessage(EM_REDO);
			break;

		case CmdCut:
			edit.Cut();
			break;

		case CmdCopy:
			edit.Copy();
			break;

		case CmdPaste:
			edit.SendMessage(WM_PASTE);
			break;

		case CmdDelete:
			edit.Clear();
			break;

		case CmdSelectAll:
			edit.SetSel(0, -1);
			break;

		case CmdBreakpoint:
			owner.ToggleBreakpoint(targetLine);
			break;

		case CmdColors:
			owner.ShowColorConfiguration();
			break;

		case CmdFunctions:
			owner.ShowIntrinsicFunctions();
			break;
    }
}

void CJsRichEditCtrl::Impl::Popup::Open(bool colorMode)
{
    colors = colorMode;
    if (!GetSafeHwnd())
    {
        const CString cls = AfxRegisterWndClass(CS_DBLCLKS, LoadCursor(nullptr, IDC_ARROW),
            reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1), nullptr);

		CRect bounds;
        host->owner.GetWindowRect(&bounds);
        bounds.left += host->Scale(30);
        bounds.top += host->Scale(30);
        bounds.right = bounds.left + host->Scale(colors ? 420 : 680);
        bounds.bottom = bounds.top + host->Scale(400);
        MONITORINFO monitor = { sizeof(monitor) };

		if (GetMonitorInfo(MonitorFromRect(&bounds, MONITOR_DEFAULTTONEAREST), &monitor))
        {
            if (bounds.right > monitor.rcWork.right)
                bounds.OffsetRect(monitor.rcWork.right - bounds.right, 0);

			if (bounds.bottom > monitor.rcWork.bottom)
                bounds.OffsetRect(0, monitor.rcWork.bottom - bounds.bottom);

			if (bounds.left < monitor.rcWork.left)
                bounds.OffsetRect(monitor.rcWork.left - bounds.left, 0);

			if (bounds.top < monitor.rcWork.top)
                bounds.OffsetRect(0, monitor.rcWork.top - bounds.top);
        }

		if (!CreateEx(WS_EX_TOOLWINDOW | WS_EX_CONTROLPARENT, cls,
					  colors ? _T("Script colors") : _T("Intrinsic functions"),
					  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
					  bounds, &host->owner, 0))
		{
			return;
		}
    }

	Populate();
    Layout();
    ShowWindow(SW_SHOWNORMAL);
    SetForegroundWindow();

	if (colors)
        list.SetFocus();
    else
        search.SetFocus();
}

void CJsRichEditCtrl::Impl::Popup::Layout()
{
    if (!list.GetSafeHwnd())
        return;

	CRect rect;
    GetClientRect(&rect);

	const int gap = host->Scale(10);
    const int buttonHeight = host->Scale(28);
    const int width = (std::max)(0, rect.Width() - 2 * gap);
    const int listTop = colors ? gap : gap + buttonHeight + gap;
    const int helpHeight = host->Scale(colors ? 48 : 72);
    const int helpTop = (std::max)(listTop, rect.Height() - gap * 3 - buttonHeight - helpHeight);
    list.MoveWindow(gap, listTop, width, (std::max)(0, helpTop - gap - listTop));
    help.MoveWindow(gap, helpTop, width, helpHeight);
    const int y = rect.Height() - gap - buttonHeight;

	if (colors)
    {
        const int buttonWidth = (std::max)(0, (width - gap * 3) / 4);
        primary.MoveWindow(gap, y, buttonWidth, buttonHeight);
        secondary.MoveWindow(gap * 2 + buttonWidth, y, buttonWidth, buttonHeight);
        defaults.MoveWindow(gap * 3 + buttonWidth * 2, y, buttonWidth, buttonHeight);
        close.MoveWindow(gap * 4 + buttonWidth * 3, y, buttonWidth, buttonHeight);
    }
    else
    {
        search.MoveWindow(gap, gap, width, buttonHeight);
        primary.MoveWindow(gap, y, host->Scale(130), buttonHeight);
        close.MoveWindow((std::max)(gap, rect.Width() - gap - host->Scale(90)), y,
            host->Scale(90), buttonHeight);
    }
}

void CJsRichEditCtrl::Impl::Popup::Populate()
{
    const int previous = list.GetCurSel();
    list.ResetContent();
    indices.clear();

	if (colors)
    {
        for (const auto* name : ColorNames)
            list.AddString(name);
    }
    else
    {
        CString filter;
        search.GetWindowText(filter);
        filter.MakeLower();

		for (size_t i = 0; i < host->intrinsics.size(); i++)
        {
            const auto &entry = host->intrinsics[i];
            CString signature = entry.name + _T("(") + entry.parameters + _T(")");
            CString searchable = signature + _T(" ") + entry.description;
            searchable.MakeLower();

			if (!filter.IsEmpty() && searchable.Find(filter) < 0)
                continue;

			list.AddString(signature);
            indices.push_back(i);
        }

		// Allow long signatures without clipping their final parameters.
        CClientDC dc(&list);
        CFont* old = dc.SelectObject(&host->uiFont);
        int extent = 0;

		for (int i = 0; i < list.GetCount(); i++)
        {
            CString signature;
            list.GetText(i, signature);
            extent = std::max<int>(extent, dc.GetTextExtent(signature).cx + host->Scale(16));
        }

		dc.SelectObject(old);
        list.SetHorizontalExtent(extent);
    }

	if (list.GetCount() > 0)
        list.SetCurSel(std::max<int>(0, (std::min)(previous, list.GetCount() - 1)));

	UpdateHelp();
    list.Invalidate(FALSE);
}

void CJsRichEditCtrl::Impl::Popup::UpdateHelp()
{
    CString message;
    const int selected = list.GetCurSel();

	if (colors)
    {
        message = _T("Click a color swatch (or double-click a row) to choose its color. ")
            _T("Changes preview immediately. Save uses the configuration hook.\r\nContext: ");
        message += host->context;
    }
    else if (selected >= 0 && static_cast<size_t>(selected) < indices.size())
    {
        const auto &entry = host->intrinsics[indices[static_cast<size_t>(selected)]];
        message = entry.name + _T("(") + entry.parameters + _T(")\r\n") + entry.description;
    }
	else
	{
		message = _T("No matching functions. Register functions through RegisterIntrinsic().");
	}

	help.SetWindowText(message);

	if (!colors)
        primary.EnableWindow(selected >= 0 && (host->edit.GetStyle() & ES_READONLY) == 0);
}

void CJsRichEditCtrl::Impl::Popup::InsertFunction()
{
    const int selected = list.GetCurSel();
    if (selected < 0 || static_cast<size_t>(selected) >= indices.size() || (host->edit.GetStyle() & ES_READONLY))
        return;

	const auto &entry = host->intrinsics[indices[static_cast<size_t>(selected)]];
	tstring insertion(entry.name.GetString());
	insertion += _T("()");

	CHARRANGE range;
    host->edit.GetSel(range);

	// The child is always a Unicode window, including in TCHAR/MBCS host builds.
    ::SendMessage(host->edit.GetSafeHwnd(), EM_REPLACESEL, TRUE,
        reinterpret_cast<LPARAM>(insertion.c_str()));

	const long caret = range.cpMin + static_cast<long>(insertion.size()) - 1;
    host->edit.SetSel(caret, caret);
    host->edit.SetFocus();
}

BOOL CJsRichEditCtrl::Impl::Popup::PreTranslateMessage(MSG* message)
{
    if (message->message == WM_KEYDOWN && message->wParam == VK_ESCAPE)
    {
        SendMessage(WM_CLOSE);
        return TRUE;
    }

	if (message->message == WM_KEYDOWN &&
        (message->wParam == VK_RETURN || (colors && message->wParam == VK_SPACE)) &&
        message->hwnd == list.GetSafeHwnd())
    {
        SendMessage(WM_COMMAND, MAKEWPARAM(201, LBN_DBLCLK), reinterpret_cast<LPARAM>(list.GetSafeHwnd()));
        return TRUE;
    }

	if (IsDialogMessage(message))
        return TRUE;

	return CWnd::PreTranslateMessage(message);
}

LRESULT CJsRichEditCtrl::Impl::Popup::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_CREATE)
    {
        list.popup = this;
        const DWORD child = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
        DWORD listStyle = child | WS_VSCROLL | WS_BORDER | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT;

		if (colors)
            listStyle |= LBS_OWNERDRAWFIXED | LBS_HASSTRINGS;
        else
            listStyle |= WS_HSCROLL;

		if (!list.Create(listStyle, CRect(0, 0, 0, 0), this, 201))
            return -1;

		list.SetFont(&host->uiFont);

		if (colors)
            list.SetItemHeight(0, host->Scale(28));

		if (!help.Create(_T(""), WS_CHILD | WS_VISIBLE | SS_LEFT, CRect(0, 0, 0, 0), this, 202))
            return -1;

		help.SetFont(&host->uiFont);
        primary.Create(colors ? _T("Save") : _T("Insert call"), child | BS_PUSHBUTTON,
            CRect(0, 0, 0, 0), this, 203);

		close.Create(_T("Close"), child | BS_PUSHBUTTON, CRect(0, 0, 0, 0), this, 206);
        primary.SetFont(&host->uiFont);
        close.SetFont(&host->uiFont);

		if (colors)
        {
            secondary.Create(_T("Reload"), child | BS_PUSHBUTTON, CRect(0, 0, 0, 0), this, 204);
            defaults.Create(_T("Defaults"), child | BS_PUSHBUTTON, CRect(0, 0, 0, 0), this, 205);
            secondary.SetFont(&host->uiFont);
            defaults.SetFont(&host->uiFont);
        }
        else
        {
            search.CreateEx(WS_EX_CLIENTEDGE, _T("EDIT"), _T(""), child | ES_AUTOHSCROLL, CRect(0, 0, 0, 0), this, 207);
            search.SetFont(&host->uiFont);
            ::SendMessageW(search.GetSafeHwnd(), EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Filter functions or parameters"));
        }

		return 0;
    }

	if (message == WM_SIZE)
    {
        Layout();
        return 0;
    }

	if (message == WM_GETMINMAXINFO)
    {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize = { host->Scale(360), host->Scale(280) };
        return 0;
    }

	if (message == WM_CLOSE)
    {
        ShowWindow(SW_HIDE); // Keep controls alive; no self-deleting CWnd ownership.
        host->edit.SetFocus();
        return 0;
    }

	if (message == WM_MEASUREITEM && colors)
    {
        auto* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        measure->itemHeight = host->Scale(28);
        return TRUE;
    }

	if (message == WM_DRAWITEM && colors)
    {
        const auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (draw->itemID >= static_cast<UINT>(ColorItem::Count))
            return TRUE;

		CDC dc;
        dc.Attach(draw->hDC);
        CRect rect(draw->rcItem);
        const bool selected = (draw->itemState & ODS_SELECTED) != 0;

		dc.FillSolidRect(rect, GetSysColor(selected ? COLOR_HIGHLIGHT : COLOR_WINDOW));
        dc.SetTextColor(GetSysColor(selected ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
        dc.SetBkMode(TRANSPARENT);

		CRect label = rect;
        label.left += host->Scale(8);
        label.right -= host->Scale(64);
        dc.DrawText(ColorNames[draw->itemID], label, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

		CRect swatch = rect;
        swatch.left = swatch.right - host->Scale(56);
        swatch.DeflateRect(host->Scale(5), host->Scale(5));
        dc.FillSolidRect(swatch, host->colors[static_cast<ColorItem>(draw->itemID)]);
        dc.Draw3dRect(swatch, GetSysColor(COLOR_WINDOWTEXT), GetSysColor(COLOR_WINDOWTEXT));

		if (draw->itemState & ODS_FOCUS)
            dc.DrawFocusRect(rect);

		dc.Detach();

		return TRUE;
    }

	if (message == WM_COMMAND)
    {
        const UINT id = LOWORD(wParam);
        const UINT notification = HIWORD(wParam);

		if (id == 206)
        {
            SendMessage(WM_CLOSE);
            return 0;
        }

		if (!colors && id == 207 && notification == EN_CHANGE)
        {
            Populate();
            return 0;
        }

		if (id == 201 && notification == LBN_SELCHANGE)
        {
            UpdateHelp();
            return 0;
        }

		const bool choose = colors && id == 201 && notification == LBN_DBLCLK;
        if (choose)
        {
            const int selected = list.GetCurSel();
            if (selected >= 0)
            {
                const auto item = static_cast<ColorItem>(selected);
                CColorDialog picker(host->colors[item], CC_FULLOPEN, this);
                if (picker.DoModal() == IDOK)
                {
                    ColorConfig value = host->colors;
                    value[item] = picker.GetColor();
                    host->owner.SetColors(value);
                }
            }

			return 0;
        }

		if (colors && id == 203)
        {
            if (!host->owner.SaveColors())
                help.SetWindowText(_T("Save hook returned false; implement SaveColorConfiguration()."));
            else
                help.SetWindowText(_T("Colors saved for this context."));

			return 0;
        }

		if (colors && id == 204)
        {
            if (!host->owner.LoadColors())
                help.SetWindowText(_T("Load hook returned false; current colors were retained."));

			return 0;
        }

		if (colors && id == 205)
        {
            host->owner.SetColors(ColorConfig());
            return 0;
        }

		if (!colors && (id == 203 || (id == 201 && notification == LBN_DBLCLK)))
        {
            InsertFunction();
            return 0;
        }
    }

	return CWnd::WindowProc(message, wParam, lParam);
}

CJsRichEditCtrl::CJsRichEditCtrl() : m_impl(new Impl(*this))
{
}

CJsRichEditCtrl::~CJsRichEditCtrl()
{
    // Tear down HWNDs while derived state is still valid.
    if (GetSafeHwnd())
        DestroyWindow();

	delete m_impl;
}

BOOL CJsRichEditCtrl::Create(CWnd* parent, const RECT &rect, UINT id, DWORD style)
{
    if (!parent || GetSafeHwnd() || m_impl->destroyed)
        return FALSE;

	const CString cls = AfxRegisterWndClass(CS_DBLCLKS, LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr);

	return CreateEx(WS_EX_CONTROLPARENT, cls, _T("Script editor"), style, rect, parent, id);
}

LRESULT CJsRichEditCtrl::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
		case WM_CREATE:
			return m_impl->Initialize() ? 0 : -1;

		case WM_SIZE:
			m_impl->Layout();
			return 0;

		case WM_SETFOCUS:
			if (m_impl->edit.GetSafeHwnd())
				m_impl->edit.SetFocus();
			return 0;

		case WM_ERASEBKGND:
			return 1;

		case WM_TIMER:
			if (wParam == PollTimer)
			{
				m_impl->Tick();
				return 0;
			}
			break;

		case WM_SHOWWINDOW:
			if (!wParam)
			{
				m_impl->MarkDirty();
				if (m_impl->colorPopup.GetSafeHwnd())
					m_impl->colorPopup.ShowWindow(SW_HIDE);
				if (m_impl->functionPopup.GetSafeHwnd())
					m_impl->functionPopup.ShowWindow(SW_HIDE);
			}
			break;

		case 0x02E3: // WM_DPICHANGED_AFTERPARENT (also works with older SDK macros).
			m_impl->MakeFonts();
			m_impl->Layout();
			m_impl->MarkDirty();
			return 0;

		case WM_COMMAND:
			if (LOWORD(wParam) == EditId)
			{
				if (HIWORD(wParam) == EN_CHANGE)
					m_impl->Changed();
				else if (HIWORD(wParam) == EN_VSCROLL || HIWORD(wParam) == EN_HSCROLL)
					m_impl->gutter.Invalidate(FALSE);
			}
			break; // Preserve normal MFC notification routing.

		case WM_NOTIFY:
			if (reinterpret_cast<NMHDR*>(lParam)->idFrom == EditId &&
				reinterpret_cast<NMHDR*>(lParam)->code == EN_SELCHANGE && !m_impl->formatting)
				m_impl->UpdateStatus();
			break;

		case WM_CONTEXTMENU:
			m_impl->ContextMenu(CPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)), this);
			return 0;

		case WM_DESTROY:
			m_impl->Stop();
			break;
    }

	return CWnd::WindowProc(message, wParam, lParam);
}

CRichEditCtrl &CJsRichEditCtrl::GetEdit()
{
    return m_impl->edit;
}

const CRichEditCtrl &CJsRichEditCtrl::GetEdit() const
{
    return m_impl->edit;
}

CStatusBarCtrl &CJsRichEditCtrl::GetStatusBar()
{
    return m_impl->status;
}

CString CJsRichEditCtrl::GetText() const
{
    // Export conventional Windows CRLF; internal Rich Edit offsets use one CR.
    tstring exported;
    exported.reserve(m_impl->text.size());

	for (TCHAR ch : m_impl->text)
    {
        exported.push_back(ch);

		if (ch == _T('\r'))
            exported.push_back(_T('\n'));
    }

	return CString(exported.c_str());
}

void CJsRichEditCtrl::SetText(LPCTSTR text)
{
    ASSERT(m_impl->edit.GetSafeHwnd());

	if (!m_impl->edit.GetSafeHwnd())
        return;

    m_impl->resetting = true;

#ifdef _UNICODE
	SETTEXTEX request = {ST_DEFAULT, 1200};
#else
	SETTEXTEX request = {ST_DEFAULT, CP_ACP};
#endif

	m_impl->edit.SendMessage(EM_SETTEXTEX, reinterpret_cast<WPARAM>(&request), reinterpret_cast<LPARAM>(text));
    m_impl->edit.EmptyUndoBuffer();

	m_impl->editHistory.clear();

	m_impl->edit.SetModify(FALSE);
    m_impl->ReadText(m_impl->text);

	BuildLines(m_impl->text, m_impl->lines);
    m_impl->resetting = false;

	ClearBreakpoints(true);
    m_impl->MarkDirty();
    m_impl->Layout();
    m_impl->UpdateStatus();
}

void CJsRichEditCtrl::SetReadOnly(bool readOnly)
{
    m_impl->edit.SetReadOnly(readOnly ? TRUE : FALSE);

	if (m_impl->functionPopup.GetSafeHwnd())
        m_impl->functionPopup.UpdateHelp();
}

void CJsRichEditCtrl::GoTo(int line, int column)
{
    line = (std::max)(1, (std::min)(line, static_cast<int>(m_impl->lines.size())));

	const long start = m_impl->lines[line - 1];

	const long end = static_cast<size_t>(line) < m_impl->lines.size() ?
        m_impl->lines[line] - 1 : static_cast<long>(m_impl->text.size());

	const long offset = (std::min)(static_cast<long>((std::max)(1, column) - 1), end - start);

	m_impl->edit.SetSel(start + offset, start + offset);
    m_impl->edit.SendMessage(EM_SCROLLCARET);
    m_impl->UpdateStatus();
}

void CJsRichEditCtrl::GetCaretPosition(int &line, int &column) const
{
    const long position = m_impl->Caret();
    line = LineAt(m_impl->lines, position);

	column = static_cast<int>(position - m_impl->lines[line - 1] + 1);
}

int CJsRichEditCtrl::GetSelectedLineCount() const
{
    return m_impl->SelectedLines();
}

void CJsRichEditCtrl::SetBreakpointCallback(BreakpointCallback callback, void* userData)
{
    m_impl->callback = callback;
    m_impl->callbackData = userData;
}

bool CJsRichEditCtrl::SetBreakpoint(int line, bool enabled, bool notify)
{
    if (line < 1 || static_cast<size_t>(line) > m_impl->lines.size())
        return false;

	auto &points = m_impl->breakpoints;
    auto position = std::lower_bound(points.begin(), points.end(), line);

	const bool exists = position != points.end() && *position == line;
	if (exists == enabled)
        return false;

	if (enabled)
        points.insert(position, line);
    else
        points.erase(position);

	if (m_impl->gutter.GetSafeHwnd())
        m_impl->gutter.Invalidate(FALSE);

	if (notify)
        m_impl->Notify({ enabled ? BreakpointAction::Added : BreakpointAction::Removed, enabled ? 0 : line, enabled ? line : 0 });

	return true;
}

bool CJsRichEditCtrl::ToggleBreakpoint(int line)
{
    return SetBreakpoint(line, !HasBreakpoint(line));
}

bool CJsRichEditCtrl::HasBreakpoint(int line) const
{
    return std::binary_search(m_impl->breakpoints.begin(), m_impl->breakpoints.end(), line);
}

void CJsRichEditCtrl::ClearBreakpoints(bool notify)
{
    std::vector<int> removed;
    removed.swap(m_impl->breakpoints);

	if (m_impl->gutter.GetSafeHwnd())
        m_impl->gutter.Invalidate(FALSE);

	if (notify)
    {
        for (int line : removed)
            m_impl->Notify({ BreakpointAction::Removed, line, 0 });
    }
}

const std::vector<int> &CJsRichEditCtrl::GetBreakpoints() const
{
    return m_impl->breakpoints;
}

void CJsRichEditCtrl::RegisterIntrinsic(LPCTSTR name, LPCTSTR parameters, LPCTSTR description)
{
    if (!name || !*name)
        return;

	IntrinsicFunction* found = nullptr;

	for (auto &entry : m_impl->intrinsics)
    {
        if (entry.name == name)
        {
            found = &entry;
            break;
        }
    }

	if (!found)
    {
        m_impl->intrinsics.emplace_back();
        found = &m_impl->intrinsics.back();
    }

	found->name = name;
    found->parameters = parameters ? parameters : _T("");
    found->description = description ? description : _T("");

	m_impl->MarkDirty();

	if (m_impl->functionPopup.GetSafeHwnd())
        m_impl->functionPopup.Populate();
}

void CJsRichEditCtrl::ClearIntrinsics()
{
    m_impl->intrinsics.clear();
    m_impl->MarkDirty();

	if (m_impl->functionPopup.GetSafeHwnd())
        m_impl->functionPopup.Populate();
}

const std::vector<CJsRichEditCtrl::IntrinsicFunction> &CJsRichEditCtrl::GetIntrinsics() const
{
    return m_impl->intrinsics;
}

void CJsRichEditCtrl::ShowIntrinsicFunctions()
{
    m_impl->functionPopup.Open(false);
}

void CJsRichEditCtrl::SetConfigurationContext(LPCTSTR context)
{
    m_impl->context = context ? context : _T("");

	if (m_impl->colorPopup.GetSafeHwnd())
        m_impl->colorPopup.UpdateHelp();
}

const CString &CJsRichEditCtrl::GetConfigurationContext() const
{
    return m_impl->context;
}

const CJsRichEditCtrl::ColorConfig &CJsRichEditCtrl::GetColors() const
{
    return m_impl->colors;
}

void CJsRichEditCtrl::SetColors(const ColorConfig &colors)
{
    m_impl->colors = colors;
    m_impl->MarkDirty();

	if (m_impl->edit.GetSafeHwnd())
    {
        m_impl->edit.SetBackgroundColor(FALSE, colors[ColorItem::Background]);
        m_impl->gutter.Invalidate(FALSE);
    }

	if (m_impl->colorPopup.GetSafeHwnd())
        m_impl->colorPopup.list.Invalidate(FALSE);
}

bool CJsRichEditCtrl::LoadColors()
{
    ColorConfig colors = m_impl->colors;
    if (!LoadColorConfiguration(m_impl->context, colors))
        return false;

	SetColors(colors);

	return true;
}

bool CJsRichEditCtrl::SaveColors()
{
    return SaveColorConfiguration(m_impl->context, m_impl->colors);
}

bool CJsRichEditCtrl::LoadColorConfiguration(LPCTSTR context, ColorConfig &colors)
{
    UNREFERENCED_PARAMETER(context);
    UNREFERENCED_PARAMETER(colors);

	return false;
}

bool CJsRichEditCtrl::SaveColorConfiguration(LPCTSTR context, const ColorConfig &colors)
{
    UNREFERENCED_PARAMETER(context);
    UNREFERENCED_PARAMETER(colors);

	return false;
}

void CJsRichEditCtrl::ShowColorConfiguration()
{
    m_impl->colorPopup.Open(true);
}

void CJsRichEditCtrl::SetEditorFont(LPCTSTR face, int pointSize)
{
    m_impl->fontFace = face && *face ? face : _T("Consolas");
    m_impl->pointSize = (std::max)(6, (std::min)(72, pointSize));

	if (m_impl->edit.GetSafeHwnd())
    {
        m_impl->MakeFonts();
        m_impl->Layout();
        m_impl->MarkDirty();
    }
}

void CJsRichEditCtrl::SetMinimumLineNumberDigits(unsigned digits)
{
    m_impl->minDigits = (std::max)(1U, (std::min)(10U, digits));

	if (GetSafeHwnd())
        m_impl->Layout();
}

void CJsRichEditCtrl::SetColorizeDelay(UINT milliseconds)
{
    m_impl->delay = milliseconds;
}

void CJsRichEditCtrl::RequestColorization()
{
    m_impl->MarkDirty();
}

const CString &CJsRichEditCtrl::GetLastError() const
{
    return m_impl->lastError;
}
