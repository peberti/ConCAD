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

#if !defined(AFX_CONCADMULTIDOC_H__7E25C39B_649E_4421_A207_635409612FB6__INCLUDED_)
#define AFX_CONCADMULTIDOC_H__7E25C39B_649E_4421_A207_635409612FB6__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "ConCadDoc.h"
#include "MultiSheetDoc.h"

// ConCadMultiDoc.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CConCadMultiDoc document

class CConCadMultiDoc: public CMultiSheetDoc
{
protected:
	DECLARE_DYNCREATE( CConCadMultiDoc)

	void UnTag();

	// Save this collection out...
	bool SaveXML(CXMLWriter&);

	// Load the document
	BOOL ReadFile(CStreamFile& file);

	void SetTabsFromDocument();

	void Clear();
	void InsertSheet(int i, CConCadDoc *pDoc = NULL);
	void DeleteSheet(int i);
	void MoveSheet(int index, bool left);

	// Attributes
public:

	// Construction
	CConCadMultiDoc();

	// Force an autosave of the document
	virtual void AutoSave();

	// Is this document editing a library?
	virtual bool IsLibInUse(CLibraryStore *lib);

	// get the number of documents in this multi-doc
	virtual int GetNumberOfSheets();
	virtual void SetActiveSheetIndex(int i);
	virtual int GetActiveSheetIndex()
	{
		return m_active_doc;
	}
	virtual CString GetSheetName(int i);

	// Get the currently active sheet to work with
	virtual CConCadDoc* GetSheet(int i);

	virtual void OnFolderContextMenu();

	// Get the file path name during loading or saving
	virtual CString GetXMLPathName();

	// Is this a write-protected version (File -> Create version)?
	virtual bool IsWriteProtected()
	{
		return m_bWriteProtected;
	}

	virtual void SetPathName(LPCTSTR lpszPathName, BOOL bAddToMRU = TRUE);
	virtual BOOL SaveModified();
	virtual BOOL DoSave(LPCTSTR lpszPathName, BOOL bReplace = TRUE);
	virtual BOOL OnOpenDocument(LPCTSTR lpszPathName);
	virtual void OnCloseDocument();

	// At start-up: offer the untitled designs autosaved by a ConCAD that
	// did not exit normally, opening them through pTemplate
	static void RecoverUnsavedDesigns(CDocTemplate* pTemplate);

protected:
	// The file this design autosaves to: "<path>.autosave" next to a saved
	// design, or "Untitled-<pid>-<n>.con" in the recovery folder for an
	// untitled one.  Empty until the first autosave; deleted on save/close.
	CString m_sAutoSavePath;

	// Set while an untitled design is opened from the recovery folder, so
	// SetPathName leaves it untitled
	bool m_bRecovered;

	void DeleteAutoSave();
	static CString GetRecoveryDir();
	static CString NewRecoveryPath(const CString& dir);

	// The recovery file being opened by RecoverUnsavedDesigns
	static CString s_sRecovering;
	static int s_nUntitled;

	// Set by File -> Create version, cleared by File -> Edit file; saved as
	// the write_protected attribute of <TinyCADSheets>.
	bool m_bWriteProtected;

	// Write Revision, Date and the write-protect flag to every sheet
	void SetVersionFields(const CString& sRevision, const CString& sDate, bool bWriteProtected,
		const CRevisionHistory& history);

	typedef std::vector<CConCadDoc*> sheetCollection;
	sheetCollection m_sheets;

	unsigned int m_active_doc;

	// The filename during loading/saving
	CString m_xml_filename;

	// Operations
public:

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CConCadMultiDoc)
public:
	virtual void Serialize(CArchive& ar); // overridden for document i/o
protected:
	virtual BOOL OnNewDocument();
	//}}AFX_VIRTUAL

	// Implementation
public:
	virtual ~CConCadMultiDoc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
protected:
	//{{AFX_MSG(CConCadMultiDoc)
	afx_msg void OnContextAddsheet();
	afx_msg void OnContextDeletesheet();
	afx_msg void OnUpdateContextDeletesheet(CCmdUI* pCmdUI);
	afx_msg void OnContextRenamesheet();
	afx_msg void OnContextMoveSheetLeft();
	afx_msg void OnContextMoveSheetRight();
	afx_msg void OnLibraryAddpin();
	afx_msg void OnUpdateLibraryAddpin(CCmdUI* pCmdUI);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnContextAddhierarchicalsymbol();
	afx_msg void OnUpdateContextAddhierarchicalsymbol(CCmdUI *pCmdUI);
	afx_msg void OnUpdateContextRenamesheet(CCmdUI *pCmdUI);
	afx_msg void OnFileCreateVersion();
	afx_msg void OnUpdateFileCreateVersion(CCmdUI *pCmdUI);
	afx_msg void OnFileEditFile();
	afx_msg void OnUpdateFileEditFile(CCmdUI *pCmdUI);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CONCADMULTIDOC_H__7E25C39B_649E_4421_A207_635409612FB6__INCLUDED_)
