// LayoutInput.cpp : clicking and double clicking the cells of the Layout tab's lists.
//
// A click selects the value in the hex view (and makes it the value "Change Value" writes). A double click opens
// what the value refers to (a string, a bitmap, a layout, a font, a command) or lets the value be changed (colors,
// the style and the position of text).

#include "stdafx.h"
#include "iPodWizard.h"
#include "LayoutDialog.h"

typedef PixoScript::Ref Ref;

// Where the value in a list column of a layout element is stored, relative to the start of the element
void CLayoutDialog::ViewCellRegion(const resource_type_view_elem *e, int col, DWORD *pOffset, DWORD *pSize)
{
	const BYTE *b = (const BYTE *)e;
	*pOffset = 0;
	*pSize = 0;
	CLayoutAttrs::Extra x;
	CLayoutAttrs::ExtraOf(e, &x);
	switch (col)
	{
	case 0:  *pOffset = 0;  *pSize = 4; break;
	case 1:  *pOffset = 4;  *pSize = 4; break;
	case 2:  *pOffset = 8;  *pSize = 4; break;
	case 3:  *pOffset = 12; *pSize = 2; break;
	case 4:  *pOffset = 14; *pSize = 2; break;
	case 5:  *pOffset = 16; *pSize = 4; break;
	case 6:  *pOffset = 20; *pSize = 4; break;
	case 7:  *pOffset = 24; *pSize = 2; break;
	case 8:  *pOffset = 26; *pSize = 2; break;
	case 9:  *pOffset = (DWORD)((const BYTE *)&e->left.val - b);   *pSize = 4; break;
	case 10: *pOffset = (DWORD)((const BYTE *)&e->top.val - b);    *pSize = 4; break;
	case 11: *pOffset = (DWORD)((const BYTE *)&e->right.val - b);  *pSize = 4; break;
	case 12: *pOffset = (DWORD)((const BYTE *)&e->bottom.val - b); *pSize = 4; break;
	case 13: *pOffset = 76; *pSize = x.gradient ? 4 : 0; break;
	case 14: *pOffset = 80; *pSize = x.gradient ? 4 : 0; break;
	case 15: *pOffset = 76; *pSize = 0; break;
	case 18: *pOffset = (DWORD)((const BYTE *)&e->left - b);   *pSize = 8; break;      // the descriptors: reference, mode and flag of each edge
	case 19: *pOffset = (DWORD)((const BYTE *)&e->top - b);    *pSize = 8; break;
	case 20: *pOffset = (DWORD)((const BYTE *)&e->right - b);  *pSize = 8; break;
	case 21: *pOffset = (DWORD)((const BYTE *)&e->bottom - b); *pSize = 8; break;
	case 16:
		if (x.text >= 0)
		{
			*pOffset = 76 + 4 * x.text + 2;       // the high half of the {string, font type} value
			*pSize = 2;
		}
		else
			*pOffset = 76;
		break;
	}
}

