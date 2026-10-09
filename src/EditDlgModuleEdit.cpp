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

CEditDlgModuleEdit::CEditDlgModuleEdit()
{
	m_capture = FALSE;
	m_index = -1;
	m_column = 1;
	m_group = 0;
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
END_MESSAGE_MAP()

BOOL CEditDlgModuleEdit::OnInitDialog()
{
	CEditDlg::OnInitDialog();
	m_list.SetExtendedStyle(m_list.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_list.InsertColumn(0, _T("Parameter"), LVCFMT_LEFT, 110);
	m_list.InsertColumn(1, _T("Value"), LVCFMT_LEFT, 160);
	return TRUE;
}

void CEditDlgModuleEdit::OnOK()
{
	EndEdit();
	SetFocus();
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

void CEditDlgModuleEdit::ReadFields()
{
	m_list.DeleteAllItems();
	CDrawModuleInfo *pInfo = GetInfo();
	if (pInfo != NULL)
	{
		for (int i = 0; i < (int)pInfo->m_fields.size(); i++)
		{
			m_list.InsertItem(i, pInfo->m_fields[i].name);
			m_list.SetItemText(i, 1, pInfo->m_fields[i].value);
		}
	}
	m_Delete.EnableWindow(FALSE);
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

	m_index = hit.iItem;
	const bool own = m_index >= CDrawModuleInfo::FixedFields; // Name and Reference are fixed
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

	CRect r;
	m_list.GetSubItemRect(index, column, LVIR_LABEL, r);
	if (column == 0)
	{
		r.right = r.left + m_list.GetColumnWidth(0);
	}
	m_list.ClientToScreen(&r);
	ScreenToClient(&r);

	CString v = column == 0 ? pInfo->m_fields[index].name : pInfo->m_fields[index].value;
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
	m_edit_control.GetWindowText(v);
	m_edit_control.DestroyWindow();

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
		m_list.SetItemText(m_index, m_column, v);
		Changed();
	}
}

void CEditDlgModuleEdit::OnKillfocusEdit()
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
	if (pInfo == NULL || m_index < CDrawModuleInfo::FixedFields || m_index >= (int)pInfo->m_fields.size())
	{
		return;
	}
	if (m_capture)
	{
		m_capture = FALSE;
		m_edit_control.DestroyWindow();
	}
	pInfo->m_fields.erase(pInfo->m_fields.begin() + m_index);
	m_index = -1;
	Changed();
	ReadFields();
}
