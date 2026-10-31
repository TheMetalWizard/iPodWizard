#pragma once

#include "Picture.h"
#include "RDragDropFiles.h"

// CPictureWnd

class CPictureWnd : public CWnd
{
	DECLARE_DYNAMIC(CPictureWnd)

public:
	CPictureWnd();
	virtual ~CPictureWnd();

protected:
	DECLARE_MESSAGE_MAP()

public:
	BOOL RegisterWindowClass();
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
	afx_msg LRESULT OnDarkModeChanged(WPARAM wParam, LPARAM lParam);
	CPicture * m_pPicture;
	void SetPicture(CPicture *pPicture);
	CPoint m_Origin;
	CScrollBar m_SbV, m_SbH;             // the scroll bars are controls of their own (like the ones of the font viewer), at the right and bottom edge
	CRect ViewRect() const;              // the part of the window the picture shows in (without the scroll bars)
	void LayoutBars();
	int m_ZoomIdx;                       // entry of the list of zooms (same list as the font preview)
	int GetZoom() const;
	BOOL StepZoom(int dir);              // returns TRUE if the zoom changed
	CSize ScaledSize();
	void UpdateScrollBars();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDropFiles(HDROP hDropInfo);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
};


