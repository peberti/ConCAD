/*
 * ConCAD: Options -> Keyboard Shortcuts dialog
 * License:		Lesser GNU Public License 2.1 (LGPL)
 */

#include "stdafx.h"
#include "ShortcutsDlg.h"

static CString CleanMenuText(CString s)
{
	int tab = s.Find(_T('\t'));
	if (tab >= 0)
	{
		s = s.Left(tab);
	}
	s.Replace(_T("&&"), _T("\x01"));
	s.Remove(_T('&'));
	s.Replace(_T("\x01"), _T("&"));
	s.TrimRight(_T(". "));
	return s;
}

CShortcutsDlg::CShortcutsDlg(CWnd* pParent) :
	CDialog(IDD, pParent)
{
}

void CShortcutsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SC_LIST, m_list);
	DDX_Control(pDX, IDC_SC_KEY, m_key);
}

BEGIN_MESSAGE_MAP(CShortcutsDlg, CDialog)
	ON_BN_CLICKED(IDC_SC_ASSIGN, OnAssign)
	ON_BN_CLICKED(IDC_SC_REMOVE, OnRemove)
	ON_BN_CLICKED(IDC_SC_RESET, OnReset)
END_MESSAGE_MAP()

void CShortcutsDlg::LoadDefaults(std::vector<ACCEL> &accels)
{
	accels.clear();
	HACCEL h = ::LoadAccelerators(AfxGetResourceHandle(), MAKEINTRESOURCE(IDR_MAINFRAME));
	if (h != NULL)
	{
		int n = ::CopyAcceleratorTable(h, NULL, 0);
		accels.resize(n);
		if (n > 0)
		{
			::CopyAcceleratorTable(h, &accels[0], n);
		}
		::DestroyAcceleratorTable(h);
	}
}

CString CShortcutsDlg::FormatAccel(const ACCEL &a)
{
	ACCEL copy = a;
	CMFCAcceleratorKey key(&copy);
	CString s;
	key.Format(s);
	return s;
}

bool CShortcutsDlg::SameKey(const ACCEL &a, const ACCEL &b)
{
	return (a.fVirt & ~FNOINVERT) == (b.fVirt & ~FNOINVERT) && a.key == b.key;
}

CString CShortcutsDlg::GetShortcutText(HACCEL hAccel, UINT id)
{
	if (hAccel == NULL)
	{
		return CString();
	}
	int n = ::CopyAcceleratorTable(hAccel, NULL, 0);
	if (n <= 0)
	{
		return CString();
	}
	std::vector<ACCEL> accels(n);
	::CopyAcceleratorTable(hAccel, &accels[0], n);

	// Prefer a letter/digit key (Ctrl+Z rather than Alt+Backspace)
	const ACCEL *found = NULL;
	for (int i = 0; i < n; i++)
	{
		if (accels[i].cmd != id)
		{
			continue;
		}
		const bool alnum = (accels[i].fVirt & FVIRTKEY) && ((accels[i].key >= 'A' && accels[i].key <= 'Z') || (accels[i].key >= '0' && accels[i].key <= '9'));
		if (found == NULL || alnum)
		{
			found = &accels[i];
			if (alnum)
			{
				break;
			}
		}
	}
	return found != NULL ? FormatAccel(*found) : CString();
}

void CShortcutsDlg::AddCommand(UINT id, const CString &name)
{
	for (size_t i = 0; i < m_commands.size(); i++)
	{
		if (m_commands[i].id == id)
		{
			return;
		}
	}
	Command c;
	c.id = id;
	c.name = name;
	m_commands.push_back(c);
}

