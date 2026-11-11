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

	// Do the instructions hang together (stack depth, jumps, arguments, variables)? Every command of the 52 firmwares of the collection does, so a
	// command that fails was changed, or this is a firmware that is not known.
	{
		CString why;
		if (!script.Verify(*pf, &why))
			lines.push_back(CommentLine(_T("Check failed: ") + why + _T(" The iPod may not run this command correctly.")));
	}

	// the game this command belongs to: the numbers in the firmware that decide how it plays
	if (m_Commands.IsBrickCommand(functionId))
		CmdBrickLines(lines);
	if (m_Commands.IsStackerCommand(functionId))
		CmdStackerLines(lines);

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

// Brick (BlockO): the numbers in the firmware that decide how the game plays, each one a link to its bytes. The rules are written down in
// Brick-Game-Logic.txt (research folder). The level table has one row per level: the balls, the rows of bricks (12 bricks per row, so the bricks
// to destroy are 12 x rows) and the width of the paddle.
void CLayoutDialog::CmdBrickLines(std::vector<Line> &lines)
{
	const CCommandTable::BrickInfo &bi = m_Commands.Brick();
	if (m_pFirmware == NULL || !bi.valid || bi.table == 0xFFFFFFFF)
		return;
	const BYTE *fw = m_pFirmware->GetFirmwareBuffer();
	DWORD size = m_pFirmware->GetFirmwareSize();
	if ((ULONGLONG)bi.table + 23 * 12 > size)
		return;
	lines.push_back(CommentLine(_T("Brick (BlockO) game. This command starts, pauses or asks about the game; the game itself is code of the operating system, not of this script.")));
	lines.push_back(CommentLine(_T("What decides how it plays (click a number to see its bytes, Change Value changes it; keep a copy of the firmware). Rules: Brick-Game-Logic.txt.")));
	if (bi.lives != 0xFFFFFFFF && bi.lives < size)
	{
		CLineBuilder b;
		Ref r;
		r.type = REF_NONE;
		r.absolute = true;
		r.off = (int)bi.lives;
		r.len = 1;
		r.tip = _T("Lives of a new game: the number in the instruction 'mov r0,#3' (1 to 255). Only 3 balls can be in play, more lives are spare.");
		CString v;
		v.Format(_T("%d"), fw[bi.lives]);
		b.Add(_T("  Lives at the start: "));
		b.Token(v, r);
		lines.push_back(b.Make());
	}
	if (bi.vx != 0xFFFFFFFF && bi.vx + 8 <= size)
	{
		DWORD vx = CCommandTable::MovValue(*(const DWORD *)(fw + bi.vx));
		DWORD vy = CCommandTable::MovValue(*(const DWORD *)(fw + bi.vy));
		CLineBuilder b;
		Ref r;
		r.type = REF_NONE;
		r.absolute = true;
		r.off = (int)bi.vx;
		r.len = 8;
		r.tip = _T("Start speed of a ball: the two instructions that load 0x28000 (across) and 0x21000 (down), 16.16 fixed point. Change Value takes a percent of the original speed (100 = original, 200 = twice as fast).");
		CString pct, rest;
		pct.Format(_T("%d%%"), (int)((vx * 100.0) / 0x28000 + 0.5));
		rest.Format(_T(" of the original (a new ball moves %.2f pixels across and %.2f down every 40 ms; 100%% = 2.50 and 2.06)"), vx / 65536.0, vy / 65536.0);
		b.Add(_T("  Ball speed: "));
		b.Token(pct, r);
		b.Add(rest);
		lines.push_back(b.Make());
	}
	// the other numbers of the game (each found in the code of the game, see Brick-Game-Logic.txt)
	if (bi.score != 0xFFFFFFFF && bi.score < size)
	{
		CLineBuilder b;
		Ref r;
		r.type = REF_NONE;
		r.absolute = true;
		r.off = (int)bi.score;
		r.len = 1;
		r.tip = _T("Points for every brick destroyed: the number in the instruction 'add r1,r0,#1' (1 to 255).");
		CString v;
		v.Format(_T("%d"), fw[bi.score]);
		b.Add(_T("  Points for each brick: "));
		b.Token(v, r);
		lines.push_back(b.Make());
	}
	if (bi.bonus != 0xFFFFFFFF && bi.bonus + 4 <= size)
	{
		DWORD w = *(const DWORD *)(fw + bi.bonus);
		CLineBuilder b;
		Ref r;
		r.type = REF_NONE;
		r.absolute = true;
		r.off = (int)bi.bonus;
		r.len = 4;
		r.tip = _T("Lives given when every brick of a level is gone. Stock: one for every ball still in play (the instruction 'add r0,r0,r1'). Change Value: 1 = that (the original), 0 or 2 to 255 = that many lives whatever the number of balls.");
		CString v = (w & 0xFFFFF000) == 0xE2800000 ? CString() : CString(_T("one for each ball still in play"));
		if (v.IsEmpty())
			v.Format(_T("%d"), (int)(w & 0xFFF));
		b.Add(_T("  Lives gained when a level is cleared: "));
		b.Token(v, r);
		lines.push_back(b.Make());
	}
	if (bi.ballW != 0xFFFFFFFF && bi.ballH != 0xFFFFFFFF && bi.ballW < size && bi.ballH < size)
	{
		CLineBuilder b;
		Ref r;
		r.type = REF_NONE;
		r.absolute = true;
		r.len = 1;
		CString v;
		r.off = (int)bi.ballW;
		r.tip = _T("Width of a ball in pixels (the instruction that sets the right edge of its rectangle, 1 to 20). The rectangle is also what hits bricks, the paddle and the walls.");
		v.Format(_T("%d"), fw[bi.ballW]);
		b.Add(_T("  Ball size: "));
		b.Token(v, r);
		b.Add(_T(" wide, "));
		r.off = (int)bi.ballH;
		r.tip = _T("Height of a ball in pixels (the number added to its top to get the bottom edge, 1 to 20).");
		v.Format(_T("%d"), fw[bi.ballH]);
		b.Token(v, r);
		b.Add(_T(" high (pixels)"));
		lines.push_back(b.Make());
	}
	if (bi.ballGap != 0xFFFFFFFF && bi.ballGap < size)
	{
		CLineBuilder b;
		Ref r;
		r.type = REF_NONE;
		r.absolute = true;
		r.off = (int)bi.ballGap;
		r.len = 1;
		r.tip = _T("How far below the last row of bricks a new ball starts, in pixels (0 to 60).");
		CString v;
		v.Format(_T("%d"), fw[bi.ballGap]);
		b.Add(_T("  A new ball starts "));
		b.Token(v, r);
		b.Add(_T(" pixels below the bricks"));
		lines.push_back(b.Make());
	}
	lines.push_back(CommentLine(_T("  Fixed by the game's code (not a single number): 3 balls at most, 6 rows of 12 bricks, bricks 4 pixels high, a 40 ms step, the paddle moves 4 pixels for every")));
	lines.push_back(CommentLine(_T("  click of the wheel (10 for the buttons), and a ball that hits an outer third of the paddle is sped up x1.5 or slowed x0.75 sideways.")));
	lines.push_back(CommentLine(_T("  The levels (the game goes on to the next one when every brick is gone, and repeats the last one):")));
	for (int lvl = 0; lvl < 22; lvl++)
	{
		DWORD at = bi.table + 12 * lvl;
		const DWORD *w = (const DWORD *)(fw + at);
		if (w[0] == 0)
			break;
		CLineBuilder b;
		CString t;
		Ref r;
		r.type = REF_NONE;
		r.absolute = true;
		r.len = 4;
		t.Format(_T("    Level %d: "), lvl + 1);
		b.Add(t);
		r.off = (int)at;
		r.tip = _T("Balls in play at the start of this level (1 to 3).");
		t.Format(_T("%u"), w[0]);
		b.Token(t, r);
		b.Add(w[0] == 1 ? _T(" ball, ") : _T(" balls, "));
		r.off = (int)at + 4;
		r.tip = _T("Rows of bricks (1 to 6). Each row has 12 bricks, so the bricks to destroy in this level are 12 x this number.");
		t.Format(_T("%u"), w[1]);
		b.Token(t, r);
		t.Format(w[1] == 1 ? _T(" row = %u bricks to destroy, paddle ") : _T(" rows = %u bricks to destroy, paddle "), w[1] * 12);
		b.Add(t);
		r.off = (int)at + 8;
		r.tip = _T("Width of the paddle in pixels in this level (1 to 160; the field is 160 pixels wide on a 4G).");
		t.Format(_T("%u"), w[2]);
		b.Token(t, r);
		b.Add(_T(" pixels"));
		lines.push_back(b.Make());
	}
	lines.push_back(Line());
}

