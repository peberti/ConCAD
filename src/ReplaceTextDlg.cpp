/*
 * Project:		ConCAD (TinyCAD fork)
 * License:		Lesser GNU Public License 2.1 (LGPL)
 *				http://www.opensource.org/licenses/lgpl-license.html
 */

// Edit > Replace Text (CHANGES.md section 41)

#include "stdafx.h"
#include "ConCadView.h"
#include "ConCadDoc.h"
#include "MultiSheetDoc.h"
#include "ReplaceTextDlg.h"
#include "Log.h"
#include <set>

extern CConCadView *g_currentview;

static const TCHAR RegSection[] = _T("ReplaceText");

// Tick boxes: control and registry name
static const struct
{
	UINT id;
	const TCHAR *name;
	int def;
} Settings[] =
{
	{ IDC_RT_TEXT,      _T("Text"),      1 },
	{ IDC_RT_NOTES,     _T("Notes"),     1 },
	{ IDC_RT_LABELS,    _T("Labels"),    1 },
	{ IDC_RT_BUSNAMES,  _T("BusNames"),  1 },
	{ IDC_RT_POWER,     _T("Power"),     1 },
	{ IDC_RT_REFS,      _T("Refs"),      1 },
	{ IDC_RT_FIELDS,    _T("Fields"),    1 },
	{ IDC_RT_WHOLEWORD, _T("WholeWord"), 0 },
	{ IDC_RT_MATCHCASE, _T("MatchCase"), 0 },
};

CReplaceTextDlg *CReplaceTextDlg::s_pDlg = NULL;

BEGIN_MESSAGE_MAP(CReplaceTextDlg, CDialog)
	ON_BN_CLICKED(IDC_RT_FINDNEXT, OnFindNext)
	ON_BN_CLICKED(IDC_RT_REPLACEONE, OnReplace)
	ON_BN_CLICKED(IDC_RT_REPLACEALL, OnReplaceAll)
	ON_EN_CHANGE(IDC_RT_FIND, OnChangeFind)
END_MESSAGE_MAP()

CReplaceTextDlg::CReplaceTextDlg() :
	CDialog(IDD, NULL), m_pMatchDesign(NULL), m_matchPos(0), m_bHaveMatch(false)
{
	m_match.sheet = 0;
	m_match.obj = NULL;
	m_match.field = -1;
}

CReplaceTextDlg::~CReplaceTextDlg()
{
}

void CReplaceTextDlg::Show()
{
	if (s_pDlg == NULL)
	{
		s_pDlg = new CReplaceTextDlg();
		if (!s_pDlg->Create(IDD, AfxGetMainWnd()))
		{
			delete s_pDlg;
			s_pDlg = NULL;
			return;
		}
	}
	s_pDlg->ShowWindow(SW_SHOW);
	s_pDlg->SetForegroundWindow();
	s_pDlg->GotoDlgCtrl(s_pDlg->GetDlgItem(IDC_RT_FIND));
}

void CReplaceTextDlg::DoDataExchange(CDataExchange *pDX)
{
	CDialog::DoDataExchange(pDX);
}

BOOL CReplaceTextDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	CWinApp *pApp = AfxGetApp();
	for (int i = 0; i < _countof(Settings); ++i)
	{
		CheckDlgButton(Settings[i].id, pApp->GetProfileInt(RegSection, Settings[i].name, Settings[i].def) ? BST_CHECKED : BST_UNCHECKED);
	}
	const bool all = pApp->GetProfileInt(RegSection, _T("AllSheets"), 0) != 0;
	CheckRadioButton(IDC_RT_SHEET, IDC_RT_ALLSHEETS, all ? IDC_RT_ALLSHEETS : IDC_RT_SHEET);
	SetDlgItemText(IDC_RT_FIND, pApp->GetProfileString(RegSection, _T("Find")));
	SetDlgItemText(IDC_RT_REPLACE, pApp->GetProfileString(RegSection, _T("Replace")));
	return TRUE;
}

void CReplaceTextDlg::SaveSettings()
{
	CWinApp *pApp = AfxGetApp();
	for (int i = 0; i < _countof(Settings); ++i)
	{
		pApp->WriteProfileInt(RegSection, Settings[i].name, IsDlgButtonChecked(Settings[i].id) == BST_CHECKED ? 1 : 0);
	}
	pApp->WriteProfileInt(RegSection, _T("AllSheets"), IsDlgButtonChecked(IDC_RT_ALLSHEETS) == BST_CHECKED ? 1 : 0);
	CString s;
	GetDlgItemText(IDC_RT_FIND, s);
	pApp->WriteProfileString(RegSection, _T("Find"), s);
	GetDlgItemText(IDC_RT_REPLACE, s);
	pApp->WriteProfileString(RegSection, _T("Replace"), s);
}

