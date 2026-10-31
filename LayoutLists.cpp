// LayoutLists.cpp : the lists of the Layout tab (layouts, menus, font types) with their decoded values.
//
// Each list row shows the raw numbers of the entry, as before, and each cell has a tooltip that says what the
// number means for that kind of entry (see LayoutAttrs.h). A last column sums the entry up in words.

#include "stdafx.h"
#include "iPodWizard.h"
#include "LayoutDialog.h"

static CString HexText(DWORD v)
{
	CString s;
	s.Format(_T("0x%X"), v);
	return s;
}

static CString Hex8(DWORD v)
{
	CString s;
	s.Format(_T("0x%08X"), v);
	return s;
}

// ---- columns

void CLayoutDialog::SetupViewColumns()
{
	while (m_ResourceList.DeleteColumn(0) == TRUE);
	static const TCHAR *names[22] = { _T("Block size"), _T("Class code"), _T("Resource type"), _T("ID"), _T("unk1"), _T("z1"), _T("z2"),
		_T("z3"), _T("z4"), _T("left"), _T("top"), _T("right"), _T("bottom"), _T("Color1"), _T("Color2"), _T("Extra"),
		_T("Font type"), _T("Summary"), _T("left mode"), _T("top mode"), _T("right mode"), _T("bottom mode") };
	static const int widths[22] = { 62, 72, 94, 52, 44, 58, 52, 56, 56, 40, 40, 44, 52, 78, 78, 44, 66, 520, 92, 92, 92, 92 };
	for (int i = 0; i < 22; i++)
		m_ResourceList.InsertColumn(i, names[i], LVCFMT_LEFT, widths[i]);
	// the four columns of the edge descriptors (reference / mode / flag) come right after the positions they belong to; the
	// numbers of the columns stay what they are, only the order in which they are shown changes
	int order[22];
	int n = 0;
	for (int i = 0; i <= 12; i++)
		order[n++] = i;
	for (int i = 18; i <= 21; i++)
		order[n++] = i;
	for (int i = 13; i <= 17; i++)
		order[n++] = i;
	m_ResourceList.SendMessage(LVM_SETCOLUMNORDERARRAY, 22, (LPARAM)order);
}

// The list of menu entries has more columns than an entry has fields: each of the two commands takes two columns, the
// number of the command (its ID) and its name.
//   list column:  0 size, 1 z1, 2 highlight cmd ID, 3 highlight cmd, 4 select cmd ID, 5 select cmd, 6 submenuID, 7 z3,
//                 8 textID, 9 z4, 10 z5, 11 z6, 12 textID2, 13 z7, 14 typeID, 15 Summary
//   field (the numbering CLayoutAttrs::MenuField, MenuTip and the links use):
static const int kMenuLogical[16] = { 0, 1, 2, 2, 3, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 };

int CLayoutDialog::MenuLogicalCol(int listCol)
{
	return listCol >= 0 && listCol < 16 ? kMenuLogical[listCol] : listCol;
}

void CLayoutDialog::SetupMenuColumns()
{
	while (m_ResourceList.DeleteColumn(0) == TRUE);
	static const TCHAR *names[17] = { _T("Block size"), _T("z1"), _T("Highlight ID"), _T("Highlight command"), _T("Select ID"),
		_T("Select command"), _T("submenuID"), _T("z3"), _T("textID"), _T("z4"), _T("z5"), _T("z6"), _T("textID2"), _T("z7"),
		_T("typeID (Font used)"), _T("Summary"), _T("Used by") };
	static const int widths[17] = { 62, 44, 76, 150, 64, 150, 66, 40, 52, 44, 66, 66, 56, 40, 120, 520, 300 };
	for (int i = 0; i < 17; i++)
		m_ResourceList.InsertColumn(i, names[i], LVCFMT_LEFT, widths[i]);
	// "Used by" (the layouts that show the menu) is shown in front of the long summary; the numbers of the columns stay what they are
	int order[17];
	for (int i = 0; i < 15; i++)
		order[i] = i;
	order[15] = 16;
	order[16] = 15;
	m_ResourceList.SendMessage(LVM_SETCOLUMNORDERARRAY, 17, (LPARAM)order);
}

