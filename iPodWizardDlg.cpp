// iPodWizardDlg.cpp : implementation file
//

#include "stdafx.h"
#include "iPodWizard.h"
#include "iPodWizardDlg.h"
#include ".\ipodwizarddlg.h"
#include <afxctl.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include "ResourceManager.h"
#include "Picture.h"
#include "ScanDialog.h"
#include "TweaksDialog.h"
#include "HelpDialog.h"
#include "TipDlg.h"

#define BUFSIZE 512

#include <windows.h>
#include <dbt.h>

//Help button
BOOL (WINAPI *_TransparentBlt)(HDC, int, int, int, int, HDC, int, int, int, int, UINT) = NULL;

//#import "Zip.lib"

class MyITunesServiceCallback : public IItunesServiceCallback
{
public:
	CiPodWizardDlg *ipw;
	UINT refCount;
	DWORD cookie;

	MyITunesServiceCallback(CiPodWizardDlg *pipw)
	{
		ipw=pipw;
		refCount = 1;
		cookie = 0;
	}

	STDMETHOD(QueryInterface)(REFIID iid, void** ppvObject)
	{		
		if (iid == IID_IUnknown)
		{
			*ppvObject = static_cast<IUnknown*>(this);
		}
		else if (iid == IID_IDispatch)
		{
			*ppvObject = static_cast<IDispatch*>(this);
		}
		else if (iid == IID_IItunesServiceCallback)
		{
			*ppvObject = static_cast<IItunesServiceCallback*>(this);
		}
		else
		{
			*ppvObject = NULL;
			return E_NOINTERFACE;
		}
		static_cast<IUnknown*>(*ppvObject)->AddRef();
		return S_OK;
	}
	STDMETHOD_(ULONG,AddRef)(void)
	{
		refCount++;
		return S_OK;
	}
	STDMETHOD_(ULONG,Release)(void)
	{
		refCount--;
		if (refCount == 0)
			delete this;
		return S_OK;
	}

	STDMETHOD(GetTypeInfoCount)(UINT* pctinfo)
	{
		return E_FAIL;
	}
	STDMETHOD(GetTypeInfo)(UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo)
	{
		return E_FAIL;
	}
	STDMETHOD(GetIDsOfNames)(const IID &riid, LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID* rgDispId)
	{
		return E_FAIL;
	}
	STDMETHOD(Invoke)(DISPID dispIdMember, const IID &riid, LCID lcid, WORD wFlags,
					  DISPPARAMS *pDispParams, VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
	{
		return E_FAIL;
	}
	STDMETHOD(OnDeviceStateChanged)(long hDevice, EItunesDeviceClass devClass, _EDeviceStatus lNewState )
	{
		ipw->RefreshiPodDrives();
		return S_OK;
	}
    STDMETHOD(OnRequestClientCookie)(unsigned long * pdwCookie)
	{
		if (pdwCookie)
		{
			*pdwCookie = cookie;
			return S_OK;
		}
		else
			return E_FAIL;
	}

};

//Identify iPod
IItunesService *itsv;
IItunesDevices *itdv;
//IItunesServiceCallback *itcb;

//Downgrade
BOOL bEnumSuccess;
int iTotal;

// CAboutDlg dialog used for App About

class CAboutDlg : public CDarkDialog
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	afx_msg void OnLogoClicked();
	void ShowLogo();
	bool m_bOldLogo;

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDarkDialog(CAboutDlg::IDD), m_bOldLogo(false)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDarkDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDarkDialog)
	ON_STN_CLICKED(IDC_ABOUT_ICON, OnLogoClicked)
END_MESSAGE_MAP()

// the logo is shown at 64 x 64 (a picture control would use the 32 x 32 one)
BOOL CAboutDlg::OnInitDialog()
{
	CDarkDialog::OnInitDialog();
	ShowLogo();
	return TRUE;
}

void CAboutDlg::ShowLogo()
{
	HICON hIcon = (HICON)::LoadImage(AfxGetResourceHandle(), MAKEINTRESOURCE(m_bOldLogo ? IDI_OLD_LOGO : IDR_MAINFRAME), IMAGE_ICON, 64, 64, LR_SHARED);
	CWnd *pIcon = GetDlgItem(IDC_ABOUT_ICON);
	if (hIcon != NULL && pIcon != NULL)
		pIcon->SendMessage(STM_SETICON, (WPARAM)hIcon, 0);
}

// a click on the logo turns it into the old one and back (only in this box)
void CAboutDlg::OnLogoClicked()
{
	m_bOldLogo = !m_bOldLogo;
	ShowLogo();
}


// CFirmwarePickDlg — modal dialog for selecting a firmware entry from the updater

class CFirmwarePickDlg : public CDarkDialog
{
public:
	int m_sel;
	CComboBox* m_pCombo;
	CFirmwarePickDlg(CComboBox* pCombo, CWnd* pParent)
		: CDarkDialog(IDD_SELECT_FIRMWARE, pParent), m_sel(-1), m_pCombo(pCombo) {}
protected:
	CListCtrl m_list;
	BOOL OnInitDialog() {
		CDarkDialog::OnInitDialog();
		m_list.SubclassDlgItem(IDC_FIRMLIST, this);
		m_list.SetExtendedStyle(m_list.GetExtendedStyle() | LVS_EX_FULLROWSELECT);
		CRect rc;
		m_list.GetClientRect(&rc);
		int w = rc.Width() - GetSystemMetrics(SM_CXVSCROLL);
		m_list.InsertColumn(0, _T("iPod"), LVCFMT_LEFT, w * 6 / 10);
		m_list.InsertColumn(1, _T("Firmware"), LVCFMT_LEFT, w - w * 6 / 10);
		for (int i = 0; i < m_pCombo->GetCount(); i++) {
			CString s; m_pCombo->GetLBText(i, s);
			CString name = s, fw;
			int p = s.ReverseFind(_T('('));
			if (p >= 0 && s.Right(1) == _T(")")) {
				name = s.Left(p);
				name.TrimRight();
				fw = s.Mid(p + 1, s.GetLength() - p - 2);
				if (fw.Left(1).CompareNoCase(_T("v")) == 0) fw = fw.Mid(1);
			}
			m_list.InsertItem(i, name);
			m_list.SetItemText(i, 1, fw);
		}
		if (m_pCombo->GetCount() > 0) m_list.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		return TRUE;
	}
	void OnOK() { m_sel = m_list.GetNextItem(-1, LVNI_SELECTED); CDarkDialog::OnOK(); }
	DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CFirmwarePickDlg, CDarkDialog)
END_MESSAGE_MAP()


// CiPodWizardDlg dialog



CiPodWizardDlg::CiPodWizardDlg(CWnd* pParent /*=NULL*/)
	: CExDialog(CiPodWizardDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_FirmwareFileName.Empty();
	m_FirmwareiPSWFileName.Empty();
	m_FirmMode = -1;
	m_FirmSourceDriveIdx = -1;
	m_TipFirst = TRUE;
	m_minWidth  = 0;
	m_minHeight = 0;
	m_bAutoLoadFirmware = FALSE;
}

void CiPodWizardDlg::DoDataExchange(CDataExchange* pDX)
{
	CExDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_FIRMWARE_COMBO, m_FirmwareCombo);
	DDX_Control(pDX, IDC_MODE_COMBO, m_EditModeCombo);
	DDX_Control(pDX, IDC_OPT_TAB, m_OptionsTab);
	DDX_Control(pDX, IDC_FIRMWARE_LIST, m_FirmwareList);
	DDX_Control(pDX, IDC_IPODDRIVE_COMBO, m_iPodDriveCombo);
}

BEGIN_MESSAGE_MAP(CiPodWizardDlg, CExDialog)
	ON_WM_CREATE()
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_DESTROY()
	ON_WM_TIMER()
	ON_WM_SIZE()
	ON_WM_GETMINMAXINFO()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(ID_OPEN, OnBnClickedOpen)
	ON_BN_CLICKED(ID_LOAD_FIRMWARE, OnBnClickedLoadFirmware)
	ON_NOTIFY(TCN_SELCHANGE, IDC_OPT_TAB, OnTcnSelchangeOptionTab)
	ON_BN_CLICKED(IDC_WRITE_FIRMWARE_BUTTON, OnBnClickedWriteFirmwareButton)
	ON_BN_CLICKED(IDC_USYSINFO_BUTTON, OnBnClickedUpdateSysInfo)
	//ON_BN_CLICKED(IDC_REFRESHDRV_BUTTON, OnBnClickedRefreshDrives)
	ON_BN_CLICKED(ID_TWEAKS, OnBnClickedTweaks)
	ON_CBN_SELCHANGE(IDC_IPODDRIVE_COMBO, OnCbnSelChangeiPodDriveCombo)
	ON_BN_CLICKED(IDC_LOADIPODFW_BUTTON, OnBnClickedLoadipodfwButton)
	ON_CBN_SELCHANGE(IDC_MODE_COMBO, OnCbnSelchangeModeCombo)
	ON_BN_CLICKED(ID_EJECT_BUTTON, &CiPodWizardDlg::OnBnClickedEjectButton)
	ON_MESSAGE(WM_APP, OniPodEvent)
	ON_MESSAGE(WM_DEVICECHANGE, OnDeviceChange)
	ON_BN_CLICKED(ID_OPEN_FFILE, &CiPodWizardDlg::OnBnClickedOpenFfile)
	ON_BN_CLICKED(IDC_WRITE2IPOD_FIRMWARE_BUTTON, &CiPodWizardDlg::OnBnClickedWrite2ipodFirmwareButton)
	ON_BN_CLICKED(IDC_SAVETOFILE_BUTTON, &CiPodWizardDlg::OnBnClickedSaveToFileButton)
	ON_BN_CLICKED(IDC_SAVETOIPSW_BUTTON, &CiPodWizardDlg::OnBnClickedSaveToIpswButton)
	ON_NOTIFY(NM_RCLICK, IDC_FIRMWARE_LIST, &CiPodWizardDlg::OnLvnItemRightClickedFirmwareList)
END_MESSAGE_MAP()

// CiPodWizardDlg message handlers

void ScanLogMark(const TCHAR *what);	// iPodWizard.cpp

BOOL CiPodWizardDlg::OnInitDialog()
{
	ScanLogMark(_T("OnInitDialog begin"));
	CExDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	//ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	//ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	//initialize modes
	m_EditModeCombo.ResetContent();
	m_EditModeCombo.AddString(_T("iPod"));
	m_EditModeCombo.AddString(_T("Firmware Binary"));
	m_EditModeCombo.AddString(_T("iPodSoftware File"));
	m_EditModeCombo.AddString(_T("Updater"));
	m_EditModeCombo.SetCurSel(0);

	// initialize list
	m_FirmwareList.SetExtendedStyle(m_FirmwareList.GetExtendedStyle()|LVS_EX_FULLROWSELECT);
	m_FirmwareList.InsertColumn(0, TEXT("#"), LVCFMT_LEFT, 20);
	m_FirmwareList.InsertColumn(1, TEXT("Image Checksum"), LVCFMT_LEFT, 95);
	m_FirmwareList.InsertColumn(2, TEXT("Table Checksum"), LVCFMT_LEFT, 95);
	m_FirmwareList.InsertColumn(3, TEXT("Status"), LVCFMT_LEFT, 65);

	//Initialize pages

	CRect rect;

	m_OptionsTab.InsertItem(0, TEXT("Firmware editor"));
	m_OptionsTab.InsertItem(1, TEXT("Firmware extractor"));
	m_OptionsTab.InsertItem(2, TEXT("Updater"));
	m_OptionsTab.InsertItem(3, TEXT("Themes"));
	m_OptionsTab.InsertItem(4, TEXT("Preferences"));

	m_EditorDialog.Create(m_EditorDialog.IDD, &m_OptionsTab);
	m_ThemesDialog.Create(m_ThemesDialog.IDD, &m_OptionsTab);
	m_FWExtractorDialog.Create(m_FWExtractorDialog.IDD, &m_OptionsTab);
	m_UpdaterDialog.Create(m_UpdaterDialog.IDD, &m_OptionsTab);
	m_PrefsDialog.Create(m_PrefsDialog.IDD, &m_OptionsTab);

	m_OptionsTab.GetClientRect(&rect);
	m_OptionsTab.AdjustRect(FALSE, &rect);

	m_EditorDialog.MoveWindow(rect);
	m_ThemesDialog.MoveWindow(rect);
	m_FWExtractorDialog.MoveWindow(rect);
	m_UpdaterDialog.MoveWindow(rect);
	m_PrefsDialog.MoveWindow(rect);

	UpdatePages();

	CString inipath;
	GetAppPath(inipath);
	inipath.AppendFormat(TEXT("\\iPW.ini"));
	theApp.m_IniPath.SetString(inipath);

	// try to read last things
	//
	CWinApp *pApp = AfxGetApp();

	m_Filename = pApp->GetProfileString(TEXT("Main"), TEXT("Updater"));
	if (!m_Filename.IsEmpty())
	{
		OpenUpdater(TRUE);
	}

	int iChosenMode=0;
	iChosenMode=_ttoi(pApp->GetProfileString(TEXT("Main"), TEXT("Mode")));
	m_EditModeCombo.SetCurSel(iChosenMode);
	OnCbnSelchangeModeCombo();

	CString s;
	CString firmware = pApp->GetProfileString(TEXT("Main"), TEXT("Firmware"));
	if (!firmware.IsEmpty())
	{
		for (int i = 0; i < m_FirmwareCombo.GetCount(); i++)
		{
			//m_FirmwareCombo.GetLBText(i, s);
			s=m_FirmwareNames.GetAt(i);
			if (s == firmware)
			{
				m_FirmwareCombo.SetCurSel(i);
				break;
			}
		}
	}

	ScanLogMark(_T("OnInitDialog: controls ready, starting COM"));
	//ipod detector
	CoInitialize(NULL);

	const char * gds={"iTunes"};
	_bstr_t ghh(gds);

	//m_pSink=new MFCSink;
	//m_pSink->SetHandler((CWnd *)this);
	
	//HRESULT gb=CoCreateInstance(CLSID_ItunesService, pUnkOuter,CLSCTX_LOCAL_SERVER,DIID_IItunesService,(void**) &itsv);
	HRESULT gb=CoCreateInstance(CLSID_ItunesService, NULL,CLSCTX_INPROC_SERVER,IID_IItunesService,(void**) &itsv);
	
	if (gb != S_OK)
	{
		itsv=NULL;
		CoUninitialize();
		m_COMFindiPod=FALSE;
		goto refresh;
	}

	m_COMFindiPod=TRUE;
	
	IGlobalInterfaceTable *gpGIT;
	HRESULT csdfdsf=CoCreateInstance(CLSID_StdGlobalInterfaceTable,NULL,CLSCTX_INPROC_SERVER,IID_IGlobalInterfaceTable,(void **)&gpGIT);
	m_dwCookieGlobal=0;
	//ServiceCallbackImp *y=new ServiceCallbackImp;
	MyITunesServiceCallback *itcb=new MyITunesServiceCallback(this);
	HRESULT dfbc=gpGIT->RegisterInterfaceInGlobal((IUnknown *)itcb, IID_IItunesServiceCallback, &m_dwCookieGlobal);
	gpGIT->Release();
	csdfdsf=CoCreateInstance(CLSID_StdGlobalInterfaceTable,NULL,CLSCTX_INPROC_SERVER,IID_IGlobalInterfaceTable,(void **)&gpGIT);
	HRESULT ibb=gpGIT->GetInterfaceFromGlobal(m_dwCookieGlobal, IID_IItunesServiceCallback, (void **)&itcb);

	HRESULT gdf=itsv->Login(4, ghh, (IItunesServiceCallback *)itcb, &m_dwCookie);

	itsv->GetItunesDevices(m_dwCookie, (IItunesDevices **)&itdv);

	HRESULT cvnn=gpGIT->RegisterInterfaceInGlobal((IUnknown *)itdv, IID_IItunesDevices, &m_dwCookieGlobal);
	
	/*LPUNKNOWN m_pUnk=m_pSink->GetIDispatch(FALSE);
	if(!AfxConnectionAdvise(y,IID_IItunesServiceCallback,m_pUnk,FALSE,&m_dwCookieSink))
	{
		itcb=NULL;
		CoUninitialize();
		goto refresh;
	}*/
	
refresh:
	ScanLogMark(_T("OnInitDialog: COM done"));
	SetTimer(1, 50, NULL); // defer disk scan until window is visible
	//

	//check automatic theme loader
	ThemeChecker();

	ScanLogMark(_T("OnInitDialog: end"));
	CRect rc;
	GetWindowRect(&rc);
	m_minWidth  = rc.Width();
	m_minHeight = rc.Height();
	m_Stretch.Init(this);			// the window can be made wider than this; the controls follow

	return TRUE;  // return TRUE  unless you set the focus to a control
}

