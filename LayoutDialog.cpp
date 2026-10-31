// LayoutDialog.cpp : implementation file
//

#include "stdafx.h"
#include <set>
#include "iPodWizard.h"
#include "LayoutDialog.h"
#include ".\LayoutDialog.h"
#include <algorithm>

// CLayoutDialog dialog

// The hex digits of a text, in capitals, and no more than the eight of an address
CString HexDigitsOf(const CString &s)
{
	CString d;
	for (int i = 0; i < s.GetLength() && d.GetLength() < 8; i++)
	{
		TCHAR c = s[i];
		if (c >= 'a' && c <= 'f')
			c = (TCHAR)(c - 'a' + 'A');
		if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))
			d += c;
	}
	return d;
}

// Digits in pairs: "0123ABCD" -> "01 23 AB CD" (a last single digit stays alone)
CString SpacedHex(const CString &digits)
{
	CString r;
	for (int i = 0; i < digits.GetLength(); i++)
	{
		if (i > 0 && (i % 2) == 0)
			r += ' ';
		r += digits[i];
	}
	return r;
}

CString OffsetCue(DWORD offset)
{
	CString s;
	s.Format(TEXT("%08X"), offset);
	return SpacedHex(s);
}

// The box is empty until something is typed (it shows the current offset of the hex view in grey, see CCueEdit), so there is
// nothing to put into it from the value.
void DDX_MyTextAddrHex2(CDataExchange *pDX, int nIDC, DWORD &value)
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
			if (c == ' ')
				continue;                    // the digits are shown in pairs
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
				pDX->m_pDlgWnd->GetDlgItem(nIDC)->SetWindowText(TEXT(""));
				return;
			}
		}
		value=temp;
	}
}

BEGIN_MESSAGE_MAP(CCueEdit, CEdit)
	ON_WM_PAINT()
	ON_WM_SETFOCUS()
	ON_WM_KILLFOCUS()
END_MESSAGE_MAP()

void CCueEdit::SetCue(const CString &s)
{
	if (s == m_Cue)
		return;
	m_Cue = s;
	if (m_hWnd != NULL)
		Invalidate();
}

void CCueEdit::OnPaint()
{
	Default();                                           // the box as it is
	if (m_Cue.IsEmpty() || GetWindowTextLength() > 0 || ::GetFocus() == m_hWnd)
		return;
	CClientDC dc(this);
	CRect rc;
	GetRect(&rc);
	CFont *pOld = dc.SelectObject(GetFont());
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(DarkMode::Enabled() ? DarkMode::DisabledTextColor() : ::GetSysColor(COLOR_GRAYTEXT));
	dc.IntersectClipRect(rc);
	dc.DrawText(m_Cue, &rc, DT_SINGLELINE | DT_LEFT | DT_TOP | DT_NOPREFIX);
	dc.SelectObject(pOld);
}

void CCueEdit::OnSetFocus(CWnd *pOld)
{
	CEdit::OnSetFocus(pOld);
	Invalidate();
}

void CCueEdit::OnKillFocus(CWnd *pNew)
{
	CEdit::OnKillFocus(pNew);
	Invalidate();
}

IMPLEMENT_DYNAMIC(CLayoutDialog, CDarkDialog)
CLayoutDialog::CLayoutDialog(CWnd* pParent /*=NULL*/)
	: CDarkDialog(CLayoutDialog::IDD, pParent)
	, m_pFirmware(NULL)
	, m_ResourceType(0)
	, m_CmdFunction(0)
	, m_ClickedCol(-1), m_bLinkClick(false), m_bValBigEndian(false)
	, m_RequestedMode(0)
	, m_UsedShown(false)
	, m_listTop0(0)
	, m_curTree(0)
	, m_LastCmdMode(3)
	, m_bNoRestore(false)
	, m_bListsBuilt(false)
	, m_bModesReady(false)
	, m_FindString(_T(""))
	, m_ChangeString(_T(""))
	, m_HexAddr(0)
	, m_bSFilter(FALSE)
	, m_hexBottom(0)
	, m_selectorColor1(0)
	, m_selectorColor2(0)
	, m_selectorCtxValid(FALSE)
	, m_paneTop(0)
	, m_paneHeight(0)
	, m_paneHeightMin(0)
	, m_splitterGap(0)
	, m_dragging(false)
	, m_dragStartY(0)
	, m_dragStartPaneHeight(0)
	, m_treeLeft(0)
	, m_treeWidth(0)
	, m_colGap(0)
	, m_listRightMargin(0)
	, m_draggingCol(false)
	, m_dragStartX(0)
	, m_dragStartTreeWidth(0)
{
	memset(m_selectorCtxPre,  0, sizeof(m_selectorCtxPre));
	memset(m_selectorCtxPost, 0, sizeof(m_selectorCtxPost));
	memset(m_hTree, 0, sizeof(m_hTree));
	memset(m_treeBuilt, 0, sizeof(m_treeBuilt));
}

CLayoutDialog::~CLayoutDialog()
{
}

void CLayoutDialog::DoDataExchange(CDataExchange* pDX)
{
	CDarkDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_RESOURCE_LIST, m_ResourceList);
	DDX_Control(pDX, IDC_RESOURCE_TREE, m_ResourceTree);
	DDX_Control(pDX, IDC_HEX, m_HexWnd);
	DDX_Control(pDX, IDC_LANGIDX_COMBO, m_ResourceTypeIdx);
	DDX_Text(pDX, IDC_FIND_EDIT, m_FindString);
	DDX_Text(pDX, IDC_CHANGE_EDIT, m_ChangeString);
	DDX_MyTextAddrHex2(pDX, IDC_ADDR_EDIT, m_HexAddr);
}


BEGIN_MESSAGE_MAP(CLayoutDialog, CDarkDialog)
	ON_BN_CLICKED(IDC_FIND_BUTTON, OnBnClickedFindButton)
	ON_EN_CHANGE(IDC_FIND_EDIT, OnEnChangeFindEdit)
	ON_BN_CLICKED(IDC_FINDNEXT_BUTTON, OnBnClickedFindnextButton)
	ON_BN_CLICKED(IDC_CHANGETEXT_BUTTON, OnBnClickedChangeTextButton)
	ON_EN_CHANGE(IDC_CHANGE_EDIT, OnEnChangeChangeEdit)
	ON_BN_CLICKED(IDC_SAVECHANGES_BUTTON, OnBnClickedSaveResourceButton)
	ON_BN_CLICKED(IDC_LOADCHANGES_BUTTON, OnBnClickedLoadChangesButton)
	ON_BN_CLICKED(IDC_JUMP_BUTTON, OnBnClickedJumpToButton)
	ON_WM_TIMER()
	ON_WM_DESTROY()
	ON_BN_CLICKED(IDC_RADIO_STRINGVAL, OnBnClickedValueKind)
	ON_BN_CLICKED(IDC_RADIO_HEXVAL, OnBnClickedValueKind)
	ON_BN_CLICKED(IDC_RADIO_DECVAL, OnBnClickedValueKind)
	ON_EN_CHANGE(IDC_ADDR_EDIT, OnEnChangeAddrEdit)
	ON_CBN_SELCHANGE(IDC_LANGIDX_COMBO, OnCbnSelchangeResourceTypeidxCombo)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_RESOURCE_LIST, OnLvnItemchangedResourceList)
	ON_NOTIFY(NM_DBLCLK, IDC_RESOURCE_LIST, OnLvnItemDblClickedResourceList)
	ON_NOTIFY(NM_CLICK, IDC_RESOURCE_LIST, OnLvnItemClickedResourceList)
	//ON_NOTIFY(LVN_ITEMACTIVATE, IDC_RESOURCE_LIST, OnLvnItemchangedResourceList)
	ON_NOTIFY(TVN_SELCHANGED, IDC_RESOURCE_TREE, &CLayoutDialog::OnTvnSelchangedResourceTree)
	ON_NOTIFY(TVN_ITEMEXPANDING, IDC_RESOURCE_TREE, &CLayoutDialog::OnTvnItemexpandingResourceTree)
	ON_NOTIFY(TCN_SELCHANGE, IDC_CMD_SUBTABS, &CLayoutDialog::OnTcnSelchangeCmdTabs)
	ON_NOTIFY(NM_RCLICK, IDC_RESOURCE_LIST, &CLayoutDialog::OnNMRClickResourceList)
	ON_NOTIFY(NM_RCLICK, IDC_RESOURCE_TREE, &CLayoutDialog::OnNMRClickResourceTree)
	ON_BN_CLICKED(IDC_SAVEALLRESOURCES_BUTTON, &CLayoutDialog::OnBnClickedSaveallresourcesButton)
	ON_BN_CLICKED(IDC_LOADALLRESOURCES_BUTTON, &CLayoutDialog::OnBnClickedLoadallresourcesButton)
	ON_BN_CLICKED(IDC_SELECTORBAR_BUTTON, &CLayoutDialog::OnBnClickedSelectorBarButton)
#ifdef IPW_TESTHOOK
	ON_MESSAGE(WM_USER + 777, OnTestHook)
#endif
	ON_WM_SIZE()
	ON_WM_SETCURSOR()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_CBN_SELENDOK(IDC_USEDBY_COMBO, OnCbnSelendokUsedCombo)
	ON_MESSAGE(WM_USER + 778, OnListPainted)
END_MESSAGE_MAP()

// CLayoutDialog message handlers

static bool s_bTreeBuilding = false;                 // the tree is being filled or emptied: its selection changes are not acted on

