// CommandTable.cpp - the command table as the Layout tab shows it. See CommandTable.h.

#include "stdafx.h"
#include "iPodWizard.h"
#include "Firmware.h"
#include "LayoutNames.h"
#include "CommandTable.h"
#include "BrickParams.h"
#include <algorithm>

CCommandTable::CCommandTable()
	: m_pFw(NULL), m_pLayoutNames(NULL), m_dwOffset(0), m_bNamesInferred(FALSE)
{
	memset(&m_Brick, 0, sizeof(m_Brick));
	memset(&m_Stacker, 0, sizeof(m_Stacker));
}

static DWORD Crc32Update(DWORD crc, const BYTE *p, size_t n)
{
	static DWORD table[256];
	static bool ready = false;
	if (!ready)
	{
		for (DWORD i = 0; i < 256; i++)
		{
			DWORD c = i;
			for (int k = 0; k < 8; k++)
				c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
			table[i] = c;
		}
		ready = true;
	}
	crc = ~crc;
	for (size_t i = 0; i < n; i++)
		crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
	return ~crc;
}

// The number of functions, then the ID and the code length of every function, in the order of the table
DWORD CCommandTable::Fingerprint() const
{
	if (!m_Script.IsValid())
		return 0;
	DWORD crc = 0;
	DWORD n = (DWORD)m_Script.GetNumFunctions();
	BYTE b[4] = { (BYTE)n, (BYTE)(n >> 8), (BYTE)(n >> 16), (BYTE)(n >> 24) };
	crc = Crc32Update(crc, b, 4);
	for (int i = 0; i < m_Script.GetNumFunctions(); i++)
	{
		const PixoScript::Function &f = m_Script.GetFunction(i);
		BYTE e[4] = { (BYTE)f.id, (BYTE)((unsigned)f.id >> 8), (BYTE)f.codeLen, (BYTE)(f.codeLen >> 8) };
		crc = Crc32Update(crc, e, 4);
	}
	return crc;
}

int CCommandTable::RefToFunction(unsigned int value)
{
	if ((value & 3) != 2)
		return -1;
	return (int)(value >> 2);
}

const PixoScript::Function *CCommandTable::FindFunction(int id) const
{
	int index = m_Script.FindFunction(id);
	return index < 0 ? NULL : &m_Script.GetFunction(index);
}

DWORD CCommandTable::MovValue(DWORD instruction)
{
	return BrickArmImm(instruction);
}

// Where Brick's numbers are in the operating system image (BrickParams.h finds them by the code around them, so a firmware whose numbers were
// changed is found too). `bufStart` is where the image starts in the firmware buffer, so the offsets stored are offsets in the buffer.
void CCommandTable::FindBrick(const BYTE *img, DWORD len, DWORD bufStart)
{
	memset(&m_Brick, 0, sizeof(m_Brick));
	m_Brick.table = m_Brick.vx = m_Brick.vy = m_Brick.lives = 0xFFFFFFFF;
	m_Brick.score = m_Brick.bonus = m_Brick.ballW = m_Brick.ballH = m_Brick.ballGap = 0xFFFFFFFF;
	BrickParams p;
	if (!FindBrickParams(img, len, &p))
		return;
	const DWORD none = 0xFFFFFFFF;
	m_Brick.table = bufStart + p.tableAt;
	m_Brick.vx = p.vxAt == none ? none : bufStart + p.vxAt;
	m_Brick.vy = p.vyAt == none ? none : bufStart + p.vyAt;
	m_Brick.lives = p.livesAt == none ? none : bufStart + p.livesAt;
	m_Brick.score = p.scoreAt == none ? none : bufStart + p.scoreAt;
	m_Brick.bonus = p.bonusAt == none ? none : bufStart + p.bonusAt;
	m_Brick.ballW = p.ballWAt == none ? none : bufStart + p.ballWAt;
	m_Brick.ballH = p.ballHAt == none ? none : bufStart + p.ballHAt;
	m_Brick.ballGap = p.ballGapAt == none ? none : bufStart + p.ballGapAt;
	m_Brick.valid = TRUE;
}

