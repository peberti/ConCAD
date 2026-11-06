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
#include "DetailsPropertySheet.h"
#include "ConCadDoc.h"
#include "Object.h"
#include "Log.h"
#include "ConCadRegistry.h"

IMPLEMENT_DYNCREATE(CDetailsPropertyPage1, CPropertyPage)
IMPLEMENT_DYNCREATE(CDetailsPropertyPage2, CPropertyPage)
IMPLEMENT_DYNCREATE(CDetailsPropertyPage3, CPropertyPage)
IMPLEMENT_DYNCREATE(CDetailsPropertyPage4, CPropertyPage)
IMPLEMENT_DYNCREATE(CDetailsPropertyPage5, CPropertyPage)

// Reserved token names (built-ins exposed by CDetails::Resolve)
static bool IsReservedTokenName(const CString& sName)
{
	return CDetails::IsBuiltInToken(sName);
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

// True if the vector already holds the name (case-insensitive).
static bool VecContainsNoCase(const std::vector<CString>& v, const CString& s)
{
	for (size_t i = 0; i < v.size(); ++i)
	{
		if (v[i].CompareNoCase(s) == 0)
		{
			return true;
		}
	}
	return false;
}

// Scan a string for {name} references and append valid, non-built-in, not-yet-
// seen names to both 'out' (the result, preserving discovery order) and
// 'pending' (a worklist whose stored values are scanned next, so a token whose
// value references another token keeps that one "used" too).
static void AppendReferencedNames(const CString& text, std::vector<CString>& out, std::vector<CString>& pending)
{
	int pos = 0;
	while ((pos = text.Find(_T('{'), pos)) >= 0)
	{
		int end = text.Find(_T('}'), pos + 1);
		if (end < 0)
		{
			break;
		}
		CString name = text.Mid(pos + 1, end - pos - 1);
		name.Trim();
		if (!IsValidTokenName(name))
		{
			// e.g. an unclosed "{Name" before a later "{Other}": rescan from
			// the next brace so "{Other}" is still found.
			pos = pos + 1;
			continue;
		}
		pos = end + 1;

		if (IsValidTokenName(name) && !IsReservedTokenName(name) && !VecContainsNoCase(out, name))
		{
			out.push_back(name);
			pending.push_back(name);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// CAnchorLayout

void CAnchorLayout::Init(CWnd* pParent)
{
	m_pParent = pParent;
	m_items.clear();
	CRect rc;
	pParent->GetClientRect(rc);
	m_szInit = rc.Size();
}

void CAnchorLayout::Add(UINT nID, int flags)
{
	CWnd* pWnd = (m_pParent != NULL) ? m_pParent->GetDlgItem(nID) : NULL;
	if (pWnd != NULL)
	{
		Add(pWnd->GetSafeHwnd(), flags);
	}
}

void CAnchorLayout::Add(HWND hWnd, int flags)
{
	if (m_pParent == NULL || hWnd == NULL)
	{
		return;
	}
	SItem item;
	item.hWnd = hWnd;
	item.flags = flags;
	::GetWindowRect(hWnd, &item.rc);
	m_pParent->ScreenToClient(&item.rc);
	m_items.push_back(item);
}

void CAnchorLayout::Apply()
{
	if (m_pParent == NULL || !::IsWindow(m_pParent->GetSafeHwnd()) || m_items.empty())
	{
		return;
	}
	CRect rcClient;
	m_pParent->GetClientRect(rcClient);
	const int dx = max(0, rcClient.Width() - m_szInit.cx);
	const int dy = max(0, rcClient.Height() - m_szInit.cy);

	HDWP hdwp = ::BeginDeferWindowPos(static_cast<int>(m_items.size()));
	for (size_t i = 0; i < m_items.size(); ++i)
	{
		CRect rc = m_items[i].rc;
		const int flags = m_items[i].flags;
		if (flags & MoveX) rc.OffsetRect(dx, 0);
		if (flags & MoveY) rc.OffsetRect(0, dy);
		if (flags & SizeX) rc.right += dx;
		if (flags & SizeY) rc.bottom += dy;
		if (hdwp != NULL)
		{
			hdwp = ::DeferWindowPos(hdwp, m_items[i].hWnd, NULL, rc.left, rc.top, rc.Width(), rc.Height(),
				SWP_NOZORDER | SWP_NOACTIVATE);
		}
	}
	if (hdwp != NULL)
	{
		::EndDeferWindowPos(hdwp);
	}
	m_pParent->Invalidate();
}

// A page that becomes active asks the sheet to size it to the tab area
static void ResizeToSheet(CPropertyPage* pPage)
{
	CWnd* pSheet = pPage->GetParent();
	if (pSheet != NULL)
	{
		pSheet->PostMessage(WM_DETAILS_RESIZEPAGE);
	}
}

// Design page: edit and "Keep" box of each CDetails::EKeepField
static const UINT KeepFieldEdit[CDetails::kfCount] =
{
	DESIGNBOX_TITLE, DESIGNBOX_AUTHOR, DESIGNBOX_ORGANISATION, IDC_DESIGNBOX_DESCRIPTION, DESIGNBOX_REVISION, DESIGNBOX_DOCUMENT
};
static const UINT KeepFieldBox[CDetails::kfCount] =
{
	IDC_KEEP_TITLE, IDC_KEEP_AUTHOR, IDC_KEEP_ORGANISATION, IDC_KEEP_DESCRIPTION, IDC_KEEP_REVISION, IDC_KEEP_DOCUMENT
};

/////////////////////////////////////////////////////////////////////////////
// CDetailsPropertyPage1 property page

CDetailsPropertyPage1::CDetailsPropertyPage1(CMultiSheetDoc* pDesign) :
	CPropertyPage(CDetailsPropertyPage1::IDD)
{
	//{{AFX_DATA_INIT(CDetailsPropertyPage1)
	m_sAuthor = _T("");
	m_sDate = _T("");
	m_sOrg = _T("");
	m_sDescription = _T("");
	m_sDoc = _T("");
	m_sRevision = _T("");
	m_sSheets = _T("");
	m_sTitle = _T("");
	m_bIsVisible = FALSE;
	//}}AFX_DATA_INIT

	m_pDesign = pDesign;
	m_scrollTop = 0;
	m_tokensDirty = false;
	m_visibleRows = 0;
	m_createdRows = 0;
	m_bLayoutReady = false;
	for (int f = 0; f < CDetails::kfCount; ++f)
	{
		m_bKeep[f] = FALSE;
		m_bKeepInitial[f] = false;
	}
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
	DDX_Text(pDX, IDC_DESIGNBOX_DESCRIPTION, m_sDescription);
	DDX_Text(pDX, DESIGNBOX_DOCUMENT, m_sDoc);
	DDX_Text(pDX, DESIGNBOX_REVISION, m_sRevision);
	DDX_Text(pDX, DESIGNBOX_SHEET, m_sSheets);
	DDX_Text(pDX, DESIGNBOX_TITLE, m_sTitle);
	DDX_Check(pDX, DESIGNBOX_DISPLAY, m_bIsVisible);
	//}}AFX_DATA_MAP
	DDX_Control(pDX, IDC_TOK_SCROLL, m_tokenScroll);
	for (int f = 0; f < CDetails::kfCount; ++f)
	{
		DDX_Check(pDX, KeepFieldBox[f], m_bKeep[f]);
	}
}

BEGIN_MESSAGE_MAP(CDetailsPropertyPage1, CPropertyPage)
//{{AFX_MSG_MAP(CDetailsPropertyPage1)
//}}AFX_MSG_MAP
	ON_WM_VSCROLL()
	ON_WM_SIZE()
	ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()

CString CDetailsPropertyPage1::GetFieldValue(int f) const
{
	switch (f)
	{
		case CDetails::kfTitle:        return m_sTitle;
		case CDetails::kfAuthor:       return m_sAuthor;
		case CDetails::kfOrganisation: return m_sOrg;
		case CDetails::kfDescription:  return m_sDescription;
		case CDetails::kfRevision:     return m_sRevision;
		case CDetails::kfDocument:     return m_sDoc;
		default:                       return CString();
	}
}

BOOL CDetailsPropertyPage1::OnSetActive()
{
	ResizeToSheet(this);
	return CPropertyPage::OnSetActive();
}

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

	// Read the visible token rows and build the validated token map first, so a
	// bad name aborts the Apply before any field is written.
	CommitVisibleRows();
	CDetailsTokenMap newTokens;
	for (size_t r = 0; r < m_tokenRows.size(); ++r)
	{
		CString name = m_tokenRows[r].name;
		name.Trim();
		if (name.IsEmpty())
		{
			continue;   // blank row — silently dropped
		}
		if (!IsValidTokenName(name))
		{
			CString msg;
			msg.Format(_T("\"%s\" is not a valid variable name.\n\nUse letters, digits and underscores only; it cannot start with a digit."), (LPCTSTR)name);
			AfxMessageBox(msg, MB_ICONEXCLAMATION);
			return FALSE;
		}
		if (IsReservedTokenName(name))
		{
			CString msg;
			msg.Format(_T("\"%s\" is a built-in name and cannot be redefined as a variable."), (LPCTSTR)name);
			AfxMessageBox(msg, MB_ICONEXCLAMATION);
			return FALSE;
		}
		newTokens[name] = m_tokenRows[r].value;   // duplicate names collapse (last wins)
	}

	// Keep: ticked values are what new designs start with.  A value is
	// stored only when it was edited or newly ticked, so pressing OK in
	// someone else's design does not take over their values.
	for (int f = 0; f < CDetails::kfCount; ++f)
	{
		const CString value = GetFieldValue(f);
		const bool keep = (m_bKeep[f] != FALSE);
		if (keep && (!m_bKeepInitial[f] || value != m_sInitial[f]))
		{
			CDetails::SetKeptValue(f, value);
		}
		if (keep != m_bKeepInitial[f])
		{
			CDetails::SetFieldKept(f, keep);
		}
		m_bKeepInitial[f] = keep;
		m_sInitial[f] = value;
	}
	CDetailsTokenMap kept;
	CDetails::GetKeptTokens(kept);
	bool bKeptChanged = false;
	for (size_t r = 0; r < m_tokenRows.size(); ++r)
	{
		STokenRow& row = m_tokenRows[r];
		CString name = row.name;
		name.Trim();
		if (name.IsEmpty())
		{
			continue;
		}
		if (row.keep && (!row.keepInitial || row.value != row.initialValue))
		{
			kept[name] = row.value;
			bKeptChanged = true;
		}
		else if (!row.keep && row.keepInitial)
		{
			kept.erase(name);
			bKeptChanged = true;
		}
		row.keepInitial = row.keep;
		row.initialValue = row.value;
	}
	if (bKeptChanged)
	{
		CDetails::SetKeptTokens(kept);
	}

	CDetails& current = m_pDesign->GetCurrentSheet()->GetDetails();
	current.SetVisible(m_bIsVisible == TRUE);
	current.SetTitle(m_sTitle);
	current.SetAuthor(m_sAuthor);
	current.SetRevision(m_sRevision);
	current.SetDocumentNumber(m_sDoc);
	current.SetOrganisation(m_sOrg);
	current.SetDescription(m_sDescription);
	current.SetUserTokens(newTokens);
	// Note: Date is NOT written back — it is set only by File → Create version.
	// Note: m_sSheets is NOT written back — the title block's "N of M" is
	// computed automatically from the multi-doc sheet count.

	// Propagate the design-level fields (incl. tokens, via CopyDesignFields) to
	// every other sheet so all sheets share the same title-block content.
	int total = m_pDesign->GetNumberOfSheets();
	for (int i = 0; i < total; ++i)
	{
		CConCadDoc* pSheet = m_pDesign->GetSheet(i);
		if (pSheet != NULL && pSheet != m_pDesign->GetCurrentSheet())
		{
			pSheet->GetDetails().CopyDesignFields(current);
		}
	}

	// Force the modified flag on ::CDocument
	m_pDesign->GetCurrentSheet()->GetParent()->SetModifiedFlag();

	return CPropertyPage::OnApply();
}

//-------------------------------------------------------------------------
// Embedded variables (token) editor
//-------------------------------------------------------------------------

void CDetailsPropertyPage1::CollectReferencedTokenNames(std::vector<CString>& out)
{
	out.clear();
	if (m_pDesign == NULL)
	{
		return;
	}

	const CDetails& d = m_pDesign->GetCurrentSheet()->GetDetails();
	const CDetailsTokenMap& stored = d.GetUserTokens();

	// Build a blob of everywhere a {token} actually gets substituted: the design
	// fields, the SVG title block, and the free-text / note objects on every
	// sheet.  (Labels and pin refs are excluded — tokens are not substituted
	// there.)
	CString blob;
	blob += d.GetTitle();          blob += _T('\n');
	blob += d.GetAuthor();         blob += _T('\n');
	blob += d.GetRevision();       blob += _T('\n');
	blob += d.GetDocumentNumber(); blob += _T('\n');
	blob += d.GetOrganisation();   blob += _T('\n');
	blob += d.GetDescription();    blob += _T('\n');
	blob += d.GetTitleBlockSvg();

	const int total = m_pDesign->GetNumberOfSheets();
	for (int s = 0; s < total; ++s)
	{
		CConCadDoc* pSheet = m_pDesign->GetSheet(s);
		if (pSheet == NULL)
		{
			continue;
		}
		for (drawingIterator dit = pSheet->GetDrawingBegin(); dit != pSheet->GetDrawingEnd(); ++dit)
		{
			CDrawingObject* pObj = *dit;
			if (pObj == NULL)
			{
				continue;
			}
			switch (pObj->GetType())
			{
				case xText:
				case xTextEx:
				case xTextEx2:
					blob += _T('\n');
					blob += pSheet->ResolveModuleTokens(static_cast<CDrawText*>(pObj)->GetValue(), pObj->m_group); // a module's parameters are not design variables
					break;
				case xNoteText:
					blob += _T('\n');
					blob += pSheet->ResolveModuleTokens(static_cast<CDrawNoteText*>(pObj)->GetValue(), pObj->m_group);
					break;
				default:
					break;
			}
		}
	}

	// Names referenced directly in the content, then transitively through the
	// stored values of those tokens (so {A} with value "{B}" keeps B used too).
	std::vector<CString> pending;
	AppendReferencedNames(blob, out, pending);
	while (!pending.empty())
	{
		CString name = pending.back();
		pending.pop_back();
		CDetailsTokenMap::const_iterator it = stored.find(name);
		if (it != stored.end())
		{
			AppendReferencedNames(it->second, out, pending);
		}
	}
}

void CDetailsPropertyPage1::LoadTokens()
{
	m_tokenRows.clear();
	m_scrollTop = 0;
	if (m_pDesign == NULL)
	{
		return;
	}

	// Only list tokens actually referenced in the design; pull each one's saved
	// value from storage if it has one.  Stored-but-unreferenced tokens are
	// dropped (and pruned from storage on the next OK).
	const CDetailsTokenMap& stored = m_pDesign->GetCurrentSheet()->GetDetails().GetUserTokens();

	std::vector<CString> referenced;
	CollectReferencedTokenNames(referenced);

	CDetailsTokenMap kept;
	CDetails::GetKeptTokens(kept);

	for (size_t i = 0; i < referenced.size(); ++i)
	{
		STokenRow row;
		row.name = referenced[i];
		CDetailsTokenMap::const_iterator it = stored.find(referenced[i]);
		row.value = (it != stored.end()) ? it->second : CString();
		row.keep = (kept.find(referenced[i]) != kept.end());
		row.keepInitial = row.keep;
		row.initialValue = row.value;
		m_tokenRows.push_back(row);
	}
}

void CDetailsPropertyPage1::RefreshTokenView()
{
	const int count = (int)m_tokenRows.size();
	for (int i = 0; i < m_visibleRows; ++i)
	{
		int idx = m_scrollTop + i;
		CWnd* pName = GetDlgItem(IDC_TOK_NAME_BASE + i);
		CWnd* pVal  = GetDlgItem(IDC_TOK_VAL_BASE + i);
		CWnd* pKeep = GetDlgItem(IDC_TOK_KEEP_BASE + i);
		if (pName == NULL || pVal == NULL || pKeep == NULL)
		{
			continue;
		}
		const bool bUsed = (idx < count);
		pName->SetWindowText(bUsed ? m_tokenRows[idx].name : CString());
		pVal->SetWindowText(bUsed ? m_tokenRows[idx].value : CString());
		CheckDlgButton(IDC_TOK_KEEP_BASE + i, (bUsed && m_tokenRows[idx].keep) ? BST_CHECKED : BST_UNCHECKED);
		pName->EnableWindow(bUsed);
		pVal->EnableWindow(bUsed);
		pKeep->EnableWindow(bUsed);
		pKeep->ShowWindow(bUsed ? SW_SHOW : SW_HIDE);
	}
	UpdateTokenScrollBar();
}

void CDetailsPropertyPage1::CommitVisibleRows()
{
	const int count = (int)m_tokenRows.size();
	for (int i = 0; i < m_visibleRows; ++i)
	{
		int idx = m_scrollTop + i;
		if (idx >= count)
		{
			break;
		}
		CWnd* pName = GetDlgItem(IDC_TOK_NAME_BASE + i);
		CWnd* pVal  = GetDlgItem(IDC_TOK_VAL_BASE + i);
		if (pName == NULL || pVal == NULL)
		{
			continue;
		}
		CString n, v;
		pName->GetWindowText(n);
		pVal->GetWindowText(v);
		n.Trim();
		m_tokenRows[idx].name = n;
		m_tokenRows[idx].value = v;
		m_tokenRows[idx].keep = (IsDlgButtonChecked(IDC_TOK_KEEP_BASE + i) == BST_CHECKED);
	}
}

void CDetailsPropertyPage1::UpdateTokenScrollBar()
{
	if (m_tokenScroll.GetSafeHwnd() == NULL)
	{
		return;
	}
	const int count = (int)m_tokenRows.size();
	SCROLLINFO si;
	ZeroMemory(&si, sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS;
	si.nMin   = 0;
	si.nMax   = (count > 0) ? count - 1 : 0;
	si.nPage  = m_visibleRows;
	si.nPos   = m_scrollTop;
	m_tokenScroll.SetScrollInfo(&si, TRUE);
	m_tokenScroll.EnableWindow(count > m_visibleRows);
}

void CDetailsPropertyPage1::ScrollTokensTo(int top)
{
	CommitVisibleRows();
	const int count  = (int)m_tokenRows.size();
	const int maxTop = (count > m_visibleRows) ? (count - m_visibleRows) : 0;
	if (top > maxTop) top = maxTop;
	if (top < 0)      top = 0;
	m_scrollTop = top;
	RefreshTokenView();
}

int CDetailsPropertyPage1::TokenRowFromWnd(HWND hWnd) const
{
	if (hWnd == NULL || ::GetParent(hWnd) != m_hWnd)
	{
		return -1;
	}
	const int row = ::GetDlgCtrlID(hWnd) - IDC_TOK_VAL_BASE;
	return (row >= 0 && row < m_visibleRows) ? row : -1;
}

// Fit as many variable rows as the token area (the scrollbar's height) holds,
// creating row controls on demand, and place them.
void CDetailsPropertyPage1::LayoutTokenRows()
{
	CWnd* pKeepTitle = GetDlgItem(IDC_KEEP_TITLE);
	if (m_tokenScroll.GetSafeHwnd() == NULL || pKeepTitle == NULL)
	{
		return;
	}
	CRect rcArea, rcKeep;
	m_tokenScroll.GetWindowRect(rcArea);
	ScreenToClient(rcArea);
	pKeepTitle->GetWindowRect(rcKeep);
	ScreenToClient(rcKeep);

	CRect m1(8, 16, 73, 12);      // name x, row pitch, value x, edit height (DLUs)
	MapDialogRect(m1);
	CRect m2(4, 2, 62, 10);       // gap, label offset, name width, label height
	MapDialogRect(m2);
	const int nameX = m1.left, pitch = m1.top, valX = m1.right, editH = m1.bottom;
	const int gap = m2.left, labelDY = m2.top, nameW = m2.right, labelH = m2.bottom;

	int rows = (rcArea.Height() + (pitch - editH)) / pitch;
	if (rows < 1)        rows = 1;
	if (rows > kMaxRows) rows = kMaxRows;

	CommitVisibleRows();

	HFONT hFont = (HFONT)GetFont()->GetSafeHandle();
	HINSTANCE hInst = AfxGetInstanceHandle();
	while (m_createdRows < rows)
	{
		const int i = m_createdRows;
		HWND hName = ::CreateWindowEx(0, _T("STATIC"), _T(""), WS_CHILD | SS_LEFT | SS_NOPREFIX | SS_ENDELLIPSIS,
			0, 0, 0, 0, m_hWnd, (HMENU)(UINT_PTR)(IDC_TOK_NAME_BASE + i), hInst, NULL);
		HWND hVal = ::CreateWindowEx(WS_EX_CLIENTEDGE, _T("EDIT"), _T(""), WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
			0, 0, 0, 0, m_hWnd, (HMENU)(UINT_PTR)(IDC_TOK_VAL_BASE + i), hInst, NULL);
		HWND hKeep = ::CreateWindowEx(0, _T("BUTTON"), _T(""), WS_CHILD | BS_AUTOCHECKBOX,
			0, 0, 0, 0, m_hWnd, (HMENU)(UINT_PTR)(IDC_TOK_KEEP_BASE + i), hInst, NULL);
		HWND created[] = { hName, hVal, hKeep };
		for (int c = 0; c < 3; ++c)
		{
			if (created[c] != NULL)
			{
				::SendMessage(created[c], WM_SETFONT, (WPARAM)hFont, FALSE);
				// Tab order = z-order: after all template controls, row by row
				::SetWindowPos(created[c], HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
			}
		}
		++m_createdRows;
	}

	const int valW = max(20, rcKeep.left - gap - valX);
	for (int i = 0; i < m_createdRows; ++i)
	{
		CWnd* pName = GetDlgItem(IDC_TOK_NAME_BASE + i);
		CWnd* pVal  = GetDlgItem(IDC_TOK_VAL_BASE + i);
		CWnd* pKeep = GetDlgItem(IDC_TOK_KEEP_BASE + i);
		if (pName == NULL || pVal == NULL || pKeep == NULL)
		{
			continue;
		}
		if (i < rows)
		{
			const int y = rcArea.top + i * pitch;
			pName->SetWindowPos(NULL, nameX, y + labelDY, nameW, labelH, SWP_NOZORDER | SWP_NOACTIVATE);
			pVal->SetWindowPos(NULL, valX, y, valW, editH, SWP_NOZORDER | SWP_NOACTIVATE);
			pKeep->SetWindowPos(NULL, rcKeep.left, y + (editH - rcKeep.Height()) / 2, rcKeep.Width(), rcKeep.Height(),
				SWP_NOZORDER | SWP_NOACTIVATE);
			pName->ShowWindow(SW_SHOW);
			pVal->ShowWindow(SW_SHOW);
		}
		else
		{
			pName->ShowWindow(SW_HIDE);
			pVal->ShowWindow(SW_HIDE);
			pKeep->ShowWindow(SW_HIDE);
		}
	}
	m_visibleRows = rows;

	const int count  = (int)m_tokenRows.size();
	const int maxTop = (count > m_visibleRows) ? (count - m_visibleRows) : 0;
	if (m_scrollTop > maxTop)
	{
		m_scrollTop = maxTop;
	}
	RefreshTokenView();   // also shows the Keep boxes of the rows in use
}

void CDetailsPropertyPage1::OnSize(UINT nType, int cx, int cy)
{
	CPropertyPage::OnSize(nType, cx, cy);
	if (m_bLayoutReady)
	{
		m_layout.Apply();
		LayoutTokenRows();
	}
}

BOOL CDetailsPropertyPage1::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	if ((int)m_tokenRows.size() > m_visibleRows)
	{
		ScrollTokensTo(m_scrollTop - zDelta / WHEEL_DELTA);
		return TRUE;
	}
	return CPropertyPage::OnMouseWheel(nFlags, zDelta, pt);
}

// Tab from the last visible variable scrolls the next one into view (and
// Shift+Tab from the first scrolls back), so Tab walks every variable.
BOOL CDetailsPropertyPage1::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_TAB
		&& (GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_MENU) & 0x8000) == 0)
	{
		const int row = TokenRowFromWnd(pMsg->hwnd);
		if (row >= 0)
		{
			const bool bBack = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
			const int count = (int)m_tokenRows.size();
			if (!bBack && row == m_visibleRows - 1 && m_scrollTop + row + 1 < count)
			{
				ScrollTokensTo(m_scrollTop + 1);
				GotoDlgCtrl(GetDlgItem(IDC_TOK_VAL_BASE + row));
				return TRUE;
			}
			if (bBack && row == 0 && m_scrollTop > 0)
			{
				ScrollTokensTo(m_scrollTop - 1);
				GotoDlgCtrl(GetDlgItem(IDC_TOK_VAL_BASE));
				return TRUE;
			}
		}
	}
	return CPropertyPage::PreTranslateMessage(pMsg);
}

void CDetailsPropertyPage1::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	if (pScrollBar != NULL && pScrollBar->GetSafeHwnd() == m_tokenScroll.GetSafeHwnd())
	{
		int top = m_scrollTop;
		switch (nSBCode)
		{
			case SB_LINEUP:        top -= 1;             break;
			case SB_LINEDOWN:      top += 1;             break;
			case SB_PAGEUP:        top -= m_visibleRows; break;
			case SB_PAGEDOWN:      top += m_visibleRows; break;
			case SB_THUMBTRACK:
			case SB_THUMBPOSITION: top = (int)nPos;      break;
			case SB_TOP:           top = 0;              break;
			case SB_BOTTOM:        top = (int)m_tokenRows.size(); break;
			default: break;
		}
		ScrollTokensTo(top);
		return;
	}
	CPropertyPage::OnVScroll(nSBCode, nPos, pScrollBar);
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
	m_sDescription = m_pDesign->GetCurrentSheet()->GetDetails().GetDescription();

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

	// Date field is read-only — it is set only by File → Create version.
	CWnd* pDateCtrl = GetDlgItem(DESIGNBOX_DATE);
	if (pDateCtrl != NULL)
	{
		pDateCtrl->EnableWindow(FALSE);
	}

	// Keep boxes of the fixed fields: ticked as stored in the registry
	for (int f = 0; f < CDetails::kfCount; ++f)
	{
		m_bKeepInitial[f] = CDetails::IsFieldKept(f);
		m_bKeep[f] = m_bKeepInitial[f] ? TRUE : FALSE;
		m_sInitial[f] = GetFieldValue(f);
		CheckDlgButton(KeepFieldBox[f], m_bKeep[f] ? BST_CHECKED : BST_UNCHECKED);
	}

	// Resizing: the text fields grow, the Keep column and the variables'
	// scrollbar move with the right edge; the variable rows fill the height.
	m_layout.Init(this);
	for (int f = 0; f < CDetails::kfCount; ++f)
	{
		if (f != CDetails::kfRevision)
		{
			m_layout.Add(KeepFieldEdit[f], CAnchorLayout::SizeX);
		}
		m_layout.Add(KeepFieldBox[f], CAnchorLayout::MoveX);
	}
	m_layout.Add(IDC_KEEP_HEADER, CAnchorLayout::MoveX);
	m_layout.Add(IDC_TOK_SCROLL, CAnchorLayout::MoveX | CAnchorLayout::SizeY);

	// Populate the embedded variables (token) editor.
	LoadTokens();
	LayoutTokenRows();
	m_bLayoutReady = true;

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

BOOL CDetailsPropertyPage2::OnSetActive()
{
	ResizeToSheet(this);
	return CPropertyPage::OnSetActive();
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

	MergeReferencedTokens();
	RebuildList();

	return TRUE;
}

void CDetailsPropertyPage3::MergeReferencedTokens()
{
	if (m_pDesign == NULL)
	{
		return;
	}

	const CDetails& d = m_pDesign->GetCurrentSheet()->GetDetails();

	// Gather every place a {token} can be referenced: the design fields, the
	// SVG title-block template, the values of already-defined tokens (a token
	// value may itself reference another), and the free-text / note objects on
	// every sheet (the only drawing objects whose text is token-substituted).
	CString blob;
	blob += d.GetTitle();          blob += _T('\n');
	blob += d.GetAuthor();         blob += _T('\n');
	blob += d.GetRevision();       blob += _T('\n');
	blob += d.GetDocumentNumber(); blob += _T('\n');
	blob += d.GetOrganisation();   blob += _T('\n');
	blob += d.GetDescription();    blob += _T('\n');
	blob += d.GetTitleBlockSvg();
	for (CDetailsTokenMap::const_iterator it = m_oTokens.begin(); it != m_oTokens.end(); ++it)
	{
		blob += _T('\n');
		blob += it->second;
	}

	const int total = m_pDesign->GetNumberOfSheets();
	for (int s = 0; s < total; ++s)
	{
		CConCadDoc* pSheet = m_pDesign->GetSheet(s);
		if (pSheet == NULL)
		{
			continue;
		}
		for (drawingIterator dit = pSheet->GetDrawingBegin(); dit != pSheet->GetDrawingEnd(); ++dit)
		{
			CDrawingObject* pObj = *dit;
			if (pObj == NULL)
			{
				continue;
			}
			switch (pObj->GetType())
			{
				case xText:
				case xTextEx:
				case xTextEx2:
					blob += _T('\n');
					blob += pSheet->ResolveModuleTokens(static_cast<CDrawText*>(pObj)->GetValue(), pObj->m_group); // a module's parameters are not design variables
					break;
				case xNoteText:
					blob += _T('\n');
					blob += pSheet->ResolveModuleTokens(static_cast<CDrawNoteText*>(pObj)->GetValue(), pObj->m_group);
					break;
				default:
					break;
			}
		}
	}

	int pos = 0;
	while ((pos = blob.Find(_T('{'), pos)) >= 0)
	{
		int end = blob.Find(_T('}'), pos + 1);
		if (end < 0)
		{
			break;
		}
		CString name = blob.Mid(pos + 1, end - pos - 1);
		name.Trim();
		if (!IsValidTokenName(name))
		{
			pos = pos + 1;   // unclosed "{Name": rescan from the next brace
			continue;
		}
		pos = end + 1;

		if (IsValidTokenName(name)
			&& !IsReservedTokenName(name)
			&& m_oTokens.find(name) == m_oTokens.end())
		{
			m_oTokens[name] = _T("");
			m_bDirty = true;
		}
	}
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
			CConCadDoc* pSheet = m_pDesign->GetSheet(i);
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
		const CDetails& d = m_pDesign->GetCurrentSheet()->GetDetails();
		m_sSvg  = d.m_sTitleBlockSvg;
		m_sName = d.m_sTitleBlockName;
	}
}

