/*
 ConCAD (a fork of TinyCAD) - revision-history table drawing object

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
 */

#include "stdafx.h"
#include "ConCad.h"
#include "ConCadView.h"
#include "colour.h"
#include "SvgTitleBlock.h"
#include <shlobj.h>

////// The revision-history table //////

// The linked template, first match wins:
//   %APPDATA%\ConCAD\templates\revision.svg   (per-user override)
//   <exe-dir>\templates\revision.svg          (installer-bundled)
//   <exe-dir>\..\templates\revision.svg       (dev build)
static CString FindRevisionTemplate()
{
	CString candidates[3];
	TCHAR appData[MAX_PATH] = { 0 };
	if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, appData)))
	{
		candidates[0] = CString(appData) + _T("\\ConCAD\\templates\\revision.svg");
	}
	const CString mainDir = CConCadApp::GetMainDir();
	candidates[1] = mainDir + _T("templates\\revision.svg");
	candidates[2] = mainDir + _T("..\\templates\\revision.svg");
	for (int i = 0; i < 3; ++i)
	{
		if (!candidates[i].IsEmpty() && GetFileAttributes(candidates[i]) != INVALID_FILE_ATTRIBUTES)
		{
			return candidates[i];
		}
	}
	return CString();
}

CDrawRevisionHistory::CDrawRevisionHistory(CConCadDoc *pDesign) :
	CDrawingObject(pDesign)
{
	m_bVisible = true;
	m_ftLinked.dwLowDateTime = m_ftLinked.dwHighDateTime = 0;
	m_point_a = m_point_b = CDPoint(0, 0);
	m_segment = 0;
}

const TCHAR* CDrawRevisionHistory::GetXMLTag()
{
	return _T("REVISION_TABLE");
}

ObjType CDrawRevisionHistory::GetType()
{
	return xRevisionHistory;
}

CString CDrawRevisionHistory::GetName() const
{
	return _T("Revision history");
}

// The linked file is re-read whenever it changes on disk, so edits to
// revision.svg show up without reopening the design.  The last good text is
// kept as the embedded copy, which is saved with the design and used on
// machines that do not have the file.
CString CDrawRevisionHistory::GetSvg()
{
	const CString path = FindRevisionTemplate();
	WIN32_FILE_ATTRIBUTE_DATA fad;
	if (!path.IsEmpty() && GetFileAttributesEx(path, GetFileExInfoStandard, &fad))
	{
		if (path != m_sLinkedPath || CompareFileTime(&fad.ftLastWriteTime, &m_ftLinked) != 0)
		{
			CString svg;
			if (CTitleBlockTemplateStore::ReadFile(path, svg))
			{
				m_sLinkedSvg = svg;
				m_sEmbeddedSvg = svg;
			}
			m_sLinkedPath = path;
			m_ftLinked = fad.ftLastWriteTime;
		}
		if (!m_sLinkedSvg.IsEmpty())
		{
			return m_sLinkedSvg;
		}
	}
	return m_sEmbeddedSvg;
}

// Parse the template with one row per history entry; sets m_point_b.
bool CDrawRevisionHistory::LoadSvg(CSvgTitleBlock& svg, int& rows)
{
	const CString text = GetSvg();
	if (text.IsEmpty())
	{
		return false;
	}
	const CRevisionHistory& history = m_pDesign->GetDetails().GetRevisionHistory();
	std::vector<CString> descriptions;
	for (size_t i = 0; i < history.size(); ++i)
	{
		descriptions.push_back(history[i].description);
	}
	if (!svg.Load(CTitleBlockTemplateStore::ExpandRevisionRows(text, descriptions, &rows)))
	{
		return false;
	}
	double w_px = 0.0, h_px = 0.0;
	if (!svg.GetNaturalSizePixels(w_px, h_px) || w_px * h_px <= 0.0)
	{
		return false;
	}
	// NanoSVG sizes are at 96 dpi; convert to internal units.
	const double kPxToCad = PIXELSPERMM * 25.4 / 96.0;
	m_point_b = CDPoint(m_point_a.x + w_px * kPxToCad, m_point_a.y - h_px * kPxToCad);
	return true;
}

