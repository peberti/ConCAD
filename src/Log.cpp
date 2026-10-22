/*
 * ConCAD: log file and crash handling
 * License:		Lesser GNU Public License 2.1 (LGPL)
 */

#include "stdafx.h"
#include <shlobj.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#include <share.h>
#include <signal.h>
#include <map>
#include <dbghelp.h>
#include "Log.h"
#include "ConCad.h"
#include "ConCadRegistry.h"
#include "BuildId.h"

FILE* CLog::s_file = NULL;
CString CLog::s_dir;
CString CLog::s_sessionFlag;
bool CLog::s_unexpectedEnd = false;

static CRITICAL_SECTION s_lock;
static bool s_lockReady = false;
static const int KeepDays = 14;

//-------------------------------------------------------------------------
// Crash handling

// Write a crash dump next to the logs; the file name, or empty
static CString WriteMiniDump(EXCEPTION_POINTERS* ep)
{
	typedef BOOL (WINAPI *MiniDumpWriteDumpFn)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
		PMINIDUMP_EXCEPTION_INFORMATION, PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
	HMODULE hDbgHelp = ::LoadLibrary(_T("dbghelp.dll"));
	MiniDumpWriteDumpFn pWrite = hDbgHelp != NULL ? (MiniDumpWriteDumpFn) ::GetProcAddress(hDbgHelp, "MiniDumpWriteDump") : NULL;
	if (pWrite == NULL || CLog::GetLogDir().IsEmpty())
	{
		return CString();
	}

	SYSTEMTIME t;
	::GetLocalTime(&t);
	CString path;
	path.Format(_T("%scrash-%04d%02d%02d-%02d%02d%02d.dmp"), (LPCTSTR) CLog::GetLogDir(),
		t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
	HANDLE hFile = ::CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return CString();
	}
	MINIDUMP_EXCEPTION_INFORMATION info;
	info.ThreadId = ::GetCurrentThreadId();
	info.ExceptionPointers = ep;
	info.ClientPointers = FALSE;
	const MINIDUMP_TYPE type = (MINIDUMP_TYPE) (MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo);
	const BOOL ok = pWrite(::GetCurrentProcess(), ::GetCurrentProcessId(), hFile, type, ep != NULL ? &info : NULL, NULL, NULL);
	::CloseHandle(hFile);
	return ok ? path : CString();
}

static LONG WINAPI CrashFilter(EXCEPTION_POINTERS* ep)
{
	static volatile LONG s_handled = 0;
	if (::InterlockedExchange(&s_handled, 1) != 0)
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}

	// Where: the module and the offset in it
	DWORD code = 0;
	void* address = NULL;
	if (ep != NULL && ep->ExceptionRecord != NULL)
	{
		code = ep->ExceptionRecord->ExceptionCode;
		address = ep->ExceptionRecord->ExceptionAddress;
	}
	TCHAR module[MAX_PATH] = _T("?");
	HMODULE hModule = NULL;
	if (::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCTSTR) address, &hModule))
	{
		::GetModuleFileName(hModule, module, MAX_PATH);
	}
	CLog::Write(_T("CRASH: exception 0x%08lX at %p (%s + 0x%IX)"), code, address, ::PathFindFileName(module),
		(size_t) ((BYTE*) address - (BYTE*) hModule));

	const CString dump = WriteMiniDump(ep);
	CLog::Write(dump.IsEmpty() ? _T("CRASH: no crash dump could be written") : _T("CRASH: dump %s"), (LPCTSTR) dump);
	return EXCEPTION_EXECUTE_HANDLER; // end the process
}

// Errors that do not raise an exception by themselves: raise one, so the
// crash filter writes the log line and the dump (no C++ objects here,
// because of __try)
static void RaiseCrash(DWORD code)
{
	__try
	{
		::RaiseException(code, EXCEPTION_NONCONTINUABLE, 0, NULL);
	}
	__except (CrashFilter(GetExceptionInformation()))
	{
	}
	::TerminateProcess(::GetCurrentProcess(), code);
}

static void InvalidParameterHandler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t)
{
	RaiseCrash(0xE0000001);
}

static void PureCallHandler()
{
	RaiseCrash(0xE0000002);
}

