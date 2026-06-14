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

// ConCadMultiSymbolDoc.cpp : implementation file
//

#include "stdafx.h"
#include "concad.h"
#include "ConCadMultiSymbolDoc.h"
#include "DlgPartsInPackage.h"
#include ".\ConCadMultiSymbolDoc.h"
#include "DlgUpdateBox.h"
#include "HeaderStamp.h"

/////////////////////////////////////////////////////////////////////////////
// CConCadMultiSymbolDoc
IMPLEMENT_DYNCREATE(CConCadMultiSymbolDoc, CMultiSheetDoc)

CConCadMultiSymbolDoc::CConCadMultiSymbolDoc()
{
	m_symbols.resize(1);
	m_symbols[0] = new CConCadSymbolDoc(this);
	m_ppp = 1;
	m_heterogeneous = false;
	m_current_index = 0;
}

CConCadMultiSymbolDoc::CConCadMultiSymbolDoc(CLibraryStore* pLib, CLibraryStoreNameSet &symbol)
{
	m_symboledit = symbol;
	m_libedit = pLib;

	m_heterogeneous = false;
	m_current_index = 0;

	// Set our name according to the symbol
	CString title = "Symbol - " + symbol.GetRecord(0).name;
	SetTitle(title);

	// Get the file to load the symbol details from
	CStream* s = symbol.GetMethodArchive();
	if (!s)
	{
		// Create a blank symbol, this must a new one!
		m_symbols.resize(1);
		m_symbols[0] = new CConCadSymbolDoc(this);
		m_ppp = 1;
		m_heterogeneous = false;
		m_current_index = 0;

		return;
	}

	// We have to see if this is an XML file or an old binary format 
	// symbol
	LONG pos = s->GetPos();
	CHeaderStamp oHeader;
	oHeader.Read(*s);
	s->Seek(pos);

	if (oHeader.IsChecked(false))
	{
		m_symbols.resize(1);
		m_symbols[0] = new CConCadSymbolDoc(this);
		m_symbols[0]->ReadFile(*s);
		m_symbols[0]->setSymbol();
		m_ppp = m_symbols[0]->GetPartsPerPackage();
	}
	else
	{
		CXMLReader xml(s);
		CString tag;
		xml.nextTag(tag);
		if (tag == "TinyCADSheets")
		{
			LoadXML(xml, true);
		}
		else if (tag == "TinyCAD")
		{
			LoadXML(xml, false);
		}
		else
		{
			// Not much to do about this!
		}
	}

	delete s;
}

BOOL CConCadMultiSymbolDoc::OnNewDocument()
{
	if (!CMultiSheetDoc::OnNewDocument()) return FALSE;
	return TRUE;
}

CConCadMultiSymbolDoc::~CConCadMultiSymbolDoc()
{
	sheetCollection::iterator i = m_symbols.begin();
	while (i != m_symbols.end())
	{
		delete *i;
		++i;
	}
}

BEGIN_MESSAGE_MAP(CConCadMultiSymbolDoc, CMultiSheetDoc)
	//{{AFX_MSG_MAP(CConCadMultiSymbolDoc)
	ON_COMMAND(ID_LIBRARY_ADDPIN, OnLibraryAddpin)
	ON_COMMAND(ID_FILE_SAVE, OnFileSave)
	ON_COMMAND(ID_FILE_SAVE_AS, OnFileSaveAs)
	ON_COMMAND(ID_LIBRARY_SETPARTSPERPACKAGE, OnLibrarySetpartsperpackage)
	//}}AFX_MSG_MAP
	ON_COMMAND(ID_LIBRARY_HETEROGENEOUS, OnLibraryHeterogeneous)
	ON_UPDATE_COMMAND_UI(ID_LIBRARY_HETEROGENEOUS, OnUpdateLibraryHeterogeneous)
	ON_COMMAND(ID_LIBRARY_HOMOGENEOUS, OnLibraryHomogeneous)
	ON_UPDATE_COMMAND_UI(ID_LIBRARY_HOMOGENEOUS, OnUpdateLibraryHomogeneous)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CConCadMultiSymbolDoc diagnostics