LRESULT CiPodWizardDlg::OniPodEvent(WPARAM wParam, LPARAM lParam)
{
	if (wParam == NULL || lParam == NULL)
		return 0;
	int mg=(int)wParam;
	if (mg!=14)
		return 0;
	HIPOD ipodid=(HIPOD)lParam;
	RefreshiPodDrives();
	return 0;
}

void CiPodWizardDlg::ThemeChecker()
{
	m_ThemeFirmware.Empty();
	m_ThemeFile.Empty();

	CString m_strCommandLine;
	CString strParam;
	CString strFlag;
	int nParam = 0;
	TCHAR seps1[] = _T("\"");
	TCHAR seps2[] = _T(" ");
	BOOL bRet = FALSE;
	
	strFlag = _T("");
	strParam = _T("");

	m_strCommandLine = ::GetCommandLine();
	m_strCommandLine.TrimLeft();
	m_strCommandLine.TrimRight();
	//MessageBox(m_strCommandLine);
	
	int pos1=0,pos2=0;
	strParam = m_strCommandLine.Tokenize(seps1, pos1);
	strParam = m_strCommandLine.Tokenize(seps1, pos1);
	strFlag = strParam.Tokenize(seps2, pos2);
	if (strFlag.Compare(_TEXT("-theme"))==0)
	{
		strParam = m_strCommandLine.Tokenize(seps1, pos1);
		if (PathFileExists(strParam)==TRUE)
		{
			m_ThemeFile = strParam;

			CFile file;
			if (!file.Open(m_ThemeFile, CFile::modeRead))
			{
				MessageBox(TEXT("Can't open theme file!"));
				return;
			}
			WORD w;
			WORD warr[255];
			int n=0;
			while (TRUE)
			{
				if (file.Read(&w, 2) < 2)
				{
					MessageBox(_T("Error reading theme file!"));
					file.Close();
					return;
				}
				warr[n]=w;
				n++;
				if (w==0)
					break;
			}
			m_ThemeFirmware.Format(TEXT("%s"), (LPCTSTR)warr);
			for (int n=0;n<m_FirmwareNames.GetCount();n++)
			{
				if (m_FirmwareNames.GetAt(n).Compare(m_ThemeFirmware)==0)
				{
					m_FirmwareCombo.SetCurSel(n);
					UpdateData(FALSE);
					file.Close();
					m_bAutoLoadFirmware = TRUE;
					OnBnClickedLoadFirmware();
					m_bAutoLoadFirmware = FALSE;
					return;
				}
			}
			file.Close();
		}
		else
		{
			MessageBox(_TEXT("Theme file not found!"));
		}
	}
}

// The folder of the file based tweaks: iPod_Control\Device of the iPod selected in the iPod Detector (empty when there is none)
void CiPodWizardDlg::UpdateIpodDrive()
{
	theApp.m_iPodDRV.Empty();
	if ((GetDlgItem(IDC_STATIC_NOTFOUND)->GetStyle() & WS_VISIBLE) == 0 && m_iPodDriveCombo.GetCurSel() >= 0)
	{
		CString entry;
		m_iPodDriveCombo.GetLBText(m_iPodDriveCombo.GetCurSel(), entry);
		if (entry.GetLength() >= 2 && entry[1] == _T(':'))
			theApp.m_iPodDRV = entry.Left(2) + _T("\\iPod_Control\\Device");
	}
	m_EditorDialog.m_TweaksDialog.Refresh();
}

int CiPodWizardDlg::CheckiPod(BOOL bSilent)
{
	if (GetDlgItem(IDC_STATIC_NOTFOUND)->IsWindowVisible()==TRUE)
	{
		if (bSilent==FALSE)
			MessageBox(TEXT("No iPod is connected to the computer!"));
		return -1;
	}
	return 0;
	CString temp, path;
	m_iPodDriveCombo.GetLBText(m_iPodDriveCombo.GetCurSel(), temp);
	path.SetString(temp.Mid(0,2));
	path.Append(TEXT("\\iPod_Control\\Device"));
	theApp.m_iPodDRV.SetString(path);
	if (PathFileExists(path)==FALSE)
	{
		if (bSilent==FALSE)
			MessageBox(TEXT("The iPod drive you chose from the list below is not a valid iPod!"));
		return -1;
	}
	return 0;
}

void CiPodWizardDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CExDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.
int CiPodWizardDlg::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CExDialog::OnCreate(lpCreateStruct) == -1)
		return -1;
	return 0;
}

void CiPodWizardDlg::ShowTipAtStartup(void)
{
	// CG: This function added by 'Tip of the Day' component.

	CCommandLineInfo cmdInfo;
	theApp.ParseCommandLine(cmdInfo);
	if (cmdInfo.m_bShowSplash)
	{
		// tips disabled (Preferences): the dialog is not even made, because making it is what creates Tips.txt when there is none
		if (theApp.GetProfileInt(_T("Tip"), _T("StartUp"), 0))
			return;
		CTipDlg dlg;
		if (dlg.m_bStartup)
			dlg.DoModal();
	}

}

void CiPodWizardDlg::ShowTipOfTheDay(void)
{
	// CG: This function added by 'Tip of the Day' component.

	CTipDlg dlg;
	dlg.DoModal();

}

void CiPodWizardDlg::OnDestroy()
{
	if (itsv)
	{
		itsv->Logout(m_dwCookie);
		//LPUNKNOWN m_pUnk=m_pSink->GetIDispatch(FALSE);
		//AfxConnectionUnadvise(itcb,IID_IItunesServiceCallback,m_pUnk,FALSE,m_dwCookieSink);

		//if(m_pSink!=NULL)
		//{
		//	delete m_pSink;
		//	m_pSink=NULL;
			itsv=NULL;
			//itcb=NULL;
			itdv=NULL;
		//}
		CoUninitialize();
	}
	CExDialog::OnDestroy();
}

void CiPodWizardDlg::OnTimer(UINT nIDEvent)
{
	if (nIDEvent == 1)
	{
		KillTimer(1);
		ScanLogMark(_T("first scan timer fired"));
		RefreshiPodDrives();
		return;
	}
	if (nIDEvent == 2)
	{
		KillTimer(2);
		RefreshiPodDrives();
		return;
	}
	CDarkDialog::OnTimer(nIDEvent);
}

LRESULT CiPodWizardDlg::OnDeviceChange(WPARAM wParam, LPARAM lParam)
{
	if (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE)
	{
		// Defer refresh so the OS has time to finish mounting/unmounting the volume
		SetTimer(2, 1000, NULL);
	}
	return TRUE;
}

void CiPodWizardDlg::OnSize(UINT nType, int cx, int cy)
{
	CExDialog::OnSize(nType, cx, cy);
	if (nType == SIZE_MINIMIZED || !IsWindow(m_OptionsTab.GetSafeHwnd()))
		return;

	// Width: the tab and the firmware information box grow with the window, the buttons at the right follow it
	static const int stretchIds[] = { IDC_OPT_TAB, IDC_FIRMINFO_GROUP, IDC_FIRMWARE_LIST };
	int extra = m_Stretch.IsInit() ? cx - m_Stretch.BaseWidth() : 0;
	StretchExtraWidth() = extra;
	m_Stretch.Apply(this, extra, stretchIds, 3, NULL, 0);

	CRect tabRect;
	m_OptionsTab.GetWindowRect(&tabRect);
	ScreenToClient(&tabRect);
	int newTabHeight = cy - tabRect.top - 4;
	m_OptionsTab.SetWindowPos(NULL, tabRect.left, tabRect.top,
	                          tabRect.Width(), newTabHeight,
	                          SWP_NOZORDER | SWP_NOACTIVATE);

	CRect clientRect;
	m_OptionsTab.GetClientRect(&clientRect);
	m_OptionsTab.AdjustRect(FALSE, &clientRect);
	m_EditorDialog.MoveWindow(clientRect);
	m_ThemesDialog.MoveWindow(clientRect);
	m_FWExtractorDialog.MoveWindow(clientRect);
	m_UpdaterDialog.MoveWindow(clientRect);
	m_PrefsDialog.MoveWindow(clientRect);
}

void CiPodWizardDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
	CExDialog::OnGetMinMaxInfo(lpMMI);
	if (m_minWidth == 0)
		return;
	lpMMI->ptMinTrackSize.x = m_minWidth;
	lpMMI->ptMinTrackSize.y = m_minHeight;
}

void CiPodWizardDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else if (m_TipFirst)
	{
		m_TipFirst=FALSE;
		ShowTipAtStartup();
	}
	else
	{
		CExDialog::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CiPodWizardDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CiPodWizardDlg::OnBnClickedOpen()
{
	CFileDialog dlg(TRUE, TEXT("exe"), TEXT("iPod Updater*.exe"), OFN_HIDEREADONLY, TEXT("EXE files (*.exe)|*.exe||"), this);
	MO_LOAD_FIRMWARE_PATH(dlg)

	if (dlg.DoModal() != IDOK)
		return;
	MO_SAVE_FIRMWARE_PATH(dlg)

	m_Filename = dlg.GetPathName();

	OpenUpdater(FALSE);
}

void CiPodWizardDlg::OnBnClickedTweaks()
{
	if (CheckiPod(FALSE)==-1)
		return;
	CTweaksDialog dlg(this);
	INT_PTR nRet = -1;
	nRet = dlg.DoModal();
	switch (nRet)
	{
	case -1:
		MessageBox(TEXT("Could not open tweaks dialog!"));
		break;
	default:
		dlg.DestroyWindow();
		break;
	}
}

/*void CiPodWizardDlg::OnBnClickedRefreshDrives()
{
	RefreshiPodDrives();
}*/

LPCTSTR BSTR2TCHAR(_bstr_t st)
{
	const int cchWideChar = MultiByteToWideChar(CP_ACP, 0, (LPCSTR)st, -1, NULL, 0);
	LPWSTR lpw = new WCHAR [cchWideChar];
	if(lpw == NULL){
		return NULL;
	}
	*lpw = L'\0';

	const int nUnicodeCount = MultiByteToWideChar(CP_ACP, 0, (LPCSTR)st, -1, lpw, cchWideChar);
	if( nUnicodeCount <= 0 ){
		delete[] lpw;
		return NULL;
	}
	return (LPCTSTR)lpw;
}

CString CiPodWizardDlg::GetPhysicalDiskFromDevice(LPCTSTR lpDrive, BOOL macFormatted)
{
	CString ipod_list;
	CString sDrive;
	TCHAR drive[9] = TEXT("\\\\.\\A:"); 
	DWORD returned;
	STORAGE_DEVICE_NUMBER sdn_drive;
	drive[4]=*lpDrive;
	TCHAR szName[12];

	HANDLE hDrive = CreateFile(drive, 0, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);  
	if (hDrive != (HANDLE)-1)  
	{   
		if (DeviceIoControl(hDrive, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, &sdn_drive, sizeof(sdn_drive), &returned, NULL))  
			sdn_drive.PartitionNumber=1;
		else
			sdn_drive.PartitionNumber=0;
		CloseHandle(hDrive);
	}

	for (int y=1;y<10;y++)
	{
		TCHAR devstring[25];
		wsprintf (devstring, TEXT("\\\\.\\PHYSICALDRIVE%i"), y);
		HANDLE h = CreateFile(devstring, 0, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (h != (HANDLE)-1)  
		{  
			STORAGE_DEVICE_NUMBER sdn;  
		
			if (DeviceIoControl(h, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, &sdn, sizeof(sdn), &returned, NULL))  
			{
				if (sdn_drive.DeviceNumber == sdn.DeviceNumber && sdn_drive.DeviceType == sdn.DeviceType)
				{
					if (macFormatted)
					{
							sDrive.Format(TEXT(" [Mac Formatted]"));
					}
					else
					{
						sDrive.Format(TEXT("%s:"), lpDrive);
						if (GetVolumeInformation(sDrive, szName, 12, NULL, NULL, NULL, NULL, NULL))
						{
							if (!_tcscmp(szName, TEXT("")))
								sDrive.Format(TEXT(" No Name"));
							else
								sDrive.Format(TEXT(" %s"), szName);
						}
					}
					ipod_list.Format(TEXT("%s: %s|"), lpDrive, sDrive);
					m_iPodDevices.Add(devstring);
					if (lpDrive)
						delete[] lpDrive;
					CloseHandle(h);
					break;
				}
			}
			CloseHandle(h);
		}
	}
	return ipod_list;
}

// Optional timing log for slow systems: set Main\ScanLog=1 (DWORD) in the registry and the time each iPod scan
// takes, split into its phases, is appended to %TEMP%\iPodWizard-scan.log. Nothing else is changed by this.
static double g_scanFastMs = 0, g_scanRawMs = 0, g_scanVolMs = 0, g_scanT0 = 0;

static double ScanNowMs()
{
	LARGE_INTEGER f, c;
	QueryPerformanceFrequency(&f);
	QueryPerformanceCounter(&c);
	return 1000.0 * (double)c.QuadPart / (double)f.QuadPart;
}

void CiPodWizardDlg::RefreshiPodDrives()
{
	BOOL bLog = theApp.GetProfileInt(_T("Main"), _T("ScanLog"), 0) ? TRUE : FALSE;
	g_scanFastMs = g_scanRawMs = g_scanVolMs = 0;
	g_scanT0 = bLog ? ScanNowMs() : 0;

	RefreshiPodDrivesImpl();

	if (bLog)
	{
		TCHAR path[MAX_PATH];
		if (GetTempPath(MAX_PATH, path))
		{
			_tcscat_s(path, MAX_PATH, _T("iPodWizard-scan.log"));
			FILE *fl = _tfopen(path, _T("a"));
			if (fl)
			{
				SYSTEMTIME st;
				GetLocalTime(&st);
				_ftprintf(fl, _T("%02d:%02d:%02d.%03d  scan: drives %.0f ms, raw disks %.0f ms, volumes %.0f ms, whole refresh %.0f ms; iPods listed: %d (iTunes service: %s)\n"),
				          st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, g_scanFastMs, g_scanRawMs, g_scanVolMs,
				          ScanNowMs() - g_scanT0, m_iPodDriveCombo.GetCount(), m_COMFindiPod ? _T("yes") : _T("no"));
				fclose(fl);
			}
		}
	}
}

void CiPodWizardDlg::RefreshiPodDrivesImpl()
{
	double tPhase = g_scanT0 ? ScanNowMs() : 0;

	//clean combo
	m_iPodDriveCombo.ResetContent();
	m_iPodDevices.RemoveAll();
	m_iPodHandles.RemoveAll();
	CString sDrive;

	CString ipod_list; //ipod drives list

	if (m_COMFindiPod)
	{
		//#########COM WAY##############
		long ipodid=-1;
		int cn=1;
		itdv->FindFirstDevice(&ipodid);

		while (ipodid>0)
		{
			VARIANT_BOOL lMnt;
			itdv->IsMounted(ipodid, &lMnt);
			m_iPodHandles.Add((DWORD)ipodid);
			EFormatState kFormat;
			if (lMnt==FALSE) //check if mounted
			{
				unsigned short ver=0;
				itdv->GetFirmwareVersion(ipodid, &ver);
				//ipod->GetDeviceUniqueID(appid, ipodid)

				//check for mac ipod
				itdv->GetDriveFormat(ipodid, &kFormat);
				if (kFormat==kFormatMac || kFormat==kFormatUnknown || kFormat==kFormatMacNoFirmware)
				{
					_bstr_t abc2("DDSDGFGFDSGFGDF");
					
					itdv->GetDriveLetter(ipodid, abc2.GetAddress());
					
					LPCTSTR lpDrive2=BSTR2TCHAR(abc2);

					ipod_list.Append(GetPhysicalDiskFromDevice(lpDrive2, TRUE));
				}
				else
				{
					ipod_list.AppendFormat(TEXT("Unmounted%d (v%d)|"), cn, ver);
					cn++;
					m_iPodDevices.Add(TEXT(""));
				}
			}
			else if (lMnt==-1)
			{
				itdv->GetDriveFormat(ipodid, &kFormat);
				if (kFormat==kFormatFat32) //only format of fat32 is valid
				{
					//get physical disk
					_bstr_t abc("DDSDGFGFDSGFGDF");
					
					itdv->GetDriveLetter(ipodid, abc.GetAddress());
					
					LPCTSTR lpDrive=BSTR2TCHAR(abc);
					
					ipod_list.Append(GetPhysicalDiskFromDevice(lpDrive, FALSE));
				}
			}
			itdv->FindNextDevice(&ipodid);
		}
		//#########COM WAY##############
	}
	else
	{
		// Fast path: check for iPod_Control directory on any local drive (no admin required)
		{
			DWORD driveMask3 = GetLogicalDrives();
			for (int idx3 = 2; idx3 < 26; idx3++)
			{
				if (!((driveMask3 >> idx3) & 1))
					continue;
				TCHAR rootPath3[5];
				wsprintf(rootPath3, TEXT("%c:\\"), 'A' + idx3);
				UINT dtype3 = GetDriveType(rootPath3);
				// Skip only optical drives and network drives; iPods can appear as
				// DRIVE_FIXED, DRIVE_REMOVABLE, or other types depending on disk-use setting
				if (dtype3 == DRIVE_CDROM || dtype3 == DRIVE_REMOTE || dtype3 == DRIVE_NO_ROOT_DIR)
					continue;
				TCHAR checkPath3[MAX_PATH];
				wsprintf(checkPath3, TEXT("%c:\\iPod_Control"), 'A' + idx3);
				if (GetFileAttributes(checkPath3) != INVALID_FILE_ATTRIBUTES)
				{
					ipod_list.AppendFormat(TEXT("%c:\\|"), 'A' + idx3);
					// Resolve drive letter to physical disk path
					TCHAR volPath3[10];
					wsprintf(volPath3, TEXT("\\\\.\\%c:"), 'A' + idx3);
					HANDLE hVol3 = CreateFile(volPath3, 0, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
					if (hVol3 != INVALID_HANDLE_VALUE) {
						STORAGE_DEVICE_NUMBER sdn3;
						DWORD dwRet3;
						if (DeviceIoControl(hVol3, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, &sdn3, sizeof(sdn3), &dwRet3, NULL)) {
							TCHAR physPath3[30];
							wsprintf(physPath3, TEXT("\\\\.\\PHYSICALDRIVE%d"), sdn3.DeviceNumber);
							m_iPodDevices.Add(physPath3);
						} else {
							m_iPodDevices.Add(TEXT(""));
						}
						CloseHandle(hVol3);
					} else {
						m_iPodDevices.Add(TEXT(""));
					}
				}
			}
			if (!ipod_list.IsEmpty())
			{
				if (ipod_list.Right(1) == TEXT("|"))
					ipod_list.Truncate(ipod_list.GetLength() - 1);
				goto ShowStatus;
			}
		}

		if (g_scanT0) { g_scanFastMs = ScanNowMs() - tPhase; tPhase = ScanNowMs(); }

		//Find the iPod devices (check hidden partition == firmware)
		int x,list[10]={0},y=0;
		for (x = 1; x < 10; x++)
		{
			TCHAR devstring[25];
			int j,dev=-1;

			wsprintf (devstring, TEXT("\\\\.\\PHYSICALDRIVE%i"), x);
			theApp.InitSectorSize(devstring, true);
			dev = _wopen (devstring, O_RDONLY | _O_RAW);

			if (dev == -1)
				continue;

			theApp.InitHDDValues(dev);

			const int scanSize = (theApp.BLOCK_SIZE > 0) ? theApp.BLOCK_SIZE : 512;
			unsigned char *buffer = new unsigned char[scanSize];
			memset(buffer, 0, scanSize);
			_lseek(dev, theApp.FIRMWARE_START, SEEK_SET);
			int nRead = _read(dev, buffer, scanSize);

			for (j = 0; j < nRead - 6; j++)
			{
				if (buffer [j] == 'S' && buffer [j+2] == 'T' && buffer [j+4] == 'O' && buffer [j+6] == 'P')
				{
					if (y < 9)
					{
						y++;
						list[y]=x;
					}
					break;
				}
			}

			delete[] buffer;
			_close(dev);
		}
		if (g_scanT0) { g_scanRawMs = ScanNowMs() - tPhase; tPhase = ScanNowMs(); }
		if (y==0)
			goto ShowStatus;

		//Get all volumes devicenumber/devicetype in order to compare with our ipod devices.
		int idx;
		STORAGE_DEVICE_NUMBER sdn_drives[26];
		memset(sdn_drives, 0xFF, sizeof(sdn_drives));
		TCHAR drive[9] = TEXT("\\\\.\\A:");
	 
		DWORD driveMask = GetLogicalDrives();  
		for(idx = 0; idx < 26; idx++, driveMask >>= 1)  
		{  
			if (idx==0)
				continue;
			BOOL b = (driveMask & 1);   
			if (b)  
			{  
				drive[4] = 'A' + idx;
				HANDLE hDrive = CreateFile(drive, 0, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);  
	 
				if (hDrive != (HANDLE)-1)  
				{   
					DWORD returned2;  

					if (DeviceIoControl(hDrive, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, &sdn_drives[idx], sizeof(sdn_drives[idx]), &returned2, NULL))  
						sdn_drives[idx].PartitionNumber=1;
					else
						sdn_drives[idx].PartitionNumber=0;
					CloseHandle(hDrive);
				}
			}
		}


		if (g_scanT0) { g_scanVolMs = ScanNowMs() - tPhase; }

		//Check what volumes are compatible with the iPod devices we've found.
		for (y=1;y<10;y++)
			if (list[y]!=0)
			{
				TCHAR devstring2[25];
				wsprintf (devstring2, TEXT("\\\\.\\PHYSICALDRIVE%i"), list[y]);
				HANDLE h = CreateFile(devstring2, 0, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);  
		 
				if (h != (HANDLE)-1)  
				{  
					STORAGE_DEVICE_NUMBER sdn;  
					DWORD returned;  

					if (DeviceIoControl(h, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, &sdn, sizeof(sdn), &returned, NULL))  
					{
						for (idx=0;idx<26;idx++)
							if (sdn_drives[idx].DeviceNumber == sdn.DeviceNumber && sdn_drives[idx].DeviceType == sdn.DeviceType)
							{
								ipod_list.AppendFormat(TEXT("%c:|"), 'A' + idx);
								m_iPodDevices.Add(devstring2);
								break;
							}
					}
					CloseHandle(h);
				}
				y++;
			}	
	}

	if (ipod_list.Right(1).Compare(TEXT("|"))==0)
			ipod_list.Truncate(ipod_list.GetLength()-1);

	if (!m_COMFindiPod)
		goto ShowStatus;
	
	//Find all iPods	
	//TCHAR szName[12];
	WORD wlast=0;
	if (ipod_list.IsEmpty()==TRUE)
	{
		GetDlgItem(IDC_IPODDRIVE_COMBO)->ShowWindow(SW_HIDE);
		GetDlgItem(ID_EJECT_BUTTON)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_STATIC_NOTFOUND)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_IPODDETECTOR_GROUP)->SetWindowText(TEXT("iPod Detector"));
		theApp.m_DeviceSel.Empty();
		UpdateIpodDrive();
		UpdateFirmwareButtons();
	}
	else
	{
		GetDlgItem(IDC_IPODDRIVE_COMBO)->ShowWindow(SW_SHOW);
		GetDlgItem(ID_EJECT_BUTTON)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_STATIC_NOTFOUND)->ShowWindow(SW_HIDE);
		GetDlgItem(ID_EJECT_BUTTON)->EnableWindow(TRUE);
		if (ipod_list.Find(TEXT("|"),0)>0)
		{
			WORD i;
			for (i=0;i<ipod_list.GetLength();i++)
				if (ipod_list.Mid(i,1).Compare(TEXT("|"))==0)
				{
					m_iPodDriveCombo.AddString(ipod_list.Mid(wlast, i-wlast));
					wlast=i+1;
				}
			m_iPodDriveCombo.AddString(ipod_list.Mid(wlast, i-wlast));
		}
		else
		{
			m_iPodDriveCombo.AddString(ipod_list);
		}
		sDrive.Format(TEXT("iPod Detector (%d)"), m_iPodDriveCombo.GetCount());
		GetDlgItem(IDC_IPODDETECTOR_GROUP)->SetWindowText(sDrive);
		m_iPodDriveCombo.SetCurSel(0);
		OnCbnSelChangeiPodDriveCombo();
		UpdateFirmwareButtons();
		//add something that if editing ipod firm, make sure the ipod editing isn't out
	}
	return;

ShowStatus:
	//Find all iPods	
	TCHAR szName[12];
	WORD last=0;
	if (ipod_list.IsEmpty()==TRUE)
	{
		GetDlgItem(IDC_IPODDRIVE_COMBO)->ShowWindow(SW_HIDE);
		GetDlgItem(ID_EJECT_BUTTON)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_STATIC_NOTFOUND)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_IPODDETECTOR_GROUP)->SetWindowText(TEXT("iPod Detector"));
		GetDlgItem(IDC_LOADIPODFW_BUTTON)->EnableWindow(FALSE);
		theApp.m_DeviceSel.Empty();
		UpdateIpodDrive();
		UpdateFirmwareButtons();
	}
	else
	{
		GetDlgItem(IDC_IPODDRIVE_COMBO)->ShowWindow(SW_SHOW);
		GetDlgItem(ID_EJECT_BUTTON)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_STATIC_NOTFOUND)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_LOADIPODFW_BUTTON)->EnableWindow(TRUE);
		CString sDrive;
		if (ipod_list.Find(TEXT("|"),0)>0)
		{
			for (WORD i=0;i<ipod_list.GetLength();i++)
				if (ipod_list.Mid(i,1).Compare(TEXT("|"))==0)
				{
					if (GetVolumeInformation(ipod_list.Mid(last, 3), szName, 12, NULL, NULL, NULL, NULL, NULL))
					{
						sDrive.Format(TEXT("%s"),ipod_list.Mid(last, 3));
						if (!_tcscmp(szName, TEXT("")))
							sDrive.AppendFormat(TEXT(" No Name"));
						else
							sDrive.AppendFormat(TEXT(" %s"), szName);
					}
					m_iPodDriveCombo.AddString(sDrive);
					last=i+1;
				}
			if (GetVolumeInformation(ipod_list.Mid(last, 3), szName, 12, NULL, NULL, NULL, NULL, NULL))
			{
				sDrive.Format(TEXT("%s"),ipod_list.Mid(last, 3));
				if (!_tcscmp(szName, TEXT("")))
					sDrive.AppendFormat(TEXT(" No Name"));
				else
					sDrive.AppendFormat(TEXT(" %s"), szName);
			}
			m_iPodDriveCombo.AddString(sDrive);
		}
		else
		{
			if (GetVolumeInformation(ipod_list, szName, 12, NULL, NULL, NULL, NULL, NULL))
				if (!_tcscmp(szName, TEXT("")))
					ipod_list.AppendFormat(TEXT(" No Name"));
				else
					ipod_list.AppendFormat(TEXT(" %s"), szName);
			m_iPodDriveCombo.AddString(ipod_list);
		}
		sDrive.Format(TEXT("iPod Detector (%d)"), m_iPodDriveCombo.GetCount());
		GetDlgItem(IDC_IPODDETECTOR_GROUP)->SetWindowText(sDrive);
		m_iPodDriveCombo.SetCurSel(0);
		theApp.m_DeviceSel=m_iPodDevices.GetAt(m_iPodDriveCombo.GetCurSel());
		UpdateIpodDrive();
		UpdateFirmwareButtons();
	}
}

