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
#include "ConCadView.h"
#include "special.h"
#include "library.h"
#include "ConCad.h"
#include "DlgBOMExport.h"
#include "BOMGenerator.h"

////// The auto annotate special function //////

AnnotateSetup theASetup;

//Compare positions of two objects
static bool DORefComp(CDrawingObject* o1, CDrawingObject* o2)
{
	CDPoint c1, c2;
	c1.x = (o1->m_point_a.x + o1->m_point_b.x) / 2;
	c1.y = (o1->m_point_a.y + o1->m_point_b.y) / 2;
	c2.x = (o2->m_point_a.x + o2->m_point_b.x) / 2;
	c2.y = (o2->m_point_a.y + o2->m_point_b.y) / 2;
	if (c1.y < c2.y) return true;
	if (c1.y > c2.y) return false;
	if (c1.x < c2.x) return true;
	return false;
}

CString AnnotatePagePrefix(CConCadDoc *pSheet)
{
	CString prefix;
	CMultiSheetDoc *pMulti = pSheet != NULL ? pSheet->GetParent() : NULL;
	if (!theASetup.sheet_prefix || pMulti == NULL)
	{
		return prefix;
	}
	for (int i = 0; i < pMulti->GetNumberOfSheets(); ++i)
	{
		if (pMulti->GetSheet(i) == pSheet)
		{
			prefix.Format(_T("%d-"), i + 1);
			break;
		}
	}
	return prefix;
}

// Auto annotate the design
void CConCadView::OnSpecialAnnotate()
{
	CDlgAnnotateBox theDialog(this, theASetup);

	// Get rid of any drawing tool
	GetCurrentDocument()->SelectObject(new CDrawEditItem(GetCurrentDocument()));

	// Do the dialog
	size_t action = theDialog.DoModal();

	if (action == IDC_REF_PAINTER)
	{
		theASetup = theDialog.v;
		GetCurrentDocument()->SelectObject(new CDrawRefPainter(GetCurrentDocument(), theASetup.startval));
		return;
	}
	else if (action != IDOK)
	{
		return;
	}

	theASetup = theDialog.v;

	// Set the busy icon
	SetCursor(AfxGetApp()->LoadStandardCursor(IDC_WAIT));

	// Now add/remove references
	CDrawMethod *thisMethod;
	CSymbolRecord *thisSymbol;
	int value = 0;
	//int part=0;
	BOOL IsSet, IsMatch = FALSE, MissingSymbols = FALSE;

	for (int whichPass = 0; whichPass < 2; whichPass++)
	{
		int sheet = theASetup.all_sheets ? 0 : GetDocument()->GetActiveSheetIndex();
		do
		{
			// Sheet number prefix: sheet 1 gets P1-1, P1-2 ..., sheet 2 P2-1 ...
			const CString pagePrefix = AnnotatePagePrefix(GetDocument()->GetSheet(sheet));

			// Signal change for undo
			if (whichPass == 0)
			{
				GetDocument()->GetSheet(sheet)->BeginNewChangeSet();
			}

			//copy all objects in the current sheet to a new vector so that it can be sorted
			std::vector<CDrawingObject*> objs;
			objs.assign(GetDocument()->GetSheet(sheet)->GetDrawingBegin(), GetDocument()->GetSheet(sheet)->GetDrawingEnd());
			//sort it according to the objects positions
			std::sort(objs.begin(), objs.end(), DORefComp);

			//drawingIterator 
			std::vector<CDrawingObject*>::iterator it = objs.begin();
			while (it != objs.end())
			{
				CDrawingObject *pointer = *it;
				// Look for method objects
				ObjType objType = pointer->GetType();
				if (objType == xMethodEx3 || objType == xHierarchicalSymbol)
				{
					thisMethod = static_cast<CDrawMethod *> (pointer);
					thisSymbol = thisMethod->GetSymbolData();

					// If there is no symbol then cannot modify this symbol!
					if (thisMethod->IsNoSymbol())
					{
						MissingSymbols = TRUE;
						++it;
						continue;
					}

					// Has this symbol got a reference?
					IsSet = thisMethod->HasRef();

					switch (theASetup.reference)
					{
						case AnnotateSetup::ALL: // All references
							IsMatch = TRUE;
							break;
						case AnnotateSetup::UNNUMBERED: // Un-numbered references
							IsMatch = !thisMethod->HasRef();
							break;
						case AnnotateSetup::MATCHING: // References that match...
							IsMatch = theASetup.matchval == thisSymbol->reference;
							break;
					}

					if (IsMatch)
					{
						// Signal change for undo
						GetDocument()->GetSheet(sheet)->MarkChangeForUndo(thisMethod);

						// First pass  - we remove references if necessary,
						// Second pass - we add refences back in...
						//
						if (whichPass == 0)
						{
							// Remove any matching references (if necessary)
							// (with the sheet prefix, a reference without
							// this sheet's prefix is always renumbered)
							const int refVal = pagePrefix.IsEmpty() ? thisMethod->GetRefVal() : thisMethod->GetPagedRefVal(pagePrefix);
							if (IsSet && (theASetup.value!=AnnotateSetup::SPECIFIED 
								|| refVal < 0 || refVal>=theASetup.startval) ) 
							{
								thisMethod->RemoveReference();
							}
						}
						else
						{
							// Now add back any references
							if (theASetup.action == AnnotateSetup::ADD)
							{
								/* OK, why the special case? 
								 if (theASetup.reference != 1)
								 {
								 value = (theASetup.value == 0) ? 1 : theASetup.startval;
								 }
								 */
								value = (theASetup.value == AnnotateSetup::SPECIFIED) ? theASetup.startval : 1;
								thisMethod->AddReference(value, theASetup.all_sheets, pagePrefix);
							}
						}
					}
				}

				++it;
			}
			++sheet;
		} while (theASetup.all_sheets && sheet < GetDocument()->GetNumberOfSheets());
	}

	// Were there any missing symbols?
	if (MissingSymbols) Message(IDS_MISSMETH, MB_ICONEXCLAMATION);

	// Restore the correct cursor
	SetCursor(AfxGetApp()->LoadStandardCursor(IDC_ARROW));

	// Ensure the window is re-drawn
	GetCurrentDocument()->SetModifiedFlag(TRUE);
	Invalidate();
}