void CLayoutDialog::OnLvnItemClickedResourceList(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMLV = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);

	if (m_ResourceList.GetFirstSelectedItemPosition()<=0)
		return;
	if (pNMLV->iItem==-1)
		return;

	int iItem=pNMLV->iItem;
	int col=pNMLV->iSubItem;
	m_ClickedCol=(m_ResourceType==1) ? MenuLogicalCol(col) : col;       // for a menu: the field its column shows
	m_bLinkClick=false;

	// a marked part of a cell: the bytes it stands for are shown in the hex view
	{
		int hitItem, hitSub, hitLink;
		if (m_ResourceList.HitLink(pNMLV->ptAction, &hitItem, &hitSub, &hitLink))
		{
			const std::vector<CCellLink> *pLinks=m_ResourceList.GetCellLinks(hitItem, hitSub);
			if (pLinks!=NULL && hitLink<(int)pLinks->size())
			{
				ShowLink(pLinks->at(hitLink));
				UpdateData(FALSE);
				return;
			}
		}
	}
	int sel_id=_ttoi(m_ResourceTree.GetItemText(m_ResourceTree.GetSelectedItem()));
	LPBYTE lpBuf=m_pFirmware->GetFirmwareBuffer();
	DWORD j, rel=0, sz=0;
	const BYTE *base=NULL;

	switch(m_ResourceType)
	{
	case 0: //view
		for (j = 0; j < ViewList->size(); j++)
			if (ViewList->at(j)->id==sel_id)
			{
				if (iItem < ViewList->at(j)->num_elems)
				{
					const resource_type_view_elem *e=ViewList->at(j)->ViewElemList[iItem]->element;
					ViewCellRegion(e, col, &rel, &sz);
					base=(const BYTE *)e;
				}
				break;
			}
		break;
	case 1: //menu
		for (j = 0; j < MenuList->size(); j++)
			if (MenuList->at(j)->id==sel_id)
			{
				if (iItem < MenuList->at(j)->num_elems)
				{
					const resource_type_menu_elem *e=MenuList->at(j)->MenuElemList[iItem]->element;
					if (MenuLogicalCol(col)<=12)
						m_Attrs.MenuField(MenuLogicalCol(col), &rel, &sz);
					base=(const BYTE *)e;
				}
				break;
			}
		break;
	case 2: //type
		{
			TypeRecord *pType=SelectedType();
			if (pType!=NULL && iItem==0)
			{
				static const DWORD toff[7]={0,2,4,8,12,13,14};
				static const DWORD tsz[7]={2,2,4,4,1,1,2};
				if (col>=0 && col<7)
				{
					rel=toff[col];
					sz=tsz[col];
				}
				base=(const BYTE *)pType->element;
			}
		}
		break;
	case 3: //commands
	case 4:
		CmdSelectRow(iItem);
		break;
	}

	if (base!=NULL)
	{
		DWORD iOffset=(DWORD)(base-lpBuf)+rel;
		m_ValSize=sz;
		m_ValOffset=iOffset;
		m_HexWnd.SetRange(iOffset, sz);
	}

	UpdateData(FALSE);
	RefreshChangeCue();
}

