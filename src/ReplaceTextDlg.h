/*
 * Project:		ConCAD (TinyCAD fork)
 * License:		Lesser GNU Public License 2.1 (LGPL)
 *				http://www.opensource.org/licenses/lgpl-license.html
 */

// Edit > Replace Text: find and replace text in free texts, notes, labels,
// bus names, power names and component references / fields, on the
// current sheet or on all sheets.  Modeless; it works on the design of
// the active drawing window.

#pragma once

#include "resource.h"
#include <vector>

class CMultiSheetDoc;
class CConCadDoc;
class CDrawingObject;

class CReplaceTextDlg: public CDialog
{
public:
	// Open (or bring to front) the single Replace Text window
	static void Show();

	enum { IDD = IDD_REPLACE_TEXT };

private:
	CReplaceTextDlg();
	virtual ~CReplaceTextDlg();

	// One searchable text: an object's text, or one field of a component
	struct STarget
	{
		int sheet;
		CDrawingObject *obj;
		int field;          // -1 = the object's own text
	};

	static CReplaceTextDlg *s_pDlg;

	// The current match (Find Next), checked again before Replace
	CMultiSheetDoc *m_pMatchDesign;
	STarget m_match;
	int m_matchPos;
	bool m_bHaveMatch;

	CMultiSheetDoc* GetDesign();
	void ReadOptions(CString &what, bool &wholeWord, bool &matchCase);
	void CollectTargets(CMultiSheetDoc *pDesign, std::vector<STarget> &targets);
	bool IncludeObject(CDrawingObject *obj, int &firstField, int &lastField);
	static bool GetTargetText(const STarget &t, CString &s);
	static void SetTargetText(const STarget &t, const CString &s);
	static int FindIn(const CString &s, const CString &what, int from, bool wholeWord, bool matchCase);
	bool FindNext(bool bReport);
	void ShowMatch(CMultiSheetDoc *pDesign);
	void SetStatus(const CString &s);
	void SaveSettings();

protected:
	virtual void DoDataExchange(CDataExchange *pDX);
	virtual BOOL OnInitDialog();
	virtual void OnCancel();
	virtual void PostNcDestroy();

	afx_msg void OnFindNext();
	afx_msg void OnReplace();
	afx_msg void OnReplaceAll();
	afx_msg void OnChangeFind();
	DECLARE_MESSAGE_MAP()
};
