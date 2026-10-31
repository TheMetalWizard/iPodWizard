// LayoutNames.cpp - descriptive names for the entries of the Layout tab. See LayoutNames.h.

#include "stdafx.h"
#include "iPodWizard.h"
#include "Firmware.h"
#include "Picture.h"
#include "Font.h"
#include "OTFFontParser.h"
#include "LayoutNames.h"

namespace
{
	const int kMaxDepth = 8;

	bool IsType(const resource_type_view_elem *e, const char *tag)
	{
		return strncmp(e->type, tag, 4) == 0;
	}
}

CLayoutNames::CLayoutNames()
	: m_pFw(NULL)
{
}

CString CLayoutNames::Label(int id, const CString &description)
{
	CString s;
	s.Format(_T("%d"), id);
	if (!description.IsEmpty())
	{
		s += _T(" - ");
		s += description;
	}
	return s;
}

void CLayoutNames::Build(CFirmware *pFirmware)
{
	m_pFw = pFirmware;
	m_Views.clear();
	m_Menus.clear();
	m_ViewParents.clear();
	m_Strings.clear();
	m_SubmenuName.clear();
	m_TitleBar.clear();
	m_TitleBarItems.clear();
	m_ScreenWidths.clear();
	m_Pictures.clear();
	m_Types.clear();
	m_BmpFontKeys.clear();
	m_OtfFontKeys.clear();
	m_ViewCache.clear();
	m_MenuCache.clear();
	m_ViewBusy.clear();
	if (m_pFw == NULL)
		return;

	// The fonts the font types can name: a type that names no bitmap font but an OpenType one is labelled OTF.
	// Bitmap fonts: Regular 0 / Bold 1. OpenType fonts: Bold 0 / Regular 1.
	{
		LPBYTE lpEnd = m_pFw->GetFirmwareBuffer() + m_pFw->GetFirmwareSize();
		CIpodFont fnt;
		for (DWORD k = 0; k < m_pFw->GetNumFonts(); k++)
			if (fnt.Read(m_pFw->GetFont(k), lpEnd) == TRUE)
			{
				CString key;
				key.Format(_T("%s|%d|%d"), fnt.GetFontName(), (int)fnt.GetFontSize(), (int)fnt.GetFontStyleID());
				m_BmpFontKeys.insert(key);
			}
		COTFFont ofnt;
		for (DWORD k = 0; k < m_pFw->GetNumOTFFonts(); k++)
			if (ofnt.Read(m_pFw->GetOTFFont(k), TRUE))
			{
				int style = -1;
				if (!wcscmp(ofnt.GetFontStyle(), _T("Bold")))
					style = 0;
				else if (!wcscmp(ofnt.GetFontStyle(), _T("Regular")))
					style = 1;
				CString key;
				key.Format(_T("%s|%d|%d"), ofnt.GetFontName(), (int)ofnt.GetFontSize(), style);
				m_OtfFontKeys.insert(key);
			}
	}

	// Strings: the last language block, the one the "jump to string" feature of the Layout tab uses too
	DWORD nLang = m_pFw->GetNumLangs();
	if (nLang > 0)
	{
		DWORD L = nLang - 1;
		DWORD n = m_pFw->GetNumLangStrings(L);
		for (DWORD i = 0; i < n; i++)
		{
			const char *raw = (const char *)m_pFw->GetLangString(L, i);
			DWORD size = m_pFw->GetLangStringSize(L, i);
			if (raw == NULL || size == 0 || size > 1000)
				continue;
			char tmp[1024];
			memcpy(tmp, raw, size);
			tmp[size] = 0;
			WCHAR wide[1024];
			if (MultiByteToWideChar(CP_UTF8, 0, tmp, -1, wide, 1024) > 0)
				m_Strings[(int)m_pFw->GetLangStringID(L, i)] = wide;
		}
	}

	ViewTable *pViews = m_pFw->GetViewList();
	MenuTable *pMenus = m_pFw->GetMenuList();

	for (DWORD i = 0; i < m_pFw->GetNumPictures(); i++)
		if (m_pFw->GetPictureType(i) == 0)
			m_Pictures.insert((int)m_pFw->GetPictureID(i));
	TypeTable *pTypeList = m_pFw->GetTypeList();
	if (pTypeList)
		for (size_t i = 0; i < pTypeList->size(); i++)
			m_Types.insert(pTypeList->at(i)->id);

	if (pMenus)
		for (size_t i = 0; i < pMenus->size(); i++)
			m_Menus[pMenus->at(i)->id] = pMenus->at(i);

	if (pViews)
	{
		for (size_t i = 0; i < pViews->size(); i++)
			m_Views[pViews->at(i)->id] = pViews->at(i);

		for (size_t i = 0; i < pViews->size(); i++)
			for (int k = 0; k < pViews->at(i)->num_elems; k++)
			{
				const resource_type_view_elem *e = pViews->at(i)->ViewElemList[k]->element;
				if (IsType(e, "enoN") && e->id != pViews->at(i)->id && m_Views.count(e->id))
					m_ViewParents[e->id]++;
			}
	}

	// Menu entries that open another menu name that menu
	if (pMenus)
		for (size_t i = 0; i < pMenus->size(); i++)
			for (int k = 0; k < pMenus->at(i)->num_elems; k++)
			{
				const resource_type_menu_elem *e = pMenus->at(i)->MenuElemList[k]->element;
				if (e->submenuID != 0 && m_Menus.count(e->submenuID) && !m_SubmenuName.count(e->submenuID))
				{
					CString t = Flatten(StringText(e->textID));
					if (!t.IsEmpty())
						m_SubmenuName[e->submenuID] = t;
				}
			}

	// The title bar: a layout nobody holds which contains one full-screen layout, which in turn holds a thin
	// strip along the top (the title bar) next to the large area all the screens live in.
	if (pViews)
		for (size_t i = 0; i < pViews->size(); i++)
		{
			ViewRecord *pRoot = pViews->at(i);
			if (m_ViewParents.count(pRoot->id))
				continue;
			const resource_type_view_elem *only = NULL;
			int nKids = 0;
			for (int k = 0; k < pRoot->num_elems; k++)
			{
				const resource_type_view_elem *e = pRoot->ViewElemList[k]->element;
				if (IsType(e, "enoN") && m_Views.count(e->id))
				{
					only = e;
					nKids++;
				}
			}
			if (nKids != 1 || only->right.val <= 0 || only->bottom.val <= 0)
				continue;
			m_ScreenWidths.insert(only->right.val);
			ViewRecord *pScreen = m_Views[only->id];
			std::vector<const resource_type_view_elem *> kids;
			for (int k = 0; k < pScreen->num_elems; k++)
			{
				const resource_type_view_elem *e = pScreen->ViewElemList[k]->element;
				if (IsType(e, "enoN") && m_Views.count(e->id))
					kids.push_back(e);
			}
			if (kids.size() >= 2 && kids[0]->left.val == 0 && kids[0]->top.val == 0 && kids[0]->right.val == 0 &&
			    kids[0]->bottom.val > 0 && kids[0]->bottom.val <= 32 && kids[1]->bottom.val == 0)
			{
				m_TitleBar.insert(kids[0]->id);
				ViewRecord *pBar = m_Views[kids[0]->id];
				for (int k = 0; k < pBar->num_elems; k++)
				{
					const resource_type_view_elem *e = pBar->ViewElemList[k]->element;
					if (IsType(e, "enoN") && m_Views.count(e->id))
						m_TitleBarItems.insert(e->id);
				}
			}
		}

	// Later iPods build their screens differently. There the title bar is a strip placed along the top of
	// the screen whose background is a bitmap as wide as the screen. If strips nest, the outermost one counts.
	if (pViews && !m_ScreenWidths.empty())
	{
		std::set<int> candidates;
		std::map<int, std::vector<int> > holders;
		for (size_t i = 0; i < pViews->size(); i++)
			for (int k = 0; k < pViews->at(i)->num_elems; k++)
			{
				const resource_type_view_elem *e = pViews->at(i)->ViewElemList[k]->element;
				if (!IsType(e, "enoN") || e->id == pViews->at(i)->id || !m_Views.count(e->id))
					continue;
				holders[e->id].push_back(pViews->at(i)->id);
				if (e->left.val == 0 && e->top.val == 0 && e->right.val == 0 && e->bottom.val > 0 && e->bottom.val <= 32 &&
				    !candidates.count(e->id) && HasBanner(e->id, 0))
					candidates.insert(e->id);
			}
		for (std::set<int>::const_iterator ci = candidates.begin(); ci != candidates.end(); ++ci)
		{
			bool inner = false;
			const std::vector<int> &hv = holders[*ci];
			for (size_t h = 0; h < hv.size(); h++)
				if (candidates.count(hv[h]))
					inner = true;
			if (inner)
				continue;
			m_TitleBar.insert(*ci);
			ViewRecord *pBar = m_Views[*ci];
			for (int k = 0; k < pBar->num_elems; k++)
			{
				const resource_type_view_elem *e = pBar->ViewElemList[k]->element;
				if (IsType(e, "enoN") && e->id != *ci && m_Views.count(e->id))
					m_TitleBarItems.insert(e->id);
			}
		}
	}
}