BOOL CLayoutDialog::OnInitDialog()
{
	CDarkDialog::OnInitDialog();

	// the tip of the two Load buttons; a button that is off gets no mouse messages, so PreTranslateMessage hands them to the tip
	m_LoadTip.Create(this, TTS_ALWAYSTIP);
	m_LoadTip.SetMaxTipWidth(300);
	m_LoadTip.AddTool(GetDlgItem(IDC_LOADCHANGES_BUTTON), _T("Old resource saving is enabled."));
	m_LoadTip.AddTool(GetDlgItem(IDC_LOADALLRESOURCES_BUTTON), _T("Old resource saving is enabled."));
	m_LoadTip.Activate(FALSE);

	// the scroll bars of the tree and the list look like the ones of the font viewers (arrow buttons), where they are and used the same
	m_ChangeEdit.SubclassDlgItem(IDC_CHANGE_EDIT, this);
	m_AddrEdit.SubclassDlgItem(IDC_ADDR_EDIT, this);
	m_AddrCueShown = 0xFFFFFFFF;
	SetTimer(IDT_ADDRCUE, 150, NULL);                // the hex view moves its cursor by itself: the hint of Hex GoTo follows it
	DarkMode::UseClassicScrollBars(m_ResourceTree.m_hWnd);
	DarkMode::UseClassicScrollBars(m_ResourceList.m_hWnd);

	// TODO:  Add extra initialization here
	reinterpret_cast<CEdit*>(GetDlgItem(IDC_ADDR_EDIT))->LimitText(11);       // eight digits and the three spaces between the pairs

	m_ResourceList.SetExtendedStyle(m_ResourceList.GetExtendedStyle()|LVS_EX_FULLROWSELECT);

	this->CheckDlgButton(IDC_RADIO_DECVAL, 1);

	CRect clientRect, treeRect, hexRect;
	GetClientRect(&clientRect);
	m_ResourceTree.GetWindowRect(&treeRect);
	ScreenToClient(&treeRect);
	m_HexWnd.GetWindowRect(&hexRect);
	ScreenToClient(&hexRect);
	m_paneTop      = treeRect.top;
	m_paneHeight   = treeRect.Height();
	m_paneHeightMin = treeRect.Height();
	m_splitterGap  = hexRect.top - treeRect.bottom;
	m_hexBottom    = clientRect.Height() - hexRect.bottom;

	CRect listRect0;
	m_ResourceList.GetWindowRect(&listRect0);
	ScreenToClient(&listRect0);
	m_treeLeft         = treeRect.left;
	m_treeWidth        = treeRect.Width();
	m_colGap           = listRect0.left - treeRect.right;
	m_listRightMargin  = clientRect.Width() - listRect0.right;
	m_Stretch.Init(this);
	m_listTop0 = listRect0.top;

	// one tree for each mode, made once; m_ResourceTree stands for the one that is shown (ActivateTree)
	for (int i = 0; i < 4; i++)
	{
		m_hTree[i] = NULL;
		m_treeBuilt[i] = false;
	}
	m_hTree[0] = m_ResourceTree.UnsubclassWindow();
	for (int i = 1; i < 4; i++)
	{
		m_hTree[i] = ::CreateWindowEx(::GetWindowLong(m_hTree[0], GWL_EXSTYLE), WC_TREEVIEW, _T(""),
		                              (::GetWindowLong(m_hTree[0], GWL_STYLE) & ~WS_VISIBLE) | WS_CHILD,
		                              treeRect.left, treeRect.top, treeRect.Width(), treeRect.Height(),
		                              m_hWnd, (HMENU)(UINT_PTR)IDC_RESOURCE_TREE, AfxGetInstanceHandle(), NULL);
		::SendMessage(m_hTree[i], WM_SETFONT, ::SendMessage(m_hTree[0], WM_GETFONT, 0, 0), TRUE);
		DarkMode::ApplyToWindow(m_hTree[i]);
		DarkMode::UseClassicScrollBars(m_hTree[i]);       // (the first tree is done above; these are made later and nothing styles them otherwise)
	}
	m_ResourceTree.SubclassWindow(m_hTree[0]);
	m_curTree = 0;

	// "Table" and "Instructions": the two ways the commands are shown, above the list at the right
	m_CmdTabs.Create(WS_CHILD | WS_TABSTOP, CRect(0, 0, 100, 22), this, IDC_CMD_SUBTABS);
	m_CmdTabs.SetFont(GetFont());
	m_CmdTabs.InsertItem(0, _T("Table"));
	m_CmdTabs.InsertItem(1, _T("Instructions"));
	DarkMode::ApplyToWindow(m_CmdTabs.m_hWnd);

	// the drop-down of the places a font type is used in; it sits over the cell and is only shown for font types
	m_UsedCombo.Create(CBS_DROPDOWNLIST | WS_CHILD | WS_VSCROLL | WS_TABSTOP, CRect(0, 0, 100, 200), &m_ResourceList, IDC_USEDBY_COMBO);
	// the drop-down is a child of the list itself: the list does not draw over its own children, and the drop-down needs
	// no pushing back on top after every draw
	m_ResourceList.ModifyStyle(0, WS_CLIPCHILDREN);
	m_UsedCombo.SetFont(GetFont());
	m_UsedCombo.SetItemHeight(-1, 12);
	DarkMode::ApplyToWindow(m_UsedCombo.m_hWnd);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CLayoutDialog::SetFirmware(CFirmware *pFirmware)
{
	m_pFirmware = pFirmware;
	m_bListsBuilt = false;
	m_bModesReady = false;
	for (int i = 0; i < 5; i++)
		m_State[i] = ModeState();
	m_LastCmdMode = 3;
	s_bTreeBuilding = true;                        // what is selected while the trees are emptied is of no interest
	for (int i = 0; i < 4; i++)
	{
		m_treeBuilt[i] = false;
		if (m_hTree[i] != NULL)
			TreeView_DeleteAllItems(m_hTree[i]);
	}
	s_bTreeBuilding = false;
	m_Names.Build(m_pFirmware);
	m_Commands.Build(m_pFirmware, &m_Names);
	m_Attrs.Build(m_pFirmware, &m_Names, &m_Commands);
	m_CmdFunction = 0;
	m_CmdLines.clear();

	// update strings
	m_ResourceTypeIdx.ResetContent();
	CString s;
	
	ViewList=m_pFirmware->GetViewList();
	MenuList=m_pFirmware->GetMenuList();
	TypeList=m_pFirmware->GetTypeList();
	TCMDList=m_pFirmware->GetTCMDList();
	m_ResourceTypeIdx.InsertString(0, _T("Layout"));
	m_ResourceTypeIdx.InsertString(1, _T("Menus"));
	m_ResourceTypeIdx.InsertString(2, _T("Font Types"));
	m_ResourceTypeIdx.InsertString(3, TEXT("Command Table"));
	m_ResourceTypeIdx.InsertString(4, TEXT("Command Instructions"));
	m_ResourceTypeIdx.SetCurSel(m_RequestedMode);

	//MessageBox(_T("S"));

	if (ViewList==NULL || MenuList==NULL || TypeList==NULL)
		goto disable;
	if (ViewList->size()<1 || MenuList->size()<1 || TypeList->size()<1)
		goto disable;

	m_bModesReady = true;
	if (theApp.m_LayoutRender==FALSE)
		OnCbnSelchangeResourceTypeidxCombo();
	else
	{
		m_ResourceTree.DeleteAllItems();
		UpdateModeText();
	}

	m_HexWnd.SetFirmware(m_pFirmware);
	GetDlgItem(IDC_FIND_EDIT)->EnableWindow(TRUE);
	GetDlgItem(IDC_CHANGE_EDIT)->EnableWindow(TRUE);
	//GetDlgItem(IDC_CHKWILD)->EnableWindow(TRUE);
	GetDlgItem(IDC_LOADCHANGES_BUTTON)->EnableWindow(TRUE);
	GetDlgItem(IDC_JUMP_BUTTON)->EnableWindow(TRUE);
	GetDlgItem(IDC_ADDR_EDIT)->EnableWindow(TRUE);
	GetDlgItem(IDC_RADIO_STRINGVAL)->EnableWindow(TRUE);
	GetDlgItem(IDC_RADIO_HEXVAL)->EnableWindow(TRUE);
	GetDlgItem(IDC_LANGIDX_COMBO)->EnableWindow(TRUE);
	GetDlgItem(IDC_SAVEALLRESOURCES_BUTTON)->EnableWindow(TRUE);
	GetDlgItem(IDC_LOADALLRESOURCES_BUTTON)->EnableWindow(TRUE);
	GetDlgItem(IDC_SELECTORBAR_BUTTON)->EnableWindow(TRUE);
	UpdateLoadButtons();

	UpdateData(FALSE);
	return;

disable:
	GetDlgItem(IDC_FIND_EDIT)->EnableWindow(FALSE);
	GetDlgItem(IDC_CHANGE_EDIT)->EnableWindow(FALSE);
	//GetDlgItem(IDC_CHKWILD)->EnableWindow(FALSE);
	GetDlgItem(IDC_LOADCHANGES_BUTTON)->EnableWindow(FALSE);
	GetDlgItem(IDC_JUMP_BUTTON)->EnableWindow(FALSE);
	GetDlgItem(IDC_ADDR_EDIT)->EnableWindow(FALSE);
	GetDlgItem(IDC_RADIO_STRINGVAL)->EnableWindow(FALSE);
	GetDlgItem(IDC_RADIO_HEXVAL)->EnableWindow(FALSE);
	GetDlgItem(IDC_LANGIDX_COMBO)->EnableWindow(FALSE);
	GetDlgItem(IDC_SAVEALLRESOURCES_BUTTON)->EnableWindow(FALSE);
	GetDlgItem(IDC_LOADALLRESOURCES_BUTTON)->EnableWindow(FALSE);
	GetDlgItem(IDC_SELECTORBAR_BUTTON)->EnableWindow(FALSE);

	UpdateData(FALSE);
}

// ---------------------------------------------------------------------------------------------------------
// "Change Selector Bar Color": the two colors of the gradient of the selection bar
//
// 5.5G and the later 5G builds (13.6.2.1 and up): every element of the layouts carries the two colors as its first two
//     extra values (about 1650 places, found from the layouts: the places of one build are not those of another).
// 5G, the older builds (13.6.0 to 13.6.1.2): the colors are in the program code: one small function stores them as the
//     immediates of six MOV instructions (mov r1,#R / strb r1,[r0] / mov r1,#G / strb r1,[r0,#1] / mov r1,#B /
//     strb r1,[r0,#2], then the same with r2 for the second color). The pattern below does not depend on the colors.
// ---------------------------------------------------------------------------------------------------------

// The function of the 5G that stores the two colors, from its first MOV on (the 17 instruction words; the immediate of
// the six MOVs that hold the colors is not compared). Words 0, 2 and 4 hold the red, green and blue of the light color,
// words 9, 11 and 13 those of the dark one.
static const DWORD kSelectorCode5G[17][2] = {
	{ 0xE3A01000, 0xFFFFFF00 }, { 0xE5C01000, 0xFFFFFFFF },        // mov r1,#R   strb r1,[r0]
	{ 0xE3A01000, 0xFFFFFF00 }, { 0xE5C01001, 0xFFFFFFFF },        // mov r1,#G   strb r1,[r0,#1]
	{ 0xE3A01000, 0xFFFFFF00 }, { 0xE5C01002, 0xFFFFFFFF },        // mov r1,#B   strb r1,[r0,#2]
	{ 0xE3A010FF, 0xFFFFFFFF }, { 0xE5C01003, 0xFFFFFFFF },        // mov r1,#255 strb r1,[r0,#3]  (opacity)
	{ 0xE2800004, 0xFFFFFFFF },                                    // add r0,r0,#4
	{ 0xE3A02000, 0xFFFFFF00 }, { 0xE5C02000, 0xFFFFFFFF },        // mov r2,#R   strb r2,[r0]
	{ 0xE3A02000, 0xFFFFFF00 }, { 0xE5C02001, 0xFFFFFFFF },        // mov r2,#G   strb r2,[r0,#1]
	{ 0xE3A02000, 0xFFFFFF00 }, { 0xE5C02002, 0xFFFFFFFF },        // mov r2,#B   strb r2,[r0,#2]
	{ 0xE5C01003, 0xFFFFFFFF },                                    // strb r1,[r0,#3]
	{ 0xE12FFF1E, 0xFFFFFFFF },                                    // bx lr
};

static void FindSelectorCode5G(const BYTE *buf, DWORD sz, vector<DWORD> &hits)
{
	for (DWORD i = 0; i + 17 * 4 <= sz; i += 4)
	{
		const DWORD *w = (const DWORD *)&buf[i];
		if ((w[0] & 0xFFFFFF00) != 0xE3A01000)
			continue;
		bool ok = true;
		for (int k = 1; k < 17 && ok; k++)
			ok = (w[k] & kSelectorCode5G[k][1]) == kSelectorCode5G[k][0];
		if (ok)
			hits.push_back(i);
	}
}

// The places of the two colors in a 5.5G firmware: the first two extra values of every layout element that has them
void CLayoutDialog::FindSelectorSlots(vector<DWORD> &slots)
{
	if (ViewList == NULL || m_pFirmware == NULL)
		return;
	const BYTE *buf = m_pFirmware->GetFirmwareBuffer();
	for (size_t v = 0; v < ViewList->size(); v++)
	{
		ViewRecord *pv = ViewList->at(v);
		for (size_t k = 0; k < pv->ViewElemList.size(); k++)
		{
			const resource_type_view_elem *e = pv->ViewElemList[k]->element;
			CLayoutAttrs::Extra x;
			CLayoutAttrs::ExtraOf(e, &x);
			if (x.gradient && x.p[0] != x.p[1])
				slots.push_back((DWORD)((const BYTE *)e - buf) + 76);
		}
	}
}

// The notice of the tweak (with its credit) has the choice of the iPod on it: 5G, 5.5G or Cancel
static INT_PTR CALLBACK SelectorBarDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INITDIALOG:
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_SELBAR_5G:
		case IDC_SELBAR_55G:
		case IDCANCEL:
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		break;
	}
	return FALSE;
}

