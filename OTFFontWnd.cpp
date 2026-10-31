// FontWnd.cpp : implementation file
//

#include "stdafx.h"
#include "iPodWizard.h"
#include "OTFFontWnd.h"

#define FONTWND_CLASSNAME    _T("IpodOTFFontWnd")  // Window class name

// same registered message as DarkMode::ChangedMessage()
static UINT s_uDarkChanged = ::RegisterWindowMessageW(L"iPodWizard_DarkModeChanged");

int OTFADD_PIXELS=5;
int OTFZOOM_FACTOR=6;

// COTFFontWnd

IMPLEMENT_DYNAMIC(COTFFontWnd, CWnd)
COTFFontWnd::COTFFontWnd()
{
	m_pFont = NULL;
	m_ZoomMode = FALSE;
	m_Grid = FALSE;
	m_bTextMode = FALSE;
	m_pVScroll = NULL;
	m_bDrag = FALSE;
	m_bShowMetrics = TRUE;
	m_bShowRuler = TRUE;
	m_PZoom = 1;
	m_bPGrid = FALSE;
	m_bPMetrics = TRUE;
	m_Sel = -1;
	m_Ascent = m_Descent = 0;
	m_FontLine = m_TotalLines = 0;

	m_Origin = CPoint(0, 0);

	m_BearingX = 0;
	m_BearingY = 0;
	m_Width = 0;
	m_Height = 0;

	OTFZOOM_FACTOR = 6;

	RegisterWindowClass();
}

COTFFontWnd::~COTFFontWnd()
{
}

BEGIN_MESSAGE_MAP(COTFFontWnd, CWnd)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_REGISTERED_MESSAGE(s_uDarkChanged, OnDarkModeChanged)
	ON_WM_HSCROLL()
	ON_WM_VSCROLL()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEWHEEL()
	ON_WM_KEYDOWN()
	ON_WM_GETDLGCODE()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_WM_CAPTURECHANGED()
END_MESSAGE_MAP()

BOOL COTFFontWnd::RegisterWindowClass()
{
    WNDCLASS wndcls;
    HINSTANCE hInst = AfxGetInstanceHandle();

    if (!(::GetClassInfo(hInst, FONTWND_CLASSNAME, &wndcls)))
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
        wndcls.lpszClassName    = FONTWND_CLASSNAME;

        if (!AfxRegisterClass(&wndcls))
        {
            AfxThrowResourceException();
            return FALSE;
        }
    }

    return TRUE;
}

// COTFFontWnd message handlers

void COTFFontWnd::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (m_ZoomMode)
	{
		// the glyph view: a press starts a drag, the image follows the mouse (see OnMouseMove)
		m_bDrag = TRUE;
		m_DragStart = point;
		m_DragOrigin = m_Origin;
		SetCapture();
		return;
	}
	if (m_bTextMode && m_ZoomMode==FALSE)
	{
		// the character under the click becomes the selection; the dialog shows its glyph
		SetFocus();            // the arrow keys now step through the characters (see OnKeyDown)
		int z = max(1, m_PZoom);
		int lh = LineHeight();
		int px = point.x / z, py = point.y / z;
		for (size_t i = 0; i < m_Cells.size(); i++)
		{
			const TextCell &c = m_Cells[i];
			int top = (c.line - m_FontLine) * lh;
			int adv = max((int)c.adv, 4);
			if (c.line >= m_FontLine && px >= c.penx && px < c.penx + adv && py >= top && py < top + lh)
			{
				m_Sel = (int)i;
				Invalidate();
				if (c.valid)
					::SendMessage(::GetParent(m_hWnd), WM_APP, (WPARAM)10, (LPARAM)c.glyph);
				break;
			}
		}
		return;
	}
	if (nFlags==MK_LBUTTON && m_ZoomMode==FALSE)
	{
		//Calc glyph num
		CRect clientRect;
		GetClientRect(&clientRect);

		LONG xoffset = clientRect.left;
		LONG yoffset = clientRect.top;

		BYTE width=0,height=0,advance=0;
		CHAR bearingx=0,bearingy=0;

		BYTE maxh=0;

		int z=0; //font line counter
		int iGlyph=-1;
		for (int c = 0; c < m_pFont->GetNumGlyphs(); c++)
		{
			m_pFont->GetCharMetrics(c, &width, &height, &bearingx, &bearingy, &advance);

			if (height>maxh)
				maxh=height;

			if (xoffset + (width + 2) > clientRect.right)
			{
				z++;
				xoffset = clientRect.left;
				if (z>m_FontLine)
				{
					yoffset += maxh + 2;
					maxh=0;
				}
			}

			if (yoffset + height > clientRect.bottom)
				break;

			if (z>=m_FontLine)
			{
				if (point.x >= xoffset && point.x < (xoffset + width) && point.y >= yoffset && point.y < (yoffset + height))
				{
					iGlyph = c;
					break;
				}
			}
			xoffset += width + 2;
		}
		//
		::SendMessage(::GetParent(m_hWnd), WM_APP, (WPARAM)10, (LPARAM)iGlyph);
	}
}