// Does the layout (or one of the layouts directly inside it) draw a bitmap as wide as a screen and thin enough
// to be a bar? That is the background of the title bar.
bool CLayoutNames::HasBanner(int viewId, int depth)
{
	std::map<int, struct _ViewRecord *>::const_iterator vit = m_Views.find(viewId);
	if (vit == m_Views.end())
		return false;
	ViewRecord *pView = vit->second;
	for (int k = 0; k < pView->num_elems; k++)
	{
		const resource_type_view_elem *e = pView->ViewElemList[k]->element;
		if (IsType(e, "paMB"))
		{
			int w, h;
			if (!ImageSize(e->id, &w, &h).IsEmpty() && m_ScreenWidths.count(w) && h > 0 && h <= 40)
				return true;
		}
		else if (depth < 1 && IsType(e, "enoN") && e->id != viewId && m_Views.count(e->id) && HasBanner(e->id, depth + 1))
			return true;
	}
	return false;
}

CString CLayoutNames::StringText(int id)
{
	std::map<int, CString>::const_iterator it = m_Strings.find(id);
	return it == m_Strings.end() ? CString() : it->second;
}

CString CLayoutNames::Flatten(const CString &src)
{
	CString s = src;
	s.Replace(_T('\r'), _T(' '));
	s.Replace(_T('\n'), _T(' '));
	s.Replace((TCHAR)0xA0, _T(' '));
	s.Trim();
	return s;
}

