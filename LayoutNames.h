// LayoutNames.h - descriptive names for the entries of the Layout tab.
//
// The Layout tab lists scheme layouts, menus and font types by their numeric ID only. This class works
// out a short description for each of them from what they contain and what they refer to (menu entries,
// strings, bitmaps, fonts and the way layouts are nested in each other), so the list can read
// "10371 - Title bar: ..." instead of just "10371". The ID itself is never changed; the description is
// only ever appended. Nothing here writes to the firmware.
//
// The descriptions are a best effort: layouts whose purpose cannot be told from their contents are
// summarised by what they hold, as counts (for example "2 layouts" or "1 text, 1 image").
//
// Released under the GNU General Public License v2, like the rest of iPodWizard.

#pragma once

#include <map>
#include <set>
#include <vector>

class CFirmware;

class CLayoutNames
{
public:
	CLayoutNames();

	void Build(CFirmware *pFirmware);              // call whenever a firmware has been loaded

	CString View(int id);                          // description of a scheme layout (empty when nothing can be said)
	CString Menu(int id);                          // description of a menu
	CString Type(int id);                          // description of a font type

	// "ID" or "ID - description": the text to show in the tree
	static CString Label(int id, const CString &description);

	CString StringText(int id);                    // text of a string (English block), empty when there is none
	CString Flatten(const CString &s);             // on one line: line breaks and non-breaking spaces become spaces
	CString MenuEntryText(const struct resource_type_menu_elem *e);               // what a menu entry says
	CString ImageSize(int pictureId, int *pw, int *ph);

	// A number found in a script or in a layout may be the ID of several kinds of resources. Probe lists every
	// resource the number is an ID of.
	enum MatchKind { MATCH_STRING = 1, MATCH_BITMAP, MATCH_VIEW, MATCH_MENU, MATCH_TYPE };
	struct Match
	{
		int kind;
		int id;
		CString text;                              // what the resource is, in a few words
	};
	void Probe(int value, std::vector<Match> &out);
	static const TCHAR *KindName(int kind);

private:
	CFirmware *m_pFw;
	std::map<int, struct _ViewRecord *> m_Views;
	std::map<int, struct _MenuRecord *> m_Menus;
	std::map<int, int> m_ViewParents;              // layout ID -> how many layouts hold it
	std::map<int, CString> m_Strings;              // string ID -> text (English block)
	std::map<int, CString> m_SubmenuName;          // menu ID -> text of the menu entry that opens it
	std::set<int> m_TitleBar, m_TitleBarItems;
	std::set<int> m_ScreenWidths;                  // widths of the screens the firmware draws on
	std::set<int> m_Pictures, m_Types;             // IDs of the bitmaps and font types
	std::set<CString> m_BmpFontKeys, m_OtfFontKeys; // "name|size|style" of the bitmap fonts and the OpenType fonts (style numbers as the font types use them)
	bool HasBanner(int viewId, int depth);
	std::map<int, CString> m_ViewCache;
	std::map<int, CString> m_MenuCache;
	std::set<int> m_ViewBusy;

	CString MenuItems(struct _MenuRecord *pMenu);
	CString DescribeView(int id, int depth);
	CString DescribeViewCore(int id, int depth);
};