void COTFFontWnd::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_bDrag && m_ZoomMode && (nFlags & MK_LBUTTON))
	{
		CRect rc;
		GetClientRect(&rc);
		int sizeX = m_Width * OTFZOOM_FACTOR, sizeY = m_Height * OTFZOOM_FACTOR;
		int nx = m_DragOrigin.x + (point.x - m_DragStart.x);
		int ny = m_DragOrigin.y + (point.y - m_DragStart.y);
		// some of the image always stays in view
		nx = min(max(nx, 16 - sizeX), rc.Width() - 16);
		ny = min(max(ny, 16 - sizeY), rc.Height() - 16);
		if (nx != m_Origin.x || ny != m_Origin.y)
		{
			m_Origin.x = nx;
			m_Origin.y = ny;
			Invalidate();
		}
		return;
	}
	CWnd::OnMouseMove(nFlags, point);
}

void COTFFontWnd::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_bDrag)
	{
		m_bDrag = FALSE;
		if (GetCapture() == this)
			ReleaseCapture();
	}
	CWnd::OnLButtonUp(nFlags, point);
}

void COTFFontWnd::OnCaptureChanged(CWnd *pWnd)
{
	m_bDrag = FALSE;
	CWnd::OnCaptureChanged(pWnd);
}

void COTFFontWnd::ChangeZoom(BOOL bMode)
{
	if (bMode==TRUE) //Zoom In
	{
		if (OTFZOOM_FACTOR < 12)
		{
			OTFZOOM_FACTOR += 2;
			OTFADD_PIXELS += 1;
		}
	}
	else //Zoom Out
	{
		if (OTFZOOM_FACTOR > 2)
		{
			OTFZOOM_FACTOR -= 2;
			OTFADD_PIXELS -= 1;
		}
	}

	SetOTFFont(m_pFont);
}

void COTFFontWnd::UpdateDefaultOrigin()
{
	CRect clientRect;
	GetClientRect(&clientRect);
	int zf=OTFZOOM_FACTOR;
	if (m_Grid==TRUE)
		zf-=1;
	m_Origin.x=clientRect.Width()/2-m_Width*zf/2;
	m_Origin.y=clientRect.Height()/2-m_Height*zf/2;
}

void COTFFontWnd::SetGrid(BOOL bGridMode)
{
	m_Grid=bGridMode;
	Invalidate();
}

int COTFFontWnd::GetZoom()
{
	return OTFZOOM_FACTOR;
}

// The canvas is only what is behind the font pixels (they are all drawn explicitly on top, exactly as stored),
// so the dark option changes the look of the empty area and nothing else.
BOOL COTFFontWnd::OnEraseBkgnd(CDC *pDC)
{
	// the glyph view and the text preview paint the whole window themselves (in memory, then in one go): an erase first only
	// shows as a white flash every time the image moves
	if (m_ZoomMode || m_bTextMode)
		return TRUE;
	if (!DarkMode::FontCanvasEnabled())
		return CWnd::OnEraseBkgnd(pDC);

	CRect rc;
	GetClientRect(&rc);
	pDC->FillSolidRect(&rc, DarkMode::CanvasColor());
	return TRUE;
}

LRESULT COTFFontWnd::OnDarkModeChanged(WPARAM, LPARAM)
{
	Invalidate();
	return 0;
}

