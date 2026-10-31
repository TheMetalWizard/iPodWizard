// FontsDialog.cpp : implementation file
//

#include "stdafx.h"
#include "iPodWizard.h"
#include "FontsDialog.h"
#include ".\fontsdialog.h"
#include "OTFFontParser.h"
#include "Picture.h"


// helper routines

/*void DDX_MyText(CDataExchange *pDX, int nIDC, DWORD &value)
{
	CString s;

	if (pDX->m_bSaveAndValidate)
	{
		pDX->m_pDlgWnd->GetDlgItem(nIDC)->GetWindowText(s);
		WORD temp;
		WORD &val=temp;
		if (_stscanf(s, TEXT("%u"), &val) != 1)
		{
			val = 0;
		}
		value=val;
	}
	else
	{
		s.Format(TEXT("%d"), value);
		pDX->m_pDlgWnd->GetDlgItem(nIDC)->SetWindowText(s);
	}
}*/

void DDX_MyText(CDataExchange *pDX, int nIDC, WORD &value)
{
	CString s;

	if (pDX->m_bSaveAndValidate)
	{
		pDX->m_pDlgWnd->GetDlgItem(nIDC)->GetWindowText(s);
		if (_stscanf(s, TEXT("%d"), &value) != 1)
		{
			value = 0;
		}
	}
	else
	{
		s.Format(TEXT("%d"), value);
		pDX->m_pDlgWnd->GetDlgItem(nIDC)->SetWindowText(s);
	}
}

void DDX_MyText(CDataExchange *pDX, int nIDC, SHORT &value)
{
	CString s;

	if (pDX->m_bSaveAndValidate)
	{
		pDX->m_pDlgWnd->GetDlgItem(nIDC)->GetWindowText(s);
		if (_stscanf(s, TEXT("%d"), &value) != 1)
		{
			value = 0;
		}
	}
	else
	{
		s.Format(TEXT("%d"), value);
		pDX->m_pDlgWnd->GetDlgItem(nIDC)->SetWindowText(s);
	}
}

void DDX_MyTextHex(CDataExchange *pDX, int nIDC, WORD &value)
{
	CString s;

	if (pDX->m_bSaveAndValidate)
	{
		value = 0;
		pDX->m_pDlgWnd->GetDlgItem(nIDC)->GetWindowText(s);
		s.MakeLower();
		for (int i = 0; i < min(4, s.GetLength()); i++)
		{
			TCHAR c = ((LPCTSTR)s)[i];
			if (c >= '0' && c <= '9')
			{
				value = (value << 4) + (c - '0');
			}
			else if (c >= 'a' && c <= 'f')
			{
				value = (value << 4) + (c - 'a' + 10);
			}
			else
				return;
		}
	}
	else
	{
		s.Format(TEXT("%04X"), value);
		pDX->m_pDlgWnd->GetDlgItem(nIDC)->SetWindowText(s);
	}
}

struct RecGuard
{
	static int depth;
	const char *name;
	RecGuard(const char *n) : name(n) { depth++; }
	~RecGuard() { depth--; }
};
int RecGuard::depth = 0;



// The panel of the Fonts tab: the characters of the font in a text box, a preview of that text drawn the way the iPod does it
// (the pen moves by the width of each character, a character starts "indent" to the right of the pen), and the bitmap of the font
// as an editor. It is a child of the tab and works on the font of the tab, so what is changed here is changed in the firmware.
#define WM_FONTPANEL_PICK (WM_APP + 30)		// to the tab: the character (wParam) that was clicked in the preview
#define WM_FONTPANEL_EDITED (WM_APP + 31)	// to the tab: pixels of the bitmap were changed

class CFontPreviewDlg : public CDarkDialog
{
public:
	CFontPreviewDlg(CIpodFont *pFont, CWnd *pNotify) : CDarkDialog(IDD_FONTPREVIEW_DIALOG, pNotify), m_pFont(pFont), m_pNotify(pNotify), m_bLoaded(FALSE), m_Sel(-1), m_bEdited(FALSE), m_bPainting(FALSE), m_PaintVal(0), m_Pick(0), m_Depth(1), m_RulerOff(0), m_bUserText(FALSE), m_bSettingText(FALSE), m_bEditor(FALSE), m_EditorPct(60), m_bSplitDrag(FALSE), m_bGlyphFocus(FALSE), m_bExt(FALSE), m_ExtO1(0), m_ExtO2(0), m_ExtW(0), m_ExtI(0), m_BZoom(5), m_PZoomIdx(0), m_BScroll(0), m_BRow(0), m_BRowStart(0), m_PScroll(0), m_ContentH(0), m_LastX(-1), m_LastY(-1), m_Zoom(1) {}
	CIpodFont *m_pFont;
	CWnd *m_pNotify;
	BOOL m_bLoaded;            // the font has been read: it can be drawn
	void SetEditor(BOOL bOpen);
	void UpdateRulerControls();
	void Reload();             // another font (or the same one, changed): start over with it
	void Undo() { UndoRedo(TRUE); }
	void SelectGlyph(int tableIdx, int glyph);
	int m_RulerOff;                            // the height of the ruler above the baseline of the font (only what is shown)   // the selection moves to a glyph that was chosen in the boxes of the tab
	void Redo() { UndoRedo(FALSE); }
	void RebuildText(BOOL bForce = FALSE);   // the text box gets every character that the font has (unless the user typed a text of their own: then only with bForce)

	// every character of the preview as drawn
	struct Cell
	{
		CRect rc;
		WCHAR ch;
		BOOL mapped;
		DWORD tableIndex;      // entry of the character mapping table
		DWORD glyph;           // entry of the metrics (what the table points at)
		DWORD o1, o2;
		SHORT width, ident;
		int penx, peny, gw;    // in pixels of the bitmap, from the top left of the preview
	};
	std::vector<Cell> m_Cells;
	std::vector<int> m_Lines;  // top of each line of text, in pixels of the bitmap
	int m_Zoom;
	int m_Sel;                 // the character that was clicked, -1 none
	BOOL m_bEdited;            // pixels of the font bitmap were changed here
	struct PixEdit { int x, y, oldv, newv; };
	typedef std::vector<PixEdit> Stroke;       // what one press-drag-release of the mouse changed
	std::vector<Stroke> m_UndoStack, m_RedoStack;
	Stroke m_Stroke;
	BOOL m_bPainting;
	int m_PaintVal;                            // the level that the stroke paints (0 black .. 255 white)
	int m_Pick;                                // the colour of the picker (an index, 0 darkest)
	int m_Depth;                               // bit depth of the font; the picker is there when it is more than 1
	int m_BZoom;                               // zoom of the bitmap viewer (an entry of the list of sizes of a cell)
	BOOL m_bUserText;                          // the text in the box is the user's own
	BOOL m_bSettingText;                       // the text box is being filled by the program
	BOOL m_bExt;                               // the selection is a glyph chosen in the boxes (its metrics below), not a character of the preview
	DWORD m_ExtO1, m_ExtO2;
	int m_ExtW, m_ExtI;
	int m_PZoomIdx;                            // zoom of the preview (an entry of the list of zooms)
	int m_BScroll;                             // the first column of the font bitmap that the viewer shows
	int m_BRow;                                // the first row that it shows (the zoom can be higher than the viewer)
	int m_BRowStart;                           // what BitLayout made of it
	int m_PScroll;                             // the first row of the preview that is shown (pixels of the bitmap)
	int m_ContentH;                            // height of all the text of the preview (pixels of the bitmap)
	int m_LastX, m_LastY;

protected:
	virtual BOOL PreTranslateMessage(MSG *pMsg);
	virtual void OnOK() {}
	virtual void OnCancel() {}
	afx_msg HBRUSH OnCtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	void SelectCell(int hit, BOOL bScrollIntoView);
	afx_msg void OnSetFocus(CWnd *pOld);
	afx_msg void OnEditSetFocus();
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
	BOOL m_bEditor;                            // the bitmap editor is open (closed: only the text box and the preview, nothing can be edited)
	int m_EditorPct;                           // how much of the room under the options the editor gets (the line between it and the preview can be dragged)
	BOOL m_bSplitDrag;
	BOOL SplitterRect(CRect &rc);
	afx_msg BOOL OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT message);
	afx_msg void OnCaptureChanged(CWnd *pWnd);
	BOOL m_bGlyphFocus;                        // the panel itself takes the focus (a click on a glyph), not its first control
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnBZoomIn();
	afx_msg void OnBZoomOut();
	afx_msg void OnPZoomIn();
	afx_msg void OnPZoomOut();
	afx_msg void OnResetText();
	afx_msg void OnEditorOptions();
	afx_msg void OnRulerSpin(NMHDR *pNMHDR, LRESULT *pResult);
	void ShowRulerOff();
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg void OnEnChangeText();
	afx_msg void OnCbnSelchangeZoom();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	DECLARE_MESSAGE_MAP()

private:
	void AreaRect(CRect &rc);
	void DrawText2(CDC &dc, const CRect &area, int zoom);
	void DrawOverlay(CDC &dc, const CRect &area, int zoom, int h);
	void BitAreaRect(CRect &rc);
	BOOL BitLayout(const CRect &rc, int *cell, int *start, int *cols, int *rows);
	void DrawBitEditor(CDC &dc);
	void PaletteRect(CRect &rc);
	void DrawPalette(CDC &dc);
	int Levels() { return m_Depth >= 4 ? 16 : (1 << m_Depth); }
	int LevelValue(int i) { return i * 255 / (Levels() - 1); }
	void PaintCell(int x, int y);
	void EndStroke();
	void UndoRedo(BOOL undo);
	void UpdateUndoButtons();
	void PreviewZoom(int dir);
	BOOL SelMetrics(DWORD *o1, DWORD *o2, int *w, int *i);
	void UpdateScrollBar();
	void SetScroll(int v);
	void SetPScroll(int v);
	void SetRowScroll(int v);
	void FitBitZoom();
	void UpdatePScrollBar(int pageRows);
	void CenterOnSel();
	void BitZoom(int dir);
	BOOL CellAt(CPoint point, int *x, int *y, BOOL clamp);
};

// CFontsDialog dialog

IMPLEMENT_DYNAMIC(CFontsDialog, CDarkDialog)
CFontsDialog::CFontsDialog(CWnd* pParent /*=NULL*/)
	: CDarkDialog(CFontsDialog::IDD, pParent)
	, m_FontName(_T(""))
	, m_pFirmware(NULL)
	, m_FontIndex(0)
	, m_FontSize(0)
	, m_Offset1(0)
	, m_Offset2(0)
	, m_Width(0)
	, m_Ident(0)
	, m_BitDepth(0)
	, m_GroupLen(0)
	, m_GroupStart(0)
	, m_GroupOffset(0)
	, m_GroupExplain(_T(""))
	, m_bDetailsPending(FALSE)
	, m_bLoadingFont(FALSE)
	, m_nFillPhase(0)
	, m_nFillPos(0)
	, m_nFillNumChars(0)
	, m_nFillNumUnicode(0)
	, m_pPanel(NULL)
	, m_bBaseSet(FALSE)
	, m_bEditorOpen(FALSE)
	, m_bReadingBoxes(FALSE)
	, m_WorkLen(0)
{
}

CFontsDialog::~CFontsDialog()
{
	delete m_pPanel;
}

void CFontsDialog::DoDataExchange(CDataExchange* pDX)
{
	CDarkDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_FONTNAME_EDIT, m_FontName);
	DDX_Text(pDX, IDC_FONTSIZE_EDIT, m_FontSize);
	DDX_Text(pDX, IDC_FONTSTYLE_EDIT, m_FontStyle);
	DDX_Control(pDX, IDC_FONTIDX_COMBO, m_FontIndexCombo);
	DDX_MyText(pDX, IDC_OFFSET1_EDIT, m_Offset1);
	DDX_MyText(pDX, IDC_OFFSET2_EDIT, m_Offset2);
	DDX_MyText(pDX, IDC_WIDTH_EDIT, m_Width);
	DDX_MyText(pDX, IDC_IDENT_EDIT, m_Ident);
	DDX_Text(pDX, IDC_BITDEPTH_EDIT, m_BitDepth);
	DDX_Control(pDX, IDC_GROUPIDX_COMBO, m_GroupIndexCombo);
	DDX_MyText(pDX, IDC_GROUPLEN_EDIT, m_GroupLen);
	DDX_MyText(pDX, IDC_GROUPOFFSET_EDIT, m_GroupOffset);
	DDX_MyTextHex(pDX, IDC_GROUPSTART_EDIT, m_GroupStart);
	DDX_Control(pDX, IDC_GROUPOFFSET_DROP, m_GroupOffsetCombo);
	DDX_Control(pDX, IDC_CHARMAPIDX_COMBO, m_CharMapCombo1);
	DDX_Control(pDX, IDC_CHARMAPIDX2_COMBO, m_CharMapCombo2);
}


BEGIN_MESSAGE_MAP(CFontsDialog, CDarkDialog)
	ON_BN_CLICKED(ID_SAVE_BITMAP, OnBnClickedSaveBitmap)
	ON_BN_CLICKED(ID_LOAD_BITMAP, OnBnClickedLoadBitmap)
	ON_CBN_SELCHANGE(IDC_FONTIDX_COMBO, OnCbnSelchangeFontidxCombo)
	ON_WM_TIMER()
	ON_CBN_SELCHANGE(IDC_GROUPIDX_COMBO, OnCbnSelchangeGroupidxCombo)
	ON_CBN_SELCHANGE(IDC_CHARMAPIDX2_COMBO, OnCbnSelchangeCharmapidx2Combo)
	ON_CBN_SELCHANGE(IDC_CHARMAPIDX_COMBO, OnCbnSelchangeCharmapidxCombo)
	ON_EN_CHANGE(IDC_OFFSET1_EDIT, OnEnChangeOffset1Edit)
	ON_EN_CHANGE(IDC_OFFSET2_EDIT, OnEnChangeOffset2Edit)
	ON_EN_CHANGE(IDC_WIDTH_EDIT, OnEnChangeWidthEdit)
	ON_EN_CHANGE(IDC_IDENT_EDIT, OnEnChangeIdentEdit)
	ON_EN_CHANGE(IDC_GROUPSTART_EDIT, OnEnChangeGroupstartEdit)
	ON_EN_CHANGE(IDC_GROUPLEN_EDIT, OnEnChangeGrouplenEdit)
	ON_EN_CHANGE(IDC_GROUPOFFSET_EDIT, OnEnChangeGrouplenEdit)
	ON_CBN_SELCHANGE(IDC_GROUPOFFSET_DROP, OnCbnSelchangeGroupoffsetDrop)
	ON_BN_CLICKED(ID_LOAD_METRICS, OnBnClickedLoadMetrics)
	ON_BN_CLICKED(ID_SAVE_METRICS, OnBnClickedSaveMetrics)
	ON_BN_CLICKED(ID_MAKE_FONT, OnBnClickedMakeFont)
	ON_MESSAGE(WM_FONTPANEL_PICK, OnPanelPick)
	ON_MESSAGE(WM_FONTPANEL_EDITED, OnPanelEdited)
	ON_BN_CLICKED(IDC_FONT_WRITE, OnWriteFont)
	ON_BN_CLICKED(IDC_FONT_OPENEDITOR, OnBnClickedOpenEditor)
	ON_BN_CLICKED(IDC_EXPLAIN_BUTTON, OnBnClickedCharMapExplain)
	ON_BN_CLICKED(IDC_BUTTON1, &CFontsDialog::OnBnClickedButton1)
	ON_EN_CHANGE(IDC_FONTNAME_EDIT, &CFontsDialog::OnEnChangeFontnameEdit)
	ON_WM_SHOWWINDOW()
	ON_WM_SIZE()
	ON_WM_CTLCOLOR()
	ON_BN_CLICKED(IDC_FONTPREVIEW_UNDO, OnPanelUndo)
	ON_BN_CLICKED(IDC_FONTPREVIEW_REDO, OnPanelRedo)
	ON_EN_CHANGE(IDC_FONTSTYLE_EDIT, &CFontsDialog::OnEnChangeFontstyleEdit)
	ON_EN_CHANGE(IDC_FONTSIZE_EDIT, &CFontsDialog::OnEnChangeFontsizeEdit)
	ON_BN_CLICKED(IDC_MULTISAVE_GLYPH, &CFontsDialog::OnBnClickedMultisaveGlyph)
	ON_BN_CLICKED(IDC_LOADFONT_BUTTON, &CFontsDialog::OnBnClickedLoadfontButton)
	ON_WM_DRAWITEM()
	ON_WM_MEASUREITEM()
	ON_BN_CLICKED(IDC_SAVE_IFONT, &CFontsDialog::OnBnClickedSaveIfont)
	ON_CBN_DROPDOWN(IDC_FONTIDX_COMBO, OnCbnDropdownFontidxCombo)
	ON_CBN_CLOSEUP(IDC_FONTIDX_COMBO, OnCbnCloseupFontidxCombo)
END_MESSAGE_MAP()


// CFontsDialog message handlers

BOOL CFontsDialog::OnInitDialog()
{
	CDarkDialog::OnInitDialog();
	DarkMode::NoDefaultPushButtons(m_hWnd);

	{
		// The style is its name (text) and its number (the only part that can be edited, a byte: up to 3 digits). The row is
		// laid out from the right: label, number, name; what is left of the row goes to the list of fonts.
		CWnd *edit = GetDlgItem(IDC_FONTSTYLE_EDIT), *name = GetDlgItem(IDC_FONTSTYLENAME_STATIC), *label = NULL;
		for (CWnd *c = GetWindow(GW_CHILD); c; c = c->GetWindow(GW_HWNDNEXT))
		{
			CString t;
			c->GetWindowText(t);
			if (t == TEXT("Style:"))
				label = c;
		}
		if (edit && name && label)
		{
			CClientDC dc(this);
			CFont *old = dc.SelectObject(edit->GetFont());
			int nameW = max(dc.GetTextExtent(TEXT("Regular")).cx, dc.GetTextExtent(TEXT("Bold")).cx);
			int numW = dc.GetTextExtent(TEXT("255")).cx + 2 * GetSystemMetrics(SM_CXEDGE) + 8;
			dc.SelectObject(old);
			CRect re, rl, rc, gap(4, 0, 4, 0);
			MapDialogRect(&gap);
			edit->GetWindowRect(&re);
			ScreenToClient(&re);
			label->GetWindowRect(&rl);
			ScreenToClient(&rl);
			m_FontIndexCombo.GetWindowRect(&rc);
			ScreenToClient(&rc);
			int nameLeft = re.right - nameW;
			int numLeft = nameLeft - gap.left - numW;
			CEdit *ed = (CEdit *)edit;
			ed->LimitText(3);
			edit->MoveWindow(numLeft, re.top, numW, re.Height());
			CRect rn;
			name->GetWindowRect(&rn);
			ScreenToClient(&rn);
			name->MoveWindow(nameLeft, re.top + (re.Height() - rn.Height()) / 2, nameW, rn.Height());
			label->MoveWindow(numLeft - gap.left - rl.Width(), rl.top, rl.Width(), rl.Height());
			int comboRight = numLeft - gap.left - rl.Width() - gap.right;
			m_FontIndexCombo.MoveWindow(rc.left, rc.top, comboRight - rc.left, rc.Height());
		}
	}

	// TODO:  Add extra initialization here

	CaptureBaseLayout();

	// the panel (text box, preview, bitmap editor) takes the place of the holder
	{
		CRect hr;
		GetDlgItem(IDC_FONTPANEL_HOLDER)->GetWindowRect(&hr);
		ScreenToClient(&hr);
		m_pPanel = new CFontPreviewDlg(&m_Font, this);
		m_pPanel->Create(IDD_FONTPREVIEW_DIALOG, this);
		m_pPanel->MoveWindow(&hr);
		m_pPanel->ModifyStyle(0, WS_CLIPSIBLINGS);                              // (Undo and Redo are over the panel: it must not paint over them)
		GetDlgItem(IDC_FONTPREVIEW_UNDO)->ModifyStyle(0, WS_CLIPSIBLINGS);
		GetDlgItem(IDC_FONTPREVIEW_REDO)->ModifyStyle(0, WS_CLIPSIBLINGS);
		m_pPanel->ShowWindow(SW_SHOW);
	}


	//reinterpret_cast<CEdit*>(GetDlgItem(IDC_FONTNAME_EDIT))->SetReadOnly(TRUE);
	//reinterpret_cast<CEdit*>(GetDlgItem(IDC_FONTSTYLE_EDIT))->SetReadOnly(TRUE);
	//reinterpret_cast<CEdit*>(GetDlgItem(IDC_FONTSIZE_EDIT))->SetReadOnly(TRUE);
	int selH = (int)m_FontIndexCombo.SendMessage(CB_GETITEMHEIGHT, (WPARAM)-1, 0);
	m_idxComboClosedH = selH + 8;
	CRect rc; m_FontIndexCombo.GetWindowRect(&rc); ScreenToClient(&rc);
	m_FontIndexCombo.MoveWindow(rc.left, rc.top, rc.Width(), m_idxComboClosedH);

	ApplyEditorState();       // the editor starts closed

	return TRUE;
}

