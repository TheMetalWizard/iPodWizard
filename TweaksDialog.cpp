// TweaksDialog.cpp : implementation file
//

#include "stdafx.h"
#include "iPodWizard.h"
#include "TweaksDialog.h"
#include ".\tweaksdialog.h"
#include "LayoutDialog.h"
#include "ClickTable.h"
#include <vector>


// CTweaksDialog dialog

IMPLEMENT_DYNAMIC(CTweaksDialog, CDarkDialog)
CTweaksDialog::CTweaksDialog(CWnd* pParent /*=NULL*/)
	: CDarkDialog(CTweaksDialog::IDD, pParent)
	, m_pFirmware(NULL)
	, m_pLayout(NULL)
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
	ON_BN_CLICKED(IDC_SELECTORBAR_BUTTON, OnBnClickedSelectorBar)
	ON_BN_CLICKED(IDC_CLICKPITCH_BUTTON, OnBnClickedClickPitch)
	ON_BN_CLICKED(IDC_LCDINVERT_BUTTON, OnBnClickedLcdInvert)
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

	// the reflash byte of the loaded firmware: the button names what it does next to the iPod's reflashing, grey when the firmware has none to change
	BOOL bCan = m_pFirmware != NULL && m_pFirmware->CanFlipReflashByte();
	SetDlgItemText(IDC_FLIP_REFLASH_BUTTON, bCan && !m_pFirmware->ReflashByteSet() ? TEXT("Disable reflash") : TEXT("Enable reflash"));    // byte 0 = the iPod reflashes
	GetDlgItem(IDC_FLIP_REFLASH_BUTTON)->EnableWindow(bCan);
	GetDlgItem(IDC_TWEAKS_VOLUMEHACK)->EnableWindow(m_pFirmware != NULL);
	GetDlgItem(IDC_SELECTORBAR_BUTTON)->EnableWindow(m_pFirmware != NULL && m_pLayout != NULL);
	{
		vector<DWORD> cp, cl;
		GetDlgItem(IDC_CLICKPITCH_BUTTON)->EnableWindow(m_pFirmware != NULL);     // always (when a firmware is loaded): the click is found by its instructions, or the user picks the firmware from the list
		vector<DWORD> inv;
		BYTE v[4];
		DWORD initSite;
		BOOL wide;
		vector<DWORD> skip, consts;
		BOOL bInv = m_pFirmware != NULL && m_pFirmware->GetLcdInvertSites(inv, v, &initSite, &wide, &skip, &consts);
		SetDlgItemText(IDC_LCDINVERT_BUTTON, bInv && (wide ? *(DWORD *)&m_pFirmware->GetFirmwareBuffer()[inv[0]] != 0x03800001 : (m_pFirmware->GetFirmwareBuffer()[inv[0]] == 0x27 || m_pFirmware->GetFirmwareBuffer()[inv[0]] == 0x19)) ? TEXT("Disable screen invert") : TEXT("Enable screen invert"));    // names what the button does next
		GetDlgItem(IDC_LCDINVERT_BUTTON)->EnableWindow(bInv);
	}
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

// "Click Pitch" (4G): the click of the wheel is a pulse of a number N, the pitch is 93750 / (N + 1) Hz (stock N = 85: 1090 Hz). The number is in two instructions of the firmware.
static int g_ClickCode = 85;                                   // in: the number the firmware has; out: the one chosen
static int g_ClickLen = 3000;
static BOOL g_ClickImm = FALSE;                                // the 3G form: the numbers are bytes (0 - 255) of immediate instructions and their meaning is not known yet                                  // the same for the length, microseconds (stock 3000 = 3 ms)

