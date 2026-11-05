/*
 * ConCAD: File -> Export BoM to Excel
 * License:		Lesser GNU Public License 2.1 (LGPL)
 */

#include "stdafx.h"
#include <algorithm>
#include <map>
#include <shlobj.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#include "resource.h"
#include "ConCad.h"
#include "ConCadDoc.h"
#include "ConCadMultiDoc.h"
#include "DrawModuleInfo.h"
#include "ExportBom.h"
#include "Log.h"

static const TCHAR* const RegSection = _T("BoM");
static const TCHAR* const DefaultFileName = _T("BOM_{Title}_{Revision}");
static const TCHAR* const DefaultTemplate = _T("ConCAD-BOM");

//-------------------------------------------------------------------------
// Collecting

static CString ModuleField(CDrawModuleInfo* info, const TCHAR* name)
{
	for (size_t i = CDrawModuleInfo::FixedFields; i < info->m_fields.size(); i++)
	{
		if (info->m_fields[i].name.CompareNoCase(name) == 0)
		{
			CString v = info->Resolve(info->m_fields[i].value);
			v.Trim();
			return v;
		}
	}
	return CString();
}

static bool ItemLess(const CDlgExportBom::Item& a, const CDlgExportBom::Item& b)
{
	return StrCmpLogicalW(a.partnumber, b.partnumber) < 0;
}

// One row per Partnumber of the placed modules on all sheets
static void CollectItems(CConCadMultiDoc* pDesign, std::vector<CDlgExportBom::Item>& items, int& modules, int& missing)
{
	std::map<CString, size_t> index;
	modules = 0;
	missing = 0;
	for (int s = 0; s < pDesign->GetNumberOfSheets(); s++)
	{
		CConCadDoc* sheet = pDesign->GetSheet(s);
		for (drawingIterator it = sheet->GetDrawingBegin(); it != sheet->GetDrawingEnd(); ++it)
		{
			if ((*it)->GetType() != xModuleInfo)
			{
				continue;
			}
			CDrawModuleInfo* info = static_cast<CDrawModuleInfo*> (*it);
			modules++;
			const CString pn = ModuleField(info, _T("Partnumber"));
			if (pn.IsEmpty())
			{
				missing++;
				continue;
			}
			std::map<CString, size_t>::iterator found = index.find(pn);
			if (found != index.end())
			{
				items[found->second].qty++;
				continue;
			}
			CDlgExportBom::Item item;
			item.partnumber = pn;
			item.type = ModuleField(info, _T("Type"));
			item.name = info->m_fields.empty() ? CString() : info->Resolve(info->m_fields[0].value);
			item.name.Trim();
			item.qty = 1;
			index[pn] = items.size();
			items.push_back(item);
		}
	}
	std::sort(items.begin(), items.end(), ItemLess);
}

// The design variables for the template and the file name
static void CollectTokens(CConCadMultiDoc* pDesign, CDlgExportBom::TokenList& tokens)
{
	CDetails& d = pDesign->GetSheet(0)->GetDetails();
	static const TCHAR* const builtIn[] =
	{
		_T("Title"), _T("Author"), _T("Revision"), _T("DocNo"), _T("Organisation"),
		_T("Date"), _T("Description"), _T("RevisedBy")
	};
	for (int i = 0; i < _countof(builtIn); i++)
	{
		const CString token = CString(_T("{")) + builtIn[i] + _T("}");
		CString v = d.Resolve(token);
		tokens.push_back(std::make_pair(CString(builtIn[i]), v == token ? CString() : v));
	}
	const CDetailsTokenMap& user = d.GetUserTokens();
	for (CDetailsTokenMap::const_iterator it = user.begin(); it != user.end(); ++it)
	{
		const CString token = _T("{") + it->first + _T("}");
		CString v = d.Resolve(token);
		tokens.push_back(std::make_pair(it->first, v == token ? CString() : v));
	}
	tokens.push_back(std::make_pair(CString(_T("Today")), CTime::GetCurrentTime().Format(_T("%Y-%m-%d"))));
	CString file = pDesign->GetPathName();
	file = file.Mid(file.ReverseFind(_T('\\')) + 1);
	if (file.ReverseFind(_T('.')) > 0)
	{
		file = file.Left(file.ReverseFind(_T('.')));
	}
	tokens.push_back(std::make_pair(CString(_T("FileName")), file));
}