// The global that holds an application of the game host (gBlockOApp, gStackerApp ...): the script makes it once, at boot, with
// gSomeApp = TSomeApp.GetCurrentSomeApp();  where TSomeApp is the native object made from `resource`. The games have methods of the same names
// (ResetGame, StartGame ...), so the global, not the method, says which game a command is about.
static int FindAppGlobal(const PixoScript::CScript &script, const CNativeTable &natives, int resource)
{
	for (int i = 0; i < script.GetNumFunctions(); i++)
	{
		std::vector<PixoScript::Instruction> ins;
		if (!script.Decode(script.GetFunction(i), ins))
			continue;
		for (size_t k = 0; k + 1 < ins.size(); k++)
		{
			if (ins[k].op != 0x3b)                                     // name
				continue;
			const CNativeTable::Object *po = natives.FindObject(ins[k].arg);
			if (po == NULL || po->resource != resource)
				continue;
			for (size_t j = k + 1; j < ins.size() && j < k + 6; j++)
				if (ins[j].op == 0x6d)                                 // setname: the global that gets the application
					return ins[j].arg;
		}
	}
	return -1;
}

BOOL CCommandTable::UsesApp(int functionId, int resource) const
{
	if (!m_Native.IsValid())
		return FALSE;
	std::map<int, int>::const_iterator g = m_AppGlobal.find(resource);
	if (g == m_AppGlobal.end())
		g = m_AppGlobal.insert(std::make_pair(resource, FindAppGlobal(m_Script, m_Native, resource))).first;
	if (g->second < 0)
		return FALSE;
	int key = resource * 100000 + functionId;
	std::map<int, BOOL>::const_iterator it = m_AppCmd.find(key);
	if (it != m_AppCmd.end())
		return it->second;
	BOOL result = FALSE;
	const PixoScript::Function *f = FindFunction(functionId);
	std::vector<PixoScript::Instruction> ins;
	if (f != NULL && m_Script.Decode(*f, ins))
		for (size_t i = 0; i < ins.size() && !result; i++)
			if (ins[i].op == 0x3b && ins[i].arg == g->second)
				result = TRUE;
	m_AppCmd[key] = result;
	return result;
}

BOOL CCommandTable::IsBrickCommand(int functionId) const
{
	return m_Brick.valid && UsesApp(functionId, 0x2880);
}

BOOL CCommandTable::IsStackerCommand(int functionId) const
{
	return m_Stacker.valid && UsesApp(functionId, 0x4680);
}

// Stacker exists in one firmware only (the 3G prototype 2.2d200), so its numbers are looked for at the places they have there, and only taken when the
// instructions around each of them are the ones that were read from that firmware (the numbers themselves are not compared, so a changed firmware is found).
void CCommandTable::FindStacker(const BYTE *img, DWORD len, DWORD bufStart)
{
	memset(&m_Stacker, 0, sizeof(m_Stacker));
	const DWORD none = 0xFFFFFFFF;
	m_Stacker.table = m_Stacker.points = m_Stacker.firstPiece = none;
	m_Stacker.tick[0] = m_Stacker.tick[1] = m_Stacker.tick[2] = m_Stacker.column[0] = m_Stacker.column[1] = none;
	if (img == NULL || len != 0x2D9D9C || (((DWORD_PTR)img) & 3) != 0)
		return;
	const DWORD *W = (const DWORD *)img;
	#define IMM(at, hi) ((W[(at) / 4] & 0xFFFFF000) == (hi))
	if (W[0xD018C / 4] != 0xE92D41F0 ||                                                // the constructor of the view
	    !(W[0x3403C / 4] == 0xE594006C && W[0x34040 / 4] == 0xE28FE018 && IMM(0x34044, 0xE2801000) && W[0x34048 / 4] == 0xE584106C) ||     // add r1,r0,#points
	    !(IMM(0xD0230, 0xE3A01000) && IMM(0xB986C, 0xE3A01000) && IMM(0x758EC, 0xE3A01000)) ||                                              // mov r1,#tick
	    !(IMM(0x340C4, 0xE3A00000) && W[0x340C8 / 4] == 0xE2841074 && IMM(0xA97F0, 0xE3A01000) && W[0xA97F4 / 4] == 0xE280C074) ||          // the column
	    !(IMM(0xA9808, 0xE3A01000) && W[0xA980C / 4] == 0xE280C07C))                                                                          // the first piece
		return;
	#undef IMM
	DWORD table = W[0x34134 / 4];
	if (table % 4 != 0 || (ULONGLONG)table + 16 * 32 > len || W[0x3516C / 4] != table)
		return;
	m_Stacker.table = bufStart + table;
	m_Stacker.points = bufStart + 0x34044;
	m_Stacker.tick[0] = bufStart + 0xD0230;
	m_Stacker.tick[1] = bufStart + 0xB986C;
	m_Stacker.tick[2] = bufStart + 0x758EC;
	m_Stacker.column[0] = bufStart + 0x340C4;
	m_Stacker.column[1] = bufStart + 0xA97F0;
	m_Stacker.firstPiece = bufStart + 0xA9808;
	m_Stacker.valid = TRUE;
}