void CLayoutDialog::SetupTypeColumns()
{
	while (m_ResourceList.DeleteColumn(0) == TRUE);
	static const TCHAR *names[8] = { _T("fontID (text string)"), _T("z1"), _T("Font Size"), _T("RGB Color"), _T("Text Style"), _T("Text Align"),
		_T("z2"), _T("Used by") };
	static const int widths[8] = { 110, 44, 66, 84, 72, 72, 44, 520 };
	for (int i = 0; i < 8; i++)
		m_ResourceList.InsertColumn(i, names[i], LVCFMT_LEFT, widths[i]);
}

// ---- rows

void CLayoutDialog::FillViewList(ViewRecord *pRec)
{
	CString temp;
	for (int k = 0; k < pRec->num_elems; k++)
	{
		const resource_type_view_elem *e = pRec->ViewElemList[k]->element;
		CLayoutAttrs::Extra x;
		CLayoutAttrs::ExtraOf(e, &x);
		int ft = m_Attrs.ViewFontType(e);

		temp.Format(TEXT("%d"), e->size);
		m_ResourceList.InsertItem(k, temp);
		m_ResourceList.SetItemText(k, 1, HexText(e->z0));
		m_ResourceList.SetItemText(k, 2, CLayoutAttrs::RawKind(e));           // as stored; its tooltip says what the kind is
		temp.Format(TEXT("%d"), e->id);
		m_ResourceList.SetItemText(k, 3, temp);
		temp.Format(TEXT("%d"), e->unk1);
		m_ResourceList.SetItemText(k, 4, temp);
		temp.Format(TEXT("%d"), e->z1);
		m_ResourceList.SetItemText(k, 5, temp);
		temp.Format(TEXT("%d"), e->z2);
		m_ResourceList.SetItemText(k, 6, temp);
		m_ResourceList.SetItemText(k, 7, HexText(e->z3));
		m_ResourceList.SetItemText(k, 8, HexText(e->z4));
		temp.Format(TEXT("%d"), e->left.val);
		m_ResourceList.SetItemText(k, 9, temp);
		temp.Format(TEXT("%d"), e->top.val);
		m_ResourceList.SetItemText(k, 10, temp);
		temp.Format(TEXT("%d"), e->right.val);
		m_ResourceList.SetItemText(k, 11, temp);
		temp.Format(TEXT("%d"), e->bottom.val);
		m_ResourceList.SetItemText(k, 12, temp);
		if (x.gradient)
		{
			m_ResourceList.SetItemText(k, 13, Hex8(x.p[0]));
			m_ResourceList.SetItemText(k, 14, Hex8(x.p[1]));
		}
		else
		{
			m_ResourceList.SetItemText(k, 13, _T("-"));
			m_ResourceList.SetItemText(k, 14, _T("-"));
		}
		temp.Format(TEXT("%d"), x.n);
		m_ResourceList.SetItemText(k, 15, temp);
		if (ft)
		{
			temp.Format(TEXT("%d"), ft);
			m_ResourceList.SetItemText(k, 16, temp);
		}
		std::vector<CCellLink> links;
		CString summary = m_Attrs.ViewSummary(e, &links);
		m_ResourceList.SetItemText(k, 17, summary);
		m_ResourceList.SetCellLinks(k, 17, links);
		{
			// the descriptors of the edges: reference / mode in decimal, the flag in hex (0x8000 is the common one)
			const resource_view_temp *ed[4] = { &e->left, &e->top, &e->right, &e->bottom };
			for (int q = 0; q < 4; q++)
			{
				temp.Format(TEXT("%u / %u / 0x%X"), (unsigned)ed[q]->a1, (unsigned)ed[q]->a2, (unsigned)ed[q]->a3);
				m_ResourceList.SetItemText(k, 18 + q, temp);
			}
		}

		for (int col = 0; col <= 21; col++)
		{
			CString tip = m_Attrs.ViewTip(e, col);
			if (!tip.IsEmpty())
				m_ResourceList.SetItemToolTipText(k, col, tip);
		}
		m_ResourceList.SetItemToolTipText(k, 17, summary);
	}
}

