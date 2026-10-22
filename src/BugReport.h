/*
 * ConCAD: Help -> Report a Bug
 * License:		Lesser GNU Public License 2.1 (LGPL)
 *
 * Asks what happened, packs the description, system information, the
 * recent logs, the latest crash dump and a copy of the open design into a
 * zip in the log folder, and opens an email to the developer with the zip
 * attached (Simple MAPI: classic Outlook etc.).  Without such a mail
 * program the zip is copied to the Desktop and a plain email is opened.
 */

#pragma once

class CConCadMultiDoc;

class CDlgBugReport: public CDialog
{
public:
	CDlgBugReport(CConCadMultiDoc* pDesign, bool afterCrash, CWnd* pParent = NULL);

	// Show the dialog and send the report; afterCrash pre-selects the
	// crash information
	static void Run(bool afterCrash);

	CString m_description;
	BOOL m_includeLogs;
	BOOL m_includeCrash;
	BOOL m_includeDesign;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	DECLARE_MESSAGE_MAP()

private:
	CConCadMultiDoc* m_pDesign;
	bool m_afterCrash;
	CString m_crashDump;

	bool BuildReport(CString& zipPath);
	static CString Address();
};