static CString Lookup(const CDlgExportBom::TokenList& tokens, const CString& name)
{
	// Later entries (user variables) win over the built-ins
	CString value;
	for (size_t i = 0; i < tokens.size(); i++)
	{
		if (tokens[i].first.CompareNoCase(name) == 0)
		{
			value = tokens[i].second;
		}
	}
	return value;
}

// File name from the pattern: variables filled in (unknown/empty ones give
// nothing), characters Windows does not allow replaced
static CString ResolveFileName(const CString& pattern, const CDlgExportBom::TokenList& tokens)
{
	CString out;
	for (int i = 0; i < pattern.GetLength(); i++)
	{
		if (pattern[i] == _T('{'))
		{
			int j = pattern.Find(_T('}'), i + 1);
			if (j > i)
			{
				CString name = pattern.Mid(i + 1, j - i - 1);
				name.Trim();
				out += Lookup(tokens, name);
				i = j;
				continue;
			}
		}
		out += pattern[i];
	}
	for (int i = 0; i < out.GetLength(); i++)
	{
		if (_tcschr(_T("\\/:*?\"<>|"), out[i]) != NULL || out[i] < 32)
		{
			out.SetAt(i, _T('_'));
		}
	}
	while (out.Replace(_T("__"), _T("_")) > 0)
	{
	}
	out.Trim(_T(" ._-"));
	if (out.IsEmpty())
	{
		out = _T("BOM");
	}
	if (out.Right(5).CompareNoCase(_T(".xlsx")) != 0)
	{
		out += _T(".xlsx");
	}
	return out;
}

static CString XmlAttr(const CString& s)
{
	CString out;
	for (int i = 0; i < s.GetLength(); i++)
	{
		TCHAR c = s[i];
		switch (c)
		{
		case _T('&'): out += _T("&amp;"); break;
		case _T('<'): out += _T("&lt;"); break;
		case _T('>'): out += _T("&gt;"); break;
		case _T('"'): out += _T("&quot;"); break;
		case _T('\r'): out += _T("&#13;"); break;
		case _T('\n'): out += _T("&#10;"); break;
		case _T('\t'): out += _T("&#9;"); break;
		default:
			if (c >= 32)
			{
				out += c;
			}
		}
	}
	return out;
}

static bool WriteUtf8(const CString& path, const CString& text)
{
	CStringA data(CW2A(text, CP_UTF8));
	CFile f;
	if (!f.Open(path, CFile::modeCreate | CFile::modeWrite))
	{
		return false;
	}
	f.Write((LPCSTR) data, data.GetLength());
	f.Close();
	return true;
}

static CString ReadUtf8(const CString& path)
{
	CFile f;
	if (!f.Open(path, CFile::modeRead | CFile::shareDenyNone))
	{
		return CString();
	}
	const UINT size = (UINT) f.GetLength();
	CStringA data;
	f.Read(data.GetBuffer(size + 1), size);
	data.ReleaseBuffer(size);
	if (data.Left(3) == "\xEF\xBB\xBF")
	{
		data = data.Mid(3);
	}
	return CString(CA2W(data, CP_UTF8));
}