static void AbortHandler(int)
{
	RaiseCrash(0xE0000003);
}

static void TerminateHandler()
{
	RaiseCrash(0xE0000004);
}

static void InstallCrashHandlers()
{
	::SetUnhandledExceptionFilter(CrashFilter);
	_set_invalid_parameter_handler(InvalidParameterHandler);
	_set_purecall_handler(PureCallHandler);
	signal(SIGABRT, AbortHandler);
	set_terminate(TerminateHandler);

	// A 32-bit program on 64-bit Windows: do not let Windows swallow
	// exceptions in window procedures silently (they must reach the filter)
	typedef BOOL (WINAPI *GetPolicyFn)(LPDWORD);
	typedef BOOL (WINAPI *SetPolicyFn)(DWORD);
	HMODULE hKernel = ::GetModuleHandle(_T("kernel32.dll"));
	GetPolicyFn pGet = (GetPolicyFn) ::GetProcAddress(hKernel, "GetProcessUserModeExceptionPolicy");
	SetPolicyFn pSet = (SetPolicyFn) ::GetProcAddress(hKernel, "SetProcessUserModeExceptionPolicy");
	DWORD flags = 0;
	if (pGet != NULL && pSet != NULL && pGet(&flags))
	{
		pSet(flags & ~0x1); // PROCESS_CALLBACK_FILTER_ENABLED
	}
}

//-------------------------------------------------------------------------
// The log file

CString CLog::GetLogDir()
{
	return s_dir;
}

void CLog::Init()
{
	::InitializeCriticalSection(&s_lock);
	s_lockReady = true;

	TCHAR local[MAX_PATH] = { 0 };
	if (SUCCEEDED(::SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, local)))
	{
		s_dir = CString(local) + _T("\\ConCAD\\Logs\\");
		::SHCreateDirectoryEx(NULL, s_dir, NULL);
	}
	InstallCrashHandlers();
	if (s_dir.IsEmpty())
	{
		return;
	}

	CheckPreviousSessions();
	CleanUp();

	SYSTEMTIME t;
	::GetLocalTime(&t);
	CString path;
	path.Format(_T("%sConCAD-%04d-%02d-%02d.log"), (LPCTSTR) s_dir, t.wYear, t.wMonth, t.wDay);
	s_file = _tfsopen(path, _T("a, ccs=UTF-8"), _SH_DENYNO);

	// Present while this ConCAD runs; left behind if it does not end normally
	s_sessionFlag.Format(_T("%ssession-%lu.running"), (LPCTSTR) s_dir, ::GetCurrentProcessId());
	CFile flag;
	if (flag.Open(s_sessionFlag, CFile::modeCreate | CFile::modeWrite))
	{
		flag.Close();
	}

	Write(_T("==== ConCAD %s (%s) started, process %lu"), (LPCTSTR) CConCadApp::GetVersion(),
		(LPCTSTR) CConCadApp::GetReleaseType(), ::GetCurrentProcessId());
	if (s_unexpectedEnd)
	{
		Write(_T("The previous ConCAD session did not end normally"));
	}
}

void CLog::Shutdown()
{
	Write(_T("==== ConCAD closed normally"));
	if (!s_sessionFlag.IsEmpty())
	{
		::DeleteFile(s_sessionFlag);
	}
	if (s_file != NULL)
	{
		fclose(s_file);
		s_file = NULL;
	}
}

void CLog::Write(LPCTSTR format, ...)
{
	if (s_file == NULL)
	{
		return;
	}
	CString text;
	va_list args;
	va_start(args, format);
	text.FormatV(format, args);
	va_end(args);
	text.Replace(_T("\r\n"), _T(" | "));
	text.Replace(_T("\n"), _T(" | "));

	SYSTEMTIME t;
	::GetLocalTime(&t);
	CString line;
	line.Format(_T("%02d:%02d:%02d.%03d  %s\n"), t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, (LPCTSTR) text);

	if (s_lockReady)
	{
		::EnterCriticalSection(&s_lock);
	}
	_fputts(line, s_file);
	fflush(s_file); // a crash must not lose the last lines
	if (s_lockReady)
	{
		::LeaveCriticalSection(&s_lock);
	}
}

