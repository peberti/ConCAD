/*
 * ConCAD: log file and crash handling
 * License:		Lesser GNU Public License 2.1 (LGPL)
 *
 * One log file per day in %LOCALAPPDATA%\ConCAD\Logs (kept 14 days):
 * start-up information, opened/saved files, the commands used, messages
 * shown and errors.  A crash writes the exception and a crash dump (.dmp)
 * there too; the next start offers to send a bug report.
 */

#pragma once

class CLog
{
public:
	// Start logging and install the crash handlers (first thing at start-up)
	static void Init();
	// Normal end of ConCAD
	static void Shutdown();

	// A line in the log, printf style
	static void Write(LPCTSTR format, ...);
	// A command from a menu, toolbar or shortcut
	static void Command(UINT id);
	// Version, Windows, screens, libraries
	static void WriteStartupInfo();

	static CString GetLogDir();
	// The newest crash dump written within 'days' days, or empty
	static CString GetRecentCrashDump(int days);
	// The log files of the last 'days' days, oldest first
	static void GetRecentLogs(int days, CStringArray& files);
	// Did the previous ConCAD end without closing normally (crash, killed)?
	static bool PreviousSessionEndedUnexpectedly()
	{
		return s_unexpectedEnd;
	}

private:
	static void CleanUp();
	static void CheckPreviousSessions();
	static CString CommandName(UINT id);

	static FILE* s_file;
	static CString s_dir;
	static CString s_sessionFlag;
	static bool s_unexpectedEnd;
};
