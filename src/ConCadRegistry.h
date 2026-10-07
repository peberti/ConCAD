/*
 * Project:		TinyCAD program for schematic capture
 *				https://www.tinycad.net
 * Copyright:	© 1994-2019 Matt Pyne
 * License:		Lesser GNU Public License 2.1 (LGPL)
 *				http://www.opensource.org/licenses/lgpl-license.html
 */

#pragma once

#include "registry.h"

//*************************************************************************
//*                                                                       *
//*   Registry entries only for application TinyCAD                       *
//*                                                                       *
//*************************************************************************

//=========================================================================
class CConCadRegistry: public CRegistry
{
	typedef CRegistry super;

	//=====================================================================
	//== ctor/dtor/initializing                                          ==
	//=====================================================================
public:
	CConCadRegistry();
	virtual ~CConCadRegistry();

	//-- Write the initial data to the registry
private:
	static void CreateDefaultEntries();
	static void CopyExampleFiles();

	//=====================================================================
	//== Accessor for application settings                               ==
	//=====================================================================
	//-- Returns the page size
public:
	static CSize GetPageSize();

	//-- Returns the print scale
	static double GetPrintScale();

	//-- Returns black and white print
	static bool GetPrintBandW();

	//-- Returns MDI child window state
	static bool GetMDIMaximize();

	//-- Returns main window state
	static bool GetMaximize();

	//-- Returns list of libraries
	static CStringList* GetLibraryNames();

	//-- Are automatic updates on (1), off (0) or unconfigured (-1)?
	static int GetAutomaticUpdatesOn();

	//-- What was the last version of TinyCAD update the user was told about?
	static CString GetLastAutomaticUpdateVersion();

	//-- Title-block template for new designs (template name; empty = built-in)
	static CString GetNewTitleBlock();

	//-- Ask for the title block on File -> New?
	static bool GetAskNewTitleBlock();

	//-- "Revised by" last entered in File -> Create version
	static CString GetLastRevisedBy();

	//-- Library that Special -> Create Module stores into (library name; empty = none)
	static CString GetModuleLibrary();

	//=====================================================================
	//== Mutator for application settings                               ==
	//=====================================================================
	//-- Changes the page size
	static void SetPageSize(CSize szPage);

	//-- Changes the print scale
	static void SetPrintScale(double nPrintScale);

	//-- Changes black and white print
	static void SetPrintBandW(bool nPrintBandW);

	//-- Changes MDI child window state
	static void SetMDIMaximize(bool bMDIMaximize);

	//-- Changes main window state
	static void SetMaximize(bool bWndMaximize);

	//-- Save auto-update preferences
	static void SetAutomaticUpdatesOn(bool on);

	//-- What was the last version of TinyCAD update the user was told about
	static void SetLastAutomaticUpdateVersion(CString version);

	//-- Changes the title-block template for new designs
	static void SetNewTitleBlock(CString name);

	//-- Changes whether File -> New asks for the title block
	static void SetAskNewTitleBlock(bool bAsk);

	//-- Remembers the "Revised by" entered in File -> Create version
	static void SetLastRevisedBy(CString name);

	//-- Changes the module library
	static void SetModuleLibrary(CString name);

	// Get when TinyCAD was installed
	static CString GetInstalledFileTime();

	//=====================================================================
	//== class constants                                                 ==
	//=====================================================================
	//-- The revision number for storage in the registry database
private:
	static const CString M_SKEY;

	static const CString M_SPAGESIZE;
	static const CString M_BUILDID;
	static const CString M_INSTALLED;
	static const CString M_SPRINTSCALE;
	static const CString M_SPRINTBANDW;
	static const CString M_SMDIMAXIMIZE;
	static const CString M_SMAXIMIZE;
	static const CString M_SLIBRARIES;
	static const CString M_AUTOUDPATES;
	static const CString M_AUTOUDPATEVERSION;
	static const CString M_SNEWTITLEBLOCK;
	static const CString M_SASKNEWTITLEBLOCK;
	static const CString M_SLASTREVISEDBY;
	static const CString M_SMODULELIBRARY;
};
//=========================================================================

