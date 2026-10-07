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
#include "concad.h"
#include "LibraryDoc.h"
#include "ConCadSymbolDoc.h"
#include "ConCadMultiSymbolDoc.h"
#include "DlgUpdateBox.h"
#include "StreamMemory.h"

// Modules (Special -> Create Module) hold design objects rather than a symbol
// drawing.  Opening one in the symbol editor and storing it again would lose
// those objects, so modules are copied as raw data instead.
static bool IsModule(CLibraryStoreNameSet& s)
{
	return s.GetNumRecords() > 0 && s.GetRecord(0).is_module;
}

// Store a copy of module src into target, as a new record (asNew) or in place;
// showDialog lets the user edit names and fields first.
static void StoreModuleCopy(CLibraryStoreNameSet& src, CLibraryStore* target, bool asNew, bool showDialog, const CString& namePrefix)
{
	CStream* pData = src.GetMethodArchive();   // also loads the fields
	CStreamMemory* pMem = dynamic_cast<CStreamMemory*>(pData);
	if (pMem == NULL)
	{
		delete pData;
		return;
	}

	CLibraryStoreNameSet copy = src;
	copy.lib = target;
	if (asNew)
	{
		copy.FilePos = (DWORD) -1;
		for (int i = 0; i < copy.GetNumRecords(); i++)
		{
			copy.GetRecord(i).NameID = (DWORD) -1;
		}
		copy.GetRecord(0).name = namePrefix + copy.GetRecord(0).name;
	}

	bool store = true;
	if (showDialog)
	{
		CDlgUpdateBox dlg(AfxGetMainWnd());
		dlg.SetSymbol(&copy);
		dlg.SetModuleMode();
		store = dlg.DoModal() == IDOK;
	}
	if (store)
	{
		for (int i = 0; i < copy.GetNumRecords(); i++)
		{
			copy.GetRecord(i).is_module = TRUE;
		}
		if (!target->StoreModule(&copy, *pMem) && target->MustUpgrade())
		{
			AfxMessageBox(_T("Modules can only be stored in SQLite libraries (.TCLib)."), MB_ICONINFORMATION);
		}
	}
	delete pData;
}

/////////////////////////////////////////////////////////////////////////////
// CLibraryDoc
IMPLEMENT_DYNCREATE(CLibraryDoc, CDocument)

CLibraryDoc::CLibraryDoc()
{
	m_selected = -1;
	m_pLibrary = NULL;
}

BOOL CLibraryDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument()) return FALSE;
	return TRUE;
}

CLibraryDoc::~CLibraryDoc()
{
}

BEGIN_MESSAGE_MAP(CLibraryDoc, CDocument)
	//{{AFX_MSG_MAP(CLibraryDoc)
	ON_COMMAND(ID_FILE_EXPORTLIBRARY, OnFileExportlibrary)
	ON_COMMAND(ID_FILE_IMPORTLIBRARY, OnFileImportlibrary)
	ON_COMMAND(ID_CONTEXT_EXPORTSYMBOL, OnContextExportsymbol)
	ON_UPDATE_COMMAND_UI(ID_CONTEXT_EXPORTSYMBOL, OnUpdateContextExportsymbol)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CLibraryDoc diagnostics

#ifdef _DEBUG
void CLibraryDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CLibraryDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG
/////////////////////////////////////////////////////////////////////////////
// CLibraryDoc serialization

void CLibraryDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
}

/////////////////////////////////////////////////////////////////////////////
// CLibraryDoc commands

void CLibraryDoc::setLibrary(CLibraryStore *pLibrary)
{
	// Keep a track of this library
	m_pLibrary = pLibrary;

	// Now set our name accordingly
	SetTitle(pLibrary->m_name);

	// Make a map of the library names, so they are sorted in
	// name order in our list...
	m_SymbolMap.clear();
	for (CLibraryStore::symbolCollection::iterator i = m_pLibrary->m_Symbols.begin(); i != m_pLibrary->m_Symbols.end(); i++)
	{
		int id = i->first;
		CLibraryStoreNameSet &name_set = i->second;
		CSymbolRecord &r = name_set.GetRecord(0);

		m_SymbolMap.insert(std::pair<CString, int>(r.name, id));
	}

	// We are no longer selecting an object
	m_selected = -1;

	// .. and make a check of the symbol changes
	SetModifiedFlag(FALSE);
}

void CLibraryDoc::ResetSymbols()
{
	// Erase everything
	m_selected = -1;

	// Now re-load the symbols
	setLibrary(m_pLibrary);

	// Get the last edited symbol as our selection
	int id = -1;
	int which = -1;
	int c = 0;
	for (idCollection::iterator i = m_SymbolMap.begin(); i != m_SymbolMap.end(); i++)
	{
		if (i->second > id)
		{
			which = c;
			id = i->second;
		}
		c++;
	}
	m_selected = which;

	// Now invalidate this window
	UpdateAllViews(NULL);
}

// Is this document editing a library?
bool CLibraryDoc::IsLibInUse(CLibraryStore *lib)
{
	return lib == m_pLibrary;
}

CLibraryStoreNameSet &CLibraryDoc::getNameSet(const idCollection::iterator &it)
{
	return m_pLibrary->m_Symbols[it->second];
}

CLibraryStore::symbolCollection::iterator CLibraryDoc::getSymbol(int which)
{
	idCollection::iterator i;

	// Determine which symbol we are talking about
	for (i = m_SymbolMap.begin(); i != m_SymbolMap.end() && which > 0; i++)
	{
		which--;
	}

	return m_pLibrary->m_Symbols.find(i->second);
}

