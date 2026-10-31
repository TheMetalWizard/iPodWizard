#include "stdafx.h"
#include "iPodWizard.h"
#include "EditorDialog.h"

IMPLEMENT_DYNAMIC(CEditorDialog, CDarkDialog)

CEditorDialog::CEditorDialog(CWnd* pParent /*=NULL*/)
	: CDarkDialog(CEditorDialog::IDD, pParent)
	, bFonts(FALSE)
	, bOTFCreate(FALSE)
	, m_pThemePreview(NULL)
{
}

CEditorDialog::~CEditorDialog()
{
}

void CEditorDialog::DoDataExchange(CDataExchange* pDX)
{
	CDarkDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PROP_TAB, m_PropsTab);
}

BEGIN_MESSAGE_MAP(CEditorDialog, CDarkDialog)
	ON_NOTIFY(TCN_SELCHANGE, IDC_PROP_TAB, OnTcnSelchangePropTab)
	ON_MESSAGE(WM_APP, OnSwitchFontSystem)
	ON_WM_SIZE()
END_MESSAGE_MAP()

BOOL CEditorDialog::OnInitDialog()
{
	CDarkDialog::OnInitDialog();
	// initialize pages

	CRect rect;
	m_PropsTab.InsertItem(TAB_PICTURES, TEXT("Graphics"));
	m_PropsTab.InsertItem(TAB_FONTS, TEXT("Fonts"));
	m_PropsTab.InsertItem(TAB_STRINGS, TEXT("Strings"));
	m_PropsTab.InsertItem(TAB_LAYOUT, TEXT("Layout"));
	m_PropsTab.InsertItem(TAB_MENUS, TEXT("Menus"));
	m_PropsTab.InsertItem(TAB_TYPES, TEXT("Font Types"));
	m_PropsTab.InsertItem(TAB_COMMANDS, TEXT("Commands"));
	m_PropsTab.InsertItem(TAB_TWEAKS, TEXT("Tweaks"));
	m_PropsTab.UpdateGaps();

	m_PicsDialog.Create(m_PicsDialog.IDD, &m_PropsTab);
	m_FontsDialog.Create(m_FontsDialog.IDD, &m_PropsTab);
	m_StringDialog.Create(m_StringDialog.IDD, &m_PropsTab);
	m_OTFDialog.Create(m_OTFDialog.IDD, &m_PropsTab);
	m_LayoutDialog.Create(m_LayoutDialog.IDD, &m_PropsTab);
	m_TweaksDialog.Create(m_TweaksDialog.IDD, &m_PropsTab);

	m_PropsTab.GetClientRect(&rect);
	m_PropsTab.AdjustRect(FALSE, &rect);

	m_PicsDialog.MoveWindow(rect);
	m_FontsDialog.MoveWindow(rect);
	m_StringDialog.MoveWindow(rect);
	m_OTFDialog.MoveWindow(rect);
	m_LayoutDialog.MoveWindow(rect);
	m_TweaksDialog.MoveWindow(rect);

	UpdatePages(FALSE);

	return TRUE;
}

void CEditorDialog::OnOK()
{
	// TODO: Add your specialized code here and/or call the base class

	// CDarkDialog::OnOK();
}

void CEditorDialog::SetPreviewButton(CWnd *pWnd)
{
	m_pThemePreview = pWnd;
}

