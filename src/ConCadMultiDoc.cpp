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

// ConCadMultiDoc.cpp : implementation file
//

#include "stdafx.h"
#include "concad.h"
#include "ConCadMultiDoc.h"
#include "HeaderStamp.h"
#include "DlgRenameSheet.h"
#include ".\ConCadMultiDoc.h"
#include "ConCadHierarchicalDoc.h"
#include "ConCadRegistry.h"
#include "SvgTitleBlock.h"
#include <shlobj.h>

static CString FileNamePart(const CString& path);

/////////////////////////////////////////////////////////////////////////////
// CConCadMultiDoc
IMPLEMENT_DYNCREATE(CConCadMultiDoc, CMultiSheetDoc)

CString CConCadMultiDoc::s_sRecovering;
int CConCadMultiDoc::s_nUntitled = 0;

CConCadMultiDoc::CConCadMultiDoc()
{
	m_active_doc = 0;
	m_bWriteProtected = false;
	m_bRecovered = false;
}

BOOL CConCadMultiDoc::OnNewDocument()
{
	if (!CMultiSheetDoc::OnNewDocument()) return FALSE;

	Clear();
	m_bWriteProtected = false;

	m_sheets.push_back(new CConCadDoc(this));

	// Start with the title block last chosen for new designs (see
	// CConCadApp::OnFileNewDesign).  A template that is no longer installed
	// leaves the built-in title block.
	CString sName = CConCadRegistry::GetNewTitleBlock();
	CString sSvg;
	if (!sName.IsEmpty() && CTitleBlockTemplateStore::FindByName(sName, sSvg))
	{
		CDetails& details = m_sheets[0]->GetDetails();
		details.m_sTitleBlockName = sName;
		details.m_sTitleBlockSvg  = sSvg;
		details.ResolveTitleBlock();
	}

	return TRUE;
}

CConCadMultiDoc::~CConCadMultiDoc()
{
	m_active_doc = 0;
	Clear();
}

BEGIN_MESSAGE_MAP(CConCadMultiDoc, CMultiSheetDoc)
	//{{AFX_MSG_MAP(CConCadMultiDoc)
	ON_COMMAND(ID_CONTEXT_ADDSHEET, OnContextAddsheet)
	ON_COMMAND(ID_CONTEXT_DELETESHEET, OnContextDeletesheet)
	ON_UPDATE_COMMAND_UI(ID_CONTEXT_DELETESHEET, OnUpdateContextDeletesheet)
	ON_COMMAND(ID_CONTEXT_RENAMESHEET, OnContextRenamesheet)
	ON_COMMAND(ID_CONTEXT_MOVESHEET, OnContextMoveSheetLeft)
	ON_COMMAND(ID_CONTEXT_MOVESHEETRIGHT, OnContextMoveSheetRight)

	//}}AFX_MSG_MAP
	ON_COMMAND(ID_CONTEXT_ADDHIERARCHICALSYMBOL, OnContextAddhierarchicalsymbol)
	ON_UPDATE_COMMAND_UI(ID_CONTEXT_ADDHIERARCHICALSYMBOL, OnUpdateContextAddhierarchicalsymbol)
	ON_COMMAND(ID_LIBRARY_ADDPIN, OnLibraryAddpin)
	ON_UPDATE_COMMAND_UI(ID_LIBRARY_ADDPIN, OnUpdateLibraryAddpin)
	ON_UPDATE_COMMAND_UI(ID_CONTEXT_RENAMESHEET, OnUpdateContextRenamesheet)
	ON_COMMAND(IDM_FILE_CREATEVERSION, OnFileCreateVersion)
	ON_UPDATE_COMMAND_UI(IDM_FILE_CREATEVERSION, OnUpdateFileCreateVersion)
	ON_COMMAND(IDM_FILE_EDITFILE, OnFileEditFile)
	ON_UPDATE_COMMAND_UI(IDM_FILE_EDITFILE, OnUpdateFileEditFile)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CConCadMultiDoc diagnostics

#ifdef _DEBUG
void CConCadMultiDoc::AssertValid() const
{
	CMultiSheetDoc::AssertValid();
}

void CConCadMultiDoc::Dump(CDumpContext& dc) const
{
	CMultiSheetDoc::Dump(dc);
}
#endif //_DEBUG
/////////////////////////////////////////////////////////////////////////////
// CConCadMultiDoc serialization


