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
#include "ConCad.h"
#include "SpecialPrintDlg.h"
#include "ConCadDoc.h"
#include "ConCadView.h"
#include "OptionsPropertySheet.h"
#include "DlgExportPng.h"
#include "DlgColours.h"
#include "MainFrm.h"
#include "ConCadRegistry.h"
#include "ShortcutsDlg.h"
#include "DrawModuleInfo.h"
#include "diag.h"
#include "EditToolbar.h"
#include "DlgPositionBox.h"
#include "DlgUpdateBox.h"
#include "LibraryCollection.h"
#include "StreamMemory.h"
#include ".\ConCadView.h"

#include <winspool.h>

extern CDlgERCListBox theERCListBox;

CConCadView* g_currentview = NULL;

/////////////////////////////////////////////////////////////////////////////
// CConCadView
IMPLEMENT_DYNCREATE(CConCadView, CFolderView)

BEGIN_MESSAGE_MAP(CConCadView, CFolderView)
	//{{AFX_MSG_MAP(CConCadView)
	ON_UPDATE_COMMAND_UI(IDM_EDITEDIT, OnUpdateEditedit)
	ON_UPDATE_COMMAND_UI(IDM_BUSBACK, OnUpdateBusback)
	ON_UPDATE_COMMAND_UI(IDM_BUSSLASH, OnUpdateBusslash)
	ON_UPDATE_COMMAND_UI(IDM_TOOLARC, OnUpdateToolarc)
	ON_UPDATE_COMMAND_UI(IDM_TOOLBUS, OnUpdateToolbus)
	ON_UPDATE_COMMAND_UI(IDM_TOOLBUSNAME, OnUpdateToolbusname)
	ON_UPDATE_COMMAND_UI(IDM_TOOLCIRCLE, OnUpdateToolcircle)
	ON_UPDATE_COMMAND_UI(IDM_TOOLCONNECT, OnUpdateToolconnect)
	ON_UPDATE_COMMAND_UI(IDM_TOOLORIGIN, OnUpdateToolorigin)
	ON_UPDATE_COMMAND_UI(IDM_TOOLGET, OnUpdateToolget)
	ON_UPDATE_COMMAND_UI(IDM_TOOLJUNC, OnUpdateTooljunc)
	ON_UPDATE_COMMAND_UI(IDM_TOOLLABEL, OnUpdateToollabel)
	ON_UPDATE_COMMAND_UI(IDM_TOOLHIERARCHICAL, OnUpdateToolHierarchical)
	ON_UPDATE_COMMAND_UI(IDM_TOOLPOLYGON, OnUpdateToolpolygon)
	ON_UPDATE_COMMAND_UI(IDM_TOOLPOWER, OnUpdateToolpower)
	ON_UPDATE_COMMAND_UI(IDM_TOOLSQUARE, OnUpdateToolsquare)
	ON_UPDATE_COMMAND_UI(IDM_TOOLNOTETEXT, OnUpdateNoteTextText)
	ON_UPDATE_COMMAND_UI(IDM_TOOLTEXT, OnUpdateTooltext)
	ON_UPDATE_COMMAND_UI(IDM_TOOLWIRE, OnUpdateToolwire)
	ON_UPDATE_COMMAND_UI(IDM_VIEWCENTRE, OnUpdateViewcentre)
	ON_WM_LBUTTONUP()
	ON_WM_SETCURSOR()
	ON_WM_SIZE()
	ON_UPDATE_COMMAND_UI(IDM_EDITDRAG, OnUpdateEditdrag)
	ON_UPDATE_COMMAND_UI(IDM_EDITDUP, OnUpdateEditdup)
	ON_UPDATE_COMMAND_UI(IDM_EDITROTATE, OnUpdateEditrotate)
	ON_UPDATE_COMMAND_UI(IDM_EDITMOVE, OnUpdateEditmove)
	ON_COMMAND(IDM_SNAPTOGRID, OnSnaptogrid)
	ON_UPDATE_COMMAND_UI(IDM_SNAPTOGRID, OnUpdateSnaptogrid)
	ON_COMMAND(IDM_TOGGLE_GRIDSIZE, OnToggleGridSize)
	//ON_UPDATE_COMMAND_UI(IDM_TOGGLE_GRIDSIZE, OnUpdateGridSize)
	ON_UPDATE_COMMAND_UI(POSITIONBOX_GRIDSIZE, OnUpdateGridSize)
	ON_UPDATE_COMMAND_UI(IDM_REPEATNAMEDOWN, OnUpdateRepeatnamedown)
	ON_UPDATE_COMMAND_UI(IDM_REPEATNAMEUP, OnUpdateRepeatnameup)
	ON_UPDATE_COMMAND_UI(IDM_REPEATPINDOWN, OnUpdateRepeatpindown)
	ON_UPDATE_COMMAND_UI(IDM_REPEATPINUP, OnUpdateRepeatpinup)
	ON_UPDATE_COMMAND_UI(IDM_EDITPASTE, OnUpdateEditpaste)
	ON_UPDATE_COMMAND_UI(IDM_EDITCUT, OnUpdateEditcut)
	ON_UPDATE_COMMAND_UI(IDM_EDITCOPY, OnUpdateEditcopy)
	ON_UPDATE_COMMAND_UI(IDM_EDITDELITEM, OnUpdateEditDelete)
	ON_UPDATE_COMMAND_UI(IDM_EDITSELECTALL, OnUpdateEditSelectAll)
	ON_UPDATE_COMMAND_UI(IDM_EDITROTATELEFT, OnUpdateEditRotateLRF)
	ON_UPDATE_COMMAND_UI(IDM_EDITROTATERIGHT, OnUpdateEditRotateLRF)
	ON_UPDATE_COMMAND_UI(IDM_EDITFLIP, OnUpdateEditRotateLRF)
	ON_WM_DESTROY()
	ON_WM_MOUSEWHEEL()
	ON_COMMAND(ID_RULER_VERT, OnRulerVert)
	ON_UPDATE_COMMAND_UI(ID_RULER_VERT, OnUpdateRulerVert)
	ON_COMMAND(ID_RULER_HORIZ, OnRulerHoriz)
	ON_UPDATE_COMMAND_UI(ID_RULER_HORIZ, OnUpdateRulerHoriz)
	ON_WM_LBUTTONDBLCLK()
	ON_COMMAND(IDM_VIEW_OPTIONS, OnViewOptions)
	ON_COMMAND(ID_CONTEXT_ARCIN, OnContextArcin)
	ON_COMMAND(ID_CONTEXT_ARCOUT, OnContextArcout)
	ON_COMMAND(ID_CONTEXT_CURVE, OnContextCurve)
	ON_COMMAND(ID_CONTEXT_CANCELDRAWING, OnContextCanceldrawing)
	ON_COMMAND(ID_CONTEXT_FINISHDRAWING, OnContextFinishdrawing)
	ON_COMMAND(ID_CONTEXT_FREELINE, OnContextFreeline)
	ON_COMMAND(ID_CONTEXT_ADDHANDLE, OnContextAddhandle)
	ON_COMMAND(ID_CONTEXT_DELETEHANDLE, OnContextDeletehandle)
	ON_COMMAND(ID_CONTEXT_ZORDER_BRINGTOFRONT, OnContextZorderBringtofront)
	ON_COMMAND(ID_CONTEXT_ZORDER_SENDTOBACK, OnContextZorderSendtoback)
	ON_UPDATE_COMMAND_UI(ID_CONTEXT_ZORDER_BRINGTOFRONT, OnUpdateContextZorderBringtofront)
	ON_UPDATE_COMMAND_UI(ID_CONTEXT_ZORDER_SENDTOBACK, OnUpdateContextZorderSendtoback)
	ON_UPDATE_COMMAND_UI(IDM_EDITDUPLICATE, OnUpdateEditduplicate)
	ON_COMMAND(ID_SPECIAL_CREATESPICEFILE, OnSpecialCreatespicefile)
	ON_COMMAND(ID_SPECIAL_VHDL, OnSpecialVHDL)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPYTO, OnUpdateEditCopyto)
	ON_COMMAND(ID_EDIT_COPYTO, OnEditCopyto)
	ON_COMMAND(ID_CONTEXT_MAKEHORIZONTAL, OnContextMakehorizontal)
	ON_COMMAND(ID_CONTEXT_MAKEVERTICAL, OnContextMakevertical)
	ON_COMMAND(ID_FILE_SAVEASBITMAP, OnFileSaveasbitmap)
	ON_COMMAND(IDM_SPECIAL_CREATEMODULE, OnSpecialCreateModule)
	ON_UPDATE_COMMAND_UI(IDM_SPECIAL_CREATEMODULE, OnUpdateEditcopy)
	ON_COMMAND(IDM_MODULE_EDIT, OnModuleEdit)
	ON_COMMAND(IDM_MODULE_UNGROUP, OnModuleUngroup)
	ON_COMMAND(IDM_MODULE_FINISHEDIT, OnModuleFinishEdit)
	ON_COMMAND(IDM_OBJECT_GROUP, OnObjectGroup)
	ON_UPDATE_COMMAND_UI(IDM_OBJECT_GROUP, OnUpdateObjectGroup)
	ON_UPDATE_COMMAND_UI(IDM_MODULE_EDIT, OnUpdateModuleGroupSelected)
	ON_UPDATE_COMMAND_UI(IDM_MODULE_UNGROUP, OnUpdateModuleGroupSelected)
	ON_UPDATE_COMMAND_UI(IDM_MODULE_FINISHEDIT, OnUpdateModuleFinishEdit)
	ON_COMMAND(IDM_COLOR_CONSAT, OnColorConsat)
	ON_COMMAND(IDM_COLOR_FACTORY, OnColorFactory)
	ON_COMMAND(IDM_COLOR_CUSTOM, OnColorCustom)
	ON_COMMAND(IDM_COLOR_DEFAULT, OnColorDefault)
	ON_UPDATE_COMMAND_UI(IDM_COLOR_CONSAT, OnUpdateColor)
	ON_UPDATE_COMMAND_UI(IDM_COLOR_FACTORY, OnUpdateColor)
	ON_UPDATE_COMMAND_UI(IDM_COLOR_CUSTOM, OnUpdateColor)
	ON_UPDATE_COMMAND_UI(IDM_COLOR_DEFAULT, OnUpdateColor)
	ON_COMMAND(IDM_OPTIONS_SHORTCUTS, OnOptionsShortcuts)
	ON_COMMAND(IDM_EDIT_REVISIONHISTORY, OnEditRevisionHistory)
	ON_UPDATE_COMMAND_UI(IDM_EDIT_REVISIONHISTORY, OnUpdateEditRevisionHistory)
	ON_COMMAND(ID_FILE_EXPORTPDF, OnFileExportpdf)
	ON_COMMAND(ID_OPTIONS_COLOURS, OnOptionsColours)
	ON_COMMAND(ID_CONTEXT_REPLACESYMBOL, OnContextReplacesymbol)
	ON_COMMAND(ID_EDIT_INSERTPICTURE, OnEditInsertpicture)
	//}}AFX_MSG_MAP
	// Standard printing commands
	ON_COMMAND(ID_FILE_PRINT, CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_DIRECT, CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_PREVIEW, CView::OnFilePrintPreview)

	ON_WM_INITMENUPOPUP()
	ON_WM_COMPACTING()
	ON_WM_DESTROYCLIPBOARD()
	ON_WM_CREATE()
	ON_WM_DESTROY()

	ON_WM_LBUTTONDOWN()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_MBUTTONDOWN()
	ON_WM_MBUTTONUP()
	ON_WM_MOUSEMOVE()

	ON_WM_SYSKEYDOWN()
	ON_WM_SYSKEYUP()
	
	ON_WM_KEYDOWN()

	ON_WM_CLOSE()
	ON_WM_VSCROLL()
	ON_WM_HSCROLL()
	ON_WM_SIZE()

	// The File Menu
	ON_COMMAND( IDM_FILEDESIGN, OnFileDesign )
	ON_COMMAND( IDM_FILEIMPORT, OnFileImport )
	ON_COMMAND( ID_FILE_PRINT, OnFilePrint )
	ON_COMMAND( IDM_FILEPAGESET, OnFilePageSet )

	// The Edit Menu
	ON_COMMAND( IDM_EDITUNDO, OnEditUndo )
	ON_COMMAND( IDM_EDITREDO, OnEditRedo )
	ON_COMMAND( IDM_EDITEDIT, OnEditEdit )
	ON_COMMAND( IDM_EDITDELITEM, OnEditDelete )
	ON_COMMAND( IDM_EDITDRAG, OnEditDrag )
	ON_COMMAND( IDM_EDITMOVE, OnEditMove )
	ON_COMMAND( IDM_EDITDUP, OnEditDup )
	ON_COMMAND( IDM_EDITROTATE, OnEditRotate )
	ON_COMMAND( ID_FIND_FIND, OnFindFind )
	ON_COMMAND( IDM_EDITCOPY, OnEditCopy )
	ON_COMMAND( IDM_EDITCUT, OnEditCut )
	ON_COMMAND( IDM_EDITPASTE, OnEditPaste )
	ON_COMMAND( IDM_EDITSELECTALL, OnEditSelectAll )
	ON_COMMAND( IDM_EDITDUPLICATE, OnEditDuplicate )
	ON_COMMAND( IDM_EDITROTATELEFT, OnEditRotateLeft )
	ON_COMMAND( IDM_EDITROTATERIGHT, OnEditRotateRight )
	ON_COMMAND( IDM_EDITFLIP, OnEditFlip )


	// The Keyboard options
	ON_COMMAND( IDM_VIEWZOOMIN, OnViewZoomIn )
	ON_COMMAND( IDM_VIEWZOOMOUT, OnViewZoomOut )
	
	// The Toolbar Menu
	ON_COMMAND( IDM_VIEWCENTRE, OnViewCentre )
	ON_COMMAND( IDM_TOOLBUSNAME, OnSelectBusName )
	ON_COMMAND( IDM_TOOLBUS, OnSelectBus )
	ON_COMMAND( IDM_BUSSLASH, OnSelectBusSlash )
	ON_COMMAND( IDM_BUSBACK, OnSelectBusBack )
	ON_COMMAND( IDM_TOOLCONNECT, OnSelectConnect )
	ON_COMMAND( IDM_TOOLORIGIN, OnSelectOrigin )
	ON_COMMAND( IDM_TOOLLABEL, OnSelectLabel )
	ON_COMMAND( IDM_TOOLHIERARCHICAL, OnSelectHierarchical )
	ON_COMMAND( IDM_TOOLPOWER, OnSelectPower )
	ON_COMMAND( IDM_TOOLPOLYGON, OnSelectPolygon )
	ON_COMMAND( IDM_TOOLWIRE, OnSelectWire )
	ON_COMMAND( IDM_TOOLCABLE, OnSelectCable )
	ON_COMMAND( IDM_TOOLJUNC, OnSelectJunction )
	ON_COMMAND( IDM_TOOLARC, OnSelectArc )
	ON_COMMAND( IDM_TOOLSQUARE, OnSelectSquare )
	ON_COMMAND( IDM_TOOLCIRCLE, OnSelectCircle )
	ON_COMMAND( IDM_TOOLNOTETEXT, OnSelectNoteText )
	ON_COMMAND( IDM_TOOLTEXT, OnSelectText )
	ON_COMMAND( IDM_TOOLGET, OnSelectGet )
	ON_COMMAND( IDC_SHOW_SYMBOL, OnSelectGet )

	// The Special Menu
	ON_COMMAND( IDM_SPECIALANNOTATE, OnSpecialAnnotate )
	ON_COMMAND( IDM_SPECIALBOM, OnSpecialBom )
	ON_COMMAND( IDM_SPECIALNET, OnSpecialNet )
	ON_COMMAND( IDM_SPECIALCHECK, OnSpecialCheck )
	ON_COMMAND( IDM_SPECIALVHDLCHECK, OnSpecialVHDLCheck)

	// The Repeat Menu
	ON_COMMAND( IDM_REPEATNAMEUP, OnRepeatNameUp )
	ON_COMMAND( IDM_REPEATNAMEDOWN, OnRepeatNameDown )
	ON_COMMAND( IDM_REPEATPINUP, OnRepeatPinUp )
	ON_COMMAND( IDM_REPEATPINDOWN, OnRepeatPinDown )

	ON_COMMAND(ID_CONTEXT_OPENDESIGN, OnContextOpendesign)
	ON_COMMAND(ID_CONTEXT_RELOADSYMBOLFROMDESIGN, OnContextReloadsymbolfromdesign)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CConCadView construction/destruction

