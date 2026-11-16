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
// inside a combo box or list) and the user has selected text in it, or it
// is a cell editor (IsCellEditor, any selection or just a caret): Copy /
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
	if (pDlg->IsCellEditor(pFocus->m_hWnd))
	{
		return pFocus;
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

//-------------------------------------------------------------------------
// Read-only Tool Options (write-protected versions, CHANGES.md §52)

bool CEditDlg::IsReadOnly() const
{
	return m_pDesign != NULL && m_pDesign->GetParent() != NULL && m_pDesign->GetParent()->IsWriteProtected();
}

static bool IsWindowClass(HWND h, LPCTSTR name)
{
	TCHAR cls[32] = { 0 };
	::GetClassName(h, cls, _countof(cls));
	return _tcsicmp(cls, name) == 0;
}

struct ReadOnlyPass
{
	bool read_only;
	std::vector<HWND>* changed;
};

static BOOL CALLBACK SetEditReadOnly(HWND h, LPARAM lParam)
{
	ReadOnlyPass* pass = reinterpret_cast<ReadOnlyPass*>(lParam);
	if (pass->read_only && IsWindowClass(h, _T("Edit")) && (::GetWindowLong(h, GWL_STYLE) & ES_READONLY) == 0)
	{
		::SendMessage(h, EM_SETREADONLY, TRUE, 0);
		pass->changed->push_back(h);
	}
	return TRUE;
}

// Make every text field read-only (selectable and copyable) while the
// dialog shows an object of a write-protected version, editable otherwise
void CEditDlg::ApplyReadOnly()
{
	if (GetSafeHwnd() == NULL)
	{
		return;
	}
	if (IsReadOnly())
	{
		ReadOnlyPass pass = { true, &m_readOnlyEdits };
		::EnumChildWindows(m_hWnd, SetEditReadOnly, reinterpret_cast<LPARAM>(&pass));
	}
	else
	{
		for (size_t i = 0; i < m_readOnlyEdits.size(); i++)
		{
			if (::IsWindow(m_readOnlyEdits[i]))
			{
				::SendMessage(m_readOnlyEdits[i], EM_SETREADONLY, FALSE, 0);
			}
		}
		m_readOnlyEdits.clear();
	}
}

// The mnemonic (Alt+letter) of a control of the dialog
static BOOL CALLBACK FindMnemonic(HWND h, LPARAM lParam)
{
	std::pair<TCHAR, bool>* p = reinterpret_cast<std::pair<TCHAR, bool>*>(lParam);
	TCHAR text[128] = { 0 };
	::GetWindowText(h, text, _countof(text));
	for (const TCHAR* s = text; *s != 0; s++)
	{
		if (*s == _T('&') && s[1] != 0 && s[1] != _T('&')
			&& _totupper(s[1]) == _totupper(p->first))
		{
			p->second = true;
			return FALSE;
		}
	}
	return TRUE;
}

// On a read-only dialog: text fields can be clicked, selected and copied
// from and lists scrolled, but buttons, tick boxes, drop-downs, spin
// boxes and the keys that change them do nothing
bool CEditDlg::BlockReadOnlyInput(const MSG* pMsg)
{
	const UINT msg = pMsg->message;
	const bool mouse = msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK || msg == WM_RBUTTONDOWN
		|| msg == WM_RBUTTONDBLCLK || msg == WM_MBUTTONDOWN || msg == WM_MOUSEWHEEL;
	const bool key = msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN || msg == WM_CHAR || msg == WM_SYSCHAR;
	if ((!mouse && !key) || pMsg->hwnd == NULL || !::IsChild(m_hWnd, pMsg->hwnd))
	{
		return false;
	}
	const HWND h = pMsg->hwnd;

	// Alt+letter would click a button or tick box
	if (msg == WM_SYSCHAR)
	{
		std::pair<TCHAR, bool> p((TCHAR)pMsg->wParam, false);
		::EnumChildWindows(m_hWnd, FindMnemonic, reinterpret_cast<LPARAM>(&p));
		return p.second;
	}

	const WPARAM k = pMsg->wParam;
	const bool changing_key = (msg == WM_KEYDOWN && (k == VK_SPACE || k == VK_UP || k == VK_DOWN || k == VK_PRIOR
		|| k == VK_NEXT || k == VK_F4 || ((k == VK_HOME || k == VK_END) && ::GetKeyState(VK_CONTROL) >= 0)))
		|| (msg == WM_SYSKEYDOWN && (k == VK_UP || k == VK_DOWN))
		|| (msg == WM_CHAR && k != VK_TAB && k != VK_ESCAPE && k != VK_RETURN && k >= 0x20);

	if (IsWindowClass(h, _T("Edit")))
	{
		// Also cell editors opened over a list
		if ((::GetWindowLong(h, GWL_STYLE) & ES_READONLY) == 0)
		{
			::SendMessage(h, EM_SETREADONLY, TRUE, 0);
			m_readOnlyEdits.push_back(h);
		}
		// Up / Down / F4 in a drop-down's text pick another entry
		const bool in_combo = IsWindowClass(::GetParent(h), _T("ComboBox"));
		return in_combo && key && msg != WM_CHAR && changing_key
			&& k != VK_SPACE && k != VK_HOME && k != VK_END;
	}
	if (IsWindowClass(h, _T("SysListView32")) || IsWindowClass(h, _T("SysHeader32")) || IsWindowClass(h, _T("ScrollBar")))
	{
		// Clicking a cell shows its text read-only; tick boxes are guarded
		// by the dialog itself
		return false;
	}
	// Enter or Space would click a focused button
	return mouse || changing_key || (msg == WM_KEYDOWN && k == VK_RETURN && IsWindowClass(h, _T("Button")));
}

BOOL CEditDlg::PreTranslateMessage(MSG* pMsg)
{
	extern CConCadView *g_currentview;

	if (IsReadOnly() && BlockReadOnlyInput(pMsg))
	{
		return TRUE;
	}

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