void CConCadMultiDoc::Serialize(CArchive& ar)
{
	m_xml_filename = ar.m_strFileName;

	if (ar.IsStoring())
	{
		UnTag();
		CStreamFile stream(&ar);
		CXMLWriter xml(&stream);
		SaveXML(xml);
		sheetCollection::iterator i = m_sheets.begin();
		while (i != m_sheets.end())
		{
			(*i)->MarkDocSavedForUndo();
			i++;
		}

	}
	else
	{
		CStreamFile stream(&ar);
		if (ReadFile(stream))
		{
			SetModifiedFlag(FALSE);
		}
		else
		{
			OnNewDocument();
		}

	}
}

// Get the file path name during loading or saving
CString CConCadMultiDoc::GetXMLPathName()
{
	return m_xml_filename;
}

void CConCadMultiDoc::Clear()
{
	sheetCollection::iterator i = m_sheets.begin();
	while (i != m_sheets.end())
	{
		delete (*i);
		++i;
	}

	m_sheets.clear();
}

void CConCadMultiDoc::UnTag()
{
	sheetCollection::iterator i = m_sheets.begin();
	while (i != m_sheets.end())
	{
		(*i)->GetOptions()->UnTag();
		++i;
	}

}

/////////////////////////////////////////////////////////////////////////////
// CConCadMultiDoc commands

CConCadDoc* CConCadMultiDoc::GetSheet(int i)
{
	if (m_sheets.size() == 0)
	{
		return NULL;
	}

	if (i < (int) m_sheets.size())
	{
		return m_sheets[i];
	}
	else
	{
		return m_sheets.front();
	}
}

//-------------------------------------------------------------------------
void CConCadMultiDoc::InsertSheet(int i, CConCadDoc *pDoc)
{
	sheetCollection::iterator it = m_sheets.begin();

	if (!pDoc)
	{
		pDoc = new CConCadDoc(this);
		if (GetCurrentSheet())
		{
			pDoc->GetDetails() = GetCurrentSheet()->GetDetails();
		}
		pDoc->GetDetails().m_sSheets.Format(_T("%d"), m_sheets.size() + 1);
	}

	if (i == -1)
	{
		m_sheets.insert(it, pDoc);
	}
	else if (i != static_cast<int>(m_sheets.size()))
	{
		m_sheets.insert(it + i + 1, pDoc);
	}
	else
	{
		m_sheets.push_back(pDoc);
	}
	m_active_doc = i + 1;
}

//-------------------------------------------------------------------------
void CConCadMultiDoc::DeleteSheet(int i)
{
	sheetCollection::iterator it = m_sheets.begin();
	it += i;

	// First erase the document...
	delete *it;

	// Now remove it from the list
	m_sheets.erase(it);

	if (m_active_doc >= m_sheets.size())
	{
		m_active_doc--;
	}
}

//-------------------------------------------------------------------------
void CConCadMultiDoc::MoveSheet(int index, bool left)
{

	if (left && index > 0) 
	{
		auto from = m_sheets.begin() + index;
		auto to = m_sheets.begin() + index - 1;
		std::swap(*to, *from);

		if (m_active_doc == index)
		{
			--m_active_doc;
		}
	}

	if (!left && index < m_sheets.size()-1)
	{
		auto from = m_sheets.begin() + index;
		auto to = m_sheets.begin() + index + 1;
		std::swap(*to, *from);
		
		if (m_active_doc == index)
		{
			++m_active_doc;
		}
	}

	SetTabsFromDocument();
}