void CCommandTable::Build(CFirmware *pFirmware, CLayoutNames *pNames)
{
	m_pFw = pFirmware;
	m_pLayoutNames = pNames;
	m_dwOffset = 0;
	m_SortedIds.clear();
	m_Uses.clear();
	m_Callees.clear();
	m_Sites.clear();
	m_Names.clear();
	m_bNamesInferred = FALSE;
	m_Script.SetExternalNames(&m_Names);
	m_Native.Clear();
	memset(&m_Brick, 0, sizeof(m_Brick));
	memset(&m_Stacker, 0, sizeof(m_Stacker));
	m_AppCmd.clear();
	m_AppGlobal.clear();
	m_Script.SetNativeTable(&m_Native);
	m_Script.Parse(NULL, 0);                       // forget the previous firmware's script
	if (m_pFw == NULL)
		return;

	ScriptTable *pScripts = m_pFw->GetScriptList();
	if (pScripts == NULL)
		return;
	LPBYTE base = m_pFw->GetFirmwareBuffer();
	for (size_t i = 0; i < pScripts->size(); i++)
	{
		const ScriptRecord &rec = pScripts->at(i);
		if (m_Script.Parse(rec.data, rec.size))
		{
			m_dwOffset = (DWORD)(rec.data - base);
			break;
		}
	}
	if (!m_Script.IsValid())
		return;

	// the objects and methods the firmware implements itself, from the tables in its operating system image
	DWORD osStart, osLen;
	if (m_pFw->GetImageRange("soso", &osStart, &osLen))
	{
		m_Native.Build(base + osStart, osLen);
		FindBrick(base + osStart, osLen, osStart);
		FindStacker(base + osStart, osLen, osStart);
	}

	// only the development build names its atoms itself; for the others PixoNames.txt next to the program may know them
	if (!m_Script.HasNames())
	{
		TCHAR path[MAX_PATH];
		DWORD n = GetModuleFileName(NULL, path, MAX_PATH);
		if (n > 0 && n < MAX_PATH)
		{
			TCHAR *slash = _tcsrchr(path, _T('\\'));
			if (slash && (slash - path) + 16 < MAX_PATH)
			{
				_tcscpy(slash + 1, _T("PixoNames.txt"));
				if (LoadNamesFor(path, Fingerprint()))
					m_bNamesInferred = TRUE;
			}
		}
	}

	std::set<int> ids;
	for (int i = 0; i < m_Script.GetNumFunctions(); i++)
		ids.insert(m_Script.GetFunction(i).id);
	m_SortedIds.assign(ids.begin(), ids.end());

	// functions the firmware starts itself
	for (int i = 0; i < m_Script.GetNumEntryPoints(); i++)
	{
		int id = m_Script.GetEntryPoint(i);
		if (!ids.count(id))
			continue;
		Use u;
		u.kind = USE_ENTRY;
		u.id = 0;
		u.item = 0;
		u.addr = m_dwOffset + m_Script.GetEntryPointOffset(i);
		u.len = 2;
		m_Uses[id].push_back(u);
	}

	// menu entries: the command that is run when an entry is chosen (cmd) and the one run when it is highlighted (z2)
	MenuTable *pMenus = m_pFw->GetMenuList();
	if (pMenus)
		for (size_t i = 0; i < pMenus->size(); i++)
			for (int k = 0; k < pMenus->at(i)->num_elems; k++)
			{
				const resource_type_menu_elem *e = pMenus->at(i)->MenuElemList[k]->element;
				int fnSelect = RefToFunction(e->cmd);
				int fnHilite = RefToFunction(e->z2);
				if (fnSelect >= 0 && ids.count(fnSelect))
				{
					Use u;
					u.kind = USE_SELECT;
					u.id = pMenus->at(i)->id;
					u.item = k;
					u.addr = (DWORD)((const BYTE *)&e->cmd - base);
					u.len = 4;
					m_Uses[fnSelect].push_back(u);
				}
				if (fnHilite >= 0 && ids.count(fnHilite))
				{
					Use u;
					u.kind = USE_HILITE;
					u.id = pMenus->at(i)->id;
					u.item = k;
					u.addr = (DWORD)((const BYTE *)&e->z2 - base);
					u.len = 4;
					m_Uses[fnHilite].push_back(u);
				}
			}

	// calls
	for (int i = 0; i < m_Script.GetNumFunctions(); i++)
	{
		const PixoScript::Function &f = m_Script.GetFunction(i);
		std::map<int, std::vector<int> > sites;
		m_Script.CollectFunctionSites(f, sites);
		sites.erase(f.id);
		std::vector<int> &callees = m_Callees[f.id];
		for (std::map<int, std::vector<int> >::const_iterator it = sites.begin(); it != sites.end(); ++it)
		{
			callees.push_back(it->first);
			m_Sites[f.id][it->first] = it->second;
			Use u;
			u.kind = USE_CALLER;
			u.id = f.id;
			u.item = 0;
			u.addr = GetCodeOffset(f) + (DWORD)it->second[0] + 1;       // the operand of the first instruction that names it
			u.len = 2;
			m_Uses[it->first].push_back(u);
		}
	}
}