// The 12 bit field of "mov rX,#imm" for a value (8 bits rotated right by an even number of bits), or -1 when the value does not fit
static int MovField(DWORD value)
{
	if (value == 0)
		return -1;
	for (DWORD rot = 0; rot < 16; rot++)
	{
		DWORD r = 2 * rot;
		DWORD imm = r ? ((value << r) | (value >> (32 - r))) : value;
		if (imm < 256)
			return (int)((rot << 8) | imm);
	}
	return -1;
}

// Stacker (the falling block game of the 3G prototype): the numbers that decide how it plays, each one a link to its bytes. The rules are written down
// in Stacker-Game-Logic.txt (research folder). A piece falls every 12 ticks, the tick is 40 ms; the shapes are 16 entries (4 kinds x 4 rotations).
void CLayoutDialog::CmdStackerLines(std::vector<Line> &lines)
{
	const CCommandTable::StackerInfo &si = m_Commands.Stacker();
	if (m_pFirmware == NULL || !si.valid)
		return;
	const BYTE *fw = m_pFirmware->GetFirmwareBuffer();
	DWORD size = m_pFirmware->GetFirmwareSize();
	if ((ULONGLONG)si.table + 16 * 32 > size || si.firstPiece >= size || si.tick[2] >= size || si.points >= size || si.column[1] >= size)
		return;
	lines.push_back(CommentLine(_T("Stacker game (falling blocks, 12 x 16 board). This command starts, pauses or asks about the game; the game itself is code of the operating system, not of this script.")));
	lines.push_back(CommentLine(_T("What decides how it plays (click a number to see its bytes, Change Value changes it; keep a copy of the firmware). Rules: Stacker-Game-Logic.txt.")));
	CString v;
	{
		CLineBuilder b;
		Ref r;
		r.type = REF_NONE;
		r.absolute = true;
		r.len = 1;
		r.off = (int)si.points;
		r.tip = _T("Points for every full row: the number in the instruction 'add r1,r0,#12' (1 to 255).");
		v.Format(_T("%d"), fw[si.points]);
		b.Add(_T("  Points for a full row: "));
		b.Token(v, r);
		lines.push_back(b.Make());
	}
	{
		CLineBuilder b;
		Ref r;
		r.type = REF_NONE;
		r.absolute = true;
		r.len = 1;
		r.off = (int)si.tick[0];
		r.tip = _T("Length of a tick in milliseconds: the number in 'mov r1,#40', in three places that Change Value changes together (5 to 255). A piece falls every 12 ticks.");
		v.Format(_T("%d"), fw[si.tick[0]]);
		b.Add(_T("  A tick is "));
		b.Token(v, r);
		v.Format(_T(" ms: a piece falls one row every %d ms (12 ticks); the fast drop and the wheel are handled on every tick"), 12 * fw[si.tick[0]]);
		b.Add(v);
		lines.push_back(b.Make());
	}
	{
		CLineBuilder b;
		Ref r;
		r.type = REF_NONE;
		r.absolute = true;
		r.len = 1;
		r.off = (int)si.column[0];
		r.tip = _T("The column (0 to 11) a new piece appears in: the number in 'mov r0,#6' (and in the one for the first piece; Change Value changes both).");
		v.Format(_T("%d"), fw[si.column[0]]);
		b.Add(_T("  A new piece appears in column "));
		b.Token(v, r);
		r.off = (int)si.firstPiece;
		r.tip = _T("The kind of the first piece of a game (0 square, 1 S/Z, 2 T, 3 bar): the shape number in 'mov r1,#4' divided by 4. The later pieces are random.");
		v.Format(_T("%d"), fw[si.firstPiece] / 4);
		b.Add(_T(", the first piece is kind "));
		b.Token(v, r);
		b.Add(_T(" (0 to 3)"));
		lines.push_back(b.Make());
	}
	lines.push_back(CommentLine(_T("  The pieces: columns x rows, the origin (the cell that sits on the piece's position) and the 4 x 4 cells (the number is the block picture, . is empty):")));
	for (int e = 0; e < 16; e++)
	{
		DWORD at = si.table + 32 * e;
		const DWORD *w = (const DWORD *)(fw + at);
		CLineBuilder b;
		Ref r;
		r.type = REF_NONE;
		r.absolute = true;
		r.len = 4;
		CString t;
		t.Format(_T("    Kind %d, turn %d: "), e / 4, e % 4);
		b.Add(t);
		static const TCHAR *const tip[4] = { _T("Columns the shape is wide (1 to 4)."), _T("Rows the shape is high (1 to 4)."),
		                                     _T("Column of the shape that sits on the piece's position (0 to 3)."), _T("Row of the shape that sits on the piece's position (0 to 3).") };
		static const TCHAR *const sep[4] = { _T(" x "), _T(", origin "), _T(","), _T("   cells ") };
		for (int k = 0; k < 4; k++)
		{
			r.off = (int)at + 4 * k;
			r.len = 4;
			r.tip = tip[k];
			t.Format(_T("%u"), w[k]);
			b.Token(t, r);
			b.Add(sep[k]);
		}
		CString cells;
		for (int row = 0; row < 4; row++)
		{
			if (row)
				cells += _T(" ");
			for (int col = 0; col < 4; col++)
			{
				BYTE c = fw[at + 16 + 4 * row + col];
				cells += c == 0 ? _T('.') : (c < 10 ? (TCHAR)(_T('0') + c) : _T('#'));
			}
		}
		r.off = (int)at + 16;
		r.len = 16;
		r.tip = _T("The 4 x 4 cells of the shape, row by row, one byte each: 0 empty, 1 to 4 the block picture. Change Value with Hex: type the 16 bytes.");
		b.Token(cells, r);
		lines.push_back(b.Make());
	}
	lines.push_back(CommentLine(_T("  Fixed by the game's code (not a single number): the 12 x 16 board, 6 pixel cells, 4 kinds of piece chosen at random, the wheel moves a piece one cell, Select turns it.")));
	lines.push_back(Line());
}