CConCadView::CConCadView()
{
	vRuler = NULL;
	hRuler = NULL;

	// We have not captured the mouse
	m_captured = 0;
	m_panning = 0;

	// Turn on off-screen bitmap drawing
	m_use_offscreen_drawing = TRUE;

	m_PrintAllSheets = TRUE;
	m_Printing = FALSE;
}

CConCadView::~CConCadView()
{
	g_currentview = NULL;
}

BOOL CConCadView::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	// Create our own Window's class for this window
	static CString theClass = AfxRegisterWndClass(0, AfxGetApp()->LoadStandardCursor(IDC_ARROW));
	cs.lpszClass = theClass;

	return CView::PreCreateWindow(cs);
}

/////////////////////////////////////////////////////////////////////////////
// CConCadView printing

// Our own version of this function, so that we
// can set portrait/landscape mode automatically
BOOL CConCadView::DoPreparePrinting(CPrintInfo* pInfo)
{
	ASSERT(pInfo != NULL);
	ASSERT(pInfo->m_pPD != NULL);

	if (pInfo->m_pPD->m_pd.nMinPage > pInfo->m_pPD->m_pd.nMaxPage) pInfo->m_pPD->m_pd.nMaxPage = pInfo->m_pPD->m_pd.nMinPage;

	// don't prompt the user if we're doing print preview, printing directly,
	// or printing via IPrint and have been instructed not to ask

	CConCadApp* pApp = static_cast<CConCadApp*> (AfxGetApp());
	if (pInfo->m_bPreview || pInfo->m_bDirect || (pInfo->m_bDocObject && ! (pInfo->m_dwFlags & PRINTFLAG_PROMPTUSER)))
	{
		if (pInfo->m_pPD->m_pd.hDC == NULL)
		{
			// if no printer set then, get default printer DC and create DC without calling
			//   print dialog.
			if (!pApp->GetPrinterDeviceDefaults(&pInfo->m_pPD->m_pd))
			{
				// bring up dialog to alert the user they need to install a printer.
				if (!pInfo->m_bDocObject || (pInfo->m_dwFlags & PRINTFLAG_MAYBOTHERUSER)) if (pApp->DoPrintDialog(pInfo->m_pPD) != IDOK) return FALSE;
			}

			if (pInfo->m_pPD->m_pd.hDC == NULL)
			{
				// Switch to landscape mode...
				DEVMODE *pdev_mode;
				pdev_mode = pInfo->m_pPD->GetDevMode();
				pdev_mode->dmOrientation = GetCurrentDocument()->GetDetails().IsPortrait() ? DMORIENT_PORTRAIT : DMORIENT_LANDSCAPE;
				GlobalUnlock(pdev_mode);

				// call CreatePrinterDC if DC was not created by above
				if (pInfo->m_pPD->CreatePrinterDC() == NULL) return FALSE;
			}
		}

		// set up From and To page range from Min and Max
		pInfo->m_pPD->m_pd.nFromPage = (WORD) pInfo->GetMinPage();
		pInfo->m_pPD->m_pd.nToPage = (WORD) pInfo->GetMaxPage();
	}
	else
	{
		// Get the print scaling to determine the correct "fit to page"
		// scaling factor
		CPrintDialog pdlg(FALSE, 0, AfxGetMainWnd());
		HDC hdc = NULL;

		if (AfxGetApp()->GetPrinterDeviceDefaults(&pdlg.m_pd))
		{
			if (pdlg.m_pd.hDC == NULL)
			{
				// call CreatePrinterDC if DC was not created by above
				hdc = pdlg.CreatePrinterDC();
			}
		}

		int width = 1024;
		int height = 768;
		if (hdc)
		{
			// Change the scaling to print correct size
			width = max( ::GetDeviceCaps(hdc, HORZSIZE), ::GetDeviceCaps(hdc, VERTSIZE) );
			height = min( ::GetDeviceCaps(hdc, HORZSIZE), ::GetDeviceCaps(hdc, VERTSIZE) );
		}

		::DeleteDC(hdc);

		// otherwise, bring up the print dialog and allow user to change things
		// preset From-To range same as Min-Max range
		delete pInfo->m_pPD;
		CSpecialPrintDlg *dlg = new CSpecialPrintDlg(FALSE);

		if (GetCurrentDocument()->GetDetails().IsPortrait())
		{
			dlg->m_FitScale = min(
					static_cast<double>(width * 100 * PIXELSPERMM) / static_cast<double>(GetCurrentDocument()->GetDetails().GetPageBoundsAsPoint().y),
					static_cast<double>(height * 100 * PIXELSPERMM) / static_cast<double>(GetCurrentDocument()->GetDetails().GetPageBoundsAsPoint().x) );
		}
		else
		{
			dlg->m_FitScale = min(
					static_cast<double>(width * 100 * PIXELSPERMM) / static_cast<double>(GetCurrentDocument()->GetDetails().GetPageBoundsAsPoint().x),
					static_cast<double>(height * 100 * PIXELSPERMM) / static_cast<double>(GetCurrentDocument()->GetDetails().GetPageBoundsAsPoint().y) );
		}
		pInfo->m_pPD = dlg;
		pInfo->m_pPD->m_pd.nFromPage = (WORD) pInfo->GetMinPage();
		pInfo->m_pPD->m_pd.nToPage = 0xfff; // (WORD)pInfo->GetMaxPage();
		pInfo->m_pPD->m_pd.Flags &= ~PD_RETURNDC;

		if (pApp->DoPrintDialog(pInfo->m_pPD) != IDOK) return FALSE; // do not print

		m_PrintAllSheets = dlg->m_print_all_sheets != 0;

		// Switch to landscape mode...
		DEVMODE *pdev_mode;
		pdev_mode = dlg->GetDevMode();
		pdev_mode->dmOrientation = GetCurrentDocument()->GetDetails().IsPortrait() ? DMORIENT_PORTRAIT : DMORIENT_LANDSCAPE;
		pdev_mode->dmCopies = static_cast<short>(dlg->m_Copies);
		GlobalUnlock(pdev_mode);

		// call CreatePrinterDC if DC was not created by above
		if (pInfo->m_pPD->CreatePrinterDC() == NULL) return FALSE;

	}

	ASSERT(pInfo->m_pPD != NULL);
	ASSERT(pInfo->m_pPD->m_pd.hDC != NULL);
	if (pInfo->m_pPD->m_pd.hDC == NULL) return FALSE;

	pInfo->m_nNumPreviewPages = pApp->m_nNumPreviewPages;
	VERIFY(pInfo->m_strPageDesc.LoadString(AFX_IDS_PREVIEWPAGEDESC));
	return TRUE;
}

