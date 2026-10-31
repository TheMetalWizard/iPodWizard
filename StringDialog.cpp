// StringDialog.cpp : implementation file
//

#include "stdafx.h"
#include "iPodWizard.h"
#include "StringDialog.h"
#include ".\stringdialog.h"
#include <vector>

// CStringDialog dialog

// the Hex GoTo box works like the one of the Layout tab (LayoutDialog.cpp): the digits show in pairs, "01 23 AB CD"
CString HexDigitsOf(const CString &s);
CString SpacedHex(const CString &digits);
void DDX_MyTextAddrHex2(CDataExchange *pDX, int nIDC, DWORD &value);

// The name of a language block, empty when it is not known. The blocks are numbered per model family:
// 25 blocks on the 5G/5.5G, 19 on the other models (the same lists as in the String information window).
static CString LangBlockName(DWORD numLangs, DWORD lang)
{
	static const TCHAR *const names25[25] = {
		_T("Slovak"), _T("Danish"), _T("German"), _T("German"), _T("Russian"), _T("Spanish"), _T("Finnish"), _T("French"),
		_T("French"), _T("French"), _T("Hungarian"), _T("Italian"), _T("Japanese"), _T("Korean"), _T("Dutch"), _T("Dutch"),
		_T("Norwegian"), _T("Polish"), _T("Spanish"), _T("Russian"), _T("Swedish"), _T("Turkish"), _T("S.Chinese"), _T("T.Chinese"),
		_T("English") };
	static const TCHAR *const names19[19] = {
		_T("Danish"), _T("German"), _T("German"), _T("Spanish"), _T("Finnish"), _T("French"), _T("French"), _T("French"),
		_T("Italian"), _T("Japanese"), _T("Korean"), _T("Dutch"), _T("Dutch"), _T("Norwegian"), _T("Spanish"), _T("Swedish"),
		_T("S.Chinese"), _T("T.Chinese"), _T("English") };
	if (numLangs == 25 && lang < 25)
		return names25[lang];
	if (numLangs == 19 && lang < 19)
		return names19[lang];
	return CString();
}

void DDX_MyTextAddrHex(CDataExchange *pDX, int nIDC, DWORD &value)
{
	CString s;

	if (pDX->m_bSaveAndValidate)
	{
		DWORD temp = 0;
		pDX->m_pDlgWnd->GetDlgItem(nIDC)->GetWindowText(s);
		s.MakeLower();
		for (int i = 0; i < s.GetLength(); i++)
		{
			TCHAR c = ((LPCTSTR)s)[i];
			if (c >= '0' && c <= '9')
			{
				temp = (temp << 4) + (c - '0');
			}
			else if (c >= 'a' && c <= 'f')
			{
				temp = (temp << 4) + (c - 'a' + 10);
			}
			else
			{
				s.Format(TEXT("%08X"), value);
				pDX->m_pDlgWnd->GetDlgItem(nIDC)->SetWindowText(s);
				return;
			}
		}
		value=temp;
	}
	else
	{
		s.Format(TEXT("%08X"), value);
		pDX->m_pDlgWnd->GetDlgItem(nIDC)->SetWindowText(s);
	}
}

IMPLEMENT_DYNAMIC(CStringDialog, CDarkDialog)
CStringDialog::CStringDialog(CWnd* pParent /*=NULL*/)
	: CDarkDialog(CStringDialog::IDD, pParent)
	, m_pFirmware(NULL)
	, m_FindString(_T(""))
	, m_ChangeString(_T(""))
	, m_HexAddr(0)
	, m_bSFilter(FALSE)
	, m_bShowAll(FALSE)
	, m_hexBottom(0)
	, m_listTop(0)
	, m_listHeight(0)
	, m_listHeightMin(0)
	, m_splitterGap(0)
	, m_dragging(false)
	, m_dragStartY(0)
	, m_dragStartListHeight(0)
{
}

CStringDialog::~CStringDialog()
{
}

void CStringDialog::DoDataExchange(CDataExchange* pDX)
{
	CDarkDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_STRING_LIST, m_StringList);
	DDX_Control(pDX, IDC_LANGSTRING_LIST, m_LangStringList);
	DDX_Control(pDX, IDC_HEX, m_HexWnd);
	DDX_Control(pDX, IDC_LANGIDX_COMBO, m_Langidx);
	DDX_Text(pDX, IDC_FIND_EDIT, m_FindString);
	DDX_Text(pDX, IDC_CHANGE_EDIT, m_ChangeString);
	DDX_MyTextAddrHex2(pDX, IDC_ADDR_EDIT, m_HexAddr);
}


BEGIN_MESSAGE_MAP(CStringDialog, CDarkDialog)
	ON_BN_CLICKED(IDC_FIND_BUTTON, OnBnClickedFindButton)
	ON_EN_CHANGE(IDC_FIND_EDIT, OnEnChangeFindEdit)
	ON_LBN_SELCHANGE(IDC_STRING_LIST, OnLbnSelchangeStringList)
	ON_BN_CLICKED(IDC_FINDNEXT_BUTTON, OnBnClickedFindnextButton)
	ON_BN_CLICKED(IDC_CHANGETEXT_BUTTON, OnBnClickedChangeTextButton)
	ON_EN_CHANGE(IDC_CHANGE_EDIT, OnEnChangeChangeEdit)
	ON_BN_CLICKED(IDC_CHECKTEXT_BUTTON, OnBnClickedCheckTextButton)
	ON_BN_CLICKED(IDC_CHKHEX, OnBnClickedHexString)
	ON_BN_CLICKED(IDC_SAVECHANGES_BUTTON, OnBnClickedSaveAllChangesButton)
	ON_BN_CLICKED(IDC_LOADCHANGES_BUTTON, OnBnClickedLoadChangesButton)
	ON_BN_CLICKED(IDC_JUMP_BUTTON, OnBnClickedJumpToButton)
	ON_WM_TIMER()
	ON_WM_DESTROY()
	ON_EN_CHANGE(IDC_ADDR_EDIT, OnEnChangeAddrEdit)
	ON_BN_CLICKED(IDC_SFILTER_BUTTON, OnBnClickedStringFilterButton)
	ON_CBN_SELCHANGE(IDC_LANGIDX_COMBO, OnCbnSelchangeLangidxCombo)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LANGSTRING_LIST, OnLvnItemchangedLangstringList)
	//ON_NOTIFY(LVN_ITEMACTIVATE, IDC_LANGSTRING_LIST, OnLvnItemchangedLangstringList)
	ON_BN_CLICKED(IDC_TRANSLATE_BUTTON, OnBnClickedTranslate)
	ON_WM_SIZE()
	ON_WM_SETCURSOR()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_CBN_DROPDOWN(IDC_LANGIDX_COMBO, OnCbnDropdownLangidxCombo)
	ON_CBN_CLOSEUP(IDC_LANGIDX_COMBO, OnCbnCloseupLangidxCombo)
