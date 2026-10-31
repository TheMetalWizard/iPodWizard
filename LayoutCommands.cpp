// LayoutCommands.cpp : the command table modes of the Layout tab.
//
// The command table is compiled script (see PixoScript.h). Mode 3 lists a function as source, mode 4 as
// assembly. Numbers and names in the listing that refer to something else (another function, a menu, a
// string, a bitmap, a layout) can be double-clicked to go there.

#include "stdafx.h"
#include "iPodWizard.h"
#include "LayoutDialog.h"

using namespace PixoScript;

#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER 0x1501                  // (ECM_FIRST + 1: the grey text of an empty edit box; this is not in the headers of the compiler)
#endif

static const int kFunctionMatch = 100;                  // kind of a candidate that is a function (the others are MATCH_*)

// The bytes as hex, in the order they are stored: "58 05 6B"
static CString HexBytes(const BYTE *p, int n)
{
	CString s, t;
	for (int i = 0; i < n; i++)
	{
		t.Format(_T("%02X"), p[i]);
		if (i)
			s += _T(" ");
		s += t;
	}
	return s;
}

static bool IsWordChar(TCHAR ch)
{
	return _istalnum(ch) || ch == _T('_');
}

// Where a part of a line is: the first place at or after `from` where the token stands on its own (not inside a longer
// word or number)
static int FindToken(const CString &text, const CString &token, int from)
{
	int n = text.GetLength(), m = token.GetLength();
	if (m == 0)
		return -1;
	for (int pos = text.Find(token, from); pos >= 0; pos = text.Find(token, pos + 1))
	{
		bool lead = pos == 0 || !IsWordChar(text[pos - 1]) || !IsWordChar(token[0]);
		bool trail = pos + m >= n || !IsWordChar(text[pos + m]) || !IsWordChar(token[m - 1]);
		if (lead && trail)
			return pos;
	}
	return -1;
}

void CLayoutDialog::CmdSetupColumns()
{
	while (m_ResourceList.DeleteColumn(0) == TRUE);
	m_ResourceList.InsertColumn(0, TEXT("Offset"), LVCFMT_LEFT, 80);
	m_ResourceList.InsertColumn(1, TEXT("Address"), LVCFMT_LEFT, 74);
	if (m_ResourceType == 3)
	{
		m_ResourceList.InsertColumn(2, TEXT("Source"), LVCFMT_LEFT, 700);
	}
	else
	{
		m_ResourceList.InsertColumn(2, TEXT("Bytes"), LVCFMT_LEFT, 120);
		m_ResourceList.InsertColumn(3, TEXT("Instruction"), LVCFMT_LEFT, 500);
	}
}

void CLayoutDialog::CmdFillTree()
{
	m_ResourceTree.DeleteAllItems();
	if (!m_Commands.IsValid())
	{
		m_ResourceTree.InsertItem(_T("This firmware has no command table I can read"), TVI_ROOT, TVI_LAST);
		return;
	}
	for (int i = 0; i < m_Commands.GetNumFunctions(); i++)
	{
		int id = m_Commands.GetFunctionId(i);
		InsertLazy(m_Commands.Label(id), TVI_ROOT, id, !m_Commands.GetCallees(id).empty());
	}
}

// The functions a function refers to are added when its item is opened
void CLayoutDialog::OnTvnItemexpandingResourceTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
	*pResult = 0;
	if (pNMTreeView->action != TVE_EXPAND)
		return;
	if (m_ResourceType == 0)
	{
		ExpandViewItem(pNMTreeView->itemNew.hItem);
		return;
	}
	if (!IsCommandMode())
		return;
	HTREEITEM h = pNMTreeView->itemNew.hItem;
	if (m_ResourceTree.GetChildItem(h) != NULL)
		return;
	int id = (int)m_ResourceTree.GetItemData(h);
	const std::vector<int> &callees = m_Commands.GetCallees(id);
	for (size_t i = 0; i < callees.size(); i++)
	{
		InsertLazy(m_Commands.Label(callees[i]), h, callees[i], !m_Commands.GetCallees(callees[i]).empty());
	}
}

// Raw bytes of a function (header, byte code and padding) inside the firmware image
bool CLayoutDialog::CmdRecord(int functionId, LPBYTE *pBuf, UINT *pLen)
{
	const Function *pf = m_Commands.FindFunction(functionId);
	if (pf == NULL || m_pFirmware == NULL)
		return false;
	DWORD len = m_Commands.Script().GetRecordLength(*pf);
	DWORD start = m_Commands.GetFunctionOffset(*pf);
	if (start + len > m_pFirmware->GetFirmwareSize())
		len = m_pFirmware->GetFirmwareSize() - start;
	*pBuf = m_pFirmware->GetFirmwareBuffer() + start;
	*pLen = len;
	return true;
}

// Read a function from a file into the firmware. The file has to be this function: same ID and same code length
// (the length of a function cannot change, everything behind it would have to move).
bool CLayoutDialog::CmdReadRecord(CFile &file, int functionId, LPBYTE dest, UINT len)
{
	const Function *pf = m_Commands.FindFunction(functionId);
	if (pf == NULL || len < 12)
	{
		file.Close();
		MessageBox(_T("This is not a function of the command table."));
		return false;
	}
	BYTE *tmp = new BYTE[len];
	UINT got = file.Read(tmp, len);
	file.Close();
	bool ok = (got == len);
	if (ok)
	{
		int id = (short)(tmp[0] | (tmp[1] << 8));
		int codeLen = tmp[8] | (tmp[9] << 8);
		ok = (id == pf->id && codeLen == (int)pf->codeLen);
	}
	if (!ok)
	{
		delete[] tmp;
		MessageBox(_T("The resource file you chose isn't this function: its ID or the length of its code is different."));
		return false;
	}
	memcpy(dest, tmp, len);
	delete[] tmp;
	return true;
}

static Line CommentLine(const CString &text)
{
	Line l;
	l.text = _T("// ") + text;
	return l;
}

// A comment line built from parts. Some parts stand for numbers stored in the firmware (see Ref): the list frames
// them, and a click on one shows the bytes in the hex view.
class CLineBuilder
{
public:
	void Add(const CString &t) { m_Text += t; }
	void Add(const TCHAR *t) { m_Text += t; }
	void Token(const CString &t, Ref r)
	{
		r.token = t;
		m_Text += t;
		m_Refs.push_back(r);
	}
	Line Make() const
	{
		Line l;
		l.text = _T("// ") + m_Text;
		l.refs = m_Refs;
		return l;
	}

private:
	CString m_Text;
	std::vector<Ref> m_Refs;
};