// the 3G's length is "movne r1,#imm8 ror (2 * rot)": the stored values are imm8 << k (k even); returns the nearest one and its two fields
static int ClickEncodeImm(UINT v, BYTE *pImm8, BYTE *pRot)
{
	int k = 0;
	while (k < 24 && ((v + ((1u << k) >> 1)) >> k) > 255)
		k += 2;
	UINT m = (v + ((1u << k) >> 1)) >> k;
	if (m > 255)
		m = 255;
	*pImm8 = (BYTE)m;
	*pRot = (BYTE)(((32 - k) / 2) & 15);
	return (int)(m << k);
}

// the readouts under the two numbers: the frequency (93750 / (N + 1)) and the length in milliseconds, or what is wrong with the entry
static BOOL ClickPitchReadout(HWND hDlg, int *pN)
{
	BOOL ok = FALSE;
	UINT n = GetDlgItemInt(hDlg, IDC_CLICKPITCH_EDIT, &ok, FALSE);
	CString t;
	if (g_ClickImm)
	{
		// the 3G: 750000 / N Hz [measured: N = 255 gave 2941 Hz]; N is stored as an 8-bit number shifted by an even number of bits, so the nearest storable N is used
		if (!ok || n < 1 || n > 65535)
		{
			t = _T("Enter a number from 1 to 65535.");
			ok = FALSE;
		}
		else
		{
			BYTE imm8, rot;
			int real = ClickEncodeImm(n, &imm8, &rot);
			if (real == (int)n)
				t.Format(_T("Frequency: %.0f Hz"), 750000.0 / real);
			else
				t.Format(_T("Stored as %d: %.0f Hz (the nearest the 3G can hold)"), real, 750000.0 / real);
			*pN = (int)n;
		}
		SetDlgItemText(hDlg, IDC_CLICKPITCH_INFO, t);
		return ok;
	}
	if (!ok || n < 1 || n > 255)
	{
		t = _T("Enter a number from 1 to 255.");
		ok = FALSE;
	}
	else
	{
		double hz = 93750.0 / (n + 1);
		t.Format(_T("Frequency: %.0f Hz"), hz);
		*pN = (int)n;
	}
	SetDlgItemText(hDlg, IDC_CLICKPITCH_INFO, t);
	return ok;
}

static BOOL ClickLengthReadout(HWND hDlg, int *pUs)
{
	BOOL ok = FALSE;
	UINT us = GetDlgItemInt(hDlg, IDC_CLICKLEN_EDIT, &ok, FALSE);
	CString t;
	if (g_ClickImm)
	{
		// the 3G stores the length as an 8-bit number shifted by an even number of bits: the nearest length it can store is used
		if (!ok || us < 1 || us > 100000)
		{
			t = _T("Enter a length from 1 to 100000 microseconds.");
			ok = FALSE;
		}
		else
		{
			BYTE imm8, rot;
			int real = ClickEncodeImm(us, &imm8, &rot);
			if (real == (int)us)
				t.Format(_T("%u microseconds = %.2f ms"), us, us / 1000.0);
			else
				t.Format(_T("Stored as %d microseconds = %.2f ms (the nearest the 3G can hold)"), real, real / 1000.0);
			*pUs = (int)us;
		}
		SetDlgItemText(hDlg, IDC_CLICKLEN_INFO, t);
		return ok;
	}
	if (!ok || us < 100 || us > 100000)
	{
		t = _T("Enter a length from 100 to 100000 microseconds.");
		ok = FALSE;
	}
	else
	{
		t.Format(_T("%u microseconds = %.1f ms"), us, us / 1000.0);
		*pUs = (int)us;
	}
	SetDlgItemText(hDlg, IDC_CLICKLEN_INFO, t);
	return ok;
}

