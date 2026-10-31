// PixoScript.cpp - reader and decompiler for the iPod's command table. See PixoScript.h.

#include "stdafx.h"
#include "PixoScript.h"
#include <algorithm>

namespace PixoScript
{

// ---------------------------------------------------------------------------------------------------------
// Instruction set: SpiderMonkey's, numbered as in the iPod firmware
// ---------------------------------------------------------------------------------------------------------

enum
{
	OP_NOP = 0x00, OP_PUSH = 0x01, OP_POPV = 0x02, OP_ENTERWITH = 0x03, OP_LEAVEWITH = 0x04, OP_RETURN = 0x05,
	OP_GOTO = 0x06, OP_IFEQ = 0x07, OP_IFNE = 0x08, OP_ARGUMENTS = 0x09, OP_FORARG = 0x0a, OP_FORVAR = 0x0b,
	OP_DUP = 0x0c, OP_DUP2 = 0x0d, OP_SETCONST = 0x0e,
	OP_BITOR = 0x0f, OP_BITXOR = 0x10, OP_BITAND = 0x11, OP_EQ = 0x12, OP_NE = 0x13, OP_LT = 0x14, OP_LE = 0x15,
	OP_GT = 0x16, OP_GE = 0x17, OP_LSH = 0x18, OP_RSH = 0x19, OP_URSH = 0x1a, OP_ADD = 0x1b, OP_SUB = 0x1c,
	OP_MUL = 0x1d, OP_DIV = 0x1e, OP_MOD = 0x1f, OP_NOT = 0x20, OP_BITNOT = 0x21, OP_NEG = 0x22,
	OP_NEW = 0x23, OP_DELNAME = 0x24, OP_DELPROP = 0x25, OP_DELELEM = 0x26, OP_TYPEOF = 0x27, OP_VOID = 0x28,
	OP_INCNAME = 0x29, OP_INCPROP = 0x2a, OP_INCELEM = 0x2b, OP_DECNAME = 0x2c, OP_DECPROP = 0x2d, OP_DECELEM = 0x2e,
	OP_NAMEINC = 0x2f, OP_PROPINC = 0x30, OP_ELEMINC = 0x31, OP_NAMEDEC = 0x32, OP_PROPDEC = 0x33, OP_ELEMDEC = 0x34,
	OP_GETPROP = 0x35, OP_SETPROP = 0x36, OP_GETELEM = 0x37, OP_SETELEM = 0x38, OP_PUSHOBJ = 0x39, OP_CALL = 0x3a,
	OP_NAME = 0x3b, OP_NUMBER = 0x3c, OP_STRING = 0x3d, OP_ZERO = 0x3e, OP_ONE = 0x3f, OP_NULL = 0x40, OP_THIS = 0x41,
	OP_FALSE = 0x42, OP_TRUE = 0x43, OP_OR = 0x44, OP_AND = 0x45, OP_TABLESWITCH = 0x46, OP_LOOKUPSWITCH = 0x47,
	OP_POP = 0x51, OP_GETARG = 0x54, OP_SETARG = 0x55, OP_GETVAR = 0x56, OP_SETVAR = 0x57, OP_UINT16 = 0x58,
	OP_BINDNAME = 0x6c, OP_SETNAME = 0x6d, OP_CONDSWITCH = 0x76, OP_CASE = 0x77, OP_DEFAULT = 0x78
};

static const char *g_OpNames[256];
static BYTE g_OpLen[256];          // 0 unknown, 1 or 3 bytes, 0xFF = switch table (variable)
static BOOL g_bOpsReady = FALSE;

static void SetOp(int op, const char *name, int len)
{
	g_OpNames[op] = name;
	g_OpLen[op] = (BYTE)len;
}

static void InitOps()
{
	if (g_bOpsReady)
		return;
	memset(g_OpNames, 0, sizeof(g_OpNames));
	memset(g_OpLen, 0, sizeof(g_OpLen));
	SetOp(OP_NOP, "nop", 1); SetOp(OP_PUSH, "push", 1); SetOp(OP_POPV, "popv", 1); SetOp(OP_ENTERWITH, "enterwith", 1);
	SetOp(OP_LEAVEWITH, "leavewith", 1); SetOp(OP_RETURN, "return", 1); SetOp(OP_GOTO, "goto", 3); SetOp(OP_IFEQ, "ifeq", 3);
	SetOp(OP_IFNE, "ifne", 3); SetOp(OP_ARGUMENTS, "arguments", 1); SetOp(OP_FORARG, "forarg", 3); SetOp(OP_FORVAR, "forvar", 3);
	SetOp(OP_DUP, "dup", 1); SetOp(OP_DUP2, "dup2", 1); SetOp(OP_SETCONST, "setconst", 3);
	SetOp(OP_BITOR, "bitor", 1); SetOp(OP_BITXOR, "bitxor", 1); SetOp(OP_BITAND, "bitand", 1); SetOp(OP_EQ, "eq", 1);
	SetOp(OP_NE, "ne", 1); SetOp(OP_LT, "lt", 1); SetOp(OP_LE, "le", 1); SetOp(OP_GT, "gt", 1); SetOp(OP_GE, "ge", 1);
	SetOp(OP_LSH, "lsh", 1); SetOp(OP_RSH, "rsh", 1); SetOp(OP_URSH, "ursh", 1); SetOp(OP_ADD, "add", 1); SetOp(OP_SUB, "sub", 1);
	SetOp(OP_MUL, "mul", 1); SetOp(OP_DIV, "div", 1); SetOp(OP_MOD, "mod", 1); SetOp(OP_NOT, "not", 1); SetOp(OP_BITNOT, "bitnot", 1);
	SetOp(OP_NEG, "neg", 1); SetOp(OP_NEW, "new", 3); SetOp(OP_DELNAME, "delname", 3); SetOp(OP_DELPROP, "delprop", 3);
	SetOp(OP_DELELEM, "delelem", 1); SetOp(OP_TYPEOF, "typeof", 1); SetOp(OP_VOID, "void", 1);
	SetOp(OP_INCNAME, "incname", 3); SetOp(OP_INCPROP, "incprop", 3); SetOp(OP_INCELEM, "incelem", 1);
	SetOp(OP_DECNAME, "decname", 3); SetOp(OP_DECPROP, "decprop", 3); SetOp(OP_DECELEM, "decelem", 1);
	SetOp(OP_NAMEINC, "nameinc", 3); SetOp(OP_PROPINC, "propinc", 3); SetOp(OP_ELEMINC, "eleminc", 1);
	SetOp(OP_NAMEDEC, "namedec", 3); SetOp(OP_PROPDEC, "propdec", 3); SetOp(OP_ELEMDEC, "elemdec", 1);
	SetOp(OP_GETPROP, "getprop", 3); SetOp(OP_SETPROP, "setprop", 3); SetOp(OP_GETELEM, "getelem", 1); SetOp(OP_SETELEM, "setelem", 1);
	SetOp(OP_PUSHOBJ, "pushobj", 1); SetOp(OP_CALL, "call", 3); SetOp(OP_NAME, "name", 3); SetOp(OP_NUMBER, "number", 3);
	SetOp(OP_STRING, "string", 3); SetOp(OP_ZERO, "zero", 1); SetOp(OP_ONE, "one", 1); SetOp(OP_NULL, "null", 1);
	SetOp(OP_THIS, "this", 1); SetOp(OP_FALSE, "false", 1); SetOp(OP_TRUE, "true", 1); SetOp(OP_OR, "or", 3); SetOp(OP_AND, "and", 3);
	SetOp(OP_TABLESWITCH, "tableswitch", 0xFF); SetOp(OP_LOOKUPSWITCH, "lookupswitch", 0xFF);
	SetOp(OP_POP, "pop", 1); SetOp(OP_GETARG, "getarg", 3); SetOp(OP_SETARG, "setarg", 3); SetOp(OP_GETVAR, "getvar", 3);
	SetOp(OP_SETVAR, "setvar", 3); SetOp(OP_UINT16, "uint16", 3); SetOp(OP_BINDNAME, "bindname", 3); SetOp(OP_SETNAME, "setname", 3);
	SetOp(OP_CONDSWITCH, "condswitch", 1); SetOp(OP_CASE, "case", 3); SetOp(OP_DEFAULT, "default", 3);
	g_bOpsReady = TRUE;
}

const TCHAR *OpName(int op)
{
	InitOps();
	static TCHAR buf[8][16];
	static int n = 0;
	if (op < 0 || op > 255 || !g_OpNames[op])
	{
		TCHAR *b = buf[n++ & 7];
		wsprintf(b, _T("op%02X"), op & 255);
		return b;
	}
	TCHAR *b = buf[n++ & 7];
	int i = 0;
	for (; g_OpNames[op][i] && i < 15; i++)
		b[i] = (TCHAR)g_OpNames[op][i];
	b[i] = 0;
	return b;
}

// ---------------------------------------------------------------------------------------------------------
// Resource parsing
// ---------------------------------------------------------------------------------------------------------

static inline DWORD Rd16(const BYTE *p, DWORD off) { return (DWORD)p[off] | ((DWORD)p[off + 1] << 8); }
static inline DWORD Rd32(const BYTE *p, DWORD off) { return (DWORD)p[off] | ((DWORD)p[off + 1] << 8) | ((DWORD)p[off + 2] << 16) | ((DWORD)p[off + 3] << 24); }

CScript::CScript()
	: m_bValid(FALSE), m_pRes(NULL), m_dwSize(0), m_dwEntryBase(0), m_dwPoolBase(0), m_dwKeysOffset(0), m_pNames(NULL)
{
	InitOps();
}

BOOL CScript::Parse(const BYTE *res, DWORD size)
{
	m_bValid = FALSE;
	m_pRes = res;
	m_dwSize = size;
	m_Functions.clear();
	m_IdToIndex.clear();
	m_Keys.clear();
	m_EntryPoints.clear();
	m_dwEntryBase = 0;
	m_AtomOffsets.clear();
	m_dwPoolBase = 0;
	m_dwKeysOffset = 0;

	if (res == NULL || size < 16 || size > 0x4000000)
		return FALSE;

	DWORD z1 = Rd16(res, 4);
	DWORD hs = Rd16(res, 6);
	DWORD p;

	if (z1 == 0 && hs > 0 && hs < 1000)
	{
		// the usual format: a table of numbers ("keys") follows the header
		if (8 + hs * 4 + 4 > size)
			return FALSE;
		for (DWORD i = 0; i < hs; i++)
			m_Keys.push_back(Rd32(res, 8 + i * 4));
		m_dwKeysOffset = 8;
		p = 8 + hs * 4;
	}
	else
	{
		// development build: the names of all atoms come first. A table of ascending offsets leads into the
		// name pool; the table has no length, it ends where the numbers stop ascending (the pool begins with
		// the last number that did)
		if (Rd32(res, 4) != 0)
			return FALSE;
		DWORD pos = 4;
		std::vector<DWORD> vals;
		while (pos + 4 <= size && vals.size() < 70000)
		{
			DWORD v = Rd32(res, pos);
			if (!vals.empty() && v < vals.back())
				break;
			vals.push_back(v);
			pos += 4;
		}
		if (vals.size() < 3 || pos + 4 > size)
			return FALSE;
		vals.pop_back();
		DWORD poolBase = pos - 4;
		if (vals.back() >= size - poolBase)
			return FALSE;

		// the pool ends after the last name; the constants follow, 4 byte aligned, as [0][count][-1 ...]
		DWORD q = poolBase + vals.back();
		while (q < size && res[q] != 0)
			q++;
		q++;
		q = (q - 1) & ~3;
		BOOL bFound = FALSE;
		for (; q + 12 <= size; q += 4)
		{
			if (Rd32(res, q) == 0 && Rd32(res, q + 4) > 0 && Rd32(res, q + 4) < 2000 && Rd32(res, q + 8) == 0xFFFFFFFF)
			{
				bFound = TRUE;
				break;
			}
		}
		if (!bFound)
			return FALSE;
		DWORD nk = Rd32(res, q + 4);
		if (q + 8 + nk * 4 + 4 > size)
			return FALSE;
		for (DWORD i = 0; i < nk; i++)
			m_Keys.push_back(Rd32(res, q + 8 + i * 4));
		m_dwKeysOffset = q + 8;
		p = q + 8 + nk * 4;
		m_AtomOffsets = vals;
		m_dwPoolBase = poolBase;
	}

	// function table: count, number of entry points, the entry points, [padding], offsets, functions
	if (p + 4 > size)
		return FALSE;
	DWORD nofs = Rd16(res, p);
	DWORD tl = Rd16(res, p + 2);
	if (nofs == 0 || p + 4 + tl * 2 > size)
		return FALSE;
	m_dwEntryBase = p + 4;
	for (DWORD i = 0; i < tl; i++)
		m_EntryPoints.push_back((int)Rd16(res, p + 4 + i * 2));
	DWORD offp = p + 4 + tl * 2;
	if (tl & 1)
		offp += 2;
	if (offp + 4 * (nofs + 1) > size)
		return FALSE;
	DWORD dbase = offp + 4 * (nofs + 1);

	// function record: id, parameters, variables, stack depth, byte code length, byte code, 2 bytes of padding
	for (DWORD k = 0; k < nofs; k++)
	{
		ULONGLONG s = (ULONGLONG)dbase + Rd32(res, offp + 4 * k);
		if (s + 10 > size)
			break;
		DWORD sl = Rd16(res, (DWORD)s + 8);
		if (s + 10 + sl > size)
			break;
		Function f;
		f.id = (short)Rd16(res, (DWORD)s);
		f.nParams = (int)Rd16(res, (DWORD)s + 2);
		f.nVars = (int)Rd16(res, (DWORD)s + 4);
		f.depth = (int)Rd16(res, (DWORD)s + 6);
		f.recOffset = (DWORD)s;
		f.codeLen = sl;
		m_IdToIndex[f.id] = (int)m_Functions.size();
		m_Functions.push_back(f);
	}
	if (m_Functions.empty())
		return FALSE;
	m_bValid = TRUE;
	return TRUE;
}

int CScript::FindFunction(int id) const
{
	std::map<int, int>::const_iterator it = m_IdToIndex.find(id);
	return it == m_IdToIndex.end() ? -1 : it->second;
}

const BYTE *CScript::GetCode(const Function &f) const
{
	return m_pRes + f.recOffset + 10;
}

BOOL CScript::IsEntryPoint(int id) const
{
	for (size_t i = 0; i < m_EntryPoints.size(); i++)
		if (m_EntryPoints[i] == id)
			return TRUE;
	return FALSE;
}

CString CScript::GetAtomName(int atom) const
{
	if (!m_AtomOffsets.empty())
	{
		if (atom >= 0 && atom < (int)m_AtomOffsets.size())
		{
			DWORD o = m_dwPoolBase + m_AtomOffsets[atom];
			CString s;
			while (o < m_dwSize && m_pRes[o] != 0 && s.GetLength() < 200)
				s += (TCHAR)m_pRes[o++];
			return s;
		}
		return CString();
	}
	if (m_pNames)
	{
		std::map<int, CString>::const_iterator it = m_pNames->find(atom);
		if (it != m_pNames->end())
			return it->second;
	}
	return CString();
}

CString CScript::AtomText(int atom) const
{
	CString s = GetAtomName(atom);
	if (s.IsEmpty())
		s.Format(_T("a%d"), atom);
	return s;
}

BOOL CScript::GetNumberValue(int operand, DWORD *pValue) const
{
	if (operand < 1)
		return FALSE;
	int idx = (operand - 1) >> 2;
	if (idx < 0 || idx >= (int)m_Keys.size())
		return FALSE;
	*pValue = m_Keys[idx];
	return TRUE;
}

DWORD CScript::GetNumberOffset(int operand) const
{
	if (operand < 1)
		return 0xFFFFFFFF;
	int idx = (operand - 1) >> 2;
	if (idx < 0 || idx >= (int)m_Keys.size())
		return 0xFFFFFFFF;
	return m_dwKeysOffset + 4 * (DWORD)idx;
}

CString CScript::NumberText(int operand) const
{
	DWORD v;
	CString s;
	if (!GetNumberValue(operand, &v))
	{
		s.Format(_T("k%d"), operand);
		return s;
	}
	if (v > 0x1000000)
	{
		// four printable characters: a message / type code such as 'Weel'
		BOOL bAscii = TRUE;
		for (int i = 0; i < 4; i++)
		{
			DWORD c = (v >> (24 - 8 * i)) & 0xFF;
			if (c < 32 || c >= 127)
				bAscii = FALSE;
		}
		if (bAscii)
		{
			s.Format(_T("'%c%c%c%c'"), (TCHAR)(v >> 24), (TCHAR)((v >> 16) & 0xFF), (TCHAR)((v >> 8) & 0xFF), (TCHAR)(v & 0xFF));
			return s;
		}
	}
	int sv = (int)v;
	if (sv > -1000000 && sv < 1000000)
		s.Format(_T("%d"), sv);
	else
		s.Format(_T("0x%x"), v);
	return s;
}

// ---------------------------------------------------------------------------------------------------------
// Decoding
// ---------------------------------------------------------------------------------------------------------

BOOL CScript::Decode(const Function &f, std::vector<Instruction> &out) const
{
	out.clear();
	const BYTE *code = GetCode(f);
	int n = (int)f.codeLen;
	int i = 0;
	while (i < n)
	{
		int op = code[i];
		Instruction ins;
		ins.off = i;
		ins.op = op;
		ins.arg = 0;
		ins.dflt = ins.low = ins.high = 0;
		if (op == 0 && i >= n - 1)
		{
			ins.len = 1;                         // padding at the very end
			out.push_back(ins);
			i++;
			continue;
		}
		BYTE len = g_OpLen[op];
		if (len == 0)
			return FALSE;
		if (op == OP_TABLESWITCH)
		{
			if (i + 7 > n)
				return FALSE;
			ins.dflt = (short)((code[i + 1] << 8) | code[i + 2]);
			ins.low = (code[i + 3] << 8) | code[i + 4];
			ins.high = (code[i + 5] << 8) | code[i + 6];
			int cnt = ins.high - ins.low + 1;
			if (cnt < 0 || cnt > 4096 || i + 7 + 2 * cnt > n)
				return FALSE;
			for (int c = 0; c < cnt; c++)
				ins.offs.push_back((short)((code[i + 7 + 2 * c] << 8) | code[i + 8 + 2 * c]));
			ins.len = 7 + 2 * cnt;
		}
		else if (op == OP_LOOKUPSWITCH)
		{
			return FALSE;                        // never seen in a firmware
		}
		else if (len == 3)
		{
			if (i + 3 > n)
				return FALSE;
			int a = (code[i + 1] << 8) | code[i + 2];
			if ((op == OP_GOTO || op == OP_IFEQ || op == OP_IFNE || op == OP_OR || op == OP_AND || op == OP_CASE || op == OP_DEFAULT) && a >= 0x8000)
				a -= 0x10000;
			ins.arg = a;
			ins.len = 3;
		}
		else
			ins.len = 1;
		out.push_back(ins);
		i += ins.len;
	}
	return i == n;
}

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

BOOL CScript::Disassemble(const Function &f, std::vector<Line> &out) const
{
	out.clear();
	std::vector<Instruction> ins;
	if (!Decode(f, ins))
		return FALSE;
	for (size_t k = 0; k < ins.size(); k++)
	{
		const Instruction &I = ins[k];
		if (I.op == 0 && I.off >= (int)f.codeLen - 1 && g_OpNames[0])
		{
			Line l;
			l.off = I.off; l.len = I.len; l.text = _T("(padding)");
			out.push_back(l);
			continue;
		}
		Line l;
		l.off = I.off;
		l.len = I.len;
		CString t = OpName(I.op);
		CString a;
		switch (I.op)
		{
		case OP_NAME: case OP_BINDNAME: case OP_SETNAME: case OP_GETPROP: case OP_SETPROP: case OP_DELNAME: case OP_DELPROP:
		case OP_INCNAME: case OP_INCPROP: case OP_DECNAME: case OP_DECPROP: case OP_NAMEINC: case OP_PROPINC:
		case OP_NAMEDEC: case OP_PROPDEC: case OP_SETCONST: case OP_NEW: case OP_STRING:
			a.Format(_T("%s"), (LPCTSTR)AtomText(I.arg));
			if (FindFunction(I.arg) >= 0 && (I.op == OP_NAME || I.op == OP_GETPROP))
			{
				Ref r; r.type = REF_FUNCTION; r.value = I.arg;
				r.token = a;
				r.off = r.insOff = (int)GetCodeOffset(f) + I.off + 1;
				r.len = 2;
				r.bigEndian = true;
				l.refs.push_back(r);
			}
			break;
		case OP_NUMBER:
			a = NumberText(I.arg);
			{
				DWORD v;
				if (GetNumberValue(I.arg, &v))
				{
					Ref r; r.type = REF_CONSTANT; r.value = (int)v;
					r.token = a;
					DWORD at = GetNumberOffset(I.arg);
					if (at != 0xFFFFFFFF)
					{
						r.off = (int)at;               // the number itself, in the table of constants
						r.len = 4;
					}
					r.insOff = (int)GetCodeOffset(f) + I.off + 1;
					l.refs.push_back(r);
				}
			}
			break;
		case OP_UINT16:
			a.Format(_T("%d"), I.arg);
			if (I.arg >= 128)
			{
				Ref r; r.type = REF_CONSTANT; r.value = I.arg;
				r.token = a;
				r.off = r.insOff = (int)GetCodeOffset(f) + I.off + 1;
				r.len = 2;
				r.bigEndian = true;
				l.refs.push_back(r);
			}
			break;
		case OP_CALL: case OP_GETARG: case OP_SETARG: case OP_GETVAR: case OP_SETVAR:
			a.Format(_T("%d"), I.arg);
			break;
		case OP_GOTO: case OP_IFEQ: case OP_IFNE: case OP_OR: case OP_AND: case OP_CASE: case OP_DEFAULT:
			a.Format(_T("L%d"), I.off + I.arg);
			l.gotoTarget = I.off + I.arg;
			break;
		case OP_TABLESWITCH:
			a.Format(_T("default L%d, %d..%d"), I.off + I.dflt, I.low, I.high);
			break;
		}
		if (!a.IsEmpty())
		{
			CString pad = t;
			while (pad.GetLength() < 12)
				pad += _T(" ");
			t = pad + a;
		}
		l.text = t;
		out.push_back(l);
		if (I.op == OP_TABLESWITCH)
		{
			for (size_t c = 0; c < I.offs.size(); c++)
			{
				Line cl;
				cl.indent = 1;
				cl.off = -1;
				cl.text.Format(_T("case %d: L%d"), I.low + (int)c, I.off + I.offs[c]);
				cl.gotoTarget = I.off + I.offs[c];
				out.push_back(cl);
			}
		}
	}
	return TRUE;
}

void CScript::CollectFunctionRefs(const Function &f, std::set<int> &ids) const
{
	std::vector<Instruction> ins;
	if (!Decode(f, ins))
		return;
	for (size_t k = 0; k < ins.size(); k++)
		if ((ins[k].op == OP_NAME || ins[k].op == OP_GETPROP) && FindFunction(ins[k].arg) >= 0)
			ids.insert(ins[k].arg);
}

void CScript::CollectFunctionSites(const Function &f, std::map<int, std::vector<int> > &sites) const
{
	std::vector<Instruction> ins;
	if (!Decode(f, ins))
		return;
	for (size_t k = 0; k < ins.size(); k++)
		if ((ins[k].op == OP_NAME || ins[k].op == OP_GETPROP) && FindFunction(ins[k].arg) >= 0)
			sites[ins[k].arg].push_back(ins[k].off);
}

// ---------------------------------------------------------------------------------------------------------
// Decompiler
// ---------------------------------------------------------------------------------------------------------

enum ExprKind { EK_NORMAL = 0, EK_THIS, EK_BIND, EK_VARREF, EK_ASSIGN };

struct Expr
{
	CString t;
	int p;
	int kind;
	std::vector<Ref> refs;
	Expr() : p(100), kind(EK_NORMAL) {}
	Expr(const TCHAR *text, int prec = 100, int k = EK_NORMAL) : t(text), p(prec), kind(k) {}
};

static CString Paren(const Expr &e, int minp)
{
	if (e.p < minp)
		return _T("(") + e.t + _T(")");
	return e.t;
}

static int PrecOf(int op, const TCHAR **sym)
{
	switch (op)
	{
	case OP_EQ: *sym = _T("=="); return 6;
	case OP_NE: *sym = _T("!="); return 6;
	case OP_LT: *sym = _T("<"); return 7;
	case OP_LE: *sym = _T("<="); return 7;
	case OP_GT: *sym = _T(">"); return 7;
	case OP_GE: *sym = _T(">="); return 7;
	case OP_ADD: *sym = _T("+"); return 9;
	case OP_SUB: *sym = _T("-"); return 9;
	case OP_MUL: *sym = _T("*"); return 10;
	case OP_DIV: *sym = _T("/"); return 10;
	case OP_MOD: *sym = _T("%"); return 10;
	case OP_BITAND: *sym = _T("&"); return 5;
	case OP_BITOR: *sym = _T("|"); return 3;
	case OP_BITXOR: *sym = _T("^"); return 4;
	case OP_LSH: *sym = _T("<<"); return 8;
	case OP_RSH: *sym = _T(">>"); return 8;
	case OP_URSH: *sym = _T(">>>"); return 8;
	}
	return 0;
}

class CDecompiler
{
public:
	CDecompiler(const CScript &s, const std::vector<Instruction> &ins, int codeBase)
		: m_s(s), m_ins(ins), m_end(0), m_bFail(false), m_bLeftover(false), m_lastEnd(0), m_codeBase(codeBase)
	{
		for (size_t k = 0; k < ins.size(); k++)
			m_idx[ins[k].off] = (int)k;
		m_end = ins.empty() ? 0 : ins.back().off + ins.back().len;
		m_idx[m_end] = (int)ins.size();
	}