// Absolute place in the firmware image a reference of a line is stored at
DWORD CLayoutDialog::CmdRefAddress(const Ref &r)
{
	return r.absolute ? (DWORD)r.off : m_Commands.GetResourceOffset() + (DWORD)r.off;
}

// How a command is used by the function that names it: the statements of that function that hold the call (a key
// handler registered, a timer started, the title and command of a game added to the games list ...), with the IDs of
// strings in them spelled out
CString CLayoutDialog::CmdCallContext(const Function &caller, int calleeId, const std::vector<int> &sites)
{
	CScript &script = m_Commands.Script();
	std::map<int, std::vector<Line> >::iterator it = m_CmdBodies.find(caller.id);
	if (it == m_CmdBodies.end())
	{
		std::vector<Line> src;
		BOOL bClean = FALSE;
		if (!script.Decompile(caller, src, &bClean))
			src.clear();
		it = m_CmdBodies.insert(std::make_pair(caller.id, src)).first;
	}
	const std::vector<Line> &body = it->second;
	if (body.empty())
		return CString();
	CString token = script.AtomText(calleeId);
	std::vector<int> cand;                         // the lines that name the command
	for (size_t k = 0; k < body.size(); k++)
		if (FindToken(body[k].text, token, 0) >= 0)
			cand.push_back((int)k);
	if (cand.empty())
		return CString();
	CString all;
	std::set<int> shown;
	for (size_t i = 0; i < sites.size(); i++)
	{
		int pick = -1;
		if (cand.size() == sites.size())
			pick = cand[i];                         // in code order, one line per place
		else
		{
			int bestDist = INT_MAX;                 // else the line whose instruction is nearest to the place
			for (size_t c = 0; c < cand.size(); c++)
			{
				int d = abs(body[cand[c]].off - sites[i]);
				if (body[cand[c]].off >= 0 && d < bestDist)
				{
					bestDist = d;
					pick = cand[c];
				}
			}
		}
		if (pick < 0 || !shown.insert(pick).second)
			continue;
		const Line &l = body[pick];
		CString t = l.text;
		t.Trim();
		for (size_t r = 0; r < l.refs.size(); r++)
		{
			const Ref &ref = l.refs[r];
			if (ref.type != REF_CONSTANT || !CmdConstantResolves(ref.value))
				continue;
			std::vector<CLayoutNames::Match> matches;
			m_Names.Probe(ref.value, matches);
			for (size_t m = 0; m < matches.size(); m++)
				if (matches[m].kind == CLayoutNames::MATCH_STRING && !matches[m].text.IsEmpty())
				{
					t.AppendFormat(_T("   [%d is '%s']"), ref.value, (LPCTSTR)matches[m].text);
					break;
				}
		}
		if (!all.IsEmpty())
			all += _T("  |  ");
		all += t;
	}
	return all;
}

// One place a function is used: a menu entry that runs it, a function that calls it, the list of entry points
Line CLayoutDialog::CmdUseLine(int functionId, const CCommandTable::Use &u, int indent)
{
	CLineBuilder b;
	b.Add(CString(_T(' '), indent));
	CString offText, offTip;
	Ref r;
	r.off = (int)u.addr;
	r.len = (int)u.len;
	r.absolute = true;
	switch (u.kind)
	{
	case CCommandTable::USE_ENTRY:
		r.type = REF_NONE;
		b.Token(_T("Entry point"), r);
		b.Add(_T(": Ran at boot time"));
		break;
	case CCommandTable::USE_SELECT:
	case CCommandTable::USE_HILITE:
		{
			bool bSelect = u.kind == CCommandTable::USE_SELECT;
			r.type = REF_MENU;
			r.value = u.id;
			r.item = u.item;
			r.tip.Format(_T("Menu %d entry %d: runs command %d when %s."),
			             u.id, u.item + 1, functionId, bSelect ? _T("chosen") : _T("highlighted"));
			CString where;
			where.Format(_T("menu %d, entry %d"), u.id, u.item + 1);
			b.Add(bSelect ? _T("Run when chosen: ") : _T("Run when highlighted: "));
			b.Token(where, r);
			CString text = m_Commands.MenuEntryText(u.id, u.item);
			if (!text.IsEmpty())
				b.Add(_T(" '") + text + _T("'"));
		}
		break;
	case CCommandTable::USE_CALLER:
		{
			// "Called by 1387 (name), at offset 12, 40"; the number is where the caller is, the others where it names this
			const Function *pc = m_Commands.FindFunction(u.id);
			CString id;
			id.Format(_T("%d"), u.id);
			b.Add(_T("Called by command "));
			if (pc)
			{
				r.off = (int)m_Commands.GetFunctionOffset(*pc);
				r.len = 2;
			}
			r.type = REF_FUNCTION;
			r.value = u.id;
			r.tip.Format(_T("Start of command %d record."), u.id);
			b.Token(id, r);
			CString name = m_Commands.Name(u.id);
			if (!name.IsEmpty())
				b.Add(_T(" (") + name + _T(")"));
			const std::vector<int> &sites = m_Commands.GetSites(u.id, functionId);
			for (size_t i = 0; i < sites.size() && pc; i++)
			{
				Ref s;
				s.type = REF_NONE;
				s.absolute = true;
				s.off = (int)(m_Commands.GetCodeOffset(*pc) + sites[i] + 1);
				s.len = 2;
				s.bigEndian = true;
				CString at;
				at.Format(_T("%d"), sites[i]);
				offText += (offText.IsEmpty() ? _T("") : _T(", ")) + at;
				b.Add(i == 0 ? (sites.size() == 1 ? _T(", at offset ") : _T(", at offsets ")) : _T(", "));
				b.Token(at, s);
			}
			if (pc && !sites.empty())
			{
				CString ctx = CmdCallContext(*pc, functionId, sites);
				if (!ctx.IsEmpty())
					b.Add(_T(":  ") + ctx);
			}
		}
		break;
	}
	Line l = b.Make();
	l.offText = offText;
	l.offTip = offTip;
	return l;
}