void CLayoutDialog::OnBnClickedSelectorBarButton()
{
	INT_PTR which = DialogBox(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDD_SELECTOR_BAR), m_hWnd, SelectorBarDlgProc);
	if (which == IDC_SELBAR_5G)
		ChangeSelectorBar5G();
	else if (which == IDC_SELBAR_55G)
		ChangeSelectorBar55G();
}

void CLayoutDialog::ChangeSelectorBar5G()
{
	LPBYTE buf = m_pFirmware->GetFirmwareBuffer();
	DWORD  sz  = m_pFirmware->GetFirmwareSize();

	// the 5G and the 5.5G are the family 0xB of the images; the photo (0x6) and the nano (0xC) are not supported yet
	if ((m_pFirmware->GetFirmwareVersion() >> 12) != 0xB)
	{
		MessageBox(_T("This is not a 5G firmware. Only the 5G and the 5.5G are supported."), _T("Selector Bar Color Changer"));
		return;
	}

	// the later 5G builds (13.6.2.1, 13.6.3) keep the colors in the layouts, like the 5.5G
	vector<DWORD> slots;
	FindSelectorSlots(slots);
	if (!slots.empty())
	{
		ChangeSelectorBar55G();
		return;
	}

	vector<DWORD> hits;
	FindSelectorCode5G(buf, sz, hits);
	if (hits.empty())
	{
		MessageBox(_T("Selector bar gradient not found. Is this a 5G firmware?"), _T("Selector Bar Color Changer"));
		return;
	}

	// the colors the first function holds (COLORREF: red in the lowest byte)
	static const int kLight[3] = { 0, 2, 4 };
	static const int kDark[3]  = { 9, 11, 13 };
	const DWORD *w = (const DWORD *)&buf[hits[0]];
	unsigned int light = RGB(w[kLight[0]] & 0xFF, w[kLight[1]] & 0xFF, w[kLight[2]] & 0xFF);
	unsigned int dark  = RGB(w[kDark[0]] & 0xFF,  w[kDark[1]] & 0xFF,  w[kDark[2]] & 0xFF);

	// the dark color is asked first, as for the 5.5G, where the gradient starts with it
	bool changed = false;
	unsigned int ret_color;
	if (GetUserColor(dark, &ret_color, GetParent()->GetParent()->m_hWnd) == true)
	{
		dark = ret_color & 0x00FFFFFF;
		changed = true;
	}
	if (GetUserColor(light, &ret_color, GetParent()->GetParent()->m_hWnd) == true)
	{
		light = ret_color & 0x00FFFFFF;
		changed = true;
	}
	if (!changed)
		return;

	// only the immediates change; the opacity stays
	for (size_t h = 0; h < hits.size(); h++)
		for (int k = 0; k < 6; k++)
		{
			int idx = k < 3 ? kLight[k] : kDark[k - 3];
			unsigned int color = k < 3 ? light : dark;
			DWORD at = hits[h] + 4 * idx;
			DWORD word = (*(const DWORD *)&buf[at] & 0xFFFFFF00) | ((color >> (8 * (k % 3))) & 0xFF);
			m_pFirmware->SetData(at, (LPBYTE)&word, 4);
		}

	m_selectorColor1 = dark | 0xFF000000;
	m_selectorColor2 = light | 0xFF000000;
	m_HexWnd.Invalidate();

	CString msg;
	msg.Format(_T("Selector bar color updated at %d location(s).\n\nWrite the firmware to the iPod to apply the change."), (int)hits.size());
	MessageBox(msg, _T("Selector Bar Color Changer"));
}

void CLayoutDialog::ChangeSelectorBar55G()
{
	LPBYTE buf = m_pFirmware->GetFirmwareBuffer();
	DWORD  sz  = m_pFirmware->GetFirmwareSize();

	vector<DWORD> slots;
	FindSelectorSlots(slots);

	if (slots.empty())
	{
		vector<DWORD> code;
		FindSelectorCode5G(buf, sz, code);
		if ((m_pFirmware->GetFirmwareVersion() >> 12) == 0xB && !code.empty())
			MessageBox(_T("This firmware keeps the selector bar colors in its program code. Choose 5G."), _T("Selector Bar Color Changer"));
		else
			MessageBox(_T("Selector bar gradient not found. Is this a 5.5G firmware?"), _T("Selector Bar Color Changer"));
		return;
	}

	// Read the current gradient colours from the first element that has them.
	unsigned int color1 = *(unsigned int *)&buf[slots[0]];
	unsigned int color2 = *(unsigned int *)&buf[slots[0] + 4];
	const unsigned int old1 = color1, old2 = color2;

	bool changed = false;
	unsigned int ret_color;
	// Mask out alpha byte before passing to ChooseColor — COLORREF must be <= 0x00FFFFFF
	if (GetUserColor(color1 & 0x00FFFFFF, &ret_color, GetParent()->GetParent()->m_hWnd) == true)
	{
		color1 = ret_color | 0xFF000000;
		changed = true;
	}
	if (GetUserColor(color2 & 0x00FFFFFF, &ret_color, GetParent()->GetParent()->m_hWnd) == true)
	{
		color2 = ret_color | 0xFF000000;
		changed = true;
	}

	if (!changed)
		return;

	BYTE newPair[8];
	*(DWORD *)&newPair[0] = color1;
	*(DWORD *)&newPair[4] = color2;

	// Write the new colours to every place that holds the colours that were read (an element with a gradient of
	// its own keeps it).
	int count = 0;
	for (size_t k = 0; k < slots.size(); k++)
	{
		DWORD off = slots[k];
		if (off + 7 < sz && *(unsigned int *)&buf[off] == old1 && *(unsigned int *)&buf[off + 4] == old2)
		{
			memcpy(&buf[off], newPair, 8);
			count++;
		}
	}

	m_selectorColor1 = color1;
	m_selectorColor2 = color2;

	CString msg;
	msg.Format(_T("Selector bar color updated at %d location(s).\n\nWrite the firmware to the iPod to apply the change."), count);
	MessageBox(msg, _T("Selector Bar Color Changer"));
}

void CLayoutDialog::OnTimer(UINT_PTR nIDEvent)
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

void CLayoutDialog::OnDestroy()
{
	KillTimer(IDT_ADDRCUE);
	CDarkDialog::OnDestroy();
}

void CLayoutDialog::OnBnClickedJumpToButton()
{
	if (GetDlgItem(IDC_ADDR_EDIT)->GetWindowTextLength() == 0)
		return;                                  // nothing typed: the box only shows the current offset
	if (m_HexAddr<=m_pFirmware->GetFirmwareSize())
		m_HexWnd.SetOffset(m_HexAddr);
}

static const int LAY_SPLITTER_HIT = 5;
static const int LAY_PANE_MIN     = 20;

int CLayoutDialog::EffectivePaneHeight()
{
	CRect clientRect;
	GetClientRect(&clientRect);
	int maxPaneHeight = clientRect.Height() - m_paneTop - m_splitterGap - LAY_PANE_MIN - m_hexBottom;
	int paneH = m_paneHeight;
	if (paneH < m_paneHeightMin) paneH = m_paneHeightMin;
	if (paneH > maxPaneHeight)   paneH = maxPaneHeight;
	if (paneH < m_paneHeightMin) paneH = m_paneHeightMin;
	return paneH;
}