const std::vector<CCommandTable::Use> &CCommandTable::GetUses(int functionId) const
{
	std::map<int, std::vector<Use> >::const_iterator it = m_Uses.find(functionId);
	return it == m_Uses.end() ? m_NoUses : it->second;
}

const std::vector<int> &CCommandTable::GetCallees(int functionId) const
{
	std::map<int, std::vector<int> >::const_iterator it = m_Callees.find(functionId);
	return it == m_Callees.end() ? m_NoCallees : it->second;
}

const std::vector<int> &CCommandTable::GetSites(int caller, int callee) const
{
	std::map<int, std::map<int, std::vector<int> > >::const_iterator it = m_Sites.find(caller);
	if (it == m_Sites.end())
		return m_NoSites;
	std::map<int, std::vector<int> >::const_iterator jt = it->second.find(callee);
	return jt == it->second.end() ? m_NoSites : jt->second;
}

CString CCommandTable::Name(int functionId) const
{
	return m_Script.GetAtomName(functionId);
}

CString CCommandTable::NativeText(int atom) const
{
	CString s;
	if (!m_Native.IsValid())
		return s;
	const CNativeTable::Object *po = m_Native.FindObject(atom);
	if (po != NULL)
	{
		s = _T("Object the firmware implements itself");
		if (po->resource >= 0)
			s.AppendFormat(_T(", made from resource %d"), po->resource);
		if (po->end > po->first)
			s.AppendFormat(_T("; its methods are the atoms %d to %d"), po->first, po->end - 1);
		return s;
	}
	int lo, hi;
	if (!m_Native.GetParams(atom, &lo, &hi))
		return s;
	s = _T("Method the firmware implements itself: ");
	if (lo == hi)
		s.AppendFormat(_T("%d parameter%s"), lo, lo == 1 ? _T("") : _T("s"));
	else
		s.AppendFormat(_T("%d to %d parameters (depends on the object)"), lo, hi);
	int owner = m_Native.GetOwner(atom);
	if (owner != 0)
		s += _T(". Member of ") + m_Script.AtomText(owner);
	return s;
}

CString CCommandTable::MenuEntryText(int menuId, int item) const
{
	if (m_pFw == NULL || m_pLayoutNames == NULL)
		return CString();
	MenuTable *pMenus = m_pFw->GetMenuList();
	if (pMenus == NULL)
		return CString();
	for (size_t i = 0; i < pMenus->size(); i++)
		if (pMenus->at(i)->id == menuId)
		{
			if (item < 0 || item >= pMenus->at(i)->num_elems)
				return CString();
			return m_pLayoutNames->MenuEntryText(pMenus->at(i)->MenuElemList[item]->element);
		}
	return CString();
}

