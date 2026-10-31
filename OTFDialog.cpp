// OTFDialog.cpp : implementation file
//

#include "stdafx.h"
#include "iPodWizard.h"
#include "OTFDialog.h"
#include ".\otfdialog.h"

// helper routines

const wchar_t *GetUnicodeBlockName(WORD ch);      // (FontsDialog.cpp)
CString CombinedFontLabel(CFirmware *pFw, DWORD index);      // (FontsDialog.cpp)

void DDX_OTFText(CDataExchange *pDX, int nIDC, BYTE &value)
{
	CString s;

	if (pDX->m_bSaveAndValidate)
	{
		pDX->m_pDlgWnd->GetDlgItem(nIDC)->GetWindowText(s);
		int temp=_wtoi(s.GetBuffer());
		if (temp>=0 && temp<=255)
			value=temp;
		else
			value=0;
	}
	else
	{
		s.Format(TEXT("%d"), value);
		pDX->m_pDlgWnd->GetDlgItem(nIDC)->SetWindowText(s);
	}
}

void DDX_OTFText(CDataExchange *pDX, int nIDC, CHAR &value)
{
	CString s;

	if (pDX->m_bSaveAndValidate)
	{
		pDX->m_pDlgWnd->GetDlgItem(nIDC)->GetWindowText(s);
		int temp=_wtoi(s.GetBuffer());
		if (temp>-128 && temp<128)
			value=temp;
		else
			value=0;
	}
	else
	{
		s.Format(TEXT("%d"), value);
		pDX->m_pDlgWnd->GetDlgItem(nIDC)->SetWindowText(s);
	}
}

void DDX_OTFText(CDataExchange *pDX, int nIDC, WORD &value)
{
	CString s;

	if (pDX->m_bSaveAndValidate)
	{
		pDX->m_pDlgWnd->GetDlgItem(nIDC)->GetWindowText(s);
		int temp=_wtoi(s.GetBuffer());
		if (temp>=0 && temp <=0xffff)
			value=temp;
		else
			value=0;
	}
	else
	{
		s.Format(TEXT("%d"), value);
		pDX->m_pDlgWnd->GetDlgItem(nIDC)->SetWindowText(s);
	}
}

// The start (in hex) and the length (in decimal) of a unicode group: a group is the characters from start to end, and end is
// start + length - 1.
void DDX_OTFTextHex(CDataExchange *pDX, int nIDC, int nIDC2, WORD &value, WORD &value2)
{
	CString s;

	if (pDX->m_bSaveAndValidate)
	{
		WORD newStart = 0;
		pDX->m_pDlgWnd->GetDlgItem(nIDC)->GetWindowText(s);
		s.MakeLower();
		for (int i = 0; i < min(4, s.GetLength()); i++)
		{
			TCHAR c = ((LPCTSTR)s)[i];
			if (c >= '0' && c <= '9')
				newStart = (newStart << 4) + (c - '0');
			else if (c >= 'a' && c <= 'f')
				newStart = (newStart << 4) + (c - 'a' + 10);
			else
				return;
		}
		pDX->m_pDlgWnd->GetDlgItem(nIDC2)->GetWindowText(s);
		int len = _wtoi(s.GetBuffer());
		if (len < 1)
			len = 1;
		if (len > 0xfffe)
			len = 0xfffe;
		int start = newStart, end = start + len - 1;
		if (end > 0xfffe)                      // 0xffff is a reserved code in a font
		{
			end = 0xfffe;
			start = max(0, end - len + 1);
		}
		value = (WORD)start;
		value2 = (WORD)end;
	}
	else
	{
		s.Format(TEXT("%04X"), value);
		pDX->m_pDlgWnd->GetDlgItem(nIDC)->SetWindowText(s);
		s.Format(TEXT("%d"), (int)value2 - (int)value + 1);
		pDX->m_pDlgWnd->GetDlgItem(nIDC2)->SetWindowText(s);
	}
}

// COTFDialog dialog

IMPLEMENT_DYNAMIC(COTFDialog, CDarkDialog)
COTFDialog::COTFDialog(CWnd* pParent /*=NULL*/)
	: CDarkDialog(COTFDialog::IDD, pParent)
	, m_FontName(_T(""))
	, m_PrevZoomIdx(0)
	, m_bUserText(FALSE)
	, m_bSettingText(FALSE)
	, m_bReadingBoxes(FALSE)
	, m_bBaseSet(FALSE)
	, m_GlyphW(0)
	, m_bSplit(FALSE)
	, m_SplitX(0)
	, m_SplitW(0)
	, m_pFirmware(NULL)
	, m_FontIndex(0)
	, m_FontSize(0)
	, m_BitDepth(0)
	, m_Width(0)
	, m_BearingX(0)
	, m_BearingY(0)
	, m_Advance(0)
	, m_GroupEnd(0)
	, m_GroupStart(0)
	, m_GroupOffset(0)
	, m_GroupExplain(_T(""))
	, m_ALeft(0)
	, m_ARight(0)
	, m_BaseLine(0)
{
}

COTFDialog::~COTFDialog()
{
}

void COTFDialog::DoDataExchange(CDataExchange* pDX)
{
	CDarkDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_FONTSIZE_EDIT, m_FontSize);
	DDX_Text(pDX, IDC_FONTSTYLE_EDIT, m_FontStyleID);
	DDX_Control(pDX, IDC_PICTURE, m_FontWnd);
	DDX_Control(pDX, IDC_FONTZOOM, m_ZoomWnd);
	DDX_Control(pDX, IDC_FONTIDX_COMBO, m_FontIndexCombo);
	DDX_Text(pDX, IDC_BITDEPTH_EDIT, m_BitDepth);
	DDX_Control(pDX, IDC_CHAR_COMBO, m_CharCombo);
	DDX_Control(pDX, IDC_GROUPIDX_COMBO, m_GroupIndexCombo);
	DDX_OTFTextHex(pDX, IDC_GROUPSTART_EDIT, IDC_GROUPEND_EDIT, m_GroupStart, m_GroupEnd);
	DDX_OTFText(pDX, IDC_GROUPOFFSET_EDIT, m_GroupOffset);
	DDX_OTFText(pDX, IDC_WIDTH_EDIT, m_Width);
	DDX_OTFText(pDX, IDC_HEIGHT_TEXT, m_Height);
	DDX_OTFText(pDX, IDC_BEARINGX_EDIT, m_BearingX);
	DDX_OTFText(pDX, IDC_BEARINGY_EDIT, m_BearingY);
	DDX_OTFText(pDX, IDC_ADVANCE_EDIT, m_Advance);
	DDX_Control(pDX, IDC_GLYPHIDX_COMBO, m_GlyphCombo);

	DDX_OTFText(pDX, IDC_ALEFT_EDIT, m_ALeft);
	DDX_OTFText(pDX, IDC_ARIGHT_EDIT, m_ARight);
	DDX_OTFText(pDX, IDC_BASELINE_EDIT, m_BaseLine);
}


