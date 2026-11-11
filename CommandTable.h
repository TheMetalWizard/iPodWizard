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
#include "NativeTable.h"
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
	CString Group(int functionId) const;                                        // what kind of job it has ("Opens a screen", "Music and playback", ...)
	BOOL UsedAs(int functionId, int kind) const;                                // is it run by at least one menu entry in that way (USE_SELECT / USE_HILITE)?
	CString Describe(int functionId) const;                                     // one line: label, group, parameters
	// Is `newId` a sensible replacement for `oldId` in a menu entry's highlight (bHilite) or select command? Returns text that tells what was
	// found (empty when there is nothing to say); *pbWarn is set when something looks wrong.
	CString Compare(int oldId, int newId, BOOL bHilite, BOOL *pbWarn) const;
	CString MenuEntryText(int menuId, int item) const;

	// Brick (the BlockO game): where the numbers that decide how it plays are stored in the firmware (offsets in the firmware buffer, 0xFFFFFFFF
	// when not found). The game is code of the operating system; the monochrome iPods (1G/2G to 4G) have the level table and the start speeds.
	struct BrickInfo
	{
		BOOL valid;
		DWORD table;                      // 22 levels of {balls, rows of bricks, paddle width} (32 bit words), then {0, 0, 0}
		DWORD vx, vy;                     // the instructions mov rX,#0x28000 and mov rX,#0x21000: speed of a new ball (16.16 fixed point)
		DWORD lives;                      // the lowest byte of the instruction mov r0,#3: lives of a new game
		DWORD score;                      // the lowest byte of the instruction add r1,r0,#1: points for a brick
		DWORD bonus;                      // the instruction add r0,r0,r1: lives for a cleared level (one per ball still in play)
		DWORD ballW, ballH, ballGap;      // lowest bytes of mov lr,#3 (width of a ball), add r12,r12,#3 (height), add r12,r3,#4 (start below the bricks)
	};
	const BrickInfo &Brick() const { return m_Brick; }
	static DWORD MovValue(DWORD instruction);                                   // the number a mov rX,#imm instruction loads
	BOOL IsBrickCommand(int functionId) const;                                  // does it use the BlockO application (gBlockOApp)?

	// Stacker, the falling block game of the 3G prototype 2.2d200 (only that firmware has it). Offsets in the firmware buffer, 0xFFFFFFFF when not found.
	struct StackerInfo
	{
		BOOL valid;
		DWORD table;                      // 16 shapes of 32 bytes: columns, rows, origin x, origin y (32 bit words), then 4 x 4 cells (bytes)
		DWORD points;                     // lowest byte of 'add r1,r0,#12': points for a full row
		DWORD tick[3];                    // lowest bytes of 'mov r1,#40': the length of a tick in ms, in three places that belong together
		DWORD column[2];                  // lowest bytes of 'mov r0,#6' / 'mov r1,#6': the column a piece appears in
		DWORD firstPiece;                 // lowest byte of 'mov r1,#4': the shape (type x 4) of the first piece
	};
	const StackerInfo &Stacker() const { return m_Stacker; }
	BOOL IsStackerCommand(int functionId) const;                                // does it use the Stacker application (gStackerApp)?
	// what the firmware's own tables say about an object or method the script calls (see NativeTable.h), empty when it is neither
	CString NativeText(int atom) const;
	const CNativeTable &Natives() const { return m_Native; }

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
	void FindBrick(const BYTE *img, DWORD len, DWORD bufStart);
	void FindStacker(const BYTE *img, DWORD len, DWORD bufStart);
	BOOL UsesApp(int functionId, int resource) const;
	CFirmware *m_pFw;
	CLayoutNames *m_pLayoutNames;
	PixoScript::CScript m_Script;
	CNativeTable m_Native;
	BrickInfo m_Brick;
	StackerInfo m_Stacker;
	mutable std::map<int, BOOL> m_AppCmd;       // (application resource, command) -> does the command use the application's global
	mutable std::map<int, int> m_AppGlobal;     // application resource -> atom of the global that holds it (gBlockOApp ...), -1 none
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
