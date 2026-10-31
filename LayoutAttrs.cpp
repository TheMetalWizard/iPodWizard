// LayoutAttrs.cpp - what the numbers in layouts, menus and font types say. See LayoutAttrs.h.

#include "stdafx.h"
#include "iPodWizard.h"
#include "Firmware.h"
#include "LayoutNames.h"
#include "CommandTable.h"
#include "LayoutAttrs.h"
#include <algorithm>

namespace
{
	// the four edges of a layout element in the order they are stored (the same as in resource_type_view_elem)
	const TCHAR *kEdgeName[4] = { _T("top"), _T("left"), _T("bottom"), _T("right") };

	const BYTE *Bytes(const void *p) { return (const BYTE *)p; }

	DWORD ReadAt(const void *base, DWORD offset, DWORD size)
	{
		const BYTE *p = Bytes(base) + offset;
		switch (size)
		{
		case 1: return p[0];
		case 2: return (DWORD)p[0] | ((DWORD)p[1] << 8);
		default: return (DWORD)p[0] | ((DWORD)p[1] << 8) | ((DWORD)p[2] << 16) | ((DWORD)p[3] << 24);
		}
	}

	CString Hex(DWORD v)
	{
		CString s;
		s.Format(_T("0x%X"), v);
		return s;
	}

	// the tag of a layout element as it reads ("Str ", "BMap", "None"): it is stored back to front
	void ReadableTag(const resource_type_view_elem *e, TCHAR out[5])
	{
		for (int i = 0; i < 4; i++)
		{
			unsigned char c = (unsigned char)e->type[3 - i];
			out[i] = (c >= 32 && c < 127) ? (TCHAR)c : _T('?');
		}
		out[4] = 0;
	}

	int Small16(DWORD v) { return (short)(v & 0xFFFF); }
}

namespace
{
	// Builds the text of a summary and remembers which parts of it are numbers stored in the entry, together with the
	// bytes they are stored in (the list frames those parts and shows the bytes in the hex view when one is clicked)
	class CSummary
	{
	public:
		CSummary(const CLayoutAttrs *pAttrs, const void *pElem, const BYTE *pFirmware, std::vector<CCellLink> *pLinks)
			: m_pAttrs(pAttrs), m_Base(pElem ? (DWORD)((const BYTE *)pElem - pFirmware) : 0), m_pLinks(pLinks)
		{
		}

		CString text;

		void Add(const CString &t) { text += t; }
		void Add(const TCHAR *t) { text += t; }

		// text that stands for `size` bytes at `rel` (counting from the start of the entry); col: the list column
		// whose value they are (what Change Value would change), -1 when there is none
		void Part(const CString &t, DWORD rel, DWORD size, int col, const CString &what, bool bigEndian = false)
		{
			int start = text.GetLength();
			text += t;
			if (m_pLinks == NULL || size == 0 || t.IsEmpty())
				return;
			CCellLink l;
			l.start = start;
			l.len = t.GetLength();
			l.offset = m_Base + rel;
			l.size = size;
			l.col = col;
			l.tip = what;
			m_pLinks->push_back(l);
		}

		void Num(int value, DWORD rel, DWORD size, int col, const CString &what)
		{
			CString t;
			t.Format(_T("%d"), value);
			Part(t, rel, size, col, what);
		}

	private:
		const CLayoutAttrs *m_pAttrs;
		DWORD m_Base;
		std::vector<CCellLink> *m_pLinks;
	};
}

CLayoutAttrs::CLayoutAttrs()
	: m_pFw(NULL), m_pNames(NULL), m_pCmds(NULL), m_bMenuShifted(false), m_bOtf(false)
{
}

// Where a value is stored, for tooltips: the bytes in the order they are in the firmware, and what they read as
CString CLayoutAttrs::StoredTip(DWORD offset, DWORD size, bool bigEndian) const
{
	CString s;
	if (m_pFw == NULL || size == 0 || (ULONGLONG)offset + size > m_pFw->GetFirmwareSize())
		return s;
	const BYTE *p = m_pFw->GetFirmwareBuffer() + offset;
	CString bytes;
	for (DWORD i = 0; i < size; i++)
	{
		CString t;
		t.Format(_T("%02X"), p[i]);
		if (i)
			bytes += _T(" ");
		bytes += t;
	}
	s.Format(_T("0x%08X: %s"), offset, (LPCTSTR)bytes);
	if (size == 2 || size == 4)
	{
		DWORD v = 0;
		for (DWORD i = 0; i < size; i++)
			v |= (DWORD)p[bigEndian ? size - 1 - i : i] << (8 * i);
		CString num;
		if (size == 4 && (int)v < 0 && (int)v > -65536)
			num.Format(_T("0x%X = %d"), v, (int)v);
		else
			num.Format(_T("0x%X = %u"), v, v);
		s.AppendFormat(bigEndian ? _T(" (BE) = %s") : _T(" (LE) = %s"), (LPCTSTR)num);
	}
	return s;
}

