/*
 TinyCAD program for schematic capture
 Copyright 1994/1995/2002 Matt Pyne.

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

class CConCadDoc;

class AnnotateSetup
{
public:
	enum AddOrRemove
	{
		ADD, 
		REMOVE
	};
	
	enum WhichReferences
	{
		ALL, 
		UNNUMBERED, 
		MATCHING
	};
	
	enum StartValue
	{
		DEFAULT, 
		SPECIFIED
	};

	enum AddOrRemove action;
	enum WhichReferences reference;
	enum StartValue value;

	CString matchval;
	int startval;
	bool all_sheets;
	bool sheet_prefix;   // number per sheet after the sheet number: P1-1, P1-2 on sheet 1

	AnnotateSetup();
};

// The current Generate Symbol References settings (also used by the
// reference painter)
extern AnnotateSetup theASetup;

// Sheet number prefix of the references on a sheet: "1-" on the first
// sheet (Generate Symbol References -> Sheet number prefix); empty when
// that option is off
CString AnnotatePagePrefix(CConCadDoc *pSheet);

// The dialog for the auto annotate function
class CDlgAnnotateBox: public CDialog
{

protected:
	void ReadData();
	void OnChangeStart();
	virtual void OnOK();
	virtual void OnRefPainter();
	virtual BOOL OnInitDialog();

public:
	AnnotateSetup v;

	CDlgAnnotateBox(CWnd *pParentWnd, AnnotateSetup NewV) :
		CDialog(IDD_ANNOTATE, pParentWnd)
	{
		v = NewV;
	}

	DECLARE_MESSAGE_MAP()
};