void CLayoutDialog::LayoutSplitter()
{
	if (m_paneHeight == 0) return;
	if (!IsWindow(m_ResourceTree.GetSafeHwnd())) return;
	if (!IsWindow(m_ResourceList.GetSafeHwnd())) return;
	if (!IsWindow(m_HexWnd.GetSafeHwnd())) return;

	CRect clientRect, treeRect, listRect, hexRect;
	GetClientRect(&clientRect);

	// the hex view and the two boxes Find and Change Value take the extra width, and the buttons beside the boxes go with their
	// right end; the buttons of the top row stay at the left; the tree and the list are placed below
	static const int stretchIds[] = { IDC_HEX, IDC_FIND_EDIT, IDC_CHANGE_EDIT };
	static const int skipIds[]    = { IDC_RESOURCE_TREE, IDC_RESOURCE_LIST };
	static const int moveIds[]    = { IDC_FIND_BUTTON, IDC_FINDNEXT_BUTTON, IDC_CHKWILD, IDC_CHANGETEXT_BUTTON,
	                                  IDC_RADIO_STRINGVAL, IDC_RADIO_HEXVAL, IDC_RADIO_DECVAL };
	m_Stretch.Apply(this, StretchExtraWidth(), stretchIds, 3, skipIds, 2, false, moveIds, 7);

	m_ResourceTree.GetWindowRect(&treeRect); ScreenToClient(&treeRect);
	m_ResourceList.GetWindowRect(&listRect); ScreenToClient(&listRect);
	m_HexWnd.GetWindowRect(&hexRect);        ScreenToClient(&hexRect);

	int paneH = EffectivePaneHeight();
	int treeW = EffectiveTreeWidth();
	int listLeft  = m_treeLeft + treeW + m_colGap;
	int listWidth = PageWidth() - m_listRightMargin - listLeft;
	if (listWidth < 20) listWidth = 20;

	for (int i = 0; i < 4; i++)
		if (m_hTree[i] != NULL)
			::SetWindowPos(m_hTree[i], NULL, m_treeLeft, treeRect.top, treeW, paneH, SWP_NOZORDER | SWP_NOACTIVATE);
	const int tabH = IsCommandMode() ? 22 : 0;
	m_ResourceList.SetWindowPos(NULL, listLeft, m_listTop0 + tabH,
	                             listWidth, paneH - tabH,
	                             SWP_NOZORDER | SWP_NOACTIVATE);
	if (m_CmdTabs.m_hWnd != NULL)
		m_CmdTabs.SetWindowPos(NULL, listLeft, m_listTop0, listWidth, 22, SWP_NOZORDER | SWP_NOACTIVATE);

	int hexTop    = m_paneTop + paneH + m_splitterGap;
	int hexHeight = clientRect.Height() - hexTop - m_hexBottom;
	if (hexHeight < LAY_PANE_MIN) hexHeight = LAY_PANE_MIN;
	m_HexWnd.SetWindowPos(NULL, hexRect.left, hexTop,
	                       hexRect.Width(), hexHeight,
	                       SWP_NOZORDER | SWP_NOACTIVATE);
}

void CLayoutDialog::OnSize(UINT nType, int cx, int cy)
{
	CDarkDialog::OnSize(nType, cx, cy);
	if (nType == SIZE_MINIMIZED) return;
	LayoutSplitter();
}

static const int LAY_COLUMN_MIN = 80;

// The width the page is laid out for: its template width plus how much wider the main window is than its minimum
int CLayoutDialog::PageWidth()
{
	return m_Stretch.BaseWidth() + StretchExtraWidth();
}

int CLayoutDialog::EffectiveTreeWidth()
{
	int maxW = PageWidth() - m_listRightMargin - m_treeLeft - m_colGap - LAY_COLUMN_MIN;
	int w = m_treeWidth;
	if (w > maxW) w = maxW;
	if (w < LAY_COLUMN_MIN) w = LAY_COLUMN_MIN;
	return w;
}

// Is the point on the narrow gap between the tree and the list?
bool CLayoutDialog::OnColumnSplitter(CPoint pt)
{
	if (m_paneHeight <= 0)
		return false;
	int mid = m_treeLeft + EffectiveTreeWidth() + m_colGap / 2;
	return abs(pt.x - mid) <= LAY_SPLITTER_HIT && pt.y >= m_paneTop && pt.y <= m_paneTop + EffectivePaneHeight();
}

BOOL CLayoutDialog::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	if (m_paneHeight > 0)
	{
		CPoint pt;
		GetCursorPos(&pt);
		ScreenToClient(&pt);
		if (OnColumnSplitter(pt))
		{
			SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZEWE));
			return TRUE;
		}
		int splitterMid = m_paneTop + EffectivePaneHeight() + m_splitterGap / 2;
		if (abs(pt.y - splitterMid) <= LAY_SPLITTER_HIT)
		{
			SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZENS));
			return TRUE;
		}
	}
	return CDarkDialog::OnSetCursor(pWnd, nHitTest, message);
}

void CLayoutDialog::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (OnColumnSplitter(point))
	{
		m_draggingCol        = true;
		m_dragStartX         = point.x;
		m_dragStartTreeWidth = EffectiveTreeWidth();
		SetCapture();
		return;
	}
	if (m_paneHeight > 0)
	{
		int effH        = EffectivePaneHeight();
		int splitterMid = m_paneTop + effH + m_splitterGap / 2;
		if (abs(point.y - splitterMid) <= LAY_SPLITTER_HIT)
		{
			m_dragging             = true;
			m_dragStartY           = point.y;
			m_dragStartPaneHeight  = effH;
			SetCapture();
			return;
		}
	}
	CDarkDialog::OnLButtonDown(nFlags, point);
}

void CLayoutDialog::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_draggingCol)
	{
		m_treeWidth = m_dragStartTreeWidth + (point.x - m_dragStartX);
		m_treeWidth = EffectiveTreeWidth();         // within the limits
		LayoutSplitter();
		return;
	}
	if (m_dragging)
	{
		CRect clientRect;
		GetClientRect(&clientRect);

		int delta     = point.y - m_dragStartY;
		int newHeight = m_dragStartPaneHeight + delta;
		int maxHeight = clientRect.Height() - m_paneTop - m_splitterGap - LAY_PANE_MIN - m_hexBottom;
		if (newHeight < m_paneHeightMin) newHeight = m_paneHeightMin;
		if (newHeight > maxHeight)       newHeight = maxHeight;
		m_paneHeight = newHeight;
		LayoutSplitter();
		return;
	}
	CDarkDialog::OnMouseMove(nFlags, point);
}

void CLayoutDialog::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_draggingCol)
	{
		m_draggingCol = false;
		ReleaseCapture();
		return;
	}
	if (m_dragging)
	{
		m_dragging = false;
		ReleaseCapture();
		return;
	}
	CDarkDialog::OnLButtonUp(nFlags, point);
}

void CLayoutDialog::OnBnClickedFindButton()
{
	if (IsCommandMode())
	{
		CmdFind(false);
		return;
	}
	//FindString(m_ResourceTree.GetNextItem(m_ResourceTree.GetRootItem(), TVGN_NEXT));
	bool startsearch=true;
	ExpandAllViews();
	FindResource(m_ResourceTree.GetRootItem(), false, NULL, &startsearch);
}

void CLayoutDialog::OnBnClickedLoadChangesButton()
{
	if (m_ResourceTree.GetSelectedItem()==NULL)
		return;
	CString filename;
	CString sID=m_ResourceTree.GetItemText(m_ResourceTree.GetSelectedItem());
	CString temp;
	DWORD j;
	int k;
	int sel_id;
	bool found=false;
	sel_id=_ttoi(sID.GetBuffer());
	LPBYTE lpBuf;
	UINT nCount;
	
	switch (m_ResourceType)
	{
	case 0: //view
		filename.Format(TEXT("view%d.rsrc"), sel_id);
		for (j = 0; j < ViewList->size(); j++)
		{
			if (ViewList->at(j)->id==sel_id)
			{
				nCount=4;
				for (k = 0; (DWORD)k < (DWORD)ViewList->at(j)->num_elems; k++)
				{
					nCount+=ViewList->at(j)->ViewElemList[k]->element->size+4;
				}
				lpBuf=(LPBYTE)ViewList->at(j)->ViewElemList[0]->element-4;
				found=true;
				break;
			}
		}
		break;
	case 1: //menu
		filename.Format(TEXT("menu%d.rsrc"), sel_id);
		for (j = 0; j < MenuList->size(); j++)
		{
			if (MenuList->at(j)->id==sel_id)
			{
				nCount=4;
				for (k = 0; (DWORD)k < (DWORD)MenuList->at(j)->num_elems; k++)
				{
					nCount+=MenuList->at(j)->MenuElemList[k]->element->size+4;
				}
				lpBuf=(LPBYTE)MenuList->at(j)->MenuElemList[0]->element-4;
				found=true;
				break;
			}
		}
		break;
	case 2: //type
		filename.Format(TEXT("type%d.rsrc"), sel_id);
		if (theApp.m_OldResourceSaving)
			found=ResourceBytes(2, sel_id, &lpBuf, &nCount);        // the first font type of the ID, as in 1.3
		else
		{
			TypeRecord *pType=SelectedType();
			if (pType!=NULL)
			{
				nCount=sizeof(*pType->element);
				lpBuf=(LPBYTE)pType->element;
				found=true;
			}
		}
		break;
	case 3: //commands: the function as it is stored
	case 4:
		filename.Format(TEXT("tcmd%d.rsrc"), sel_id);
		if (theApp.m_OldResourceSaving)
			found=OldTcmdBytes(sel_id, &lpBuf, &nCount);
		else
			found=CmdRecord(sel_id, &lpBuf, &nCount);
		break;
	}
		
	if (found==false)
	{
		MessageBox(_T("This is not a valid resource for saving!"));
		return;
	}
	CFile file;
	
	CFileDialog dlgb(TRUE, TEXT("rsrc"), 0, OFN_HIDEREADONLY, TEXT("iPod Resource Files (*.rsrc)|*.rsrc||"), this);
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
	if (filelen!=nCount)
	{
		MessageBox(_T("The resource file you chose doesn't fit this resource!"));
		return;
	}
	else
	{
		if (IsCommandMode())
		{
			if (theApp.m_OldResourceSaving)
			{
				// the old record of a command (see OldTcmdBytes) does not say what the function is: nothing of it can be put into the firmware
				file.Close();
				MessageBox(TEXT("This is an old command resource. It fits, but it is not applied: the firmware is not changed."));
				return;
			}
			if (!CmdReadRecord(file, sel_id, lpBuf, nCount))
				return;
		}
		else
		{
			file.Read(lpBuf, nCount);
			file.Close();
		}
		MessageBox(TEXT("Loaded resource file successfully!"));
		
		LRESULT res;
		NMTREEVIEW nm;
		nm.itemNew.hItem=m_ResourceTree.GetSelectedItem();
		OnTvnSelchangedResourceTree((NMHDR *)&nm, &res);
		m_HexWnd.Invalidate();
	}	
}