CDetailsPropertyPage4::~CDetailsPropertyPage4()
{
}

void CDetailsPropertyPage4::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TBSVG_LIST, m_wndList);
}

BEGIN_MESSAGE_MAP(CDetailsPropertyPage4, CPropertyPage)
	ON_BN_CLICKED(IDC_TBSVG_IMPORT,      OnImport)
	ON_BN_CLICKED(IDC_TBSVG_USE_BUILTIN, OnUseBuiltin)
	ON_LBN_SELCHANGE(IDC_TBSVG_LIST,     OnListSelChange)
	ON_WM_SIZE()
END_MESSAGE_MAP()

BOOL CDetailsPropertyPage4::OnSetActive()
{
	ResizeToSheet(this);
	return CPropertyPage::OnSetActive();
}

void CDetailsPropertyPage4::OnSize(UINT nType, int cx, int cy)
{
	CPropertyPage::OnSize(nType, cx, cy);
	m_layout.Apply();
}

BOOL CDetailsPropertyPage4::OnInitDialog()
{
	CPropertyPage::OnInitDialog();
	PopulateList();

	// Preselect the currently-referenced template, if it is still installed.
	if (!m_sName.IsEmpty())
	{
		for (size_t i = 0; i < m_templates.size(); ++i)
		{
			if (m_templates[i].name.CompareNoCase(m_sName) == 0)
			{
				m_wndList.SetCurSel((int)i);
				break;
			}
		}
	}

	UpdateStateLabel();

	m_layout.Init(this);
	m_layout.Add(IDC_TBSVG_HELP, CAnchorLayout::SizeX);
	m_layout.Add(IDC_TBSVG_LIST, CAnchorLayout::SizeX | CAnchorLayout::SizeY);
	m_layout.Add(IDC_TBSVG_IMPORT, CAnchorLayout::MoveX);
	m_layout.Add(IDC_TBSVG_USE_BUILTIN, CAnchorLayout::MoveX);
	m_layout.Add(IDC_TBSVG_STATE, CAnchorLayout::MoveY | CAnchorLayout::SizeX);
	return TRUE;
}

