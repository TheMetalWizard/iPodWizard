// PictureWnd.cpp : implementation file
//

#include "stdafx.h"
#include "iPodWizard.h"
#include "PictureWnd.h"

#define PICTUREWND_CLASSNAME    _T("IpodPictureWnd")  // Window class name

// same registered message as DarkMode::ChangedMessage(); registering it here avoids depending on start-up order
static UINT s_uDarkChanged = ::RegisterWindowMessageW(L"iPodWizard_DarkModeChanged");

// CPictureWnd

IMPLEMENT_DYNAMIC(CPictureWnd, CWnd)
CPictureWnd::CPictureWnd()
{
	m_pPicture = NULL;

	m_Origin = CPoint(0, 0);
	m_ZoomIdx = 0;

	RegisterWindowClass();
}

static const int kPicZooms[] = { 1, 2, 3, 4, 6, 8, 10, 12, 16, 20 };
static const int kNumPicZooms = sizeof(kPicZooms) / sizeof(kPicZooms[0]);

int CPictureWnd::GetZoom() const
{
	return kPicZooms[m_ZoomIdx];
}

CSize CPictureWnd::ScaledSize()
{
	if (m_pPicture == NULL)
		return CSize(0, 0);
	CSize size = m_pPicture->GetPictureSize();
	return CSize(size.cx * GetZoom(), size.cy * GetZoom());
}

CRect CPictureWnd::ViewRect() const
{
	CRect rc;
	GetClientRect(&rc);
	if (m_SbV.m_hWnd != NULL)
		rc.right = max(rc.left, rc.right - ::GetSystemMetrics(SM_CXVSCROLL));
	if (m_SbH.m_hWnd != NULL)
		rc.bottom = max(rc.top, rc.bottom - ::GetSystemMetrics(SM_CYHSCROLL));
	return rc;
}

// the bars lie along the right and bottom edge inside the window, as in the font viewer
void CPictureWnd::LayoutBars()
{
	if (m_SbV.m_hWnd == NULL || m_SbH.m_hWnd == NULL)
		return;
	CRect rc;
	GetClientRect(&rc);
	int w = ::GetSystemMetrics(SM_CXVSCROLL), h = ::GetSystemMetrics(SM_CYHSCROLL);
	m_SbV.MoveWindow(rc.right - w, rc.top, w, max(0, rc.Height() - h));
	m_SbH.MoveWindow(rc.left, rc.bottom - h, max(0, rc.Width() - w), h);
}

BOOL CPictureWnd::StepZoom(int dir)
{
	int idx = min(max(m_ZoomIdx + dir, 0), kNumPicZooms - 1);
	if (idx == m_ZoomIdx)
		return FALSE;

	// keep the centre of the view where it is
	CRect rc = ViewRect();
	int oldZoom = GetZoom();
	CPoint centre(rc.Width() / 2 - m_Origin.x, rc.Height() / 2 - m_Origin.y);
	m_ZoomIdx = idx;
	int newZoom = GetZoom();
	m_Origin.x = rc.Width() / 2 - centre.x / oldZoom * newZoom;
	m_Origin.y = rc.Height() / 2 - centre.y / oldZoom * newZoom;

	UpdateScrollBars();
	Invalidate();
	return TRUE;
}

// scroll ranges follow the zoomed size; the origin is kept inside them
void CPictureWnd::UpdateScrollBars()
{
	if (m_SbV.m_hWnd == NULL || m_SbH.m_hWnd == NULL)
		return;
	if (m_pPicture == NULL)
	{
		m_SbV.EnableScrollBar(ESB_DISABLE_BOTH);
		m_SbH.EnableScrollBar(ESB_DISABLE_BOTH);
		return;
	}

	CRect clientRect = ViewRect();
	CSize size = ScaledSize();

	LONG maxx = max(0, size.cx - clientRect.Width());
	LONG maxy = max(0, size.cy - clientRect.Height());
	if (m_Origin.x > 0 || maxx == 0) m_Origin.x = 0;
	if (m_Origin.x < -maxx) m_Origin.x = -maxx;
	if (m_Origin.y > 0 || maxy == 0) m_Origin.y = 0;
	if (m_Origin.y < -maxy) m_Origin.y = -maxy;

	// the same kind of bars as the font viewer: the thumb is as long as the part of the picture that shows, a click on the
	// trough moves by that part, and a bar stays (greyed) when there is nothing to scroll
	m_SbV.EnableScrollBar(ESB_ENABLE_BOTH);
	m_SbH.EnableScrollBar(ESB_ENABLE_BOTH);
	SCROLLINFO si;
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL | SIF_DISABLENOSCROLL;
	si.nMin = 0;
	si.nMax = max(size.cx, 1) - 1;
	si.nPage = clientRect.Width();
	si.nPos = -m_Origin.x;
	m_SbH.SetScrollInfo(&si, TRUE);
	si.nMax = max(size.cy, 1) - 1;
	si.nPage = clientRect.Height();
	si.nPos = -m_Origin.y;
	m_SbV.SetScrollInfo(&si, TRUE);
}