//Switch page when user clicked OTF Fonts/Bitmap Fonts
LRESULT CEditorDialog::OnSwitchFontSystem(WPARAM wParam, LPARAM lParam)
{
	if (wParam == NULL)
		return 0;
	if (m_pFirmware==NULL)
		return 0;
	int mg=(int)wParam;
	CString temp;
	CIpodFont fnt;
	COTFFont ofnt;
	bool found;
	switch(mg)
	{
	case 10:
		bFonts=(BOOL)lParam;
		UpdatePages(bFonts);
		break;
	case 11:
		m_StringDialog.m_Langidx.SetCurSel(m_StringDialog.m_Langidx.GetCount()-2);
		m_StringDialog.OnCbnSelchangeLangidxCombo();
		LRESULT Result;
		NMLISTVIEW dNMHDR;
		dNMHDR.uOldState = 0;
		temp.Format(_T("%d"), (int)lParam);
		int i;
		for (i=0;i<m_StringDialog.m_LangStringList.GetItemCount();i++)
		{
			if (m_StringDialog.m_LangStringList.GetItemText(i, 0).Compare(temp)==0)
				break;
		}
		if (i!=m_StringDialog.m_LangStringList.GetItemCount())
		{
			m_StringDialog.m_LangStringList.SetSelectionMark(i);
			LVITEM lim;
			lim.mask=LVIF_STATE;
			lim.iItem=i;
			lim.stateMask=LVIS_SELECTED;
			lim.state=LVIS_SELECTED;
			lim.iSubItem=0;
			m_StringDialog.m_LangStringList.SetItem(&lim);
			m_StringDialog.m_LangStringList.EnsureVisible(i, FALSE);

			dNMHDR.iItem = i;
			m_StringDialog.OnLvnItemchangedLangstringList((NMHDR *)&dNMHDR, &Result);
			m_PropsTab.SetCurSel(TAB_STRINGS);
			UpdatePages(bFonts);
		}
		else
			MessageBox(_T("String not found in firmware, it may be generated on the fly by the iPod's program."));
		break;
	case 12:
		//if (m_PicsDialog.m_PicIndexCombo.GetCurSel()!=(int)lParam)
		if ((DWORD)lParam==0xFFFFFFFF)
			MessageBox(_T("Picture not found in firmware, it may be generated on the fly by the iPod's program."));
		else
		{
			m_PicsDialog.m_PicIndexCombo.SetCurSel((int)lParam);
			m_PicsDialog.OnCbnSelchangePicidxCombo();
			m_PropsTab.SetCurSel(TAB_PICTURES);
			UpdatePages(bFonts);
		}
		break;
	case 13:
		resource_type_type_elem *type;
		type=(resource_type_type_elem *)lParam;
		DWORD j;
		int k;
		LPBYTE lpEnd;
		lpEnd=m_pFirmware->GetFirmwareBuffer()+m_pFirmware->GetFirmwareSize();
		found=false;
		unsigned char fstyle;
		fstyle=type->style;
		for (j=0;j<m_pFirmware->GetNumLangStrings(m_pFirmware->GetNumLangs()-1);j++)
		{
			if (m_pFirmware->GetLangStringID(m_pFirmware->GetNumLangs()-1, j)==type->fontID)
			{
				unsigned char style=0;
				wchar_t *sOut;
				Utf8Decode((char *)m_pFirmware->GetLangString(m_pFirmware->GetNumLangs()-1, j), &sOut);
					// The font menu (the Fonts page) is where a font is shown. The OpenType page only gets the click when the firmware
					// has no font of this name, size and style for the Fonts page.
					bool hasBitmap;
					hasBitmap=false;
					for (k=0;(DWORD)k<m_pFirmware->GetNumFonts();k++)
						if (fnt.Read(m_pFirmware->GetFont(k), lpEnd)==TRUE && !wcscmp(fnt.GetFontName(), sOut) && fnt.GetFontSize()==type->size && fnt.GetFontStyleID()==fstyle)
						{
							hasBitmap=true;
							break;
						}
				for (k=0;!hasBitmap && (DWORD)k<m_pFirmware->GetNumOTFFonts();k++)
				{
					if (ofnt.Read(m_pFirmware->GetOTFFont(k), TRUE))
					{
						if (!wcscmp(ofnt.GetFontStyle(), _T("Bold")))
							style=0;
						else if (!wcscmp(ofnt.GetFontStyle(), _T("Regular")))
							style=1;
						if (!wcscmp(ofnt.GetFontName(), sOut) && ofnt.GetFontSize()==type->size && fstyle==style)
						{
							m_OTFDialog.m_FontIndexCombo.SetCurSel(m_pFirmware->GetNumFonts() + k);
							m_OTFDialog.OnCbnSelchangeFontidxCombo();
							bFonts=TRUE;
							m_PropsTab.SetCurSel(TAB_FONTS);
							UpdatePages(bFonts);
							found=true;
							break;
						}
					}
				}
				for (k=0;(DWORD)k<m_pFirmware->GetNumFonts();k++)
				{
					if (fnt.Read(m_pFirmware->GetFont(k), lpEnd)==TRUE)
					{
						if (!wcscmp(fnt.GetFontStyle(), _T("Bold")))
							style=1;
						else if (!wcscmp(fnt.GetFontStyle(), _T("Regular")))
							style=0;
						if (!wcscmp(fnt.GetFontName(), sOut) && fnt.GetFontSize()==type->size && fnt.GetFontStyleID()==fstyle)
						{
								bFonts=FALSE;
								m_FontsDialog.m_FontIndexCombo.SetCurSel(k);
							m_FontsDialog.OnCbnSelchangeFontidxCombo();
							m_PropsTab.SetCurSel(TAB_FONTS);
							UpdatePages(bFonts);
							found=true;
							break;
						}
					}
				}
				delete sOut;
				break;
			}
		}
		if (found==false)
			MessageBox(_T("Font not found in firmware, it may be generated on the fly by the iPod's program."));
		break;
	case 14:
		// an entry of the list of fonts that the page that is shown does not edit (the list is the bitmap fonts, then the OpenType ones)
		{
			DWORD sel = (DWORD)lParam;
			if (sel < m_pFirmware->GetNumFonts())
			{
				bFonts=FALSE;
				m_FontsDialog.m_FontIndexCombo.SetCurSel((int)sel);
				m_FontsDialog.OnCbnSelchangeFontidxCombo();
			}
			else
			{
				bFonts=TRUE;
				m_OTFDialog.m_FontIndexCombo.SetCurSel((int)sel);
				m_OTFDialog.OnCbnSelchangeFontidxCombo();
			}
			m_PropsTab.SetCurSel(TAB_FONTS);
			UpdatePages(bFonts);
		}
		break;
	case 21:
		//show layouts dialog
		m_PropsTab.SetCurSel(TAB_LAYOUT);
		UpdatePages(bFonts);
		break;
	case 22:
		//the layout editor changed its mode (0 layouts, 1 menus, 2 font types, 3 / 4 commands): the tab follows, but only
		//when the layout editor is what is shown (loading a firmware must not take anyone away from the page they are on)
		{
			int cur = m_PropsTab.GetCurSel();
			int want = (int)lParam == 4 ? TAB_COMMANDS : TAB_LAYOUT + (int)lParam;      // both ways of showing the commands are one tab
			if ((int)lParam >= 0 && (int)lParam <= 4 && cur >= TAB_LAYOUT && cur <= TAB_COMMANDS && cur != want)
			{
				m_PropsTab.SetCurSel(want);
				UpdatePages(bFonts);
			}
		}
		break;
	default:
		break;
	}
	return 0;
}