void COTFFontWnd::OnPaint()
{
	CPaintDC paintDC(this); // device context for painting
	
	CRect clientRect;
	GetClientRect(&clientRect);
	CDoubleBufferDC dc(&paintDC, clientRect);   // everything below draws in memory; one blit at the end

	if (m_pFont != NULL && m_bTextMode && !m_ZoomMode)
	{
		// the text, line by line from the first visible one; a pixel of a glyph is m_PZoom pixels of the screen
		int z = max(1, m_PZoom);
		int lh = LineHeight();
		int visLines = VisibleLines();
		// text is ink on white, as on the iPod: white is no ink, so it does not wipe out the glyph next to it (they overlap when
		// BearingX is negative or the advance is smaller than the glyph)
		dc.FillSolidRect(&clientRect, RGB(255, 255, 255));
		COLORREF gridCol = RGB(205, 205, 205);
		if (m_bPGrid && z >= 3)
		{
			for (int gxl = z - 1; gxl < clientRect.Width(); gxl += z)
				dc.FillSolidRect(gxl, 0, 1, clientRect.Height(), gridCol);
			for (int gyl = z - 1; gyl < clientRect.Height(); gyl += z)
				dc.FillSolidRect(0, gyl, clientRect.Width(), 1, gridCol);
		}
		COLORREF blue = RGB(0, 120, 215);
		for (size_t i = 0; i < m_Cells.size(); i++)
		{
			const TextCell &c = m_Cells[i];
			if (c.line < m_FontLine)
				continue;
			if (c.line >= m_FontLine + visLines + 1)
				break;
			int top = (c.line - m_FontLine) * lh;            // (in pixels of a glyph)
			if (c.valid)
			{
				int gx = c.penx + c.bx;
				int gy = top + m_Ascent - c.by;
				for (int y = 0; y < c.h; y++)
					for (int x = 0; x < c.w; x++)
					{
						COLORREF col = m_pFont->GetFontPixel(c.glyph, CPoint(x, y));
						if (col == RGB(255, 255, 255))
							continue;
						if (z == 1)
							dc.SetPixel(gx + x, gy + y, col);
						else
							dc.FillSolidRect((gx + x) * z, (gy + y) * z, (m_bPGrid && z >= 3) ? z - 1 : z, (m_bPGrid && z >= 3) ? z - 1 : z, col);
					}
			}
			else
			{
				// no glyph in this font for it: an empty box
				CRect rb(c.penx * z, (top + m_Ascent - max(m_Ascent, 6) + 1) * z, (c.penx + max((int)c.adv, 4) - 1) * z, (top + m_Ascent) * z);
				CPen pen(PS_SOLID, 0, RGB(200, 0, 0));
				CPen *old = dc.SelectObject(&pen);
				CBrush *oldb = (CBrush *)dc.SelectStockObject(NULL_BRUSH);
				dc.Rectangle(&rb);
				dc.SelectObject(old);
				dc.SelectObject(oldb);
			}
		}
		// the selected character: its box, and with Metrics the marks of its glyph (the pen and the end of the advance in red, the
		// bitmap of the glyph with its width and height in blue)
		if (m_Sel >= 0 && m_Sel < (int)m_Cells.size() && m_Cells[m_Sel].line >= m_FontLine)
		{
			const TextCell &c = m_Cells[m_Sel];
			int top = (c.line - m_FontLine) * lh;
			int adv = max((int)c.adv, 4);
			CRect rs((c.penx - 1) * z, top * z, (c.penx + adv) * z, (top + lh) * z);
			CPen pen(PS_SOLID, 0, blue);
			CPen *old = dc.SelectObject(&pen);
			CBrush *oldb = (CBrush *)dc.SelectStockObject(NULL_BRUSH);
			dc.Rectangle(&rs);
			dc.SelectObject(old);
			dc.SelectObject(oldb);
			if (m_bPMetrics && c.valid)
			{
				// the same marks, in the same colours, as the glyph view (see OTFFontWnd.h)
				int gx = (c.penx + c.bx) * z, gy = (top + m_Ascent - c.by) * z;
				int gw = c.w * z, gh = c.h * z;
				int off = max(3, z);                                                       // how far the brackets are from the bitmap
				int baseY = (top + m_Ascent) * z;
				// the outline of the bitmap
				if (c.w > 0 && c.h > 0)
				{
					COLORREF edge = RGB(120, 120, 120);
					dc.FillSolidRect(gx - 1, gy - 1, gw + 2, 1, edge);
					dc.FillSolidRect(gx - 1, gy + gh, gw + 2, 1, edge);
					dc.FillSolidRect(gx - 1, gy, 1, gh, edge);
					dc.FillSolidRect(gx + gw, gy, 1, gh, edge);
				}
				dc.FillSolidRect(0, baseY, clientRect.Width(), 1, kOtfColBearingY);          // the baseline
				dc.FillSolidRect(c.penx * z, top * z, 1, lh * z, kOtfColBearingX);           // where the pen was
				dc.FillSolidRect((c.penx + c.adv) * z, top * z, 1, lh * z, kOtfColAdvance);  // where the pen is after the glyph
				if (c.w > 0)
				{
					dc.FillSolidRect(gx, gy - off, gw, 1, kOtfColWidth);                       // the width: a bracket above the bitmap
					dc.FillSolidRect(gx, gy - off, 1, off - 1, kOtfColWidth);
					dc.FillSolidRect(gx + gw - 1, gy - off, 1, off - 1, kOtfColWidth);
				}
				if (c.h > 0)
				{
					dc.FillSolidRect(gx + gw + off, gy, 1, gh, kOtfColHeight);                 // the height: a bracket at the right
					dc.FillSolidRect(gx + gw + 1, gy, off, 1, kOtfColHeight);
					dc.FillSolidRect(gx + gw + 1, gy + gh - 1, off, 1, kOtfColHeight);
				}
			}
		}
	}
	else if (m_pFont != NULL)
	{
		if (m_ZoomMode)
		{
			LONG x, y, px, py;
			int z = OTFZOOM_FACTOR;
			int zf = (m_Grid == TRUE) ? z - 1 : z;

			while (m_Origin.x % z != 0)
				m_Origin.x++;

			while (m_Origin.y % z != 0)
				m_Origin.y++;

			// What is outside of the bitmap of the glyph is grey, what is inside is white with an outline around it. (The window is
			// not erased before it is painted, so all of it is painted here.)
			BOOL canvas = DarkMode::FontCanvasEnabled();
			dc.FillSolidRect(&clientRect, canvas ? DarkMode::CanvasColor() : RGB(200, 200, 200));
			CRect bm(m_Origin.x, m_Origin.y, m_Origin.x + m_Width * z, m_Origin.y + m_Height * z);
			if (m_Width != 0 && m_Height != 0)
				dc.FillSolidRect(&bm, RGB(255, 255, 255));

			// the grid: the gap after every pixel of the glyph (darker outside of the bitmap)
			if (m_Grid == TRUE)
			{
				COLORREF gridOut = canvas ? DarkMode::CanvasGridColor() : RGB(180, 180, 180);
				COLORREF gridIn = RGB(222, 222, 222);
				for (px = z - 1; px < clientRect.Width(); px += z)
					dc.FillSolidRect(px, 0, 1, clientRect.Height(), gridOut);
				for (py = z - 1; py < clientRect.Height(); py += z)
					dc.FillSolidRect(0, py, clientRect.Width(), 1, gridOut);
				if (m_Width != 0 && m_Height != 0)
				{
					for (px = z - 1; px < clientRect.Width(); px += z)
						if (px >= bm.left && px < bm.right)
							dc.FillSolidRect(px, bm.top, 1, bm.Height(), gridIn);
					for (py = z - 1; py < clientRect.Height(); py += z)
						if (py >= bm.top && py < bm.bottom)
							dc.FillSolidRect(bm.left, py, bm.Width(), 1, gridIn);
				}
			}

			// the pixels of the glyph as they are (white is no ink: the white of the bitmap is already there)
			for (x = 0; x < m_Width; x++)
			{
				for (y = 0; y < m_Height; y++)
				{
					COLORREF color = m_pFont->GetFontPixel(m_Glyph, CPoint(x, y));
					if (color == RGB(255, 255, 255))
						continue;
					px = x * z + m_Origin.x;
					py = y * z + m_Origin.y;
					dc.FillSolidRect(px, py, zf, zf, color);
				}
			}

			// the outline of the bitmap
			if (m_Width != 0 && m_Height != 0)
			{
				COLORREF edge = canvas ? RGB(235, 235, 235) : RGB(40, 40, 40);
				dc.FillSolidRect(bm.left - 1, bm.top - 1, bm.Width() + 2, 1, edge);
				dc.FillSolidRect(bm.left - 1, bm.bottom, bm.Width() + 2, 1, edge);
				dc.FillSolidRect(bm.left - 1, bm.top, 1, bm.Height(), edge);
				dc.FillSolidRect(bm.right, bm.top, 1, bm.Height(), edge);
			}

			// draw markers: one colour for each number of the glyph (the boxes of the dialog have the same colours)
			CPen penBX(PS_SOLID, 0, kOtfColBearingX);       // where the pen was (the left line)
			CPen penAdv(PS_SOLID, 0, kOtfColAdvance);       // where the pen is after the glyph (the right line)
			CPen penBY(PS_SOLID, 0, kOtfColBearingY);       // the baseline
			CPen penW(PS_SOLID, 0, kOtfColWidth);           // the bracket above the bitmap
			CPen penH(PS_SOLID, 0, kOtfColHeight);          // the bracket at the right of the bitmap
			CPen hidePen(PS_NULL, 0, RGB(0, 0, 0));         // (what is switched off is drawn with this pen: nothing)
			CPen *pOldPen = dc.SelectObject(m_bShowMetrics ? &penBX : &hidePen);
			int marker1,marker2,hmarker;
			marker1=m_Origin.x-m_BearingX*OTFZOOM_FACTOR-1;
			marker2=m_Origin.x+m_Width*OTFZOOM_FACTOR+(m_Advance-(m_BearingX+m_Width))*OTFZOOM_FACTOR-1;
			hmarker=m_Origin.y+m_BearingY*OTFZOOM_FACTOR-1;

			dc.MoveTo(marker1, 0);
			dc.LineTo(marker1, clientRect.Height());

			dc.SelectObject(m_bShowMetrics ? &penAdv : &hidePen);
			dc.MoveTo(marker2, 0);
			dc.LineTo(marker2, clientRect.Height());

			dc.SelectObject(m_bShowRuler ? &penBY : &hidePen);      // the baseline
			dc.MoveTo(0, hmarker);
			dc.LineTo(clientRect.Width(), hmarker);

			// draw width indicator
			if (m_Width != 0)
			{
				dc.SelectObject(m_bShowMetrics ? &penW : &hidePen);
				dc.MoveTo(m_Origin.x-1, m_Origin.y-2);
				dc.LineTo(m_Origin.x-1, m_Origin.y - OTFADD_PIXELS*2 - 1);
				dc.LineTo(m_Origin.x+m_Width*OTFZOOM_FACTOR-1, m_Origin.y - OTFADD_PIXELS*2 - 1);
				dc.LineTo(m_Origin.x+m_Width*OTFZOOM_FACTOR-1, m_Origin.y-1);
			}

			// draw height indicator
			if (m_Height != 0)
			{
				dc.SelectObject(m_bShowMetrics ? &penH : &hidePen);
				dc.MoveTo(m_Origin.x+m_Width*OTFZOOM_FACTOR, m_Origin.y-1);
				dc.LineTo(m_Origin.x+m_Width*OTFZOOM_FACTOR+OTFADD_PIXELS*2, m_Origin.y-1);
				dc.LineTo(m_Origin.x+m_Width*OTFZOOM_FACTOR+OTFADD_PIXELS*2, m_Origin.y+m_Height*OTFZOOM_FACTOR-1);
				dc.LineTo(m_Origin.x+m_Width*OTFZOOM_FACTOR, m_Origin.y+m_Height*OTFZOOM_FACTOR-1);
			}

			dc.SelectObject(pOldPen);
		}
		else
		{
			LONG xoffset = clientRect.left;
			LONG yoffset = clientRect.top;

			BYTE width=0,height=0,advance=0;
			CHAR bearingx=0,bearingy=0;

			BYTE maxh=0;

			int z=0; //font line counter
			for (int c = 0; c < m_pFont->GetNumGlyphs(); c++)
			{
				int x, y;

				m_pFont->GetCharMetrics(c, &width, &height, &bearingx, &bearingy, &advance);

				if (height>maxh)
					maxh=height;

				if (xoffset + (width + 2) > clientRect.right)
				{
					z++;
					xoffset = clientRect.left;
					if (z>m_FontLine)
					{
						yoffset += maxh + 2;
						maxh=0;
					}
				}

				if (yoffset + height > clientRect.bottom)
					break;

				if (z>=m_FontLine)
				{
					for (y = 0; y < height; y++)
					{
						for (x = 0; x < width; x++)
						{
							COLORREF cr = m_pFont->GetFontPixel(c, CPoint(x, y));
							// if (c != RGB(255, 255, 255))
							dc.SetPixel(x + xoffset /* + ident */, y + yoffset, cr);
						}
					}
				}
				xoffset += width + 2;
			}
		}
	}

	// Do not call CWnd::OnPaint() for painting messages
}