void CDetailsPropertyPage4::PopulateList()
{
	m_templates = CTitleBlockTemplateStore::Enumerate();
	m_wndList.ResetContent();
	for (size_t i = 0; i < m_templates.size(); ++i)
	{
		m_wndList.AddString(m_templates[i].displayName);
	}
}

void CDetailsPropertyPage4::OnListSelChange()
{
	const int sel = m_wndList.GetCurSel();
	if (sel < 0 || sel >= (int)m_templates.size()) return;

	CString svg;
	if (!CTitleBlockTemplateStore::ReadFile(m_templates[sel].fullPath, svg))
	{
		AfxMessageBox(_T("Could not read template SVG file."));
		return;
	}
	m_sSvg    = svg;
	m_sName   = m_templates[sel].name;
	m_bDirty  = true;
	UpdateStateLabel();
	SetModified(TRUE);
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

void CDetailsPropertyPage4::OnImport()
{
	CFileDialog dlg(TRUE, _T("svg"), NULL,
		OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
		_T("SVG files (*.svg)|*.svg|All files (*.*)|*.*||"), this);
	if (dlg.DoModal() != IDOK) return;

	CString svg;
	if (!CTitleBlockTemplateStore::ReadFile(dlg.GetPathName(), svg))
	{
		AfxMessageBox(_T("Could not read SVG file (missing, empty, or larger than 5 MB)."));
		return;
	}

	CString converted, report;
	if (!CTitleBlockTemplateStore::ConvertImportedSvg(svg, converted, report))
	{
		AfxMessageBox(_T("Could not import the SVG file:\n\n") + report, MB_ICONEXCLAMATION);
		return;
	}

	// Save it as a user template named after the file.  A bundled template
	// of the same name would win the by-name lookup, so pick a free name;
	// an existing user template may be replaced.
	const CString stem = dlg.GetFileTitle();
	const CString userFolder = CTitleBlockTemplateStore::GetUserFolder();
	const std::vector<STitleBlockTemplate> all = CTitleBlockTemplateStore::Enumerate();
	CString name;
	for (int n = 1; name.IsEmpty(); ++n)
	{
		CString candidate = stem;
		if (n > 1) candidate.Format(_T("%s-%d"), (LPCTSTR)stem, n);
		bool bundled = false, user = false;
		for (size_t i = 0; i < all.size(); ++i)
		{
			if (all[i].name.CompareNoCase(candidate) != 0) continue;
			if (all[i].fullPath.Left(userFolder.GetLength()).CompareNoCase(userFolder) == 0) user = true;
			else bundled = true;
		}
		if (bundled) continue;
		if (user)
		{
			const int answer = AfxMessageBox(_T("A user title block named \"") + candidate +
				_T("\" already exists. Replace it?\n\nNo keeps it and imports under a new name."),
				MB_YESNOCANCEL | MB_ICONQUESTION);
			if (answer == IDCANCEL) return;
			if (answer == IDNO) continue;
		}
		name = candidate;
	}

	CString path;
	if (!CTitleBlockTemplateStore::SaveUserTemplate(name, converted, path))
	{
		AfxMessageBox(_T("Could not save the title block to\n") + path, MB_ICONEXCLAMATION);
		return;
	}
	CLog::Write(_T("Title block imported: %s -> %s"), (LPCTSTR)dlg.GetPathName(), (LPCTSTR)path);

	PopulateList();
	m_wndList.SetCurSel(-1);
	for (size_t i = 0; i < m_templates.size(); ++i)
	{
		if (m_templates[i].fullPath.CompareNoCase(path) == 0) m_wndList.SetCurSel((int)i);
	}
	m_sSvg   = converted;
	m_sName  = name;
	m_bDirty = true;
	UpdateStateLabel();
	SetModified(TRUE);

	AfxMessageBox(_T("Imported as title block \"") + name + _T(" (user)\".\n\n") + report, MB_ICONINFORMATION);
}

void CDetailsPropertyPage4::OnUseBuiltin()
{
	if (!m_sSvg.IsEmpty() || !m_sName.IsEmpty())
	{
		m_sSvg.Empty();
		m_sName.Empty();
		m_bDirty = true;
		m_wndList.SetCurSel(-1);
		UpdateStateLabel();
		SetModified(TRUE);
	}
}

BOOL CDetailsPropertyPage4::OnApply()
{
	if (m_bDirty && m_pDesign != NULL)
	{
		CDetails& current = m_pDesign->GetCurrentSheet()->GetDetails();
		current.m_sTitleBlockName = m_sName;
		current.m_sTitleBlockSvg  = m_sSvg;
		current.ResolveTitleBlock();

		// Title-block SVG is design-wide — propagate to every other sheet.
		int total = m_pDesign->GetNumberOfSheets();
		for (int i = 0; i < total; ++i)
		{
			CConCadDoc* pSheet = m_pDesign->GetSheet(i);
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

/////////////////////////////////////////////////////////////////////////////
// CPickTitleTemplateDlg

CPickTitleTemplateDlg::CPickTitleTemplateDlg(CWnd* pParent) :
	CDialog(CPickTitleTemplateDlg::IDD, pParent),
	m_bDontAsk(FALSE)
{
}

void CPickTitleTemplateDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TBPICK_LIST, m_wndList);
	DDX_Check(pDX, IDC_TBPICK_DONTASK, m_bDontAsk);
}

BEGIN_MESSAGE_MAP(CPickTitleTemplateDlg, CDialog)
	ON_LBN_DBLCLK(IDC_TBPICK_LIST, OnListDblClk)
END_MESSAGE_MAP()

BOOL CPickTitleTemplateDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_templates = CTitleBlockTemplateStore::Enumerate();
	m_wndList.AddString(_T("(Built-in title block)"));
	int sel = 0;
	for (size_t i = 0; i < m_templates.size(); ++i)
	{
		m_wndList.AddString(m_templates[i].displayName);
		if (!m_sName.IsEmpty() && m_templates[i].name.CompareNoCase(m_sName) == 0)
		{
			sel = (int)i + 1;
		}
	}
	m_wndList.SetCurSel(sel);
	m_wndList.SetFocus();

	return FALSE;   // focus set to the list
}

void CPickTitleTemplateDlg::OnOK()
{
	if (!UpdateData(TRUE))
	{
		return;
	}

	const int sel = m_wndList.GetCurSel();
	if (sel >= 1 && sel <= (int)m_templates.size())
	{
		m_sName = m_templates[sel - 1].name;
	}
	else
	{
		m_sName.Empty();
	}

	CDialog::OnOK();
}

void CPickTitleTemplateDlg::OnListDblClk()
{
	OnOK();
}

/////////////////////////////////////////////////////////////////////////////
// CDetailsPropertyPage5 - Revisions: edit or delete revision-history entries

// A change description on one list row: line breaks shown as " / ".
static CString OneLine(const CString& s)
{
	CString r = s;
	r.Replace(_T("\r\n"), _T("\n"));
	r.Replace(_T("\n"), _T(" / "));
	return r;
}

CDetailsPropertyPage5::CDetailsPropertyPage5(CMultiSheetDoc* pDesign) :
	CPropertyPage(CDetailsPropertyPage5::IDD)
{
	m_pDesign = pDesign;
	m_bDirty = false;
	if (m_pDesign != NULL)
	{
		m_oHistory = m_pDesign->GetCurrentSheet()->GetDetails().GetRevisionHistory();
	}
}

CDetailsPropertyPage5::~CDetailsPropertyPage5()
{
}

void CDetailsPropertyPage5::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_REV_LIST, m_wndList);
}

