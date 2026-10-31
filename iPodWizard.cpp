// iPodWizard.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "iPodWizard.h"
#include "iPodWizardDlg.h"
#include <wincred.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CiPodWizardApp

BEGIN_MESSAGE_MAP(CiPodWizardApp, CWinApp)
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()


// CiPodWizardApp construction

CiPodWizardApp::CiPodWizardApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
	m_bAdminPrivilege=FALSE;
}


// The one and only CiPodWizardApp object

CiPodWizardApp theApp;


// CiPodWizardApp initialization

// Start-up timing marks for the optional scan log (registry Main\ScanLog=1): each line says
// how long after the process was created it was written.
void ScanLogMark(const TCHAR *what)
{
	if (!theApp.GetProfileInt(_T("Main"), _T("ScanLog"), 0))
		return;
	FILETIME ftCreate, ftExit, ftKernel, ftUser, ftNow;
	GetProcessTimes(GetCurrentProcess(), &ftCreate, &ftExit, &ftKernel, &ftUser);
	GetSystemTimeAsFileTime(&ftNow);
	ULARGE_INTEGER c, n;
	c.LowPart = ftCreate.dwLowDateTime; c.HighPart = ftCreate.dwHighDateTime;
	n.LowPart = ftNow.dwLowDateTime;    n.HighPart = ftNow.dwHighDateTime;
	double sinceStart = (double)(n.QuadPart - c.QuadPart) / 10000.0;
	double cpuMs = (double)(((ULARGE_INTEGER*)&ftKernel)->QuadPart + ((ULARGE_INTEGER*)&ftUser)->QuadPart) / 10000.0;
	TCHAR path[MAX_PATH];
	if (!GetTempPath(MAX_PATH, path))
		return;
	_tcscat_s(path, MAX_PATH, _T("iPodWizard-scan.log"));
	FILE *fl = _tfopen(path, _T("a"));
	if (!fl)
		return;
	SYSTEMTIME st;
	GetLocalTime(&st);
	_ftprintf(fl, _T("%02d:%02d:%02d.%03d  start-up: %s, %.0f ms after process start (cpu used %.0f ms)\n"),
	          st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, what, sinceStart, cpuMs);
	fclose(fl);
}

#ifdef IPW_TESTHOOK
// test builds only: where an access violation happens (module base + offset), for the map file
static LONG CALLBACK TestCrashLogger(PEXCEPTION_POINTERS p)
{
	DWORD c = p->ExceptionRecord->ExceptionCode;
	if (c == 0xC0000005 || c == 0xC0000094 || c == 0xC00000FD || c == 0xC0000374)
	{
		TCHAR path[MAX_PATH];
		GetTempPath(MAX_PATH, path);
		_tcscat(path, _T("ipw_crash.txt"));
		FILE *f = _tfopen(path, _T("a"));
		if (f)
		{
			HMODULE m = GetModuleHandle(NULL);
			void *frames[24];
			typedef USHORT (WINAPI *PCapture)(ULONG, ULONG, PVOID *, PULONG);
			PCapture cap = (PCapture)GetProcAddress(GetModuleHandle(_T("ntdll.dll")), "RtlCaptureStackBackTrace");
			int n = cap ? cap(0, 24, frames, NULL) : 0;
			_ftprintf(f, _T("exception %08X at +%08X (base %p) access %d address %p\n"), c,
			          (unsigned)((BYTE *)p->ExceptionRecord->ExceptionAddress - (BYTE *)m), m,
			          p->ExceptionRecord->NumberParameters > 0 ? (int)p->ExceptionRecord->ExceptionInformation[0] : -1,
			          p->ExceptionRecord->NumberParameters > 1 ? (void *)p->ExceptionRecord->ExceptionInformation[1] : NULL);
			for (int i = 0; i < n; i++)
				_ftprintf(f, _T("   +%08X\n"), (unsigned)((BYTE *)frames[i] - (BYTE *)m));
			fclose(f);
		}
	}
	return EXCEPTION_CONTINUE_SEARCH;
}
#endif

