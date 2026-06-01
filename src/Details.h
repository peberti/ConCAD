/*
 * Project:		TinyCAD program for schematic capture
 *				https://www.tinycad.net
 * Copyright:	� 1994-2019 Matt Pyne
 * License:		Lesser GNU Public License 2.1 (LGPL)
 *				http://www.opensource.org/licenses/lgpl-license.html
 */

#pragma once

#include "context.h"
#include "stream.h"
#include "xmlwriter.h"
#include <map>

//=========================================================================
// Case-insensitive comparator for CString keys in the user-token map.
struct CDetailsTokenKeyLess
{
	bool operator()(const CString& a, const CString& b) const
	{
		return a.CompareNoCase(b) < 0;
	}
};

typedef std::map<CString, CString, CDetailsTokenKeyLess> CDetailsTokenMap;

class CDetails
{
private:
	static const int M_NBOXWIDTH;
	static const int M_NLINEHEIGHT;
	static const int M_NRULERHEIGHT;
	//-- How many pixels in each milimetre
	static const int M_NPIXELSPERMM;
	//-- The default size of the page
public:
	static const CSize M_SZMAX;
	// The size of the page
	CSize m_szPage;
	//-- Do we display the details box?
	bool m_bIsVisible;
	//-- Do we show the rulers at the side of the page?
	bool m_bHasRulers;
	//-- How many divisions in the horizontal ruler?
	int m_iHorizRulerSize;
	//-- How many divisions in the vertical ruler?
	int m_iVertRulerSize;
	//-- The date when this design was last edited/saved
	CString m_szLastChange;
	//-- The title of this design
	CString m_sTitle;
	//-- The author of this design
	CString m_sAuthor;
	//-- The revision code of this design
	CString m_sRevision;
	//-- The document number of this design
	CString m_sDocNo;
	//-- The organisation which designed this design
	CString m_sOrg;
	//-- The number of sheets in this design
	CString m_sSheets;
	//-- User-defined name/value tokens, referenced as {name} in any field
	CDetailsTokenMap m_oUserTokens;
	//-- Custom title-block as an SVG XML string.  When non-empty, the SVG
	//-- replaces the built-in DisplayBox.  Shared across all sheets.
	CString m_sTitleBlockSvg;
	//-- Transient sheet context (this sheet's 1-based number / total sheets)
	//-- set by the owning CTinyCadDoc just before rendering or saving. Not serialized.
	int m_iSheetNum;
	int m_iSheetTotal;
	CDetails();
	~CDetails();

private:
	void Init();

public:
	void Reset();
	void Read(CStream& oArchive);
	void ReadEx(CStream& oArchive);
	void ReadXML(CXMLReader& xml, TransformSnap& oSnap);
	void WriteXML(CXMLWriter& xml) const;

	//-- Draw the details box
private:
	void DisplayBox(CContext & dc, COption& oOption, CString sPathName) const;
	void DisplayRulers(CContext & dc, COption& oOption) const;

public:
	void Display(CContext & dc, COption& oOption, CString sPathName) const;
	bool IsVisible() const;
	bool HasRulers() const;
	bool IsPortrait() const;
	CString GetLastChange() const;
	CString GetTitle() const;
	CString GetAuthor() const;
	CString GetRevision() const;
	CString GetDocumentNumber() const;
	CString GetOrganisation() const;
	CString GetSheets() const;
	CPoint GetPageBoundsAsPoint() const;
	CDPoint GetOverlap() const;
	CRect GetPageBoundsAsRect() const;
	void SetVisible(bool bIsVisible);
	void SetRulers(bool bHasRulers, int v, int h);
	void SetLastChange(const TCHAR* szLastChange);
	void SetTitle(CString sTitle);
	void SetAuthor(CString sAuthor);
	void SetRevision(CString sRevision);
	void SetDocumentNumber(CString sDocNo);
	void SetOrganisation(CString sOrganisation);
	void SetSheets(CString sSheets);
	//-- User token accessors
	const CDetailsTokenMap& GetUserTokens() const;
	void SetUserTokens(const CDetailsTokenMap& tokens);
	//-- Resolve {token} references inside a string.  Built-in tokens map to the
	//-- fixed title-block fields; user-defined tokens override built-ins.
	CString Resolve(const CString& sInput) const;
	//-- Set the transient sheet context.  When num+total are both >0, m_sSheets
	//-- is rewritten as "<num> of <total>" so subsequent rendering and saving
	//-- pick up the auto-computed value.
	void SetSheetContext(int num, int total);
	//-- The Sheets field as it should be displayed (auto "N of M" when context
	//-- is set; otherwise the stored m_sSheets).
	CString GetSheetsDisplay() const;
	//-- Copy the fields that are considered design-wide (Title/Author/Revision/
	//-- DocNo/Organisation/Date and the user tokens) from another CDetails.
	//-- Page size, rulers and visibility are NOT copied.
	void CopyDesignFields(const CDetails& src);
	//-- Set the page boundries from a CPoint
	void SetPageBounds(CPoint ptBounds);
	//-- Update the page boundries etc, using a printer device context
	void SetPageBounds(PRINTDLG& pd);
};
//=========================================================================