//-------------------------------------------------------------------------
void CConCadMultiDoc::AutoSave()
{
	// Nothing to protect, and a write-protected version is never edited
	if (m_bWriteProtected || !IsModified())
	{
		return;
	}

	// A saved design backs up next to its file; an untitled one into the
	// recovery folder, offered again at the next start
	if (m_sAutoSavePath.IsEmpty())
	{
		if (!GetPathName().IsEmpty())
		{
			m_sAutoSavePath = GetPathName() + _T(".autosave");
		}
		else
		{
			CString dir = GetRecoveryDir();
			if (dir.IsEmpty())
			{
				return;
			}
			SHCreateDirectoryEx(NULL, dir, NULL);
			m_sAutoSavePath = NewRecoveryPath(dir);
		}
	}

	// Show the busy icon
	SetCursor(AfxGetApp()->LoadStandardCursor(IDC_WAIT));

	// Get the filename
	CString theFileName = m_sAutoSavePath;
	CString theFileNameNew = theFileName + ".new";

	// 
	CFile theFile;

	// Open the file for saving as a CFile for a CArchive
	BOOL r = theFile.Open(theFileNameNew, CFile::modeCreate | CFile::modeWrite);

	if (r)
	{
		BOOL saved = false;
		{
			// Now save the file (re-tag resources, as a normal save does)
			UnTag();
			CStreamFile stream(&theFile, CArchive::store);
			CXMLWriter xml(&stream);
			saved = SaveXML(xml);
		}
		theFile.Close();
		CFileStatus status;
		if (saved)
		{
			try
			{
				if (CFile::GetStatus(theFileName, status))
				{
					CFile::Remove(theFileName);
				}
				CFile::Rename(theFileNameNew, theFileName);
			}catch (CException *e)
			{
				// Could not rename the file properly
				e->ReportError();
				e->Delete();
			}
		}
		else if (CFile::GetStatus(theFileNameNew, status))
		{
			try
			{
				CFile::Remove(theFileNameNew);
			}
			catch (CException *e)
			{
				// Could not remove the file properly
				e->ReportError();
				e->Delete();
			}
		}
	}
	
	// Turn the cursor back to normal
	SetCursor(AfxGetApp()->LoadStandardCursor(IDC_ARROW));

	// Was there an error opening the file?
	if (!r)
	{
		// Could not open file to start saving
		Message(IDS_ABORTAUTOSAVE, MB_ICONEXCLAMATION);
	}

}

//-------------------------------------------------------------------------
// %APPDATA%\ConCAD\Recovery: autosaves of untitled designs
CString CConCadMultiDoc::GetRecoveryDir()
{
	TCHAR appData[MAX_PATH] = { 0 };
	if (FAILED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, appData)))
	{
		return CString();
	}
	return CString(appData) + _T("\\ConCAD\\Recovery");
}

// "Untitled-<pid>-<n>.con": the process id tells a later ConCAD whether
// the design's owner is still running
CString CConCadMultiDoc::NewRecoveryPath(const CString& dir)
{
	CString path;
	path.Format(_T("%s\\Untitled-%lu-%d.con"), (LPCTSTR)dir, GetCurrentProcessId(), ++s_nUntitled);
	return path;
}

void CConCadMultiDoc::DeleteAutoSave()
{
	if (!m_sAutoSavePath.IsEmpty())
	{
		::DeleteFile(m_sAutoSavePath);
		m_sAutoSavePath.Empty();
	}
}

//-------------------------------------------------------------------------
// After a real save the autosave is no longer needed.  Save a Copy As
// (bReplace == FALSE) leaves the design itself unsaved, so it stays.
BOOL CConCadMultiDoc::DoSave(LPCTSTR lpszPathName, BOOL bReplace)
{
	if (!CMultiSheetDoc::DoSave(lpszPathName, bReplace))
	{
		return FALSE;
	}
	if (bReplace)
	{
		DeleteAutoSave();
	}
	return TRUE;
}

// Closing (after Save or Don't Save) ends the need for the autosave; only
// a crash leaves one behind.
void CConCadMultiDoc::OnCloseDocument()
{
	DeleteAutoSave();
	CMultiSheetDoc::OnCloseDocument();
}

//-------------------------------------------------------------------------
BOOL CConCadMultiDoc::OnOpenDocument(LPCTSTR lpszPathName)
{
	const CString sPath = lpszPathName;

	// An untitled design from the recovery folder: it stays untitled and
	// keeps autosaving to the same file
	if (!s_sRecovering.IsEmpty() && sPath.CompareNoCase(s_sRecovering) == 0)
	{
		if (!CMultiSheetDoc::OnOpenDocument(lpszPathName))
		{
			return FALSE;
		}
		m_sAutoSavePath = sPath;
		m_bRecovered = true;
		SetModifiedFlag(TRUE);
		return TRUE;
	}

	// An autosave newer than the file means ConCAD stopped before the
	// changes were saved
	const CString sBackup = sPath + _T(".autosave");
	CFileStatus backup, file;
	if (CFile::GetStatus(sBackup, backup) && CFile::GetStatus(sPath, file))
	{
		if (backup.m_mtime > file.m_mtime)
		{
			CString msg;
			msg.Format(_T("%s has unsaved changes from %s that were autosaved before ConCAD closed unexpectedly.\n\n")
				_T("Yes\t- open the autosaved changes (save to keep them)\n")
				_T("No\t- open the file as last saved and discard the autosave"),
				(LPCTSTR)FileNamePart(sPath), (LPCTSTR)backup.m_mtime.Format(_T("%Y-%m-%d %H:%M")));
			if (AfxMessageBox(msg, MB_YESNO | MB_ICONQUESTION) == IDYES)
			{
				if (!CMultiSheetDoc::OnOpenDocument(sBackup))
				{
					return FALSE;
				}
				m_sAutoSavePath = sBackup;
				SetModifiedFlag(TRUE);
				return TRUE;
			}
		}
		// Discarded, or older than the file: of no further use
		::DeleteFile(sBackup);
	}

	return CMultiSheetDoc::OnOpenDocument(lpszPathName);
}

