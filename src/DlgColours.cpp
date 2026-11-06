/*
 * Project:		TinyCAD program for schematic capture
 *				https://www.tinycad.net
 * Copyright:	© 1994-2019 Matt Pyne
 * License:		Lesser GNU Public License 2.1 (LGPL)
 *				http://www.opensource.org/licenses/lgpl-license.html
 */

#include "stdafx.h"
#include "DlgColours.h"
#include "concad.h"
#include "assert.h"
#include "ConCadRegistry.h"

BEGIN_MESSAGE_MAP(CDlgColours, CDialog)
//{{AFX_MSG_MAP(CDlgColours)
	ON_BN_CLICKED(IDC_COL_BUS, OnColBus)
	ON_BN_CLICKED(IDC_COL_JUNCTION, OnColJunction)
	ON_BN_CLICKED(IDC_COL_NOCONNECT, OnColNoconnect)
	ON_BN_CLICKED(IDC_COL_PIN, OnColPin)
	ON_BN_CLICKED(IDC_COL_POWER, OnColPower)
	ON_BN_CLICKED(IDC_COL_WIRE, OnColWire)
	ON_BN_CLICKED(IDC_COL_CABLE, OnColCable)
	ON_BN_CLICKED(IDC_COL_CONSAT, OnColConsat)
	ON_BN_CLICKED(IDC_COL_FACTORY, OnColFactory)
	ON_BN_CLICKED(IDC_COL_BACKGROUND, OnColBackground )
	ON_BN_CLICKED(IDC_COL_NOTETEXT_FILL, OnColNoteTextFill )
	ON_BN_CLICKED(IDC_COL_NOTETEXT_LINE, OnColNoteTextLine )
	ON_BN_CLICKED(IDC_COL_NOTETEXT_TEXT, OnColNoteTextText )
	ON_WM_DRAWITEM()
	ON_BN_CLICKED(IDC_RESET, OnReset)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

//=========================================================================
//== ctor/dtor/initializing                                              ==
//=========================================================================

//-------------------------------------------------------------------------
CDlgColours::CDlgColours( CUserColor& oColor )
: super( CDlgColours::IDD, NULL ), m_oColorRef ( oColor )
{
	m_oColor = CUserColor( m_oColorRef );

	// Sets all custom colors to black
	memset( m_oaCustomColors, 0, sizeof(m_oaCustomColors) );

	m_id_map[ IDC_COL_BUS ] = CUserColor::BUS;
	m_id_map[ IDC_COL_JUNCTION ] = CUserColor::JUNCTION;
	m_id_map[ IDC_COL_NOCONNECT ] = CUserColor::NOCONNECT;
	m_id_map[ IDC_COL_PIN ] = CUserColor::PIN;
	m_id_map[ IDC_COL_POWER ] = CUserColor::POWER;
	m_id_map[ IDC_COL_WIRE ] = CUserColor::WIRE;
	m_id_map[ IDC_COL_CABLE ] = CUserColor::CABLE;

	m_consatColor  = CConCadRegistry::GetConsatColor();
	m_factoryColor = CConCadRegistry::GetFactoryColor();
	m_id_map[ IDC_COL_BACKGROUND ] = CUserColor::BACKGROUND;
	m_id_map[ IDC_COL_NOTETEXT_FILL ] = CUserColor::NOTETEXT_FILL;
	m_id_map[ IDC_COL_NOTETEXT_LINE ] = CUserColor::NOTETEXT_LINE;
	m_id_map[ IDC_COL_NOTETEXT_TEXT ] = CUserColor::NOTETEXT_TEXT;
}
//-------------------------------------------------------------------------

//=========================================================================
//== Message handling                                                    ==
//=========================================================================