static INT_PTR CALLBACK ClickPitchDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_INITDIALOG)
	{
		if (g_ClickImm)
		{
			SetDlgItemText(hDlg, IDC_CLICKPITCH_LABEL, _T("Pitch value (0 - 255):"));
			SetDlgItemText(hDlg, IDC_CLICKLEN_LABEL, _T("Length (1 - 100000):"));
			SetDlgItemText(hDlg, IDC_CLICKPITCH_LABEL, _T("Pitch number (1 - 65535):"));
		}
		SetDlgItemInt(hDlg, IDC_CLICKPITCH_EDIT, g_ClickCode, FALSE);
		SetDlgItemInt(hDlg, IDC_CLICKLEN_EDIT, g_ClickLen, FALSE);
		int n;
		ClickPitchReadout(hDlg, &n);
		ClickLengthReadout(hDlg, &n);
		return TRUE;
	}
	if (msg == WM_COMMAND && HIWORD(wParam) == EN_CHANGE && (LOWORD(wParam) == IDC_CLICKPITCH_EDIT || LOWORD(wParam) == IDC_CLICKLEN_EDIT))
	{
		int n;
		if (LOWORD(wParam) == IDC_CLICKPITCH_EDIT)
			ClickPitchReadout(hDlg, &n);                                  // the readouts follow every change of the numbers
		else
			ClickLengthReadout(hDlg, &n);
		return TRUE;
	}
	if (msg == WM_COMMAND && LOWORD(wParam) == IDC_CLICKSTOCK_BUTTON)
	{
		SetDlgItemInt(hDlg, IDC_CLICKPITCH_EDIT, g_ClickImm ? 100 : 85, FALSE);      // the click of the stock firmwares: pitch 85 (1090 Hz), 3000 microseconds (the 3G: 100 and 100)
		SetDlgItemInt(hDlg, IDC_CLICKLEN_EDIT, g_ClickImm ? 100 : 3000, FALSE);
		return TRUE;
	}
	if (msg == WM_COMMAND && LOWORD(wParam) == IDOK)
	{
		int n = 0, us = 0;
		if (!ClickPitchReadout(hDlg, &n))
		{
			MessageBox(hDlg, g_ClickImm ? _T("Enter a pitch number from 1 to 65535.") : _T("Enter a pitch number from 1 to 255."), _T("Clicker Tone"), MB_OK | MB_ICONWARNING);
			return TRUE;
		}
		if (!ClickLengthReadout(hDlg, &us))
		{
			MessageBox(hDlg, g_ClickImm ? _T("Enter a length from 1 to 100000 microseconds.") : _T("Enter a length from 100 to 100000 microseconds."), _T("Clicker Tone"), MB_OK | MB_ICONWARNING);
			return TRUE;
		}
		g_ClickCode = n;
		g_ClickLen = us;
		EndDialog(hDlg, IDOK);
		return TRUE;
	}
	if (msg == WM_COMMAND && LOWORD(wParam) == IDCANCEL)
	{
		EndDialog(hDlg, IDCANCEL);
		return TRUE;
	}
	return FALSE;
}

// The list of the firmwares whose click places are known (ClickTable.h): used when the places cannot be found by the instructions (an unknown build, or a click that was changed so that it no longer looks like the stock one).
static int g_ClickPick = -1;

static INT_PTR CALLBACK ClickListDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_INITDIALOG)
	{
		for (int i = 0; i < (int)(sizeof(g_ClickTable) / sizeof(g_ClickTable[0])); i++)
		{
			CString s;
			s.Format(_T("%S  -  %S"), g_ClickTable[i].ipod, g_ClickTable[i].version);
			SendDlgItemMessage(hDlg, IDC_CLICKFW_LIST, LB_ADDSTRING, 0, (LPARAM)(LPCTSTR)s);
		}
		return TRUE;
	}
	if (msg == WM_COMMAND && (LOWORD(wParam) == IDOK || (LOWORD(wParam) == IDC_CLICKFW_LIST && HIWORD(wParam) == LBN_DBLCLK)))
	{
		int sel = (int)SendDlgItemMessage(hDlg, IDC_CLICKFW_LIST, LB_GETCURSEL, 0, 0);
		if (sel < 0)
		{
			MessageBox(hDlg, _T("Choose a firmware from the list."), _T("Clicker Tone"), MB_OK | MB_ICONWARNING);
			return TRUE;
		}
		g_ClickPick = sel;
		EndDialog(hDlg, IDOK);
		return TRUE;
	}
	if (msg == WM_COMMAND && LOWORD(wParam) == IDCANCEL)
	{
		EndDialog(hDlg, IDCANCEL);
		return TRUE;
	}
	return FALSE;
}