void CLayoutAttrs::Build(CFirmware *pFirmware, CLayoutNames *pNames, CCommandTable *pCommands)
{
	m_pFw = pFirmware;
	m_pNames = pNames;
	m_pCmds = pCommands;
	m_bMenuShifted = false;
	m_bOtf = false;
	if (m_pFw == NULL)
		return;
	m_bOtf = m_pFw->GetNumOTFFonts() > 0;

	// Menu entries store the number of the string they show twice (+32 always); where the first copy sits tells
	// which of the two layouts the firmware uses (+20 in most, +24 in the 5.5G and the development nano).
	int nOld = 0, nNew = 0;
	MenuTable *pMenus = m_pFw->GetMenuList();
	if (pMenus)
		for (size_t i = 0; i < pMenus->size(); i++)
			for (int k = 0; k < pMenus->at(i)->num_elems; k++)
			{
				const resource_type_menu_elem *e = pMenus->at(i)->MenuElemList[k]->element;
				if (e->size < 36)
					continue;
				DWORD text2 = ReadAt(e, 32, 2);
				bool bOld = ReadAt(e, 20, 2) == text2;
				bool bNew = ReadAt(e, 24, 2) == text2;
				if (bOld && !bNew)
					nOld++;
				else if (bNew && !bOld)
					nNew++;
			}
	m_bMenuShifted = nNew > nOld;

	// where the font types are used: the entries of menus and the texts of layouts that name them
	m_TypeUses.clear();
	const BYTE *fw = m_pFw->GetFirmwareBuffer();
	if (pMenus)
		for (size_t i = 0; i < pMenus->size(); i++)
			for (int k = 0; k < pMenus->at(i)->num_elems; k++)
			{
				const resource_type_menu_elem *e = pMenus->at(i)->MenuElemList[k]->element;
				if (e->typeID == 0)
					continue;
				TypeUse u;
				u.kind = TU_MENU;
				u.id = pMenus->at(i)->id;
				u.row = k;
				u.stringId = 0;
				u.offset = (DWORD)((const BYTE *)&e->typeID - fw);
				u.size = 4;
				u.text = m_pNames ? m_pNames->MenuEntryText(e) : CString();
				m_TypeUses[e->typeID].push_back(u);
			}
	ViewTable *pViews = m_pFw->GetViewList();
	if (pViews)
		for (size_t i = 0; i < pViews->size(); i++)
			for (int k = 0; k < pViews->at(i)->num_elems; k++)
			{
				const resource_type_view_elem *e = pViews->at(i)->ViewElemList[k]->element;
				if (!IsKind(e, " rtS"))
					continue;
				Extra x;
				ExtraOf(e, &x);
				int t = ViewFontType(e);
				if (t == 0 || x.text < 0)
					continue;
				TypeUse u;
				u.kind = TU_TEXT;
				u.id = pViews->at(i)->id;
				u.row = k;
				u.stringId = e->id != 0 ? e->id : Small16(x.p[x.text]);
				u.offset = (DWORD)((const BYTE *)e - fw) + 76 + 4 * x.text + 2;     // the high half of {string, font type}
				u.size = 2;
				u.text = (m_pNames && u.stringId != 0) ? m_pNames->Flatten(m_pNames->StringText(u.stringId)) : CString();
				m_TypeUses[t].push_back(u);
			}

	// where the menus are used: the lists (class 0x1800) of layouts that show them
	m_MenuUses.clear();
	if (pViews)
		for (size_t i = 0; i < pViews->size(); i++)
			for (int k = 0; k < pViews->at(i)->num_elems; k++)
			{
				const resource_type_view_elem *e = pViews->at(i)->ViewElemList[k]->element;
				if (!IsKind(e, "enoN") || e->z0 != 0x1800 || e->id == 0)
					continue;
				TypeUse u;
				u.kind = TU_LIST;
				u.id = pViews->at(i)->id;
				u.row = k;
				u.stringId = 0;
				u.offset = (DWORD)((const BYTE *)&e->id - fw);
				u.size = 2;
				m_MenuUses[(short)e->id].push_back(u);
			}

	// the font the entries of a menu use when they name none: the one of the text in the layout of the menu
	m_MenuFont.clear();
	if (pMenus && pViews)
	{
		std::map<int, const ViewRecord *> byId;
		for (size_t i = 0; i < pViews->size(); i++)
			byId[pViews->at(i)->id] = pViews->at(i);
		for (size_t i = 0; i < pMenus->size(); i++)
		{
			std::vector<int> seen;
			int font = LayoutFont(byId, (int)(short)pMenus->at(i)->id, 0, seen);
			if (font != 0)
				m_MenuFont[pMenus->at(i)->id] = font;
		}
	}

	// the entries of a menu that name no font type take the one of the menu's layout: they use that font type as well, though no
	// number of it is stored in them
	if (pMenus)
		for (size_t i = 0; i < pMenus->size(); i++)
		{
			std::map<int, int>::const_iterator f = m_MenuFont.find(pMenus->at(i)->id);
			if (f == m_MenuFont.end())
				continue;
			for (int k = 0; k < pMenus->at(i)->num_elems; k++)
			{
				const resource_type_menu_elem *e = pMenus->at(i)->MenuElemList[k]->element;
				if (e->typeID != 0)
					continue;
				TypeUse u;
				u.kind = TU_MENU;
				u.id = pMenus->at(i)->id;
				u.row = k;
				u.stringId = 0;
				u.offset = (DWORD)((const BYTE *)&e->typeID - fw);
				u.size = 4;
				u.text = m_pNames ? m_pNames->MenuEntryText(e) : CString();
				u.bImplicit = true;
				m_TypeUses[f->second].push_back(u);
			}
		}
}

// The font type of the first text of a layout, looking into the layouts it contains
int CLayoutAttrs::LayoutFont(const std::map<int, const ViewRecord *> &byId, int layoutId, int depth, std::vector<int> &seen) const
{
	if (depth > 6 || std::find(seen.begin(), seen.end(), layoutId) != seen.end())
		return 0;
	std::map<int, const ViewRecord *>::const_iterator it = byId.find(layoutId);
	if (it == byId.end())
		return 0;
	seen.push_back(layoutId);
	const ViewRecord *pView = it->second;
	for (int k = 0; k < pView->num_elems; k++)
	{
		const resource_type_view_elem *e = pView->ViewElemList[k]->element;
		int font = 0;
		if (IsKind(e, " rtS"))
			font = ViewFontType(e);
		else if (IsKind(e, "enoN") && e->id != 0)
			font = LayoutFont(byId, e->id, depth + 1, seen);
		if (font != 0)
			return font;
	}
	return 0;
}