CString CCommandTable::Role(int functionId) const
{
	const std::vector<Use> &uses = GetUses(functionId);
	int nSelect = 0, nHilite = 0, nCallers = 0;
	bool bEntry = false;
	const Use *firstSelect = NULL, *firstHilite = NULL, *firstCaller = NULL;
	for (size_t i = 0; i < uses.size(); i++)
		switch (uses[i].kind)
		{
		case USE_ENTRY:
			bEntry = true;
			break;
		case USE_SELECT:
			if (nSelect++ == 0)
				firstSelect = &uses[i];
			break;
		case USE_HILITE:
			if (nHilite++ == 0)
				firstHilite = &uses[i];
			break;
		case USE_CALLER:
			if (nCallers++ == 0)
				firstCaller = &uses[i];
			break;
		}

	CString s;
	if (nSelect > 1)
		s.Format(_T("Chosen in %d menu entries"), nSelect);
	else if (firstSelect)
	{
		CString text = MenuEntryText(firstSelect->id, firstSelect->item);
		if (!text.IsEmpty())
			s.Format(_T("Chosen: '%s' (menu %d)"), (LPCTSTR)text, firstSelect->id);
		else
			s.Format(_T("Chosen in menu %d"), firstSelect->id);
	}
	else if (nHilite > 0)
	{
		if (nHilite == 1)
			s.Format(_T("Highlight in menu %d"), firstHilite->id);
		else
			s.Format(_T("Highlight in %d menu entries"), nHilite);
	}
	else if (bEntry)
		s = _T("Entry point");
	else if (nCallers == 1)
		s.Format(_T("Called by %d"), firstCaller->id);
	else if (nCallers > 1)
		s.Format(_T("Called by %d commands"), nCallers);
	return s;
}

CString CCommandTable::Label(int functionId) const
{
	CString desc = Name(functionId);
	if (desc.IsEmpty())
		desc = Role(functionId);
	CString s;
	s.Format(_T("%d"), functionId);
	if (!desc.IsEmpty())
		s += _T(" - ") + desc;
	return s;
}

// What kind of job a command has. Names exist only for some firmwares (see PixoNames.txt), so this goes by the name when there is one and
// otherwise by how the command is used.
CString CCommandTable::Group(int functionId) const
{
	CString n = Name(functionId);
	n.MakeLower();
	if (!n.IsEmpty())
	{
		static const struct { const TCHAR *key; const TCHAR *group; } keys[] = {
			{ _T("game"), _T("Games") }, { _T("brick"), _T("Games") }, { _T("blocko"), _T("Games") }, { _T("solitaire"), _T("Games") },
			{ _T("parachute"), _T("Games") }, { _T("quiz"), _T("Games") }, { _T("klondike"), _T("Games") },
			{ _T("play"), _T("Music and playback") }, { _T("pause"), _T("Music and playback") }, { _T("song"), _T("Music and playback") },
			{ _T("music"), _T("Music and playback") }, { _T("shuffle"), _T("Music and playback") }, { _T("repeat"), _T("Music and playback") },
			{ _T("volume"), _T("Music and playback") }, { _T("track"), _T("Music and playback") }, { _T("artist"), _T("Music and playback") },
			{ _T("album"), _T("Music and playback") }, { _T("genre"), _T("Music and playback") }, { _T("playlist"), _T("Music and playback") },
			{ _T("backlight"), _T("Settings") }, { _T("setting"), _T("Settings") }, { _T("toggle"), _T("Settings") }, { _T("contrast"), _T("Settings") },
			{ _T("clicker"), _T("Settings") }, { _T("reset"), _T("Settings") }, { _T("language"), _T("Settings") }, { _T("sleep"), _T("Settings") },
			{ _T("clock"), _T("Clock and calendar") }, { _T("date"), _T("Clock and calendar") }, { _T("alarm"), _T("Clock and calendar") },
			{ _T("stopwatch"), _T("Clock and calendar") }, { _T("calendar"), _T("Clock and calendar") }, { _T("timer"), _T("Clock and calendar") },
			{ _T("photo"), _T("Photos") }, { _T("slide"), _T("Photos") }, { _T("voice"), _T("Voice memos") }, { _T("memo"), _T("Voice memos") },
			{ _T("hilite"), _T("Highlight") }, { _T("highlight"), _T("Highlight") },
			{ _T("goto"), _T("Opens a screen") }, { _T("open"), _T("Opens a screen") }, { _T("show"), _T("Opens a screen") },
			{ _T("screen"), _T("Opens a screen") }, { _T("menu"), _T("Opens a screen") }, { _T("view"), _T("Opens a screen") },
		};
		for (int i = 0; i < (int)(sizeof(keys) / sizeof(keys[0])); i++)
			if (n.Find(keys[i].key) >= 0)
				return keys[i].group;
		return _T("Other");
	}
	if (UsedAs(functionId, USE_SELECT))
		return _T("Unnamed, chosen in a menu");
	if (UsedAs(functionId, USE_HILITE))
		return _T("Unnamed, highlight");
	return _T("Unnamed, other");
}