BEGIN_MESSAGE_MAP(CDetailsPropertyPage5, CPropertyPage)
	ON_BN_CLICKED(IDC_REV_EDIT, OnEdit)
	ON_BN_CLICKED(IDC_REV_DELETE, OnDelete)
	ON_NOTIFY(NM_DBLCLK, IDC_REV_LIST, OnDoubleClickList)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_REV_LIST, OnItemChanged)
	ON_WM_SIZE()
END_MESSAGE_MAP()

BOOL CDetailsPropertyPage5::OnSetActive()
{
	ResizeToSheet(this);
	return CPropertyPage::OnSetActive();
}

void CDetailsPropertyPage5::OnSize(UINT nType, int cx, int cy)
{
	CPropertyPage::OnSize(nType, cx, cy);
	m_layout.Apply();
	// The change description column takes the extra width
	if (m_wndList.GetSafeHwnd() != NULL && m_wndList.GetHeaderCtrl() != NULL
		&& m_wndList.GetHeaderCtrl()->GetItemCount() == 4)
	{
		m_wndList.SetColumnWidth(3, LVSCW_AUTOSIZE_USEHEADER);
	}
}

BOOL CDetailsPropertyPage5::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	m_wndList.SetExtendedStyle(m_wndList.GetExtendedStyle()
		| LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	CRect rc;
	m_wndList.GetClientRect(&rc);
	const int w = rc.Width();
	const int wRev = w * 14 / 100, wDate = w * 22 / 100, wBy = w * 24 / 100;
	m_wndList.InsertColumn(0, _T("Version"), LVCFMT_LEFT, wRev);
	m_wndList.InsertColumn(1, _T("Issue date"), LVCFMT_LEFT, wDate);
	m_wndList.InsertColumn(2, _T("Revised by"), LVCFMT_LEFT, wBy);
	m_wndList.InsertColumn(3, _T("Change description"), LVCFMT_LEFT, max(60, w - wRev - wDate - wBy - 4));

	RebuildList(static_cast<int>(m_oHistory.size()) - 1);   // newest selected

	m_layout.Init(this);
	m_layout.Add(IDC_REV_HELP, CAnchorLayout::SizeX);
	m_layout.Add(IDC_REV_LIST, CAnchorLayout::SizeX | CAnchorLayout::SizeY);
	m_layout.Add(IDC_REV_EDIT, CAnchorLayout::MoveX | CAnchorLayout::MoveY);
	m_layout.Add(IDC_REV_DELETE, CAnchorLayout::MoveX | CAnchorLayout::MoveY);
	return TRUE;
}

