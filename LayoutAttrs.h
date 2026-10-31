// LayoutAttrs.h - what the numbers in layouts, menus and font types say.
//
// The entries of the Layout tab are lists of numbers: sizes, IDs, flags. Their meaning depends on what they
// belong to (a string ID in one entry is a bitmap ID in the next, one number is a position, another a colour),
// so this class looks at each value in its context and describes it: which string or bitmap an ID points to,
// which function a command is, what a position or a colour is, what the bits of a flag word are. It is used
// for the tooltips and the "Summary" columns. Nothing here writes to the firmware.
//
// The summaries know which parts of their text are numbers stored in the entry. They hand those parts out as
// links (see CCellLink): the list frames them, and a click on one shows the bytes the number is stored in.
//
// Released under the GNU General Public License v2, like the rest of iPodWizard.

#pragma once

#include <vector>
#include <map>
#include "ToolTipListCtrl.h"

class CFirmware;
class CLayoutNames;
class CCommandTable;

class CLayoutAttrs
{
public:
	CLayoutAttrs();

	void Build(CFirmware *pFirmware, CLayoutNames *pNames, CCommandTable *pCommands);

	// ---- font types (16 bytes)
	CString StyleName(int style) const;                     // "Regular", "Bold"
	static CString AlignName(int align);                    // "Left", "Center", "Right"
	static CString ColorName(DWORD argb);                   // "#2A2929, opaque" ...
	CString TypeSummary(const struct resource_type_type_elem *e) const;
	CString TypeTip(const struct resource_type_type_elem *e, int col) const;
	const struct resource_type_type_elem *FindType(int id) const;

	// Where a font type is used: the texts of layouts and the entries of menus that name it
	enum { TU_TEXT = 0, TU_MENU = 1, TU_LIST = 2 };
	struct TypeUse
	{
		int kind;                    // TU_TEXT or TU_MENU
		int id;                      // the layout / the menu
		int row;                     // its place in the list of that layout / menu (counting from 0)
		int stringId;                // text: the string it shows (0: none)
		DWORD offset;                // where the font type number is stored in the firmware image
		DWORD size;                  // 2 bytes (text) or 4 bytes (menu entry)
		CString text;                // what it says
		bool bImplicit;              // a menu entry that names no font type: it takes the one of the text of its menu's layout
		TypeUse() : kind(0), id(0), row(0), stringId(0), offset(0), size(0), bImplicit(false) {}
	};
	const std::vector<TypeUse> &TypeUses(int typeId) const;
	const std::vector<TypeUse> &MenuUses(int menuId) const;           // the lists of layouts that show a menu (kind TU_LIST)
	CString TypeUseText(const TypeUse &u, int *pLinkStart, int *pLinkLen) const;     // "layout 10371, row 4 - 'Now Playing'"
	CString TypeUseTip(const TypeUse &u, int typeId) const;

	// ---- menus (the list has one entry per row)
	// Entries come in two layouts of the same 40 bytes; which one a firmware uses is worked out once.
	bool MenuHasShiftedFields() const { return m_bMenuShifted; }
	void MenuField(int col, DWORD *pOffset, DWORD *pSize) const;     // where the value of a list column is (offset, bytes)
	int  MenuTextId(const struct resource_type_menu_elem *e) const;
	int  MenuTag(const struct resource_type_menu_elem *e) const;
	CString MenuSummary(int menuId, int item, const struct resource_type_menu_elem *e, std::vector<CCellLink> *pLinks = NULL) const;
	CString MenuTip(const struct resource_type_menu_elem *e, int col, int menuId = 0) const;
	// The font type the entries of a menu use when they name none (their font type is 0): the font of the text in the menu's
	// layout (the layout with the ID of the menu, or the layouts it contains). 0: not known.
	int MenuDefaultFont(int menuId) const;
	int LayoutFont(const std::map<int, const struct _ViewRecord *> &byId, int layoutId, int depth, std::vector<int> &seen) const;

	// ---- layouts
	struct Extra
	{
		int n;                       // number of extra 32 bit values behind the fixed part
		const DWORD *p;
		bool gradient;               // p[0] and p[1] are the two colours of a gradient
		int text;                    // index of {string ID, font type ID}, followed by two flag values (-1: none)
		int scroll;                  // index of four scrolling values (-1: none)
	};
	static void ExtraOf(const struct resource_type_view_elem *e, Extra *x);
	static CString KindName(const struct resource_type_view_elem *e);
	static CString RawKind(const struct resource_type_view_elem *e);               // the four characters of the type as they are stored ("enoN")
	static bool IsKind(const struct resource_type_view_elem *e, const char *tag);
	CString ViewSummary(const struct resource_type_view_elem *e, std::vector<CCellLink> *pLinks = NULL) const;
	CString ViewTip(const struct resource_type_view_elem *e, int col) const;
	CString EdgeText(const struct resource_type_view_elem *e, int edge) const;     // 0 top, 1 left, 2 bottom, 3 right
	int  ViewFontType(const struct resource_type_view_elem *e) const;               // font type ID of a text, or 0
	bool ViewColorOffsets(const struct resource_type_view_elem *e, DWORD *pOffset1, DWORD *pOffset2) const;

	// What a number may be the ID of ("string 12 'xyz'; bitmap 12"), empty when nothing has that ID
	CString Probe(int value, bool bSkipSmall = true) const;
	CString FunctionText(unsigned int ref, const TCHAR *what) const;                // text for a command reference

	// Where a value is stored: "Stored at 0x0012F4A0 as 6B 05 (low byte first, so it reads 0x56B = 1387)"
	CString StoredTip(DWORD offset, DWORD size, bool bigEndian) const;

private:
	CFirmware *m_pFw;
	CLayoutNames *m_pNames;
	CCommandTable *m_pCmds;
	bool m_bMenuShifted;
	bool m_bOtf;
	CString DescribeBits(DWORD value, const TCHAR *name) const;
	std::map<int, std::vector<TypeUse> > m_MenuUses;         // menu ID -> the lists of layouts that show it
	std::map<int, std::vector<TypeUse> > m_TypeUses;         // font type ID -> where it is used
	std::map<int, int> m_MenuFont;                           // menu ID -> the font type its entries use by default
	std::vector<TypeUse> m_NoUses;
};
