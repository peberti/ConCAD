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
#include "resource.h"
#include "OptionsSheets.h"
#include "OptionsPropertySheet.h"
#include "ConCadDoc.h"
#include "Registry.h"
#include "ConCadRegistry.h"
#include "LibraryCollection.h"
#include "AutoSave.h"
#include "UserColor.h"
#include <afxdlgs.h>

IMPLEMENT_DYNCREATE( COptionsGrid, CPropertyPage )
IMPLEMENT_DYNCREATE( COptionsAutosnap, CPropertyPage )
IMPLEMENT_DYNCREATE( COptionsAutoSave, CPropertyPage )
IMPLEMENT_DYNCREATE( COptionsDrawing, CPropertyPage )

/////////////////////////////////////////////////////////////////////////////
// COptionsGrid property page

COptionsGrid::COptionsGrid() :
	CPropertyPage(COptionsGrid::IDD)
{
	//{{AFX_DATA_INIT(COptionsGrid)
	// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

COptionsGrid::~COptionsGrid()
{
}

void COptionsGrid::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptionsGrid)
	// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(COptionsGrid, CPropertyPage)
	//{{AFX_MSG_MAP(COptionsGrid)
	ON_BN_CLICKED(GRIDBOX_MM, OnMm)
	ON_BN_CLICKED(GRIDBOX_INCHES, OnInches)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CConCadDoc *COptionsGrid::GetDocument()
{
	return static_cast<COptionsPropertySheet*> (GetParent())->m_pDocument;
}

BOOL COptionsGrid::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	TransformSnap* pSnap = & (GetDocument()->m_snap);

	Grid = pSnap->GetAccurateGrid();
	SnapGrid = pSnap->GetGridSnap();
	ShowGrid = GetDocument()->GetOptions()->ShowGrid();
	Units = GetDocument()->GetOptions()->GetUnits();

	CheckDlgButton(GRIDBOX_SHOW, ShowGrid);
	CheckDlgButton(GRIDBOX_SNAP, SnapGrid);

	CheckRadioButton(GRIDBOX_MM, GRIDBOX_INCHES, GRIDBOX_MM + Units);
	if (Grid == NormalGrid) CheckRadioButton(GRIDBOX_NORMAL, GRIDBOX_USER, GRIDBOX_NORMAL);
	else if (Grid == FineGrid) CheckRadioButton(GRIDBOX_NORMAL, GRIDBOX_USER, GRIDBOX_FINE);
	else CheckRadioButton(GRIDBOX_NORMAL, GRIDBOX_USER, GRIDBOX_USER);

	// Set up the display of the grid spacing correctly
	OnChange();

	return TRUE; // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void COptionsGrid::OnMm()
{
	OnChange();
}

void COptionsGrid::OnInches()
{
	OnChange();
}

void COptionsGrid::OnChange()
{
	// The current grid in .1mm's
	double ScaledGrid;

	if (GetCheckedRadioButton(GRIDBOX_MM, GRIDBOX_INCHES) == GRIDBOX_MM)
	{
		ScaledGrid = Grid / static_cast<double> (PIXELSPERMM);
		SetDlgItemText(GRIDBOX_UNITS, _T(".1mm"));
		Units = 0;
	}
	else
	{
		SetDlgItemText(GRIDBOX_UNITS, _T(".001\""));
		ScaledGrid = (1000.0 * Grid) / (254.0 * PIXELSPERMM);
		Units = 1;
	}

	CString s;
	s.Format(_T("%lg"), ScaledGrid);
	SetDlgItemText(GRIDBOX_SPACING, s);
}

