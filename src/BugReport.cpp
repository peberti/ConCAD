/*
 * ConCAD: Help -> Report a Bug
 * License:		Lesser GNU Public License 2.1 (LGPL)
 */

#include "stdafx.h"
#include <shlobj.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#include <MAPI.h>
#include <afxole.h>
#include "resource.h"
#include "BugReport.h"
#include "Log.h"
#include "ConCad.h"
#include "ConCadMultiDoc.h"
#include "BuildId.h"

// Where reports go; HKCU\Software\ConCAD\ConCAD\BugReport\Address overrides it
static const TCHAR* const DefaultAddress = _T("per.bertilsson@consat.se");

//-------------------------------------------------------------------------
// A minimal zip writer (files stored uncompressed, UTF-8 names)

static DWORD Crc32(const BYTE* data, size_t size)
{
	static DWORD table[256];
	static bool ready = false;
	if (!ready)
	{
		for (DWORD n = 0; n < 256; n++)
		{
			DWORD c = n;
			for (int k = 0; k < 8; k++)
			{
				c = (c & 1) ? 0xEDB88320 ^ (c >> 1) : c >> 1;
			}
			table[n] = c;
		}
		ready = true;
	}
	DWORD crc = 0xFFFFFFFF;
	for (size_t i = 0; i < size; i++)
	{
		crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
	}
	return crc ^ 0xFFFFFFFF;
}

class CZipWriter
{
public:
	bool Open(const CString& path)
	{
		return m_file.Open(path, CFile::modeCreate | CFile::modeWrite) != FALSE;
	}

	bool AddData(const CString& name, const BYTE* data, size_t size)
	{
		Entry e;
		e.name = Utf8(name);
		e.crc = Crc32(data, size);
		e.size = (DWORD) size;
		e.packedSize = (DWORD) size;
		e.offset = (DWORD) m_file.GetPosition();
		DosTime(e.time, e.date);

		WriteHeader(e, false);
		if (size > 0)
		{
			m_file.Write(data, (UINT) size);
		}
		m_entries.push_back(e);
		return true;
	}

	bool AddFile(const CString& name, const CString& path)
	{
		CFile f;
		if (!f.Open(path, CFile::modeRead | CFile::shareDenyNone)) // the log is open for writing
		{
			return false;
		}
		const size_t size = (size_t) f.GetLength();
		std::vector<BYTE> data(size + 1);
		const size_t read = f.Read(&data[0], (UINT) size);
		return AddData(name, &data[0], read);
	}

	void Close()
	{
		const DWORD start = (DWORD) m_file.GetPosition();
		for (size_t i = 0; i < m_entries.size(); i++)
		{
			WriteHeader(m_entries[i], true);
		}
		const DWORD length = (DWORD) m_file.GetPosition() - start;
		Put32(0x06054b50);
		Put16(0);
		Put16(0);
		Put16((WORD) m_entries.size());
		Put16((WORD) m_entries.size());
		Put32(length);
		Put32(start);
		Put16(0);
		m_file.Close();
	}

private:
	struct Entry
	{
		CStringA name;
		DWORD crc, size, packedSize, offset;
		WORD time, date;
	};
	CFile m_file;
	std::vector<Entry> m_entries;

	static CStringA Utf8(const CString& s)
	{
		CStringA name(CW2A(s, CP_UTF8));
		name.Replace('\\', '/');
		return name;
	}

	static void DosTime(WORD& time, WORD& date)
	{
		SYSTEMTIME t;
		::GetLocalTime(&t);
		time = (WORD) ((t.wHour << 11) | (t.wMinute << 5) | (t.wSecond / 2));
		date = (WORD) (((t.wYear - 1980) << 9) | (t.wMonth << 5) | t.wDay);
	}

	void Put16(WORD v)
	{
		m_file.Write(&v, 2);
	}
	void Put32(DWORD v)
	{
		m_file.Write(&v, 4);
	}

	void WriteHeader(const Entry& e, bool central)
	{
		Put32(central ? 0x02014b50 : 0x04034b50);
		if (central)
		{
			Put16(10); // made by
		}
		Put16(10);         // version needed
		Put16(0x0800);     // UTF-8 names
		Put16(0);          // stored
		Put16(e.time);
		Put16(e.date);
		Put32(e.crc);
		Put32(e.packedSize);
		Put32(e.size);
		Put16((WORD) e.name.GetLength());
		Put16(0); // extra
		if (central)
		{
			Put16(0); // comment
			Put16(0); // disk
			Put16(0); // internal attributes
			Put32(0); // external attributes
			Put32(e.offset);
		}
		m_file.Write((LPCSTR) e.name, e.name.GetLength());
	}
};