// The undo keys of the font editor also work when the focus is on the tabs (a click on a tab moves it there). They are only taken
// while the Fonts page is shown with its editor open: with the editor closed, or another tab shown, they are not.
BOOL CEditorDialog::PreTranslateMessage(MSG *pMsg)
{
	if (m_FontsDialog.GetSafeHwnd() && m_FontsDialog.HandleUndoKey(pMsg))
		return TRUE;
	return CDarkDialog::PreTranslateMessage(pMsg);
}

void CEditorDialog::UpdatePages(BOOL bFonts)
{
	int i = m_PropsTab.GetCurSel();
	if (m_PropsTab.IsGap(i))
		return;

	// the tabs from "Layout" to "Commands" are the pages of one dialog, the layout editor
	BOOL bLayout = i >= TAB_LAYOUT && i <= TAB_COMMANDS;
	if (bLayout)
		m_LayoutDialog.SetPage(i - TAB_LAYOUT);

	// the font list has the focus (a font was picked, the arrow keys step through the fonts): it keeps it when the page of the other
	// kind of font takes over, instead of the focus being lost with the page that goes
	CWnd *pFocus = GetFocus();
	bool bFontListFocus = pFocus != NULL && (pFocus->m_hWnd == m_FontsDialog.m_FontIndexCombo.m_hWnd || pFocus->m_hWnd == m_OTFDialog.m_FontIndexCombo.m_hWnd);

	m_PicsDialog.ShowWindow(i == TAB_PICTURES ? SW_SHOW : SW_HIDE);
	m_FontsDialog.ShowWindow(i == TAB_FONTS && !bFonts ? SW_SHOW : SW_HIDE);
	m_OTFDialog.ShowWindow(i == TAB_FONTS && bFonts ? SW_SHOW : SW_HIDE);
	if (bFontListFocus && i == TAB_FONTS)
		(bFonts ? m_OTFDialog.m_FontIndexCombo : m_FontsDialog.m_FontIndexCombo).SetFocus();
	m_StringDialog.ShowWindow(i == TAB_STRINGS ? SW_SHOW : SW_HIDE);
	m_LayoutDialog.ShowWindow(bLayout ? SW_SHOW : SW_HIDE);
	m_TweaksDialog.ShowWindow(i == TAB_TWEAKS ? SW_SHOW : SW_HIDE);
	if (i == TAB_TWEAKS)
		m_TweaksDialog.Refresh();
}

