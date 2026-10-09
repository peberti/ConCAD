/*
 * ConCAD: a library module opened for editing
 * License:		Lesser GNU Public License 2.1 (LGPL)
 */

#include "stdafx.h"
#include "ConCad.h"
#include "ConCadMultiModuleDoc.h"
#include "LibraryStore.h"
#include "DlgUpdateBox.h"
#include "StreamMemory.h"

CConCadMultiModuleDoc::CConCadMultiModuleDoc(CLibraryStore* pLib, CLibraryStoreNameSet &module)
{
	m_libedit = pLib;
	m_loaded = false;

	Clear();
	CConCadDoc *pSheet = new CConCadDoc(this);
	pSheet->GetDetails().SetVisible(false); // a module has no title block
	m_sheets.push_back(pSheet);
	m_active_doc = 0;

	if (pLib != NULL)
	{
		CStream *pData = module.GetMethodArchive(); // also loads the module's fields
		if (pData != NULL)
		{
			m_loaded = ReadModule(*pData);
			delete pData;
		}
	}
	m_moduleedit = module;
	m_moduleedit.lib = pLib;
	UpdateTitle();
}

CConCadMultiModuleDoc::~CConCadMultiModuleDoc()
{
}

BEGIN_MESSAGE_MAP(CConCadMultiModuleDoc, CConCadMultiDoc)
	ON_COMMAND(ID_FILE_SAVE, OnFileSave)
	ON_COMMAND(ID_FILE_SAVE_AS, OnFileSave)
	ON_COMMAND(ID_CONTEXT_ADDSHEET, OnNotForModules)
	ON_UPDATE_COMMAND_UI(ID_CONTEXT_ADDSHEET, OnUpdateNotForModules)
	ON_COMMAND(ID_CONTEXT_ADDHIERARCHICALSYMBOL, OnNotForModules)
	ON_UPDATE_COMMAND_UI(ID_CONTEXT_ADDHIERARCHICALSYMBOL, OnUpdateNotForModules)
	ON_COMMAND(IDM_FILE_CREATEVERSION, OnNotForModules)
	ON_UPDATE_COMMAND_UI(IDM_FILE_CREATEVERSION, OnUpdateNotForModules)
	ON_COMMAND(IDM_FILE_EDITFILE, OnNotForModules)
	ON_UPDATE_COMMAND_UI(IDM_FILE_EDITFILE, OnUpdateNotForModules)
END_MESSAGE_MAP()

void CConCadMultiModuleDoc::UpdateTitle()
{
	SetTitle(_T("Module: ") + m_moduleedit.GetRecord(0).name);
}

bool CConCadMultiModuleDoc::ReadModule(CStream &stream)
{
	CConCadDoc *pSheet = m_sheets[0];
	drawingCollection drawing;
	if (!pSheet->ReadFile(stream, FALSE, drawing))
	{
		return false;
	}
	pSheet->Add(drawing);
	pSheet->UnSelect();
	return true;
}

bool CConCadMultiModuleDoc::ReadModuleXML(CXMLReader &xml)
{
	CConCadDoc *pSheet = m_sheets[0];
	drawingCollection drawing;
	if (!pSheet->ReadFileXML(xml, FALSE, drawing, TRUE))
	{
		return false;
	}
	pSheet->Add(drawing);
	pSheet->UnSelect();
	m_loaded = true;
	return true;
}

void CConCadMultiModuleDoc::WriteModuleXML(CXMLWriter &xml)
{
	CConCadDoc *pSheet = m_sheets[0];
	pSheet->UnSelect();
	pSheet->SelectAll();
	pSheet->SaveModuleXML(xml);
	pSheet->UnSelect();
}

// File -> Save: store the module back into its library
BOOL CConCadMultiModuleDoc::Store()
{
	CConCadDoc *pSheet = m_sheets[0];
	pSheet->SelectObject(new CDrawEditItem(pSheet)); // finish any edit in progress

	CDlgUpdateBox dlg(AfxGetMainWnd());
	dlg.SetSymbol(&m_moduleedit);
	dlg.SetModuleMode();
	if (dlg.DoModal() != IDOK)
	{
		return FALSE;
	}

	CStreamMemory stream;
	{
		CXMLWriter xml(&stream);
		WriteModuleXML(xml);
	}

	for (int i = 0; i < m_moduleedit.GetNumRecords(); i++)
	{
		m_moduleedit.GetRecord(i).is_module = TRUE;
	}
	m_moduleedit.lib = m_libedit;
	if (!m_libedit->StoreModule(&m_moduleedit, stream)) // reports its own errors
	{
		return FALSE;
	}

	pSheet->MarkDocSavedForUndo();
	CDocument::SetModifiedFlag(FALSE);
	UpdateTitle();
	return TRUE;
}

void CConCadMultiModuleDoc::OnFileSave()
{
	Store();
}

BOOL CConCadMultiModuleDoc::SaveModified()
{
	if (!IsModified())
	{
		return TRUE;
	}

	CString prompt;
	AfxFormatString1(prompt, AFX_IDP_ASK_TO_SAVE, m_moduleedit.GetRecord(0).name);
	switch (AfxMessageBox(prompt, MB_YESNOCANCEL, AFX_IDP_ASK_TO_SAVE))
	{
		case IDCANCEL:
			return FALSE;
		case IDYES:
			return Store();
		default:
			return TRUE;
	}
}

bool CConCadMultiModuleDoc::IsLibInUse(CLibraryStore *lib)
{
	return lib == m_libedit;
}

// Sheets, hierarchical symbols and versions do not apply to a module
void CConCadMultiModuleDoc::OnNotForModules()
{
}

void CConCadMultiModuleDoc::OnUpdateNotForModules(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(FALSE);
}
