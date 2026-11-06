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
#include "option.h"
#include "revision.h"
#include "registry.h"
#include "ConCadView.h"
#include "ConCad.h"
#include "MainFrm.h"
#include "diag.h"

CDlgERCListBox theERCListBox;

////// The CEditDlg dialog //////

CEditDlg::CEditDlg()
{
	m_pDesign = NULL;
	m_pObject = NULL;
	opens = 0;

	//{{AFX_DATA_INIT(CEditDlg)
	//}}AFX_DATA_INIT
}

void CEditDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CEditDlg)
	//}}AFX_DATA_MAP
}

void CEditDlg::OnCancel()
{	
	// This member is called when edit dialog is in focus and user presses VK_ESCAPE.
	// Process this situation in the same way as what would happen if the main application window would
	// have been in focus and user pressed VK_ESCAPE.

	extern CConCadView *g_currentview;

	if (g_currentview) {
		// CConCadView maps the VK_ESCAPE to this command
		g_currentview->SendMessage(WM_COMMAND, IDM_EDITEDIT, 0);
	}
}

// The command the main window's (user-editable) shortcut table maps this
// key press to, or 0.
static UINT FindShortcutCommand(const MSG* pMsg)
{
	CFrameWnd* pFrame = static_cast<CFrameWnd*>(AfxGetMainWnd());
	HACCEL h = pFrame != NULL ? pFrame->m_hAccelTable : NULL;
	int n = h != NULL ? ::CopyAcceleratorTable(h, NULL, 0) : 0;
	if (n <= 0)
	{
		return 0;
	}
	std::vector<ACCEL> accels(n);
	::CopyAcceleratorTable(h, &accels[0], n);

	BYTE mods = FVIRTKEY;
	if (::GetKeyState(VK_CONTROL) < 0) mods |= FCONTROL;
	if (::GetKeyState(VK_SHIFT) < 0) mods |= FSHIFT;
	if (::GetKeyState(VK_MENU) < 0) mods |= FALT;

	for (int i = 0; i < n; ++i)
	{
		if (accels[i].key == pMsg->wParam && (accels[i].fVirt & ~FNOINVERT) == mods)
		{
			return accels[i].cmd;
		}
	}
	return 0;
}

// While a text, label etc. is selected its text field here has the focus, so
// the dialog would take the copy/cut keys for the field's text. Send them to
// the drawing instead, so they copy/cut the selected objects. Paste goes to
// the drawing only when the clipboard holds ConCAD objects; plain text is
// still pasted into the field.  When the user has selected text in a field
// (other than the whole name, as right after the click), Copy / Cut / Paste
// act on that text instead.  Delete deletes the object while the name
// field of a text, label, pin or power symbol still has all its text
// selected (as it has right after the object was clicked); once the caret
// is placed in the field, Delete edits the text.
// The focus is in the name field of a text, label, pin or power symbol and
// all of its text is selected (or it is empty)
bool CEditDlg::IsWholeNameFieldSelected()
{
	CWnd *pFocus = GetFocus();
	if (pFocus == NULL || pFocus->GetParent() != this)
	{
		return false;
	}
	const int id = pFocus->GetDlgCtrlID();
	if (id != TEXTBOX_TEXT && id != PINBOX_NAME && id != POWERBOX_TEXT)
	{
		return false;
	}
	CEdit *pEdit = static_cast<CEdit*>(pFocus);
	int start = 0, end = 0;
	pEdit->GetSel(start, end);
	return start == 0 && end == pEdit->GetWindowTextLength();
}