BOOL CiPodWizardApp::InitInstance()
{
#ifdef IPW_TESTHOOK
	{
		typedef PVOID (WINAPI *PAdd)(ULONG, PVECTORED_EXCEPTION_HANDLER);
		PAdd add = (PAdd)GetProcAddress(GetModuleHandle(_T("kernel32.dll")), "AddVectoredExceptionHandler");
		if (add)
			add(1, TestCrashLogger);
	}
#endif
	// InitCommonControls() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	InitCommonControls();

	//In case you use MFC shared DLL, uncomment those
	//HWND p_WindowHandle = CreateWindowEx(0, "STATIC", "IPW_TEMP", 0, 0, 0, 0, 0, 0, 0, m_hInstance, 0);
	//DestroyWindow(p_WindowHandle);

	CWinApp::InitInstance();

	AfxEnableControlContainer();

	LoadStdProfileSettings();  // Load standard INI file options (including MRU)

	// Parse command line for standard shell commands, DDE, file open
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need
	// Change the registry key under which our settings are stored
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization
	SetRegistryKey(_T("iPodWizard"));

	ScanLogMark(_T("InitInstance reached"));
	LoadPreferences();
	DarkMode::Init();	// after the profile is available, before any window is created

	//Check for .NET Framework existance:
	LPWSTR lpw = new WCHAR [MAX_PATH];
	GetSystemDirectory(lpw, MAX_PATH);
	CString sPath(lpw);
	sPath.AppendFormat(TEXT("\\gdi32.dll"));
	if (PathFileExists(sPath)==FALSE)
	{
		if (MessageBox(0, TEXT("Your computer doesn't have .NET Framework v1.0+ installed.\nYou need this piece of software in order for iPodWizard to work.\nPlease download and install it from Windows Update website.\nAlternatively, if you click yes, an Internet Explorer will pop up."), TEXT("Error"), MB_YESNO) == IDYES)
		{
			HINSTANCE result = ShellExecute(NULL, _T("open"), TEXT("http://www.microsoft.com/downloads/details.aspx?familyid=0856eacb-4362-4b0d-8edd-aab15c5e04f5&displaylang=en"), NULL,NULL, SW_SHOW);
		}
	}
	else
	{
		CiPodWizardDlg dlg;
		m_pMainWnd = &dlg;
		INT_PTR nResponse = dlg.DoModal();
		if (nResponse == IDOK)
		{
			// TODO: Place code here to handle when the dialog is
			//  dismissed with OK
		}
		else if (nResponse == IDCANCEL)
		{
			// TODO: Place code here to handle when the dialog is
			//  dismissed with Cancel
		}
	}

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}

void CiPodWizardApp::InitSectorSize(TCHAR *devstring, bool quiet)
{
	DISK_GEOMETRY gdg;
	DWORD ret;
	CString err_t;
	HANDLE h = CreateFile(devstring, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == (HANDLE)ERROR_INVALID_HANDLE || h==(HANDLE)0xFFFFFFFF)
	{
		if (!quiet)
		{
			ret=GetLastError();
			CString err_t;
			if (ret==5) //access denied: not running as administrator. The caller opens the same disk next and
			{           //explains this itself, so stay quiet here instead of showing two warnings in a row.
				return;
			}
			else
			{
				err_t.Format(_T("Error accessing iPod hard drive parameters!\nCode1: %d\nTrying to load firmware anyway."), ret);
				MessageBox(0, err_t, _T("Error"), 0);
			}
		}
		return;		// no usable handle: nothing more to ask of it (it would only produce a second error)
	}
	if (!DeviceIoControl((HANDLE)h, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &gdg, sizeof(gdg), &ret, NULL))
	{
		if (!quiet)
		{
			ret=GetLastError();
			err_t.Format(_T("Error accessing iPod hard drive parameters!\nCode2: %d\nTrying to load firmware anyway."), ret);
			MessageBox(0, err_t, _T("Error"), 0);
			theApp.BLOCK_SIZE = 512;
			//return;
		}
	}
	else
	{
		theApp.BLOCK_SIZE = (int)gdg.BytesPerSector;
	}
	CloseHandle(h);
}

// Size of the buffer CalcPartMapAddr may read, when the caller knows it (raw disk reads).
static size_t s_calcPartMapLen = (size_t)-1;