END_MESSAGE_MAP()

// CStringDialog message handlers

BOOL CStringDialog::OnInitDialog()
{
	CDarkDialog::OnInitDialog();

	// TODO:  Add extra initialization here
	m_ChangeEdit.SubclassDlgItem(IDC_CHANGE_EDIT, this);
	m_AddrEdit.SubclassDlgItem(IDC_ADDR_EDIT, this);
	m_AddrCueShown = 0xFFFFFFFF;
	SetTimer(IDT_ADDRCUE, 150, NULL);                // the hex view moves its cursor by itself: the hint of Hex GoTo follows it
	reinterpret_cast<CEdit*>(GetDlgItem(IDC_ADDR_EDIT))->LimitText(11);       // eight digits and the three spaces between the pairs

	m_LangStringList.SetExtendedStyle(m_LangStringList.GetExtendedStyle()|LVS_EX_FULLROWSELECT);
	m_LangStringList.InsertColumn(0, TEXT("ID"), LVCFMT_LEFT, 30);
	m_LangStringList.InsertColumn(1, TEXT("String Data"), LVCFMT_LEFT, 320);

	CRect clientRect, listRect, hexRect;
	GetClientRect(&clientRect);
	m_LangStringList.GetWindowRect(&listRect);
	ScreenToClient(&listRect);
	m_HexWnd.GetWindowRect(&hexRect);
	ScreenToClient(&hexRect);
	m_listTop        = listRect.top;
	m_listHeight     = listRect.Height();
	m_listHeightMin  = listRect.Height();
	m_splitterGap    = hexRect.top - listRect.bottom;
	m_hexBottom   = clientRect.Height() - hexRect.bottom;
	m_Stretch.Init(this);

	int selH = (int)m_Langidx.SendMessage(CB_GETITEMHEIGHT, (WPARAM)-1, 0);
	m_idxComboClosedH = selH + 8;
	CRect rc; m_Langidx.GetWindowRect(&rc); ScreenToClient(&rc);
	m_Langidx.MoveWindow(rc.left, rc.top, rc.Width(), m_idxComboClosedH);

	return TRUE;
}

void CStringDialog::OnCbnDropdownLangidxCombo()
{
	CWnd* pMain = AfxGetMainWnd();
	if (!pMain) return;
	CRect rcMain, rcCombo;
	pMain->GetClientRect(&rcMain);
	m_Langidx.GetWindowRect(&rcCombo);
	pMain->ScreenToClient(&rcCombo);
	int dropH = rcMain.bottom - rcCombo.top - 8;
	if (dropH < 50) dropH = 50;
	CRect rc; m_Langidx.GetWindowRect(&rc); ScreenToClient(&rc);
	m_Langidx.MoveWindow(rc.left, rc.top, rc.Width(), dropH);
}

void CStringDialog::OnCbnCloseupLangidxCombo()
{
	CRect rc; m_Langidx.GetWindowRect(&rc); ScreenToClient(&rc);
	m_Langidx.MoveWindow(rc.left, rc.top, rc.Width(), m_idxComboClosedH);
}

void CStringDialog::SetFirmware(CFirmware *pFirmware)
{
	m_pFirmware = pFirmware;

	// update strings
	m_Langidx.ResetContent();
	CString s;
	DWORD i;
	for (i=0;i<m_pFirmware->GetNumLangs();i++)
	{
		CString name = LangBlockName(m_pFirmware->GetNumLangs(), i);
		if (name.IsEmpty())
			s.Format(TEXT("%d"),i);
		else
			s.Format(TEXT("%d  %s"), i, (LPCTSTR)name);
		m_Langidx.InsertString(i, s);
	}
	m_Langidx.InsertString(i, TEXT("Show All"));
	m_Langidx.SetCurSel(0);
	OnCbnSelchangeLangidxCombo();
	
	m_HexWnd.SetFirmware(m_pFirmware);
	GetDlgItem(IDC_STRING_LIST)->EnableWindow(TRUE);
	GetDlgItem(IDC_FIND_EDIT)->EnableWindow(TRUE);
	GetDlgItem(IDC_CHANGE_EDIT)->EnableWindow(TRUE);
	GetDlgItem(IDC_CHKWILD)->EnableWindow(TRUE);
	GetDlgItem(IDC_LOADCHANGES_BUTTON)->EnableWindow(TRUE);
	GetDlgItem(IDC_JUMP_BUTTON)->EnableWindow(TRUE);
	GetDlgItem(IDC_ADDR_EDIT)->EnableWindow(TRUE);
	GetDlgItem(IDC_CHKHEX)->EnableWindow(TRUE);
	GetDlgItem(IDC_SFILTER_BUTTON)->EnableWindow(TRUE);
	GetDlgItem(IDC_TRANSLATE_BUTTON)->EnableWindow(TRUE);

	//Zero string changes list
	m_StringChanges.RemoveAll();

	UpdateData(FALSE);
}

void CStringDialog::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == IDT_ADDRCUE)
	{
		if (m_pFirmware != NULL && m_HexWnd.m_hWnd != NULL && m_HexWnd.m_CursorPos != m_AddrCueShown)
		{
			m_AddrCueShown = m_HexWnd.m_CursorPos;
			m_AddrEdit.SetCue(OffsetCue(m_AddrCueShown));
		}
		return;
	}
	CDarkDialog::OnTimer(nIDEvent);
}

void CStringDialog::OnDestroy()
{
	KillTimer(IDT_ADDRCUE);
	CDarkDialog::OnDestroy();
}