// The focus is in a text field of a Tool Options dialog (also an edit box
// inside a combo box or list) and the user has selected text in it: Copy /
// Cut / Paste act on that text (keys, Edit menu and toolbar alike).  The
// whole text of a text / label / pin / power name field selected does not
// count: that is the state right after the object was clicked, where Copy /
// Cut / Paste act on the objects.
CWnd* CEditDlg::GetSelectedTextField()
{
	CWnd *pFocus = CWnd::GetFocus();
	if (pFocus == NULL)
	{
		return NULL;
	}
	TCHAR cls[32] = { 0 };
	::GetClassName(pFocus->m_hWnd, cls, _countof(cls));
	CString sClass(cls);
	sClass.MakeLower();
	if (sClass.Find(_T("edit")) < 0)
	{
		return NULL;
	}
	CEditDlg *pDlg = NULL;
	for (HWND h = ::GetParent(pFocus->m_hWnd); h != NULL && pDlg == NULL; h = ::GetParent(h))
	{
		pDlg = dynamic_cast<CEditDlg*>(CWnd::FromHandlePermanent(h));
	}
	if (pDlg == NULL)
	{
		return NULL;
	}
	DWORD start = 0, end = 0;
	pFocus->SendMessage(EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
	if (start == end || pDlg->IsWholeNameFieldSelected())
	{
		return NULL;
	}
	return pFocus;
}

// A key press that is a drawing shortcut rather than typing or editing in
// the focused field: with Ctrl or Alt, or a function key - except the
// field's own Ctrl keys (select all, undo, moving the caret, ...)
static bool IsDrawingShortcut(const MSG* pMsg)
{
	const WPARAM key = pMsg->wParam;
	const bool ctrl = ::GetKeyState(VK_CONTROL) < 0;
	const bool alt = ::GetKeyState(VK_MENU) < 0;
	const bool fkey = key >= VK_F1 && key <= VK_F24;
	if (!ctrl && !alt && !fkey)
	{
		return false;
	}
	if (ctrl && !alt)
	{
		TCHAR cls[32] = { 0 };
		::GetClassName(pMsg->hwnd, cls, _countof(cls));
		CString sClass(cls);
		sClass.MakeLower();
		if (sClass.Find(_T("edit")) >= 0)
		{
			switch (key)
			{
				case 'A': case 'Z': case 'Y':
				case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN:
				case VK_HOME: case VK_END: case VK_BACK: case VK_DELETE: case VK_INSERT:
					return false;
				default:
					break;
			}
		}
	}
	return true;
}

BOOL CEditDlg::PreTranslateMessage(MSG* pMsg)
{
	extern CConCadView *g_currentview;

	if ((pMsg->message == WM_KEYDOWN || pMsg->message == WM_SYSKEYDOWN) && g_currentview)
	{
		UINT id = FindShortcutCommand(pMsg);
		CWnd *pTextField = (id == IDM_EDITCOPY || id == IDM_EDITCUT || id == IDM_EDITPASTE)
			? GetSelectedTextField() : NULL;
		if (pTextField != NULL)
		{
			// Text selected in a field: copy / cut / paste act on that text
			if (id == IDM_EDITPASTE && !::IsClipboardFormatAvailable(CF_UNICODETEXT))
			{
				return TRUE;	// nothing to paste into the field
			}
			pTextField->SendMessage(id == IDM_EDITCOPY ? WM_COPY : id == IDM_EDITCUT ? WM_CUT : WM_PASTE);
			return TRUE;
		}
		if (id == IDM_EDITCOPY || id == IDM_EDITCUT
			|| (id == IDM_EDITPASTE && g_currentview->IsClipboardAvailable()))
		{
			g_currentview->SendMessage(WM_COMMAND, id, 0);
			return TRUE;
		}
		if (id == IDM_EDITDELITEM && IsWholeNameFieldSelected())
		{
			g_currentview->SendMessage(WM_COMMAND, id, 0);
			return TRUE;
		}
		if (id == IDM_EDIT_COPYTEXT || id == IDM_EDIT_CUTTEXT || id == IDM_EDIT_DELETETEXT)
		{
			// Copy / Cut / Delete Text in the name field act on the field:
			// its selected part, or all of it when nothing is selected
			// (Copy Text then copies the selected objects' texts instead)
			CWnd *pFocus = GetFocus();
			const int field = pFocus != NULL && pFocus->GetParent() == this ? pFocus->GetDlgCtrlID() : 0;
			if (field == TEXTBOX_TEXT || field == PINBOX_NAME || field == POWERBOX_TEXT)
			{
				CEdit *pEdit = static_cast<CEdit*>(pFocus);
				int start = 0, end = 0;
				pEdit->GetSel(start, end);
				if (id != IDM_EDIT_COPYTEXT || start != end)
				{
					if (start == end)
					{
						pEdit->SetSel(0, -1);
					}
					pEdit->SendMessage(id == IDM_EDIT_COPYTEXT ? WM_COPY : id == IDM_EDIT_CUTTEXT ? WM_CUT : WM_CLEAR);
					return TRUE;
				}
			}
			g_currentview->SendMessage(WM_COMMAND, id, 0);
			return TRUE;
		}
		if (id == IDM_SPECIAL_CREATEMODULE)
		{
			g_currentview->SendMessage(WM_COMMAND, id, 0);
			return TRUE;
		}
		if (id != 0 && IsDrawingShortcut(pMsg))
		{
			// Any other Ctrl / Alt / function-key shortcut (e.g. one the
			// user assigned, like Ctrl+D for Default Colour) acts on the
			// drawing; the dialog would swallow it
			AfxGetMainWnd()->SendMessage(WM_COMMAND, MAKEWPARAM(id, 1), 0);
			return TRUE;
		}
	}
	return CDialog::PreTranslateMessage(pMsg);
}

BEGIN_MESSAGE_MAP(CEditDlg, CDialog)
//{{AFX_MSG_MAP(CEditDlg)
// No message handlers
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

