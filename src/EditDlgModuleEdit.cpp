/*
 * ConCAD: Tool Options panel for a selected module (its parameters)
 * License:		Lesser GNU Public License 2.1 (LGPL)
 */

#include "stdafx.h"
#include "ConCadDoc.h"
#include "EditToolbar.h"
#include "DrawModuleInfo.h"

extern CEditToolbar g_EditToolBar;

#define IDC_MODULE_INPLACE 101
#define IDC_MODULE_INPLACE_COMBO 102

CEditDlgModuleEdit::CEditDlgModuleEdit()
{
	m_capture = FALSE;
	m_index = -1;
	m_column = 1;
	m_group = 0;
	m_pInplace = NULL;
	m_reading = false;
}

void CEditDlgModuleEdit::Create()
{
	CDialog::Create(IDD_MODULE_EDIT, &g_EditToolBar);
}

void CEditDlgModuleEdit::DoDataExchange(CDataExchange* pDX)
{
	CEditDlg::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST, m_list);
	DDX_Control(pDX, IDC_DELETE, m_Delete);
}

BEGIN_MESSAGE_MAP(CEditDlgModuleEdit, CEditDlg)
	ON_NOTIFY(NM_CLICK, IDC_LIST, OnClickList)
	ON_BN_CLICKED(IDC_ADD, OnAdd)
	ON_BN_CLICKED(IDC_DELETE, OnDelete)
	ON_EN_KILLFOCUS(IDC_MODULE_INPLACE, OnKillfocusEdit)
	ON_CBN_KILLFOCUS(IDC_MODULE_INPLACE_COMBO, OnKillfocusCombo)
	ON_NOTIFY(LVN_ITEMCHANGING, IDC_LIST, OnItemChanging)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST, OnItemChanged)
END_MESSAGE_MAP()

BOOL CEditDlgModuleEdit::OnInitDialog()
{
	CEditDlg::OnInitDialog();
	m_list.SetExtendedStyle(m_list.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES);
	m_list.InsertColumn(0, _T("Parameter"), LVCFMT_LEFT, 110);
	m_list.InsertColumn(1, _T("Value"), LVCFMT_LEFT, 160);
	return TRUE;
}

void CEditDlgModuleEdit::OnOK()
{
	EndEdit();
	SetFocus();
}

// The parameter name / value being edited: Copy / Cut / Paste act on it
bool CEditDlgModuleEdit::IsCellEditor(HWND hFocus) const
{
	const HWND h = m_capture && m_pInplace != NULL ? m_pInplace->GetSafeHwnd() : NULL;
	return h != NULL && (hFocus == h || ::IsChild(h, hFocus));
}

CDrawModuleInfo *CEditDlgModuleEdit::GetInfo()
{
	return m_pDesign != NULL ? m_pDesign->GetModuleInfo(m_group) : NULL;
}

void CEditDlgModuleEdit::Open(CConCadDoc *pDesign, CDrawModuleInfo *pInfo)
{
	if (m_capture)
	{
		EndEdit();
	}
	m_group = pInfo->m_group;
	Show(pDesign, pInfo);
	ReadFields();
}

bool CEditDlgModuleEdit::IsShowing(CConCadDoc *pDesign, int group) const
{
	return g_EditToolBar.m_pCurrentTool == this && m_pDesign == pDesign && m_group == group;
}

void CEditDlgModuleEdit::CloseIfShowing()
{
	if (g_EditToolBar.m_pCurrentTool == this)
	{
		if (m_capture)
		{
			EndEdit();
		}
		m_group = 0;
		Close();
	}
}

void CEditDlgModuleEdit::SetCheck(int row, int state)
{
	m_list.SetItemState(row, INDEXTOSTATEIMAGEMASK(state), LVIS_STATEIMAGEMASK);
}

int CEditDlgModuleEdit::FieldToRow(int field)
{
	for (int row = 0; row < m_list.GetItemCount(); row++)
	{
		if ((int)m_list.GetItemData(row) == field)
		{
			return row;
		}
	}
	return -1;
}

