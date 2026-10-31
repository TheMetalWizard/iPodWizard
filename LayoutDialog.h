#ifndef AFX_LAYOUTDIALOG_H__6D1C0A60_F616_43C6_9850_F3C0F052C0DB__INCLUDED_
#define AFX_LAYOUTDIALOG_H__6D1C0A60_F616_43C6_9850_F3C0F052C0DB__INCLUDED_

#pragma once

#include "Firmware.h"
#include "afxwin.h"
#include <Afxtempl.h>
#include "HexEditor.h"
#include "PicturesDialog.h"
#include "StringDialog.h"
#include "ToolTipListCtrl.h"
#include "LayoutNames.h"
#include "CommandTable.h"
#include "LayoutAttrs.h"
#include "StretchLayout.h"
#include <algorithm>
#include <iterator>

class CPicturesDialog;

// CLayoutDialog dialog

#include "CueEdit.h"

class CLayoutDialog : public CDarkDialog
{
	DECLARE_DYNAMIC(CLayoutDialog)

public:
	CLayoutDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CLayoutDialog();

// Dialog Data
	enum { IDD = IDD_LAYOUT_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
protected:
	virtual BOOL OnInitDialog();
	CFirmware * m_pFirmware;
	CLayoutNames m_Names;                  // descriptions appended to the IDs in the tree
	CCommandTable m_Commands;              // the command table: compiled script, read and decompiled
	CLayoutAttrs m_Attrs;                  // what the numbers in layouts, menus and font types mean
	std::vector<PixoScript::Line> m_CmdLines;   // what the rows of the list show in the command table modes
	int m_CmdFunction;                     // the function the list shows (0 when none)
	int m_ClickedCol;                      // column of the list cell that was clicked last
	bool m_bLinkClick;                     // the value is the one of a marked part that was clicked (the commands show it in the Change Value box too)
	bool m_bValBigEndian;                  // ... and it is stored high byte first
	int m_RequestedMode;                   // the mode the tab of the editor asks for (0 layouts ... 4 commands as assembly)
	bool m_bListsBuilt;                    // the tree has been filled for a firmware
	bool m_bModesReady;                    // a firmware with layouts, menus and font types is loaded
	std::vector<CLayoutAttrs::TypeUse> m_TypeUses;   // where the font type that is shown is used: the rows of its list

	// What each mode remembers while another one is shown: the selected entry of the tree (as the way down to it: which child
	// at every level), the entries that were open, how far the tree and the list were scrolled, the row and the bytes shown.
	struct ModeState
	{
		bool valid;
		std::vector<int> selPath, topPath;
		std::vector<std::vector<int> > expanded;
		int row, topRow, col, function;
		DWORD cursor, rangeStart, rangeLen, valOffset, valSize;
		ModeState() : valid(false), row(-1), topRow(0), col(-1), function(0), cursor(0), rangeStart(0), rangeLen(0), valOffset(0), valSize(0) {}
	};
	ModeState m_State[5];
	int m_LastCmdMode;                     // the command mode (3 or 4) left last: both show the same command
	bool m_bNoRestore;                     // the mode is changed to go to something: its old selection is not wanted
	void SaveModeState(int mode);
	void RestoreModeState(int mode);
	std::vector<int> TreePath(HTREEITEM h);
	HTREEITEM TreeItemOfPath(const std::vector<int> &path);
	void CollectExpanded(HTREEITEM first, std::vector<int> &path, std::vector<std::vector<int> > &out);
	std::map<int, int> m_CmdInsLen;        // offset in the byte code -> length of the instruction there (command shown)
	std::map<int, std::vector<PixoScript::Line> > m_CmdBodies;   // source of the commands that call the one shown (while its listing is built)
	CToolTipListCtrl m_ResourceList;
	CCueEdit m_ChangeEdit;                  // the box of Change Value: the value that would be changed shows in it in grey
	CCueEdit m_AddrEdit;                    // the box of Hex GoTo: the current offset of the hex view shows in it in grey
	DWORD m_AddrCueShown;
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDestroy();
	CTreeCtrl m_ResourceTree;
	CString m_FindString;
	CString m_ChangeString;
	DWORD m_HexAddr;
	CDWordArray sfilter_w;
	BOOL m_bSFilter;
	DWORD m_ValSize;
	DWORD m_ValOffset;
	int m_ResourceType;
	CHexWnd m_HexWnd;
	int m_hexBottom;
	int m_paneTop;
	int m_paneHeight;
	int m_paneHeightMin;
	int m_splitterGap;
	bool m_dragging;
	int m_dragStartY;
	int m_dragStartPaneHeight;
	// widths: the tree and the list share the width, the splitter between them can be dragged
	CStretchLayout m_Stretch;
	int m_treeLeft, m_treeWidth, m_colGap, m_listRightMargin;
	bool m_draggingCol;
	int m_dragStartX, m_dragStartTreeWidth;
	int  EffectiveTreeWidth();
	int  PageWidth();
	bool OnColumnSplitter(CPoint pt);
	int  EffectivePaneHeight();
	void LayoutSplitter();
	void Create_ViewTree(ViewRecord *view_rec, HTREEITEM hItem, vector<short> *id_list);