// Editor closed: the panel only has the text box and the preview, Undo / Redo / Write are not there and the three boxes at the
// bottom (Unicode Groups, Glyph Mapping, Glyph Metrics) are greyed out. Editor open: everything works.
void CFontsDialog::ApplyEditorState()
{
	if (!m_bBaseSet)
		return;
	int show = m_bEditorOpen ? SW_SHOW : SW_HIDE;
	GetDlgItem(IDC_FONTPREVIEW_UNDO)->ShowWindow(show);
	GetDlgItem(IDC_FONTPREVIEW_REDO)->ShowWindow(show);
	GetDlgItem(IDC_FONT_WRITE)->ShowWindow(show);
	SetDlgItemText(IDC_FONT_OPENEDITOR, m_bEditorOpen ? _T("Close Editor") : _T("Open Editor"));
	if (m_pPanel && m_pPanel->GetSafeHwnd())
		m_pPanel->SetEditor(m_bEditorOpen);

	CRect marks(0, 210, 0, 0);          // top of the three boxes at the bottom
	MapDialogRect(&marks);
	for (size_t i = 0; i < m_BaseCtls.size(); i++)
	{
		if (m_BaseCtls[i].rc.top < marks.top || !::IsWindow(m_BaseCtls[i].hwnd))
			continue;
		BOOL on = m_bEditorOpen;
		// a fixed width font has no widths and indents of its own, the lists are filled in the background
		if (on)
		{
			UINT id = ::GetDlgCtrlID(m_BaseCtls[i].hwnd);
			if (m_Font.IsFixedWidth() && (id == IDC_OFFSET1_EDIT || id == IDC_OFFSET2_EDIT || id == IDC_WIDTH_EDIT || id == IDC_IDENT_EDIT))
				on = FALSE;
			if (m_bDetailsPending && (m_BaseCtls[i].hwnd == m_CharMapCombo1.m_hWnd || m_BaseCtls[i].hwnd == m_CharMapCombo2.m_hWnd))
				on = FALSE;
		}
		::EnableWindow(m_BaseCtls[i].hwnd, on);
	}
	Invalidate();
}

void CFontsDialog::OnBnClickedOpenEditor()
{
	if (m_bEditorOpen && !ConfirmPending(this))
		return;                 // closing hides Write: what was changed is written or dropped first (or the editor stays open)
	m_bEditorOpen = !m_bEditorOpen;
	ApplyEditorState();
	UpdateWriteButton();
}

// The text of an entry of the list of fonts. The list has the bitmap fonts first (0 ... N-1) and then the OpenType fonts (N ...),
// which say so: "17   Espy Sans Bold 10" / "OTF 3   AppleGothic 18". The style is only said when it is not the regular one.
CString CombinedFontLabel(CFirmware *pFw, DWORD index)
{
	CString label, n;
	DWORD numBitmap = pFw->GetNumFonts();
	if (index < numBitmap)
	{
		CIpodFont font;
		LPBYTE end = pFw->GetFirmwareBuffer() + pFw->GetFirmwareSize();
		if (font.Read(pFw->GetFont(index), end))
		{
			CString style = font.GetFontStyle();
			if (style == TEXT("Regular"))
				n.Format(TEXT("%s %u"), (LPCTSTR)font.GetFontName(), (unsigned)font.GetFontSize());
			else
				n.Format(TEXT("%s %s %u"), (LPCTSTR)font.GetFontName(), (LPCTSTR)style, (unsigned)font.GetFontSize());
		}
		label.Format(TEXT("%u   %s"), (unsigned)index, (LPCTSTR)n);
	}
	else
	{
		DWORD k = index - numBitmap;
		COTFFont font;
		if (font.Read(pFw->GetOTFFont(k), TRUE))
		{
			CString style = font.GetFontStyle();
			if (style == TEXT("Regular"))
				n.Format(TEXT("%s %u"), (LPCTSTR)font.GetFontName(), (unsigned)font.GetFontSize());
			else
				n.Format(TEXT("%s %s %u"), (LPCTSTR)font.GetFontName(), (LPCTSTR)style, (unsigned)font.GetFontSize());
		}
		label.Format(TEXT("OTF %u   %s"), (unsigned)k, (LPCTSTR)n);
	}
	label.TrimRight();
	return label;
}

// the text of every entry of the list of fonts
void CFontsDialog::RefreshFontNames()
{
	m_FontNames.clear();
	if (!m_pFirmware)
		return;
	DWORD n = m_pFirmware->GetNumFonts() + m_pFirmware->GetNumOTFFonts();
	for (DWORD i = 0; i < n; i++)
		m_FontNames.push_back(CombinedFontLabel(m_pFirmware, i));
}

void CFontsDialog::OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpMIS)
{
	if (nIDCtl != IDC_FONTIDX_COMBO)
	{
		CDarkDialog::OnMeasureItem(nIDCtl, lpMIS);
		return;
	}
	CClientDC dc(this);
	CFont *old = dc.SelectObject(GetFont());
	TEXTMETRIC tm;
	dc.GetTextMetrics(&tm);
	dc.SelectObject(old);
	lpMIS->itemHeight = tm.tmHeight + 2;
}

// the box and the list show the index with the name, the style (when it is not regular) and the size of the font after it
void CFontsDialog::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDIS)
{
	if (nIDCtl != IDC_FONTIDX_COMBO)
	{
		CDarkDialog::OnDrawItem(nIDCtl, lpDIS);
		return;
	}
	if (lpDIS->itemID == (UINT)-1)
		return;
	CDC *dc = CDC::FromHandle(lpDIS->hDC);
	BOOL sel = (lpDIS->itemState & ODS_SELECTED) && !(lpDIS->itemState & ODS_COMBOBOXEDIT);
	BOOL dark = DarkMode::Enabled();
	COLORREF back = sel ? (dark ? DarkMode::SelectedColor() : GetSysColor(COLOR_HIGHLIGHT)) : (dark ? DarkMode::EditColor() : GetSysColor(COLOR_WINDOW));
	COLORREF text = sel ? (dark ? DarkMode::TextColor() : GetSysColor(COLOR_HIGHLIGHTTEXT)) : (dark ? DarkMode::TextColor() : GetSysColor(COLOR_WINDOWTEXT));
	dc->FillSolidRect(&lpDIS->rcItem, back);
	CString s;
	if (lpDIS->itemID < m_FontNames.size() && !m_FontNames[lpDIS->itemID].IsEmpty())
		s = m_FontNames[lpDIS->itemID];
	else
		s.Format(TEXT("%u"), lpDIS->itemID);
	dc->SetBkMode(TRANSPARENT);
	dc->SetTextColor(text);
	CRect rc = lpDIS->rcItem;
	rc.left += 3;
	dc->DrawText(s, &rc, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
	if (lpDIS->itemState & ODS_FOCUS)
		dc->DrawFocusRect(&lpDIS->rcItem);
}

void CFontsDialog::OnCbnDropdownFontidxCombo()
{
	RefreshFontNames();
	// wide enough for the longest name
	CClientDC dc(this);
	CFont *old = dc.SelectObject(GetFont());
	int w = 60;
	for (size_t i = 0; i < m_FontNames.size(); i++)
		w = max(w, dc.GetTextExtent(m_FontNames[i]).cx + 24);
	dc.SelectObject(old);
	m_FontIndexCombo.SetDroppedWidth(w);
	CWnd* pMain = AfxGetMainWnd();
	if (!pMain) return;
	CRect rcMain, rcCombo;
	pMain->GetClientRect(&rcMain);
	m_FontIndexCombo.GetWindowRect(&rcCombo);
	pMain->ScreenToClient(&rcCombo);
	int dropH = rcMain.bottom - rcCombo.top - 8;
	if (dropH < 50) dropH = 50;
	CRect rc; m_FontIndexCombo.GetWindowRect(&rc); ScreenToClient(&rc);
	m_FontIndexCombo.MoveWindow(rc.left, rc.top, rc.Width(), dropH);
}

void CFontsDialog::OnCbnCloseupFontidxCombo()
{
	CRect rc; m_FontIndexCombo.GetWindowRect(&rc); ScreenToClient(&rc);
	m_FontIndexCombo.MoveWindow(rc.left, rc.top, rc.Width(), m_idxComboClosedH);
}

void CFontsDialog::OnBnClickedCharMapExplain()
{
	MessageBox(TEXT("Offsets: Defines the drawable glyph\nWidth: The point where the next character will be placed\nIndent: The start point of the current glyph"), TEXT("What do these values mean?"));
}

void CFontsDialog::SetFirmware(CFirmware *pFirmware, CLayoutDialog *pLayoutDialog)
{
	m_pFirmware = pFirmware;

	m_pLayoutDialog = pLayoutDialog;

	m_FontIndex = 0;
	if (m_pPanel)
		m_pPanel->m_bUserText = FALSE;        // a text of the user belongs to the firmware they were working on

	m_FontIndexCombo.ResetContent();

	// the bitmap fonts, and after them the OpenType ones (the other page edits those)
	CString s;
	for (DWORD i = 0; i < m_pFirmware->GetNumFonts() + m_pFirmware->GetNumOTFFonts(); i++)
	{
		s.Format(TEXT("%d"), i);
		m_FontIndexCombo.InsertString(i, s);
	}
	m_FontIndexCombo.SetCurSel(0);
	RefreshFontNames();

	reinterpret_cast<CEdit*>(GetDlgItem(IDC_FONTNAME_EDIT))->EnableWindow(TRUE);
	reinterpret_cast<CEdit*>(GetDlgItem(IDC_FONTSTYLE_EDIT))->EnableWindow(TRUE);
	reinterpret_cast<CEdit*>(GetDlgItem(IDC_FONTSIZE_EDIT))->EnableWindow(TRUE);
	ApplyFontEditPolicy();

	UpdateFont();
}

#define IDT_FONTDETAIL 7101

void CFontsDialog::BeginDetailFill(BOOL bDefer)
{
	KillTimer(IDT_FONTDETAIL);

	m_nFillNumChars = m_Font.GetNumChars();
	m_nFillNumUnicode = m_Font.GetNumUnicodeChars();
	if (m_nFillNumUnicode > 0x10000)
		m_nFillNumUnicode = 0x10000;        // the character index is a WORD

	// which character each entry of the table is for: the groups say it (entry = table index + character - start - 1)
	m_EntryChar.assign(m_nFillNumChars, -1);
	for (DWORD g = 0; g < m_Font.GetNumUnicodeGroups(); g++)
	{
		WORD start, offset;
		DWORD len;
		m_Font.GetUnicodeGroup((WORD)g, &start, &len, &offset);
		for (DWORD k = 0; k < len; k++)
		{
			if ((DWORD)offset + k < 1)
				continue;
			DWORD ti = (DWORD)offset + k - 1;
			if (ti < m_nFillNumChars && m_EntryChar[ti] < 0)
				m_EntryChar[ti] = (int)start + (int)k;
		}
	}
	CComboFill::Begin(m_CharMapCombo1, m_nFillNumChars, 16);
	CComboFill::Begin(m_CharMapCombo2, m_nFillNumChars, 5);
	m_nFillPhase = 0;
	m_nFillPos = 1;
	m_bDetailsPending = TRUE;

	if (bDefer)
	{
		// the lists cannot be used until they are complete
		m_CharMapCombo1.EnableWindow(FALSE);
		m_CharMapCombo2.EnableWindow(FALSE);
		// WM_TIMER is only delivered when no input is waiting, so clicks and keys are never held up by the fill
		SetTimer(IDT_FONTDETAIL, 1, NULL);
		return;
	}

	StepDetailFill(0);
	FinishDetailFill();
}

BOOL CFontsDialog::StepDetailFill(DWORD maxMs)
{
	DWORD t0 = GetTickCount();

	if (m_nFillPhase == 0)
	{
		while (m_nFillPos <= m_nFillNumChars)
		{
			// "entry  U+0041 (A)": the entry with the character that it is for
			WCHAR w[48];
			int ch = (m_nFillPos >= 1 && m_nFillPos <= m_EntryChar.size()) ? m_EntryChar[m_nFillPos - 1] : -1;
			if (ch < 0)
				swprintf(w, 48, L"%u", (unsigned)m_nFillPos);
			else if (ch >= 0x20 && ch != 0x7F)
				swprintf(w, 48, L"%u  U+%04X (%c)", (unsigned)m_nFillPos, ch, (WCHAR)ch);
			else
				swprintf(w, 48, L"%u  U+%04X", (unsigned)m_nFillPos, ch);
			CComboFill::AddText(m_CharMapCombo1, w);
			CComboFill::AddNumber(m_CharMapCombo2, (int)m_nFillPos);
			m_nFillPos++;
			if (maxMs && (m_nFillPos & 63) == 0 && GetTickCount() - t0 >= maxMs)
				return FALSE;
		}
		m_nFillPhase = 2;
	}

	return TRUE;
}

void CFontsDialog::FinishDetailFill()
{
	m_bDetailsPending = FALSE;
	KillTimer(IDT_FONTDETAIL);

	CComboFill::End(m_CharMapCombo1);
	m_CharMapCombo1.SetDroppedWidth(170);
	CComboFill::End(m_CharMapCombo2);
	m_CharMapCombo1.EnableWindow(m_bEditorOpen);
	m_CharMapCombo2.EnableWindow(m_bEditorOpen);

	// the first character of the first group is the one that is shown
	m_GroupIndexCombo.SetCurSel(0);
	ReadUnicodeGroup();
	m_CharMapCombo1.SetCurSel(m_GroupOffset > 0 ? m_GroupOffset - 1 : 0);
	UpdateCharMap();
}

void CFontsDialog::EnsureDetails()
{
	if (m_bDetailsPending)
	{
		StepDetailFill(0);
		FinishDetailFill();
	}
}

void CFontsDialog::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == IDT_FONTDETAIL)
	{
		if (!m_bDetailsPending)
			KillTimer(IDT_FONTDETAIL);
		else if (StepDetailFill(12))
			FinishDetailFill();
		return;
	}
	CDarkDialog::OnTimer(nIDEvent);
}

void CFontsDialog::UpdateFont(BOOL bDeferDetails, BOOL bKeepWork)
{
	DWORD i;

	LPBYTE lpBuffer = m_pFirmware->GetFont(m_FontIndex);
	if (lpBuffer == NULL)
		return;

	LPBYTE lpEnd = m_pFirmware->GetFirmwareBuffer() + m_pFirmware->GetFirmwareSize();

	if (bKeepWork && !m_Work.empty())
	{
		if (!m_Font.Read(&m_Work[0], &m_Work[0] + m_Work.size()))
			return;
	}
	else
	{
		if (!m_Font.Read(lpBuffer, lpEnd))
			return;
		// the font is worked on in a copy (with some room after it: the reading of a font checks against the end of the memory)
		DWORD len = m_Font.GetFontBlockLen();
		m_Work.clear();
		m_WorkLen = 0;
		if (len > 0 && len < 0x100000 && lpBuffer + len <= lpEnd)
		{
			m_Work.assign(len + 256, 0);
			memcpy(&m_Work[0], lpBuffer, len);
			if (m_Font.Read(&m_Work[0], &m_Work[0] + m_Work.size()))
				m_WorkLen = len;
			else
			{
				m_Work.clear();
				if (!m_Font.Read(lpBuffer, lpEnd))        // the copy could not be read: the font itself is worked on
					return;
			}
		}
	}

	// While the values below go into the edit boxes their change handlers fire (WriteUnicodeGroup, WriteCharMetrics).
	// They would only write the very same values back, but each one used to rebuild the whole character list.
	m_bLoadingFont = TRUE;

	m_FontName = m_Font.GetFontName();
	m_FontSize = m_Font.GetFontSize();
	m_BitDepth = m_Font.GetFontBitDepth();
	m_FontStyle.Format(TEXT("%u"), (unsigned)m_Font.GetFontStyleID());
	SetDlgItemText(IDC_FONTSTYLENAME_STATIC, m_Font.GetFontStyle());

	UpdateData(FALSE);

	m_FontIndexCombo.SetCurSel(m_FontIndex);

	//

	DWORD nGroups = m_Font.GetNumUnicodeGroups();
	FillGroupCombo(0);
	ReadUnicodeGroup();
	if (m_pPanel)
	{
		m_pPanel->m_bLoaded = TRUE;
		m_pPanel->Reload();
	}

	//
	// the character mapping lists (one entry per glyph) and the unicode character selector (one entry per
	// character) are the big ones: they are filled by BeginDetailFill, either right now or in the background

	BeginDetailFill(bDeferDetails);

	BOOL bCanWrite = m_pFirmware->CanWriteFont(m_FontIndex);

	GetDlgItem(ID_SAVE_BITMAP)->EnableWindow(TRUE);
	GetDlgItem(ID_LOAD_BITMAP)->EnableWindow(bCanWrite);
	GetDlgItem(ID_SAVE_METRICS)->EnableWindow(TRUE);
	GetDlgItem(ID_LOAD_METRICS)->EnableWindow(bCanWrite);
	GetDlgItem(ID_MAKE_FONT)->EnableWindow(bCanWrite);
	GetDlgItem(IDC_LOADFONT_BUTTON)->EnableWindow(TRUE);
	GetDlgItem(IDC_SAVE_IFONT)->EnableWindow(TRUE);
	GetDlgItem(IDC_FONT_OPENEDITOR)->EnableWindow(TRUE);

	m_bLoadingFont = FALSE;
	UpdateWriteButton();
}