void CLayoutDialog::FillMenuList(MenuRecord *pRec)
{
	CString temp;
	for (int k = 0; k < pRec->num_elems; k++)
	{
		const resource_type_menu_elem *e = pRec->MenuElemList[k]->element;
		temp.Format(TEXT("%d"), e->size);
		m_ResourceList.InsertItem(k, temp);
		m_ResourceList.SetItemText(k, 1, HexText(e->z1));

		// the two commands, each in two columns: the number of the command to run and its name (a<number> where the
		// firmware has no name for it, as in the source of the command table)
		for (int c = 0; c < 2; c++)
		{
			unsigned int ref = c == 0 ? e->z2 : e->cmd;
			int fn = CCommandTable::RefToFunction(ref);
			CString id, name;
			if (fn >= 0 && m_Commands.FindFunction(fn) != NULL)
			{
				id.Format(_T("%d"), fn);
				name = m_Commands.Script().AtomText(fn);
			}
			else
				id = ref == 0 ? CString(_T("-")) : HexText(ref);
			m_ResourceList.SetItemText(k, 2 + 2 * c, id);
			m_ResourceList.SetItemText(k, 3 + 2 * c, name);
		}
		temp.Format(TEXT("%d"), e->submenuID);
		m_ResourceList.SetItemText(k, 6, temp);
		m_ResourceList.SetItemText(k, 7, HexText(e->z3));
		DWORD off, size;
		m_Attrs.MenuField(6, &off, &size);
		temp.Format(TEXT("%d"), m_Attrs.MenuTextId(e));
		m_ResourceList.SetItemText(k, 8, temp);
		temp.Format(TEXT("%d"), m_Attrs.MenuTag(e));
		m_ResourceList.SetItemText(k, 9, temp);
		m_Attrs.MenuField(8, &off, &size);
		m_ResourceList.SetItemText(k, 10, HexText(*(const DWORD *)((const BYTE *)e + off)));
		m_Attrs.MenuField(9, &off, &size);
		m_ResourceList.SetItemText(k, 11, HexText(*(const DWORD *)((const BYTE *)e + off)));
		temp.Format(TEXT("%d"), e->textID2);
		m_ResourceList.SetItemText(k, 12, temp);
		m_ResourceList.SetItemText(k, 13, HexText(e->z7));
		temp.Format(TEXT("%d"), e->typeID);
		if (e->typeID == 0 && m_Attrs.MenuDefaultFont(pRec->id) != 0)
			temp.Format(TEXT("0 (default %d)"), m_Attrs.MenuDefaultFont(pRec->id));
		m_ResourceList.SetItemText(k, 14, temp);
		std::vector<CCellLink> links;
		CString summary = m_Attrs.MenuSummary(pRec->id, k, e, &links);
		m_ResourceList.SetItemText(k, 15, summary);
		m_ResourceList.SetCellLinks(k, 15, links);

		for (int col = 0; col <= 14; col++)
		{
			CString tip = m_Attrs.MenuTip(e, MenuLogicalCol(col), pRec->id);
			if (col == 3)
				tip = _T("Command ran when highlighted");        // the name columns only name it; the ID columns open it
			else if (col == 5)
				tip = _T("Command ran when chosen");
			if (!tip.IsEmpty())
				m_ResourceList.SetItemToolTipText(k, col, tip);
		}
		m_ResourceList.SetItemToolTipText(k, 15, summary);
	}

	// where the menu is used: a drop-down in the last column lists the lists of layouts that show it, choosing one goes there
	m_TypeUses = m_Attrs.MenuUses(pRec->id);
	m_UsedCombo.ResetContent();
	if (pRec->num_elems < 1)
		return;
	if (m_TypeUses.empty())
	{
		m_ResourceList.SetItemText(0, 16, _T("not shown by a layout"));
		return;
	}
	for (size_t u = 0; u < m_TypeUses.size(); u++)
	{
		int s, n;
		m_UsedCombo.AddString(m_Attrs.TypeUseText(m_TypeUses[u], &s, &n));
	}
	m_UsedCombo.SetCurSel(0);
	m_UsedShown = false;
	PlaceUsedCombo();
}