	bool Failed() const { return m_bFail || m_bLeftover; }

	void Decompile(std::vector<Line> &out)
	{
		std::vector<Expr> stack;
		std::vector<Line> lines;
		Run(0, (int)m_ins.size(), stack, lines, 1);
		if (!stack.empty())
			m_bLeftover = true;

		// labels in front of the first line at or after each jump target
		std::set<int> targets;
		for (size_t i = 0; i < lines.size(); i++)
			if (lines[i].gotoTarget >= 0)
				targets.insert(lines[i].gotoTarget);
		if (targets.empty())
		{
			out = lines;
			return;
		}
		std::set<int>::iterator pending = targets.begin();
		for (size_t i = 0; i < lines.size(); i++)
		{
			while (pending != targets.end() && lines[i].off >= 0 && lines[i].off >= *pending)
			{
				Line l;
				l.indent = lines[i].indent;
				l.text.Format(_T("L%d:"), *pending);
				l.kind = LK_LABEL;
				out.push_back(l);
				++pending;
			}
			out.push_back(lines[i]);
		}
		for (; pending != targets.end(); ++pending)
		{
			Line l;
			l.indent = 1;
			l.text.Format(_T("L%d:"), *pending);
			l.kind = LK_LABEL;
			out.push_back(l);
		}
	}

private:
	const CScript &m_s;
	const std::vector<Instruction> &m_ins;
	std::map<int, int> m_idx;
	int m_end;
	bool m_bFail;
	bool m_bLeftover;
	int m_lastEnd;
	int m_codeBase;                    // where the byte code starts inside the resource