BOOL COptionsGrid::OnApply()
{
	ShowGrid = IsDlgButtonChecked(GRIDBOX_SHOW);
	SnapGrid = IsDlgButtonChecked(GRIDBOX_SNAP);

	int hold = GetCheckedRadioButton(GRIDBOX_MM, GRIDBOX_INCHES);
	if (hold == GRIDBOX_MM) Units = 0;
	else Units = 1;

	hold = GetCheckedRadioButton(GRIDBOX_NORMAL, GRIDBOX_USER);
	switch (hold)
	{
		case GRIDBOX_NORMAL:
			Grid = NormalGrid;
			break;
		case GRIDBOX_FINE:
			Grid = FineGrid;
			break;
		case GRIDBOX_USER:
		{
			CString s;
			GetDlgItemText(GRIDBOX_SPACING, s);
			Grid = _tstof(s);
			if (Units == 0) Grid = (Grid * static_cast<double> (PIXELSPERMM));
			else Grid = (Grid * 254.0 * PIXELSPERMM) / 1000.0;
			break;
		}
	}

	if (Grid <= 0)
	{
		Grid = FineGrid;
	}

	// Write these settings back
	TransformSnap* pSnap = & (GetDocument()->m_snap);

	pSnap->SetAccurateGrid(Grid);
	pSnap->SetGridSnap(SnapGrid);

	GetDocument()->GetOptions()->SetGridShow(ShowGrid == TRUE);
	GetDocument()->GetOptions()->SetUnits(Units == TRUE);

	// Write the settings to the registry
	CRegistry::Set("ShowGrid", ShowGrid);
	CRegistry::Set("Units", Units);
	CRegistry::Set("Grid", Grid);

	return CPropertyPage::OnApply();
}
//-------------------------------------------------------------------------

//=========================================================================
//== COptionsAutosnap property page

//-------------------------------------------------------------------------
COptionsAutosnap::COptionsAutosnap() :
	CPropertyPage(COptionsAutosnap::IDD)
{
	//{{AFX_DATA_INIT(COptionsAutosnap)
	m_Autodrag_Enable = FALSE;
	m_Autosnap_Enable = FALSE;
	m_Autosnap_Range = 0;
	m_Units = "";
	m_Autojunc_Enable = FALSE;
	//}}AFX_DATA_INIT
}

COptionsAutosnap::~COptionsAutosnap()
{
}

void COptionsAutosnap::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptionsAutosnap)
	DDX_Control(pDX, IDC_AUTOSNAP_RANGE, m_Range_Ctrl);
	DDX_Check(pDX, IDC_AUTODRAG_ENABLE, m_Autodrag_Enable);
	DDX_Check(pDX, IDC_AUTOSNAP_ENABLE, m_Autosnap_Enable);
	DDX_Text(pDX, IDC_AUTOSNAP_RANGE, m_Autosnap_Range);
	DDX_Text(pDX, IDC_AUTOSNAP_UNITS, m_Units);
	DDX_Check(pDX, IDC_AUTOJUNC_ENABLE, m_Autojunc_Enable);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(COptionsAutosnap, CPropertyPage)
	//{{AFX_MSG_MAP(COptionsAutosnap)
	ON_BN_CLICKED(IDC_AUTOSNAP_ENABLE, OnAutosnapEnable)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CConCadDoc *COptionsAutosnap::GetDocument()
{
	return static_cast<COptionsPropertySheet*> (GetParent())->m_pDocument;
}