int CLayoutAttrs::MenuDefaultFont(int menuId) const
{
	std::map<int, int>::const_iterator it = m_MenuFont.find(menuId);
	return it == m_MenuFont.end() ? 0 : it->second;
}

const std::vector<CLayoutAttrs::TypeUse> &CLayoutAttrs::MenuUses(int menuId) const
{
	std::map<int, std::vector<TypeUse> >::const_iterator it = m_MenuUses.find((short)menuId);
	return it == m_MenuUses.end() ? m_NoUses : it->second;
}

const std::vector<CLayoutAttrs::TypeUse> &CLayoutAttrs::TypeUses(int typeId) const
{
	std::map<int, std::vector<TypeUse> >::const_iterator it = m_TypeUses.find(typeId);
	return it == m_TypeUses.end() ? m_NoUses : it->second;
}

// One line of the "Used by" list. The first part says where, and is the part that stands for the stored number.
CString CLayoutAttrs::TypeUseText(const TypeUse &u, int *pLinkStart, int *pLinkLen) const
{
	CString s;
	if (u.kind == TU_LIST)
		s.Format(_T("layout %d, row %d (the list of the menu)"), u.id, u.row + 1);
	else if (u.kind == TU_MENU && u.bImplicit)
		s.Format(_T("menu %d, entry %d (inherited)"), u.id, u.row + 1);
	else if (u.kind == TU_MENU)
		s.Format(_T("menu %d, entry %d"), u.id, u.row + 1);
	else
		s.Format(_T("layout %d, row %d"), u.id, u.row + 1);
	*pLinkStart = 0;
	*pLinkLen = s.GetLength();
	if (u.kind == TU_LIST)
		return s;
	if (!u.text.IsEmpty())
		s += _T(" - '") + u.text + _T("'");
	else if (u.kind == TU_MENU)
		s += _T(" - (no text)");
	else if (u.stringId != 0)
		s.AppendFormat(_T(" - (string %d is not in this firmware)"), u.stringId);
	else
		s += _T(" - (text set by the firmware)");
	return s;
}

CString CLayoutAttrs::TypeUseTip(const TypeUse &u, int typeId) const
{
	CString s;
	if (u.kind == TU_LIST)
		s.Format(_T("Row %d of layout %d is a list that shows menu %d."), u.row + 1, u.id, typeId);
	else if (u.kind == TU_MENU && u.bImplicit)
		s.Format(_T("Entry %d of menu %d names no font type, so its text inherits the font type of the text in the layout of the menu: %d."), u.row + 1, u.id, typeId);
	else if (u.kind == TU_MENU)
		s.Format(_T("Entry %d of menu %d uses font type %d for its text."), u.row + 1, u.id, typeId);
	else
		s.Format(_T("Row %d of layout %d (a text) uses font type %d."), u.row + 1, u.id, typeId);
	CString stored = StoredTip(u.offset, u.size, false);
	if (!stored.IsEmpty())
		s += _T("\nFont type ID at ") + stored;
	s += _T("\nDouble click to go there.");
	return s;
}

// ---------------------------------------------------------------------------------------------------------
// What a number may be
// ---------------------------------------------------------------------------------------------------------

CString CLayoutAttrs::Probe(int value, bool bSkipSmall) const
{
	CString s;
	if (m_pNames == NULL)
		return s;
	if (bSkipSmall && value > -2 && value < 1000)
		return s;
	std::vector<CLayoutNames::Match> matches;
	m_pNames->Probe(value, matches);
	for (size_t i = 0; i < matches.size(); i++)
	{
		if (!s.IsEmpty())
			s += _T("; ");
		s.AppendFormat(_T("%s %d"), CLayoutNames::KindName(matches[i].kind), matches[i].id);
		if (!matches[i].text.IsEmpty())
			s += _T(" ") + matches[i].text;
	}
	return s;
}

CString CLayoutAttrs::FunctionText(unsigned int ref, const TCHAR *what) const
{
	CString s;
	if (ref == 0)
	{
		s.Format(_T("%s: none"), what);
		return s;
	}
	int fn = CCommandTable::RefToFunction(ref);
	if (fn < 0 || m_pCmds == NULL || m_pCmds->FindFunction(fn) == NULL)
	{
		s.Format(_T("%s: 0x%X is not a command of this firmware"), what, ref);
		return s;
	}
	CString name = m_pCmds->Name(fn);
	CString role = m_pCmds->Role(fn);
	s.Format(_T("%s: %d"), what, fn);
	if (!name.IsEmpty())
		s += _T(" (") + name + _T(")");
	else if (!role.IsEmpty())
		s += _T(" (") + role + _T(")");
	return s;
}

CString CLayoutAttrs::DescribeBits(DWORD value, const TCHAR *name) const
{
	CString s;
	s.Format(_T("%s 0x%X"), name, value);
	if (value == 0)
		return s + _T(": no bits set");
	s += _T(": bits ");
	bool first = true;
	for (int b = 0; b < 32; b++)
		if (value & (1u << b))
		{
			CString t;
			t.Format(first ? _T("%d") : _T(", %d"), b);
			s += t;
			first = false;
		}
	return s;
}

// ---------------------------------------------------------------------------------------------------------
// Font types
// ---------------------------------------------------------------------------------------------------------

CString CLayoutAttrs::StyleName(int style) const
{
	switch (style)
	{
	case 0: return _T("Regular");
	case 1: return _T("Bold");
	}
	CString s;
	s.Format(_T("Style %d"), style);
	return s;
}

