// DarkMode.cpp - dark mode is REMOVED FOR NOW (until further notice).
//
// The whole implementation (about 1,700 lines: restyling of controls, dark buttons/combos/lists/headers, the scroll bar overlay, the plain
// scroll bars of lists and trees) is kept in DarkMode.cpp.disabled, which is not part of the build. What is left here keeps the
// interface of DarkMode.h, so the rest of the program is unchanged: the mode is always off, nothing restyles anything, and only the two
// parts the normal look still needs are real: the gaps of CDarkTabCtrl (the grouped mode tabs) and NoDefaultPushButtons.
// Released under the GNU General Public License v2, like the rest of iPodWizard.

#include "stdafx.h"
#include "iPodWizard.h"
#include "DarkMode.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
	UINT g_uChanged = ::RegisterWindowMessageW(L"iPodWizard_DarkModeChanged");

	// Buttons of dialogs inside dialogs (the font panel) never take part in the default push button game of the dialog manager:
	// with two dialogs on top of each other it can go on for ever. They are told so by a small window procedure of their own.
	const WCHAR kNoDefProp[] = L"iPodWizardNoDefPush";
	const WCHAR kNoDefOldProc[] = L"iPodWizardNoDefOldProc";

	LRESULT CALLBACK NoDefButtonProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		WNDPROC pfnOld = (WNDPROC)::GetPropW(hWnd, kNoDefOldProc);
		if (pfnOld == NULL)
			return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
		if (uMsg == WM_GETDLGCODE)
		{
			LRESULT lCode = ::CallWindowProc(pfnOld, hWnd, uMsg, wParam, lParam);
			return (lCode & ~(LRESULT)(DLGC_UNDEFPUSHBUTTON | DLGC_DEFPUSHBUTTON)) | DLGC_BUTTON;
		}
		if (uMsg == WM_NCDESTROY)
		{
			::SetWindowLong(hWnd, GWL_WNDPROC, (LONG)(LONG_PTR)pfnOld);
			::RemovePropW(hWnd, kNoDefOldProc);
			::RemovePropW(hWnd, kNoDefProp);
		}
		return ::CallWindowProc(pfnOld, hWnd, uMsg, wParam, lParam);
	}
}

namespace DarkMode
{
	COLORREF BackColor()         { return RGB(32, 32, 32); }
	COLORREF EditColor()         { return RGB(45, 45, 45); }
	COLORREF TextColor()         { return RGB(232, 232, 232); }
	COLORREF DisabledTextColor() { return RGB(128, 128, 128); }
	COLORREF LineColor()         { return RGB(85, 85, 85); }
	COLORREF SelectedColor()     { return RGB(62, 62, 62); }
	COLORREF CanvasColor()       { return RGB(42, 42, 42); }
	COLORREF CanvasEdgeColor()   { return RGB(120, 120, 120); }
	COLORREF CanvasGridColor()   { return RGB(58, 58, 58); }

	BOOL FontCanvasEnabled()     { return FALSE; }
	UINT ChangedMessage()        { return g_uChanged; }
	BOOL Enabled()               { return FALSE; }

	void Init()                  {}
	void Refresh()               {}
	void SetEnabled(BOOL)        {}
	void ApplyToWindow(HWND)     {}
	void UseClassicScrollBars(HWND) {}

	HBRUSH CtlColor(CDC *, CWnd *, UINT, HBRUSH hDefault)
	{
		return hDefault;
	}