// Sessions whose flag file is still there and whose process has gone did
// not end normally
void CLog::CheckPreviousSessions()
{
	WIN32_FIND_DATA fd;
	HANDLE hFind = ::FindFirstFile(s_dir + _T("session-*.running"), &fd);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		return;
	}
	do
	{
		const DWORD pid = (DWORD) _tcstoul(fd.cFileName + 8, NULL, 10);
		HANDLE hProcess = ::OpenProcess(SYNCHRONIZE, FALSE, pid);
		const bool running = hProcess != NULL && ::WaitForSingleObject(hProcess, 0) == WAIT_TIMEOUT;
		if (hProcess != NULL)
		{
			::CloseHandle(hProcess);
		}
		if (!running)
		{
			s_unexpectedEnd = true;
			::DeleteFile(s_dir + fd.cFileName);
		}
	} while (::FindNextFile(hFind, &fd));
	::FindClose(hFind);
}

// Logs, crash dumps and bug reports older than KeepDays days
void CLog::CleanUp()
{
	FILETIME now;
	::GetSystemTimeAsFileTime(&now);
	const ULONGLONG limit = ((ULONGLONG) now.dwHighDateTime << 32 | now.dwLowDateTime) - (ULONGLONG) KeepDays * 24 * 3600 * 10000000;
	const TCHAR* const patterns[] = { _T("*.log"), _T("*.dmp"), _T("BugReport-*.zip") };
	for (int p = 0; p < 3; p++)
	{
		WIN32_FIND_DATA fd;
		HANDLE hFind = ::FindFirstFile(s_dir + patterns[p], &fd);
		if (hFind == INVALID_HANDLE_VALUE)
		{
			continue;
		}
		do
		{
			const ULONGLONG written = (ULONGLONG) fd.ftLastWriteTime.dwHighDateTime << 32 | fd.ftLastWriteTime.dwLowDateTime;
			if (written < limit)
			{
				::DeleteFile(s_dir + fd.cFileName);
			}
		} while (::FindNextFile(hFind, &fd));
		::FindClose(hFind);
	}
}

static ULONGLONG AgeLimit(int days)
{
	FILETIME now;
	::GetSystemTimeAsFileTime(&now);
	return ((ULONGLONG) now.dwHighDateTime << 32 | now.dwLowDateTime) - (ULONGLONG) days * 24 * 3600 * 10000000;
}

CString CLog::GetRecentCrashDump(int days)
{
	CString newest;
	ULONGLONG newestTime = AgeLimit(days);
	WIN32_FIND_DATA fd;
	HANDLE hFind = ::FindFirstFile(s_dir + _T("crash-*.dmp"), &fd);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		return newest;
	}
	do
	{
		const ULONGLONG written = (ULONGLONG) fd.ftLastWriteTime.dwHighDateTime << 32 | fd.ftLastWriteTime.dwLowDateTime;
		if (written >= newestTime)
		{
			newestTime = written;
			newest = s_dir + fd.cFileName;
		}
	} while (::FindNextFile(hFind, &fd));
	::FindClose(hFind);
	return newest;
}

void CLog::GetRecentLogs(int days, CStringArray& files)
{
	files.RemoveAll();
	const ULONGLONG limit = AgeLimit(days);
	WIN32_FIND_DATA fd;
	HANDLE hFind = ::FindFirstFile(s_dir + _T("ConCAD-*.log"), &fd);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		return;
	}
	do
	{
		const ULONGLONG written = (ULONGLONG) fd.ftLastWriteTime.dwHighDateTime << 32 | fd.ftLastWriteTime.dwLowDateTime;
		if (written >= limit)
		{
			files.Add(s_dir + fd.cFileName); // names sort by date
		}
	} while (::FindNextFile(hFind, &fd));
	::FindClose(hFind);
}

//-------------------------------------------------------------------------
// Commands, by their menu name ("Object > Create Module")