BEGIN_MESSAGE_MAP(COTFDialog, CDarkDialog)
	ON_CBN_SELCHANGE(IDC_FONTIDX_COMBO, OnCbnSelchangeFontidxCombo)
	ON_BN_CLICKED(IDC_REFRESH_BUTTON, OnBnClickedRefreshButton)
	ON_BN_CLICKED(ID_MAKE_FONT, OnBnClickedMakeFont)
	ON_CBN_SELCHANGE(IDC_CHAR_COMBO, OnCbnSelchangeCharCombo)
	ON_CBN_SELCHANGE(IDC_GROUPIDX_COMBO, OnCbnSelchangeGroupidxCombo)
	ON_BN_CLICKED(ID_ZOOMOUT_BUTTON, OnBnClickedZoomoutButton)
	ON_BN_CLICKED(ID_ZOOMIN_BUTTON, OnBnClickedZoominButton)
	ON_CBN_SELCHANGE(IDC_GLYPHIDX_COMBO, OnCbnSelchangeGlyphidxCombo)
	ON_BN_CLICKED(ID_LOAD_GLYPH, OnBnClickedLoadGlyph)
	ON_BN_CLICKED(ID_SAVE_GLYPH, OnBnClickedSaveGlyph)
	ON_BN_CLICKED(IDC_CHKGRID, OnBnClickedChkgrid)
	ON_WM_CTLCOLOR()
	ON_WM_VSCROLL()
	ON_WM_SIZE()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_WM_CAPTURECHANGED()
	ON_WM_SETCURSOR()
	ON_BN_CLICKED(IDC_OTFPREV_ZIN, OnPreviewZoomIn)
	ON_BN_CLICKED(IDC_OTFPREV_ZOUT, OnPreviewZoomOut)
	ON_BN_CLICKED(IDC_OTFPREV_GRID, ApplyPreviewOptions)
	ON_BN_CLICKED(IDC_OTFPREV_METRICS, ApplyPreviewOptions)
	ON_EN_CHANGE(IDC_ADVANCE_EDIT, OnEnChangeAdvanceEdit)
	ON_EN_CHANGE(IDC_WIDTH_EDIT, OnEnChangeWidthEdit)
	ON_EN_CHANGE(IDC_HEIGHT_TEXT, OnEnChangeHeightEdit)
	ON_EN_CHANGE(IDC_BEARINGY_EDIT, OnEnChangeBearingyEdit)
	ON_EN_CHANGE(IDC_BEARINGX_EDIT, OnEnChangeBearingxEdit)
	ON_EN_CHANGE(IDC_GROUPSTART_EDIT, OnEnChangeGroupstartEdit)
	ON_EN_CHANGE(IDC_GROUPEND_EDIT, OnEnChangeGroupendEdit)
	ON_EN_CHANGE(IDC_GROUPOFFSET_EDIT, OnEnChangeGroupoffsetEdit)
	ON_BN_CLICKED(ID_LOAD_FONT, OnBnClickedLoadFont)
	ON_BN_CLICKED(ID_SAVE_FONT, OnBnClickedSaveFont)
	ON_BN_CLICKED(ID_MULLOAD_GLYPH, OnBnClickedMulloadGlyph)
	ON_MESSAGE(WM_APP, OnSelectMetricsFromBitmap)
	ON_BN_CLICKED(ID_MULSAVE_GLYPH, &COTFDialog::OnBnClickedMulsaveGlyph)
	ON_EN_CHANGE(IDC_OTFTEXT_EDIT, OnEnChangeText)
	ON_CBN_SELCHANGE(IDC_OTF_GROUPLIST, OnCbnSelchangeGroupListCombo)
	ON_BN_CLICKED(IDC_OTFTEXT_RESET, OnBnClickedResetText)
END_MESSAGE_MAP()


// COTFDialog message handlers