void CStringDialog::OnBnClickedJumpToButton()
{
	if (GetDlgItem(IDC_ADDR_EDIT)->GetWindowTextLength() == 0)
		return;                                  // nothing typed: the box only shows the current offset
	if (m_HexAddr<=m_pFirmware->GetFirmwareSize())
		m_HexWnd.SetOffset(m_HexAddr);
}

void CStringDialog::OnBnClickedStringFilterButton()
{
	if (!m_bShowAll)
	{
		MessageBox(TEXT("This option is only valid for Show All strings and not a single language block."));
		return;
	}

	CFileDialog dlg(TRUE, TEXT("txt"), 0, OFN_HIDEREADONLY, TEXT("String List Files (*.txt)|*.txt||"), this);
	MO_LOAD_RESOURCES_PATH(dlg)

	if (dlg.DoModal() != IDOK)
		return;
	MO_SAVE_RESOURCES_PATH(dlg)
	CFile file;
	if (!file.Open(dlg.GetPathName(), CFile::modeRead))
	{
		MessageBox(TEXT("Unable to load file!"));
		return;
	}
	BYTE c=0;
	CString s;
	CStringArray sfilter;
	sfilter.RemoveAll();
	while (TRUE)
	{
		s.Format(TEXT(""));
		while (TRUE)
		{
			c=-1;
			if (file.Read(&c, 1) < 1)
				goto endoffile;
			if (c==10)
			{
				if (s.Compare(TEXT(""))!=0)
					sfilter.Add(s);
				break;
			}
			if (c!=13)
				s.AppendChar(c);
		}
		s.Format(TEXT(""));
	}
endoffile:
	file.Close();
	if (s.Compare(TEXT(""))!=0)
		sfilter.Add(s);
	// update strings
	sfilter_w.RemoveAll();
	m_StringList.ResetContent();
	DWORD y=0;
	for (DWORD i = 0; i < GetNumStrings(m_Lang); i++)
	{
		LPCTSTR st = GetString(m_Lang, i);
		for (c=0;c<sfilter.GetCount();c++)
		{
			if (sfilter.GetAt(c).Compare(st)==0)
			{
				sfilter_w.Add(i);
				m_StringList.AddString(st);
				y++;
			}
		}
	}
	m_bSFilter=TRUE;
}

static const int STR_SPLITTER_HIT = 5;
static const int STR_PANE_MIN    = 20;

int CStringDialog::EffectiveListHeight()
{
	CRect clientRect;
	GetClientRect(&clientRect);
	int maxListHeight = clientRect.Height() - m_listTop - m_splitterGap - STR_PANE_MIN - m_hexBottom;
	int listH = m_listHeight;
	if (listH < m_listHeightMin) listH = m_listHeightMin;
	if (listH > maxListHeight)   listH = maxListHeight;
	if (listH < m_listHeightMin) listH = m_listHeightMin;
	return listH;
}

void CStringDialog::LayoutSplitter()
{
	if (m_listHeight == 0) return; // not yet initialised
	if (!IsWindow(m_LangStringList.GetSafeHwnd())) return;
	if (!IsWindow(m_HexWnd.GetSafeHwnd())) return;

	CRect clientRect, listRect, hexRect;
	GetClientRect(&clientRect);

	// the lists, the hex view and the Find and Change String boxes follow the window width, the buttons at the right move
	// along with its edge
	static const int stretchIds[] = { IDC_LANGSTRING_LIST, IDC_STRING_LIST, IDC_HEX, IDC_FIND_EDIT, IDC_CHANGE_EDIT };
	m_Stretch.Apply(this, StretchExtraWidth(), stretchIds, 5, NULL, 0);

	m_LangStringList.GetWindowRect(&listRect);
	ScreenToClient(&listRect);
	m_HexWnd.GetWindowRect(&hexRect);
	ScreenToClient(&hexRect);

	int listH = EffectiveListHeight();

	m_LangStringList.SetWindowPos(NULL, listRect.left, listRect.top,
	                               listRect.Width(), listH,
	                               SWP_NOZORDER | SWP_NOACTIVATE);

	int hexTop    = m_listTop + listH + m_splitterGap;
	int hexHeight = clientRect.Height() - hexTop - m_hexBottom;
	if (hexHeight < STR_PANE_MIN) hexHeight = STR_PANE_MIN;
	m_HexWnd.SetWindowPos(NULL, hexRect.left, hexTop,
	                       hexRect.Width(), hexHeight,
	                       SWP_NOZORDER | SWP_NOACTIVATE);
}

void CStringDialog::OnSize(UINT nType, int cx, int cy)
{
	CDarkDialog::OnSize(nType, cx, cy);
	if (nType == SIZE_MINIMIZED) return;
	LayoutSplitter();
}

BOOL CStringDialog::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	if (m_listHeight > 0)
	{
		CPoint pt;
		GetCursorPos(&pt);
		ScreenToClient(&pt);
		int splitterMid = m_listTop + EffectiveListHeight() + m_splitterGap / 2;
		if (abs(pt.y - splitterMid) <= STR_SPLITTER_HIT)
		{
			SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZENS));
			return TRUE;
		}
	}
	return CDarkDialog::OnSetCursor(pWnd, nHitTest, message);
}

void CStringDialog::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (m_listHeight > 0)
	{
		int effH        = EffectiveListHeight();
		int splitterMid = m_listTop + effH + m_splitterGap / 2;
		if (abs(point.y - splitterMid) <= STR_SPLITTER_HIT)
		{
			m_dragging            = true;
			m_dragStartY          = point.y;
			m_dragStartListHeight = effH; // start from visual position, not stored one
			SetCapture();
			return;
		}
	}
	CDarkDialog::OnLButtonDown(nFlags, point);
}

void CStringDialog::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_dragging)
	{
		CRect clientRect;
		GetClientRect(&clientRect);

		int delta     = point.y - m_dragStartY;
		int newHeight = m_dragStartListHeight + delta;
		int maxHeight = clientRect.Height() - m_listTop - m_splitterGap - STR_PANE_MIN - m_hexBottom;
		if (newHeight < m_listHeightMin) newHeight = m_listHeightMin;
		if (newHeight > maxHeight)    newHeight = maxHeight;
		m_listHeight = newHeight;
		LayoutSplitter();
		return;
	}
	CDarkDialog::OnMouseMove(nFlags, point);
}