static void AddMenuNames(CMenu* pMenu, const CString& prefix, std::map<UINT, CString>& names)
{
	for (int i = 0; i < (int) pMenu->GetMenuItemCount(); i++)
	{
		CString text;
		pMenu->GetMenuString(i, text, MF_BYPOSITION);
		const int tab = text.Find(_T('\t'));
		if (tab >= 0)
		{
			text = text.Left(tab);
		}
		text.Remove(_T('&'));
		text.TrimRight(_T(". "));
		const CString path = prefix.IsEmpty() ? text : prefix + _T(" > ") + text;
		CMenu* pSub = pMenu->GetSubMenu(i);
		if (pSub != NULL)
		{
			AddMenuNames(pSub, path, names);
		}
		else
		{
			const UINT id = pMenu->GetMenuItemID(i);
			if (id != 0 && id != (UINT) -1 && names.find(id) == names.end())
			{
				names[id] = path;
			}
		}
	}
}

CString CLog::CommandName(UINT id)
{
	static std::map<UINT, CString> names;
	static bool loaded = false;
	if (!loaded)
	{
		loaded = true;
		const UINT menus[] = { IDR_TCADTYPE, IDR_LIBTYPE, IDR_MAINFRAME };
		for (int m = 0; m < 3; m++)
		{
			CMenu menu;
			if (menu.LoadMenu(menus[m]))
			{
				AddMenuNames(&menu, CString(), names);
			}
		}
	}
	std::map<UINT, CString>::const_iterator it = names.find(id);
	if (it != names.end())
	{
		return it->second;
	}

	// Toolbar tools etc.: "Description\nTooltip" from the string table
	CString s;
	if (s.LoadString(id))
	{
		const int n = s.Find(_T('\n'));
		if (n >= 0 && n + 1 < s.GetLength())
		{
			return s.Mid(n + 1);
		}
		if (n >= 0)
		{
			s = s.Left(n);
		}
		if (!s.IsEmpty())
		{
			return s;
		}
	}
	s.Format(_T("command %u"), id);
	return s;
}

void CLog::Command(UINT id)
{
	if (s_file != NULL && id != 0)
	{
		Write(_T("Command: %s"), (LPCTSTR) CommandName(id));
	}
}

//-------------------------------------------------------------------------
// Start-up information

static BOOL CALLBACK LogMonitor(HMONITOR hMonitor, HDC, LPRECT, LPARAM)
{
	MONITORINFOEX mi;
	mi.cbSize = sizeof(mi);
	if (::GetMonitorInfo(hMonitor, &mi))
	{
		CLog::Write(_T("Screen %s: %ld,%ld %ldx%ld%s"), mi.szDevice, mi.rcMonitor.left, mi.rcMonitor.top,
			mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
			(mi.dwFlags & MONITORINFOF_PRIMARY) ? _T(" (primary)") : _T(""));
	}
	return TRUE;
}

void CLog::WriteStartupInfo()
{
	TCHAR exe[MAX_PATH] = { 0 };
	::GetModuleFileName(NULL, exe, MAX_PATH);
	Write(_T("Program: %s, build %hs, branch %hs"), exe, BUILD_UUID, GIT_BRANCH);
	Write(_T("Command line: %s"), ::GetCommandLine());

	// The real Windows version (GetVersionEx is capped by the manifest)
	typedef LONG (WINAPI *RtlGetVersionFn)(OSVERSIONINFOEXW*);
	RtlGetVersionFn pGetVersion = (RtlGetVersionFn) ::GetProcAddress(::GetModuleHandle(_T("ntdll.dll")), "RtlGetVersion");
	OSVERSIONINFOEXW os = { sizeof(os) };
	if (pGetVersion != NULL && pGetVersion(&os) == 0)
	{
		Write(_T("Windows %lu.%lu build %lu"), os.dwMajorVersion, os.dwMinorVersion, os.dwBuildNumber);
	}
	HDC hdc = ::GetDC(NULL);
	Write(_T("Screen DPI: %d"), ::GetDeviceCaps(hdc, LOGPIXELSX));
	::ReleaseDC(NULL, hdc);
	::EnumDisplayMonitors(NULL, NULL, LogMonitor, 0);

	Write(_T("Libraries: %s"), (LPCTSTR) CRegistry::GetString(_T("Libraries"), _T("")));
	Write(_T("Module library: %s"), (LPCTSTR) CConCadRegistry::GetModuleLibrary());
}