BOOL CConCadView::OnPreparePrinting(CPrintInfo* pInfo)
{

	// Get rid of any drawing tool
	GetCurrentDocument()->SelectObject(new CDrawEditItem(GetCurrentDocument()));

	// Prepare using our version of this function to force
	// portrait/landscape mode
	return DoPreparePrinting(pInfo);
}

void CConCadView::OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo)
{
	// Calculate how many cols and rows of pages to use
	double width;
	double height;

	width = max( pDC->GetDeviceCaps(HORZSIZE), pDC->GetDeviceCaps(VERTSIZE) ) * PIXELSPERMM;
	height = min( pDC->GetDeviceCaps(HORZSIZE), pDC->GetDeviceCaps(VERTSIZE) ) * PIXELSPERMM;

	double scale = CConCadRegistry::GetPrintScale();
	double scale_x, scale_y;

	if (GetCurrentDocument()->GetDetails().IsPortrait())
	{
		scale_x = (scale * GetCurrentDocument()->GetDetails().GetPageBoundsAsPoint().x / height) / 100.0;
		scale_y = (scale * GetCurrentDocument()->GetDetails().GetPageBoundsAsPoint().y / width) / 100.0;
	}
	else
	{
		scale_x = (scale * GetCurrentDocument()->GetDetails().GetPageBoundsAsPoint().x / width) / 100.0;
		scale_y = (scale * GetCurrentDocument()->GetDetails().GetPageBoundsAsPoint().y / height) / 100.0;
	}

	BOOL Fit = FALSE;

	if (Fit)
	{
		m_cols = 1;
		m_rows = 1;
	}
	else
	{
		// Round to the nearest scaling factor...
		m_cols = static_cast<int> (scale_x);
		m_rows = static_cast<int> (scale_y);

		double sx = scale_x - static_cast<int> (scale_x);
		double sy = scale_y - static_cast<int> (scale_y);

		// Is this an exact fit or not?
		if (sx > 0.01)
		{
			m_cols++;
		}

		if (sy > 0.01)
		{
			m_rows++;
		}
	}

	int pages = 1;
	if (m_PrintAllSheets)
	{
		pages = m_cols * m_rows * GetDocument()->GetNumberOfSheets();
	}
	else
	{
		pages = m_cols * m_rows;
	}

	pInfo->SetMinPage(1);
	pInfo->SetMaxPage(pages);

	// Change the zoom so a single unit on the design corresponds to a pixel on the printer
	m_Printing = TRUE;
	current_Sheet = GetDocument()->GetActiveSheetIndex();
	double NewZoom = (pDC->GetDeviceCaps(LOGPIXELSX) * 1000) / (PIXELSPERMM * 254);

	if (Fit)
	{
		NewZoom = NewZoom / max(scale_x,scale_y);
	}
	else
	{
		NewZoom = (NewZoom * scale) / 100.0;
	}

	GetTransform().SetZoomFactor(NewZoom / 100.0);
}

void CConCadView::OnPrepareDC(CDC* pDC, CPrintInfo* pInfo)
{

	if (pInfo)
	{
		// Change the offset co-ordinates to print the correct portion of the design
		int width = pDC->GetDeviceCaps(HORZSIZE) * PIXELSPERMM;
		int height = pDC->GetDeviceCaps(VERTSIZE) * PIXELSPERMM;

		int col = 0;
		int row = 0;

		if (m_PrintAllSheets)
		{
			int sheets = GetDocument()->GetNumberOfSheets();
			int page = (pInfo->m_nCurPage - 1) / sheets;
			int sheet = (pInfo->m_nCurPage - 1) % sheets;
			GetDocument()->SetActiveSheetIndex(sheet);
			col = page / m_rows;
			row = page % m_rows;
		}
		else
		{
			col = (pInfo->m_nCurPage - 1) / m_rows;
			row = (pInfo->m_nCurPage - 1) % m_rows;
		}

		double scale = CConCadRegistry::GetPrintScale();
		int x = static_cast<int> ( (col * width / scale) * 100);
		int y = static_cast<int> ( (row * height / scale) * 100);
		GetTransform().SetOriginX(x);
		GetTransform().SetOriginY(y);
	}

	CView::OnPrepareDC(pDC, pInfo);
}

void CConCadView::OnEndPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
	GetDocument()->SetActiveSheetIndex(current_Sheet);
	m_Printing = FALSE;
	m_PrintAllSheets = TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CConCadView diagnostics

#ifdef _DEBUG
void CConCadView::AssertValid() const
{
	CView::AssertValid();
}

void CConCadView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}
#endif //_DEBUG

CConCadDoc* CConCadView::GetCurrentDocument() // non-debug version is inline
{
	return GetDocument()->GetCurrentSheet();
}

/////////////////////////////////////////////////////////////////////////////
// CConCadView message handlers

class CDlgPositionBox;

/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////


// The status bar indicators
static UINT BASED_CODE indicators[] =
{
	ID_SEPARATOR,			// status line indicator
	ID_INDICATOR_CAPS,
	ID_INDICATOR_NUM,
};

// The OnCreate function, called when the window is created
int CConCadView::OnCreate(LPCREATESTRUCT q)
{
	CView::OnCreate(q);

	BOOL originButton = FALSE;
	if (GetCurrentDocument())
	{
		originButton = GetCurrentDocument()->IsEditLibrary();
	}

	// Now create the new rulers
	CRect nSize;
	GetClientRect(nSize);
	vRuler = new Ruler(GetDocument(), 0, nSize, this, originButton);
	hRuler = new Ruler(GetDocument(), 2, nSize, this, originButton);

	ClipboardFormat = RegisterClipboardFormat(CLIPBOARD_FORMAT);

	// initialize the cursor array
	HINSTANCE hInst = AfxGetResourceHandle();
	m_mouse_pointers[0] = ::LoadCursor(hInst, MAKEINTRESOURCE(AFX_IDC_TRACKNWSE));
	m_mouse_pointers[1] = ::LoadCursor(hInst, MAKEINTRESOURCE(AFX_IDC_TRACKNESW));
	m_mouse_pointers[2] = m_mouse_pointers[0];
	m_mouse_pointers[3] = m_mouse_pointers[1];
	m_mouse_pointers[4] = ::LoadCursor(hInst, MAKEINTRESOURCE(AFX_IDC_TRACKNS));
	m_mouse_pointers[5] = ::LoadCursor(hInst, MAKEINTRESOURCE(AFX_IDC_TRACKWE));
	m_mouse_pointers[6] = m_mouse_pointers[4];
	m_mouse_pointers[7] = m_mouse_pointers[5];
	m_mouse_pointers[8] = ::LoadCursor(hInst, MAKEINTRESOURCE(AFX_IDC_TRACK_MOVE));
	m_mouse_pointers[9] = ::LoadCursor(hInst, MAKEINTRESOURCE(IDC_ZOOMCURSOR));
	m_mouse_pointers[10] = ::LoadCursor(hInst, MAKEINTRESOURCE(IDC_REFCURSOR));
	m_mouse_pointers[11] = ::LoadCursor(hInst, MAKEINTRESOURCE(AFX_IDC_TRACK4WAY));
	m_mouse_pointers[12] = ::LoadCursor(hInst, MAKEINTRESOURCE(AFX_IDC_TRACK_BLOCK));

	return 0;
}

// CConCadView constructor:
// Create the window with the appropriate style, size, menu, etc.
//


// Called when the window is about to be destroyed
void CConCadView::OnDestroy()
{
	// Close the ERC list box
	theERCListBox.Close();

	vRuler->DestroyWindow();
	delete vRuler;

	hRuler->DestroyWindow();
	delete hRuler;
}

// Display a standard message using one of the resource strings
int Message(int Resource, int Type, const TCHAR *NameString)
{
	TCHAR String[STRLEN], buffer[STRLEN];
	int r;

	if (LoadString(AfxGetInstanceHandle(), Resource, String, 1024) == 0) r = AfxMessageBox(_T("Could not find specified resource!  There is a fault in the file ConCAD.EXE, please re-install it."), MB_ICONEXCLAMATION | MB_OK);
	else
	{
		// Play the sound associated with this message
		MessageBeep(Type & 0x70);
		_stprintf_s(buffer, String, NameString);
		r = AfxMessageBox(buffer, Type);
	}

	return r;
}

////// The mouse movement operators //////

void CConCadView::OnMouseMove(UINT nFlags, CPoint p)
{

	CContext theContext(this, GetTransform());

	if (vRuler != NULL) vRuler->ShowPosition(p);
	if (hRuler != NULL) hRuler->ShowPosition(p);

	if (!m_pDocument) return;

	CDPoint snap_p = GetTransform().DeScale(GetCurrentDocument()->m_snap, p);
	CDPoint no_snap_p = GetTransform().DeScale(p);

	// Wait for threshold before panning gets active?
	if (m_panning == 2)
	{
		// Is threshold reached?
		if (abs(StartPosition.x - p.x) > 3 || abs(StartPosition.y - p.y) > 3)
		{
			// Panning is active now
			m_panning = 1;
		}
	}

	// Wait for threshold before mouse move gets active?
	if (m_captured == 2)
	{
		// Is threshold reached?
		if (abs(StartPosition.x - p.x) > 1 || abs(StartPosition.y - p.y) > 1)
		{
			// Mouse move is active now
			m_captured = 1;
		}
	}

	// If we try to pan, then don't track this movement...
	if (m_panning != 0)
	{
		// panning active?
		if (m_panning == 1)
		{
			// Pan the screen with the mouse
			CDPoint d = GetTransform().GetOrigin() - no_snap_p + MousePosition;
			SetScroll(d.x, d.y);
		}
		MousePosition = GetTransform().DeScale(p);
	}
	else
	{
		MousePosition = no_snap_p;

		// Now display the position
		CString pos = GetCurrentDocument()->GetOptions()->PointToUnit(snap_p - GetCurrentDocument()->GetOptions()->GetOrigin());
		static_cast<CMainFrame*> (AfxGetMainWnd())->setPositionText(pos);

		// Not when threshold not reached
		if (m_captured != 2)
		{
			// Send this to the editable device
			if (GetCurrentDocument()->GetEdit())
			{
				GetCurrentDocument()->GetEdit()->Move(snap_p, no_snap_p);
			}
		}
	}
}