void CTweaksDialog::OnBnClickedClickPitch()
{
	vector<DWORD> cp, cl;
	BOOL imm = FALSE;
	if (m_pFirmware == NULL)
		return;
	if (!m_pFirmware->FindClickSites(cp, cl, &imm))
	{
		// not found by the instructions: the user says which firmware this is, and the places stored for it are used (after looking that the instructions there are the click's)
		cp.clear();
		cl.clear();
		DWORD start, ilen;
		if (!m_pFirmware->GetImageRange("soso", &start, &ilen))
		{
			MessageBox(_T("This firmware has no operating system image."), _T("Clicker Tone"), MB_OK | MB_ICONINFORMATION);
			return;
		}
		g_ClickPick = -1;
		if (DialogBox(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDD_CLICK_FWLIST), m_hWnd, ClickListDlgProc) != IDOK || g_ClickPick < 0)
			return;
		const ClickEntry &e = g_ClickTable[g_ClickPick];
		LPBYTE b = m_pFirmware->GetFirmwareBuffer();
		BOOL good = TRUE;
		for (int i = 0; i < e.count; i++)
		{
			DWORD p = start + e.pitch[i], l = start + e.len[i];
			if (p + 4 > start + ilen || l + 4 > start + ilen)
			{
				good = FALSE;
				break;
			}
			DWORD wp = *(DWORD *)&b[p];
			if (e.form == 1)
				good = good && (wp & 0xFFFFF000) == 0x13A00000 && (*(DWORD *)&b[l] & 0xFFFFF000) == 0x13A01000;       // movne r0,#N / movne r1,#L
			else
				good = good && (wp & 0x0FFFF000) == 0x03A00000 && *(DWORD *)&b[l] != 0 && *(DWORD *)&b[l] <= 0xFFFFFF;     // mov<c> r0,#N / the literal
			cp.push_back(p);
			cl.push_back(l);
		}
		if (!good)
		{
			MessageBox(_T("The code at the places of that firmware is not the click's: this is a different firmware than the one chosen. Nothing was changed."), _T("Clicker Tone"), MB_OK | MB_ICONWARNING);
			return;
		}
		imm = e.form == 1;
	}
	g_ClickImm = imm;
	LPBYTE buf = m_pFirmware->GetFirmwareBuffer();
	g_ClickCode = buf[cp[0]];
	if (imm)
	{
		UINT v = buf[cp[0]], r = 2 * (buf[cp[0] + 1] & 15);
		g_ClickCode = (int)(r ? ((v >> r) | (v << (32 - r))) : v);
	}
	// what the firmware has now (stock 85 / 3000, the 3G 100 / 100, or what an earlier change made)
	if (imm)
	{
		UINT v = buf[cl[0]], r = 2 * (buf[cl[0] + 1] & 15);              // imm8 rotated right by 2 * rot
		g_ClickLen = (int)(r ? ((v >> r) | (v << (32 - r))) : v);
	}
	else
	{
		if (g_ClickCode < 1)
			g_ClickCode = 85;
		g_ClickLen = (int)*(DWORD *)&buf[cl[0]];
		if (g_ClickLen < 100 || g_ClickLen > 100000)
			g_ClickLen = 3000;
	}
	if (DialogBox(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDD_CLICK_PITCH), m_hWnd, ClickPitchDlgProc) != IDOK)
		return;
	BYTE code = (BYTE)g_ClickCode;
	DWORD len = (DWORD)g_ClickLen;
	for (size_t i = 0; i < cp.size(); i++)
	{
		if (imm)
		{
			BYTE imm8, rot, two[2];
			ClickEncodeImm(g_ClickCode, &imm8, &rot);
			two[0] = imm8;
			two[1] = rot;                                              // Rd = r0 and the rotation of the 3G's movne r0,#N
			m_pFirmware->SetData(cp[i], two, 2);
		}
		else
			m_pFirmware->SetData(cp[i], &code, 1);                     // the low byte of the mov r0,#N instructions
		if (imm)
		{
			BYTE imm8, rot, two[2];
			ClickEncodeImm(len, &imm8, &rot);
			two[0] = imm8;
			two[1] = (BYTE)(0x10 | rot);                                   // the Rd = r1 nibble and the rotation of the 3G's movne r1,#L
			m_pFirmware->SetData(cl[i], two, 2);
		}
		else
			m_pFirmware->SetData(cl[i], (LPBYTE)&len, 4);                  // the literals that hold the length (microseconds)
	}
	CString msg;
	if (imm)
		msg.Format(_T("The click is now pitch number %d (%.0f Hz), %d microseconds long, at %d place%s of the firmware.\n\nWrite the firmware to the iPod and listen. (The frequency formula 750000 / N was measured on a 3G with N = 255; this change has not been tried with other values: keep a backup.)"), g_ClickCode, 750000.0 / g_ClickCode, g_ClickLen, (int)cp.size(), cp.size() == 1 ? "" : "s");
	else
	msg.Format(_T("The click is now pitch number %d (%d Hz), %.1f ms long, at %d place%s of the firmware.\n\nWrite the firmware to the iPod to hear it. (The frequency formula was measured on a 4G; this change has not been tried on an iPod yet: keep a backup.)"), g_ClickCode, 93750 / (g_ClickCode + 1), g_ClickLen / 1000.0, (int)cp.size(), cp.size() == 1 ? "" : "s");
	MessageBox(msg, _T("Clicker Tone"), MB_OK | MB_ICONINFORMATION);
}