BOOL COptionsAutosnap::OnInitDialog()
{
	// Read the active state from the other dialogue
	// Create the setup
	Units = static_cast<COptionsPropertySheet*> (GetParent())->m_Page1.Units;

	m_Autodrag_Enable = GetDocument()->GetOptions()->GetAutoDrag();
	m_Autosnap_Enable = GetDocument()->GetOptions()->GetAutoSnap();
	m_Autojunc_Enable = GetDocument()->GetOptions()->GetAutoJunc();

	m_Autosnap_Range = GetDocument()->GetOptions()->GetAutoSnapRange();
	m_Units = "pixels";

	// Now create the document
	CPropertyPage::OnInitDialog();

	m_Range_Ctrl.EnableWindow(m_Autosnap_Enable);

	return TRUE; // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void COptionsAutosnap::OnAutosnapEnable()
{
	// Enable disable/the range dialogue
	UpdateData(TRUE);
	m_Range_Ctrl.EnableWindow(m_Autosnap_Enable);

}
//-------------------------------------------------------------------------
BOOL COptionsAutosnap::OnApply()
{
	UpdateData(true);

	GetDocument()->GetOptions()->SetAutoDrag(m_Autodrag_Enable == TRUE);
	GetDocument()->GetOptions()->SetAutoSnap(m_Autosnap_Enable == TRUE);
	GetDocument()->GetOptions()->SetAutoJunc(m_Autojunc_Enable == TRUE);
	GetDocument()->GetOptions()->SetAutoSnapRange(m_Autosnap_Range);

	// Write the settings to the registry
	CRegistry::Set("AutoDrag", m_Autodrag_Enable);
	CRegistry::Set("AutoSnap", m_Autosnap_Enable);
	CRegistry::Set("AutoJunc", m_Autojunc_Enable);
	CRegistry::Set("AutoDragRange", m_Autosnap_Range);

	return CPropertyPage::OnApply();
}
//-------------------------------------------------------------------------
BOOL COptionsAutosnap::OnSetActive()
{
	// Read the active state from the other dialogue
	// Create the setup
	//int new_Units = static_cast<COptionsPropertySheet*>(GetParent())->m_Page1.Units;

	//if (new_Units != Units)
	//{
	//	// Get the old scaling
	//	UpdateData( TRUE );

	//	Units = new_Units;
	//	m_Units = "pixels";

	//	UpdateData( FALSE );
	//}

	return CPropertyPage::OnSetActive();
}
//=========================================================================
BEGIN_MESSAGE_MAP(COptionsAutoSave, CPropertyPage)
	//{{AFX_MSG_MAP(COptionsAutoSave)
	ON_BN_CLICKED(IDC_AUTOSAVE_ENABLE, OnChange )
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

//-------------------------------------------------------------------------
COptionsAutoSave::COptionsAutoSave() :
	super(COptionsAutoSave::IDD)
{
}
//-------------------------------------------------------------------------
BOOL COptionsAutoSave::OnInitDialog()
{
	super::OnInitDialog();

	// 10 minutes as default
	m_nDelay = CAutoSave::GetDelay();
	m_bIsEnabled = CAutoSave::IsEnabled();
	m_spnDelay = (CSpinButtonCtrl*) GetDlgItem(IDC_SPIN);
	m_btnSave = (CButton*) GetDlgItem(IDC_AUTOSAVE_ENABLE);
	m_wndBuddy = (CEdit*) GetDlgItem(IDC_AUTOSAVE_BUDDY);

	m_btnSave->SetCheck(m_bIsEnabled);

	// valid time range from 1..60 minutes
	m_spnDelay->SetBuddy(m_wndBuddy);
	m_spnDelay->SetRange32(1, 60);
	m_spnDelay->SetPos(m_nDelay < 1 ? 10 : m_nDelay);

	OnChange();

	return TRUE;
}
//-------------------------------------------------------------------------
void COptionsAutoSave::OnChange()
{
	m_bIsEnabled = (m_btnSave->GetCheck() == 1);
	m_nDelay = m_spnDelay->GetPos();

	m_spnDelay->EnableWindow(m_bIsEnabled);
	m_wndBuddy->EnableWindow(m_bIsEnabled);
}
//-------------------------------------------------------------------------
BOOL COptionsAutoSave::OnApply()
{
	OnChange();

	CAutoSave::SetDelay(m_nDelay);
	CAutoSave::SetEnabled(static_cast<boolean>(m_bIsEnabled));
	CAutoSave::Stop();
	CAutoSave::Start();

	return super::OnApply();
}
//-------------------------------------------------------------------------
//=========================================================================
//== COptionsDrawing property page
//=========================================================================

COptionsDrawing::COptionsDrawing() :
	CPropertyPage(COptionsDrawing::IDD)
{
	m_wireWidth   = 1;
	m_cableWidth  = 3;
	m_noteFill    = TRUE;
	m_noteBorder  = 1;
	m_noteFillColour = RGB(255, 255, 175);
	m_noteLineColour = RGB(0, 0, 255);
	ZeroMemory(&m_compFont, sizeof(m_compFont));
	m_compSize = 100;
	m_askTitleBlock = TRUE;
}

COptionsDrawing::~COptionsDrawing()
{
}

CConCadDoc *COptionsDrawing::GetDocument()
{
	return static_cast<COptionsPropertySheet*> (GetParent())->m_pDocument;
}

void COptionsDrawing::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_OPT_WIRE_WIDTH, m_wireWidth);
	DDV_MinMaxInt(pDX, m_wireWidth, 1, 20);
	DDX_Text(pDX, IDC_OPT_CABLE_WIDTH, m_cableWidth);
	DDV_MinMaxInt(pDX, m_cableWidth, 1, 20);
	DDX_Text(pDX, IDC_OPT_COMP_SIZE, m_compSize);
	DDV_MinMaxInt(pDX, m_compSize, 25, 400);
	DDX_Check(pDX, IDC_OPT_NOTE_FILL, m_noteFill);
	if (pDX->m_bSaveAndValidate) // the list is filled after the first exchange (OnInitDialog)
	{
		DDX_CBIndex(pDX, IDC_OPT_NOTE_BORDER, m_noteBorder);
	}
	DDX_Check(pDX, IDC_OPT_ASK_TITLEBLOCK, m_askTitleBlock);
}