BOOL CiPodWizardDlg::FirmwareMatchesMode()
{
	// The firmware slot each Edit Mode works on: iPod=1, Firmware Binary=2, iPSW=3, Updater=0 (m_FirmMode values)
	static const int kExpected[4] = {1, 2, 3, 0};
	int mode = m_EditModeCombo.GetCurSel();
	return (mode >= 0 && mode <= 3 && m_FirmMode == kExpected[mode]);
}

void CiPodWizardDlg::UpdateFirmwareButtons()
{
	BOOL match = FirmwareMatchesMode();
	// writing to an iPod needs one connected; the detector shows "No iPod detected" while there is none
	// (the WS_VISIBLE style, so the answer is the same while the window is minimised or not shown yet)
	BOOL iPodFound = (GetDlgItem(IDC_STATIC_NOTFOUND)->GetStyle() & WS_VISIBLE) == 0;
	BOOL writeOK = match;
	if (writeOK && m_EditModeCombo.GetCurSel() == 0)
		writeOK = iPodFound;
	GetDlgItem(IDC_WRITE_FIRMWARE_BUTTON)->EnableWindow(writeOK);
	GetDlgItem(IDC_WRITE2IPOD_FIRMWARE_BUTTON)->EnableWindow(match && iPodFound);
	GetDlgItem(IDC_SAVETOFILE_BUTTON)->EnableWindow(match);
	GetDlgItem(IDC_SAVETOIPSW_BUTTON)->EnableWindow(match);
}

BOOL CiPodWizardDlg::GetSelectedIPod(CString &device, CString &volumeName)
{
	device.Empty();
	volumeName.Empty();

	int sel = m_iPodDriveCombo.GetCurSel();
	if (sel < 0 || sel >= m_iPodDevices.GetSize())
		return FALSE;

	device = m_iPodDevices.GetAt(sel);
	theApp.m_DeviceSel = device;

	CString entry;
	m_iPodDriveCombo.GetLBText(sel, entry);
	if (entry.GetLength() >= 2 && entry[1] == _T(':'))
	{
		TCHAR vol[MAX_PATH + 1] = {0};
		if (GetVolumeInformation(entry.Left(2) + _T("\\"), vol, MAX_PATH + 1, NULL, NULL, NULL, NULL, 0))
			volumeName = vol;
	}
	return !device.IsEmpty();
}

void CiPodWizardDlg::OnCbnSelChangeiPodDriveCombo()
{
	theApp.m_DeviceSel=m_iPodDevices.GetAt(m_iPodDriveCombo.GetCurSel());
	UpdateIpodDrive();
	CString checkmac;
	m_iPodDriveCombo.GetLBText(m_iPodDriveCombo.GetCurSel(), checkmac);
	
	if (checkmac.Right(15).Compare(_T("[Mac Formatted]"))==0)
	{
		GetDlgItem(ID_EJECT_BUTTON)->SetWindowTextW(TEXT("Mount"));
		GetDlgItem(ID_EJECT_BUTTON)->EnableWindow(FALSE);
		GetDlgItem(IDC_LOADIPODFW_BUTTON)->EnableWindow(TRUE);
	}
	else
	{
		if (m_COMFindiPod)
		{
			VARIANT_BOOL lMnt;
			itdv->IsMounted(m_iPodHandles.GetAt(m_iPodDriveCombo.GetCurSel()), &lMnt);
			if (lMnt==FALSE)
			{
				GetDlgItem(ID_EJECT_BUTTON)->SetWindowTextW(TEXT("Mount"));
				GetDlgItem(IDC_LOADIPODFW_BUTTON)->EnableWindow(FALSE);
			}
			else
			{
				GetDlgItem(ID_EJECT_BUTTON)->SetWindowTextW(TEXT("Eject"));
				GetDlgItem(IDC_LOADIPODFW_BUTTON)->EnableWindow(TRUE);
			}
		}
		else
		{
			GetDlgItem(ID_EJECT_BUTTON)->SetWindowTextW(TEXT("Eject"));
			GetDlgItem(IDC_LOADIPODFW_BUTTON)->EnableWindow(TRUE);
		}
	}
}

static BOOL CALLBACK myWinFunc(HWND handle, LPARAM lParam)
{
	//int len=1;
	//len=GetWindowTextLength(handle);
	wchar_t s[14];
	GetWindowText(handle, s, 14);
	wchar_t s2[7];
	GetClassName(handle, s2, 7);
	CString sr(s);
	CString sr2(s2);
	if (sr2.Compare(_T("#32770")) == 0 && sr.Left(13).Compare(_T("iPod Updater ")) == 0)
	{
		EnumChildWindows(handle, myChildWinFunc, 0);
		return FALSE;
	}
	return TRUE;
}

static BOOL CALLBACK myChildWinFunc(HWND handle, LPARAM lParam)
{
	//int len=1;
	//len=GetWindowTextLength(handle);
	wchar_t s[7];
	GetWindowText(handle, s, 7);
	wchar_t s2[7];
	GetClassName(handle, s2, 7);
	CString sr(s);
	CString sr2(s2);
	//if (sr2.Compare(_T("Button")) == 0 && sr.Compare(_T("Update")) == 0)
	if (sr2.Compare(_T("Button")) == 0)
	{
		EnableWindow(handle, TRUE);
		iTotal++;
		if (iTotal==2)
		{
			bEnumSuccess=TRUE;
			MessageBox(0, _T("Successfully reset 'Update' button state!"), _T("Success"), 0);
			return FALSE;
		}
	}
	return TRUE;
}

void CiPodWizardDlg::OnBnClickedUpdateSysInfo()
{
	bEnumSuccess=FALSE;
	iTotal=0;
	MessageBox(
		_T("WARNING: This is for iPod Updater utilities only, which have been replaced by iTunes 7 and up.\n\nEnsure the updater utility is open and has detected your iPod, then click OK."),
		_T("Downgrade Firmware"), MB_OK);
	EnumWindows(myWinFunc, 0);
	if (bEnumSuccess==TRUE)
	{
		MessageBox(_T("iPodWizard will now exit, because you can't update your iPod while iPodWizard is opened. After it is closed, switch to updater window and update as usual."), _T("Note!"), 0);
		PostQuitMessage(0);
	}
	else
	{
		MessageBox(_T("Failed to access the updater.\n\nEnsure the updater is open and has correctly detected your iPod."), _T("Error"), MB_ICONERROR | MB_OK);
	}
	/*
	if (GetDlgItem(IDC_STATIC_NOTFOUND)->IsWindowVisible()==TRUE)
	{
		MessageBox(TEXT("No iPod is connected to the computer!"));
		return;
	}
	CFile file;
	CString temp, path;
	m_iPodDriveCombo.GetLBText(m_iPodDriveCombo.GetCurSel(), temp);
	path.SetString(temp.Mid(0,2));
	path.Append(TEXT("\\iPod_Control\\Device\\SysInfo"));
	if (!file.Open(path, CFile::modeRead ))
	{
		MessageBox(TEXT("Unable to load SysInfo file! Make sure you chose the right drive."));
		return;
	}
	BYTE b;
	DWORD len, i;
	len=(DWORD)file.GetLength();
	char *buffer = new char[len];
	i=0;
	while (TRUE)
	{
		if (file.Read(&b, 1) < 1)
			break;
		buffer[i]=(char)b;
		i++;
	}
	DWORD pos;
	CString s(buffer);
	CString s2;
	CString buildID;
	buildID.Format(TEXT("buildID changes:\nBefore - "));
	for (i=0; i<len; i++)
	{
		if (buffer[i]==10 && (len-i)>11)
		{
			s2.SetString(s.Mid(i+1,11));
			if (s2.Compare(TEXT("buildID: 0x")) == 0)
			{
				pos=i+12;
				buildID.Append(s.Mid(pos-2,10));
				break;
			}
		}
	}
	buffer[pos+3]='0';
	if (buffer[pos+2] > '0')
		buffer[pos+2]--;
	else if (buffer[pos+1] > '0')
	{
		buffer[pos+2]='9';
		buffer[pos+1]--;
	}
	LPWSTR lpszW = new WCHAR[2048];
	int nLen = MultiByteToWideChar(CP_ACP, 0, (LPCSTR)buffer, -1, NULL, NULL);
	MultiByteToWideChar(CP_ACP, 0, (LPCSTR)buffer, -1, lpszW, nLen);
	s.Format(TEXT("%s"),lpszW);
	buildID.AppendFormat(TEXT("\nAfter - %s"), s.Mid(pos-2,10));
	file.Close();
	if (!file.Open(path, CFile::modeWrite))
	{
		MessageBox(TEXT("There was an error accessing the file for writing! Make sure no other program uses it."));
		return;
	}
	file.Flush();
	file.Write(buffer,len);
	file.Close();
	s.Format(TEXT("Your iPod's SysInfo file was changed successfully!\n"));
	s.Append(buildID);
	MessageBox(s, TEXT("Success"));
	*/
}

void CiPodWizardDlg::GetAppPath(CString &app_path)
{
	TCHAR szName[MAX_PATH];
	GetModuleFileName(NULL, szName, MAX_PATH);
	app_path.Format(TEXT("%s"), szName);
	app_path = app_path.Left(app_path.ReverseFind('\\'));
}

void CiPodWizardDlg::OpenUpdater(BOOL firstTime)
{
	INT_PTR numResources;

	GetDlgItem(ID_LOAD_FIRMWARE)->EnableWindow(FALSE);

	if (!m_RsrcMgr.Open(m_Filename))
	{
		DWORD err = GetLastError();
		if (err == 126 && firstTime == TRUE)
			return;
		CString s;
		s.Format(TEXT("Unable to open file! Code=%d"), err);
		MessageBox(s);
		return;
	}

	if (!m_RsrcMgr.Enum(FIRMWARE_RESOURCE_TYPE) || (numResources = m_RsrcMgr.GetNumResources()) == 0)
	{
		MessageBox(TEXT("Unable to find resources!"));
		return;
	}

	m_UpdaterDialog.GenerateList(&m_RsrcMgr, m_Filename);

	// fill combo
	m_FirmwareCombo.ResetContent();
	m_FirmwareNames.RemoveAll();
	TCHAR szTemp[BUFSIZE];

	for (int i = 0; i < numResources; i++)
	{
		if (m_RsrcMgr.GetResourceName(i).Find(TEXT("128."))!=-1 || m_RsrcMgr.GetResourceName(i).Find(TEXT("129."))!=-1)
			continue;
		m_FirmwareNames.Add(m_RsrcMgr.GetResourceName(i));
		if (GetPrivateProfileString(TEXT("FirmwareDisplay"), m_RsrcMgr.GetResourceName(i).Left(m_RsrcMgr.GetResourceName(i).Find(_T("."), m_RsrcMgr.GetResourceName(i).Find(_T(".")) + 1)), NULL, szTemp, BUFSIZE, theApp.m_IniPath)>0)
		{
			CString s(szTemp);
			s.AppendFormat(_T(" (v%s)"), m_RsrcMgr.GetResourceName(i).Right(m_RsrcMgr.GetResourceName(i).GetLength() - 13));
			m_FirmwareCombo.AddString(s);
		}
		else
			m_FirmwareCombo.AddString(m_RsrcMgr.GetResourceName(i));
	}
	m_FirmwareCombo.SetCurSel(0);

	GetDlgItem(ID_LOAD_FIRMWARE)->EnableWindow(TRUE);
}