void CConCadView::OnLButtonDown(UINT nFlags, CPoint p)
{
	// No selecting, placing or dragging on a write-protected version.
	if (IsWriteProtected())
	{
		return;
	}

	CContext theContext(this, GetTransform());

	CDPoint snapped_p = GetTransform().DeScale(GetCurrentDocument()->m_snap, p);
	CDPoint no_snap_p = GetTransform().DeScale(p);

	MousePosition = no_snap_p;

	GetCurrentDocument()->GetEdit()->LButtonDown(snapped_p, no_snap_p);

	// wait for threshold before mouse move gets active
	m_captured = 2;
	StartPosition = p;
	SetCapture();
}

void CConCadView::OnLButtonDblClk(UINT nFlags, CPoint p)
{
	if (IsWriteProtected())
	{
		AfxMessageBox(_T("This version is write-protected.\n\nUse File > Edit File to make a working copy you can change."), MB_ICONINFORMATION);
		return;
	}

	CContext theContext(this, GetTransform());

	CDPoint snapped_p = GetTransform().DeScale(GetCurrentDocument()->m_snap, p);
	CDPoint no_snap_p = GetTransform().DeScale(p);

	MousePosition = no_snap_p;

	GetCurrentDocument()->GetEdit()->DblLButtonDown(snapped_p, no_snap_p);

	CView::OnLButtonDblClk(nFlags, p);
}

void CConCadView::OnLButtonUp(UINT nFlags, CPoint p)
{
	if (m_captured != 0)
	{
		ReleaseCapture();

		CContext theContext(this, GetTransform());

		CDPoint snapped_p = GetTransform().DeScale(GetCurrentDocument()->m_snap, p);
		CDPoint no_snap_p = GetTransform().DeScale(p);

		MousePosition = no_snap_p;

		GetCurrentDocument()->GetEdit()->LButtonUp(snapped_p, no_snap_p);
	}
	m_captured = 0;
	m_panning = 0;

	CView::OnLButtonUp(nFlags, p);
}

void CConCadView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	// Check for single-key commands.
	// Don't use the Accelerator key table for this
	// because that will hog all key handling in TinyCAD.
	// By handling the single-key commands here (CConCadView::OnKeyDown)
	// they will correctly respond only when CConCadView has the keyboard focus.

	// Shift-key not pressed (=high-order bit not set)
	// Ctrl-key not pressed 
	// Alt-key not pressed 
	// 'Menu'-key not pressed
	// 'Windows'-keys not pressed 
	if (::GetKeyState(VK_SHIFT) >= 0 &&
		::GetKeyState(VK_CONTROL) >= 0 &&
		::GetKeyState(VK_MENU) >= 0 && 
		::GetKeyState(VK_APPS) >= 0 && 
		::GetKeyState(VK_LWIN) >= 0 && 
		::GetKeyState(VK_RWIN) >= 0)
	{
		// Only Find is available on a write-protected version.
		if (IsWriteProtected() && nChar != 'F')
		{
			return;
		}

		switch (nChar)
		{
			case 'R':
				OnEditRotateRight();
				break;
			case 'L':
				OnEditRotateLeft();
				break;
			case 'F':
				OnFindFind();
				break;
			case 'U':
				OnEditDuplicate();
				break;
		}
	}
}

void CConCadView::OnSysKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	// Change cursor to 'block select' cursor
	OnSetCursor(this, HTCLIENT, WM_SYSKEYDOWN);
}

void CConCadView::OnSysKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (!OnSetCursor(this, HTCLIENT, WM_SYSKEYUP))
	{
		// Change cursor back to normal
		SetCursor(AfxGetApp()->LoadStandardCursor(IDC_ARROW));
	}
}

BOOL CConCadView::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	// trackers should only be in client area
	if (nHitTest == HTCLIENT)
	{
		int cursor = -1;
		CPoint point;
		GetCursorPos(&point);
		pWnd->ScreenToClient(&point);
		CDPoint no_snap_p = GetTransform().DeScale(point);
		if (GetCurrentDocument()->GetEdit())
		{
			cursor = GetCurrentDocument()->GetEdit()->SetCursor(no_snap_p);
		}

		if (cursor != -1)
		{
			::SetCursor(m_mouse_pointers[cursor]);
			return TRUE;
		}
	}

	return CView::OnSetCursor(pWnd, nHitTest, message);
}

void CConCadView::OnRButtonDown(UINT nFlags, CPoint p)
{
	// Create a Context for this Click
	CContext theContext(this, GetTransform());

	CDPoint snapped_p = GetTransform().DeScale(GetCurrentDocument()->m_snap, p);
	CDPoint no_snap_p = GetTransform().DeScale(p);

	MousePosition = no_snap_p;

	// wait for threshold before panning gets active
	m_panning = 2;
	StartPosition = p;
	SetCapture();

	// The actual RButtonDown process will be done in the OnRButtonUp event.
	// This is because the right-mouse button is also used for panning.
}

void CConCadView::OnRButtonUp(UINT nFlags, CPoint p)
{
	CContext theContext(this, GetTransform());
	CDPoint snapped_p = GetTransform().DeScale(GetCurrentDocument()->m_snap, p);
	CDPoint no_snap_p = GetTransform().DeScale(p);

	MousePosition = no_snap_p;
	ReleaseCapture();

	// not panning and not moving?
	if (m_panning != 1 && m_captured != 1)
	{
		// Call the end function, if it returns false then delete
		// this object
		if (!GetCurrentDocument()->GetEdit()->RButtonDown(snapped_p, no_snap_p))
		{
			GetCurrentDocument()->SelectObject(new CDrawEditItem(GetCurrentDocument()));
		}
		else
		{
			// Always call RButtonUp after succesful RButtonDown
			GetCurrentDocument()->GetEdit()->RButtonUp(snapped_p, no_snap_p);
		}
	}

	// Clear panning
	m_panning = 0;
	//m_captured = 0;
}

void CConCadView::OnMButtonDown(UINT nFlags, CPoint point)
{
	// Panning is active instantly
	m_panning = 1;
	SetCapture();
}

void CConCadView::OnMButtonUp(UINT nFlags, CPoint point)
{
	// Panning is not active
	m_panning = 0;
	//m_captured = 0;
	ReleaseCapture();
}

// This function takes file names and shortens them (if necessary)
CString NameLength(const TCHAR *s, int MaxLen)
{
	CString in = s;
	int len = in.GetLength();
	int diff = MaxLen - len;

	// Is the string short enough as it is?
	if (diff >= 0) return in;

	// Otherwise remove sufficient characters so it is
	if ( (diff & 1) != 0) diff++;

	return (in.Left( (len + diff) / 2 - 2) + "..." + in.Mid( (len - diff) / 2 + 1));
}

void CConCadView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	switch (lHint)
	{
		case DOC_UPDATE_SETCURSOR:
		{
			int cursor = GetCurrentDocument()->GetEdit()->SetCursor(MousePosition);
			if (cursor != -1)
			{
				::SetCursor(m_mouse_pointers[cursor]);
			}
		}
			break;
		case DOC_UPDATE_INVALIDATE:
			Invalidate();
			break;
		case DOC_UPDATE_INVALIDRECT:
		case DOC_UPDATE_INVALIDRECTERASE:
		{
			doc_invalidrect* hint = static_cast<doc_invalidrect*> (pHint);
			CContext dc(this, GetTransform());
			dc.InvalidateRect(hint->r, lHint == DOC_UPDATE_INVALIDRECTERASE, hint->grow);
		}
			break;
		case DOC_UPDATE_TABS:
			SetTabsFromDocument();
			break;

		case DOC_UPDATE_RULERS:
			if (vRuler)
			{
				vRuler->Invalidate();
			}
			if (hRuler)
			{
				hRuler->Invalidate();
			}
			break;

	}

}

void CConCadView::OnRulerVert()
{
	GetCurrentDocument()->SelectObject(new CDrawRuler(GetCurrentDocument(), FALSE));

}

void CConCadView::OnUpdateRulerVert(CCmdUI* pCmdUI)
{
	CDrawingObject *q = GetCurrentDocument()->GetEdit();
	if (q)
	{
		pCmdUI->SetCheck(static_cast<int>(q->getMenuID() == pCmdUI->m_nID));
	}

}

void CConCadView::OnRulerHoriz()
{
	GetCurrentDocument()->SelectObject(new CDrawRuler(GetCurrentDocument(), TRUE));
}

void CConCadView::OnUpdateRulerHoriz(CCmdUI* pCmdUI)
{
	CDrawingObject *q = GetCurrentDocument()->GetEdit();
	if (q)
	{
		pCmdUI->SetCheck(static_cast<int>(q->getMenuID() == pCmdUI->m_nID));
	}
}

void CConCadView::OnUpdateEditedit(CCmdUI* pCmdUI)
{
	CDrawingObject *q = GetCurrentDocument()->GetEdit();
	if (q)
	{
		pCmdUI->SetCheck(static_cast<int>(q->getMenuID() == pCmdUI->m_nID));
	}
}

void CConCadView::OnUpdateBusback(CCmdUI* pCmdUI)
{
	CDrawingObject *q = GetCurrentDocument()->GetEdit();
	if (q)
	{
		pCmdUI->SetCheck(static_cast<int>(q->getMenuID() == pCmdUI->m_nID));
	}
}

void CConCadView::OnUpdateBusslash(CCmdUI* pCmdUI)
{
	CDrawingObject *q = GetCurrentDocument()->GetEdit();
	if (q)
	{
		pCmdUI->SetCheck(static_cast<int>(q->getMenuID() == pCmdUI->m_nID));
	}
}

void CConCadView::OnUpdateToolarc(CCmdUI* pCmdUI)
{
	CDrawingObject *q = GetCurrentDocument()->GetEdit();
	if (q)
	{
		pCmdUI->SetCheck(static_cast<int>(q->getMenuID() == pCmdUI->m_nID));
	}
}

void CConCadView::OnUpdateToolbus(CCmdUI* pCmdUI)
{
	CDrawingObject *q = GetCurrentDocument()->GetEdit();
	if (q)
	{
		pCmdUI->SetCheck(static_cast<int>(q->getMenuID() == pCmdUI->m_nID));
	}
}

void CConCadView::OnUpdateToolbusname(CCmdUI* pCmdUI)
{
	CDrawingObject *q = GetCurrentDocument()->GetEdit();
	if (q)
	{
		pCmdUI->SetCheck(static_cast<int>(q->getMenuID() == pCmdUI->m_nID));
	}
}

void CConCadView::OnUpdateToolcircle(CCmdUI* pCmdUI)
{
	CDrawingObject *q = GetCurrentDocument()->GetEdit();
	if (q)
	{
		pCmdUI->SetCheck(static_cast<int>(q->getMenuID() == pCmdUI->m_nID));
	}
}

