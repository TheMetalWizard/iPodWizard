#pragma once
#include "OTFFontParser.h"
#include <vector>

// CFontWnd

// the colour of each number of a glyph, in the glyph view and in the boxes of the dialog
#define kOtfColBearingX  RGB(220, 0, 0)
#define kOtfColBearingY  RGB(235, 120, 0)
#define kOtfColAdvance   RGB(0, 150, 0)
#define kOtfColWidth     RGB(0, 0, 230)
#define kOtfColHeight    RGB(150, 0, 190)

class COTFFontWnd : public CWnd
{
	DECLARE_DYNAMIC(COTFFontWnd)

public:
	COTFFontWnd();
	virtual ~COTFFontWnd();

protected:
	DECLARE_MESSAGE_MAP()

public:
	BOOL RegisterWindowClass();
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
	afx_msg LRESULT OnDarkModeChanged(WPARAM wParam, LPARAM lParam);
	COTFFont * m_pFont;
	void SetOTFFont(COTFFont *pFont);
	CPoint m_Origin;
	int m_FontLine,m_TotalLines;
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	BOOL m_ZoomMode, m_Grid;
	void SetZoomMode(BOOL bZoomMode);
	void ChangeZoom(BOOL bMode);
	void UpdateDefaultOrigin();
	void SetGrid(BOOL bGridMode);
	int GetZoom();
	WORD m_Glyph;
	void SetGlyph(WORD glyph);
	BYTE m_Width, m_Height, m_Advance;
	CHAR m_BearingX, m_BearingY;
	void SetMetrics(BYTE height, BYTE width, CHAR bearingx, CHAR bearingy, BYTE advance);

	// The text mode (the preview of the characters): the text is drawn with the glyphs of the font, the way the iPod places them
	// (the pen moves by the advance, a glyph starts BearingX after the pen and BearingY above the baseline).
	struct TextCell
	{
		WCHAR ch;
		BOOL valid;            // the font has a glyph for it
		WORD glyph;            // the index of GetCharMetrics / GetFontPixel
		BYTE w, h, adv;
		CHAR bx, by;
		int penx, line;
	};
	BOOL m_bShowMetrics, m_bShowRuler;   // (zoom view) the bearing / advance / width / height marks; the baseline
	int m_PZoom;                         // (text preview) the size of a pixel of a glyph on the screen
	BOOL m_bPGrid, m_bPMetrics;
	void SetPreviewOptions(int zoom, BOOL grid, BOOL metrics);
	BOOL m_bTextMode;
	CString m_Text;
	std::vector<TextCell> m_Cells;
	int m_Sel;                 // the selected cell (-1: none)
	int m_Ascent, m_Descent;   // the most that a glyph of the font goes above / below the baseline
	void SetText(const CString &text);
	void LayoutText();
	void SelectGlyph(WORD glyph);
	void OnResized();              // the window has another size: the text is placed again / the glyph is centred again
	int LineHeight() const { return m_Ascent + m_Descent + 2; }
	int VisibleLines();
	void UpdateTextScroll();
	int MaxFirstLine();
	CScrollBar *m_pVScroll;        // the scroll bar of the text (a control of the dialog, as the one of the bitmap fonts)
	void ScrollText(UINT nSBCode);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	// (glyph view) the image follows the mouse while it is dragged
	BOOL m_bDrag;
	CPoint m_DragStart, m_DragOrigin;
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnCaptureChanged(CWnd *pWnd);
	afx_msg UINT OnGetDlgCode();
};


