#pragma once

#include "Firmware.h"
#include "OTFFontParser.h"
#include "OTFFontWnd.h"
#include "afxwin.h"
#include <vector>
#include "LayoutDialog.h"

// COTFDialog dialog

class COTFDialog : public CDarkDialog
{
	DECLARE_DYNAMIC(COTFDialog)

public:
	COTFDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~COTFDialog();

// Dialog Data
	enum { IDD = IDD_OTF_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	COTFFont m_Font;
	CFirmware * m_pFirmware;
	CLayoutDialog *m_pLayoutDialog;
	void SetFirmware(CFirmware *pFirmware, CLayoutDialog *pLayoutDialog);
	void UpdateFont();
	DWORD m_FontIndex;
	COTFFontWnd m_FontWnd;
	COTFFontWnd m_ZoomWnd;
	CComboBox m_FontIndexCombo;
	CComboBox m_GlyphCombo;
	CComboBox m_CharCombo;
	CComboBox m_GroupIndexCombo;
	CString m_FontName;
	CString m_FontStyle;
	CString m_FontStyleID;      // the number of the style that the layouts use for this font (the box beside Style:)
	BOOL m_bUserText;           // the text of the preview was typed by the user: it stays when the font changes (until Reset Text)
	BOOL m_bSettingText;
	BOOL m_bReadingBoxes;       // the boxes of the glyph are being filled from the font: their change handlers must not write them back
	afx_msg void OnEnChangeWidthEdit();
	afx_msg void OnEnChangeHeightEdit();
	void RebuildText(BOOL bForce);
	void FillGroupCombo(int sel);           // "index  Block name" for every group
	void FillGroupListCombo();              // every character of the group, with its entry
	afx_msg void OnCbnSelchangeGroupListCombo();
	virtual BOOL PreTranslateMessage(MSG *pMsg);
	afx_msg HBRUSH OnCtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor);
	afx_msg void OnEnChangeText();
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar);
	// The page grows with the window: the previews take the room that is added (the glyph view keeps its width, the line between the
	// two previews can be dragged), the buttons on the right and the glyph view keep to the right edge, the boxes at the bottom keep
	// to the bottom edge.
	struct BaseCtl { HWND hwnd; CRect rc; };
	std::vector<BaseCtl> m_BaseCtls;     // where the controls are in the template (what the window grows from)
	CRect m_BaseClient;
	BOOL m_bBaseSet;
	int m_GlyphW;                        // the width of the glyph view in pixels (what the user dragged it to)
	BOOL m_bSplit;                       // the line between the previews is being dragged
	int m_SplitX, m_SplitW;
	void CaptureBaseLayout();
	BOOL SplitterRect(CRect &rc);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnCaptureChanged(CWnd *pWnd);
	afx_msg BOOL OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT message);
	afx_msg void OnBnClickedResetText();
	int m_FontSize;
	int m_BitDepth;
	BYTE m_Width;
	BYTE m_Height;
	CHAR m_BearingX;
	CHAR m_BearingY;
	BYTE m_Advance;
	WORD m_GroupOffset;
	WORD m_GroupEnd;
	WORD m_GroupStart;
	CString m_GroupExplain;
	WORD m_GlyphIndex;

	CHAR m_ALeft;
	BYTE m_ARight;
	BYTE m_BaseLine;
protected:
	virtual void OnOK();
public:
	afx_msg void OnCbnSelchangeFontidxCombo();
	afx_msg void OnBnClickedRefreshButton();
	afx_msg void OnBnClickedMakeFont();
	afx_msg void OnCbnSelchangeCharCombo();
	afx_msg void OnCbnSelchangeGroupidxCombo();
	afx_msg void OnBnClickedZoomoutButton();
	afx_msg void OnBnClickedZoominButton();
	afx_msg void OnCbnSelchangeGlyphidxCombo();

	void UpdateCharCombo();
	void UpdateGroupExplanation();
	void ReadUnicodeGroup();
	void ReadCharMetrics();
	void UpdateZoomView();
	void WriteUnicodeGroup();
	void WriteCharMetrics();
	void LoadFullFont(CString path, DWORD size, DWORD fidx=-1);
	void LoadFullFontsDir(CString folderpath);
	afx_msg void OnBnClickedLoadGlyph();
	afx_msg void OnBnClickedSaveGlyph();
	afx_msg void OnBnClickedChkgrid();
	int m_PrevZoomIdx;                  // zoom of the text preview (an entry of the list of zooms)
	afx_msg void ApplyPreviewOptions();
	afx_msg void OnPreviewZoomIn();
	afx_msg void OnPreviewZoomOut();
	afx_msg void OnEnChangeAdvanceEdit();
	afx_msg void OnEnChangeBearingyEdit();
	afx_msg void OnEnChangeBearingxEdit();
	afx_msg void OnEnChangeGroupstartEdit();
	afx_msg void OnEnChangeGroupendEdit();
	afx_msg void OnEnChangeGroupoffsetEdit();
	afx_msg void OnBnClickedLoadFont();
	afx_msg void OnBnClickedSaveFont();
	afx_msg void OnBnClickedMulloadGlyph();
	afx_msg LRESULT OnSelectMetricsFromBitmap(WPARAM wParam, LPARAM lParam);
	afx_msg void OnBnClickedMulsaveGlyph();
};