void CConCadView::OnUpdateToolconnect(CCmdUI* pCmdUI)
{
	CDrawingObject *q = GetCurrentDocument()->GetEdit();
	if (q)
	{
		pCmdUI->SetCheck(static_cast<int>(q->getMenuID() == pCmdUI->m_nID));
	}
	pCmdUI->Enable(!GetCurrentDocument()->IsEditLibrary());
}

void CConCadView::OnUpdateToolorigin(CCmdUI* pCmdUI)
{
	CDrawingObject *q = GetCurrentDocument()->GetEdit();
	if (q)
	{
		pCmdUI->SetCheck(static_cast<int>(q->getMenuID() == pCmdUI->m_nID));
	}
	pCmdUI->Enable(GetCurrentDocument()->IsEditLibrary());
}

void CConCadView::OnUpdateToolget(CCmdUI* pCmdUI)
{
	CDrawingObject *q = GetCurrentDocument()->GetEdit();
	if (q)
	{
		pCmdUI->SetCheck(static_cast<int>(q->getMenuID() == pCmdUI->m_nID));
	}
}

void CConCadView::OnUpdateTooljunc(CCmdUI* pCmdUI)
{
	CDrawingObject *q = GetCurrentDocument()->GetEdit();
	if (q)
	{
		pCmdUI->SetCheck(static_cast<int>(q->getMenuID() == pCmdUI->m_nID));
	}
}

void CConCadView::OnUpdateToollabel(CCmdUI* pCmdUI)
{
	CDrawingObject *q = GetCurrentDocument()->GetEdit();
	if (q)
	{
		pCmdUI->SetCheck(static_cast<int>(q->getMenuID() == pCmdUI->m_nID));
	}
}

void CConCadView::OnUpdateToolHierarchical(CCmdUI* pCmdUI)
{
	CDrawingObject *q = GetCurrentDocument()->GetEdit();
	if (q)
	{
		pCmdUI->SetCheck(static_cast<int>(q->getMenuID() == pCmdUI->m_nID));
	}
	pCmdUI->Enable(!GetCurrentDocument()->IsHierarchicalSymbol());
}

void CConCadView::OnUpdateToolpolygon(CCmdUI* pCmdUI)
{
	CDrawingObject *q = GetCurrentDocument()->GetEdit();
	if (q)
	{
		pCmdUI->SetCheck(static_cast<int>(q->getMenuID() == pCmdUI->m_nID));
	}
}

void CConCadView::OnUpdateToolpower(CCmdUI* pCmdUI)
{
	CDrawingObject *q = GetCurrentDocument()->GetEdit();
	if (q)
	{
		pCmdUI->SetCheck(static_cast<int>(q->getMenuID() == pCmdUI->m_nID));
	}
}

void CConCadView::OnUpdateToolsquare(CCmdUI* pCmdUI)
{
	CDrawingObject *q = GetCurrentDocument()->GetEdit();
	if (q)
	{
		pCmdUI->SetCheck(static_cast<int>(q->getMenuID() == pCmdUI->m_nID));
	}
}

void CConCadView::OnUpdateNoteTextText(CCmdUI* pCmdUI) 
{
	CDrawingObject *q = GetCurrentDocument()->GetEdit();
	if (q)	
	{
		pCmdUI->SetCheck(static_cast<int>(q->getMenuID() == pCmdUI->m_nID));
	}
}

void CConCadView::OnUpdateTooltext(CCmdUI* pCmdUI)
{
	CDrawingObject *q = GetCurrentDocument()->GetEdit();
	if (q)
	{
		pCmdUI->SetCheck(static_cast<int>(q->getMenuID() == pCmdUI->m_nID));
	}
}

void CConCadView::OnUpdateToolwire(CCmdUI* pCmdUI)
{
	CDrawingObject *q = GetCurrentDocument()->GetEdit();
	if (q)
	{
		pCmdUI->SetCheck(static_cast<int>(q->getMenuID() == pCmdUI->m_nID));
	}
}

void CConCadView::OnUpdateViewcentre(CCmdUI* pCmdUI)
{
	CDrawingObject *q = GetCurrentDocument()->GetEdit();
	if (q)
	{
		pCmdUI->SetCheck(static_cast<int>(q->getMenuID() == pCmdUI->m_nID));
	}
}

void CConCadView::OnUpdateEditdrag(CCmdUI* pCmdUI)
{
	CDrawingObject *q = GetCurrentDocument()->GetEdit();
	if (q)
	{
		pCmdUI->SetCheck(static_cast<int>(q->getMenuID() == pCmdUI->m_nID));
	}
}

void CConCadView::OnUpdateEditdup(CCmdUI* pCmdUI)
{
	CDrawingObject *q = GetCurrentDocument()->GetEdit();
	if (q)
	{
		pCmdUI->SetCheck(static_cast<int>(q->getMenuID() == pCmdUI->m_nID));
	}

}

void CConCadView::OnUpdateEditrotate(CCmdUI* pCmdUI)
{
	CDrawingObject *q = GetCurrentDocument()->GetEdit();
	if (q)
	{
		pCmdUI->SetCheck(static_cast<int>(q->getMenuID() == pCmdUI->m_nID));
	}
}

void CConCadView::OnUpdateEditmove(CCmdUI* pCmdUI)
{
	CDrawingObject *q = GetCurrentDocument()->GetEdit();
	if (q)
	{
		pCmdUI->SetCheck(static_cast<int>(q->getMenuID() == pCmdUI->m_nID));
	}
}

void CConCadView::OnInitialUpdate()
{
	CView::OnInitialUpdate();

	// Set the offsets
	SetScroll(0, 0, true);

	SetTabsFromDocument();

	if (CConCadRegistry::GetMDIMaximize())
	{
		((CMDIChildWndEx *) GetParentFrame())->MDIMaximize();
	}

	// Center Symbol inside view
	if (GetCurrentDocument()->IsEditLibrary())
	{
		// Center drawing inside view
		drawingIterator it = GetCurrentDocument()->GetDrawingBegin();
		if (it != GetCurrentDocument()->GetDrawingEnd())
		{
			CDrawingObject *obj = *it;
			CDRect ext(obj->m_point_a.x, obj->m_point_a.y, obj->m_point_b.x, obj->m_point_b.y);
			ext.NormalizeRect();
			while (it != GetCurrentDocument()->GetDrawingEnd())
			{
				obj = (CDrawingObject *) *it;
				CDRect box(obj->m_point_a.x, obj->m_point_a.y, obj->m_point_b.x, obj->m_point_b.y);
				box.NormalizeRect();

				if (ext.left > box.left) ext.left = box.left;

				if (ext.top > box.top) ext.top = box.top;

				if (ext.right < box.right) ext.right = box.right;

				if (ext.bottom < box.bottom) ext.bottom = box.bottom;

				++it;
			}

			// Is any part of the symbol outside the page?
			if (ext.left < 0 || ext.top < 0)
			{
				// Move symbol to postion 100,100 so it can be edited
				CDPoint shift(max(0, -ext.left + 100), max(0, -ext.top + 100));
				it = GetCurrentDocument()->GetDrawingBegin();
				while (it != GetCurrentDocument()->GetDrawingEnd())
				{
					(*it)->Shift(shift);
					++it;
				}
				// Also shift the extends
				ext += shift;
			}

			SetScrollCentre(CDPoint( (ext.left + ext.right) / 2, (ext.top + ext.bottom) / 2));
		}
	}

}

void CConCadView::SetTabsFromDocument()
{
	CFolderTabCtrl& ftc = GetFolderFrame()->GetFolderTabCtrl();
	CMultiSheetDoc *pDoc = GetDocument();
	int docs = pDoc->GetNumberOfSheets();

	// Has the document been initialised yet?
	if (docs == 0)
	{
		return;
	}

	while (ftc.GetItemCount() > 0)
	{
		ftc.RemoveItem(0);
	}

	for (int i = 0; i < docs; i++)
	{
		ftc.AddItem(pDoc->GetSheetName(i));
	}

	GetFolderFrame()->ShowControls(CFolderFrame::bestFit);
	ftc.SelectItem(GetDocument()->GetActiveSheetIndex());
}

////// The Snap to Grid menu //////

void CConCadView::OnSnaptogrid()
{
	GetCurrentDocument()->SetSnapToGrid(!GetCurrentDocument()->GetSnapToGrid());
}

void CConCadView::OnUpdateSnaptogrid(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetCurrentDocument()->GetSnapToGrid() ? 1 : 0);
}

////// The Toggle Grid Size //////

void CConCadView::OnToggleGridSize()
{
	double grid = GetCurrentDocument()->m_snap.GetAccurateGrid();
	if (grid == NormalGrid)
		grid = FineGrid;
	else if (grid == FineGrid)
		grid = FineGrid / 2.0;
	else if (grid == FineGrid / 2.0)
		grid = FineGrid / 4.0;
	else if (grid == FineGrid / 4.0)
		grid = FineGrid / 10.0;
	else
		grid = NormalGrid;
	GetCurrentDocument()->m_snap.SetAccurateGrid(grid);
	RedrawWindow();
}

void CConCadView::OnUpdateGridSize(CCmdUI* pCmdUI)
{
	double grid = GetCurrentDocument()->m_snap.GetAccurateGrid();
	int units = GetCurrentDocument()->GetOptions()->GetUnits(); 
	static_cast<CMainFrame*> (AfxGetMainWnd())->setGridSize(grid, units);
	pCmdUI->Enable(FALSE);
}
////// The REPEAT menu //////

void CConCadView::OnRepeatNameUp()
{
	GetCurrentDocument()->SetNameDir(1);
}

void CConCadView::OnUpdateRepeatnameup(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetCurrentDocument()->GetNameDir() == 1 ? 1 : 0);
}

void CConCadView::OnRepeatNameDown()
{
	GetCurrentDocument()->SetNameDir(-1);
}

void CConCadView::OnUpdateRepeatnamedown(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetCurrentDocument()->GetNameDir() == -1 ? 1 : 0);
}

void CConCadView::OnRepeatPinUp()
{
	GetCurrentDocument()->SetPinDir(1);
}

void CConCadView::OnUpdateRepeatpinup(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetCurrentDocument()->GetPinDir() == 1 ? 1 : 0);
}

void CConCadView::OnRepeatPinDown()
{
	GetCurrentDocument()->SetPinDir(-1);
}

void CConCadView::OnUpdateRepeatpindown(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(GetCurrentDocument()->GetPinDir() == -1 ? 1 : 0);
}

void CConCadView::OnUpdateEditpaste(CCmdUI* pCmdUI)
{
	BOOL r = IsClipboardAvailable() 
		|| ::IsClipboardFormatAvailable( CF_ENHMETAFILE )
		|| ::IsClipboardFormatAvailable( CF_BITMAP )
		|| ::IsClipboardFormatAvailable( CF_TEXT );
	pCmdUI->Enable(r);
}

void CConCadView::OnUpdateEditcut(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(GetCurrentDocument()->IsSelected());
}

void CConCadView::OnUpdateEditcopy(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(GetCurrentDocument()->IsSelected());
}