BEGIN_MESSAGE_MAP(COptionsDrawing, CPropertyPage)
	ON_BN_CLICKED(IDC_OPT_COMP_FONT, OnCompFont)
	ON_BN_CLICKED(IDC_OPT_NOTE_FILL, OnNoteFill)
	ON_BN_CLICKED(IDC_OPT_NOTE_FILL_COLOUR, OnNoteFillColour)
	ON_BN_CLICKED(IDC_OPT_NOTE_LINE_COLOUR, OnNoteLineColour)
	ON_CBN_SELCHANGE(IDC_OPT_NOTE_BORDER, OnNoteBorder)
	ON_WM_DRAWITEM()
END_MESSAGE_MAP()

BOOL COptionsDrawing::OnInitDialog()
{
	COption* pOpt = GetDocument()->GetOptions();
	m_wireWidth   = pOpt->GetWireWidth();
	m_cableWidth  = pOpt->GetCableWidth();
	m_noteFill    = pOpt->GetNoteDefaultFill();
	m_noteBorder  = pOpt->GetNoteDefaultBorder();
	m_noteFillColour = pOpt->GetUserColor().Get(CUserColor::NOTETEXT_FILL);
	m_noteLineColour = pOpt->GetUserColor().Get(CUserColor::NOTETEXT_LINE);
	m_compFont    = *pOpt->GetComponentLabelFont();
	m_compSize    = COption::GetComponentLabelSize();
	m_askTitleBlock = CConCadRegistry::GetAskNewTitleBlock();

	CPropertyPage::OnInitDialog();   // pushes the members into the controls

	CComboBox* pBorder = static_cast<CComboBox*>(GetDlgItem(IDC_OPT_NOTE_BORDER));
	pBorder->AddString(_T("Rectangle"));          // the order of CDrawNoteText::BorderStyle
	pBorder->AddString(_T("Rounded rectangle"));
	pBorder->AddString(_T("No border"));
	pBorder->SetCurSel(m_noteBorder);
	GetDlgItem(IDC_OPT_NOTE_FILL_COLOUR)->EnableWindow(m_noteFill);
	GetDlgItem(IDC_OPT_NOTE_LINE_COLOUR)->EnableWindow(m_noteBorder != 2);

	UpdateFontLabel();

	return TRUE;
}

void COptionsDrawing::UpdateFontLabel()
{
	CString s = m_compFont.lfFaceName;
	if (s.IsEmpty())
	{
		s = _T("(default)");
	}
	else
	{
		if (m_compFont.lfWeight >= FW_BOLD) s += _T(" Bold");
		if (m_compFont.lfItalic)            s += _T(" Italic");
	}
	SetDlgItemText(IDC_OPT_COMP_FONT_NAME, s);
}

void COptionsDrawing::OnNoteFill()
{
	GetDlgItem(IDC_OPT_NOTE_FILL_COLOUR)->EnableWindow(IsDlgButtonChecked(IDC_OPT_NOTE_FILL) == BST_CHECKED);
}

// The border colour only matters with a border
void COptionsDrawing::OnNoteBorder()
{
	CComboBox* pBorder = static_cast<CComboBox*>(GetDlgItem(IDC_OPT_NOTE_BORDER));
	GetDlgItem(IDC_OPT_NOTE_LINE_COLOUR)->EnableWindow(pBorder->GetCurSel() != 2);
}