void CStringDialog::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_dragging)
	{
		m_dragging = false;
		ReleaseCapture();
		return;
	}
	CDarkDialog::OnLButtonUp(nFlags, point);
}

void CStringDialog::OnBnClickedFindButton()
{
	FindString(0);
}

void CStringDialog::OnBnClickedLoadChangesButton()
{
	CFile file;
	if (m_bShowAll)
	{
		CFileDialog dlga(TRUE, TEXT("scf"), 0, OFN_HIDEREADONLY, TEXT("String Changes (*.scf)|*.scf||"), this);
		MO_LOAD_RESOURCES_PATH(dlga)

		if (dlga.DoModal() != IDOK)
			return;
		MO_SAVE_RESOURCES_PATH(dlga)
		if (!file.Open(dlga.GetPathName(), CFile::modeRead))
		{
			MessageBox(TEXT("Unable to load file!"));
			return;
		}

		if (LoadStringChanges(&file)==FALSE)
		{
			file.Close();
			MessageBox(TEXT("The file you chose is not a correct String Changes File!"), TEXT("Error"));
		}
		else
		{
			file.Close();
			MessageBox(TEXT("Changed all strings successfully!"));
		}
	}
	else
	{
		CFileDialog dlgb(TRUE, TEXT("txt"), 0, OFN_HIDEREADONLY, TEXT("String List Files (*.txt)|*.txt||"), this);
		MO_LOAD_RESOURCES_PATH(dlgb)

		if (dlgb.DoModal() != IDOK)
			return;
		MO_SAVE_RESOURCES_PATH(dlgb)
		if (!file.Open(dlgb.GetPathName(), CFile::modeRead))
		{
			MessageBox(TEXT("Unable to load file!"));
			return;
		}
		
		DWORD filelen=(DWORD)file.GetLength();
		if (LoadLanguageBlock(&file, filelen,m_Lang)==FALSE)
		{
			file.Close();
			//MessageBox(TEXT("The file you chose is not a correct String List File!"), TEXT("Error"));
		}
		else
		{
			file.Close();
			MessageBox(TEXT("Loaded string block file successfully!"));
		}
		OnCbnSelchangeLangidxCombo();
	}
}

BOOL CStringDialog::LoadLanguageBlock(CFile *pFile, DWORD filelen, DWORD lang)
{
	if (lang==-1)
		return FALSE;

	LPBYTE lpBuf;
	lpBuf = new BYTE[filelen];
	if (pFile->Read(lpBuf, (UINT)filelen) < filelen)
	{
		MessageBox(TEXT("Unable to read file!"));
		return FALSE;
	}
	//pFile->Close();
	BYTE UTF8_START[3]={0xEF,0xBB,0xBF};
	if (memcmp(lpBuf, &UTF8_START, 3))
	{
		MessageBox(TEXT("The file is not a valid UTF8 text file!"));
		return FALSE;
	}
	CDWordArray lenlist,offsets;
	lenlist.RemoveAll();
	offsets.RemoveAll();
	DWORD start=3,y;
	for (y=3;y<filelen;y++)
	{
		if (y+1 < filelen)
		{
			if (*((LPWORD)&lpBuf[y])==0x0A0D)
			{
				offsets.Add(start);
				if (y>start)
					lenlist.Add(y-start);
				else
					lenlist.Add(0);
				y++;
				start=y+1;
			}
		}
	}
	offsets.Add(start);
	lenlist.Add(y-start);
	// update strings
	DWORD total=GetNumStrings(lang);
	if ((DWORD)offsets.GetCount()!=total)
	{
		MessageBox(TEXT("The number of strings in the text file must be identical to the number of strings on the iPod language block!\nIf you would change this, your iPod could freak out."));
		return FALSE;
	}
	if (m_pFirmware->ResetLangBlock(lang, filelen-(total-1))==FALSE)
	{
		MessageBox(TEXT("There is no room for all the strings combined, trim some strings and then try again."));
		return FALSE;
	}
	LPBYTE lpszBuffer,lpPos;
	DWORD z;
	for (DWORD i = 0;i < total; i++)
	{
		if (lenlist.GetAt(i)==0)
			m_pFirmware->AddLangString(lang, i, NULL, 0);
		else
		{
			lpPos = &lpBuf[offsets.GetAt(i)];
			lpszBuffer = new BYTE[lenlist.GetAt(i)];
			for (y=0,z=0;y<lenlist.GetAt(i);y++,z++)
			{
				if (y+2 < lenlist.GetAt(i))
				{
					if (*((LPWORD)&lpPos[y])==0x000A)
					{
						lpszBuffer[z]=0x0A;
						y++;
					}
					else if (*((LPWORD)&lpPos[y])==0x000D)
					{
						lpszBuffer[z]=0x0D;
						y++;
					}
					else
						lpszBuffer[z]=lpPos[y];
				}
				else
					lpszBuffer[z]=lpPos[y];
			}
			m_pFirmware->AddLangString(lang, i, lpszBuffer, lenlist.GetAt(i));
			delete[] lpszBuffer;
		}
	}
	delete[] lpBuf;
	return TRUE;
}

BOOL CStringDialog::LoadStringChanges(CFile *pFile)
{
	WORD i=0,x=0,y=0,c=-1;
	DWORD offset=-1;
	CWordArray str;
	TCHAR buf[4];
	while (TRUE)
	{
		offset=-1;
		if (pFile->Read(&offset, 4) < 4)
			break;
		else
			x++;
		str.RemoveAll();
		while (TRUE)
		{
			c=-1;
			if (pFile->Read(&c, 2) < 2)
				goto wrongfile;
			str.Add(c);
			if (c==0)
			{
				y++;			
				break;
			}
		}
		m_HexWnd.SetOffset(offset);
		for (i=0;i<str.GetCount();i++)
		{
			wsprintf(buf, TEXT("%02X"), str.GetAt(i));
			m_HexWnd.OnKeyDown((UINT)buf[0], 0, 0);
			m_HexWnd.OnKeyDown((UINT)buf[1], 0, 0);
		}
	}
	if (x!=y)
		goto wrongfile;
	return TRUE;

wrongfile:
	return FALSE;
}