CString CLayoutAttrs::AlignName(int align)
{
	switch (align)
	{
	case 0: return _T("Left");
	case 1: return _T("Center");
	case 2: return _T("Right");
	}
	CString s;
	s.Format(_T("Unknown (%d)"), align);
	return s;
}

// Colours are stored as 0xAABBGGRR: a Windows COLORREF with the opacity in the top byte
CString CLayoutAttrs::ColorName(DWORD v)
{
	if (v == 0)
		return _T("not set");
	CString s;
	s.Format(_T("#%02X%02X%02X"), v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF);
	DWORD a = v >> 24;
	if (a == 0xFF)
		s += _T(", opaque");
	else if (a == 0)
		s += _T(", fully transparent");
	else
		s.AppendFormat(_T(", %d%% opaque"), (int)(a * 100 / 255));
	return s;
}

const resource_type_type_elem *CLayoutAttrs::FindType(int id) const
{
	if (m_pFw == NULL)
		return NULL;
	TypeTable *pTypes = m_pFw->GetTypeList();
	if (pTypes == NULL)
		return NULL;
	for (size_t i = 0; i < pTypes->size(); i++)
		if (pTypes->at(i)->id == id)
			return pTypes->at(i)->element;
	return NULL;
}

CString CLayoutAttrs::TypeSummary(const resource_type_type_elem *e) const
{
	if (e == NULL)
		return CString();
	CString name = m_pNames ? m_pNames->StringText(e->fontID) : CString();
	if (name.IsEmpty())
		name.Format(_T("font %d"), e->fontID);
	CString s;
	s.Format(_T("%s %d, %s"), (LPCTSTR)name, e->size, (LPCTSTR)StyleName(e->style).MakeLower());
	switch (e->alignment)
	{
	case 0: s += _T(", left"); break;
	case 1: s += _T(", centered"); break;
	case 2: s += _T(", right"); break;
	}
	if (e->color != 0 && e->color != 0xFF000000)
		s += _T(", color ") + ColorName(e->color);
	return s;
}

CString CLayoutAttrs::TypeTip(const resource_type_type_elem *e, int col) const
{
	CString s;
	switch (col)
	{
	case 0:
		{
			CString name = m_pNames ? m_pNames->StringText(e->fontID) : CString();
			if (name.IsEmpty())
				s.Format(_T("fontID = String\nCurrent font: string %d is not in this firmware."), e->fontID);
			else
				s.Format(_T("fontID = String\nCurrent font: %s\nDouble click to open the string."), (LPCTSTR)name);
		}
		break;
	case 1:
	case 6:
		s = _T("No known use: these two bytes are 0 in most firmwares; some old ones have stray bytes (such as 0xDADA) here.");
		break;
	case 2:
		s.Format(_T("Size of the font: %d. The font is found by its name and this size.\nDouble click to open that font."), e->size);
		break;
	case 3:
		s = _T("Text color, stored as 0xAABBGGRR.\nMonochrome iPods will be 0x00000000.\nDouble click to choose another color.");
		break;
	case 4:
		s = _T("Text style: 0 = Regular, 1 = Bold.\nOTF style: 1 = Regular");
		break;
	case 5:
		s = _T("0 = Left, 1 = Center, 2 = Right.\nDouble click to choose.");
		break;
	}
	return s;
}

// ---------------------------------------------------------------------------------------------------------
// Menus
// ---------------------------------------------------------------------------------------------------------

// list columns: 0 size, 1 z1, 2 z2, 3 command, 4 submenuID, 5 z3, 6 textID, 7 z4, 8 z5, 9 z6, 10 textID2, 11 z7, 12 typeID
void CLayoutAttrs::MenuField(int col, DWORD *pOffset, DWORD *pSize) const
{
	static const DWORD off[13] = { 0, 4, 8, 12, 16, 18, 20, 22, 24, 28, 32, 34, 36 };
	static const DWORD siz[13] = { 4, 4, 4, 4, 2, 2, 2, 2, 4, 4, 2, 2, 4 };
	if (col < 0 || col > 12)
	{
		*pOffset = 0;
		*pSize = 0;
		return;
	}
	*pOffset = off[col];
	*pSize = siz[col];
	if (m_bMenuShifted)
	{
		// the text and its number in the menu are 4 bytes further, the dword in front of them takes the place of z5
		if (col == 6)      *pOffset = 24;
		else if (col == 7) *pOffset = 26;
		else if (col == 8) *pOffset = 20;
	}
}

int CLayoutAttrs::MenuTextId(const resource_type_menu_elem *e) const
{
	DWORD off, size;
	MenuField(6, &off, &size);
	return Small16(ReadAt(e, off, size));
}

int CLayoutAttrs::MenuTag(const resource_type_menu_elem *e) const
{
	DWORD off, size;
	MenuField(7, &off, &size);
	return (int)ReadAt(e, off, size);
}