#ifdef _DEBUG
void CConCadMultiSymbolDoc::AssertValid() const
{
	CMultiSheetDoc::AssertValid();
}

void CConCadMultiSymbolDoc::Dump(CDumpContext& dc) const
{
	CMultiSheetDoc::Dump(dc);
}
#endif //_DEBUG
/////////////////////////////////////////////////////////////////////////////
// CConCadMultiSymbolDoc serialization

void CConCadMultiSymbolDoc::Serialize(CArchive& ar)
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
// CConCadMultiSymbolDoc commands

// Is this document editing a library?
bool CConCadMultiSymbolDoc::IsLibInUse(CLibraryStore *lib)
{
	return lib == m_libedit;
}

// get the number of documents in this multi-doc
int CConCadMultiSymbolDoc::GetNumberOfSheets()
{
	return m_ppp;
}

void CConCadMultiSymbolDoc::SetActiveSheetIndex(int i)
{
	if (m_heterogeneous)
	{
		m_current_index = i;
	}
	else
	{
		m_symbols[0]->EditPartInPackage(i);
	}
}

int CConCadMultiSymbolDoc::GetActiveSheetIndex()
{
	if (m_heterogeneous)
	{
		return m_current_index;
	}
	else
	{
		return m_symbols[0]->GetPart();
	}
}

CString CConCadMultiSymbolDoc::GetSheetName(int i)
{
	CString r;
	r.Format(_T("Part %c"), 'A' + i);
	return r;
}

// Get the currently active sheet to work with
CConCadDoc* CConCadMultiSymbolDoc::GetSheet(int i)
{
	if (m_heterogeneous)
	{
		return m_symbols[i];
	}
	else
	{
		return m_symbols[0];
	}
}

void CConCadMultiSymbolDoc::OnFolderContextMenu()
{
	// Get the current location of the mouse
	CPoint pt;
	GetCursorPos(&pt);

	// Now bring up the context menu..
	CMenu menu;
	menu.LoadMenu(IDR_SYMBOL_SHEET_MENU);
	menu.GetSubMenu(0)->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, AfxGetMainWnd(), NULL);
}

CConCadDoc* CConCadMultiSymbolDoc::GetActiveSheet()
{
	return GetSheet(GetActiveSheetIndex());
}

void CConCadMultiSymbolDoc::OnLibraryAddpin()
{
	GetActiveSheet()->SelectObject(new CDrawPin(GetActiveSheet()));
}

void CConCadMultiSymbolDoc::OnFileSave()
{
	Store();
}

void CConCadMultiSymbolDoc::OnFileSaveAs()
{
	Store();
}

BOOL CConCadMultiSymbolDoc::CanCloseFrame(CFrameWnd* pFrameArg)
{
	if (IsModified())
	{
		CString prompt;
		AfxFormatString1(prompt, AFX_IDP_ASK_TO_SAVE, m_symboledit.GetRecord(0).name);
		switch (AfxMessageBox(prompt, MB_YESNOCANCEL, AFX_IDP_ASK_TO_SAVE))
		{
			case IDCANCEL:
				return FALSE;
			case IDYES:
				return Store();
			case IDNO:
				break;
		}
	}

	return TRUE;
}

void CConCadMultiSymbolDoc::OnLibrarySetpartsperpackage()
{
	CDlgPartsInPackage dialog;
	dialog.m_Parts = m_ppp;

	if (dialog.DoModal() == IDOK)
	{
		if (dialog.m_Parts > 0 && dialog.m_Parts <= 26)
		{
			m_ppp = dialog.m_Parts;

			if (m_heterogeneous)
			{
				for (unsigned int i = m_ppp; i < m_symbols.size(); ++i)
				{
					delete m_symbols[i];
				}

				m_symbols.resize(m_ppp);

				for (unsigned int i = 0; i < m_symbols.size(); ++i)
				{
					if (!m_symbols[i])
					{
						m_symbols[i] = new CConCadSymbolDoc(this);
					}
				}

				m_symbols[0]->SetPartsPerPackage(1);
			}
			else
			{
				m_symbols[0]->SetPartsPerPackage(m_ppp);
			}

			UpdateAllViews(NULL, DOC_UPDATE_TABS);
		}
	}

}

