#pragma once
#include "afxcmn.h"
#include "Firmware.h"

// CHelpDialog dialog

class CHelpDialog : public CDarkDialog
{
	DECLARE_DYNAMIC(CHelpDialog)

public:
	CHelpDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CHelpDialog();

// Dialog Data
	enum { IDD = IDD_HELP_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	LRESULT OnClose(WPARAM wParam, LPARAM lParam);
private:
	CFont m_BoldFont;
};
