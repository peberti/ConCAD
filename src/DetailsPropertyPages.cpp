/*
 TinyCAD program for schematic capture
 Copyright 1994/1995/2002-2005 Matt Pyne.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// DetailsPropertyPages.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "MultiSheetDoc.h"
#include "DetailsPropertyPages.h"
#include "TinyCadDoc.h"

IMPLEMENT_DYNCREATE(CDetailsPropertyPage1, CPropertyPage)
IMPLEMENT_DYNCREATE(CDetailsPropertyPage2, CPropertyPage)
IMPLEMENT_DYNCREATE(CDetailsPropertyPage3, CPropertyPage)
IMPLEMENT_DYNCREATE(CDetailsPropertyPage4, CPropertyPage)

// Reserved token names (built-ins exposed by CDetails::Resolve)
static bool IsReservedTokenName(const CString& sName)
{
	static const TCHAR* const reserved[] = {
		_T("Title"), _T("Author"), _T("Revision"),
		_T("DocNo"), _T("Document"),
		_T("Organisation"), _T("Org"),
		_T("Sheets"), _T("Date"),
	};
	for (size_t i = 0; i < sizeof(reserved) / sizeof(reserved[0]); ++i)
	{
		if (sName.CompareNoCase(reserved[i]) == 0)
		{
			return true;
		}
	}
	return false;
}

static bool IsValidTokenName(const CString& sName)
{
	const int len = sName.GetLength();
	if (len == 0)
	{
		return false;
	}
	for (int i = 0; i < len; ++i)
	{
		TCHAR c = sName[i];
		bool ok = (c >= _T('A') && c <= _T('Z'))
		       || (c >= _T('a') && c <= _T('z'))
		       || (c >= _T('0') && c <= _T('9'))
		       || c == _T('_');
		if (!ok)
		{
			return false;
		}
	}
	// First char cannot be a digit
	TCHAR first = sName[0];
	if (first >= _T('0') && first <= _T('9'))
	{
		return false;
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////
// CDetailsPropertyPage1 property page

CDetailsPropertyPage1::CDetailsPropertyPage1(CMultiSheetDoc* pDesign) :
	CPropertyPage(CDetailsPropertyPage1::IDD)
{
	//{{AFX_DATA_INIT(CDetailsPropertyPage1)
	m_sAuthor = _T("");
	m_sDate = _T("");
	m_sOrg = _T("");
	m_sDoc = _T("");
	m_sRevision = _T("");
	m_sSheets = _T("");
	m_sTitle = _T("");
	m_bIsVisible = FALSE;
	//}}AFX_DATA_INIT

	m_pDesign = pDesign;
}

CDetailsPropertyPage1::~CDetailsPropertyPage1()
{
}

void CDetailsPropertyPage1::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDetailsPropertyPage1)
	DDX_Text(pDX, DESIGNBOX_AUTHOR, m_sAuthor);
	DDX_Text(pDX, DESIGNBOX_DATE, m_sDate);
	DDX_Text(pDX, DESIGNBOX_ORGANISATION, m_sOrg);
	DDX_Text(pDX, DESIGNBOX_DOCUMENT, m_sDoc);
	DDX_Text(pDX, DESIGNBOX_REVISION, m_sRevision);
	DDX_Text(pDX, DESIGNBOX_SHEET, m_sSheets);
	DDX_Text(pDX, DESIGNBOX_TITLE, m_sTitle);
	DDX_Check(pDX, DESIGNBOX_DISPLAY, m_bIsVisible);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CDetailsPropertyPage1, CPropertyPage)
//{{AFX_MSG_MAP(CDetailsPropertyPage1)
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDetailsPropertyPage2 property page

CDetailsPropertyPage2::CDetailsPropertyPage2(CMultiSheetDoc* pDesign) :
	CPropertyPage(CDetailsPropertyPage2::IDD)
{
	//{{AFX_DATA_INIT(CDetailsPropertyPage2)
	m_bHasRulers = FALSE;
	m_horiz = _T("");
	m_vert = _T("");
	//}}AFX_DATA_INIT

	m_pDesign = pDesign;
}

CDetailsPropertyPage2::~CDetailsPropertyPage2()
{
}

void CDetailsPropertyPage2::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDetailsPropertyPage2)
	DDX_Control(pDX, IDC_VERT_GUIDE, m_vert_enable);
	DDX_Control(pDX, IDC_HORIZ_GUIDE, m_horiz_enable);
	DDX_Check(pDX, IDC_SHOW_RULERS, m_bHasRulers);
	DDX_Text(pDX, IDC_HORIZ_GUIDE, m_horiz);
	DDX_Text(pDX, IDC_VERT_GUIDE, m_vert);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CDetailsPropertyPage2, CPropertyPage)
//{{AFX_MSG_MAP(CDetailsPropertyPage2)
	ON_BN_CLICKED(IDC_SHOW_RULERS, OnShowRulers)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CDetailsPropertyPage1::OnApply()
{
	UpdateData(TRUE);

	CDetails& current = m_pDesign->GetCurrentSheet()->GetDetails();
	current.SetVisible(m_bIsVisible == TRUE);
	current.SetTitle(m_sTitle);
	current.SetAuthor(m_sAuthor);
	current.SetRevision(m_sRevision);
	current.SetDocumentNumber(m_sDoc);
	current.SetOrganisation(m_sOrg);
	current.SetLastChange(m_sDate);
	// Note: m_sSheets is NOT written back — the title block's "N of M" is
	// computed automatically from the multi-doc sheet count.

	// Propagate the design-level fields to every other sheet so all sheets
	// share the same title-block content.
	int total = m_pDesign->GetNumberOfSheets();
	for (int i = 0; i < total; ++i)
	{
		CTinyCadDoc* pSheet = m_pDesign->GetSheet(i);
		if (pSheet != NULL && pSheet != m_pDesign->GetCurrentSheet())
		{
			pSheet->GetDetails().CopyDesignFields(current);
		}
	}

	// Force the modified flag on ::CDocument
	m_pDesign->GetCurrentSheet()->GetParent()->SetModifiedFlag();

	return CPropertyPage::OnApply();
}

BOOL CDetailsPropertyPage1::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	//	m_sFile			= m_pDesign->GetPathName();
	m_bIsVisible = m_pDesign->GetCurrentSheet()->GetDetails().IsVisible();
	m_sDate = m_pDesign->GetCurrentSheet()->GetDetails().GetLastChange();
	m_sTitle = m_pDesign->GetCurrentSheet()->GetDetails().GetTitle();
	m_sAuthor = m_pDesign->GetCurrentSheet()->GetDetails().GetAuthor();
	m_sRevision = m_pDesign->GetCurrentSheet()->GetDetails().GetRevision();
	m_sDoc = m_pDesign->GetCurrentSheet()->GetDetails().GetDocumentNumber();
	m_sOrg = m_pDesign->GetCurrentSheet()->GetDetails().GetOrganisation();

	// Sheets field is auto-computed "N of M" from the multi-doc sheet count.
	int total = m_pDesign->GetNumberOfSheets();
	int num = 0;
	for (int i = 0; i < total; ++i)
	{
		if (m_pDesign->GetSheet(i) == m_pDesign->GetCurrentSheet())
		{
			num = i + 1;
			break;
		}
	}
	if (num > 0 && total > 0)
	{
		m_sSheets.Format(_T("%d of %d"), num, total);
	}
	else
	{
		m_sSheets = m_pDesign->GetCurrentSheet()->GetDetails().GetSheets();
	}

	UpdateData(FALSE);

	// Sheets field is read-only — it's derived from the sheet count.
	CWnd* pSheetsCtrl = GetDlgItem(DESIGNBOX_SHEET);
	if (pSheetsCtrl != NULL)
	{
		pSheetsCtrl->EnableWindow(FALSE);
	}

	return TRUE; // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CDetailsPropertyPage2::OnApply()
{
	UpdateData(TRUE);

	m_vert.MakeUpper();

	int h = _tstoi(m_horiz);
	int v = m_vert.IsEmpty() ? -1 : m_vert[0] - 'A' + 1;

	m_pDesign->GetCurrentSheet()->GetDetails().SetRulers(m_bHasRulers == TRUE, v, h);

	return CPropertyPage::OnApply();
}

BOOL CDetailsPropertyPage2::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	m_bHasRulers = m_pDesign->GetCurrentSheet()->GetDetails().HasRulers();
	m_horiz.Format(_T("%d"), m_pDesign->GetCurrentSheet()->GetDetails().m_iHorizRulerSize);
	m_vert.Format(_T("%c"), 'A' + m_pDesign->GetCurrentSheet()->GetDetails().m_iVertRulerSize - 1);
	m_horiz_enable.EnableWindow(m_bHasRulers);
	m_vert_enable.EnableWindow(m_bHasRulers);

	UpdateData(FALSE);

	return TRUE; // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CDetailsPropertyPage2::OnShowRulers()
{
	UpdateData(TRUE);
	m_horiz_enable.EnableWindow(m_bHasRulers);
	m_vert_enable.EnableWindow(m_bHasRulers);
}

/////////////////////////////////////////////////////////////////////////////
// CDetailsPropertyPage3 - Variables

CDetailsPropertyPage3::CDetailsPropertyPage3(CMultiSheetDoc* pDesign) :
	CPropertyPage(CDetailsPropertyPage3::IDD)
{
	m_pDesign = pDesign;
	m_bDirty = false;
	if (m_pDesign != NULL)
	{
		m_oTokens = m_pDesign->GetCurrentSheet()->GetDetails().GetUserTokens();
	}
}

CDetailsPropertyPage3::~CDetailsPropertyPage3()
{
}

void CDetailsPropertyPage3::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TOKENS_LIST, m_wndList);
}

BEGIN_MESSAGE_MAP(CDetailsPropertyPage3, CPropertyPage)
	ON_BN_CLICKED(IDC_TOKEN_ADD, OnAdd)
	ON_BN_CLICKED(IDC_TOKEN_EDIT, OnEdit)
	ON_BN_CLICKED(IDC_TOKEN_REMOVE, OnRemove)
	ON_NOTIFY(NM_DBLCLK, IDC_TOKENS_LIST, OnDoubleClickList)
END_MESSAGE_MAP()

BOOL CDetailsPropertyPage3::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	m_wndList.SetExtendedStyle(m_wndList.GetExtendedStyle()
		| LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	CRect rcList;
	m_wndList.GetClientRect(&rcList);
	int nameCol = max(80, rcList.Width() / 3);
	m_wndList.InsertColumn(0, _T("Name"), LVCFMT_LEFT, nameCol);
	m_wndList.InsertColumn(1, _T("Value"), LVCFMT_LEFT, rcList.Width() - nameCol - 4);

	RebuildList();

	return TRUE;
}

void CDetailsPropertyPage3::RebuildList(int selectIndex)
{
	m_wndList.DeleteAllItems();

	int row = 0;
	for (CDetailsTokenMap::const_iterator it = m_oTokens.begin(); it != m_oTokens.end(); ++it, ++row)
	{
		m_wndList.InsertItem(row, it->first);
		m_wndList.SetItemText(row, 1, it->second);
	}

	if (selectIndex >= 0 && selectIndex < m_wndList.GetItemCount())
	{
		m_wndList.SetItemState(selectIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		m_wndList.EnsureVisible(selectIndex, FALSE);
	}
}

bool CDetailsPropertyPage3::GetSelectedToken(CString& sName) const
{
	POSITION pos = m_wndList.GetFirstSelectedItemPosition();
	if (pos == NULL)
	{
		return false;
	}
	int idx = m_wndList.GetNextSelectedItem(pos);
	sName = m_wndList.GetItemText(idx, 0);
	return !sName.IsEmpty();
}

void CDetailsPropertyPage3::OnAdd()
{
	CEditTokenDlg dlg(this);
	dlg.m_pExistingTokens = &m_oTokens;
	if (dlg.DoModal() == IDOK)
	{
		m_oTokens[dlg.m_sName] = dlg.m_sValue;
		int target = -1, row = 0;
		for (CDetailsTokenMap::const_iterator it = m_oTokens.begin(); it != m_oTokens.end(); ++it, ++row)
		{
			if (it->first.CompareNoCase(dlg.m_sName) == 0)
			{
				target = row;
				break;
			}
		}
		RebuildList(target);
		m_bDirty = true;
		SetModified(TRUE);
	}
}

void CDetailsPropertyPage3::OnEdit()
{
	CString sName;
	if (!GetSelectedToken(sName))
	{
		return;
	}

	CDetailsTokenMap::iterator it = m_oTokens.find(sName);
	if (it == m_oTokens.end())
	{
		return;
	}

	CEditTokenDlg dlg(this);
	dlg.m_sName = it->first;
	dlg.m_sValue = it->second;
	dlg.m_sOriginalName = it->first;
	dlg.m_pExistingTokens = &m_oTokens;

	if (dlg.DoModal() == IDOK)
	{
		// If renamed, drop the old key first.
		if (dlg.m_sName.CompareNoCase(dlg.m_sOriginalName) != 0)
		{
			m_oTokens.erase(it);
		}
		m_oTokens[dlg.m_sName] = dlg.m_sValue;

		int target = -1, row = 0;
		for (CDetailsTokenMap::const_iterator it2 = m_oTokens.begin(); it2 != m_oTokens.end(); ++it2, ++row)
		{
			if (it2->first.CompareNoCase(dlg.m_sName) == 0)
			{
				target = row;
				break;
			}
		}
		RebuildList(target);
		m_bDirty = true;
		SetModified(TRUE);
	}
}

void CDetailsPropertyPage3::OnRemove()
{
	CString sName;
	if (!GetSelectedToken(sName))
	{
		return;
	}

	CDetailsTokenMap::iterator it = m_oTokens.find(sName);
	if (it != m_oTokens.end())
	{
		int prior = -1, row = 0;
		for (CDetailsTokenMap::const_iterator it2 = m_oTokens.begin(); it2 != it; ++it2, ++row)
		{
			prior = row;
		}
		m_oTokens.erase(it);
		int sel = prior >= 0 ? prior : (m_oTokens.empty() ? -1 : 0);
		RebuildList(sel);
		m_bDirty = true;
		SetModified(TRUE);
	}
}

void CDetailsPropertyPage3::OnDoubleClickList(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
	OnEdit();
	*pResult = 0;
}

BOOL CDetailsPropertyPage3::OnApply()
{
	if (m_bDirty)
	{
		// Apply tokens to every sheet so all sheets share the same set.
		int total = m_pDesign->GetNumberOfSheets();
		for (int i = 0; i < total; ++i)
		{
			CTinyCadDoc* pSheet = m_pDesign->GetSheet(i);
			if (pSheet != NULL)
			{
				pSheet->GetDetails().SetUserTokens(m_oTokens);
			}
		}
		m_pDesign->GetCurrentSheet()->GetParent()->SetModifiedFlag();
	}
	return CPropertyPage::OnApply();
}

/////////////////////////////////////////////////////////////////////////////
// CEditTokenDlg

CEditTokenDlg::CEditTokenDlg(CWnd* pParent) :
	CDialog(CEditTokenDlg::IDD, pParent),
	m_pExistingTokens(NULL)
{
}

void CEditTokenDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_TOKEN_NAME, m_sName);
	DDX_Text(pDX, IDC_TOKEN_VALUE, m_sValue);
}

BEGIN_MESSAGE_MAP(CEditTokenDlg, CDialog)
END_MESSAGE_MAP()

void CEditTokenDlg::OnOK()
{
	if (!UpdateData(TRUE))
	{
		return;
	}

	m_sName.Trim();

	if (!IsValidTokenName(m_sName))
	{
		AfxMessageBox(_T("Variable name must start with a letter or underscore and contain only letters, digits and underscores."));
		return;
	}

	if (IsReservedTokenName(m_sName))
	{
		AfxMessageBox(_T("That name is reserved for a built-in title-block field. Pick another name."));
		return;
	}

	// Disallow duplicates (case-insensitive) except when editing the same key.
	if (m_pExistingTokens != NULL)
	{
		CDetailsTokenMap::const_iterator it = m_pExistingTokens->find(m_sName);
		if (it != m_pExistingTokens->end() && m_sName.CompareNoCase(m_sOriginalName) != 0)
		{
			AfxMessageBox(_T("A variable with that name already exists."));
			return;
		}
	}

	CDialog::OnOK();
}

/////////////////////////////////////////////////////////////////////////////
// CDetailsPropertyPage4 - SVG title-block picker

CDetailsPropertyPage4::CDetailsPropertyPage4(CMultiSheetDoc* pDesign)
	: CPropertyPage(CDetailsPropertyPage4::IDD)
{
	m_pDesign = pDesign;
	m_bDirty = false;
	if (m_pDesign != NULL)
	{
		m_sSvg = m_pDesign->GetCurrentSheet()->GetDetails().m_sTitleBlockSvg;
	}
}

CDetailsPropertyPage4::~CDetailsPropertyPage4()
{
}

void CDetailsPropertyPage4::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CDetailsPropertyPage4, CPropertyPage)
	ON_BN_CLICKED(IDC_TBSVG_BROWSE,      OnBrowse)
	ON_BN_CLICKED(IDC_TBSVG_USE_BUILTIN, OnUseBuiltin)
END_MESSAGE_MAP()

BOOL CDetailsPropertyPage4::OnInitDialog()
{
	CPropertyPage::OnInitDialog();
	UpdateStateLabel();
	return TRUE;
}

void CDetailsPropertyPage4::UpdateStateLabel()
{
	CString label;
	if (m_sSvg.IsEmpty())
	{
		label = _T("Current: built-in title block");
	}
	else
	{
		label.Format(_T("Current: custom SVG (%d characters)"), m_sSvg.GetLength());
	}
	CWnd* pState = GetDlgItem(IDC_TBSVG_STATE);
	if (pState != NULL) pState->SetWindowText(label);
}

void CDetailsPropertyPage4::OnBrowse()
{
	CFileDialog dlg(TRUE, _T("svg"), NULL,
		OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
		_T("SVG files (*.svg)|*.svg|All files (*.*)|*.*||"), this);
	if (dlg.DoModal() != IDOK) return;

	CFile file;
	if (!file.Open(dlg.GetPathName(), CFile::modeRead | CFile::shareDenyWrite))
	{
		AfxMessageBox(_T("Could not open SVG file."));
		return;
	}
	ULONGLONG len = file.GetLength();
	if (len == 0)
	{
		AfxMessageBox(_T("That SVG file is empty."));
		return;
	}
	if (len > 5 * 1024 * 1024)
	{
		AfxMessageBox(_T("SVG file is larger than 5 MB; refusing to embed."));
		return;
	}

	std::vector<char> buf((size_t)len + 1, 0);
	file.Read(buf.data(), (UINT)len);
	buf[(size_t)len] = '\0';

	// Treat the file as UTF-8 and convert to the CString native (UTF-16) format.
	CA2T converted(buf.data(), CP_UTF8);
	m_sSvg = (LPCTSTR)converted;

	m_bDirty = true;
	UpdateStateLabel();
	SetModified(TRUE);
}

void CDetailsPropertyPage4::OnUseBuiltin()
{
	if (!m_sSvg.IsEmpty())
	{
		m_sSvg.Empty();
		m_bDirty = true;
		UpdateStateLabel();
		SetModified(TRUE);
	}
}

BOOL CDetailsPropertyPage4::OnApply()
{
	if (m_bDirty && m_pDesign != NULL)
	{
		CDetails& current = m_pDesign->GetCurrentSheet()->GetDetails();
		current.m_sTitleBlockSvg = m_sSvg;

		// Title-block SVG is design-wide — propagate to every other sheet.
		int total = m_pDesign->GetNumberOfSheets();
		for (int i = 0; i < total; ++i)
		{
			CTinyCadDoc* pSheet = m_pDesign->GetSheet(i);
			if (pSheet != NULL && pSheet != m_pDesign->GetCurrentSheet())
			{
				pSheet->GetDetails().CopyDesignFields(current);
			}
		}

		m_pDesign->GetCurrentSheet()->GetParent()->SetModifiedFlag();
		m_bDirty = false;
	}
	return CPropertyPage::OnApply();
}