void CLibraryDoc::DeleteSymbol(int which)
{
	CLibraryStore::symbolCollection::iterator it = getSymbol(which);

	// First we must delete the symbol from the library store...
	m_pLibrary->DeleteSymbol(it->second);
}

void CLibraryDoc::EditSymbol(int which)
{
	// Is this a new symbol?
	if (which == -1)
	{
		CLibraryStoreNameSet new_symbol;
		new_symbol.Blank();
		new_symbol.lib = m_pLibrary;
		CConCadApp::EditSymbol(m_pLibrary, new_symbol);
	}
	else
	{
		// Determine which symbol we are talking about
		if ((bool) m_pLibrary->m_Symbols.empty()) return;	//desired symbol doesn't exist

		CLibraryStore::symbolCollection::iterator it = getSymbol(which);
		if (it != m_pLibrary->m_Symbols.end())
		{
			if (IsModule(it->second))
			{
				AfxMessageBox(_T("A module cannot be opened in the symbol editor.\n\n")
					_T("To change it, place it in a design, edit it there, and store it again with Special > Create Module. ")
					_T("Use Properties to change its name or fields."), MB_ICONINFORMATION);
				return;
			}
			CConCadApp::EditSymbol(m_pLibrary, it->second);
		}
	}
}

void CLibraryDoc::DuplicateSymbol(int which)
{
	// Determine which symbol we are talking about
	CLibraryStore::symbolCollection::iterator it = getSymbol(which);

	if (it != m_pLibrary->m_Symbols.end())
	{
		if (IsModule(it->second))
		{
			StoreModuleCopy(it->second, m_pLibrary, true, false, _T("Copy of "));
			return;
		}

		// Load this symbol into a dummy document
		CConCadMultiSymbolDoc tempDoc(m_pLibrary, it->second);

		CLibraryStoreNameSet *r = tempDoc.getSymbol();
		for (int i = 0; i < r->GetNumRecords(); i++)
		{
			r->GetRecord(i).NameID = (DWORD) -1;
			if (i == 0)
			{
				r->GetRecord(i).name = "Copy of " + r->GetRecord(i).name;
			}
		}
		r->FilePos = (DWORD) -1;
		tempDoc.Store();
	}
}

void CLibraryDoc::CopySymbol(int which, CLibraryStore *target)
{
	// Determine which symbol we are talking about
	CLibraryStore::symbolCollection::iterator it = getSymbol(which);

	if (it != m_pLibrary->m_Symbols.end())
	{
		if (IsModule(it->second))
		{
			StoreModuleCopy(it->second, target, true, false, CString());
			return;
		}

		// Load this symbol into a dummy document
		CConCadMultiSymbolDoc tempDoc(target, it->second);

		CLibraryStoreNameSet *r = tempDoc.getSymbol();
		for (int i = 0; i < r->GetNumRecords(); i++)
		{
			r->GetRecord(i).NameID = (DWORD) -1;
		}
		r->FilePos = (DWORD) -1;
		tempDoc.Store();
	}
}

void CLibraryDoc::SymbolProperties(int which)
{
	// Determine which symbol we are talking about
	CLibraryStore::symbolCollection::iterator it = getSymbol(which);

	if (it != m_pLibrary->m_Symbols.end())
	{
		if (IsModule(it->second))
		{
			StoreModuleCopy(it->second, m_pLibrary, false, true, CString());
			return;
		}

		// Load this symbol into a dummy document
		CConCadMultiSymbolDoc tempDoc(m_pLibrary, it->second);
		tempDoc.Store();
	}
}

void CLibraryDoc::OnFileExportlibrary()
{
	// Get the file name for the parts list
	TCHAR szFile[256];

	_tcscpy_s(szFile, m_pLibrary->m_name);
	_tcscat_s(szFile, _T(".xml"));

	CFileDialog dlg(FALSE, _T("*.xml"), szFile, OFN_HIDEREADONLY, _T("XML Library file (*.xml)|*.xml|All files (*.*)|*.*||"), AfxGetMainWnd());

	if (dlg.DoModal() != IDOK) return;

	m_pLibrary->SaveXML(dlg.GetPathName());

}

void CLibraryDoc::OnFileImportlibrary()
{
	CFileDialog dlg(TRUE, _T("*.xml"), NULL, OFN_HIDEREADONLY, _T("XML Library file (*.xml)|*.xml|All files (*.*)|*.*||"), AfxGetMainWnd());

	if (dlg.DoModal() != IDOK) return;

	m_pLibrary->LoadXML(dlg.GetPathName());
}

void CLibraryDoc::OnContextExportsymbol()
{

	// Determine which symbol we are talking about
	CLibraryStore::symbolCollection::iterator it = getSymbol(m_selected);
	if (it != m_pLibrary->m_Symbols.end())
	{

		CLibraryStoreNameSet &s = (*it).second;
		CSymbolRecord &r = s.GetRecord(0);

		// Get the file name for the symbol list
		TCHAR szFile[256];

		_tcscpy_s(szFile, r.name);
		_tcscat_s(szFile, _T(".xml"));

		CFileDialog dlg(FALSE, _T("*.xml"), szFile, OFN_HIDEREADONLY, _T("XML Library file (*.xml)|*.xml|All files (*.*)|*.*||"), AfxGetMainWnd());

		if (dlg.DoModal() != IDOK) return;

		m_pLibrary->SaveXML(dlg.GetPathName(), (*it).first);
	}
}

void CLibraryDoc::OnUpdateContextExportsymbol(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(getSelected() != -1);
}
