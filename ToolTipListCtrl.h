#if !defined(AFX_TOOLTIPLISTCTRL_H__EA17BA6D_ADD2_49E3_AB67_45B65316D19F__INCLUDED_)
#define AFX_TOOLTIPLISTCTRL_H__EA17BA6D_ADD2_49E3_AB67_45B65316D19F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ToolTipListCtrl.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CToolTipListCtrl window

#include <afxtempl.h>
#include <map>
#include <vector>

/////////////////////////////////////////////////////////////////////////////
// CToolTipListCtrl, v.1.0
// 
// A CListCtrl derived class that
// can display per SubItem tooltips by itself
//
// Author: João Filipe de Castro Ferreira (jfilipe@isr.uc.pt)
// Based on Nate Maynard's (nate.maynard@neomation.com) CToolTipTreeCtrl
//
// Last Modified: 7/11/2001
//
// License: Quoting Nate Maynard,
//  "use it however you want. If it helps you out, drop me a line and let me know. :-)"
//
// Disclaimer: This code comes with no warranty of any kind whatsoever. Use at your own risk.
//
/////////////////////////////////////////////////////////////////////////////


//The initial state of m_wHitMask
#define INITIAL_HITMASK LVHT_ONITEMLABEL

// A part of the text of a cell that stands for bytes of the firmware. It is drawn with a frame around it, a click on
// it shows the bytes in the hex view, and its tooltip says where they are and what they hold.
struct CCellLink
{
	int start;                 // first character of the cell text it covers
	int len;                   // number of characters
	DWORD offset;              // where the bytes are in the firmware image
	DWORD size;                // how many bytes
	int col;                   // the column whose value they are (what Change Value changes), -1 for none
	int kind;                  // free for the owner
	DWORD_PTR data;            // free for the owner
	bool bigEndian;            // the bytes hold a number stored high byte first
	CString tip;
	CCellLink() : start(0), len(0), offset(0), size(0), col(-1), kind(0), data(0), bigEndian(false) {}
};

class CToolTipListCtrl : public CListCtrl
{
// Construction
public:
	CToolTipListCtrl();

// Attributes
public:

protected:
	// Map's SubItems to related tooltip text
	CMapStringToString m_ToolTipMap; 
	// A bit mask of LVHT_* flags the control will show tooltips for
	WORD m_wHitMask; 
	// The text of the tooltip being shown (the tooltip control keeps pointing at it)
	CStringW m_TipTextW;
	CStringA m_TipTextA;
	// Parts of cells that are attached to bytes of the firmware (key: item * 100 + subitem)
	std::map<int, std::vector<CCellLink> > m_Links;
	// The tooltip of the list itself: the one of the framework went away again at once, so the list shows its own and
	// tells it which cell the pointer is over
	CToolTipCtrl m_OwnTip;
	UINT_PTR m_OwnTipKey;
	CString m_OwnTipText;
	CString TipAt(CPoint pt, UINT_PTR *pKey) const;

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CToolTipListCtrl)
	//}}AFX_VIRTUAL

// Implementation
public:


	//** CWnd Overrides **
	//Provide our own logic for HitTests, specifically, make ToolHitTests respond per SubItem
	virtual int OnToolHitTest(CPoint point, TOOLINFO * pTI) const;

	
    //** CTreeCtrl Overrides **
    //Overriding the Delete functions makes sure m_ToolTipMap doesn't have excess mappings
    virtual BOOL DeleteAllItems( );
    virtual BOOL DeleteItem( int nItem );


	//** Additional Functions **

	//Set the TVHT_* flags that will trigger the display of a tooltip
	WORD SetToolTipHitMask(WORD wHitMask);
	//Clear all tooltips
	virtual void DeleteAllToolTips();
	//Set the tooltip text for a specific SubItem
	virtual BOOL SetItemToolTipText( int nItem, int nSubItem, LPCTSTR lpszToolTipText ); 
	//Retrieves the tooltip text for a specific SubItem 
	virtual CString GetItemToolTipText( int nItem, int nSubItem );

	// Parts of the text of a cell that are attached to bytes of the firmware (see CCellLink)
	void SetCellLinks(int nItem, int nSubItem, const std::vector<CCellLink> &links);
	const std::vector<CCellLink> *GetCellLinks(int nItem, int nSubItem) const;
	BOOL GetLinkRect(int nItem, int nSubItem, const CCellLink &link, CRect *pRect) const;      // client coordinates
	BOOL HitLink(CPoint pt, int *pItem, int *pSubItem, int *pLink) const;                       // pt in client coordinates

	
	virtual ~CToolTipListCtrl();

	// Generated message map functions
protected:
	//{{AFX_MSG(CToolTipListCtrl)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG

	//Respondes to the TTN_NEEDTEXT* messages, provides the text of a tooltip
	virtual afx_msg BOOL OnToolTipText( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	virtual void PreSubclassWindow();
	virtual BOOL PreTranslateMessage(MSG *pMsg);
	afx_msg void OnCustomDraw(NMHDR *pNMHDR, LRESULT *pResult);                                // frames around the links
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);                                       // a control placed on the list tells the owner of the list

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TOOLTIPLISTCTRL_H__EA17BA6D_ADD2_49E3_AB67_45B65316D19F__INCLUDED_)