void COTFFontWnd::SetOTFFont(COTFFont *pFont)
{
	m_pFont = pFont;

	if (m_pFont == NULL)
	{
		Invalidate();
		return;
	}

	CRect clientRect;
	GetClientRect(&clientRect);

	UpdateDefaultOrigin();

	if (m_bTextMode && !m_ZoomMode)
	{
		LayoutText();
		UpdateTextScroll();
		Invalidate();
		return;
	}

	if (!m_ZoomMode)
	{
		LONG xoffset = clientRect.left;
		int z=0,y=0;
		BYTE width=0,height=0,advance=0;
		CHAR bearingx=0,bearingy=0;
		BYTE maxh=0;

		for (int c = 0; c < m_pFont->GetNumGlyphs(); c++)
		{
			m_pFont->GetCharMetrics(c, &width, &height, &bearingx, &bearingy, &advance);

			if (height>maxh)
					maxh=height;

			if (xoffset + (width + 2) > clientRect.right)
			{
				z++;
				xoffset = clientRect.left;
				y+=maxh+2;
				maxh=0;
			}
			xoffset += width + 2;
		}
		m_TotalLines=z;

		m_FontLine=0;
		if (y>(clientRect.bottom-clientRect.top))
		{
			EnableScrollBarCtrl(SB_VERT, TRUE);
			SCROLLINFO si;

			si.cbSize = sizeof(SCROLLINFO);
			si.fMask = SIF_ALL;
			si.nMin = 0;
			si.nMax = z+3;
			si.nPage = 4;
			si.nPos = 0;
			si.nTrackPos = 0;

			SetScrollInfo(SB_VERT, &si);
			SetScrollPos(SB_VERT, 0);
		}
		else
			EnableScrollBarCtrl(SB_VERT, FALSE);
	}
	else
	{
		CSize size;
		size.cx = m_Width;
		size.cy = m_Height;
		size.cx *= OTFZOOM_FACTOR;
		size.cy *= OTFZOOM_FACTOR;

		if (size.cx > clientRect.Width())
		{
			EnableScrollBar(SB_HORZ, ESB_ENABLE_BOTH);
			EnableScrollBarCtrl(SB_HORZ, TRUE);
			SetScrollRange(SB_HORZ, clientRect.Width() - size.cx, size.cx - clientRect.Width());
			SetScrollPos(SB_HORZ, m_Origin.x);
		}
		else
		{
			EnableScrollBar(SB_HORZ, ESB_DISABLE_BOTH);
			EnableScrollBarCtrl(SB_HORZ, FALSE);
			//m_Origin.x = 0;
		}

		if (size.cy > clientRect.Height())
		{
			EnableScrollBar(SB_VERT, ESB_ENABLE_BOTH);
			EnableScrollBarCtrl(SB_VERT, TRUE);
			SetScrollRange(SB_VERT, clientRect.Height() - size.cy, size.cy - clientRect.Height());
			SetScrollPos(SB_VERT, m_Origin.y);
		}
		else
		{
			EnableScrollBar(SB_VERT, ESB_DISABLE_BOTH);
			EnableScrollBarCtrl(SB_VERT, FALSE);
		}
	}

	Invalidate();
}