// How many bytes the marked bytes of a number of the games take
static DWORD BrickKindSize(int kind)
{
	switch (kind)
	{
	case 2: return 8;
	case 3: case 4: case 5: case 7: case 15: return 4;
	case 16: return 16;
	}
	return 1;
}

// What the marked bytes are, when they are one of the numbers of Brick or Stacker (see CmdBrickLines, CmdStackerLines): 0 none, 1 lives, 2 speed (percent, both
// instructions), 3 balls, 4 rows, 5 paddle width (the level table), 6 points per brick, 7 lives for a cleared level, 8 / 9 width / height of the ball, 10 where a
// ball starts below the bricks; Stacker: 11 points per row, 12 tick length (three places), 13 start column (two places), 14 kind of the first piece, 15 a number of a
// shape, 16 the cells of a shape
int CLayoutDialog::BrickKind(DWORD off, DWORD size) const
{
	const CCommandTable::StackerInfo &si = m_Commands.Stacker();
	if (si.valid)
	{
		if (off == si.points && size == 1)
			return 11;
		if (off == si.tick[0] && size == 1)
			return 12;
		if (off == si.column[0] && size == 1)
			return 13;
		if (off == si.firstPiece && size == 1)
			return 14;
		if (off >= si.table && off < si.table + 16 * 32)
		{
			DWORD in = (off - si.table) % 32;
			if (in < 16 && in % 4 == 0 && size == 4)
				return 15;
			if (in == 16 && size == 16)
				return 16;
		}
	}
	const CCommandTable::BrickInfo &bi = m_Commands.Brick();
	if (!bi.valid || bi.table == 0xFFFFFFFF)
		return 0;
	if (bi.lives != 0xFFFFFFFF && off == bi.lives && size == 1)
		return 1;
	if (bi.vx != 0xFFFFFFFF && off == bi.vx && size == 8)
		return 2;
	if (off >= bi.table && off < bi.table + 22 * 12 && size == 4 && (off - bi.table) % 4 == 0)
		return 3 + (int)(((off - bi.table) / 4) % 3);
	if (bi.score != 0xFFFFFFFF && off == bi.score && size == 1)
		return 6;
	if (bi.bonus != 0xFFFFFFFF && off == bi.bonus && size == 4)
		return 7;
	if (bi.ballW != 0xFFFFFFFF && off == bi.ballW && size == 1)
		return 8;
	if (bi.ballH != 0xFFFFFFFF && off == bi.ballH && size == 1)
		return 9;
	if (bi.ballGap != 0xFFFFFFFF && off == bi.ballGap && size == 1)
		return 10;
	return 0;
}