// Special -> Check Symbol References: report symbols that have no reference
// (still "U?" or empty) and references used by more than one symbol, over
// every sheet of the design. Read only - nothing is changed.
namespace
{
	struct RefUse
	{
		CString ref; // as shown, including any part letter (U1A)
		std::vector<CString> where; // sheet name of each use
	};

	void AddLimited(CString &list, int &shown, const CString &line)
	{
		const int maxLines = 25;
		if (shown < maxLines)
		{
			list += line + _T("\n");
		}
		++shown;
	}
}

void CConCadView::OnSpecialCheckRefs()
{
	CMultiSheetDoc *pDesign = GetDocument();
	if (pDesign == NULL)
	{
		return;
	}

	SetCursor(AfxGetApp()->LoadStandardCursor(IDC_WAIT));

	CString unassigned;
	int unassignedCount = 0;
	int symbolCount = 0;
	std::map<CString, RefUse> uses; // key: upper-case reference

	for (int sheet = 0; sheet < pDesign->GetNumberOfSheets(); ++sheet)
	{
		CConCadDoc *pSheet = pDesign->GetSheet(sheet);
		const CString sheetName = pDesign->GetSheetName(sheet);

		for (drawingIterator it = pSheet->GetDrawingBegin(); it != pSheet->GetDrawingEnd(); ++it)
		{
			ObjType type = (*it)->GetType();
			if (type != xMethodEx3 && type != xHierarchicalSymbol)
			{
				continue;
			}
			CDrawMethod *pMethod = static_cast<CDrawMethod *>(*it);
			++symbolCount;

			CString ref = pMethod->GetRef();
			ref.Trim();
			if (ref.IsEmpty() || !pMethod->HasRef())
			{
				CString line;
				line.Format(_T("  %s  (%s, sheet \"%s\")"), ref.IsEmpty() ? _T("<empty>") : (LPCTSTR)ref,
					(LPCTSTR)pMethod->GetName(), (LPCTSTR)sheetName);
				AddLimited(unassigned, unassignedCount, line);
				continue;
			}

			// Parts of one package share the reference: U1A and U1B are
			// different, two U1A are a duplicate.
			CDesignFileSymbol *pSymbol = pMethod->GetSymbolData();
			if (pSymbol != NULL && pSymbol->ppp > 1)
			{
				ref += (TCHAR) ('A' + pMethod->GetSubPart());
			}

			CString key = ref;
			key.MakeUpper();
			RefUse &use = uses[key];
			if (use.where.empty())
			{
				use.ref = ref;
			}
			use.where.push_back(sheetName);
		}
	}

	CString duplicates;
	int duplicateCount = 0;
	for (std::map<CString, RefUse>::const_iterator it = uses.begin(); it != uses.end(); ++it)
	{
		const RefUse &use = it->second;
		if (use.where.size() < 2)
		{
			continue;
		}
		CString line;
		line.Format(_T("  %s  used %d times (sheet "), (LPCTSTR)use.ref, (int)use.where.size());
		for (size_t i = 0; i < use.where.size(); ++i)
		{
			if (i > 0)
			{
				line += _T(", ");
			}
			line += _T("\"") + use.where[i] + _T("\"");
		}
		line += _T(")");
		AddLimited(duplicates, duplicateCount, line);
	}

	SetCursor(AfxGetApp()->LoadStandardCursor(IDC_ARROW));

	CString msg;
	if (unassignedCount == 0 && duplicateCount == 0)
	{
		msg.Format(_T("Symbol references OK.\n\nAll %d symbols have a reference and no reference is used twice."), symbolCount);
		AfxMessageBox(msg, MB_ICONINFORMATION);
		return;
	}

	if (unassignedCount > 0)
	{
		CString head;
		head.Format(_T("%d symbol(s) without a reference:\n"), unassignedCount);
		msg += head + unassigned;
		if (unassignedCount > 25)
		{
			head.Format(_T("  ... and %d more\n"), unassignedCount - 25);
			msg += head;
		}
	}
	if (duplicateCount > 0)
	{
		if (!msg.IsEmpty())
		{
			msg += _T("\n");
		}
		CString head;
		head.Format(_T("%d duplicate reference(s):\n"), duplicateCount);
		msg += head + duplicates;
		if (duplicateCount > 25)
		{
			head.Format(_T("  ... and %d more\n"), duplicateCount - 25);
			msg += head;
		}
	}
	msg += _T("\nUse Special -> Generate Symbol References to fix them.");
	AfxMessageBox(msg, MB_ICONEXCLAMATION);
}