void CStringDialog::OnBnClickedSaveAllChangesButton()
{
	CString filename;
	
	if (m_bShowAll)
	{
		filename.Format(TEXT("Lang-%d.scf"), m_Lang);

		CFileDialog dlga(FALSE, TEXT("scf"), filename, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, TEXT("String Changes (*.scf)|*.scf||"), this);
		MO_LOAD_RESOURCES_PATH(dlga)

		if (dlga.DoModal() != IDOK)
			return;
		MO_SAVE_RESOURCES_PATH(dlga)
		CFile filea;
		if (!filea.Open(dlga.GetPathName(), CFile::modeCreate | CFile::modeWrite))
		{
			MessageBox(TEXT("Unable to save file!"));
			return;
		}
		WORD i=0,j=0,c=0;
		DWORD offset;
		for (i=0;i<m_StringChanges.GetCount();i++)
		{
			templ=m_StringChanges.GetAt(i);
			offset=templ->offset;
			filea.Write(&offset, 4);
			for (j=0;j<templ->b_text.GetCount();j++)
			{
				c=templ->b_text.GetAt(j);
				filea.Write(&c, 2);
			}
		}
		filea.Close();
	}
	else
	{
		filename.Format(TEXT("Lang-%d.txt"), m_Lang);

		CFileDialog dlgb(FALSE, TEXT("txt"), filename, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, TEXT("String Language List (*.txt)|*.txt||"), this);
		MO_LOAD_RESOURCES_PATH(dlgb)

		if (dlgb.DoModal() != IDOK)
			return;
		MO_SAVE_RESOURCES_PATH(dlgb)
		CFile fileb;
		if (!fileb.Open(dlgb.GetPathName(), CFile::modeCreate | CFile::modeWrite))
		{
			MessageBox(TEXT("Unable to save file!"));
			return;
		}

		SaveAllStrings(&fileb);
	}
}

void CStringDialog::SaveAllStrings(CFile *fileb)
{
	//CString s;
		BYTE UTF8_START[3]={0xEF,0xBB,0xBF};
		fileb->Write(UTF8_START, 3);
		/*
		BYTE UNICODE_START[2]={0xFF,0xFE};
		fileb->Write(UNICODE_START, 2);
		char buf[4]={13,0,10,0};
		*/
		DWORD num=GetNumStrings(m_Lang);
		char buf[2]={13,10};
		for (DWORD i=0;i<num;i++)
		{
			/*
			wchar_t *sOut;
			Utf8Decode((char *)m_pFirmware->GetLangString(m_Lang, i), &sOut);
			fileb->Write(sOut, (UINT)(_tcslen(sOut)*2));
			*/
			LPBYTE lpStart=(LPBYTE)m_pFirmware->GetLangString(m_Lang, i);
			LPBYTE lpPos=lpStart;
			DWORD stsize=m_pFirmware->GetLangStringSize(m_Lang, i);
			UINT size=0;
			while (lpPos[0]!=0x00 && (unsigned)(lpPos-lpStart)<stsize)
			{
				if (*((WORD *)lpPos)==0x0A0D)
				{
					size+=4;
					lpPos+=2;
				}
				else
				{
					size++;
					lpPos++;
				}
			}
			LPBYTE lpBuf;
			lpBuf = new BYTE[size];
			UINT y=0;
			lpPos=lpStart;
			while (lpPos[0]!=0x00 && (unsigned)(lpPos-lpStart)<stsize)
			{
				if (*((WORD *)lpPos)==0x0A0D)
				{
					lpBuf[y]=0x0D;
					lpBuf[y+1]=0x00;
					lpBuf[y+2]=0x0A;
					lpBuf[y+3]=0x00;
					y+=4;
					lpPos+=2;
				}
				else
				{
					lpBuf[y]=lpPos[0];
					y++;
					lpPos++;
				}
			}
			
			fileb->Write(lpBuf, size); //remove 0 feed
			delete[] lpBuf;

			if (i+1!=num)
				fileb->Write(&buf, 2);
		}
		fileb->Close();
}

void CStringDialog::OnEnChangeFindEdit()
{
	// TODO:  If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDarkDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.

	// TODO:  Add your control notification handler code here

	UpdateData(TRUE);

	BOOL bEnable = !m_FindString.IsEmpty() && m_pFirmware != NULL;
	GetDlgItem(IDC_FIND_BUTTON)->EnableWindow(bEnable);
	GetDlgItem(IDC_FINDNEXT_BUTTON)->EnableWindow(bEnable);
}

void CStringDialog::OnEnChangeChangeEdit()
{
	UpdateData(TRUE);

	BOOL bEnable = !m_ChangeString.IsEmpty() && m_pFirmware != NULL;
	GetDlgItem(IDC_CHANGETEXT_BUTTON)->EnableWindow(bEnable);
	GetDlgItem(IDC_CHECKTEXT_BUTTON)->EnableWindow(bEnable);
}


// Ctrl+A in the boxes Find, Change Value and Hex GoTo selects all of the text, so what is typed next replaces it
BOOL CStringDialog::PreTranslateMessage(MSG *pMsg)
{
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000))
	{
		CWnd *pFocus = GetFocus();
		if (pFocus != NULL && pFocus->GetParent() == this)
		{
			int id = pFocus->GetDlgCtrlID();
			if (id == IDC_FIND_EDIT || id == IDC_CHANGE_EDIT || id == IDC_ADDR_EDIT)
			{
				((CEdit *)pFocus)->SetSel(0, -1);
				return TRUE;
			}
		}
	}
	return CDarkDialog::PreTranslateMessage(pMsg);
}

void CStringDialog::OnEnChangeAddrEdit()
{
	// what is typed is spaced out in pairs as it is typed ("0123ABCD" shows as "01 23 AB CD"); anything that is not a hex digit
	// is dropped, and the cursor stays after the same digit
	static bool s_bFormatting = false;
	if (s_bFormatting)
		return;
	CEdit *pEdit = (CEdit *)GetDlgItem(IDC_ADDR_EDIT);
	CString text;
	pEdit->GetWindowText(text);
	int selStart = 0, selEnd = 0;
	pEdit->GetSel(selStart, selEnd);
	int digitsBefore = HexDigitsOf(text.Left(selStart)).GetLength();
	CString digits = HexDigitsOf(text);
	CString spaced = SpacedHex(digits);
	if (spaced != text)
	{
		s_bFormatting = true;
		pEdit->SetWindowText(spaced);
		int pos = digitsBefore + (digitsBefore > 0 ? (digitsBefore - 1) / 2 : 0);
		if (digitsBefore > 0 && (digitsBefore % 2) == 0 && digitsBefore < digits.GetLength())
			pos = digitsBefore + digitsBefore / 2;                    // after a full pair the cursor goes behind the space that follows it
		pEdit->SetSel(pos, pos);
		s_bFormatting = false;
	}
	UpdateData(TRUE);
}

