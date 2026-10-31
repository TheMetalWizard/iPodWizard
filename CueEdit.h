#pragma once

#define IDT_ADDRCUE 4711                       // timer of a page that keeps the grey hint of its Hex GoTo box up to date

// An edit box that shows a hint in grey while it is empty and does not have the focus (a click on it or a key makes the hint go).
// Used by the Change Value / Change String boxes of the Strings and the layout pages (the code is in LayoutDialog.cpp).
// The offset of the hex view in the form the Hex GoTo box takes it ("00 1A 85 AC"), for the grey hint of that box
CString OffsetCue(DWORD offset);

class CCueEdit : public CEdit
{
public:
	void SetCue(const CString &s);
protected:
	CString m_Cue;
	afx_msg void OnPaint();
	afx_msg void OnSetFocus(CWnd *pOld);
	afx_msg void OnKillFocus(CWnd *pNew);
	DECLARE_MESSAGE_MAP()
};