// The Annotate Setup values class
AnnotateSetup::AnnotateSetup()
{
	action = ADD;
	reference = ALL;
	value = DEFAULT;
	matchval = "U?";
	startval = 1;
	all_sheets = false;
	sheet_prefix = false;
}

////// The Parts List (Bill of Materials) special function //////


void CConCadView::OnSpecialBom()
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
		_tcscpy_s(szFile, _T("output.txt"));
	}
	else
	{
#ifdef USE_VS2003
		_tcscpy(ext, _T(".txt"));
#else
		size_t remaining_space = &szFile[255] - ext + 1;
		_tcscpy_s(ext, remaining_space, _T(".txt"));
#endif
	}

	// Get the file name for the parts list
	CDlgBOMExport dlg(this);
	dlg.m_Filename = szFile;

	if (dlg.DoModal() != IDOK)
	{
		return;
	}

	FILE *theFile;
	errno_t err;

	err = _tfopen_s(&theFile, dlg.m_Filename, _T("w"));
	if ( (theFile == NULL) || (err != 0))
	{
		Message(IDS_CANNOTOPEN);
		return;
	}

	// Set the Busy icon
	SetCursor(AfxGetApp()->LoadStandardCursor(IDC_WAIT));

	CBOMGenerator bom;
	bom.GenerateBomForDesign(dlg.m_All_Sheets != 0, dlg.m_All_Attrs != 0, dlg.m_Prefix != 0, dlg.m_Hierarchical != 0, GetDocument(), dlg.m_type);

	// Now generate the output file
	if (dlg.m_type == 2)
	{
		bom.WriteToXls(theFile);
	}
	else
	{
		bom.WriteToFile(theFile, dlg.m_type == 1);
	}

	fclose(theFile);

	// Restore the normal cursor
	SetCursor(AfxGetApp()->LoadStandardCursor(IDC_ARROW));

	// Where there any errors?
	if (bom.GetMissingRef()) Message(IDS_MISSREF);

	CConCadApp::EditTextFile(dlg.m_Filename);
}