// The bytes of one resource, as they are in the firmware (what Save Resource writes and Load Resource reads back)
bool CLayoutDialog::ResourceBytes(int mode, int id, LPBYTE *pBuf, UINT *pLen)
{
	DWORD j;
	int k;
	switch (mode)
	{
	case 0: //view
		for (j = 0; j < ViewList->size(); j++)
		{
			if (ViewList->at(j)->id==id && ViewList->at(j)->num_elems>0)
			{
				UINT n=4;
				for (k = 0; k < ViewList->at(j)->num_elems; k++)
					n+=ViewList->at(j)->ViewElemList[k]->element->size+4;
				*pBuf=(LPBYTE)ViewList->at(j)->ViewElemList[0]->element-4;
				*pLen=n;
				return true;
			}
		}
		return false;
	case 1: //menu
		for (j = 0; j < MenuList->size(); j++)
		{
			if (MenuList->at(j)->id==id && MenuList->at(j)->num_elems>0)
			{
				UINT n=4;
				for (k = 0; k < MenuList->at(j)->num_elems; k++)
					n+=MenuList->at(j)->MenuElemList[k]->element->size+4;
				*pBuf=(LPBYTE)MenuList->at(j)->MenuElemList[0]->element-4;
				*pLen=n;
				return true;
			}
		}
		return false;
	case 2: //type: the first one with this ID
		for (j = 0; j < TypeList->size(); j++)
		{
			if (TypeList->at(j)->id==id)
			{
				*pBuf=(LPBYTE)TypeList->at(j)->element;
				*pLen=sizeof(*TypeList->at(j)->element);
				return true;
			}
		}
		return false;
	case 3: //commands
	case 4:
		if (theApp.m_OldResourceSaving)
			return OldTcmdBytes(id, pBuf, pLen);
		return CmdRecord(id, pBuf, pLen);
	}
	return false;
}

// What iPodWizard 1.3 saved as a command resource: the record of the command it had read, as it was in memory (the structure
// resource_type_tcmd_elem, pointer included), not the bytes of the function in the firmware.
bool CLayoutDialog::OldTcmdBytes(int id, LPBYTE *pBuf, UINT *pLen)
{
	if (TCMDList==NULL || TCMDList->size()==0)
		return false;
	for (DWORD j = 0; j < TCMDList->at(0)->element.elements.size(); j++)
	{
		if (TCMDList->at(0)->element.elements.at(j)->id==id)
		{
			*pBuf=(LPBYTE)TCMDList->at(0)->element.elements.at(j);
			*pLen=sizeof(*TCMDList->at(0)->element.elements.at(j));
			return true;
		}
	}
	return false;
}

// The resource that is selected in the tree of the current mode. The ID is the number at the start of the entry, the name behind it
// is not part of it; a font type is found by its place in the list, since a few exist in several copies.
bool CLayoutDialog::SelectedResource(int *pId, LPBYTE *pBuf, UINT *pLen)
{
	HTREEITEM h=m_ResourceTree.GetSelectedItem();
	if (h==NULL)
		return false;
	if (m_ResourceType==2)
	{
		TypeRecord *pType=TypeOfItem(h);
		if (pType==NULL)
			return false;
		*pId=pType->id;
		if (theApp.m_OldResourceSaving)                   // 1.3 saved the first font type of the ID, whichever copy is selected
			return ResourceBytes(2, *pId, pBuf, pLen);
		*pBuf=(LPBYTE)pType->element;
		*pLen=sizeof(*pType->element);
		return true;
	}
	if (IsCommandMode())
		*pId=(int)m_ResourceTree.GetItemData(h);
	else
		*pId=_ttoi(m_ResourceTree.GetItemText(h));
	return ResourceBytes(m_ResourceType, *pId, pBuf, pLen);
}

CString CLayoutDialog::ResourceFileName(int mode, int id)
{
	static const TCHAR *prefix[5] = { _T("view"), _T("menu"), _T("type"), _T("tcmd"), _T("tcmd") };
	CString s;
	s.Format(_T("%s%d.rsrc"), prefix[mode >= 0 && mode <= 4 ? mode : 0], id);
	return s;
}

void CLayoutDialog::OnBnClickedSaveResourceButton()
{
	int id;
	LPBYTE lpBuf;
	UINT nCount;
	if (!SelectedResource(&id, &lpBuf, &nCount))
	{
		MessageBox(_T("Make sure you chose a resource!"));
		return;
	}

	CFileDialog dlg(FALSE, TEXT("rsrc"), ResourceFileName(m_ResourceType, id), OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, TEXT("iPod Resource Files (*.rsrc)|*.rsrc||"), this);
	MO_LOAD_RESOURCES_PATH(dlg)

	if (dlg.DoModal()!=IDOK)
		return;
	MO_SAVE_RESOURCES_PATH(dlg)

	SaveRAWResource(dlg.GetPathName(), lpBuf, nCount);
	MessageBox(_T("Saved resource successfully!"));
}

void CLayoutDialog::OnEnChangeFindEdit()
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

void CLayoutDialog::OnEnChangeChangeEdit()
{
	UpdateData(TRUE);

	BOOL bEnable = !m_ChangeString.IsEmpty() && m_pFirmware != NULL;
	GetDlgItem(IDC_CHANGETEXT_BUTTON)->EnableWindow(bEnable);
}


// Ctrl+A in the boxes Find, Change Value and Hex GoTo selects all of the text, so what is typed next replaces it
BOOL CLayoutDialog::PreTranslateMessage(MSG *pMsg)
{
	if (m_LoadTip.GetSafeHwnd() && pMsg->message >= WM_MOUSEFIRST && pMsg->message <= WM_MOUSELAST)
	{
		// the pointer is over one of the two Load buttons while they are off because of "Use old resource saving": the tip gets the
		// message as if the button had got it
		POINT pt = { (short)LOWORD(pMsg->lParam), (short)HIWORD(pMsg->lParam) };
		::MapWindowPoints(pMsg->hwnd, NULL, &pt, 1);                    // screen
		CWnd *pOver = NULL;
		if (theApp.m_OldResourceSaving && IsCommandMode())
		{
			static const UINT ids[2] = { IDC_LOADCHANGES_BUTTON, IDC_LOADALLRESOURCES_BUTTON };
			for (int i = 0; i < 2 && pOver == NULL; i++)
			{
				CWnd *pBtn = GetDlgItem(ids[i]);
				CRect rc;
				if (pBtn != NULL && pBtn->IsWindowVisible() && !pBtn->IsWindowEnabled())
				{
					pBtn->GetWindowRect(&rc);
					if (rc.PtInRect(pt))
						pOver = pBtn;
				}
			}
		}
		m_LoadTip.Activate(pOver != NULL);
		MSG msg = *pMsg;
		if (pOver != NULL)
		{
			POINT ptBtn = pt;
			::ScreenToClient(pOver->m_hWnd, &ptBtn);
			msg.hwnd = pOver->m_hWnd;
			msg.lParam = MAKELPARAM(ptBtn.x, ptBtn.y);
		}
		m_LoadTip.RelayEvent(&msg);
	}
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

void CLayoutDialog::OnEnChangeAddrEdit()
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

void CLayoutDialog::OnOK()
{
	// TODO: Add your specialized code here and/or call the base class

	// CDarkDialog::OnOK();
}

void CLayoutDialog::OnBnClickedFindnextButton()
{
	if (IsCommandMode())
	{
		CmdFind(true);
		return;
	}
	//FindString(m_ResourceTree.GetSelectedItem());
	bool startsearch=false;
	ExpandAllViews();
	FindResource(m_ResourceTree.GetSelectedItem(), true, m_ResourceTree.GetSelectedItem(), &startsearch);
}

void CLayoutDialog::OnBnClickedChangeTextButton()
{
	if (IsCommandMode())
	{
		CmdChangeValue();
		return;
	}
	if (MenuChangeCommand())
		return;
	if (m_ValSize==0 || m_pFirmware==NULL || (ULONGLONG)m_ValOffset + m_ValSize > m_pFirmware->GetFirmwareSize())
	{
		MessageBox(_T("Not allowed to change that value. Change it manually if you want."));
		return;
	}

	// what is written: as long as the value is (a shorter entry changes the first bytes of a hex value, and the rest of a string
	// becomes zeros; a number is stored low byte first)
	std::vector<BYTE> out(m_pFirmware->GetFirmwareBuffer() + m_ValOffset, m_pFirmware->GetFirmwareBuffer() + m_ValOffset + m_ValSize);
	if (IsDlgButtonChecked(IDC_RADIO_STRINGVAL)==1)
	{
		//String change
		const int cchMultiByte = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)m_ChangeString, -1, NULL, 0, NULL, NULL);
		if (cchMultiByte <= 0)
			return;
		std::vector<char> utf8(cchMultiByte);
		if (WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)m_ChangeString, -1, &utf8[0], cchMultiByte, NULL, NULL) <= 0)
			return;
		int lenm = cchMultiByte - 1; //remove 0
		if ((DWORD)lenm > m_ValSize)
		{
			MessageBox(_T("The value you typed is too long!"));
			return;
		}
		std::fill(out.begin(), out.end(), (BYTE)0);
		for (int j = 0; j < lenm; j++)
			out[j] = (BYTE)utf8[j];
	}
	else if (IsDlgButtonChecked(IDC_RADIO_HEXVAL)==1)
	{
		//Hex value string (the digits may be spaced out in pairs: "6B 05")
		CString hexText = m_ChangeString;
		hexText.Remove(' ');
		int lenm = hexText.GetLength();
		if (lenm == 0 || (lenm % 2) != 0)
		{
			MessageBox(_T("Type the bytes as pairs of hex digits, for example 6B 05."));
			return;
		}
		for (int j = 0; j < lenm; j++)
			if (!_istxdigit(hexText[j]))
			{
				MessageBox(_T("That is not a hex value."));
				return;
			}
		if ((DWORD)(lenm / 2) > m_ValSize)
		{
			MessageBox(_T("The value you typed is too long!"));
			return;
		}
		for (int k = 0, j = 0; j < lenm; j += 2, k++)
		{
			unsigned int b = 0;
			swscanf(hexText.Mid(j, 2), TEXT("%X"), &b);
			out[k] = (BYTE)b;
		}
	}
	else
	{
		//Decimal value: stored low byte first, as many bytes as the value has (1 to 4)
		__int64 v = 0;
		if (m_ValSize > 4 || _stscanf(m_ChangeString, _T("%I64d"), &v) != 1)
		{
			MessageBox(m_ValSize > 4 ? _T("This value is too long to be typed as a number. Use Hex or String.") : _T("That is not a number."));
			return;
		}
		__int64 lo = -((__int64)1 << (8 * m_ValSize - 1)), hi = ((__int64)1 << (8 * m_ValSize)) - 1;
		if (v < lo || v > hi)
		{
			MessageBox(_T("That number does not fit in this value."));
			return;
		}
		for (DWORD k = 0; k < m_ValSize; k++)
			out[k] = (BYTE)((v >> (8 * k)) & 0xFF);
	}

	m_pFirmware->SetData(m_ValOffset, &out[0], m_ValSize);
	m_HexWnd.SetOffset(m_ValOffset);
	m_HexWnd.Invalidate();

	HTREEITEM it = m_ResourceTree.GetSelectedItem();
	LRESULT res;
	NMTREEVIEW nm;
	nm.itemNew.hItem=it;
	OnTvnSelchangedResourceTree((NMHDR *)&nm, &res);

	UpdateData(FALSE);
	//
	MessageBox(TEXT("Value changed successfully."), TEXT("Value Changer"), MB_OK);
}