CString CLayoutAttrs::MenuSummary(int menuId, int item, const resource_type_menu_elem *e, std::vector<CCellLink> *pLinks) const
{
	CSummary out(this, e, m_pFw->GetFirmwareBuffer(), pLinks);
	DWORD off, size;
	CString text = m_pNames ? m_pNames->MenuEntryText(e) : CString();
	if (!text.IsEmpty())
	{
		MenuField(6, &off, &size);
		CString what;
		what.Format(_T("The text of the entry: string %d."), MenuTextId(e));
		out.Part(_T("'") + text + _T("'"), off, size, 6, what);
	}
	else
		out.Add(_T("(no text)"));

	// the two commands: the number is the command's, which the entry stores as (ID << 2) | 2
	for (int c = 0; c < 2; c++)
	{
		unsigned int ref = c == 0 ? e->cmd : e->z2;
		int fn = CCommandTable::RefToFunction(ref);
		if (m_pCmds == NULL || fn < 0 || m_pCmds->FindFunction(fn) == NULL)
			continue;
		CString num;
		num.Format(_T("%d"), fn);
		out.Add(c == 0 ? _T(" - run -> ") : _T(" - highlight -> "));
		MenuField(c == 0 ? 3 : 2, &off, &size);
		out.Part(num, off, size, c == 0 ? 3 : 2, FunctionText(ref, c == 0 ? _T("Command run when chosen") : _T("Command run when highlighted")));
		CString name = m_pCmds->Name(fn);
		if (!name.IsEmpty())
			out.Add(_T(" (") + name + _T(")"));
	}
	return out.text;
}

CString CLayoutAttrs::MenuTip(const resource_type_menu_elem *e, int col, int menuId) const
{
	CString s;
	DWORD off, size;
	MenuField(col, &off, &size);
	DWORD value = ReadAt(e, off, size);
	switch (col)
	{
	case 0:
		s.Format(_T("Size of this entry in bytes, not counting this number itself: %d (always 36)."), e->size);
		break;
	case 1:
		s.Format(_T("Flags: %s. Always 0x20 plus up to two of the bits 0x02 and 0x04 (so 0x20, 0x22, 0x24 or 0x26). Their meaning is not known."), (LPCTSTR)Hex(e->z1));
		break;
	case 2:
		s = FunctionText(e->z2, _T("Command run when highlighted"));
		s += _T("\nDouble click to open in the command table.");
		break;
	case 3:
		s = FunctionText(e->cmd, _T("Command run when chosen"));
		s += _T("\nDouble click to open in the command table.");
		break;
	case 4:
		if (e->submenuID == 0)
			s = _T("Menu opened by this entry. 0 = none; entries open their screens through their command instead.");
		else
		{
			CString p = Probe(e->submenuID, false);
			s.Format(_T("Entry points at ID %d. %s"), e->submenuID, p.IsEmpty() ? _T("Nothing in this firmware has that ID.") : (LPCTSTR)p);
		}
		break;
	case 5:
		s = _T("No known function. Always 0.");
		break;
	case 11:
		s = _T("Not used: always 0.");
		break;
	case 6:
		{
			int id = Small16(value);
			CString text = m_pNames ? m_pNames->StringText(id) : CString();
			if (m_bMenuShifted)
				s.Format(_T("ID of string shown by the entry (in this firmware at byte 24 of the entry). %s"), text.IsEmpty() ? _T("") : (LPCTSTR)(_T("'") + text + _T("'")));
			else
				s.Format(_T("ID of string shown by the entry. %s"), text.IsEmpty() ? _T("") : (LPCTSTR)(_T("'") + text + _T("'")));
			s += _T("\nDouble click to open the string.");
		}
		break;
	case 7:
		s = _T("No known function. Counts up by 1 per entry from 775 or 776, for the first 9 entries of a menu. 0 for later entries.");
		break;
	case 8:
		if (m_bMenuShifted)
			s = _T("No known function. In this firmware: 0, 1 or 2 (mostly 0).");
		else
		{
			s = _T("No known function. Often 0, the entry's own textID, another string ID or a bitmap ID.");
			CString p = value != 0 ? Probe((int)(value & 0xFFFF), true) : CString();
			if (!p.IsEmpty())
				s += _T("\nIt may be: ") + p + _T(".");
		}
		break;
	case 9:
		s = _T("No known function. Most values also occur as z1 of layout elements in the same firmware.");
		break;
	case 10:
		{
			s = _T("Appears to always match TextID. Ensure this is changed with TextID.\nDouble click to open the string.");
		}
		break;
	case 12:
		s = _T("Font used by entry.\n0 = inherited from menu");
		if (e->typeID == 0)
		{
			int def = MenuDefaultFont(menuId);
			if (def != 0)
				s.AppendFormat(_T("\ninherited font = %d: %s\nDouble click to open it."), def, (LPCTSTR)TypeSummary(FindType(def)));
		}
		else
		{
			const resource_type_type_elem *t = FindType(e->typeID);
			if (t)
				s.AppendFormat(_T("\nFont type %d: %s.\nDouble click to open it."), e->typeID, (LPCTSTR)TypeSummary(t));
			else
				s.AppendFormat(_T("\nFont type %d, which this firmware does not have."), e->typeID);
		}
		break;
	}
	return s;
}

// ---------------------------------------------------------------------------------------------------------
// Layouts
// ---------------------------------------------------------------------------------------------------------

bool CLayoutAttrs::IsKind(const resource_type_view_elem *e, const char *tag)
{
	return strncmp(e->type, tag, 4) == 0;
}

// The type of the element exactly as the firmware stores it: the four characters in the order of the file. A type with
// characters that can not be shown is given as the number.
CString CLayoutAttrs::RawKind(const resource_type_view_elem *e)
{
	TCHAR tag[5];
	for (int i = 0; i < 4; i++)
	{
		unsigned char c = (unsigned char)e->type[i];
		if (c < 32 || c >= 127)
			return Hex(*(const DWORD *)e->type);
		tag[i] = (TCHAR)c;
	}
	tag[4] = 0;
	return tag;
}

