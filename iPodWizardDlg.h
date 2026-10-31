// iPodWizardDlg.h : header file
//

#pragma once 

#include "ResourceManager.h"
#include "Firmware.h"
#include "EditorDialog.h"
#include "ThemesDialog.h"
#include "UpdaterDialog.h"
#include "PrefsDialog.h"
#include "FWExtractorDialog.h"
#include "StretchLayout.h"
#include "afxwin.h"
#include "afxcmn.h"

//#include "MFCSink.h"
#include "Zip\ZipArchive.h"

BOOL CALLBACK myWinFunc(HWND handle, LPARAM lParam);
BOOL CALLBACK myChildWinFunc(HWND handle, LPARAM lParam);
// CiPodWizardDlg dialog
class CiPodWizardDlg : public CDarkDialog
{
protected:
	typedef CDarkDialog CExDialog;
// Construction
public:
	CiPodWizardDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_IPODWIZARD_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

// Implementation
protected:
	HICON m_hIcon;

	CString	m_Filename;
	CResourceManager m_RsrcMgr;
	CFirmware m_Firmware;
	int m_FirmMode;
	int m_FirmSourceDriveIdx;
	CString m_FirmSourceVolume;
	CString m_ThemeFirmware;
	CString m_ThemeFile;
	CString m_iPSWFile;
	int m_iPSW_FID;
	BOOL m_COMFindiPod;
	CString m_FirmwareFile;
	CString m_FirmwareFileName; //file name for presentation purpose
	CString m_FirmwareiPSWFileName;  //file name for presentation purpose
	BOOL m_bAutoLoadFirmware;
	CStretchLayout m_Stretch;              // follows a change of the window width
	int m_minWidth;
	int m_minHeight;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
	void ThemeChecker();
	CZipArchive m_zip;
	DECLARE_MESSAGE_MAP()
private:
	//MFCSink *m_pSink;
	DWORD m_dwCookie;
	//DWORD m_dwCookieSink;
	DWORD m_dwCookieGlobal;
	CString GetPhysicalDiskFromDevice(LPCTSTR lpDrive, BOOL macFormatted);
private:
	void ShowTipAtStartup(void);
	BOOL m_TipFirst;
private:
	void ShowTipOfTheDay(void);
public:
	afx_msg void OnBnClickedOpen();
	void OpenUpdater(BOOL firstTime);
	CComboBox m_EditModeCombo;
	CComboBox m_FirmwareCombo;
	CComboBox m_iPodDriveCombo;
	CStringArray m_FirmwareNames; //non visible
	afx_msg void OnBnClickedLoadFirmware();
	CDarkTabCtrl m_OptionsTab;
	CEditorDialog m_EditorDialog;
	CThemesDialog m_ThemesDialog;
	CFWExtractorDialog m_FWExtractorDialog;
	CUpdaterDialog m_UpdaterDialog;
	CPrefsDialog m_PrefsDialog;
	void UpdatePages();
	afx_msg void OnTcnSelchangeOptionTab(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnClickedWriteFirmwareButton();
	void UpdateChecksums();
	void RefreshiPodDrives();
	void RefreshiPodDrivesImpl();	// the actual scan (RefreshiPodDrives wraps it with an optional timing log)
	BOOL GetSelectedIPod(CString &device, CString &volumeName);
	BOOL FirmwareMatchesMode();
	void UpdateFirmwareButtons();
	BOOL DecideReflash(BOOL bToIpod);                      // will this write make the iPod flash? asks the boot image question when that is needed
	void ApplyBootImageReflash(BOOL bToIpod, int reflash = -1);   // reflash: the answer of DecideReflash, -1 = ask now
	CListCtrl m_FirmwareList;
	CStringArray m_iPodDevices;
	CDWordArray m_iPodHandles;
	afx_msg void OnBnClickedUpdateSysInfo();
	//afx_msg void OnBnClickedRefreshDrives();
	afx_msg void OnBnClickedTweaks();
	void GetAppPath(CString &app_path);
	int CheckiPod(BOOL bSilent=FALSE);
	void UpdateIpodDrive();                                // theApp.m_iPodDRV: iPod_Control\Device of the iPod selected in the detector
	afx_msg void OnCbnSelChangeiPodDriveCombo();
	afx_msg void OnBnClickedLoadipodfwButton();
	afx_msg void OnCbnSelchangeModeCombo();
	afx_msg LRESULT OniPodEvent(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnDeviceChange(WPARAM wParam, LPARAM lParam);
public:
	afx_msg void OnBnClickedEjectButton();
public:
	afx_msg void OnBnClickedOpenFfile();
public:
	afx_msg void OnBnClickedWrite2ipodFirmwareButton();
	afx_msg void OnBnClickedSaveToFileButton();
	afx_msg void OnBnClickedSaveToIpswButton();
	BOOL SaveFirmwareToIpsw(const CString &destFile, CString &error);
public:
	afx_msg void OnLvnItemRightClickedFirmwareList(NMHDR *pNMHDR, LRESULT *pResult);
};

static void *memmem(const void *haystack, size_t haystack_len, const void *needle, size_t needle_len)
{
    const char *begin;
    const char *const last_possible
	= (const char *) haystack + haystack_len - needle_len;

    if (needle_len == 0)
	/* The first occurrence of the empty string is deemed to occur at
	   the beginning of the string.  */
	return (void *) haystack;

    /* Sanity check, otherwise the loop might search through the whole
       memory.  */
    if (haystack_len < needle_len)
		return NULL;

    for (begin = (const char *) haystack; begin <= last_possible; ++begin)
	if (begin[0] == ((const char *) needle)[0] &&
	    !memcmp((const void *) &begin[1],
		    (const void *) ((const char *) needle + 1),
		    needle_len - 1))
	    return (void *) begin;

    return NULL;
}
