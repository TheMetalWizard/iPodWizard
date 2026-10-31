#pragma once

#include "Firmware.h"
#include "afxwin.h"
#include "afxcmn.h"

// CPrefsDialog dialog

class CPrefsDialog : public CDarkDialog
{
	DECLARE_DYNAMIC(CPrefsDialog)

public:
	CPrefsDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CPrefsDialog();

// Dialog Data
	enum { IDD = IDD_PREFS_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	CString m_Filename;
	CFirmware *	m_pFirmware;
	BOOL m_bTipStartup;
protected:
	virtual void OnOK();
	virtual BOOL PreTranslateMessage(MSG *pMsg);
	CToolTipCtrl m_Tip;
public:
	afx_msg void OnBnClickedChkFontedit();
	afx_msg void OnBnClickedChkForceReflash();
	afx_msg void OnBnClickedChkNoReflash();
	afx_msg void OnBnClickedChkOldResources();
	void SetFirmware(CFirmware *pFirmware);
	afx_msg void OnBnClickedAssociateIpw();
	afx_msg void OnBnClickedAboutButton();
	afx_msg void OnBnClickedChkShowtips();
	afx_msg void OnBnClickedChkReflashImage();
	afx_msg void OnBnClickedChkMetricsystem();
	afx_msg void OnBnClickedChkPicsystem2();
	afx_msg void OnBnClickedIpodhelpButton();
	afx_msg void OnBnClickedThemeidButton();
	afx_msg void OnBnClickedChkAllstrings();
	afx_msg void OnBnClickedChkOtfdraw();
	afx_msg void OnBnClickedChkLayoutdraw();
};