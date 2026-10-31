#pragma once
#include "Firmware.h"

class CFWExtractorDialog : public CDarkDialog
{
	DECLARE_DYNAMIC(CFWExtractorDialog)

public:
	CFWExtractorDialog(CWnd* pParent = NULL);
	virtual ~CFWExtractorDialog();

	enum { IDD = IDD_FWEXTRACTOR_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	DECLARE_MESSAGE_MAP()

protected:
	virtual void OnOK() {}

public:
	afx_msg void OnBnClickedExtractiPodFirmware();
};