BOOL COTFDialog::OnInitDialog()
{
	CDarkDialog::OnInitDialog();

	CaptureBaseLayout();

	// TODO:  Add extra initialization here

	m_ZoomWnd.SetZoomMode(TRUE);
	CheckDlgButton(IDC_OTFPREV_METRICS, BST_CHECKED);
	CheckDlgButton(IDC_CHKGRID, BST_CHECKED);        // the grid of the glyph view is on from the start
	m_ZoomWnd.SetGrid(TRUE);
	m_FontWnd.m_bTextMode = TRUE;
	m_FontWnd.m_pVScroll = (CScrollBar *)GetDlgItem(IDC_OTF_VSCROLL);
	ApplyPreviewOptions();          // the preview shows the text of the box under the font selector
	((CEdit *)GetDlgItem(IDC_OTFTEXT_EDIT))->SetLimitText(0x7FFFFFFE);

	//CheckDlgButton(IDC_CHKGRID, 1);

	m_CharCombo.SendMessage(CCM_SETUNICODEFORMAT, TRUE, 0);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

LRESULT COTFDialog::OnSelectMetricsFromBitmap(WPARAM wParam, LPARAM lParam)
{
	if (wParam == NULL)
		return 0;
	int mg=(int)wParam;
	if (mg!=10)
		return 0;
	int iGlyph=(int)lParam;
	if (iGlyph!=-1)
	{
		m_GlyphCombo.SetCurSel(m_Font.GetIndexFromGlyphTable(iGlyph));
		OnCbnSelchangeGlyphidxCombo();
	}
	return 0;
}

void COTFDialog::SetFirmware(CFirmware *pFirmware, CLayoutDialog *pLayoutDialog)
{
	m_pFirmware = pFirmware;

	m_pLayoutDialog = pLayoutDialog;

	m_FontIndex = 0;

	m_FontIndexCombo.ResetContent();

	// the same list as the other page: the bitmap fonts first, then the OpenType ones (marked OTF)
	CString s;
	CClientDC dc(this);
	CFont *oldFont = dc.SelectObject(GetFont());
	int w = 60;
	DWORD numBitmap = m_pFirmware->GetNumFonts();
	if (m_pFirmware->GetNumOTFFonts() > 0)
		for (DWORD i = 0; i < numBitmap + m_pFirmware->GetNumOTFFonts(); i++)
		{
			s = CombinedFontLabel(m_pFirmware, i);
			m_FontIndexCombo.InsertString(i, s);
			w = max(w, dc.GetTextExtent(s).cx + 24);
		}
	dc.SelectObject(oldFont);
	m_FontIndexCombo.SetDroppedWidth(w);
	if (m_pFirmware->GetNumOTFFonts() == 0)
	{
		// nothing to choose from: the box says so, and what is left of the font before is cleared
		m_FontIndexCombo.InsertString(0, TEXT("No OTF fonts found"));
		m_FontIndexCombo.SetCurSel(0);
		m_FontIndexCombo.EnableWindow(FALSE);
		m_FontWnd.SetOTFFont(NULL);          // (the font that it showed is in the firmware that was loaded before)
		m_ZoomWnd.SetOTFFont(NULL);
		SetDlgItemText(IDC_FONTSTYLE_EDIT, TEXT(""));
		SetDlgItemText(IDC_FONTSTYLENAME_STATIC, TEXT(""));
		SetDlgItemText(IDC_FONTSIZE_EDIT, TEXT(""));
		SetDlgItemText(IDC_BITDEPTH_EDIT, TEXT(""));
		static const UINT kButtons[] = { ID_LOAD_FONT, ID_SAVE_FONT, ID_MAKE_FONT, ID_LOAD_GLYPH, ID_SAVE_GLYPH, ID_MULLOAD_GLYPH,
			ID_MULSAVE_GLYPH, IDC_REFRESH_BUTTON, ID_ZOOMIN_BUTTON, ID_ZOOMOUT_BUTTON, IDC_CHKGRID };
		for (int k = 0; k < sizeof(kButtons) / sizeof(kButtons[0]); k++)
			GetDlgItem(kButtons[k])->EnableWindow(FALSE);
		m_GroupIndexCombo.ResetContent();
		m_GlyphCombo.ResetContent();
		m_CharCombo.ResetContent();
		m_bSettingText = TRUE;
		SetDlgItemText(IDC_OTFTEXT_EDIT, TEXT(""));
		m_bSettingText = FALSE;
		m_bUserText = FALSE;
		return;
	}
	m_FontIndexCombo.EnableWindow(TRUE);
	m_FontIndexCombo.SetCurSel(numBitmap);        // (the first OpenType font)

	if (theApp.m_OTFRender==FALSE)
		UpdateFont();
}

void COTFDialog::UpdateFont()
{
	WORD i;

	LPBYTE lpBuffer = m_pFirmware->GetOTFFont(m_FontIndex);
	if (lpBuffer == NULL)
		return;

	if (!m_Font.Read(lpBuffer))
		return;

	m_FontName = m_Font.GetFontName();
	m_FontSize = m_Font.GetFontSize();
	m_BitDepth = m_Font.GetFontBitDepth();
	m_FontStyle = m_Font.GetFontStyle();
	// the number is the one that the layouts use for an OpenType font, which is the other way round than for the bitmap fonts: Bold 0,
	// Regular 1. (On the 5G the types of Podium Sans, sizes 14 to 28, all have style 1, and the OpenType Podium Sans of that firmware is
	// a Regular font; every OpenType font of the 5G, 5.5G and nano is Regular. The bitmap fonts use Regular 0, Bold 1.)
	if (m_FontStyle == TEXT("Bold"))
		m_FontStyleID = TEXT("0");
	else if (m_FontStyle == TEXT("Regular"))
		m_FontStyleID = TEXT("1");
	else
		m_FontStyleID = TEXT("?");

	UpdateData(FALSE);
	SetDlgItemText(IDC_FONTSTYLENAME_STATIC, m_FontStyle);

	m_FontWnd.SetOTFFont(&m_Font);
	m_ZoomWnd.SetOTFFont(&m_Font);
	RebuildText(FALSE);

	m_FontIndexCombo.SetCurSel(m_pFirmware->GetNumFonts() + m_FontIndex);

	//

	FillGroupCombo(0);
	ReadUnicodeGroup();

	CComboFill::Begin(m_GlyphCombo, m_Font.GetNumGlyphs(), 5);
	for (i = 1; i <= m_Font.GetNumGlyphs(); i++)
		CComboFill::AddNumber(m_GlyphCombo, i);
	CComboFill::End(m_GlyphCombo);

	m_GlyphCombo.SetCurSel(m_GroupOffset-1);

	//

	UpdateCharCombo();
	OnCbnSelchangeGlyphidxCombo();

	BOOL bCanWrite = m_pFirmware->CanWriteOTFFont(m_FontIndex);

	GetDlgItem(ID_SAVE_FONT)->EnableWindow(TRUE);
	GetDlgItem(ID_LOAD_FONT)->EnableWindow(bCanWrite);
	GetDlgItem(ID_SAVE_GLYPH)->EnableWindow(TRUE);
	GetDlgItem(ID_LOAD_GLYPH)->EnableWindow(bCanWrite);
	GetDlgItem(ID_MAKE_FONT)->EnableWindow(bCanWrite);
	GetDlgItem(ID_ZOOMIN_BUTTON)->EnableWindow(TRUE);
	GetDlgItem(ID_ZOOMOUT_BUTTON)->EnableWindow(TRUE);
	GetDlgItem(IDC_CHKGRID)->EnableWindow(TRUE);
	GetDlgItem(IDC_REFRESH_BUTTON)->EnableWindow(TRUE);
	GetDlgItem(ID_MULLOAD_GLYPH)->EnableWindow(TRUE);
	GetDlgItem(ID_MULSAVE_GLYPH)->EnableWindow(TRUE);
}

void COTFDialog::UpdateZoomView()
{
	m_ZoomWnd.SetMetrics(m_Height, m_Width, m_BearingX, m_BearingY, m_Advance);
}

void COTFDialog::ReadUnicodeGroup()
{
	int i = m_GroupIndexCombo.GetCurSel();

	m_Font.GetUnicodeGroup(i, &m_GroupStart, &m_GroupEnd, &m_GroupOffset);

	GetDlgItem(IDC_GROUPOFFSET_EDIT)->EnableWindow(m_Font.GetUnicodeGroupGlyphState(i));

	UpdateGroupExplanation();
	FillGroupListCombo();
}

void COTFDialog::UpdateCharCombo()
{
	DWORD n = m_Font.GetNumUnicodeChars();
	if (n > 0x10000)
		n = 0x10000;

	CComboFill::Begin(m_CharCombo, n, 12);
	wchar_t w[64];
	WORD c, group;
	for (DWORD i = 0; i < n; i++)
	{
		c = m_Font.GetUnicodeChar((WORD)i, &group);
		swprintf(w, L"U+%04X (%c)", c, c);
		CComboFill::AddText(m_CharCombo, w);
	}
	CComboFill::End(m_CharCombo);

	m_CharCombo.SetCurSel(0);
}

void COTFDialog::UpdateGroupExplanation()
{
	// (the explanation of the group is not shown any more; the boxes are filled from the group)
	BOOL bWas = m_bReadingBoxes;
	m_bReadingBoxes = TRUE;
	UpdateData(FALSE);
	m_bReadingBoxes = bWas;
}

void COTFDialog::OnBnClickedRefreshButton()
{
	m_FontWnd.Invalidate();
}

void COTFDialog::OnCbnSelchangeFontidxCombo()
{
	DWORD sel = (DWORD)m_FontIndexCombo.GetCurSel();
	DWORD numBitmap = m_pFirmware->GetNumFonts();
	if (sel < numBitmap)
	{
		// a bitmap font: the other page of this tab edits it (the editor dialog swaps the pages)
		m_FontIndexCombo.SetCurSel((int)(numBitmap + m_FontIndex));       // (this page keeps its own font)
		::SendMessage(::GetParent(::GetParent(m_hWnd)), WM_APP, (WPARAM)14, (LPARAM)sel);
		return;
	}
	m_FontIndex = sel - numBitmap;

	UpdateFont();
}

void COTFDialog::OnOK()
{
	// TODO: Add your specialized code here and/or call the base class

	// CDarkDialog::OnOK();
}

void COTFDialog::OnBnClickedMakeFont()
{
	CFontDialog dlg;

	if (dlg.DoModal() != IDOK)
		return;

	LOGFONT lf;
	memcpy(&lf, dlg.m_cf.lpLogFont, sizeof(LOGFONT));

	if (m_BitDepth > 1)
		lf.lfQuality = ANTIALIASED_QUALITY;

	CFont font;
	VERIFY(font.CreateFontIndirect(&lf));

	//

	CDC dc;
	CDC *pDC = GetWindowDC();
	dc.CreateCompatibleDC(pDC);
	
	CBitmap bmp;
	bmp.CreateCompatibleBitmap(pDC, 60, 60);

	CBitmap *pBitmap = dc.SelectObject(&bmp);
	CFont *pOldFont = dc.SelectObject(&font);

	dc.SetTextColor(RGB(0, 0, 0));
	dc.SetBkColor(RGB(255 ,255, 255));

	//

	WORD xoffset = 0;

	int height;
	wchar_t c[2];
	WORD group;

	c[1] = 0;

	DWORD imagedatalen,totalbytes=0;
	imagedatalen=m_Font.GetGlyphDataLen();

	TEXTMETRIC tm;
	GetTextMetrics(dc.m_hDC, &tm);

	m_Font.ResetGlyphData();

	for (int i = 0; i < m_Font.GetNumUnicodeChars(); i++)
	{
		c[0] = m_Font.GetUnicodeChar(i, &group);
		
		CRect rect(0, 0, 60, 60);
		::DrawTextW(dc.m_hDC, c, 1, &rect, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT);
		height = ::DrawTextW(dc.m_hDC, c, 1, &rect, DT_LEFT | DT_TOP | DT_NOPREFIX | DT_SINGLELINE);

		ABCFLOAT abc;
		GetCharABCWidthsFloat(dc.m_hDC, c[0], c[0], &abc);

		SHORT abcA = (SHORT)abc.abcfA;
		SHORT abcB = (SHORT)abc.abcfB;
		SHORT abcC = (SHORT)abc.abcfC;

		// analyze

		int x, y;
		int wstart = -1, wend = -1, hstart = -1, hend = -1;
		BOOL bLineEmpty;

		//gets the start and end line for the actual char inside the 60 by 60 rect
		for (x = 0; x < rect.right; x++)
		{
			bLineEmpty = TRUE;
			for (y = 0; y < height; y++)
			{
				if (dc.GetPixel(x, y) != RGB(255, 255, 255))
				{
					bLineEmpty = FALSE;
					break;
				}
			}
			if (!bLineEmpty)
			{
				wstart = x;
				break;
			}
		}

		for (x = rect.right - 1; x >= 0 && wstart != -1; x--)
		{
			bLineEmpty = TRUE;
			for (y = 0; y < height; y++)
			{
				if (dc.GetPixel(x, y) != RGB(255, 255, 255))
				{
					bLineEmpty = FALSE;
					break;
				}
			}
			if (!bLineEmpty)
			{
				wend = x;
				break;
			}
		}
		if (wstart!=-1)
			wend++;

		for (y = 0; y < rect.bottom; y++)
		{
			bLineEmpty = TRUE;
			for (x = wstart; x < wend; x++)
			{
				if (dc.GetPixel(x, y) != RGB(255, 255, 255))
				{
					bLineEmpty = FALSE;
					break;
				}
			}
			if (!bLineEmpty)
			{
				hstart = y;
				break;
			}
		}

		for (y = rect.bottom - 1; y >= 0 && hstart != -1; y--)
		{
			bLineEmpty = TRUE;
			for (x = wstart; x < wend ; x++)
			{
				if (dc.GetPixel(x, y) != RGB(255, 255, 255))
				{
					bLineEmpty = FALSE;
					break;
				}
			}
			if (!bLineEmpty)
			{
				hend = y;
				break;
			}
		}
		if (hstart!=-1)
			hend++;

		WORD glyphidx=m_Font.GetUnicodeCharOffset(c[0]);

		//check that we don't override and if the size is too big, stop
		totalbytes += sizeof(smallGlyphMetrics);
		if (wstart!=-1 && hstart!=-1)
		{
			if ((wend - wstart)%2==0)
				totalbytes += (hend - hstart)*(wend - wstart)/(8/m_BitDepth);
			else
				totalbytes += (hend - hstart)*(wend - wstart + 1)/(8/m_BitDepth);
		}

		if (totalbytes >= imagedatalen)
		{
			m_Font.FillEndGlyphs(i);
			break;
		}

		// update metrics
		//bearingx = abcA
		//advance = abcA + abcB + abcC
		//width = abcB
		//bearingy = tm.tmAscent - hstart
		//height = hend - hstart
		if (hstart==-1 || wstart==-1)
			m_Font.AddCharMetrics(glyphidx, 0, 0, (CHAR)abcA, 0, (BYTE)(abcA + abcB + abcC));
		else
			m_Font.AddCharMetrics(glyphidx, wend - wstart, hend - hstart, (CHAR)abcA, (CHAR)(tm.tmAscent - hstart), (BYTE)(abcA + abcB + abcC));

		// copy
		if (abcB!=0)
		{
			for (x = wstart; x < wend; x++)
			{
				for (y = hstart; y < hend; y++)
				{
					m_Font.SetFontPixel(glyphidx, x - wstart, y - hstart, dc.GetPixel(x, y));
				}
			}
		}
	}

	//

	dc.SelectObject(pOldFont);
	dc.SelectObject(pBitmap);
	
	//

	ReleaseDC(pDC);

	UpdateFont();
	//UpdateData(FALSE);
}
void COTFDialog::OnCbnSelchangeCharCombo()
{
	int i = m_CharCombo.GetCurSel();

	WORD group;
	WORD unicodeChar = m_Font.GetUnicodeChar(i, &group);

	// update group
	m_GroupIndexCombo.SetCurSel(group);
	ReadUnicodeGroup();

	// update char
	WORD offset = m_Font.GetUnicodeCharOffset(unicodeChar);

	for (i=0;i<m_Font.GetNumGlyphs();i++)
	{
		if (m_Font.GetGlyphIndex(i)==offset)
		{
			m_GlyphCombo.SetCurSel(i);
			break;
		}
	}
	ReadCharMetrics();
	UpdateZoomView();
}

void COTFDialog::ReadCharMetrics()
{
	int i = m_GlyphCombo.GetCurSel();

	m_GlyphIndex = m_Font.GetGlyphIndex(i);

	BYTE width=0,height=0,advance=0;
	CHAR bearingx=0,bearingy=0;

	m_Font.GetCharMetrics(m_GlyphIndex, &width, &height, &bearingx, &bearingy, &advance);

	m_Width = width;
	m_Height = height;
	m_BearingX =  bearingx;
	m_BearingY = bearingy;
	m_Advance = advance;

	m_ZoomWnd.SetGlyph(m_GlyphIndex);

	GetDlgItem(IDC_WIDTH_EDIT)->EnableWindow(TRUE);
	GetDlgItem(IDC_BEARINGX_EDIT)->EnableWindow(TRUE);
	GetDlgItem(IDC_BEARINGY_EDIT)->EnableWindow(TRUE);
	//GetDlgItem(IDC_UPDATE_BUTTON)->EnableWindow(bEnable);

	BOOL bWas = m_bReadingBoxes;
	m_bReadingBoxes = TRUE;
	UpdateData(FALSE);
	m_bReadingBoxes = bWas;
}

void COTFDialog::WriteUnicodeGroup()
{
	if (m_bReadingBoxes)
		return;
	UpdateData(TRUE);

	int i = m_GroupIndexCombo.GetCurSel();

	if (i < 0)
		return;

	m_Font.SetUnicodeGroup(i, m_GroupStart, m_GroupEnd, m_GroupOffset-1);

	//if sorting then change the index:
	WORD gstart,gend,goffset;
	for (i=0;i<m_GroupIndexCombo.GetCount();i++)
	{
		m_Font.GetUnicodeGroup(i, &gstart, &gend, &goffset);
		if (gend==m_GroupEnd && gstart==m_GroupStart && goffset==m_GroupOffset)
		{
			m_GroupIndexCombo.SetCurSel(i);
			break;
		}
	}
	FillGroupCombo(m_GroupIndexCombo.GetCurSel());
	//

	UpdateGroupExplanation();

	UpdateCharCombo();
	UpdateZoomView();
}

void COTFDialog::WriteCharMetrics()
{
	if (m_bReadingBoxes)
		return;
	UpdateData(TRUE);

	int i=m_GlyphCombo.GetCurSel();
	if (i < 0)
		return;
	WORD glyph = m_Font.GetGlyphIndex(i);

	// nothing is written when the numbers are the ones that the glyph has
	BYTE width=0,height=0,advance=0;
	CHAR bearingx=0,bearingy=0;
	m_Font.GetCharMetrics(glyph, &width, &height, &bearingx, &bearingy, &advance);
	if (width == m_Width && height == m_Height && bearingx == m_BearingX && bearingy == m_BearingY && advance == m_Advance)
		return;

	m_Font.SetCharMetrics(glyph, m_Width, m_Height, m_BearingX, m_BearingY, m_Advance);

	UpdateZoomView();
	m_FontWnd.SetOTFFont(&m_Font);       // (the text is placed with the new numbers)
}

void COTFDialog::OnCbnSelchangeGroupidxCombo()
{
	ReadUnicodeGroup();

	// update char mapping

	m_GlyphCombo.SetCurSel(m_GroupOffset-1);
	ReadCharMetrics();
	UpdateZoomView();
}

void COTFDialog::OnBnClickedZoomoutButton()
{
	m_ZoomWnd.ChangeZoom(FALSE);
	CString temp;
	temp.Format(TEXT("x%d"), m_ZoomWnd.GetZoom());
	GetDlgItem(IDC_STATIC_ZOOM)->SetWindowText(temp);
}

void COTFDialog::OnBnClickedZoominButton()
{
	m_ZoomWnd.ChangeZoom(TRUE);
	CString temp;
	temp.Format(TEXT("x%d"), m_ZoomWnd.GetZoom());
	GetDlgItem(IDC_STATIC_ZOOM)->SetWindowText(temp);
}

void COTFDialog::OnCbnSelchangeGlyphidxCombo()
{
	ReadCharMetrics();
	UpdateZoomView();
	m_FontWnd.SelectGlyph((WORD)m_GlyphIndex);
}

// The text box starts with every character that the font has (its groups; control characters left out). What the user types
// stays when another font is chosen, until "Reset Text".
void COTFDialog::RebuildText(BOOL bForce)
{
	CString text;
	if (m_bUserText && !bForce)
		GetDlgItemText(IDC_OTFTEXT_EDIT, text);
	else
	{
		DWORD n = m_Font.GetNumUnicodeChars();
		if (n > 0x10000)
			n = 0x10000;
		for (DWORD i = 0; i < n; i++)
		{
			WORD group;
			WORD c = m_Font.GetUnicodeChar((WORD)i, &group);
			if (c >= 0x20 && c != 0x7F)
				text += (WCHAR)c;
		}
		m_bSettingText = TRUE;
		SetDlgItemText(IDC_OTFTEXT_EDIT, text);
		m_bSettingText = FALSE;
		m_bUserText = FALSE;
	}
	m_FontWnd.SetText(text);
}

// Ctrl+A selects all the text of an edit box (a single line edit box does not do it by itself)
BOOL COTFDialog::PreTranslateMessage(MSG *pMsg)
{
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000))
	{
		HWND hFocus = ::GetFocus();
		if (hFocus && ::IsChild(m_hWnd, hFocus))
		{
			TCHAR cls[16];
			::GetClassName(hFocus, cls, 16);
			if (_tcsicmp(cls, _T("Edit")) == 0)
			{
				::SendMessage(hFocus, EM_SETSEL, 0, -1);
				return TRUE;
			}
		}
	}
	return CDarkDialog::PreTranslateMessage(pMsg);
}