void CFontsDialog::OnBnClickedSaveBitmap()
{
	CString filename;
	filename.Format(TEXT("%s.bmp"), m_Font.GetFontName());

	CFileDialog dlg(FALSE, TEXT("bmp"), filename, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, TEXT("BMP Files (*.bmp)|*.bmp||"), this);
	MO_LOAD_RESOURCES_PATH(dlg)

	if (dlg.DoModal() != IDOK)
		return;
	MO_SAVE_RESOURCES_PATH(dlg)
	CSize size = m_Font.GetFontBitmapSize();

	CImage img;
	/*
	size.cx=m_Offset2-m_Offset1;
	size.cy--;

	int x, y;

	int hstart = -1, hend = -1;
	BOOL bLineEmpty;

	for (y = 0; y < size.cy; y++)
	{
		bLineEmpty = TRUE;
		for (x = 0; x < size.cx; x++)
		{
			if (m_Font.GetFontPixel(CPoint(x+m_Offset1, y)) != RGB(255, 255, 255))
			{
				bLineEmpty = FALSE;
				break;
			}
		}
		if (!bLineEmpty)
		{
			hstart = y;
			break;
		}
	}

	for (y = size.cy - 1; y >= 0 && hstart != -1; y--)
	{
		bLineEmpty = TRUE;
		for (x = 0; x < size.cx ; x++)
		{
			if (m_Font.GetFontPixel(CPoint(x+m_Offset1, y)) != RGB(255, 255, 255))
			{
				bLineEmpty = FALSE;
				break;
			}
		}
		if (!bLineEmpty)
		{
			hend = y;
			break;
		}
	}
	if (hstart!=-1)
		hend++;

	if (!img.Create(size.cx, hend - hstart, 24))
	{
		MessageBox(TEXT("Unable to create image!"));
		return;
	}

	for (x = 0; x < size.cx; x++)
	{
		for (y = 0; y < hend - hstart; y++)
		{
			img.SetPixel(x, y, m_Font.GetFontPixel(CPoint(x+m_Offset1, y + hstart)));
		}
	}

	if (FAILED(img.Save(dlg.GetPathName())))
	{
		MessageBox(TEXT("Unable to save image!"));
		return;
	}
	return;
	*/
	if (!img.Create(size.cx, size.cy, 24))
	{
		MessageBox(TEXT("Unable to create image!"));
		return;
	}

	int x, y;

	for (x = 0; x < size.cx; x++)
	{
		for (y = 0; y < size.cy; y++)
		{
			img.SetPixel(x, y, m_Font.GetFontPixel(CPoint(x, y)));
		}
	}

	if (FAILED(img.Save(dlg.GetPathName())))
	{
		MessageBox(TEXT("Unable to save image!"));
		return;
	}
}

void CFontsDialog::OnBnClickedLoadBitmap()
{
	CFileDialog dlg(TRUE, 0, 0, OFN_HIDEREADONLY, TEXT("Bitmap Files (*.bmp, *.gif, *.jpg)|*.bmp;*.gif;*.jpg||"), this);
	MO_LOAD_RESOURCES_PATH(dlg)

	if (dlg.DoModal() != IDOK)
		return;
	MO_SAVE_RESOURCES_PATH(dlg)
	CImage img;
	if (FAILED(img.Load(dlg.GetPathName())))
	{
		MessageBox(TEXT("Unable to load image!"));
		return;
	}

	CSize size = m_Font.GetFontBitmapSize();

	LONG x, y;
	COLORREF color;

	for (x = 0; x < size.cx; x++)
	{
		for (y = 0; y < size.cy; y++)
		{
			if (x < img.GetWidth() && y < img.GetHeight())
				color = img.GetPixel(x, y);
			else
				color = RGB(255, 255, 255);
			m_Font.SetFontPixel(x, y, color);
		}
	}

	UpdateFont(FALSE, TRUE);
}

void CFontsDialog::OnCbnSelchangeFontidxCombo()
{
	DWORD sel = (DWORD)m_FontIndexCombo.GetCurSel();
	if (sel != m_FontIndex && !ConfirmPending(this))
	{
		m_FontIndexCombo.SetCurSel((int)m_FontIndex);       // stay with the font that has the changes
		return;
	}
	if (sel >= m_pFirmware->GetNumFonts())
	{
		// an OpenType font: the other page of this tab edits it (the editor dialog swaps the pages)
		m_FontIndexCombo.SetCurSel((int)m_FontIndex);       // (this page keeps its own font)
		::SendMessage(::GetParent(::GetParent(m_hWnd)), WM_APP, (WPARAM)14, (LPARAM)sel);
		return;
	}
	m_FontIndex = sel;

	UpdateFont(TRUE);
	RefreshFontNames();
	m_FontIndexCombo.Invalidate();
}

void CFontsDialog::OnOK()
{
	// TODO: Add your specialized code here and/or call the base class

	// CDarkDialog::OnOK();
}

void CFontsDialog::OnCancel()
{
	// Esc must not close the page (it is a tab of the editor, not a dialog of its own)
}

// A character was clicked in the preview: the Unicode mapping shows its group, its entry of the table and its glyph.
LRESULT CFontsDialog::OnPanelPick(WPARAM wParam, LPARAM lParam)
{
	EnsureDetails();
	WORD ch = (WORD)wParam;
	for (DWORD i = 0; i < m_Font.GetNumUnicodeGroups(); i++)
	{
		WORD start, offset;
		DWORD len;
		m_Font.GetUnicodeGroup((WORD)i, &start, &len, &offset);
		if (ch >= start && ch < start + len)
		{
			m_GroupIndexCombo.SetCurSel((int)i);
			ReadUnicodeGroup();
			int entry = (int)offset + (ch - start) - 1;
			if (entry >= 0 && entry < m_CharMapCombo1.GetCount())
			{
				m_CharMapCombo1.SetCurSel(entry);
				UpdateCharMap();
			}
			break;
		}
	}
	return 0;
}

void CFontsDialog::UpdateZoomView()
{
	if (m_pPanel)
		m_pPanel->Invalidate();
	UpdateWriteButton();
}

// The Unicode block a character is in (the blocks of the Basic Multilingual Plane that fonts use)
static const struct { WORD first, last; const wchar_t *name; } s_UnicodeBlocks[] = {
	{0x0000, 0x007F, L"Basic Latin"}, {0x0080, 0x00FF, L"Latin-1 Supplement"}, {0x0100, 0x017F, L"Latin Extended-A"},
	{0x0180, 0x024F, L"Latin Extended-B"}, {0x0250, 0x02AF, L"IPA Extensions"}, {0x02B0, 0x02FF, L"Spacing Modifier Letters"},
	{0x0300, 0x036F, L"Combining Diacritical Marks"}, {0x0370, 0x03FF, L"Greek and Coptic"}, {0x0400, 0x04FF, L"Cyrillic"},
	{0x0500, 0x052F, L"Cyrillic Supplement"}, {0x0530, 0x058F, L"Armenian"}, {0x0590, 0x05FF, L"Hebrew"},
	{0x0600, 0x06FF, L"Arabic"}, {0x0700, 0x074F, L"Syriac"}, {0x0750, 0x077F, L"Arabic Supplement"}, {0x0780, 0x07BF, L"Thaana"},
	{0x0900, 0x097F, L"Devanagari"}, {0x0980, 0x09FF, L"Bengali"}, {0x0A00, 0x0A7F, L"Gurmukhi"}, {0x0A80, 0x0AFF, L"Gujarati"},
	{0x0B00, 0x0B7F, L"Oriya"}, {0x0B80, 0x0BFF, L"Tamil"}, {0x0C00, 0x0C7F, L"Telugu"}, {0x0C80, 0x0CFF, L"Kannada"},
	{0x0D00, 0x0D7F, L"Malayalam"}, {0x0D80, 0x0DFF, L"Sinhala"}, {0x0E00, 0x0E7F, L"Thai"}, {0x0E80, 0x0EFF, L"Lao"},
	{0x0F00, 0x0FFF, L"Tibetan"}, {0x1000, 0x109F, L"Myanmar"}, {0x10A0, 0x10FF, L"Georgian"}, {0x1100, 0x11FF, L"Hangul Jamo"},
	{0x1200, 0x137F, L"Ethiopic"}, {0x13A0, 0x13FF, L"Cherokee"}, {0x1400, 0x167F, L"Unified Canadian Aboriginal Syllabics"},
	{0x1680, 0x169F, L"Ogham"}, {0x16A0, 0x16FF, L"Runic"}, {0x1780, 0x17FF, L"Khmer"}, {0x1800, 0x18AF, L"Mongolian"},
	{0x1E00, 0x1EFF, L"Latin Extended Additional"}, {0x1F00, 0x1FFF, L"Greek Extended"}, {0x2000, 0x206F, L"General Punctuation"},
	{0x2070, 0x209F, L"Superscripts and Subscripts"}, {0x20A0, 0x20CF, L"Currency Symbols"}, {0x20D0, 0x20FF, L"Combining Marks for Symbols"},
	{0x2100, 0x214F, L"Letterlike Symbols"}, {0x2150, 0x218F, L"Number Forms"}, {0x2190, 0x21FF, L"Arrows"},
	{0x2200, 0x22FF, L"Mathematical Operators"}, {0x2300, 0x23FF, L"Miscellaneous Technical"}, {0x2400, 0x243F, L"Control Pictures"},
	{0x2440, 0x245F, L"Optical Character Recognition"}, {0x2460, 0x24FF, L"Enclosed Alphanumerics"}, {0x2500, 0x257F, L"Box Drawing"},
	{0x2580, 0x259F, L"Block Elements"}, {0x25A0, 0x25FF, L"Geometric Shapes"}, {0x2600, 0x26FF, L"Miscellaneous Symbols"},
	{0x2700, 0x27BF, L"Dingbats"}, {0x2800, 0x28FF, L"Braille Patterns"}, {0x2E80, 0x2EFF, L"CJK Radicals Supplement"},
	{0x2F00, 0x2FDF, L"Kangxi Radicals"}, {0x3000, 0x303F, L"CJK Symbols and Punctuation"}, {0x3040, 0x309F, L"Hiragana"},
	{0x30A0, 0x30FF, L"Katakana"}, {0x3100, 0x312F, L"Bopomofo"}, {0x3130, 0x318F, L"Hangul Compatibility Jamo"},
	{0x3190, 0x319F, L"Kanbun"}, {0x31A0, 0x31BF, L"Bopomofo Extended"}, {0x31F0, 0x31FF, L"Katakana Phonetic Extensions"},
	{0x3200, 0x32FF, L"Enclosed CJK Letters and Months"}, {0x3300, 0x33FF, L"CJK Compatibility"},
	{0x3400, 0x4DBF, L"CJK Unified Ideographs Extension A"}, {0x4E00, 0x9FFF, L"CJK Unified Ideographs"},
	{0xA000, 0xA4CF, L"Yi Syllables"}, {0xAC00, 0xD7AF, L"Hangul Syllables"}, {0xE000, 0xF8FF, L"Private Use Area"},
	{0xF900, 0xFAFF, L"CJK Compatibility Ideographs"}, {0xFB00, 0xFB4F, L"Alphabetic Presentation Forms"},
	{0xFB50, 0xFDFF, L"Arabic Presentation Forms-A"}, {0xFE20, 0xFE2F, L"Combining Half Marks"},
	{0xFE30, 0xFE4F, L"CJK Compatibility Forms"}, {0xFE50, 0xFE6F, L"Small Form Variants"},
	{0xFE70, 0xFEFF, L"Arabic Presentation Forms-B"}, {0xFF00, 0xFFEF, L"Halfwidth and Fullwidth Forms"},
	{0xFFF0, 0xFFFF, L"Specials"}
};

// the name of the Unicode block of a character ("" when it is in none of the blocks of the list)
const wchar_t *GetUnicodeBlockName(WORD ch)
{
	for (int b = 0; b < (int)(sizeof(s_UnicodeBlocks) / sizeof(s_UnicodeBlocks[0])); b++)
		if (ch >= s_UnicodeBlocks[b].first && ch <= s_UnicodeBlocks[b].last)
			return s_UnicodeBlocks[b].name;
	return L"";
}

// "index  Block name" for every group, the name being that of the block the first character of the group is in
void CFontsDialog::FillGroupCombo(int sel)
{
	m_GroupIndexCombo.SetRedraw(FALSE);
	m_GroupIndexCombo.ResetContent();
	DWORD n = m_Font.GetNumUnicodeGroups();
	int widest = 0;
	CClientDC dc(this);
	CFont *oldFont = dc.SelectObject(m_GroupIndexCombo.GetFont());
	for (DWORD i = 0; i < n; i++)
	{
		WORD start, offset;
		DWORD len;
		m_Font.GetUnicodeGroup((WORD)i, &start, &len, &offset);
		const wchar_t *name = L"";
		for (int b = 0; b < (int)(sizeof(s_UnicodeBlocks) / sizeof(s_UnicodeBlocks[0])); b++)
			if (start >= s_UnicodeBlocks[b].first && start <= s_UnicodeBlocks[b].last)
			{
				name = s_UnicodeBlocks[b].name;
				break;
			}
		CString s;
		s.Format(_T("%u  %s"), (unsigned)i, name);
		s.TrimRight();
		m_GroupIndexCombo.AddString(s);
		widest = max(widest, dc.GetTextExtent(s).cx);
	}
	dc.SelectObject(oldFont);
	m_GroupIndexCombo.SetDroppedWidth(widest + 8 + GetSystemMetrics(SM_CXVSCROLL));
	m_GroupIndexCombo.SetCurSel(sel < 0 ? 0 : sel);
	m_GroupIndexCombo.SetRedraw(TRUE);
	m_GroupIndexCombo.Invalidate();
}

void CFontsDialog::ReadUnicodeGroup()
{
	int i = m_GroupIndexCombo.GetCurSel();

	DWORD temp=m_GroupLen;
	m_Font.GetUnicodeGroup(i, &m_GroupStart, &temp, &m_GroupOffset);
	m_GroupLen=(WORD)temp;

	FillGroupOffsetCombo();
	UpdateGroupExplanation();
}

void CFontsDialog::WriteUnicodeGroup()
{
	if (m_bLoadingFont || m_bReadingBoxes)
		return;
	EnsureDetails();	// this rebuilds the character list, so a background fill must be completed first
	UpdateData(TRUE);

	int i = m_GroupIndexCombo.GetCurSel();

	if (i < 0)
		return;

	m_Font.SetUnicodeGroup(i, m_GroupStart, m_GroupLen, m_GroupOffset);

	//if sorting then change the index:
	DWORD glen,temp=m_GroupLen;
	WORD gstart,goffset;
	int found = m_GroupIndexCombo.GetCurSel();
	for (i=0;i<m_GroupIndexCombo.GetCount();i++)
	{
		m_Font.GetUnicodeGroup(i, &gstart, &glen, &goffset);
		if (glen==temp && gstart==m_GroupStart && goffset==m_GroupOffset)
		{
			found = i;
			break;
		}
	}
	FillGroupCombo(found);		// the names follow the start of each group, which may just have changed
	FillGroupOffsetCombo();
	//

	UpdateGroupExplanation(FALSE);

	UpdateCharCombo();
	UpdateZoomView();
}

void CFontsDialog::OnCbnSelchangeGroupidxCombo()
{
	ReadUnicodeGroup();

	// update char mapping

	m_CharMapCombo1.SetCurSel(m_GroupOffset - 1);
	UpdateCharMap();
}

void CFontsDialog::UpdateCharMap()
{
	// the group follows the entry: the box of the Unicode groups shows the group that the entry of the mapping belongs to
	{
		int entry = m_CharMapCombo1.GetCurSel() + 1;
		int cur = m_GroupIndexCombo.GetCurSel();
		for (DWORD g = 0; entry > 0 && g < m_Font.GetNumUnicodeGroups(); g++)
		{
			WORD start, offset;
			DWORD len;
			m_Font.GetUnicodeGroup((WORD)g, &start, &len, &offset);
			if (entry >= (int)offset && entry < (int)offset + (int)len)
			{
				if ((int)g != cur)
				{
					m_GroupIndexCombo.SetCurSel((int)g);
					ReadUnicodeGroup();
				}
				break;
			}
		}
	}
	m_CharMapCombo2.SetCurSel(m_Font.GetCharMapping(m_CharMapCombo1.GetCurSel()));
	// the entry in the list of the group (none when the entry is outside of the group)
	int pos = m_CharMapCombo1.GetCurSel() + 1 - (int)m_GroupOffset;
	m_GroupOffsetCombo.SetCurSel(pos >= 0 && pos < (int)m_GroupLen ? pos : -1);
	ReadCharMetrics();
	UpdateZoomView();
	SyncPanelSelection();
}

// the editor and the preview follow the entry and the glyph that the boxes show
void CFontsDialog::SyncPanelSelection()
{
	if (m_pPanel && m_pPanel->m_bLoaded && !m_bLoadingFont)
		m_pPanel->SelectGlyph(m_CharMapCombo1.GetCurSel(), m_CharMapCombo2.GetCurSel());
}

void CFontsDialog::OnCbnSelchangeCharmapidx2Combo()
{
	// write changes

	m_Font.SetCharMapping(m_CharMapCombo1.GetCurSel(), m_CharMapCombo2.GetCurSel());

	//
	ReadCharMetrics();
	UpdateZoomView();
	SyncPanelSelection();
}

void CFontsDialog::OnCbnSelchangeCharmapidxCombo()
{
	UpdateCharMap();
}

void CFontsDialog::WriteCharMetrics()
{
	if (m_bDetailsPending || m_bLoadingFont || m_bReadingBoxes)
		return;			// the lists are being rebuilt for another font; nothing valid to write to yet
	UpdateData(TRUE);

	m_Font.SetCharMetrics(m_CharMapCombo2.GetCurSel(), (WORD)m_Offset1, (WORD)m_Offset2, (SHORT)m_Width, (SHORT)m_Ident);

	UpdateZoomView();
}

void CFontsDialog::ReadCharMetrics()
{
	int i = m_CharMapCombo2.GetCurSel();

	DWORD offset1 = 0, offset2 = 0;
	SHORT width = 0, ident = 0;

	m_Font.GetCharMetrics(i, &offset1, &offset2, &width, &ident);

	m_Offset1 = (WORD)offset1;
	m_Offset2 = (WORD)offset2;
	m_Width = width;
	m_Ident = ident;

	BOOL bEnable = !m_Font.IsFixedWidth() && m_bEditorOpen;

	GetDlgItem(IDC_OFFSET1_EDIT)->EnableWindow(bEnable);
	GetDlgItem(IDC_OFFSET2_EDIT)->EnableWindow(bEnable);
	GetDlgItem(IDC_WIDTH_EDIT)->EnableWindow(bEnable);
	GetDlgItem(IDC_IDENT_EDIT)->EnableWindow(bEnable);
	//GetDlgItem(IDC_UPDATE_BUTTON)->EnableWindow(bEnable);

	// Every box that gets its text sends a change message; written back, those would put the values that the other boxes still
	// have (the ones of the glyph before) into the font and into the values that are being shown.
	BOOL bWas = m_bReadingBoxes;
	m_bReadingBoxes = TRUE;
	UpdateData(FALSE);
	m_bReadingBoxes = bWas;
}

void CFontsDialog::OnEnChangeOffset1Edit()
{
	WriteCharMetrics();
}

void CFontsDialog::OnEnChangeOffset2Edit()
{
	WriteCharMetrics();
}

void CFontsDialog::OnEnChangeWidthEdit()
{
	WriteCharMetrics();
}

void CFontsDialog::OnEnChangeIdentEdit()
{
	WriteCharMetrics();
}

void CFontsDialog::OnEnChangeGroupstartEdit()
{
	WriteUnicodeGroup();
}

void CFontsDialog::OnEnChangeGrouplenEdit()
{
	WriteUnicodeGroup();
}

// The table index of the group is the entry of its first character; every character of the group follows with the next entry:
// entry = Table index + (character - Start). The list holds all the entries of the group, with the character of each.
void CFontsDialog::FillGroupOffsetCombo()
{
	DWORD n = m_GroupLen;
	CComboFill::Begin(m_GroupOffsetCombo, n, 16);
	WCHAR w[48];
	for (DWORD i = 0; i < n; i++)
	{
		DWORD c = (DWORD)m_GroupStart + i;
		if (c >= 0x20 && c != 0x7F && c < 0x10000)
			swprintf(w, 48, L"%u  U+%04X (%c)", (unsigned)m_GroupOffset + i, c, (WCHAR)c);
		else
			swprintf(w, 48, L"%u  U+%04X", (unsigned)m_GroupOffset + i, c);
		CComboFill::AddText(m_GroupOffsetCombo, w);
	}
	CComboFill::End(m_GroupOffsetCombo);
	m_GroupOffsetCombo.SetCurSel(n ? 0 : -1);
}