void CShortcutsDlg::AddMenu(CMenu *pMenu, const CString &prefix)
{
	for (int i = 0; i < (int)pMenu->GetMenuItemCount(); i++)
	{
		CString text;
		pMenu->GetMenuString(i, text, MF_BYPOSITION);
		text = CleanMenuText(text);
		CMenu *pSub = pMenu->GetSubMenu(i);
		if (pSub != NULL)
		{
			AddMenu(pSub, prefix.IsEmpty() ? text : prefix + _T(" > ") + text);
			continue;
		}
		UINT id = pMenu->GetMenuItemID(i);
		if (id == 0 || id == (UINT)-1 || (id >= ID_FILE_MRU_FIRST && id <= ID_FILE_MRU_LAST) || id >= 0xF000 || text.IsEmpty())
		{
			continue;
		}
		AddCommand(id, prefix + _T(" > ") + text);
	}
}

CString CShortcutsDlg::GetCommandName(UINT id) const
{
	for (size_t i = 0; i < m_commands.size(); i++)
	{
		if (m_commands[i].id == id)
		{
			return m_commands[i].name;
		}
	}

	// "Description\nTooltip" from the string table
	CString s;
	if (s.LoadString(id))
	{
		int n = s.Find(_T('\n'));
		if (n >= 0)
		{
			CString tip = s.Mid(n + 1);
			if (!tip.IsEmpty())
			{
				return tip;
			}
			s = s.Left(n);
		}
		if (!s.IsEmpty())
		{
			return s;
		}
	}
	s.Format(_T("Command %u"), id);
	return s;
}

// Menu commands first (in menu order), then toolbar buttons (drawing tools
// etc.), then anything else that has a shortcut
void CShortcutsDlg::CollectCommands()
{
	m_commands.clear();

	CMenu menu;
	if (menu.LoadMenu(IDR_TCADTYPE))
	{
		AddMenu(&menu, CString());
	}

	const CObList &bars = CMFCToolBar::GetAllToolbars();
	for (POSITION pos = bars.GetHeadPosition(); pos != NULL;)
	{
		CMFCToolBar *pBar = DYNAMIC_DOWNCAST(CMFCToolBar, bars.GetNext(pos));
		if (pBar == NULL)
		{
			continue;
		}
		CString barName;
		pBar->GetWindowText(barName);
		if (barName.IsEmpty())
		{
			barName = _T("Toolbar");
		}
		for (int i = 0; i < pBar->GetCount(); i++)
		{
			CMFCToolBarButton *pButton = pBar->GetButton(i);
			if (pButton == NULL || (pButton->m_nStyle & TBBS_SEPARATOR) || pButton->m_nID == 0 || pButton->m_nID == (UINT)-1)
			{
				continue;
			}
			AddCommand(pButton->m_nID, barName + _T(" > ") + GetCommandName(pButton->m_nID));
		}
	}

	for (size_t i = 0; i < m_accels.size(); i++)
	{
		AddCommand(m_accels[i].cmd, _T("Other > ") + GetCommandName(m_accels[i].cmd));
	}
}

CString CShortcutsDlg::ShortcutsFor(UINT id) const
{
	CString s;
	for (size_t i = 0; i < m_accels.size(); i++)
	{
		if (m_accels[i].cmd == id)
		{
			if (!s.IsEmpty())
			{
				s += _T(", ");
			}
			s += FormatAccel(m_accels[i]);
		}
	}
	return s;
}

