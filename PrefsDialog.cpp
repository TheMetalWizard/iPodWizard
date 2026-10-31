#include "stdafx.h"
#include "iPodWizard.h"
#include "PrefsDialog.h"
#include "HelpDialog.h"
#include "cgfiltyp.h"

IMPLEMENT_DYNAMIC(CPrefsDialog, CDarkDialog)

CPrefsDialog::CPrefsDialog(CWnd* pParent /*=NULL*/)
	: CDarkDialog(CPrefsDialog::IDD, pParent)
{
	m_pFirmware = NULL;
}

CPrefsDialog::~CPrefsDialog()
{
}

void CPrefsDialog::DoDataExchange(CDataExchange* pDX)
{
	CDarkDialog::DoDataExchange(pDX);
	DDX_Check(pDX, IDC_CHK_SHOWTIPS, m_bTipStartup);
	DDX_Check(pDX, IDC_CHK_PICSYSTEM2, theApp.m_OldPicIndex);
	DDX_Check(pDX, IDC_CHK_METRICSYSTEM, theApp.m_OldFontMetricsSystem);
	DDX_Check(pDX, IDC_CHK_REFLASHIMAGE, theApp.m_ReflashBootImage);
	DDX_Check(pDX, IDC_CHK_ALLSTRINGS, theApp.m_AllStringsRender);
	DDX_Check(pDX, IDC_CHK_LAYOUTDRAW, theApp.m_LayoutRender);
	DDX_Check(pDX, IDC_CHK_OTFDRAW, theApp.m_OTFRender);
}

BEGIN_MESSAGE_MAP(CPrefsDialog, CDarkDialog)
	ON_BN_CLICKED(IDC_ASSOCIATE_IPW, &CPrefsDialog::OnBnClickedAssociateIpw)
	ON_BN_CLICKED(IDC_ABOUT_BUTTON, &CPrefsDialog::OnBnClickedAboutButton)
	ON_BN_CLICKED(IDC_CHK_SHOWTIPS, &CPrefsDialog::OnBnClickedChkShowtips)
	ON_BN_CLICKED(IDC_CHK_REFLASHIMAGE, &CPrefsDialog::OnBnClickedChkReflashImage)
	ON_BN_CLICKED(IDC_CHK_METRICSYSTEM, &CPrefsDialog::OnBnClickedChkMetricsystem)
	ON_BN_CLICKED(IDC_CHK_PICSYSTEM2, &CPrefsDialog::OnBnClickedChkPicsystem2)
	ON_BN_CLICKED(IDC_IPODHELP_BUTTON, &CPrefsDialog::OnBnClickedIpodhelpButton)
	ON_BN_CLICKED(IDC_THEMEID_BUTTON, &CPrefsDialog::OnBnClickedThemeidButton)
	ON_BN_CLICKED(IDC_CHK_ALLSTRINGS, &CPrefsDialog::OnBnClickedChkAllstrings)
	ON_BN_CLICKED(IDC_CHK_OTFDRAW, &CPrefsDialog::OnBnClickedChkOtfdraw)
	ON_BN_CLICKED(IDC_CHK_LAYOUTDRAW, &CPrefsDialog::OnBnClickedChkLayoutdraw)
	ON_BN_CLICKED(IDC_CHK_FONTEDIT, &CPrefsDialog::OnBnClickedChkFontedit)
	ON_BN_CLICKED(IDC_CHK_FORCEREFLASH, &CPrefsDialog::OnBnClickedChkForceReflash)
	ON_BN_CLICKED(IDC_CHK_NOREFLASH, &CPrefsDialog::OnBnClickedChkNoReflash)
	ON_BN_CLICKED(IDC_CHK_OLDRESOURCES, &CPrefsDialog::OnBnClickedChkOldResources)
END_MESSAGE_MAP()

BOOL CPrefsDialog::PreTranslateMessage(MSG *pMsg)
{
	if (m_Tip.GetSafeHwnd())
		m_Tip.RelayEvent(pMsg);
	return CDarkDialog::PreTranslateMessage(pMsg);
}

void CPrefsDialog::OnBnClickedChkForceReflash()
{
	theApp.m_ForceReflash = IsDlgButtonChecked(IDC_CHK_FORCEREFLASH) ? TRUE : FALSE;
	theApp.WriteProfileInt(_T("Main"), _T("ForceReflash"), theApp.m_ForceReflash);
	if (theApp.m_ForceReflash && theApp.m_NoReflash)          // the two ask for the opposite
	{
		theApp.m_NoReflash = FALSE;
		CheckDlgButton(IDC_CHK_NOREFLASH, FALSE);
		theApp.WriteProfileInt(_T("Main"), _T("NoReflash"), FALSE);
	}
}

