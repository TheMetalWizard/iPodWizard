#include "stdafx.h"
#include "resource.h"
#include "TipDlg.h"
// CG: This file added by 'Tip of the Day' component.

#include <winreg.h>
#include <sys\stat.h>
#include <sys\types.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTipDlg dialog

#define MAX_BUFLEN 1000

static const TCHAR szSection[] = _T("Tip");
static const TCHAR szIntFilePos[] = _T("FilePos");
static const TCHAR szTimeStamp[] = _T("TimeStamp");
static const TCHAR szIntStartup[] = _T("StartUp");

CTipDlg::CTipDlg(CWnd* pParent /*=NULL*/)
	: CDarkDialog(IDD_TIP, pParent)
{
	//{{AFX_DATA_INIT(CTipDlg)
	m_bStartup = TRUE;
	//}}AFX_DATA_INIT

	// We need to find out what the startup and file position parameters are
	// If startup does not exist, we assume that the Tips on startup is checked TRUE.
	CWinApp* pApp = AfxGetApp();
	m_bStartup = !pApp->GetProfileInt(szSection, szIntStartup, 0);
	UINT iFilePos = pApp->GetProfileInt(szSection, szIntFilePos, 0);

	m_nHelpID = 0;

	// look in the application directory for the tip file
  TCHAR drive[_MAX_DRIVE];
  TCHAR dir[_MAX_DIR];
  TCHAR fname[_MAX_FNAME];
  TCHAR ext[_MAX_EXT];
	//const char *path = _pgmptr ;
  TCHAR path[_MAX_PATH]; 
  ::GetModuleFileName(NULL, path, MAX_PATH);
  _tsplitpath(path, drive, dir, fname, ext );

   CString sPath;
	 sPath.Format(_T("%s%s%s"), drive, dir, _T("Tips.txt"));
	 TRACE("sPath is %s\n", sPath);

	 //read from resource:
	DWORD m_ResourceSize;
	HRSRC hRsrc;
	HGLOBAL hGlobal;
	unsigned char *m_ResourceBuffer;
	CFile file;

	// (the copy inside the program is written only when there is no Tips.txt next to it: that file can be edited, and writing it
	// every time made it a new file at every start, so the tips began again from the first one)
	if (!::PathFileExists(sPath))
	{
		hRsrc = FindResource(NULL, MAKEINTRESOURCE(IDR_TIPSTXT), TEXT("BYTE"));
		m_ResourceSize = SizeofResource(NULL, hRsrc);
		hGlobal = ::LoadResource(NULL, hRsrc);
		m_ResourceBuffer = (unsigned char *)LockResource(hGlobal);
	
		file.Open(sPath, CFile::modeCreate | CFile::modeWrite);
		file.Write(m_ResourceBuffer, m_ResourceSize);
		file.Close();
		::FreeResource(hGlobal);
	}


	// Now try to open the tips file
	m_pStream = fopen(Unicode2MB(sPath), "r");
	sPath.ReleaseBuffer();
	if (m_pStream == NULL) 
	{
		m_strHeader = "Oops!";
		m_strTip.LoadString(CG_IDS_FILE_ABSENT);
		m_sHelpText = _T("0");
		return;
	} 

	// If the timestamp in the INI file is different from the timestamp of
	// the tips file, then we know that the tips file has been modified
	// Reset the file position to 0 and write the latest timestamp to the
	// ini file
	struct _stat buf;
	_fstat(_fileno(m_pStream), &buf);
	CString strCurrentTime = ctime(&buf.st_ctime);
	strCurrentTime.TrimRight();
	CString strStoredTime = 
		pApp->GetProfileString(szSection, szTimeStamp, NULL);
	if (strCurrentTime != strStoredTime) 
	{
		iFilePos = 0;
		pApp->WriteProfileString(szSection, szTimeStamp, strCurrentTime);
	}

	if (fseek(m_pStream, iFilePos, SEEK_SET) != 0) 
	{
		AfxMessageBox(CG_IDP_FILE_CORRUPT);
	}
	else 
	{
		GetNextTipString();
	}
}

CTipDlg::~CTipDlg()
{
	// This destructor is executed whether the user had pressed the escape key
	// or clicked on the close button. If the user had pressed the escape key,
	// it is still required to update the filepos in the ini file with the 
	// latest position so that we don't repeat the tips! 
    
	// But make sure the tips file existed in the first place....
	if (m_pStream != NULL) 
	{
		CWinApp* pApp = AfxGetApp();
		pApp->WriteProfileInt(szSection, szIntFilePos, ftell(m_pStream));
		fclose(m_pStream);
	}
}
        
