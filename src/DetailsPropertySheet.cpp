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
// DetailsPropertySheet.cpp : implementation file
//
#include "stdafx.h"
#include "resource.h"
#include "DetailsPropertySheet.h"
/////////////////////////////////////////////////////////////////////////////
// CDetailsPropertySheet
IMPLEMENT_DYNAMIC(CDetailsPropertySheet, CPropertySheet)

CDetailsPropertySheet::CDetailsPropertySheet(CMultiSheetDoc* pDesign, CWnd* pWndParent) :
	CPropertySheet(IDS_PROPSHT_CAPTION1, pWndParent), m_pDesign(pDesign), m_szMin(0, 0),
	m_Page1(pDesign), m_Page2(pDesign), m_Page3(pDesign), m_Page4(pDesign), m_Page5(pDesign)
{
	// Add all of the property pages here.  Note that
	// the order that they appear in here will be
	// the order they appear in on screen.  By default,
	// the first page of the set is the active one.
	// One way to make a different property page the
	// active one is to call SetActivePage().

	AddPage(&m_Page1);
	AddPage(&m_Page2);
	// m_Page3 (the standalone "Variables" tab) is intentionally not added —
	// the variables editor is now embedded on the Design page (m_Page1).
	AddPage(&m_Page4);
	AddPage(&m_Page5);
}

CDetailsPropertySheet::~CDetailsPropertySheet()
{
}

BEGIN_MESSAGE_MAP(CDetailsPropertySheet, CPropertySheet)
//{{AFX_MSG_MAP(CDetailsPropertySheet)
// NOTE - the ClassWizard will add and remove mapping macros here.
//}}AFX_MSG_MAP
	ON_WM_SIZE()
	ON_WM_GETMINMAXINFO()
	ON_MESSAGE(WM_DETAILS_RESIZEPAGE, OnResizePage)
END_MESSAGE_MAP()

BOOL CDetailsPropertySheet::OnInitDialog()
{
	BOOL bResult = CPropertySheet::OnInitDialog();

	// Make the frame resizable, keeping the client area as it was
	CRect rcClient;
	GetClientRect(rcClient);
	ModifyStyle(0, WS_THICKFRAME);
	CRect rcWnd = rcClient;
	::AdjustWindowRectEx(&rcWnd, GetStyle(), FALSE, GetExStyle());
	SetWindowPos(NULL, 0, 0, rcWnd.Width(), rcWnd.Height(),
		SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

	GetWindowRect(rcWnd);
	m_szMin = rcWnd.Size();

	m_layout.Init(this);
	m_layout.Add(GetTabControl()->GetSafeHwnd(), CAnchorLayout::SizeX | CAnchorLayout::SizeY);
	const UINT buttons[] = { IDOK, IDCANCEL, ID_APPLY_NOW, IDHELP };
	for (int i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i)
	{
		CWnd* pButton = GetDlgItem(buttons[i]);
		if (pButton != NULL)
		{
			m_layout.Add(pButton->GetSafeHwnd(), CAnchorLayout::MoveX | CAnchorLayout::MoveY);
		}
	}
	return bResult;
}

void CDetailsPropertySheet::ResizeActivePage()
{
	CPropertyPage* pPage = GetActivePage();
	CTabCtrl* pTab = GetTabControl();
	if (pPage == NULL || pTab == NULL || pPage->GetSafeHwnd() == NULL || pTab->GetSafeHwnd() == NULL)
	{
		return;
	}
	CRect rc;
	pTab->GetWindowRect(rc);
	ScreenToClient(rc);
	pTab->AdjustRect(FALSE, rc);
	pPage->MoveWindow(rc);
}

void CDetailsPropertySheet::OnSize(UINT nType, int cx, int cy)
{
	CPropertySheet::OnSize(nType, cx, cy);
	if (m_szMin.cx > 0 && nType != SIZE_MINIMIZED)
	{
		m_layout.Apply();
		ResizeActivePage();
	}
}

void CDetailsPropertySheet::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
	CPropertySheet::OnGetMinMaxInfo(lpMMI);
	if (m_szMin.cx > 0)
	{
		lpMMI->ptMinTrackSize.x = m_szMin.cx;
		lpMMI->ptMinTrackSize.y = m_szMin.cy;
	}
}

// A page that just became active is sized to the current tab area (the
// property sheet itself only places it at the size the dialog opened with).
LRESULT CDetailsPropertySheet::OnResizePage(WPARAM, LPARAM)
{
	if (m_szMin.cx > 0)
	{
		ResizeActivePage();
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// CDetailsPropertySheet message handlers


