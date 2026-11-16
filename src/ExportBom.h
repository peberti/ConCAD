/*
 * ConCAD: File -> Export BoM to Excel
 * License:		Lesser GNU Public License 2.1 (LGPL)
 *
 * Lists the Partnumber of every placed module (all sheets) with its count
 * and fills an Excel template with it (tools/BomExport, CHANGES.md §33).
 */

#pragma once

#include <vector>
#include <utility>

class CConCadMultiDoc;

class CDlgExportBom: public CDialog
{
public:
	// The whole export: collect, ask, write
	static void Export(CConCadMultiDoc* pDesign);

	struct Item
	{
		CString partnumber;
		CString type;
		CString name;
		int qty;
	};
	typedef std::vector<std::pair<CString, CString> > TokenList;

	// The design variables (Design Details, user variables, {Today},
	// {FileName}) with their values
	static void GetDesignTokens(CConCadMultiDoc* pDesign, TokenList& tokens);

	// The pattern with the variables filled in (unknown / empty ones give
	// nothing) and characters Windows does not allow in a file name
	// replaced; empty when nothing is left
	static CString ResolvePattern(const CString& pattern, const TokenList& tokens);

	// Default file names, Options > Settings > File Names
	static const TCHAR* const DefaultBomFileName;
	static const TCHAR* const DefaultDrawingFileName;
	static CString GetBomFileNamePattern();
	static CString GetDrawingFileNamePattern();
	static void SetFileNamePatterns(const CString& bom, const CString& drawing);

private:
	CDlgExportBom(CConCadMultiDoc* pDesign, const TokenList& tokens, const CString& summary);

	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnTemplateBrowse();
	afx_msg void OnFolderBrowse();
	afx_msg void OnFileNameChange();
	afx_msg void OnVariableDblClk(NMHDR* pNMHDR, LRESULT* pResult);
	void FillVariables();
	DECLARE_MESSAGE_MAP()

	struct Template
	{
		CString label;
		CString path;
	};
	void FindTemplates();
	void AddTemplates(const CString& folder, const CString& suffix);
	int AddTemplate(const CString& label, const CString& path);
	CString OutputName() const;

	CConCadMultiDoc* m_pDesign;
	const TokenList& m_tokens;
	CString m_summary;
	std::vector<Template> m_templates;

public:
	// Results of the dialog
	CString m_template;
	CString m_output;
	BOOL m_open;
};