void CConCadView::OnUpdateEditDelete(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(GetCurrentDocument()->IsSelected());
}

void CConCadView::OnUpdateEditSelectAll(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(TRUE);
}

void CConCadView::OnUpdateEditRotateLRF(CCmdUI* pCmdUI)
{
	ObjType type = GetCurrentDocument()->GetEdit()->GetType();
	BOOL r = ( type == xEditItem && GetCurrentDocument()->IsSelected())
			|| type == xMethod || type == xMethodEx || type == xMethodEx2 || type == xMethodEx3
			|| type == xAnnotation 
			|| type == xLabel || type == xLabelEx || type == xLabelEx2 
			|| type == xPower
			|| type == xText || type == xTextEx || type == xTextEx2
			|| type == xBusName || type == xBusNameEx
//			|| type == xNoteText	//not sure if this belongs here or not - djl
			|| type == xBusSlash
			;

	pCmdUI->Enable(r);
}

void CConCadView::OnUpdateEditduplicate(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(GetCurrentDocument()->IsSelected());
}

void CConCadView::OnUpdateEditCopyto(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(GetCurrentDocument()->IsSelected());
}

BOOL CConCadView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	if (zDelta > 0)
	{
		OnViewZoomIn();
	}
	else
	{
		OnViewZoomOut();
	}

	return TRUE;
}

void CConCadView::OnActivateView(BOOL bActivate, CView* pActivateView, CView* pDeactiveView)
{
	//ATLTRACE2("CConCadView::OnActivateView() - bActivate:%d, %x, %x\n", bActivate, pActivateView, g_currentview);
	if (bActivate)
	{	//Activate this view
		// When switching to a different view then
		// get rid of any drawing tool on the current view
		if (g_currentview != this && g_currentview != NULL)
		{
			g_currentview->GetCurrentDocument()->SelectObject(new CDrawEditItem(g_currentview->GetCurrentDocument()));
		}
		g_currentview = this;
	}

	if (!bActivate)
	{	//Deactivate this view
		CDrawingObject* obj;
		obj = GetCurrentDocument()->GetEdit();
		//ATLTRACE2("CConCadView::OnActivateView() - de-activating this View.  obj is %s, obj->GetType() = %d\n", 
		//	(obj ? "valid":"NULL"),
		//	(obj ? obj->GetType() : -1));

		// Don't get rid of edit tool, we want to keep the dialog open when switching applications
		if (obj && obj->GetType() != xEditItem)
		{
			// Get rid of any drawing tool at this moment
			ATLTRACE2("CConCadView::OnActivateView() - de-activating this View.  Getting rid of any active drawing tools.\n");

			GetCurrentDocument()->SelectObject(new CDrawEditItem(GetCurrentDocument()));
		}
	}

	CView::OnActivateView(bActivate, pActivateView, pDeactiveView);
}

void CConCadView::OnViewOptions()
{
	COptionsPropertySheet propSheet;

	propSheet.m_pDocument = GetCurrentDocument();
	propSheet.DoModal();

	if (hRuler != NULL) hRuler->RedrawWindow();
	if (vRuler != NULL) vRuler->RedrawWindow();

	Invalidate();
}

void CConCadView::OnEditDelete()
{
	GetCurrentDocument()->GetEdit()->ContextMenu(MousePosition, IDM_EDITDELITEM);
}

void CConCadView::OnContextMakehorizontal()
{
	GetCurrentDocument()->GetEdit()->ContextMenu(MousePosition, ID_CONTEXT_MAKEHORIZONTAL);
}

void CConCadView::OnContextMakevertical()
{
	GetCurrentDocument()->GetEdit()->ContextMenu(MousePosition, ID_CONTEXT_MAKEVERTICAL);
}

void CConCadView::OnContextArcin()
{
	GetCurrentDocument()->GetEdit()->ContextMenu(MousePosition, ID_CONTEXT_ARCIN);
}

void CConCadView::OnContextArcout()
{
	GetCurrentDocument()->GetEdit()->ContextMenu(MousePosition, ID_CONTEXT_ARCOUT);
}

void CConCadView::OnContextCurve()
{
	GetCurrentDocument()->GetEdit()->ContextMenu(MousePosition, ID_CONTEXT_CURVE);
}

void CConCadView::OnContextFreeline()
{
	GetCurrentDocument()->GetEdit()->ContextMenu(MousePosition, ID_CONTEXT_FREELINE);
}

void CConCadView::OnContextAddhandle()
{
	GetCurrentDocument()->GetEdit()->ContextMenu(MousePosition, ID_CONTEXT_ADDHANDLE);
}

void CConCadView::OnContextDeletehandle()
{
	GetCurrentDocument()->GetEdit()->ContextMenu(MousePosition, ID_CONTEXT_DELETEHANDLE);
}

void CConCadView::OnContextCanceldrawing()
{
	// switch back to the Edit tool
	GetCurrentDocument()->SelectObject(new CDrawEditItem(GetCurrentDocument()));
}

void CConCadView::OnContextFinishdrawing()
{
	GetCurrentDocument()->GetEdit()->FinishDrawing(MousePosition);
}

void CConCadView::OnContextReplacesymbol()
{
	GetCurrentDocument()->GetEdit()->ContextMenu(MousePosition, ID_CONTEXT_REPLACESYMBOL);
}

void CConCadView::OnContextOpendesign()
{
	GetCurrentDocument()->GetEdit()->ContextMenu(MousePosition, ID_CONTEXT_OPENDESIGN);
}

void CConCadView::OnContextReloadsymbolfromdesign()
{
	GetCurrentDocument()->GetEdit()->ContextMenu(MousePosition, ID_CONTEXT_RELOADSYMBOLFROMDESIGN);
}

void CConCadView::OnContextZorderBringtofront()
{
	GetCurrentDocument()->BringToFront();
}

void CConCadView::OnContextZorderSendtoback()
{
	GetCurrentDocument()->SendToBack();
}

void CConCadView::OnUpdateContextZorderBringtofront(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(GetCurrentDocument()->IsSelected());
}

void CConCadView::OnUpdateContextZorderSendtoback(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(GetCurrentDocument()->IsSelected());
}

void CConCadView::OnFileSaveasbitmap()
{

	// Get the file in which to save the network
	TCHAR szFile[256];
	szFile[0] = '\0';
	_tcscpy_s(szFile, GetDocument()->GetPathName());
	TCHAR* ext = _tcsrchr(szFile, '.');
	if (!ext)
	{
		_tcscpy_s(szFile, _T("output.png"));
	}
	else
	{
#ifdef USE_VS2003
		_tcscpy(ext, _T(".png"));
#else
		size_t remaining_space = &szFile[255] - ext + 1;
		_tcscpy_s(ext, remaining_space, _T(".png"));
#endif
	}

	CDlgExportPNG dlg;
	dlg.m_Filename = szFile;

	if (dlg.DoModal() != IDOK) return;

	CClientDC dc(this);
	switch (dlg.m_type)
	{
		case 0: // Colour PNG
			GetCurrentDocument()->SavePNG(dlg.m_Filename, dc, dlg.m_Scaling, false, dlg.m_Rotate);
			break;
		case 1: // B&W PNG
			GetCurrentDocument()->SavePNG(dlg.m_Filename, dc, dlg.m_Scaling, true, dlg.m_Rotate);
			break;
		case 2: // Colour EMF
			GetCurrentDocument()->CreateMetafile(dc, dlg.m_Filename, false);
			break;
		case 3: // B&W EMF
			GetCurrentDocument()->CreateMetafile(dc, dlg.m_Filename, true);
			break;
	}
}
// Fill in a DEVMODE so the PDF page matches this sheet's page-setup size
// (e.g. A3) and orientation.
//
// The "Microsoft Print to PDF" driver is a v4 driver that ignores custom
// dmPaperWidth/dmPaperLength values at print time and falls back to its
// default paper (Letter). Standard paper-size *codes* (DMPAPER_A3, ...) do
// round-trip reliably, so we match the page to a standard size and use its
// code wherever possible, only attempting a custom size for non-standard
// pages (e.g. A1/A0) as a best effort.
static void SetDevModePageSize(DEVMODE *pDevMode, HANDLE hPrinter, LPCTSTR printerName, CConCadDoc *pSheet)
{
	CPoint page = pSheet->GetDetails().GetPageBoundsAsPoint();

	// Page dimensions in mm (PIXELSPERMM TinyCAD units per mm)
	double wmm = static_cast<double> (page.x) / PIXELSPERMM;
	double hmm = static_cast<double> (page.y) / PIXELSPERMM;
	double shortMM = min(wmm, hmm);
	double longMM = max(wmm, hmm);

	pDevMode->dmFields |= DM_ORIENTATION;
	pDevMode->dmOrientation = (page.x > page.y) ? DMORIENT_LANDSCAPE : DMORIENT_PORTRAIT;

	// Standard sizes, in portrait terms (short edge x long edge, mm)
	struct StdSize { double s; double l; short code; };
	static const StdSize kSizes[] = {
		{105.0, 148.0, DMPAPER_A6},
		{148.0, 210.0, DMPAPER_A5},
		{210.0, 297.0, DMPAPER_A4},
		{297.0, 420.0, DMPAPER_A3},
		{420.0, 594.0, DMPAPER_A2},
		{215.9, 279.4, DMPAPER_LETTER},
		{215.9, 355.6, DMPAPER_LEGAL},
		{279.4, 431.8, DMPAPER_TABLOID},  // 11 x 17 (Ledger)
	};

	const double tol = 3.0;  // mm
	short paperCode = 0;
	for (int i = 0; i < sizeof(kSizes) / sizeof(kSizes[0]); ++i)
	{
		if (fabs(shortMM - kSizes[i].s) <= tol && fabs(longMM - kSizes[i].l) <= tol)
		{
			paperCode = kSizes[i].code;
			break;
		}
	}

	if (paperCode != 0)
	{
		// Standard size: let the driver own the exact dimensions
		pDevMode->dmFields |= DM_PAPERSIZE;
		pDevMode->dmFields &= ~(DM_PAPERWIDTH | DM_PAPERLENGTH);
		pDevMode->dmPaperSize = paperCode;

		// Validate / normalise against the driver (standard codes survive this)
		::DocumentProperties(NULL, hPrinter, (LPTSTR) printerName, pDevMode, pDevMode, DM_IN_BUFFER | DM_OUT_BUFFER);
	}
	else
	{
		// Non-standard size (e.g. A1/A0): best-effort custom paper. Dimensions
		// in portrait terms (short = width, long = length), in 0.1 mm units.
		// Per Microsoft guidance, dmPaperSize must be 0 for a custom size.
		// NB: not re-validated; the v4 driver tends to discard custom sizes
		// during validation, so this may still come out at the default size.
		pDevMode->dmFields |= DM_PAPERSIZE | DM_PAPERWIDTH | DM_PAPERLENGTH;
		pDevMode->dmPaperSize = 0;
		pDevMode->dmPaperWidth = static_cast<short> (shortMM * 10.0 + 0.5);
		pDevMode->dmPaperLength = static_cast<short> (longMM * 10.0 + 0.5);
	}
}