// "index  Block name" for every group (the block of the first character of the group)
void COTFDialog::FillGroupCombo(int sel)
{
	m_GroupIndexCombo.SetRedraw(FALSE);
	m_GroupIndexCombo.ResetContent();
	int widest = 0;
	CClientDC dc(this);
	CFont *oldFont = dc.SelectObject(m_GroupIndexCombo.GetFont());
	for (WORD i = 0; i < m_Font.GetNumUnicodeGroups(); i++)
	{
		WORD start, end, offset;
		m_Font.GetUnicodeGroup(i, &start, &end, &offset);
		CString s;
		s.Format(_T("%u  %s"), (unsigned)i, GetUnicodeBlockName(start));
		s.TrimRight();
		m_GroupIndexCombo.AddString(s);
		widest = max(widest, dc.GetTextExtent(s).cx);
	}
	dc.SelectObject(oldFont);
	m_GroupIndexCombo.SetDroppedWidth(widest + 8 + GetSystemMetrics(SM_CXVSCROLL));
	m_GroupIndexCombo.SetCurSel(sel < 0 ? 0 : sel);
	m_GroupIndexCombo.SetRedraw(TRUE);
	m_GroupIndexCombo.Invalidate();
}

// every character of the group with its entry (the glyph index of the group + the place of the character in the group)
void COTFDialog::FillGroupListCombo()
{
	CComboBox *list = (CComboBox *)GetDlgItem(IDC_OTF_GROUPLIST);
	if (!list)
		return;
	DWORD n = (DWORD)m_GroupEnd - m_GroupStart + 1;
	if (m_GroupEnd < m_GroupStart || n > 0x10000)
		n = 0;
	CComboFill::Begin(*list, n, 16);
	WCHAR w[48];
	for (DWORD i = 0; i < n; i++)
	{
		DWORD ch = (DWORD)m_GroupStart + i;
		if (ch >= 0x20 && ch != 0x7F && ch < 0x10000)
			swprintf(w, 48, L"%u  U+%04X (%c)", (unsigned)m_GroupOffset + i, ch, (WCHAR)ch);
		else
			swprintf(w, 48, L"%u  U+%04X", (unsigned)m_GroupOffset + i, ch);
		CComboFill::AddText(*list, w);
	}
	CComboFill::End(*list);
	list->SetCurSel(n ? 0 : -1);
}

