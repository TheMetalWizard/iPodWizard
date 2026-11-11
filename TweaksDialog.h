#pragma once
#include "afxcmn.h"
#include "Firmware.h"

class CLayoutDialog;

// CTweaksDialog dialog

class CTweaksDialog : public CDarkDialog
{
	DECLARE_DYNAMIC(CTweaksDialog)

public:
	CTweaksDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CTweaksDialog();

// Dialog Data
	enum { IDD = IDD_TWEAKS_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedApply();
	void Refresh();
	void SetFirmware(CFirmware *pFirmware) { m_pFirmware = pFirmware; }
	void SetLayoutDialog(CLayoutDialog *pLayout) { m_pLayout = pLayout; }   // "Change Selector Bar Color" works on the layouts of the firmware: the routine is the layout dialog's
	afx_msg void OnBnClickedSelectorBar();
	afx_msg void OnBnClickedClickPitch();
	afx_msg void OnBnClickedLcdInvert();
	afx_msg void OnBnClickedFlipReflash();
	afx_msg void OnBnClickedVolumeHackButton();
	virtual BOOL PreTranslateMessage(MSG *pMsg);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDestroy();

private:
	CFirmware *m_pFirmware;
	CLayoutDialog *m_pLayout;
	CToolTipCtrl m_Tip;
	CString m_lastDrive;                 // what the boxes show: the iPod folder and which tweak files it had at the last look
	BOOL m_lastOK;
	int m_lastState[5];
	void PollTweakFiles();
};