// picking an entry of the group selects it in the character mapping
void CFontsDialog::OnCbnSelchangeGroupoffsetDrop()
{
	int pos = m_GroupOffsetCombo.GetCurSel();
	if (pos < 0)
		return;
	int entry = (int)m_GroupOffset - 1 + pos;
	if (entry < 0 || entry >= m_CharMapCombo1.GetCount())
		return;
	m_CharMapCombo1.SetCurSel(entry);
	UpdateCharMap();
}

// The boxes of the metrics are in the colour of what they are in the editor and in the preview: Offset1 and Offset2 red (the two
// rulers where the glyph is cut out), Indent blue, Width green.
HBRUSH CFontsDialog::OnCtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CDarkDialog::OnCtlColor(pDC, pWnd, nCtlColor);
	UINT id = pWnd->GetDlgCtrlID();
	int k = (id == IDC_OFFSET1_EDIT || id == IDC_OFFSET2_EDIT) ? 0 : (id == IDC_IDENT_EDIT) ? 1 : (id == IDC_WIDTH_EDIT) ? 2 : -1;
	if (k < 0 || (nCtlColor != CTLCOLOR_EDIT && nCtlColor != CTLCOLOR_STATIC))
		return hbr;
	BOOL dark = DarkMode::Enabled();
	COLORREF back = k == 0 ? (dark ? RGB(110, 40, 40) : RGB(255, 214, 214))
	              : k == 1 ? (dark ? RGB(40, 60, 120) : RGB(214, 226, 255))
	                       : (dark ? RGB(35, 90, 45) : RGB(214, 244, 214));
	static CBrush *brushes[3] = { NULL, NULL, NULL };
	static COLORREF colors[3] = { 0, 0, 0 };
	if (!brushes[k] || colors[k] != back)
	{
		delete brushes[k];
		brushes[k] = new CBrush(back);
		colors[k] = back;
	}
	pDC->SetBkColor(back);
	pDC->SetTextColor(dark ? RGB(240, 240, 240) : RGB(0, 0, 0));
	return (HBRUSH)brushes[k]->GetSafeHandle();
}

// ---- the working copy --------------------------------------------------------------------------------------------------

BOOL CFontsDialog::HasPending()
{
	if (!m_pFirmware || m_WorkLen == 0)
		return FALSE;
	LPBYTE lp = m_pFirmware->GetFont(m_FontIndex);
	return lp && memcmp(lp, &m_Work[0], m_WorkLen) != 0;
}

void CFontsDialog::UpdateWriteButton()
{
	if (!m_hWnd || !GetDlgItem(IDC_FONT_WRITE))
		return;
	BOOL on = HasPending() && m_pFirmware->CanWriteFont(m_FontIndex);
	GetDlgItem(IDC_FONT_WRITE)->EnableWindow(on);
}

// puts the font of the working copy in the firmware
BOOL CFontsDialog::WriteFont()
{
	if (!m_pFirmware || m_WorkLen == 0)
		return TRUE;
	LPBYTE dst = m_pFirmware->GetFont(m_FontIndex);
	if (!dst || !m_pFirmware->CanWriteFont(m_FontIndex))
	{
		MessageBox(_T("This font cannot be written in this firmware."));
		return FALSE;
	}
	memcpy(dst, &m_Work[0], m_WorkLen);
	RefreshFontNames();
	m_FontIndexCombo.Invalidate();
	UpdateWriteButton();
	return TRUE;
}

void CFontsDialog::OnWriteFont()
{
	WriteFont();
}

LRESULT CFontsDialog::OnPanelEdited(WPARAM wParam, LPARAM lParam)
{
	UpdateWriteButton();
	return 0;
}

// What to do with the changes that are not written when the font (or the page, or the firmware) is left.
BOOL CFontsDialog::ConfirmPending(CWnd *pOwner)
{
	if (!HasPending())
		return TRUE;
	CString s;
	s.Format(_T("Font %u has changes that are not written yet.\n\nYes: write them to the font\nNo: throw them away\nCancel: go back to the font"), (unsigned)m_FontIndex);
	int r = ::MessageBox(pOwner && pOwner->m_hWnd ? pOwner->m_hWnd : m_hWnd, s, _T("Changes of the font"), MB_YESNOCANCEL | MB_ICONQUESTION);
	if (r == IDCANCEL)
		return FALSE;
	if (r == IDYES)
		return WriteFont();
	UpdateFont(FALSE, FALSE);                 // No: the font as the firmware has it
	return TRUE;
}

// Ctrl+Z / Ctrl+Y (and Ctrl+Shift+Z) work anywhere on the Fonts page while the editor is open, not only after a click in the
// panel: a click on one of the buttons of the page moves the focus out of the panel. In an edit box the keys stay with the text.
BOOL CFontsDialog::PreTranslateMessage(MSG *pMsg)
{
	if (HandleUndoKey(pMsg))
		return TRUE;
	return CDarkDialog::PreTranslateMessage(pMsg);
}

// Only while the Fonts page is shown and its editor is open: with the editor closed, or another tab shown, the keys are not taken.
// (The editor dialog asks too, so they also work when the focus is on the tabs.)
BOOL CFontsDialog::HandleUndoKey(MSG *pMsg)
{
	if (pMsg->message == WM_KEYDOWN && m_bEditorOpen && m_pPanel && m_pPanel->GetSafeHwnd() && IsWindowVisible() &&
	    (GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000) && (pMsg->wParam == 'Z' || pMsg->wParam == 'Y'))
	{
		HWND hFocus = ::GetFocus();
		BOOL inEdit = FALSE;
		if (hFocus)
		{
			TCHAR cls[16];
			::GetClassName(hFocus, cls, 16);
			inEdit = _tcsicmp(cls, _T("Edit")) == 0;
		}
		if (!inEdit)
		{
			BOOL shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
			if (pMsg->wParam == 'Z' && !shift)
				m_pPanel->Undo();
			else
				m_pPanel->Redo();
			return TRUE;
		}
	}
	return FALSE;
}

void CFontsDialog::OnPanelUndo()
{
	if (m_pPanel)
		m_pPanel->Undo();
}

void CFontsDialog::OnPanelRedo()
{
	if (m_pPanel)
		m_pPanel->Redo();
}

// The page grows with the window: the panel (text box, preview, bitmap editor) takes all the room that is added, the buttons of the
// right bar and the Glyph Metrics box keep to the right edge, the two boxes at the bottom keep to the bottom edge.
void CFontsDialog::CaptureBaseLayout()
{
	GetClientRect(&m_BaseClient);
	m_BaseCtls.clear();
	for (CWnd *w = GetWindow(GW_CHILD); w; w = w->GetWindow(GW_HWNDNEXT))
	{
		BaseCtl b;
		b.hwnd = w->m_hWnd;
		w->GetWindowRect(&b.rc);
		ScreenToClient(&b.rc);
		m_BaseCtls.push_back(b);
	}
	m_bBaseSet = TRUE;
}

void CFontsDialog::OnSize(UINT nType, int cx, int cy)
{
	CDarkDialog::OnSize(nType, cx, cy);
	if (!m_bBaseSet || nType == SIZE_MINIMIZED || cx <= 0 || cy <= 0 || !m_pPanel || !m_pPanel->GetSafeHwnd())
		return;
	CRect marks(151, 210, 366, 0);          // left of the Glyph Mapping box, top of the three boxes at the bottom, left of the right bar
	MapDialogRect(&marks);
	int xGlyph = marks.left, yBottom = marks.top, xBar = marks.right;
	HWND hHolder = ::GetDlgItem(m_hWnd, IDC_FONTPANEL_HOLDER);
	CRect holder(0, 0, 0, 0);
	for (size_t i = 0; i < m_BaseCtls.size(); i++)
		if (m_BaseCtls[i].hwnd == hHolder)
			holder = m_BaseCtls[i].rc;
	int dx = cx - m_BaseClient.Width(), dy = cy - m_BaseClient.Height();
	dx = max(dx, 150 - holder.Width());     // the panel never gets smaller than what it needs to work
	dy = max(dy, 150 - holder.Height());
	for (size_t i = 0; i < m_BaseCtls.size(); i++)
	{
		CWnd *w = CWnd::FromHandle(m_BaseCtls[i].hwnd);
		if (!w || !::IsWindow(m_BaseCtls[i].hwnd) || m_BaseCtls[i].hwnd == m_pPanel->m_hWnd)
			continue;
		if (m_BaseCtls[i].hwnd == ::GetDlgItem(m_hWnd, IDC_FONTPREVIEW_UNDO) || m_BaseCtls[i].hwnd == ::GetDlgItem(m_hWnd, IDC_FONTPREVIEW_REDO) ||
		    m_BaseCtls[i].hwnd == ::GetDlgItem(m_hWnd, IDC_FONT_OPENEDITOR))
			continue;            // (the panel puts them in the row of the options of its editor)
		CRect r = m_BaseCtls[i].rc;
		if (m_BaseCtls[i].hwnd == hHolder)
		{
			r.right += dx;
			r.bottom += dy;
			m_pPanel->MoveWindow(&r);
			continue;
		}
		CString t;
		w->GetWindowText(t);
		if (r.top >= yBottom)
		{
			r.OffsetRect(0, dy);
			if (r.left >= xGlyph)
				r.OffsetRect(dx, 0);
			else if (t == _T("1. Unicode Groups"))
				r.right += dx;
		}
		else if (r.left >= xBar || t == _T("Bit depth:") || m_BaseCtls[i].hwnd == ::GetDlgItem(m_hWnd, IDC_BITDEPTH_EDIT))
			r.OffsetRect(dx, 0);        // (the end of the Bit depth box stays in line with the end of the Reset Text button)
		else
			continue;
		w->SetWindowPos(NULL, r.left, r.top, r.Width(), r.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
	}
	// The button that opens and closes the editor is in the row of the buttons of the editor (the bottom row of the panel), at the
	// right edge, whether the editor is open or not.
	{
		CWnd *btn = GetDlgItem(IDC_FONT_OPENEDITOR);
		CRect pr, rh(0, 12, 0, 0);
		MapDialogRect(&rh);
		m_pPanel->GetWindowRect(&pr);
		ScreenToClient(&pr);
		for (size_t i = 0; btn && i < m_BaseCtls.size(); i++)
			if (m_BaseCtls[i].hwnd == btn->m_hWnd)
			{
				CRect r = m_BaseCtls[i].rc;
				btn->SetWindowPos(NULL, r.left + dx, pr.bottom - rh.top - 1, r.Width(), rh.top, SWP_NOZORDER | SWP_NOACTIVATE);
			}
	}
	Invalidate();
}

// the characters of the font are in the text box of the panel
void CFontsDialog::UpdateCharCombo()
{
	if (m_pPanel && m_pPanel->m_bLoaded)
		m_pPanel->RebuildText();
}

// bRefreshBoxes: also write the values of the group into the Start and Length boxes. It is not done while the user is typing
// in one of them: putting the text back in the box moves the caret to the front.
void CFontsDialog::UpdateGroupExplanation(BOOL bRefreshBoxes)
{
	if (bRefreshBoxes)
	{
		BOOL bWas = m_bReadingBoxes;
		m_bReadingBoxes = TRUE;       // see ReadCharMetrics
		UpdateData(FALSE);
		m_bReadingBoxes = bWas;
	}
}

void CFontsDialog::OnBnClickedLoadMetrics()
{
	CFileDialog dlg(TRUE, TEXT("ifm"), 0, OFN_HIDEREADONLY, TEXT("Fonts Metrics (*.ifm)|*.ifm||"), this);
	MO_LOAD_RESOURCES_PATH(dlg)

	if (dlg.DoModal() != IDOK)
		return;
	MO_SAVE_RESOURCES_PATH(dlg)
	CFile file;
	if (!file.Open(dlg.GetPathName(), CFile::modeRead))
	{
		MessageBox(TEXT("Unable to load file!"));
		return;
	}

	WORD offset1, offset2;
	SHORT width, ident;
	WORD c;

	//Old Version
	if (theApp.m_OldFontMetricsSystem)
	{
		while (TRUE)
		{
			if (file.Read(&c, 2) < 2)
				break;

			if (file.Read(&offset1, 2) < 2)
				break;
			if (file.Read(&offset2, 2) < 2)
				break;
			if (file.Read(&width, 2) < 2)
				break;
			if (file.Read(&ident, 2) < 2)
				break;
			m_Font.SetCharMetrics(m_Font.GetCharMapping(m_Font.GetUnicodeCharOffset(c)), offset1, offset2, width, ident);
		}
	}
	else
	{
		/*
		WORD index1, index2, start, len;

		//Make file size validity check
		if ((file.GetLength() - 8*m_Font.GetNumUnicodeGroups())%14 != 0)
			goto NotValid;

		while (TRUE)
		{
			if (file.Read(&index1, 2) < 2)
				break;
			if (file.Read(&start, 2) < 2)
				break;
			if (file.Read(&len, 2) < 2)
				break;
			if (file.Read(&offset1, 2) < 2)
				break;
			m_Font.SetUnicodeGroup(index1, start, len, offset1);

			if (file.GetPosition() == (8*m_Font.GetNumUnicodeGroups()))
				break;
		}

		while (TRUE)
		{
			if (file.Read(&index1, 2) < 2)
				break;
			if (file.Read(&index2, 2) < 2)
				break;

			if (file.Read(&c, 2) < 2)
				break;

			if (file.Read(&offset1, 2) < 2)
				break;
			if (file.Read(&offset2, 2) < 2)
				break;
			if (file.Read(&width, 2) < 2)
				break;
			if (file.Read(&ident, 2) < 2)
				break;
			m_Font.SetCharMapping(index1, index2);
			m_Font.SetCharMetrics(index2, offset1, offset2, width, ident);
		}
		*/
		DWORD size=m_Font.GetMetricDataLen();
		DWORD length=(DWORD)file.GetLength();
		if (length!=size)
			goto NotValid;

		LPBYTE lpBuf;
		lpBuf=new BYTE[size];
		if (file.Read(lpBuf, size) == size)
		{
			m_Font.SetMetricData(lpBuf);
		}
		delete lpBuf;
	}

	file.Close();

	// update
	UpdateFont();

	return;

NotValid:
	MessageBox(TEXT("The file you chose is not a valid metrics file!"));
}

void CFontsDialog::OnBnClickedSaveMetrics()
{
	CString filename;
	CString idx;
	idx.Format(TEXT("%d"), m_FontIndexCombo.GetCurSel());
	filename.Format(TEXT("%s-%s.ifm"), m_Font.GetFontName(), idx);

	CFileDialog dlg(FALSE, TEXT("ifm"), filename, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, TEXT("Fonts Metrics (*.ifm)|*.ifm||"), this);
	MO_LOAD_RESOURCES_PATH(dlg)

	if (dlg.DoModal() != IDOK)
		return;
	MO_SAVE_RESOURCES_PATH(dlg)
	CFile file;
	if (!file.Open(dlg.GetPathName(), CFile::modeCreate | CFile::modeWrite))
	{
		MessageBox(TEXT("Unable to save file!"));
		return;
	}

	DWORD offset1, offset2;
	//WORD offset3;
	SHORT width, ident;
	WORD c, group;

	//Old version
	if (theApp.m_OldFontMetricsSystem)
	{
		for (WORD i = 0; i < m_Font.GetNumUnicodeChars(); i++)
		{
			c = m_Font.GetUnicodeChar(i, &group);

			m_Font.GetCharMetrics(m_Font.GetCharMapping(m_Font.GetUnicodeCharOffset(c)), &offset1, &offset2, &width, &ident);

			file.Write(&c, 2);
			file.Write(&offset1, 2);
			file.Write(&offset2, 2);
			file.Write(&width, 2);
			file.Write(&ident, 2);
		}
	}
	else
	{
		/*
		WORD index1, index2;

		WORD start;
		DWORD len;

		for (WORD i = 0; i < m_Font.GetNumUnicodeGroups(); i++)
		{
			m_Font.GetUnicodeGroup(i, &start, &len, &offset3);

			file.Write(&i, 2);
			file.Write(&start, 2);
			file.Write(&len, 2);
			file.Write(&offset3, 2);
		}

		for (WORD i = 0; i < m_Font.GetNumUnicodeChars(); i++)
		{
			c = m_Font.GetUnicodeChar(i, &group);

			m_Font.GetCharMetrics(m_Font.GetCharMapping(m_Font.GetUnicodeCharOffset(c)), &offset1, &offset2, &width, &ident);

			index1 = m_Font.GetUnicodeCharOffset(c);
			index2 = m_Font.GetCharMapping(index1);

			file.Write(&index1, 2);
			file.Write(&index2, 2);
			file.Write(&c, 2);
			file.Write(&offset1, 2);
			file.Write(&offset2, 2);
			file.Write(&width, 2);
			file.Write(&ident, 2);
		}*/
		LPBYTE temp=m_Font.GetMetricData();
		DWORD size;
		memcpy((LPBYTE)&size, temp, 4);
		file.Write(&temp[4], size);
	}

	file.Close();
}

void CFontsDialog::OnBnClickedMakeFont()
{
	CFontDialog dlg;

	if (dlg.DoModal() != IDOK)
		return;

	LOGFONT lf;
	memcpy(&lf, dlg.m_cf.lpLogFont, sizeof(LOGFONT));

	if (m_Font.GetFontBitDepth() > 1)
		lf.lfQuality = ANTIALIASED_QUALITY;

	CFont font;
	VERIFY(font.CreateFontIndirect(&lf));

	//

	CDC dc;
	CDC *pDC = GetWindowDC();
	dc.CreateCompatibleDC(pDC);
	
	CBitmap bmp;
	bmp.CreateCompatibleBitmap(pDC, 60, 60);

	CBitmap *pBitmap = dc.SelectObject(&bmp);
	CFont *pOldFont = dc.SelectObject(&font);

	dc.SetTextColor(RGB(0, 0, 0));
	dc.SetBkColor(RGB(255 ,255, 255));

	//

	WORD xoffset = 0;

	int height;
	wchar_t c[2];
	WORD group;

	c[1] = 0;

	WORD offset;

	CSize size = m_Font.GetFontBitmapSize();

	for (WORD i = 0; i < (WORD)m_Font.GetNumUnicodeChars(); i++)
	{
		c[0] = m_Font.GetUnicodeChar(i, &group);

		offset = i;
		m_Font.SetCharMapping(offset, offset);
		
		CRect rect(0, 0, 60, 60);
		::DrawTextW(dc.m_hDC, c, 1, &rect, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT);
		height = ::DrawTextW(dc.m_hDC, c, 1, &rect, DT_LEFT | DT_TOP | DT_NOPREFIX | DT_SINGLELINE);

		ABCFLOAT abc;
		GetCharABCWidthsFloat(dc.m_hDC, c[0], c[0], &abc);

		SHORT abcA = (SHORT)abc.abcfA;
		SHORT abcB = (SHORT)abc.abcfB;
		SHORT abcC = (SHORT)abc.abcfC;

		// analyze

		int x, y;
		int start = 0, end = 0;
		BOOL bLineEmpty;

		for (x = 0; x < rect.right; x++)
		{
			bLineEmpty = TRUE;
			for (y = 0; y < height; y++)
			{
				if (dc.GetPixel(x, y) != RGB(255, 255, 255))
				{
					bLineEmpty = FALSE;
					break;
				}
			}
			if (!bLineEmpty)
			{
				start = x;
				break;
			}
		}

		for (x = rect.right - 1; x >= 0; x--)
		{
			bLineEmpty = TRUE;
			for (y = 0; y < height; y++)
			{
				if (dc.GetPixel(x, y) != RGB(255, 255, 255))
				{
					bLineEmpty = FALSE;
					break;
				}
			}
			if (!bLineEmpty)
			{
				end = x;
				break;
			}
		}

		end++;
		// copy
		for (x = start; x < end; x++)
		{
			for (y = 0; y < height; y++)
			{
				m_Font.SetFontPixel(x + xoffset - start, y, dc.GetPixel(x, y));
			}

			for (y = height; y < size.cy; y++)
			{
				m_Font.SetFontPixel(x + xoffset - start, y, RGB(255, 255, 255));
			}
		}
		// update metrics

		m_Font.SetCharMetrics(offset, xoffset, xoffset + end - start, abcB + abcC + abcA, abcA);

		xoffset += end - start;

		if (xoffset >= size.cx)
		{
			for (i++;i < (WORD)m_Font.GetNumUnicodeChars(); i++)
			{
				m_Font.SetCharMapping(i, i);
				m_Font.SetCharMetrics(i, 0, 0, 1, 0);
			}
			break;
		}
	}

	//

	dc.SelectObject(pOldFont);
	dc.SelectObject(pBitmap);
	
	//

	ReleaseDC(pDC);

	UpdateFont();
}
void CFontsDialog::OnBnClickedButton1()
{
	EnsureDetails();
	// TODO: Add your control notification handler code here
	/*m_Font.ReMakeBitmap();
	UpdateCharCombo();
	OnCbnSelchangeCharCombo();
	//OnBnClickedSaveBitmap();*/


	if (theApp.m_OldFontMetricsSystem)
	{
		LPBYTE fntb=m_Font.MakeFont();
		UpdateFont(FALSE, TRUE);
		return;
	}

	LPITEMIDLIST il;
	TCHAR folderPath[MAX_PATH];

	BROWSEINFO br;
    br.hwndOwner = AfxGetMainWnd()->GetSafeHwnd();
    br.pidlRoot = NULL;
    br.pszDisplayName = folderPath;
    br.lpszTitle = TEXT("Select folder for files");
    br.ulFlags = BIF_RETURNONLYFSDIRS;
    br.lpfn = NULL;
    br.lParam = 0;
    br.iImage = 0;

	il = SHBrowseForFolder(&br);
	if (il == NULL)
		return;

	SHGetPathFromIDList(il, folderPath);

	CString filename = folderPath;
	filename += "\\????.bmp";

	BYTE width,height;
	WORD gidx;
	WORD offset_start=m_Offset1;
	SHORT ident=0;
	WIN32_FIND_DATA findData;
	BYTE aw=0;
	HANDLE hFind = FindFirstFile(filename, &findData);
	CSize bsize=m_Font.GetFontBitmapSize();
	gidx=m_Font.GetCharMapping(m_CharMapCombo1.GetCurSel());
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			filename = folderPath;
			filename += "\\";
			filename += findData.cFileName;
			WORD index;
			LPBYTE ib=new BYTE[8];
			index=0;
			_stscanf(findData.cFileName, TEXT("%04X"), ib);
			index=*((LPWORD)ib);
			//index<<=8;
			//index = _wtoi(findData.cFileName + 4);

			if (index>=m_GroupStart && index<=m_GroupStart+m_GroupLen)
			{
				CImage img;
				if (FAILED(img.Load(filename)))
				{
					CString msg;
					msg.Format(TEXT("Unable to load image %d!"), index);
					MessageBox(msg);
					return;
				}

				width = img.GetWidth();
				height = img.GetHeight();

				//calculate bearingy
				int x, y;

				int hstart = -1, hend = -1;
				BOOL bLineEmpty;

				/*for (y = 0; y < height; y++)
				{
					bLineEmpty = TRUE;
					for (x = 0; x < width; x++)
					{
						if (img.GetPixel(x, y) != RGB(255, 255, 255))
						{
							bLineEmpty = FALSE;
							break;
						}
					}
					if (!bLineEmpty)
					{
						hstart = y;
						break;
					}
				}*/
				hstart=0;

				for (y = height - 1; y >= 0 && hstart != -1; y--)
				{
					bLineEmpty = TRUE;
					for (x = 0; x < width ; x++)
					{
						if (img.GetPixel(x, y) != RGB(255, 255, 255))
						{
							bLineEmpty = FALSE;
							break;
						}
					}
					if (!bLineEmpty)
					{
						hend = y;
						break;
					}
				}
				if (hstart!=-1)
					hend++;
				//
				height=hend-hstart;
				
				//bearingy=m_BaseLine-hstart;
				//bearingy=height;
				//if (hend>m_BaseLine)
				//	bearingy-=hend-m_BaseLine;
				//advance=m_ALeft + width + m_ARight;
				m_Font.SetCharMetrics(gidx, offset_start, offset_start+width, width+aw, ident);
				

				//LONG j, k;
				COLORREF color;

				for (x = 0; x < img.GetWidth(); x++)
				{
					for (y = 0; y < height; y++)
					{
						color = img.GetPixel(x, y + hstart);
						m_Font.SetFontPixel(offset_start+x, y, color);
					}
					for (y=height;y<bsize.cy;y++)
						m_Font.SetFontPixel(offset_start+x, y, RGB(255, 255, 255));
				}
				offset_start+=width;
				gidx++;
			}
		} while (FindNextFile(hFind, &findData) != 0);

		FindClose(hFind);

		UpdateData(FALSE);

		ReadCharMetrics();
		UpdateZoomView();
	}
}