// Every entry of the menu, by its text
CString CLayoutNames::MenuItems(struct _MenuRecord *pMenu)
{
	CString out;
	for (int k = 0; k < pMenu->num_elems; k++)
	{
		const resource_type_menu_elem *e = pMenu->MenuElemList[k]->element;
		CString t = MenuEntryText(e);
		if (t.IsEmpty())
			continue;
		if (!out.IsEmpty())
			out += _T(", ");
		out += t;
	}
	return out;
}

// The text of a menu entry. Entries name their text twice (the second one is always the one that is filled in)
CString CLayoutNames::MenuEntryText(const struct resource_type_menu_elem *e)
{
	CString t = Flatten(StringText(e->textID));
	if (t.IsEmpty())
		t = Flatten(StringText(e->textID2));
	return t;
}

const TCHAR *CLayoutNames::KindName(int kind)
{
	switch (kind)
	{
	case MATCH_STRING: return _T("string");
	case MATCH_BITMAP: return _T("bitmap");
	case MATCH_VIEW:   return _T("layout");
	case MATCH_MENU:   return _T("menu");
	case MATCH_TYPE:   return _T("font type");
	}
	return _T("?");
}

void CLayoutNames::Probe(int value, std::vector<Match> &out)
{
	out.clear();
	if (m_pFw == NULL)
		return;
	std::map<int, CString>::const_iterator sit = m_Strings.find(value);
	if (sit != m_Strings.end())
	{
		Match m;
		m.kind = MATCH_STRING;
		m.id = value;
		m.text = _T("'") + Flatten(sit->second) + _T("'");
		out.push_back(m);
	}
	if (m_Pictures.count(value))
	{
		Match m;
		m.kind = MATCH_BITMAP;
		m.id = value;
		int w, h;
		m.text = ImageSize(value, &w, &h);
		out.push_back(m);
	}
	if (m_Views.count(value))
	{
		Match m;
		m.kind = MATCH_VIEW;
		m.id = value;
		m.text = View(value);
		out.push_back(m);
	}
	if (m_Menus.count(value))
	{
		Match m;
		m.kind = MATCH_MENU;
		m.id = value;
		m.text = Menu(value);
		out.push_back(m);
	}
	if (m_Types.count(value))
	{
		Match m;
		m.kind = MATCH_TYPE;
		m.id = value;
		m.text = Type(value);
		out.push_back(m);
	}
}