void CShortcutsDlg::FillList(int select)
{
	m_list.SetRedraw(FALSE);
	m_list.DeleteAllItems();
	for (int i = 0; i < (int)m_commands.size(); i++)
	{
		m_list.InsertItem(i, m_commands[i].name);
		m_list.SetItemText(i, 1, ShortcutsFor(m_commands[i].id));
	}
	if (select >= 0 && select < (int)m_commands.size())
	{
		m_list.SetItemState(select, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		m_list.EnsureVisible(select, FALSE);
	}
	m_list.SetRedraw(TRUE);
}

int CShortcutsDlg::GetSelectedCommand()
{
	POSITION pos = m_list.GetFirstSelectedItemPosition();
	return pos == NULL ? -1 : m_list.GetNextSelectedItem(pos);
}

BOOL CShortcutsDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	CFrameWnd *pFrame = DYNAMIC_DOWNCAST(CFrameWnd, AfxGetMainWnd());
	HACCEL h = pFrame != NULL ? pFrame->m_hAccelTable : NULL;
	int n = h != NULL ? ::CopyAcceleratorTable(h, NULL, 0) : 0;
	if (n > 0)
	{
		m_accels.resize(n);
		::CopyAcceleratorTable(h, &m_accels[0], n);
	}
	else
	{
		LoadDefaults(m_accels);
	}

	m_list.SetExtendedStyle(m_list.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	CRect r;
	m_list.GetClientRect(&r);
	m_list.InsertColumn(0, _T("Command"), LVCFMT_LEFT, r.Width() * 2 / 3);
	m_list.InsertColumn(1, _T("Shortcut"), LVCFMT_LEFT, r.Width() - r.Width() * 2 / 3 - GetSystemMetrics(SM_CXVSCROLL));

	CollectCommands();
	FillList(-1);
	return TRUE;
}

void CShortcutsDlg::OnAssign()
{
	int sel = GetSelectedCommand();
	if (sel < 0)
	{
		AfxMessageBox(_T("Select a command in the list first."), MB_ICONINFORMATION);
		return;
	}
	if (!m_key.IsKeyDefined())
	{
		AfxMessageBox(_T("Click in the shortcut box and press the key combination first."), MB_ICONINFORMATION);
		return;
	}

	ACCEL a = *m_key.GetAccel();
	a.fVirt |= FVIRTKEY | FNOINVERT;
	a.cmd = (WORD)m_commands[sel].id;

	for (std::vector<ACCEL>::iterator it = m_accels.begin(); it != m_accels.end(); ++it)
	{
		if (SameKey(*it, a))
		{
			if (it->cmd == a.cmd)
			{
				return;
			}
			CString msg;
			msg.Format(_T("%s is used by \"%s\".\n\nUse it for \"%s\" instead?"), (LPCTSTR)FormatAccel(a), (LPCTSTR)GetCommandName(it->cmd), (LPCTSTR)m_commands[sel].name);
			if (AfxMessageBox(msg, MB_YESNO | MB_ICONQUESTION) != IDYES)
			{
				return;
			}
			m_accels.erase(it);
			break;
		}
	}

	m_accels.push_back(a);
	m_key.ResetKey();
	FillList(sel);
}

void CShortcutsDlg::OnRemove()
{
	int sel = GetSelectedCommand();
	if (sel < 0)
	{
		return;
	}
	const UINT id = m_commands[sel].id;
	for (std::vector<ACCEL>::iterator it = m_accels.begin(); it != m_accels.end();)
	{
		if (it->cmd == id)
		{
			it = m_accels.erase(it);
		}
		else
		{
			++it;
		}
	}
	FillList(sel);
}

void CShortcutsDlg::OnReset()
{
	if (AfxMessageBox(_T("Reset all shortcuts to the ConCAD defaults?"), MB_YESNO | MB_ICONQUESTION) != IDYES)
	{
		return;
	}
	LoadDefaults(m_accels);
	FillList(GetSelectedCommand());
}

void CShortcutsDlg::OnOK()
{
	if (m_accels.empty())
	{
		AfxMessageBox(_T("Keep at least one shortcut (or use Reset All)."), MB_ICONINFORMATION);
		return;
	}

	CFrameWnd *pFrame = DYNAMIC_DOWNCAST(CFrameWnd, AfxGetMainWnd());
	if (afxKeyboardManager != NULL && pFrame != NULL)
	{
		afxKeyboardManager->UpdateAccelTable(NULL, &m_accels[0], (int)m_accels.size(), pFrame);
		CWinAppEx *pApp = DYNAMIC_DOWNCAST(CWinAppEx, AfxGetApp());
		if (pApp != NULL)
		{
			afxKeyboardManager->SaveState(pApp->GetRegSectionPath(), pFrame);
		}
	}

	CDialog::OnOK();
}