void CDetailsPropertyPage5::RebuildList(int selectIndex)
{
	m_wndList.DeleteAllItems();
	for (int i = 0; i < static_cast<int>(m_oHistory.size()); ++i)
	{
		const SRevisionEntry& e = m_oHistory[i];
		m_wndList.InsertItem(i, e.rev);
		m_wndList.SetItemText(i, 1, e.date);
		m_wndList.SetItemText(i, 2, e.revisedBy);
		m_wndList.SetItemText(i, 3, OneLine(e.description));
	}
	if (selectIndex >= 0 && selectIndex < m_wndList.GetItemCount())
	{
		m_wndList.SetItemState(selectIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		m_wndList.EnsureVisible(selectIndex, FALSE);
	}
	UpdateButtons();
}

int CDetailsPropertyPage5::GetSelectedIndex() const
{
	POSITION pos = m_wndList.GetFirstSelectedItemPosition();
	if (pos == NULL)
	{
		return -1;
	}
	int idx = m_wndList.GetNextSelectedItem(pos);
	return (idx >= 0 && idx < static_cast<int>(m_oHistory.size())) ? idx : -1;
}

void CDetailsPropertyPage5::UpdateButtons()
{
	const BOOL bSel = GetSelectedIndex() >= 0;
	GetDlgItem(IDC_REV_EDIT)->EnableWindow(bSel);
	GetDlgItem(IDC_REV_DELETE)->EnableWindow(bSel);
}

void CDetailsPropertyPage5::OnItemChanged(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
	UpdateButtons();
	*pResult = 0;
}

void CDetailsPropertyPage5::OnEdit()
{
	const int idx = GetSelectedIndex();
	if (idx < 0)
	{
		return;
	}
	CEditRevisionDlg dlg(this);
	dlg.m_entry = m_oHistory[idx];
	dlg.m_pHistory = &m_oHistory;
	dlg.m_nIndex = idx;
	if (dlg.DoModal() == IDOK)
	{
		m_oHistory[idx] = dlg.m_entry;
		RebuildList(idx);
		m_bDirty = true;
		SetModified(TRUE);
	}
}

void CDetailsPropertyPage5::OnDelete()
{
	const int idx = GetSelectedIndex();
	if (idx < 0)
	{
		return;
	}
	CString msg;
	msg.Format(_T("Delete version %s (%s) from the revision history?\n\n")
		_T("Only the history entry is removed; a saved version file is left as it is."),
		(LPCTSTR)m_oHistory[idx].rev, (LPCTSTR)m_oHistory[idx].date);
	if (AfxMessageBox(msg, MB_YESNO | MB_ICONQUESTION) != IDYES)
	{
		return;
	}
	m_oHistory.erase(m_oHistory.begin() + idx);
	RebuildList(min(idx, static_cast<int>(m_oHistory.size()) - 1));
	m_bDirty = true;
	SetModified(TRUE);
}

void CDetailsPropertyPage5::OnDoubleClickList(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
	OnEdit();
	*pResult = 0;
}

BOOL CDetailsPropertyPage5::OnApply()
{
	if (m_bDirty && m_pDesign != NULL)
	{
		// Runs after the Design page's OnApply.  If Revision / Date still show
		// the newest version, follow it: renaming or re-dating the newest
		// entry updates them, deleting it falls back to the one before.
		const CDetails& cur = m_pDesign->GetCurrentSheet()->GetDetails();
		const CRevisionHistory& oldHistory = cur.GetRevisionHistory();
		CString sRevision = cur.GetRevision();
		CString sDate = cur.GetLastChange();
		if (!oldHistory.empty() && !m_oHistory.empty())
		{
			if (sRevision == oldHistory.back().rev)
			{
				sRevision = m_oHistory.back().rev;
			}
			if (sDate == oldHistory.back().date)
			{
				sDate = m_oHistory.back().date;
			}
		}

		const int total = m_pDesign->GetNumberOfSheets();
		for (int i = 0; i < total; ++i)
		{
			CConCadDoc* pSheet = m_pDesign->GetSheet(i);
			if (pSheet != NULL)
			{
				CDetails& d = pSheet->GetDetails();
				d.SetRevision(sRevision);
				d.SetLastChange(sDate);
				d.SetRevisionHistory(m_oHistory);
			}
		}

		// Keep the Design page in step, so a later Apply/OK there does not
		// write the old Revision back.
		CDetailsPropertySheet* pSheetDlg = DYNAMIC_DOWNCAST(CDetailsPropertySheet, GetParent());
		if (pSheetDlg != NULL && ::IsWindow(pSheetDlg->m_Page1.GetSafeHwnd()))
		{
			pSheetDlg->m_Page1.SetDlgItemText(DESIGNBOX_REVISION, sRevision);
			pSheetDlg->m_Page1.SetDlgItemText(DESIGNBOX_DATE, sDate);
		}

		CLog::Write(_T("Revision history edited: %d entries"), static_cast<int>(m_oHistory.size()));
		m_pDesign->GetCurrentSheet()->GetParent()->SetModifiedFlag();
		m_bDirty = false;
	}
	return CPropertyPage::OnApply();
}

/////////////////////////////////////////////////////////////////////////////
// CEditRevisionDlg

CEditRevisionDlg::CEditRevisionDlg(CWnd* pParent) :
	CDialog(CEditRevisionDlg::IDD, pParent),
	m_pHistory(NULL),
	m_nIndex(-1)
{
}

void CEditRevisionDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_REV_VERSION, m_entry.rev);
	DDX_Text(pDX, IDC_REV_DATE, m_entry.date);
	DDX_Text(pDX, IDC_REV_BY, m_entry.revisedBy);
	DDX_Text(pDX, IDC_REV_DESCRIPTION, m_sDescription);
}