void CFontsDialog::OnEnChangeFontnameEdit()
{
	// TODO:  If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDarkDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.

	// TODO:  Add your control notification handler code here
	if (theApp.m_LoadingFirmware==TRUE || theApp.m_SavingFirmware==TRUE)
		return;

	UpdateData(TRUE);

	m_Font.SetFontName(m_FontName);
}

// Style and size are only editable when Preferences > Advanced allows it
void CFontsDialog::ApplyFontEditPolicy()
{
	BOOL ro = theApp.m_AllowFontEdit ? FALSE : TRUE;
	reinterpret_cast<CEdit*>(GetDlgItem(IDC_FONTSTYLE_EDIT))->SetReadOnly(ro);
	reinterpret_cast<CEdit*>(GetDlgItem(IDC_FONTSIZE_EDIT))->SetReadOnly(ro);
}

void CFontsDialog::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDarkDialog::OnShowWindow(bShow, nStatus);
	if (bShow)
		ApplyFontEditPolicy();
}

void CFontsDialog::OnEnChangeFontstyleEdit()
{
	// TODO:  If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDarkDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.

	// TODO:  Add your control notification handler code here
	if (theApp.m_LoadingFirmware==TRUE || theApp.m_SavingFirmware==TRUE)
		return;

	if (!theApp.m_AllowFontEdit)
		return;

	UpdateData(TRUE);

	int id = _ttoi(m_FontStyle);
	if (id < 0) id = 0;
	if (id > 255) id = 255;
	m_Font.SetFontStyleID((BYTE)id);
	SetDlgItemText(IDC_FONTSTYLENAME_STATIC, m_Font.GetFontStyle());
	UpdateWriteButton();
}

void CFontsDialog::OnEnChangeFontsizeEdit()
{
	// TODO:  If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDarkDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.

	// TODO:  Add your control notification handler code here
	if (theApp.m_LoadingFirmware==TRUE || theApp.m_SavingFirmware==TRUE)
		return;

	if (!theApp.m_AllowFontEdit)
		return;

	UpdateData(TRUE);

	m_Font.SetFontSize(m_FontSize);
	UpdateWriteButton();
}

void CFontsDialog::OnBnClickedMultisaveGlyph()
{
	EnsureDetails();
	LPITEMIDLIST il;
	TCHAR folderPath[MAX_PATH];

	BROWSEINFO br;
    br.hwndOwner = AfxGetMainWnd()->GetSafeHwnd();
    br.pidlRoot = NULL;
    br.pszDisplayName = folderPath;
    br.lpszTitle = TEXT("Select folder for files");
    br.ulFlags = BIF_RETURNONLYFSDIRS;
    br.lpfn = NULL;
    br.lParam = 0;
    br.iImage = 0;

	il = SHBrowseForFolder(&br);
	if (il == NULL)
		return;

	SHGetPathFromIDList(il, folderPath);

	CString filename;

	CSize size = m_Font.GetFontBitmapSize();

	CImage img;

	WORD hexcode;
	WORD p=0;

	for (hexcode=m_GroupStart;hexcode<m_GroupStart+m_GroupLen;hexcode++,p++)
	{

		m_CharMapCombo1.SetCurSel(m_GroupOffset - 1 + p);
		UpdateCharMap();

		bool reffound=false;
		int j=0;
		for (j=0;j<redirectionTableCount;j++)
		{
			if (redirectionTable[j][0]==hexcode)
			{
				reffound=true;
				break;
			}
		}
		if (reffound==false)
			continue;
		//filename.Format(_T("%s\\%0004X.bmp"), folderPath, hexcode); 
		BYTE w[2];
		w[0]=(BYTE)redirectionTable[j][1];
		w[1]=0x06;
		filename.Format(_T("%s\\%0004X.bmp"), folderPath, *((LPWORD)w));
		
		size.cx=m_Offset2-m_Offset1;
		size.cy--;

		int x, y;

		int hstart = -1, hend = -1;
		/*
		BOOL bLineEmpty;

		for (y = 0; y < size.cy; y++)
		{
			bLineEmpty = TRUE;
			for (x = 0; x < size.cx; x++)
			{
				if (m_Font.GetFontPixel(CPoint(x+m_Offset1, y)) != RGB(255, 255, 255))
				{
					bLineEmpty = FALSE;
					break;
				}
			}
			if (!bLineEmpty)
			{
				hstart = y;
				break;
			}
		}

		for (y = size.cy - 1; y >= 0 && hstart != -1; y--)
		{
			bLineEmpty = TRUE;
			for (x = 0; x < size.cx ; x++)
			{
				if (m_Font.GetFontPixel(CPoint(x+m_Offset1, y)) != RGB(255, 255, 255))
				{
					bLineEmpty = FALSE;
					break;
				}
			}
			if (!bLineEmpty)
			{
				hend = y;
				break;
			}
		}
		if (hstart!=-1)
			hend++;*/

		//nano
		//hstart=3;
		//hend=19;
		//photo
		//hstart=6;
		//hend=24;
		hstart=5;
		hend=32;

		int exwidth;
		if (m_Width<size.cx)
			exwidth=0;
		else
			exwidth=m_Width-size.cx;

		if (!img.Create(size.cx + exwidth, hend - hstart, 24))
		{
			MessageBox(TEXT("Unable to create image!"));
			return;
		}

		/*for (x = 0; x < size.cx; x++)
		{
			for (y = 0; y < hend - hstart; y++)
			{
				img.SetPixel(x, y, m_Font.GetFontPixel(CPoint(x+m_Offset1, y + hstart)));
			}
		}*/
		
		for (y = 0; y < hend - hstart; y++)
		{
			x = 0;
			for (x = 0; x < exwidth; x++)
			{
				img.SetPixel(x, y, RGB(255,255,255));
			}
			for (; x < size.cx+exwidth; x++)
			{
				img.SetPixel(x, y, m_Font.GetFontPixel(CPoint(x-exwidth+m_Offset1, y + hstart)));
			}
		}

		if (FAILED(img.Save(filename)))
		{
			MessageBox(TEXT("Unable to save image!"));
			return;
		}
		img.Destroy();
	}
	
	/*
	LPITEMIDLIST il;
	TCHAR folderPath[MAX_PATH];

	BROWSEINFO br;
    br.hwndOwner = AfxGetMainWnd()->GetSafeHwnd();
    br.pidlRoot = NULL;
    br.pszDisplayName = folderPath;
    br.lpszTitle = TEXT("Select folder for files");
    br.ulFlags = BIF_RETURNONLYFSDIRS;
    br.lpfn = NULL;
    br.lParam = 0;
    br.iImage = 0;

	il = SHBrowseForFolder(&br);
	if (il == NULL)
		return;

	SHGetPathFromIDList(il, folderPath);

	CString filename;

	CSize size = m_Font.GetFontBitmapSize();

	CImage img;

	WORD hexcode;
	WORD p=0;
	size.cy--;

	for (hexcode=m_GroupStart;hexcode<m_GroupStart+m_GroupLen;hexcode++,p++)
	{

		m_CharMapCombo1.SetCurSel(m_GroupOffset - 1 + p);
		UpdateCharMap();

		bool reffound=false;
		int j=0;
		for (j=0;j<redirectionTableCount;j++)
		{
			if (redirectionTable[j][0]==hexcode)
			{
				reffound=true;
				break;
			}
		}
		if (reffound==false)
			continue;
		//filename.Format(_T("%s\\%0004X.bmp"), folderPath, hexcode); 
		LPBYTE w=new BYTE[2];
		w[0]=(BYTE)redirectionTable[j][1];
		w[1]=0x06;
		filename.Format(_T("%s\\%0004X.bmp"), folderPath, *((LPWORD)w));
		
		size.cx=m_Offset2-m_Offset1;

		int x, y;

		int hstart = -1, hend = -1;
		
		BOOL bLineEmpty;

		for (y = 0; y < size.cy; y++)
		{
			bLineEmpty = TRUE;
			for (x = 0; x < size.cx; x++)
			{
				if (m_Font.GetFontPixel(CPoint(x+m_Offset1, y)) != RGB(255, 255, 255))
				{
					bLineEmpty = FALSE;
					break;
				}
			}
			if (!bLineEmpty)
			{
				hstart = y;
				break;
			}
		}

		for (y = size.cy - 1; y >= 0 && hstart != -1; y--)
		{
			bLineEmpty = TRUE;
			for (x = 0; x < size.cx ; x++)
			{
				if (m_Font.GetFontPixel(CPoint(x+m_Offset1, y)) != RGB(255, 255, 255))
				{
					bLineEmpty = FALSE;
					break;
				}
			}
			if (!bLineEmpty)
			{
				hend = y;
				break;
			}
		}
		if (hstart!=-1)
			hend++;

		if (!img.Create(size.cx, hend - hstart, 24))
		{
			MessageBox(TEXT("Unable to create image!"));
			return;
		}

		for (x = 0; x < size.cx; x++)
		{
			for (y = 0; y < hend - hstart; y++)
			{
				img.SetPixel(x, y, m_Font.GetFontPixel(CPoint(x+m_Offset1, y + hstart)));
			}
		}

		if (FAILED(img.Save(filename)))
		{
			MessageBox(TEXT("Unable to save image!"));
			return;
		}
		img.Destroy();
	}
	*/
}

void CFontsDialog::OnBnClickedLoadfontButton()
{
	CFileDialog dlg(TRUE, 0, 0, OFN_HIDEREADONLY, TEXT("iPod Font Files (*.fnt)|*.fnt||"), this);
	MO_LOAD_RESOURCES_PATH(dlg)

	if (dlg.DoModal() != IDOK)
		return;
	MO_SAVE_RESOURCES_PATH(dlg)
	LoadFullFont(dlg.GetPathName(), m_Font.GetFontBlockLen());

	UpdateFont();
}

void CFontsDialog::LoadFullFont(CString path, DWORD size, DWORD fidx)
{
	DWORD fontindex;
	if (fidx==-1)
		fontindex=m_FontIndex;
	else
		fontindex=fidx;

	CFile file;
	if (!file.Open(path, CFile::modeRead))
	{
		MessageBox(_T("Unable to load file!"));
		return;
	}

	if ((ULONGLONG)size==file.GetLength())
	{
		file.Read(m_pFirmware->GetFont(fontindex), (UINT)size);
	}
	else
	{
		MessageBox(_T("File size doesn't match current font!"));
		return;
	}
	UpdateFont();
}

void CFontsDialog::LoadFullFontsDir(CString folderPath)
{
	CString filename;
	filename = folderPath;
	filename += "\\*-*-*.fnt";

	WIN32_FIND_DATA findData;
	CIpodFont font;
	CString fname;
	CString fontname,fontstyle,fontsize;
	LPBYTE lpEnd=m_pFirmware->GetFirmwareBuffer()+m_pFirmware->GetFirmwareSize();
	HANDLE hFind = FindFirstFile(filename, &findData);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			filename = folderPath;
			filename += "\\";
			filename += findData.cFileName;
			fname.Format(findData.cFileName);
			int fpos=fname.Find('-');
			fontname=fname.Left(fpos);
			int ftemp=fpos+1;
			fpos=fname.Find('-', ftemp);
			fontstyle=fname.Mid(ftemp, fpos);
			ftemp=fpos+1;
			fpos=fname.Find('-', ftemp);
			fontsize=fname.Mid(ftemp, fpos);
			for (DWORD z=0;z<m_pFirmware->GetNumFonts();z++)
			{
				font.Read(m_pFirmware->GetFont(z), lpEnd);
				if (!fontname.Compare(font.GetFontName()) && !fontstyle.Compare(font.GetFontStyle()) && _ttoi(fontsize)==font.GetFontSize())
				{
					LoadFullFont(filename, font.GetFontBlockLen(), z);
					break;
				}
			}
		} while (FindNextFile(hFind, &findData) != 0);

		FindClose(hFind);

		UpdateFont();
	}
}
void CFontsDialog::OnBnClickedSaveIfont()
{
	CString filename;
	filename.Format(TEXT("%s-%s-%d.fnt"), m_Font.GetFontName(), m_Font.GetFontStyle(), m_FontSize);

	CFileDialog dlg(FALSE, TEXT("fnt"), filename, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, TEXT("iPod Font RAW Binary File (*.fnt)|*.fnt||"), this);
	MO_LOAD_RESOURCES_PATH(dlg)

	if (dlg.DoModal() != IDOK)
		return;
	MO_SAVE_RESOURCES_PATH(dlg)
	CFile file;
	if (!file.Open(dlg.GetPathName(), CFile::modeCreate | CFile::modeWrite))
	{
		MessageBox(TEXT("Unable to save file!"));
		return;
	}

	if (m_WorkLen)
		file.Write(&m_Work[0], m_WorkLen);               // with the changes that are not written yet
	else
		file.Write(m_pFirmware->GetFont(m_FontIndex), m_Font.GetFontBlockLen());
	file.Close();

	MessageBox(_T("Saved Successfully!"));
}

// ---------------------------------------------------------------------------------------------------------------------
// Preview of a text in the font that is selected: the characters are put together the way the iPod does it (the pen moves by the
// width of each character, a character starts "ident" to the right of the pen), from the font as it is now, changes included.

static const int kPrevZooms[] = { 1, 2, 3, 4, 6, 8, 10, 12, 16, 20 };
static const int kNumPrevZooms = 10;

BEGIN_MESSAGE_MAP(CFontPreviewDlg, CDarkDialog)
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_SETCURSOR()
	ON_WM_CAPTURECHANGED()
	ON_WM_HSCROLL()
	ON_WM_VSCROLL()
	ON_WM_MOUSEWHEEL()
	ON_BN_CLICKED(IDC_FONTPREVIEW_BZOOMIN, OnBZoomIn)
	ON_BN_CLICKED(IDC_FONTPREVIEW_BZOOMOUT, OnBZoomOut)
	ON_BN_CLICKED(IDC_FONTPREVIEW_PZOOMIN, OnPZoomIn)
	ON_BN_CLICKED(IDC_FONTPREVIEW_RESET, OnResetText)
	ON_BN_CLICKED(IDC_FONTPREVIEW_BGRID, OnEditorOptions)
	ON_BN_CLICKED(IDC_FONTPREVIEW_BRULER, OnEditorOptions)
	ON_BN_CLICKED(IDC_FONTPREVIEW_BMETRICS, OnEditorOptions)
	ON_NOTIFY(UDN_DELTAPOS, IDC_FONTPREVIEW_BRULSPIN, OnRulerSpin)
	ON_BN_CLICKED(IDC_FONTPREVIEW_PZOOMOUT, OnPZoomOut)
	ON_WM_CTLCOLOR()
	ON_EN_CHANGE(IDC_FONTPREVIEW_EDIT, OnEnChangeText)
	ON_BN_CLICKED(IDC_FONTPREVIEW_GRID, OnCbnSelchangeZoom)
	ON_BN_CLICKED(IDC_FONTPREVIEW_RULER, OnCbnSelchangeZoom)
	ON_BN_CLICKED(IDC_FONTPREVIEW_PMETRICS, OnCbnSelchangeZoom)
	ON_WM_SETFOCUS()
	ON_WM_ERASEBKGND()
	ON_EN_SETFOCUS(IDC_FONTPREVIEW_EDIT, OnEditSetFocus)