// Run tools/BomExport; empty result = done, else the error message
static CString RunHelper(const CString& templ, const CString& output,
	const std::vector<CDlgExportBom::Item>& items, const CDlgExportBom::TokenList& tokens)
{
	const CString exe = CConCadApp::FindTool(_T("BomExport.exe"), _T("BomExport"));
	if (exe.IsEmpty())
	{
		return _T("The BoM export program (BomExport.exe) was not found next to ConCAD.exe.");
	}

	TCHAR tempDir[MAX_PATH];
	GetTempPath(MAX_PATH, tempDir);
	CString input;
	input.Format(_T("%sConCAD-BOM-%u.xml"), tempDir, GetTickCount());

	CString xml = _T("<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n");
	xml += _T("<BomExport template=\"") + XmlAttr(templ) + _T("\" output=\"") + XmlAttr(output) + _T("\">\r\n");
	for (size_t i = 0; i < tokens.size(); i++)
	{
		xml += _T("  <Token name=\"") + XmlAttr(tokens[i].first) + _T("\" value=\"") + XmlAttr(tokens[i].second) + _T("\"/>\r\n");
	}
	for (size_t i = 0; i < items.size(); i++)
	{
		CString qty;
		qty.Format(_T("%d"), items[i].qty);
		xml += _T("  <Item partnumber=\"") + XmlAttr(items[i].partnumber) + _T("\" type=\"") + XmlAttr(items[i].type)
			+ _T("\" name=\"") + XmlAttr(items[i].name) + _T("\" qty=\"") + qty + _T("\"/>\r\n");
	}
	xml += _T("</BomExport>\r\n");
	if (!WriteUtf8(input, xml))
	{
		return _T("The temporary file could not be written:\r\n") + input;
	}

	CString cmd;
	cmd.Format(_T("\"%s\" \"%s\""), (LPCTSTR) exe, (LPCTSTR) input);
	STARTUPINFO si = { sizeof(si) };
	PROCESS_INFORMATION pi = {};
	CString error;
	BOOL started = CreateProcess(exe, cmd.GetBuffer(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
	cmd.ReleaseBuffer();
	if (!started)
	{
		error.Format(_T("The BoM export program could not be started (error %u)."), GetLastError());
	}
	else
	{
		CWaitCursor wait;
		DWORD code = 1;
		if (WaitForSingleObject(pi.hProcess, 120000) != WAIT_OBJECT_0)
		{
			TerminateProcess(pi.hProcess, 1);
			error = _T("The BoM export did not finish within 2 minutes.");
		}
		else if (GetExitCodeProcess(pi.hProcess, &code) && code != 0)
		{
			error = ReadUtf8(input + _T(".err"));
			if (error.IsEmpty())
			{
				error.Format(_T("The BoM export failed (exit code %u)."), code);
			}
		}
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}
	DeleteFile(input);
	DeleteFile(input + _T(".err"));
	return error;
}

//-------------------------------------------------------------------------

void CDlgExportBom::Export(CConCadMultiDoc* pDesign)
{
	std::vector<Item> items;
	int modules = 0, missing = 0;
	CollectItems(pDesign, items, modules, missing);
	if (items.empty())
	{
		AfxMessageBox(modules == 0
			? _T("There is no placed module in this design: the BoM lists the Partnumber of the placed modules.")
			: _T("None of the placed modules has a Partnumber (Module Tool Options)."), MB_ICONINFORMATION);
		return;
	}

	int total = 0;
	for (size_t i = 0; i < items.size(); i++)
	{
		total += items[i].qty;
	}
	CString summary;
	summary.Format(_T("%d part number(s) from %d placed module(s)."), (int) items.size(), total);
	if (missing > 0)
	{
		CString s;
		s.Format(_T(" %d module(s) without a Partnumber are left out."), missing);
		summary += s;
	}

	TokenList tokens;
	CollectTokens(pDesign, tokens);

	CDlgExportBom dlg(pDesign, tokens, summary);
	if (dlg.DoModal() != IDOK)
	{
		return;
	}

	const CString error = RunHelper(dlg.m_template, dlg.m_output, items, tokens);
	if (!error.IsEmpty())
	{
		CLog::Write(_T("BoM export to %s failed: %s"), (LPCTSTR) dlg.m_output, (LPCTSTR) error);
		AfxMessageBox(_T("The BoM could not be exported.\r\n\r\n") + error, MB_ICONEXCLAMATION);
		return;
	}
	CLog::Write(_T("BoM exported: %s (%d part numbers)"), (LPCTSTR) dlg.m_output, (int) items.size());
	if (dlg.m_open)
	{
		ShellExecute(AfxGetMainWnd()->GetSafeHwnd(), _T("open"), dlg.m_output, NULL, NULL, SW_SHOWNORMAL);
	}
	else
	{
		AfxMessageBox(_T("The BoM was saved as\r\n") + dlg.m_output, MB_ICONINFORMATION);
	}
}

//-------------------------------------------------------------------------
// The dialog

BEGIN_MESSAGE_MAP(CDlgExportBom, CDialog)
	ON_BN_CLICKED(IDC_BOM_TEMPLATE_BROWSE, OnTemplateBrowse)
	ON_BN_CLICKED(IDC_BOM_FOLDER_BROWSE, OnFolderBrowse)
	ON_EN_CHANGE(IDC_BOM_FILENAME, OnFileNameChange)
END_MESSAGE_MAP()

CDlgExportBom::CDlgExportBom(CConCadMultiDoc* pDesign, const TokenList& tokens, const CString& summary) :
	CDialog(IDD_EXPORT_BOMXL, AfxGetMainWnd()), m_tokens(tokens)
{
	m_pDesign = pDesign;
	m_summary = summary;
	m_open = TRUE;
}

int CDlgExportBom::AddTemplate(const CString& label, const CString& path)
{
	for (size_t i = 0; i < m_templates.size(); i++)
	{
		if (m_templates[i].path.CompareNoCase(path) == 0)
		{
			return (int) i;
		}
	}
	Template t;
	t.label = label;
	t.path = path;
	m_templates.push_back(t);
	return (int) m_templates.size() - 1;
}

void CDlgExportBom::AddTemplates(const CString& folder, const CString& suffix)
{
	TCHAR full[MAX_PATH];
	if (!PathCanonicalize(full, folder))
	{
		return;
	}
	CFileFind finder;
	BOOL more = finder.FindFile(CString(full) + _T("\\*.xlsx"));
	while (more)
	{
		more = finder.FindNextFile();
		if (finder.IsDirectory() || finder.GetFileName().Left(2) == _T("~$")) // Excel's lock files
		{
			continue;
		}
		AddTemplate(finder.GetFileTitle() + suffix, finder.GetFilePath());
	}
}

// Bundled templates (templates\bom next to ConCAD.exe; ..\templates\bom in a
// build tree) and the user's (%APPDATA%\ConCAD\templates\bom)
void CDlgExportBom::FindTemplates()
{
	const CString mainDir = CConCadApp::GetMainDir();
	AddTemplates(mainDir + _T("templates\\bom"), CString());
	AddTemplates(mainDir + _T("..\\templates\\bom"), CString());
	TCHAR appData[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, appData)))
	{
		AddTemplates(CString(appData) + _T("\\ConCAD\\templates\\bom"), _T(" (user)"));
	}
}