// One command a function refers to, and the places in the function that name it
Line CLayoutDialog::CmdCalleeLine(int functionId, int callee)
{
	CLineBuilder b;
	CString offText;
	b.Add(_T("  "));
	const Function *pf = m_Commands.FindFunction(functionId);
	const Function *pc = m_Commands.FindFunction(callee);
	Ref r;
	r.type = REF_FUNCTION;
	r.value = callee;
	r.absolute = true;
	if (pc)
	{
		r.off = (int)m_Commands.GetFunctionOffset(*pc);
		r.len = 2;
	}
	r.tip.Format(_T("Start of command %d record."), callee);
	CString id;
	id.Format(_T("%d"), callee);
	b.Add(_T("command "));
	b.Token(id, r);
	CString name = m_Commands.Name(callee);
	if (!name.IsEmpty())
		b.Add(_T(" (") + name + _T(")"));
	const std::vector<int> &sites = m_Commands.GetSites(functionId, callee);
	for (size_t i = 0; i < sites.size() && pf; i++)
	{
		Ref s;
		s.type = REF_NONE;
		s.absolute = true;
		s.off = (int)(m_Commands.GetCodeOffset(*pf) + sites[i] + 1);
		s.len = 2;
		s.bigEndian = true;
		CString at;
		at.Format(_T("%d"), sites[i]);
		offText += (offText.IsEmpty() ? _T("") : _T(", ")) + at;
		b.Add(i == 0 ? (sites.size() == 1 ? _T(", at offset ") : _T(", at offsets ")) : _T(", "));
		b.Token(at, s);
	}
	if (pf && !sites.empty())
	{
		CString ctx = CmdCallContext(*pf, callee, sites);
		if (!ctx.IsEmpty())
			b.Add(_T(":  ") + ctx);
	}
	Line l = b.Make();
	l.offText = offText;
	if (!offText.IsEmpty())
		l.offTip = _T("Byte code offsets in this command (counted from the start of its code, after the 10 byte header).");
	return l;
}

// The lines of a function's listing: what it is, where it is used and what it refers to, then the code, as source or assembly
void CLayoutDialog::CmdBuildLines(int functionId, bool bSource, std::vector<Line> &lines)
{
	lines.clear();
	m_CmdBodies.clear();
	const Function *pf = m_Commands.FindFunction(functionId);
	if (pf == NULL)
		return;
	CScript &script = m_Commands.Script();

	// what the function is: its size (the header of its record is where the line stands)
	{
		CLineBuilder b;
		b.Add(m_Commands.Signature(functionId));
		Line l = b.Make();
		Ref r;
		r.type = REF_NONE;
		r.absolute = true;
		r.off = (int)m_Commands.GetFunctionOffset(*pf);
		r.len = 10;
		r.tip = _T("Command header: ID, parameter count, variable count, stack depth, code length. 2 bytes each, little endian.");
		l.refs.push_back(r);
		lines.push_back(l);
	}

	// who runs it: every place, one row each
	const std::vector<CCommandTable::Use> &uses = m_Commands.GetUses(functionId);
	if (uses.empty())
		lines.push_back(CommentLine(_T("Not used by any menu entry or other command")));
	else
	{
		CString head;
		head.Format(_T("Used by %d place%s:"), (int)uses.size(), uses.size() == 1 ? _T("") : _T("s"));
		lines.push_back(CommentLine(head));
		for (size_t i = 0; i < uses.size(); i++)
			lines.push_back(CmdUseLine(functionId, uses[i]));
	}

	// what it runs: every command it refers to, one row each
	const std::vector<int> &callees = m_Commands.GetCallees(functionId);
	if (!callees.empty())
	{
		CString head;
		head.Format(_T("Refers to %d command%s (and who else uses each):"), (int)callees.size(), callees.size() == 1 ? _T("") : _T("s"));
		lines.push_back(CommentLine(head));
		for (size_t i = 0; i < callees.size(); i++)
		{
			lines.push_back(CmdCalleeLine(functionId, callees[i]));

			// who else runs that command (the other commands that call it, the menu entries that choose it)
			const std::vector<CCommandTable::Use> &others = m_Commands.GetUses(callees[i]);
			int nOther = 0;
			for (size_t k = 0; k < others.size(); k++)
				if (!(others[k].kind == CCommandTable::USE_CALLER && others[k].id == functionId))
					nOther++;
			if (nOther > 0)
			{
				CString oh;
				oh.Format(_T("      and %d other place%s:"), nOther, nOther == 1 ? _T("") : _T("s"));
				lines.push_back(CommentLine(oh.Mid(0)));
				for (size_t k = 0; k < others.size(); k++)
					if (!(others[k].kind == CCommandTable::USE_CALLER && others[k].id == functionId))
						lines.push_back(CmdUseLine(callees[i], others[k], 8));
			}
		}
	}
	lines.push_back(Line());

	// the code
	std::vector<Line> body;
	if (bSource)
	{
		CString head;
		CString name = m_Commands.Name(functionId);
		if (name.IsEmpty())
			name = script.AtomText(functionId);
		head.Format(_T("function %s("), (LPCTSTR)name);
		for (int i = 0; i < pf->nParams; i++)
		{
			CString a;
			a.Format(i ? _T(", arg%d") : _T("arg%d"), i);
			head += a;
		}
		head += _T(") {");
		Line hl;
		hl.text = head;
		lines.push_back(hl);

		BOOL bClean = FALSE;
		if (!script.Decompile(*pf, body, &bClean))
			lines.push_back(CommentLine(_T("This function's byte code cannot be read")));
		else
		{
			if (!bClean)
				lines.push_back(CommentLine(_T("Not everything in this function could be turned back into source")));
			for (size_t i = 0; i < body.size(); i++)
				lines.push_back(body[i]);
		}
		Line tl;
		tl.text = _T("}");
		lines.push_back(tl);
	}
	else
	{
		if (!script.Disassemble(*pf, body))
			lines.push_back(CommentLine(_T("This function's byte code cannot be read")));
		else
			for (size_t i = 0; i < body.size(); i++)
				lines.push_back(body[i]);
	}
}

// One listing line as plain text
CString CLayoutDialog::CmdLineText(const Line &l, const Function *pf, bool bSource)
{
	CString text(_T(' '), l.indent * 4);
	text += l.text;
	if (bSource)
		return text;
	CString off, bytes;
	if (l.off >= 0)
		off.Format(_T("%d"), l.off);
	else
		off = l.offText;
	if (pf != NULL && l.off >= 0 && l.len > 0 && l.off + l.len <= (int)pf->codeLen)
		bytes = HexBytes(m_Commands.Script().GetCode(*pf) + l.off, l.len);
	CString s;
	s.Format(_T("%6s  %-20s  %s"), (LPCTSTR)off, (LPCTSTR)bytes, (LPCTSTR)text);
	return s;
}