// picking a character of the group selects it in the mapping (and shows its glyph)
void COTFDialog::OnCbnSelchangeGroupListCombo()
{
	CComboBox *list = (CComboBox *)GetDlgItem(IDC_OTF_GROUPLIST);
	int pos = list ? list->GetCurSel() : -1;
	int g = m_GroupIndexCombo.GetCurSel();
	if (pos < 0 || g < 0)
		return;
	// the place of the character among all the characters of the font (the list of the Character box)
	int idx = pos;
	for (int i = 0; i < g; i++)
	{
		WORD s, e, o;
		m_Font.GetUnicodeGroup((WORD)i, &s, &e, &o);
		idx += (int)e - (int)s + 1;
	}
	if (idx >= m_CharCombo.GetCount())
		return;
	m_CharCombo.SetCurSel(idx);
	OnCbnSelchangeCharCombo();
}

void COTFDialog::OnEnChangeText()
{
	if (m_bSettingText)
		return;
	m_bUserText = TRUE;
	CString text;
	GetDlgItemText(IDC_OTFTEXT_EDIT, text);
	m_FontWnd.SetText(text);
}

void COTFDialog::OnBnClickedResetText()
{
	RebuildText(TRUE);
}

void COTFDialog::OnBnClickedLoadGlyph()
{
	CFileDialog dlg(TRUE, 0, 0, OFN_HIDEREADONLY, TEXT("Bitmap Files (*.bmp, *.gif, *.jpg)|*.bmp;*.gif;*.jpg||"), this);
	MO_LOAD_RESOURCES_PATH(dlg)

	if (dlg.DoModal() != IDOK)
		return;
	MO_SAVE_RESOURCES_PATH(dlg)
	CImage img;
	if (FAILED(img.Load(dlg.GetPathName())))
	{
		MessageBox(TEXT("Unable to load image!"));
		return;
	}

	//CSize size = m_Font.GetFontBitmapSize(m_GlyphIndex);

	m_Width = img.GetWidth();
	m_Height = img.GetHeight();
	m_Font.SetCharMetrics(m_GlyphIndex, m_Width, m_Height, 0, m_Height, m_Width);

	LONG x, y;
	COLORREF color;

	for (x = 0; x < img.GetWidth(); x++)
	{
		for (y = 0; y < img.GetHeight(); y++)
		{
			color = img.GetPixel(x, y);
			m_Font.SetFontPixel(m_GlyphIndex, x, y, color);
		}
	}

	UpdateData(FALSE);

	ReadCharMetrics();
	UpdateZoomView();
}

