/*
 * ConCAD: a library module opened for editing
 * License:		Lesser GNU Public License 2.1 (LGPL)
 */

#pragma once

#include "ConCadMultiDoc.h"
#include "Symbol.h"

class CLibraryStore;

// Library window -> Edit on a module: the module's objects in a one-sheet
// design.  Save stores them back into the library (Store Module dialog),
// like the symbol editor does for symbols.  Also used without a window to
// read and write module data for the library XML export/import.
class CConCadMultiModuleDoc: public CConCadMultiDoc
{
public:
	// pLib == NULL: an empty module (XML import)
	CConCadMultiModuleDoc(CLibraryStore* pLib, CLibraryStoreNameSet &module);
	virtual ~CConCadMultiModuleDoc();

	// Could the module's objects be read?
	bool IsLoaded() const
	{
		return m_loaded;
	}

	// The sheet's objects as module XML: <TinyCAD> with the fonts, styles
	// and symbol definitions they use
	void WriteModuleXML(CXMLWriter &xml);
	// Read module XML; ReadModuleXML expects to be on its <TinyCAD> tag
	bool ReadModule(CStream &stream);
	bool ReadModuleXML(CXMLReader &xml);

	BOOL Store();

	virtual bool IsLibInUse(CLibraryStore *lib);
	virtual void AutoSave()
	{
	}
	virtual BOOL SaveModified();

protected:
	CLibraryStore* m_libedit;
	CLibraryStoreNameSet m_moduleedit;
	bool m_loaded;

	void UpdateTitle();

	afx_msg void OnFileSave();
	afx_msg void OnNotForModules();
	afx_msg void OnUpdateNotForModules(CCmdUI* pCmdUI);
	DECLARE_MESSAGE_MAP()
};