	Expr Pop(std::vector<Expr> &stack)
	{
		if (stack.empty())
		{
			m_bFail = true;
			return Expr(_T("/*?*/"));
		}
		Expr e = stack.back();
		stack.pop_back();
		return e;
	}

	int IdxOf(int off)
	{
		std::map<int, int>::iterator it = m_idx.find(off);
		if (it == m_idx.end())
		{
			m_bFail = true;
			return (int)m_ins.size();
		}
		return it->second;
	}

	int OffAt(int k) const { return k < (int)m_ins.size() ? m_ins[k].off : m_end; }
	int Tgt(int k) const { return m_ins[k].off + m_ins[k].arg; }

	static Line MakeLine(int indent, const CString &text, int off, const std::vector<Ref> &refs, int kind)
	{
		Line l;
		l.indent = indent;
		l.text = text;
		l.off = off;
		l.refs = refs;
		l.kind = kind;
		return l;
	}

	void AddRefs(std::vector<Ref> &to, const std::vector<Ref> &from)
	{
		for (size_t i = 0; i < from.size(); i++)
			to.push_back(from[i]);
	}

	int Run(int k, int endk, std::vector<Expr> &stack, std::vector<Line> &out, int indent);
	bool IsSingleIf(const std::vector<Line> &lines, int indent);
	void EmitSwitch(std::vector<Line> &out, int indent, int off, const Expr &disc,
	                const std::vector<int> &caseTargets, const std::vector<CString> &caseTexts,
	                const std::vector<std::vector<Ref> > &caseRefs, bool bHasDefault, int dflt,
	                int k, int endk, std::vector<Expr> &stack);
	int SwitchEnd(int k, int endk, const std::vector<int> &targets);
};

bool CDecompiler::IsSingleIf(const std::vector<Line> &lines, int indent)
{
	if (lines.empty())
		return false;
	if (lines[0].text.Left(4) != _T("if ("))
		return false;
	for (size_t i = 0; i < lines.size(); i++)
	{
		if (lines[i].indent == indent)
		{
			if (i == 0)
				continue;
			if (i == lines.size() - 1)
			{
				if (lines[i].text != _T("}"))
					return false;
			}
			else if (lines[i].text.Left(6) != _T("} else"))
				return false;
		}
	}
	if (lines.back().indent != indent || lines.back().text != _T("}"))
		return false;
	return true;
}

int CDecompiler::SwitchEnd(int k, int endk, const std::vector<int> &targets)
{
	int last = targets[0];
	for (size_t i = 1; i < targets.size(); i++)
		if (targets[i] > last)
			last = targets[i];
	int lastk = IdxOf(last);
	std::map<int, int> beyond;
	bool bAtLast = false;
	int stop = lastk < endk ? lastk : endk;
	for (int j = k; j < stop; j++)
	{
		if (m_ins[j].op == OP_GOTO)
		{
			int t = Tgt(j);
			if (t > last)
				beyond[t]++;
			else if (t == last)
				bAtLast = true;
		}
	}
	if (!beyond.empty())
	{
		int best = -1, bestCount = -1;
		for (std::map<int, int>::iterator it = beyond.begin(); it != beyond.end(); ++it)
			if (it->second > bestCount)
			{
				best = it->first;
				bestCount = it->second;
			}
		return best;
	}
	if (bAtLast)
		return last;
	for (int j = lastk; j < endk; j++)
	{
		if (m_ins[j].op == OP_GOTO)
		{
			int t = Tgt(j);
			if (t > last)
				beyond[t]++;
		}
	}
	if (!beyond.empty())
	{
		int best = -1, bestCount = -1;
		for (std::map<int, int>::iterator it = beyond.begin(); it != beyond.end(); ++it)
			if (it->second > bestCount)
			{
				best = it->first;
				bestCount = it->second;
			}
		return best;
	}
	return m_end;
}

void CDecompiler::EmitSwitch(std::vector<Line> &out, int indent, int off, const Expr &disc,
                             const std::vector<int> &caseTargets, const std::vector<CString> &caseTexts,
                             const std::vector<std::vector<Ref> > &caseRefs, bool bHasDefault, int dflt,
                             int k, int endk, std::vector<Expr> &stack)
{
	std::set<int> tset;
	for (size_t i = 0; i < caseTargets.size(); i++)
		tset.insert(caseTargets[i]);
	if (bHasDefault)
		tset.insert(dflt);
	std::vector<int> targets(tset.begin(), tset.end());
	if (targets.empty())
	{
		m_lastEnd = m_end;
		return;
	}
	int endT = SwitchEnd(k, endk, targets);
	m_lastEnd = endT;
	std::vector<std::vector<Line> > bodies(targets.size());
	for (size_t bi = 0; bi < targets.size(); bi++)
	{
		int boundTo = bi + 1 < targets.size() ? targets[bi + 1] : endT;
		std::vector<Expr> st = stack;
		Run(IdxOf(targets[bi]), IdxOf(boundTo), st, bodies[bi], indent + 2);
		std::vector<Line> &o = bodies[bi];
		CString gt;
		gt.Format(_T("goto L%d;"), endT);
		if (!o.empty() && o.back().kind == LK_GOTO && o.back().text == gt)
		{
			o.back().text = _T("break;");
			o.back().kind = LK_STATEMENT;
			o.back().gotoTarget = -1;
		}
	}
	m_lastEnd = endT;                            // a switch inside one of the bodies has changed it
	out.push_back(MakeLine(indent, _T("switch (") + disc.t + _T(") {"), off, disc.refs, LK_IF));
	for (size_t bi = 0; bi < targets.size(); bi++)
	{
		int tgt = targets[bi];
		for (size_t c = 0; c < caseTargets.size(); c++)
			if (caseTargets[c] == tgt)
				out.push_back(MakeLine(indent + 1, _T("case ") + caseTexts[c] + _T(":"), -1, caseRefs[c], LK_CASE));
		if (bHasDefault && dflt == tgt && !(tgt == endT && bodies[bi].empty()))
			out.push_back(MakeLine(indent + 1, _T("default:"), -1, std::vector<Ref>(), LK_CASE));
		for (size_t i = 0; i < bodies[bi].size(); i++)
			out.push_back(bodies[bi][i]);
	}
	out.push_back(MakeLine(indent, _T("}"), -1, std::vector<Ref>(), LK_BRACE));
}

int CDecompiler::Run(int k, int endk, std::vector<Expr> &stack, std::vector<Line> &out, int indent)
{
	const std::vector<Instruction> &ins = m_ins;
	while (k < endk && k < (int)ins.size())
	{
		const Instruction &I = ins[k];
		int op = I.op;
		int off = I.off;
		switch (op)
		{
		case OP_NOP:
			k++;
			continue;
		case OP_NAME:
			{
				Expr e(m_s.AtomText(I.arg), 100);
				if (m_s.FindFunction(I.arg) >= 0)
				{
					Ref r; r.type = REF_FUNCTION; r.value = I.arg;
					r.token = e.t;
					r.off = r.insOff = m_codeBase + off + 1;
					r.len = 2;
					r.bigEndian = true;
					e.refs.push_back(r);
				}
				stack.push_back(e);
				k++;
				continue;
			}
		case OP_NUMBER:
			{
				Expr e(m_s.NumberText(I.arg), 100);
				DWORD v;
				if (m_s.GetNumberValue(I.arg, &v))
				{
					Ref r; r.type = REF_CONSTANT; r.value = (int)v;
					r.token = e.t;
					DWORD at = m_s.GetNumberOffset(I.arg);
					if (at != 0xFFFFFFFF)
					{
						r.off = (int)at;               // the number itself, in the table of constants
						r.len = 4;
					}
					r.insOff = m_codeBase + off + 1;
					e.refs.push_back(r);
				}
				stack.push_back(e);
				k++;
				continue;
			}
		case OP_UINT16:
			{
				CString t;
				t.Format(_T("%d"), I.arg);
				Expr e(t, 100);
				Ref r; r.type = REF_CONSTANT; r.value = I.arg;
				r.token = t;
				r.off = r.insOff = m_codeBase + off + 1;
				r.len = 2;
				r.bigEndian = true;
				e.refs.push_back(r);
				stack.push_back(e);
				k++;
				continue;
			}
		case OP_ZERO: stack.push_back(Expr(_T("0"))); k++; continue;
		case OP_ONE: stack.push_back(Expr(_T("1"))); k++; continue;
		case OP_TRUE: stack.push_back(Expr(_T("true"))); k++; continue;
		case OP_FALSE: stack.push_back(Expr(_T("false"))); k++; continue;
		case OP_NULL: stack.push_back(Expr(_T("null"))); k++; continue;
		case OP_THIS: stack.push_back(Expr(_T("this"))); k++; continue;
		case OP_PUSH: stack.push_back(Expr(_T("undefined"))); k++; continue;
		case OP_GETARG:
			{
				CString t; t.Format(_T("arg%d"), I.arg);
				stack.push_back(Expr(t));
				k++;
				continue;
			}
		case OP_GETVAR:
			{
				CString t; t.Format(_T("var%d"), I.arg);
				stack.push_back(Expr(t, 100, EK_VARREF));
				k++;
				continue;
			}
		case OP_PUSHOBJ: stack.push_back(Expr(_T(""), 100, EK_THIS)); k++; continue;
		case OP_GETPROP:
			{
				Expr obj = Pop(stack);
				Expr e(Paren(obj, 100) + _T(".") + m_s.AtomText(I.arg), 100);
				e.refs = obj.refs;
				if (m_s.FindFunction(I.arg) >= 0)
				{
					Ref r; r.type = REF_FUNCTION; r.value = I.arg;
					r.token = m_s.AtomText(I.arg);
					r.off = r.insOff = m_codeBase + off + 1;
					r.len = 2;
					r.bigEndian = true;
					e.refs.push_back(r);
				}
				stack.push_back(e);
				k++;
				continue;
			}
		case OP_CALL:
			{
				int argc = I.arg;
				std::vector<Expr> args;
				for (int a = 0; a < argc; a++)
					args.push_back(Pop(stack));
				std::reverse(args.begin(), args.end());
				Expr thisv = Pop(stack);
				Expr fn = Pop(stack);
				CString t = Paren(fn, 100) + _T("(");
				for (size_t a = 0; a < args.size(); a++)
				{
					if (a)
						t += _T(", ");
					t += args[a].t;
				}
				t += _T(")");
				Expr e(t, 100);
				e.refs = fn.refs;
				for (size_t a = 0; a < args.size(); a++)
					AddRefs(e.refs, args[a].refs);
				stack.push_back(e);
				k++;
				continue;
			}
		case OP_EQ: case OP_NE: case OP_LT: case OP_LE: case OP_GT: case OP_GE: case OP_ADD: case OP_SUB:
		case OP_MUL: case OP_DIV: case OP_MOD: case OP_BITAND: case OP_BITOR: case OP_BITXOR: case OP_LSH:
		case OP_RSH: case OP_URSH:
			{
				const TCHAR *sym = _T("?");
				int p = PrecOf(op, &sym);
				Expr b = Pop(stack);
				Expr a = Pop(stack);
				Expr e(Paren(a, p) + _T(" ") + sym + _T(" ") + Paren(b, p + 1), p);
				e.refs = a.refs;
				AddRefs(e.refs, b.refs);
				stack.push_back(e);
				k++;
				continue;
			}
		case OP_NOT:
			{
				Expr a = Pop(stack);
				Expr e(_T("!") + Paren(a, 99), 99);
				e.refs = a.refs;
				stack.push_back(e);
				k++;
				continue;
			}
		case OP_NEG:
			{
				Expr a = Pop(stack);
				Expr e(_T("-") + Paren(a, 99), 99);
				e.refs = a.refs;
				stack.push_back(e);
				k++;
				continue;
			}
		case OP_BINDNAME: stack.push_back(Expr(_T(""), 100, EK_BIND)); k++; continue;
		case OP_SETNAME:
			{
				Expr v = Pop(stack);
				Expr b = Pop(stack);
				Expr e(m_s.AtomText(I.arg) + _T(" = ") + v.t, 0, EK_ASSIGN);
				e.refs = v.refs;
				stack.push_back(e);
				k++;
				continue;
			}
		case OP_SETVAR:
		case OP_SETARG:
			{
				Expr v = Pop(stack);
				CString t;
				t.Format(op == OP_SETVAR ? _T("var%d = ") : _T("arg%d = "), I.arg);
				Expr e(t + v.t, 0, EK_ASSIGN);
				e.refs = v.refs;
				stack.push_back(e);
				k++;
				continue;
			}
		case OP_POPV:
		case OP_POP:
			{
				Expr e = Pop(stack);
				CString txt = e.t + _T(";");
				if (e.kind == EK_VARREF)
					txt = _T("var ") + txt;
				out.push_back(MakeLine(indent, txt, off, e.refs, LK_STATEMENT));
				k++;
				continue;
			}
		case OP_RETURN:
			{
				Expr e = Pop(stack);
				out.push_back(MakeLine(indent, _T("return ") + e.t + _T(";"), off, e.refs, LK_STATEMENT));
				k++;
				continue;
			}
		case OP_OR:
		case OP_AND:
			{
				int T = Tgt(k);
				int tk = IdxOf(T);
				Expr lhs = Pop(stack);
				std::vector<Expr> nested;
				std::vector<Line> tmp;
				Run(k + 1, tk, nested, tmp, indent);
				Expr rhs;
				if (!tmp.empty() || nested.size() != 1)
				{
					m_bFail = true;
					rhs = Expr(_T("/*?*/"));
				}
				else
					rhs = nested.back();
				const TCHAR *sym = op == OP_OR ? _T("||") : _T("&&");
				int p = op == OP_OR ? 1 : 2;
				Expr e(Paren(lhs, p) + _T(" ") + sym + _T(" ") + Paren(rhs, p + 1), p);
				e.refs = lhs.refs;
				AddRefs(e.refs, rhs.refs);
				stack.push_back(e);
				k = tk;
				continue;
			}
		case OP_IFEQ:
		case OP_IFNE:
			{
				Expr cond = Pop(stack);
				if (op == OP_IFNE)
				{
					Expr c(_T("!") + Paren(cond, 99), 99);
					c.refs = cond.refs;
					cond = c;
				}
				int T = Tgt(k);
				if (T < off)
				{
					m_bFail = true;
					CString t;
					t.Format(_T("if (%s) goto L%d;"), (LPCTSTR)cond.t, T);
					Line l = MakeLine(indent, t, off, cond.refs, LK_GOTO);
					l.gotoTarget = T;
					out.push_back(l);
					k++;
					continue;
				}
				int tk = IdxOf(T);
				int last = tk - 1;

				// while loop: the instruction in front of the exit label jumps back to where the condition started
				if (last > k && ins[last].op == OP_GOTO && Tgt(last) <= off && m_idx.find(Tgt(last)) != m_idx.end())
				{
					int L0 = Tgt(last);
					bool bAny = false;
					for (size_t i = 0; i < out.size(); i++)
						if (out[i].off >= L0)
						{
							bAny = true;
							break;
						}
					if (!bAny)
					{
						std::vector<Expr> st1 = stack;
						std::vector<Line> o1;
						Run(k + 1, last, st1, o1, indent + 1);
						if (st1.size() != stack.size())
							m_bFail = true;
						out.push_back(MakeLine(indent, _T("while (") + cond.t + _T(") {"), off, cond.refs, LK_IF));
						for (size_t i = 0; i < o1.size(); i++)
							out.push_back(o1[i]);
						out.push_back(MakeLine(indent, _T("}"), -1, std::vector<Ref>(), LK_BRACE));
						k = tk;
						continue;
					}
				}

				bool bElse = false;
				int ek = tk;
				int thenFrom = k + 1, thenTo = tk, elseFrom = 0, elseTo = 0;
				if (last > k && ins[last].op == OP_GOTO && Tgt(last) > T)
				{
					int E = Tgt(last);
					ek = IdxOf(E);
					thenTo = last;
					elseFrom = tk;
					elseTo = ek;
					bElse = true;
				}
				std::vector<Expr> st1 = stack;
				std::vector<Line> o1;
				Run(thenFrom, thenTo, st1, o1, indent + 1);
				if (bElse)
				{
					std::vector<Expr> st2 = stack;
					std::vector<Line> o2;
					Run(elseFrom, elseTo, st2, o2, indent + 1);
					if (st1.size() == stack.size() + 1 && st2.size() == stack.size() + 1 && o1.empty() && o2.empty())
					{
						// conditional expression
						Expr e(Paren(cond, 2) + _T(" ? ") + Paren(st1.back(), 1) + _T(" : ") + Paren(st2.back(), 1), 0);
						e.refs = cond.refs;
						AddRefs(e.refs, st1.back().refs);
						AddRefs(e.refs, st2.back().refs);
						stack.push_back(e);
						k = ek;
						continue;
					}
					if (st1.size() != stack.size() || st2.size() != stack.size())
						m_bFail = true;
					out.push_back(MakeLine(indent, _T("if (") + cond.t + _T(") {"), off, cond.refs, LK_IF));
					for (size_t i = 0; i < o1.size(); i++)
						out.push_back(o1[i]);
					if (IsSingleIf(o2, indent + 1))
					{
						o2[0].text = _T("} else ") + o2[0].text;
						o2[0].indent = indent;
						o2[0].kind = LK_ELSEIF;
						for (size_t i = 1; i < o2.size(); i++)
							o2[i].indent--;
						for (size_t i = 0; i < o2.size(); i++)
							out.push_back(o2[i]);
						k = ek;
						continue;
					}
					out.push_back(MakeLine(indent, _T("} else {"), -1, std::vector<Ref>(), LK_BRACE));
					for (size_t i = 0; i < o2.size(); i++)
						out.push_back(o2[i]);
					out.push_back(MakeLine(indent, _T("}"), -1, std::vector<Ref>(), LK_BRACE));
				}
				else
				{
					if (st1.size() != stack.size())
						m_bFail = true;
					out.push_back(MakeLine(indent, _T("if (") + cond.t + _T(") {"), off, cond.refs, LK_IF));
					for (size_t i = 0; i < o1.size(); i++)
						out.push_back(o1[i]);
					out.push_back(MakeLine(indent, _T("}"), -1, std::vector<Ref>(), LK_BRACE));
				}
				k = ek;
				continue;
			}
		case OP_GOTO:
			{
				int T = Tgt(k);
				int endoff = OffAt(endk);
				if (T == endoff || (k + 1 < (int)ins.size() && T == ins[k + 1].off))
				{
					k++;
					continue;                      // jumps to where it would continue anyway
				}
				if (T == m_end)
				{
					out.push_back(MakeLine(indent, _T("return;"), off, std::vector<Ref>(), LK_STATEMENT));
					k++;
					continue;
				}
				CString t;
				t.Format(_T("goto L%d;"), T);
				Line l = MakeLine(indent, t, off, std::vector<Ref>(), LK_GOTO);
				l.gotoTarget = T;
				out.push_back(l);
				k++;
				continue;
			}
		case OP_CONDSWITCH:
			{
				Expr disc = Pop(stack);
				k++;
				std::vector<int> caseTargets;
				std::vector<CString> caseTexts;
				std::vector<std::vector<Ref> > caseRefs;
				bool bHasDefault = false;
				int dflt = 0;
				while (k < endk)
				{
					if (ins[k].op == OP_DEFAULT)
					{
						dflt = Tgt(k);
						bHasDefault = true;
						k++;
						break;
					}
					int j = k;
					while (j < (int)ins.size() && ins[j].op != OP_CASE)
						j++;
					if (j >= (int)ins.size())
					{
						m_bFail = true;
						break;
					}
					std::vector<Expr> nested;
					std::vector<Line> tmp;
					Run(k, j, nested, tmp, indent);
					Expr ce = nested.empty() ? Expr(_T("?")) : nested.back();
					caseTexts.push_back(ce.t);
					caseTargets.push_back(Tgt(j));
					caseRefs.push_back(ce.refs);
					k = j + 1;
				}
				EmitSwitch(out, indent, off, disc, caseTargets, caseTexts, caseRefs, bHasDefault, dflt, k, endk, stack);
				k = IdxOf(m_lastEnd);
				continue;
			}
		case OP_TABLESWITCH:
			{
				Expr disc = Pop(stack);
				int dflt = off + I.dflt;
				std::vector<int> caseTargets;
				std::vector<CString> caseTexts;
				std::vector<std::vector<Ref> > caseRefs;
				for (size_t c = 0; c < I.offs.size(); c++)
				{
					CString t;
					t.Format(_T("%d"), I.low + (int)c);
					caseTexts.push_back(t);
					caseTargets.push_back(off + I.offs[c]);
					caseRefs.push_back(std::vector<Ref>());
				}
				EmitSwitch(out, indent, off, disc, caseTargets, caseTexts, caseRefs, true, dflt, k + 1, endk, stack);
				k = IdxOf(m_lastEnd);
				continue;
			}
		default:
			{
				m_bFail = true;
				CString t;
				t.Format(_T("/* %s */"), OpName(op));
				out.push_back(MakeLine(indent, t, off, std::vector<Ref>(), LK_STATEMENT));
				k++;
				continue;
			}
		}
	}
	return k;
}

BOOL CScript::Decompile(const Function &f, std::vector<Line> &out, BOOL *pbClean) const
{
	out.clear();
	std::vector<Instruction> ins;
	if (!Decode(f, ins))
		return FALSE;
	CDecompiler d(*this, ins, (int)GetCodeOffset(f));
	d.Decompile(out);
	if (pbClean)
		*pbClean = d.Failed() ? FALSE : TRUE;
	return TRUE;
}

}   // namespace PixoScript