//-------------------------------------------------------------------------
static bool IsProcessRunning(DWORD pid)
{
	HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, pid);
	if (h == NULL)
	{
		// Exists but belongs to someone else: treat as running
		return GetLastError() == ERROR_ACCESS_DENIED;
	}
	const bool running = WaitForSingleObject(h, 0) == WAIT_TIMEOUT;
	CloseHandle(h);
	return running;
}

void CConCadMultiDoc::RecoverUnsavedDesigns(CDocTemplate* pTemplate)
{
	const CString dir = GetRecoveryDir();
	if (dir.IsEmpty() || pTemplate == NULL)
	{
		return;
	}

	// Files of a ConCAD that is still running are not orphans
	std::vector<CString> files;
	CFileFind finder;
	BOOL more = finder.FindFile(dir + _T("\\Untitled-*.con"));
	while (more)
	{
		more = finder.FindNextFile();
		if (finder.IsDirectory())
		{
			continue;
		}
		const DWORD pid = _tcstoul(finder.GetFileName().Mid(9), NULL, 10);
		if (pid != 0 && IsProcessRunning(pid))
		{
			continue;
		}
		files.push_back(finder.GetFilePath());
	}
	finder.Close();
	if (files.empty())
	{
		return;
	}

	CString msg;
	msg.Format(_T("ConCAD closed unexpectedly with %d unsaved design(s) that had never been saved.\n\n")
		_T("Yes\t- open them (save each one to keep it)\n")
		_T("No\t- delete them"), (int)files.size());
	const bool open = AfxMessageBox(msg, MB_YESNO | MB_ICONQUESTION) == IDYES;

	for (size_t i = 0; i < files.size(); ++i)
	{
		if (!open)
		{
			::DeleteFile(files[i]);
			continue;
		}
		// Take the file over under this process's id, so another ConCAD
		// does not offer it while it is open here
		const CString sMine = NewRecoveryPath(dir);
		if (!::MoveFile(files[i], sMine))
		{
			continue;
		}
		s_sRecovering = sMine;
		pTemplate->OpenDocumentFile(sMine);
		s_sRecovering.Empty();
	}
}

//-------------------------------------------------------------------------
BOOL CConCadMultiDoc::ReadFile(CStreamFile& file)
{
	// Is this an old style document?
	//CDrawingObject*	obj		= NULL;
	//BYTE			tp		= xNULL;
	CHeaderStamp oHeader;

	Clear();
	m_bWriteProtected = false;

	LONG pos = file.GetPos();

	oHeader.Read(file);

	// Return the file position back the beginning
	file.Seek(pos);

	if (oHeader.IsChecked(false))
	{
		// Use the old loader...
		CConCadDoc *pNewDoc = new CConCadDoc(this);
		if (pNewDoc->ReadFile(file))
		{
			m_sheets.push_back(pNewDoc);
			return TRUE;
		}
		else
		{
			delete pNewDoc;
		}

	}
	else
	{
		// Use the XML loader
		CString name;
		CXMLReader xml(&file);

		xml.nextTag(name);

		if (name == "TinyCAD")
		{
			// Single sheet loader...
			CConCadDoc *pNewDoc = new CConCadDoc(this);
			if (pNewDoc->ReadFileXML(xml, TRUE))
			{
				m_sheets.push_back(pNewDoc);
				return TRUE;
			}
			else
			{
				delete pNewDoc;
				return FALSE;
			}
		}

		if (name != "TinyCADSheets")
		{
			Message(IDS_ABORTVERSION, MB_ICONEXCLAMATION);
			return FALSE;
		}

		CString sWriteProtected;
		if (xml.getAttribute(_T("write_protected"), sWriteProtected))
		{
			m_bWriteProtected = sWriteProtected == _T("1");
		}

		xml.intoTag();

		while (xml.nextTag(name))
		{
			// Save the old layer setting
			//CDrawingObject *obj = NULL;

			if (name == "DETAILS")
			{
			}
			else if (name == "TinyCAD")
			{
				// Single sheet loader...
				CConCadDoc *pNewDoc = new CConCadDoc(this);
				pNewDoc->ReadFileXML(xml, TRUE);
				m_sheets.push_back(pNewDoc);
			}
			else if ( (name == _T("HierarchicalSymbol")) || (name == _T("HierachicalSymbol"))) //Unfortunately, "hierarchical" was misspelled as "hierachical" and must still be recognized as a valid tag name
			{
				// Hierarchical symbol loader...
				CConCadDoc *pNewDoc = new CConCadHierarchicalDoc(this);
				pNewDoc->ReadFileXML(xml, TRUE);
				m_sheets.push_back(pNewDoc);
			}
		}

		xml.outofTag();

		return TRUE;
	}

	return FALSE;
}