void CEditDlgModuleEdit::ReadFields()
{
	m_reading = true;
	m_list.DeleteAllItems();
	CDrawModuleInfo *pInfo = GetInfo();
	if (pInfo != NULL)
	{
		for (int i = 0; i < (int)pInfo->m_fields.size(); i++)
		{
			int row = m_list.GetItemCount();
			m_list.InsertItem(row, pInfo->m_fields[i].name);
			m_list.SetItemText(row, 1, pInfo->m_fields[i].value);
			m_list.SetItemData(row, (DWORD_PTR)i);
			SetCheck(row, 0); // only the BoM row has a tick box

			if (pInfo->m_fields[i].name.CompareNoCase(_T("Partnumber")) == 0)
			{
				row = m_list.GetItemCount();
				m_list.InsertItem(row, _T("Exclude from BoM"));
				m_list.SetItemData(row, (DWORD_PTR)(INT_PTR)BomRow);
				SetCheck(row, pInfo->m_excludeFromBom ? 2 : 1);
			}
		}
	}
	m_Delete.EnableWindow(FALSE);
	m_reading = false;
}

// Only the BoM row's tick box can change (Space would add one to any row)
void CEditDlgModuleEdit::OnItemChanging(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLISTVIEW* pNM = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
	*pResult = 0;
	if (!m_reading && (pNM->uChanged & LVIF_STATE) != 0
		&& ((pNM->uNewState ^ pNM->uOldState) & LVIS_STATEIMAGEMASK) != 0
		&& (int)m_list.GetItemData(pNM->iItem) != BomRow)
	{
		*pResult = TRUE;
	}
}

void CEditDlgModuleEdit::OnItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLISTVIEW* pNM = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
	*pResult = 0;
	if (m_reading || (pNM->uChanged & LVIF_STATE) == 0 || ((pNM->uNewState ^ pNM->uOldState) & LVIS_STATEIMAGEMASK) == 0
		|| (int)m_list.GetItemData(pNM->iItem) != BomRow)
	{
		return;
	}
	CDrawModuleInfo *pInfo = GetInfo();
	const bool exclude = (pNM->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK(2);
	if (pInfo != NULL && pInfo->m_excludeFromBom != exclude)
	{
		pInfo->m_excludeFromBom = exclude;
		Changed();
	}
}

void CEditDlgModuleEdit::Changed()
{
	m_pDesign->SetModifiedFlag(TRUE);
	m_pDesign->Invalidate(); // the module's texts show the new value
}

void CEditDlgModuleEdit::OnClickList(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;

	LVHITTESTINFO hit;
	const MSG *pMsg = GetCurrentMessage();
	hit.pt = CPoint(pMsg->pt.x, pMsg->pt.y);
	m_list.ScreenToClient(&hit.pt);
	if (m_list.SubItemHitTest(&hit) < 0 || hit.iItem < 0)
	{
		m_Delete.EnableWindow(FALSE);
		return;
	}

	if (m_capture)
	{
		EndEdit();
	}

	if ((int)m_list.GetItemData(hit.iItem) == BomRow)
	{
		// The BoM row: a click beside the tick box also toggles it
		m_Delete.EnableWindow(FALSE);
		if ((hit.flags & LVHT_ONITEMSTATEICON) == 0)
		{
			const bool ticked = m_list.GetItemState(hit.iItem, LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK(2);
			SetCheck(hit.iItem, ticked ? 1 : 2);
		}
		return;
	}

	m_index = (int)m_list.GetItemData(hit.iItem);
	CDrawModuleInfo *pInfo = GetInfo();
	const bool own = pInfo != NULL && m_index >= CDrawModuleInfo::FixedFields && m_index < (int)pInfo->m_fields.size()
		&& !CDrawModuleInfo::IsStandardField(pInfo->m_fields[m_index].name); // Name, Partnumber, Type are fixed
	m_Delete.EnableWindow(own);

	// Values can always be changed, names only of the module's own fields
	if (hit.iSubItem == 1 || own)
	{
		BeginEdit(m_index, hit.iSubItem == 0 ? 0 : 1);
	}
}

void CEditDlgModuleEdit::BeginEdit(int index, int column)
{
	CDrawModuleInfo *pInfo = GetInfo();
	if (pInfo == NULL || index < 0 || index >= (int)pInfo->m_fields.size())
	{
		return;
	}

	m_capture = TRUE;
	m_index = index;
	m_column = column;

	const int row = FieldToRow(index);
	CRect r;
	m_list.GetSubItemRect(row, column, LVIR_LABEL, r);
	if (column == 0)
	{
		// up to the Value column (the label starts after the tick box space)
		CRect value;
		m_list.GetSubItemRect(row, 1, LVIR_BOUNDS, value);
		r.right = value.left;
	}
	m_list.ClientToScreen(&r);
	ScreenToClient(&r);

	CString v = column == 0 ? pInfo->m_fields[index].name : pInfo->m_fields[index].value;

	// A value with a choice list (set when the module was stored): a
	// drop-down with the choices that still takes any typed value
	std::vector<CString> choices;
	if (column == 1)
	{
		CDrawModuleInfo::SplitChoices(pInfo->m_fields[index].choices, choices);
	}
	if (!choices.empty())
	{
		const int height = r.Height();
		r.bottom = r.top + 200;
		m_combo_control.Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWN | CBS_AUTOHSCROLL, r, this, IDC_MODULE_INPLACE_COMBO);
		m_combo_control.SetFont(GetFont());
		m_combo_control.SetItemHeight(-1, max(height - 6, 8));
		for (size_t i = 0; i < choices.size(); i++)
		{
			m_combo_control.AddString(choices[i]);
		}
		m_combo_control.LimitText(255);
		m_combo_control.SetWindowText(v);
		m_combo_control.SetEditSel(0, -1);
		m_pInplace = &m_combo_control;
		m_combo_control.SetFocus();
		m_combo_control.ShowDropDown(TRUE);
		return;
	}

	m_pInplace = &m_edit_control;
	m_edit_control.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, r, this, IDC_MODULE_INPLACE);
	m_edit_control.SetLimitText(255);
	m_edit_control.SetFont(GetFont());
	m_edit_control.SetWindowText(v);
	m_edit_control.SetSel(0, v.GetLength());
	m_edit_control.SetFocus();
}