	// lists of layouts, menus and font types with their decoded values (LayoutLists.cpp, LayoutInput.cpp)
	void SetupViewColumns();
	void SetupMenuColumns();
	static int MenuLogicalCol(int listCol);                    // the field of a menu entry a column of the list shows
	void SetupTypeColumns();
	void FillViewList(ViewRecord *pRec);
	void FillMenuList(MenuRecord *pRec);
	void FillTypeList(TypeRecord *pRec);
	static void ViewCellRegion(const resource_type_view_elem *e, int col, DWORD *pOffset, DWORD *pSize);
	bool PickValue(const TCHAR *const *choices, int n, int current, DWORD offset, DWORD size);
	bool PickColor(DWORD offset, DWORD current);
	void RefreshSelection();
	CComboBox m_UsedCombo;                                      // the drop-down in the "Used by" cell of a font type
	void PlaceUsedCombo();
	CRect m_UsedPlaced;                                         // where the drop-down was put last (it is only moved when that changes)
	bool m_UsedShown;
	CDarkTabCtrl m_CmdTabs;                                     // "Table" and "Instructions": the two ways the commands are shown
	HWND m_hTree[4];                                            // the trees of the modes: layouts, menus, font types, commands
	int m_curTree;                                              // the one m_ResourceTree stands for now
	bool m_treeBuilt[4];                                        // the tree was filled for this firmware
	static int TreeIndex(int mode);
	void ActivateTree(int idx);
	int m_listTop0;                                             // top of the list when the sub-tabs are not shown
	void ShowCommandTabs();
	void UpdateLoadButtons();              // Load Resource / Load All are off in the Commands view when "Use old resource saving" is on
	afx_msg void OnTcnSelchangeCmdTabs(NMHDR *pNMHDR, LRESULT *pResult);
	HTREEITEM InsertLazy(const CString &text, HTREEITEM parent, DWORD_PTR data, bool hasChildren);   // an entry whose children are added when it is opened
	void ExpandViewItem(HTREEITEM h);                           // the layouts inside a layout are added when it is opened
	void ExpandAllViews();
	afx_msg void OnCbnSelendokUsedCombo();
	afx_msg LRESULT OnListPainted(WPARAM wParam, LPARAM lParam);
	void ShowLink(const CCellLink &link);                       // show the bytes a marked part of a cell stands for
	TypeRecord *SelectedType();
	TypeRecord *TypeOfItem(HTREEITEM h);