bool CLayoutDialog::FindResource(HTREEITEM hParentItem, bool next, HTREEITEM hMarkItem, bool *startsearch)
{
	HTREEITEM hCurrItem;
	hCurrItem=hParentItem;
	if (hCurrItem==NULL)
		return false;

	UpdateData(TRUE);

	if (m_FindString.IsEmpty())
		return false;

	m_FindString.MakeLower();

	// search

	CWaitCursor wait;

	if (next)
	{
		HTREEITEM hCurrItem2;
		HTREEITEM hRootItem;
		hRootItem=m_ResourceTree.GetRootItem();
		hCurrItem2=m_ResourceTree.GetNextItem(hCurrItem, TVGN_PARENT);
		while (hCurrItem2!=NULL)
		{
			hCurrItem=hCurrItem2;
			hCurrItem2=m_ResourceTree.GetNextItem(hCurrItem, TVGN_PARENT);
		}
	}
	
	bool found=false;
	while (hCurrItem!=NULL && !found)
	{
		if (*startsearch==true)
		{
			if (m_ResourceTree.GetItemText(hCurrItem).Find(m_FindString)!=-1)
			{
				m_ResourceTree.SelectItem(hCurrItem);
				RECT rct;
				m_ResourceTree.GetItemRect(hCurrItem, &rct, FALSE);
				m_ResourceTree.SetCaretPos(CPoint(rct.left, rct.top));
				found=true;
				break;
			}
		}
		if (hCurrItem==hMarkItem)
			*startsearch=true;
		found=FindResource(m_ResourceTree.GetNextItem(hCurrItem, TVGN_CHILD), false, hMarkItem, startsearch);
		hCurrItem=m_ResourceTree.GetNextItem(hCurrItem, TVGN_NEXT);
	}
	return found;
}

void CLayoutDialog::FindString(HTREEITEM hStartItem)
{
	UpdateData(TRUE);

	if (m_FindString.IsEmpty())
		return;

	m_FindString.MakeLower();

	// search

	CWaitCursor wait;

	HTREEITEM hCurrItem;
	hCurrItem=m_ResourceTree.GetNextItem(m_ResourceTree.GetRootItem(), TVGN_NEXT);
	DWORD k;
	k=0;
	int sel_id;
	sel_id=_ttoi(m_FindString);
	bool found;
	bool start=false;
	while (hCurrItem!=NULL)
	{
		if (start)
		{
			found=false;
			switch (m_ResourceType)
			{
			case 0:
				if (ViewList->at(k)->id==sel_id)
				{
					m_ResourceTree.SelectItem(m_ResourceTree.GetPrevSiblingItem(hCurrItem));
					found=true;
				}
				break;
			case 1:
				if (MenuList->at(k)->id==sel_id)
				{
					m_ResourceTree.SelectItem(m_ResourceTree.GetPrevSiblingItem(hCurrItem));
					found=true;
				}
				break;
			case 2:
				if (TypeList->at(k)->id==sel_id)
				{
					m_ResourceTree.SelectItem(m_ResourceTree.GetPrevSiblingItem(hCurrItem));
					found=true;
				}
				break;
			}
			if (found)
				break;
		}
		if (hCurrItem==hStartItem)
			start=true;
		k++;
		hCurrItem=m_ResourceTree.GetNextItem(hCurrItem, TVGN_NEXT);
	}
}

void CLayoutDialog::Create_ViewTree(ViewRecord *view_rec, HTREEITEM hItem, vector<short> *id_list)
{
	DWORD k;
	CString sID;
	HTREEITEM hParentItem;
	ViewRecord *new_rec;

	if (view_rec==NULL)
		return;

	(*id_list).push_back(view_rec->id);

	for (k = 0; k < (DWORD)view_rec->num_elems; k++)
	{
		if (IsNotOnList(id_list, view_rec->ViewElemList[k]->element->id))
		{
			sID=CLayoutNames::Label(view_rec->ViewElemList[k]->element->id, m_Names.View(view_rec->ViewElemList[k]->element->id));
			new_rec=view_rec->ViewElemList[k]->next;
			// what is inside a layout is added when the entry is opened (ExpandViewItem)
			hParentItem=InsertLazy(sID, hItem, 0, new_rec!=NULL && new_rec->num_elems>0);
		}
	}
}

// An entry that shows [+] without having the entries inside it: they are added when it is opened (a tree with many of them
// takes a long time to empty, so the entries that are not wanted are not made)
HTREEITEM CLayoutDialog::InsertLazy(const CString &text, HTREEITEM parent, DWORD_PTR data, bool hasChildren)
{
	TVINSERTSTRUCT tvi;
	memset(&tvi, 0, sizeof(tvi));
	tvi.hParent = parent;
	tvi.hInsertAfter = TVI_LAST;
	tvi.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN;
	tvi.item.pszText = (LPTSTR)(LPCTSTR)text;
	tvi.item.lParam = data;
	tvi.item.cChildren = hasChildren ? 1 : 0;
	return m_ResourceTree.InsertItem(&tvi);
}

// With "Use old resource saving" a command resource is the old record of the command, which says nothing of the function: it can
// be saved, but nothing of it can be loaded into the firmware. So the two buttons that load are off in the Commands view then.
void CLayoutDialog::UpdateLoadButtons()
{
	if (!m_bModesReady || GetDlgItem(IDC_LOADCHANGES_BUTTON) == NULL)
		return;
	BOOL bOn = !(theApp.m_OldResourceSaving && IsCommandMode());
	GetDlgItem(IDC_LOADCHANGES_BUTTON)->EnableWindow(bOn);
	GetDlgItem(IDC_LOADALLRESOURCES_BUTTON)->EnableWindow(bOn);
}

void CLayoutDialog::ShowCommandTabs()
{
	UpdateLoadButtons();
	bool cmd = IsCommandMode();
	if (m_CmdTabs.m_hWnd != NULL)
	{
		if (cmd)
			m_CmdTabs.SetCurSel(m_ResourceType == 4 ? 1 : 0);
		m_CmdTabs.ShowWindow(cmd ? SW_SHOW : SW_HIDE);
	}
	LayoutSplitter();
}

void CLayoutDialog::OnTcnSelchangeCmdTabs(NMHDR *pNMHDR, LRESULT *pResult)
{
	*pResult = 0;
	SetMode(m_CmdTabs.GetCurSel() == 1 ? 4 : 3);
}

// The page of the editor: the commands are one page that shows them as a table or as instructions, as it did last
void CLayoutDialog::SetPage(int page)
{
	if (page == 3)
		page = IsCommandMode() ? m_ResourceType : ((m_LastCmdMode == 4) ? 4 : 3);
	SetMode(page);
	UpdateLoadButtons();                   // (the option can have been changed on the Preferences page since)
}

// A layout that holds other layouts shows them when it is opened (11 thousand entries in all, in the biggest firmware)
void CLayoutDialog::ExpandViewItem(HTREEITEM h)
{
	if (m_ResourceTree.GetChildItem(h) != NULL)
		return;
	TVITEM probe;
	memset(&probe, 0, sizeof(probe));
	probe.mask = TVIF_HANDLE | TVIF_CHILDREN;
	probe.hItem = h;
	if (!m_ResourceTree.GetItem(&probe) || probe.cChildren == 0)
		return;

	// the way down to this entry, the layout it stands for, and the layouts above it (a layout is not shown inside itself)
	std::vector<HTREEITEM> chain;
	for (HTREEITEM p = h; p != NULL; p = m_ResourceTree.GetParentItem(p))
		chain.insert(chain.begin(), p);
	if (chain.size() < 2)
		return;
	vector<short> above;
	for (size_t i = 0; i + 1 < chain.size(); i++)
	{
		short id = (short)_ttoi(m_ResourceTree.GetItemText(chain[i]));
		if (i == 0)
			above.push_back(id);
		else
		{
			vector<short> n;
			n.push_back(0);
			n.insert(n.end(), above.begin(), above.end());
			n.push_back(id);
			above.swap(n);
		}
	}
	vector<short> list;
	list.push_back(0);
	list.insert(list.end(), above.begin(), above.end());
	int id = _ttoi(m_ResourceTree.GetItemText(h));
	ViewRecord *rec = NULL;
	for (size_t j = 0; j < ViewList->size(); j++)
		if (ViewList->at(j)->id == id)
			rec = ViewList->at(j);
	Create_ViewTree(rec, h, &list);
	if (m_ResourceTree.GetChildItem(h) == NULL)        // nothing to show after all: no [+]
	{
		probe.cChildren = 0;
		m_ResourceTree.SetItem(&probe);
	}
}