void CiPodWizardApp::InitHDDValues(int fd)
{
	if (fd==-1)
		return;
	
	if (BLOCK_SIZE <= 0 || BLOCK_SIZE > 65536)
		BLOCK_SIZE = 512;

	// A disk that is being ejected or has just gone away fails these reads; never
	// carry on with uninitialised data.
	unsigned char mbr[16384];
	memset(mbr, 0, sizeof(mbr));
	_lseek(fd, 0, SEEK_SET);
	if (_read(fd, mbr, 16384) <= 0)
		return;
	unsigned char macsig[2]={'E', 'R'};

	if (memcmp(mbr, macsig, 2))
	{
		//memcpy(&BLOCK_SIZE, &mbr[SECTOR_SIZE_PTR], 2);
		DWORD rs;
		memcpy(&rs, &mbr[RELATIVE_SECTOR], 4);
		FIRMWARE_START=rs*BLOCK_SIZE;
		//FIRMWARE_START=78*BLOCK_SIZE;
	}
	else
	{
		FIRMWARE_START=63*BLOCK_SIZE;
	}

	_lseek(fd, FIRMWARE_START, SEEK_SET);
	const int bufSize = BLOCK_SIZE*100;
	unsigned char *buf=new unsigned char[bufSize];
	memset(buf, 0, bufSize);
	if (_read(fd, buf, bufSize) < 512)
	{
		delete[] buf;
		return;
	}

	int size=sizeof(APPLE_WARN);
	char strwarn[] = APPLE_WARN;
	char *warn=new char[size+2];
	memset(warn, 0, size+1);
	memcpy(warn, buf, size);
	if (strncmp(warn, strwarn, size))
	{
		if (BLOCK_SIZE==512)
			BLOCK_SIZE=2048;
		else
			BLOCK_SIZE=512;
		FIRMWARE_START=63*BLOCK_SIZE;
	}
	
	s_calcPartMapLen = (size_t)bufSize;
	CalcPartMapAddr(buf);
	s_calcPartMapLen = (size_t)-1;
	delete[] buf;
}

void CiPodWizardApp::LoadPreferences()
{
	m_OldPicIndex=(BOOL)theApp.GetProfileIntW(_T("Main"), _T("PicIndexSystem"), FALSE);
	m_OldFontMetricsSystem=(BOOL)theApp.GetProfileIntW(_T("Main"), _T("FontMetricSystem"), FALSE);
	m_ReflashBootImage=(BOOL)theApp.GetProfileIntW(_T("Main"), _T("ReflashBootImage"), FALSE);
	m_LayoutRender=(BOOL)theApp.GetProfileIntW(_T("Main"), _T("LayoutRender"), FALSE);
	m_OTFRender=(BOOL)theApp.GetProfileIntW(_T("Main"), _T("OTFRender"), FALSE);
	m_AllStringsRender=(BOOL)theApp.GetProfileIntW(_T("Main"), _T("AllStringsRender"), FALSE);
	m_DarkFontCanvas=(BOOL)theApp.GetProfileIntW(_T("Main"), _T("DarkFontCanvas"), FALSE);
	m_AllowFontEdit=(BOOL)theApp.GetProfileIntW(_T("Main"), _T("AllowFontEdit"), FALSE);
	m_ForceReflash=(BOOL)theApp.GetProfileIntW(_T("Main"), _T("ForceReflash"), FALSE);
	m_NoReflash=(BOOL)theApp.GetProfileIntW(_T("Main"), _T("NoReflash"), FALSE);
	m_OldResourceSaving=(BOOL)theApp.GetProfileIntW(_T("Main"), _T("OldResourceSaving"), FALSE);
}