void CStringDialog::OnOK()
{
	// TODO: Add your specialized code here and/or call the base class

	// CDarkDialog::OnOK();
}

void CStringDialog::OnLbnSelchangeStringList()
{
	int i = m_StringList.GetCurSel();

	m_HexWnd.SetOffset(GetStringOffset(i));
	RefreshChangeCue();
}

// The selected string is shown in grey in the Change String box while the box is empty: a click on the box or a key makes it go,
// so the new text is typed straight over it. The form follows "Hex string": the text, or its bytes as hex.
void CStringDialog::RefreshChangeCue()
{
	CString cue;
	int sel = -1;
	if (m_pFirmware != NULL)
	{
		if (m_bShowAll)
			sel = m_StringList.GetCurSel();
		else
		{
			POSITION pos = m_LangStringList.GetFirstSelectedItemPosition();
			if (pos != NULL)
				sel = m_LangStringList.GetNextSelectedItem(pos);
		}
	}
	if (sel >= 0)
	{
		DWORD off = GetStringOffset(sel);
		DWORD size = m_pFirmware->GetFirmwareSize();
		LPBYTE fw = m_pFirmware->GetFirmwareBuffer();
		DWORD n = 0;
		while (off + n < size && fw[off + n] != 0 && n < 200)
			n++;
		if (IsDlgButtonChecked(IDC_CHKHEX) == 0)
		{
			CStringA utf8((LPCSTR)(fw + off), (int)n);
			int cch = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
			if (cch > 0)
			{
				LPWSTR w = cue.GetBuffer(cch);
				MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w, cch);
				cue.ReleaseBuffer();
			}
			cue.Replace(_T("\r"), _T(" "));
			cue.Replace(_T("\n"), _T(" "));
		}
		else
		{
			for (DWORD i = 0; i < n; i++)
				cue.AppendFormat(_T("%02X"), fw[off + i]);
		}
	}
	m_ChangeEdit.SetCue(cue);
}

void CStringDialog::OnBnClickedHexString()
{
	RefreshChangeCue();
}

void CStringDialog::OnBnClickedFindnextButton()
{
	if (m_bShowAll)
		FindString(m_StringList.GetTopIndex());
	else
	{
		POSITION p=m_LangStringList.GetFirstSelectedItemPosition();
		FindString(m_LangStringList.GetNextSelectedItem(p));
	}
		//FindString(m_LangStringList.GetTopIndex());
}

void CStringDialog::OnBnClickedChangeTextButton()
{
	if (m_bShowAll)
	{
		WORD idx = m_StringList.GetCurSel();
		if (idx!=65535)
		{
			if (idx>=GetNumStrings(m_Lang))
			{
				MessageBox(TEXT("The 'iPod' string you are about to change is probably not safe to change (there should be 'Cancel' string after it)! Consider your actions and if you still want to, edit manually in hex editor."), TEXT("Warning"));
				return;
			}
			CString s1,s2;
			s1.Format(TEXT("%s"),GetString(m_Lang, idx));
			s2.Format(TEXT("%s"),GetString(m_Lang, idx+1));
			if (s1.Compare(TEXT("iPod"))==0 && s2.Compare(TEXT("Cancel"))!=0)
			{
				if (MessageBox(TEXT("The 'iPod' string you are about to change is probably not safe to change (there should be 'Cancel' string after it)! Consider your actions, do you still want to change the string? (not safe)"), TEXT("Warning"), MB_YESNO) != IDYES)
					return;
			}
		}
	}

	int j,lenm;
	LPBYTE str;
	if (IsDlgButtonChecked(IDC_CHKHEX)==0)
	{
		//String change
		const int cchMultiByte = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)m_ChangeString, -1, NULL, 0, NULL, NULL);
		LPSTR lpa = new CHAR [cchMultiByte];
		if(lpa == NULL){
			return;
		}
		*lpa = '\0';
		const int nMultiCount = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)m_ChangeString, -1, lpa, cchMultiByte, NULL, NULL);
		if( nMultiCount <= 0 ){
			delete[] lpa;
			return;
		}

		lenm=nMultiCount-1; //remove 0
		str=new BYTE[lenm];
		for (j = 0; j < lenm; j++)
		{
			WORD c;
			if (lpa[j]<0)
			{
				c=256;
				c+=lpa[j];
			}
			else
				c=lpa[j];
			str[j]=(BYTE)c;
		}
	}
	else
	{
		//Hex values string
		lenm=m_ChangeString.GetLength();
		str=new BYTE[lenm/2];
		//for (int k=0,j=lenm-2;j >=0;j-=2,k++)
		for (int k=0,j=0;j<lenm;j+=2,k++)
		{
			swscanf(m_ChangeString.Mid(j, 2), TEXT("%X"), &str[k]);
		}
		lenm=lenm/2;
	}

	int p;
	if (m_bShowAll)
	{
		p = m_StringList.GetCurSel();
		templ = new STRING_CHANGE;
		templ->offset=m_HexWnd.m_CursorPos;
		templ->b_text.RemoveAll();
		for (j=0;j<lenm;j++)
			templ->b_text.Add((WORD)str[j]);
		templ->b_text.Add(0);
		m_StringChanges.Add(templ);
		m_pFirmware->SetData(templ->offset, str, lenm);
		BYTE temp=0;
		m_pFirmware->SetData(templ->offset+lenm, &temp, 1);
	}
	else
	{
		POSITION temp=m_LangStringList.GetFirstSelectedItemPosition();
		p = m_LangStringList.GetNextSelectedItem(temp);
		//p--;
		if (!m_pFirmware->SetLangString(m_Lang, p, str, lenm))
		{
			MessageBox(TEXT("There is not enough room to change this string!"));
			return;
		}
		m_HexWnd.SetOffset(GetStringOffset(p));
	}
	m_HexWnd.Invalidate();
	
	//Change the listbox item as well if appropriate
	LPBYTE str2;
	str2=new BYTE[lenm+1];
	memcpy(str2, str, lenm);
	str2[lenm]=0;
	const int cchWideChar = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)str2, -1, NULL, 0);
    LPWSTR lpw = new WCHAR [cchWideChar];
    if(lpw == NULL){
        return;
    }
    *lpw = L'\0';

    const int nUnicodeCount = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)str2, -1, lpw, cchWideChar);
    if( nUnicodeCount <= 0 ){
        delete[] lpw;
        return;
    }

	if (m_bShowAll)
	{
		DWORD off=GetStringOffset(p);
		if (off==m_HexWnd.m_CursorPos)
		{
			m_StringList.DeleteString(p);
			m_StringList.InsertString(p, (LPCTSTR)lpw);
			m_StringList.SetCurSel(p);
		}
	}
	else
	{
		m_LangStringList.SetItemText(p, 1, (LPCTSTR)lpw);
	}
	UpdateData(FALSE);
	RefreshChangeCue();
	//
	delete[] str;
	MessageBox(TEXT("String changed successfully."), TEXT("String Changer"), MB_OK);
	if (m_bShowAll)
		GetDlgItem(IDC_SAVECHANGES_BUTTON)->EnableWindow(TRUE);
}