void COTFDialog::OnBnClickedSaveGlyph()
{
	CString filename;
	filename.Format(TEXT("%s-%d.bmp"), m_Font.GetFontName(), m_GlyphIndex);

	CFileDialog dlg(FALSE, TEXT("bmp"), filename, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, TEXT("BMP Files (*.bmp)|*.bmp||"), this);
	MO_LOAD_RESOURCES_PATH(dlg)

	if (dlg.DoModal() != IDOK)
		return;
	MO_SAVE_RESOURCES_PATH(dlg)
	CSize size = m_Font.GetFontBitmapSize(m_GlyphIndex);

	CImage img;

	if (!img.Create(size.cx, size.cy, 24))
	{
		MessageBox(TEXT("Unable to create image!"));
		return;
	}

	int x, y;

	for (x = 0; x < size.cx; x++)
	{
		for (y = 0; y < size.cy; y++)
		{
			img.SetPixel(x, y, m_Font.GetFontPixel(m_GlyphIndex, CPoint(x, y)));
		}
	}

	if (FAILED(img.Save(dlg.GetPathName())))
	{
		MessageBox(TEXT("Unable to save image!"));
		return;
	}
}

void COTFDialog::OnBnClickedChkgrid()
{
	if (IsDlgButtonChecked(IDC_CHKGRID) == 0)
		m_ZoomWnd.SetGrid(FALSE);
	else
		m_ZoomWnd.SetGrid(TRUE);
}

// ---- the size of the page ------------------------------------------------------------------------------------------------

void COTFDialog::CaptureBaseLayout()
{
	GetClientRect(&m_BaseClient);
	m_BaseCtls.clear();
	for (CWnd *w = GetWindow(GW_CHILD); w; w = w->GetWindow(GW_HWNDNEXT))
	{
		BaseCtl b;
		b.hwnd = w->m_hWnd;
		w->GetWindowRect(&b.rc);
		ScreenToClient(&b.rc);
		m_BaseCtls.push_back(b);
		if (w->GetDlgCtrlID() == IDC_FONTZOOM)
			m_GlyphW = b.rc.Width();
	}
	m_bBaseSet = TRUE;
}

void COTFDialog::OnSize(UINT nType, int cx, int cy)
{
	CDarkDialog::OnSize(nType, cx, cy);
	if (!m_bBaseSet || nType == SIZE_MINIMIZED || cx <= 0 || cy <= 0)
		return;

	CRect marks(151, 186, 366, 0);          // left of the Glyph Mapping box, top of the boxes at the bottom, left of the buttons on the right
	MapDialogRect(&marks);
	int xMap = marks.left, yBoxes = marks.top, xBar = marks.right;

	CRect fp(0, 0, 0, 0), gp(0, 0, 0, 0);
	int metricsRight = 0;
	CRect sbBase(0, 0, 0, 0);
	for (size_t i = 0; i < m_BaseCtls.size(); i++)
	{
		int id = ::GetDlgCtrlID(m_BaseCtls[i].hwnd);
		if (id == IDC_PICTURE)
			fp = m_BaseCtls[i].rc;
		else if (id == IDC_FONTZOOM)
			gp = m_BaseCtls[i].rc;
		else if (id == IDC_OTFPREV_METRICS)
			metricsRight = m_BaseCtls[i].rc.right;
		else if (id == IDC_OTF_VSCROLL)
			sbBase = m_BaseCtls[i].rc;
	}
	int dx = cx - m_BaseClient.Width(), dy = cy - m_BaseClient.Height();
	dy = max(dy, 60 - fp.Height());                   // the previews never get smaller than what they need to be of use
	int gap = gp.left - sbBase.right;                  // the line between the scroll bar of the font preview and the glyph view
	int totalW = (gp.right - fp.left) + dx;           // both previews and the line between them
	// the font preview is at least as wide as its options (to the end of the Metrics box): the zoom of the glyph view would
	// otherwise be over them
	int minFont = max(60, metricsRight - fp.left);
	totalW = max(totalW, minFont + gap + 40);
	int glyphW = min(max(m_GlyphW, 40), totalW - gap - minFont);
	if (m_bSplit)
		m_GlyphW = glyphW;                            // (the line stops where the previews cannot get smaller)
	int glyphLeft = fp.left + totalW - glyphW;

	int sbW = sbBase.Width();
	CString t;
	for (size_t i = 0; i < m_BaseCtls.size(); i++)
	{
		HWND h = m_BaseCtls[i].hwnd;
		if (!::IsWindow(h))
			continue;
		CRect r = m_BaseCtls[i].rc;
		int id = ::GetDlgCtrlID(h);
		if (id == IDC_PICTURE)
		{
			r.right = glyphLeft - gap - sbW;
			r.bottom += dy;
		}
		else if (id == IDC_OTF_VSCROLL)
		{
			r.left = glyphLeft - gap - sbW;
			r.right = glyphLeft - gap;
			r.bottom += dy;
		}
		else if (id == IDC_FONTZOOM)
		{
			r.left = glyphLeft;
			r.right = glyphLeft + glyphW;
			r.bottom += dy;
		}
		else if (id == ID_ZOOMOUT_BUTTON || id == ID_ZOOMIN_BUTTON || id == IDC_STATIC_ZOOM || id == IDC_CHKGRID)
			r.OffsetRect(glyphLeft - gp.left, 0);             // (the zoom of the glyph view stays at its left edge)
		else if (id == IDC_OTFTEXT_EDIT)
			r.right += dx;
		else if (id == IDC_OTFTEXT_RESET || id == IDC_BITDEPTH_EDIT)
			r.OffsetRect(dx, 0);
		else if (r.top >= yBoxes)
		{
			r.OffsetRect(0, dy);
			if (r.left >= xMap)
				r.OffsetRect(dx, 0);
			else
			{
				::GetWindowText(h, t.GetBuffer(64), 64);
				t.ReleaseBuffer();
				if (t == _T("1. Unicode Groups"))
					r.right += dx;
			}
		}
		else if (r.left >= xBar)
			r.OffsetRect(dx, 0);
		else
		{
			// the label of the bit depth (the one text between the top row and the buttons that moves)
			::GetWindowText(h, t.GetBuffer(64), 64);
			t.ReleaseBuffer();
			if (t == _T("Bit depth:"))
				r.OffsetRect(dx, 0);
			else
				continue;
		}
		::SetWindowPos(h, NULL, r.left, r.top, r.Width(), r.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
	}
	m_FontWnd.OnResized();
	m_ZoomWnd.OnResized();
	Invalidate();
}

void COTFDialog::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
	if (pScrollBar && pScrollBar->GetDlgCtrlID() == IDC_OTF_VSCROLL)
	{
		m_FontWnd.ScrollText(nSBCode);
		return;
	}
	CDarkDialog::OnVScroll(nSBCode, nPos, pScrollBar);
}