CString CLayoutNames::Menu(int id)
{
	std::map<int, CString>::const_iterator cached = m_MenuCache.find(id);
	if (cached != m_MenuCache.end())
		return cached->second;
	std::map<int, struct _MenuRecord *>::const_iterator it = m_Menus.find(id);
	if (it == m_Menus.end())
		return CString();
	CString items = MenuItems(it->second);
	std::map<int, CString>::const_iterator sub = m_SubmenuName.find(id);
	CString s;
	if (sub != m_SubmenuName.end())
		s = sub->second + _T(" menu");
	else
		s = _T("Menu");
	if (!items.IsEmpty())
		s += _T(": ") + items;
	m_MenuCache[id] = s;
	return s;
}

CString CLayoutNames::Type(int id)
{
	if (m_pFw == NULL)
		return CString();
	TypeTable *pTypes = m_pFw->GetTypeList();
	if (pTypes == NULL)
		return CString();
	for (size_t i = 0; i < pTypes->size(); i++)
		if (pTypes->at(i)->id == id)
		{
			const resource_type_type_elem *e = pTypes->at(i)->element;
			CString name = Flatten(StringText(e->fontID));
			if (name.IsEmpty())
				return CString();
			CString s;
			s.Format(_T("%s %dpt"), (LPCTSTR)name, e->size);
			CString key;
			key.Format(_T("%s|%d|%d"), (LPCTSTR)StringText(e->fontID), (int)e->size, (int)e->style);
			bool bOtf = m_BmpFontKeys.find(key) == m_BmpFontKeys.end() && m_OtfFontKeys.find(key) != m_OtfFontKeys.end();
			if (bOtf)
			{
				s += _T(" OTF");
				if (e->style == 0)
					s += _T(" bold");
			}
			else if (e->style == 1)
				s += _T(" bold");
			if (e->alignment == 1)
				s += _T(", centered");
			else if (e->alignment == 2)
				s += _T(", right");
			return s;
		}
	return CString();
}

CString CLayoutNames::ImageSize(int pictureId, int *pw, int *ph)
{
	*pw = *ph = 0;
	if (m_pFw == NULL || pictureId == 0)
		return CString();
	DWORD idx = m_pFw->GetPictureIndexFromID((DWORD)pictureId);
	if (idx == 0xFFFFFFFF)
		return CString();
	CPicture pic;
	pic.SetPictureMode(m_pFw->GetPictureType(idx) != 0 ? 1 : 0);
	if (!pic.Read(m_pFw->GetPicture(idx), FALSE, m_pFw->GetAUPDBuffer()))
		return CString();
	CSize sz = pic.GetPictureSize();
	*pw = sz.cx;
	*ph = sz.cy;
	CString s;
	s.Format(_T("%dx%d"), sz.cx, sz.cy);
	return s;
}

CString CLayoutNames::View(int id)
{
	return DescribeView(id, 0);
}

CString CLayoutNames::DescribeView(int id, int depth)
{
	std::map<int, CString>::const_iterator it = m_ViewCache.find(id);
	if (it != m_ViewCache.end())
		return it->second;
	if (depth > kMaxDepth || m_ViewBusy.count(id))
		return CString();
	m_ViewBusy.insert(id);
	CString s = DescribeViewCore(id, depth);
	m_ViewBusy.erase(id);
	if (depth == 0)
		m_ViewCache[id] = s;
	return s;
}

