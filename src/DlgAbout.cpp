/*
 * Project:		TinyCAD program for schematic capture
 *				https://www.tinycad.net
 * Copyright:	© 1994-2009 Matt Pyne
 * License:		Lesser GNU Public License 2.1 (LGPL)
 *				http://www.opensource.org/licenses/lgpl-license.html
 */

#include "stdafx.h"
#include "resource.h"
#include "DlgAbout.h"
#include "ConCad.h"

//*************************************************************************
//*                                                                       *
//* Shows information of the program like name of the programmer e.g.     *
//*                                                                       *
//*************************************************************************

//=========================================================================
//== ctor/dtor/initializing                                              ==
//=========================================================================

//-------------------------------------------------------------------------
CDlgAbout::CDlgAbout() :
	super(IDD_ABOUTBOX)
{
}
//-------------------------------------------------------------------------
BOOL CDlgAbout::OnInitDialog()
{
	super::OnInitDialog();

	CString sVersion;
	sVersion.Format(_T("%s %s [%s]"), (LPCTSTR)CConCadApp::GetName(), (LPCTSTR)CConCadApp::GetVersion(), (LPCTSTR)CConCadApp::GetReleaseType());
	GetDlgItem(IDC_VERSION)->SetWindowText(sVersion);

	return TRUE;
}
//-------------------------------------------------------------------------
