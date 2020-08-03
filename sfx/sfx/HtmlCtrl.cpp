// Microsoft Systems Journal -- January 2000
// If this code works, it was written by Paul DiLascia.
// If not, I don't know who wrote it.
// Compiles with Visual C++ 6.0, runs on Windows 98 and probably Windows NT too.

#include "StdAfx.h"
#include "HtmlCtrl.h"								//Web browser control

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

IMPLEMENT_DYNAMIC(CHtmlCtrl, CHtmlView)
BEGIN_MESSAGE_MAP(CHtmlCtrl, CHtmlView)
ON_WM_DESTROY()
ON_WM_MOUSEACTIVATE()
END_MESSAGE_MAP()

//////////////////
// Create control in same position as an existing static control with
// the same ID (could be any kind of control, really)
//
BOOL CHtmlCtrl::CreateFromStatic(UINT nID, CWnd* pParent)
{
	CStatic wndStatic;
	if (!wndStatic.SubclassDlgItem(nID, pParent))
		return FALSE;
	
	// Get static control rect, convert to parent's client coords.
	CRect rc;
	wndStatic.GetWindowRect(&rc);
	pParent->ScreenToClient(&rc);
	wndStatic.DestroyWindow();
	
	// create HTML control (CHtmlView)
	BOOL ret = Create(NULL,				// class name
		NULL,							// title
		(WS_CHILD | WS_VISIBLE),		// style
		rc,								// rectangle
		pParent,						// parent
		nID,							// control ID
		NULL);							// frame/doc context not used

	return ret;
}

////////////////
// Override to avoid CView stuff that assumes a frame.
//
void CHtmlCtrl::OnDestroy()
{
	// This is probably unecessary since ~CHtmlView does it, but
	// safer to mimic CHtmlView::OnDestroy.
	if (m_pBrowserApp) 
	{
		//m_pBrowserApp->Release();
		m_pBrowserApp = NULL;
	}

	CWnd::OnDestroy(); // bypass CView doc/frame stuff
}

////////////////
// Override to avoid CView stuff that assumes a frame.
//
int CHtmlCtrl::OnMouseActivate(CWnd* pDesktopWnd, UINT nHitTest, UINT msg)
{
	// bypass CView doc/frame stuff
	return CWnd::OnMouseActivate(pDesktopWnd, nHitTest, msg);
}

//////////////////
// Override navigation handler to pass to "app:" links to virtual handler.
// Cancels the navigation in the browser, since app: is a pseudo-protocol.
//
void CHtmlCtrl::OnBeforeNavigate2( LPCTSTR		lpszURL,
								  DWORD			/*nFlags*/,
								  LPCTSTR		/*lpszTargetFrameName*/,
								  CByteArray&	/*baPostedData*/,
								  LPCTSTR		/*lpszHeaders*/,
								  BOOL*			pbCancel )
{
	if (_tcsstr(lpszURL, _T("res:")) != lpszURL) 
	{
		// Don't navigate in this window - open a browser if there's a link that got clicked.
		ShellExecute(0, _T("open"), lpszURL, NULL, NULL, SW_SHOWNORMAL);

		*pbCancel = TRUE;
	}
	else
	{
		*pbCancel = FALSE;
	}
}


void CHtmlCtrl::OnDraw(CDC *pDC)
{
	// Draw the html control without flickering -- the unfortunate thing is that due to the time taken
	// by the control to render itself, we get a blank space until it's done. Sigh. Better than flashies.

	CPaintDC dc(this); // device context for painting

	CMemDC mdc(dc, this);

	CHtmlView::OnDraw((CDC *)&mdc);

	CRect rcli;
	GetClientRect(rcli);

	pDC->BitBlt(0, 0, rcli.Width(), rcli.Height(), &(mdc.GetDC()), 0, 0, SRCCOPY);
}
