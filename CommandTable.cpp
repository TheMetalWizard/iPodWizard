// CommandTable.cpp - the command table as the Layout tab shows it. See CommandTable.h.

#include "stdafx.h"
#include "iPodWizard.h"
#include "Firmware.h"
#include "LayoutNames.h"
#include "CommandTable.h"
#include <algorithm>

CCommandTable::CCommandTable()
	: m_pFw(NULL), m_pLayoutNames(NULL), m_dwOffset(0), m_bNamesInferred(FALSE)
{
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