	// Command table modes (the compiled script: 3 shows it as source, 4 as assembly)
	bool IsCommandMode() const { return m_ResourceType == 3 || m_ResourceType == 4; }
	void CmdSetupColumns();
	void CmdFillTree();
	void CmdBuildLines(int functionId, bool bSource, std::vector<PixoScript::Line> &lines);
	CString CmdLineText(const PixoScript::Line &l, const PixoScript::Function *pf, bool bSource);
	void CmdFillList(int functionId);
	bool CmdRecord(int functionId, LPBYTE *pBuf, UINT *pLen);
	bool CmdReadRecord(CFile &file, int functionId, LPBYTE dest, UINT len);
	void CmdSelectRow(int row);
	PixoScript::Line CmdUseLine(int functionId, const CCommandTable::Use &u, int indent = 2);
	CString CmdCallContext(const PixoScript::Function &caller, int calleeId, const std::vector<int> &sites);
	PixoScript::Line CmdCalleeLine(int functionId, int callee);
	bool CmdLineAddress(const PixoScript::Line &l, const PixoScript::Function *pf, DWORD *pOff, DWORD *pLen);
	DWORD CmdRefAddress(const PixoScript::Ref &r);
	const PixoScript::Ref *CmdPrimaryRef(const PixoScript::Line &l);
	CString CmdLinkTip(const PixoScript::Ref &r);
	bool CmdConstantResolves(int value);
	void CmdFind(bool bNext);
	void CmdActivateRow(int row, CPoint screenPt);
	CString CmdRefTip(const PixoScript::Ref &ref);
	bool CmdChangeValue();
	void SelectResource(int mode, int id, int row = -1, DWORD hexOff = 0xFFFFFFFF, DWORD hexLen = 0, int col = -1);
	void GoToMatch(int kind, int id, int item = -1);
	void GoToRef(const std::vector<PixoScript::Ref> &refs, CPoint screenPt);
	afx_msg void OnTvnItemexpandingResourceTree(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnNMRClickResourceList(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnNMRClickResourceTree(NMHDR *pNMHDR, LRESULT *pResult);
	bool MenuChangeCommand();
	CString ChangeCueText();                  // the value that Change Value would change, in the form the box takes it
	void RefreshChangeCue();                  // shown in grey in the box while it is empty
	afx_msg void OnBnClickedValueKind();
	bool IsNotOnList(vector<short> *id_list, short id)
	{
		for (size_t i=0;i<(*id_list).size();i++)
		{
			if ((*id_list).at(i)==id)
				return false;
		}
		return true;
	}
protected:
	virtual void OnOK();
public:
	void SaveRAWResource(CString filename, LPBYTE lpBuf, UINT nCount);
	bool ResourceBytes(int mode, int id, LPBYTE *pBuf, UINT *pLen);          // one resource of a mode as it is in the firmware
	bool OldTcmdBytes(int id, LPBYTE *pBuf, UINT *pLen);                       // "Old resource saving": the old record of a command (what iPodWizard 1.3 saved)
	bool SelectedResource(int *pId, LPBYTE *pBuf, UINT *pLen);               // the resource that is selected in the tree
	static CString ResourceFileName(int mode, int id);                         // what a saved resource is called: view17.rsrc, menu17.rsrc, type17.rsrc, tcmd17.rsrc (the theme maker reads these)
	CComboBox m_ResourceTypeIdx;
	void SetFirmware(CFirmware *pFirmware);
	void SetMode(int mode);                // 0 layouts, 1 menus, 2 font types, 3 commands as source, 4 as assembly
	void SetPage(int page);                // the page of the editor: 0 layouts, 1 menus, 2 font types, 3 commands (either way of showing them)
	void UpdateModeText();
	afx_msg void OnBnClickedFindButton();
	afx_msg void OnEnChangeFindEdit();
	afx_msg void OnEnChangeChangeEdit();
	afx_msg void OnEnChangeAddrEdit();
	virtual BOOL PreTranslateMessage(MSG *pMsg);
	CToolTipCtrl m_LoadTip;                // "Old resource saving is enabled." over the two Load buttons while they are off because of it
	afx_msg void OnBnClickedFindnextButton();
	afx_msg void OnBnClickedChangeTextButton();
	afx_msg void OnBnClickedSaveResourceButton();
	afx_msg void OnBnClickedLoadChangesButton();
	afx_msg void OnBnClickedJumpToButton();
	afx_msg void OnBnClickedSelectorBarButton();
	void ChangeSelectorBar5G();                                 // the two colors of the selection bar: 5G (in the program code) ...
	void ChangeSelectorBar55G();                                // ... and 5.5G (in the layouts)
	void FindSelectorSlots(std::vector<DWORD> &slots);               // where the 5.5G keeps the two colors
	void FindString(HTREEITEM hStartItem);
	bool FindResource(HTREEITEM hParentItem, bool next, HTREEITEM hMarkItem, bool *startsearch);
	LPCTSTR GetString(DWORD lang, DWORD index);
	DWORD GetNumStrings(DWORD lang);
	DWORD GetStringOffset(DWORD index);
	afx_msg void OnCbnSelchangeResourceTypeidxCombo();
	afx_msg void OnLvnItemchangedResourceList(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnItemDblClickedResourceList(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnItemClickedResourceList(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnTvnSelchangedResourceTree(NMHDR *pNMHDR, LRESULT *pResult);
	DWORD m_selectorColor1;
	DWORD m_selectorColor2;
	BOOL  m_selectorCtxValid;
	BYTE  m_selectorCtxPre[4];
	BYTE  m_selectorCtxPost[4];
	ViewTable *ViewList;
	MenuTable *MenuList;
	TypeTable *TypeList;
	TCMDTable *TCMDList;
	void LoadAllResources(LPCTSTR folderPath, DWORD res_type, BOOL silent=FALSE);
public:
	afx_msg void OnBnClickedSaveallresourcesButton();
public:
	afx_msg void OnBnClickedLoadallresourcesButton();
	afx_msg void OnSize(UINT nType, int cx, int cy);
#ifdef IPW_TESTHOOK
	afx_msg LRESULT OnTestHook(WPARAM wParam, LPARAM lParam);       // test builds only: select/click/double click a list cell
#endif
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
};

#endif