void CReplaceTextDlg::OnCancel()
{
	SaveSettings();
	DestroyWindow();
}

void CReplaceTextDlg::PostNcDestroy()
{
	CDialog::PostNcDestroy();
	s_pDlg = NULL;
	delete this;
}

// The design of the active drawing window (NULL: none)
CMultiSheetDoc* CReplaceTextDlg::GetDesign()
{
	if (g_currentview == NULL || !::IsWindow(g_currentview->GetSafeHwnd()))
	{
		return NULL;
	}
	return g_currentview->GetDocument();
}

void CReplaceTextDlg::SetStatus(const CString &s)
{
	SetDlgItemText(IDC_RT_STATUS, s);
}

void CReplaceTextDlg::ReadOptions(CString &what, bool &wholeWord, bool &matchCase)
{
	GetDlgItemText(IDC_RT_FIND, what);
	wholeWord = IsDlgButtonChecked(IDC_RT_WHOLEWORD) == BST_CHECKED;
	matchCase = IsDlgButtonChecked(IDC_RT_MATCHCASE) == BST_CHECKED;
}

void CReplaceTextDlg::OnChangeFind()
{
	m_bHaveMatch = false;
	SetStatus(_T(""));
}

// Whether the object's text is searched (by the tick boxes), and which of a
// component's fields: firstField..lastField (-1..-1 = the object's text)
bool CReplaceTextDlg::IncludeObject(CDrawingObject *obj, int &firstField, int &lastField)
{
	firstField = lastField = -1;
	UINT box = 0;
	switch (obj->GetType())
	{
		case xText:
		case xTextEx:
		case xTextEx2:
			box = IDC_RT_TEXT;
			break;
		case xNoteText:
			box = IDC_RT_NOTES;
			break;
		case xLabel:
		case xLabelEx:
		case xLabelEx2:
			box = IDC_RT_LABELS;
			break;
		case xBusName:
		case xBusNameEx:
			box = IDC_RT_BUSNAMES;
			break;
		case xPower:
			box = IDC_RT_POWER;
			break;
		case xMethodEx3:
		case xHierarchicalSymbol:
		{
			const int count = static_cast<CDrawMethod*>(obj)->GetFieldCount();
			const bool refs = IsDlgButtonChecked(IDC_RT_REFS) == BST_CHECKED;
			const bool fields = IsDlgButtonChecked(IDC_RT_FIELDS) == BST_CHECKED;
			if (count == 0 || (!refs && !fields))
			{
				return false;
			}
			firstField = refs ? CDrawMethod::Ref : CDrawMethod::Ref + 1;
			lastField = fields ? count - 1 : CDrawMethod::Ref;
			return firstField <= lastField;
		}
		default:
			return false;
	}
	return IsDlgButtonChecked(box) == BST_CHECKED;
}

// All searchable texts in the scope, in drawing order, starting with the
// current sheet
void CReplaceTextDlg::CollectTargets(CMultiSheetDoc *pDesign, std::vector<STarget> &targets)
{
	targets.clear();
	const int sheets = pDesign->GetNumberOfSheets();
	const int current = pDesign->GetActiveSheetIndex();
	const bool all = IsDlgButtonChecked(IDC_RT_ALLSHEETS) == BST_CHECKED;
	for (int n = 0; n < (all ? sheets : 1); ++n)
	{
		const int sheet = (current + n) % sheets;
		CConCadDoc *pSheet = pDesign->GetSheet(sheet);
		if (pSheet == NULL)
		{
			continue;
		}
		for (drawingIterator it = pSheet->GetDrawingBegin(); it != pSheet->GetDrawingEnd(); ++it)
		{
			int first, last;
			if (!IncludeObject(*it, first, last))
			{
				continue;
			}
			for (int f = first; f <= last; ++f)
			{
				STarget t;
				t.sheet = sheet;
				t.obj = *it;
				t.field = f;
				targets.push_back(t);
			}
		}
	}
}

bool CReplaceTextDlg::GetTargetText(const STarget &t, CString &s)
{
	CDrawingObject *obj = t.obj;
	switch (obj->GetType())
	{
		case xText:
		case xTextEx:
		case xTextEx2:
		case xBusName:
		case xBusNameEx:
			s = static_cast<CDrawText*>(obj)->GetValue();
			return true;
		case xNoteText:
			s = static_cast<CDrawNoteText*>(obj)->GetValue();
			return true;
		case xLabel:
		case xLabelEx:
		case xLabelEx2:
			s = static_cast<CDrawLabel*>(obj)->GetValue();
			return true;
		case xPower:
			s = static_cast<CDrawPower*>(obj)->GetValue();
			return true;
		case xMethodEx3:
		case xHierarchicalSymbol:
			if (t.field >= 0 && t.field < static_cast<CDrawMethod*>(obj)->GetFieldCount())
			{
				s = static_cast<CDrawMethod*>(obj)->GetFieldValue(t.field);
				return true;
			}
			return false;
		default:
			return false;
	}
}