// Export the whole design to a PDF file, one page per sheet, using the
// built-in "Microsoft Print to PDF" printer driver (Windows 10 and later).
// Each sheet is scaled to fit a page, so no third-party PDF library is needed
// and the output stays fully vectorised (lines and text remain selectable).
void CConCadView::OnFileExportpdf()
{
	static const TCHAR *kPdfPrinter = _T("Microsoft Print to PDF");

	// Get rid of any drawing tool so a half-finished object isn't rendered
	GetCurrentDocument()->SelectObject(new CDrawEditItem(GetCurrentDocument()));

	// Build a default output filename from the design's path
	TCHAR szFile[MAX_PATH];
	szFile[0] = '\0';
	_tcscpy_s(szFile, GetDocument()->GetPathName());
	TCHAR* ext = _tcsrchr(szFile, '.');
	if (!ext)
	{
		_tcscpy_s(szFile, _T("output.pdf"));
	}
	else
	{
		size_t remaining_space = &szFile[MAX_PATH - 1] - ext + 1;
		_tcscpy_s(ext, remaining_space, _T(".pdf"));
	}

	// Let the user choose where to save the PDF
	CFileDialog dlg(FALSE, _T("pdf"), szFile,
					OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
					_T("PDF Files (*.pdf)|*.pdf|All Files (*.*)|*.*||"), this);
	if (dlg.DoModal() != IDOK)
	{
		return;
	}
	CString filename = dlg.GetPathName();

	// Make sure the "Microsoft Print to PDF" driver is available
	HANDLE hPrinter = NULL;
	if (!::OpenPrinter((LPTSTR) kPdfPrinter, &hPrinter, NULL))
	{
		AfxMessageBox(_T("The \"Microsoft Print to PDF\" printer is not available.\n")
					  _T("It is included with Windows 10 and later; it can be enabled\n")
					  _T("under \"Turn Windows features on or off\"."), MB_ICONEXCLAMATION);
		return;
	}

	// Fetch a correctly-sized DEVMODE for the driver
	LONG cbNeeded = ::DocumentProperties(NULL, hPrinter, (LPTSTR) kPdfPrinter, NULL, NULL, 0);
	if (cbNeeded <= 0)
	{
		::ClosePrinter(hPrinter);
		AfxMessageBox(_T("Could not query the PDF printer settings."), MB_ICONEXCLAMATION);
		return;
	}

	BYTE *pDevModeBuffer = new BYTE[cbNeeded];
	DEVMODE *pDevMode = reinterpret_cast<DEVMODE *> (pDevModeBuffer);
	if (::DocumentProperties(NULL, hPrinter, (LPTSTR) kPdfPrinter, pDevMode, NULL, DM_OUT_BUFFER) != IDOK)
	{
		delete[] pDevModeBuffer;
		::ClosePrinter(hPrinter);
		AfxMessageBox(_T("Could not read the PDF printer settings."), MB_ICONEXCLAMATION);
		return;
	}
	// Keep the printer handle open so DocumentProperties can validate the
	// custom page sizes we set per sheet below.

	// Size the first page to match its sheet's page setup before creating the DC
	int sheets = GetDocument()->GetNumberOfSheets();
	CConCadDoc *pFirstSheet = (sheets > 0) ? GetDocument()->GetSheet(0) : NULL;
	if (pFirstSheet != NULL)
	{
		SetDevModePageSize(pDevMode, hPrinter, kPdfPrinter, pFirstSheet);
	}

	// Create a DC bound to the PDF "printer"
	HDC hdcPdf = ::CreateDC(_T("WINSPOOL"), kPdfPrinter, NULL, pDevMode);
	if (!hdcPdf)
	{
		::ClosePrinter(hPrinter);
		delete[] pDevModeBuffer;
		AfxMessageBox(_T("Could not create the PDF output device."), MB_ICONEXCLAMATION);
		return;
	}

	// Remove any stale output so the driver writes a fresh file rather than failing
	::DeleteFile(filename);

	CDC dc;
	dc.Attach(hdcPdf);

	// Pass the output path in DOCINFO so the driver writes straight to the file
	// instead of prompting with a "Save Print Output As" dialog.
	CString docName = GetDocument()->GetTitle();
	DOCINFO di;
	memset(&di, 0, sizeof(di));
	di.cbSize = sizeof(di);
	di.lpszDocName = docName;
	di.lpszOutput = filename;

	bool ok = false;

	if (dc.StartDoc(&di) > 0)
	{
		ok = true;
		for (int sheet = 0; sheet < sheets; ++sheet)
		{
			CConCadDoc *pSheet = GetDocument()->GetSheet(sheet);
			if (pSheet == NULL)
			{
				continue;
			}

			// Size this page to match the sheet's page setup (size + orientation)
			SetDevModePageSize(pDevMode, hPrinter, kPdfPrinter, pSheet);
			dc.ResetDC(pDevMode);

			if (dc.StartPage() <= 0)
			{
				ok = false;
				break;
			}

			pSheet->SavePDFPage(dc);

			if (dc.EndPage() <= 0)
			{
				ok = false;
				break;
			}
		}

		if (ok)
		{
			dc.EndDoc();
		}
		else
		{
			dc.AbortDoc();
		}
	}

	dc.Detach();
	::DeleteDC(hdcPdf);
	::ClosePrinter(hPrinter);
	delete[] pDevModeBuffer;

	if (!ok)
	{
		AfxMessageBox(_T("Failed to write the PDF file."), MB_ICONEXCLAMATION);
	}
}
//-------------------------------------------------------------------------
void CConCadView::OnOptionsColours()
{
	CDlgColours(GetCurrentDocument()->GetOptions()->GetUserColor()).DoModal();

	RedrawWindow();
}
//-------------------------------------------------------------------------
// The user has changed the current folder
void CConCadView::OnChangedFolder(int iPage)
{
	// switch back to the Edit tool
	GetCurrentDocument()->SelectObject(new CDrawEditItem(GetCurrentDocument()));

	// Now change the active sheet
	GetDocument()->SetActiveSheetIndex(iPage);
	SetScroll(GetTransform().GetOrigin().x, GetTransform().GetOrigin().y, true);
	RedrawWindow();
}
//-------------------------------------------------------------------------

void CConCadView::OnFolderContextMenu()
{
	GetDocument()->OnFolderContextMenu();
}

#define ALL_IMAGE_FILES _T("*.png;*.emf;*.bmp;*.jpeg;*.jpe;*.jpg")
void CConCadView::OnEditInsertpicture()
{
	// switch back to the Edit tool
	GetCurrentDocument()->SelectObject(new CDrawEditItem(GetCurrentDocument()));

	CFileDialog dlg( TRUE, ALL_IMAGE_FILES, ALL_IMAGE_FILES, OFN_HIDEREADONLY,
		_T("Image files|") ALL_IMAGE_FILES _T("|")
		_T("Portable network graphic (*.png)|*.png|")
		_T("JPEG (*.jpeg)|*.jpeg;*.jpg;*.jpe|")
		_T("Windows bitmaps(*.bmp,*.dib)|*.bmp;*.dib|")
		_T("Enhanced metafile (*.emf)|*.emf|")
		_T("All files (*.*)|*.*||"), AfxGetMainWnd() ); 

	if (dlg.DoModal() != IDOK) return;

	CDrawMetaFile *pObject = new CDrawMetaFile(GetCurrentDocument());
	if (pObject->setImageFile(dlg.GetPathName()))
	{
		CClientDC dc(this);
		pObject->determineSize(dc);
		GetCurrentDocument()->AddImage(pObject);
	}
	else
	{
		delete pObject;
	}
}

void CConCadView::SelectSheet(int sheet)
{
	CFolderTabCtrl& ftc = GetFolderFrame()->GetFolderTabCtrl();
	GetDocument()->SetActiveSheetIndex(sheet);
	ftc.SelectItem(GetDocument()->GetActiveSheetIndex());
}

void CConCadView::ChangeDir(int dir)
{
	ObjType type = GetCurrentDocument()->GetEdit()->GetType();
	// Rotate selection
	if (type == xEditItem)
	{
		static_cast<CDrawEditItem*> (GetCurrentDocument()->GetEdit())->ChangeDir(dir);

		if (GetCurrentDocument()->IsSingleItemSelected() && (GetCurrentDocument()->GetSingleSelectedItem())->CanEdit())
		{
			// reflect updates in tool window
			(GetCurrentDocument()->GetSingleSelectedItem())->BeginEdit(TRUE);
		}
	}

	// Rotate annotation (E.g. import symbol into other symbol)
	else if (type == xAnnotation)
	{
		// Use the ChangeDir function in CDrawEditItem
		CDrawEditItem* edit = new CDrawEditItem(GetCurrentDocument());
		edit->ChangeDir(dir);
		delete edit;
	}

	// Rotate object while placing it
	else if (type == xMethod || type == xMethodEx || type == xMethodEx2 || type == xMethodEx3  
	      || type == xLabel || type == xLabelEx || type == xLabelEx2 
	      || type == xPower
	      || type == xText || type == xTextEx || type == xTextEx2
	      || type == xBusName || type == xBusNameEx
//		  || type == xNoteText		//not sure if this belongs here or not - djl
	      || type == xBusSlash
		  )
	{
		CDrawMethod* edit = static_cast<CDrawMethod*> (GetCurrentDocument()->GetEdit());
		// Update screen
		edit->Display(TRUE);

		edit->Rotate(CDPoint(0, 0), dir);
		edit->Display(TRUE);

		// reflect updates in tool window
		edit->BeginEdit(TRUE);
	}
}

//-------------------------------------------------------------------------
// Write-protected versions (File -> Create version)

bool CConCadView::IsWriteProtected()
{
	CMultiSheetDoc* pDoc = GetDocument();
	return pDoc != NULL && pDoc->IsWriteProtected();
}

// Commands of this view / its document that stay available on a
// write-protected version: viewing, finding, copying out and output.
static bool IsAllowedWhenWriteProtected(UINT nID)
{
	switch (nID)
	{
		case IDM_VIEWZOOMIN:
		case IDM_VIEWZOOMOUT:
		case IDM_VIEWCENTRE:
		case ID_RULER_VERT:
		case ID_RULER_HORIZ:
		case IDM_SNAPTOGRID:
		case IDM_TOGGLE_GRIDSIZE:
		case POSITIONBOX_GRIDSIZE:
		case IDM_VIEW_OPTIONS:
		case ID_OPTIONS_COLOURS:
		case ID_FIND_FIND:
		case IDM_EDITEDIT:
		case IDM_EDITSELECTALL:
		case IDM_EDITCOPY:
		case ID_CONTEXT_OPENDESIGN:
		case ID_FILE_PRINT:
		case ID_FILE_PRINT_DIRECT:
		case ID_FILE_PRINT_PREVIEW:
		case ID_FILE_SAVEASBITMAP:
		case ID_FILE_EXPORTPDF:
		case IDM_SPECIALNET:
		case IDM_SPECIALBOM:
		case ID_SPECIAL_CREATESPICEFILE:
		case ID_SPECIAL_VHDL:
		case ID_FILE_CLOSE:
		case ID_FILE_SAVE_AS:
		case IDM_FILE_CREATEVERSION:
		case IDM_FILE_EDITFILE:
		case IDM_SPECIAL_CREATEMODULE:
		case IDM_OPTIONS_SHORTCUTS:
			return true;
		default:
			return false;
	}
}

