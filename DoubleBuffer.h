// DoubleBuffer.h - draw a window's whole client area into memory, then send it to the screen in one go.
//
// Several of iPodWizard's preview windows draw one pixel at a time (SetPixel) or with thousands of tiny
// FillSolidRect calls. Straight onto a window each call goes through the display driver, which is slow
// on any machine and very slow in a virtual machine. Drawing into an in-memory bitmap first keeps the
// calls inside the program, and the result reaches the window with a single BitBlt.
//
// The drawing code itself is unchanged: it just draws on this object instead of the CPaintDC, so every
// pixel ends up exactly where it did before. Released under the GNU General Public License v2.

#pragma once

class CDoubleBufferDC : public CDC
{
public:
	CDoubleBufferDC(CDC *pDest, const CRect &rcClient)
		: m_pDest(pDest), m_hBmp(NULL), m_hOldBmp(NULL), m_rc(rcClient)
	{
		int w = m_rc.Width(), h = m_rc.Height();
		if (w < 1) w = 1;       // nothing to draw: keep a 1x1 surface so the drawing code still has a DC
		if (h < 1) h = 1;
		m_nW = w;
		m_nH = h;

		CreateCompatibleDC(pDest);

		BITMAPINFO bmi;
		ZeroMemory(&bmi, sizeof(bmi));
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = w;
		bmi.bmiHeader.biHeight = -h;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;
		void *pBits = NULL;
		m_hBmp = ::CreateDIBSection(pDest->GetSafeHdc(), &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
		if (!m_hBmp)
			m_hBmp = ::CreateCompatibleBitmap(pDest->GetSafeHdc(), w, h);
		m_hOldBmp = (HBITMAP)::SelectObject(m_hDC, m_hBmp);

		// start from what is on the window now, so anything the drawing code does not touch stays as it was
		::BitBlt(m_hDC, 0, 0, w, h, pDest->GetSafeHdc(), m_rc.left, m_rc.top, SRCCOPY);
	}

	~CDoubleBufferDC()
	{
		if (m_hDC)
		{
			::BitBlt(m_pDest->GetSafeHdc(), m_rc.left, m_rc.top, m_nW, m_nH, m_hDC, 0, 0, SRCCOPY);
			::SelectObject(m_hDC, m_hOldBmp);
		}
		if (m_hBmp)
			::DeleteObject(m_hBmp);
		// the CDC base class deletes the memory DC
	}

private:
	CDC *m_pDest;
	HBITMAP m_hBmp;
	HBITMAP m_hOldBmp;
	CRect m_rc;
	int m_nW, m_nH;
};
