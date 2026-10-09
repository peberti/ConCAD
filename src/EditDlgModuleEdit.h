/*
 * ConCAD: Tool Options panel for a selected module (its parameters)
 * License:		Lesser GNU Public License 2.1 (LGPL)
 */

#pragma once

#include "Diag.h"

class CDrawModuleInfo;

// Shows Name, Reference and the module's own fields; a changed value is
// shown at once by the module's texts that contain {field}.  It keeps the
// group id, not the object, and looks the parameters up on every use, so
// undo or delete cannot leave it with a stale pointer.
class CEditDlgModuleEdit: public CEditDlg
{
public:
	CEditDlgModuleEdit();

	enum
	{
		IDD = IDD_MODULE_EDIT
	};

	void Create();
	void Open(CConCadDoc *pDesign, CDrawModuleInfo *pInfo);
	bool IsShowing(CConCadDoc *pDesign, int group) const;
	void CloseIfShowing();

	// Stop Enter closing this dialog
	void OnOK();

protected:
	CListCtrl m_list;
	CButton m_Delete;
	CEdit m_edit_control;
	BOOL m_capture;
	int m_index;
	int m_column;
	int m_group;

	CDrawModuleInfo *GetInfo();
	void ReadFields();
	void BeginEdit(int index, int column);
	void EndEdit();
	void Changed();

	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	afx_msg void OnClickList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnAdd();
	afx_msg void OnDelete();
	afx_msg void OnKillfocusEdit();
	DECLARE_MESSAGE_MAP()
};