//-------------------------------------------------------------------------

BEGIN_MESSAGE_MAP(CDlgBugReport, CDialog)
END_MESSAGE_MAP()

CDlgBugReport::CDlgBugReport(CConCadMultiDoc* pDesign, bool afterCrash, CWnd* pParent) :
	CDialog(IDD_BUG_REPORT, pParent)
{
	m_pDesign = pDesign;
	m_afterCrash = afterCrash;
	m_includeLogs = TRUE;
	m_includeCrash = TRUE;
	m_includeDesign = pDesign != NULL;
}

CString CDlgBugReport::Address()
{
	return AfxGetApp()->GetProfileString(_T("BugReport"), _T("Address"), DefaultAddress);
}

void CDlgBugReport::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_BUG_DESCRIPTION, m_description);
	DDX_Check(pDX, IDC_BUG_LOGS, m_includeLogs);
	DDX_Check(pDX, IDC_BUG_CRASH, m_includeCrash);
	DDX_Check(pDX, IDC_BUG_DESIGN, m_includeDesign);
}

BOOL CDlgBugReport::OnInitDialog()
{
	m_crashDump = CLog::GetRecentCrashDump(7);
	m_includeCrash = !m_crashDump.IsEmpty();

	CDialog::OnInitDialog();

	if (m_afterCrash)
	{
		SetDlgItemText(IDC_BUG_INTRO, _T("ConCAD did not close normally the last time. Please describe what you were doing, so the problem can be found and fixed."));
	}
	if (m_crashDump.IsEmpty())
	{
		GetDlgItem(IDC_BUG_CRASH)->EnableWindow(FALSE);
		SetDlgItemText(IDC_BUG_CRASH, _T("Include crash information (none in the last 7 days)"));
	}
	if (m_pDesign == NULL)
	{
		GetDlgItem(IDC_BUG_DESIGN)->EnableWindow(FALSE);
		SetDlgItemText(IDC_BUG_DESIGN, _T("Include the open design (no design is open)"));
	}
	else
	{
		CString title = m_pDesign->GetTitle();
		SetDlgItemText(IDC_BUG_DESIGN, _T("Include the open design: ") + title);
	}
	SetDlgItemText(IDC_BUG_ADDRESS, _T("The report is sent to ") + Address() + _T(". Your mail program opens with the email ready to send."));
	GetDlgItem(IDC_BUG_DESCRIPTION)->SetFocus();
	return FALSE;
}

void CDlgBugReport::OnOK()
{
	UpdateData(TRUE);
	CString text = m_description;
	text.Trim();
	if (text.IsEmpty())
	{
		AfxMessageBox(_T("Please describe what happened."), MB_ICONINFORMATION);
		GetDlgItem(IDC_BUG_DESCRIPTION)->SetFocus();
		return;
	}
	CDialog::OnOK();
}