void CiPodWizardDlg::OnBnClickedLoadFirmware()
{
	CScanDialog dlg;
	CString name;
	theApp.m_BootPicChanged=FALSE;
	if (m_FirmMode==3 && m_EditModeCombo.GetCurSel()==2)
	{
		name.Format(_T("A"));
		dlg.Create(dlg.IDD, this);

		dlg.SendMessage(WM_APP, (WPARAM)TEXT("Extracting iPod Software file..."), 0);
		CZipArchive ipsw;
		ipsw.Open(m_iPSWFile, CZipArchive::zipOpenReadOnly);
		CZipFileHeader finfo;
		if (!ipsw.OpenFile(m_iPSW_FID))
			return;
		if (!ipsw.GetFileInfo(finfo, m_iPSW_FID))
		{
			//error
		}
		dlg.ScanFirmware(NULL, finfo.m_uUncomprSize);
		DWORD iRead;
		DWORD nBufSize = 65535;
		DWORD ptr=0;
		unsigned char *buf=new unsigned char[finfo.m_uUncomprSize];
		CZipAutoBuffer buffer(nBufSize);
		do
		{
			iRead = ipsw.ReadFile(buffer, buffer.GetSize());
			if (iRead)
			{
				memcpy(&buf[ptr], buffer, iRead);
				ptr+=iRead;
				dlg.SendMessage(WM_APP, NULL, ptr);
			}
		}
		while (iRead == buffer.GetSize());

		ipsw.Close();
		//ipsw.ExtractFile(0, app_pth)

		theApp.BLOCK_SIZE=2048;
		if (finfo.m_uUncomprSize%theApp.BLOCK_SIZE!=0)
			theApp.BLOCK_SIZE=512;

		if (!m_Firmware.SetFirmware(name, buf, finfo.m_uUncomprSize))
		{
			MessageBox(TEXT("Unable to allocate memory!"));
			delete buf;
			return;
		}

		delete buf;

		UpdateChecksums();

		//dlg.Create(dlg.IDD, this);
		dlg.ScanFirmware(&m_Firmware);
		dlg.SetWindowTextW(_T("Loading iPodWizard screens..."));

		m_PrefsDialog.SetFirmware(&m_Firmware);
		m_ThemesDialog.SetFirmware(&m_Firmware, &m_EditorDialog);
		m_EditorDialog.SetFirmware(&m_Firmware);

		dlg.DestroyWindow();
		
		GetDlgItem(IDC_WRITE_FIRMWARE_BUTTON)->EnableWindow(TRUE);
		
		GetDlgItem(IDC_SAVETOFILE_BUTTON)->EnableWindow(TRUE);
		GetDlgItem(IDC_WRITE2IPOD_FIRMWARE_BUTTON)->EnableWindow(TRUE);
		UpdateFirmwareButtons();
	}
	else
	{
		m_FirmwareFileName.Empty();
		m_FirmwareiPSWFileName.Empty();

		if (!m_bAutoLoadFirmware)
		{
			CFirmwarePickDlg dlgPick(&m_FirmwareCombo, this);
			if (dlgPick.DoModal() != IDOK || dlgPick.m_sel < 0)
				return;
			m_FirmwareCombo.SetCurSel(dlgPick.m_sel);
		}

		//Load the firmware resource into memory
		name = m_FirmwareNames.GetAt(m_FirmwareCombo.GetCurSel());
		if (!m_RsrcMgr.LoadResource(FIRMWARE_RESOURCE_TYPE, name))
		{
			MessageBox(TEXT("Unable to load resource!"));
			return;
		}

		theApp.BLOCK_SIZE=2048;
		if (m_RsrcMgr.GetResourceSize()%theApp.BLOCK_SIZE!=0)
			theApp.BLOCK_SIZE=512;

		if (!m_Firmware.SetFirmware(name, m_RsrcMgr.GetResourceBuffer(), m_RsrcMgr.GetResourceSize()))
		{
			MessageBox(TEXT("Unable to allocate memory!"));
			return;
		}

		UpdateChecksums();

		dlg.Create(dlg.IDD, this);
		dlg.ScanFirmware(&m_Firmware);

		dlg.SetWindowTextW(_T("Loading iPodWizard screens..."));

		m_PrefsDialog.SetFirmware(&m_Firmware);
		m_ThemesDialog.SetFirmware(&m_Firmware, &m_EditorDialog);
		m_EditorDialog.SetFirmware(&m_Firmware);

		dlg.DestroyWindow();
		
		m_FirmMode=0;
		GetDlgItem(IDC_WRITE_FIRMWARE_BUTTON)->EnableWindow(TRUE);
		GetDlgItem(IDC_SAVETOFILE_BUTTON)->EnableWindow(TRUE);
		GetDlgItem(IDC_WRITE2IPOD_FIRMWARE_BUTTON)->EnableWindow(FALSE);
		UpdateFirmwareButtons();

		{
			CString iniKey = name.Left(name.Find(_T("."), name.Find(_T(".")) + 1));
			TCHAR szModel[BUFSIZE];
			CString sTitle;
			if (GetPrivateProfileString(_T("FirmwareDisplay"), iniKey, NULL, szModel, BUFSIZE, theApp.m_IniPath) > 0)
				sTitle.Format(_T("%s - %s"), szModel, (LPCTSTR)name);
			else
				sTitle.Format(_T("%s"), (LPCTSTR)name);
			GetDlgItem(IDC_FIRMINFO_GROUP)->SetWindowText(sTitle);
		}

		// save settings
		CWinApp *pApp = AfxGetApp();
		pApp->WriteProfileString(TEXT("Main"), TEXT("Updater"), m_Filename);
		pApp->WriteProfileString(TEXT("Main"), TEXT("Firmware"), name);

		//Check if we need to auto load a theme:
		if (m_ThemeFirmware.IsEmpty()==FALSE)
		{
			m_ThemeFirmware.Empty();
			m_ThemesDialog.LoadTheme(m_ThemeFile);
		}
	}
}

void CiPodWizardDlg::OnTcnSelchangeOptionTab(NMHDR *pNMHDR, LRESULT *pResult)
{
	// changes of the font that are not written: ask before the editor is left
	if (m_OptionsTab.GetCurSel() != 0 && m_EditorDialog.m_FontsDialog.IsWindowVisible() && !m_EditorDialog.m_FontsDialog.ConfirmPending(this))
	{
		m_OptionsTab.SetCurSel(0);
		*pResult = 0;
		return;
	}
	UpdatePages();

	*pResult = 0;
}

void CiPodWizardDlg::UpdatePages()
{
	int i = m_OptionsTab.GetCurSel();

	switch (i)
	{
	case 0: //Editor
		m_EditorDialog.ShowWindow(SW_SHOW);
		m_FWExtractorDialog.ShowWindow(SW_HIDE);
		m_UpdaterDialog.ShowWindow(SW_HIDE);
		m_ThemesDialog.ShowWindow(SW_HIDE);
		m_PrefsDialog.ShowWindow(SW_HIDE);
		break;
	case 1: //Firmware extractor
		m_EditorDialog.ShowWindow(SW_HIDE);
		m_FWExtractorDialog.ShowWindow(SW_SHOW);
		m_UpdaterDialog.ShowWindow(SW_HIDE);
		m_ThemesDialog.ShowWindow(SW_HIDE);
		m_PrefsDialog.ShowWindow(SW_HIDE);
		break;
	case 2: //Updater
		m_EditorDialog.ShowWindow(SW_HIDE);
		m_FWExtractorDialog.ShowWindow(SW_HIDE);
		m_UpdaterDialog.ShowWindow(SW_SHOW);
		m_ThemesDialog.ShowWindow(SW_HIDE);
		m_PrefsDialog.ShowWindow(SW_HIDE);
		break;
	case 3: //Themes
		m_EditorDialog.ShowWindow(SW_HIDE);
		m_FWExtractorDialog.ShowWindow(SW_HIDE);
		m_UpdaterDialog.ShowWindow(SW_HIDE);
		m_ThemesDialog.ShowWindow(SW_SHOW);
		m_PrefsDialog.ShowWindow(SW_HIDE);
		break;
	case 4: //Preferences
		m_EditorDialog.ShowWindow(SW_HIDE);
		m_FWExtractorDialog.ShowWindow(SW_HIDE);
		m_UpdaterDialog.ShowWindow(SW_HIDE);
		m_ThemesDialog.ShowWindow(SW_HIDE);
		m_PrefsDialog.ShowWindow(SW_SHOW);
		break;
	}
}

// The firmware is put in the form of the file for a write (sums made, the "dpua" image encrypted again), and kept in it while the function that
// writes runs. When that function ends, however it ends, the form the program works on is put back and the list of the checksums shows it. Left in
// the form of the file, the boot pictures (the type 2 graphics) could no longer be read after a save. The windows that draw from the firmware do not
// draw while it is saved (theApp.m_SavingFirmware); that ends here too, so that a write that stops half way does not leave them blank.
class CWriteForm
{
public:
	CWriteForm(CiPodWizardDlg *dlg, CFirmware &fw) : m_pDlg(dlg), m_fw(fw), m_bOn(true) { m_fw.SyncChecksum(); }
	~CWriteForm() { Release(); }
	void Release()
	{
		if (!m_bOn)
			return;
		m_bOn = false;
		m_fw.RestoreWorkingForm();
		theApp.m_SavingFirmware = FALSE;
		m_pDlg->UpdateChecksums();
	}

private:
	CiPodWizardDlg *m_pDlg;
	CFirmware &m_fw;
	bool m_bOn;
};

// What the iPod does with a changed boot picture: its flash is only written again when the firmware says so (the "flash"
// address of the aupd partition is 0). That is done once per change, for the write that follows: a reset that was made for an
// earlier write is taken back first, or every later write would make the iPod flash again.
// Whether the write that follows makes the iPod flash its update image again. The firmware says it itself (flash byte 0: it does); then
// Preferences > Advanced overwrites that (never / always); otherwise it is done for changed boot pictures, when the preference says so or the
// answer to the question is yes.
BOOL CiPodWizardDlg::DecideReflash(BOOL bToIpod)
{
	m_Firmware.RestoreImageAdresses();           // what an earlier write did to the byte is not the firmware's own
	if (bToIpod && theApp.m_NoReflash)
		return FALSE;
	if (bToIpod && theApp.m_ForceReflash)
		return TRUE;
	if (bToIpod && !m_Firmware.ReflashByteSet())
		return TRUE;
	if (!theApp.m_BootPicChanged)
		return FALSE;
	return theApp.m_ReflashBootImage ||
	    MessageBox(_T("You've changed the boot flash image (boot images).\nDo you want to reflash it so it updates the image upon iPod reset?\nWarning: Make sure you don't disconnect your iPod during the flash update (bar filling)!"), _T("Question"), MB_YESNO) == IDYES;
}

void CiPodWizardDlg::ApplyBootImageReflash(BOOL bToIpod, int reflash)
{
	if (reflash < 0)
		reflash = DecideReflash(bToIpod) ? 1 : 0;
	m_Firmware.RestoreImageAdresses();
	if (reflash)
		m_Firmware.ResetImageAdresses();
	else if (bToIpod && theApp.m_NoReflash)
		m_Firmware.PreventReflash();             // Preferences > Advanced: the reflash byte is overwritten, the iPod never flashes
	theApp.m_BootPicChanged = FALSE;
}

void CiPodWizardDlg::OnBnClickedSaveToFileButton()
{
	if (!FirmwareMatchesMode())
		return;
	if (!m_EditorDialog.m_FontsDialog.ConfirmPending(this))
		return;
	CString name;
	switch (m_FirmMode)
	{
	case 0: name = m_Firmware.GetName(); break;
	case 1: name = m_FirmSourceVolume.IsEmpty() ? CString(_T("iPod")) : m_FirmSourceVolume; break;
	case 2: name = m_FirmwareFileName; break;
	case 3: name = m_FirmwareiPSWFileName; break;
	default: return;
	}
	if (name.IsEmpty())
		name = _T("firmware");
	name.Replace(_T('\\'), _T('_')); name.Replace(_T('/'), _T('_')); name.Replace(_T(':'), _T('_'));
	name.Replace(_T('*'), _T('_'));  name.Replace(_T('?'), _T('_')); name.Replace(_T('"'), _T('_'));
	name.Replace(_T('<'), _T('_'));  name.Replace(_T('>'), _T('_')); name.Replace(_T('|'), _T('_'));
	if (m_FirmMode != 2 && name.Right(4).CompareNoCase(_T(".bin")) != 0)
		name += _T(".bin");

	CFileDialog dlg(FALSE, TEXT("bin"), name, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, TEXT("Binary Files (*.bin)|*.bin|All Files (*.*)|*.*||"), this);
	MO_LOAD_FIRMWARE_PATH(dlg)
	if (dlg.DoModal() != IDOK)
		return;
	MO_SAVE_FIRMWARE_PATH(dlg)

	if (m_Firmware.GetNumOTFFonts() > 0)
	{
		COTFFont font;
		for (DWORD i = 0; i < m_Firmware.GetNumOTFFonts(); i++)
		{
			if (!font.Read(m_Firmware.GetOTFFont(i), FALSE))
			{
				MessageBox(TEXT("One of the OTF fonts is damaged and therefore iPodWizard cannot save the firmware.\nPlease reload the firmware and make the changes again."));
				return;
			}
			font.SyncChecksums();
		}
	}

	ApplyBootImageReflash(FALSE);

	theApp.m_SavingFirmware = TRUE;
	CWriteForm writeForm(this, m_Firmware);
	UpdateChecksums();

	CFile f;
	if (!f.Open(dlg.GetPathName(), CFile::modeCreate | CFile::modeWrite))
	{
		writeForm.Release();
		MessageBox(TEXT("Unable to save the firmware to the selected file."), TEXT("Error"), MB_OK | MB_ICONERROR);
		return;
	}
	f.Write(m_Firmware.GetFirmwareBuffer(), m_Firmware.GetFirmwareSize());
	f.Close();
	theApp.m_SavingFirmware = FALSE;
	writeForm.Release();

	MessageBox(TEXT("Firmware saved to file successfully."), TEXT("Success"));
}

