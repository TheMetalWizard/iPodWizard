// NativeTable.h - the native objects and methods the user interface script talks to.
//
// The script (see PixoScript.h) calls methods of objects that the firmware itself implements (the system application, the
// UI framework, the music model ...). The firmware's own code (the "osos" image) carries two tables for them, found by
// their shape because their address changes with every build:
//   - the methods: entries of 8 bytes, a 16 bit atom, the number of parameters, flags (1, 8 or 12) and the offset of a
//     string of type codes inside the image. The same atom can be listed more than once (once per class that has it),
//     so a method can take a different number of parameters in different classes.
//   - the objects: records of 34 bytes (36 from 1.1.2 on), the first 16 bit word is (atom << 2) | 2, the second 0x409.
//     Word 4 is the record of the parent class (0xFFFF: none), word 2 the ID of the resource the object is made from
//     (0xFFFF: none), words 9 and 10 the first atom of the members and the end of the run of atoms they take.
// There is no address of the code behind a method in either table. The parameter counts agree with every call in the
// scripts of all 52 firmwares of the collection (67115 calls).
// Nothing here writes to the firmware.
//
// Released under the GNU General Public License v2, like the rest of iPodWizard.

#pragma once

#include <cstddef>
#include <map>
#include <vector>

class CNativeTable
{
public:
	struct Object
	{
		int atom;
		int resource;              // ID of the resource it is made from, -1: none
		int parent;                // index of the record of the parent class, -1: none
		int first, end;            // members: atoms first .. end - 1
	};
	struct Method
	{
		int atom;
		int params;
		int flags;
	};

	CNativeTable() { }

	void Clear() { m_Objects.clear(); m_Methods.clear(); m_ByAtom.clear(); m_ObjectByAtom.clear(); }

	// img: the "osos" image of the firmware
	bool Build(const unsigned char *img, unsigned long len)
	{
		Clear();
		if (img == NULL || len < 4096)
			return false;
		FindMethods(img, len);
		if (m_Methods.size() < 64)
		{
			Clear();
			return false;
		}
		FindObjects(img, len);
		for (size_t i = 0; i < m_Methods.size(); i++)
			m_ByAtom[m_Methods[i].atom].push_back((int)i);
		for (size_t i = 0; i < m_Objects.size(); i++)
			m_ObjectByAtom[m_Objects[i].atom] = (int)i;
		return true;
	}

	bool IsValid() const { return !m_Methods.empty(); }
	int  GetNumMethods() const { return (int)m_Methods.size(); }
	int  GetNumObjects() const { return (int)m_Objects.size(); }
	const Method &GetMethod(int i) const { return m_Methods[i]; }
	const Object &GetObject(int i) const { return m_Objects[i]; }

	// the lowest and highest number of parameters the atom takes; false when it is not a native method
	bool GetParams(int atom, int *pMin, int *pMax) const
	{
		std::map<int, std::vector<int> >::const_iterator it = m_ByAtom.find(atom);
		if (it == m_ByAtom.end())
			return false;
		int lo = 1000, hi = -1;
		for (size_t i = 0; i < it->second.size(); i++)
		{
			int p = m_Methods[it->second[i]].params;
			if (p < lo) lo = p;
			if (p > hi) hi = p;
		}
		*pMin = lo;
		*pMax = hi;
		return true;
	}

	const Object *FindObject(int atom) const
	{
		std::map<int, int>::const_iterator it = m_ObjectByAtom.find(atom);
		return it == m_ObjectByAtom.end() ? NULL : &m_Objects[it->second];
	}

	// the object whose run of member atoms holds the atom, 0 when none does (some members are listed apart from the run)
	int GetOwner(int atom) const
	{
		for (size_t i = 0; i < m_Objects.size(); i++)
			if (atom >= m_Objects[i].first && atom < m_Objects[i].end)
				return m_Objects[i].atom;
		return 0;
	}

private:
	static unsigned int U16(const unsigned char *p) { return p[0] | (p[1] << 8); }
	static unsigned long U32(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned long)p[3] << 24); }

	static bool MethodOk(const unsigned char *p, unsigned long len)
	{
		unsigned int a = U16(p);
		unsigned int fl = p[3];
		unsigned long ptr = U32(p + 4);
		return a > 0 && a < 4000 && p[2] < 12 && (fl == 1 || fl == 8 || fl == 12) && ptr > 0 && ptr < len;
	}

	// the longest run of entries that look like methods
	void FindMethods(const unsigned char *img, unsigned long len)
	{
		unsigned long bestAt = 0, bestCount = 0;
		unsigned long i = 0;
		while (i + 64 <= len)
		{
			if (!MethodOk(img + i, len))
			{
				i += 4;
				continue;
			}
			unsigned long j = i, count = 0;
			while (j + 8 <= len && MethodOk(img + j, len))
			{
				j += 8;
				count++;
			}
			if (count > bestCount)
			{
				bestCount = count;
				bestAt = i;
			}
			i = count > 1 ? j : i + 4;
		}
		for (unsigned long k = 0; k < bestCount; k++)
		{
			const unsigned char *p = img + bestAt + 8 * k;
			Method m;
			m.atom = (int)U16(p);
			m.params = p[2];
			m.flags = p[3];
			m_Methods.push_back(m);
		}
	}

	// the longest run of records that look like objects (their atoms follow each other)
	void FindObjects(const unsigned char *img, unsigned long len)
	{
		static const unsigned long strides[2] = { 0x22, 0x24 };
		unsigned long bestAt = 0, bestCount = 0, bestStride = 0x22;
		for (int s = 0; s < 2; s++)
		{
			unsigned long stride = strides[s];
			unsigned long i = 0;
			while (i + stride * 8 <= len)
			{
				unsigned int w0 = U16(img + i);
				if ((w0 & 3) != 2 || U16(img + i + 2) != 0x409)
				{
					i += 2;
					continue;
				}
				unsigned long j = i, count = 0;
				unsigned int prev = w0 - 4;
				while (j + stride <= len && (U16(img + j) & 3) == 2 && U16(img + j + 2) == 0x409 && U16(img + j) == prev + 4)
				{
					prev = U16(img + j);
					j += stride;
					count++;
				}
				if (count > bestCount)
				{
					bestCount = count;
					bestAt = i;
					bestStride = stride;
				}
				i = count > 1 ? j : i + 2;
			}
		}
		if (bestCount < 8)
			return;
		for (unsigned long k = 0; k < bestCount; k++)
		{
			const unsigned char *p = img + bestAt + bestStride * k;
			Object o;
			o.atom = (int)(U16(p) >> 2);
			o.resource = U16(p + 4) == 0xFFFF ? -1 : (int)U16(p + 4);
			o.parent = U16(p + 8) == 0xFFFF ? -1 : (int)U16(p + 8);
			o.first = (int)U16(p + 18);
			o.end = (int)U16(p + 20);
			m_Objects.push_back(o);
		}
	}

	std::vector<Object> m_Objects;
	std::vector<Method> m_Methods;
	std::map<int, std::vector<int> > m_ByAtom;       // atom -> indexes into m_Methods
	std::map<int, int> m_ObjectByAtom;
};