void CEditorDialog::SetFirmware(CFirmware *pFirmware)
{
	//Initialize firmware on all sections
	m_PicsDialog.SetFirmware(pFirmware, &m_LayoutDialog, m_pThemePreview);
	m_FontsDialog.SetFirmware(pFirmware, &m_LayoutDialog);
	m_OTFDialog.SetFirmware(pFirmware, &m_LayoutDialog);
	m_StringDialog.SetFirmware(pFirmware);
	m_LayoutDialog.SetFirmware(pFirmware);
	m_TweaksDialog.SetFirmware(pFirmware);
	m_TweaksDialog.Refresh();
	m_pFirmware=pFirmware;
	if (pFirmware->GetNumOTFFonts()==0) //if no OTF fonts then show only bitmap (even if before user was on OTF)
	{
		bFonts=FALSE;
		UpdatePages(bFonts);
	}
}

void CEditorDialog::OnSize(UINT nType, int cx, int cy)
{
	CDarkDialog::OnSize(nType, cx, cy);
	if (nType == SIZE_MINIMIZED || !IsWindow(m_PropsTab.GetSafeHwnd()))
		return;

	m_PropsTab.SetWindowPos(NULL, 0, 0, cx, cy, SWP_NOZORDER | SWP_NOACTIVATE);

	CRect clientRect;
	m_PropsTab.GetClientRect(&clientRect);
	m_PropsTab.AdjustRect(FALSE, &clientRect);
	m_PicsDialog.MoveWindow(clientRect);
	m_FontsDialog.MoveWindow(clientRect);
	m_StringDialog.MoveWindow(clientRect);
	m_OTFDialog.MoveWindow(clientRect);
	m_LayoutDialog.MoveWindow(clientRect);
	m_TweaksDialog.MoveWindow(clientRect);
}

void CEditorDialog::OnTcnSelchangePropTab(NMHDR *pNMHDR, LRESULT *pResult)
{
	// changes of the font that are not written: ask before the page is left
	if (m_PropsTab.GetCurSel() != TAB_FONTS && m_FontsDialog.IsWindowVisible() && !m_FontsDialog.ConfirmPending(this))
	{
		m_PropsTab.SetCurSel(TAB_FONTS);
		*pResult = 0;
		return;
	}
	//show page according to user selection
	UpdatePages(bFonts);

	*pResult = 0;
}