void CTipDlg::DoDataExchange(CDataExchange* pDX)
{
	CDarkDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTipDlg)
	DDX_Check(pDX, IDC_STARTUP, m_bStartup);
	DDX_Text(pDX, ST_FOOTER, m_strFooter);
	DDX_Text(pDX, ST_HEADER, m_strHeader);
	DDX_Text(pDX, IDC_TIPSTRING, m_strTip);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CTipDlg, CDarkDialog)
	//{{AFX_MSG_MAP(CTipDlg)
	ON_BN_CLICKED(IDC_NEXTTIP, OnNextTip)
	ON_BN_CLICKED(IDB_MORE, OnMore)
	ON_WM_CTLCOLOR()
	ON_WM_PAINT()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTipDlg message handlers



void CTipDlg::OnNextTip()
{
	GetNextTipString();

	// if the Help ID is 0, disable the Help button
	//GetDlgItem(IDB_MORE)->EnableWindow(m_nHelpID != 0);
	GetDlgItem(IDB_MORE)->ShowWindow(_tcscmp(m_sHelpText, _T("0")) != 0 ? SW_SHOW : SW_HIDE);      // no link: no "Tell Me More"
	UpdateData(FALSE);
}



// Tips.txt: one tip is a block of lines, blocks are separated by a blank line.
//
//   1: the text of the tip                      (needed)
//   2: the bold line above it                   (default: "Did you know?")
//   3: the small text at the bottom right       (default: none)
//   4: the address "Tell Me More" opens         (default: none, there is no "Tell Me More" button)
//
// A line that starts with # is a comment. A line that starts with a space or a tab goes on with the line before it. A block without
// a 1: line is skipped.
static CString TipTrim(CString s)
{
	s.TrimLeft(_T(" \t"));
	s.TrimRight(_T(" \t\r\n"));
	return s;
}

void CTipDlg::GetNextTipString()
{
	m_strHeader.LoadString(CG_IDS_DIDYOUKNOW);
	m_strFooter.Empty();
	m_strTip.Empty();
	m_sHelpText = _T("0");                                   // "0": no link
	m_nHelpID = 0;

	CString sTip, sTitle, sCategory, sLink;
	BOOL bRewound = FALSE;
	BOOL bFound = FALSE;

	while (!bFound)
	{
		CString *pField = NULL;                              // the field a continuation line belongs to
		BOOL bInBlock = FALSE;
		sTip.Empty(); sTitle.Empty(); sCategory.Empty(); sLink.Empty();
		BOOL bEof = FALSE;

		for (;;)
		{
			TCHAR line[MAX_BUFLEN];
			if (_fgetts(line, MAX_BUFLEN, m_pStream) == NULL)
			{
				bEof = TRUE;
				break;
			}
			CString s(line);
			CString t = TipTrim(s);
			if (t.IsEmpty())
			{
				if (bInBlock)
					break;                                   // the blank line after a tip: the block is complete
				continue;
			}
			if (s[0] == _T('#'))
				continue;
			if (s[0] == _T(' ') || s[0] == _T('\t'))
			{
				if (pField != NULL)
				{
					if (!pField->IsEmpty())
						*pField += _T(" ");
					*pField += t;
				}
				continue;
			}
			int colon = t.Find(_T(':'));
			if (colon <= 0)
				continue;
			CString name = t.Left(colon);
			CString value = TipTrim(t.Mid(colon + 1));
			if (name == _T("1"))             pField = &sTip;
			else if (name == _T("2"))        pField = &sTitle;
			else if (name == _T("3"))        pField = &sCategory;
			else if (name == _T("4"))        pField = &sLink;
			else { pField = NULL; continue; }
			*pField = value;
			bInBlock = TRUE;
		}

		if (!sTip.IsEmpty())
			bFound = TRUE;
		else if (bEof)
		{
			if (bRewound || fseek(m_pStream, 0, SEEK_SET) != 0)
				break;                                       // the whole file has no tip
			bRewound = TRUE;
		}
	}

	if (!bFound)
	{
		m_strHeader = _T("Oops!");
		m_strTip = _T("There is no tip in Tips.txt. Delete the file and iPodWizard makes it again.");
		return;
	}
	m_strTip = sTip;
	if (!sTitle.IsEmpty())
		m_strHeader = sTitle;
	m_strFooter = sCategory;
	if (!sLink.IsEmpty())
		m_sHelpText = sLink;
}



