/*
 * ConCAD: Options -> Keyboard Shortcuts dialog
 * License:		Lesser GNU Public License 2.1 (LGPL)
 */

#pragma once

#include <vector>
#include "resource.h"

// Lists the commands (from the menus and toolbars) with their shortcuts and
// lets the user assign or remove shortcuts.  The table belongs to MFC's
// keyboard manager, which saves it in the registry.
class CShortcutsDlg: public CDialog
{
public:
	CShortcutsDlg(CWnd* pParent = NULL);

	enum
	{
		IDD = IDD_SHORTCUTS
	};

	// The accelerator table in ConCad.rc
	static void LoadDefaults(std::vector<ACCEL> &accels);
	// e.g. "Ctrl+Shift+G"
	static CString FormatAccel(const ACCEL &a);
	static bool SameKey(const ACCEL &a, const ACCEL &b);
	// Text for a menu item: the command's preferred shortcut, or empty
	static CString GetShortcutText(HACCEL hAccel, UINT id);

protected:
	struct Command
	{
		UINT id;
		CString name;
	};
	std::vector<Command> m_commands;
	std::vector<ACCEL> m_accels;
	CListCtrl m_list;
	CMFCAcceleratorKeyAssignCtrl m_key;

	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual void OnOK();

	void CollectCommands();
	void AddMenu(CMenu *pMenu, const CString &prefix);
	void AddCommand(UINT id, const CString &name);
	CString GetCommandName(UINT id) const;
	CString ShortcutsFor(UINT id) const;
	void FillList(int select);
	int GetSelectedCommand();

	afx_msg void OnAssign();
	afx_msg void OnRemove();
	afx_msg void OnReset();
	DECLARE_MESSAGE_MAP()
};