// the line between the font preview and the glyph view: a few pixels wide, as high as the previews
BOOL COTFDialog::SplitterRect(CRect &rc)
{
	if (!m_FontWnd.GetSafeHwnd() || !m_ZoomWnd.GetSafeHwnd())
		return FALSE;
	CRect a, b;
	// the line is between the scroll bar of the font preview and the glyph view (not on the scroll bar)
	CWnd *sb = GetDlgItem(IDC_OTF_VSCROLL);
	if (sb)
		sb->GetWindowRect(&a);
	else
		m_FontWnd.GetWindowRect(&a);
	m_ZoomWnd.GetWindowRect(&b);
	ScreenToClient(&a);
	ScreenToClient(&b);
	rc = CRect(a.right, a.top, b.left, a.bottom);
	if (rc.Width() < 4)
		rc.InflateRect((4 - rc.Width() + 1) / 2, 0);        // (thin, but it can be hit)
	return b.left > a.right - 1;
}

void COTFDialog::OnLButtonDown(UINT nFlags, CPoint point)
{
	CRect rc;
	if (SplitterRect(rc) && rc.PtInRect(point))
	{
		m_bSplit = TRUE;
		m_SplitX = point.x;
		m_SplitW = m_GlyphW;
		SetCapture();
		return;
	}
	CDarkDialog::OnLButtonDown(nFlags, point);
}

void COTFDialog::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_bSplit && (nFlags & MK_LBUTTON))
	{
		int w = m_SplitW - (point.x - m_SplitX);       // the glyph view grows when the line goes to the left
		if (w != m_GlyphW)
		{
			m_GlyphW = max(w, 40);
			CRect cr;
			GetClientRect(&cr);
			OnSize(0, cr.Width(), cr.Height());
			m_GlyphW = max(m_GlyphW, 40);
		}
		return;
	}
	CDarkDialog::OnMouseMove(nFlags, point);
}

void COTFDialog::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_bSplit)
	{
		m_bSplit = FALSE;
		if (GetCapture() == this)
			ReleaseCapture();
		return;
	}
	CDarkDialog::OnLButtonUp(nFlags, point);
}

void COTFDialog::OnCaptureChanged(CWnd *pWnd)
{
	m_bSplit = FALSE;
	CDarkDialog::OnCaptureChanged(pWnd);
}

BOOL COTFDialog::OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT message)
{
	CRect rc;
	CPoint pt;
	::GetCursorPos(&pt);
	ScreenToClient(&pt);
	if (m_bSplit || (SplitterRect(rc) && rc.PtInRect(pt)))
	{
		::SetCursor(::LoadCursor(NULL, IDC_SIZEWE));
		return TRUE;
	}
	return CDarkDialog::OnSetCursor(pWnd, nHitTest, message);
}

// The boxes of the glyph are in the colour of their marks in the glyph view (see OTFFontWnd.h): BearingX red (the line at the
// pen), Advance green (the line at the end of the advance), BearingY orange (the baseline), Width blue and Height purple (the two
// brackets).
HBRUSH COTFDialog::OnCtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CDarkDialog::OnCtlColor(pDC, pWnd, nCtlColor);
	UINT id = pWnd->GetDlgCtrlID();
	int k = (id == IDC_BEARINGX_EDIT) ? 0 : (id == IDC_BEARINGY_EDIT) ? 1 : (id == IDC_ADVANCE_EDIT) ? 2 : (id == IDC_WIDTH_EDIT) ? 3 : (id == IDC_HEIGHT_TEXT) ? 4 : -1;
	if (k < 0 || (nCtlColor != CTLCOLOR_EDIT && nCtlColor != CTLCOLOR_STATIC))
		return hbr;
	BOOL dark = DarkMode::Enabled();
	static const COLORREF light[5] = { RGB(255, 214, 214), RGB(255, 228, 200), RGB(214, 244, 214), RGB(214, 226, 255), RGB(236, 214, 248) };
	static const COLORREF darkc[5] = { RGB(110, 40, 40), RGB(120, 70, 20), RGB(35, 90, 45), RGB(40, 60, 120), RGB(85, 40, 110) };
	COLORREF back = dark ? darkc[k] : light[k];
	static CBrush *brushes[5] = { NULL, NULL, NULL, NULL, NULL };
	static COLORREF colors[5] = { 0, 0, 0, 0, 0 };
	if (!brushes[k] || colors[k] != back)
	{
		delete brushes[k];
		brushes[k] = new CBrush(back);
		colors[k] = back;
	}
	pDC->SetBkColor(back);
	pDC->SetTextColor(dark ? RGB(240, 240, 240) : RGB(0, 0, 0));
	return (HBRUSH)brushes[k]->GetSafeHandle();
}

// The options of the text preview, like the ones of the preview of the bitmap fonts: zoom, grid, metrics (the marks of the selected
// glyph) and ruler (the baseline of every line).
static const int kOtfPrevZooms[] = { 1, 2, 3, 4, 6, 8, 10, 12, 16, 20 };
static const int kNumOtfPrevZooms = sizeof(kOtfPrevZooms) / sizeof(kOtfPrevZooms[0]);

void COTFDialog::ApplyPreviewOptions()
{
	CString t;
	t.Format(_T("x%d"), kOtfPrevZooms[m_PrevZoomIdx]);
	SetDlgItemText(IDC_OTFPREV_ZTXT, t);
	m_FontWnd.SetPreviewOptions(kOtfPrevZooms[m_PrevZoomIdx], IsDlgButtonChecked(IDC_OTFPREV_GRID) != 0,
	                            IsDlgButtonChecked(IDC_OTFPREV_METRICS) != 0);
}

void COTFDialog::OnPreviewZoomIn()
{
	m_PrevZoomIdx = min(m_PrevZoomIdx + 1, kNumOtfPrevZooms - 1);
	ApplyPreviewOptions();
}

void COTFDialog::OnPreviewZoomOut()
{
	m_PrevZoomIdx = max(m_PrevZoomIdx - 1, 0);
	ApplyPreviewOptions();
}

void COTFDialog::OnEnChangeWidthEdit()
{
	WriteCharMetrics();
}

void COTFDialog::OnEnChangeHeightEdit()
{
	WriteCharMetrics();
}

void COTFDialog::OnEnChangeAdvanceEdit()
{
	WriteCharMetrics();
}

void COTFDialog::OnEnChangeBearingyEdit()
{
	WriteCharMetrics();
}

void COTFDialog::OnEnChangeBearingxEdit()
{
	WriteCharMetrics();
}

void COTFDialog::OnEnChangeGroupstartEdit()
{
	WriteUnicodeGroup();
}

void COTFDialog::OnEnChangeGroupendEdit()
{
	WriteUnicodeGroup();
}

void COTFDialog::OnEnChangeGroupoffsetEdit()
{
	WriteUnicodeGroup();
}

void COTFDialog::OnBnClickedLoadFont()
{
	CFileDialog dlg(TRUE, TEXT("ttf"), 0, OFN_HIDEREADONLY, TEXT("TrueType Bitmap Fonts (*.ttf)|*.ttf||"), this);
	MO_LOAD_RESOURCES_PATH(dlg)

	if (dlg.DoModal() != IDOK)
		return;
	MO_SAVE_RESOURCES_PATH(dlg)
	LoadFullFont(dlg.GetPathName(), m_Font.GetFontBlockLen());
}

void COTFDialog::LoadFullFont(CString path, DWORD size, DWORD fidx)
{
	DWORD fontindex;
	if (fidx==-1)
		fontindex=m_FontIndex;
	else
		fontindex=fidx;

	CFile file;
	if (!file.Open(path, CFile::modeRead))
	{
		MessageBox(TEXT("Unable to load file!"));
		return;
	}

	LPBYTE lpBuffer = m_pFirmware->GetOTFFont(fontindex);
	if (file.Read(lpBuffer, size) < size)
	{
		MessageBox(TEXT("Error reading font file!"));
		return;
	}

	UpdateFont();
}