void CiPodWizardApp::CalcPartMapAddr(unsigned char *buf)
{
	unsigned short firmver=*((unsigned short *)&buf[266]);
	switch(firmver)
	{
	case 0:
		// The first software of the 2nd generation (1.1.0.2, 1.1.0.4) carries no version number, but its table of
		// images is where version 1 has it. Only taken when the table is really there: a disk that is not an iPod
		// has a zero in this place too.
		if ((s_calcPartMapLen == (size_t)-1 || s_calcPartMapLen >= 0x4008) &&
		    (memcmp(&buf[0x4000], &ata, 4) == 0 || memcmp(&buf[0x4000], &nand, 4) == 0))
			PARTITION_MAP_ADDRESS=0x4000;
		break;
	case 1:
		PARTITION_MAP_ADDRESS=0x4000;
		break;
	case 2:
	case 3:
		long diraddr=*((long *)&buf[260]);
		/*
		PARTITION_MAP_ADDRESS=diraddr;
		int z=0;
		while (memcmp(&buf[PARTITION_MAP_ADDRESS], &ata, 4) != 0 && memcmp(&buf[PARTITION_MAP_ADDRESS], &nand, 4) != 0 && z<50)
		{
			PARTITION_MAP_ADDRESS+=512;
			z++;
		}*/
		//PARTITION_MAP_ADDRESS=diraddr+1024;
		PARTITION_MAP_ADDRESS=diraddr+BLOCK_SIZE;
		if (s_calcPartMapLen != (size_t)-1 &&
		    (diraddr < 0 || (size_t)diraddr + 512 + 16384 + (size_t)BLOCK_SIZE + 16 > s_calcPartMapLen))
		{
			// Directory address is outside the data we read: not a usable iPod firmware header.
			PARTITION_MAP_ADDRESS=0;
			break;
		}
		if (memcmp(&buf[PARTITION_MAP_ADDRESS], &ata, 4) != 0 && memcmp(&buf[PARTITION_MAP_ADDRESS], &nand, 4) != 0)
		{
			PARTITION_MAP_ADDRESS=diraddr+512;
			while (memcmp(&buf[PARTITION_MAP_ADDRESS], &ata, 4) != 0 && memcmp(&buf[PARTITION_MAP_ADDRESS], &nand, 4) != 0 && PARTITION_MAP_ADDRESS<(diraddr+16384))
				PARTITION_MAP_ADDRESS+=512;
		}

		long pos=PARTITION_MAP_ADDRESS+BLOCK_SIZE;
		long sum=0;
		for (long i=0;i<16;sum+=buf[pos],pos++,i++);
		if (sum==0)
		{
			if (BLOCK_SIZE==512)
				BLOCK_SIZE=2048;
			else
				BLOCK_SIZE=512;
		}
		break;
	}
}

void CiPodWizardApp::RunAsAdmin()
{
	if (m_bAdminPrivilege)
		return;

	CREDUI_INFO cui;
	pszName=new TCHAR[CREDUI_MAX_USERNAME_LENGTH+1];
	pszPwd=new TCHAR[CREDUI_MAX_PASSWORD_LENGTH+1];
	BOOL fSave;
	DWORD dwErr;

	cui.cbSize = sizeof(CREDUI_INFO);
	cui.hwndParent = NULL;
	//  Ensure that MessageText and CaptionText identify what credentials
	//  to use and which application requires them.
	cui.pszMessageText = TEXT("You need to enter administrator account information in order to load firmware off the iPod");
	cui.pszCaptionText = TEXT("iPod Firmware Loading");
	cui.hbmBanner = NULL;
	fSave = FALSE;
	SecureZeroMemory(pszName, sizeof(pszName));
	SecureZeroMemory(pszPwd, sizeof(pszPwd));
	dwErr = CredUIPromptForCredentials( 
	&cui,                         // CREDUI_INFO structure
	TEXT("TheServer"),            // Target for credentials
								  //   (usually a server)
	NULL,                         // Reserved
	0,                            // Reason
	pszName,                      // User name
	CREDUI_MAX_USERNAME_LENGTH+1, // Max number of char for user name
	pszPwd,                       // Password
	CREDUI_MAX_PASSWORD_LENGTH+1, // Max number of char for password
	&fSave,                       // State of save check box
	CREDUI_FLAGS_GENERIC_CREDENTIALS |  // flags
	CREDUI_FLAGS_ALWAYS_SHOW_UI |
	CREDUI_FLAGS_DO_NOT_PERSIST |
	CREDUI_FLAGS_REQUEST_ADMINISTRATOR);  

	if(!dwErr)
	{
	//  Put code that uses the credentials here.

	
		m_bAdminPrivilege=TRUE;
	}
}

void CiPodWizardApp::EndRunAsAdmin()
{
	if (m_bAdminPrivilege)
	{
		m_bAdminPrivilege=FALSE;

		//  When you have finished using the credentials,
	//  erase them from memory.
		SecureZeroMemory(pszName, sizeof(pszName));
		SecureZeroMemory(pszPwd, sizeof(pszPwd));
	}
}