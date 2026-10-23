/*
 TinyCAD program for schematic capture
 Copyright 1994/1995/2002,2003 Matt Pyne.

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

#include "stdafx.h"
#include <assert.h>
#include "ConCadView.h"
#include "registry.h"
#include "ConCad.h"
#include "LibraryDb.h"
#include "LibrarySQLite.h"
#include "DlgLibraryBox.h"
#include "LibraryCollection.h"
#include ".\dlglibrarybox.h"

//*************************************************************************
//*                                                                       *
//* The dialog for editing which libraries are in use                     *
//*                                                                       *
//*************************************************************************
BEGIN_MESSAGE_MAP( CDlgLibraryBox, CDialog )
	//{{AFX_MSG_MAP(CDlgLibraryBox)
	ON_COMMAND( LIBRARYBOX_ADD, OnAdd )
	ON_COMMAND( LIBRARYBOX_REMOVE, OnRemove )
	ON_COMMAND( LIBRARYBOX_EDIT, OnEdit )
	ON_LBN_DBLCLK(LIBRARYBOX_AVAILABLE, OnDblclkAvailable)
	ON_LBN_SELCHANGE(LIBRARYBOX_AVAILABLE, OnSelchangeAvailable)
	ON_BN_CLICKED(LIBRARYBOX_NEW, OnNew)
	ON_BN_CLICKED(LIBRARYBOX_CLEANUP, OnCleanup)
	ON_BN_CLICKED(LIBRARYBOX_UPGRADE, OnUpgrade)
	ON_NOTIFY(TCN_SELCHANGE, IDC_LIBRARY_TABS, OnTabChange)
	//}}AFX_MSG_MAP
	ON_WM_SIZE()
END_MESSAGE_MAP()

//=========================================================================
//== ctor/dtor/initializing                                              ==
//=========================================================================

//-------------------------------------------------------------------------
CDlgLibraryBox::CDlgLibraryBox(CWnd* pParentWnd) :
	CDialog(IDD_LIBRARY, pParentWnd)
{
	//{{AFX_DATA_INIT(CDlgLibraryBox)
	// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	m_lstLibs = NULL;
	m_modulesTab = false;
}
//-------------------------------------------------------------------------
//-- Creating and filling listbox
BOOL CDlgLibraryBox::OnInitDialog()
{
	// Set up the list box with the current library names in it
	m_lstLibs = (CListBox*) GetDlgItem(LIBRARYBOX_AVAILABLE);

	m_tabs.SubclassDlgItem(IDC_LIBRARY_TABS, this);
	m_tabs.InsertItem(0, _T("Symbol libraries"));
	m_tabs.InsertItem(1, _T("Module libraries"));

	// Get the position of the buttons
	CRect client, button, list;
	GetClientRect(client);
	GetDlgItem(LIBRARYBOX_ADD)->GetWindowRect(button);
	ScreenToClient(button);

	m_lstLibs->GetWindowRect(list);
	ScreenToClient(list);

	m_button_pos = client.right - button.left;
	m_list_pos = client.right - list.right;
	m_list_bottom_border = client.bottom - list.bottom;

	RefreshComponents(true);

	return TRUE;
}
//-------------------------------------------------------------------------

//==========================================================================
//== accessor                                                             ==
//==========================================================================

//-------------------------------------------------------------------------
//-- Returns a reference on the current selected listbox item,
//-- otherwise NULL.
CLibraryStore* CDlgLibraryBox::GetSelectedLibrary() const
{
	CLibraryStore* oReturn = NULL;
	int nSelIndex = m_lstLibs->GetCurSel();

	// Get the item which has been selected (item data = library index; the
	// same name can be both a symbol and a module library)
	if (nSelIndex != LB_ERR)
	{
		oReturn = CLibraryCollection::GetLibraryByIndex((int) m_lstLibs->GetItemData(nSelIndex));
	}

	return oReturn;
}
//-------------------------------------------------------------------------

//=========================================================================
//== mutator                                                             ==
//=========================================================================

//-------------------------------------------------------------------------
//-- Controls En/Disabling of some buttons of the dialog
//-- Parameter bHasToRefill forces redrawing of listbox
void CDlgLibraryBox::RefreshComponents(bool bHasToRefill)
{
	CButton* btnRemove = static_cast<CButton*> (GetDlgItem(LIBRARYBOX_REMOVE));
	CButton* btnEdit = static_cast<CButton*> (GetDlgItem(LIBRARYBOX_EDIT));
	CButton* btnCleanup = static_cast<CButton*> (GetDlgItem(LIBRARYBOX_CLEANUP));
	CButton* btnUpgrade = static_cast<CButton*> (GetDlgItem(LIBRARYBOX_UPGRADE));
	BOOL bIsLibSelected = m_lstLibs->GetCurSel() != LB_ERR;

	if (bHasToRefill)
	{
		CLibraryCollection::FillLibraryNames(m_lstLibs, m_modulesTab);

		// refilling unselects the listbox
		bIsLibSelected = false;
	}

	// activ if any listbox entry is selected
	btnRemove->EnableWindow(bIsLibSelected);
	btnEdit->EnableWindow(bIsLibSelected);
	btnCleanup->EnableWindow(bIsLibSelected);
	btnUpgrade->EnableWindow(bIsLibSelected);
	btnUpgrade->ShowWindow(m_modulesTab ? SW_HIDE : SW_SHOW);   // old formats are symbol libraries
}
//-------------------------------------------------------------------------
//-- Show the symbol (false) or module (true) libraries
void CDlgLibraryBox::ShowTab(bool modules)
{
	m_modulesTab = modules;
	m_tabs.SetCurSel(modules ? 1 : 0);
	RefreshComponents(true);
}
//-------------------------------------------------------------------------
//-- New symbol or module library object for a file name extension
CLibraryStore* CDlgLibraryBox::NewLibraryFor(const CString& ext)
{
	if (ext == _T("idx"))
	{
		return new CLibraryFile;
	}
	if (ext == _T("mdb"))
	{
		return new CLibraryDb;
	}
	return new CLibrarySQLite(ext == _T("ccmod"));
}
//-------------------------------------------------------------------------


//=========================================================================
//== message handling                                                    ==
//=========================================================================

//-------------------------------------------------------------------------
//-- Closes dialog and saves library pathes to registry
void CDlgLibraryBox::OnOK()
{
	CLibraryCollection::SaveToRegistry();
	EndDialog(IDOK);
}
//-------------------------------------------------------------------------
//-- Proofs vality of a library
//-- Adds it to collection and listbox
void CDlgLibraryBox::OnAdd()
{
	CFileDialog dlg(TRUE, m_modulesTab ? _T("*.CCMod") : _T("*.TCLib"), NULL, OFN_ALLOWMULTISELECT,
		m_modulesTab ? _T("ConCAD Module Library (*.CCMod)|*.CCMod|All files (*.*)|*.*||")
			: _T("ConCAD Library (*.TCLib)|*.TCLib|Legacy Library (*.mdb, *idx, *.TCLib)|*.mdb;*.idx;*.TCLib|All files (*.*)|*.*||"), AfxGetMainWnd());

	// Create buffer for file names. 
	const DWORD numberOfFileNames = 100;
	const DWORD fileNameMaxLength = MAX_PATH + 1;
	const DWORD bufferSize = (numberOfFileNames * fileNameMaxLength) + 1;
	TCHAR* filenamesBuffer = new TCHAR[bufferSize];

	// Initialize beginning and end of buffer.
	filenamesBuffer[0] = NULL;
	filenamesBuffer[bufferSize - 1] = NULL;

	// Attach buffer to OPENFILENAME member.
	dlg.m_ofn.lpstrFile = filenamesBuffer;
	dlg.m_ofn.nMaxFile = bufferSize;

	if (dlg.DoModal() == IDOK)
	{
		// Retrieve file name(s).
		POSITION fileNamesPosition = dlg.GetStartPosition();
		while (fileNamesPosition != NULL)
		{
			CString csFileName(dlg.GetNextPathName(fileNamesPosition));
			CString csExt;

			// Remove the extension
			int lastindex = csFileName.ReverseFind('.');
			if (lastindex > 0)
			{
				csExt = csFileName.Right(csFileName.GetLength() - (lastindex + 1)).MakeLower();
				csFileName = csFileName.Left(lastindex);
			}
			
			// Create the library (the extension decides symbol or module
			// library, whichever tab is open)
			CLibraryStore* NewLib = NewLibraryFor(csExt);

			BOOL libraryLoaded = NewLib->Attach(csFileName);

			// Now add it to the library linked list
			if (libraryLoaded && !CLibraryCollection::Contains(NewLib))
			{
				CLibraryCollection::Add(NewLib);
				ShowTab(NewLib->m_modules);
			}
			else
			{
				delete NewLib;
			}
		}
		// Finally update the symbol picker...
		CConCadApp::ResetAllSymbols();
	}

	// Release file names buffer. 
	delete[] filenamesBuffer;
}

//-------------------------------------------------------------------------
//-- Removes a library from collection and listbox
void CDlgLibraryBox::OnRemove()
{
	CLibraryStore* pLib = GetSelectedLibrary();

	// Get the item which has been selected
	if (pLib != NULL)
	{
		if (CConCadApp::IsLibInUse(pLib))
		{
			// We cannot delete whilst in use
			AfxMessageBox(IDS_NODELETE);
		}
		else
		{
			CLibraryCollection::Remove(pLib);
			RefreshComponents(true);

			// Finally update the symbol picker...
			CConCadApp::ResetAllSymbols();
		}
	}
}
//-------------------------------------------------------------------------
//-- Opens a thumbnail view of the selected library
void CDlgLibraryBox::OnEdit()
{
	CLibraryStore* pLib = GetSelectedLibrary();

	// If no library was selected, simply return without starting the editor.
	// This condition is caused by double clicking somewhere in the library list that doesn't 
	// have the name of a library.  Formerly, there was an assert here for (pLib != NULL), but
	// it was causing a program break if the user simply double clicked within the library list box,
	// but missed one of the list items.
	//
	// Should be impossible to edit a non-selected library
	// assert( pLib != NULL );

	if (pLib == NULL) return;

	// First can this library be upgraded?
	if (pLib->MustUpgrade())
	{
		if (AfxMessageBox(IDS_MUSTUPGRADE, MB_YESNO) != IDYES)
		{
			return;
		}

		pLib = CLibraryCollection::Upgrade(pLib);
		if (!pLib)
		{
			return;
		}
	}

	CConCadApp::EditLibrary(pLib);
	CLibraryCollection::SaveToRegistry();
	EndDialog(IDOK);
}
//-------------------------------------------------------------------------
//--
void CDlgLibraryBox::OnNew()
{
	CFileDialog dlg(FALSE, m_modulesTab ? _T("CCMod") : _T("TCLib"), NULL, OFN_HIDEREADONLY, 
					m_modulesTab ? _T("ConCAD Module Library (*.CCMod)|*.CCMod||")
						: _T("ConCAD Library (*.TCLib)|*.TCLib|Library database in old MSJet format (*.mdb)|*.mdb|All files (*.*)|*.*||"),
					AfxGetMainWnd());

	if (dlg.DoModal() == IDOK)
	{
		// Remove the extension
		CString csFileName(dlg.GetPathName());
		CString csExt;

		// Remove the extension
		int lastindex = csFileName.ReverseFind('.');
		if (lastindex > 0)
		{
			csExt = csFileName.Right(csFileName.GetLength() - (lastindex + 1)).MakeLower();
			csFileName = csFileName.Left(lastindex);
		}

		// Create the library object.  When you explicitly ask for an MDB,
		// you get an MDB!  A module library is SQLite with .CCMod.
		CLibraryStore* NewLib;

		if (csExt == _T("mdb"))
		{
			NewLib = new CLibraryDb;
		}
		else
		{
			NewLib = new CLibrarySQLite(m_modulesTab || csExt == _T("ccmod"));
		}

		// .. and create the file
		if (NewLib->Create(csFileName))
		{
			// Now add it to the library linked list
			CLibraryCollection::Add(NewLib);

			// .. and to the list box
			ShowTab(NewLib->m_modules);
		}
		else
		{
			delete NewLib;
			return;
		}
	}
}

//-------------------------------------------------------------------------
//-- Cleanup a library (runs VACUUM)
void CDlgLibraryBox::OnCleanup()
{
	CLibraryStore* pOldLib = GetSelectedLibrary();
	CLibraryCollection::Cleanup(pOldLib);
}
//-------------------------------------------------------------------------
//-- Converts a library from the old format to
//-- the new Microsoft Access database format.
void CDlgLibraryBox::OnUpgrade()
{
	CLibraryStore* pOldLib = GetSelectedLibrary();
	CLibraryCollection::Upgrade(pOldLib);
}
//-------------------------------------------------------------------------
//-- single click on listbox refreshes it
void CDlgLibraryBox::OnSelchangeAvailable()
{
	RefreshComponents(false);
}
//-------------------------------------------------------------------------
//-- double click opens a thumbnail view of the selected library
void CDlgLibraryBox::OnDblclkAvailable()
{
	OnEdit();
}
//-------------------------------------------------------------------------
//-- switch between symbol and module libraries
void CDlgLibraryBox::OnTabChange(NMHDR* pNMHDR, LRESULT* pResult)
{
	ShowTab(m_tabs.GetCurSel() == 1);
	*pResult = 0;
}
//-------------------------------------------------------------------------


void CDlgLibraryBox::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);

	// Move the tabs and the list box into position
	if (m_tabs.GetSafeHwnd())
	{
		CRect r;
		m_tabs.GetWindowRect(r);
		ScreenToClient(r);
		r.right = cx - m_list_pos;
		m_tabs.MoveWindow(r);
	}
	if (m_lstLibs)
	{
		CRect r;
		m_lstLibs->GetWindowRect(r);
		ScreenToClient(r);
		r.right = cx - m_list_pos;
		r.bottom = cy - m_list_bottom_border;
		m_lstLibs->MoveWindow(r);
	}

	const int ids[] = {LIBRARYBOX_ADD, LIBRARYBOX_REMOVE, LIBRARYBOX_EDIT, LIBRARYBOX_NEW, LIBRARYBOX_CLEANUP, LIBRARYBOX_UPGRADE, IDOK};

	for (int i = 0; i < sizeof (ids) / sizeof(int); ++i)
	{
		CWnd *pButton = GetDlgItem(ids[i]);
		if (pButton)
		{
			CRect r;
			pButton->GetWindowRect(r);
			ScreenToClient(r);
			r += CPoint( (cx - m_button_pos) - r.left, 0);
			pButton->MoveWindow(r);
		}
	}
}
