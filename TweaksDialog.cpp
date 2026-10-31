// TweaksDialog.cpp : implementation file
//

#include "stdafx.h"
#include "iPodWizard.h"
#include "TweaksDialog.h"
#include ".\tweaksdialog.h"
#include <vector>


// CTweaksDialog dialog

IMPLEMENT_DYNAMIC(CTweaksDialog, CDarkDialog)
CTweaksDialog::CTweaksDialog(CWnd* pParent /*=NULL*/)
	: CDarkDialog(CTweaksDialog::IDD, pParent)
	, m_pFirmware(NULL)
{
}

CTweaksDialog::~CTweaksDialog()
{
}

void CTweaksDialog::DoDataExchange(CDataExchange* pDX)
{
	CDarkDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CTweaksDialog, CDarkDialog)
	ON_BN_CLICKED(ID_APPLY_BUTTON, OnBnClickedApply)
	ON_BN_CLICKED(IDC_FLIP_REFLASH_BUTTON, OnBnClickedFlipReflash)
	ON_BN_CLICKED(IDC_TWEAKS_VOLUMEHACK, OnBnClickedVolumeHackButton)
	ON_WM_TIMER()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

static const struct { UINT id; LPCTSTR file; } kTweakFiles[5] = {
	{ ID_VOLTAGE_CHKBOX, TEXT("_show_voltage") }, { ID_CPU_CHKBOX, TEXT("_show_speed") }, { ID_DEEPSLEEP_CHKBOX, TEXT("_short_deepsleep") },
	{ ID_CACHE_CHKBOX, TEXT("_disable_cache") }, { ID_NODEEPSLEEP_CHKBOX, TEXT("_no_deepsleep") } };

// The file based tweaks are files in iPod_Control\Device of the iPod selected in the iPod Detector (theApp.m_iPodDRV, empty: none selected)
static BOOL TweakFolderOK()
{
	return !theApp.m_iPodDRV.IsEmpty() && PathFileExists(theApp.m_iPodDRV);
}

void CTweaksDialog::Refresh()
{
	if (!GetSafeHwnd())
		return;
	BOOL bDrive = TweakFolderOK();
	for (int i = 0; i < 5; i++)
	{
		CString path = theApp.m_iPodDRV + TEXT("\\") + kTweakFiles[i].file;
		m_lastState[i] = bDrive && PathFileExists(path) ? 1 : 0;
		CheckDlgButton(kTweakFiles[i].id, m_lastState[i]);
		GetDlgItem(kTweakFiles[i].id)->EnableWindow(bDrive);
	}
	m_lastDrive = theApp.m_iPodDRV;
	m_lastOK = bDrive;
	GetDlgItem(ID_APPLY_BUTTON)->EnableWindow(bDrive);

	// the reflash byte of the loaded firmware: the button names what it does next, grey when the firmware has none to change
	BOOL bCan = m_pFirmware != NULL && m_pFirmware->CanFlipReflashByte();
	SetDlgItemText(IDC_FLIP_REFLASH_BUTTON, bCan && m_pFirmware->ReflashByteSet() ? TEXT("Disable reflash byte") : TEXT("Enable reflash byte"));
	GetDlgItem(IDC_FLIP_REFLASH_BUTTON)->EnableWindow(bCan);
	GetDlgItem(IDC_TWEAKS_VOLUMEHACK)->EnableWindow(m_pFirmware != NULL);
}

// The boxes show the tweak files that are on the selected iPod now: looked at once a second, a box only changes when its file has (a tick
// you have set and not applied yet stays), and a different or missing iPod folder redraws all of them.
void CTweaksDialog::PollTweakFiles()
{
	if (theApp.m_iPodDRV != m_lastDrive || TweakFolderOK() != m_lastOK)
	{
		Refresh();
		return;
	}
	if (!m_lastOK)
		return;
	for (int i = 0; i < 5; i++)
	{
		int s = PathFileExists(theApp.m_iPodDRV + TEXT("\\") + kTweakFiles[i].file) ? 1 : 0;
		if (s != m_lastState[i])
		{
			m_lastState[i] = s;
			CheckDlgButton(kTweakFiles[i].id, s);
		}
	}
}

void CTweaksDialog::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == 1 && IsWindowVisible())
		PollTweakFiles();
	CDarkDialog::OnTimer(nIDEvent);
}

void CTweaksDialog::OnDestroy()
{
	KillTimer(1);
	CDarkDialog::OnDestroy();
}