void COTFFontWnd::SetGlyph(WORD glyph)
{
	m_Glyph = glyph;
	Invalidate();
}

void COTFFontWnd::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	CRect clientRect;
	GetClientRect(&clientRect);
	CSize size;
	size.cx = m_Width;
	size.cy = m_Height;

	size.cx *= OTFZOOM_FACTOR;
	size.cy *= OTFZOOM_FACTOR;

	LONG maxx = clientRect.Width() - size.cx;
	LONG minx = size.cx - clientRect.Width();

	SCROLLINFO si;

	switch (nSBCode)
	{
	case SB_LEFT:
		m_Origin.x = minx;
		break;
	case SB_ENDSCROLL:
		break;
	case SB_LINELEFT:
		m_Origin.x += OTFZOOM_FACTOR;
		break;
	case SB_LINERIGHT:
		m_Origin.x -= OTFZOOM_FACTOR;
		break;
	case SB_PAGELEFT:
		m_Origin.x += OTFZOOM_FACTOR*2;
		break;
	case SB_PAGERIGHT:
		m_Origin.x -= OTFZOOM_FACTOR*2;
		break;
	case SB_RIGHT:
		m_Origin.x = maxx;
		break;
	case SB_THUMBPOSITION:
		// Get true 32-bit scroll position
		si.cbSize = sizeof(SCROLLINFO);
		si.fMask = SIF_TRACKPOS;
		GetScrollInfo(SB_HORZ, &si);
		nPos = si.nTrackPos;
		//
		m_Origin.x = -(LONG)nPos;
		break;
	case SB_THUMBTRACK:
		// Get true 32-bit scroll position
		si.cbSize = sizeof(SCROLLINFO);
		si.fMask = SIF_TRACKPOS;
		GetScrollInfo(SB_HORZ, &si);
		nPos = si.nTrackPos;
		//
		m_Origin.x = -(LONG)nPos;
		break;
	}

	if (m_Origin.x > minx) m_Origin.x = minx;
	if (m_Origin.x < maxx) m_Origin.x = maxx;

	SetScrollPos(SB_HORZ, -m_Origin.x);

	Invalidate();
}