void CPrefsDialog::OnBnClickedChkNoReflash()
{
	theApp.m_NoReflash = IsDlgButtonChecked(IDC_CHK_NOREFLASH) ? TRUE : FALSE;
	theApp.WriteProfileInt(_T("Main"), _T("NoReflash"), theApp.m_NoReflash);
	if (theApp.m_NoReflash && theApp.m_ForceReflash)
	{
		theApp.m_ForceReflash = FALSE;
		CheckDlgButton(IDC_CHK_FORCEREFLASH, FALSE);
		theApp.WriteProfileInt(_T("Main"), _T("ForceReflash"), FALSE);
	}
}

void CPrefsDialog::OnBnClickedChkOldResources()
{
	theApp.m_OldResourceSaving = IsDlgButtonChecked(IDC_CHK_OLDRESOURCES) ? TRUE : FALSE;
	theApp.WriteProfileInt(_T("Main"), _T("OldResourceSaving"), theApp.m_OldResourceSaving);
}

void CPrefsDialog::OnBnClickedChkFontedit()
{
	theApp.m_AllowFontEdit = IsDlgButtonChecked(IDC_CHK_FONTEDIT) ? TRUE : FALSE;
	theApp.WriteProfileInt(_T("Main"), _T("AllowFontEdit"), theApp.m_AllowFontEdit);
}

BOOL CPrefsDialog::OnInitDialog()
{
	CDarkDialog::OnInitDialog();

	CheckDlgButton(IDC_CHK_PICSYSTEM2, theApp.m_OldPicIndex);
	CheckDlgButton(IDC_CHK_METRICSYSTEM, theApp.m_OldFontMetricsSystem);
	CheckDlgButton(IDC_CHK_REFLASHIMAGE, theApp.m_ReflashBootImage);
	CheckDlgButton(IDC_CHK_SHOWTIPS, theApp.GetProfileIntW(_T("Tip"), _T("StartUp"), FALSE) ? TRUE : FALSE);      // ticked: tips are disabled (off by default)
	CheckDlgButton(IDC_CHK_OTFDRAW, theApp.m_OTFRender);
	CheckDlgButton(IDC_CHK_LAYOUTDRAW, theApp.m_LayoutRender);
	CheckDlgButton(IDC_CHK_ALLSTRINGS, theApp.m_AllStringsRender);
	CheckDlgButton(IDC_CHK_FONTEDIT, theApp.m_AllowFontEdit);
	CheckDlgButton(IDC_CHK_FORCEREFLASH, theApp.m_ForceReflash);
	CheckDlgButton(IDC_CHK_NOREFLASH, theApp.m_NoReflash);
	CheckDlgButton(IDC_CHK_OLDRESOURCES, theApp.m_OldResourceSaving);

	m_Tip.Create(this, TTS_ALWAYSTIP);
	m_Tip.SetMaxTipWidth(320);
	m_Tip.SetDelayTime(TTDT_AUTOPOP, 30000);
	m_Tip.AddTool(GetDlgItem(IDC_CHK_FONTEDIT), _T("This will allow you to edit the style and size of a font.\r\n\r\nSince fonts are identified in the Layouts tab by their ID, style, and size, you must find and change all layouts which use the font you have changed."));
	m_Tip.AddTool(GetDlgItem(IDC_CHK_FORCEREFLASH), _T("This will force the iPod to be reflashed when written to an iPod."));
	m_Tip.AddTool(GetDlgItem(IDC_CHK_NOREFLASH), _T("The firmware tells the iPod to flash the ROM when the 'flash byte' is set to 0. When checked, the flash byte is set to 1 regardless of the firmware's own setting.\r\n\r\nReflashing is not necessary to write the majority of iPodWizard modifications. Keep this turned off unless needed."));
	m_Tip.AddTool(GetDlgItem(IDC_CHK_OLDRESOURCES), _T("Make Save Resource, Save All, Load Resource and Load All in the Layout tab function as they did in V1.3.0.\r\n\r\nThis is used to make themes which will be compatible with V1.3.0, and is not recommended.\r\n\r\nFont Types: The first copy of an ID is chosen when saving/loading.\r\n\r\nCommand modifications will never be written."));
	m_Tip.Activate(TRUE);

	return TRUE;
}

void CPrefsDialog::OnOK()
{
	// TODO: Add your specialized code here and/or call the base class

	// CDarkDialog::OnOK();
}
void CPrefsDialog::OnBnClickedAboutButton()
{
	CDarkDialog dlg(IDD_ABOUTBOX, this);
	dlg.DoModal();
}