void COTFDialog::LoadFullFontsDir(CString folderPath)
{
	CString filename;
	filename = folderPath;
	filename += "\\*-*-*.ttf";

	WIN32_FIND_DATA findData;
	COTFFont font;
	CString fname;
	CString fontname,fontstyle,fontsize;
	LPBYTE lpEnd=m_pFirmware->GetFirmwareBuffer()+m_pFirmware->GetFirmwareSize();
	HANDLE hFind = FindFirstFile(filename, &findData);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			filename = folderPath;
			filename += "\\";
			filename += findData.cFileName;
			fname.Format(findData.cFileName);
			int fpos=fname.Find('-');
			fontname=fname.Left(fpos);
			int ftemp=fpos+1;
			fpos=fname.Find('-', ftemp);
			fontstyle=fname.Mid(ftemp, fpos);
			ftemp=fpos+1;
			fpos=fname.Find('-', ftemp);
			fontsize=fname.Mid(ftemp, fpos);
			for (DWORD z=0;z<m_pFirmware->GetNumFonts();z++)
			{
				font.Read(m_pFirmware->GetFont(z), TRUE);
				if (!fontname.Compare(font.GetFontName()) && !fontstyle.Compare(font.GetFontStyle()) && _ttoi(fontsize)==font.GetFontSize())
				{
					LoadFullFont(filename, font.GetFontBlockLen(), z);
					break;
				}
			}
		} while (FindNextFile(hFind, &findData) != 0);

		FindClose(hFind);

		UpdateFont();
	}
}

void COTFDialog::OnBnClickedSaveFont()
{
	CString filename;
	filename.Format(TEXT("%s.ttf"), m_Font.GetFontName());

	CFileDialog dlg(FALSE, TEXT("ttf"), filename, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, TEXT("TrueType Bitmap Font (*.ttf)|*.ttf||"), this);
	MO_LOAD_RESOURCES_PATH(dlg)

	if (dlg.DoModal() != IDOK)
		return;
	MO_SAVE_RESOURCES_PATH(dlg)
	CFile file;
	if (!file.Open(dlg.GetPathName(), CFile::modeCreate | CFile::modeWrite))
	{
		MessageBox(TEXT("Unable to save file!"));
		return;
	}

	LPBYTE lpBuffer = m_pFirmware->GetOTFFont(m_FontIndex);
	file.Write(lpBuffer, m_Font.GetFontBlockLen());
	file.Close();
}

void COTFDialog::OnBnClickedMulloadGlyph()
{
	UpdateData(TRUE);

	LPITEMIDLIST il;
	TCHAR folderPath[MAX_PATH];

	BROWSEINFO br;
    br.hwndOwner = AfxGetMainWnd()->GetSafeHwnd();
    br.pidlRoot = NULL;
    br.pszDisplayName = folderPath;
    br.lpszTitle = TEXT("Select folder for files");
    br.ulFlags = BIF_RETURNONLYFSDIRS;
    br.lpfn = NULL;
    br.lParam = 0;
    br.iImage = 0;

	il = SHBrowseForFolder(&br);
	if (il == NULL)
		return;

	SHGetPathFromIDList(il, folderPath);

	WORD index=0;
	CString filename = folderPath;
	filename += "\\????.bmp";

	BYTE width,height,advance;
	CHAR bearingy;
	WORD gidx;
	WIN32_FIND_DATA findData;
	HANDLE hFind = FindFirstFile(filename, &findData);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			filename = folderPath;
			filename += "\\";
			filename += findData.cFileName;
			_stscanf(findData.cFileName, TEXT("%04X"), &index);
			//index = _wtoi(findData.cFileName + 4);

			if (index>=m_GroupStart && index<=m_GroupEnd)
			{
				CImage img;
				if (FAILED(img.Load(filename)))
				{
					CString msg;
					msg.Format(TEXT("Unable to load image %d!"), index);
					MessageBox(msg);
					return;
				}

				width = img.GetWidth();
				height = img.GetHeight();
				gidx=m_Font.GetGlyphIndex(m_GroupOffset-1+index-m_GroupStart);

				//calculate bearingy
				int x, y;

				int hstart = -1, hend = -1;
				BOOL bLineEmpty;

				for (y = 0; y < height; y++)
				{
					bLineEmpty = TRUE;
					for (x = 0; x < width; x++)
					{
						if (img.GetPixel(x, y) != RGB(255, 255, 255))
						{
							bLineEmpty = FALSE;
							break;
						}
					}
					if (!bLineEmpty)
					{
						hstart = y;
						break;
					}
				}

				for (y = height - 1; y >= 0 && hstart != -1; y--)
				{
					bLineEmpty = TRUE;
					for (x = 0; x < width ; x++)
					{
						if (img.GetPixel(x, y) != RGB(255, 255, 255))
						{
							bLineEmpty = FALSE;
							break;
						}
					}
					if (!bLineEmpty)
					{
						hend = y;
						break;
					}
				}
				if (hstart!=-1)
					hend++;
				//
				height=hend-hstart;
				bearingy=m_BaseLine-hstart;
				//bearingy=height;
				//if (hend>m_BaseLine)
				//	bearingy-=hend-m_BaseLine;
				advance=m_ALeft + width + m_ARight;
				m_Font.SetCharMetrics(gidx, width, height, m_ALeft, bearingy, advance);

				//LONG j, k;
				COLORREF color;

				for (x = 0; x < img.GetWidth(); x++)
				{
					for (y = 0; y < height; y++)
					{
						color = img.GetPixel(x, y + hstart);
						m_Font.SetFontPixel(gidx, x, y, color);
					}
				}
			}
		} while (FindNextFile(hFind, &findData) != 0);

		FindClose(hFind);

		UpdateData(FALSE);

		ReadCharMetrics();
		UpdateZoomView();
	}
	index=0;
}

void COTFDialog::OnBnClickedMulsaveGlyph()
{
	UpdateData(TRUE);

	LPITEMIDLIST il;
	TCHAR folderPath[MAX_PATH];

	BROWSEINFO br;
    br.hwndOwner = AfxGetMainWnd()->GetSafeHwnd();
    br.pidlRoot = NULL;
    br.pszDisplayName = folderPath;
    br.lpszTitle = TEXT("Select folder for files to save");
    br.ulFlags = BIF_RETURNONLYFSDIRS;
    br.lpfn = NULL;
    br.lParam = 0;
    br.iImage = 0;

	il = SHBrowseForFolder(&br);
	if (il == NULL)
		return;

	SHGetPathFromIDList(il, folderPath);

	int i = m_CharCombo.GetCurSel();

	CString filename;
	WORD group;
	for (int i=0;i<m_CharCombo.GetCount();i++)
	{
		WORD unicodeChar = m_Font.GetUnicodeChar(i, &group);
		WORD offset = m_Font.GetUnicodeCharOffset(unicodeChar);
		
		CSize size=m_Font.GetFontBitmapSize(offset);
		if (size.cx<=0 || size.cy<=0)
			continue;
		filename.Format(_T("%s\\%04X.bmp"), folderPath, unicodeChar);
		CImage img;
		if (!img.Create(size.cx, size.cy, 24))
		{
			MessageBox(_T("Error saving glyphs!"));
			return;
		}

		for (int x=0;x<size.cx;x++)
			for (int y=0;y<size.cy;y++)
			{
				img.SetPixel(x, y, m_Font.GetFontPixel(offset, CPoint(x, y)));
			}
		if (FAILED(img.Save(filename)))
		{
			MessageBox(_T("Error saving glyphs!"));
		}
	}
}
