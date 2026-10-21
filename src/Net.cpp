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
#include <set>
#include <map>

#include "ConCadView.h"
#include "ConCad.h"
#include "special.h"
#include "library.h"
#include "net.h"
#include "ConCadRegistry.h"
#include "diag.h"
#include "DlgPCBExport.h"
#include "MultiSheetDoc.h"
#include "ConCadMultiDoc.h"

////// Generate the net list for this design //////


void CConCadView::OnSpecialNet()
{
	// Get rid of any drawing tool
	GetCurrentDocument()->SelectObject(new CDrawEditItem(GetCurrentDocument()));

	// Get the file in which to save the network
	TCHAR szFile[256];
	szFile[0] = '\0';

	_tcscpy_s(szFile, GetDocument()->GetPathName());
	TCHAR* ext = _tcsrchr(szFile, '.');
	if (!ext)
	{
		_tcscpy_s(szFile, _T("output.net"));
	}
	else
	{
#ifdef USE_VS2003
		_tcscpy_s(ext, _T(".net"));
#else
		size_t remaining_space = &szFile[255] - ext + 1;
		_tcscpy_s(ext, remaining_space, _T(".net"));
#endif
	}

	CDlgPCBExport dlg;
	dlg.m_Filename = szFile;

	if (dlg.DoModal() != IDOK) return;

	// Generate the net list file
	CNetList netlist;
	netlist.m_prefix_references = dlg.m_Prefix;
	netlist.WriteNetListFile(dlg.m_type, static_cast<CConCadMultiDoc*> (GetDocument()), dlg.m_Filename);

	// Now open the netlist for the user
	CConCadApp::EditTextFile(dlg.m_Filename);
}