// Where the bytes of a line are in the firmware image: the instruction it comes from, or what its first reference is stored in
bool CLayoutDialog::CmdLineAddress(const Line &l, const Function *pf, DWORD *pOff, DWORD *pLen)
{
	if (pf != NULL && l.off >= 0 && l.off < (int)pf->codeLen)
	{
		*pOff = m_Commands.GetCodeOffset(*pf) + l.off;
		int len = l.len;
		if (len <= 0)
		{
			std::map<int, int>::const_iterator it = m_CmdInsLen.find(l.off);
			len = it == m_CmdInsLen.end() ? 1 : it->second;
		}
		*pLen = (DWORD)len;
		return true;
	}
	const Ref *pr = CmdPrimaryRef(l);
	if (pr == NULL)
		return false;
	*pOff = CmdRefAddress(*pr);
	*pLen = (DWORD)pr->len;
	return true;
}

// What a row that is not code stands for in the firmware: the first plain location it has (where the reference is
// stored: the operand of a call, the place of an entry), else the first part of it that has a place
const Ref *CLayoutDialog::CmdPrimaryRef(const Line &l)
{
	const Ref *first = NULL;
	for (size_t i = 0; i < l.refs.size(); i++)
	{
		if (l.refs[i].off < 0 || l.refs[i].len <= 0)
			continue;
		if (l.refs[i].type == REF_NONE)
			return &l.refs[i];
		if (first == NULL)
			first = &l.refs[i];
	}
	return first;
}

// Does a number from the code look like the ID of something in this firmware? (Those are the ones that are marked.)
bool CLayoutDialog::CmdConstantResolves(int value)
{
	if (value > -2 && value < 1000)
		return false;
	std::vector<CLayoutNames::Match> matches;
	m_Names.Probe(value, matches);
	return !matches.empty();
}

// The tooltip of a marked part: what it points at, and where it is stored and how
CString CLayoutDialog::CmdLinkTip(const Ref &r)
{
	CString s = r.tip;
	if (s.IsEmpty())
		s = CmdRefTip(r);
	return s;
}

void CLayoutDialog::CmdFillList(int functionId)
{
	m_ResourceList.DeleteAllItems();
	m_CmdLines.clear();
	m_CmdInsLen.clear();
	m_CmdFunction = 0;
	const Function *pf = m_Commands.FindFunction(functionId);
	if (pf == NULL)
		return;
	m_CmdFunction = functionId;
	bool bSource = (m_ResourceType == 3);
	CmdBuildLines(functionId, bSource, m_CmdLines);
	int tcol = bSource ? 2 : 3;                // the column of the text of a line

	std::vector<Instruction> ins;
	if (m_Commands.Script().Decode(*pf, ins))
		for (size_t i = 0; i < ins.size(); i++)
			m_CmdInsLen[ins[i].off] = ins[i].len;

	// into the list
	const BYTE *code = m_Commands.Script().GetCode(*pf);
	DWORD codeBase = m_Commands.GetCodeOffset(*pf);
	m_ResourceList.SetRedraw(FALSE);
	for (size_t i = 0; i < m_CmdLines.size(); i++)
	{
		const Line &l = m_CmdLines[i];
		int row = (int)i;
		CString off;
		if (l.off >= 0)
			off.Format(_T("%d"), l.off);
		else
			off = l.offText;
		m_ResourceList.InsertItem(row, off);
		if (l.off < 0 && !l.offTip.IsEmpty())
			m_ResourceList.SetItemToolTipText(row, 0, l.offTip);

		// where the line is stored: its address, framed
		DWORD aoff, alen;
		if (CmdLineAddress(l, pf, &aoff, &alen))
		{
			CString addr;
			addr.Format(_T("%08X"), aoff);
			m_ResourceList.SetItemText(row, 1, addr);
			CCellLink al;
			al.start = 0;
			al.len = addr.GetLength();
			al.offset = aoff;
			al.size = alen;
			{
				const Ref *pr0 = CmdPrimaryRef(l);
				if (pr0 != NULL && pr0->bigEndian && alen == 2)
					al.bigEndian = true;                 // an operand of the byte code
			}
			if (l.off >= 0 && l.off < (int)pf->codeLen)
			{
				if (l.len > 0 && l.off + l.len <= (int)pf->codeLen)
					al.tip = PixoScript::OpName(code[l.off]);
			}
			else
			{
				// a row about a use or a reference: the part it stands for has the tip
				const Ref *pr = CmdPrimaryRef(l);
				if (pr != NULL)
					al.tip = pr->tip.IsEmpty() ? CmdRefTip(*pr) : pr->tip;
			}
			std::vector<CCellLink> alinks;
			alinks.push_back(al);
			m_ResourceList.SetCellLinks(row, 1, alinks);
		}

		CString text(_T(' '), l.indent * 4);
		text += l.text;
		CString bytes;
		if (bSource)
			m_ResourceList.SetItemText(row, tcol, text);
		else
		{
			if (l.off >= 0 && l.len > 0 && l.off + l.len <= (int)pf->codeLen)
				bytes = HexBytes(code + l.off, l.len);
			m_ResourceList.SetItemText(row, 2, bytes);
			m_ResourceList.SetItemText(row, tcol, text);
		}

		// the parts of the text that stand for stored numbers
		std::vector<CCellLink> links, blinks;
		int cursor = l.indent * 4;
		for (size_t r = 0; r < l.refs.size(); r++)
		{
			const Ref &ref = l.refs[r];
			if (ref.token.IsEmpty() || ref.off < 0 || ref.len <= 0)
				continue;
			if (ref.type == REF_CONSTANT && !CmdConstantResolves(ref.value))
				continue;
			int pos = FindToken(text, ref.token, cursor);
			if (pos < 0)
				continue;
			cursor = pos + ref.token.GetLength();
			CCellLink link;
			link.start = pos;
			link.len = ref.token.GetLength();
			link.offset = CmdRefAddress(ref);
			link.size = (DWORD)ref.len;
			link.kind = 2;
			link.data = (DWORD_PTR)r;
			link.tip = CmdLinkTip(ref);
			link.bigEndian = ref.bigEndian;
			links.push_back(link);

			// in the assembly the bytes of the instruction are shown too: the ones of its operand are marked as well
			if (!bSource && ref.insOff >= 0 && l.off >= 0 && l.len == 3 && (DWORD)ref.insOff == codeBase - m_Commands.GetResourceOffset() + l.off + 1)
			{
				CCellLink bl;
				bl.start = 3;
				bl.len = 5;
				bl.offset = m_Commands.GetResourceOffset() + (DWORD)ref.insOff;
				bl.size = 2;
				bl.kind = 2;
				bl.data = (DWORD_PTR)r;
				bl.tip = _T("Operand: 16 bit, big endian.");
				bl.bigEndian = true;
				blinks.push_back(bl);
			}
		}
		if (!links.empty())
			m_ResourceList.SetCellLinks(row, tcol, links);
		if (!blinks.empty())
			m_ResourceList.SetCellLinks(row, 2, blinks);

		if (!l.refs.empty())
		{
			CString tip;
			for (size_t r = 0; r < l.refs.size(); r++)
			{
				CString t = CmdRefTip(l.refs[r]);
				if (t.IsEmpty())
					continue;
				if (!tip.IsEmpty())
					tip += _T("\n");
				tip += t;
			}
			if (!tip.IsEmpty())
			{
				tip += _T("\nDouble click: open.");
				m_ResourceList.SetItemToolTipText(row, tcol, tip);
			}
		}
	}
	// the column of the text is as wide as its longest line (the rows about the places a command is used can be long)
	m_ResourceList.SetColumnWidth(tcol, LVSCW_AUTOSIZE);
	if (m_ResourceList.GetColumnWidth(tcol) < 500)
		m_ResourceList.SetColumnWidth(tcol, 500);
	m_ResourceList.SetRedraw(TRUE);

	// the number of the command is the first thing in its record (shown without taking the keyboard from the tree)
	m_HexWnd.SetRange(m_Commands.GetFunctionOffset(*pf), 2, FALSE);
}