void CPrefsDialog::OnBnClickedAssociateIpw()
{
	CGCFileTypeAccess TheFTA;

	// get full file path to program executable file
	TCHAR	szProgPath[MAX_PATH * 2];
	::GetModuleFileName(NULL, szProgPath, sizeof(szProgPath)/sizeof(TCHAR));

	CString csTempText;

	TheFTA.SetExtension(_TEXT("iPW"));

	// just pass file path in quotes on command line
	//csTempText  = szProgPath;
	//csTempText += _TEXT(" \"%1\"");
	csTempText.Format(_TEXT("\"%s\" -theme %s"),szProgPath,_TEXT("\"%1\""));
	TheFTA.SetShellOpenCommand(csTempText);
	TheFTA.SetDocumentShellOpenCommand(csTempText);

	TheFTA.SetDocumentClassName(_TEXT("iPodWizard.Document"));

	// use first icon in program
	csTempText  = szProgPath;
	csTempText += _TEXT(",0");
	//csTempText.Format(_TEXT("%s%s"),szProgPath,_TEXT(",0"));
	TheFTA.SetDocumentDefaultIcon(csTempText);

	// set the necessary registry entries	
	TheFTA.RegSetAllInfo();

	MessageBox(_TEXT("Associated successfully!"));
}

void CPrefsDialog::OnBnClickedChkShowtips()
{
	UpdateData(TRUE);

	theApp.WriteProfileInt(_T("Tip"), _T("StartUp"), m_bTipStartup ? 1 : 0);      // ticked: "Disable tips"
}

void CPrefsDialog::OnBnClickedChkReflashImage()
{
	UpdateData(TRUE);
	theApp.WriteProfileInt(_T("Main"), _T("ReflashBootImage"), theApp.m_ReflashBootImage);
}

void CPrefsDialog::OnBnClickedChkMetricsystem()
{
	UpdateData(TRUE);
	theApp.WriteProfileInt(_T("Main"), _T("FontMetricSystem"), theApp.m_OldFontMetricsSystem);
}

void CPrefsDialog::OnBnClickedChkPicsystem2()
{
	UpdateData(TRUE);
	theApp.WriteProfileInt(_T("Main"), _T("PicIndexSystem"), theApp.m_OldPicIndex);
}

void CPrefsDialog::OnBnClickedIpodhelpButton()
{
	CHelpDialog dlg(this);
	//if (CheckiPod(TRUE)==-1)
	//	dlg.DisableFind();
	INT_PTR nRet = -1;
	nRet = dlg.DoModal();
	switch (nRet)
	{
	case -1:
		MessageBox(TEXT("Could not open help dialog!"));
		break;
	default:
		dlg.DestroyWindow();
		break;
	}
}

void CPrefsDialog::OnBnClickedThemeidButton()
{
	if (!m_pFirmware)
	{
		MessageBox(_T("Please load a firmware before making a theme id file!"));
		return;
	}

	CFileDialog dlg(FALSE, TEXT(""), TEXT("firmware-id"), OFN_HIDEREADONLY, TEXT("Firmware theme fingerprint|firmware-id||"), this);
	MO_LOAD_RESOURCES_PATH(dlg)

	if (dlg.DoModal() != IDOK)
		return;
	MO_SAVE_RESOURCES_PATH(dlg)

	CFile file;
	if (file.Open(dlg.GetPathName(), CFile::modeCreate | CFile::modeWrite))
	{
		DWORD fm_ver=m_pFirmware->GetFirmwareVersion();
		file.Write(&fm_ver, 4);
		file.Close();
	}
	else
	{
		MessageBox(_T("Unable to save file!"));
	}
}

void CPrefsDialog::OnBnClickedChkAllstrings()
{
	UpdateData(TRUE);
	theApp.WriteProfileInt(_T("Main"), _T("AllStringsRender"), theApp.m_AllStringsRender);
}

void CPrefsDialog::OnBnClickedChkOtfdraw()
{
	UpdateData(TRUE);
	theApp.WriteProfileInt(_T("Main"), _T("OTFRender"), theApp.m_OTFRender);
}

void CPrefsDialog::OnBnClickedChkLayoutdraw()
{
	UpdateData(TRUE);
	theApp.WriteProfileInt(_T("Main"), _T("LayoutRender"), theApp.m_LayoutRender);
}

void CPrefsDialog::SetFirmware(CFirmware *pFirmware)
{
	m_pFirmware=pFirmware;
}
