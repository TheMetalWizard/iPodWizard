// PixoScript.h - reader and decompiler for the iPod's "command table" resource ('dmCT').
//
// The command table is not a table of commands: it is compiled script. The iPod's user interface is written
// in PixoScript, Pixo's small embedded JavaScript, and this resource holds the compiled functions (menus and
// views refer to them by ID). The byte code is SpiderMonkey's: the same instruction set (goto, ifeq, call,
// getprop, switches and so on) with the same operand layout, so every function can be turned back into
// readable, structured source with the control flow rebuilt.
//
//   resource  = header, constants ("keys"), the function offset table, then the functions
//   function  = ID (the script's atom number for its name), number of parameters, number of local variables,
//               stack depth, byte code length, byte code, two bytes of padding
//
// Names for atoms (functions, objects, methods) are only stored in one firmware (nano 14.5.2, a development
// build). Everywhere else the script refers to them by number; they are shown as a<number>.
//
// Nothing here writes to the firmware.
//
// Released under the GNU General Public License v2, like the rest of iPodWizard.

#pragma once

#include <vector>
#include <map>
#include <set>

namespace PixoScript
{
	enum RefType
	{
		REF_NONE     = 0,      // only says where something is stored (value is not used)
		REF_FUNCTION = 1,      // value is a function ID of this script
		REF_CONSTANT = 2,      // value is a number that may be the ID of a view, menu, string, bitmap or font style
		REF_MENU     = 3       // value is a menu ID (used by the lists built around the script)
	};

	// Something a line of the listing refers to. Besides what it points at it knows which part of the line's text
	// stands for it and where the number behind it is stored in the firmware, so the part can be marked and shown in
	// the hex view.
	struct Ref
	{
		int type;
		int value;
		CString token;         // the part of the line's text that stands for it ("" when there is none)
		int off;               // where the number is stored: offset inside the resource (inside the firmware image when
		                       // `absolute`), -1 when that is not known
		int len;               // number of bytes there
		bool bigEndian;        // stored high byte first (operands of byte code); otherwise low byte first
		bool absolute;
		int insOff;            // where the 16 bit operand of the instruction holding it is stored (resource offset), -1 if none
		int item;              // for a menu: the entry (counting from 0), otherwise -1
		CString tip;           // what is stored there; empty: the tooltip makes it from the bytes

		Ref() : type(0), value(0), off(-1), len(0), bigEndian(false), absolute(false), insOff(-1), item(-1) {}
	};

	// One line of the decompiled source (or of the disassembly)
	struct Line
	{
		int indent;
		CString text;
		int off;               // byte code offset of the instruction this line comes from, -1 when none
		int len;               // number of byte code bytes it covers (disassembly only)
		int kind;              // see LineKind
		int gotoTarget;        // for a jump: where to (-1 otherwise)
		CString offText;       // the offsets shown in the Offset column of a row that is not an instruction (places in the byte code that name a command)
		CString offTip;        // what they are
		std::vector<Ref> refs;

		Line() : indent(0), off(-1), len(0), kind(0), gotoTarget(-1) {}
	};

	enum LineKind
	{
		LK_STATEMENT = 0,
		LK_IF,                 // opens a block: if / while / switch
		LK_ELSEIF,
		LK_BRACE,              // closing brace or else
		LK_CASE,
		LK_LABEL,
		LK_GOTO
	};

	struct Function
	{
		int id;
		int nParams;
		int nVars;
		int depth;
		DWORD recOffset;       // where the function record starts inside the resource
		DWORD codeLen;         // length of the byte code
	};

	struct Instruction
	{
		int off;               // offset inside the function's byte code
		int op;
		int len;               // bytes taken, including a switch table
		int arg;               // 16 bit operand (signed for jumps)
		int dflt, low, high;   // tableswitch
		std::vector<int> offs; // tableswitch targets (relative to off)
	};

	class CScript
	{
	public:
		CScript();

		// Parse a resource. res/size describe the whole 'dmCT' resource data. The data is not copied.
		BOOL Parse(const BYTE *res, DWORD size);
		BOOL IsValid() const { return m_bValid; }

		int  GetNumFunctions() const { return (int)m_Functions.size(); }
		const Function &GetFunction(int index) const { return m_Functions[index]; }
		int  FindFunction(int id) const;                       // index, or -1
		const BYTE *GetCode(const Function &f) const;          // byte code of a function
		DWORD GetCodeOffset(const Function &f) const { return f.recOffset + 10; }      // inside the resource
		DWORD GetTailOffset(const Function &f) const { return f.recOffset + 10 + f.codeLen; }
		DWORD GetRecordLength(const Function &f) const { return 10 + f.codeLen + 2; }   // header + code + padding
		const BYTE *GetResource() const { return m_pRes; }
		DWORD GetResourceSize() const { return m_dwSize; }

		int  GetNumEntryPoints() const { return (int)m_EntryPoints.size(); }
		int  GetEntryPoint(int i) const { return m_EntryPoints[i]; }   // IDs of functions the firmware itself calls
		DWORD GetEntryPointOffset(int i) const { return m_dwEntryBase + 2 * (DWORD)i; }   // where it is stored (inside the resource)
		BOOL IsEntryPoint(int id) const;

		BOOL HasNames() const { return !m_AtomOffsets.empty(); }
		int  GetNumAtoms() const { return (int)m_AtomOffsets.size(); }
		CString GetAtomName(int atom) const;                   // "" when unknown
		void SetExternalNames(const std::map<int, CString> *pNames) { m_pNames = pNames; }
		CString AtomText(int atom) const;                      // the name, or a<number>

		int  GetNumConstants() const { return (int)m_Keys.size(); }
		BOOL GetNumberValue(int operand, DWORD *pValue) const; // value a 'number' instruction refers to
		DWORD GetNumberOffset(int operand) const;              // where that value is stored in the resource (0xFFFFFFFF: nowhere)
		CString NumberText(int operand) const;

		// decoding
		BOOL Decode(const Function &f, std::vector<Instruction> &out) const;
		BOOL Disassemble(const Function &f, std::vector<Line> &out) const;
		BOOL Decompile(const Function &f, std::vector<Line> &out, BOOL *pbClean = NULL) const;

		// everything a function calls or refers to by name (function IDs of this script)
		void CollectFunctionRefs(const Function &f, std::set<int> &ids) const;
		// the same with the places: function ID -> offsets (inside the byte code) of the instructions that name it
		void CollectFunctionSites(const Function &f, std::map<int, std::vector<int> > &sites) const;

	private:
		BOOL m_bValid;
		const BYTE *m_pRes;
		DWORD m_dwSize;
		std::vector<Function> m_Functions;
		std::map<int, int> m_IdToIndex;
		std::vector<DWORD> m_Keys;
		std::vector<int> m_EntryPoints;
		DWORD m_dwEntryBase;                                   // where the entry points are stored
		std::vector<DWORD> m_AtomOffsets;                      // debug format only
		DWORD m_dwPoolBase;
		DWORD m_dwKeysOffset;                                  // where the table of constants starts
		const std::map<int, CString> *m_pNames;

		friend class CDecompiler;
	};

	const TCHAR *OpName(int op);
}