// the text of the preview: a line for an arrow, the lines that fit (less one) for a click beside the thumb
void COTFFontWnd::ScrollText(UINT nSBCode)
{
	SCROLLINFO ti;
	memset(&ti, 0, sizeof(ti));
	ti.cbSize = sizeof(ti);
	ti.fMask = SIF_ALL;
	if (m_pVScroll)
		m_pVScroll->GetScrollInfo(&ti);
	else
		GetScrollInfo(SB_VERT, &ti);
	int pos = m_FontLine, page = max(1, VisibleLines() - 1);
	switch (nSBCode)
	{
	case SB_LINEUP: pos -= 1; break;
	case SB_LINEDOWN: pos += 1; break;
	case SB_PAGEUP: pos -= page; break;
	case SB_PAGEDOWN: pos += page; break;
	case SB_TOP: pos = 0; break;
	case SB_BOTTOM: pos = MaxFirstLine(); break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION: pos = ti.nTrackPos; break;
	default: return;
	}
	m_FontLine = min(max(pos, 0), MaxFirstLine());
	if (m_pVScroll)
		m_pVScroll->SetScrollPos(m_FontLine);
	else
		SetScrollPos(SB_VERT, m_FontLine);
	Invalidate();
}

void COTFFontWnd::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	if (m_bTextMode && !m_ZoomMode)
	{
		ScrollText(nSBCode);
		return;
	}
	SCROLLINFO si;

	if (m_ZoomMode)
	{
		CRect clientRect;
		GetClientRect(&clientRect);
		CSize size;
		if (m_Advance > m_Width)
			size.cx = m_Advance;
		else
			size.cx = m_Width;
		size.cy = m_Height;

		size.cx *= OTFZOOM_FACTOR;
		size.cy *= OTFZOOM_FACTOR;

		LONG miny = clientRect.Height() - size.cy;
		LONG maxy = size.cy - clientRect.Height();

		switch (nSBCode)
		{
		case SB_BOTTOM:
			m_Origin.y = miny;
			break;
		case SB_ENDSCROLL:
			break;
		case SB_LINEDOWN:
			m_Origin.y -= OTFZOOM_FACTOR;
			break;
		case SB_LINEUP:
			m_Origin.y += OTFZOOM_FACTOR;
			break;
		case SB_PAGEDOWN:
			m_Origin.y -= OTFZOOM_FACTOR*2;
			break;
		case SB_PAGEUP:
			m_Origin.y += OTFZOOM_FACTOR*2;
			break;
		case SB_TOP:
			m_Origin.y = maxy;
			break;
		case SB_THUMBPOSITION:
			// Get true 32-bit scroll position
			si.cbSize = sizeof(SCROLLINFO);
			si.fMask = SIF_TRACKPOS;
			GetScrollInfo(SB_VERT, &si);
			nPos = si.nTrackPos;
			//
			m_Origin.y = -(LONG)nPos;
			break;
		case SB_THUMBTRACK:
			// Get true 32-bit scroll position
			si.cbSize = sizeof(SCROLLINFO);
			si.fMask = SIF_TRACKPOS;
			GetScrollInfo(SB_VERT, &si);
			nPos = si.nTrackPos;
			//
			m_Origin.y = -(LONG)nPos;
			break;
		}

		if (m_Origin.y < miny) m_Origin.y = miny;
		if (m_Origin.y > maxy) m_Origin.y = maxy;

		SetScrollPos(SB_VERT, -m_Origin.y);
	}
	else //Font picture
	{
		switch (nSBCode)
		{
		case SB_BOTTOM:
			m_FontLine=0;
			break;
		case SB_ENDSCROLL:
			break;
		case SB_LINEDOWN:
			if (m_FontLine!=m_TotalLines)
				m_FontLine++;
			break;
		case SB_LINEUP:
			if (m_FontLine!=0)
				m_FontLine--;
			break;
		case SB_PAGEDOWN:
			m_FontLine += 4;
			if (m_FontLine>m_TotalLines)
				m_FontLine=m_TotalLines;
			break;
		case SB_PAGEUP:
			m_FontLine -= 4;
			if (m_FontLine<0)
				m_FontLine=0;
			break;
		case SB_TOP:
			m_FontLine = m_TotalLines;
			break;
		case SB_THUMBPOSITION:
			// Get true 32-bit scroll position
			si.cbSize = sizeof(SCROLLINFO);
			si.fMask = SIF_TRACKPOS;
			GetScrollInfo(SB_VERT, &si);
			nPos = si.nTrackPos;
			//
			m_FontLine = (LONG)nPos;
			break;
		case SB_THUMBTRACK:
			// Get true 32-bit scroll position
			si.cbSize = sizeof(SCROLLINFO);
			si.fMask = SIF_TRACKPOS;
			GetScrollInfo(SB_VERT, &si);
			nPos = si.nTrackPos;
			//
			m_FontLine = (LONG)nPos;
			break;
		}
		SetScrollPos(SB_VERT, m_FontLine);
	}

	Invalidate();
}

