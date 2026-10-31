// hexeditor.h

#pragma once

#include "Firmware.h"
#include "iPodWizard.h"

// CHexWnd

class CHexWnd : public CWnd
{
	DECLARE_DYNAMIC(CHexWnd)

public:
	CHexWnd();
	virtual ~CHexWnd();

protected:
	DECLARE_MESSAGE_MAP()

public:
	BOOL RegisterWindowClass();
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
	afx_msg LRESULT OnDarkModeChanged(WPARAM wParam, LPARAM lParam);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	LPBYTE m_Buffer;
	DWORD m_BufferLen;
	CFirmware *m_pFirmware;
	DWORD m_Offset;
	void SetFirmware(CFirmware *pFirmware);
	void SetOffset(DWORD offset, BOOL bSetFocus = TRUE);                    // put the cursor there (and clear the marked bytes)
	void SetRange(DWORD offset, DWORD len, BOOL bSetFocus = TRUE);          // same, and mark len bytes from there: the stored form of a value
	void ClearRange();
	DWORD m_RangeStart, m_RangeLen;                     // marked bytes (len 0: none)
	BOOL InRange(DWORD pos) const { return m_RangeLen != 0 && pos >= m_RangeStart && pos - m_RangeStart < m_RangeLen; }
	int DrawText(CDC *pDC, CString &s, LPRECT lpRect, LPRECT lpCalcRect = NULL);
	DWORD m_CursorPos;
	int m_CursorHalf;
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg UINT OnGetDlgCode();
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	BOOL m_bFocus;
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	CRect m_CharRect;
	DWORD m_NumLines;
	int m_SbMax, m_SbPage;                       // what the scroll bar was last told (it is only told again when this changes)
	CScrollBar m_SbV;                              // the scroll bar is a control of its own at the right edge (like the one of the font viewer)
	void EnsureBar();
	void LayoutBar();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	void UpdateScroll();                          // a thumb as long as the lines that show, like the other viewers
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
};
