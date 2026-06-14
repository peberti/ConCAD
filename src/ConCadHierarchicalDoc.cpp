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

// ConCadHierarchicalDoc.cpp : implementation file
//

#include "stdafx.h"
#include "ConCad.h"
#include "ConCadHierarchicalDoc.h"

// CConCadHierarchicalDoc

CConCadHierarchicalDoc::CConCadHierarchicalDoc(CMultiSheetDoc*pParent) :
	CConCadDoc(pParent)
{
}

CConCadHierarchicalDoc::~CConCadHierarchicalDoc()
{
}

CString CConCadHierarchicalDoc::GetSheetName() const
{
	return "Hierarchical Symbol";
}

void CConCadHierarchicalDoc::SetSheetName(const TCHAR *sName)
{
}

bool CConCadHierarchicalDoc::IsHierarchicalSymbol() const
{
	return true;
}

const CString CConCadHierarchicalDoc::GetXMLTag()
{
	return _T("HierarchicalSymbol"); //Historically, this was misspelled "HierachicalSymbol" and has been fixed in many places in a backwards compatible way
}

const CString CConCadHierarchicalDoc::GetAltXMLTag()
{ //Note:  Never "fix" the misspelled text string below - this is needed to be able to read in old designs where the keyword actually was misspelled
	return _T("HierachicalSymbol"); //Historically, this was misspelled "HierachicalSymbol"
}

// Called after a paste or import to enable the
// document to sort out the imported block when
// necessary
void CConCadHierarchicalDoc::PostPaste()
{
	UngroupSymbols();
}

// Select/add a new symbol for editing
void CConCadHierarchicalDoc::SelectSymbol(CLibraryStoreSymbol *theSymbol)
{
	SelectObject(NULL);

	CDesignFileSymbol *pSymbol = theSymbol->GetDesignSymbol(this);

	drawingCollection method;
	if (pSymbol)
	{
		pSymbol->GetMethod(0, false, method);

		drawingIterator it = method.begin();
		while (it != method.end())
		{
			CDrawingObject *q = Dup(*it);
			Add(q);
			Select(q);
			++it;
		}

		delete pSymbol;
	}

	CDrawBlockImport *pImport = new CDrawBlockImport(this);
	SelectObject(pImport);
	pImport->Import();
}

CString CConCadHierarchicalDoc::getDefaultReferenceString() const
{
	return CConCadDoc::getDefaultReferenceString();
}