// The current value of one of them as Change Value takes it (decimal)
CString CLayoutDialog::BrickValueText(int kind, DWORD off)
{
	CString s;
	if (m_pFirmware == NULL || (ULONGLONG)off + BrickKindSize(kind) > m_pFirmware->GetFirmwareSize())
		return s;
	const BYTE *fw = m_pFirmware->GetFirmwareBuffer();
	switch (kind)
	{
	case 1: case 6: case 8: case 9: case 10: case 11: case 12: case 13:
		s.Format(_T("%d"), fw[off]);
		break;
	case 14:
		s.Format(_T("%d"), fw[off] / 4);
		break;
	case 2:
		s.Format(_T("%d"), (int)((CCommandTable::MovValue(*(const DWORD *)(fw + off)) * 100.0) / 0x28000 + 0.5));
		break;
	case 7:
		{
			DWORD w = *(const DWORD *)(fw + off);
			s.Format(_T("%d"), (w & 0xFFFFF000) == 0xE2800000 ? (int)(w & 0xFFF) : 1);          // 1: one for every ball still in play (the original)
		}
		break;
	case 16:
		break;                                                                                  // cells: Hex only
	default:
		s.Format(_T("%u"), *(const DWORD *)(fw + off));
		break;
	}
	return s;
}

// "Change Value" on one of the numbers of Brick or Stacker. Handled here when the marked bytes are one of them. Hex: the bytes themselves; Decimal: the value.
bool CLayoutDialog::BrickChange()
{
	if (!m_bLinkClick || m_pFirmware == NULL)
		return false;
	DWORD off = m_ValOffset;
	int kind = BrickKind(off, m_ValSize);
	if (kind == 0)
		return false;
	if (IsDlgButtonChecked(IDC_RADIO_STRINGVAL) == 1)
	{
		MessageBox(_T("Type the number as Decimal or Hex."));
		return true;
	}
	const CCommandTable::StackerInfo &si = m_Commands.Stacker();
	if (IsDlgButtonChecked(IDC_RADIO_HEXVAL) == 1)
	{
		// Hex: the bytes themselves ("05 00 80 E2"), as many as are typed (not more than are selected); the rest stay as they are
		CString hexText = m_ChangeString;
		hexText.Remove(' ');
		int lenm = hexText.GetLength();
		if (lenm == 0 || (lenm % 2) != 0)
		{
			MessageBox(_T("Type the bytes as pairs of hex digits, for example 05 00 80 E2."));
			return true;
		}
		for (int j = 0; j < lenm; j++)
			if (!_istxdigit(hexText[j]))
			{
				MessageBox(_T("That is not a hex value."));
				return true;
			}
		if ((DWORD)(lenm / 2) > m_ValSize)
		{
			MessageBox(_T("The value you typed is too long!"));
			return true;
		}
		BYTE raw[16];
		for (int k = 0, j = 0; j < lenm; j += 2, k++)
		{
			unsigned int bv = 0;
			swscanf(hexText.Mid(j, 2), _T("%X"), &bv);
			raw[k] = (BYTE)bv;
		}
		m_pFirmware->SetData(off, raw, lenm / 2);
		if (kind == 12)                                            // the other places of the same number
		{
			m_pFirmware->SetData(si.tick[1], raw, 1);
			m_pFirmware->SetData(si.tick[2], raw, 1);
		}
		else if (kind == 13)
			m_pFirmware->SetData(si.column[1], raw, 1);
		BrickChanged(off);
		return true;
	}
	if (kind == 16)
	{
		MessageBox(_T("The cells of a shape are 16 bytes: choose Hex and type them (for example 01 01 00 00 01 01 00 00 00 00 00 00 00 00 00 00)."));
		return true;
	}
	long value = 0;
	if (_stscanf(m_ChangeString, _T("%ld"), &value) != 1)
	{
		MessageBox(_T("That is not a number."));
		return true;
	}
	static const long lo[17] = { 0, 1, 10, 1, 1, 1, 1, 0, 1, 1, 0, 1, 5, 0, 0, 0, 0 }, hi[17] = { 0, 255, 400, 3, 6, 160, 255, 255, 20, 20, 60, 255, 255, 11, 3, 4, 0 };
	static const TCHAR *const what[17] = { _T(""), _T("The lives have to be between 1 and 255."), _T("The speed has to be between 10 and 400 percent of the original."),
		_T("A level has 1 to 3 balls."), _T("A level has 1 to 6 rows of bricks."), _T("The paddle has to be between 1 and 160 pixels wide."),
		_T("The points for a brick have to be between 1 and 255."), _T("Type 1 for one life per ball still in play (the original), or 0 or 2 to 255 for that many lives in all."),
		_T("The ball has to be between 1 and 20 pixels wide."), _T("The ball has to be between 1 and 20 pixels high."),
		_T("A ball starts 0 to 60 pixels below the bricks."), _T("The points for a row have to be between 1 and 255."),
		_T("A tick has to be between 5 and 255 milliseconds."), _T("A piece appears in a column from 0 to 11."),
		_T("The first piece is a kind from 0 to 3 (square, S/Z, T, bar)."), _T("A shape number has to be between 0 and 4."), _T("") };
	if (value < lo[kind] || value > hi[kind])
	{
		MessageBox(what[kind]);
		return true;
	}
	LPBYTE fw = m_pFirmware->GetFirmwareBuffer();
	if (kind == 2)
	{
		// the speed is stored as an immediate of two instructions: only some values fit
		int fx = MovField((DWORD)(0x28000 * (double)value / 100.0)), fy = MovField((DWORD)(0x21000 * (double)value / 100.0));
		if (fx < 0 || fy < 0)
		{
			CString near_;
			int n = 0;
			for (int pct = 10; pct <= 400 && n < 8; pct++)
				if (abs(pct - (int)value) <= 25 && MovField((DWORD)(0x28000 * (double)pct / 100.0)) >= 0 && MovField((DWORD)(0x21000 * (double)pct / 100.0)) >= 0)
				{
					CString p;
					p.Format(n ? _T(", %d") : _T("%d"), pct);
					near_ += p;
					n++;
				}
			CString msg;
			msg.Format(_T("The speed %ld%% cannot be stored exactly: the game keeps it as an 8 bit number shifted by an even amount, so only some values fit."), value);
			if (!near_.IsEmpty())
				msg += _T("\n\nSpeeds near it that fit (percent): ") + near_;
			MessageBox(msg, _T("Value Changer"));
			return true;
		}
		DWORD w0 = (*(DWORD *)(fw + off) & 0xFFFFF000) | (DWORD)fx;
		DWORD w1 = (*(DWORD *)(fw + off + 4) & 0xFFFFF000) | (DWORD)fy;
		m_pFirmware->SetData(off, (LPBYTE)&w0, 4);
		m_pFirmware->SetData(off + 4, (LPBYTE)&w1, 4);
	}
	else if (kind == 12)
	{
		BYTE b = (BYTE)value;
		for (int k = 0; k < 3; k++)
			m_pFirmware->SetData(si.tick[k], &b, 1);
	}
	else if (kind == 13)
	{
		BYTE b = (BYTE)value;
		m_pFirmware->SetData(si.column[0], &b, 1);
		m_pFirmware->SetData(si.column[1], &b, 1);
	}
	else if (kind == 14)
	{
		BYTE b = (BYTE)(value * 4);                               // the shape number of the first piece: kind x 4
		m_pFirmware->SetData(off, &b, 1);
	}
	else if (kind == 1 || kind == 6 || kind == 11 || (kind >= 8 && kind <= 10))
	{
		BYTE b = (BYTE)value;                                     // the 8 bit number of "mov rX,#n" / "add rX,rY,#n"
		m_pFirmware->SetData(off, &b, 1);
	}
	else if (kind == 7)
	{
		DWORD w = value == 1 ? 0xE0800001 : (0xE2800000 | (DWORD)value);       // add r0,r0,r1 (a life per ball in play) / add r0,r0,#n
		m_pFirmware->SetData(off, (LPBYTE)&w, 4);
	}
	else
	{
		DWORD v = (DWORD)value;
		m_pFirmware->SetData(off, (LPBYTE)&v, 4);
	}
	BrickChanged(off);
	return true;
}