BOOL CDlgExportBom::OnInitDialog()
{
	CDialog::OnInitDialog();
	SetDlgItemText(IDC_BOM_SUMMARY, m_summary);

	FindTemplates();
	CWinApp* app = AfxGetApp();
	const CString last = app->GetProfileString(RegSection, _T("Template"));
	int select = -1;
	if (!last.IsEmpty() && GetFileAttributes(last) != INVALID_FILE_ATTRIBUTES)
	{
		CString title = last.Mid(last.ReverseFind(_T('\\')) + 1);
		select = AddTemplate(title.Left(title.GetLength() - 5), last);
	}
	CComboBox* combo = static_cast<CComboBox*> (GetDlgItem(IDC_BOM_TEMPLATE));
	for (size_t i = 0; i < m_templates.size(); i++)
	{
		int at = combo->AddString(m_templates[i].label);
		combo->SetItemData(at, i);
		if (select < 0 && m_templates[i].label.CompareNoCase(DefaultTemplate) == 0)
		{
			select = (int) i;
		}
	}
	for (int i = 0; i < combo->GetCount(); i++)
	{
		if ((int) combo->GetItemData(i) == (select < 0 ? 0 : select))
		{
			combo->SetCurSel(i);
		}
	}

	SetDlgItemText(IDC_BOM_FILENAME, app->GetProfileString(RegSection, _T("FileName"), DefaultFileName));

	// Default folder: the design's
	CString folder = m_pDesign->GetPathName();
	if (folder.ReverseFind(_T('\\')) > 0)
	{
		folder = folder.Left(folder.ReverseFind(_T('\\')));
	}
	else
	{
		TCHAR docs[MAX_PATH];
		folder = SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, 0, docs)) ? CString(docs) : CString();
	}
	SetDlgItemText(IDC_BOM_FOLDER, folder);
	CheckDlgButton(IDC_BOM_OPEN, app->GetProfileInt(RegSection, _T("Open"), 1) ? BST_CHECKED : BST_UNCHECKED);
	OnFileNameChange();
	return TRUE;
}

