// ToolTipListCtrl.cpp : implementation file
//

#include "stdafx.h"
#include "ToolTipListCtrl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CToolTipListCtrl

CToolTipListCtrl::CToolTipListCtrl()
{
	m_wHitMask = INITIAL_HITMASK;
	m_OwnTipKey = (UINT_PTR)-1;
}

CToolTipListCtrl::~CToolTipListCtrl()
{
	// Cleanup when destroyed

	DeleteAllToolTips();
}


BEGIN_MESSAGE_MAP(CToolTipListCtrl, CListCtrl)
	//{{AFX_MSG_MAP(CToolTipListCtrl)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP

   //Trap all TTN_NEEDTEXT* Messages
   //TTN_NEEDTEXT* messages are sent when a ToolTipCtrl wants a control
   //to provide it with text to display as the tooltip.
   //Specifically, when the TOOLINFO structure passed back to the ToolTipCtrl
   //after ::OnToolHitTest has it's lpszText memeber set to LPSTR_TEXTCALLBACK.

	ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTW, 0, 0xFFFF, OnToolTipText)
	ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTA, 0, 0xFFFF, OnToolTipText)
	ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, OnCustomDraw)
	ON_WM_MOUSEMOVE()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CToolTipListCtrl message handlers

int CToolTipListCtrl::OnToolHitTest(CPoint point, TOOLINFO * pTI) const {
	// the list shows its own tooltip (see OnMouseMove); none from the framework, or two would fight over the pointer
	return -1;
}

// The text for the tooltip at a point (client coordinates): the tip of a link, or else the one of the cell. pKey tells
// which one it is (item, subitem and link), so a change of it can be noticed.
CString CToolTipListCtrl::TipAt(CPoint pt, UINT_PTR *pKey) const
{
	*pKey = (UINT_PTR)-1;
	LVHITTESTINFO info;
	memset(&info, 0, sizeof(info));
	info.pt = pt;
	int nItem = ListView_SubItemHitTest(m_hWnd, &info);
	if (nItem < 0 || info.iSubItem < 0 || !(info.flags & m_wHitMask))
		return CString();
	CString text;
	int nLink = -1, hitItem, hitSub;
	if (HitLink(pt, &hitItem, &hitSub, &nLink) && hitItem == nItem && hitSub == info.iSubItem)
	{
		const std::vector<CCellLink> *pLinks = GetCellLinks(nItem, info.iSubItem);
		if (pLinks != NULL && nLink >= 0 && nLink < (int)pLinks->size() && !pLinks->at(nLink).tip.IsEmpty())
		{
			*pKey = (UINT_PTR)((nItem * 100 + info.iSubItem) * 1024 + nLink + 1);
			return pLinks->at(nLink).tip;
		}
	}
	CString strKey;
	strKey.Format(_T("%d"), nItem * 100 + info.iSubItem);
	if (!m_ToolTipMap.Lookup(strKey, text) || text.IsEmpty())
		return CString();
	*pKey = (UINT_PTR)((nItem * 100 + info.iSubItem) * 1024);
	return text;
}

void CToolTipListCtrl::PreSubclassWindow()
{
	CListCtrl::PreSubclassWindow();
	if (m_OwnTip.m_hWnd == NULL && m_OwnTip.Create(this, TTS_ALWAYSTIP | TTS_NOPREFIX))
	{
		m_OwnTip.AddTool(this, _T(" "));
		m_OwnTip.SetMaxTipWidth(480);
		m_OwnTip.SetDelayTime(TTDT_INITIAL, 400);
		m_OwnTip.SetDelayTime(TTDT_AUTOPOP, 30000);
		m_OwnTip.SetDelayTime(TTDT_RESHOW, 100);
		m_OwnTip.Activate(FALSE);
	}
}

BOOL CToolTipListCtrl::PreTranslateMessage(MSG *pMsg)
{
	if (m_OwnTip.m_hWnd != NULL && pMsg->hwnd == m_hWnd)
		m_OwnTip.RelayEvent(pMsg);
	return CListCtrl::PreTranslateMessage(pMsg);
}

void CToolTipListCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_OwnTip.m_hWnd != NULL)
	{
		UINT_PTR key;
		CString text = TipAt(point, &key);
		if (key != m_OwnTipKey || text != m_OwnTipText)
		{
			m_OwnTipKey = key;
			m_OwnTipText = text;
			m_OwnTip.Pop();                      // another cell: the old tip goes, the new one comes after the delay
			if (text.IsEmpty())
				m_OwnTip.Activate(FALSE);
			else
			{
				m_OwnTip.UpdateTipText(text, this);
				m_OwnTip.Activate(TRUE);
			}
		}
	}
	CListCtrl::OnMouseMove(nFlags, point);
}