void COTFFontWnd::SetZoomMode(BOOL bZoomMode)
{
	m_ZoomMode = bZoomMode;

	if (IsWindow(m_hWnd))
		Invalidate();
}

void COTFFontWnd::SetMetrics(BYTE height, BYTE width, CHAR bearingx, CHAR bearingy, BYTE advance)
{
	m_BearingX = bearingx;
	m_BearingY = bearingy;
	m_Width = width;
	m_Height = height;
	m_Advance = advance;

	UpdateDefaultOrigin();

	SetScrollPos(SB_HORZ, -m_Origin.x);

	if (IsWindow(m_hWnd))
		Invalidate();
}

// ---- the text mode ----

void COTFFontWnd::SetText(const CString &text)
{
	m_bTextMode = TRUE;
	m_Text = text;
	m_Sel = -1;
	m_FontLine = 0;
	if (m_pFont == NULL || !IsWindow(m_hWnd))
		return;
	LayoutText();
	UpdateTextScroll();
	Invalidate();
}

// every character gets its glyph and its place (the pen moves by the advance; a line is full when the next character does not fit)
void COTFFontWnd::LayoutText()
{
	m_Cells.clear();
	m_Ascent = m_Descent = 0;
	m_TotalLines = 0;
	if (m_pFont == NULL || !IsWindow(m_hWnd))
		return;
	CRect rc;
	GetClientRect(&rc);
	int width = rc.Width() / max(1, m_PZoom);        // in pixels of a glyph

	m_Cells.reserve(m_Text.GetLength());
	int penx = 2, line = 0;
	for (int i = 0; i < m_Text.GetLength(); i++)
	{
		WCHAR ch = m_Text[i];
		if (ch == L'\r')
			continue;
		if (ch == L'\n')
		{
			penx = 2;
			line++;
			continue;
		}
		TextCell c;
		memset(&c, 0, sizeof(c));
		c.ch = ch;
		WORD glyph = 0;
		c.valid = m_pFont->GetCharGlyph((WORD)ch, &glyph);
		if (c.valid)
		{
			c.glyph = glyph;
			m_pFont->GetCharMetrics(glyph, &c.w, &c.h, &c.bx, &c.by, &c.adv);
			m_Ascent = max(m_Ascent, (int)c.by);
			m_Descent = max(m_Descent, (int)c.h - (int)c.by);
		}
		else
			c.adv = 6;
		int adv = (int)c.adv;          // the pen moves by the advance of the glyph, as it does on the iPod
		if (penx + adv > width - 2 && penx > 2)
		{
			penx = 2;
			line++;
		}
		c.penx = penx;
		c.line = line;
		penx += adv;
		m_Cells.push_back(c);
	}
	m_TotalLines = line;
	if (m_Ascent + m_Descent == 0)
		m_Ascent = 10;            // (nothing to draw: still a line of some height)
}