// What a search of the tree has to look through
void CLayoutDialog::ExpandAllViews()
{
	if (m_ResourceType != 0)
		return;
	CWaitCursor wait;
	m_ResourceTree.SetRedraw(FALSE);
	std::vector<HTREEITEM> todo;
	for (HTREEITEM r = m_ResourceTree.GetRootItem(); r != NULL; r = m_ResourceTree.GetNextSiblingItem(r))
		todo.push_back(r);
	while (!todo.empty())
	{
		HTREEITEM h = todo.back();
		todo.pop_back();
		ExpandViewItem(h);
		for (HTREEITEM c = m_ResourceTree.GetChildItem(h); c != NULL; c = m_ResourceTree.GetNextSiblingItem(c))
			todo.push_back(c);
	}
	m_ResourceTree.SetRedraw(TRUE);
}

// The tree that belongs to a mode: layouts, menus, font types and commands (both ways of showing them) each have one, so a
// tree is made once and only shown again when the mode is chosen again (emptying and filling a big tree took a long time)
int CLayoutDialog::TreeIndex(int mode)
{
	return (mode == 4) ? 3 : mode;
}

void CLayoutDialog::ActivateTree(int idx)
{
	if (idx == m_curTree || idx < 0 || idx > 3 || m_hTree[idx] == NULL)
		return;
	::ShowWindow(m_hTree[m_curTree], SW_HIDE);
	m_ResourceTree.UnsubclassWindow();
	m_ResourceTree.SubclassWindow(m_hTree[idx]);
	m_curTree = idx;
}

void CLayoutDialog::OnCbnSelchangeResourceTypeidxCombo()
{
	if (m_bListsBuilt)
		SaveModeState(m_ResourceType);             // the mode that is left remembers where it was
	const int newMode = m_ResourceTypeIdx.GetCurSel();
	const int treeIdx = TreeIndex(newMode);
	const bool cached = m_treeBuilt[treeIdx];      // the tree was made before: it is only shown again
	s_bTreeBuilding = true;
	ActivateTree(treeIdx);
	m_ResourceTree.SetRedraw(FALSE);
	m_ResourceType=newMode;
	m_ResourceList.DeleteAllItems();
	if (m_UsedCombo.m_hWnd)
		m_UsedCombo.ShowWindow(SW_HIDE);
	m_UsedShown=false;
	m_CmdLines.clear();
	m_CmdFunction=0;

	HTREEITEM hRootItem;
	DWORD j;
	int k;
	CString sID;

	switch(m_ResourceType)
	{
		case 0: //view
			SetupViewColumns();
			if (!cached)
			{
				m_ResourceTree.DeleteAllItems();
				for (j = 0; j < ViewList->size(); j++)
				{
					sID=CLayoutNames::Label(ViewList->at(j)->id, m_Names.View(ViewList->at(j)->id));
					hRootItem=m_ResourceTree.InsertItem(sID, TVI_ROOT, TVI_LAST);
					vector<short> id_list;
					Create_ViewTree(ViewList->at(j), hRootItem, &id_list);
				}
			}
			break;
		case 1: //menu
			SetupMenuColumns();
			if (!cached)
			{
				m_ResourceTree.DeleteAllItems();
				for (j = 0; j < MenuList->size(); j++)
				{
					sID=CLayoutNames::Label(MenuList->at(j)->id, m_Names.Menu(MenuList->at(j)->id));
					hRootItem=m_ResourceTree.InsertItem(sID, TVI_ROOT, TVI_LAST);

					for (k=0;k<MenuList->at(j)->num_elems;k++)
					{
						if (MenuList->at(j)->MenuElemList[k]->element->submenuID != 0)
						{
							sID=CLayoutNames::Label(MenuList->at(j)->MenuElemList[k]->element->submenuID, m_Names.Menu(MenuList->at(j)->MenuElemList[k]->element->submenuID));
							m_ResourceTree.InsertItem(sID, hRootItem, TVI_LAST);
						}
					}
				}
			}
			break;
		case 2: //type
			SetupTypeColumns();
			if (!cached)
			{
				m_ResourceTree.DeleteAllItems();
				std::map<int, int> copies;
				for (j = 0; j < TypeList->size(); j++)
				{
					sID=CLayoutNames::Label(TypeList->at(j)->id, m_Names.Type(TypeList->at(j)->id));
					int copy=++copies[TypeList->at(j)->id];
					if (copy>1)
						sID.AppendFormat(_T(" (copy %d)"), copy);
					hRootItem=m_ResourceTree.InsertItem(sID, TVI_ROOT, TVI_LAST);
					m_ResourceTree.SetItemData(hRootItem, j+1);
				}
			}
			break;
		case 3: //commands (compiled script), shown as source
		case 4: //commands, shown as assembly
			CmdSetupColumns();
			if (!cached)
				CmdFillTree();
			break;
	}
	m_treeBuilt[treeIdx]=true;
	s_bTreeBuilding = false;
	GetDlgItem(IDC_SAVECHANGES_BUTTON)->EnableWindow(TRUE);

	m_bListsBuilt = true;
	m_RequestedMode = m_ResourceType;
	UpdateModeText();
	if (!m_bNoRestore)
		RestoreModeState(m_ResourceType);         // and finds it again when it is shown again
	// The selection changes made while the tree was built were not acted on, and a tree that is shown again keeps its
	// selection without telling: the list is filled once, for what is selected now
	if (m_ResourceList.GetItemCount() == 0 || m_ResourceTree.GetSelectedItem() == NULL)
	{
		HTREEITEM hSel = m_ResourceTree.GetSelectedItem();
		if (hSel == NULL)
			hSel = m_ResourceTree.GetRootItem();
		if (hSel != NULL)
		{
			if (m_ResourceTree.GetSelectedItem() != hSel)
				m_ResourceTree.SelectItem(hSel);
			else
			{
				LRESULT res;
				NMTREEVIEW nm;
				memset(&nm, 0, sizeof(nm));
				nm.itemNew.hItem = hSel;
				OnTvnSelchangedResourceTree((NMHDR *)&nm, &res);
			}
		}
	}
	m_ResourceTree.SetRedraw(TRUE);
	m_ResourceTree.ShowWindow(SW_SHOW);
	m_ResourceTree.Invalidate();
	ShowCommandTabs();
	// the tab of the editor that belongs to the mode follows (it finds itself right when the mode came from the tab)
	HWND hEditor = ::GetParent(::GetParent(m_hWnd));
	if (hEditor != NULL)
		::SendMessage(hEditor, WM_APP, (WPARAM)22, (LPARAM)m_ResourceType);
}

// OnLvnItemDblClickedResourceList and OnLvnItemClickedResourceList are in LayoutInput.cpp

void CLayoutDialog::OnLvnItemchangedResourceList(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	//LPNMITEMACTIVATE pNMLV = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	CString sID=m_ResourceTree.GetItemText(m_ResourceTree.GetSelectedItem());
	if (pNMLV->uOldState!=0)
		return;


	
	UpdateData(FALSE);

	*pResult = 0;
}

void CLayoutDialog::OnTvnSelchangedResourceTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
	if (s_bTreeBuilding)
		return;                                   // the tree is being rebuilt: what is selected on the way is of no interest
	CString sID=m_ResourceTree.GetItemText(pNMTreeView->itemNew.hItem);
	CString temp;
	DWORD j;
	int k;
	int sel_id;
	bool found=false;
	sel_id=_ttoi(sID.GetBuffer());
	m_ResourceList.ShowWindow(SW_HIDE);
	GetDlgItem(IDC_STATIC_LOADINGLIST)->ShowWindow(SW_SHOW);
	m_ResourceList.DeleteAllItems();
	if (m_UsedCombo.m_hWnd)
		m_UsedCombo.ShowWindow(SW_HIDE);
	m_UsedShown=false;
	
	switch(m_ResourceType)
	{
	case 0: //view
		for (j = 0; j < ViewList->size(); j++)
		{
			if (ViewList->at(j)->id==sel_id)
			{
				FillViewList(ViewList->at(j));
				found=true;
				break;
			}
		}
		if (found==false) //it's not a view resource, try find the link
		{
			sID=m_ResourceTree.GetItemText(m_ResourceTree.GetParentItem(pNMTreeView->itemNew.hItem));
			int sel_id_parent=_ttoi(sID.GetBuffer());
			for (j = 0; j < ViewList->size(); j++)
			{
				if (ViewList->at(j)->id==sel_id_parent)
				{
					for (k = 0; k < ViewList->at(j)->num_elems; k++)
					{
						if (ViewList->at(j)->ViewElemList[k]->element->id==sel_id)
						{
							if (!strncmp(ViewList->at(j)->ViewElemList[k]->element->type, " rtS", 4))
							{
								//let's find that string!
								::SendMessage(::GetParent(::GetParent(m_hWnd)), WM_APP, (WPARAM)11, (LPARAM)ViewList->at(j)->ViewElemList[k]->element->id);
							}
							else if (!strncmp(ViewList->at(j)->ViewElemList[k]->element->type, "paMB", 4))
							{
								//let's find that bitmap!
								::SendMessage(::GetParent(::GetParent(m_hWnd)), WM_APP, (WPARAM)12, (LPARAM)m_pFirmware->GetPictureIndexFromID((DWORD)ViewList->at(j)->ViewElemList[k]->element->id));
							}
							else if (!strncmp(ViewList->at(j)->ViewElemList[k]->element->type, "eypT", 4))
							{
								//let's find that bitmap!
								::SendMessage(::GetParent(::GetParent(m_hWnd)), WM_APP, (WPARAM)13, (LPARAM)ViewList->at(j)->ViewElemList[k]->element->id);
							}
						}
					}
				}
			}
		}
		
		break;
	case 1: //menu
		for (j = 0; j < MenuList->size(); j++)
		{
			if (MenuList->at(j)->id==sel_id)
			{
				FillMenuList(MenuList->at(j));
				break;
			}
		}
		break;
	case 2: //type
		{
			TypeRecord *pType=TypeOfItem(pNMTreeView->itemNew.hItem);
			if (pType!=NULL)
				FillTypeList(pType);
		}
		break;
	case 3: //commands
	case 4:
		{
			DWORD_PTR fnData = pNMTreeView->itemNew.hItem ? m_ResourceTree.GetItemData(pNMTreeView->itemNew.hItem) : 0;
			CmdFillList((fnData == (DWORD_PTR)-1) ? 0 : (int)fnData);
		}
		break;
	}

	m_ResourceList.ShowWindow(SW_SHOW);
	UpdateData(FALSE);
	GetDlgItem(IDC_STATIC_LOADINGLIST)->ShowWindow(SW_HIDE);
	*pResult = 0;
}