BOOL CToolTipListCtrl::OnToolTipText( UINT id, NMHDR * pNMHDR, LRESULT * pResult ){
	// The list shows its own tooltip (see OnMouseMove). The tip window of the list view itself asks for text too, and
	// when it was given some, a second tip of another font and size came up over the first one a moment later: it gets none.
	*pResult = 0;
	if (pNMHDR->code == TTN_NEEDTEXTA)
		((TOOLTIPTEXTA*)pNMHDR)->lpszText = "";
	else
		((TOOLTIPTEXTW*)pNMHDR)->lpszText = L"";
	return TRUE;
}

//Sets the tooltip text for a specific item
BOOL CToolTipListCtrl::SetItemToolTipText( int nItem, int nSubItem, LPCTSTR lpszToolTipText ){

  CString strKey;

  strKey.Format(_T("%d"),  nItem * 100 + nSubItem);

  m_ToolTipMap.SetAt( strKey, lpszToolTipText );

  return TRUE;

}

//Retrieve the tooltip text for a specific list item
CString CToolTipListCtrl::GetItemToolTipText( int nItem, int nSubItem){

  CString itemToolTipText;

  CString strKey;

  strKey.Format(_T("%d"),  nItem * 100 + nSubItem);


  if( !m_ToolTipMap.Lookup( strKey, itemToolTipText ) ){
     itemToolTipText = "";
  }

  return itemToolTipText;
}


WORD CToolTipListCtrl::SetToolTipHitMask( WORD wHitMask ){
   WORD oldHitMask = m_wHitMask;

   m_wHitMask = wHitMask;

   return oldHitMask;
}

void CToolTipListCtrl::DeleteAllToolTips()
{
   m_ToolTipMap.RemoveAll();
   m_Links.clear();
}

BOOL CToolTipListCtrl::DeleteAllItems( ){
	//Call the base class method
	BOOL retVal = CListCtrl::DeleteAllItems();

	if( retVal ){
		//If it succeeded, remove all tooltips
		  DeleteAllToolTips();
	}

	return retVal;
}

BOOL CToolTipListCtrl::DeleteItem( int nItem ){

    //Call the base class method
    BOOL retVal = CListCtrl::DeleteItem( nItem );

    if( retVal ){
		//If it succeeded, remove it's tooltip from the map

		LVCOLUMN *lvColumn = (LVCOLUMN*) malloc (sizeof(LVCOLUMN));

		int i;
		for (i = 0; GetColumn(i, lvColumn) != 0; i++);
		{
			CString strKey;

			strKey.Format(_T("%d"),  nItem * 100 + i);

			m_ToolTipMap.RemoveKey( strKey );
		}
    }

	return retVal;
}

/////////////////////////////////////////////////////////////////////////////
// Links: parts of the text of a cell that are attached to bytes of the firmware

void CToolTipListCtrl::SetCellLinks(int nItem, int nSubItem, const std::vector<CCellLink> &links)
{
	int key = nItem * 100 + nSubItem;
	if (links.empty())
		m_Links.erase(key);
	else
		m_Links[key] = links;
}

const std::vector<CCellLink> *CToolTipListCtrl::GetCellLinks(int nItem, int nSubItem) const
{
	std::map<int, std::vector<CCellLink> >::const_iterator it = m_Links.find(nItem * 100 + nSubItem);
	return it == m_Links.end() ? NULL : &it->second;
}

// Where the text of a cell starts: the list control draws it a little inside the cell
static int TextInset()
{
	return 3 * ::GetSystemMetrics(SM_CXEDGE);
}

// The rectangle a link occupies in the list (client coordinates): the characters it covers, measured in the font of the
// list, plus a little room for its frame
BOOL CToolTipListCtrl::GetLinkRect(int nItem, int nSubItem, const CCellLink &link, CRect *pRect) const
{
	RECT rcCell;
	if (!ListView_GetSubItemRect(m_hWnd, nItem, nSubItem, LVIR_LABEL, &rcCell))
		return FALSE;
	CRect cell(rcCell);
	CString text = GetItemText(nItem, nSubItem);
	if (link.start < 0 || link.len <= 0 || link.start + link.len > text.GetLength())
		return FALSE;
	HDC hdc = ::GetDC(m_hWnd);
	if (hdc == NULL)
		return FALSE;
	HFONT hFont = (HFONT)::SendMessage(m_hWnd, WM_GETFONT, 0, 0);
	HGDIOBJ hOld = ::SelectObject(hdc, hFont ? hFont : (HFONT)::GetStockObject(DEFAULT_GUI_FONT));
	SIZE s1, s2;
	::GetTextExtentPoint32(hdc, (LPCTSTR)text, link.start, &s1);
	::GetTextExtentPoint32(hdc, (LPCTSTR)text, link.start + link.len, &s2);
	::SelectObject(hdc, hOld);
	::ReleaseDC(m_hWnd, hdc);
	int left = cell.left + TextInset();
	pRect->SetRect(left + s1.cx - 1, cell.top, left + s2.cx + 1, cell.bottom);
	return TRUE;
}