void CDrawRevisionHistory::UpdateExtent()
{
	CSvgTitleBlock svg;
	int rows = 0;
	if (!LoadSvg(svg, rows))
	{
		m_point_b = m_point_a;
	}
}

void CDrawRevisionHistory::Paint(CContext &dc, paint_options options)
{
	if (!m_bVisible)
	{
		return;
	}

	CSvgTitleBlock svg;
	int rows = 0;
	if (!LoadSvg(svg, rows))
	{
		return;
	}

	// The history tokens are numbered by table row: tell the resolver how
	// many rows this table shows (the title block may show a different count).
	CDetails details = m_pDesign->GetDetails();
	details.m_nHistoryRows = rows;
	svg.Paint(dc, CDPoint(m_point_a.x, m_point_b.y), CDPoint(m_point_b.x, m_point_a.y), details);

	if (options == draw_selected || options == draw_selectable)
	{
		dc.SelectPen(PS_DOT, 1, options == draw_selected ? cSELECT : cPIN_CLK);
		dc.SelectBrush();
		dc.Rectangle(CDRect(m_point_a.x, m_point_b.y, m_point_b.x, m_point_a.y));
	}
}

void CDrawRevisionHistory::Display(BOOL erase)
{
	UpdateExtent();
	CDRect r(m_point_a.x, m_point_b.y, m_point_b.x, m_point_a.y);
	r.NormalizeRect();
	r.InflateRect(2, 2, 2, 2);
	m_pDesign->InvalidateRect(r, erase, 0);
}

double CDrawRevisionHistory::DistanceFromPoint(CDPoint p)
{
	if (!m_bVisible)
	{
		return 1E5;
	}
	const double left = min(m_point_a.x, m_point_b.x), right = max(m_point_a.x, m_point_b.x);
	const double top = min(m_point_a.y, m_point_b.y), bottom = max(m_point_a.y, m_point_b.y);
	if (p.x >= left && p.x <= right && p.y >= top && p.y <= bottom)
	{
		return 0.0;
	}
	return 1E5;
}

BOOL CDrawRevisionHistory::IsInside(double left, double right, double top, double bottom)
{
	if (!m_bVisible)
	{
		return FALSE;
	}
	const double l = min(m_point_a.x, m_point_b.x), r = max(m_point_a.x, m_point_b.x);
	const double t = min(m_point_a.y, m_point_b.y), b = max(m_point_a.y, m_point_b.y);
	return !(r < left || l > right || b < top || t > bottom);
}

CDrawingObject* CDrawRevisionHistory::Store()
{
	CDrawRevisionHistory *NewObject = new CDrawRevisionHistory(m_pDesign);
	*NewObject = *this;
	m_pDesign->Add(NewObject);
	return NewObject;
}

// <REVISION_TABLE pos='x,y' visible='1'>base64 of the template</REVISION_TABLE>
void CDrawRevisionHistory::SaveXML(CXMLWriter &xml)
{
	GetSvg();   // refresh the embedded copy from the linked file

	xml.addTag(GetXMLTag());
	xml.addAttribute(_T("pos"), CDPoint(m_point_a));
	xml.addAttribute(_T("visible"), m_bVisible ? 1 : 0);
	if (!m_sEmbeddedSvg.IsEmpty())
	{
		xml.addChildData(CTitleBlockTemplateStore::EncodeSvgBase64(m_sEmbeddedSvg));
	}
	xml.closeTag();
}

void CDrawRevisionHistory::LoadXML(CXMLReader &xml)
{
	int visible = 1;
	xml.getAttribute(_T("pos"), m_point_a);
	xml.getAttribute(_T("visible"), visible);
	m_bVisible = visible != 0;

	CString data;
	xml.getChildData(data);
	m_sEmbeddedSvg.Empty();
	if (!data.IsEmpty())
	{
		CTitleBlockTemplateStore::DecodeSvgBase64(data, m_sEmbeddedSvg);
	}
	m_point_b = m_point_a;
}