//VOLUME CAPPER START

// The question of the Volume Limit Hack window: word-wrapped text, a part between ** ** in bold, \n starts a new paragraph.
struct RichWord { CString w; BOOL bold, space, para; };

static void DrawRichText(HDC hdc, const RECT &rc, HFONT normal, HFONT bold, const CString &text)
{
	std::vector<RichWord> words;
	BOOL b = FALSE, space = FALSE;
	CString cur;
	for (int i = 0; i <= text.GetLength(); i++)
	{
		TCHAR c = i < text.GetLength() ? text[i] : _T('\n');
		BOOL marker = (c == _T('*') && i + 1 < text.GetLength() && text[i + 1] == _T('*'));
		if (c != _T(' ') && c != _T('\n') && !marker)
		{
			cur += c;
			continue;
		}
		if (!cur.IsEmpty())
		{
			RichWord w = { cur, b, space, FALSE };
			words.push_back(w);
			cur.Empty();
			space = FALSE;
		}
		if (marker)
		{
			b = !b;
			i++;
		}
		else if (c == _T(' '))
			space = TRUE;
		else
		{
			RichWord w = { CString(), FALSE, FALSE, TRUE };
			words.push_back(w);
			space = FALSE;
		}
	}

	HGDIOBJ old = SelectObject(hdc, normal);
	TEXTMETRIC tm;
	GetTextMetrics(hdc, &tm);
	SIZE sp;
	GetTextExtentPoint32(hdc, _T(" "), 1, &sp);
	int lineH = tm.tmHeight, gap = tm.tmHeight / 2;
	int x = rc.left, y = rc.top;
	SetBkMode(hdc, TRANSPARENT);
	SetTextColor(hdc, GetSysColor(COLOR_BTNTEXT));
	for (size_t n = 0; n < words.size(); n++)
	{
		const RichWord &w = words[n];
		if (w.para)
		{
			x = rc.left;
			y += lineH + gap;
			continue;
		}
		SelectObject(hdc, w.bold ? bold : normal);
		SIZE sz;
		GetTextExtentPoint32(hdc, w.w, w.w.GetLength(), &sz);
		int lead = (w.space && x > rc.left) ? sp.cx : 0;
		if (x > rc.left && x + lead + sz.cx > rc.right)
		{
			x = rc.left;
			y += lineH;
			lead = 0;
		}
		TextOut(hdc, x + lead, y, w.w, w.w.GetLength());
		x += lead + sz.cx;
	}
	SelectObject(hdc, old);
}

static INT_PTR CALLBACK VolumeHackDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static const TCHAR *s_text = NULL;
	switch (msg)
	{
	case WM_INITDIALOG:
		s_text = (const TCHAR *)lParam;
		return TRUE;
	case WM_DRAWITEM:
		if (wParam == IDC_VOLHACK_TEXT && s_text != NULL)
		{
			const DRAWITEMSTRUCT *dis = (const DRAWITEMSTRUCT *)lParam;
			HFONT normal = (HFONT)SendMessage(hDlg, WM_GETFONT, 0, 0);
			LOGFONT lf;
			GetObject(normal, sizeof(lf), &lf);
			lf.lfWeight = FW_BOLD;
			HFONT bold = CreateFontIndirect(&lf);
			FillRect(dis->hDC, &dis->rcItem, GetSysColorBrush(COLOR_BTNFACE));
			DrawRichText(dis->hDC, dis->rcItem, normal, bold, CString(s_text));
			DeleteObject(bold);
			return TRUE;
		}
		break;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDYES)
			EndDialog(hDlg, IDYES);
		else if (LOWORD(wParam) == IDNO || LOWORD(wParam) == IDCANCEL)
			EndDialog(hDlg, IDNO);
		break;
	}
	return FALSE;
}