// draw text controls with a white background
HBRUSH CTipDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	if (pWnd->GetDlgCtrlID() == IDC_TIPSTRING)
		return (HBRUSH)GetStockObject(WHITE_BRUSH);
	if (pWnd->GetDlgCtrlID() == ST_FOOTER)
		return (HBRUSH)GetStockObject(WHITE_BRUSH);
	if (pWnd->GetDlgCtrlID() == ST_HEADER)
		return (HBRUSH)GetStockObject(WHITE_BRUSH);

	return CDarkDialog::OnCtlColor(pDC, pWnd, nCtlColor);
}



void CTipDlg::OnOK()
{
	CDarkDialog::OnOK();
	
    // Update the startup information stored in the INI file
	CWinApp* pApp = AfxGetApp();
	pApp->WriteProfileInt(szSection, szIntStartup, !m_bStartup);
}



BOOL CTipDlg::OnInitDialog()
{

	CDarkDialog::OnInitDialog();

	// If Tips file does not exist then disable NextTip
	if (m_pStream == NULL)
		GetDlgItem(IDC_NEXTTIP)->EnableWindow(FALSE);

	//GetDlgItem(IDB_MORE)->EnableWindow(m_nHelpID != 0);
	GetDlgItem(IDB_MORE)->ShowWindow(_tcscmp(m_sHelpText, _T("0")) != 0 ? SW_SHOW : SW_HIDE);      // no link: no "Tell Me More"

	// set the font for the Header
	CFont Font;
	Font.CreatePointFont(12, _T("Arial"));
	GetDlgItem(ST_HEADER)->SetFont(&Font);

	return TRUE;  // return TRUE unless you set the focus to a control
}



void CTipDlg::OnPaint()
{
	CPaintDC dc(this); // device context for painting

//	DoPaint (&dc, FALSE) ;

//	return;

	// Get paint area for the big static control
	CWnd* pStatic = GetDlgItem(IDC_TIP_RECT);
	CRect rcBig, rcBulb;

	pStatic->GetWindowRect(&rcBig);
	ScreenToClient(&rcBig);

	// decrease the size slightly so there is a thin gray border between
	// the white rectangle and the static control
	rcBig.InflateRect(-6, -6, -6, -6);

	// Paint the background white.
	CBrush brush;
	brush.CreateStockObject(WHITE_BRUSH);
	dc.FillRect(rcBig, &brush);
	brush.Detach();

	// now change the brush to the button shadow color
	brush.CreateSolidBrush( ::GetSysColor(COLOR_BTNSHADOW)) ;


	// Get the position of the lightbulb
	pStatic = GetDlgItem(IDB_LIGHTBULB);
	pStatic->GetWindowRect(&rcBulb);
	ScreenToClient(&rcBulb);


	// Load bitmap and get dimensions of the bitmap
	CBitmap bmp;
	bmp.LoadBitmap(IDB_LIGHTBULB);
	BITMAP bmpInfo;	
	bmp.GetBitmap(&bmpInfo);
	rcBulb.bottom = bmpInfo.bmHeight + rcBulb.top;
	rcBulb.right = bmpInfo.bmWidth + rcBulb.left;

	// change that right side of rcBig so it is even with the right side of the bitmap
	rcBig.right = rcBulb.right;

	// draw the left side shadow
	dc.FillRect(rcBig, &brush);


	// select the bitmap into the device context and draw
	CDC dcTmp;

	dcTmp.CreateCompatibleDC(&dc);
	dcTmp.SelectObject(&bmp);
	dcTmp.SelectObject(&brush) ;
	dcTmp.ExtFloodFill(0,0, 0x00ffffff, FLOODFILLSURFACE) ;
	dc.BitBlt(rcBulb.left, rcBulb.top, rcBulb.Width(), rcBulb.Height(), 
		&dcTmp, 0, 0, SRCCOPY);


	// Do not call CDarkDialog::OnPaint() for painting messages
}


// if the user clicks the More Info button, display the help topic 
// associated with the tip
void CTipDlg::OnMore()
{
	ASSERT(_tcscmp(m_sHelpText, _T("0")) != 0);
	//AfxGetApp()->WinHelp(m_nHelpID);
	ShellExecute(0, _T("Open"), m_sHelpText, NULL, NULL, SW_SHOW);
}