void CPictureWnd::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);
	LayoutBars();
	UpdateScrollBars();
	Invalidate();
}

CPictureWnd::~CPictureWnd()
{
}


BEGIN_MESSAGE_MAP(CPictureWnd, CWnd)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_REGISTERED_MESSAGE(s_uDarkChanged, OnDarkModeChanged)
	ON_WM_HSCROLL()
	ON_WM_VSCROLL()
	ON_WM_MOUSEWHEEL()
	ON_WM_SIZE()
	ON_WM_CREATE()
	ON_WM_DROPFILES()
END_MESSAGE_MAP()

BOOL CPictureWnd::RegisterWindowClass()
{
    WNDCLASS wndcls;
    HINSTANCE hInst = AfxGetInstanceHandle();

    if (!(::GetClassInfo(hInst, PICTUREWND_CLASSNAME, &wndcls)))
    {
        // otherwise we need to register a new class
        wndcls.style            = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
        wndcls.lpfnWndProc      = ::DefWindowProc;
        wndcls.cbClsExtra       = wndcls.cbWndExtra = 0;
        wndcls.hInstance        = hInst;
        wndcls.hIcon            = NULL;
        wndcls.hCursor          = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
        wndcls.hbrBackground    = (HBRUSH) (COLOR_3DFACE + 1);
        wndcls.lpszMenuName     = NULL;
        wndcls.lpszClassName    = PICTUREWND_CLASSNAME;

        if (!AfxRegisterClass(&wndcls))
        {
            AfxThrowResourceException();
            return FALSE;
        }
    }

    return TRUE;
}

// CPictureWnd message handlers

int CPictureWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	DragAcceptFiles(TRUE);
	ModifyStyle(WS_VSCROLL | WS_HSCROLL, WS_CLIPCHILDREN);
	if (m_SbV.m_hWnd == NULL)
	{
		m_SbV.Create(WS_CHILD | WS_VISIBLE | SBS_VERT, CRect(0, 0, 0, 0), this, 100);
		m_SbH.Create(WS_CHILD | WS_VISIBLE | SBS_HORZ, CRect(0, 0, 0, 0), this, 101);
		DarkMode::ApplyToWindow(m_SbV.m_hWnd);
		DarkMode::ApplyToWindow(m_SbH.m_hWnd);
	}
	LayoutBars();
	UpdateScrollBars();
	return 0;
}

void CPictureWnd::OnDropFiles(HDROP hDropInfo)
{
	rjc::RDragDropFiles myFiles (hDropInfo);
    CString buf;

	int iL = 0;
	int filesRead = 0;
	while (myFiles ()) {
		myFiles.sNextFile (buf);
		iL++;
		if (!(GetFileAttributes(buf.GetBuffer()) & FILE_ATTRIBUTE_DIRECTORY))
		{
			GetParent()->SendMessage(WM_APP, 11, (LPARAM)buf.GetBuffer());
			filesRead++;
			if (filesRead==1)
				GetParent()->SendMessage(WM_APP, 10, (LPARAM)buf.Left(buf.ReverseFind('\\')).GetBuffer());
		}
		else
			GetParent()->SendMessage(WM_APP, 10, (LPARAM)buf.GetBuffer());
	}
}

// Dark mode only changes the canvas behind the picture. The picture itself is still drawn pixel by pixel
// from its own data (CPicture::Draw), so every colour, white included, shows exactly as stored.
BOOL CPictureWnd::OnEraseBkgnd(CDC *pDC)
{
	// OnPaint paints all of the window (in memory, then in one go): an erase before it only shows as a flash every time the
	// picture is scrolled or zoomed
	return TRUE;
}

LRESULT CPictureWnd::OnDarkModeChanged(WPARAM, LPARAM)
{
	if (m_SbV.m_hWnd != NULL)
	{
		DarkMode::ApplyToWindow(m_SbV.m_hWnd);
		DarkMode::ApplyToWindow(m_SbH.m_hWnd);
	}
	Invalidate();
	return 0;
}