//-------------------------------------------------------------------------
bool CConCadMultiDoc::SaveXML(CXMLWriter &xml)
{
	// Write the objects to the file
	try
	{
		CString comment;

		comment.Format(_T("This file was written by ConCAD (a fork of TinyCAD) %s %s\n")
		_T("ConCAD files are compatible with TinyCAD; see https://www.tinycad.net\n")
		_T("for the upstream TinyCAD project."), (LPCTSTR)CConCadApp::GetVersion(), (LPCTSTR)CConCadApp::GetReleaseType());

		xml.addComment(comment);

		xml.addTag(_T("TinyCADSheets"));
		if (m_bWriteProtected)
		{
			xml.addAttribute(_T("write_protected"), 1);
		}

		sheetCollection::iterator i = m_sheets.begin();
		while (i != m_sheets.end())
		{
			(*i)->SaveXML(xml, TRUE, FALSE);
			++i;
		}

		xml.closeTag();
	} catch (CException *e)
	{
		// Could not save the file properly
		e->ReportError();
		e->Delete();
		return false;
	}
	return true;
}

// Is this document editing a library?
bool CConCadMultiDoc::IsLibInUse(CLibraryStore *lib)
{
	sheetCollection::iterator i = m_sheets.begin();
	while (i != m_sheets.end())
	{
		if ( (*i)->IsLibInUse(lib))
		{
			return true;
		}
		++i;
	}

	return false;
}

// get the number of documents in this multi-doc
int CConCadMultiDoc::GetNumberOfSheets()
{
	return static_cast<int> (m_sheets.size());
}

// get the number of documents in this multi-doc
void CConCadMultiDoc::SetActiveSheetIndex(int i)
{
	ASSERT( i >= 0 && i < GetNumberOfSheets() );
	m_active_doc = i;
}

CString CConCadMultiDoc::GetSheetName(int i)
{
	ASSERT( i >= 0 && i < GetNumberOfSheets() );

	CString r = m_sheets[i]->GetSheetName();

	if (r.IsEmpty())
	{
		r.Format(_T("Sheet %d"), i + 1);
		m_sheets[i]->SetSheetName(r);
	}

	return r;
}

void CConCadMultiDoc::OnFolderContextMenu()
{
	// Get the current location of the mouse
	CPoint pt;
	GetCursorPos(&pt);

	// Now bring up the context menu..
	CMenu menu;
	menu.LoadMenu(IDR_SHEET_MENU);
	menu.GetSubMenu(0)->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, AfxGetMainWnd(), NULL);
}

void CConCadMultiDoc::OnContextAddsheet()
{
	// switch back to the Edit tool
	GetCurrentSheet()->SelectObject(new CDrawEditItem(GetCurrentSheet()));

	// Insert the new sheet
	InsertSheet(GetActiveSheetIndex());

	CString s;
	s.Format(_T("Sheet %d"), GetNumberOfSheets());
	GetCurrentSheet()->SetSheetName(s);

	// Now redo the tabs
	SetTabsFromDocument();
}