// Puts the firmware that is edited into a copy of the .ipsw that was opened: the manifest and the name and the place of the
// firmware entry stay as the original has them (the manifest names the entry), only the firmware itself is replaced. The .ipsw that was
// opened is not touched. The copy is made beside the final file and moved over it when it is complete and has been read back, so a
// failure leaves no half written file.
BOOL CiPodWizardDlg::SaveFirmwareToIpsw(const CString &destFile, CString &error)
{
	CString tmpIpsw = destFile + _T(".tmp");
	if (!CopyFile(m_iPSWFile, tmpIpsw, FALSE))
	{
		error = _T("Unable to create the file. Make sure the folder can be written to.");
		return FALSE;
	}

	// the firmware goes to a temporary file first (the zip library adds files from disk)
	TCHAR tmpDir[MAX_PATH], tmpFw[MAX_PATH];
	tmpFw[0] = 0;
	if (!GetTempPath(MAX_PATH, tmpDir) || !GetTempFileName(tmpDir, _T("ipw"), 0, tmpFw))
	{
		DeleteFile(tmpIpsw);
		error = _T("Unable to create a temporary file.");
		return FALSE;
	}

	BOOL ok = FALSE;
	CString filen;
	int count = 0;
	try
	{
		CZipArchive ipsw;
		ipsw.Open(tmpIpsw, CZipArchive::zipOpen);
		CZipFileHeader finfo;
		count = (int)ipsw.GetCount(false);
		if (m_iPSW_FID < 0 || m_iPSW_FID >= count || !ipsw.GetFileInfo(finfo, m_iPSW_FID) ||
		    finfo.GetFileName().CompareNoCase(_T("manifest.plist")) == 0)
		{
			error = _T("The firmware could not be found in the iPSW file.");
		}
		else
		{
			filen = finfo.GetFileName();
			CFile f_wr;
			if (!f_wr.Open(tmpFw, CFile::modeCreate | CFile::modeWrite))
				error = _T("Unable to create a temporary file.");
			else
			{
				f_wr.Write(m_Firmware.GetFirmwareBuffer(), m_Firmware.GetFirmwareSize());
				f_wr.Close();

				// replaced where it is and with the name it has (added with just the path, it would be stored under the whole path)
				CZipAddNewFileInfo info(tmpFw, filen);
				info.m_uReplaceIndex = (ZIP_U16_U64)m_iPSW_FID;
				if (ipsw.AddNewFile(info))
					ok = TRUE;
				else
					error = _T("Unable to put the firmware in the iPSW file.");
			}
		}
		ipsw.Close();
	}
	catch (CException *e)
	{
		e->Delete();
		ok = FALSE;
		error = _T("The iPSW file could not be written.");
	}
	DeleteFile(tmpFw);

	if (ok)
	{
		// read back: the firmware entry is where it was, has its name and size, and is intact
		try
		{
			CZipArchive chk;
			chk.Open(tmpIpsw, CZipArchive::zipOpenReadOnly);
			CZipFileHeader h;
			ok = (int)chk.GetCount(false) == count && chk.GetFileInfo(h, m_iPSW_FID) && h.GetFileName() == filen &&
			     (DWORD)h.m_uUncomprSize == m_Firmware.GetFirmwareSize() && chk.TestFile(m_iPSW_FID);
			chk.Close();
		}
		catch (CException *e)
		{
			e->Delete();
			ok = FALSE;
		}
		if (!ok)
			error = _T("The iPSW file did not pass the check after it was written, so it was not saved.");
	}

	if (ok && !MoveFileEx(tmpIpsw, destFile, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
	{
		ok = FALSE;
		error = _T("Unable to write the file. Make sure it is not in use.");
	}
	if (!ok)
		DeleteFile(tmpIpsw);
	return ok;
}

void CiPodWizardDlg::OnBnClickedSaveToIpswButton()
{
	if (m_FirmMode != 3 || !FirmwareMatchesMode())
		return;
	if (!m_EditorDialog.m_FontsDialog.ConfirmPending(this))
		return;

	// the suggested name is a new one, so the .ipsw that was opened is only written over when that is picked on purpose
	CString name = m_iPSWFile.Mid(m_iPSWFile.ReverseFind('\\') + 1);
	if (name.Right(5).CompareNoCase(_T(".ipsw")) == 0)
		name.Truncate(name.GetLength() - 5);
	name += _T("_modified.ipsw");

	CFileDialog dlg(FALSE, TEXT("ipsw"), name, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, TEXT("iPod Software files (*.ipsw)|*.ipsw||"), this);
	MO_LOAD_FIRMWARE_PATH(dlg)
	if (dlg.DoModal() != IDOK)
		return;
	MO_SAVE_FIRMWARE_PATH(dlg)

	if (m_Firmware.GetNumOTFFonts() > 0)
	{
		COTFFont font;
		for (DWORD i = 0; i < m_Firmware.GetNumOTFFonts(); i++)
		{
			if (!font.Read(m_Firmware.GetOTFFont(i), FALSE))
			{
				MessageBox(TEXT("One of the OTF fonts is damaged and therefore iPodWizard cannot save the firmware.\nPlease reload the firmware and make the changes again."));
				return;
			}
			font.SyncChecksums();
		}
	}

	ApplyBootImageReflash(FALSE);

	theApp.m_SavingFirmware = TRUE;
	CWriteForm writeForm(this, m_Firmware);
	UpdateChecksums();

	CString error;
	BOOL ok;
	{
		CWaitCursor wait;
		ok = SaveFirmwareToIpsw(dlg.GetPathName(), error);
	}
	theApp.m_SavingFirmware = FALSE;
	writeForm.Release();

	if (!ok)
	{
		MessageBox(error, TEXT("Error"), MB_OK | MB_ICONERROR);
		return;
	}
	MessageBox(TEXT("Firmware saved to the iPSW file successfully.\nWarning: According to Apple's EULA, uploading firmware to 3rd party websites is prohibited! Use this firmware for personal use only!"), TEXT("Success"));
}

// "This will reflash your iPod." / "This will not reflash your iPod.", centered, with the "will" / "will not" in bold
static int g_WriteReflash = 0;

static void DrawReflashInfo(const DRAWITEMSTRUCT *dis, HWND hDlg)
{
	HFONT normal = (HFONT)SendMessage(hDlg, WM_GETFONT, 0, 0);
	LOGFONT lf;
	GetObject(normal, sizeof(lf), &lf);
	lf.lfWeight = FW_BOLD;
	HFONT bold = CreateFontIndirect(&lf);
	const TCHAR *a = _T("This ");
	const TCHAR *b = g_WriteReflash ? _T("will") : _T("will not");
	const TCHAR *c = _T(" reflash your iPod.");
	SIZE sa, sb, sc;
	HGDIOBJ old = SelectObject(dis->hDC, normal);
	GetTextExtentPoint32(dis->hDC, a, (int)_tcslen(a), &sa);
	GetTextExtentPoint32(dis->hDC, c, (int)_tcslen(c), &sc);
	SelectObject(dis->hDC, bold);
	GetTextExtentPoint32(dis->hDC, b, (int)_tcslen(b), &sb);
	int x = dis->rcItem.left + ((dis->rcItem.right - dis->rcItem.left) - (sa.cx + sb.cx + sc.cx)) / 2;
	int y = dis->rcItem.top + ((dis->rcItem.bottom - dis->rcItem.top) - sa.cy) / 2;
	FillRect(dis->hDC, &dis->rcItem, GetSysColorBrush(COLOR_BTNFACE));
	SetBkMode(dis->hDC, TRANSPARENT);
	SetTextColor(dis->hDC, GetSysColor(COLOR_BTNTEXT));
	SelectObject(dis->hDC, normal);
	TextOut(dis->hDC, x, y, a, (int)_tcslen(a));
	SelectObject(dis->hDC, bold);
	TextOut(dis->hDC, x + sa.cx, y, b, (int)_tcslen(b));
	SelectObject(dis->hDC, normal);
	TextOut(dis->hDC, x + sa.cx + sb.cx, y, c, (int)_tcslen(c));
	SelectObject(dis->hDC, old);
	DeleteObject(bold);
}

static INT_PTR CALLBACK CrossIpodWarnDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_INITDIALOG)                                                  // lParam: 1 = the write flashes the iPod
        g_WriteReflash = lParam ? 1 : 0;
    if (msg == WM_DRAWITEM && wParam == IDC_WRITE_REFLASH_INFO)
    {
        DrawReflashInfo((const DRAWITEMSTRUCT *)lParam, hDlg);
        return TRUE;
    }
    if (msg == WM_COMMAND && (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL))
        EndDialog(hDlg, LOWORD(wParam));
    return FALSE;
}

void CiPodWizardDlg::OnBnClickedWriteFirmwareButton()
{
	if (!FirmwareMatchesMode())
		return;
	if (!m_EditorDialog.m_FontsDialog.ConfirmPending(this))
		return;
	if (m_FirmMode==1)
	{
		if (m_iPodDriveCombo.GetCurSel() != m_FirmSourceDriveIdx)
			if (DialogBox(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDD_CROSS_IPOD_WARNING), m_hWnd, CrossIpodWarnDlgProc) != IDOK)
				return;

		int reflash = DecideReflash(TRUE) ? 1 : 0;
		if (DialogBoxParam(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDD_WRITE_CONFIRM), m_hWnd, CrossIpodWarnDlgProc, (LPARAM)reflash) != IDOK)
			return;

		ApplyBootImageReflash(TRUE, reflash);

		if (m_Firmware.GetNumOTFFonts()>0)
		{
			COTFFont m_Font;
			for (DWORD i=0;i<m_Firmware.GetNumOTFFonts();i++)
			{
				if (!m_Font.Read(m_Firmware.GetOTFFont(i), FALSE))
				{
					MessageBox(TEXT("One of the OTF fonts is damaged and therefore iPodWizard cannot write the update as it may cause damage to iPod.\nPlease reload the firmware and make the changes again."));
					return;
				}
				m_Font.SyncChecksums();
			}
		}

		theApp.m_SavingFirmware=TRUE;

		CWriteForm writeForm(this, m_Firmware);

		UpdateChecksums();

		if (CheckiPod(FALSE)==-1)
			return;

		TCHAR devstring[25];
		wsprintf (devstring, TEXT("%s"), theApp.m_DeviceSel);

		theApp.InitSectorSize(devstring);
		{
			int devInit = _wopen(devstring, O_RDONLY | _O_RAW);
			if (devInit != -1)
			{
				theApp.InitHDDValues(devInit);
				_close(devInit);
			}
		}

		int dev = _wopen (devstring, O_WRONLY | _O_RAW);
		if (dev==-1)
		{
			MessageBox(TEXT("Unable to access iPod. Make sure iPodWizard is running as administrator and iTunes is closed."));
			return;
		}

		DWORD size=m_Firmware.GetFirmwareSize();
		LPBYTE lpBuf=m_Firmware.GetFirmwareBuffer();
		_lseek(dev, theApp.FIRMWARE_START, SEEK_SET);
		
		CScanDialog dlg;
		dlg.Create(dlg.IDD, this);

		dlg.SendMessage(WM_APP, (WPARAM)_T("Writing iPod firmware to disk"), 0);
		dlg.m_ProgressCtrl.SetRange32(0, size);
		dlg.m_ProgressCtrl.SetPos(0);
		/*
		for (int i=0;i<size;i+=theApp.BLOCK_SIZE)
		{
			ret=_write(dev, &lpBuf[i], theApp.BLOCK_SIZE);
			if (ret==-1 && m_UpdatesDone==0)
			{
				_close(dev);
				MessageBox(TEXT("Can't write the modded firmware to your iPod.\r\nYour iPod has been left untouched.\r\nRestarting your computer may help to solve this problem."), TEXT("Error"));
				return;
			}
			else if (ret!=theApp.BLOCK_SIZE)
			{
				_close(dev);
				MessageBox(TEXT("A severe writing error occurred on the iPod and the firmware might be damaged.\nIn order to fix this, you must go into disk mode (see more info on our website www.iPodWizard.net) and restore or update using original Apple updater."), TEXT("Error"));
				return;
			}
			m_UpdatesDone++;
			if (i%(theApp.BLOCK_SIZE*2000)==0)
			{
				dlg.m_ProgressCtrl.SetPos(i);
				dlg.m_ProgressCtrl.UpdateWindow();
			}
		}*/

		DWORD szret=_pdwrite(dev, &lpBuf[0], size, theApp.BLOCK_SIZE);
		if (szret!=size)
		{
			WCHAR title[64];
			wsprintf(title, _T("Error %d"), (int)szret);
			if (szret == (DWORD)-1)
			{
				CString driveLabel;
				m_iPodDriveCombo.GetLBText(m_iPodDriveCombo.GetCurSel(), driveLabel);
				CString msg;
				msg.Format(_T("Failed to write firmware to iPod (%s).\n\nEnsure iTunes is closed, iPodWizard is running as administrator, and no other programs are using the iPod."), (LPCTSTR)driveLabel);
				MessageBox(msg, title, MB_OK | MB_ICONERROR);
			}
			else
				MessageBox(_T("Failed to write firmware to iPod. The firmware may not be compatible with this iPod model."), title, MB_OK | MB_ICONERROR);
			return;
		}

		dlg.m_ProgressCtrl.SetPos(size);
		dlg.m_ProgressCtrl.UpdateWindow();
		_close(dev);
		dlg.DestroyWindow();

		theApp.m_SavingFirmware=FALSE;
		writeForm.Release();

		MessageBox(TEXT("Successfully written to your iPod!\r\n\r\nWarning: According to Apple's EULA, modifying & distributing firmwares is prohibited. Firmwares are without warranty for personal use only."));
	}
	else if (m_FirmMode==0)
	{
		if (MessageBox(TEXT("Are you sure you want to write the modified firmware to the updater?"), TEXT("Warning"), MB_YESNO) != IDYES)
			return;

		//Check for diskspace:
		int f=_wopen(m_Filename, O_RDONLY | _O_RAW);
		__int64	m_uliFreeBytesAvailable,m_uliTotalNumberOfBytes;
		if( f==-1 || !GetDiskFreeSpaceEx(
			m_Filename.Left(3),                  // directory name
			(PULARGE_INTEGER)&m_uliFreeBytesAvailable,         // bytes available to caller
			(PULARGE_INTEGER)&m_uliTotalNumberOfBytes,         // bytes on disk
			NULL) )   // free bytes on disk
		{
			MessageBox(TEXT("Unable to access hard disk!"));
			return;
		}
		
		if (m_uliFreeBytesAvailable < (_filelength(f) + 0xFFFF))
		{
			MessageBox(TEXT("You don't have enough disk space to write the updater file!"));
			return;
		}
		_close(f);

		CString title, title_new;
		GetWindowText(title);
		title_new.Format(TEXT("%s - Writing firmware, please wait..."), title);
		SetWindowText(title_new);

		if (m_Firmware.GetNumOTFFonts()>0)
		{
			COTFFont m_Font;
			for (DWORD i=0;i<m_Firmware.GetNumOTFFonts();i++)
			{
				if (!m_Font.Read(m_Firmware.GetOTFFont(i), FALSE))
				{
					MessageBox(TEXT("One of the OTF fonts is damaged and therefore iPodWizard cannot write the update as it may cause damage to iPod.\nPlease reload the firmware and make the changes again."));
					return;
				}
				m_Font.SyncChecksums();
			}
		}

		theApp.m_SavingFirmware=TRUE;

		CWriteForm writeForm(this, m_Firmware);

		UpdateChecksums();

		BOOL wroteResource = m_RsrcMgr.WriteResource(m_Filename, FIRMWARE_RESOURCE_TYPE, m_Firmware.GetName(), m_Firmware.GetFirmwareBuffer(), m_Firmware.GetFirmwareSize());
		writeForm.Release();
		if (!wroteResource)
		{
			SetWindowText(title);
			MessageBox(TEXT("Unable to write modified firmware!"));
		}
		else
		{
			CString s;
			s.Format(TEXT("1"));
			if (!m_RsrcMgr.Open(m_Filename))
			{
				DWORD err = GetLastError();
				s.Format(TEXT("Unable to reopen file! Code=%d"), err);
			}
			SetWindowText(title);
			MessageBox(TEXT("Updater modified successfully!\nWarning: According to Apple's EULA, uploading firmware to 3rd party websites is prohibited! Use this firmware for personal use only!"), TEXT("Success"));
			if (s.Compare(TEXT("1")))
				MessageBox(s);
		}

		theApp.m_SavingFirmware=FALSE;
	}
	else if (m_FirmMode==2)
	{
		if (MessageBox(TEXT("Are you sure you want to write the modified firmware to the file?"), TEXT("Warning"), MB_YESNO) != IDYES)
			return;

		CString title, title_new;
		GetWindowText(title);
		title_new.Format(TEXT("%s - Writing firmware, please wait..."), title);
		SetWindowText(title_new);

		if (m_Firmware.GetNumOTFFonts()>0)
		{
			COTFFont m_Font;
			for (DWORD i=0;i<m_Firmware.GetNumOTFFonts();i++)
			{
				if (!m_Font.Read(m_Firmware.GetOTFFont(i), FALSE))
				{
					MessageBox(TEXT("One of the OTF fonts is damaged and therefore iPodWizard cannot write the update as it may cause damage to iPod.\nPlease reload the firmware and make the changes again."));
					return;
				}
				m_Font.SyncChecksums();
			}
		}

		ApplyBootImageReflash(FALSE);

		theApp.m_SavingFirmware=TRUE;

		CWriteForm writeForm(this, m_Firmware);

		UpdateChecksums();

		CFile f_wr;
		if (!f_wr.Open(m_FirmwareFile, CFile::modeCreate | CFile::modeWrite))
		{
			//error
			SetWindowText(title);
			theApp.m_SavingFirmware=FALSE;
			writeForm.Release();
			MessageBox(TEXT("Unable to write modified firmware!"));
			return;                                  // (it went on to write to a file that was not open, and said it had worked)
		}
		f_wr.Write(m_Firmware.GetFirmwareBuffer(), m_Firmware.GetFirmwareSize());
		f_wr.Close();

		theApp.m_SavingFirmware=FALSE;
		writeForm.Release();

		SetWindowText(title);
		MessageBox(TEXT("Firmware file modified successfully!\nWarning: According to Apple's EULA, uploading firmware to 3rd party websites is prohibited! Use this firmware for personal use only!"), TEXT("Success"));
	}
}

