// ComboFill.h - fast bulk filling of combo boxes.
//
// Adding thousands of items one by one to a visible combo box is slow: the list grows a little at a time and
// the control redraws as it goes (a font with 2,600 glyphs took several seconds to fill three lists). Reserving
// the space first and holding redraws back until the end makes the same fill roughly 20 times faster.
//
// Released under the GNU General Public License v2, like the rest of iPodWizard.

#pragma once

class CComboFill
{
public:
	// empties the combo and prepares it for nItems short strings
	static void Begin(CComboBox &cb, DWORD nItems, DWORD nCharsPerItem = 12)
	{
		cb.SetRedraw(FALSE);
		cb.ResetContent();
		if (nItems)
			cb.InitStorage(nItems, nItems * (nCharsPerItem + 1) * sizeof(WCHAR));
	}

	static void End(CComboBox &cb)
	{
		cb.SetRedraw(TRUE);
		cb.Invalidate();
	}

	static void AddNumber(CComboBox &cb, int n)
	{
		WCHAR w[16];
		_itow_s(n, w, 16, 10);
		::SendMessageW(cb.m_hWnd, CB_ADDSTRING, 0, (LPARAM)w);
	}

	static void AddText(CComboBox &cb, const WCHAR *psz)
	{
		::SendMessageW(cb.m_hWnd, CB_ADDSTRING, 0, (LPARAM)psz);
	}
};
