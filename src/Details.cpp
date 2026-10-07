/*
 * Project:		TinyCAD program for schematic capture
 *				https://www.tinycad.net
 * Copyright:	� 1994-2019 Matt Pyne
 * License:		Lesser GNU Public License 2.1 (LGPL)
 *				http://www.opensource.org/licenses/lgpl-license.html
 */

#include "stdafx.h"
#include "details.h"
#include "concadregistry.h"
#include "colour.h"
#include "SvgTitleBlock.h"

const int CDetails::M_NBOXWIDTH = 400;
const int CDetails::M_NLINEHEIGHT = 18;
const int CDetails::M_NRULERHEIGHT = 15;
const int CDetails::M_NPIXELSPERMM = 5;
const CSize CDetails::M_SZMAX(297 * M_NPIXELSPERMM, 210 * M_NPIXELSPERMM);

//-------------------------------------------------------------------------
CDetails::CDetails()
{
	Init();
}
//-------------------------------------------------------------------------
CDetails::~CDetails()
{
}

//-------------------------------------------------------------------------
void CDetails::Init()
{
	m_bIsVisible = true;
	m_szLastChange = ""; // CTime::GetCurrentTime();
	m_bHasRulers = CConCadRegistry::GetBool("ShowDesignRulers", false);
	m_iHorizRulerSize = CConCadRegistry::GetInt("HorizRulerSize", 7);
	m_iVertRulerSize = CConCadRegistry::GetInt("VertRulerSize", 5);
	m_iSheetNum = 0;
	m_iSheetTotal = 0;

	Reset();
}
//-------------------------------------------------------------------------
void CDetails::Reset()
{
	m_szPage = M_SZMAX;
	m_sTitle = "";
	m_sAuthor = "";
	m_sRevision = "1.0";
	m_sDocNo = "";
	m_sOrg = "";
	m_sDescription = "";
	m_oRevisionHistory.clear();
	m_nHistoryRows = 0;
	m_sSheets = "1 of 1";
	m_oUserTokens.clear();
	m_sTitleBlockName = "";
	m_sTitleBlockSvg = "";
	m_sEffectiveSvg = "";
	m_szPage = CConCadRegistry::GetPageSize();
}
//-------------------------------------------------------------------------
bool CDetails::IsVisible() const
{
	return m_bIsVisible;
}
//-------------------------------------------------------------------------
bool CDetails::HasRulers() const
{
	return m_bHasRulers;
}
//-------------------------------------------------------------------------
bool CDetails::IsPortrait() const
{
	return m_szPage.cx < m_szPage.cy;
}
//-------------------------------------------------------------------------
CString CDetails::GetLastChange() const
{
	return m_szLastChange;
}
//-------------------------------------------------------------------------
CString CDetails::GetTitle() const
{
	return m_sTitle;
}
//-------------------------------------------------------------------------
CString CDetails::GetAuthor() const
{
	return m_sAuthor;
}
//-------------------------------------------------------------------------
CString CDetails::GetRevision() const
{
	return m_sRevision;
}
//-------------------------------------------------------------------------
CString CDetails::GetDocumentNumber() const
{
	return m_sDocNo;
}
//-------------------------------------------------------------------------
CString CDetails::GetOrganisation() const
{
	return m_sOrg;
}
//-------------------------------------------------------------------------
CString CDetails::GetDescription() const
{
	return m_sDescription;
}
//-------------------------------------------------------------------------
const CRevisionHistory& CDetails::GetRevisionHistory() const
{
	return m_oRevisionHistory;
}
//-------------------------------------------------------------------------
CString CDetails::GetSheets() const
{
	return m_sSheets;
}
//-------------------------------------------------------------------------
CString CDetails::GetTitleBlockSvg() const
{
	return m_sEffectiveSvg.IsEmpty() ? m_sTitleBlockSvg : m_sEffectiveSvg;
}
//-------------------------------------------------------------------------
// Return the page boundries in a CPoint
CPoint CDetails::GetPageBoundsAsPoint() const
{
	return CPoint(m_szPage);
}
//---------------------------------------------------------------------
CDPoint CDetails::GetOverlap() const
{
	return CDPoint(m_szPage.cx / 10.0, m_szPage.cy / 10.0);
}
//-------------------------------------------------------------------------
// Return the page boundries in a CRect
CRect CDetails::GetPageBoundsAsRect() const
{
	return CRect(CPoint(), m_szPage);
}
//-------------------------------------------------------------------------
void CDetails::SetVisible(bool bIsVisible)
{
	m_bIsVisible = bIsVisible;
}
//-------------------------------------------------------------------------
void CDetails::SetRulers(bool bHasRulers, int v, int h)
{
	m_bHasRulers = bHasRulers;

	if (h >= 1 && h <= 26)
	{
		m_iHorizRulerSize = h;
	}
	if (v >= 1 && v <= 26)
	{
		m_iVertRulerSize = v;
	}

	CConCadRegistry::Set("ShowDesignRulers", m_bHasRulers);
	CConCadRegistry::Set("HorizRulerSize", m_iHorizRulerSize);
	CConCadRegistry::Set("VertRulerSize", m_iVertRulerSize);
}
//-------------------------------------------------------------------------
void CDetails::SetLastChange(const TCHAR * szLastChange)
{
	m_szLastChange = szLastChange;
}
//-------------------------------------------------------------------------
void CDetails::SetTitle(CString sTitle)
{
	m_sTitle = sTitle;
}
//-------------------------------------------------------------------------
void CDetails::SetAuthor(CString sAuthor)
{
	m_sAuthor = sAuthor;
}
//-------------------------------------------------------------------------
void CDetails::SetRevision(CString sRevision)
{
	m_sRevision = sRevision;
}
//-------------------------------------------------------------------------
void CDetails::SetDocumentNumber(CString sDocNo)
{
	m_sDocNo = sDocNo;
}
//-------------------------------------------------------------------------
void CDetails::SetOrganisation(CString sOrg)
{
	m_sOrg = sOrg;
}
//-------------------------------------------------------------------------
void CDetails::SetDescription(CString sDescription)
{
	m_sDescription = sDescription;
}
//-------------------------------------------------------------------------
void CDetails::SetRevisionHistory(const CRevisionHistory& history)
{
	m_oRevisionHistory = history;
	ResolveTitleBlock();   // a growing revision table changes size
}
//-------------------------------------------------------------------------
void CDetails::SetSheets(CString sSheets)
{
	m_sSheets = sSheets;
}
//-------------------------------------------------------------------------
const CDetailsTokenMap& CDetails::GetUserTokens() const
{
	return m_oUserTokens;
}
//-------------------------------------------------------------------------
void CDetails::SetUserTokens(const CDetailsTokenMap& tokens)
{
	m_oUserTokens = tokens;
}
//-------------------------------------------------------------------------
void CDetails::SetSheetContext(int num, int total)
{
	m_iSheetNum = num;
	m_iSheetTotal = total;
	if (num > 0 && total > 0)
	{
		m_sSheets.Format(_T("%d of %d"), num, total);
	}
}
//-------------------------------------------------------------------------
CString CDetails::GetSheetsDisplay() const
{
	if (m_iSheetNum > 0 && m_iSheetTotal > 0)
	{
		CString s;
		s.Format(_T("%d of %d"), m_iSheetNum, m_iSheetTotal);
		return s;
	}
	return m_sSheets;
}
//-------------------------------------------------------------------------
void CDetails::CopyDesignFields(const CDetails& src)
{
	m_sTitle           = src.m_sTitle;
	m_sAuthor          = src.m_sAuthor;
	m_sRevision        = src.m_sRevision;
	m_sDocNo           = src.m_sDocNo;
	m_sOrg             = src.m_sOrg;
	m_sDescription     = src.m_sDescription;
	m_oRevisionHistory = src.m_oRevisionHistory;
	m_nHistoryRows     = src.m_nHistoryRows;
	m_szLastChange     = src.m_szLastChange;
	m_bIsVisible       = src.m_bIsVisible;
	m_oUserTokens      = src.m_oUserTokens;
	m_sTitleBlockName  = src.m_sTitleBlockName;
	m_sTitleBlockSvg   = src.m_sTitleBlockSvg;
	m_sEffectiveSvg    = src.m_sEffectiveSvg;
}
//-------------------------------------------------------------------------
// Split a revision-history token "Rev<N>[Date|Desc|By]" into its row number
// and field suffix.  Returns false for any other name.
static bool ParseHistoryToken(const CString& sName, int& row, CString& sField)
{
	if (sName.GetLength() < 4 || sName.Left(3).CompareNoCase(_T("Rev")) != 0)
	{
		return false;
	}
	int i = 3;
	while (i < sName.GetLength() && _istdigit(sName[i]))
	{
		++i;
	}
	if (i == 3)
	{
		return false;
	}
	row = _ttoi(sName.Mid(3, i - 3));
	sField = sName.Mid(i);
	return row > 0 && (sField.IsEmpty()
		|| sField.CompareNoCase(_T("Date")) == 0
		|| sField.CompareNoCase(_T("Desc")) == 0
		|| sField.CompareNoCase(_T("By")) == 0);
}
//-------------------------------------------------------------------------
// {Rev<N>}, {Rev<N>Date}, {Rev<N>Desc}, {Rev<N>By}: row N of the title
// block's revision table.  The table shows the newest m_nHistoryRows
// entries, oldest at row 1; rows without an entry resolve to "".
bool CDetails::ResolveHistoryToken(const CString& sName, CString& sValue) const
{
	int row = 0;
	CString sField;
	if (!ParseHistoryToken(sName, row, sField))
	{
		return false;
	}

	const int count = (int)m_oRevisionHistory.size();
	const int rows = m_nHistoryRows > 0 ? m_nHistoryRows : count;
	const int first = count > rows ? count - rows : 0;
	const int index = first + row - 1;

	sValue.Empty();
	if (index < count)
	{
		const SRevisionEntry& e = m_oRevisionHistory[index];
		if (sField.IsEmpty())                          sValue = e.rev;
		else if (sField.CompareNoCase(_T("Date")) == 0) sValue = e.date;
		else if (sField.CompareNoCase(_T("Desc")) == 0) sValue = e.description;
		else                                           sValue = e.revisedBy;
	}
	return true;
}
//-------------------------------------------------------------------------
bool CDetails::IsBuiltInToken(const CString& sName)
{
	static const TCHAR* const builtIns[] = {
		_T("Title"), _T("Author"), _T("Revision"),
		_T("DocNo"), _T("Document"),
		_T("Organisation"), _T("Org"),
		_T("Sheets"), _T("Date"),
		_T("Description"), _T("RevisedBy"),
	};
	for (size_t i = 0; i < sizeof(builtIns) / sizeof(builtIns[0]); ++i)
	{
		if (sName.CompareNoCase(builtIns[i]) == 0)
		{
			return true;
		}
	}
	int row = 0;
	CString sField;
	return ParseHistoryToken(sName, row, sField);
}
//-------------------------------------------------------------------------
// Resolve {token} occurrences in sInput.  User-defined tokens override
// built-ins (Title, Author, Revision, DocNo, Organisation, Sheets, Date).
// Substitutions are re-applied so a token value can reference other tokens,
// up to a fixed expansion depth which protects against cycles.
CString CDetails::Resolve(const CString& sInput) const
{
	// Fast path: nothing to substitute if there is no opening brace.  This
	// keeps the per-paint cost negligible for the many text strings (object
	// text, labels, rulers) that contain no token references.
	if (sInput.Find(_T('{')) < 0)
	{
		return sInput;
	}

	CString sResult = sInput;
	const int kMaxPasses = 8;

	for (int pass = 0; pass < kMaxPasses; ++pass)
	{
		bool bChanged = false;
		CString sNext;
		int i = 0;
		const int len = sResult.GetLength();

		while (i < len)
		{
			TCHAR c = sResult[i];
			if (c == _T('{'))
			{
				int j = i + 1;
				while (j < len && sResult[j] != _T('}'))
				{
					++j;
				}
				if (j < len)
				{
					CString sName = sResult.Mid(i + 1, j - i - 1);
					sName.Trim();
					CString sValue;
					bool bResolved = false;

					CDetailsTokenMap::const_iterator it = m_oUserTokens.find(sName);
					if (it != m_oUserTokens.end())
					{
						sValue = it->second;
						bResolved = true;
					}
					else if (sName.CompareNoCase(_T("Title")) == 0)        { sValue = m_sTitle;        bResolved = true; }
					else if (sName.CompareNoCase(_T("Author")) == 0)       { sValue = m_sAuthor;       bResolved = true; }
					else if (sName.CompareNoCase(_T("Revision")) == 0)     { sValue = m_sRevision;     bResolved = true; }
					else if (sName.CompareNoCase(_T("DocNo")) == 0 ||
					         sName.CompareNoCase(_T("Document")) == 0)     { sValue = m_sDocNo;        bResolved = true; }
					else if (sName.CompareNoCase(_T("Organisation")) == 0 ||
					         sName.CompareNoCase(_T("Org")) == 0)          { sValue = m_sOrg;          bResolved = true; }
					else if (sName.CompareNoCase(_T("Sheets")) == 0)       { sValue = GetSheetsDisplay(); bResolved = true; }
					else if (sName.CompareNoCase(_T("Date")) == 0)         { sValue = m_szLastChange;  bResolved = true; }
					else if (sName.CompareNoCase(_T("Description")) == 0)  { sValue = m_sDescription;  bResolved = true; }
					else if (sName.CompareNoCase(_T("RevisedBy")) == 0)
					{
						if (!m_oRevisionHistory.empty())
						{
							sValue = m_oRevisionHistory.back().revisedBy;
						}
						bResolved = true;
					}
					else
					{
						bResolved = ResolveHistoryToken(sName, sValue);
					}

					if (bResolved)
					{
						sNext += sValue;
						bChanged = true;
						i = j + 1;
						continue;
					}
				}
			}
			sNext += c;
			++i;
		}

		sResult = sNext;
		if (!bChanged)
		{
			break;
		}
	}

	return sResult;
}
//-------------------------------------------------------------------------
//-- Set the page boundries from a CPoint
void CDetails::SetPageBounds(CPoint ptBounds)
{
	m_szPage = ptBounds;
}
//-------------------------------------------------------------------------
//-- Update the page boundries etc, using a printer device context
void CDetails::SetPageBounds(PRINTDLG& pd)
{
	CDC* dc = CDC::FromHandle(pd.hDC);

	// Change the scaling to print correct size
	m_szPage.cx = dc->GetDeviceCaps(HORZSIZE) * PIXELSPERMM;
	m_szPage.cy = dc->GetDeviceCaps(VERTSIZE) * PIXELSPERMM;
}
//-------------------------------------------------------------------------
void CDetails::Read(CStream& oArchive)
{
	Init();
	BYTE readb;
	LONG l;

	oArchive >> m_szPage.cx;
	oArchive >> m_szPage.cy;
	oArchive >> m_sTitle;
	oArchive >> m_sAuthor;
	oArchive >> m_sRevision;
	oArchive >> m_sDocNo;
	oArchive >> m_sOrg;
	oArchive >> m_sSheets;
	oArchive >> readb;
	oArchive >> l;

	CTime d(l);
	m_szLastChange = d.Format("%d %B %Y");
	m_bIsVisible = readb != 0;
}
//-------------------------------------------------------------------------
void CDetails::ReadEx(CStream& oArchive)
{
	BYTE nShows;
	CString sDate;

	Init();

	oArchive >> m_szPage.cx;
	oArchive >> m_szPage.cy;
	oArchive >> m_sTitle;
	oArchive >> m_sAuthor;
	oArchive >> m_sRevision;
	oArchive >> m_sDocNo;
	oArchive >> m_sOrg;
	oArchive >> m_sSheets;
	oArchive >> nShows;
	oArchive >> sDate;

	SetLastChange(sDate);
	SetVisible( (nShows & 1) != 0);
	SetRulers( (nShows & 2) != 0, 5, 7);
}
//-------------------------------------------------------------------------
//-- Load a design from a file, loaded design will be selected
void CDetails::ReadXML(CXMLReader& xml, TransformSnap& oSnap)
{
	CString sName;

	xml.intoTag();

	while (xml.nextTag(sName))
	{
		if (sName == _T("Size"))
		{
			xml.getAttribute(_T("width"), m_szPage.cx);
			xml.getAttribute(_T("height"), m_szPage.cy);
		}
		else if (sName == _T("TITLE"))
		{
			xml.getChildData(m_sTitle);
		}
		else if (sName == _T("AUTHOR"))
		{
			xml.getChildData(m_sAuthor);
		}
		else if (sName == _T("REVISION"))
		{
			xml.getChildData(m_sRevision);
		}
		else if (sName == _T("DOCNUMBER"))
		{
			xml.getChildData(m_sDocNo);
		}
		else if (sName == _T("ORGANISATION"))
		{
			xml.getChildData(m_sOrg);
		}
		else if (sName == _T("DESCRIPTION"))
		{
			xml.getChildData(m_sDescription);
		}
		else if (sName == _T("REVISION_HISTORY"))
		{
			m_oRevisionHistory.clear();
			xml.intoTag();
			CString sEntryTag;
			while (xml.nextTag(sEntryTag))
			{
				if (sEntryTag == _T("ENTRY"))
				{
					SRevisionEntry e;
					xml.getAttribute(_T("rev"), e.rev);
					xml.getAttribute(_T("date"), e.date);
					xml.getAttribute(_T("by"), e.revisedBy);
					xml.getChildData(e.description);
					m_oRevisionHistory.push_back(e);
				}
			}
			xml.outofTag();
		}
		else if (sName == _T("SHEETS"))
		{
			xml.getChildData(m_sSheets);
		}
		else if (sName == _T("SHOWS"))
		{
			int nShows = 0;
			xml.getChildData(nShows);
			m_bHasRulers = (nShows & 2) != 0;
			SetVisible( (nShows & 1) != 0);
		}
		else if (sName == _T("GUIDES"))
		{
			xml.getAttribute(_T("horiz"), m_iHorizRulerSize);
			xml.getAttribute(_T("vert"), m_iVertRulerSize);
		}
		else if (sName == _T("DATE"))
		{
			CString sLastChange;

			xml.getChildData(sLastChange);
			SetLastChange(sLastChange);
		}
		else if (sName == _T("USERTOKEN"))
		{
			CString sTokenName;
			CString sTokenValue;
			xml.getAttribute(_T("name"), sTokenName);
			xml.getChildData(sTokenValue);
			sTokenName.Trim();
			if (!sTokenName.IsEmpty())
			{
				m_oUserTokens[sTokenName] = sTokenValue;
			}
		}
		else if (sName == _T("TITLEBLOCK_SVG"))
		{
			CString sEnc;
			xml.getAttribute(_T("name"), m_sTitleBlockName);
			xml.getAttribute(_T("enc"), sEnc);

			CString sData;
			xml.getChildData(sData);

			if (sEnc.CompareNoCase(_T("base64")) == 0)
			{
				CTitleBlockTemplateStore::DecodeSvgBase64(sData, m_sTitleBlockSvg);
			}
			else
			{
				// Legacy files store the raw SVG as plain child data.
				m_sTitleBlockSvg = sData;
			}
		}
		else if (sName == _T("GRID"))
		{
			oSnap.LoadXML(xml);
		}
	}

	// Pick the SVG to render: named template if installed, else embedded copy.
	ResolveTitleBlock();
}
//-------------------------------------------------------------------------
void CDetails::ResolveTitleBlock()
{
	m_sEffectiveSvg.Empty();

	CString svg;
	if (!m_sTitleBlockName.IsEmpty() && CTitleBlockTemplateStore::FindByName(m_sTitleBlockName, svg))
	{
		m_sEffectiveSvg = svg;
	}
	else
	{
		// Fall back to the embedded copy — covers one-off (Browse...) SVGs and
		// machines that lack the named template (the shared-.dsn guarantee).
		m_sEffectiveSvg = m_sTitleBlockSvg;
	}

	// A growing revision table gets one row per history entry.
	std::vector<CString> descriptions;
	for (size_t i = 0; i < m_oRevisionHistory.size(); ++i)
	{
		descriptions.push_back(m_oRevisionHistory[i].description);
	}
	m_sEffectiveSvg = CTitleBlockTemplateStore::ExpandRevisionRows(m_sEffectiveSvg, descriptions);

	// The revision table has as many rows as the highest {Rev<N>...} token.
	m_nHistoryRows = 0;
	int pos = 0;
	while ((pos = m_sEffectiveSvg.Find(_T('{'), pos)) >= 0)
	{
		int end = m_sEffectiveSvg.Find(_T('}'), pos + 1);
		if (end < 0)
		{
			break;
		}
		int row = 0;
		CString sField;
		CString sName = m_sEffectiveSvg.Mid(pos + 1, end - pos - 1);
		sName.Trim();
		if (ParseHistoryToken(sName, row, sField) && row > m_nHistoryRows)
		{
			m_nHistoryRows = row;
		}
		pos = end + 1;
	}
}
//-------------------------------------------------------------------------
void CDetails::WriteXML(CXMLWriter& xml) const
{
	int nShows = 0;

	if (m_bIsVisible)
	{
		nShows |= 1;
	}

	if (m_bHasRulers)
	{
		nShows |= 2;
	}

	xml.addTag(_T("Size"));
	xml.addAttribute(_T("width"), m_szPage.cx);
	xml.addAttribute(_T("height"), m_szPage.cy);
	xml.closeTag();

	xml.addTag(_T("GUIDES"));
	xml.addAttribute(_T("horiz"), m_iHorizRulerSize);
	xml.addAttribute(_T("vert"), m_iVertRulerSize);
	xml.closeTag();

	xml.addTag(_T("TITLE"), m_sTitle);
	xml.addTag(_T("AUTHOR"), m_sAuthor);
	xml.addTag(_T("REVISION"), m_sRevision);
	xml.addTag(_T("DOCNUMBER"), m_sDocNo);
	xml.addTag(_T("ORGANISATION"), m_sOrg);
	if (!m_sDescription.IsEmpty())
	{
		xml.addTag(_T("DESCRIPTION"), m_sDescription);
	}
	if (!m_oRevisionHistory.empty())
	{
		xml.addTag(_T("REVISION_HISTORY"));
		for (size_t i = 0; i < m_oRevisionHistory.size(); ++i)
		{
			const SRevisionEntry& e = m_oRevisionHistory[i];
			xml.addTag(_T("ENTRY"));
			xml.addAttribute(_T("rev"), e.rev);
			xml.addAttribute(_T("date"), e.date);
			xml.addAttribute(_T("by"), e.revisedBy);
			xml.addChildData(e.description);
			xml.closeTag();
		}
		xml.closeTag();
	}
	xml.addTag(_T("SHEETS"), m_sSheets);
	xml.addTag(_T("SHOWS"), nShows);
	xml.addTag(_T("DATE"), m_szLastChange);

	for (CDetailsTokenMap::const_iterator it = m_oUserTokens.begin(); it != m_oUserTokens.end(); ++it)
	{
		xml.addTag(_T("USERTOKEN"));
		xml.addAttribute(_T("name"), it->first);
		xml.addChildData(it->second);
		xml.closeTag();
	}

	if (!m_sTitleBlockSvg.IsEmpty())
	{
		// Hybrid storage: a name reference (resolved against the template store
		// on load, so central edits propagate) plus a base64-encoded embedded
		// copy (escape-safe, and renders when the named template is absent).
		xml.addTag(_T("TITLEBLOCK_SVG"));
		if (!m_sTitleBlockName.IsEmpty())
		{
			xml.addAttribute(_T("name"), m_sTitleBlockName);
		}
		xml.addAttribute(_T("enc"), _T("base64"));
		xml.addChildData(CTitleBlockTemplateStore::EncodeSvgBase64(m_sTitleBlockSvg));
		xml.closeTag();
	}
}
//-------------------------------------------------------------------------
// Draw the details box
void CDetails::Display(CContext& dc, COption& oOption, CString sPathName) const
{
	DisplayBox(dc, oOption, sPathName);
	DisplayRulers(dc, oOption);
}
//-------------------------------------------------------------------------
// Draw the details box
void CDetails::DisplayBox(CContext& dc, COption& oOption, CString sPathName) const
{
	// Do we display the details?
	if (m_bIsVisible)
	{
		// Compute the bottom-right title-block rectangle.
		CDPoint tl = CDPoint(m_szPage.cx - M_NBOXWIDTH - 2, m_szPage.cy - M_NLINEHEIGHT * 9 - 2);
		CDPoint br = CDPoint(m_szPage.cx - 2, m_szPage.cy - 2);
		if (m_bHasRulers)
		{
			tl -= CDPoint(M_NRULERHEIGHT, M_NRULERHEIGHT);
			br -= CDPoint(M_NRULERHEIGHT, M_NRULERHEIGHT);
		}

		// If a user SVG title block is set, render it instead of the built-in
		// box.  Falls through to the procedural drawing on parse failure.
		// Use the resolved SVG (named template wins); fall back to the embedded
		// copy if ResolveTitleBlock() has not run yet.
		const CString& sRenderSvg = m_sEffectiveSvg.IsEmpty() ? m_sTitleBlockSvg : m_sEffectiveSvg;
		if (!sRenderSvg.IsEmpty())
		{
			CSvgTitleBlock svg;
			if (svg.Load(sRenderSvg))
			{
				double w_px = 0.0, h_px = 0.0;
				CDPoint svgTl = tl, svgBr = br;
				if (svg.GetNaturalSizePixels(w_px, h_px))
				{
					// NanoSVG returns dimensions at 96 dpi; convert to the
					// document's internal CAD units (which run at
					// M_NPIXELSPERMM pixels per millimetre).
					const double kPxToCad = (double)M_NPIXELSPERMM * 25.4 / 96.0;
					const double svgW = w_px * h_px > 0.0 ? w_px * kPxToCad : (br.x - tl.x);
					const double svgH = w_px * h_px > 0.0 ? h_px * kPxToCad : (br.y - tl.y);
					// Anchor to the bottom-right corner of the page (where a
					// title block traditionally lives), with the same 2-unit
					// margin and ruler offset used by the procedural rect.
					svgBr = CDPoint(m_szPage.cx - 2, m_szPage.cy - 2);
					svgTl = CDPoint(svgBr.x - svgW, svgBr.y - svgH);
					if (m_bHasRulers)
					{
						svgTl -= CDPoint(M_NRULERHEIGHT, M_NRULERHEIGHT);
						svgBr -= CDPoint(M_NRULERHEIGHT, M_NRULERHEIGHT);
					}
				}
				svg.Paint(dc, svgTl, svgBr, *this);
				return;
			}
		}

		// Select the correct pen
		dc.SelectPen(PS_SOLID, 1, cBLACK);

		// Select the correct font
		dc.SelectFont(*oOption.GetFont(fTEXT), 3);

		int LineHeight = M_NLINEHEIGHT;
		int TextSpace = LineHeight / 2;
		double BottomRow = (br.x - tl.x) / 3;
		double MiddleRow = br.x - (br.x - tl.x) / 5;

		// Now draw the outline
		dc.MoveTo(tl);
		dc.LineTo(CDPoint(br.x, tl.y));
		dc.LineTo(br);
		dc.LineTo(CDPoint(tl.x, br.y));
		dc.LineTo(tl);

		// Draw the horizontal lines
		dc.MoveTo(CDPoint(tl.x, tl.y + LineHeight * 2));
		dc.LineTo(CDPoint(br.x, tl.y + LineHeight * 2));
		dc.MoveTo(CDPoint(tl.x, tl.y + LineHeight * 5));
		dc.LineTo(CDPoint(br.x, tl.y + LineHeight * 5));
		dc.MoveTo(CDPoint(tl.x, tl.y + LineHeight * 7));
		dc.LineTo(CDPoint(br.x, tl.y + LineHeight * 7));

		// Draw the vertical lines
		dc.MoveTo(CDPoint(MiddleRow, tl.y + LineHeight * 5));
		dc.LineTo(CDPoint(MiddleRow, tl.y + LineHeight * 7));
		dc.MoveTo(CDPoint(tl.x + BottomRow, tl.y + LineHeight * 7));
		dc.LineTo(CDPoint(tl.x + BottomRow, tl.y + LineHeight * 9));
		dc.MoveTo(CDPoint(MiddleRow, tl.y + LineHeight * 7));
		dc.LineTo(CDPoint(MiddleRow, tl.y + LineHeight * 9));

		// Add the text
		dc.SetTextAlign(TA_LEFT | TA_BOTTOM | TA_NOUPDATECP);
		dc.SetTextColor(cBLACK);
		dc.SetROP2(R2_COPYPEN);
		dc.SetBkMode(TRANSPARENT);

		dc.TextOut(tl.x + TextSpace, tl.y + LineHeight, _T("Title"));
		dc.TextOut(tl.x + TextSpace, tl.y + LineHeight * 3, _T("Author"));
		dc.TextOut(tl.x + TextSpace, tl.y + LineHeight * 6, _T("File"));
		dc.TextOut(tl.x + TextSpace, tl.y + LineHeight * 8, _T("Document"));
		dc.TextOut(MiddleRow + TextSpace, tl.y + LineHeight * 6, _T("Sheets"));
		dc.TextOut(tl.x + BottomRow + TextSpace, tl.y + LineHeight * 8, _T("Date"));
		dc.TextOut(MiddleRow + TextSpace, tl.y + LineHeight * 8, _T("Revision"));

		// Add the actual data!
		dc.TextOut(tl.x + TextSpace * 2, tl.y + LineHeight * 2, Resolve(m_sTitle));
		dc.TextOut(tl.x + TextSpace * 2, tl.y + LineHeight * 4, Resolve(m_sAuthor));
		dc.TextOut(tl.x + TextSpace * 2, tl.y + LineHeight * 5, Resolve(m_sOrg));


		// Display the file path name
		dc.TextOut(tl.x + TextSpace * 2, tl.y + LineHeight * 7, static_cast<int> (MiddleRow - tl.x - TextSpace * 4), sPathName);


		dc.TextOut(tl.x + TextSpace * 2, tl.y + LineHeight * 9, Resolve(m_sDocNo));
		dc.TextOut(MiddleRow + TextSpace * 2, tl.y + LineHeight * 7, GetSheetsDisplay());
		dc.TextOut(tl.x + BottomRow + TextSpace * 2, tl.y + LineHeight * 9, Resolve(GetLastChange()));
		dc.TextOut(MiddleRow + TextSpace * 2, tl.y + LineHeight * 9, Resolve(m_sRevision));

	}
}
//-------------------------------------------------------------------------
// Draw the design rulers
void CDetails::DisplayRulers(CContext& dc, COption& oOption) const
{
	if (m_bHasRulers)
	{
		// Select the correct pen
		dc.SelectPen(PS_SOLID, 1, cLINE);

		// Draw the outline box
		CDPoint tl1 = CDPoint(0, 0);
		CDPoint tl2 = CDPoint(M_NRULERHEIGHT + 2, M_NRULERHEIGHT + 2);
		CDPoint br2 = CDPoint(m_szPage.cx - M_NRULERHEIGHT - 2, m_szPage.cy - M_NRULERHEIGHT - 2);
		CDPoint br1 = CDPoint(m_szPage.cx - 2, m_szPage.cy - 2);

		// Draw the 4 lines that make up the outer boarder
		dc.MoveTo(tl1);
		dc.LineTo(CDPoint(br1.x, tl1.y));
		dc.LineTo(br1);
		dc.LineTo(CDPoint(tl1.x, br1.y));
		dc.LineTo(tl1);

		// Draw the 4 lines that make up the inner boarder
		dc.MoveTo(tl2);
		dc.LineTo(CDPoint(br2.x, tl2.y));
		dc.LineTo(br2);
		dc.LineTo(CDPoint(tl2.x, br2.y));
		dc.LineTo(tl2);

		// Now draw the cross bracings
		dc.MoveTo(tl1);
		dc.LineTo(tl2);
		dc.MoveTo(br1);
		dc.LineTo(br2);
		dc.MoveTo(CDPoint(br1.x, tl1.y));
		dc.LineTo(CDPoint(br2.x, tl2.y));
		dc.MoveTo(CDPoint(tl1.x, br1.y));
		dc.LineTo(CDPoint(tl2.x, br2.y));

		// Now lay out in a 5 x 7 layout...
		dc.SetTextAlign(TA_CENTER | TA_BOTTOM | TA_NOUPDATECP);
		dc.SetTextColor(cBLACK);
		dc.SetROP2(R2_COPYPEN);
		dc.SetBkMode(TRANSPARENT);

		// Select the correct font
		dc.SelectFont(*oOption.GetFont(fPIN), 3);

		int cols = m_iHorizRulerSize;
		int rows = m_iVertRulerSize;

		if (IsPortrait())
		{
			cols = 5;
			rows = 7;
		}

		double split = (br1.x - tl1.x) / cols;
		for (int col = 0; col < cols; col++)
		{
			CString s;
			s.Format(_T("%d"), col + 1);

			dc.MoveTo(CDPoint(col * split, tl1.y));
			dc.LineTo(CDPoint(col * split, tl2.y));
			dc.TextOut(split * col + split / 2, tl2.y - 2, s);

			dc.MoveTo(CDPoint(col * split, br1.y));
			dc.LineTo(CDPoint(col * split, br2.y));
			dc.TextOut(split * col + split / 2, br1.y - 2, s);

		}

		// Select the correct font
		dc.SelectFont(*oOption.GetFont(fPIN), 0);

		split = (br1.y - tl1.y) / rows;
		for (int row = 0; row < rows; row++)
		{
			CString s;
			s.Format(_T("%c"), 'A' + row);

			dc.MoveTo(CDPoint(tl1.x, row * split));
			dc.LineTo(CDPoint(tl2.x, row * split));
			dc.TextOut(tl2.x - 2, split * row + split / 2, s);

			dc.MoveTo(CDPoint(br1.x, row * split));
			dc.LineTo(CDPoint(br2.x, row * split));
			dc.TextOut(br1.x - 1, split * row + split / 2, s);
		}
	}
}
//-------------------------------------------------------------------------