void CConCadMultiDoc::OnContextDeletesheet()
{
	if (AfxMessageBox(IDS_DELETE_SHEET, MB_YESNO) == IDYES)
	{
		// switch back to the Edit tool
		GetCurrentSheet()->SelectObject(new CDrawEditItem(GetCurrentSheet()));

		// Insert the new sheet
		DeleteSheet(GetActiveSheetIndex());

		// Now redo the tabs
		SetTabsFromDocument();
	}
}

void CConCadMultiDoc::OnUpdateContextDeletesheet(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(GetNumberOfSheets() > 1);
}

void CConCadMultiDoc::OnContextRenamesheet()
{
	CDlgRenameSheet s;
	s.m_Name = GetCurrentSheet()->GetSheetName();

	if (s.DoModal() == IDOK && !s.m_Name.IsEmpty())
	{
		GetCurrentSheet()->SetSheetName(s.m_Name);
		SetTabsFromDocument();
	}
}

void CConCadMultiDoc::OnContextMoveSheetLeft()
{
	int index = GetActiveSheetIndex();
	MoveSheet(index, true);
}

void CConCadMultiDoc::OnContextMoveSheetRight()
{
	int index = GetActiveSheetIndex();
	MoveSheet(index, false);
}


void CConCadMultiDoc::OnUpdateContextRenamesheet(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(!GetCurrentSheet()->IsHierarchicalSymbol());
}

void CConCadMultiDoc::SetTabsFromDocument()
{
	UpdateAllViews(NULL, DOC_UPDATE_TABS);
}

void CConCadMultiDoc::OnContextAddhierarchicalsymbol()
{
	// Add in a new document that is the hierarchical symbol
	GetCurrentSheet()->SelectObject(new CDrawEditItem(GetCurrentSheet()));

	// Insert the new sheet		
	CConCadDoc *pDoc = new CConCadHierarchicalDoc(this);
	if (GetCurrentSheet())
	{
		pDoc->GetDetails() = GetCurrentSheet()->GetDetails();
	}

	InsertSheet(-1, pDoc);

	// Now redo the tabs
	SetTabsFromDocument();
}

void CConCadMultiDoc::OnUpdateContextAddhierarchicalsymbol(CCmdUI *pCmdUI)
{
	// Determine if we already have the hierarchical symbol
	pCmdUI->Enable(!m_sheets[0]->IsHierarchicalSymbol());
}

void CConCadMultiDoc::OnLibraryAddpin()
{
	// Add a new pin to this drawing
	GetCurrentSheet()->SelectObject(new CDrawPin(GetCurrentSheet()));
}

void CConCadMultiDoc::OnUpdateLibraryAddpin(CCmdUI* pCmdUI)
{
	// Only allow pin additions on the hierarchical symbol sheet
	pCmdUI->Enable(GetCurrentSheet()->IsHierarchicalSymbol());
}

//=========================================================================
//== File -> Create version / File -> Edit file                          ==
//=========================================================================

static bool EndsWithNoCase(const CString& s, const CString& suffix)
{
	return s.GetLength() >= suffix.GetLength()
		&& s.Right(suffix.GetLength()).CompareNoCase(suffix) == 0;
}

// Full path without its extension.
static CString StripExtension(const CString& path)
{
	int dot = path.ReverseFind(_T('.'));
	int sep = max(path.ReverseFind(_T('\\')), path.ReverseFind(_T('/')));
	return dot > sep ? path.Left(dot) : path;
}

// The path a version is saved next to, without the version suffix.  A working
// copy "Name_<ver>_working" (made by File -> Edit file) maps back to "Name",
// so versions do not pile up suffixes ("Name_1.0_working" -> "Name_1.1", not
// "Name_1.0_working_1.1").  Versions cannot contain '_', so "_<ver>" is
// everything after the last '_'.
static CString VersionBasePath(const CString& path)
{
	CString base = StripExtension(path);
	const CString working = _T("_working");
	if (EndsWithNoCase(base, working))
	{
		base = base.Left(base.GetLength() - working.GetLength());
		int us = base.ReverseFind(_T('_'));
		int sep = max(base.ReverseFind(_T('\\')), base.ReverseFind(_T('/')));
		if (us > sep + 1)
		{
			base = base.Left(us);
		}
	}
	return base;
}

static CString FileNamePart(const CString& path)
{
	int sep = max(path.ReverseFind(_T('\\')), path.ReverseFind(_T('/')));
	return path.Mid(sep + 1);
}