void CStringDialog::OnBnClickedCheckTextButton()
{
	if (m_HexWnd.m_CursorHalf == 1)
	{
		MessageBox(TEXT("Can't check string changing when starting in a middle of a char. Point the cursor in Hex Editor to the start of the char."), TEXT("Note"), MB_OK);
		return;
	}
	if (m_bShowAll)
	{
		DWORD i;
		int cnt=0;
		i = m_HexWnd.m_CursorPos;
		if (m_HexWnd.m_Buffer[i] == 0 && m_HexWnd.m_Buffer[i-1] != 0)
		{
			MessageBox(TEXT("Can't change a string starting from this position because it will ruin the string before (joins it with the one you're about to add)."), TEXT("String Checker"), MB_OK);
			return;
		}
		while (TRUE)
		{
			if (m_HexWnd.m_Buffer[i] == 0)
				break;
			cnt++;
			i++;
		}
		int len=0;
		if (IsDlgButtonChecked(IDC_CHKHEX)==0)
		{
			char buffer[2048];
			int numofbytes = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)m_ChangeString, m_ChangeString.GetAllocLength(), buffer, sizeof(buffer), 0, 0);
			len=(int)strlen(buffer);
		}
		else
		{
			len=m_ChangeString.GetAllocLength()/2;
		}
		if (cnt == len)
			MessageBox(TEXT("String can be changed exactly, just click 'Change String'"), TEXT("String Checker"), MB_OK);
		else if (len < cnt)
			MessageBox(TEXT("String can be changed and you will be left with extra space (which you cannot really use)."), TEXT("String Checker"), MB_OK);
		else
			MessageBox(TEXT("String cannot be changed without ruining the next string(s) but it's still possible."), TEXT("String Checker"), MB_OK);
	}
	else
	{
		MessageBox(TEXT("Try changing the string using 'Change String' and it will auto tell you if the string is too big."), TEXT("Note"), MB_OK);
	}
}

void CStringDialog::FindString(int start)
{
	UpdateData(TRUE);

	if (m_FindString.IsEmpty())
		return;

	m_FindString.MakeLower();

	// search

	CWaitCursor wait;

	CString s;
	BOOL bFound;
	for (DWORD i = start + 1; i < GetNumStrings(m_Lang); i++)
	{
		s = (LPCTSTR)GetString(m_Lang, i);
		s.MakeLower();

		bFound=FALSE;
		if (IsDlgButtonChecked(IDC_CHKWILD) == 0)
		{
			if (s.Find(m_FindString) != -1)
				bFound=TRUE;
		}
		else
		{
			if (WildMatch(m_FindString, s, TEXT("")))
				bFound=TRUE;
		}
		if (bFound==TRUE)
		{
			if (m_bShowAll)
			{
				m_StringList.SetTopIndex(i);
				m_StringList.SetCurSel(i);
				OnLbnSelchangeStringList();
			}
			else
			{
				m_LangStringList.SetSelectionMark(i);
				LVITEM lim;
				lim.mask=LVIF_STATE;
				lim.iItem=i;
				lim.stateMask=LVIS_SELECTED;
				lim.state=LVIS_SELECTED;
				lim.iSubItem=0;
				m_LangStringList.SetItem(&lim);
				m_LangStringList.EnsureVisible(i, FALSE);
			}
			break;
		}
	}
}

void CStringDialog::OnCbnSelchangeLangidxCombo()
{
	int i=m_Langidx.GetCurSel();
	if (i+1==m_Langidx.GetCount())
		m_bShowAll=TRUE;
	else
		m_bShowAll=FALSE;
	m_Lang=i;
	if (m_bShowAll)
	{
		MessageBox(TEXT("Be Warned!\nEditing this block of strings (the whole raw strings) is very risky and should be done only by program experts!"), TEXT("Warning"));
		GetDlgItem(IDC_SAVECHANGES_BUTTON)->SetWindowText(TEXT("Save Changes"));
		GetDlgItem(IDC_LOADCHANGES_BUTTON)->SetWindowText(TEXT("Load Changes"));
		if (m_StringChanges.GetCount()==0)
			GetDlgItem(IDC_SAVECHANGES_BUTTON)->EnableWindow(FALSE);
	}
	else
	{
		GetDlgItem(IDC_SAVECHANGES_BUTTON)->SetWindowText(TEXT("Save List"));
		GetDlgItem(IDC_LOADCHANGES_BUTTON)->SetWindowText(TEXT("Load List"));
		GetDlgItem(IDC_SAVECHANGES_BUTTON)->EnableWindow(TRUE);
	}
	
	DWORD j;
	if (m_bShowAll)
	{
		m_StringList.ResetContent();
		
		for (j = 0; j < GetNumStrings(m_Lang); j++)
		{
			m_StringList.AddString(GetString(m_Lang, j));
		}
		m_LangStringList.ShowWindow(FALSE);
		m_StringList.ShowWindow(TRUE);
	}
	else
	{
		m_LangStringList.DeleteAllItems();
		CString temp;
		for (j = 0; j < GetNumStrings(m_Lang); j++)
		{
			temp.Format(TEXT("%d"), m_pFirmware->GetLangStringID(m_Lang, j));
			m_LangStringList.InsertItem(j, temp);
			m_LangStringList.SetItemText(j, 1, GetString(m_Lang, j));
		}
		m_StringList.ShowWindow(FALSE);
		m_LangStringList.ShowWindow(TRUE);
	}
	RefreshChangeCue();
}