// Right click: copy the listing, or save every command as text
void CLayoutDialog::OnNMRClickResourceList(NMHDR *pNMHDR, LRESULT *pResult)
{
	*pResult = 0;
	if (!IsCommandMode() || m_CmdFunction == 0)
		return;
	CPoint pt;
	GetCursorPos(&pt);
	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING, 1, _T("Copy the selected lines"));
	menu.AppendMenu(MF_STRING, 2, _T("Copy this whole function"));
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(MF_STRING, 3, _T("Save every command as text (source)..."));
	menu.AppendMenu(MF_STRING, 4, _T("Save every command as text (assembly)..."));
	int cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, this);
	if (cmd == 0)
		return;

	bool bSource = (m_ResourceType == 3);
	const Function *pf = m_Commands.FindFunction(m_CmdFunction);
	if (cmd == 1 || cmd == 2)
	{
		CString text;
		for (int i = 0; i < (int)m_CmdLines.size(); i++)
		{
			if (cmd == 1 && m_ResourceList.GetItemState(i, LVIS_SELECTED) == 0)
				continue;
			text += CmdLineText(m_CmdLines[i], pf, bSource) + _T("\r\n");
		}
		if (text.IsEmpty() || !OpenClipboard())
			return;
		EmptyClipboard();
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (text.GetLength() + 1) * sizeof(TCHAR));
		if (hMem)
		{
			memcpy(GlobalLock(hMem), (LPCTSTR)text, (text.GetLength() + 1) * sizeof(TCHAR));
			GlobalUnlock(hMem);
			SetClipboardData(CF_UNICODETEXT, hMem);
		}
		CloseClipboard();
		return;
	}

	bSource = (cmd == 3);
	CFileDialog dlg(FALSE, TEXT("txt"), bSource ? _T("commands_source.txt") : _T("commands_assembly.txt"), OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
	                TEXT("Text files (*.txt)|*.txt||"), this);
	if (dlg.DoModal() != IDOK)
		return;
	CWaitCursor wait;
	CString text;
	for (int i = 0; i < m_Commands.GetNumFunctions(); i++)
	{
		int id = m_Commands.GetFunctionId(i);
		std::vector<Line> lines;
		CmdBuildLines(id, bSource, lines);
		const Function *pfi = m_Commands.FindFunction(id);
		for (size_t k = 0; k < lines.size(); k++)
			text += CmdLineText(lines[k], pfi, bSource) + _T("\r\n");
		text += _T("\r\n");
	}
	int n = WideCharToMultiByte(CP_UTF8, 0, text, text.GetLength(), NULL, 0, NULL, NULL);
	CFile file;
	if (n <= 0 || !file.Open(dlg.GetPathName(), CFile::modeCreate | CFile::modeWrite))
	{
		MessageBox(_T("Can't create the text file!"));
		return;
	}
	char *utf8 = new char[n];
	WideCharToMultiByte(CP_UTF8, 0, text, text.GetLength(), utf8, n, NULL, NULL);
	static const BYTE bom[3] = { 0xEF, 0xBB, 0xBF };
	file.Write(bom, 3);
	file.Write(utf8, n);
	file.Close();
	delete[] utf8;
	CString msg;
	msg.Format(_T("Saved %d commands."), m_Commands.GetNumFunctions());
	MessageBox(msg);
}

// What a reference in the listing points at, for the tooltip
CString CLayoutDialog::CmdRefTip(const Ref &ref)
{
	CString s;
	switch (ref.type)
	{
	case REF_FUNCTION:
		{
			const Function *pf = m_Commands.FindFunction(ref.value);
			if (pf == NULL)
				return s;
			CString name = m_Commands.Name(ref.value);
			CString role = m_Commands.Role(ref.value);
			s.Format(_T("Command %d"), ref.value);
			if (!name.IsEmpty())
				s += _T(" (") + name + _T(")");
			s.AppendFormat(_T(": %d param%s, %d bytes of code"), pf->nParams, pf->nParams == 1 ? _T("") : _T("s"), (int)pf->codeLen);
			if (!role.IsEmpty())
				s += _T(". ") + role;
		}
		break;
	case REF_MENU:
		s.Format(_T("Menu %d: %s"), ref.value, (LPCTSTR)m_Names.Menu(ref.value));
		break;
	case REF_CONSTANT:
		{
			if (ref.value > -2 && ref.value < 1000)
				return s;                          // too small to be an ID
			std::vector<CLayoutNames::Match> matches;
			m_Names.Probe(ref.value, matches);
			for (size_t i = 0; i < matches.size(); i++)
			{
				if (!s.IsEmpty())
					s += _T("\n");
				s.AppendFormat(_T("%d is %s %d"), ref.value, CLayoutNames::KindName(matches[i].kind), matches[i].id);
				if (!matches[i].text.IsEmpty())
					s += _T(": ") + matches[i].text;
			}
		}
		break;
	}
	return s;
}