void CTweaksDialog::OnBnClickedVolumeHackButton()
{
	CString s,a,b;
	int mode=m_pFirmware->isCapped();
	if (mode==0)
	{
		a.Format(TEXT("uncapped"));
		b.Format(TEXT("cap"));
	}
	else if (mode==1)
	{
		a.Format(TEXT("capped"));
		b.Format(TEXT("uncap"));
	}
	else
	{
		MessageBox(TEXT("Couldn't find volume capping mode, your firmware might be too new and iPodWizard doesn't support it or there's a problem in it."));
		return;
	}
	s.Format(TEXT("This hack will remove (or add) the EU volume cap to your iPod.\nVolume hacking is not compatible with the 1/2G or 5.5G iPod.\nThe current firmware's volume is **%s**, do you want to %s it?"), a, b);
	if (DialogBoxParam(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDD_VOLUME_HACK), m_hWnd, VolumeHackDlgProc, (LPARAM)(LPCTSTR)s) != IDYES)
		return;
	int ret;
	if (a.Compare(TEXT("capped"))==0)
		ret=m_pFirmware->unCap();
	else
		ret=m_pFirmware->reCap();
	//if you want to point to the offset in the firmware we changed the capping bytes, uncomment the two following lines
	//if (ret>0)
	//	m_HexWnd.SetOffset(ret);
}
//VOLUME CAPPER END

void CTweaksDialog::OnBnClickedFlipReflash()
{
	if (m_pFirmware == NULL || !m_pFirmware->CanFlipReflashByte())
		return;
	m_pFirmware->FlipReflashByte();
	Refresh();
}

// The tip of the button also shows while the button is grey: the mouse message that is over it is given to the tip as if the button had got it.
BOOL CTweaksDialog::PreTranslateMessage(MSG *pMsg)
{
	if (m_Tip.GetSafeHwnd() && pMsg->message >= WM_MOUSEFIRST && pMsg->message <= WM_MOUSELAST)
	{
		MSG msg = *pMsg;
		POINT pt = { (short)LOWORD(pMsg->lParam), (short)HIWORD(pMsg->lParam) };
		::MapWindowPoints(pMsg->hwnd, NULL, &pt, 1);                    // screen
		CWnd *pBtn = GetDlgItem(IDC_FLIP_REFLASH_BUTTON);
		CRect rc;
		if (pBtn != NULL && pBtn->IsWindowVisible())
		{
			pBtn->GetWindowRect(&rc);
			if (rc.PtInRect(pt))
			{
				::ScreenToClient(pBtn->m_hWnd, &pt);
				msg.hwnd = pBtn->m_hWnd;
				msg.lParam = MAKELPARAM(pt.x, pt.y);
			}
		}
		m_Tip.RelayEvent(&msg);
	}
	return CDarkDialog::PreTranslateMessage(pMsg);
}

// CTweaksDialog message handlers

BOOL CTweaksDialog::OnInitDialog()
{
	CDarkDialog::OnInitDialog();

	m_Tip.Create(this, TTS_ALWAYSTIP);
	m_Tip.SetMaxTipWidth(320);
	m_Tip.SetDelayTime(TTDT_AUTOPOP, 30000);
	// the tip belongs to the button; while the button is grey it gets no mouse messages, so PreTranslateMessage hands them to the tip
	m_Tip.AddTool(GetDlgItem(IDC_FLIP_REFLASH_BUTTON), _T("This will flip the firmware's reflash byte to trigger or disable a reflash once ejected."));
	m_Tip.Activate(TRUE);

	m_lastOK = FALSE;
	for (int i = 0; i < 5; i++)
		m_lastState[i] = 0;
	Refresh();
	SetTimer(1, 1000, NULL);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CTweaksDialog::OnBnClickedApply()
{
	if (!TweakFolderOK())
	{
		MessageBox(TEXT("No iPod is selected in the iPod Detector."), TEXT("iPod Tweaks"), MB_OK | MB_ICONERROR);
		Refresh();
		return;
	}
	int failed = 0;
	for (int i = 0; i < 5; i++)
	{
		CString path = theApp.m_iPodDRV + TEXT("\\") + kTweakFiles[i].file;
		if (IsDlgButtonChecked(kTweakFiles[i].id) == 0)
		{
			SetFileAttributes(path, FILE_ATTRIBUTE_NORMAL);          // a read-only / hidden marker file would not delete
			if (!DeleteFile(path) && GetLastError() != ERROR_FILE_NOT_FOUND)
				failed++;
		}
		else
		{
			CFile file;
			if (PathFileExists(path))
				continue;                                            // already there
			if (file.Open(path, CFile::modeCreate))
				file.Close();
			else
				failed++;
		}
	}
	if (failed)
		MessageBox(TEXT("Some tweaks could not be written to the iPod."), TEXT("iPod Tweaks"), MB_OK | MB_ICONERROR);
	else
		MessageBox(TEXT("Tweaks applied successfully."), TEXT("iPod Tweaks"), MB_OK);
	Refresh();
}