CString CLayoutAttrs::KindName(const resource_type_view_elem *e)
{
	if (IsKind(e, "enoN"))
		return _T("Layout");
	if (IsKind(e, " rtS"))
		return _T("Text");
	if (IsKind(e, "paMB"))
		return _T("Bitmap");
	if (IsKind(e, "ltnC"))
		return _T("Control");
	if (IsKind(e, "tStS"))
		return _T("Static text");
	if (IsKind(e, "warD"))
		return _T("Drawing");
	if (IsKind(e, "mTtD"))
		return _T("Date and time");
	TCHAR tag[5];
	ReadableTag(e, tag);
	CString s;
	if (e->type[0] == 0x01)
		s.Format(_T("Custom element %s"), (LPCTSTR)Hex(*(const DWORD *)e->type));
	else
		s.Format(_T("Element '%s'"), tag);
	return s;
}

void CLayoutAttrs::ExtraOf(const resource_type_view_elem *e, Extra *x)
{
	int total = (int)e->size + 4;
	x->n = total > 76 ? (total - 76) / 4 : 0;
	x->p = (const DWORD *)(Bytes(e) + 76);
	x->gradient = x->n >= 2 && (x->p[0] >> 24) == 0xFF && (x->p[1] >> 24) == 0xFF;
	x->text = -1;
	x->scroll = -1;
	if (IsKind(e, " rtS") && x->n >= 3)
	{
		// {string ID, font type ID} and two flag values; behind the colors of a gradient, and in front of
		// four values for text that scrolls when it is too long
		x->text = x->gradient ? 2 : 0;
		if (x->text + 3 > x->n)
			x->text = -1;
		else if (x->n >= x->text + 3 + 4)
			x->scroll = x->text + 3;
	}
}

int CLayoutAttrs::ViewFontType(const resource_type_view_elem *e) const
{
	Extra x;
	ExtraOf(e, &x);
	if (x.text < 0)
		return 0;
	int id = Small16(x.p[x.text] >> 16);        // signed: the font types of newer firmware have negative IDs
	return FindType(id) ? id : 0;
}

bool CLayoutAttrs::ViewColorOffsets(const resource_type_view_elem *e, DWORD *pOffset1, DWORD *pOffset2) const
{
	Extra x;
	ExtraOf(e, &x);
	if (!x.gradient)
		return false;
	*pOffset1 = 76;
	*pOffset2 = 80;
	return true;
}

// The edge descriptor {reference, mode, flag, value}: with the flag 0x8000 the edge is described relative to the
// parent, without it relative to another element of the same layout (reference = its ID, 0 = the one before).
CString CLayoutAttrs::EdgeText(const resource_type_view_elem *e, int edge) const
{
	const resource_view_temp *ed[4] = { &e->top, &e->left, &e->bottom, &e->right };
	const resource_view_temp *d = ed[edge];
	bool bHoriz = (edge == 1 || edge == 3);       // left and right take widths, top and bottom heights
	const TCHAR *name = kEdgeName[edge];
	CString s;
	CString rel;
	if (d->a1 == 0)
		rel = _T("the previous element");
	else
		rel.Format(_T("element %d"), (int)d->a1);

	if (d->a3 == 0x8000)
	{
		DWORD mode = d->a2;
		DWORD flag10 = mode & 0x10;
		switch (mode & ~0x10u)
		{
		case 0x02:
			if (edge == 2 || edge == 3)
				s.Format(_T("The %s edge is %d from the parent's %s edge (negative = inside)."), name, d->val, name);
			else
				s.Format(_T("The %s edge is %d from the parent's %s edge."), name, d->val, name);
			break;
		case 0x04:
			if (bHoriz)
				s.Format(_T("The element is %d wide."), d->val);
			break;
		case 0x08:
			if (!bHoriz)
				s.Format(_T("The element is %d high."), d->val);
			break;
		case 0x22:
			s.Format(_T("The %s edge is %d from the middle of the parent (probably)."), name, d->val);
			break;
		case 0x01:
			s.Format(_T("The %s edge is at %d (fixed position, probably)."), name, d->val);
			break;
		}
		if (s.IsEmpty())
			s.Format(_T("Mode 0x%X, value %d (relative to the parent; this mode is not known)."), mode, d->val);
		else if (flag10)
			s.Append(_T(" Flag 0x10 is also set (meaning unknown)."));
	}
	else if (d->a1 == 1)
	{
		// reference 1 is the parent itself: the middle of it (the arrow of a row is centered this way)
		s.Format(_T("The %s edge is %d from the middle of the parent."), name, d->val);
	}
	else
	{
		// the mode names the edge of the other element the value is measured from: the vertical edges of all the firmwares use its top (0x100)
		// and bottom (0x300), the horizontal ones its left (0x200) and right (0x400)
		switch (d->a2)
		{
		case 0x100:
			s.Format(_T("The %s edge is %d from the top edge of %s."), name, d->val, (LPCTSTR)rel);
			break;
		case 0x200:
			s.Format(_T("The %s edge is %d from the left edge of %s."), name, d->val, (LPCTSTR)rel);
			break;
		case 0x300:
			s.Format(_T("The %s edge is %d from the bottom edge of %s.%s"), name, d->val, (LPCTSTR)rel, edge == 0 ? _T(" The element is placed below it.") : _T(""));
			break;
		case 0x400:
			s.Format(_T("The %s edge is %d from the right edge of %s.%s"), name, d->val, (LPCTSTR)rel, edge == 1 ? _T(" The element is placed to the right of it.") : _T(""));
			break;
		case 0x02:
			s.Format(_T("The %s edge is %d from the %s edge of %s."), name, d->val, name, (LPCTSTR)rel);
			break;
		case 0x01:
			s.Format(_T("The %s edge is at %d (fixed position, probably)."), name, d->val);
			break;
		}
		if (s.IsEmpty())
			s.Format(_T("Mode 0x%X, value %d, relative to %s (this mode is not known)."), d->a2, d->val, (LPCTSTR)rel);
	}
	return s;
}