bool COptionsDrawing::PickColour(COLORREF& c)
{
	static COLORREF custom[16] = { 0 };
	CHOOSECOLOR cc;
	memset(&cc, 0, sizeof(cc));
	cc.lStructSize = sizeof(cc);
	cc.hwndOwner = m_hWnd;
	cc.rgbResult = c;
	cc.Flags = CC_ANYCOLOR | CC_RGBINIT | CC_FULLOPEN;
	cc.lpCustColors = custom;
	if (!ChooseColor(&cc))
	{
		return false;
	}
	c = cc.rgbResult;
	SetModified();
	return true;
}

void COptionsDrawing::OnNoteFillColour()
{
	if (PickColour(m_noteFillColour))
	{
		GetDlgItem(IDC_OPT_NOTE_FILL_COLOUR)->Invalidate();
	}
}

void COptionsDrawing::OnNoteLineColour()
{
	if (PickColour(m_noteLineColour))
	{
		GetDlgItem(IDC_OPT_NOTE_LINE_COLOUR)->Invalidate();
	}
}

// The colour buttons show their colour (greyed while not used)
void COptionsDrawing::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	if (nIDCtl != IDC_OPT_NOTE_FILL_COLOUR && nIDCtl != IDC_OPT_NOTE_LINE_COLOUR)
	{
		CPropertyPage::OnDrawItem(nIDCtl, lpDrawItemStruct);
		return;
	}
	const COLORREF colour = nIDCtl == IDC_OPT_NOTE_FILL_COLOUR ? m_noteFillColour : m_noteLineColour;
	const bool enabled = (lpDrawItemStruct->itemState & ODS_DISABLED) == 0;
	CDC dc;
	dc.Attach(lpDrawItemStruct->hDC);
	CBrush brush(enabled ? colour : ::GetSysColor(COLOR_BTNFACE));
	CPen pen(PS_SOLID, 1, enabled ? RGB(0, 0, 0) : ::GetSysColor(COLOR_GRAYTEXT));
	CBrush* oldBrush = dc.SelectObject(&brush);
	CPen* oldPen = dc.SelectObject(&pen);
	dc.Rectangle(&lpDrawItemStruct->rcItem);
	if ((lpDrawItemStruct->itemState & ODS_FOCUS) != 0)
	{
		CRect r = lpDrawItemStruct->rcItem;
		r.DeflateRect(3, 3);
		dc.DrawFocusRect(&r);
	}
	dc.SelectObject(oldPen);
	dc.SelectObject(oldBrush);
	dc.Detach();
}

void COptionsDrawing::OnCompFont()
{
	CFontDialog dlg(&m_compFont, CF_SCREENFONTS | CF_NOSIZESEL, NULL, this);
	if (dlg.DoModal() == IDOK)
	{
		dlg.GetCurrentFont(&m_compFont);
		UpdateFontLabel();
	}
}

BOOL COptionsDrawing::OnApply()
{
	if (!UpdateData(TRUE))   // validate widths
	{
		return FALSE;
	}

	COption* pOpt = GetDocument()->GetOptions();
	pOpt->SetWireWidth(m_wireWidth);
	pOpt->SetCableWidth(m_cableWidth);
	pOpt->SetNoteDefaultFill(m_noteFill);
	pOpt->SetNoteDefaultBorder(m_noteBorder);
	if (pOpt->GetUserColor().Get(CUserColor::NOTETEXT_FILL) != m_noteFillColour
		|| pOpt->GetUserColor().Get(CUserColor::NOTETEXT_LINE) != m_noteLineColour)
	{
		// The same colours as Options -> Colours -> Note fill / Note line
		pOpt->GetUserColor().Set(CUserColor::NOTETEXT_FILL, m_noteFillColour);
		pOpt->GetUserColor().Set(CUserColor::NOTETEXT_LINE, m_noteLineColour);
		pOpt->GetUserColor().WriteRegistry();
	}

	pOpt->SetComponentLabelFont(m_compFont);
	COption::SetComponentLabelSize(m_compSize);

	CConCadRegistry::SetAskNewTitleBlock(m_askTitleBlock == TRUE);

	// Widths / label font are applied live, so refresh the drawing.
	// (Wire, cable, Consat and Factory colours: Options > Colours.)
	GetDocument()->Invalidate();
	// The component label font and size apply to every open design
	AfxGetMainWnd()->RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);

	return CPropertyPage::OnApply();
}
//-------------------------------------------------------------------------
//=========================================================================
