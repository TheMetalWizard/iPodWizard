#pragma once

#include "Firmware.h"
#include "afxwin.h"
#include <Afxtempl.h>
#include "HexEditor.h"
#include "HScrollListBox.h"
#include "StretchLayout.h"
#include "CueEdit.h"

struct STRING_CHANGE
{
	CWordArray b_text;
	DWORD offset;
};

// CStringDialog dialog

class CStringDialog : public CDarkDialog
{
	DECLARE_DYNAMIC(CStringDialog)

public:
	CStringDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CStringDialog();

// Dialog Data
	enum { IDD = IDD_STRINGS_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	void SaveAllStrings(CFile *fileb);
	virtual BOOL OnInitDialog();
	CFirmware * m_pFirmware;
	void SetFirmware(CFirmware *pFirmware);
	CHScrollListBox m_StringList;
	CListCtrl m_LangStringList;
	CString m_FindString;
	CString m_ChangeString;
	CComboBox m_Langidx;
	CCueEdit m_ChangeEdit;                 // the box of Change String: the selected string shows in it in grey
	void RefreshChangeCue();               // the selected string, as text or as hex bytes (whichever is picked)
	afx_msg void OnBnClickedHexString();
	CCueEdit m_AddrEdit;                   // the box of Hex GoTo: the current offset of the hex view shows in it in grey
	DWORD m_AddrCueShown;
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDestroy();
	DWORD m_HexAddr;
	CDWordArray sfilter_w;
	BOOL m_bSFilter;
	BOOL m_bShowAll;
	int m_Lang;
	afx_msg void OnBnClickedFindButton();
	afx_msg void OnEnChangeFindEdit();
	afx_msg void OnEnChangeChangeEdit();
	afx_msg void OnEnChangeAddrEdit();
	virtual BOOL PreTranslateMessage(MSG *pMsg);
	CHexWnd m_HexWnd;
	CStretchLayout m_Stretch;              // follows a change of the window width
	int m_hexBottom;
	int m_listTop;
	int m_listHeight;
	int m_listHeightMin;
	int m_splitterGap;
	int m_idxComboClosedH;
	bool m_dragging;
	int m_dragStartY;
	int m_dragStartListHeight;
	int  EffectiveListHeight();
	void LayoutSplitter();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
protected:
	virtual void OnOK();
public:
	afx_msg void OnLbnSelchangeStringList();
	afx_msg void OnBnClickedFindnextButton();
	afx_msg void OnBnClickedChangeTextButton();
	afx_msg void OnBnClickedCheckTextButton();
	afx_msg void OnBnClickedSaveAllChangesButton();
	afx_msg void OnBnClickedLoadChangesButton();
	afx_msg void OnBnClickedJumpToButton();
	afx_msg void OnBnClickedStringFilterButton();
	void FindString(int start);
	BOOL LoadStringChanges(CFile *pFile);
	BOOL LoadLanguageBlock(CFile *pFile, DWORD filelen, DWORD lang=-1);
	STRING_CHANGE* templ;
	CArray<STRING_CHANGE*,STRING_CHANGE*> m_StringChanges;
	LPCTSTR GetString(DWORD lang, DWORD index);
	DWORD GetNumStrings(DWORD lang);
	DWORD GetStringOffset(DWORD index);
	afx_msg void OnCbnSelchangeLangidxCombo();
	afx_msg void OnLvnItemchangedLangstringList(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnClickedTranslate();
	afx_msg void OnCbnDropdownLangidxCombo();
	afx_msg void OnCbnCloseupLangidxCombo();
};