void CLayoutDialog::FillTypeList(TypeRecord *pRec)
{
	const resource_type_type_elem *e = pRec->element;
	CString temp;
	temp.Format(TEXT("%d"), e->fontID);
	m_ResourceList.InsertItem(0, temp);
	m_ResourceList.SetItemText(0, 1, HexText(e->z1));
	temp.Format(TEXT("%d"), e->size);
	m_ResourceList.SetItemText(0, 2, temp);
	m_ResourceList.SetItemText(0, 3, Hex8(e->color));
	temp.Format(TEXT("%d"), (int)e->style);
	m_ResourceList.SetItemText(0, 4, temp);
	temp.Format(TEXT("%d"), (int)e->alignment);
	m_ResourceList.SetItemText(0, 5, temp);
	m_ResourceList.SetItemText(0, 6, HexText(e->z2));
	for (int col = 0; col <= 6; col++)
	{
		CString tip = m_Attrs.TypeTip(e, col);
		if (!tip.IsEmpty())
			m_ResourceList.SetItemToolTipText(0, col, tip);
	}

	// where the font is used: a drop-down in the last column lists every text and menu entry, choosing one goes there
	m_TypeUses = m_Attrs.TypeUses(pRec->id);
	m_UsedCombo.ResetContent();
	if (m_TypeUses.empty())
	{
		m_ResourceList.SetItemText(0, 7, _T("not used"));
		return;
	}
	for (size_t u = 0; u < m_TypeUses.size(); u++)
	{
		int s, n;
		m_UsedCombo.AddString(m_Attrs.TypeUseText(m_TypeUses[u], &s, &n));
	}
	m_UsedCombo.SetCurSel(0);
	m_UsedShown = false;
	PlaceUsedCombo();
}

// The drop-down covers the "Used by" cell of the font type (and follows it when the list is scrolled or resized)
void CLayoutDialog::PlaceUsedCombo()
{
	if (m_UsedCombo.m_hWnd == NULL)
		return;
	RECT cell, client;
	if ((m_ResourceType != 1 && m_ResourceType != 2) || m_TypeUses.empty() || m_ResourceList.GetItemCount() < 1 ||
	    !ListView_GetSubItemRect(m_ResourceList.m_hWnd, 0, m_ResourceType == 1 ? 16 : 7, LVIR_BOUNDS, &cell))
	{
		if (m_UsedShown)
			m_UsedCombo.ShowWindow(SW_HIDE);
		m_UsedShown = false;
		return;
	}
	m_ResourceList.GetClientRect(&client);
	int right = cell.right < client.right ? cell.right : client.right;
	if (cell.left < 0 || right - cell.left < 60 || cell.top < 0)
	{
		if (m_UsedShown)
			m_UsedCombo.ShowWindow(SW_HIDE);
		m_UsedShown = false;
		return;
	}
	CRect rc(cell.left, cell.top, right, cell.top + 18);          // in the client area of the list, where the drop-down is a child
	// The list tells every time it is drawn; the drop-down is moved only when its place really changed, or it would draw
	// the list again, and that would move it again, without end.
	if (m_UsedShown && rc == m_UsedPlaced && m_UsedCombo.IsWindowVisible())
		return;
	m_UsedPlaced = rc;
	m_UsedShown = true;
	m_UsedCombo.SetWindowPos(&CWnd::wndTop, rc.left, rc.top, rc.Width(), 200, SWP_SHOWWINDOW | SWP_NOACTIVATE);
}