/////////////////////////////////////////////////////////////////////////////
// CDlgCreateVersion - asks for the version string, shows the resulting file

class CDlgCreateVersion: public CDialog
{
public:
	CString m_sVersion;
	CString m_sRevisedBy;   // required
	CString m_sChange;      // change description (required)

	CDlgCreateVersion(const CString& sBasePath, CWnd* pParent = NULL) :
		CDialog(IDD_CREATE_VERSION, pParent),
		m_sBasePath(sBasePath)
	{
	}

	CString GetTargetPath() const
	{
		return m_sBasePath + _T("_") + m_sVersion + _T(".con");
	}

protected:
	CString m_sBasePath;

	virtual void DoDataExchange(CDataExchange* pDX)
	{
		CDialog::DoDataExchange(pDX);
		DDX_Text(pDX, IDC_VERSION_EDIT, m_sVersion);
		DDX_Text(pDX, IDC_VERSION_REVISEDBY, m_sRevisedBy);
		DDX_Text(pDX, IDC_VERSION_HISTORY, m_sChange);
	}

	virtual BOOL OnInitDialog()
	{
		CDialog::OnInitDialog();
		UpdateFileName();
		return TRUE;
	}

	virtual void OnOK()
	{
		if (!UpdateData(TRUE))
		{
			return;
		}
		m_sVersion.Trim();
		if (m_sVersion.IsEmpty() || m_sVersion.FindOneOf(_T("\\/:*?\"<>|_")) >= 0)
		{
			AfxMessageBox(_T("Enter a version, e.g. R7 or 1.0. It becomes part of the file name, so it cannot contain \\ / : * ? \" < > | or _"), MB_ICONEXCLAMATION);
			return;
		}
		m_sRevisedBy.Trim();
		if (m_sRevisedBy.IsEmpty())
		{
			AfxMessageBox(_T("Enter who revised the design."), MB_ICONEXCLAMATION);
			GetDlgItem(IDC_VERSION_REVISEDBY)->SetFocus();
			return;
		}
		m_sChange.Trim();
		if (m_sChange.IsEmpty())
		{
			AfxMessageBox(_T("Enter a short change description for this version."), MB_ICONEXCLAMATION);
			GetDlgItem(IDC_VERSION_HISTORY)->SetFocus();
			return;
		}
		if (GetFileAttributes(GetTargetPath()) != INVALID_FILE_ATTRIBUTES)
		{
			CString msg;
			msg.Format(_T("%s already exists.\n\nA version is never overwritten - choose another version."), (LPCTSTR)FileNamePart(GetTargetPath()));
			AfxMessageBox(msg, MB_ICONEXCLAMATION);
			return;
		}
		CDialog::OnOK();
	}

	void UpdateFileName()
	{
		GetDlgItemText(IDC_VERSION_EDIT, m_sVersion);
		m_sVersion.Trim();
		SetDlgItemText(IDC_VERSION_FILENAME, GetTargetPath());
	}

	afx_msg void OnVersionChange()
	{
		UpdateFileName();
	}

	DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(CDlgCreateVersion, CDialog)
	ON_EN_CHANGE(IDC_VERSION_EDIT, OnVersionChange)
END_MESSAGE_MAP()

//-------------------------------------------------------------------------
void CConCadMultiDoc::SetVersionFields(const CString& sRevision, const CString& sDate, bool bWriteProtected,
	const CRevisionHistory& history)
{
	m_bWriteProtected = bWriteProtected;
	for (sheetCollection::iterator i = m_sheets.begin(); i != m_sheets.end(); ++i)
	{
		CDetails& details = (*i)->GetDetails();
		details.SetRevision(sRevision);
		details.SetLastChange(sDate);
		details.SetRevisionHistory(history);
	}
}

//-------------------------------------------------------------------------
// Show "[Write protected]" after the file name in the window titles.
void CConCadMultiDoc::SetPathName(LPCTSTR lpszPathName, BOOL bAddToMRU)
{
	// A design recovered from the recovery folder stays untitled
	if (m_bRecovered)
	{
		m_bRecovered = false;
		static int s_nRecovered = 0;
		CString title;
		title.Format(_T("Recovered %d"), ++s_nRecovered);
		SetTitle(title);
		return;
	}

	CMultiSheetDoc::SetPathName(lpszPathName, bAddToMRU);
	if (m_bWriteProtected)
	{
		SetTitle(GetTitle() + _T(" [Write protected]"));
	}
}

//-------------------------------------------------------------------------
// A write-protected version is never saved over.  Editing commands are
// disabled on it, so any in-memory change (e.g. ERC markers left by a
// netlist run) is discarded on close without asking.
BOOL CConCadMultiDoc::SaveModified()
{
	if (m_bWriteProtected)
	{
		return TRUE;
	}
	return CMultiSheetDoc::SaveModified();
}

//-------------------------------------------------------------------------
// Save the design as "Name_<version>.con" with Revision = version and
// Date = today, and continue with that file write-protected.
void CConCadMultiDoc::OnFileCreateVersion()
{
	if (GetPathName().IsEmpty())
	{
		AfxMessageBox(_T("Save the design first - the version is saved next to it."), MB_ICONINFORMATION);
		if (!DoSave(NULL))
		{
			return;
		}
	}

	// Design-level fields are shared by all sheets, so sheet 0 speaks for all.
	const CString sOldRevision = GetSheet(0)->GetDetails().GetRevision();
	const CString sOldDate = GetSheet(0)->GetDetails().GetLastChange();

	CDlgCreateVersion dlg(VersionBasePath(GetPathName()), AfxGetMainWnd());
	dlg.m_sVersion = sOldRevision;
	dlg.m_sRevisedBy = CConCadRegistry::GetLastRevisedBy();
	if (dlg.m_sRevisedBy.IsEmpty())
	{
		TCHAR user[256];
		DWORD len = 256;
		if (GetUserName(user, &len))
		{
			dlg.m_sRevisedBy = user;
		}
	}
	if (dlg.DoModal() != IDOK)
	{
		return;
	}

	// Remember who revised, as the default for the next version.
	CConCadRegistry::SetLastRevisedBy(dlg.m_sRevisedBy);

	// Drop any half-finished drawing tool before the design is frozen.
	GetCurrentSheet()->SelectObject(new CDrawEditItem(GetCurrentSheet()));

	const CRevisionHistory oldHistory = GetSheet(0)->GetDetails().GetRevisionHistory();
	const CString sToday = CTime::GetCurrentTime().Format(_T("%Y-%m-%d"));

	SRevisionEntry entry;
	entry.rev = dlg.m_sVersion;
	entry.date = sToday;
	entry.description = dlg.m_sChange;
	entry.description.Replace(_T("\r\n"), _T("\n"));
	entry.revisedBy = dlg.m_sRevisedBy;
	CRevisionHistory newHistory = oldHistory;
	newHistory.push_back(entry);

	SetVersionFields(dlg.m_sVersion, sToday, true, newHistory);
	if (!DoSave(dlg.GetTargetPath(), TRUE))
	{
		SetVersionFields(sOldRevision, sOldDate, false, oldHistory);
		UpdateAllViews(NULL);
		return;
	}
	UpdateAllViews(NULL);
}

void CConCadMultiDoc::OnUpdateFileCreateVersion(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(!m_bWriteProtected);
}

//-------------------------------------------------------------------------
// Copy this write-protected version to "Name_<version>_working.con" and
// continue editing that copy.
void CConCadMultiDoc::OnFileEditFile()
{
	if (!m_bWriteProtected || GetPathName().IsEmpty())
	{
		return;
	}

	const CString sTarget = StripExtension(GetPathName()) + _T("_working.con");
	if (GetFileAttributes(sTarget) != INVALID_FILE_ATTRIBUTES)
	{
		CString msg;
		msg.Format(_T("A working copy already exists:\n\n%s\n\n")
			_T("Yes\t- open the existing working copy\n")
			_T("No\t- replace it with a fresh copy of this version"), (LPCTSTR)FileNamePart(sTarget));
		int r = AfxMessageBox(msg, MB_YESNOCANCEL | MB_ICONQUESTION);
		if (r == IDCANCEL)
		{
			return;
		}
		if (r == IDYES)
		{
			AfxGetApp()->OpenDocumentFile(sTarget);
			return;
		}
	}

	m_bWriteProtected = false;
	if (!DoSave(sTarget, TRUE))
	{
		m_bWriteProtected = true;
		return;
	}
	UpdateAllViews(NULL);
}

void CConCadMultiDoc::OnUpdateFileEditFile(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_bWriteProtected);
}