void CLayoutDialog::CmdSelectRow(int row)
{
	m_bLinkClick = false;
	if (row < 0 || row >= (int)m_CmdLines.size())
		return;
	const Function *pf = m_Commands.FindFunction(m_CmdFunction);
	if (pf == NULL)
		return;
	const Line &l = m_CmdLines[row];
	DWORD off, len;
	m_ValSize = 0;
	if (l.off >= 0 && l.off < (int)pf->codeLen)
	{
		if (m_ResourceType == 4 && l.len == 3)
		{
			m_ValSize = 2;                       // the operand
			m_ValOffset = m_Commands.GetCodeOffset(*pf) + l.off + 1;
		}
		else
			m_ValOffset = m_Commands.GetCodeOffset(*pf) + l.off;
	}
	else
		m_ValOffset = m_Commands.GetFunctionOffset(*pf);
	if (CmdLineAddress(l, pf, &off, &len))
		m_HexWnd.SetRange(off, len);
	else
		m_HexWnd.SetRange(m_Commands.GetFunctionOffset(*pf), 2);
}

void CLayoutDialog::CmdActivateRow(int row, CPoint screenPt)
{
	if (row < 0 || row >= (int)m_CmdLines.size())
		return;
	GoToRef(m_CmdLines[row].refs, screenPt);
}

// Select an entry of one of the modes (0 layouts, 1 menus, 2 font types, 3/4 commands) by its ID, optionally a row of its
// list as well; hexLen > 0 marks bytes in the hex view and makes them the value Change Value changes (col: its column)
void CLayoutDialog::SelectResource(int mode, int id, int row, DWORD hexOff, DWORD hexLen, int col)
{
	if (mode == 3)                                  // the commands: shown the way they are shown now (or were last)
		mode = IsCommandMode() ? m_ResourceType : (m_LastCmdMode == 4 ? 4 : 3);
	if (m_ResourceTypeIdx.GetCurSel() != mode)
	{
		m_ResourceTypeIdx.SetCurSel(mode);
		m_bNoRestore = true;                        // what the mode had selected is replaced by what is wanted
		OnCbnSelchangeResourceTypeidxCombo();
		m_bNoRestore = false;
	}
	HTREEITEM h = m_ResourceTree.GetRootItem();
	while (h != NULL)
	{
		if (_ttoi(m_ResourceTree.GetItemText(h)) == id)
		{
			m_ResourceTree.SelectItem(h);
			m_ResourceTree.EnsureVisible(h);
			if (row >= 0 && row < m_ResourceList.GetItemCount())
			{
				m_ResourceList.SetItemState(row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
				m_ResourceList.EnsureVisible(row, FALSE);
			}
			if (hexLen > 0)
			{
				m_ClickedCol = col;
				m_ValOffset = hexOff;
				m_ValSize = hexLen;
				m_HexWnd.SetRange(hexOff, hexLen);
				RefreshChangeCue();
			}
			return;
		}
		h = m_ResourceTree.GetNextSiblingItem(h);
	}
	CString s;
	s.Format(_T("%d is not in this list."), id);
	MessageBox(s);
}

void CLayoutDialog::GoToMatch(int kind, int id, int item)
{
	switch (kind)
	{
	case kFunctionMatch:
		SelectResource(m_ResourceType, id);
		break;
	case CLayoutNames::MATCH_STRING:
		::SendMessage(::GetParent(::GetParent(m_hWnd)), WM_APP, (WPARAM)11, (LPARAM)id);
		break;
	case CLayoutNames::MATCH_BITMAP:
		::SendMessage(::GetParent(::GetParent(m_hWnd)), WM_APP, (WPARAM)12, (LPARAM)m_pFirmware->GetPictureIndexFromID((DWORD)id));
		break;
	case CLayoutNames::MATCH_VIEW:
		SelectResource(0, id);
		break;
	case CLayoutNames::MATCH_MENU:
		SelectResource(1, id, item);
		break;
	case CLayoutNames::MATCH_TYPE:
		SelectResource(2, id);
		break;
	}
}

// Open whatever the references of a line point at: directly when there is one place to go, from a menu otherwise
struct GoCandidate
{
	int kind;
	int id;
	int item;                 // for a menu: the entry to select (counting from 0), else -1
	CString text;
};

void CLayoutDialog::GoToRef(const std::vector<Ref> &refs, CPoint screenPt)
{
	typedef GoCandidate Candidate;
	std::vector<Candidate> list;
	bool bAnyRef = false;
	bool bAskedAboutNumber = false;           // a number that looks like an ID was found, but nothing has that ID
	for (size_t i = 0; i < refs.size(); i++)
	{
		Candidate c;
		c.item = -1;
		if (refs[i].type == REF_FUNCTION)
		{
			bAnyRef = true;
			if (m_Commands.FindFunction(refs[i].value) == NULL)
				continue;
			c.kind = kFunctionMatch;
			c.id = refs[i].value;
			CString name = m_Commands.Name(c.id);
			c.text.Format(_T("Function %d"), c.id);
			if (!name.IsEmpty())
				c.text += _T(" (") + name + _T(")");
			list.push_back(c);
		}
		else if (refs[i].type == REF_MENU)
		{
			bAnyRef = true;
			c.kind = CLayoutNames::MATCH_MENU;
			c.id = refs[i].value;
			c.item = refs[i].item;
			c.text.Format(_T("Menu %d"), c.id);
			list.push_back(c);
		}
		else if (refs[i].type == REF_CONSTANT)
		{
			bAnyRef = true;
			if (refs[i].value > -2 && refs[i].value < 1000)
				continue;
			std::vector<CLayoutNames::Match> matches;
			m_Names.Probe(refs[i].value, matches);
			if (matches.empty())
				bAskedAboutNumber = true;
			for (size_t m = 0; m < matches.size(); m++)
			{
				c.kind = matches[m].kind;
				c.id = matches[m].id;
				c.text.Format(_T("The %s %d"), CLayoutNames::KindName(matches[m].kind), matches[m].id);
				if (!matches[m].text.IsEmpty())
					c.text += _T(": ") + matches[m].text;
				list.push_back(c);
			}
		}
	}
	if (!bAnyRef)
		return;
	if (list.empty())
	{
		if (bAskedAboutNumber)
			MessageBox(_T("That number is not the ID of a string, bitmap, layout, menu or font type in this firmware."));
		return;
	}
	if (list.size() == 1)
	{
		GoToMatch(list[0].kind, list[0].id, list[0].item);
		return;
	}
	CMenu menu;
	menu.CreatePopupMenu();
	for (size_t i = 0; i < list.size(); i++)
		menu.AppendMenu(MF_STRING, 1000 + (UINT)i, list[i].text);
	int sel = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, screenPt.x, screenPt.y, this);
	if (sel >= 1000 && sel < 1000 + (int)list.size())
		GoToMatch(list[sel - 1000].kind, list[sel - 1000].id, list[sel - 1000].item);
}