// On a write-protected version, disable (grey out and ignore) every command
// this view or its document handles, except the ones allowed above.
// Commands handled elsewhere (main frame, application) are not affected.
BOOL CConCadView::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	if (pHandlerInfo == NULL && (nCode == CN_COMMAND || nCode == CN_UPDATE_COMMAND_UI)
		&& IsWriteProtected() && !IsAllowedWhenWriteProtected(nID))
	{
		AFX_CMDHANDLERINFO info;
		if (CFolderView::OnCmdMsg(nID, CN_COMMAND, NULL, &info))
		{
			if (nCode == CN_UPDATE_COMMAND_UI)
			{
				static_cast<CCmdUI*>(pExtra)->Enable(FALSE);
			}
			return TRUE;
		}
	}
	return CFolderView::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

//-------------------------------------------------------------------------
// Edit -> Revision History

// The revision-history table on a sheet, or NULL.
static CDrawRevisionHistory* FindRevisionTable(CConCadDoc* pSheet)
{
	for (drawingIterator it = pSheet->GetDrawingBegin(); it != pSheet->GetDrawingEnd(); ++it)
	{
		if ((*it)->GetType() == xRevisionHistory)
		{
			return static_cast<CDrawRevisionHistory*>(*it);
		}
	}
	return NULL;
}

// Show or hide the revision-history table.  It always lives on the first
// sheet; the first time it is placed at the bottom-left of the page, from
// where it can be moved like any other object.
void CConCadView::OnEditRevisionHistory()
{
	CMultiSheetDoc* pDoc = GetDocument();
	CConCadDoc* pFirst = pDoc->GetSheet(0);
	if (pFirst == NULL)
	{
		return;
	}
	if (pDoc->GetActiveSheetIndex() != 0)
	{
		pDoc->SelectSheetView(0);
	}

	pFirst->BeginNewChangeSet();
	CDrawRevisionHistory* pTable = FindRevisionTable(pFirst);
	if (pTable == NULL)
	{
		CDrawRevisionHistory table(pFirst);
		const CPoint page = pFirst->GetDetails().GetPageBoundsAsPoint();
		table.m_point_a = CDPoint(10, page.y - 10);
		table.UpdateExtent();
		if (table.m_point_a == table.m_point_b)
		{
			AfxMessageBox(_T("The revision history template (templates\\revision.svg) was not found."), MB_ICONEXCLAMATION);
			return;
		}
		pTable = static_cast<CDrawRevisionHistory*>(table.Store());
	}
	else
	{
		pFirst->MarkChangeForUndo(pTable);
		pTable->SetVisible(!pTable->IsVisible());
	}
	pFirst->Invalidate();
}

void CConCadView::OnUpdateEditRevisionHistory(CCmdUI* pCmdUI)
{
	CConCadDoc* pFirst = GetDocument()->GetSheet(0);
	CDrawRevisionHistory* pTable = pFirst != NULL ? FindRevisionTable(pFirst) : NULL;
	pCmdUI->SetCheck(pTable != NULL && pTable->IsVisible());
}

//-------------------------------------------------------------------------
// Modules: a group of drawing objects stored in the module library
// (Options > Settings > Drawing) and inserted like a paste.

void CConCadView::OnSpecialCreateModule()
{
	CConCadDoc* pDoc = GetCurrentDocument();
	if (pDoc->GetEdit() == NULL || pDoc->GetEdit()->GetType() != xEditItem || !pDoc->IsSelected())
	{
		AfxMessageBox(_T("Select the objects to store as a module first."), MB_ICONINFORMATION);
		return;
	}

	const CString sModuleLib = CConCadRegistry::GetModuleLibrary();
	CLibraryStore* pLib = sModuleLib.IsEmpty() ? NULL : CLibraryCollection::GetLibrary(sModuleLib);
	if (pLib == NULL || pLib->MustUpgrade())
	{
		AfxMessageBox(_T("No module library is set.\n\nChoose one under Options > Settings > Drawing > Module library. ")
			_T("Only SQLite libraries (.TCLib) can hold modules."), MB_ICONINFORMATION);
		return;
	}

	// The selection as module XML (objects + the symbols etc. they use)
	CStreamMemory stream;
	{
		CXMLWriter xml(&stream);
		pDoc->SaveModuleXML(xml);
	}

	CLibraryStoreNameSet module;
	module.Blank();
	module.lib = pLib;
	module.GetRecord(0).name = _T("New module");
	module.GetRecord(0).description = _T("");
	module.GetRecord(0).reference = _T("");

	// Storing a placed module again: start from its current parameters
	const int group = pDoc->GetSelectedGroup();
	CDrawModuleInfo *pInfo = pDoc->IsSelectionOneGroup(group) ? pDoc->GetModuleInfo(group) : NULL;
	if (pInfo != NULL)
	{
		CSymbolRecord &r = module.GetRecord(0);
		r.name = pInfo->m_fields[0].value;
		r.reference = pInfo->m_fields[1].value;
		for (size_t i = CDrawModuleInfo::FixedFields; i < pInfo->m_fields.size(); i++)
		{
			CSymbolField f;
			f.field_name = pInfo->m_fields[i].name;
			f.field_default = pInfo->m_fields[i].value;
			f.field_type = default_show;
			r.fields.push_back(f);
		}
	}

	CDlgUpdateBox dlg(AfxGetMainWnd());
	dlg.SetSymbol(&module);
	dlg.SetModuleMode();
	if (dlg.DoModal() != IDOK)
	{
		return;
	}

	for (int i = 0; i < module.GetNumRecords(); i++)
	{
		module.GetRecord(i).is_module = TRUE;
	}
	pLib->StoreModule(&module, stream);   // reports its own errors
}

void CConCadView::PlaceModule(CLibraryStoreSymbol* theModule)
{
	CConCadDoc* pDoc = GetCurrentDocument();
	CStream* pStream = theModule->m_pParent->GetMethodArchive();
	if (pStream == NULL)
	{
		return;
	}

	// Same as Edit -> Paste: import the objects, then let them follow the
	// mouse until the user clicks to drop them.
	pDoc->BeginNewChangeSet();
	pDoc->SelectObject(new CDrawEditItem(pDoc));
	pDoc->SelectObject(NULL);
	// The placed objects form one module (a group)
	const int group = pDoc->GetNewGroupId();
	if (pDoc->Import(*pStream, group))
	{
		// ... with its parameters: Name, Reference and the module's fields
		CDrawModuleInfo *pInfo = new CDrawModuleInfo(pDoc);
		pInfo->m_group = group;
		pInfo->m_fields[0].value = theModule->name;
		pInfo->m_fields[1].value = theModule->reference;
		for (size_t i = 0; i < theModule->fields.size(); i++)
		{
			CDrawModuleInfo::Field f;
			f.name = theModule->fields[i].field_name;
			f.value = theModule->fields[i].field_default;
			pInfo->m_fields.push_back(f);
		}
		pDoc->Add(pInfo);
		pDoc->Select(pInfo);

		pDoc->PostPaste();
		CDrawBlockImport *pImport = new CDrawBlockImport(pDoc);
		pDoc->SelectObject(pImport);
		pImport->Import();
	}
	else
	{
		pDoc->SelectObject(new CDrawEditItem(pDoc));
	}
	delete pStream;
}

// Esc: back to the select tool; in the select tool it also finishes
// editing an open group
void CConCadView::OnEditEdit()
{
	CConCadDoc* pDoc = GetCurrentDocument();
	const bool finish_group = pDoc->GetOpenGroup() != 0 && pDoc->GetEdit() != NULL && pDoc->GetEdit()->GetType() == xEditItem;
	pDoc->SelectObject(new CDrawEditItem(pDoc));
	if (finish_group)
	{
		pDoc->CloseGroup();
	}
}

// Object -> Create Group (Ctrl+G)
void CConCadView::OnObjectGroup()
{
	CConCadDoc* pDoc = GetCurrentDocument();
	if (pDoc->GetEdit() == NULL || pDoc->GetEdit()->GetType() != xEditItem)
	{
		return;
	}
	g_EditToolBar.m_ModuleEdit.CloseIfShowing();
	pDoc->GroupSelection();
}

void CConCadView::OnUpdateObjectGroup(CCmdUI* pCmdUI)
{
	CConCadDoc* pDoc = GetCurrentDocument();
	pCmdUI->Enable(pDoc->IsSelected() && !pDoc->IsSingleItemSelected());
}

void CConCadView::OnUpdateModuleGroupSelected(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(GetCurrentDocument()->GetSelectedGroup() != 0);
}

void CConCadView::OnUpdateModuleFinishEdit(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(GetCurrentDocument()->GetOpenGroup() != 0);
}

// Object -> Colour (also on the right-click menu)
void CConCadView::OnColorConsat()
{
	GetCurrentDocument()->SetSelectionColor(TRUE, CConCadRegistry::GetConsatColor());
}

void CConCadView::OnColorFactory()
{
	GetCurrentDocument()->SetSelectionColor(TRUE, CConCadRegistry::GetFactoryColor());
}

void CConCadView::OnColorCustom()
{
	CConCadDoc* pDoc = GetCurrentDocument();
	if (!pDoc->IsColorableSelected())
	{
		return;
	}
	COLORREF c = RGB(0, 0, 0);
	pDoc->GetSelectionColor(c);
	CColorDialog dlg(c, CC_FULLOPEN | CC_RGBINIT, this);
	if (dlg.DoModal() == IDOK)
	{
		pDoc->SetSelectionColor(TRUE, dlg.GetColor());
	}
}

void CConCadView::OnColorDefault()
{
	GetCurrentDocument()->SetSelectionColor(FALSE, RGB(0, 0, 0));
}

void CConCadView::OnUpdateColor(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(GetCurrentDocument()->IsColorableSelected());
}

void CConCadView::OnOptionsShortcuts()
{
	CShortcutsDlg dlg(AfxGetMainWnd());
	dlg.DoModal();
}

// Edit Group (right-click or double-click a group / placed module): its
// objects can be selected and changed one by one until an object outside
// it is clicked, Esc is pressed or Finish Editing Group is chosen
void CConCadView::OnModuleEdit()
{
	CConCadDoc* pDoc = GetCurrentDocument();
	int group = pDoc->GetSelectedGroup();
	if (group != 0)
	{
		pDoc->SelectObject(new CDrawEditItem(pDoc));
		pDoc->OpenGroup(group);
	}
}

void CConCadView::OnModuleUngroup()
{
	g_EditToolBar.m_ModuleEdit.CloseIfShowing();
	GetCurrentDocument()->UngroupSelection();
}

void CConCadView::OnModuleFinishEdit()
{
	CConCadDoc* pDoc = GetCurrentDocument();
	pDoc->SelectObject(new CDrawEditItem(pDoc));
	pDoc->CloseGroup();
}