bool CDlgBugReport::BuildReport(CString& zipPath)
{
	SYSTEMTIME t;
	::GetLocalTime(&t);
	CString stamp;
	stamp.Format(_T("%04d%02d%02d-%02d%02d%02d"), t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
	zipPath = CLog::GetLogDir() + _T("BugReport-") + stamp + _T(".zip");

	CLog::Write(_T("Bug report: %s"), (LPCTSTR) zipPath);
	CLog::WriteStartupInfo(); // the current state, for the report's log

	CZipWriter zip;
	if (CLog::GetLogDir().IsEmpty() || !zip.Open(zipPath))
	{
		return false;
	}

	// The description and the basics
	TCHAR user[256] = { 0 };
	DWORD userLength = 256;
	::GetUserName(user, &userLength);
	TCHAR computer[256] = { 0 };
	DWORD computerLength = 256;
	::GetComputerName(computer, &computerLength);
	CString report;
	report.Format(_T("ConCAD bug report\r\n\r\nVersion: ConCAD %s (%s), build %hs\r\nDate: %04d-%02d-%02d %02d:%02d\r\nUser: %s on %s\r\nDesign: %s\r\n\r\nWhat happened:\r\n%s\r\n"),
		(LPCTSTR) CConCadApp::GetVersion(), (LPCTSTR) CConCadApp::GetReleaseType(), BUILD_UUID,
		t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, user, computer,
		m_pDesign != NULL ? (LPCTSTR) m_pDesign->GetPathName() : _T("(none)"), (LPCTSTR) m_description);
	CStringA reportUtf8(CW2A(report, CP_UTF8));
	zip.AddData(_T("report.txt"), (const BYTE*) (LPCSTR) reportUtf8, reportUtf8.GetLength());

	if (m_includeLogs)
	{
		CStringArray logs;
		CLog::GetRecentLogs(3, logs);
		for (int i = 0; i < logs.GetSize(); i++)
		{
			zip.AddFile(_T("logs\\") + CString(::PathFindFileName(logs[i])), logs[i]);
		}
	}
	if (m_includeCrash && !m_crashDump.IsEmpty())
	{
		zip.AddFile(CString(::PathFindFileName(m_crashDump)), m_crashDump);
	}
	if (m_includeDesign && m_pDesign != NULL)
	{
		// As it is now, unsaved changes included
		TCHAR temp[MAX_PATH] = { 0 };
		::GetTempPath(MAX_PATH, temp);
		const CString copy = CString(temp) + _T("ConCAD-bugreport-design.con");
		if (m_pDesign->SaveCopyTo(copy))
		{
			CString name = m_pDesign->GetTitle();
			name.Remove(_T('*'));
			name.Trim();
			if (name.Right(4).CompareNoCase(_T(".con")) != 0)
			{
				name += _T(".con");
			}
			zip.AddFile(_T("design\\") + name, copy);
			::DeleteFile(copy);
		}
	}
	zip.Close();
	return true;
}

// An email with the zip attached in the user's mail program (Simple MAPI);
// MAPI_USER_ABORT if the user cancelled it, another error if there is no
// such mail program
static ULONG SendWithMapi(HWND hWnd, const CString& address, const CString& subject, const CString& body, const CString& zipPath)
{
	HMODULE hMapi = ::LoadLibrary(_T("MAPI32.DLL"));
	if (hMapi == NULL)
	{
		return MAPI_E_FAILURE;
	}
	ULONG result = MAPI_E_FAILURE;
	const CString to = _T("SMTP:") + address;
	const CString fileName = ::PathFindFileName(zipPath);

	typedef ULONG (FAR PASCAL *SendMailWFn)(LHANDLE, ULONG_PTR, lpMapiMessageW, FLAGS, ULONG);
	SendMailWFn pSendW = (SendMailWFn) ::GetProcAddress(hMapi, "MAPISendMailW");
	if (pSendW != NULL)
	{
		MapiRecipDescW recipient = { 0 };
		recipient.ulRecipClass = MAPI_TO;
		recipient.lpszName = (PWSTR) (LPCTSTR) address;
		recipient.lpszAddress = (PWSTR) (LPCTSTR) to;
		MapiFileDescW file = { 0 };
		file.nPosition = (ULONG) -1;
		file.lpszPathName = (PWSTR) (LPCTSTR) zipPath;
		file.lpszFileName = (PWSTR) (LPCTSTR) fileName;
		MapiMessageW message = { 0 };
		message.lpszSubject = (PWSTR) (LPCTSTR) subject;
		message.lpszNoteText = (PWSTR) (LPCTSTR) body;
		message.nRecipCount = 1;
		message.lpRecips = &recipient;
		message.nFileCount = 1;
		message.lpFiles = &file;
		result = pSendW(0, (ULONG_PTR) hWnd, &message, MAPI_DIALOG | MAPI_LOGON_UI, 0);
	}
	else
	{
		typedef ULONG (FAR PASCAL *SendMailFn)(LHANDLE, ULONG_PTR, lpMapiMessage, FLAGS, ULONG);
		SendMailFn pSend = (SendMailFn) ::GetProcAddress(hMapi, "MAPISendMail");
		if (pSend != NULL)
		{
			CStringA aAddress(address), aTo(to), aPath(zipPath), aName(fileName), aSubject(subject), aBody(body);
			MapiRecipDesc recipient = { 0 };
			recipient.ulRecipClass = MAPI_TO;
			recipient.lpszName = aAddress.GetBuffer();
			recipient.lpszAddress = aTo.GetBuffer();
			MapiFileDesc file = { 0 };
			file.nPosition = (ULONG) -1;
			file.lpszPathName = aPath.GetBuffer();
			file.lpszFileName = aName.GetBuffer();
			MapiMessage message = { 0 };
			message.lpszSubject = aSubject.GetBuffer();
			message.lpszNoteText = aBody.GetBuffer();
			message.nRecipCount = 1;
			message.lpRecips = &recipient;
			message.nFileCount = 1;
			message.lpFiles = &file;
			result = pSend(0, (ULONG_PTR) hWnd, &message, MAPI_DIALOG | MAPI_LOGON_UI, 0);
		}
	}
	::FreeLibrary(hMapi);
	return result;
}

static CString UrlEncode(const CString& s)
{
	CStringA utf8(CW2A(s, CP_UTF8));
	CString out;
	for (int i = 0; i < utf8.GetLength(); i++)
	{
		const unsigned char c = (unsigned char) utf8[i];
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
		{
			out += (TCHAR) c;
		}
		else
		{
			CString hex;
			hex.Format(_T("%%%02X"), c);
			out += hex;
		}
	}
	return out;
}

void CDlgBugReport::Run(bool afterCrash)
{
	// The design in the active window, if any
	CConCadMultiDoc* pDesign = NULL;
	CMDIFrameWnd* pFrame = DYNAMIC_DOWNCAST(CMDIFrameWnd, AfxGetMainWnd());
	CMDIChildWnd* pChild = pFrame != NULL ? pFrame->MDIGetActive() : NULL;
	if (pChild != NULL)
	{
		pDesign = DYNAMIC_DOWNCAST(CConCadMultiDoc, pChild->GetActiveDocument());
	}

	CDlgBugReport dlg(pDesign, afterCrash, AfxGetMainWnd());
	if (dlg.DoModal() != IDOK)
	{
		return;
	}

	CWaitCursor wait;
	CString zipPath;
	if (!dlg.BuildReport(zipPath))
	{
		AfxMessageBox(_T("The bug report could not be created."), MB_ICONEXCLAMATION);
		return;
	}

	const CString address = Address();
	CString subject;
	subject.Format(_T("ConCAD %s bug report"), (LPCTSTR) CConCadApp::GetVersion());
	CString summary = dlg.m_description;
	if (summary.GetLength() > 1500)
	{
		summary = summary.Left(1500) + _T(" ...");
	}
	const CString body = summary + _T("\r\n\r\n(The details are in the attached zip file.)\r\n");

	// The compose window stays open until the user sends or closes it, and
	// Outlook runs that as a COM call into outlook.exe: without this, MFC's
	// OLE message filter shows "Server Busy" after a few seconds
	COleMessageFilter* pFilter = AfxOleGetMessageFilter();
	if (pFilter != NULL)
	{
		pFilter->EnableBusyDialog(FALSE);
		pFilter->EnableNotRespondingDialog(FALSE);
	}
	const ULONG result = SendWithMapi(AfxGetMainWnd()->GetSafeHwnd(), address, subject, body, zipPath);
	if (pFilter != NULL)
	{
		pFilter->EnableBusyDialog(TRUE);
		pFilter->EnableNotRespondingDialog(TRUE);
	}
	CLog::Write(_T("Bug report: MAPI result %lu"), result);
	if (result == SUCCESS_SUCCESS)
	{
		return;
	}
	if (result == MAPI_USER_ABORT)
	{
		AfxMessageBox(_T("The email was not sent. The bug report is saved as:\n\n") + zipPath, MB_ICONINFORMATION);
		return;
	}

	// No mail program that takes attachments: the zip on the Desktop and a
	// plain email
	TCHAR desktop[MAX_PATH] = { 0 };
	CString target = zipPath;
	if (SUCCEEDED(::SHGetFolderPath(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktop)))
	{
		const CString onDesktop = CString(desktop) + _T("\\") + ::PathFindFileName(zipPath);
		if (::CopyFile(zipPath, onDesktop, FALSE))
		{
			target = onDesktop;
		}
	}
	const CString mailto = _T("mailto:") + address + _T("?subject=") + UrlEncode(subject)
		+ _T("&body=") + UrlEncode(summary.Left(500) + _T("\r\n\r\nPlease attach the file ") + CString(::PathFindFileName(target)) + _T(" from your Desktop."));
	::ShellExecute(NULL, _T("open"), mailto, NULL, NULL, SW_SHOWNORMAL);
	AfxMessageBox(_T("Your mail program could not attach the bug report by itself.\n\nThe report is saved as:\n") + target
		+ _T("\n\nPlease attach this file to the email that opens (or send it to ") + address + _T(")."), MB_ICONINFORMATION);
}