void CReplaceTextDlg::SetTargetText(const STarget &t, const CString &s)
{
	CDrawingObject *obj = t.obj;
	switch (obj->GetType())
	{
		case xText:
		case xTextEx:
		case xTextEx2:
		case xBusName:
		case xBusNameEx:
			static_cast<CDrawText*>(obj)->SetText(s);
			break;
		case xNoteText:
			static_cast<CDrawNoteText*>(obj)->SetText(s);
			break;
		case xLabel:
		case xLabelEx:
		case xLabelEx2:
			static_cast<CDrawLabel*>(obj)->SetText(s);
			break;
		case xPower:
			static_cast<CDrawPower*>(obj)->SetText(s);
			break;
		case xMethodEx3:
		case xHierarchicalSymbol:
			static_cast<CDrawMethod*>(obj)->SetFieldValue(t.field, s);
			break;
		default:
			break;
	}
}

static bool IsWordChar(TCHAR c)
{
	return _istalnum(c) || c == _T('_');
}

// Position of the first match at or after 'from', or -1
int CReplaceTextDlg::FindIn(const CString &s, const CString &what, int from, bool wholeWord, bool matchCase)
{
	if (what.IsEmpty() || from < 0)
	{
		return -1;
	}
	CString hay = s, needle = what;
	if (!matchCase)
	{
		hay.MakeLower();
		needle.MakeLower();
	}
	const int len = needle.GetLength();
	for (int pos = hay.Find(needle, from); pos >= 0; pos = hay.Find(needle, pos + 1))
	{
		if (!wholeWord)
		{
			return pos;
		}
		const bool startOk = pos == 0 || !IsWordChar(s[pos - 1]);
		const bool endOk = pos + len >= s.GetLength() || !IsWordChar(s[pos + len]);
		if (startOk && endOk)
		{
			return pos;
		}
	}
	return -1;
}

// Select the matched object and bring it into view
void CReplaceTextDlg::ShowMatch(CMultiSheetDoc *pDesign)
{
	pDesign->GetCurrentSheet()->SelectObject(new CDrawEditItem(pDesign->GetCurrentSheet()));
	pDesign->SelectERCSheetView(m_match.sheet, m_match.obj);
	CConCadDoc *pSheet = pDesign->GetSheet(m_match.sheet);
	pSheet->UnSelect();
	pSheet->Select(m_match.obj);
	pSheet->Invalidate();

	CString where = m_match.obj->GetName();
	if (m_match.field >= 0)
	{
		CDrawMethod *pMethod = static_cast<CDrawMethod*>(m_match.obj);
		where.Format(_T("%s %s, %s"), (LPCTSTR)m_match.obj->GetName(), (LPCTSTR)pMethod->GetField(CDrawMethod::Ref),
			(LPCTSTR)pMethod->GetFieldName(m_match.field));
	}
	CString status;
	status.Format(_T("Found: %s on %s"), (LPCTSTR)where, (LPCTSTR)pDesign->GetSheetName(m_match.sheet));
	SetStatus(status);
}

// Find the next match after the current one (from the start of the
// current sheet when there is none), wrapping around
bool CReplaceTextDlg::FindNext(bool bReport)
{
	CMultiSheetDoc *pDesign = GetDesign();
	if (pDesign == NULL)
	{
		SetStatus(_T("No design is open."));
		return false;
	}
	CString what;
	bool wholeWord, matchCase;
	ReadOptions(what, wholeWord, matchCase);
	if (what.IsEmpty())
	{
		SetStatus(_T("Enter the text to find."));
		return false;
	}

	std::vector<STarget> targets;
	CollectTargets(pDesign, targets);
	const int n = static_cast<int>(targets.size());

	int start = 0, startPos = 0;
	if (m_bHaveMatch && m_pMatchDesign == pDesign)
	{
		for (int i = 0; i < n; ++i)
		{
			if (targets[i].obj == m_match.obj && targets[i].field == m_match.field && targets[i].sheet == m_match.sheet)
			{
				start = i;
				startPos = m_matchPos;
				break;
			}
		}
	}

	for (int i = 0; i <= n && n > 0; ++i)
	{
		const int idx = (start + i) % n;
		CString s;
		if (!GetTargetText(targets[idx], s))
		{
			continue;
		}
		const int pos = FindIn(s, what, i == 0 ? startPos : 0, wholeWord, matchCase);
		if (pos >= 0 && (i < n || pos < startPos))
		{
			m_pMatchDesign = pDesign;
			m_match = targets[idx];
			m_matchPos = pos;
			m_bHaveMatch = true;
			ShowMatch(pDesign);
			// The next search starts after this match
			m_matchPos = pos;
			return true;
		}
	}

	m_bHaveMatch = false;
	if (bReport)
	{
		SetStatus(_T("Not found."));
	}
	return false;
}