END_MESSAGE_MAP()

// a dialog hands the focus to its first control (here the text box); after a click on a glyph the panel keeps it,
// so the arrow keys can step through the characters
void CFontPreviewDlg::OnSetFocus(CWnd *pOld)
{
	if (m_bGlyphFocus)
		return;         // the panel had the focus (a glyph was clicked): it gets it back as it was, the text is not selected
	CDarkDialog::OnSetFocus(pOld);
}

// The preview and the bitmap editor are painted (in memory, then in one go) over all of their area, so an erase of the panel leaves
// them out: it only shows as a flash while they are scrolled.
BOOL CFontPreviewDlg::OnEraseBkgnd(CDC *pDC)
{
	static const int kIds[] = { IDC_FONTPREVIEW_AREA, IDC_FONTPREVIEW_BITAREA };
	int saved = pDC->SaveDC();          // (this is the DC that the painting that follows uses: its clipping goes back as it was)
	for (int k = 0; m_bLoaded && k < 2; k++)
	{
		CWnd *w = GetDlgItem(kIds[k]);
		if (w && w->IsWindowVisible())
		{
			CRect r;
			w->GetWindowRect(&r);
			ScreenToClient(&r);
			pDC->ExcludeClipRect(&r);
		}
	}
	BOOL ret = CDarkDialog::OnEraseBkgnd(pDC);
	pDC->RestoreDC(saved);
	return ret;
}

// the text box is used: the focus is not the panel's any more
void CFontPreviewDlg::OnEditSetFocus()
{
	m_bGlyphFocus = FALSE;
}

// Ctrl+A selects all the text of the text box (a multi line edit box does not do it by itself)
BOOL CFontPreviewDlg::PreTranslateMessage(MSG *pMsg)
{
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000) &&
	    GetFocus() == GetDlgItem(IDC_FONTPREVIEW_EDIT))
	{
		((CEdit *)GetDlgItem(IDC_FONTPREVIEW_EDIT))->SetSel(0, -1);
		return TRUE;
	}
	// left / right: the previous / next character of the preview becomes the selection (in the text box the keys move the caret)
	if (pMsg->message == WM_KEYDOWN && (pMsg->wParam == VK_LEFT || pMsg->wParam == VK_RIGHT) && m_bLoaded && !m_bPainting &&
	    m_Sel >= 0 && m_Sel < (int)m_Cells.size() && !(GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000))
	{
		CWnd *pFocus = GetFocus();
		BOOL ours = pFocus && (pFocus->m_hWnd == m_hWnd);
		if (ours)
		{
			int next = m_Sel + (pMsg->wParam == VK_RIGHT ? 1 : -1);
			if (next >= 0 && next < (int)m_Cells.size())
				SelectCell(next, TRUE);
			return TRUE;
		}
	}
	// undo and redo of the pixels (in the text box the keys belong to the text)
	if (m_bEditor && pMsg->message == WM_KEYDOWN && (GetKeyState(VK_CONTROL) & 0x8000) && GetFocus() != GetDlgItem(IDC_FONTPREVIEW_EDIT))
	{
		BOOL shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
		if (pMsg->wParam == 'Z' && !shift)
		{
			UndoRedo(TRUE);
			return TRUE;
		}
		if (pMsg->wParam == 'Y' || (pMsg->wParam == 'Z' && shift))
		{
			UndoRedo(FALSE);
			return TRUE;
		}
	}
	return CDarkDialog::PreTranslateMessage(pMsg);
}

HBRUSH CFontPreviewDlg::OnCtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor)
{
	return CDarkDialog::OnCtlColor(pDC, pWnd, nCtlColor);
}

BOOL CFontPreviewDlg::OnInitDialog()
{
	CDarkDialog::OnInitDialog();
	DarkMode::NoDefaultPushButtons(m_hWnd);
	PreviewZoom(0);
	CheckDlgButton(IDC_FONTPREVIEW_BGRID, BST_CHECKED);
	CheckDlgButton(IDC_FONTPREVIEW_BRULER, BST_CHECKED);
	CheckDlgButton(IDC_FONTPREVIEW_BMETRICS, BST_CHECKED);
	CheckDlgButton(IDC_FONTPREVIEW_PMETRICS, BST_CHECKED);
	((CEdit *)GetDlgItem(IDC_FONTPREVIEW_EDIT))->SetLimitText(0x7FFFFFFE);
	GetDlgItem(IDC_FONTPREVIEW_PALETTE)->ShowWindow(SW_HIDE);
	UpdateUndoButtons();
	UpdateRulerControls();
	CRect cr;
	GetClientRect(&cr);
	OnSize(0, cr.Width(), cr.Height());
	return TRUE;
}

// opens or closes the bitmap editor (below the preview)
void CFontPreviewDlg::SetEditor(BOOL bOpen)
{
	if (m_bEditor == bOpen)
		return;
	m_bPainting = FALSE;
	m_bEditor = bOpen;
	UpdateRulerControls();
	CRect cr;
	GetClientRect(&cr);
	OnSize(0, cr.Width(), cr.Height());
	Invalidate();
}

// another font, or the same one after a change that was made somewhere else: nothing of what was done to the old one stays
void CFontPreviewDlg::Reload()
{
	if (!m_bLoaded)
		return;
	m_Sel = -1;
	m_bExt = FALSE;
	m_bPainting = FALSE;
	m_Stroke.clear();
	m_UndoStack.clear();
	m_RedoStack.clear();
	m_BScroll = 0;
	m_BRow = 0;
	m_PScroll = 0;
	m_RulerOff = 0;
	ShowRulerOff();
	m_Depth = max(1, (int)m_pFont->GetFontBitDepth());
	m_Pick = 0;
	GetDlgItem(IDC_FONTPREVIEW_PALETTE)->ShowWindow(m_bEditor && m_Depth > 1 ? SW_SHOW : SW_HIDE);      // a 1 bit font has no colours to pick
	UpdateUndoButtons();
	RebuildText();
	CRect cr;
	GetClientRect(&cr);
	OnSize(0, cr.Width(), cr.Height());
	FitBitZoom();
	UpdateScrollBar();
	Invalidate();
}

// every character that the font can draw, in the order of its Unicode groups (the control characters have no place in a text box)
void CFontPreviewDlg::RebuildText(BOOL bForce)
{
	if (!m_bLoaded)
		return;
	if (m_bUserText && !bForce)
		return;               // the text of the user stays until it is reset
	CString s;
	DWORD numChars = m_pFont->GetNumChars();
	DWORD numMetrics = m_pFont->GetNumMetrics();
	for (DWORD g = 0; g < m_pFont->GetNumUnicodeGroups() && s.GetLength() < 60000; g++)
	{
		WORD start, offset;
		DWORD len;
		m_pFont->GetUnicodeGroup((WORD)g, &start, &len, &offset);
		for (DWORD k = 0; k < len; k++)
		{
			DWORD ch = (DWORD)start + k, ti = (DWORD)offset + k - 1;
			if (ch < 0x20 || ch == 0x7F || ch >= 0x10000 || (offset + k) < 1 || ti >= numChars)
				continue;
			if (m_pFont->GetCharMapping((WORD)ti) >= numMetrics && !m_pFont->IsFixedWidth())
				continue;              // the table points at a glyph that does not exist
			s += (WCHAR)ch;
		}
	}
	m_Sel = -1;
	m_bSettingText = TRUE;
	SetDlgItemText(IDC_FONTPREVIEW_EDIT, s);
	m_bSettingText = FALSE;
	m_bUserText = FALSE;
}

void CFontPreviewDlg::OnSize(UINT nType, int cx, int cy)
{
	RecGuard rg_("OnSize");   // a resize made while laying out must not lay out again
	if (RecGuard::depth > 1) return;
	CDarkDialog::OnSize(nType, cx, cy);
	CWnd *edit = GetDlgItem(IDC_FONTPREVIEW_EDIT), *area = GetDlgItem(IDC_FONTPREVIEW_AREA);
	CWnd *bg = GetDlgItem(IDC_FONTPREVIEW_BITGROUP), *ba = GetDlgItem(IDC_FONTPREVIEW_BITAREA);
	CWnd *sb = GetDlgItem(IDC_FONTPREVIEW_HSCROLL), *vs = GetDlgItem(IDC_FONTPREVIEW_VSCROLL), *bvs = GetDlgItem(IDC_FONTPREVIEW_BVSCROLL);
	if (!edit || !area || !bg || !ba || !sb || !vs || !bvs || cx == 0)
		return;
	CRect m(0, 3, 0, 3), d(6, 12, 3, 3), w(60, 0, 14, 0), w2(26, 0, 8, 0), minP(0, 40, 0, 30);
	MapDialogRect(&m);
	MapDialogRect(&d);
	MapDialogRect(&w);
	MapDialogRect(&w2);
	MapDialogRect(&minP);
	int left = 0, right = cx;
	CRect re, ra, rb;
	CWnd *reset = GetDlgItem(IDC_FONTPREVIEW_RESET);
	edit->GetWindowRect(&re);
	ScreenToClient(&re);
	reset->GetWindowRect(&rb);
	ScreenToClient(&rb);
	edit->MoveWindow(0, re.top, cx - rb.Width() - m.top, re.Height());
	reset->MoveWindow(cx - rb.Width(), re.top, rb.Width(), re.Height());
	area->GetWindowRect(&ra);
	ScreenToClient(&ra);
	// what is below the text box and its options: the preview and the bitmap viewer share it
	int top = ra.top, gap = m.top;
	int avail = max(minP.top + minP.bottom + gap, cy - top);
	int panelH = max(minP.top, (avail - gap) * m_EditorPct / 100);       // the share of the editor (the user can drag the line)
	int prevH = max(minP.bottom, avail - gap - panelH);
	if (!m_bEditor)
		prevH = avail;                                          // the editor is closed: the preview has all the room
	area->MoveWindow(left, top, right - left, prevH);
	int vsW = GetSystemMetrics(SM_CXVSCROLL);
	vs->MoveWindow(right - vsW - 2, top + 2, vsW, prevH - 4);
	{
		// the controls of the bitmap editor are only there while it is open (closed: out of the way, so no click lands on them)
		static const int kEditorIds[] = { IDC_FONTPREVIEW_BZOOMOUT, IDC_FONTPREVIEW_BZOOMIN, IDC_FONTPREVIEW_BZOOMTXT,
			IDC_FONTPREVIEW_BGRID, IDC_FONTPREVIEW_BMETRICS, IDC_FONTPREVIEW_BRULER,
			IDC_FONTPREVIEW_PALETTE, IDC_FONTPREVIEW_BITAREA, IDC_FONTPREVIEW_BVSCROLL,
			IDC_FONTPREVIEW_HSCROLL };
		for (int k = 0; k < sizeof(kEditorIds) / sizeof(kEditorIds[0]); k++)
		{
			CWnd *c = GetDlgItem(kEditorIds[k]);
			if (!c)
				continue;
			BOOL show = m_bEditor && !(kEditorIds[k] == IDC_FONTPREVIEW_PALETTE && m_Depth <= 1);
			c->ShowWindow(show ? SW_SHOW : SW_HIDE);
		}
	}
	if (!m_bEditor)
	{
		ba->MoveWindow(0, 0, 0, 0);
		bvs->MoveWindow(0, 0, 0, 0);
		sb->MoveWindow(0, 0, 0, 0);
		Invalidate();
		return;
	}
	int panelTop = top + prevH + gap;
	bg->ShowWindow(SW_HIDE);                    // the frame of the editor only costs room
	int x = left;
	int rowH = d.top;
	int y0 = panelTop + panelH - rowH - 1;         // the options of the editor (zoom, grid, metrics, ruler) are under it, so the line between it and the preview is thin
	GetDlgItem(IDC_FONTPREVIEW_BZOOMOUT)->MoveWindow(x, y0, w.right, rowH);
	GetDlgItem(IDC_FONTPREVIEW_BZOOMIN)->MoveWindow(x + w.right + 2, y0, w.right, rowH);
	GetDlgItem(IDC_FONTPREVIEW_BZOOMTXT)->MoveWindow(x + 2 * (w.right + 2) + 4, y0 + 2, w2.left, rowH - 2);
	x += 2 * (w.right + 2) + 4 + w2.left + w2.right;
	CRect og(36, 36, 46, 0), ov(18, 11, 0, 0);
	MapDialogRect(&og);
	MapDialogRect(&ov);
	GetDlgItem(IDC_FONTPREVIEW_BGRID)->MoveWindow(x, y0 + 1, og.left, rowH - 2);
	GetDlgItem(IDC_FONTPREVIEW_BMETRICS)->MoveWindow(x + og.left, y0 + 1, og.right, rowH - 2);
	int xr = x + og.left + og.right;
	GetDlgItem(IDC_FONTPREVIEW_BRULER)->MoveWindow(xr, y0 + 1, og.top, rowH - 2);
	x = xr + og.top + d.left;       // (the position of the ruler and its arrows are above the preview, in the row of its options)
	// Undo and Redo (buttons of the page) follow the Ruler box in this row; the picker of the colours comes after them
	CRect bw(40, 0, 0, 0);
	MapDialogRect(&bw);
	int btnW = bw.left;
	{
		CWnd *bar = GetParent();
		CWnd *bu = bar ? bar->GetDlgItem(IDC_FONTPREVIEW_UNDO) : NULL, *brd = bar ? bar->GetDlgItem(IDC_FONTPREVIEW_REDO) : NULL;
		if (bu && brd)
		{
			CRect pr;
			GetWindowRect(&pr);
			bar->ScreenToClient(&pr);
			bu->SetWindowPos(&CWnd::wndTop, pr.left + x, pr.top + y0, btnW, rowH, SWP_NOACTIVATE);
			brd->SetWindowPos(&CWnd::wndTop, pr.left + x + btnW + 4, pr.top + y0, btnW, rowH, SWP_NOACTIVATE);
		}
	}
	x += 2 * btnW + 4 + d.left;
	if (m_Depth > 1)
		GetDlgItem(IDC_FONTPREVIEW_PALETTE)->MoveWindow(x, y0, max(60, min(right - d.left - x, 16 * 10 * w.right / 14)), rowH);
	int y1 = panelTop + 1;
	int sbH = GetSystemMetrics(SM_CYHSCROLL);
	int viewH = max(20, y0 - 3 - sbH - y1);
	ba->MoveWindow(left, y1, right - left - vsW, viewH);
	bvs->MoveWindow(right - vsW, y1, vsW, viewH);
	sb->MoveWindow(left, y1 + viewH, right - left - vsW, sbH);
	UpdateScrollBar();
	Invalidate();
}

void CFontPreviewDlg::AreaRect(CRect &rc)
{
	GetDlgItem(IDC_FONTPREVIEW_AREA)->GetWindowRect(&rc);
	ScreenToClient(&rc);
	rc.DeflateRect(2, 2);
	rc.right -= GetSystemMetrics(SM_CXVSCROLL);     // the scroll bar of the text is at the right edge
}

void CFontPreviewDlg::OnEnChangeText()
{
	if (!m_bSettingText)
		m_bUserText = TRUE;           // typed, pasted or cut by the user
	m_Sel = -1;
	m_PScroll = 0;
	CRect rc;
	AreaRect(rc);
	InvalidateRect(rc, FALSE);
	BitAreaRect(rc);
	InvalidateRect(rc, FALSE);
}

// the ruler position (readout and arrows) only means something while a ruler is shown: the one of the preview, or the one of the
// editor while the editor is open
void CFontPreviewDlg::UpdateRulerControls()
{
	BOOL on = IsDlgButtonChecked(IDC_FONTPREVIEW_RULER) || (m_bEditor && IsDlgButtonChecked(IDC_FONTPREVIEW_BRULER));
	CWnd *spin = GetDlgItem(IDC_FONTPREVIEW_BRULSPIN), *val = GetDlgItem(IDC_FONTPREVIEW_BRULVAL);
	if (spin)
		spin->EnableWindow(on);
	if (val)
		val->EnableWindow(on);
}

void CFontPreviewDlg::OnCbnSelchangeZoom()
{
	UpdateRulerControls();
	CRect rc;
	AreaRect(rc);
	InvalidateRect(rc, FALSE);
}

// Is the character in one of the font's Unicode groups? (a character that is in none has no glyph: the iPod draws nothing for it)
static BOOL FontHasChar(CIpodFont *f, WORD ch, DWORD *tableIndex)
{
	for (DWORD i = 0; i < f->GetNumUnicodeGroups(); i++)
	{
		WORD start, offset;
		DWORD len;
		f->GetUnicodeGroup((WORD)i, &start, &len, &offset);
		if (ch >= start && ch < start + len)
		{
			*tableIndex = (DWORD)offset + (ch - start) - 1;
			return TRUE;
		}
	}
	return FALSE;
}

