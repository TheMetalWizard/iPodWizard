#pragma once

#include "Firmware.h"
#include "PicturesDialog.h"
#include "FontsDialog.h"
#include "StringDialog.h"
#include "OTFDialog.h"
#include "LayoutDialog.h"
#include "TweaksDialog.h"
#include "afxwin.h"
#include "afxcmn.h"

// CEditorDialog dialog

class CEditorDialog : public CDarkDialog
{
	DECLARE_DYNAMIC(CEditorDialog)

public:
	CEditorDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CEditorDialog();

// Dialog Data
	enum { IDD = IDD_EDITOR_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage(MSG *pMsg);
	// The tabs of the second row. Two gaps (2 and 9) separate the groups: pictures and fonts, the data of the firmware
	// (strings and the modes of the layout editor), the tweaks.
	enum { TAB_PICTURES = 0, TAB_FONTS = 1, TAB_STRINGS = 2, TAB_LAYOUT = 3, TAB_MENUS = 4, TAB_TYPES = 5,
	       TAB_COMMANDS = 6, TAB_TWEAKS = 7 };       // (TAB_PICTURES is the "Graphics" tab; the "Fonts" tab has the bitmap page and the OpenType page)
	CDarkTabCtrl m_PropsTab;
	void SetFirmware(CFirmware *pFirmware);
	void UpdatePages(BOOL bFonts = FALSE);
	CPicturesDialog m_PicsDialog;
	CFontsDialog m_FontsDialog;
	CStringDialog m_StringDialog;
	COTFDialog m_OTFDialog;
	CLayoutDialog m_LayoutDialog;
	CTweaksDialog m_TweaksDialog;
	BOOL bFonts,bOTFCreate;
	void SetPreviewButton(CWnd *pWnd);
	CWnd *m_pThemePreview;
protected:
	virtual void OnOK();
	CFirmware *m_pFirmware;
public:
	afx_msg void OnTcnSelchangePropTab(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg LRESULT OnSwitchFontSystem(WPARAM wParam, LPARAM lParam);
	afx_msg void OnSize(UINT nType, int cx, int cy);
};