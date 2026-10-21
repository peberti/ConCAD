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
#include "concad.h"
#include "startup.h"
#include "revision.h"
#include "ImagePng.h"

// The splash picture, a PNG resource (IDB_SPLASH = res/splash.png, exported
// from res/splash.svg).  NULL if it cannot be loaded.
static CImagePNG* LoadSplashImage(CSize& size)
{
	HINSTANCE hInst = AfxGetResourceHandle();
	HRSRC hRes = ::FindResource(hInst, MAKEINTRESOURCE(IDB_SPLASH), _T("PNG"));
	HGLOBAL hData = hRes != NULL ? ::LoadResource(hInst, hRes) : NULL;
	const void* pData = hData != NULL ? ::LockResource(hData) : NULL;
	if (pData == NULL)
	{
		return NULL;
	}
	const DWORD length = ::SizeofResource(hInst, hRes);
	unsigned char* copy = new unsigned char[length]; // owned by the image
	memcpy(copy, pData, length);

	CImagePNG* pImage = new CImagePNG();
	pImage->SetCompressedData(copy, length);
	CWindowDC screen(NULL);
	size = pImage->GetSize(screen);
	return pImage;
}

// The constructor (which loads the picture)
CDlgStartUpWindow::CDlgStartUpWindow(CWnd *Parent)
{
	theSize = CSize(618, 246);
	m_pImage = LoadSplashImage(theSize);

	// Centre it on the monitor ConCAD opens on: the main window is not at
	// its saved position yet, so use that position (primary monitor if none)
	CRect saved;
	CConCadApp *pApp = static_cast<CConCadApp*>(AfxGetApp());
	HMONITOR hMonitor = pApp != NULL && pApp->GetSavedWindowRect(saved)
		? ::MonitorFromRect(saved, MONITOR_DEFAULTTONEAREST)
		: ::MonitorFromPoint(CPoint(0, 0), MONITOR_DEFAULTTOPRIMARY);
	MONITORINFO mi;
	mi.cbSize = sizeof(mi);
	CRect pRect;
	if (::GetMonitorInfo(hMonitor, &mi))
	{
		pRect = mi.rcWork;
	}
	else
	{
		GetDesktopWindow()->GetWindowRect(pRect);
	}
	CPoint Centre = CPoint(pRect.left + pRect.Width() / 2 - theSize.cx / 2, pRect.top + pRect.Height() / 2 - theSize.cy / 2);
	CRect ClientRect = CRect(Centre.x, Centre.y, Centre.x + theSize.cx, Centre.y + theSize.cy);

	// Create our own Window's class for this window
	CString theClass = AfxRegisterWndClass(0);

	// Now create the window
	CreateEx(0, theClass, _T("ConCAD"), WS_POPUP | WS_VISIBLE, ClientRect.left, ClientRect.top, ClientRect.Width(), ClientRect.Height(), Parent->m_hWnd, NULL);

	// Set a timer to destroy this window in TIME_OUT miliseconds time
	timerID = (int) SetTimer(1, TIME_OUT, NULL);
}

// The destructor
CDlgStartUpWindow::~CDlgStartUpWindow()
{
	delete m_pImage;
}

BEGIN_MESSAGE_MAP( CDlgStartUpWindow, CWnd ) 
	ON_WM_PAINT()
	ON_WM_TIMER()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

// The picture, then the version where the hidden "Version" layer of
// res/splash.svg has it (x 207, baseline 139, Segoe UI 18 px, #EEEEEE)
void CDlgStartUpWindow::OnPaint()
{
	CPaintDC dc(this);
	CRect rect;
	GetClientRect(rect);

	if (m_pImage != NULL)
	{
		m_pImage->Paint(dc, rect, 0);
	}
	else
	{
		CBrush bg(RGB(0x01, 0x69, 0x84));
		dc.FillRect(rect, &bg);
	}

	CFont font;
	font.CreateFont(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, _T("Segoe UI"));
	CFont* oldFont = dc.SelectObject(&font);
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(RGB(0xEE, 0xEE, 0xEE));
	dc.SetTextAlign(TA_LEFT | TA_BASELINE);

	CString version;
	version.Format(_T("Version %s  (%s)"), (LPCTSTR) CConCadApp::GetVersion(), (LPCTSTR) CConCadApp::GetReleaseType());
	dc.TextOut(207, 139, version);

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