// the text on white, black where the glyphs have ink (the bitmaps of the fonts are gray levels). Every character goes through
// the Unicode groups, the character mapping table and the metrics of the font: the pen moves by the width, the bitmap is
// placed by the ident and cut out by the two offsets. A character the font does not have is drawn as a red box.
void CFontPreviewDlg::DrawText2(CDC &dc, const CRect &area, int zoom)
{
	CString text;
	GetDlgItemText(IDC_FONTPREVIEW_EDIT, text);
	m_Cells.clear();
	m_Lines.clear();
	m_Zoom = zoom;
	CSize bs = m_pFont->GetFontBitmapSize();
	int h = bs.cy;
	if (h <= 0)
		return;
	int aw = area.Width() / zoom, ah = area.Height() / zoom;
	if (aw <= 0 || ah <= 0)
		return;
	int penx = 2, peny = 2;
	m_Lines.push_back(peny);
	DWORD numChars = m_pFont->GetNumChars(), numMetrics = m_pFont->GetNumMetrics();
	m_Cells.reserve(text.GetLength());
	// 1. every character gets its place (the pen moves by the width; a line is full when the next character does not fit)
	for (int i = 0; i < text.GetLength(); i++)
	{
		WCHAR ch = text[i];
		if (ch == L'\r')
			continue;
		if (ch == L'\n')
		{
			penx = 2;
			peny += h + 2;
			m_Lines.push_back(peny);
			continue;
		}
		Cell cell;
		memset(&cell, 0, sizeof(cell));
		cell.ch = ch;
		DWORD ti = 0;
		cell.mapped = FontHasChar(m_pFont, ch, &ti) && ti < numChars;
		DWORD o1 = 0, o2 = 0;
		SHORT width = 0, ident = 0;
		if (cell.mapped)
		{
			cell.tableIndex = ti;
			WORD c = m_pFont->GetCharMapping((WORD)ti);
			cell.glyph = c;
			if (c < numMetrics || m_pFont->IsFixedWidth())
				m_pFont->GetCharMetrics(c, &o1, &o2, &width, &ident);
			else
				cell.mapped = FALSE;
		}
		if (!cell.mapped)
			width = (SHORT)max(4, h / 2);
		cell.o1 = o1;
		cell.o2 = o2;
		cell.width = width;
		cell.ident = ident;
		int gw = (o2 > o1 && o2 - o1 < 512) ? (int)(o2 - o1) : 0;
		if (penx + width > aw - 2 && penx > 2)
		{
			penx = 2;
			peny += h + 2;
			m_Lines.push_back(peny);
		}
		cell.penx = penx;
		cell.peny = peny;
		cell.gw = gw;
		penx += width;
		m_Cells.push_back(cell);
	}
	// 2. the text is taller than the preview: it scrolls
	m_ContentH = peny + h + 2;
	m_PScroll = min(max(m_PScroll, 0), max(0, m_ContentH - ah));
	UpdatePScrollBar(ah);
	for (size_t k = 0; k < m_Cells.size(); k++)
	{
		Cell &cell = m_Cells[k];
		cell.peny -= m_PScroll;
		cell.rc = CRect(cell.penx * zoom, cell.peny * zoom, (cell.penx + cell.width) * zoom, (cell.peny + h) * zoom);
	}
	for (size_t k = 0; k < m_Lines.size(); k++)
		m_Lines[k] -= m_PScroll;
	// 3. what is in view is drawn
	std::vector<unsigned int> px((size_t)aw * ah, 0x00FFFFFF);
	std::vector<CRect> boxes;
	for (size_t k = 0; k < m_Cells.size(); k++)
	{
		const Cell &cell = m_Cells[k];
		if (cell.peny + h < 0 || cell.peny >= ah)
			continue;
		if (cell.mapped)
		{
			for (int y = 0; y < h; y++)
				for (int x = 0; x < cell.gw; x++)
				{
					int dx = cell.penx + cell.ident + x, dy = cell.peny + y;
					if (dx < 0 || dx >= aw || dy < 0 || dy >= ah)
						continue;
					COLORREF cl = m_pFont->GetFontPixel(CPoint((int)cell.o1 + x, y));
					int g = GetRValue(cl);
					unsigned int &d = px[(size_t)dy * aw + dx];
					if (g < (int)(d & 255))
						d = (g << 16) | (g << 8) | g;
				}
		}
		else if (cell.ch != L' ')
			boxes.push_back(CRect(cell.penx, cell.peny + 1, cell.penx + cell.width - 1, cell.peny + h - 1));
	}
	BITMAPINFO bi;
	memset(&bi, 0, sizeof(bi));
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = aw;
	bi.bmiHeader.biHeight = -ah;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;
	dc.SetStretchBltMode(COLORONCOLOR);
	StretchDIBits(dc.GetSafeHdc(), area.left, area.top, aw * zoom, ah * zoom, 0, 0, aw, ah, &px[0], &bi, DIB_RGB_COLORS, SRCCOPY);
	// what the stretch does not cover
	if (aw * zoom < area.Width())
		dc.FillSolidRect(area.left + aw * zoom, area.top, area.Width() - aw * zoom, area.Height(), RGB(255, 255, 255));
	if (ah * zoom < area.Height())
		dc.FillSolidRect(area.left, area.top + ah * zoom, area.Width(), area.Height() - ah * zoom, RGB(255, 255, 255));
	CPen pen(PS_SOLID, 1, RGB(200, 0, 0));
	CPen *op = dc.SelectObject(&pen);
	CBrush *ob = (CBrush *)dc.SelectStockObject(NULL_BRUSH);
	for (size_t k = 0; k < boxes.size(); k++)
		dc.Rectangle(area.left + boxes[k].left * zoom, area.top + boxes[k].top * zoom, area.left + boxes[k].right * zoom,
		             area.top + boxes[k].bottom * zoom);
	dc.SelectObject(ob);
	dc.SelectObject(op);
	DrawOverlay(dc, area, zoom, h);
}

// the grid (one cell for each pixel of the bitmap), the baseline of every line, and, for the character that was clicked, the
// way the metrics of the font place it: two red rulers where the bitmap is cut out (Offset1, Offset2), a blue bar for the
// ident and a green one for the width
void CFontPreviewDlg::DrawOverlay(CDC &dc, const CRect &area, int zoom, int h)
{
	if (((CButton *)GetDlgItem(IDC_FONTPREVIEW_GRID))->GetCheck() && zoom >= 2)
	{
		// one cell for each pixel, as high as the bitmap, from the start of each character's advance (the pen) to its end, or
		// as far as its bitmap reaches when that is further: the space between the characters is part of the grid
		for (size_t i = 0; i < m_Cells.size(); i++)
		{
			const Cell &c = m_Cells[i];
			if (!c.mapped)
				continue;
			int lo = min(0, (int)c.ident), hi = max((int)c.width, (int)c.ident + c.gw);
			if (hi <= lo)
				continue;
			int left = area.left + (c.penx + lo) * zoom, top = area.top + c.peny * zoom;
			int cols = hi - lo, w = cols * zoom, ht = h * zoom;
			for (int x = 0; x <= cols; x++)
				dc.FillSolidRect(left + x * zoom - (x == cols ? 1 : 0), top, 1, ht, RGB(190, 190, 190));
			for (int y = 0; y <= h; y++)
				dc.FillSolidRect(left, top + y * zoom - (y == h ? 1 : 0), w, 1, RGB(190, 190, 190));
		}
	}
	if (((CButton *)GetDlgItem(IDC_FONTPREVIEW_RULER))->GetCheck())
	{
		int base = h - (int)m_pFont->GetFontDescent() - m_RulerOff;
		for (size_t i = 0; i < m_Lines.size(); i++)
		{
			int y = area.top + (m_Lines[i] + base) * zoom;
			if (y < area.bottom)
				dc.FillSolidRect(area.left, y, area.Width(), 1, RGB(0, 120, 215));
		}
	}
	if (IsDlgButtonChecked(IDC_FONTPREVIEW_PMETRICS) && m_Sel >= 0 && m_Sel < (int)m_Cells.size() && m_Cells[m_Sel].mapped)
	{
		const Cell &c = m_Cells[m_Sel];
		int top = area.top + c.peny * zoom, bot = area.top + (c.peny + h) * zoom;
		int x1 = area.left + (c.penx + c.ident) * zoom, x2 = x1 + c.gw * zoom;
		dc.FillSolidRect(x1, top, 1, bot - top + 4, RGB(220, 0, 0));
		dc.FillSolidRect(x2, top, 1, bot - top + 4, RGB(220, 0, 0));
		CBrush red(RGB(220, 0, 0));
		CBrush *ob = dc.SelectObject(&red);
		CPen nopen(PS_NULL, 0, RGB(0, 0, 0));
		CPen *op = dc.SelectObject(&nopen);
		POINT t1[3] = { { x1 - 3, bot + 5 }, { x1 + 4, bot + 5 }, { x1, bot + 1 } };
		POINT t2[3] = { { x2 - 3, bot + 5 }, { x2 + 4, bot + 5 }, { x2, bot + 1 } };
		dc.Polygon(t1, 3);
		dc.Polygon(t2, 3);
		dc.SelectObject(op);
		dc.SelectObject(ob);
		int px0 = area.left + c.penx * zoom;
		if (c.ident != 0)
			dc.FillSolidRect(min(px0, px0 + c.ident * zoom), bot + 8, abs(c.ident * zoom), 2, RGB(0, 0, 220));
		dc.FillSolidRect(px0, bot + 12, c.width * zoom, 2, RGB(0, 150, 0));
		dc.FillSolidRect(px0, bot + 10, 1, 6, RGB(0, 150, 0));
		dc.FillSolidRect(px0 + c.width * zoom, bot + 10, 1, 6, RGB(0, 150, 0));
	}
}


void CFontPreviewDlg::BitAreaRect(CRect &rc)
{
	GetDlgItem(IDC_FONTPREVIEW_BITAREA)->GetWindowRect(&rc);
	ScreenToClient(&rc);
	rc.DeflateRect(2, 2);
}

static const int kBitZooms[] = { 2, 3, 4, 6, 8, 10, 12, 16, 20 };
static const int kNumBitZooms = 9;

// How the bitmap of the font is cut into cells in the viewer: every row of the bitmap is shown (the cell is made smaller when
// the viewer is too low for the zoom), as many columns as fit starting at the one that was scrolled to.
BOOL CFontPreviewDlg::BitLayout(const CRect &rc, int *cell, int *start, int *cols, int *rows)
{
	if (!m_bLoaded)
		return FALSE;
	CSize bs = m_pFont->GetFontBitmapSize();
	if (bs.cx <= 0 || bs.cy <= 0)
		return FALSE;
	int ce = kBitZooms[m_BZoom];
	int room = rc.Height();
	if (IsDlgButtonChecked(IDC_FONTPREVIEW_BMETRICS) && room > 32)
		room -= 16;                  // under the bitmap: the bars of the indent and the width
	// the zoom is what was asked for; when the viewer is not high enough for all the rows it scrolls up and down
	int vr = max(1, min(room / ce, (int)bs.cy));
	m_BRowStart = min(max(m_BRow, 0), bs.cy - vr);
	int vis = max(1, rc.Width() / ce);
	if (vis > bs.cx)
		vis = bs.cx;
	int st = min(max(m_BScroll, 0), bs.cx - vis);
	*cell = ce;
	*start = st;
	*cols = vis;
	*rows = vr;
	return TRUE;
}

void CFontPreviewDlg::UpdateScrollBar()
{
	CScrollBar *sb = (CScrollBar *)GetDlgItem(IDC_FONTPREVIEW_HSCROLL);
	CRect br;
	BitAreaRect(br);
	int cell, start, cols, rows;
	CRect all(0, 0, br.Width(), br.Height());
	if (!sb || !BitLayout(all, &cell, &start, &cols, &rows))
		return;
	m_BScroll = start;
	CSize bs = m_pFont->GetFontBitmapSize();
	SCROLLINFO si;
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = 0;
	si.nMax = bs.cx - 1;
	si.nPage = cols;
	si.nPos = start;
	sb->SetScrollInfo(&si, TRUE);
	CScrollBar *vb = (CScrollBar *)GetDlgItem(IDC_FONTPREVIEW_BVSCROLL);
	if (vb)
	{
		SCROLLINFO sv;
		memset(&sv, 0, sizeof(sv));
		sv.cbSize = sizeof(sv);
		sv.fMask = SIF_ALL;
		sv.nMin = 0;
		sv.nMax = bs.cy - 1;
		sv.nPage = rows;
		sv.nPos = m_BRowStart;
		vb->SetScrollInfo(&sv, TRUE);
	}
	CString t;
	t.Format(_T("x%d"), cell);
	SetDlgItemText(IDC_FONTPREVIEW_BZOOMTXT, t);
}

void CFontPreviewDlg::SetPScroll(int v)
{
	m_PScroll = v;
	CRect area;
	AreaRect(area);
	InvalidateRect(area, FALSE);
}

// the scroll bar of the text: the range is the height of all of it, a page is what the preview shows (pixels of the bitmap)
void CFontPreviewDlg::UpdatePScrollBar(int pageRows)
{
	CScrollBar *vs = (CScrollBar *)GetDlgItem(IDC_FONTPREVIEW_VSCROLL);
	if (!vs)
		return;
	SCROLLINFO si;
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = 0;
	si.nMax = max(m_ContentH, 1) - 1;
	si.nPage = pageRows;
	si.nPos = m_PScroll;
	vs->SetScrollInfo(&si, TRUE);
}

void CFontPreviewDlg::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
	if (pScrollBar && pScrollBar->GetDlgCtrlID() == IDC_FONTPREVIEW_BVSCROLL)
	{
		SCROLLINFO sv;
		memset(&sv, 0, sizeof(sv));
		sv.cbSize = sizeof(sv);
		sv.fMask = SIF_ALL;
		pScrollBar->GetScrollInfo(&sv);
		int r = sv.nPos;
		switch (nSBCode)
		{
		case SB_LINEUP: r -= 1; break;
		case SB_LINEDOWN: r += 1; break;
		case SB_PAGEUP: r -= (int)sv.nPage; break;
		case SB_PAGEDOWN: r += (int)sv.nPage; break;
		case SB_TOP: r = 0; break;
		case SB_BOTTOM: r = sv.nMax; break;
		case SB_THUMBTRACK:
		case SB_THUMBPOSITION: r = sv.nTrackPos; break;
		default: return;
		}
		SetRowScroll(r);
		return;
	}
	if (!pScrollBar || pScrollBar->GetDlgCtrlID() != IDC_FONTPREVIEW_VSCROLL)
	{
		CDarkDialog::OnVScroll(nSBCode, nPos, pScrollBar);
		return;
	}
	SCROLLINFO si;
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	pScrollBar->GetScrollInfo(&si);
	int v = si.nPos;
	int line = max(1, (int)m_pFont->GetFontBitmapSize().cy);
	switch (nSBCode)
	{
	case SB_LINEUP: v -= line; break;
	case SB_LINEDOWN: v += line; break;
	case SB_PAGEUP: v -= (int)si.nPage; break;
	case SB_PAGEDOWN: v += (int)si.nPage; break;
	case SB_TOP: v = 0; break;
	case SB_BOTTOM: v = si.nMax; break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION: v = si.nTrackPos; break;
	default: return;
	}
	SetPScroll(v);
}

void CFontPreviewDlg::SetRowScroll(int v)
{
	m_BRow = v;
	UpdateScrollBar();
	CRect br;
	BitAreaRect(br);
	InvalidateRect(br, FALSE);
}

// the biggest zoom of the viewer that still shows every row of the bitmap
void CFontPreviewDlg::FitBitZoom()
{
	CRect br;
	BitAreaRect(br);
	CSize bs = m_pFont->GetFontBitmapSize();
	if (bs.cy <= 0)
		return;
	int room = br.Height();
	if (IsDlgButtonChecked(IDC_FONTPREVIEW_BMETRICS) && room > 32)
		room -= 16;
	int best = 0;
	for (int i = 0; i < kNumBitZooms; i++)
		if (kBitZooms[i] * bs.cy <= room)
			best = i;
	m_BZoom = best;
}

void CFontPreviewDlg::SetScroll(int v)
{
	m_BScroll = v;
	UpdateScrollBar();
	CRect br;
	BitAreaRect(br);
	InvalidateRect(br, FALSE);
}

// the viewer scrolls so that the selected character is in the middle of it
// the metrics of what is selected: a glyph chosen in the boxes of the tab, or the character that was clicked in the preview
BOOL CFontPreviewDlg::SelMetrics(DWORD *o1, DWORD *o2, int *w, int *i)
{
	if (m_bExt)
	{
		*o1 = m_ExtO1; *o2 = m_ExtO2; *w = m_ExtW; *i = m_ExtI;
		return TRUE;
	}
	if (m_Sel >= 0 && m_Sel < (int)m_Cells.size() && m_Cells[m_Sel].mapped)
	{
		const Cell &c = m_Cells[m_Sel];
		*o1 = c.o1; *o2 = c.o2; *w = c.width; *i = c.ident;
		return TRUE;
	}
	return FALSE;
}

// An entry / glyph was chosen in the boxes: its offsets, width and indent are shown in the editor, the editor goes to them, and
// the preview selects (and shows) the first character that is drawn with that entry.
void CFontPreviewDlg::SelectGlyph(int tableIdx, int glyph)
{
	if (!m_bLoaded)
		return;
	if (glyph < 0 || (glyph >= (int)m_pFont->GetNumMetrics() && !m_pFont->IsFixedWidth()))
	{
		m_bExt = FALSE;
		m_Sel = -1;
		Invalidate();
		return;
	}
	DWORD o1 = 0, o2 = 0;
	SHORT w = 0, i = 0;
	m_pFont->GetCharMetrics((WORD)glyph, &o1, &o2, &w, &i);
	m_bExt = TRUE;
	m_ExtO1 = o1; m_ExtO2 = o2; m_ExtW = w; m_ExtI = i;
	if (!(m_Sel >= 0 && m_Sel < (int)m_Cells.size() && m_Cells[m_Sel].mapped && (int)m_Cells[m_Sel].tableIndex == tableIdx))
	{
		m_Sel = -1;
		for (size_t k = 0; k < m_Cells.size(); k++)
			if (m_Cells[k].mapped && (int)m_Cells[k].tableIndex == tableIdx)
			{
				m_Sel = (int)k;
				// the line of that character is brought into view
				CRect ar;
				AreaRect(ar);
				int ah = max(1, ar.Height() / kPrevZooms[m_PZoomIdx]);
				int h = max(1, (int)m_pFont->GetFontBitmapSize().cy);
				int top = m_Cells[k].peny + m_PScroll;
				if (top < m_PScroll || top + h > m_PScroll + ah)
					m_PScroll = max(0, top - 2);
				break;
			}
	}
	CRect ar, br;
	AreaRect(ar);
	BitAreaRect(br);
	int cell, start, cols, rows;
	CRect all(0, 0, br.Width(), br.Height());
	if (BitLayout(all, &cell, &start, &cols, &rows))
	{
		int mid = (int)o1 + (int)(o2 > o1 ? (o2 - o1) / 2 : 0);
		m_BScroll = mid - cols / 2;
		UpdateScrollBar();
	}
	InvalidateRect(ar, FALSE);
	InvalidateRect(br, FALSE);
}

void CFontPreviewDlg::CenterOnSel()
{
	DWORD so1, so2;
	int sw, si;
	if (!SelMetrics(&so1, &so2, &sw, &si))
		return;
	CRect br;
	BitAreaRect(br);
	int cell, start, cols, rows;
	CRect all(0, 0, br.Width(), br.Height());
	if (!BitLayout(all, &cell, &start, &cols, &rows))
		return;
	int mid = (int)so1 + (int)(so2 > so1 ? (so2 - so1) / 2 : 0);
	SetScroll(mid - cols / 2);
}

void CFontPreviewDlg::BitZoom(int dir)
{
	CRect br;
	BitAreaRect(br);
	int cell, start, cols, rows;
	CRect all(0, 0, br.Width(), br.Height());
	if (!BitLayout(all, &cell, &start, &cols, &rows))
		return;
	int centre = start + cols / 2, centreRow = m_BRowStart + rows / 2;
	m_BZoom = min(max(m_BZoom + dir, 0), kNumBitZooms - 1);
	BitLayout(all, &cell, &start, &cols, &rows);
	m_BRow = centreRow - rows / 2;
	SetScroll(centre - cols / 2);
}

void CFontPreviewDlg::OnBZoomIn() { BitZoom(1); }
void CFontPreviewDlg::OnBZoomOut() { BitZoom(-1); }

void CFontPreviewDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
	if (!pScrollBar || pScrollBar->GetDlgCtrlID() != IDC_FONTPREVIEW_HSCROLL)
	{
		CDarkDialog::OnHScroll(nSBCode, nPos, pScrollBar);
		return;
	}
	SCROLLINFO si;
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	pScrollBar->GetScrollInfo(&si);
	int v = si.nPos;
	switch (nSBCode)
	{
	case SB_LINELEFT: v -= 1; break;
	case SB_LINERIGHT: v += 1; break;
	case SB_PAGELEFT: v -= (int)si.nPage; break;
	case SB_PAGERIGHT: v += (int)si.nPage; break;
	case SB_LEFT: v = 0; break;
	case SB_RIGHT: v = si.nMax; break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION: v = si.nTrackPos; break;
	default: return;
	}
	SetScroll(v);
}

// over the viewer the mouse wheel scrolls it, with Ctrl it zooms
BOOL CFontPreviewDlg::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	CRect br, pr;
	BitAreaRect(br);
	ClientToScreen(&br);
	AreaRect(pr);
	ClientToScreen(&pr);
	int clicks = zDelta / 120;
	if (clicks == 0)
		clicks = zDelta > 0 ? 1 : -1;
	if (pr.PtInRect(pt))
	{
		SetPScroll(m_PScroll - clicks * 12);
		return TRUE;
	}
	if (!br.PtInRect(pt))
		return CDarkDialog::OnMouseWheel(nFlags, zDelta, pt);
	if (nFlags & MK_CONTROL)
		BitZoom(clicks);
	else
		SetScroll(m_BScroll - clicks * 3);
	return TRUE;
}