void CiPodWizardDlg::UpdateChecksums()
{
	DWORD checksum1, checksum2;

	m_FirmwareList.DeleteAllItems();
	CString s;

	for (int i = 0; i < m_Firmware.GetNumPartitions(); i++)
	{
		s.Format(TEXT("%d"), i);
		m_FirmwareList.InsertItem(i, s);

		m_Firmware.GetPartitionChecksums(i, &checksum1, &checksum2);

		s.Format(TEXT("0x%X"), checksum1);
		m_FirmwareList.SetItemText(i, 1, s);
		s.Format(TEXT("0x%X"), checksum2);
		m_FirmwareList.SetItemText(i, 2, s);

		if (checksum1 == checksum2)
		{
			s = "Ok";
		}
		else
		{
			s = "Read-only";
		}
		m_FirmwareList.SetItemText(i, 3, s);
	}
}

void CiPodWizardDlg::OnBnClickedLoadipodfwButton()
{
	if (m_FirmMode == 1 && m_iPodDriveCombo.GetCurSel() != m_FirmSourceDriveIdx)
		if (MessageBox(TEXT("Are you sure you want to load a different firmware?\n\nAny unsaved changes will be lost."), TEXT("Warning"), MB_YESNO) != IDYES)
			return;

	if (CheckiPod(FALSE)==-1)
		return;

	theApp.m_BootPicChanged=FALSE;

	TCHAR devstring[25];
	wsprintf (devstring, TEXT("%s"), theApp.m_DeviceSel);

	theApp.InitSectorSize(devstring);

	int dev = _wopen (devstring, O_RDONLY | _O_RAW);
	if (dev==-1)
	{
		MessageBox(TEXT("Unable to access iPod. Make sure iPodWizard is running as administrator and iTunes is closed."));
		return;
	}

	m_FirmwareFileName.Empty();
	m_FirmwareiPSWFileName.Empty();

ReadStart:
	theApp.InitHDDValues(dev);

	DWORD size=0;
	LPBYTE partitions;
	long pos=theApp.FIRMWARE_START + theApp.PARTITION_MAP_ADDRESS;
	pos%=theApp.BLOCK_SIZE;
	_lseek(dev, theApp.FIRMWARE_START + theApp.PARTITION_MAP_ADDRESS - pos, SEEK_SET);
	partitions = new BYTE[theApp.BLOCK_SIZE*20];
	_read(dev, partitions, theApp.BLOCK_SIZE*20);
	//theApp.CalcPartMapAddr(partitions);
	IPOD_PARTITION_HEADER *	m_pParts;
	int num=0;
	m_pParts = (IPOD_PARTITION_HEADER *)(&partitions[pos]);
	//CFile f;
	//f.Open(_T("C:\\b.bin"), CFile::modeCreate | CFile::modeWrite);
	//f.Write(partitions, theApp.BLOCK_SIZE*20);
	//f.Close();
	//TCHAR aa[500];
	//wsprintf(aa, _T("%d %d %d"), theApp.FIRMWARE_START, theApp.PARTITION_MAP_ADDRESS, theApp.BLOCK_SIZE);
	//wsprintf(aa, _T("%s"), m_pParts[num].type);
	//wsprintf(aa, _T("%d"), b);
	//MessageBox(aa, _T("num"), 0);
	//MessageBoxA(0, m_pParts[num].type, "hi", 0);
	while (memcmp(m_pParts[num].type, ata, 4) == 0 || memcmp(m_pParts[num].type, nand, 4) == 0)
	{
		if (m_pParts[num].devOffset+m_pParts[num].len > size)
			size=m_pParts[num].devOffset+m_pParts[num].len;
		num++;
	}
	delete[] partitions;

	if (num==0)
	{
		if (theApp.BLOCK_SIZE==512)
		{
			theApp.BLOCK_SIZE=2048;
			goto ReadStart;
		}
		_close(dev);
		MessageBox(_T("Error finding firmware images"));
		return;
	}
	
	LPBYTE buffer;
	DWORD i=0;
	buffer=NULL;
	for (i=0;i<size;i+=theApp.BLOCK_SIZE);
	size=i+theApp.BLOCK_SIZE; //extra read
	buffer = new BYTE[size+1];
	if (buffer==NULL)
	{
		_close(dev);
		MessageBox(_T("Couldn't allocate memory!!!"));
		return;
	}
	memset(buffer, 0, size);
	long retpos=_lseek(dev, theApp.FIRMWARE_START, SEEK_SET);
	if (retpos!=theApp.FIRMWARE_START)
	{
		_close(dev);
		MessageBox(_T("Couldn't seek for data!!!"));
		return;
	}

	CScanDialog dlg;
	dlg.Create(dlg.IDD, this);

	dlg.SendMessage(WM_APP, (WPARAM)_T("Reading iPod firmware from disk"), 0);
	dlg.m_ProgressCtrl.SetRange32(0, size);
	dlg.m_ProgressCtrl.SetPos(0);
	//for (i=0;i<size;i+=theApp.BLOCK_SIZE)
	//{
	//	_read(dev, &buffer[i], theApp.BLOCK_SIZE);
	//	if (i%2500==0)
	//		dlg.m_ProgressCtrl.SetPos(i);
	//}
	DWORD szret=_pdread(dev, buffer, size, theApp.BLOCK_SIZE);
	if (szret!=size)
	{
		if (theApp.BLOCK_SIZE==512)
		{
			theApp.BLOCK_SIZE=2048;
			goto ReadStart;
		}
		else
		{
			WCHAR cmsg[200];
			wsprintf(cmsg, _T("Error reading firmware from iPod, error value: %d, %d, %d\nTry rebooting your PC or restoring the iPod (warning: restoring deletes all the iPod contents!)"), szret, errno, size);
			MessageBox(cmsg);
			return;
		}
	}

	theApp.m_LoadingFirmware=TRUE;

	CString name = _T("iPod Firmware");
	if (!m_Firmware.SetFirmware(name, buffer, size))
	{
		delete[] buffer;
		MessageBox(TEXT("Unable to allocate memory!"));
		return;
	}

	delete[] buffer;
	_close(dev);

	UpdateChecksums();

	dlg.ScanFirmware(&m_Firmware);
	dlg.SetWindowTextW(_T("Loading iPodWizard screens..."));

	m_PrefsDialog.SetFirmware(&m_Firmware);
	m_ThemesDialog.SetFirmware(&m_Firmware, &m_EditorDialog);
	m_EditorDialog.SetFirmware(&m_Firmware);

	dlg.DestroyWindow();
	
	m_FirmMode=1;
	m_FirmSourceDriveIdx=m_iPodDriveCombo.GetCurSel();
	{
		CString entry;
		m_iPodDriveCombo.GetLBText(m_FirmSourceDriveIdx, entry);
		TCHAR vol[MAX_PATH+1] = {0};
		m_FirmSourceVolume.Empty();
		if (entry.GetLength() >= 3 && GetVolumeInformation(entry.Left(3), vol, MAX_PATH+1, NULL, NULL, NULL, NULL, 0))
			m_FirmSourceVolume = vol;
	}
	GetDlgItem(IDC_WRITE_FIRMWARE_BUTTON)->EnableWindow(TRUE);
	GetDlgItem(IDC_SAVETOFILE_BUTTON)->EnableWindow(TRUE);
	UpdateFirmwareButtons();

	{
		CString sDrive;
		m_iPodDriveCombo.GetLBText(m_iPodDriveCombo.GetCurSel(), sDrive);
		GetDlgItem(IDC_FIRMINFO_GROUP)->SetWindowText(sDrive);
	}

	theApp.m_LoadingFirmware=FALSE;
}

void CiPodWizardDlg::OnCbnSelchangeModeCombo()
{
	CRect rTop(160,14,206,28), rBot(160,36,206,50);
	CRect rS1(160,8,206,22), rS2(160,24,206,38), rS3(160,40,206,54);          // the three buttons of the iPSW mode
	MapDialogRect(&rTop);
	MapDialogRect(&rBot);
	MapDialogRect(&rS1);
	MapDialogRect(&rS2);
	MapDialogRect(&rS3);

	// "Save to bin" is the Save to File button of the iPSW mode, "Save to ipsw" only shows there
	GetDlgItem(IDC_SAVETOFILE_BUTTON)->SetWindowText(m_EditModeCombo.GetCurSel() == 2 ? _T("Save to bin") : _T("Save to File"));
	GetDlgItem(IDC_SAVETOIPSW_BUTTON)->ShowWindow(m_EditModeCombo.GetCurSel() == 2 ? SW_SHOW : SW_HIDE);

	switch(m_EditModeCombo.GetCurSel())
	{
	case 0: //ipod
		GetDlgItem(IDC_WRITE_FIRMWARE_BUTTON)->SetWindowText(_T("Write to iPod"));
		GetDlgItem(IDC_FIRMWARE_COMBO)->ShowWindow(SW_HIDE);
		GetDlgItem(ID_LOAD_FIRMWARE)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_LOADIPODFW_BUTTON)->ShowWindow(SW_SHOW);
		GetDlgItem(ID_OPEN_FFILE)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_WRITE2IPOD_FIRMWARE_BUTTON)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_WRITE_FIRMWARE_BUTTON)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_SAVETOFILE_BUTTON)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_SAVETOFILE_BUTTON)->MoveWindow(rTop);
		GetDlgItem(IDC_WRITE_FIRMWARE_BUTTON)->MoveWindow(rBot);
		break;
	case 1: //firmware file
		GetDlgItem(ID_OPEN_FFILE)->SetWindowTextW(_T("Open File"));
		GetDlgItem(IDC_LOADIPODFW_BUTTON)->ShowWindow(SW_HIDE);
		GetDlgItem(ID_OPEN_FFILE)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_FIRMWARE_COMBO)->ShowWindow(SW_HIDE);
		GetDlgItem(ID_LOAD_FIRMWARE)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_WRITE_FIRMWARE_BUTTON)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_WRITE2IPOD_FIRMWARE_BUTTON)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_SAVETOFILE_BUTTON)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_SAVETOFILE_BUTTON)->MoveWindow(rTop);
		GetDlgItem(IDC_WRITE2IPOD_FIRMWARE_BUTTON)->MoveWindow(rBot);
		break;
	case 2: //ipsw file
		GetDlgItem(ID_OPEN_FFILE)->SetWindowTextW(_T("Open iPSW"));
		GetDlgItem(IDC_LOADIPODFW_BUTTON)->ShowWindow(SW_HIDE);
		GetDlgItem(ID_OPEN_FFILE)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_FIRMWARE_COMBO)->ShowWindow(SW_HIDE);
		GetDlgItem(ID_LOAD_FIRMWARE)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_WRITE_FIRMWARE_BUTTON)->ShowWindow(SW_HIDE);        // writing into the .ipsw is "Save to ipsw" (a copy), not a Write
		GetDlgItem(IDC_SAVETOFILE_BUTTON)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_WRITE2IPOD_FIRMWARE_BUTTON)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_SAVETOFILE_BUTTON)->MoveWindow(rS1);
		GetDlgItem(IDC_SAVETOIPSW_BUTTON)->MoveWindow(rS2);
		GetDlgItem(IDC_WRITE2IPOD_FIRMWARE_BUTTON)->MoveWindow(rS3);
		break;
	case 3: //updater
		GetDlgItem(IDC_WRITE_FIRMWARE_BUTTON)->SetWindowText(_T("&Write"));
		GetDlgItem(IDC_LOADIPODFW_BUTTON)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_FIRMWARE_COMBO)->ShowWindow(SW_HIDE);
		GetDlgItem(ID_LOAD_FIRMWARE)->ShowWindow(SW_SHOW);
		GetDlgItem(ID_OPEN_FFILE)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_WRITE2IPOD_FIRMWARE_BUTTON)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_WRITE_FIRMWARE_BUTTON)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_SAVETOFILE_BUTTON)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_SAVETOFILE_BUTTON)->MoveWindow(rTop);
		GetDlgItem(IDC_WRITE_FIRMWARE_BUTTON)->MoveWindow(rBot);
		break;
	}
	UpdateFirmwareButtons();

	RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);

	CWinApp *pApp = AfxGetApp();
	CString sMode;
	sMode.Format(_T("%d"), m_EditModeCombo.GetCurSel());
	pApp->WriteProfileString(_T("Main"), _T("Mode"), sMode);
}

