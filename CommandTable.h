// CommandTable.h - the command table as the Layout tab shows it.
//
// The command table is the compiled script the iPod's user interface runs on (see PixoScript.h). This class
// reads it from a loaded firmware and works out how its functions are tied to the rest of the interface:
// which menu entries run them, which functions call them, which ones the firmware starts itself. That is what
// the descriptions in the tree and the "used by" lines of the Layout tab are built from.
// Nothing here writes to the firmware.
//
// Released under the GNU General Public License v2, like the rest of iPodWizard.

#pragma once

#include "PixoScript.h"
#include <map>
#include <set>
#include <vector>

class CFirmware;
class CLayoutNames;

class CCommandTable
{
public:
	CCommandTable();

	void Build(CFirmware *pFirmware, CLayoutNames *pNames);       // call whenever a firmware has been loaded
	BOOL IsValid() const { return m_Script.IsValid(); }
	PixoScript::CScript &Script() { return m_Script; }

	// Where the resource lives in the firmware image
	DWORD GetResourceOffset() const { return m_dwOffset; }
	DWORD GetResourceSize() const { return m_Script.GetResourceSize(); }
	DWORD GetFunctionOffset(const PixoScript::Function &f) const { return m_dwOffset + f.recOffset; }
	DWORD GetCodeOffset(const PixoScript::Function &f) const { return m_dwOffset + m_Script.GetCodeOffset(f); }

	int  GetNumFunctions() const { return (int)m_SortedIds.size(); }
	int  GetFunctionId(int rank) const { return m_SortedIds[rank]; }            // IDs in ascending order
	const PixoScript::Function *FindFunction(int id) const;

	// How a function is tied to the rest of the interface
	enum UseKind
	{
		USE_ENTRY = 1,                    // the firmware calls it itself
		USE_SELECT,                       // run when a menu entry is chosen (id: menu, item: entry number)
		USE_HILITE,                       // run when a menu entry is highlighted
		USE_CALLER                        // called by another function (id: that function)
	};
	struct Use
	{
		int kind;
		int id;                           // menu / calling function
		int item;                         // menu entry (counting from 0)
		DWORD addr;                       // where the reference is stored in the firmware image (0xFFFFFFFF: unknown)
		DWORD len;                        // number of bytes there
	};
	const std::vector<Use> &GetUses(int functionId) const;
	const std::vector<int> &GetCallees(int functionId) const;                  // functions it refers to, ascending
	// the places inside the code of `caller` (offsets of instructions) that name `callee`
	const std::vector<int> &GetSites(int caller, int callee) const;

	// Short descriptions
	CString Name(int functionId) const;                                         // known name, empty when there is none
	CString Role(int functionId) const;                                         // how it is used, empty when it is not
	CString Label(int functionId) const;                                        // "ID" or "ID - name/role", for the tree
	CString Signature(int functionId) const;                                    // "a2070(): 0 parameters, 0 variables, 25 bytes"
	CString MenuEntryText(int menuId, int item) const;

	// Names for atoms (functions, objects, methods). Firmwares only carry them in a development build; the rest
	// can be given names here: one "atom<TAB>name" per line.
	BOOL LoadNames(LPCTSTR path);
	BOOL LoadNamesFor(LPCTSTR path, DWORD fingerprint);                        // the section of a names file that belongs to this table
	int  GetNumNames() const { return (int)m_Names.size(); }
	BOOL NamesAreInferred() const { return m_bNamesInferred; }                  // names came from the names file, not from the firmware
	DWORD Fingerprint() const;                                                  // CRC-32 of the table of functions: how a names file finds its section

	// Function references: the jsval-like value menus use to name a command is (ID << 2) | 2.
	static int  RefToFunction(unsigned int value);                              // function ID, or -1
	static unsigned int FunctionToRef(int id) { return ((unsigned int)id << 2) | 2; }

private:
	CFirmware *m_pFw;
	CLayoutNames *m_pLayoutNames;
	PixoScript::CScript m_Script;
	DWORD m_dwOffset;
	BOOL m_bNamesInferred;
	std::vector<int> m_SortedIds;
	std::map<int, std::vector<Use> > m_Uses;
	std::map<int, std::vector<int> > m_Callees;
	std::map<int, std::map<int, std::vector<int> > > m_Sites;     // caller -> callee -> instruction offsets
	std::map<int, CString> m_Names;
	std::vector<Use> m_NoUses;
	std::vector<int> m_NoCallees;
	std::vector<int> m_NoSites;
};