// the bitmap of the font: the pixels as they are; for the selected character its own columns (Offset1 up to Offset2) are between
// two red lines and the rest of the bitmap is paler
void CFontPreviewDlg::DrawBitEditor(CDC &dc)
{
	CRect rc;
	BitAreaRect(rc);
	if (rc.Width() <= 0 || rc.Height() <= 0)
		return;
	CDC mem;
	mem.CreateCompatibleDC(&dc);
	CBitmap bmp;
	bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
	CBitmap *old = mem.SelectObject(&bmp);
	CRect all(0, 0, rc.Width(), rc.Height());
	mem.FillSolidRect(&all, DarkMode::Enabled() ? DarkMode::BackColor() : GetSysColor(COLOR_BTNFACE));
	int cell, start, cols, rows;
	if (BitLayout(all, &cell, &start, &cols, &rows))
	{
		DWORD o1 = 0, o2 = 0;
		int selW = 0, selI = 0;
		BOOL sel = SelMetrics(&o1, &o2, &selW, &selI);
		BOOL metrics = IsDlgButtonChecked(IDC_FONTPREVIEW_BMETRICS);
		for (int x = 0; x < cols; x++)
			for (int y = 0; y < rows; y++)
			{
				int col = start + x;
				int v = GetRValue(m_pFont->GetFontPixel(CPoint(col, m_BRowStart + y)));
				if (sel && metrics && (col < (int)o1 || col >= (int)o2))
					v = 255 - (255 - v) * 45 / 100;
				mem.FillSolidRect(x * cell, y * cell, cell, cell, RGB(v, v, v));
			}
		BOOL grid = IsDlgButtonChecked(IDC_FONTPREVIEW_BGRID), rulers = IsDlgButtonChecked(IDC_FONTPREVIEW_BRULER);
		if (grid)
		{
			for (int x = 0; x <= cols; x++)
				mem.FillSolidRect(x * cell, 0, 1, rows * cell, RGB(190, 190, 190));
			for (int y = 0; y <= rows; y++)
				mem.FillSolidRect(0, y * cell, cols * cell, 1, RGB(190, 190, 190));
		}
		// the metrics of the selected glyph: where it is cut out (red), its indent (blue) and its width (green)
		if (metrics && sel)
		{
			{
				// Offset1 and Offset2: where the glyph is cut out of the bitmap
				int x1 = ((int)o1 - start) * cell, x2 = ((int)o2 - start) * cell;
				if (x1 >= 0 && x1 <= cols * cell)
					mem.FillSolidRect(x1 - 1, 0, 2, rows * cell, RGB(220, 0, 0));
				if (x2 >= 0 && x2 <= cols * cell)
					mem.FillSolidRect(x2 - 1, 0, 2, rows * cell, RGB(220, 0, 0));
				// the pen is where the glyph would start without its indent; the next character starts one width after the pen
				int pen = (int)o1 - selI;
				int xp = (pen - start) * cell, xe = (pen + selW - start) * cell, xg = ((int)o1 - start) * cell;
				int barY = rows * cell + 2;
				// indent (blue): from the pen to the first column of the glyph
				if (selI != 0)
				{
					int a = min(xp, xg), b = max(xp, xg);
					mem.FillSolidRect(max(a, 0), barY, max(min(b, cols * cell) - max(a, 0), 0), 4, RGB(0, 0, 220));
				}
				// width (green): the room the glyph takes, from the pen to the start of the next character
				{
					int a = max(xp, 0), b = min(xe, cols * cell);
					if (b > a)
						mem.FillSolidRect(a, barY + 7, b - a, 4, RGB(0, 150, 0));
					if (xp >= 0 && xp <= cols * cell)
						mem.FillSolidRect(xp, 0, 1, rows * cell + 14, RGB(0, 150, 0));
					if (xe >= 0 && xe <= cols * cell)
						mem.FillSolidRect(xe, 0, 1, rows * cell + 14, RGB(0, 150, 0));
				}
			}
		}
		if (rulers)
		{
			// the baseline
			int base = (int)m_pFont->GetFontBitmapSize().cy - (int)m_pFont->GetFontDescent() - m_RulerOff - m_BRowStart;
			if (base >= 0 && base <= rows)
				mem.FillSolidRect(0, base * cell - 1, cols * cell, 2, RGB(0, 120, 215));
		}
	}
	dc.BitBlt(rc.left, rc.top, rc.Width(), rc.Height(), &mem, 0, 0, SRCCOPY);
	mem.SelectObject(old);
}

void CFontPreviewDlg::PaletteRect(CRect &rc)
{
	GetDlgItem(IDC_FONTPREVIEW_PALETTE)->GetWindowRect(&rc);
	ScreenToClient(&rc);
	rc.DeflateRect(2, 2);
}

void CFontPreviewDlg::DrawPalette(CDC &dc)
{
	if (m_Depth <= 1)
		return;
	CRect rc;
	PaletteRect(rc);
	int n = Levels();
	for (int i = 0; i < n; i++)
	{
		int x1 = rc.left + i * rc.Width() / n, x2 = rc.left + (i + 1) * rc.Width() / n;
		int v = LevelValue(i);
		dc.FillSolidRect(x1, rc.top, x2 - x1, rc.Height(), RGB(v, v, v));
		if (i == m_Pick)
		{
			COLORREF mark = v < 128 ? RGB(255, 255, 255) : RGB(0, 0, 0);
			dc.FillSolidRect(x1, rc.top, x2 - x1, 2, RGB(220, 0, 0));
			dc.FillSolidRect(x1, rc.bottom - 2, x2 - x1, 2, RGB(220, 0, 0));
			dc.FillSolidRect(x1, rc.top, 2, rc.Height(), RGB(220, 0, 0));
			dc.FillSolidRect(x2 - 2, rc.top, 2, rc.Height(), RGB(220, 0, 0));
			(void)mark;
		}
	}
}

// the pixel of the font bitmap under a point of the editor; with clamp the nearest one when the point is outside of it
BOOL CFontPreviewDlg::CellAt(CPoint point, int *x, int *y, BOOL clamp)
{
	if (!m_bEditor)
		return FALSE;
	CRect br;
	BitAreaRect(br);
	int cell, start, cols, rows;
	CRect all(0, 0, br.Width(), br.Height());
	if (!BitLayout(all, &cell, &start, &cols, &rows))
		return FALSE;
	if (!clamp && !br.PtInRect(point))
		return FALSE;
	int cx = (point.x - br.left) / cell, cy = (point.y - br.top) / cell;
	if (point.x < br.left)
		cx = 0;
	if (point.y < br.top)
		cy = 0;
	if (!clamp && (cx >= cols || cy >= rows))
		return FALSE;
	cx = min(max(cx, 0), cols - 1);
	cy = min(max(cy, 0), rows - 1);
	*x = start + cx;
	*y = m_BRowStart + cy;
	return TRUE;
}

void CFontPreviewDlg::PaintCell(int x, int y)
{
	int old = GetRValue(m_pFont->GetFontPixel(CPoint(x, y)));
	if (old == m_PaintVal)
		return;
	m_pFont->SetFontPixel(x, y, RGB(m_PaintVal, m_PaintVal, m_PaintVal));
	PixEdit e = { x, y, old, m_PaintVal };
	m_Stroke.push_back(e);
	m_bEdited = TRUE;
	CRect area, br;
	AreaRect(area);
	BitAreaRect(br);
	InvalidateRect(area, FALSE);
	InvalidateRect(br, FALSE);
}

void CFontPreviewDlg::EndStroke()
{
	if (!m_bPainting)
		return;
	m_bPainting = FALSE;
	if (GetCapture() == this)
		ReleaseCapture();
	if (!m_Stroke.empty())
	{
		m_UndoStack.push_back(m_Stroke);
		if (m_UndoStack.size() > 100)
			m_UndoStack.erase(m_UndoStack.begin());       // the history is the last 100 changes
		m_RedoStack.clear();
	}
	m_Stroke.clear();
	UpdateUndoButtons();
	if (m_pNotify)
		m_pNotify->SendMessage(WM_FONTPANEL_EDITED, 0, 0);
}

// the line between the preview and the bitmap editor: a few pixels high, as wide as the panel (only while the editor is open)
BOOL CFontPreviewDlg::SplitterRect(CRect &rc)
{
	if (!m_bEditor || !m_bLoaded)
		return FALSE;
	CWnd *area = GetDlgItem(IDC_FONTPREVIEW_AREA);
	if (!area)
		return FALSE;
	CRect ra, cr, g(0, 3, 0, 0);
	area->GetWindowRect(&ra);
	ScreenToClient(&ra);
	GetClientRect(&cr);
	MapDialogRect(&g);
	rc = CRect(0, ra.bottom - 1, cr.right, ra.bottom + g.top + 1);
	return TRUE;
}

BOOL CFontPreviewDlg::OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT message)
{
	CRect rc;
	CPoint pt;
	::GetCursorPos(&pt);
	ScreenToClient(&pt);
	if (m_bSplitDrag || (SplitterRect(rc) && rc.PtInRect(pt)))
	{
		::SetCursor(::LoadCursor(NULL, IDC_SIZENS));
		return TRUE;
	}
	return CDarkDialog::OnSetCursor(pWnd, nHitTest, message);
}

void CFontPreviewDlg::OnCaptureChanged(CWnd *pWnd)
{
	m_bSplitDrag = FALSE;
	CDarkDialog::OnCaptureChanged(pWnd);
}

void CFontPreviewDlg::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_bSplitDrag)
	{
		m_bSplitDrag = FALSE;
		if (GetCapture() == this)
			ReleaseCapture();
		return;
	}
	EndStroke();
	CDarkDialog::OnLButtonUp(nFlags, point);
}

// dragging with the button down paints the pixels that the pointer passes (also the ones between two mouse messages)
void CFontPreviewDlg::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_bSplitDrag && (nFlags & MK_LBUTTON))
	{
		// the editor gets what is under the line: its top is where the mouse is
		CRect cr, ra, g(0, 3, 0, 0);
		GetClientRect(&cr);
		GetDlgItem(IDC_FONTPREVIEW_AREA)->GetWindowRect(&ra);
		ScreenToClient(&ra);
		MapDialogRect(&g);
		int avail = cr.Height() - ra.top;
		int panelH = cr.Height() - (point.y + g.top / 2);
		int pct = panelH * 100 / max(1, avail - g.top);
		pct = min(max(pct, 15), 85);
		if (pct != m_EditorPct)
		{
			m_EditorPct = pct;
			OnSize(0, cr.Width(), cr.Height());
			RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);     // (the controls that moved are drawn again)
		}
		return;
	}
	if (m_bPainting && (nFlags & MK_LBUTTON))
	{
		int x, y;
		if (CellAt(point, &x, &y, TRUE))
		{
			int x0 = m_LastX, y0 = m_LastY;
			int dx = abs(x - x0), dy = abs(y - y0), sx = x0 < x ? 1 : -1, sy = y0 < y ? 1 : -1, err = dx - dy;
			for (;;)
			{
				PaintCell(x0, y0);
				if (x0 == x && y0 == y)
					break;
				int e2 = 2 * err;
				if (e2 > -dy)
				{
					err -= dy;
					x0 += sx;
				}
				if (e2 < dx)
				{
					err += dx;
					y0 += sy;
				}
			}
			m_LastX = x;
			m_LastY = y;
		}
	}
	CDarkDialog::OnMouseMove(nFlags, point);
}

void CFontPreviewDlg::UpdateUndoButtons()
{
	CWnd *bar = GetParent();      // the buttons are on the right bar of the tab
	if (bar && bar->GetDlgItem(IDC_FONTPREVIEW_UNDO))
	{
		bar->GetDlgItem(IDC_FONTPREVIEW_UNDO)->EnableWindow(!m_UndoStack.empty());
		bar->GetDlgItem(IDC_FONTPREVIEW_REDO)->EnableWindow(!m_RedoStack.empty());
	}
}

void CFontPreviewDlg::UndoRedo(BOOL undo)
{
	EndStroke();
	std::vector<Stroke> &from = undo ? m_UndoStack : m_RedoStack;
	std::vector<Stroke> &to = undo ? m_RedoStack : m_UndoStack;
	if (from.empty())
		return;
	Stroke s = from.back();
	from.pop_back();
	for (size_t i = 0; i < s.size(); i++)
	{
		const PixEdit &e = undo ? s[s.size() - 1 - i] : s[i];
		int v = undo ? e.oldv : e.newv;
		m_pFont->SetFontPixel(e.x, e.y, RGB(v, v, v));
	}
	to.push_back(s);
	if (to.size() > 100)
		to.erase(to.begin());
	m_bEdited = TRUE;
	UpdateUndoButtons();
	if (m_pNotify)
		m_pNotify->SendMessage(WM_FONTPANEL_EDITED, 0, 0);
	CRect area, br;
	AreaRect(area);
	BitAreaRect(br);
	InvalidateRect(area, FALSE);
	InvalidateRect(br, FALSE);
}

// zoom of the preview, the same way as the zoom of the bitmap editor (up to 20 times)
void CFontPreviewDlg::PreviewZoom(int dir)
{
	m_PZoomIdx = min(max(m_PZoomIdx + dir, 0), kNumPrevZooms - 1);
	CString t;
	t.Format(_T("x%d"), kPrevZooms[m_PZoomIdx]);
	SetDlgItemText(IDC_FONTPREVIEW_PZOOMTXT, t);
	CRect area;
	AreaRect(area);
	InvalidateRect(area, FALSE);
}

// the text box goes back to every character that the font has now (its groups, the mapping and the metrics as they are)
void CFontPreviewDlg::OnResetText()
{
	RebuildText(TRUE);
	m_PScroll = 0;
}

// the grid and the rulers of the editor: the room under the bitmap for the width and the indent changes with the rulers
// The ruler is the baseline of the font (the number of rows of the bitmap minus the descent of its header) moved up or down as far
// as the user wants. 0 is the baseline of the header; it is only what is shown, nothing is changed in the font.
void CFontPreviewDlg::ShowRulerOff()
{
	CString s;
	if (m_RulerOff > 0)
		s.Format(_T("+%d"), m_RulerOff);
	else
		s.Format(_T("%d"), m_RulerOff);
	SetDlgItemText(IDC_FONTPREVIEW_BRULVAL, s);
}

void CFontPreviewDlg::OnRulerSpin(NMHDR *pNMHDR, LRESULT *pResult)
{
	NMUPDOWN *pUD = (NMUPDOWN *)pNMHDR;
	*pResult = 1;                          // the arrows do not keep a position of their own
	if (!m_bLoaded)
		return;
	int h = (int)m_pFont->GetFontBitmapSize().cy, base = h - (int)m_pFont->GetFontDescent();
	int off = m_RulerOff + (pUD->iDelta < 0 ? 1 : -1);      // the arrow up (a negative change: the range of the control runs from 100 down to 0) moves the ruler up
	if (base - off < 0)
		off = base;
	if (base - off > h)
		off = base - h;
	m_RulerOff = off;
	ShowRulerOff();
	CRect ar, br;
	AreaRect(ar);
	BitAreaRect(br);
	InvalidateRect(ar, FALSE);
	InvalidateRect(br, FALSE);
}

void CFontPreviewDlg::OnEditorOptions()
{
	UpdateRulerControls();
	UpdateScrollBar();
	CRect br;
	BitAreaRect(br);
	InvalidateRect(br, FALSE);
}

void CFontPreviewDlg::OnPZoomIn() { PreviewZoom(1); }
void CFontPreviewDlg::OnPZoomOut() { PreviewZoom(-1); }

void CFontPreviewDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
	CRect splitRc;
	if (SplitterRect(splitRc) && splitRc.PtInRect(point))
	{
		m_bSplitDrag = TRUE;
		SetCapture();
		return;
	}
	// the picker: a click on a colour
	if (m_Depth > 1)
	{
		CRect pr;
		PaletteRect(pr);
		if (pr.PtInRect(point))
		{
			int n = Levels();
			int i = (point.x - pr.left) * n / max(1, pr.Width());
			m_Pick = min(max(i, 0), n - 1);
			InvalidateRect(pr, FALSE);
			return;
		}
	}
	// a press in the bitmap editor starts a stroke: the first pixel is inverted (with a picker: given the colour that is picked,
	// or the opposite one when it already has it) and what is dragged over afterwards gets the same colour
	{
		int x, y;
		if (CellAt(point, &x, &y, FALSE))
		{
			CRect br;
			BitAreaRect(br);
			int cell, start, cols, rows;
			CRect all(0, 0, br.Width(), br.Height());
			BitLayout(all, &cell, &start, &cols, &rows);
			int cur = GetRValue(m_pFont->GetFontPixel(CPoint(x, y)));
			if (m_Depth == 1)
				m_PaintVal = cur < 128 ? 255 : 0;
			else
			{
				int pv = LevelValue(m_Pick);
				m_PaintVal = (cur == pv) ? 255 - pv : pv;
			}
			m_Stroke.clear();
			m_bPainting = TRUE;
			SetCapture();
			m_LastX = x;
			m_LastY = y;
			PaintCell(x, y);
			return;
		}
		CRect br;
		BitAreaRect(br);
		if (br.PtInRect(point))
			return;
	}
	CRect area;
	AreaRect(area);
	CPoint rel(point.x - area.left, point.y - area.top);
	if (!area.PtInRect(point))
		return;
	int hit = -1;
	for (size_t i = 0; i < m_Cells.size(); i++)
		if (m_Cells[i].rc.PtInRect(rel))
		{
			hit = (int)i;
			break;
		}
	if (hit < 0)
		return;
	SelectCell(hit, FALSE);
	m_bGlyphFocus = TRUE;
	SetFocus();		// the arrow keys now step through the characters (see PreTranslateMessage)
}

// the character at m_Cells[hit] becomes the selection (a click, or a step with the arrow keys)
void CFontPreviewDlg::SelectCell(int hit, BOOL bScrollIntoView)
{
	if (hit < 0 || hit >= (int)m_Cells.size())
		return;
	CRect area;
	AreaRect(area);
	const Cell &c = m_Cells[hit];
	m_Sel = hit;
	m_bExt = FALSE;
	if (bScrollIntoView)
	{
		// the line of that character is brought into view
		int ah = max(1, area.Height() / kPrevZooms[m_PZoomIdx]);
		int h = max(1, (int)m_pFont->GetFontBitmapSize().cy);
		int top = c.peny + m_PScroll;
		if (top < m_PScroll || top + h > m_PScroll + ah)
			m_PScroll = max(0, top - 2);
	}
	CenterOnSel();
	if (m_pNotify && c.mapped)
		m_pNotify->SendMessage(WM_FONTPANEL_PICK, (WPARAM)c.ch, 0);
	{
		CRect br;
		BitAreaRect(br);
		InvalidateRect(br, FALSE);
	}
	InvalidateRect(area, FALSE);
}

void CFontPreviewDlg::OnPaint()
{
	CPaintDC dc(this);
	CRect area;
	AreaRect(area);
	int zoom = kPrevZooms[m_PZoomIdx];
	if (!m_bLoaded)
	{
		dc.FillSolidRect(&area, RGB(255, 255, 255));
		return;
	}
	if (area.Width() <= 0 || area.Height() <= 0)
		return;
	CDC mem;
	mem.CreateCompatibleDC(&dc);
	CBitmap bmp;
	bmp.CreateCompatibleBitmap(&dc, area.Width(), area.Height());
	CBitmap *old = mem.SelectObject(&bmp);
	DrawText2(mem, CRect(0, 0, area.Width(), area.Height()), zoom);
	dc.BitBlt(area.left, area.top, area.Width(), area.Height(), &mem, 0, 0, SRCCOPY);
	mem.SelectObject(old);
	if (m_bEditor)
	{
		DrawBitEditor(dc);
		DrawPalette(dc);
	}
}