void CConCadMultiSymbolDoc::OnLibraryHeterogeneous()
{
	// If we are already hetrogeneous then ignore this click
	if (m_heterogeneous)
	{
		return;
	}

	// We can only change if there is only a single part per-package
	if (m_ppp != 1)
	{
		AfxMessageBox(IDS_BAD_PPP);
	}
	else
	{
		m_heterogeneous = true;
	}
}

void CConCadMultiSymbolDoc::OnUpdateLibraryHeterogeneous(CCmdUI *pCmdUI)
{
	// TODO: Add your command update UI handler code here
	pCmdUI->SetCheck(m_heterogeneous ? 1 : 0);
}

void CConCadMultiSymbolDoc::OnLibraryHomogeneous()
{
	// If we are already homogeneous then ignore this click
	if (!m_heterogeneous)
	{
		return;
	}

	// We can only change if there is only a single part per-package
	if (m_ppp != 1)
	{
		AfxMessageBox(IDS_BAD_PPP);
	}
	else
	{
		m_heterogeneous = false;
	}
}

void CConCadMultiSymbolDoc::OnUpdateLibraryHomogeneous(CCmdUI *pCmdUI)
{
	// TODO: Add your command update UI handler code here
	pCmdUI->SetCheck(m_heterogeneous ? 0 : 1);
}

// Write this symbol back to the library
BOOL CConCadMultiSymbolDoc::Store()
{
	m_symboledit.ppp = (BYTE) m_ppp;

	CDlgUpdateBox dlg(AfxGetMainWnd());
	dlg.SetSymbol(&m_symboledit);

	if (dlg.DoModal() == IDOK)
	{
		(m_symbols[0])->GetOptions()->UnTag();
		m_libedit->Store(getSymbol(), *this);
		return TRUE;
	}
	else
	{
		return FALSE;
	}

}

void CConCadMultiSymbolDoc::SaveXML(CXMLWriter &xml)
{
	if (m_heterogeneous)
	{
		xml.addTag(_T("TinyCADSheets"));
		sheetCollection::iterator i = m_symbols.begin();
		while (i != m_symbols.end())
		{
			(*i)->SaveXML(xml, FALSE, FALSE);
			(*i)->MarkDocSavedForUndo();
			++i;
		}
		xml.closeTag();
	}
	else
	{
		m_symbols[0]->SaveXML(xml, FALSE, FALSE);
		m_symbols[0]->MarkDocSavedForUndo();
	}
}

void CConCadMultiSymbolDoc::LoadXML(CXMLReader &xml, bool heterogeneous)
{
	m_heterogeneous = heterogeneous;

	// Clear out the old design
	sheetCollection::iterator i = m_symbols.begin();
	while (i != m_symbols.end())
	{
		delete *i;
		++i;
	}
	m_symbols.resize(0);

	// Load in the new design
	if (m_heterogeneous)
	{
		m_ppp = 0;
		xml.intoTag();
		CString tag;
		while (xml.nextTag(tag))
		{
			if (tag == "TinyCAD")
			{
				CConCadSymbolDoc *pDesign = new CConCadSymbolDoc(this);
				pDesign->ReadFileXML(xml, TRUE);
				pDesign->setSymbol();
				m_symbols.push_back(pDesign);
				++m_ppp;
			}
		}
		xml.outofTag();
	}
	else
	{
		CConCadSymbolDoc *pDesign = new CConCadSymbolDoc(this);
		pDesign->ReadFileXML(xml, TRUE);
		pDesign->setSymbol();
		m_ppp = pDesign->GetPartsPerPackage();
		m_symbols.push_back(pDesign);
	}
}