CString CLayoutAttrs::ViewSummary(const resource_type_view_elem *e, std::vector<CCellLink> *pLinks) const
{
	CSummary out(this, e, m_pFw->GetFirmwareBuffer(), pLinks);
	CString kind = KindName(e);
	Extra x;
	ExtraOf(e, &x);
	int id = e->id;
	DWORD rel = 12;                    // the ID of the element is at byte 12, 2 bytes

	if (IsKind(e, " rtS"))
	{
		int sid = id != 0 ? id : (x.text >= 0 ? Small16(x.p[x.text]) : 0);
		DWORD sRel = id != 0 ? 12 : (DWORD)(76 + 4 * (x.text >= 0 ? x.text : 0));
		int sCol = id != 0 ? 3 : -1;
		CString text = (m_pNames && sid != 0) ? m_pNames->Flatten(m_pNames->StringText(sid)) : CString();
		CString what;
		what.Format(_T("The text shown: string %d."), sid);
		if (!text.IsEmpty())
		{
			out.Add(_T("Text "));
			out.Part(_T("'") + text + _T("'"), sRel, 2, sCol, what);
		}
		else if (sid != 0)
		{
			out.Add(_T("Text (string "));
			out.Num(sid, sRel, 2, sCol, what);
			out.Add(_T(")"));
		}
		else
			out.Add(_T("Text (set by the firmware)"));
		int ft = ViewFontType(e);
		if (ft)
		{
			out.Add(_T(", "));
			CString fwhat;
			fwhat = _T("Font defined in resource");
			out.Part(TypeSummary(FindType(ft)), (DWORD)(76 + 4 * x.text + 2), 2, 16, fwhat);
		}
		if (x.scroll >= 0)
			out.Add(_T(", scrolls when too long"));
	}
	else if (IsKind(e, "paMB"))
	{
		int w = 0, h = 0;
		CString size = (m_pNames && id != 0) ? m_pNames->ImageSize(id, &w, &h) : CString();
		if (id == 0)
			out.Add(_T("Bitmap (set by the firmware)"));
		else
		{
			out.Add(_T("Bitmap "));
			out.Num(id, rel, 2, 3, _T("The bitmap shown."));
			if (size.IsEmpty())
				out.Add(_T(" (not in this firmware)"));
			else
				out.Add(_T(", ") + size);
		}
		if (!x.gradient && x.n == 3 && x.p[2] == 1 && x.p[1] > 0 && x.p[1] < 1000)
		{
			// {time per picture in ms, height of one picture, 1}: the bitmap is a strip of pictures shown one after the other
			if (h > 0 && h % (int)x.p[1] == 0)
			{
				CString t;
				t.Format(_T(", animation of %d pictures "), h / (int)x.p[1]);
				out.Add(t);
			}
			else
				out.Add(_T(", animation: pictures "));
			out.Num((int)x.p[1], 80, 4, -1, _T("Height of one picture of the animation."));
			out.Add(_T(" high, "));
			out.Num((int)x.p[0], 76, 4, -1, _T("Time each picture is shown, in milliseconds."));
			out.Add(_T(" ms each"));
		}
	}
	else if (IsKind(e, "enoN"))
	{
		if (id == 0)
			out.Add(_T("Empty layout (spacer or container)"));
		else
		{
			CString desc = m_pNames ? m_pNames->View(id) : CString();
			CString what;
			what.Format(_T("The layout this element contains: layout %d."), id);
			if (!desc.IsEmpty())
				out.Part(desc, rel, 2, 3, what);                // what the layout holds; its number is in the ID column and in the tip
			else
			{
				out.Add(_T("Layout "));
				out.Num(id, rel, 2, 3, what);
			}
		}
	}
	else
	{
		out.Add(kind);
		if (id != 0)
		{
			out.Add(_T(" "));
			out.Num(id, rel, 2, 3, _T("The ID of the element."));
		}
	}
	if (x.gradient)
	{
		out.Add(_T(", gradient "));
		out.Part(ColorName(x.p[0]).SpanExcluding(_T(",")), 76, 4, 13, _T("Start color of the gradient."));
		out.Add(_T(" to "));
		out.Part(ColorName(x.p[1]).SpanExcluding(_T(",")), 80, 4, 14, _T("End color of the gradient."));
	}
	return out.text;
}