void CEditDlgModuleEdit::EndEdit()
{
	if (!m_capture)
	{
		return;
	}
	m_capture = FALSE;

	CString v;
	if (m_pInplace != NULL)
	{
		m_pInplace->GetWindowText(v);
		m_pInplace->DestroyWindow();
		m_pInplace = NULL;
	}

	CDrawModuleInfo *pInfo = GetInfo();
	if (pInfo == NULL || m_index < 0 || m_index >= (int)pInfo->m_fields.size())
	{
		return;
	}

	CDrawModuleInfo::Field &f = pInfo->m_fields[m_index];
	CString &target = m_column == 0 ? f.name : f.value;
	if (m_column == 0)
	{
		v.Trim();
		v.Remove(_T('{'));
		v.Remove(_T('}'));
		if (v.IsEmpty())
		{
			return;
		}
	}
	if (target != v)
	{
		target = v;
		m_list.SetItemText(FieldToRow(m_index), m_column, v);
		Changed();
	}
}

void CEditDlgModuleEdit::OnKillfocusEdit()
{
	EndEdit();
}

void CEditDlgModuleEdit::OnKillfocusCombo()
{
	EndEdit();
}

void CEditDlgModuleEdit::OnAdd()
{
	CDrawModuleInfo *pInfo = GetInfo();
	if (pInfo == NULL)
	{
		return;
	}
	if (m_capture)
	{
		EndEdit();
	}

	CDrawModuleInfo::Field f;
	f.name = _T("Other");
	f.value = _T("..");
	pInfo->m_fields.push_back(f);
	Changed();
	ReadFields();

	// Start with the new field's name
	BeginEdit((int)pInfo->m_fields.size() - 1, 0);
}

void CEditDlgModuleEdit::OnDelete()
{
	CDrawModuleInfo *pInfo = GetInfo();
	if (pInfo == NULL || m_index < CDrawModuleInfo::FixedFields || m_index >= (int)pInfo->m_fields.size()
		|| CDrawModuleInfo::IsStandardField(pInfo->m_fields[m_index].name))
	{
		return;
	}
	if (m_capture)
	{
		m_capture = FALSE;
		if (m_pInplace != NULL)
		{
			m_pInplace->DestroyWindow();
			m_pInplace = NULL;
		}
	}
	pInfo->m_fields.erase(pInfo->m_fields.begin() + m_index);
	m_index = -1;
	Changed();
	ReadFields();
}