LPCTSTR CStringDialog::GetString(DWORD lang, DWORD index)
{
	wchar_t *sOut;
	CString s;
	if (m_bShowAll)
	{
		s.Format(TEXT("%s"), m_pFirmware->GetString(index));
	}
	else
	{
		s.Format(TEXT("%s"), m_pFirmware->GetLangString(lang, index));
	}
	Utf8Decode((char *)s.GetBuffer(), &sOut);
	return (LPTSTR)sOut;
}

DWORD CStringDialog::GetNumStrings(DWORD lang)
{
	if (m_bShowAll)
	{
		return m_pFirmware->GetNumStrings();
	}
	else
	{
		return m_pFirmware->GetNumLangStrings(lang);
	}
}

DWORD CStringDialog::GetStringOffset(DWORD index)
{
	DWORD offset=0;
	if (!m_bSFilter)
	{
		if (m_bShowAll)
			offset = (DWORD)((LPBYTE)m_pFirmware->GetString(index) - m_pFirmware->GetFirmwareBuffer());
		else
			offset=(DWORD)((LPBYTE)m_pFirmware->GetLangString(m_Lang, index) - m_pFirmware->GetFirmwareBuffer());
	}
	else
		offset = (DWORD)((LPBYTE)m_pFirmware->GetString(sfilter_w.GetAt(index)) - m_pFirmware->GetFirmwareBuffer());
	return offset;
}

void CStringDialog::OnLvnItemchangedLangstringList(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	//LPNMITEMACTIVATE pNMLV = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);

	if (pNMLV->uOldState!=0)
		return;

	//DWORD i = (DWORD)m_LangStringList.GetFirstSelectedItemPosition();
	//if (i==0)
	//	return;
	//i--;

	m_HexWnd.SetOffset(GetStringOffset(pNMLV->iItem));

	// the string shows in the box in grey and is not put in it: what is typed replaces it with no need to clear the box first
	m_ChangeString.Empty();
	UpdateData(FALSE);
	RefreshChangeCue();

	*pResult = 0;
}

void CStringDialog::OnBnClickedTranslate()
{
	if (m_Lang==m_pFirmware->GetNumLangs()-1)
	{
		MessageBox(TEXT("Choose another language than English"));
		return;
	}

	MessageBox(TEXT("This function compares the selected language with the English language and creates a text language file intended for that language but with English words so it will be easier to translate.\nTranslation will replace selected language.\nAfter you've translated the file, just use Load List."));

	CString filename;

	filename.Format(TEXT("%s.txt"), m_pFirmware->GetName());

	CFileDialog dlgb(FALSE, TEXT("txt"), filename, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, TEXT("String Language List (*.txt)|*.txt||"), this);
	MO_LOAD_RESOURCES_PATH(dlgb)

	if (dlgb.DoModal() != IDOK)
		return;
	MO_SAVE_RESOURCES_PATH(dlgb)
	CFile fileb;
	if (!fileb.Open(dlgb.GetPathName(), CFile::modeCreate | CFile::modeWrite))
	{
		MessageBox(TEXT("Unable to save file!"));
		return;
	}

	BYTE UTF8_START[3]={0xEF,0xBB,0xBF};
	fileb.Write(UTF8_START, 3);
	char buf[2]={13,10};

	DWORD endid,firstid=m_pFirmware->GetNumLangs()-1,secid=m_Lang,i,j,sum1,sum2;
	sum1=GetNumStrings(firstid);
	sum2=GetNumStrings(secid);

	endid=0xFFFFFFFF;
	for (i=sum1-1;i>0;i--)
	{
		for (j=sum2-1;j>0;j--)
		{
			 if (m_pFirmware->GetLangStringID(firstid, i)==m_pFirmware->GetLangStringID(secid, j))
			 {
				 endid=m_pFirmware->GetLangStringID(firstid, i);
				 break;
			 }
		}
		if (endid!=0xFFFFFFFF)
				 break;
	}
	
	for (i=0;i<sum1;i++)
	{
		for (j=0;j<sum2;j++)
		{
			 if (m_pFirmware->GetLangStringID(firstid, i)==m_pFirmware->GetLangStringID(secid, j))
			 {
					LPBYTE lpStart=(LPBYTE)m_pFirmware->GetLangString(firstid, i);
					LPBYTE lpPos=lpStart;
					DWORD stsize=m_pFirmware->GetLangStringSize(firstid, i);
					UINT size=0;
					while (lpPos[0]!=0x00 && (unsigned)(lpPos-lpStart)<stsize)
					{
						if (*((WORD *)lpPos)==0x0A0D)
						{
							size+=4;
							lpPos+=2;
						}
						else
						{
							size++;
							lpPos++;
						}
					}
					LPBYTE lpBuf;
					lpBuf = new BYTE[size];
					UINT y=0;
					lpPos=lpStart;
					while (lpPos[0]!=0x00 && (unsigned)(lpPos-lpStart)<stsize)
					{
						if (*((WORD *)lpPos)==0x0A0D)
						{
							lpBuf[y]=0x0D;
							lpBuf[y+1]=0x00;
							lpBuf[y+2]=0x0A;
							lpBuf[y+3]=0x00;
							y+=4;
							lpPos+=2;
						}
						else
						{
							lpBuf[y]=lpPos[0];
							y++;
							lpPos++;
						}
					}
					
					fileb.Write(lpBuf, size); //remove 0 feed
					delete[] lpBuf;

					if (endid!=m_pFirmware->GetLangStringID(firstid, i))
						fileb.Write(&buf, 2);
			 }
		}
	}
	
	//__int32 temp;
	//for (DWORD i=0;i<GetNumStrings(13);i++)
	//{
	//	temp=m_pFirmware->GetLangStringID(13, i);
	//	fileb.Write(&temp, 4);
	//}

	fileb.Close();
}