CString CLayoutAttrs::ViewTip(const resource_type_view_elem *e, int col) const
{
	CString s;
	Extra x;
	ExtraOf(e, &x);
	switch (col)
	{
	case 0:
		s.Format(_T("Size of row element in bytes, not including this column's value.\nFirst 72 bytes are type, ID, flags, and edges.\nAdditional %d bytes (%d values) are extra data which depends on the type of element."),
		         (int)e->size - 72 > 0 ? (int)e->size - 72 : 0, x.n);
		break;
	case 1:
		s.Format(_T("Class code %s. Elements of the same type share it."), (LPCTSTR)Hex(e->z0));
		break;
	case 2:
		s = _T("paMB = Bitmap\nrtS = String\nenoN = Layout (container or spacer)\nltnC = Control\ntStS = Static text\nwarD = Drawing\nmTtD = Date and time\nNumber = Custom element");
		break;
	case 3:
		if (IsKind(e, " rtS"))
		{
			int sid = e->id != 0 ? e->id : (x.text >= 0 ? Small16(x.p[x.text]) : 0);
			CString text = (m_pNames && sid != 0) ? m_pNames->StringText(sid) : CString();
			s = _T("ID of string shown by the element.");
			if (!text.IsEmpty())
				s += _T(" '") + text + _T("'");
			if (sid != 0)
				s += _T("\nDouble click to open the string.");
		}
		else if (IsKind(e, "paMB"))
			s = _T("ID of bitmap shown by the element.\nDouble click to open the bitmap.");
		else if (IsKind(e, "enoN"))
		{
			s = _T("ID of layout contained by the element. 0 = none.");
			if (e->id != 0)
				s += _T("\nDouble click to open the layout.");
		}
		else
			s = _T("No known function");
		break;
	case 4:
		s = _T("Not used: always 0.");
		break;
	case 5:
	case 6:
		{
			int v = col == 5 ? e->z1 : e->z2;
			CString p;
			if (v != 0 && !(col == 5 && v == -1))
				p = Probe(v, false);
			if (p.IsEmpty())
				s = _T("No known function");
			else
				s = _T("It may be: ") + p + _T(".");
		}
		break;
	case 7:
		s = _T("No known function");
		break;
	case 8:
		s = _T("No known function");
		break;
	case 9:
	case 10:
	case 11:
	case 12:
		{
			static const TCHAR *const edge[4] = { _T("left"), _T("top"), _T("right"), _T("bottom") };
			static const TCHAR *const Edge[4] = { _T("Left"), _T("Top"), _T("Right"), _T("Bottom") };
			const TCHAR *n = edge[col - 9];
			s.Format(_T("Position value of the %s edge.\nDefined by parent's %s edge. %s mode can change the reference position."), n, n, Edge[col - 9]);
		}
		break;
	case 18:      // the descriptors of the four edges (reference / mode / flag, as stored just before each position value)
	case 19:
	case 20:
	case 21:
		{
			static const TCHAR *const Edge[4] = { _T("Left"), _T("Top"), _T("Right"), _T("Bottom") };
			s.Format(_T("%s position mode\nReference/Mode/Flag\n\n")
			         _T("References:\n")
			         _T("0 = parent, 1 = middle of parent, other = ID of another element in parent\n")
			         _T("Modes:\n")
			         _T("2 = distance from an edge, 4 = width, 8 = height\n")
			         _T("0x22 = distance from middle of parent (probably), 1 = fixed position (probably)\n")
			         _T("0x100 = top, 0x200 = left, 0x300 = bottom, 0x400 = right\n")
			         _T("Mode defines which edge of the referenced element is used.\n")
			         _T("Modes 4 and 8 also appear as 0x14 and 0x18 (meaning unknown).\n")
			         _T("Flags:\n")
			         _T("Flag 0x8000 = parent layout, 0x0000 = set by mode"), Edge[col - 18]);
		}
		break;
	case 13:
	case 14:
		if (x.gradient)
		{
			DWORD c = x.p[col == 13 ? 0 : 1];
			s.Format(_T("%s color of the gradient this element is filled with: %s (stored as 0xAABBGGRR at byte %d).\nDouble click to choose another color."),
			         col == 13 ? _T("Start") : _T("End"), (LPCTSTR)ColorName(c), col == 13 ? 76 : 80);
		}
		else
			s = _T("This element has no gradient colors.");
		break;
	case 15:
		if (x.n == 0)
			s = _T("No extra data.");
		else
		{
			s.Format(_T("%d extra values after the fixed part:"), x.n);
			for (int i = 0; i < x.n; i++)
				s.AppendFormat(_T(" %X"), x.p[i]);
			if (x.gradient)
				s += _T("\nThe first two are a gradient.");
			if (x.text >= 0)
				s.AppendFormat(_T("\nValue %d is {string number %d, font type %d} followed by two flag values."), x.text + 1, Small16(x.p[x.text]), Small16(x.p[x.text] >> 16));
			if (x.scroll >= 0)
				s.AppendFormat(_T("\nValues %d to %d are probably the scrolling of text that is too long: %d pixels of space between the repeats, %d ms before it starts, %d pixels per step, %d ms per step."),
				               x.scroll + 1, x.scroll + 4, (int)x.p[x.scroll], (int)x.p[x.scroll + 1], (int)x.p[x.scroll + 2], (int)x.p[x.scroll + 3]);
			{
				for (int i = (x.gradient ? 2 : 0); i < x.n; i++)
				{
					if (i >= x.text && i < x.text + 3 && x.text >= 0)
						continue;
					if (x.scroll >= 0 && i >= x.scroll && i < x.scroll + 4)
						continue;
					if (x.p[i] == 0)
						continue;
					CString p = Probe((int)x.p[i], true);
					if (p.IsEmpty() && (x.p[i] >> 16) != 0 && (x.p[i] >> 16) != 0xFFFF)
					{
						CString lo = Probe(Small16(x.p[i]), true);
						CString hi = Probe(Small16(x.p[i] >> 16), true);
						if (!lo.IsEmpty() || !hi.IsEmpty())
							p = (lo.IsEmpty() ? CString() : (_T("low half: ") + lo + _T(" ")) ) + (hi.IsEmpty() ? CString() : (_T("high half: ") + hi));
					}
					if (!p.IsEmpty())
					{
						s.AppendFormat(_T("\nValue %d (0x%X) may be: "), i + 1, x.p[i]);
						s += p;
					}
				}
			}
			if (x.n == 3 && !x.gradient && IsKind(e, "paMB") && x.p[2] == 1)
				s.AppendFormat(_T("\nA bitmap with the values {time, height, 1} is an animation: a strip of pictures %d high, shown %d ms each."), (int)x.p[1], (int)x.p[0]);
		}
		break;
	case 16:
		{
			int ft = ViewFontType(e);
			if (ft)
				s.Format(_T("Font type %d: %s.\nDouble click to open it."), ft, (LPCTSTR)TypeSummary(FindType(ft)));
			else if (IsKind(e, " rtS"))
				s = _T("This text uses the default font.");
			else
				s = _T("Only text has a font.");
		}
		break;
	}
	return s;
}