void CiPodWizardDlg::OnBnClickedEjectButton()
{
	if (!m_COMFindiPod)
	{
		// Non-COM path: eject via DeviceIoControl
		CString driveText;
		m_iPodDriveCombo.GetLBText(m_iPodDriveCombo.GetCurSel(), driveText);
		if (driveText.GetLength() < 1)
		{
			MessageBox(TEXT("No iPod drive selected."));
			return;
		}
		TCHAR vol[8];
		wsprintf(vol, TEXT("\\\\.\\%c:"), driveText[0]);
		HANDLE h = CreateFile(vol, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE,
		                      NULL, OPEN_EXISTING, 0, NULL);
		if (h == INVALID_HANDLE_VALUE)
		{
			MessageBox(TEXT("Unable to open iPod drive for eject."));
			return;
		}
		DWORD ret;
		DeviceIoControl(h, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &ret, NULL);
		if (!DeviceIoControl(h, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &ret, NULL))
		{
			CloseHandle(h);
			MessageBox(TEXT("Unable to eject iPod. Make sure no programs are using it."));
			return;
		}
		DeviceIoControl(h, IOCTL_STORAGE_EJECT_MEDIA, NULL, 0, NULL, 0, &ret, NULL);
		CloseHandle(h);
		// Auto-dismiss after 5 seconds using the undocumented but universally
		// available MessageBoxTimeout export from user32.dll.
		typedef int (WINAPI *FnMsgBoxTimeout)(HWND, LPCWSTR, LPCWSTR, UINT, WORD, DWORD);
		FnMsgBoxTimeout pMBT = (FnMsgBoxTimeout)GetProcAddress(
			GetModuleHandle(TEXT("user32.dll")), "MessageBoxTimeoutW");
		if (pMBT)
			pMBT(m_hWnd, TEXT("iPod ejected successfully.\n\nThis window will close automatically."), TEXT("iPod Ejected"), MB_OK, 0, 10000);
		else
			MessageBox(TEXT("iPod ejected successfully.\n\nThis window will close automatically."));
		RefreshiPodDrives();
		return;
	}

	long ipodid=m_iPodHandles.GetAt(m_iPodDriveCombo.GetCurSel());
	VARIANT_BOOL lMnt;
	itdv->IsMounted(ipodid, &lMnt);
	if (lMnt==-1)
	{
		//itdv->ReleaseDevice(ipodid);
		itdv->Unmount(ipodid);
	}
	else if (lMnt==FALSE)
	{
		//itdv->RetainDevice(ipodid);
		itdv->Mount(ipodid);
	}
}

void CiPodWizardDlg::OnBnClickedOpenFfile()
{
	CScanDialog dlg;
	CFile f;

	if (m_EditModeCombo.GetCurSel()==1)
	{
		CFileDialog dlg2(TRUE, TEXT("*"), TEXT("*"), OFN_HIDEREADONLY, TEXT("Binary files (*)|*||"), this);
		MO_LOAD_FIRMWARE_PATH(dlg2)

		if (dlg2.DoModal() != IDOK)
			return;
		
		MO_SAVE_FIRMWARE_PATH(dlg2);

		theApp.m_LoadingFirmware = TRUE;
		m_FirmwareFile.SetString(dlg2.GetPathName());

		f.Open(dlg2.GetPathName(), CFile::modeRead);
		unsigned char *buf=new unsigned char[(DWORD)f.GetLength()];
		f.Read(buf, f.GetLength());
		theApp.BLOCK_SIZE = 512;
		CString name(_T("A"));

		if (!m_Firmware.SetFirmware(name, buf, (DWORD)f.GetLength()))
		{
			MessageBox(TEXT("Unable to allocate memory!"));
			theApp.m_LoadingFirmware = FALSE;
			delete buf;
			return;
		}
		delete buf;

		m_FirmwareFileName=dlg2.GetPathName().Right(dlg2.GetPathName().GetLength()-dlg2.GetPathName().ReverseFind('\\')-1);
		GetDlgItem(IDC_FIRMINFO_GROUP)->SetWindowText(m_FirmwareFileName);

		UpdateChecksums();

		dlg.Create(dlg.IDD, this);
		dlg.ScanFirmware(&m_Firmware);
		dlg.SetWindowTextW(_T("Loading iPodWizard screens..."));

		m_PrefsDialog.SetFirmware(&m_Firmware);
		m_ThemesDialog.SetFirmware(&m_Firmware, &m_EditorDialog);
		m_EditorDialog.SetFirmware(&m_Firmware);

		dlg.DestroyWindow();
		
		m_FirmMode=2;
		GetDlgItem(IDC_WRITE_FIRMWARE_BUTTON)->EnableWindow(TRUE);
		GetDlgItem(IDC_SAVETOFILE_BUTTON)->EnableWindow(TRUE);
		GetDlgItem(IDC_WRITE2IPOD_FIRMWARE_BUTTON)->EnableWindow(TRUE);
		UpdateFirmwareButtons();
	}
	else
	{
		CFileDialog dlg3(TRUE, TEXT("*.ipsw"), TEXT("*.ipsw"), OFN_HIDEREADONLY, TEXT("iPod Software files (*.ipsw)|*.ipsw||"), this);
		MO_LOAD_FIRMWARE_PATH(dlg3)

		if (dlg3.DoModal() != IDOK)
			return;
		MO_SAVE_FIRMWARE_PATH(dlg3)

		m_iPSWFile.SetString(dlg3.GetPathName());

		CZipArchive ipsw;
		ipsw.Open(m_iPSWFile, CZipArchive::zipOpenReadOnly);
		CZipFileHeader finfo;
		int file_id=ipsw.FindFile(_T("manifest.plist"));
		if (!ipsw.OpenFile(file_id))
		{
			MessageBox(_T("iPSW file corrupted!"));
			return;
		}
		if (!ipsw.GetFileInfo(finfo, file_id))
		{
			//error
			MessageBox(_T("iPSW file corrupted!"));
			return;
		}
		m_iPSW_FID=1-file_id;
		DWORD iRead;
		DWORD nBufSize = 65535;
		DWORD ptr=0;
		unsigned char *buf=new unsigned char[finfo.m_uUncomprSize];
		CZipAutoBuffer buffer(nBufSize);
		do
		{
			iRead = ipsw.ReadFile(buffer, buffer.GetSize());
			if (iRead)
			{
				memcpy(&buf[ptr], buffer, iRead);
				ptr+=iRead;
			}
		}
		while (iRead == buffer.GetSize());
		ipsw.Close();

		char str[] = "FamilyID</key>\r\n\t<integer>";
		char str2[] = "VisibleBuildID</key>\r\n\t<integer>";
		char *p=(char *)memmem(buf, finfo.m_uUncomprSize, str, strlen(str));
		char *p2=(char *)memmem(buf, finfo.m_uUncomprSize, str2, strlen(str2));
		int i=0;
		if (p && p2)
		{
			p+=strlen(str);
			i=0;
			while (p[i]!='<')
				i++;
			p[i]=0;
			int genid=-1;
			genid=atoi((char *)p);
			
			p2+=strlen(str2);
			i=0;
			while (p2[i]!='<')
				i++;
			p2[i]=0;
			long vbuildid=atoi(p2);
			TCHAR vbuild[20];
			wsprintf(vbuild, _T("%X"), vbuildid);

			CString final_s;
			switch (genid)
			{
			case 1:
				final_s = _T("1/2g iPod v");
				break;
			case 2:
				final_s = _T("3g iPod v");
				break;
			case 3:
				final_s = _T("2g iPod mini v");
				break;
			case 4:
				final_s = _T("4g iPod v");
				break;
			case 5:
				final_s = _T("iPod color v");
				break;
			case 7:
				final_s = _T("1g iPod nano v");
				break;
			case 13:
				final_s = _T("5g iPod v");
				break;
			case 19:
				final_s = _T("2g iPod nano v");
				break;
			default:
				final_s = _T("iPod v");
				break;
			}
			for (i=0;i<(int)_tcslen(vbuild)-4;i++)
			{
				final_s.AppendFormat(_T("%c"), vbuild[i]);
				final_s.AppendFormat(_T("%c"), '.');
			}
			if (final_s.Right(1).Compare(_T("."))==0)
				final_s.Truncate(final_s.GetLength()-1);
			m_FirmwareiPSWFileName=final_s;
			GetDlgItem(IDC_FIRMINFO_GROUP)->SetWindowText(m_FirmwareiPSWFileName);
		}

		delete buf;
		
		
		m_FirmMode=3;
		GetDlgItem(ID_LOAD_FIRMWARE)->EnableWindow(TRUE);

		theApp.m_LoadingFirmware = TRUE;

		OnBnClickedLoadFirmware();
	}
	theApp.m_LoadingFirmware = FALSE;
}

void CiPodWizardDlg::OnBnClickedWrite2ipodFirmwareButton()
{
	if (!FirmwareMatchesMode())
		return;
	if (!m_EditorDialog.m_FontsDialog.ConfirmPending(this))
		return;
	if (m_FirmMode == 1 && m_iPodDriveCombo.GetCurSel() != m_FirmSourceDriveIdx)
		if (MessageBox(TEXT("The firmware was loaded from a different iPod.\nWriting it to this iPod may cause damage if the models are not identical.\nAre you sure you want to continue?"), TEXT("Warning"), MB_YESNO) != IDYES)
			return;

	int reflash = DecideReflash(TRUE) ? 1 : 0;
	if (DialogBoxParam(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDD_WRITE_CONFIRM), m_hWnd, CrossIpodWarnDlgProc, (LPARAM)reflash) != IDOK)
		return;

	ApplyBootImageReflash(TRUE, reflash);

	if (m_Firmware.GetNumOTFFonts()>0)
	{
		COTFFont m_Font;
		for (DWORD i=0;i<m_Firmware.GetNumOTFFonts();i++)
		{
			if (!m_Font.Read(m_Firmware.GetOTFFont(i), FALSE))
			{
				MessageBox(TEXT("One of the OTF fonts is damaged and therefore iPodWizard cannot write the update as it may cause damage to iPod.\nPlease reload the firmware and make the changes again."));
				return;
			}
			m_Font.SyncChecksums();
		}
	}

	theApp.m_SavingFirmware=TRUE;

	CWriteForm writeForm(this, m_Firmware);

	UpdateChecksums();

	if (CheckiPod(FALSE)==-1)
		return;

	TCHAR devstring[25];
	wsprintf (devstring, TEXT("%s"), theApp.m_DeviceSel);
	int dev = _wopen (devstring, O_RDWR | O_RAW);
	if (dev==-1)
	{
		MessageBox(TEXT("Unable to access iPod. Make sure iPodWizard is running as administrator and iTunes is closed."));
		return;
	}

	theApp.InitHDDValues(dev);

	DWORD size=m_Firmware.GetFirmwareSize();
	LPBYTE lpBuf=m_Firmware.GetFirmwareBuffer();
	_lseek(dev, theApp.FIRMWARE_START, SEEK_SET);
	
	CScanDialog dlg;
	dlg.Create(dlg.IDD, this);

	dlg.SendMessage(WM_APP, (WPARAM)_T("Writing iPod firmware to disk"), 0);
	dlg.m_ProgressCtrl.SetRange32(0, size);
	dlg.m_ProgressCtrl.SetPos(0);

	/*for (int i=0;i<size;i+=theApp.BLOCK_SIZE)
	{
		ret=_write(dev, &lpBuf[i], theApp.BLOCK_SIZE);
		if (ret==-1 && m_UpdatesDone==0)
		{
			_close(dev);
			MessageBox(TEXT("Can't write the modded firmware to your iPod.\r\nYour iPod has been left untouched.\r\nRestarting your computer may help to solve this problem."), TEXT("Error"));
			return;
		}
		else if (ret!=theApp.BLOCK_SIZE)
		{
			_close(dev);
			MessageBox(TEXT("A severe writing error occurred on the iPod and the firmware might be damaged.\nIn order to fix this, you must go into disk mode (see more info on our website www.iPodWizard.net) and restore or update using original Apple updater."), TEXT("Error"));
			return;
		}
		m_UpdatesDone++;
		if (i%(theApp.BLOCK_SIZE*2000)==0)
		{
			dlg.m_ProgressCtrl.SetPos(i);
			dlg.m_ProgressCtrl.UpdateWindow();
		}
	}*/

	DWORD szret=_pdwrite(dev, lpBuf, size, theApp.BLOCK_SIZE);
	if (szret!=size)
	{
		int temp=errno;
		WCHAR cc[200];
		wsprintf(cc, _T("Error writing firmware! Code: %d, %d, %d"), szret, temp, size);
		MessageBox(cc);
		return;
	}

	dlg.m_ProgressCtrl.SetPos(size);
	dlg.m_ProgressCtrl.UpdateWindow();
	_close(dev);
	dlg.DestroyWindow();

	theApp.m_SavingFirmware=FALSE;
	writeForm.Release();
	GetDlgItem(IDC_WRITE2IPOD_FIRMWARE_BUTTON)->EnableWindow(FALSE);

	MessageBox(TEXT("Successfully written to your iPod!\r\n\r\nWarning: According to Apple's EULA, modifying & distributing firmwares is prohibited. Firmwares are without warranty for personal use only."));
}

void CiPodWizardDlg::OnLvnItemRightClickedFirmwareList(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

	if (pNMLV->iItem==-1)
		return;
	
	HMENU hMenu = LoadMenu (NULL, MAKEINTRESOURCE (IDR_MENU1));
	HMENU hPopupMenu = GetSubMenu (hMenu, 0);
	POINT pt;
	SetMenuDefaultItem (hPopupMenu, -1, TRUE);
	GetCursorPos (&pt);
	SetForegroundWindow();
	HMENU hSel=(HMENU)TrackPopupMenu (hPopupMenu, 
	  TPM_LEFTALIGN | TPM_RETURNCMD, pt.x, pt.y, 0, this->m_hWnd, NULL);
	if (hSel!=0)
	{
		m_Firmware.FixChecksum(pNMLV->iItem);
		UpdateChecksums();
	}
	SetForegroundWindow();
	DestroyMenu (hPopupMenu);
	DestroyMenu (hMenu);

	*pResult = 0;
}