// The link under a point (client coordinates)
BOOL CToolTipListCtrl::HitLink(CPoint pt, int *pItem, int *pSubItem, int *pLink) const
{
	if (m_Links.empty())
		return FALSE;
	LVHITTESTINFO info;
	memset(&info, 0, sizeof(info));
	info.pt = pt;
	int nItem = ListView_SubItemHitTest(m_hWnd, &info);
	if (nItem < 0 || info.iSubItem < 0)
		return FALSE;
	const std::vector<CCellLink> *pLinks = GetCellLinks(nItem, info.iSubItem);
	if (pLinks == NULL)
		return FALSE;
	for (size_t i = 0; i < pLinks->size(); i++)
	{
		CRect rc;
		if (GetLinkRect(nItem, info.iSubItem, pLinks->at(i), &rc) && rc.PtInRect(pt))
		{
			*pItem = nItem;
			*pSubItem = info.iSubItem;
			*pLink = (int)i;
			return TRUE;
		}
	}
	return FALSE;
}

// After a row has been drawn the frames of the links in it are drawn on top
void CToolTipListCtrl::OnCustomDraw(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLVCUSTOMDRAW pDraw = reinterpret_cast<LPNMLVCUSTOMDRAW>(pNMHDR);
	*pResult = CDRF_DODEFAULT;
	if (pDraw->nmcd.dwDrawStage == CDDS_PREPAINT && GetParent() != NULL)
		GetParent()->PostMessage(WM_USER + 778);        // the list was drawn again (scrolled, resized): the owner may move what sits over it
	if (m_Links.empty())
		return;
	switch (pDraw->nmcd.dwDrawStage)
	{
	case CDDS_PREPAINT:
		*pResult = CDRF_NOTIFYITEMDRAW;
		break;
	case CDDS_ITEMPREPAINT:
		{
			int item = (int)pDraw->nmcd.dwItemSpec;
			std::map<int, std::vector<CCellLink> >::const_iterator it = m_Links.lower_bound(item * 100);
			if (it != m_Links.end() && it->first < item * 100 + 100)
				*pResult = CDRF_NOTIFYPOSTPAINT;
		}
		break;
	case CDDS_ITEMPOSTPAINT:
		{
			int item = (int)pDraw->nmcd.dwItemSpec;
			bool bSelected = (GetItemState(item, LVIS_SELECTED) & LVIS_SELECTED) != 0;
			bool bFocused = ::GetFocus() == m_hWnd;
			// on the selection the frames take the colour of its text, elsewhere the colour links have
			COLORREF color = (bSelected && bFocused) ? ::GetSysColor(COLOR_HIGHLIGHTTEXT) : RGB(0, 102, 204);
			if (!(bSelected && bFocused))
			{
				COLORREF back = ListView_GetBkColor(m_hWnd);
				// a dark list (dark mode) needs a light frame
				if (back != CLR_NONE && GetRValue(back) + GetGValue(back) + GetBValue(back) < 3 * 100)
					color = RGB(110, 170, 255);
			}
			HDC hdc = pDraw->nmcd.hdc;
			HPEN hPen = ::CreatePen(PS_SOLID, 1, color);
			HGDIOBJ hOldPen = ::SelectObject(hdc, hPen);
			HGDIOBJ hOldBrush = ::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
			std::map<int, std::vector<CCellLink> >::const_iterator it = m_Links.lower_bound(item * 100);
			for (; it != m_Links.end() && it->first < item * 100 + 100; ++it)
			{
				int sub = it->first - item * 100;
				CRect cell;
				if (!GetSubItemRect(item, sub, LVIR_LABEL, cell))
					continue;
				int saved = ::SaveDC(hdc);
				::IntersectClipRect(hdc, cell.left, cell.top, cell.right, cell.bottom);
				for (size_t i = 0; i < it->second.size(); i++)
				{
					CRect rc;
					if (!GetLinkRect(item, sub, it->second[i], &rc))
						continue;
					::RoundRect(hdc, rc.left, rc.top + 1, rc.right, rc.bottom - 1, 4, 4);
				}
				::RestoreDC(hdc, saved);
			}
			::SelectObject(hdc, hOldBrush);
			::SelectObject(hdc, hOldPen);
			::DeleteObject(hPen);
		}
		break;
	}
}

// A control that sits on the list (the drop-down of the font types) sends its messages to the list; they are for its owner
BOOL CToolTipListCtrl::OnCommand(WPARAM wParam, LPARAM lParam)
{
	if (lParam != 0 && GetParent() != NULL)
	{
		GetParent()->SendMessage(WM_COMMAND, wParam, lParam);
		return TRUE;
	}
	return CListCtrl::OnCommand(wParam, lParam);
}