int COTFFontWnd::VisibleLines()
{
	CRect rc;
	GetClientRect(&rc);
	return max(1, rc.Height() / (LineHeight() * max(1, m_PZoom)));
}

// the scroll bar of the text, as the one of the bitmap fonts: the range is all the lines, a page is the lines that fit, the position
// is the first line that is shown (the last one that can be is the one that has the last line at the bottom)
int COTFFontWnd::MaxFirstLine()
{
	return max(0, m_TotalLines + 1 - VisibleLines());
}

void COTFFontWnd::UpdateTextScroll()
{
	m_FontLine = min(max(m_FontLine, 0), MaxFirstLine());
	SCROLLINFO si;
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(SCROLLINFO);
	si.fMask = SIF_ALL;
	si.nMin = 0;
	si.nMax = m_TotalLines;
	si.nPage = VisibleLines();
	si.nPos = m_FontLine;
	if (m_pVScroll)
		m_pVScroll->SetScrollInfo(&si, TRUE);
	else
	{
		EnableScrollBarCtrl(SB_VERT, TRUE);
		SetScrollInfo(SB_VERT, &si);
	}
}

void COTFFontWnd::OnResized()
{
	if (m_pFont == NULL || !IsWindow(m_hWnd))
		return;
	if (m_bTextMode && !m_ZoomMode)
	{
		LayoutText();
		UpdateTextScroll();
		Invalidate();
	}
	else
		SetOTFFont(m_pFont);
}

// the glyph that the boxes of the dialog show becomes the selection (the first character that has it)
void COTFFontWnd::SelectGlyph(WORD glyph)
{
	if (!m_bTextMode)
		return;
	if (m_Sel >= 0 && m_Sel < (int)m_Cells.size() && m_Cells[m_Sel].valid && m_Cells[m_Sel].glyph == glyph)
		return;
	for (size_t i = 0; i < m_Cells.size(); i++)
		if (m_Cells[i].valid && m_Cells[i].glyph == glyph)
		{
			m_Sel = (int)i;
			int line = m_Cells[i].line;
			int vis = VisibleLines();
			if (line < m_FontLine || line >= m_FontLine + vis)
			{
				m_FontLine = line;
				UpdateTextScroll();
			}
			Invalidate();
			return;
		}
	m_Sel = -1;
	Invalidate();
}

// zoom, grid, metrics and ruler of the text preview
void COTFFontWnd::SetPreviewOptions(int zoom, BOOL grid, BOOL metrics)
{
	BOOL relayout = (zoom != m_PZoom);
	m_PZoom = max(1, zoom);
	m_bPGrid = grid;
	m_bPMetrics = metrics;
	if (relayout && m_bTextMode && m_pFont != NULL && IsWindow(m_hWnd))
	{
		int selLine = (m_Sel >= 0 && m_Sel < (int)m_Cells.size()) ? m_Cells[m_Sel].line : -1;
		LayoutText();
		if (selLine >= 0 && m_Sel < (int)m_Cells.size())
			m_FontLine = m_Cells[m_Sel].line;         // (the line of the selected character stays in view)
		UpdateTextScroll();
	}
	if (IsWindow(m_hWnd))
		Invalidate();
}

// the dialog manager would take the arrow keys to move the focus to another control: this window wants them
UINT COTFFontWnd::OnGetDlgCode()
{
	if (m_bTextMode && !m_ZoomMode)
		return DLGC_WANTARROWS;
	return CWnd::OnGetDlgCode();
}

// Left / Right: the previous / next character of the text becomes the selection (the dialog shows its glyph)
void COTFFontWnd::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (m_bTextMode && !m_ZoomMode && m_pFont != NULL && (nChar == VK_LEFT || nChar == VK_RIGHT) && !m_Cells.empty())
	{
		int next = (m_Sel < 0) ? 0 : m_Sel + (nChar == VK_RIGHT ? 1 : -1);
		if (next >= 0 && next < (int)m_Cells.size())
		{
			m_Sel = next;
			const TextCell &c = m_Cells[next];
			int vis = VisibleLines();
			if (c.line < m_FontLine)
				m_FontLine = c.line;
			else if (c.line >= m_FontLine + vis)
				m_FontLine = c.line - vis + 1;
			UpdateTextScroll();
			Invalidate();
			if (c.valid)
				::SendMessage(::GetParent(m_hWnd), WM_APP, (WPARAM)10, (LPARAM)c.glyph);
		}
		return;
	}
	CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}

BOOL COTFFontWnd::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	if (!m_bTextMode || m_ZoomMode)
		return CWnd::OnMouseWheel(nFlags, zDelta, pt);
	int step = zDelta > 0 ? -3 : 3;
	int old = m_FontLine;
	m_FontLine = min(max(m_FontLine + step, 0), MaxFirstLine());
	if (m_FontLine != old)
	{
		UpdateTextScroll();
		Invalidate();
	}
	return TRUE;
}