//-------------------------------------------------------------------------
void CDlgColours::OnColBackground()
{
	EditColor(CUserColor::BACKGROUND);
}
//-------------------------------------------------------------------------
void CDlgColours::OnColBus()
{
	EditColor(CUserColor::BUS);
}
//-------------------------------------------------------------------------
void CDlgColours::OnColJunction()
{
	EditColor(CUserColor::JUNCTION);
}
//-------------------------------------------------------------------------
void CDlgColours::OnColNoconnect()
{
	EditColor(CUserColor::NOCONNECT);
}
//-------------------------------------------------------------------------
void CDlgColours::OnColPin()
{
	EditColor(CUserColor::PIN);
}
//-------------------------------------------------------------------------
void CDlgColours::OnColPower()
{
	EditColor(CUserColor::POWER);
}
//-------------------------------------------------------------------------
void CDlgColours::OnColWire()
{
	EditColor(CUserColor::WIRE);
}
//-------------------------------------------------------------------------
void CDlgColours::OnColCable()
{
	EditColor(CUserColor::CABLE);
}
//-------------------------------------------------------------------------
void CDlgColours::OnColConsat()
{
	PickColor(m_consatColor);
}
//-------------------------------------------------------------------------
void CDlgColours::OnColFactory()
{
	PickColor(m_factoryColor);
}
//-------------------------------------------------------------------------
void CDlgColours::PickColor(COLORREF& c)
{
	CHOOSECOLOR oDlg;
	memset(&oDlg, 0, sizeof (oDlg));
	oDlg.lStructSize = sizeof (oDlg);
	oDlg.hwndOwner = m_hWnd;
	oDlg.rgbResult = c;
	oDlg.Flags = CC_ANYCOLOR | CC_RGBINIT;
	oDlg.lpCustColors = m_oaCustomColors;
	if (ChooseColor(&oDlg))
	{
		c = oDlg.rgbResult;
	}
	RedrawWindow();
}
//-------------------------------------------------------------------------
void CDlgColours::OnColNoteTextFill()
{
	EditColor(CUserColor::NOTETEXT_FILL);
}
//-------------------------------------------------------------------------
void CDlgColours::OnColNoteTextLine()
{
	EditColor(CUserColor::NOTETEXT_LINE);
}
//-------------------------------------------------------------------------
void CDlgColours::OnColNoteTextText()
{
	EditColor(CUserColor::NOTETEXT_TEXT);
}
//-------------------------------------------------------------------------
void CDlgColours::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	COLORREF crButton = nIDCtl == IDC_COL_CONSAT ? m_consatColor
		: nIDCtl == IDC_COL_FACTORY ? m_factoryColor
		: m_oColor.Get(m_id_map[nIDCtl]);
	CDC dc;
	CBrush brush(crButton);

	dc.Attach(lpDrawItemStruct->hDC);
	dc.SelectStockObject(BLACK_PEN);
	dc.SelectObject(&brush);
	dc.Rectangle(&lpDrawItemStruct->rcItem);
	dc.Detach();

	super::OnDrawItem(nIDCtl, lpDrawItemStruct);
}
//-------------------------------------------------------------------------
//-- Bring up the colour dialogue
void CDlgColours::EditColor(int nID)
{
	CHOOSECOLOR oDlg;

	memset(&oDlg, 0, sizeof (oDlg));

	oDlg.lStructSize = sizeof (oDlg);
	oDlg.hwndOwner = m_hWnd;
	oDlg.rgbResult = m_oColor.Get(nID);
	oDlg.Flags = CC_ANYCOLOR | CC_RGBINIT;
	oDlg.lpCustColors = m_oaCustomColors;

	ChooseColor(&oDlg);

	m_oColor.Set(nID, oDlg.rgbResult);

	RedrawWindow();
}
//-------------------------------------------------------------------------
void CDlgColours::OnOK()
{
	m_oColorRef = CUserColor(m_oColor);
	m_oColorRef.WriteRegistry();
	CConCadRegistry::SetConsatColor(m_consatColor);
	CConCadRegistry::SetFactoryColor(m_factoryColor);

	EndDialog(IDOK);
}
//-------------------------------------------------------------------------
void CDlgColours::OnReset()
{
	m_oColor.Init();
	m_consatColor  = RGB(0, 0, 255);
	m_factoryColor = RGB(255, 0, 0);
	RedrawWindow();
}
//-------------------------------------------------------------------------
