// DarkMode.h - optional dark mode. REMOVED FOR NOW (until further notice): DarkMode.cpp is a small stub with this interface, the whole
// implementation is in DarkMode.cpp.disabled. To bring it back, put that file back as DarkMode.cpp and re-add the handlers OnCtlColor,
// OnEraseBkgnd, OnShowWindow, OnDarkModeChanged to CDarkDialog and OnPaint, OnEraseBkgnd to CDarkTabCtrl, and the Preferences options.
//
// Works on any Windows that runs iPodWizard: colours are painted by the program
// itself. Windows 10/11 extras (dark title bar, dark scroll bars, buttons, lists)
// are looked up at run time and used only where they exist, so Windows XP is
// unaffected. With dark mode off nothing here changes how the program looks.
//
// Released under the GNU General Public License v2, like the rest of iPodWizard.

#pragma once

namespace DarkMode
{
	void Init();                          // call once at start-up, after the profile is available
	BOOL Enabled();
	void SetEnabled(BOOL bOn);            // turns dark mode on/off, saves the choice and restyles every open window
	void ApplyToWindow(HWND hRoot);       // restyle hRoot and all of its child controls to the current mode
	void UseClassicScrollBars(HWND hWnd);    // a list or tree: in the light mode its scroll bars get the plain look with arrow buttons
	void NoDefaultPushButtons(HWND hRoot);   // the push buttons of hRoot never become the "default button" of the dialog manager
	HBRUSH CtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor, HBRUSH hDefault);

	COLORREF BackColor();
	COLORREF EditColor();
	COLORREF TextColor();
	COLORREF DisabledTextColor();
	COLORREF LineColor();
	COLORREF SelectedColor();
	COLORREF CanvasColor();               // background behind pictures; picture pixels are always drawn as they are
	COLORREF CanvasEdgeColor();
	COLORREF CanvasGridColor();           // cells of the zoomed font canvas

	BOOL FontCanvasEnabled();             // dark mode on AND the "dark font canvas" preference ticked
	void Refresh();                       // repaint everything after a related preference changed

	UINT ChangedMessage();                // registered message sent to every window when the mode changes
}

// Base for every iPodWizard dialog: paints dark when dark mode is on, otherwise
// behaves exactly like CDialog.
class CDarkDialog : public CDialog
{
	DECLARE_DYNAMIC(CDarkDialog)

public:
	CDarkDialog();
	explicit CDarkDialog(UINT nIDTemplate, CWnd *pParentWnd = NULL);
	explicit CDarkDialog(LPCTSTR lpszTemplateName, CWnd *pParentWnd = NULL);

protected:
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
};

// Tab control that draws its own tabs when dark mode is on.
//
// Its tabs can be put into groups: a "gap" is an item that is not a tab but a piece of room between two groups.
// The room is cut out of the control, so what is behind it shows, and it can never be selected.
class CDarkTabCtrl : public CTabCtrl
{
	DECLARE_DYNAMIC(CDarkTabCtrl)

public:
	void AddGap(int index);               // insert a gap as item number `index`
	BOOL IsGap(int index) const;
	void UpdateGaps();                    // cut the gaps out; call after the items or the size have changed

protected:
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);

	DECLARE_MESSAGE_MAP()
};