void CPictureWnd::OnPaint()
{
	if (theApp.m_LoadingFirmware==TRUE || theApp.m_SavingFirmware==TRUE)
		return;

	CPaintDC paintDC(this); // device context for painting

	CRect clientRect;
	GetClientRect(&clientRect);
	CDoubleBufferDC dc(&paintDC, clientRect);   // everything below draws in memory; one blit at the end
	dc.FillSolidRect(&clientRect, DarkMode::Enabled() ? DarkMode::CanvasColor() : ::GetSysColor(COLOR_3DFACE));      // (what an erase did)
	if (m_pPicture != NULL)
	{
		dc.IntersectClipRect(ViewRect());
		CSize size = m_pPicture->GetPictureSize();
		int zoom = GetZoom();
		CSize zs(size.cx * zoom, size.cy * zoom);
		if (DarkMode::Enabled())
		{
			// thin marks just outside the right and bottom edges so the picture's bounds stay visible;
			// they never touch a picture pixel (those are all inside 0..width-1 / 0..height-1)
			CRect rcV(m_Origin.x + zs.cx, m_Origin.y, m_Origin.x + zs.cx + 1, m_Origin.y + zs.cy + 1);
			CRect rcH(m_Origin.x, m_Origin.y + zs.cy, m_Origin.x + zs.cx + 1, m_Origin.y + zs.cy + 1);
			dc.FillSolidRect(&rcV, DarkMode::CanvasEdgeColor());
			dc.FillSolidRect(&rcH, DarkMode::CanvasEdgeColor());
		}
		if (zoom == 1)
			m_pPicture->Draw(&dc, m_Origin);
		else if (size.cx > 0 && size.cy > 0)
		{
			// draw at 1:1 into memory, then enlarge with whole pixels (no smoothing)
			CDC mem;
			CBitmap bmp;
			mem.CreateCompatibleDC(&dc);
			bmp.CreateCompatibleBitmap(&dc, size.cx, size.cy);
			CBitmap *pOld = mem.SelectObject(&bmp);
			m_pPicture->Draw(&mem, CPoint(0, 0));
			int oldMode = dc.SetStretchBltMode(COLORONCOLOR);
			dc.StretchBlt(m_Origin.x, m_Origin.y, zs.cx, zs.cy, &mem, 0, 0, size.cx, size.cy, SRCCOPY);
			dc.SetStretchBltMode(oldMode);
			mem.SelectObject(pOld);
		}
	}

	// Do not call CWnd::OnPaint() for painting messages
}

void CPictureWnd::SetPicture(CPicture *pPicture)
{
	m_pPicture = pPicture;

	if (m_pPicture != NULL)
	{
		m_Origin = CPoint(0, 0);
		UpdateScrollBars();
	}

	Invalidate();
}

// one handler for both bars: bar is SB_HORZ or SB_VERT
static int NewScrollPos(CScrollBar *pBar, UINT nSBCode, int line)
{
	SCROLLINFO si;
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	pBar->GetScrollInfo(&si);
	int maxPos = si.nMax - (int)si.nPage + 1;
	if (maxPos < 0)
		maxPos = 0;
	int v = si.nPos;
	switch (nSBCode)
	{
	case SB_TOP: v = 0; break;
	case SB_BOTTOM: v = maxPos; break;
	case SB_LINEUP: v -= line; break;
	case SB_LINEDOWN: v += line; break;
	case SB_PAGEUP: v -= (int)si.nPage; break;
	case SB_PAGEDOWN: v += (int)si.nPage; break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION: v = si.nTrackPos; break;
	default: return -1;
	}
	if (v > maxPos) v = maxPos;
	if (v < 0) v = 0;
	return v;
}

void CPictureWnd::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	if (m_pPicture == NULL)
		return;
	int v = NewScrollPos(&m_SbH, nSBCode, GetZoom());
	if (v < 0)
		return;
	m_Origin.x = -v;
	UpdateScrollBars();
	Invalidate();
}

void CPictureWnd::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	if (m_pPicture == NULL)
		return;
	int v = NewScrollPos(&m_SbV, nSBCode, GetZoom());
	if (v < 0)
		return;
	m_Origin.y = -v;
	UpdateScrollBars();
	Invalidate();
}

// the mouse wheel scrolls the picture (Shift: sideways), three lines a click like the font viewer
BOOL CPictureWnd::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	if (m_pPicture == NULL)
		return CWnd::OnMouseWheel(nFlags, zDelta, pt);
	int clicks = zDelta / WHEEL_DELTA;
	if (clicks == 0)
		clicks = zDelta > 0 ? 1 : -1;
	BOOL bHorz = (nFlags & MK_SHIFT) ? TRUE : FALSE;
	CScrollBar *pBar = bHorz ? &m_SbH : &m_SbV;
	SCROLLINFO si;
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	pBar->GetScrollInfo(&si);
	int maxPos = max(0, si.nMax - (int)si.nPage + 1);
	int v = min(max(si.nPos - clicks * 3 * GetZoom(), 0), maxPos);
	if (bHorz) m_Origin.x = -v; else m_Origin.y = -v;
	UpdateScrollBars();
	Invalidate();
	return TRUE;
}