LRESULT CLayoutDialog::OnListPainted(WPARAM, LPARAM)
{
	PlaceUsedCombo();
	return 0;
}

void CLayoutDialog::OnCbnSelendokUsedCombo()
{
	int sel = m_UsedCombo.GetCurSel();
	if (sel < 0 || sel >= (int)m_TypeUses.size())
		return;
	CLayoutAttrs::TypeUse u = m_TypeUses[sel];
	SelectResource(u.kind == CLayoutAttrs::TU_MENU ? 1 : 0, u.id, u.row, u.offset, u.size, u.kind == CLayoutAttrs::TU_MENU ? 12 : (u.kind == CLayoutAttrs::TU_LIST ? 3 : 16));
}

// ---- editing

// Show the list of the choices a value can have; the one picked is written into the firmware.
bool CLayoutDialog::PickValue(const TCHAR *const *choices, int n, int current, DWORD offset, DWORD size)
{
	CPoint pt;
	GetCursorPos(&pt);
	CMenu menu;
	menu.CreatePopupMenu();
	for (int i = 0; i < n; i++)
		menu.AppendMenu(MF_STRING | (i == current ? MF_CHECKED : 0), 1000 + i, choices[i]);
	int sel = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, this);
	if (sel < 1000 || sel >= 1000 + n)
		return false;
	DWORD value = sel - 1000;
	m_pFirmware->SetData(offset, (LPBYTE)&value, size);
	m_HexWnd.SetOffset(offset);
	m_HexWnd.Invalidate();
	RefreshSelection();
	return true;
}