// The value that Change Value would change, in the form that is typed (as Decimal, Hex or String, whichever is picked). It is
// shown in grey in the box while the box is empty: a click on the box or a key makes it go.
CString CLayoutDialog::ChangeCueText()
{
	if (m_pFirmware == NULL || m_ValSize == 0)
		return CString();
	LPBYTE fw = m_pFirmware->GetFirmwareBuffer();
	if ((ULONGLONG)m_ValOffset + m_ValSize > m_pFirmware->GetFirmwareSize())
		return CString();
	BOOL bHex = IsDlgButtonChecked(IDC_RADIO_HEXVAL) == 1, bString = IsDlgButtonChecked(IDC_RADIO_STRINGVAL) == 1;
	CString s;

	// a number: the command of a menu entry (its number, not the way it is stored) or the number an instruction carries
	long number = 0;
	bool bNumber = false;
	if (m_ResourceType == 1 && (m_ClickedCol == 2 || m_ClickedCol == 3) && m_ValSize == 4)
	{
		DWORD raw = *(DWORD *)(fw + m_ValOffset);
		if ((raw & 3) == 2)
		{
			number = (long)(raw >> 2);
			bNumber = true;
		}
	}
	else if (m_ResourceType == 4 && m_ValSize == 2)
	{
		POSITION pos = m_ResourceList.GetFirstSelectedItemPosition();
		int row = pos ? m_ResourceList.GetNextSelectedItem(pos) : -1;
		const Function *pf = m_Commands.FindFunction(m_CmdFunction);
		if (pf != NULL && row >= 0 && row < (int)m_CmdLines.size() && m_CmdLines[row].off >= 0 && m_CmdLines[row].len == 3)
		{
			const BYTE *code = m_Commands.Script().GetCode(*pf);
			int op = code[m_CmdLines[row].off];
			int operand = (code[m_CmdLines[row].off + 1] << 8) | code[m_CmdLines[row].off + 2];
			DWORD v = (DWORD)operand;
			if (op == 0x3c)
				m_Commands.Script().GetNumberValue(operand, &v);
			number = (long)v;
			bNumber = true;
		}
	}
	if (bNumber)
	{
		if (bString)
			return CString();
		s.Format(bHex ? _T("%X") : _T("%ld"), number);
		return s;
	}
	if ((m_ResourceType == 3 || m_ResourceType == 4) && !(m_bLinkClick && m_ValSize >= 1 && m_ValSize <= 4 && m_ValSize != 3))
		return CString();

	const BYTE *p = fw + m_ValOffset;
	if (bString)
	{
		for (DWORD i = 0; i < m_ValSize && i < 60; i++)
		{
			if (p[i] < 0x20 || p[i] > 0x7E)
				return CString();
			s += (TCHAR)p[i];
		}
		return s;
	}
	if (bHex)
		return m_ValSize <= 16 ? HexBytes(p, (int)m_ValSize) : CString();
	if (m_ValSize > 4)
		return CString();
	DWORD v = 0;
	for (DWORD i = 0; i < m_ValSize; i++)
		v |= (DWORD)p[i] << (8 * (m_bValBigEndian ? m_ValSize - 1 - i : i));
	s.Format(_T("%u"), v);
	return s;
}

void CLayoutDialog::RefreshChangeCue()
{
	if (m_ChangeEdit.m_hWnd != NULL)
		m_ChangeEdit.SetCue(ChangeCueText());
}

void CLayoutDialog::OnBnClickedValueKind()
{
	RefreshChangeCue();
}