BEGIN_MESSAGE_MAP(CEditRevisionDlg, CDialog)
END_MESSAGE_MAP()

BOOL CEditRevisionDlg::OnInitDialog()
{
	m_sDescription = m_entry.description;
	m_sDescription.Replace(_T("\r\n"), _T("\n"));
	m_sDescription.Replace(_T("\n"), _T("\r\n"));
	CDialog::OnInitDialog();   // DDX fills the controls
	return TRUE;
}

void CEditRevisionDlg::OnOK()
{
	if (!UpdateData(TRUE))
	{
		return;
	}
	m_entry.rev.Trim();
	if (m_entry.rev.IsEmpty() || m_entry.rev.FindOneOf(_T("\\/:*?\"<>|_")) >= 0)
	{
		AfxMessageBox(_T("Enter a version, e.g. R7 or 1.0. It cannot contain \\ / : * ? \" < > | or _"), MB_ICONEXCLAMATION);
		GetDlgItem(IDC_REV_VERSION)->SetFocus();
		return;
	}
	if (m_pHistory != NULL)
	{
		for (int i = 0; i < static_cast<int>(m_pHistory->size()); ++i)
		{
			if (i != m_nIndex && (*m_pHistory)[i].rev.CompareNoCase(m_entry.rev) == 0)
			{
				AfxMessageBox(_T("Another entry in the revision history already has that version."), MB_ICONEXCLAMATION);
				GetDlgItem(IDC_REV_VERSION)->SetFocus();
				return;
			}
		}
	}
	m_entry.date.Trim();
	if (m_entry.date.IsEmpty())
	{
		AfxMessageBox(_T("Enter the issue date (YYYY-MM-DD)."), MB_ICONEXCLAMATION);
		GetDlgItem(IDC_REV_DATE)->SetFocus();
		return;
	}
	m_entry.revisedBy.Trim();
	if (m_entry.revisedBy.IsEmpty())
	{
		AfxMessageBox(_T("Enter who revised the design."), MB_ICONEXCLAMATION);
		GetDlgItem(IDC_REV_BY)->SetFocus();
		return;
	}
	m_sDescription.Trim();
	if (m_sDescription.IsEmpty())
	{
		AfxMessageBox(_T("Enter a short change description."), MB_ICONEXCLAMATION);
		GetDlgItem(IDC_REV_DESCRIPTION)->SetFocus();
		return;
	}
	m_entry.description = m_sDescription;
	m_entry.description.Replace(_T("\r\n"), _T("\n"));
	CDialog::OnOK();
}