void CLayoutDialog::OnLvnItemDblClickedResourceList(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMLV = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);

	if (m_ResourceList.GetFirstSelectedItemPosition()<=0 && *pResult!=1)
		return;
	if (pNMLV->iItem==-1)
		return;

	int iItem=pNMLV->iItem;
	int col=pNMLV->iSubItem;
	int sel_id=_ttoi(m_ResourceTree.GetItemText(m_ResourceTree.GetSelectedItem()));
	LPBYTE lpBuf=m_pFirmware->GetFirmwareBuffer();
	CPoint pt(pNMLV->ptAction);
	m_ResourceList.ClientToScreen(&pt);
	HWND hEditor=::GetParent(::GetParent(m_hWnd));
	DWORD j;

	switch(m_ResourceType)
	{
	case 0: //view
		for (j = 0; j < ViewList->size(); j++)
			if (ViewList->at(j)->id==sel_id)
				break;
		if (j==ViewList->size() || iItem>=ViewList->at(j)->num_elems)
			return;
		{
			const resource_type_view_elem *e=ViewList->at(j)->ViewElemList[iItem]->element;
			CLayoutAttrs::Extra x;
			CLayoutAttrs::ExtraOf(e, &x);
			switch (col)
			{
			case 3: //what the element shows
				if (CLayoutAttrs::IsKind(e, " rtS"))
				{
					int sid=e->id!=0 ? e->id : (x.text>=0 ? (short)(x.p[x.text] & 0xFFFF) : 0);
					if (sid!=0)
						::SendMessage(hEditor, WM_APP, (WPARAM)11, (LPARAM)sid);
				}
				else if (CLayoutAttrs::IsKind(e, "paMB"))
					::SendMessage(hEditor, WM_APP, (WPARAM)12, (LPARAM)m_pFirmware->GetPictureIndexFromID((DWORD)e->id));
				else if (CLayoutAttrs::IsKind(e, "enoN") && e->id!=0)
				{
					for (DWORD k = 0; k < ViewList->size(); k++)
						if (ViewList->at(k)->id==e->id)
						{
							SelectResource(0, e->id);
							break;
						}
				}
				break;
			case 5: //numbers that may be the ID of something
			case 6:
				{
					Ref r;
					r.type=PixoScript::REF_CONSTANT;
					r.value=col==5 ? e->z1 : e->z2;
					std::vector<PixoScript::Ref> refs;
					refs.push_back(r);
					GoToRef(refs, pt);
				}
				break;
			case 13: //the two colors of a gradient
			case 14:
				{
					DWORD o1, o2;
					if (!m_Attrs.ViewColorOffsets(e, &o1, &o2))
					{
						MessageBox(_T("This element has no gradient colors."));
						break;
					}
					DWORD off=col==13 ? o1 : o2;
					PickColor((DWORD)((const BYTE *)e-lpBuf)+off, *(const DWORD *)((const BYTE *)e+off));
				}
				break;
			case 16: //the font of a text
				{
					int ft=m_Attrs.ViewFontType(e);
					if (ft!=0)
						SelectResource(2, ft);
				}
				break;
			}
		}
		break;
	case 1: //menu
		for (j = 0; j < MenuList->size(); j++)
			if (MenuList->at(j)->id==sel_id)
				break;
		if (j==MenuList->size() || iItem>=MenuList->at(j)->num_elems)
			return;
		{
			const resource_type_menu_elem *e=MenuList->at(j)->MenuElemList[iItem]->element;
			switch (MenuLogicalCol(col))
			{
			case 2: //commands: opened from the ID columns only
			case 3:
				if (col!=2 && col!=4)
					break;
				{
					int fn=CCommandTable::RefToFunction(MenuLogicalCol(col)==2 ? e->z2 : e->cmd);
					if (fn>=0 && m_Commands.FindFunction(fn)!=NULL)
						SelectResource(3, fn);
				}
				break;
			case 4: //an ID
				{
					Ref r;
					r.type=PixoScript::REF_CONSTANT;
					r.value=e->submenuID;
					std::vector<PixoScript::Ref> refs;
					refs.push_back(r);
					GoToRef(refs, pt);
				}
				break;
			case 6: //the text
				::SendMessage(hEditor, WM_APP, (WPARAM)11, (LPARAM)m_Attrs.MenuTextId(e));
				break;
			case 10:
				::SendMessage(hEditor, WM_APP, (WPARAM)11, (LPARAM)e->textID2);
				break;
			case 8: //numbers that may be the ID of something
			case 9:
				{
					DWORD off, size;
					m_Attrs.MenuField(MenuLogicalCol(col), &off, &size);
					DWORD v=*(const DWORD *)((const BYTE *)e+off);
					std::vector<PixoScript::Ref> refs;
					Ref r;
					r.type=PixoScript::REF_CONSTANT;
					r.value=(int)(v & 0xFFFF);
					refs.push_back(r);
					if (v>>16)
					{
						r.value=(int)(v>>16);
						refs.push_back(r);
					}
					GoToRef(refs, pt);
				}
				break;
			case 12: //the font
				if (e->typeID!=0)
					SelectResource(2, e->typeID);
				else if (m_Attrs.MenuDefaultFont(MenuList->at(j)->id)!=0)
					SelectResource(2, m_Attrs.MenuDefaultFont(MenuList->at(j)->id));
				break;
			}
		}
		break;
	case 2: //type
		if (SelectedType()==NULL)
			return;
		{
			resource_type_type_elem *e=SelectedType()->element;
			DWORD base=(DWORD)((const BYTE *)e-lpBuf);
			switch (col)
			{
			case 0: //the name of the font
				::SendMessage(hEditor, WM_APP, (WPARAM)11, (LPARAM)e->fontID);
				break;
			case 2: //the font itself
				::SendMessage(hEditor, WM_APP, (WPARAM)13, (LPARAM)e);
				break;
			case 3: //the color
				PickColor(base+8, e->color);
				break;
			case 4: //style: set in the hex value, no chooser
				break;
			case 7: //where the font is used: go there
					if (iItem<(int)m_TypeUses.size())
					{
						CLayoutAttrs::TypeUse u=m_TypeUses[iItem];
						SelectResource(u.kind==CLayoutAttrs::TU_MENU ? 1 : 0, u.id, u.row, u.offset, u.size, u.kind==CLayoutAttrs::TU_MENU ? 12 : (u.kind==CLayoutAttrs::TU_LIST ? 3 : 16));
					}
					break;
				case 5: //position of the text
				{
					static const TCHAR *const aligns[3]={ _T("Left (0)"), _T("Center (1)"), _T("Right (2)") };
					PickValue(aligns, 3, e->alignment, base+13, 1);
				}
				break;
			}
		}
		break;
	case 3: //commands
	case 4:
		CmdActivateRow(iItem, pt);
		break;
	}

	UpdateData(FALSE);
}