// "Change Value" in the assembly listing: the number the selected instruction carries
bool CLayoutDialog::CmdChangeValue()
{
	if (m_ResourceType != 4)
	{
		MessageBox(_T("Switch to the assembly listing (Command Table (Assembly)) to change the numbers in the code."));
		return true;
	}
	POSITION pos = m_ResourceList.GetFirstSelectedItemPosition();
	int row = pos ? m_ResourceList.GetNextSelectedItem(pos) : -1;
	const Function *pf = m_Commands.FindFunction(m_CmdFunction);
	if (row < 0 || row >= (int)m_CmdLines.size() || pf == NULL)
	{
		MessageBox(_T("Select an instruction first."));
		return true;
	}
	const Line &l = m_CmdLines[row];
	if (l.off < 0 || l.len != 3)
	{
		MessageBox(_T("This line has no number that can be changed. Only instructions that carry a number can be (their bytes are shown in the hex view)."));
		return true;
	}
	if (IsDlgButtonChecked(IDC_RADIO_STRINGVAL) == 1)
	{
		MessageBox(_T("Type the number as Decimal or Hex."));
		return true;
	}
	long value = 0;
	if (IsDlgButtonChecked(IDC_RADIO_HEXVAL) == 1)
	{
		if (_stscanf(m_ChangeString, _T("%x"), &value) != 1)
		{
			MessageBox(_T("That is not a hex number."));
			return true;
		}
	}
	else if (_stscanf(m_ChangeString, _T("%ld"), &value) != 1)
	{
		MessageBox(_T("That is not a number."));
		return true;
	}

	CScript &script = m_Commands.Script();
	const BYTE *code = script.GetCode(*pf);
	int op = code[l.off];
	LPBYTE fw = m_pFirmware->GetFirmwareBuffer();

	if (op == 0x3c)       // number: the operand points into the table of constants, so it is the constant that changes
	{
		int operand = (code[l.off + 1] << 8) | code[l.off + 2];
		DWORD at = script.GetNumberOffset(operand);
		if (at == 0xFFFFFFFF)
		{
			MessageBox(_T("This constant is not in the table."));
			return true;
		}
		int uses = 0;
		for (int f = 0; f < script.GetNumFunctions(); f++)
		{
			std::vector<Instruction> ins;
			if (!script.Decode(script.GetFunction(f), ins))
				continue;
			for (size_t k = 0; k < ins.size(); k++)
				if (ins[k].op == 0x3c && ins[k].arg == operand)
					uses++;
		}
		CString msg;
		msg.Format(_T("This number is a constant that %d place%s in the command table share. Changing it changes all of them.\n\nChange it to %d?"),
		           uses, uses == 1 ? _T("") : _T("s"), (int)value);
		if (MessageBox(msg, _T("Value Changer"), MB_YESNO) != IDYES)
			return true;
		DWORD v = (DWORD)value;
		m_pFirmware->SetData(m_Commands.GetResourceOffset() + at, (LPBYTE)&v, 4);
		m_HexWnd.SetOffset(m_Commands.GetResourceOffset() + at);
	}
	else
	{
		bool bJump = (op == 0x06 || op == 0x07 || op == 0x08 || op == 0x44 || op == 0x45 || op == 0x77 || op == 0x78);
		if (bJump ? (value < -32768 || value > 32767) : (value < 0 || value > 65535))
		{
			MessageBox(bJump ? _T("A jump distance has to be between -32768 and 32767.") : _T("The number has to be between 0 and 65535."));
			return true;
		}
		BYTE b[2];
		b[0] = (BYTE)((value >> 8) & 0xFF);
		b[1] = (BYTE)(value & 0xFF);
		DWORD at = m_Commands.GetCodeOffset(*pf) + l.off + 1;
		m_pFirmware->SetData(at, b, 2);
		m_HexWnd.SetOffset(at);
	}
	m_HexWnd.Invalidate();
	int keepRow = row;
	CmdFillList(m_CmdFunction);
	if (keepRow < m_ResourceList.GetItemCount())
	{
		m_ResourceList.SetItemState(keepRow, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		m_ResourceList.EnsureVisible(keepRow, FALSE);
	}
	MessageBox(TEXT("Value changed successfully."), TEXT("Value Changer"), MB_OK);
	return true;
}

// Find (and Find Next) in the command table modes: looks through the names and the code of the functions
// (in the order of their numbers), selects the first function after the current one that contains the text and
// marks the first line of it.
void CLayoutDialog::CmdFind(bool bNext)
{
	UpdateData(TRUE);
	CString needle = m_FindString;
	needle.MakeLower();
	if (needle.IsEmpty() || !m_Commands.IsValid())
		return;
	CWaitCursor wait;
	bool bSource = (m_ResourceType == 3);
	int n = m_Commands.GetNumFunctions();
	int start = -1;
	if (bNext && m_CmdFunction != 0)
		for (int i = 0; i < n; i++)
			if (m_Commands.GetFunctionId(i) == m_CmdFunction)
			{
				start = i;
				break;
			}

	// Next lines of the current function first (a function may contain the text several times)
	int firstRow = -1;
	if (bNext && m_CmdFunction != 0)
	{
		POSITION pos = m_ResourceList.GetFirstSelectedItemPosition();
		int cur = pos ? m_ResourceList.GetNextSelectedItem(pos) : -1;
		for (int r = cur + 1; r < (int)m_CmdLines.size(); r++)
		{
			CString t = m_CmdLines[r].text;
			t.MakeLower();
			if (t.Find(needle) >= 0)
			{
				firstRow = r;
				break;
			}
		}
		if (firstRow >= 0)
		{
			m_ResourceList.SetItemState(firstRow, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			m_ResourceList.EnsureVisible(firstRow, FALSE);
			CmdSelectRow(firstRow);
			return;
		}
	}

	for (int k = 1; k <= n; k++)
	{
		int i = (start + k) % n;
		if (start < 0 && k > n)
			break;
		int id = m_Commands.GetFunctionId(i);
		CString label = m_Commands.Label(id);
		label.MakeLower();
		bool bHit = label.Find(needle) >= 0;
		std::vector<Line> lines;
		if (!bHit)
		{
			CmdBuildLines(id, bSource, lines);
			for (size_t r = 0; r < lines.size() && !bHit; r++)
			{
				CString t = lines[r].text;
				t.MakeLower();
				bHit = t.Find(needle) >= 0;
			}
		}
		if (!bHit)
			continue;
		SelectResource(m_ResourceType, id);
		for (int r = 0; r < (int)m_CmdLines.size(); r++)
		{
			CString t = m_CmdLines[r].text;
			t.MakeLower();
			if (t.Find(needle) >= 0)
			{
				m_ResourceList.SetItemState(r, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
				m_ResourceList.EnsureVisible(r, FALSE);
				CmdSelectRow(r);
				break;
			}
		}
		return;
	}
	MessageBox(_T("No (more) commands contain that text."), _T("Find"));
}

// "Change Value" on the command columns of a menu entry: the number typed is the number of a command (a function
// of the command table), which is stored the way menus store commands.
bool CLayoutDialog::MenuChangeCommand()
{
	if (m_ResourceType != 1 || (m_ClickedCol != 2 && m_ClickedCol != 3) || m_ValSize != 4)
		return false;
	if (IsDlgButtonChecked(IDC_RADIO_STRINGVAL) == 1)
	{
		MessageBox(_T("Type the number of the command as Decimal or Hex."));
		return true;
	}
	long fn = 0;
	if (IsDlgButtonChecked(IDC_RADIO_HEXVAL) == 1)
	{
		if (_stscanf(m_ChangeString, _T("%x"), &fn) != 1)
		{
			MessageBox(_T("That is not a hex number."));
			return true;
		}
	}
	else if (_stscanf(m_ChangeString, _T("%ld"), &fn) != 1)
	{
		MessageBox(_T("That is not a number."));
		return true;
	}
	if (m_Commands.FindFunction((int)fn) == NULL)
	{
		MessageBox(_T("There is no command with that number. Use a number from the Command Table list."), _T("Value Changer"));
		return true;
	}
	DWORD v = CCommandTable::FunctionToRef((int)fn);
	m_pFirmware->SetData(m_ValOffset, (LPBYTE)&v, 4);
	m_HexWnd.SetOffset(m_ValOffset);
	m_HexWnd.Invalidate();
	RefreshSelection();
	MessageBox(TEXT("Value changed successfully."), TEXT("Value Changer"), MB_OK);
	return true;
}

// Right click on an entry of the tree: find the commands that mention its number
void CLayoutDialog::OnNMRClickResourceTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	*pResult = 0;
	if (IsCommandMode() || !m_Commands.IsValid())
		return;
	CPoint pt;
	GetCursorPos(&pt);
	CPoint client = pt;
	m_ResourceTree.ScreenToClient(&client);
	UINT flags = 0;
	HTREEITEM h = m_ResourceTree.HitTest(client, &flags);
	if (h == NULL || !(flags & TVHT_ONITEM))
		return;
	m_ResourceTree.SelectItem(h);
	int id = _ttoi(m_ResourceTree.GetItemText(h));
	CString text;
	text.Format(_T("Find the commands that mention %d"), id);
	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING, 1, text);
	if (menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, this) != 1)
		return;
	m_FindString.Format(_T("%d"), id);
	UpdateData(FALSE);
	m_ResourceTypeIdx.SetCurSel(3);
	OnCbnSelchangeResourceTypeidxCombo();
	CmdFind(false);
}