	void NoDefaultPushButtons(HWND hRoot)
	{
		for (HWND h = ::GetWindow(hRoot, GW_CHILD); h; h = ::GetWindow(h, GW_HWNDNEXT))
		{
			WCHAR cls[16];
			if (::GetClassNameW(h, cls, 16) && !_wcsicmp(cls, L"Button"))
			{
				LONG t = ::GetWindowLong(h, GWL_STYLE) & BS_TYPEMASK;
				if ((t == BS_PUSHBUTTON || t == BS_DEFPUSHBUTTON) && !::GetPropW(h, kNoDefOldProc))
				{
					::SetPropW(h, kNoDefProp, (HANDLE)1);
					WNDPROC pfnOld = (WNDPROC)(LONG_PTR)::SetWindowLong(h, GWL_WNDPROC, (LONG)(LONG_PTR)NoDefButtonProc);
					::SetPropW(h, kNoDefOldProc, (HANDLE)pfnOld);
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------
// CDarkDialog: a plain CDialog now

IMPLEMENT_DYNAMIC(CDarkDialog, CDialog)

CDarkDialog::CDarkDialog()
{
}

CDarkDialog::CDarkDialog(UINT nIDTemplate, CWnd *pParentWnd)
	: CDialog(nIDTemplate, pParentWnd)
{
}

CDarkDialog::CDarkDialog(LPCTSTR lpszTemplateName, CWnd *pParentWnd)
	: CDialog(lpszTemplateName, pParentWnd)
{
}

BEGIN_MESSAGE_MAP(CDarkDialog, CDialog)
END_MESSAGE_MAP()

BOOL CDarkDialog::OnInitDialog()
{
	return CDialog::OnInitDialog();
}

// ---------------------------------------------------------------------------
// CDarkTabCtrl: the tabs are the ones of Windows; only the gaps between groups of tabs are its own

IMPLEMENT_DYNAMIC(CDarkTabCtrl, CTabCtrl)

BEGIN_MESSAGE_MAP(CDarkTabCtrl, CTabCtrl)
	ON_WM_SIZE()
	ON_WM_KEYDOWN()
	ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

// Items that are gaps carry this in their lParam
static const LPARAM kGapMarker = 0x47415021;

void CDarkTabCtrl::AddGap(int index)
{
	TCITEM ti;
	ZeroMemory(&ti, sizeof(ti));
	ti.mask = TCIF_TEXT | TCIF_PARAM;
	ti.pszText = const_cast<LPTSTR>(_T("   "));         // a little room: the width of a gap is that of its text
	ti.lParam = kGapMarker;
	InsertItem(index, &ti);
	SendMessage(TCM_SETMINTABWIDTH, 0, 8);               // by default no tab is narrower than a gap should be
}

BOOL CDarkTabCtrl::IsGap(int index) const
{
	if (index < 0 || index >= GetItemCount())
		return FALSE;
	TCITEM ti;
	ZeroMemory(&ti, sizeof(ti));
	ti.mask = TCIF_PARAM;
	if (!const_cast<CDarkTabCtrl *>(this)->GetItem(index, &ti))
		return FALSE;
	return ti.lParam == kGapMarker;
}

// The gaps are cut out of the window: nothing is drawn there and clicks go through. A little is kept on each side (the
// selected tab is drawn a little wider than its place) and at the bottom (the line along the page).
void CDarkTabCtrl::UpdateGaps()
{
	if (!::IsWindow(m_hWnd))
		return;
	CRect rcClient;
	GetClientRect(&rcClient);
	HRGN hRgn = NULL;
	if (!rcClient.IsRectEmpty())
		for (int i = 0; i < GetItemCount(); i++)
		{
			if (!IsGap(i))
				continue;
			CRect rc;
			GetItemRect(i, &rc);
			if (rc.Width() <= 6)
				continue;
			if (hRgn == NULL)
				hRgn = ::CreateRectRgn(0, 0, rcClient.Width(), rcClient.Height());
			HRGN hCut = ::CreateRectRgn(rc.left + 2, 0, rc.right - 2, rc.bottom - 2);
			::CombineRgn(hRgn, hRgn, hCut, RGN_DIFF);
			::DeleteObject(hCut);
		}
	SetWindowRgn(hRgn, TRUE);               // the window owns the region now; NULL takes one from before away
}

void CDarkTabCtrl::OnSize(UINT nType, int cx, int cy)
{
	CTabCtrl::OnSize(nType, cx, cy);
	UpdateGaps();
}

// The arrow keys move from tab to tab; a gap is jumped over
void CDarkTabCtrl::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (nChar == VK_LEFT || nChar == VK_RIGHT)
	{
		int step = nChar == VK_RIGHT ? 1 : -1;
		int cur = GetCurSel();
		int n = GetItemCount();
		int next = cur + step;
		if (cur >= 0 && next >= 0 && next < n && IsGap(next))
		{
			while (next >= 0 && next < n && IsGap(next))
				next += step;
			if (next >= 0 && next < n)
			{
				NMHDR nm;
				nm.hwndFrom = m_hWnd;
				nm.idFrom = GetDlgCtrlID();
				nm.code = TCN_SELCHANGING;
				CWnd *pParent = GetParent();
				if (pParent == NULL || !pParent->SendMessage(WM_NOTIFY, nm.idFrom, (LPARAM)&nm))
				{
					SetCurSel(next);
					nm.code = TCN_SELCHANGE;
					if (pParent != NULL)
						pParent->SendMessage(WM_NOTIFY, nm.idFrom, (LPARAM)&nm);
				}
			}
			return;
		}
	}
	Default();
}

void CDarkTabCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
	TCHITTESTINFO hit;
	hit.pt = point;
	hit.flags = 0;
	int i = HitTest(&hit);
	if (i >= 0 && IsGap(i))
		return;                             // what is left of a gap next to its neighbours is not a tab
	Default();
}