// After a change: show the bytes, make the listing again and select the row that was selected
void CLayoutDialog::BrickChanged(DWORD off)
{
	m_HexWnd.SetOffset(off);
	m_HexWnd.Invalidate();
	int row = -1;
	POSITION pos = m_ResourceList.GetFirstSelectedItemPosition();
	if (pos)
		row = m_ResourceList.GetNextSelectedItem(pos);
	CmdFillList(m_CmdFunction);
	if (row >= 0 && row < m_ResourceList.GetItemCount())
	{
		m_ResourceList.SetItemState(row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		m_ResourceList.EnsureVisible(row, FALSE);
	}
	MessageBox(_T("Value changed successfully."), _T("Value Changer"), MB_OK);
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
			bool bOpens = false;           // a native method has nothing to open
			for (size_t r = 0; r < l.refs.size(); r++)
			{
				CString t = CmdRefTip(l.refs[r]);
				if (t.IsEmpty())
					continue;
				if (l.refs[r].type != REF_NATIVE)
					bOpens = true;
				if (!tip.IsEmpty())
					tip += _T("\n");
				tip += t;
			}
			if (!tip.IsEmpty())
			{
				if (bOpens)
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

	// a command cell of a menu entry: choose another command
	if (m_ResourceType == 1 && m_pFirmware != NULL && m_Commands.IsValid())
	{
		LPNMITEMACTIVATE pia = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
		LVHITTESTINFO hit;
		memset(&hit, 0, sizeof(hit));
		hit.pt = pia->ptAction;
		m_ResourceList.SubItemHitTest(&hit);
		if (hit.iItem >= 0 && hit.iSubItem >= 2 && hit.iSubItem <= 5)
		{
			m_ResourceList.SetItemState(hit.iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			pia->iItem = hit.iItem;
			pia->iSubItem = hit.iSubItem;
			LRESULT r = 0;
			OnLvnItemClickedResourceList(pNMHDR, &r);          // the cell is selected as if it had been clicked
			int logical = MenuLogicalCol(hit.iSubItem);
			CPoint pt;
			GetCursorPos(&pt);
			CMenu menu;
			menu.CreatePopupMenu();
			menu.AppendMenu(MF_STRING, 1, _T("Choose another command..."));
			menu.AppendMenu(MF_STRING, 2, _T("Open this command"));
			int cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, this);
			if (cmd == 1)
				MenuPickCommand(hit.iItem, logical);
			else if (cmd == 2 && m_ValSize == 4)
			{
				int fn = CCommandTable::RefToFunction(*(const DWORD *)(m_pFirmware->GetFirmwareBuffer() + m_ValOffset));
				if (fn >= 0 && m_Commands.FindFunction(fn) != NULL)
					SelectResource(3, fn);
			}
			return;
		}
	}

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
	case REF_NATIVE:
		s = m_Commands.NativeText(ref.value);
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
	else if (IsCommandMode() && m_bLinkClick)
	{
		// one of the numbers of Brick: its value (the same as the Brick lines show), as Decimal or Hex
		int kind = BrickKind(m_ValOffset, m_ValSize);
		if (kind != 0)
		{
			if (bHex)
				return m_ValSize <= 16 ? HexBytes(fw + m_ValOffset, (int)m_ValSize) : CString();     // the selected bytes, as the hex view shows them
			number = _ttol(BrickValueText(kind, m_ValOffset));
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
	if (BrickChange())
		return true;
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
	MenuRelink(m_ClickedCol, m_ValOffset, (int)fn, false);
	return true;
}

// Store a new command in a menu entry. Unless the caller has already shown the comparison (the list of commands does), the new command
// is compared with the one it replaces first and anything odd is shown, with the choice to go on.
bool CLayoutDialog::MenuRelink(int logicalCol, DWORD offset, int newFn, bool bQuiet)
{
	if (m_pFirmware == NULL || (ULONGLONG)offset + 4 > m_pFirmware->GetFirmwareSize() || m_Commands.FindFunction(newFn) == NULL)
		return false;
	int oldFn = CCommandTable::RefToFunction(*(const DWORD *)(m_pFirmware->GetFirmwareBuffer() + offset));
	if (oldFn == newFn)
	{
		if (!bQuiet)
			MessageBox(_T("That entry already runs this command."), _T("Value Changer"));
		return true;
	}
	CString info;
	if (!bQuiet)
	{
		BOOL bWarn = FALSE;
		info = m_Commands.Compare(oldFn, newFn, logicalCol == 2, &bWarn);
		if (bWarn && MessageBox(info + _T("\nChange it anyway?"), _T("Value Changer"), MB_YESNO | MB_ICONWARNING) != IDYES)
			return false;
	}
	DWORD v = CCommandTable::FunctionToRef(newFn);
	m_pFirmware->SetData(offset, (LPBYTE)&v, 4);
	m_HexWnd.SetOffset(offset);
	m_HexWnd.Invalidate();
	RefreshSelection();
	UpdateCommandInfo();
	if (!bQuiet)
		MessageBox(_T("Value changed successfully.\n\n") + m_Commands.Describe(newFn) + _T("."), _T("Value Changer"), MB_OK);
	return true;
}

// The line under the buttons says what the selected command cell runs (otherwise it describes the list, as always)
void CLayoutDialog::UpdateCommandInfo()
{
	CWnd *pText = GetDlgItem(IDC_MODE_TEXT);
	if (pText == NULL)
		return;
	if (m_ResourceType == 1 && (m_ClickedCol == 2 || m_ClickedCol == 3) && m_ValSize == 4 && m_pFirmware != NULL && m_Commands.IsValid() &&
	    (ULONGLONG)m_ValOffset + 4 <= m_pFirmware->GetFirmwareSize())
	{
		int fn = CCommandTable::RefToFunction(*(const DWORD *)(m_pFirmware->GetFirmwareBuffer() + m_ValOffset));
		if (fn >= 0 && m_Commands.FindFunction(fn) != NULL)
		{
			pText->SetWindowText(m_Commands.Describe(fn) + (m_ClickedCol == 2 ? _T(" (run when highlighted). Right click the cell to choose another command.")
			                                                                 : _T(" (run when chosen). Right click the cell to choose another command.")));
			return;
		}
	}
	UpdateModeText();
}

// ---- the list of commands to choose from. It is a dialog built in memory, so the resource file does not change.

enum { PK_SEARCH = 101, PK_GROUP, PK_SAME, PK_LIST, PK_INFO, PK_TEXT1, PK_TEXT2 };

static void PutW(std::vector<BYTE> &b, WORD w) { b.push_back((BYTE)(w & 0xFF)); b.push_back((BYTE)(w >> 8)); }
static void PutD(std::vector<BYTE> &b, DWORD d) { PutW(b, LOWORD(d)); PutW(b, HIWORD(d)); }
static void PutS(std::vector<BYTE> &b, const WCHAR *s) { do PutW(b, *s); while (*s++); }
static void PutAlign(std::vector<BYTE> &b) { while (b.size() % 4) b.push_back(0); }
static void PutItem(std::vector<BYTE> &b, DWORD style, int x, int y, int cx, int cy, WORD id, const WCHAR *cls, const WCHAR *text)
{
	PutAlign(b);
	PutD(b, style | WS_CHILD | WS_VISIBLE);
	PutD(b, 0);
	PutW(b, (WORD)x); PutW(b, (WORD)y); PutW(b, (WORD)cx); PutW(b, (WORD)cy);
	PutW(b, id);
	PutS(b, cls);
	PutS(b, text);
	PutW(b, 0);                                          // no creation data
}

// The template of the dialog (also used by the test program of the picker)
static void BuildCommandPickTemplate(std::vector<BYTE> &b)
{
	b.clear();
	PutD(b, DS_MODALFRAME | DS_SETFONT | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU);
	PutD(b, 0);
	PutW(b, 9);                                          // number of controls
	PutW(b, 0); PutW(b, 0); PutW(b, 360); PutW(b, 254);
	PutW(b, 0);                                          // no menu
	PutW(b, 0);                                          // standard class
	PutS(b, L"Choose a command");
	PutW(b, 8);
	PutS(b, L"MS Shell Dlg");
	PutItem(b, SS_LEFT, 7, 9, 30, 8, PK_TEXT1, L"STATIC", L"Search:");
	PutItem(b, ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP, 40, 7, 150, 12, PK_SEARCH, L"EDIT", L"");
	PutItem(b, SS_LEFT, 200, 9, 22, 8, PK_TEXT2, L"STATIC", L"Show:");
	PutItem(b, CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 224, 7, 129, 120, PK_GROUP, L"COMBOBOX", L"");
	PutItem(b, BS_AUTOCHECKBOX | WS_TABSTOP, 7, 24, 346, 10, PK_SAME, L"BUTTON", L"");
	PutItem(b, LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_BORDER | WS_TABSTOP, 7, 38, 346, 112, PK_LIST, L"SysListView32", L"");
	PutItem(b, SS_LEFT, 7, 154, 346, 74, PK_INFO, L"STATIC", L"");
	PutItem(b, BS_DEFPUSHBUTTON | WS_TABSTOP, 203, 234, 85, 14, IDOK, L"BUTTON", L"Use this command");
	PutItem(b, BS_PUSHBUTTON | WS_TABSTOP, 293, 234, 60, 14, IDCANCEL, L"BUTTON", L"Cancel");
}

class CCommandPickDlg : public CDialog
{
public:
	CCommandPickDlg(const CCommandTable *pCmds, int curId, BOOL bHilite, CWnd *pParent)
		: m_pCmds(pCmds), m_CurId(curId), m_bHilite(bHilite), m_Chosen(-1), m_bReady(FALSE)
	{
		BuildCommandPickTemplate(m_Tpl);
		InitModalIndirect((LPCDLGTEMPLATE)&m_Tpl[0], pParent);
	}
	int m_Chosen;

protected:
	struct Row { int id; CString text; CString group; int nParams; };

	const CCommandTable *m_pCmds;
	int m_CurId;
	BOOL m_bHilite;
	BOOL m_bReady;
	std::vector<BYTE> m_Tpl;
	std::vector<Row> m_Rows;
	CListCtrl m_List;

	static CString Lower(CString s) { s.MakeLower(); return s; }

	BOOL Matches(const Row &r, const CString &words, const CString &group, BOOL bSame)
	{
		if (!group.IsEmpty() && r.group != group)
			return FALSE;
		if (bSame && !m_pCmds->UsedAs(r.id, m_bHilite ? CCommandTable::USE_HILITE : CCommandTable::USE_SELECT))
			return FALSE;
		CString hay, id;
		id.Format(_T("%d"), r.id);
		hay = Lower(id + _T(" ") + r.text + _T(" ") + r.group);
		int pos = 0;
		CString w = words.Tokenize(_T(" "), pos);
		while (!w.IsEmpty())
		{
			if (hay.Find(w) < 0)
				return FALSE;
			w = words.Tokenize(_T(" "), pos);
		}
		return TRUE;
	}

	void Refill()
	{
		CString words;
		GetDlgItemText(PK_SEARCH, words);
		words = Lower(words);
		words.Trim();
		CString group;
		CComboBox *pc = (CComboBox *)GetDlgItem(PK_GROUP);
		if (pc->GetCurSel() > 0)
			pc->GetLBText(pc->GetCurSel(), group);
		BOOL bSame = IsDlgButtonChecked(PK_SAME) == BST_CHECKED;

		int keep = m_Chosen;
		POSITION pos = m_List.GetFirstSelectedItemPosition();
		if (pos)
			keep = (int)m_List.GetItemData(m_List.GetNextSelectedItem(pos));
		m_List.SetRedraw(FALSE);
		m_List.DeleteAllItems();
		int sel = -1;
		for (size_t i = 0; i < m_Rows.size(); i++)
		{
			const Row &r = m_Rows[i];
			if (!Matches(r, words, group, bSame))
				continue;
			CString id, np;
			id.Format(_T("%d"), r.id);
			np.Format(_T("%d"), r.nParams);
			int n = m_List.InsertItem(m_List.GetItemCount(), id);
			m_List.SetItemText(n, 1, r.text);
			m_List.SetItemText(n, 2, r.group);
			m_List.SetItemText(n, 3, np);
			m_List.SetItemData(n, r.id);
			if (r.id == keep)
				sel = n;
		}
		m_List.SetRedraw(TRUE);
		if (sel >= 0)
		{
			m_List.SetItemState(sel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			m_List.EnsureVisible(sel, FALSE);
		}
		ShowInfo();
	}

	void ShowInfo()
	{
		POSITION pos = m_List.GetFirstSelectedItemPosition();
		CString info;
		BOOL bOk = FALSE;
		if (pos)
		{
			int id = (int)m_List.GetItemData(m_List.GetNextSelectedItem(pos));
			BOOL bWarn = FALSE;
			info = m_pCmds->Compare(m_CurId, id, m_bHilite, &bWarn);
			info.Replace(_T("\n"), _T("\r\n"));
			bOk = id != m_CurId;
		}
		else
			info.Format(_T("%d command%s match. Choose one to see how it compares with the current command."), m_List.GetItemCount(), m_List.GetItemCount() == 1 ? _T("") : _T("s"));
		SetDlgItemText(PK_INFO, info);
		GetDlgItem(IDOK)->EnableWindow(bOk);
	}

	virtual BOOL OnInitDialog()
	{
		CDialog::OnInitDialog();
		m_List.Attach(GetDlgItem(PK_LIST)->m_hWnd);
		m_List.SetExtendedStyle(LVS_EX_FULLROWSELECT);
		m_List.InsertColumn(0, _T("Command"), LVCFMT_LEFT, 62);
		m_List.InsertColumn(1, _T("Name, or where it is used"), LVCFMT_LEFT, 250);
		m_List.InsertColumn(2, _T("Kind"), LVCFMT_LEFT, 120);
		m_List.InsertColumn(3, _T("Parameters"), LVCFMT_LEFT, 70);

		std::set<CString> groups;
		for (int i = 0; i < m_pCmds->GetNumFunctions(); i++)
		{
			Row r;
			r.id = m_pCmds->GetFunctionId(i);
			const PixoScript::Function *pf = m_pCmds->FindFunction(r.id);
			r.nParams = pf ? pf->nParams : 0;
			r.text = m_pCmds->Name(r.id);
			if (r.text.IsEmpty())
				r.text = m_pCmds->Role(r.id);
			if (r.text.IsEmpty())
				r.text = _T("(no menu entry uses it)");
			r.group = m_pCmds->Group(r.id);
			groups.insert(r.group);
			m_Rows.push_back(r);
		}
		CComboBox *pc = (CComboBox *)GetDlgItem(PK_GROUP);
		pc->AddString(_T("All kinds"));
		for (std::set<CString>::const_iterator it = groups.begin(); it != groups.end(); ++it)
			pc->AddString(*it);
		pc->SetCurSel(0);

		CString lbl;
		lbl.Format(_T("Only commands that menus use as %s commands (the safest choice)"), m_bHilite ? _T("highlight") : _T("select"));
		SetDlgItemText(PK_SAME, lbl);
		CString title;
		title.Format(_T("Choose a %s command"), m_bHilite ? _T("highlight") : _T("select"));
		SetWindowText(title);
		// the list starts with the commands used the same way, unless the current one is not among them
		CheckDlgButton(PK_SAME, m_pCmds->UsedAs(m_CurId, m_bHilite ? CCommandTable::USE_HILITE : CCommandTable::USE_SELECT) ? BST_CHECKED : BST_UNCHECKED);
		m_Chosen = m_CurId;
		m_bReady = TRUE;
		Refill();
		GetDlgItem(PK_SEARCH)->SetFocus();
		return FALSE;
	}

	virtual void OnOK()
	{
		POSITION pos = m_List.GetFirstSelectedItemPosition();
		if (!pos)
			return;
		m_Chosen = (int)m_List.GetItemData(m_List.GetNextSelectedItem(pos));
		if (m_Chosen == m_CurId)
			return;
		CDialog::OnOK();
	}

	afx_msg void OnFilter() { if (m_bReady) Refill(); }
	afx_msg void OnListChanged(NMHDR *, LRESULT *pResult) { *pResult = 0; if (m_bReady) ShowInfo(); }
	afx_msg void OnListDbl(NMHDR *, LRESULT *pResult) { *pResult = 0; OnOK(); }
	afx_msg void OnDestroy() { m_List.Detach(); CDialog::OnDestroy(); }

	DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(CCommandPickDlg, CDialog)
	ON_EN_CHANGE(PK_SEARCH, OnFilter)
	ON_CBN_SELCHANGE(PK_GROUP, OnFilter)
	ON_BN_CLICKED(PK_SAME, OnFilter)
	ON_NOTIFY(LVN_ITEMCHANGED, PK_LIST, OnListChanged)
	ON_NOTIFY(NM_DBLCLK, PK_LIST, OnListDbl)
	ON_WM_DESTROY()
END_MESSAGE_MAP()

// Right click on a command cell of a menu: choose another command from the list
bool CLayoutDialog::MenuPickCommand(int item, int logicalCol)
{
	int sel_id = _ttoi(m_ResourceTree.GetItemText(m_ResourceTree.GetSelectedItem()));
	LPBYTE lpBuf = m_pFirmware->GetFirmwareBuffer();
	for (size_t j = 0; MenuList != NULL && j < MenuList->size(); j++)
	{
		if (MenuList->at(j)->id != sel_id)
			continue;
		if (item < 0 || item >= MenuList->at(j)->num_elems)
			return false;
		const resource_type_menu_elem *e = MenuList->at(j)->MenuElemList[item]->element;
		DWORD rel = 0, sz = 0;
		m_Attrs.MenuField(logicalCol, &rel, &sz);
		if (sz != 4)
			return false;
		DWORD offset = (DWORD)((const BYTE *)e - lpBuf) + rel;
		int oldFn = CCommandTable::RefToFunction(*(const DWORD *)(lpBuf + offset));
		CCommandPickDlg dlg(&m_Commands, oldFn, logicalCol == 2, this);
		if (dlg.DoModal() != IDOK || dlg.m_Chosen < 0)
			return false;
		return MenuRelink(logicalCol, offset, dlg.m_Chosen, true);
	}
	return false;
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

// For the hex view: the function record of the command table that holds the byte at pos (its number, the header, the code and the padding
// after it), so the grey behind a selected command starts at the command and not at the start of the whole table.
BOOL CLayoutDialog::HexExtentHook(void *ctx, DWORD pos, DWORD *pStart, DWORD *pLen)
{
	CLayoutDialog *pThis = (CLayoutDialog *)ctx;
	if (pThis == NULL)
		return FALSE;

	// a layout or a menu: the element (one row of the list) that holds pos. An element is its size field and the number of bytes the field says
	// follow it, so the grey runs from the start of that element and not over the whole layout.
	if (pThis->m_pFirmware != NULL && (pThis->m_ResourceType == 0 || pThis->m_ResourceType == 1) && pThis->m_ResourceTree.m_hWnd != NULL)
	{
		HTREEITEM it = pThis->m_ResourceTree.GetSelectedItem();
		LPBYTE lpBuf = pThis->m_pFirmware->GetFirmwareBuffer();
		if (it != NULL && lpBuf != NULL)
		{
			int sel_id = _ttoi(pThis->m_ResourceTree.GetItemText(it));
			if (pThis->m_ResourceType == 0 && pThis->ViewList != NULL)
			{
				for (size_t j = 0; j < pThis->ViewList->size(); j++)
				{
					if (pThis->ViewList->at(j)->id != sel_id)
						continue;
					for (int k = 0; k < pThis->ViewList->at(j)->num_elems; k++)
					{
						const resource_type_view_elem *e = pThis->ViewList->at(j)->ViewElemList[k]->element;
						DWORD start = (DWORD)((const BYTE *)e - lpBuf);
						DWORD len = e->size + 4;
						if (e->size >= 0x20 && e->size < 0x1000 && pos >= start && pos - start < len)
						{
							*pStart = start;
							*pLen = len;
							return TRUE;
						}
					}
					break;
				}
			}
			else if (pThis->m_ResourceType == 1 && pThis->MenuList != NULL)
			{
				for (size_t j = 0; j < pThis->MenuList->size(); j++)
				{
					if (pThis->MenuList->at(j)->id != sel_id)
						continue;
					for (int k = 0; k < pThis->MenuList->at(j)->num_elems; k++)
					{
						const resource_type_menu_elem *e = pThis->MenuList->at(j)->MenuElemList[k]->element;
						DWORD start = (DWORD)((const BYTE *)e - lpBuf);
						DWORD len = (DWORD)e->size + 4;
						if (e->size >= 0x20 && e->size < 0x1000 && pos >= start && pos - start < len)
						{
							*pStart = start;
							*pLen = len;
							return TRUE;
						}
					}
					break;
				}
			}
		}
	}

	if (!pThis->m_Commands.IsValid())
		return FALSE;
	DWORD resStart = pThis->m_Commands.GetResourceOffset();
	if (pos < resStart || pos - resStart >= pThis->m_Commands.GetResourceSize())
		return FALSE;
	for (int i = 0; i < pThis->m_Commands.GetNumFunctions(); i++)
	{
		const Function *pf = pThis->m_Commands.FindFunction(pThis->m_Commands.GetFunctionId(i));
		if (pf == NULL)
			continue;
		DWORD start = pThis->m_Commands.GetFunctionOffset(*pf);
		DWORD len = pThis->m_Commands.Script().GetRecordLength(*pf);
		if (pos >= start && pos - start < len)
		{
			*pStart = start;
			*pLen = len;
			return TRUE;
		}
	}
	return FALSE;
}