void CLayoutDialog::OnBnClickedSaveallresourcesButton()
{
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

	CString filename;
	LPBYTE lpBuf;
	UINT nCount;
	int rs_count=0;

	// every resource of the mode, each in a file of its own that is called by kind and ID; of the copies of a font type only the first
	std::vector<int> ids;
	switch (m_ResourceType)
	{
	case 0:
		for (size_t j = 0; j < ViewList->size(); j++)
			ids.push_back(ViewList->at(j)->id);
		break;
	case 1:
		for (size_t j = 0; j < MenuList->size(); j++)
			ids.push_back(MenuList->at(j)->id);
		break;
	case 2:
		for (size_t j = 0; j < TypeList->size(); j++)
			ids.push_back(TypeList->at(j)->id);
		break;
	case 3:
	case 4:
		for (int fi = 0; fi < m_Commands.GetNumFunctions(); fi++)
			ids.push_back(m_Commands.GetFunctionId(fi));
		break;
	}
	std::set<int> done;
	if (theApp.m_OldResourceSaving && (m_ResourceType==2 || IsCommandMode()))
	{
		// as iPodWizard 1.3 did: every record that was read is written, so of the copies of a font type the last one stays, and a command
		// is the old record that was read of it
		if (m_ResourceType==2)
		{
			for (size_t n = 0; n < TypeList->size(); n++)
			{
				nCount=sizeof(*TypeList->at(n)->element);
				lpBuf=(LPBYTE)TypeList->at(n)->element;
				filename.Format(TEXT("%s\\%s"), folderPath, ResourceFileName(m_ResourceType, TypeList->at(n)->id));
				SaveRAWResource(filename, lpBuf, nCount);
				rs_count++;
			}
		}
		else if (TCMDList!=NULL && TCMDList->size()>0)
		{
			for (size_t n = 0; n < TCMDList->at(0)->element.elements.size(); n++)
			{
				nCount=sizeof(*TCMDList->at(0)->element.elements.at(n));
				lpBuf=(LPBYTE)TCMDList->at(0)->element.elements.at(n);
				filename.Format(TEXT("%s\\%s"), folderPath, ResourceFileName(m_ResourceType, TCMDList->at(0)->element.elements.at(n)->id));
				SaveRAWResource(filename, lpBuf, nCount);
				rs_count++;
			}
		}
	}
	else
	for (size_t n = 0; n < ids.size(); n++)
	{
		if (!done.insert(ids[n]).second)
			continue;
		if (!ResourceBytes(m_ResourceType, ids[n], &lpBuf, &nCount))
			continue;
		filename.Format(TEXT("%s\\%s"), folderPath, ResourceFileName(m_ResourceType, ids[n]));
		SaveRAWResource(filename, lpBuf, nCount);
		rs_count++;
	}

	CString sMsg;
	sMsg.Format(_T("Saved %d resources successfully!"), rs_count);
	MessageBox(sMsg);
}

void CLayoutDialog::SaveRAWResource(CString filename, LPBYTE lpBuf, UINT nCount)
{
	CFile file;

	if (!file.Open(filename, CFile::modeCreate | CFile::modeWrite))
	{
		MessageBox(_T("Can't create resource file for saving!"));
		return;
	}

	file.Write(lpBuf, nCount);
	file.Close();
}

void CLayoutDialog::OnBnClickedLoadallresourcesButton()
{
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
	LoadAllResources(folderPath, m_ResourceType);
}

void CLayoutDialog::LoadAllResources(LPCTSTR folderPath, DWORD res_type, BOOL silent)
{
	DWORD index;
	CString filename = folderPath;
	
	switch (res_type)
	{
	case 0: //view
		filename += "\\view??????.rsrc";
		break;
	case 1: //menu
		filename += "\\menu??????.rsrc";
		break;
	case 2: //type =font
		filename += "\\type??????.rsrc";
		break;
	case 3: //commands
	case 4:
		filename += "\\tcmd??????.rsrc";
		break;
	}

	bool found=false;
	LPBYTE lpBuf;
	UINT nCount;
	int rs_count=0;
	int rs_old=0;                                          // old command resources that fit (Old resource saving)

	WIN32_FIND_DATA findData;
	HANDLE hFind = FindFirstFile(filename, &findData);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			filename = folderPath;
			filename += "\\";
			filename += findData.cFileName;
			index = 0xFFFFFFFF;

			found=false;
			int sel_id=_wtoi(findData.cFileName + 4);
			DWORD j;
			int k;
			switch (res_type)
			{
			case 0: //view
				for (j = 0; j < ViewList->size(); j++)
				{
					if (ViewList->at(j)->id==sel_id)
					{
						nCount=4;
						for (k = 0; k < ViewList->at(j)->num_elems; k++)
						{
							nCount+=ViewList->at(j)->ViewElemList[k]->element->size+4;
						}
						lpBuf=(LPBYTE)ViewList->at(j)->ViewElemList[0]->element-4;
						found=true;
						break;
					}
				}
				break;
			case 1: //menu
				for (j = 0; j < MenuList->size(); j++)
				{
					if (MenuList->at(j)->id==sel_id)
					{
						nCount=4;
						for (k = 0; k < MenuList->at(j)->num_elems; k++)
						{
							nCount+=MenuList->at(j)->MenuElemList[k]->element->size+4;
						}
						lpBuf=(LPBYTE)MenuList->at(j)->MenuElemList[0]->element-4;
						found=true;
						break;
					}
				}
				break;
			case 2: //type
				for (j = 0; j < TypeList->size(); j++)
				{
					if (TypeList->at(j)->id==sel_id)
					{
						nCount=sizeof(*TypeList->at(j)->element);
						lpBuf=(LPBYTE)TypeList->at(j)->element;
						found=true;
						break;
					}
				}
				break;
			case 3: //commands
			case 4:
				if (theApp.m_OldResourceSaving)
					found=OldTcmdBytes(sel_id, &lpBuf, &nCount);
				else
					found=CmdRecord(sel_id, &lpBuf, &nCount);
				break;
			}

			if (found==true)
			{
				index=sel_id;
				CFile file;
	
				if (!file.Open(filename, CFile::modeRead))
				{
					MessageBox(TEXT("Unable to load a resource file!"));
					return;
				}
				
				DWORD filelen=(DWORD)file.GetLength();
				if (filelen!=nCount)
				{
					MessageBox(_T("One of the resource files you chose doesn't fit this resource!"));
					return;
				}
				else
				{
					if (res_type == 3 || res_type == 4)
					{
						if (theApp.m_OldResourceSaving)
						{
							file.Close();                  // an old command resource: it fits, nothing of it can be put into the firmware
							rs_old++;
						}
						else if (!CmdReadRecord(file, sel_id, lpBuf, nCount))
							return;
					}
					else
					{
						file.Read(lpBuf, nCount);
						file.Close();
					}
					rs_count++;
				}	
			}
		} while (FindNextFile(hFind, &findData) != 0);

		FindClose(hFind);

	}

	if (rs_old>0 && !silent)
	{
		CString sMsg;
		sMsg.Format(TEXT("%d old command resource(s) fit, but are not applied: the firmware is not changed."), rs_old);
		MessageBox(sMsg);
	}

	if (rs_count>0)
	{
		if (!silent)
		{
			CString sMsg;
			sMsg.Format(TEXT("Loaded %d resources successfully!"), rs_count);
			MessageBox(sMsg);
		}

		LRESULT res;
		NMTREEVIEW nm;
		nm.itemNew.hItem=m_ResourceTree.GetRootItem();
		OnTvnSelchangedResourceTree((NMHDR *)&nm, &res);
		m_HexWnd.Invalidate();
	}
	else if (!silent && rs_old==0)
	{
		MessageBox(_T("No resources loaded!"));
	}
}

#ifdef IPW_TESTHOOK
// wParam: list row, lParam: column | (1 << 16 for a double click instead of a click) | (2 << 16 for a click on the marked
// part of the cell that has the number in bits 20 and up) | (3 << 16: only report). The answer is what is marked in the
// hex view: (length << 24) | offset, with bit 31 set when anything is marked.
LRESULT CLayoutDialog::OnTestHook(WPARAM wParam, LPARAM lParam)
{
	int row = (int)wParam;
	int col = (int)(lParam & 0xFFFF);
	int how = (int)((lParam >> 16) & 0xF);
	int which = (int)(lParam >> 20);
	if (how == 5)
	{
		// go to the entry with the ID `row` of the mode `col`
		SelectResource(col, row);
		return 1;
	}
	if (how == 4)
	{
		// change the mode (row is the mode) and say how many milliseconds it took, plus one
		DWORD t0 = ::GetTickCount();
		SetMode(row);
		return (LRESULT)(::GetTickCount() - t0 + 1);
	}
	if (how < 3 && (row < 0 || row >= m_ResourceList.GetItemCount()))
		return 0;
	if (how < 3)
		m_ResourceList.SetItemState(row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	NMITEMACTIVATE nm;
	memset(&nm, 0, sizeof(nm));
	nm.iItem = row;
	nm.iSubItem = col;
	nm.ptAction = CPoint(20, 20);
	LRESULT res = 0;
	if (how == 1)
		OnLvnItemDblClickedResourceList((NMHDR *)&nm, &res);
	else if (how == 0)
		OnLvnItemClickedResourceList((NMHDR *)&nm, &res);
	else if (how == 2)
	{
		const std::vector<CCellLink> *pl = m_ResourceList.GetCellLinks(row, col);
		if (pl == NULL || which >= (int)pl->size())
			return 0;
		ShowLink(pl->at(which));
	}
	if (m_HexWnd.m_RangeLen == 0)
		return 1;
	return (LRESULT)(0x80000000u | ((m_HexWnd.m_RangeLen & 0x7F) << 24) | (m_HexWnd.m_RangeStart & 0xFFFFFF));
}
#endif