CString CLayoutNames::DescribeViewCore(int id, int depth)
{
	std::map<int, struct _ViewRecord *>::const_iterator vit = m_Views.find(id);
	if (vit == m_Views.end())
		return CString();
	ViewRecord *pView = vit->second;

	if (m_TitleBar.count(id))
		return _T("Title bar");

	CString menu = Menu(id);
	CString core;

	int nKids = 0, nStr = 0, nBmp = 0, nCtl = 0, nOther = 0;
	int firstKid = 0;
	const resource_type_view_elem *screenKid = NULL;
	std::vector<int> bmpIds;
	CString firstText;
	for (int k = 0; k < pView->num_elems; k++)
	{
		const resource_type_view_elem *e = pView->ViewElemList[k]->element;
		if (IsType(e, "enoN"))
		{
			if (e->id != id && m_Views.count(e->id))
			{
				if (nKids == 0)
				{
					firstKid = e->id;
					screenKid = e;
				}
				nKids++;
			}
		}
		else if (IsType(e, " rtS"))
		{
			nStr++;
			if (firstText.IsEmpty())
				firstText = Flatten(StringText(e->id));
		}
		else if (IsType(e, "paMB"))
		{
			nBmp++;
			bmpIds.push_back(e->id);
		}
		else if (IsType(e, "ltnC"))
			nCtl++;
		else
			nOther++;
	}

	if (!menu.IsEmpty())
		core = menu;
	else if (!m_ViewParents.count(id) && nKids == 1 && nStr + nBmp + nCtl + nOther == 0 &&
	         screenKid->right.val > 0 && screenKid->bottom.val > 0)
		core.Format(_T("Screen: %dx%d"), screenKid->right.val, screenKid->bottom.val);
	else if (nKids == 1 && nStr + nBmp + nCtl + nOther == 0)
		core = DescribeView(firstKid, depth + 1);              // a plain wrapper: it is what it wraps
	else if (nKids == 0 && nBmp == 0 && nCtl == 0 && nOther == 0 && nStr == 1 && !firstText.IsEmpty())
		core = _T("Text: '") + firstText + _T("'");
	else if (nKids == 0 && nStr == 0 && nCtl == 0 && nOther == 0 && nBmp > 0)
	{
		std::map<CString, int> sizes;
		CString best;
		int bestN = 0;
		for (size_t i = 0; i < bmpIds.size(); i++)
		{
			int w, h;
			CString sz = ImageSize(bmpIds[i], &w, &h);
			if (sz.IsEmpty())
				continue;
			int n = ++sizes[sz];
			if (n > bestN)
			{
				bestN = n;
				best = sz;
			}
		}
		int known = 0;
		for (std::map<CString, int>::const_iterator si = sizes.begin(); si != sizes.end(); ++si)
			known += si->second;
		if (best.IsEmpty() || known != nBmp)         // some bitmaps are not in the firmware: only the count is certain
		{
			CString t;
			t.Format(_T("%d %s"), nBmp, nBmp == 1 ? _T("image") : _T("images"));
			core = t;
		}
		else if (sizes.size() == 1)
		{
			CString t;
			if (nBmp == 1)
				t.Format(_T("Image: %s"), (LPCTSTR)best);
			else
				t.Format(_T("%d images: %s"), nBmp, (LPCTSTR)best);
			core = t;
		}
		else
		{
			CString t;
			t.Format(_T("%d images"), nBmp);
			core = t;
		}
	}
	else
	{
		// Anything else: say what it holds, as counts ("2 layouts", "1 text, 1 image")
		CString parts;
		struct { int n; const TCHAR *one; const TCHAR *many; } list[] = {
			{ nKids, _T("layout"), _T("layouts") },
			{ nStr,  _T("text"),   _T("texts")   },
			{ nBmp,  _T("image"),  _T("images")  },
			{ nCtl,  _T("control"), _T("controls") },
			{ nOther, _T("other item"), _T("other items") },
		};
		for (int i = 0; i < 5; i++)
			if (list[i].n > 0)
			{
				CString t;
				t.Format(_T("%d %s"), list[i].n, list[i].n == 1 ? list[i].one : list[i].many);
				if (!parts.IsEmpty())
					parts += _T(", ");
				parts += t;
			}
		if (!parts.IsEmpty())
			core = parts;
	}

	if (m_TitleBarItems.count(id))
		return core.IsEmpty() ? CString(_T("Title bar item")) : _T("Title bar: ") + core;
	return core;
}