CString CDlgExportBom::OutputName() const
{
	CString pattern;
	GetDlgItemText(IDC_BOM_FILENAME, pattern);
	return ResolveFileName(pattern, m_tokens);
}

void CDlgExportBom::OnFileNameChange()
{
	SetDlgItemText(IDC_BOM_PREVIEW, _T("\x2192 ") + OutputName());
}

void CDlgExportBom::OnTemplateBrowse()
{
	CFileDialog dlg(TRUE, _T("xlsx"), NULL, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
		_T("Excel workbook (*.xlsx)|*.xlsx||"), this);
	if (dlg.DoModal() != IDOK)
	{
		return;
	}
	const int index = AddTemplate(dlg.GetFileTitle(), dlg.GetPathName());
	CComboBox* combo = static_cast<CComboBox*> (GetDlgItem(IDC_BOM_TEMPLATE));
	for (int i = 0; i < combo->GetCount(); i++)
	{
		if ((int) combo->GetItemData(i) == index)
		{
			combo->SetCurSel(i);
			return;
		}
	}
	int at = combo->AddString(m_templates[index].label);
	combo->SetItemData(at, index);
	combo->SetCurSel(at);
}

void CDlgExportBom::OnFolderBrowse()
{
	CString folder;
	GetDlgItemText(IDC_BOM_FOLDER, folder);
	CFolderPickerDialog dlg(folder, 0, this);
	if (dlg.DoModal() == IDOK)
	{
		SetDlgItemText(IDC_BOM_FOLDER, dlg.GetPathName());
	}
}

void CDlgExportBom::OnOK()
{
	CComboBox* combo = static_cast<CComboBox*> (GetDlgItem(IDC_BOM_TEMPLATE));
	if (combo->GetCurSel() < 0)
	{
		AfxMessageBox(_T("Choose a BoM template (an .xlsx file with a {BOM_Partnumber} row)."), MB_ICONINFORMATION);
		return;
	}
	m_template = m_templates[combo->GetItemData(combo->GetCurSel())].path;

	CString folder;
	GetDlgItemText(IDC_BOM_FOLDER, folder);
	folder.Trim();
	if (folder.IsEmpty() || !PathIsDirectory(folder))
	{
		AfxMessageBox(_T("The folder does not exist:\r\n") + folder, MB_ICONEXCLAMATION);
		return;
	}
	if (folder.Right(1) != _T("\\"))
	{
		folder += _T("\\");
	}
	m_output = folder + OutputName();
	if (m_output.CompareNoCase(m_template) == 0)
	{
		AfxMessageBox(_T("The BoM cannot replace its own template."), MB_ICONEXCLAMATION);
		return;
	}
	if (GetFileAttributes(m_output) != INVALID_FILE_ATTRIBUTES
		&& AfxMessageBox(m_output + _T("\r\nalready exists. Replace it?"), MB_YESNO | MB_ICONQUESTION) != IDYES)
	{
		return;
	}
	m_open = IsDlgButtonChecked(IDC_BOM_OPEN) == BST_CHECKED;

	CString pattern;
	GetDlgItemText(IDC_BOM_FILENAME, pattern);
	CWinApp* app = AfxGetApp();
	app->WriteProfileString(RegSection, _T("Template"), m_template);
	app->WriteProfileString(RegSection, _T("FileName"), pattern);
	app->WriteProfileInt(RegSection, _T("Open"), m_open ? 1 : 0);
	CDialog::OnOK();
}
