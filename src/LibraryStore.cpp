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
#include "ConCad.h"
#include "LibraryStore.h"
#include "ConCadDoc.h"
#include "ConCadMultiSymbolDoc.h"
#include "ConCadMultiModuleDoc.h"
#include "StreamMemory.h"

// The constructor
CLibraryStore::CLibraryStore()
{
	// Default the name
	m_name = "Library";
}

// The destructor
CLibraryStore::~CLibraryStore()
{
}

// Extract a symbol from this library
CLibraryStoreNameSet *CLibraryStore::Extract(const TCHAR *key)
{
	//CLibraryStoreNameSet *temp=NULL;

	symbolCollection::iterator it = m_Symbols.begin();

	while (it != m_Symbols.end())
	{
		CLibraryStoreNameSet *pSymbol = & (it->second);

		// Do this for each of the names in the symbol set
		for (int i = 0; i < pSymbol->GetNumRecords(); i++)
		{
			CSymbolRecord &r = pSymbol->GetRecord(i);

			CString name = r.name;

			if (name.CompareNoCase(key) == 0)
			{
				return pSymbol;
			}
		}

		++it;
	}

	return NULL;
}

void CLibraryStore::AddToListBox(const TCHAR *theString, CListBox *theListBox)
{
	symbolCollection::iterator it = m_Symbols.begin();

	while (it != m_Symbols.end())
	{
		CLibraryStoreNameSet *pSymbol = & (it->second);

		// Do this for each of the names in the symbol set
		for (int i = 0; i < pSymbol->GetNumRecords(); i++)
		{
			CLibraryStoreSymbol &r = pSymbol->GetRecord(i);
			if (r.IsMatching(theString))
			{
				int index = theListBox->AddString(r.name + " - " + r.description);
				theListBox->SetItemDataPtr(index, &r);
			}
		}

		++it;
	}
}
void CLibraryStore::AddToTreeCtrl(const TCHAR *theString, CTreeCtrl* Tree, HTREEITEM hLib)
{
	symbolCollection::iterator it = m_Symbols.begin();

	while (it != m_Symbols.end())
	{
		CLibraryStoreNameSet *pSymbol = & (it->second);

		// Do this for each of the names in the symbol set
		for (int i = 0; i < pSymbol->GetNumRecords(); i++)
		{
			CLibraryStoreSymbol &r = pSymbol->GetRecord(i);
			if (r.IsMatching(theString))
			{
				HTREEITEM hItem = Tree->InsertItem(r.name + " - " + r.description, hLib);
				Tree->SetItemData(hItem, (DWORD_PTR) &r);
			}
		}

		++it;
	}
}
int CLibraryStore::GetMatchCount(const TCHAR *theString)
{
	int result = 0;

	symbolCollection::iterator it = m_Symbols.begin();

	while (it != m_Symbols.end())
	{
		CLibraryStoreNameSet *pSymbol = & (it->second);

		// Do this for each of the names in the symbol set
		for (int i = 0; i < pSymbol->GetNumRecords(); i++)
		{
			CLibraryStoreSymbol &r = pSymbol->GetRecord(i);
			if (r.IsMatching(theString)) result++;
		}

		++it;
	}
	return result;
}

// Check to see if the item exists in this library
BOOL CLibraryStore::DoesExist(CLibraryStoreNameSet *pSymbol)
{
	symbolCollection::iterator it = m_Symbols.begin();

	while (it != m_Symbols.end())
	{
		if (pSymbol == & (it->second))
		{
			return TRUE;
		}
		++it;
	}

	return FALSE;
}

// Re-read the library
void CLibraryStore::ReRead()
{
	// Get rid of the hash table
	m_Symbols.erase(m_Symbols.begin(), m_Symbols.end());

	// Re-read the symbol table in
	Attach(m_name);

	// Inform the document views
	static_cast<CConCadApp*> (AfxGetApp())->ResetAllSymbols();
}