// Show the list again, for the entry that is selected in the tree (after a value has been changed)
void CLayoutDialog::RefreshSelection()
{
	HTREEITEM it = m_ResourceTree.GetSelectedItem();
	if (it == NULL)
		return;
	POSITION pos = m_ResourceList.GetFirstSelectedItemPosition();
	int row = pos ? m_ResourceList.GetNextSelectedItem(pos) : -1;
	LRESULT res;
	NMTREEVIEW nm;
	nm.itemNew.hItem = it;
	OnTvnSelchangedResourceTree((NMHDR *)&nm, &res);
	if (row >= 0 && row < m_ResourceList.GetItemCount())
	{
		m_ResourceList.SetItemState(row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		m_ResourceList.EnsureVisible(row, FALSE);
	}
}

// Choose a color for a stored value. The opacity stays as it is: the color dialog only knows the three colors.
bool CLayoutDialog::PickColor(DWORD offset, DWORD current)
{
	if (MessageBox(_T("Do you want to change this color by picking a color from the palette?"), _T("Color changer"), MB_YESNO) != IDYES)
		return false;
	unsigned int ret;
	if (!GetUserColor(current & 0x00FFFFFF, &ret, GetParent()->GetParent()->GetParent()->m_hWnd))
		return false;
	DWORD value = (current & 0xFF000000) | (ret & 0x00FFFFFF);
	m_pFirmware->SetData(offset, (LPBYTE)&value, 4);
	m_HexWnd.SetOffset(offset);
	m_HexWnd.Invalidate();
	RefreshSelection();
	return true;
}

// The font type that is selected in the tree. A few font types exist in several copies with the same ID, so the
// entry in the tree (which remembers its place in the list) decides, not the ID.
TypeRecord *CLayoutDialog::SelectedType()
{
	return TypeOfItem(m_ResourceTree.GetSelectedItem());
}

TypeRecord *CLayoutDialog::TypeOfItem(HTREEITEM h)
{
	if (h == NULL || TypeList == NULL)
		return NULL;
	DWORD_PTR data = m_ResourceTree.GetItemData(h);
	if (data > 0 && data <= TypeList->size())
		return TypeList->at(data - 1);
	int id = _ttoi(m_ResourceTree.GetItemText(h));
	for (size_t i = 0; i < TypeList->size(); i++)
		if (TypeList->at(i)->id == id)
			return TypeList->at(i);
	return NULL;
}

// The mode the tabs of the editor ask for: 0 layouts, 1 menus, 2 font types, 3 commands as source, 4 as assembly
void CLayoutDialog::SetMode(int mode)
{
	if (mode < 0 || mode > 4)
		return;
	m_RequestedMode = mode;
	if (!m_bModesReady || (m_bListsBuilt && m_ResourceType == mode && m_ResourceTypeIdx.GetCurSel() == mode))
	{
		UpdateModeText();
		return;
	}
	m_ResourceTypeIdx.SetCurSel(mode);
	OnCbnSelchangeResourceTypeidxCombo();
}

// One line under the buttons that says what the list below shows
void CLayoutDialog::UpdateModeText()
{
	static const TCHAR *const text[5] = {
		_T("Each row is one element of the selected layout: a text, a bitmap or another layout, with its position and size."),
		_T("Each row is one entry of the selected menu: its text, the commands it runs when chosen and when highlighted, its font."),
		_T("How text is drawn: the font, size, color, style and position of each font type, and every text or menu entry that uses it."),
		_T("The commands the menus run, as readable source (read only). Each lists where it is used and which commands it refers to."),
		_T("The same commands as byte code: one instruction per row with its bytes. Here the numbers inside a command can be changed."),
	};
	int mode = m_RequestedMode < 0 || m_RequestedMode > 4 ? 0 : m_RequestedMode;
	CWnd *pText = GetDlgItem(IDC_MODE_TEXT);
	if (pText != NULL)
		pText->SetWindowText(text[mode]);
}

// A click on a marked part of a cell: its bytes are shown in the hex view, and they are what Change Value changes
void CLayoutDialog::ShowLink(const CCellLink &link)
{
	m_bLinkClick = true;
	m_bValBigEndian = link.bigEndian;
	m_ClickedCol = link.col;
	m_ValOffset = link.offset;
	m_ValSize = link.size;
	m_HexWnd.SetRange(link.offset, link.size);
	RefreshChangeCue();
}

// ---- what each mode remembers while another one is shown

// The way down to an item of the tree: the number of the child (counting from 0) to take at every level
std::vector<int> CLayoutDialog::TreePath(HTREEITEM h)
{
	std::vector<int> path;
	while (h != NULL)
	{
		int index = 0;
		for (HTREEITEM s = m_ResourceTree.GetPrevSiblingItem(h); s != NULL; s = m_ResourceTree.GetPrevSiblingItem(s))
			index++;
		path.insert(path.begin(), index);
		h = m_ResourceTree.GetParentItem(h);
	}
	return path;
}

// The item at the end of such a way; the items on it are opened (the commands are added when an item is opened)
HTREEITEM CLayoutDialog::TreeItemOfPath(const std::vector<int> &path)
{
	HTREEITEM h = NULL, parent = NULL;
	for (size_t level = 0; level < path.size(); level++)
	{
		if (level == 0)
			h = m_ResourceTree.GetRootItem();
		else
		{
			m_ResourceTree.Expand(parent, TVE_EXPAND);
			h = m_ResourceTree.GetChildItem(parent);
		}
		for (int i = 0; i < path[level] && h != NULL; i++)
			h = m_ResourceTree.GetNextSiblingItem(h);
		if (h == NULL)
			return NULL;
		parent = h;
	}
	return h;
}

// The ways down to every item that is open, the ones further up first
void CLayoutDialog::CollectExpanded(HTREEITEM first, std::vector<int> &path, std::vector<std::vector<int> > &out)
{
	int index = 0;
	for (HTREEITEM h = first; h != NULL; h = m_ResourceTree.GetNextSiblingItem(h), index++)
	{
		if (!(m_ResourceTree.GetItemState(h, TVIS_EXPANDED) & TVIS_EXPANDED))
			continue;
		path.push_back(index);
		out.push_back(path);
		CollectExpanded(m_ResourceTree.GetChildItem(h), path, out);
		path.pop_back();
	}
}

void CLayoutDialog::SaveModeState(int mode)
{
	if (mode < 0 || mode > 4)
		return;
	ModeState &st = m_State[mode];
	st.valid = true;
	st.selPath = TreePath(m_ResourceTree.GetSelectedItem());
	st.topPath = TreePath(m_ResourceTree.GetFirstVisibleItem());
	st.expanded.clear();
	std::vector<int> path;
	CollectExpanded(m_ResourceTree.GetRootItem(), path, st.expanded);
	POSITION pos = m_ResourceList.GetFirstSelectedItemPosition();
	st.row = pos ? m_ResourceList.GetNextSelectedItem(pos) : -1;
	st.topRow = m_ResourceList.GetTopIndex();
	st.col = m_ClickedCol;
	st.valOffset = m_ValOffset;
	st.valSize = m_ValSize;
	st.cursor = m_HexWnd.m_CursorPos;
	st.rangeStart = m_HexWnd.m_RangeStart;
	st.rangeLen = m_HexWnd.m_RangeLen;
	st.function = m_CmdFunction;
	if (mode == 3 || mode == 4)
		m_LastCmdMode = mode;
}

// Select again what was selected when the mode was left, show it, and put the hex view where it was. The two command
// modes show the same command: the one that was selected last in either.
void CLayoutDialog::RestoreModeState(int mode)
{
	if (mode < 0 || mode > 4)
		return;
	const ModeState &own = m_State[mode];
	const ModeState &ts = (mode == 3 || mode == 4) ? m_State[m_LastCmdMode] : own;
	if (!ts.valid)
		return;

	for (size_t i = 0; i < ts.expanded.size(); i++)
	{
		HTREEITEM h = TreeItemOfPath(ts.expanded[i]);
		if (h != NULL)
			m_ResourceTree.Expand(h, TVE_EXPAND);
	}
	HTREEITEM sel = ts.selPath.empty() ? NULL : TreeItemOfPath(ts.selPath);
	if (sel != NULL)
		m_ResourceTree.SelectItem(sel);
	HTREEITEM top = ts.topPath.empty() ? NULL : TreeItemOfPath(ts.topPath);
	if (top != NULL)
		m_ResourceTree.SelectSetFirstVisible(top);
	if (sel != NULL)
		m_ResourceTree.EnsureVisible(sel);
	m_ResourceTree.SendMessage(WM_HSCROLL, MAKELONG(SB_LEFT, 0), 0);      // showing the item must not leave the tree pushed sideways

	// the list: the row, how far it was scrolled, the value that was selected in it (the command modes only when it
	// is the same command)
	if (sel == NULL || !own.valid || (mode >= 3 && own.function != m_CmdFunction))
		return;
	int count = m_ResourceList.GetItemCount();
	if (own.topRow > 0 && own.topRow < count)
	{
		int last = own.topRow + m_ResourceList.GetCountPerPage() - 1;
		m_ResourceList.EnsureVisible(last < count ? last : count - 1, FALSE);
	}
	if (own.row >= 0 && own.row < count)
	{
		m_ResourceList.SetItemState(own.row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		m_ResourceList.EnsureVisible(own.row, FALSE);
	}
	m_ClickedCol = own.col;
	m_ValOffset = own.valOffset;
	m_ValSize = own.valSize;
	if (own.rangeLen > 0)
		m_HexWnd.SetRange(own.rangeStart, own.rangeLen, FALSE);
	else
		m_HexWnd.SetOffset(own.cursor, FALSE);
	RefreshChangeCue();
}