// "Change Selector Bar Color" (moved here from the Layout tab): the notice with 5G / 5.5G / Cancel, then the two colors; the work is done by the layout dialog, which holds the parsed layouts
void CTweaksDialog::OnBnClickedSelectorBar()
{
	if (m_pFirmware == NULL || m_pLayout == NULL)
		return;
	m_pLayout->SelectorBarTweak();
}

// "Enable screen invert" (4G): flips bit 1 of the LCD display control register for good: the one byte that decides it in the firmware is 1 (stock) or 3 (the screen is inverted)
void CTweaksDialog::OnBnClickedLcdInvert()
{
	vector<DWORD> sites;
	BYTE v[4];
	DWORD initSite;
	BOOL wide;
	vector<DWORD> skip, consts;
	if (m_pFirmware == NULL || !m_pFirmware->GetLcdInvertSites(sites, v, &initSite, &wide, &skip, &consts))
		return;
	if (wide)
	{
		// 4G and minis: register 7 = the table value | 1 or | 3 (stock), or 0x27 whatever the backlight (Rockbox's value for the inverted panel)
		static const DWORD kInvA = 0x27;                                  // Rockbox's inverted value. On a mini 6.2.6 0x27, 0x19 and even 0 (blank) all changed nothing: the patch did not reach the running firmware (not found yet why)
		BOOL on = *(DWORD *)&m_pFirmware->GetFirmwareBuffer()[sites[0]] == 0x03800001;      // stock now: turn the inverted values on
		DWORD w1 = on ? (0x03A00000 | kInvA) : 0x03800001, w2 = on ? (0x13A00000 | kInvA) : 0x13800003;
		m_pFirmware->SetData(sites[0], (LPBYTE)&w1, 4);
		m_pFirmware->SetData(sites[0] + 4, (LPBYTE)&w2, 4);
		for (size_t i = 1; i < sites.size(); i++)
		{
			DWORD w = on ? (0xE3A04000 | kInvA) : 0xE3804001;                       // the display-on writes: R7 = 0x27 / the table value | 1
			m_pFirmware->SetData(sites[i], (LPBYTE)&w, 4);
		}
		for (size_t i = 0; i < skip.size(); i++)
		{
			DWORD s = on ? 0xE1A00000 : 0x0A000006;                        // nop in place of the beq that skips the write of the register 7 when the display is initialised at boot
			m_pFirmware->SetData(skip[i], (LPBYTE)&s, 4);
		}
		if (initSite != 0)
		{
			DWORD g = on ? 0x13B01001 : 0x13510000;                            // movsne r1,#1 / cmpne r1,#0: the backlight flag setter always redraws the register 7
			m_pFirmware->SetData(initSite, (LPBYTE)&g, 4);
		}
		Refresh();
		return;
	}
	LPBYTE buf = m_pFirmware->GetFirmwareBuffer();
	BOOL turnOn = !(buf[sites[0]] == 0x27 || buf[sites[0]] == 0x19);              // stock now: switch the inverted values on (0x19: a firmware saved by an earlier version)
	for (size_t i = 0; i < sites.size(); i++)
	{
		for (int k = 0; k < 2; k++)                                  // the two conditional instructions, in either order: eq = the backlight off (9), ne = on (0xB); both 0x27 when inverted
		{
			BOOL eq = (buf[sites[i] + 4 * k + 3] >> 4) == 0;
			BOOL early = consts.empty();                                    // the 3G 2.2.0.x / Stacker: 0x27 whatever the backlight (0x19 for "backlight on" showed the NORMAL screen on a 2.2.0.0); the other builds: the Rockbox pair (works on a 3G 2.2.3)
			BYTE imm = turnOn ? ((eq || early) ? 0x27 : 0x19) : (eq ? 9 : 0xB);
			m_pFirmware->SetData(sites[i] + 4 * k, &imm, 1);
		}
	}
	if (initSite != 0)
	{
		DWORD g = *(DWORD *)&buf[initSite];                                  // the guard of the backlight flag function: "cmpne rB,#0" <-> "movsne rB,#1" (always go on)
		DWORD rd = (g & 0xFFF0FFFF) == 0x13500000 ? (g >> 16) & 15 : (g >> 12) & 15;
		g = turnOn ? (0x13B00001 | (rd << 12)) : (0x13500000 | (rd << 16));
		m_pFirmware->SetData(initSite, (LPBYTE)&g, 4);
	}
	for (size_t i = 0; i < consts.size(); i++)
	{
		BYTE imm = turnOn ? 0x27 : 9;                                // the constant writes of the register 7 ("mov r2,#9"): the value for the backlight off
		m_pFirmware->SetData(consts[i], &imm, 1);
	}
	Refresh();
}

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
		static const UINT kTipIds[2] = { IDC_FLIP_REFLASH_BUTTON, IDC_LCDINVERT_BUTTON };
		for (int t = 0; t < 2; t++)
		{
			CWnd *pBtn = GetDlgItem(kTipIds[t]);
			CRect rc;
			if (pBtn != NULL && pBtn->IsWindowVisible())
			{
				pBtn->GetWindowRect(&rc);
				if (rc.PtInRect(pt))
				{
					POINT p2 = pt;
					::ScreenToClient(pBtn->m_hWnd, &p2);
					msg.hwnd = pBtn->m_hWnd;
					msg.lParam = MAKELPARAM(p2.x, p2.y);
					break;
				}
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
	m_Tip.AddTool(GetDlgItem(IDC_LCDINVERT_BUTTON), _T("1G, 2G, 3G, 4G and mini: inverts the screen (light text on a dark background) with the backlight on and off, by swapping the two values the firmware writes to the LCD display control register for the two backlight states. Write the firmware to the iPod to see it. Not tried on an iPod yet: keep a backup."));
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