// Write this library to an XML file
void CLibraryStore::SaveXML(const TCHAR *filename, int id)
{
	// First open the stream to save to
	CFile theFile;

	// Open the file for saving as a CFile for a CArchive
	BOOL r = theFile.Open(filename, CFile::modeCreate | CFile::modeWrite);
	if (!r)
	{
		CString msg;
		msg.Format(_T("Could not create %s"), filename);
		AfxMessageBox(msg, MB_ICONEXCLAMATION);
		return;
	}

	int symbols = 0;
	int modules = 0;
	{
		// Create the XML stream writer
		CStreamFile stream(&theFile, CArchive::store);
		CXMLWriter xml(&stream);

		CString comment;
		comment.Format( _T("This file was written by ConCAD (a fork of TinyCAD) %s %s\n")
						_T("ConCAD files are compatible with TinyCAD; see https://www.tinycad.net\n")
						_T("for the upstream TinyCAD project."),
			(LPCTSTR)CConCadApp::GetVersion(),
			(LPCTSTR)CConCadApp::GetReleaseType());

		xml.addComment(comment);

		xml.addTag(_T("Library"));

		// Now write the library out..
		symbolCollection::iterator it = m_Symbols.begin();

		while (it != m_Symbols.end())
		{
			const bool is_module = it->second.GetNumRecords() > 0 && it->second.GetRecord(0).is_module;
			if ((id == -1 || id == it->first) && is_module)
			{
				// A module: its name-set details plus its objects as a
				// <TinyCAD> document (read back by LoadXML as a module)
				CLibraryStoreNameSet &module = it->second;
				CConCadMultiModuleDoc temp_doc(this, module);
				if (temp_doc.IsLoaded())
				{
					xml.addTag(_T("MODULE"));
					module.SaveXML(xml);
					temp_doc.WriteModuleXML(xml);
					xml.closeTag();
					++modules;
				}
			}
			else if (id == -1 || id == it->first)
			{
				CLibraryStoreNameSet &symbol = it->second;
				CConCadMultiSymbolDoc temp_doc(this, symbol);

				xml.addTag(_T("SYMBOL"));

				// Save out the name-set details
				symbol.SaveXML(xml);

				// Now the symbol details
				temp_doc.SaveXML(xml);

				xml.closeTag();
				++symbols;
			}

			++it;
		}

		xml.closeTag();

	}

	CString msg;
	msg.Format(_T("Exported %d symbol(s) and %d module(s) to\n%s"), symbols, modules, filename);
	AfxMessageBox(msg, MB_ICONINFORMATION);
}

// Import library symbols from an XML file
void CLibraryStore::LoadXML(const TCHAR *filename)
{
	// First open the stream to save to
	CFile theFile;

	// Open the file for saving as a CFile for a CArchive
	BOOL r = theFile.Open(filename, CFile::modeRead);

	if (r)
	{
		CString name;

		// Create the XML stream writer
		CStreamFile stream(&theFile, CArchive::load);
		CXMLReader xml(&stream);

		// Get the library tag
		xml.nextTag(name);
		if (name != "Library")
		{
			Message(IDS_ABORTVERSION, MB_ICONEXCLAMATION);
			return;
		}
		xml.intoTag();

		int symbols = 0;
		int modules = 0;
		int skipped = 0;
		CConCadApp::SetLockOutSymbolRedraw(true);
		while (xml.nextTag(name))
		{
			// Is this a symbol?
			if (name == "SYMBOL")
			{
				// Load in the details
				xml.intoTag();
				CConCadMultiSymbolDoc temp_doc;
				drawingCollection drawing;
				CLibraryStoreNameSet s;
				s.LoadXML(&temp_doc, xml);
				xml.outofTag();

				// ... and store the symbol
				Store(&s, temp_doc);
				++symbols;
			}
			else if (name == "MODULE")
			{
				// Name-set details and a <TinyCAD> document with the objects
				CLibraryStoreNameSet s;
				CLibraryStoreNameSet empty;
				CConCadMultiModuleDoc temp_doc(NULL, empty);
				CString tag;
				xml.intoTag();
				while (xml.nextTag(tag))
				{
					if (tag == _T("PPP"))
					{
						xml.getChildData(s.ppp);
					}
					else if (tag == _T("ORIENTATION"))
					{
						xml.getChildData(s.orientation);
					}
					else if (tag == _T("DETAILS"))
					{
						CLibraryStoreSymbol r;
						xml.intoTag();
						r.LoadXML(xml);
						xml.outofTag();
						s.PushBackRecord(r);
					}
					else if (tag == _T("TinyCAD"))
					{
						temp_doc.ReadModuleXML(xml);
					}
				}
				xml.outofTag();

				if (s.GetNumRecords() > 0 && temp_doc.IsLoaded())
				{
					CStreamMemory data;
					{
						CXMLWriter w(&data);
						temp_doc.WriteModuleXML(w);
					}
					for (int i = 0; i < s.GetNumRecords(); i++)
					{
						s.GetRecord(i).is_module = TRUE;
					}
					s.lib = this;
					if (StoreModule(&s, data))
					{
						++modules;
					}
					else
					{
						++skipped;
					}
				}
			}
		}
		xml.outofTag();

		CConCadApp::SetLockOutSymbolRedraw(false);

		CString msg;
		msg.Format(_T("Imported %d symbol(s) and %d module(s)."), symbols, modules);
		if (skipped > 0)
		{
			CString more;
			more.Format(_T("\n\n%d module(s) were not imported: modules can only be stored in SQLite libraries (.TCLib)."), skipped);
			msg += more;
		}
		AfxMessageBox(msg, MB_ICONINFORMATION);
	}
}