BOOL CCommandTable::UsedAs(int functionId, int kind) const
{
	const std::vector<Use> &uses = GetUses(functionId);
	for (size_t i = 0; i < uses.size(); i++)
		if (uses[i].kind == kind)
			return TRUE;
	return FALSE;
}

CString CCommandTable::Describe(int functionId) const
{
	const PixoScript::Function *f = FindFunction(functionId);
	CString s;
	if (f == NULL)
	{
		s.Format(_T("%d is not a command of this firmware"), functionId);
		return s;
	}
	CString name = Name(functionId);
	if (name.IsEmpty())
		s.Format(_T("Command %d"), functionId);
	else
		s.Format(_T("Command %d '%s'"), functionId, (LPCTSTR)name);
	s.AppendFormat(_T(": %s, %d parameter%s"), (LPCTSTR)Group(functionId), f->nParams, f->nParams == 1 ? _T("") : _T("s"));
	CString msg;
	if (!m_Script.Verify(*f, &msg))
		s += _T(", check failed");
	return s;
}

CString CCommandTable::Compare(int oldId, int newId, BOOL bHilite, BOOL *pbWarn) const
{
	CString s;
	BOOL bWarn = FALSE;
	const PixoScript::Function *pn = FindFunction(newId);
	const PixoScript::Function *po = FindFunction(oldId);
	if (pn == NULL)
	{
		if (pbWarn)
			*pbWarn = TRUE;
		s.Format(_T("%d is not a command of this firmware."), newId);
		return s;
	}
	const TCHAR *slot = bHilite ? _T("highlight") : _T("select");
	s = Describe(newId) + _T(".\n");
	if (po != NULL)
		s += _T("Now: ") + Describe(oldId) + _T(".\n");

	CString msg;
	if (!m_Script.Verify(*pn, &msg))
	{
		s += _T("- Its code failed the integrity check (") + msg + _T("). The iPod may not run it correctly.\n");
		bWarn = TRUE;
	}

	// how the stock firmware uses it
	int nSel = 0, nHil = 0, nCall = 0;
	bool bEntry = false;
	const std::vector<Use> &uses = GetUses(newId);
	for (size_t i = 0; i < uses.size(); i++)
	{
		if (uses[i].kind == USE_SELECT) nSel++;
		else if (uses[i].kind == USE_HILITE) nHil++;
		else if (uses[i].kind == USE_CALLER) nCall++;
		else if (uses[i].kind == USE_ENTRY) bEntry = true;
	}
	if (nSel + nHil == 0)
	{
		if (bEntry)
			s += _T("- The firmware starts this command itself; no menu entry uses it.\n");
		else if (nCall > 0)
			s += _T("- No menu uses it; it is only called by other commands, so it may expect to be called from them.\n");
		else
			s += _T("- Nothing in the firmware runs this command.\n");
		bWarn = TRUE;
	}
	else if (bHilite && nHil == 0)
	{
		s += _T("- Menus only run it when an entry is chosen, never as a highlight command.\n");
		bWarn = TRUE;
	}
	else if (!bHilite && nSel == 0)
	{
		s += _T("- Menus only run it when an entry is highlighted, never as a select command.\n");
		bWarn = TRUE;
	}

	// the number of parameters the commands used in this place take (the most common count)
	std::map<int, int> counts;
	int kind = bHilite ? USE_HILITE : USE_SELECT;
	for (std::map<int, std::vector<Use> >::const_iterator it = m_Uses.begin(); it != m_Uses.end(); ++it)
	{
		const PixoScript::Function *pf = FindFunction(it->first);
		if (pf == NULL)
			continue;
		for (size_t i = 0; i < it->second.size(); i++)
			if (it->second[i].kind == kind)
			{
				counts[pf->nParams]++;
				break;
			}
	}
	int best = -1, bestN = 0, total = 0;
	for (std::map<int, int>::const_iterator it = counts.begin(); it != counts.end(); ++it)
	{
		total += it->second;
		if (it->second > bestN)
		{
			best = it->first;
			bestN = it->second;
		}
	}
	if (total > 0 && pn->nParams != best)
	{
		CString t;
		t.Format(_T("- It takes %d parameter%s; %d of the %d %s commands in this firmware take %d.\n"), pn->nParams,
		         pn->nParams == 1 ? _T("") : _T("s"), bestN, total, slot, best);
		s += t;
		bWarn = TRUE;
	}
	if (po != NULL && po->nParams != pn->nParams)
	{
		CString t;
		t.Format(_T("- The command it replaces takes %d parameter%s, this one %d.\n"), po->nParams, po->nParams == 1 ? _T("") : _T("s"), pn->nParams);
		s += t;
		bWarn = TRUE;
	}
	if (!bWarn)
		s += _T("Nothing unusual: it is used the same way by other menu entries.\n");
	if (pbWarn)
		*pbWarn = bWarn;
	return s;
}

