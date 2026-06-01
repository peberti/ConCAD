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

#include "stdafx.h"
#include "tinycad.h"
#include "startup.h"
#include "revision.h"
#include "BuildID.h"

// The constructor (Which loads etc., the bitmap)
CDlgStartUpWindow::CDlgStartUpWindow(CWnd *Parent)
{
	// Load a bitmap into a compatible DC
	theBitmap.LoadBitmap(_T("START_BITMAP"));
	BITMAP theBitmapSize;

	// Get it's size etc..
	theBitmap.GetObject(sizeof (theBitmapSize), &theBitmapSize);
	theSize = CSize(theBitmapSize.bmWidth, theBitmapSize.bmHeight);

	// Centre the window in the parent window
	CRect pRect;
	GetDesktopWindow()->GetWindowRect(pRect);
	CPoint Centre = CPoint(pRect.left + pRect.Width() / 2 - theSize.cx / 2, pRect.top + pRect.Height() / 2 - theSize.cy / 2);
	CRect ClientRect = CRect(Centre.x, Centre.y, Centre.x + theSize.cx, Centre.y + theSize.cy);

	// Create our own Window's class for this window
	CString theClass = AfxRegisterWndClass(0);

	// Now create the window
	CreateEx(0, theClass, _T("TinyCAD"), WS_POPUP | WS_VISIBLE, ClientRect.left, ClientRect.top, ClientRect.Width(), ClientRect.Height(), Parent->m_hWnd, NULL);

	// Set a timer to destroy this window in TIME_OUT miliseconds time
	timerID = (int) SetTimer(1, TIME_OUT, NULL);
}

// The destructor (which gets rid of the Bitmap)
CDlgStartUpWindow::~CDlgStartUpWindow()
{

	theBitmap.DeleteObject();
}

BEGIN_MESSAGE_MAP( CDlgStartUpWindow, CWnd ) 
	ON_WM_PAINT()
	ON_WM_TIMER()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

// The On Paint Handler — draws the ConCAD splash.  STARTUP.BMP is still
// loaded by the constructor so the window inherits its dimensions, but its
// pixels are overpainted entirely so the splash is recognisably ConCAD.
void CDlgStartUpWindow::OnPaint()
{
	CPaintDC dc(this);
	CRect rect;
	GetClientRect(rect);

	// Background
	CBrush bg(RGB(15, 32, 64));
	dc.FillRect(rect, &bg);

	// White rule border
	CPen border(PS_SOLID, 2, RGB(255, 255, 255));
	CPen* oldPen = dc.SelectObject(&border);
	CBrush* oldBrush = (CBrush*) dc.SelectStockObject(NULL_BRUSH);
	dc.Rectangle(rect.left + 1, rect.top + 1, rect.right - 1, rect.bottom - 1);
	dc.SelectObject(oldBrush);
	dc.SelectObject(oldPen);

	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(RGB(255, 255, 255));

	// Big centred "ConCAD"
	CFont fontTitle;
	fontTitle.CreateFont(-60, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
		ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		DEFAULT_QUALITY, DEFAULT_PITCH | FF_MODERN, _T("Arial"));
	CFont* oldFont = dc.SelectObject(&fontTitle);
	dc.SetTextAlign(TA_CENTER | TA_TOP);
	dc.TextOut(rect.Width() / 2, rect.Height() / 2 - 60, _T("ConCAD"));
	dc.SelectObject(oldFont);

	// Version line
	CFont fontMed;
	fontMed.CreateFont(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, _T("Segoe UI"));
	dc.SelectObject(&fontMed);

	CString version;
	version.Format(_T("Version %s"), (LPCTSTR) CTinyCadApp::GetVersion());
	dc.TextOut(rect.Width() / 2, rect.Height() / 2 + 10, version);

	// Tagline / branch (small)
	CFont fontSmall;
	fontSmall.CreateFont(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, _T("Segoe UI"));
	dc.SelectObject(&fontSmall);

	CString tagline;
	tagline.Format(_T("branch %hs  -  based on TinyCAD"), GIT_BRANCH);
	dc.TextOut(rect.Width() / 2, rect.Height() - 28, tagline);

	dc.SelectObject(oldFont);
}

// When this is called destroy the window
void CDlgStartUpWindow::OnTimer(UINT t)
{
	// Get rid of the timer
	KillTimer(timerID);

	// Get rid of the resources being used by this window
	// theBitmap.DeleteObject();

	// Now delete the window
	DestroyWindow();
}

// Called when the window is destroyed
void CDlgStartUpWindow::OnDestroy()
{
	delete this;
}