void CReplaceTextDlg::OnFindNext()
{
	// Continue after the current match
	if (m_bHaveMatch)
	{
		++m_matchPos;
	}
	FindNext(true);
}

// Replace the current match (if it is still there), then find the next
void CReplaceTextDlg::OnReplace()
{
	CMultiSheetDoc *pDesign = GetDesign();
	CString what, with;
	bool wholeWord, matchCase;
	ReadOptions(what, wholeWord, matchCase);
	GetDlgItemText(IDC_RT_REPLACE, with);

	bool replaced = false;
	if (m_bHaveMatch && pDesign != NULL && m_pMatchDesign == pDesign)
	{
		// The match must still exist and be in the scope
		std::vector<STarget> targets;
		CollectTargets(pDesign, targets);
		CString s;
		for (size_t i = 0; i < targets.size(); ++i)
		{
			const STarget &t = targets[i];
			if (t.obj == m_match.obj && t.field == m_match.field && t.sheet == m_match.sheet
				&& GetTargetText(t, s) && FindIn(s, what, m_matchPos, wholeWord, matchCase) == m_matchPos)
			{
				CConCadDoc *pSheet = pDesign->GetSheet(t.sheet);
				pSheet->BeginNewChangeSet();
				pSheet->MarkChangeForUndo(t.obj);
				SetTargetText(t, s.Left(m_matchPos) + with + s.Mid(m_matchPos + what.GetLength()));
				pSheet->SetModifiedFlag(TRUE);
				pSheet->Invalidate();
				replaced = true;
				// Continue after the replacement (it may contain the text)
				m_matchPos += with.GetLength();
				break;
			}
		}
	}
	if (!replaced)
	{
		m_bHaveMatch = false;
	}
	if (!FindNext(true) && replaced)
	{
		SetStatus(_T("Replaced. No more matches."));
	}
}

// Replace every match in the scope; one undo step per sheet
void CReplaceTextDlg::OnReplaceAll()
{
	CMultiSheetDoc *pDesign = GetDesign();
	if (pDesign == NULL)
	{
		SetStatus(_T("No design is open."));
		return;
	}
	CString what, with;
	bool wholeWord, matchCase;
	ReadOptions(what, wholeWord, matchCase);
	GetDlgItemText(IDC_RT_REPLACE, with);
	if (what.IsEmpty())
	{
		SetStatus(_T("Enter the text to find."));
		return;
	}

	pDesign->GetCurrentSheet()->SelectObject(new CDrawEditItem(pDesign->GetCurrentSheet()));

	std::vector<STarget> targets;
	CollectTargets(pDesign, targets);

	int occurrences = 0;
	std::set<CDrawingObject*> changedObjects;
	std::set<int> changedSheets;
	for (size_t i = 0; i < targets.size(); ++i)
	{
		const STarget &t = targets[i];
		CString s;
		if (!GetTargetText(t, s))
		{
			continue;
		}
		CString result;
		int from = 0, count = 0;
		for (int pos = FindIn(s, what, 0, wholeWord, matchCase); pos >= 0; pos = FindIn(s, what, from, wholeWord, matchCase))
		{
			result += s.Mid(from, pos - from) + with;
			from = pos + what.GetLength();
			++count;
		}
		if (count == 0)
		{
			continue;
		}
		result += s.Mid(from);

		CConCadDoc *pSheet = pDesign->GetSheet(t.sheet);
		if (changedSheets.insert(t.sheet).second)
		{
			pSheet->UnSelect();
			pSheet->BeginNewChangeSet();
		}
		if (changedObjects.insert(t.obj).second)
		{
			pSheet->MarkChangeForUndo(t.obj);
		}
		SetTargetText(t, result);
		occurrences += count;
	}

	for (std::set<int>::iterator it = changedSheets.begin(); it != changedSheets.end(); ++it)
	{
		pDesign->GetSheet(*it)->SetModifiedFlag(TRUE);
		pDesign->GetSheet(*it)->Invalidate();
	}

	m_bHaveMatch = false;
	CString status;
	if (occurrences == 0)
	{
		status = _T("Not found.");
	}
	else
	{
		status.Format(_T("Replaced %d occurrence(s) in %d object(s) on %d sheet(s)."),
			occurrences, static_cast<int>(changedObjects.size()), static_cast<int>(changedSheets.size()));
		CLog::Write(_T("Replace Text: %s"), (LPCTSTR)status);
	}
	SetStatus(status);
}