CString CCommandTable::Signature(int functionId) const
{
	const PixoScript::Function *f = FindFunction(functionId);
	if (f == NULL)
		return CString();
	CString name = Name(functionId);
	if (name.IsEmpty())
		name = m_Script.AtomText(functionId);
	else
		name.AppendFormat(_T(" (%d)"), functionId);
	CString s;
	s.Format(_T("%s: %d parameter%s, %d variable%s, stack depth %d, %d bytes of code"), (LPCTSTR)name,
	         f->nParams, f->nParams == 1 ? _T("") : _T("s"), f->nVars, f->nVars == 1 ? _T("") : _T("s"),
	         f->depth, (int)f->codeLen);
	if (m_bNamesInferred)
		s += _T(" (names are inferred)");
	return s;
}

// A names file holds one section per firmware: "[crc]" (several separated by commas), then one line per atom,
// the number, a tab and the name. Lines starting with # are comments.
BOOL CCommandTable::LoadNamesFor(LPCTSTR path, DWORD fingerprint)
{
	m_Names.clear();
	CStdioFile file;
	if (!file.Open(path, CFile::modeRead | CFile::typeText))
		return FALSE;
	CString line;
	bool bIn = false, bDone = false;
	while (!bDone && file.ReadString(line))
	{
		if (line.IsEmpty() || line[0] == _T('#'))
			continue;
		if (line[0] == _T('['))
		{
			if (bIn)
				break;                                // the next section: ours is read
			int close = line.Find(_T(']'));
			CString list = close > 0 ? line.Mid(1, close - 1) : CString();
			int pos = 0;
			CString tok = list.Tokenize(_T(","), pos);
			while (!tok.IsEmpty())
			{
				if (_tcstoul(tok, NULL, 16) == fingerprint)
					bIn = true;
				tok = list.Tokenize(_T(","), pos);
			}
			continue;
		}
		if (!bIn)
			continue;
		int tab = line.Find(_T('\t'));
		if (tab <= 0)
			continue;
		int atom = _ttoi(line.Left(tab));
		CString name = line.Mid(tab + 1);
		name.Trim();
		if (atom > 0 && !name.IsEmpty())
			m_Names[atom] = name;
	}
	file.Close();
	return !m_Names.empty();
}

// One line per atom: its number, a tab and its name. Lines starting with ; or # are comments.
BOOL CCommandTable::LoadNames(LPCTSTR path)
{
	m_Names.clear();
	CStdioFile file;
	if (!file.Open(path, CFile::modeRead | CFile::typeText))
		return FALSE;
	CString line;
	while (file.ReadString(line))
	{
		line.Trim();
		if (line.IsEmpty() || line[0] == _T(';') || line[0] == _T('#'))
			continue;
		int tab = line.Find(_T('\t'));
		if (tab <= 0)
			continue;
		int atom = _ttoi(line.Left(tab));
		CString name = line.Mid(tab + 1);
		name.Trim();
		if (atom > 0 && !name.IsEmpty())
			m_Names[atom] = name;
	}
	file.Close();
	return !m_Names.empty();
}
