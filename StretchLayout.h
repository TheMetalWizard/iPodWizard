// StretchLayout.h - lets a dialog follow a change of its width.
//
// The dialog templates are laid out for one fixed width. Init() remembers where every control sat at that
// width; Apply() then, for the current width,
//  - widens the controls named as "stretch" by the extra width,
//  - moves controls that sat in the right half of the dialog along with the right edge (unless told not to: a page whose
//    buttons stay at the left; the controls named as "move" go with the right edge whatever the page does),
//  - leaves every other control, and the ones named as "skip" (which the dialog places itself), alone.
// Only horizontal position and width are touched; vertical position and height stay as they are.
//
// Released under the GNU General Public License v2, like the rest of iPodWizard.

#pragma once

#include <vector>

// How much wider the main window is than at its minimum width. The main window sets it; the pages inside
// it read it, so they all use the same measure whatever size their own template had.
inline int &StretchExtraWidth()
{
	static int s_extra = 0;
	return s_extra;
}

class CStretchLayout
{
public:
	CStretchLayout() : m_baseCx(0) {}

	BOOL IsInit() const { return m_baseCx > 0; }
	int BaseWidth() const { return m_baseCx; }

	void Init(CWnd *pDlg)
	{
		CRect rcClient;
		pDlg->GetClientRect(&rcClient);
		m_baseCx = rcClient.Width();
		m_Items.clear();
		for (CWnd *pWnd = pDlg->GetWindow(GW_CHILD); pWnd; pWnd = pWnd->GetNextWindow())
		{
			Item it;
			it.hWnd = pWnd->m_hWnd;
			pWnd->GetWindowRect(&it.rc);
			pDlg->ScreenToClient(&it.rc);
			m_Items.push_back(it);
		}
	}

	// dx: how much wider the dialog is than it was laid out (negative: narrower)
	void Apply(CWnd *pDlg, int dx, const int *stretchIds, int nStretch, const int *skipIds, int nSkip, bool bMoveRight = true,
	           const int *moveIds = NULL, int nMove = 0)
	{
		if (!IsInit())
			return;
		for (size_t i = 0; i < m_Items.size(); i++)
		{
			HWND hWnd = m_Items[i].hWnd;
			if (!::IsWindow(hWnd))
				continue;
			int id = ::GetDlgCtrlID(hWnd);
			if (Contains(skipIds, nSkip, id))
				continue;

			CRect cur;
			::GetWindowRect(hWnd, &cur);
			pDlg->ScreenToClient(&cur);
			const CRect &orig = m_Items[i].rc;

			if (Contains(stretchIds, nStretch, id))
			{
				if (cur.left != orig.left || cur.Width() != orig.Width() + dx)
					::SetWindowPos(hWnd, NULL, orig.left, cur.top, orig.Width() + dx, cur.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
			}
			else if (Contains(moveIds, nMove, id) || (bMoveRight && orig.left > m_baseCx / 2))
			{
				if (cur.left != orig.left + dx)
					::SetWindowPos(hWnd, NULL, orig.left + dx, cur.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
			}
		}
	}

private:
	struct Item
	{
		HWND hWnd;
		CRect rc;
	};
	std::vector<Item> m_Items;
	int m_baseCx;

	static bool Contains(const int *ids, int n, int id)
	{
		for (int i = 0; i < n; i++)
			if (ids[i] == id)
				return true;
		return false;
	}
};
